#include "platform/windows/operations/live_move_writer.h"

#include "core/geometry/checked_arithmetic.h"
#include "platform/windows/operations/window_rect_adjustment.h"

#include <stdexcept>
#include <utility>

namespace panebind::platform::windows::operations {
namespace {

bool same_positive_size(const core::geometry::Rect& a,
                        const core::geometry::Rect& b) noexcept {
    using core::geometry::checked_difference;
    const auto aw = checked_difference(a.right(), a.left());
    const auto ah = checked_difference(a.bottom(), a.top());
    const auto bw = checked_difference(b.right(), b.left());
    const auto bh = checked_difference(b.bottom(), b.top());
    return aw && ah && bw && bh && *aw > 0 && *ah > 0 &&
           *aw == *bw && *ah == *bh;
}

} // namespace

MovePreflightVerdict classify_move_preflight(
    const MovePreflightFacts& facts) noexcept {
    if (!facts.context_valid) return MovePreflightVerdict::ContextInvalid;
    if (facts.matching_raw_up) return MovePreflightVerdict::NormalUpRevoked;
    return facts.gesture_active ? MovePreflightVerdict::Allowed
                                : MovePreflightVerdict::ContextInvalid;
}

bool move_post_context_valid(const MovePostContextFacts& facts) noexcept {
    return facts.source_identity_valid && facts.participant_identity_valid &&
        facts.geometry_context_valid && facts.captured_after_matches;
}

bool move_write_receipt_failed(const MoveWriteReceipt& receipt,
                               bool continuity_committed) noexcept {
    if (receipt.exact && (!receipt.native_attempted || continuity_committed))
        return false;
    if (!receipt.native_attempted &&
        (receipt.reason == "superseded_before_native" ||
         receipt.reason == "retired_before_native" ||
         receipt.reason == "normal_up_before_native"))
        return false;
    return true;
}

LiveMoveWriter::LiveMoveWriter(std::uint64_t generation,
                               MoveWriteCallbacks callbacks)
    : generation_(generation), callbacks_(std::move(callbacks)),
      retired_(generation == 0 || !callbacks_.capture ||
               !callbacks_.fresh_authority || !callbacks_.place ||
               !callbacks_.post_context) {}

bool LiveMoveWriter::ready() const noexcept {
    std::lock_guard lock{mutex_};
    return !retired_;
}

bool LiveMoveWriter::active(std::uint64_t generation) const noexcept {
    std::lock_guard lock{mutex_};
    return generation == generation_ && !retired_ && reserved_quantum_ != 0;
}

bool LiveMoveWriter::begin_native_attempt(std::uint64_t generation) noexcept {
    std::lock_guard lock{mutex_};
    if (generation != generation_ || reserved_quantum_ == 0 ||
        attempt_claimed_) return false;
    if (retired_) {
        attempt_rejected_ = true;
        return false;
    }
    attempt_claimed_ = true;
    return true;
}

bool LiveMoveWriter::offer(const MoveWriteRequest& request) {
    std::lock_guard lock{mutex_};
    if (retired_ || request.generation != generation_ || request.quantum == 0 ||
        request.quantum <= last_started_quantum_ ||
        request.quantum < last_offered_quantum_) {
        return false;
    }
    // Repeated offers for an unstarted quantum replace the one slot. Once a
    // quantum is consumed, it can never issue a second native placement.
    pending_ = request;
    last_offered_quantum_ = request.quantum;
    condition_.notify_one();
    return true;
}

MoveRetireFacts LiveMoveWriter::retire(std::uint64_t generation) noexcept {
    std::lock_guard lock{mutex_};
    MoveRetireFacts facts;
    if (generation != generation_ || retired_) {
        return facts;
    }
    facts.retired_now = true;
    facts.pending_discarded = pending_.has_value();
    facts.dispatch_reserved = reserved_quantum_ != 0;
    facts.reserved_quantum = reserved_quantum_;
    facts.attempt_started_before_revoke = attempt_claimed_;
    retired_ = true;
    pending_.reset();
    condition_.notify_all();
    return facts;
}

std::optional<MoveWriteReceipt> LiveMoveWriter::wait_and_execute() {
    return pop_and_execute(true);
}

std::optional<MoveWriteReceipt> LiveMoveWriter::try_execute() {
    return pop_and_execute(false);
}

std::optional<MoveWriteReceipt> LiveMoveWriter::pop_and_execute(bool wait) {
    MoveWriteRequest request;
    {
        std::unique_lock lock{mutex_};
        if (worker_active_) {
            throw std::logic_error{"LiveMoveWriter requires a single consumer"};
        }
        worker_active_ = true;
        if (wait) {
            condition_.wait(lock, [this] { return retired_ || pending_.has_value(); });
        }
        if (retired_ || !pending_) {
            worker_active_ = false;
            return std::nullopt;
        }
        request = *pending_;
        pending_.reset();
        last_started_quantum_ = request.quantum;
    }
    auto receipt = execute(request);
    {
        std::lock_guard lock{mutex_};
        worker_active_ = false;
    }
    return receipt;
}

MoveWriteReceipt LiveMoveWriter::execute(MoveWriteRequest request) {
    MoveWriteReceipt receipt;
    receipt.generation = request.generation;
    receipt.quantum = request.quantum;
    receipt.target_visible = request.target_visible;
    receipt.snapped = request.snapped;
    const auto fail_closed = [this, &receipt](std::string_view reason) {
        receipt.reason = reason;
        std::lock_guard lock{mutex_};
        retired_ = true;
        pending_.reset();
        condition_.notify_all();
    };

    try {
        receipt.before = callbacks_.capture();
        if (!receipt.before) {
            fail_closed("capture_failed");
            return receipt;
        }
        if (!same_positive_size(receipt.before->visible,
                                request.target_visible)) {
            fail_closed("not_move_only");
            return receipt;
        }
        const auto prepared = prepare_visible_rect_adjustment(
            receipt.before->positioning, receipt.before->visible,
            request.target_visible);
        if (prepared.status != RectAdjustmentStatus::Succeeded ||
            !prepared.positioning) {
            fail_closed("geometry_bridge_failed");
            return receipt;
        }
        receipt.expected_positioning = *prepared.positioning;
        const auto preflight = callbacks_.fresh_authority(*receipt.before);
        if (preflight == MovePreflightVerdict::NormalUpRevoked) {
            (void)retire(request.generation);
            receipt.reason = "normal_up_before_native";
            return receipt;
        }
        if (preflight != MovePreflightVerdict::Allowed) {
            fail_closed("fresh_authority_rejected");
            return receipt;
        }

        if (receipt.before->visible == request.target_visible) {
            // Idempotent geometry needs no native operation, but authority was
            // still checked. A later quantum can issue a real placement.
            receipt.after = receipt.before;
            receipt.geometry_exact = true;
            receipt.post_context_exact = callbacks_.post_context(
                *receipt.before, *receipt.after);
            if (!receipt.post_context_exact) {
                fail_closed("post_context_rejected");
                return receipt;
            }
            receipt.exact = true;
            receipt.reason = "already_exact";
            return receipt;
        }

        {
            std::lock_guard lock{mutex_};
            if (retired_) {
                receipt.reason = "retired_before_native";
                return receipt;
            }
            if (pending_ && pending_->quantum > request.quantum) {
                receipt.reason = "superseded_before_native";
                return receipt;
            }
            // Dispatch reservation is linearized here. It is NOT proof that
            // the adapter has entered a native API. Raw UP/stop retirement
            // takes the same mutex and never waits for a stuck callback.
            reserved_quantum_ = request.quantum;
            attempt_claimed_ = false;
            attempt_rejected_ = false;
            receipt.dispatch_reserved = true;
        }

        try {
            const auto native = callbacks_.place(*receipt.before,
                                                  request.target_visible,
                                                  *receipt.expected_positioning,
                                                  request.quantum);
            receipt.native_attempted = native.attempted;
            receipt.native_succeeded = native.succeeded;
            receipt.native_outcome_known = native.outcome_known;
        } catch (...) {
            receipt.reason = "native_callback_exception";
            receipt.native_outcome_known = false;
        }
        if (receipt.reason == "none" && receipt.native_attempted) {
            try {
                receipt.after = callbacks_.capture();
            } catch (...) {
                receipt.reason = "postcapture_exception";
            }
        }
        if (receipt.reason == "none" && receipt.after) {
            try {
                receipt.post_context_exact = callbacks_.post_context(
                    *receipt.before, *receipt.after);
            } catch (...) {
                receipt.reason = "postcontext_exception";
            }
        }

        {
            std::lock_guard lock{mutex_};
            receipt.retired_during_dispatch = retired_;
            receipt.attempt_start_order = attempt_claimed_
                ? MoveAttemptStartOrder::ClaimedBeforeRevoke
                : attempt_rejected_
                    ? MoveAttemptStartOrder::RejectedAfterRevoke
                    : MoveAttemptStartOrder::NotStarted;
            reserved_quantum_ = 0;
            attempt_claimed_ = false;
            attempt_rejected_ = false;
        }
        if (receipt.reason == "native_callback_exception" ||
            receipt.reason == "postcapture_exception" ||
            receipt.reason == "postcontext_exception") {
            fail_closed(receipt.reason);
            return receipt;
        }
        if (receipt.native_attempted &&
            receipt.attempt_start_order != MoveAttemptStartOrder::ClaimedBeforeRevoke) {
            fail_closed("native_attempt_without_gate");
            return receipt;
        }
        if (!receipt.native_attempted &&
            receipt.attempt_start_order == MoveAttemptStartOrder::ClaimedBeforeRevoke) {
            fail_closed("gate_claim_without_native_attempt");
            return receipt;
        }
        if (!receipt.native_attempted && receipt.retired_during_dispatch) {
            receipt.reason = "retired_before_native";
            return receipt;
        }
        if (!receipt.native_attempted || !receipt.native_succeeded) {
            fail_closed("native_placement_failed");
            return receipt;
        }
        receipt.geometry_exact = receipt.after &&
            receipt.after->visible == request.target_visible &&
            receipt.after->positioning == *receipt.expected_positioning;
        if (!receipt.geometry_exact) {
            fail_closed("exact_postverify_failed");
            return receipt;
        }
        if (!receipt.post_context_exact) {
            fail_closed("post_context_rejected");
            return receipt;
        }
        receipt.exact = true;
        receipt.reason = "exact";
        return receipt;
    } catch (...) {
        fail_closed("writer_preflight_exception");
        return receipt;
    }
}

} // namespace panebind::platform::windows::operations
