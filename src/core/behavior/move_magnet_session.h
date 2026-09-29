#pragma once

#include "core/behavior/cursor_move_magnet_intent.h"
#include "core/behavior/move_handoff_gate.h"

#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace panebind::core::behavior {

// A cursor quantum produces one whole-window target, never a free placement
// followed by a correction. This is a proposal, not native write authority.
struct MoveWritePlan {
    std::uint64_t generation{}, quantum{};
    geometry::Rect free_visible, target_visible;
    bool snapped{};
    magnet::MotionState motion{magnet::MotionState::BelowThreshold};
    std::string_view reason;
};

struct MoveWriteCompletion {
    bool recognized{}, exact{}, after_retirement{};
};

// Thread-safe, one-gesture pure session. The receiver may sample/take a plan
// and offer it to a separate one-slot native writer. No native operation is
// performed while this lock is held. Raw UP/stop first atomically revoke the
// generation, then clear the short-lived pending plan under the lock. A plan
// taken before revocation is NOT proof that a native call was issued: the
// adapter must recheck generation/authority at its actual call boundary and
// account separately for an already-in-flight native call.
class MoveMagnetSession final {
public:
    MoveMagnetSession() = default;
    MoveMagnetSession(const MoveMagnetSession&) = delete;
    MoveMagnetSession& operator=(const MoveMagnetSession&) = delete;

    [[nodiscard]] bool begin(std::uint64_t generation, geometry::Point anchor,
                             geometry::Rect initial_visible,
                             std::span<const magnet::MagnetTarget> frozen_targets,
                             std::uint64_t start_qpc, std::uint64_t frequency,
                             bool exact_source, bool down_held,
                             bool plain_titlebar_move) {
        std::lock_guard lock(mutex_);
        if (begun_) return false;
        CursorMoveMagnetIntent intent;
        MoveHandoffGate gate;
        if (!gate.begin(generation, exact_source, down_held, plain_titlebar_move) ||
            !intent.start(anchor, initial_visible, frozen_targets, start_qpc, frequency))
            return false;
        intent_ = std::move(intent);
        gate_ = std::move(gate);
        generation_.store(generation, std::memory_order_release);
        revoked_.store(false, std::memory_order_release);
        begun_ = true;
        return true;
    }

    [[nodiscard]] bool isolation_ready(std::uint64_t generation,
                                        bool actual_hit_and_context_proven) {
        std::lock_guard lock(mutex_);
        return current_locked(generation) &&
            gate_.isolation_ready(generation, actual_hit_and_context_proven);
    }

    [[nodiscard]] bool may_cancel(std::uint64_t generation) const {
        std::lock_guard lock(mutex_);
        return current_locked(generation) && gate_.may_cancel(generation);
    }

    [[nodiscard]] bool cancel_issued(std::uint64_t generation) {
        std::lock_guard lock(mutex_);
        return current_locked(generation) && gate_.cancel_issued(generation);
    }

    [[nodiscard]] bool native_end_observed(std::uint64_t generation,
                                           bool matching_end) {
        std::lock_guard lock(mutex_);
        return begun_ && generation == generation_.load(std::memory_order_acquire) &&
            gate_.native_end_observed(generation, matching_end);
    }

    // The adapter supplies fresh facts for THIS quantum. A false fact retires
    // the session; it is never latched into authority for a later quantum.
    [[nodiscard]] bool sample_cursor(std::uint64_t generation,
                                     geometry::Point cursor, std::uint64_t qpc,
                                     bool fresh_authority, bool actual_stable,
                                     bool isolation_hits) {
        std::lock_guard lock(mutex_);
        if (!current_locked(generation)) return false;
        if (!fresh_authority || !actual_stable || !isolation_hits) {
            revoke_locked(generation, MoveHandoffEscapeReason::ContextLost);
            return false;
        }
        if (!gate_.may_write(generation, fresh_authority, actual_stable,
                             isolation_hits)) return false;
        // An unconsumed proposal never leaks into a later receiver event.
        pending_.reset();
        auto target = intent_.sample(cursor, qpc);
        if (!target) {
            if (!intent_.active())
                revoke_locked(generation, MoveHandoffEscapeReason::NativeFailure);
            return false;
        }
        if (quantum_ == std::numeric_limits<std::uint64_t>::max()) {
            revoke_locked(generation, MoveHandoffEscapeReason::NativeFailure);
            return false;
        }
        pending_ = MoveWritePlan{generation, ++quantum_, target->free_visible,
                                 target->target_visible, target->snapped(),
                                 target->motion, target->reason};
        // Raw UP can set the atomic flag before it acquires this short lock.
        if (revoked_.load(std::memory_order_acquire)) {
            pending_.reset();
            return false;
        }
        return true;
    }

