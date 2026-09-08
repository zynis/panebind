#pragma once

#include "core/geometry/geometry.h"
#include "core/model/window_id.h"
#include "core/movement/move_plan.h"
#include "core/movement/translation.h"
#include "core/topology/window_adjacency.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace panebind::core::behavior {

using SessionGeneration = std::uint64_t;
using OperationGeneration = std::uint64_t;
using EventSequence = std::uint64_t;

enum class GlueMoveState {
    Idle,
    Armed,
    Active,
    Completing,
    Completed,
    Aborted,
};

enum class GlueWindowRole {
    Leader,
    Follower,
    Unrelated,
};

enum class GlueEventKind {
    MoveResizeStarted,
    GeometryChanged,
    MoveResizeEnded,
};

struct GlueEventReceipt {
    SessionGeneration session_generation{};
    EventSequence sequence{};
    GlueWindowRole role{GlueWindowRole::Unrelated};
    GlueEventKind kind{GlueEventKind::GeometryChanged};
    std::optional<geometry::Rect> visible_rect;
};

struct FollowerMoveCommand {
    SessionGeneration session_generation{};
    OperationGeneration operation_generation{};
    EventSequence source_leader_sequence{};
    model::WindowId follower_id;
    geometry::Rect target_visible_rect;
};

enum class FollowerOperationOutcome {
    Exact,
    NativeApplyFailed,
    PostVerificationFailed,
    TargetInvalidated,
};

struct FollowerOperationResult {
    SessionGeneration session_generation{};
    OperationGeneration operation_generation{};
    model::WindowId follower_id;
    FollowerOperationOutcome outcome{FollowerOperationOutcome::NativeApplyFailed};
    std::optional<geometry::Rect> actual_visible_rect;
};

struct GlueFinalSnapshot {
    SessionGeneration session_generation{};
    geometry::Rect leader_visible_rect;
    geometry::Rect follower_visible_rect;
};

enum class GlueAbortReason {
    IllegalTransition,
    InvalidTopology,
    InvalidEvent,
    SessionGenerationMismatch,
    EventSequenceNotMonotonic,
    OperationGenerationMismatch,
    GenerationExhausted,
    ResizeOrMixed,
    LeaderDidNotTranslate,
    PendingLedgerOverflow,
    EventQueueOverflow,
    EventSourceFailure,
    TimedOut,
    OperationResultMissing,
    NativeApplyFailed,
    PostVerificationFailed,
    TargetInvalidated,
    UnexpectedFollowerLifecycle,
    UnexpectedFollowerGeometry,
    AmbiguousFollowerFeedback,
    FinalLeaderMismatch,
    FinalFollowerMismatch,
};

enum class GlueDecisionKind {
    None,
    Armed,
    Activated,
    FollowerMoveRequested,
    LeaderNoOp,
    OperationRecorded,
    FeedbackObservedPendingResult,
    FeedbackAcknowledged,
    DuplicateFeedbackSuppressed,
    UnrelatedEventIgnored,
    TerminalInputIgnored,
    Completing,
    Completed,
    Aborted,
};

struct GlueDecision {
    GlueDecisionKind kind{GlueDecisionKind::None};
    std::optional<FollowerMoveCommand> command;
    std::optional<GlueAbortReason> abort_reason;
};

struct GlueMoveStats {
    std::size_t leader_start_count{};
    std::size_t leader_location_count{};
    std::size_t leader_end_count{};
    std::size_t follower_operation_count{};
    std::size_t follower_noop_count{};
    std::size_t follower_feedback_count{};
    std::size_t suppressed_feedback_count{};
    std::size_t duplicate_feedback_count{};
    std::size_t missing_feedback_count{};
    std::size_t reconciled_feedback_count{};
    std::size_t unexpected_feedback_count{};
    std::size_t unrelated_event_count{};
    std::size_t max_pending_depth{};
};

struct GlueMoveConfig {
    std::size_t max_pending_operations{32};
};

// Platform-neutral coordinator for one frozen Leader/Follower translation
// session. Native identity resolution, event capture, and window operations are
// deliberately outside this type. The owner must record every returned command
// before executing it and return an exact operation result before processing the
// next Leader geometry receipt.
class GlueMoveCoordinator final {
public:
    explicit GlueMoveCoordinator(GlueMoveConfig config = {}) : config_(config) {
        if (config_.max_pending_operations == 0) {
            throw std::invalid_argument{"pending operation capacity must be positive"};
        }
    }

    [[nodiscard]] GlueMoveState state() const noexcept { return state_; }

    [[nodiscard]] SessionGeneration session_generation() const noexcept {
        return session_generation_;
    }

    [[nodiscard]] std::optional<GlueAbortReason> abort_reason() const noexcept {
        return abort_reason_;
    }

    [[nodiscard]] const GlueMoveStats& stats() const noexcept { return stats_; }

    [[nodiscard]] std::size_t pending_operation_count() const noexcept {
        return pending_.size();
    }

    [[nodiscard]] GlueDecision arm(
        const topology::WindowAdjacencyGraph& graph,
        const model::WindowId& leader,
        const model::WindowId& follower) {
        if (state_ == GlueMoveState::Armed || state_ == GlueMoveState::Active ||
            state_ == GlueMoveState::Completing) {
            return abort(GlueAbortReason::IllegalTransition);
        }
        if (session_generation_ ==
            std::numeric_limits<SessionGeneration>::max()) {
            return abort(GlueAbortReason::GenerationExhausted);
        }

        reset_session();
        ++session_generation_;

        if (leader == follower) {
            return abort(GlueAbortReason::InvalidTopology);
        }
        if (graph.windows().size() != 2U ||
            graph.relations().size() != 1U) {
            return abort(GlueAbortReason::InvalidTopology);
        }

        const topology::WindowGeometry* leader_window = nullptr;
        const topology::WindowGeometry* follower_window = nullptr;
        for (const auto& window : graph.windows()) {
            if (window.id == leader) {
                leader_window = &window;
            }
            if (window.id == follower) {
                follower_window = &window;
            }
        }
        if (leader_window == nullptr || follower_window == nullptr) {
            return abort(GlueAbortReason::InvalidTopology);
        }

        std::vector<model::WindowId> component;
        try {
            component = graph.connected_component(leader);
        } catch (const std::invalid_argument&) {
            return abort(GlueAbortReason::InvalidTopology);
        }
        if (component.size() != 2 ||
            std::find(component.begin(), component.end(), follower) == component.end()) {
            return abort(GlueAbortReason::InvalidTopology);
        }

        translation_session_.emplace(graph, leader);
        const auto initial_plan = translation_session_->plan(leader_window->visible_rect);
        if (!initial_plan.has_value() || initial_plan->size() != 1 ||
            initial_plan->front().id != follower ||
            initial_plan->front().target_visible_rect != follower_window->visible_rect) {
            translation_session_.reset();
            return abort(GlueAbortReason::InvalidTopology);
        }

        follower_id_ = follower;
        initial_leader_visible_rect_ = leader_window->visible_rect;
        current_follower_visible_rect_ = follower_window->visible_rect;
        state_ = GlueMoveState::Armed;
        return decision(GlueDecisionKind::Armed);
    }

    [[nodiscard]] GlueDecision on_event(const GlueEventReceipt& receipt) {
        if (state_ == GlueMoveState::Completed || state_ == GlueMoveState::Aborted) {
            return decision(GlueDecisionKind::TerminalInputIgnored);
        }
        if (state_ == GlueMoveState::Idle) {
            return abort(GlueAbortReason::IllegalTransition);
        }
        if (receipt.session_generation != session_generation_) {
            return abort(GlueAbortReason::SessionGenerationMismatch);
        }
        if (receipt.sequence == 0 || receipt.sequence <= last_event_sequence_) {
            return abort(GlueAbortReason::EventSequenceNotMonotonic);
        }
        last_event_sequence_ = receipt.sequence;

        if (receipt.role == GlueWindowRole::Unrelated) {
            ++stats_.unrelated_event_count;
            return decision(GlueDecisionKind::UnrelatedEventIgnored);
        }
        if (receipt.role == GlueWindowRole::Leader) {
            return on_leader_event(receipt);
        }
        return on_follower_event(receipt);
    }