    // Consume immediately after sample_cursor in the receiver callback; do
    // not treat this as a queue spanning events. The adapter's one-slot writer
    // owns any plan it accepts, with an independent retirement signal.
    [[nodiscard]] std::optional<MoveWritePlan> take_pending(
        std::uint64_t generation, bool fresh_authority, bool actual_stable,
        bool isolation_hits) {
        std::lock_guard lock(mutex_);
        if (!current_locked(generation)) return std::nullopt;
        if (!fresh_authority || !actual_stable || !isolation_hits) {
            revoke_locked(generation, MoveHandoffEscapeReason::ContextLost);
            return std::nullopt;
        }
        if (!gate_.may_write(generation, fresh_authority, actual_stable,
                             isolation_hits) || !pending_) return std::nullopt;
        auto result = std::exchange(pending_, std::nullopt);
        last_taken_quantum_ = result->quantum;
        if (revoked_.load(std::memory_order_acquire)) return std::nullopt;
        return result;
    }

    // Called only after a native placement actually returns. An exact result
    // after Raw UP describes an already-in-flight call, not resumed authority.
    [[nodiscard]] MoveWriteCompletion write_result(const MoveWritePlan& plan,
                                                    bool native_success,
                                                    geometry::Rect actual_visible) {
        std::lock_guard lock(mutex_);
        if (!begun_ || plan.generation != generation_.load(std::memory_order_acquire) ||
            !plan.quantum || plan.quantum > last_taken_quantum_ ||
            plan.quantum <= last_completed_quantum_) return {};
        last_completed_quantum_ = plan.quantum;
        const bool after_retirement = revoked_.load(std::memory_order_acquire);
        const bool exact = native_success && actual_visible == plan.target_visible;
        if (!exact) revoke_locked(plan.generation, MoveHandoffEscapeReason::NativeFailure);
        return {true, exact, after_retirement};
    }

    // Physical UP must be established by the adapter's actual Raw observation.
    // The atomic store precedes any wait for the short state lock.
    [[nodiscard]] bool raw_up_observed(std::uint64_t generation,
                                       bool matching_physical_up) {
        if (!matching_physical_up || generation != generation_.load(std::memory_order_acquire))
            return false;
        revoked_.store(true, std::memory_order_release);
        std::lock_guard lock(mutex_);
        pending_.reset();
        intent_.release();
        return gate_.raw_up_observed(generation, true);
    }

    [[nodiscard]] bool escape(std::uint64_t generation,
                              MoveHandoffEscapeReason reason) {
        if (reason == MoveHandoffEscapeReason::None ||
            generation != generation_.load(std::memory_order_acquire)) return false;
        revoked_.store(true, std::memory_order_release);
        std::lock_guard lock(mutex_);
        pending_.reset();
        intent_.release();
        return gate_.escape(generation, reason);
    }

    [[nodiscard]] bool legacy_up_delivery_observed(std::uint64_t generation,
                                                   bool actual_delivery_proven) {
        std::lock_guard lock(mutex_);
        return begun_ && generation == generation_.load(std::memory_order_acquire) &&
            gate_.legacy_up_delivery_observed(generation, actual_delivery_proven);
    }

    [[nodiscard]] bool removal_allowed(std::uint64_t generation) const {
        std::lock_guard lock(mutex_);
        return begun_ && generation == generation_.load(std::memory_order_acquire) &&
            gate_.removal_allowed(generation);
    }

    [[nodiscard]] bool isolation_removed(std::uint64_t generation) {
        std::lock_guard lock(mutex_);
        if (!begun_ || generation != generation_.load(std::memory_order_acquire) ||
            !gate_.isolation_removed(generation)) return false;
        revoked_.store(true, std::memory_order_release);
        pending_.reset();
        intent_.release();
        return true;
    }

    [[nodiscard]] bool retired(std::uint64_t generation) const noexcept {
        return generation != generation_.load(std::memory_order_acquire) ||
            revoked_.load(std::memory_order_acquire);
    }

private:
    [[nodiscard]] bool current_locked(std::uint64_t generation) const noexcept {
        return begun_ && generation == generation_.load(std::memory_order_acquire) &&
            !revoked_.load(std::memory_order_acquire);
    }
    void revoke_locked(std::uint64_t generation,
                       MoveHandoffEscapeReason reason) noexcept {
        revoked_.store(true, std::memory_order_release);
        pending_.reset();
        intent_.release();
        (void)gate_.escape(generation, reason);
    }

    mutable std::mutex mutex_;
    std::atomic<std::uint64_t> generation_{0};
    std::atomic<bool> revoked_{true};
    CursorMoveMagnetIntent intent_;
    MoveHandoffGate gate_;
    std::optional<MoveWritePlan> pending_;
    std::uint64_t quantum_{}, last_taken_quantum_{}, last_completed_quantum_{};
    bool begun_{};
};

} // namespace panebind::core::behavior