    [[nodiscard]] GlueDecision
    on_operation_result(const FollowerOperationResult& result) {
        if (state_ == GlueMoveState::Completed || state_ == GlueMoveState::Aborted) {
            return decision(GlueDecisionKind::TerminalInputIgnored);
        }
        if (state_ != GlueMoveState::Active &&
            state_ != GlueMoveState::Completing) {
            return abort(GlueAbortReason::IllegalTransition);
        }
        if (result.session_generation != session_generation_) {
            return abort(GlueAbortReason::SessionGenerationMismatch);
        }

        const auto found = std::find_if(
            pending_.begin(), pending_.end(), [&](const PendingOperation& operation) {
                return operation.operation_generation == result.operation_generation;
            });
        if (found == pending_.end() || !follower_id_.has_value() ||
            result.follower_id != *follower_id_) {
            return abort(GlueAbortReason::OperationGenerationMismatch);
        }

        switch (result.outcome) {
        case FollowerOperationOutcome::NativeApplyFailed:
            return abort(GlueAbortReason::NativeApplyFailed);
        case FollowerOperationOutcome::PostVerificationFailed:
            return abort(GlueAbortReason::PostVerificationFailed);
        case FollowerOperationOutcome::TargetInvalidated:
            return abort(GlueAbortReason::TargetInvalidated);
        case FollowerOperationOutcome::Exact:
            break;
        }

        if (!result.actual_visible_rect.has_value() ||
            *result.actual_visible_rect != found->expected_visible_rect) {
            return abort(GlueAbortReason::PostVerificationFailed);
        }
        if (found->exact_result_recorded) {
            return abort(GlueAbortReason::OperationGenerationMismatch);
        }

        found->exact_result_recorded = true;
        current_follower_visible_rect_ = *result.actual_visible_rect;
        if (!found->feedback_observed) {
            return decision(GlueDecisionKind::OperationRecorded);
        }

        last_acknowledged_feedback_rect_ = found->expected_visible_rect;
        pending_.erase(found);
        return decision(GlueDecisionKind::FeedbackAcknowledged);
    }

    [[nodiscard]] GlueDecision complete(const GlueFinalSnapshot& snapshot) {
        if (state_ == GlueMoveState::Completed || state_ == GlueMoveState::Aborted) {
            return decision(GlueDecisionKind::TerminalInputIgnored);
        }
        if (state_ != GlueMoveState::Completing) {
            return abort(GlueAbortReason::IllegalTransition);
        }
        if (snapshot.session_generation != session_generation_) {
            return abort(GlueAbortReason::SessionGenerationMismatch);
        }
        if (!final_leader_visible_rect_.has_value() ||
            snapshot.leader_visible_rect != *final_leader_visible_rect_) {
            return abort(GlueAbortReason::FinalLeaderMismatch);
        }
        if (std::any_of(pending_.begin(), pending_.end(), [](const auto& operation) {
                return !operation.exact_result_recorded;
            })) {
            return abort(GlueAbortReason::OperationResultMissing);
        }

        const auto change = movement::classify_geometry_change(
            initial_leader_visible_rect_, snapshot.leader_visible_rect);
        if (change.kind == movement::GeometryChangeKind::ResizeOrMixed) {
            return abort(GlueAbortReason::ResizeOrMixed);
        }
        if (change.kind != movement::GeometryChangeKind::Translation) {
            return abort(GlueAbortReason::LeaderDidNotTranslate);
        }

        const auto target = follower_target(snapshot.leader_visible_rect);
        if (!target.has_value() || snapshot.follower_visible_rect != *target ||
            current_follower_visible_rect_ != snapshot.follower_visible_rect) {
            return abort(GlueAbortReason::FinalFollowerMismatch);
        }

        stats_.missing_feedback_count += pending_.size();
        stats_.reconciled_feedback_count += pending_.size();
        pending_.clear();
        state_ = GlueMoveState::Completed;
        return decision(GlueDecisionKind::Completed);
    }

    [[nodiscard]] GlueDecision
    report_queue_overflow(SessionGeneration generation) {
        return abort_external(generation, GlueAbortReason::EventQueueOverflow);
    }

    [[nodiscard]] GlueDecision
    report_target_invalidated(SessionGeneration generation) {
        return abort_external(generation, GlueAbortReason::TargetInvalidated);
    }

    [[nodiscard]] GlueDecision
    report_event_source_failure(SessionGeneration generation) {
        return abort_external(generation, GlueAbortReason::EventSourceFailure);
    }

    [[nodiscard]] GlueDecision report_timeout(SessionGeneration generation) {
        return abort_external(generation, GlueAbortReason::TimedOut);
    }

    [[nodiscard]] GlueDecision
    report_leader_resize_or_mixed(SessionGeneration generation) {
        return abort_external(generation, GlueAbortReason::ResizeOrMixed);
    }

    [[nodiscard]] GlueDecision report_unexpected_follower_feedback(
        SessionGeneration generation) {
        ++stats_.unexpected_feedback_count;
        return abort_external(generation,
                              GlueAbortReason::UnexpectedFollowerGeometry);
    }

private:
    struct PendingOperation {
        OperationGeneration operation_generation{};
        EventSequence source_leader_sequence{};
        geometry::Rect expected_visible_rect;
        bool exact_result_recorded{};
        bool feedback_observed{};
    };

    [[nodiscard]] static GlueDecision decision(GlueDecisionKind kind) {
        return GlueDecision{kind, std::nullopt, std::nullopt};
    }

    [[nodiscard]] GlueDecision abort(GlueAbortReason reason) {
        state_ = GlueMoveState::Aborted;
        abort_reason_ = reason;
        pending_.clear();
        return GlueDecision{GlueDecisionKind::Aborted, std::nullopt, reason};
    }

    [[nodiscard]] GlueDecision abort_external(SessionGeneration generation,
                                              GlueAbortReason reason) {
        if (state_ == GlueMoveState::Completed || state_ == GlueMoveState::Aborted) {
            return decision(GlueDecisionKind::TerminalInputIgnored);
        }
        if (state_ == GlueMoveState::Idle) {
            return abort(GlueAbortReason::IllegalTransition);
        }
        if (generation != session_generation_) {
            return abort(GlueAbortReason::SessionGenerationMismatch);
        }
        return abort(reason);
    }

    void reset_session() {
        state_ = GlueMoveState::Idle;
        abort_reason_.reset();
        stats_ = {};
        last_event_sequence_ = 0;
        translation_session_.reset();
        follower_id_.reset();
        initial_leader_visible_rect_ = {};
        current_follower_visible_rect_ = {};
        final_leader_visible_rect_.reset();
        last_acknowledged_feedback_rect_.reset();
        pending_.clear();
    }

    [[nodiscard]] GlueDecision on_leader_event(const GlueEventReceipt& receipt) {
        switch (receipt.kind) {
        case GlueEventKind::MoveResizeStarted:
            ++stats_.leader_start_count;
            if (state_ != GlueMoveState::Armed) {
                return abort(GlueAbortReason::IllegalTransition);
            }
            state_ = GlueMoveState::Active;
            return decision(GlueDecisionKind::Activated);

        case GlueEventKind::GeometryChanged:
            ++stats_.leader_location_count;
            if (state_ != GlueMoveState::Active) {
                return abort(GlueAbortReason::IllegalTransition);
            }
            if (!receipt.visible_rect.has_value()) {
                return abort(GlueAbortReason::InvalidEvent);
            }
            return plan_follower(receipt.sequence, *receipt.visible_rect);

        case GlueEventKind::MoveResizeEnded: {
            ++stats_.leader_end_count;
            if (state_ != GlueMoveState::Active) {
                return abort(GlueAbortReason::IllegalTransition);
            }
            if (!receipt.visible_rect.has_value()) {
                return abort(GlueAbortReason::InvalidEvent);
            }
            const auto change = movement::classify_geometry_change(
                initial_leader_visible_rect_, *receipt.visible_rect);
            if (change.kind == movement::GeometryChangeKind::ResizeOrMixed) {
                return abort(GlueAbortReason::ResizeOrMixed);
            }
            if (change.kind != movement::GeometryChangeKind::Translation) {
                return abort(GlueAbortReason::LeaderDidNotTranslate);
            }
            final_leader_visible_rect_ = *receipt.visible_rect;
            state_ = GlueMoveState::Completing;
            return decision(GlueDecisionKind::Completing);
        }
        }

        return abort(GlueAbortReason::InvalidEvent);
    }

    [[nodiscard]] GlueDecision on_follower_event(const GlueEventReceipt& receipt) {
        if (receipt.kind != GlueEventKind::GeometryChanged) {
            return abort(GlueAbortReason::UnexpectedFollowerLifecycle);
        }
        if (state_ != GlueMoveState::Active &&
            state_ != GlueMoveState::Completing) {
            return abort(GlueAbortReason::UnexpectedFollowerGeometry);
        }
        if (!receipt.visible_rect.has_value()) {
            return abort(GlueAbortReason::InvalidEvent);
        }

        ++stats_.follower_feedback_count;
        std::optional<std::size_t> match;
        for (std::size_t index = 0; index < pending_.size(); ++index) {
            if (pending_[index].expected_visible_rect == *receipt.visible_rect) {
                if (match.has_value()) {
                    ++stats_.unexpected_feedback_count;
                    return abort(GlueAbortReason::AmbiguousFollowerFeedback);
                }
                match = index;
            }
        }

        if (!match.has_value()) {
            if (last_acknowledged_feedback_rect_.has_value() &&
                *last_acknowledged_feedback_rect_ == *receipt.visible_rect) {
                ++stats_.suppressed_feedback_count;
                ++stats_.duplicate_feedback_count;
                return decision(GlueDecisionKind::DuplicateFeedbackSuppressed);
            }
            ++stats_.unexpected_feedback_count;
            return abort(GlueAbortReason::UnexpectedFollowerGeometry);
        }

        auto& operation = pending_[*match];
        ++stats_.suppressed_feedback_count;
        if (operation.feedback_observed) {
            ++stats_.duplicate_feedback_count;
            return decision(GlueDecisionKind::DuplicateFeedbackSuppressed);
        }
        operation.feedback_observed = true;
        if (!operation.exact_result_recorded) {
            return decision(GlueDecisionKind::FeedbackObservedPendingResult);
        }

        last_acknowledged_feedback_rect_ = operation.expected_visible_rect;
        pending_.erase(pending_.begin() + static_cast<std::ptrdiff_t>(*match));
        return decision(GlueDecisionKind::FeedbackAcknowledged);
    }

    [[nodiscard]] GlueDecision plan_follower(EventSequence sequence,
                                             const geometry::Rect& leader_rect) {
        if (std::any_of(pending_.begin(), pending_.end(), [](const auto& operation) {
                return !operation.exact_result_recorded;
            })) {
            return abort(GlueAbortReason::OperationResultMissing);
        }

        const auto target = follower_target(leader_rect);
        if (!target.has_value()) {
            return abort(GlueAbortReason::ResizeOrMixed);
        }
        if (*target == current_follower_visible_rect_) {
            ++stats_.follower_noop_count;
            return decision(GlueDecisionKind::LeaderNoOp);
        }
        if (std::any_of(pending_.begin(), pending_.end(), [&](const auto& operation) {
                return operation.expected_visible_rect == *target;
            })) {
            ++stats_.unexpected_feedback_count;
            return abort(GlueAbortReason::AmbiguousFollowerFeedback);
        }
        if (pending_.size() >= config_.max_pending_operations) {
            return abort(GlueAbortReason::PendingLedgerOverflow);
        }
        if (operation_generation_ ==
            std::numeric_limits<OperationGeneration>::max()) {
            return abort(GlueAbortReason::GenerationExhausted);
        }

        ++operation_generation_;
        pending_.push_back(PendingOperation{operation_generation_,
                                            sequence,
                                            *target,
                                            false,
                                            false});
        ++stats_.follower_operation_count;
        stats_.max_pending_depth = std::max(stats_.max_pending_depth, pending_.size());

        return GlueDecision{
            GlueDecisionKind::FollowerMoveRequested,
            FollowerMoveCommand{session_generation_,
                                operation_generation_,
                                sequence,
                                *follower_id_,
                                *target},
            std::nullopt};
    }

    [[nodiscard]] std::optional<geometry::Rect>
    follower_target(const geometry::Rect& leader_rect) const {
        if (!translation_session_.has_value() || !follower_id_.has_value()) {
            return std::nullopt;
        }
        const auto plan = translation_session_->plan(leader_rect);
        if (!plan.has_value() || plan->size() != 1 ||
            plan->front().id != *follower_id_) {
            return std::nullopt;
        }
        return plan->front().target_visible_rect;
    }

    GlueMoveConfig config_;
    GlueMoveState state_{GlueMoveState::Idle};
    SessionGeneration session_generation_{};
    OperationGeneration operation_generation_{};
    EventSequence last_event_sequence_{};
    std::optional<GlueAbortReason> abort_reason_;
    GlueMoveStats stats_;
    std::optional<movement::TranslationSession> translation_session_;
    std::optional<model::WindowId> follower_id_;
    geometry::Rect initial_leader_visible_rect_;
    geometry::Rect current_follower_visible_rect_;
    std::optional<geometry::Rect> final_leader_visible_rect_;
    std::optional<geometry::Rect> last_acknowledged_feedback_rect_;
    std::vector<PendingOperation> pending_;
};

} // namespace panebind::core::behavior
