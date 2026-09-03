#include "core/behavior/glue_move_coordinator.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace behavior = panebind::core::behavior;
namespace geometry = panebind::core::geometry;
namespace model = panebind::core::model;
namespace movement = panebind::core::movement;
namespace topology = panebind::core::topology;

namespace {

constexpr geometry::Rect initial_leader{0, 0, 100, 100};
constexpr geometry::Rect initial_follower{100, 0, 200, 100};

int failures = 0;

void expect(bool condition, std::string_view description) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << description << '\n';
    }
}

template <typename Exception, typename Action>
void expect_throws(Action action, std::string_view description) {
    try {
        action();
        expect(false, description);
    } catch (const Exception&) {
        expect(true, description);
    } catch (...) {
        expect(false, description);
    }
}

[[nodiscard]] topology::WindowGeometry window(std::string id,
                                              geometry::Rect rect) {
    return {model::WindowId{std::move(id)}, rect};
}

[[nodiscard]] topology::WindowAdjacencyGraph pair_graph() {
    const std::array windows{
        window("leader", initial_leader),
        window("follower", initial_follower),
    };
    return topology::WindowAdjacencyGraph::build(
        std::span<const topology::WindowGeometry>{windows}, {});
}

[[nodiscard]] behavior::GlueEventReceipt event(
    behavior::SessionGeneration generation,
    behavior::EventSequence sequence,
    behavior::GlueWindowRole role,
    behavior::GlueEventKind kind,
    std::optional<geometry::Rect> rect = std::nullopt) {
    return {generation, sequence, role, kind, rect};
}

behavior::GlueDecision arm(behavior::GlueMoveCoordinator& coordinator) {
    const auto graph = pair_graph();
    return coordinator.arm(
        graph, model::WindowId{"leader"}, model::WindowId{"follower"});
}

behavior::GlueDecision start(behavior::GlueMoveCoordinator& coordinator,
                             behavior::EventSequence sequence = 1) {
    return coordinator.on_event(event(coordinator.session_generation(),
                                      sequence,
                                      behavior::GlueWindowRole::Leader,
                                      behavior::GlueEventKind::MoveResizeStarted));
}

void expect_abort(const behavior::GlueMoveCoordinator& coordinator,
                  const behavior::GlueDecision& decision,
                  behavior::GlueAbortReason reason,
                  std::string_view description) {
    expect(decision.kind == behavior::GlueDecisionKind::Aborted &&
               decision.abort_reason == reason &&
               coordinator.state() == behavior::GlueMoveState::Aborted &&
               coordinator.abort_reason() == reason,
           description);
}

[[nodiscard]] behavior::FollowerMoveCommand require_command(
    const behavior::GlueDecision& decision,
    std::string_view description) {
    expect(decision.kind == behavior::GlueDecisionKind::FollowerMoveRequested &&
               decision.command.has_value() && !decision.abort_reason.has_value(),
           description);
    if (!decision.command.has_value()) {
        return {0, 0, 0, model::WindowId{"missing"}, {}};
    }
    return *decision.command;
}

behavior::GlueDecision record_exact(
    behavior::GlueMoveCoordinator& coordinator,
    const behavior::FollowerMoveCommand& command) {
    return coordinator.on_operation_result(
        {command.session_generation,
         command.operation_generation,
         command.follower_id,
         behavior::FollowerOperationOutcome::Exact,
         command.target_visible_rect});
}

void test_happy_path_and_feedback_semantics() {
    behavior::GlueMoveCoordinator coordinator;
    expect(arm(coordinator).kind == behavior::GlueDecisionKind::Armed &&
               coordinator.state() == behavior::GlueMoveState::Armed &&
               coordinator.session_generation() == 1,
           "arm freezes a two-window topology and allocates generation one");
    const auto generation = coordinator.session_generation();

    expect(start(coordinator).kind == behavior::GlueDecisionKind::Activated &&
               coordinator.state() == behavior::GlueMoveState::Active,
           "Leader START activates an armed session");
    expect(coordinator
                   .on_event(event(generation,
                                   2,
                                   behavior::GlueWindowRole::Unrelated,
                                   behavior::GlueEventKind::GeometryChanged,
                                   geometry::Rect{700, 700, 800, 800}))
                   .kind == behavior::GlueDecisionKind::UnrelatedEventIgnored,
           "unrelated-window interleaving is ignored");

    constexpr geometry::Rect moved_leader{25, -10, 125, 90};
    const auto command = require_command(
        coordinator.on_event(event(generation,
                                   3,
                                   behavior::GlueWindowRole::Leader,
                                   behavior::GlueEventKind::GeometryChanged,
                                   moved_leader)),
        "Leader LOCATION requests one follower translation");
    expect(command.session_generation == generation &&
               command.operation_generation != 0 &&
               command.source_leader_sequence == 3 &&
               command.follower_id == model::WindowId{"follower"} &&
               command.target_visible_rect == geometry::Rect{125, -10, 225, 90},
           "command binds session, operation, source sequence, role, and exact target");

    expect(record_exact(coordinator, command).kind ==
               behavior::GlueDecisionKind::OperationRecorded,
           "exact native/post-verification receipt records operation success");
    const auto feedback = coordinator.on_event(
        event(generation,
              4,
              behavior::GlueWindowRole::Follower,
              behavior::GlueEventKind::GeometryChanged,
              command.target_visible_rect));
    expect(feedback.kind == behavior::GlueDecisionKind::FeedbackAcknowledged &&
               !feedback.command.has_value() && coordinator.pending_operation_count() == 0,
           "exact follower feedback acknowledges without recursively issuing a command");

    const auto duplicate = coordinator.on_event(
        event(generation,
              5,
              behavior::GlueWindowRole::Follower,
              behavior::GlueEventKind::GeometryChanged,
              command.target_visible_rect));
    expect(duplicate.kind ==
                   behavior::GlueDecisionKind::DuplicateFeedbackSuppressed &&
               !duplicate.command.has_value(),
           "repeated current feedback is suppressed as a duplicate");

    expect(coordinator
                   .on_event(event(generation,
                                   6,
                                   behavior::GlueWindowRole::Leader,
                                   behavior::GlueEventKind::MoveResizeEnded,
                                   moved_leader))
                   .kind == behavior::GlueDecisionKind::Completing &&
               coordinator.state() == behavior::GlueMoveState::Completing,
           "Leader END enters Completing without issuing another move");
    expect(coordinator
                   .complete({generation,
                              moved_leader,
                              command.target_visible_rect})
                   .kind == behavior::GlueDecisionKind::Completed &&
               coordinator.state() == behavior::GlueMoveState::Completed,
           "exact final R1-A target completes the session");

    const auto stats = coordinator.stats();
    expect(stats.leader_start_count == 1 && stats.leader_location_count == 1 &&
               stats.leader_end_count == 1 && stats.follower_operation_count == 1 &&
               stats.follower_feedback_count == 2 &&
               stats.suppressed_feedback_count == 2 &&
               stats.duplicate_feedback_count == 1 &&
               stats.unrelated_event_count == 1 && stats.max_pending_depth == 1,
           "happy path exposes deterministic lifecycle and suppression counters");

    expect(coordinator
                   .on_event(event(generation,
                                   7,
                                   behavior::GlueWindowRole::Follower,
                                   behavior::GlueEventKind::GeometryChanged,
                                   command.target_visible_rect))
                   .kind == behavior::GlueDecisionKind::TerminalInputIgnored,
           "late terminal feedback cannot restart behavior");
}

void test_illegal_transitions_and_generation_guards() {
    {
        behavior::GlueMoveCoordinator coordinator;
        const auto decision = coordinator.on_event(
            event(0,
                  1,
                  behavior::GlueWindowRole::Leader,
                  behavior::GlueEventKind::MoveResizeStarted));
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::IllegalTransition,
                     "START while Idle is rejected");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        const auto decision = coordinator.on_event(
            event(coordinator.session_generation(),
                  1,
                  behavior::GlueWindowRole::Leader,
                  behavior::GlueEventKind::GeometryChanged,
                  initial_leader));
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::IllegalTransition,
                     "LOCATION before START is rejected");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        const auto decision = coordinator.on_event(
            event(coordinator.session_generation(),
                  1,
                  behavior::GlueWindowRole::Leader,
                  behavior::GlueEventKind::MoveResizeEnded,
                  initial_leader));
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::IllegalTransition,
                     "END before START is rejected");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        start(coordinator);
        const auto decision = start(coordinator, 2);
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::IllegalTransition,
                     "a second START while Active is rejected");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        start(coordinator);
        const auto decision = coordinator.on_event(
            event(coordinator.session_generation(),
                  2,
                  behavior::GlueWindowRole::Follower,
                  behavior::GlueEventKind::MoveResizeStarted));
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::UnexpectedFollowerLifecycle,
                     "Follower START aborts an active session");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        const auto decision = coordinator.on_event(
            event(coordinator.session_generation() + 1,
                  1,
                  behavior::GlueWindowRole::Leader,
                  behavior::GlueEventKind::MoveResizeStarted));
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::SessionGenerationMismatch,
                     "wrong session generation fails closed");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        start(coordinator, 2);
        const auto decision = coordinator.on_event(
            event(coordinator.session_generation(),
                  2,
                  behavior::GlueWindowRole::Unrelated,
                  behavior::GlueEventKind::GeometryChanged));
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::EventSequenceNotMonotonic,
                     "duplicate event sequence fails closed even for unrelated input");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        start(coordinator);
        const auto decision = coordinator.on_event(
            event(coordinator.session_generation(),
                  2,
                  behavior::GlueWindowRole::Leader,
                  behavior::GlueEventKind::GeometryChanged));
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::InvalidEvent,
                     "geometry event without geometry is rejected");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        start(coordinator);
        const auto decision = arm(coordinator);
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::IllegalTransition,
                     "arming over an active session is rejected");
    }
}

void test_topology_and_translation_contracts() {
    {
        behavior::GlueMoveCoordinator coordinator;
        const std::array windows{
            window("leader", {0, 0, 100, 100}),
            window("follower", {100, 0, 200, 100}),
            window("third", {200, 0, 300, 100}),
        };
        const auto graph = topology::WindowAdjacencyGraph::build(
            std::span<const topology::WindowGeometry>{windows}, {});
        const auto decision = coordinator.arm(
            graph, model::WindowId{"leader"}, model::WindowId{"follower"});
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::InvalidTopology,
                     "R1-C2B rejects a connected component with a third window");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        const std::array windows{
            window("leader", {0, 0, 100, 100}),
            window("follower", {100, 0, 200, 100}),
            window("third", {500, 500, 600, 600}),
        };
        const auto graph = topology::WindowAdjacencyGraph::build(
            std::span<const topology::WindowGeometry>{windows}, {});
        const auto decision = coordinator.arm(
            graph, model::WindowId{"leader"}, model::WindowId{"follower"});
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::InvalidTopology,
                     "R1-C2B rejects even an isolated third topology node");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        const std::array windows{
            window("leader", {0, 0, 100, 100}),
            window("follower", {300, 0, 400, 100}),
        };
        const auto graph = topology::WindowAdjacencyGraph::build(
            std::span<const topology::WindowGeometry>{windows}, {});
        const auto decision = coordinator.arm(
            graph, model::WindowId{"leader"}, model::WindowId{"follower"});
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::InvalidTopology,
                     "non-adjacent Leader/Follower cannot arm");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        const auto graph = pair_graph();
        const auto decision = coordinator.arm(
            graph, model::WindowId{"leader"}, model::WindowId{"leader"});
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::InvalidTopology,
                     "Leader and Follower must be distinct");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        start(coordinator);
        expect(coordinator
                       .on_event(event(coordinator.session_generation(),
                                       2,
                                       behavior::GlueWindowRole::Leader,
                                       behavior::GlueEventKind::GeometryChanged,
                                       initial_leader))
                       .kind == behavior::GlueDecisionKind::LeaderNoOp,
               "unchanged Leader geometry skips follower apply");

        constexpr geometry::Rect moved{10, 15, 110, 115};
        const auto command = require_command(
            coordinator.on_event(event(coordinator.session_generation(),
                                       3,
                                       behavior::GlueWindowRole::Leader,
                                       behavior::GlueEventKind::GeometryChanged,
                                       moved)),
            "translation produces a command");
        record_exact(coordinator, command);
        static_cast<void>(coordinator.on_event(
            event(coordinator.session_generation(),
                  4,
                  behavior::GlueWindowRole::Follower,
                  behavior::GlueEventKind::GeometryChanged,
                  command.target_visible_rect)));
        expect(coordinator
                       .on_event(event(coordinator.session_generation(),
                                       5,
                                       behavior::GlueWindowRole::Leader,
                                       behavior::GlueEventKind::GeometryChanged,
                                       moved))
                       .kind == behavior::GlueDecisionKind::LeaderNoOp,
               "duplicate Leader geometry does not create a second operation");

        const auto decision = coordinator.on_event(
            event(coordinator.session_generation(),
                  6,
                  behavior::GlueWindowRole::Leader,
                  behavior::GlueEventKind::GeometryChanged,
                  geometry::Rect{10, 15, 120, 115}));
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::ResizeOrMixed,
                     "ResizeOrMixed Leader geometry aborts");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        start(coordinator);
        const auto decision = coordinator.on_event(
            event(coordinator.session_generation(),
                  2,
                  behavior::GlueWindowRole::Leader,
                  behavior::GlueEventKind::MoveResizeEnded,
                  initial_leader));
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::LeaderDidNotTranslate,
                     "END with no Leader translation aborts");
    }
}

behavior::FollowerMoveCommand prepare_one_command(
    behavior::GlueMoveCoordinator& coordinator) {
    arm(coordinator);
    start(coordinator);
    return require_command(
        coordinator.on_event(event(coordinator.session_generation(),
                                   2,
                                   behavior::GlueWindowRole::Leader,
                                   behavior::GlueEventKind::GeometryChanged,
                                   geometry::Rect{20, 10, 120, 110})),
        "test setup creates one command");
}

void test_operation_and_external_failure_paths() {
    {
        behavior::GlueMoveCoordinator coordinator;
        const auto command = prepare_one_command(coordinator);
        const auto decision = coordinator.on_operation_result(
            {command.session_generation,
             command.operation_generation,
             command.follower_id,
             behavior::FollowerOperationOutcome::NativeApplyFailed,
             std::nullopt});
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::NativeApplyFailed,
                     "native follower failure aborts immediately");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        const auto command = prepare_one_command(coordinator);
        const auto decision = coordinator.on_operation_result(
            {command.session_generation,
             command.operation_generation,
             command.follower_id,
             behavior::FollowerOperationOutcome::PostVerificationFailed,
             command.target_visible_rect});
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::PostVerificationFailed,
                     "reported post-verification failure aborts immediately");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        const auto command = prepare_one_command(coordinator);
        const auto decision = coordinator.on_operation_result(
            {command.session_generation,
             command.operation_generation,
             command.follower_id,
             behavior::FollowerOperationOutcome::Exact,
             geometry::Rect{121, 10, 221, 110}});
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::PostVerificationFailed,
                     "an inconsistent Exact receipt fails closed");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        const auto command = prepare_one_command(coordinator);
        const auto decision = coordinator.on_operation_result(
            {command.session_generation,
             command.operation_generation + 1,
             command.follower_id,
             behavior::FollowerOperationOutcome::Exact,
             command.target_visible_rect});
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::OperationGenerationMismatch,
                     "unknown operation generation aborts");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        prepare_one_command(coordinator);
        const auto decision = coordinator.on_event(
            event(coordinator.session_generation(),
                  3,
                  behavior::GlueWindowRole::Leader,
                  behavior::GlueEventKind::GeometryChanged,
                  geometry::Rect{30, 20, 130, 120}));
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::OperationResultMissing,
                     "a new Leader plan cannot overtake an unreported operation");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        start(coordinator);
        const auto decision = coordinator.report_queue_overflow(
            coordinator.session_generation());
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::EventQueueOverflow,
                     "event receipt queue overflow aborts");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        start(coordinator);
        const auto decision = coordinator.report_target_invalidated(
            coordinator.session_generation());
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::TargetInvalidated,
                     "target invalidation aborts");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        const auto decision = coordinator.report_event_source_failure(
            coordinator.session_generation());
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::EventSourceFailure,
                     "event-source lifecycle failure aborts armed behavior");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        const auto decision = coordinator.report_timeout(
            coordinator.session_generation());
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::TimedOut,
                     "bounded UAT timeout aborts without polling");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        const auto decision = coordinator.report_leader_resize_or_mixed(
            coordinator.session_generation());
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::ResizeOrMixed,
                     "platform START preflight reports queued resize as terminal");
    }
}

void test_feedback_ordering_capacity_and_reconciliation() {
    {
        behavior::GlueMoveCoordinator coordinator;
        const auto command = prepare_one_command(coordinator);
        const auto observed = coordinator.on_event(
            event(coordinator.session_generation(),
                  3,
                  behavior::GlueWindowRole::Follower,
                  behavior::GlueEventKind::GeometryChanged,
                  command.target_visible_rect));
        expect(observed.kind ==
                   behavior::GlueDecisionKind::FeedbackObservedPendingResult &&
                   coordinator.pending_operation_count() == 1,
               "feedback before operation result remains provisional");
        expect(record_exact(coordinator, command).kind ==
                   behavior::GlueDecisionKind::FeedbackAcknowledged &&
                   coordinator.pending_operation_count() == 0,
               "later exact operation result completes provisional acknowledgement");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        prepare_one_command(coordinator);
        const auto decision = coordinator.on_event(
            event(coordinator.session_generation(),
                  3,
                  behavior::GlueWindowRole::Follower,
                  behavior::GlueEventKind::GeometryChanged,
                  geometry::Rect{999, 999, 1099, 1099}));
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::UnexpectedFollowerGeometry,
                     "unmatched follower geometry aborts");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        start(coordinator);
        const auto first = require_command(
            coordinator.on_event(event(coordinator.session_generation(),
                                       2,
                                       behavior::GlueWindowRole::Leader,
                                       behavior::GlueEventKind::GeometryChanged,
                                       geometry::Rect{10, 0, 110, 100})),
            "first delayed-feedback operation is issued");
        record_exact(coordinator, first);
        const auto second = require_command(
            coordinator.on_event(event(coordinator.session_generation(),
                                       3,
                                       behavior::GlueWindowRole::Leader,
                                       behavior::GlueEventKind::GeometryChanged,
                                       geometry::Rect{20, 0, 120, 100})),
            "second delayed-feedback operation is issued");
        record_exact(coordinator, second);

        expect(coordinator
                       .on_event(event(coordinator.session_generation(),
                                       4,
                                       behavior::GlueWindowRole::Follower,
                                       behavior::GlueEventKind::GeometryChanged,
                                       second.target_visible_rect))
                       .kind == behavior::GlueDecisionKind::FeedbackAcknowledged,
               "newer exact feedback can arrive first");
        expect(coordinator
                       .on_event(event(coordinator.session_generation(),
                                       5,
                                       behavior::GlueWindowRole::Follower,
                                       behavior::GlueEventKind::GeometryChanged,
                                       first.target_visible_rect))
                       .kind == behavior::GlueDecisionKind::FeedbackAcknowledged &&
                   coordinator.pending_operation_count() == 0,
               "older delayed feedback remains attributable by expected geometry");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        arm(coordinator);
        start(coordinator);
        const auto first = require_command(
            coordinator.on_event(event(coordinator.session_generation(),
                                       2,
                                       behavior::GlueWindowRole::Leader,
                                       behavior::GlueEventKind::GeometryChanged,
                                       geometry::Rect{10, 0, 110, 100})),
            "prior operation for delayed same-batch feedback is issued");
        record_exact(coordinator, first);
        const auto second = require_command(
            coordinator.on_event(event(coordinator.session_generation(),
                                       3,
                                       behavior::GlueWindowRole::Leader,
                                       behavior::GlueEventKind::GeometryChanged,
                                       geometry::Rect{20, 0, 120, 100})),
            "next Leader receipt may issue before prior feedback is delivered");
        record_exact(coordinator, second);
        expect(coordinator
                       .on_event(event(coordinator.session_generation(),
                                       4,
                                       behavior::GlueWindowRole::Follower,
                                       behavior::GlueEventKind::GeometryChanged,
                                       first.target_visible_rect))
                       .kind == behavior::GlueDecisionKind::FeedbackAcknowledged,
               "delayed prior feedback remains legal after the next exact apply");
        expect(coordinator
                       .on_event(event(coordinator.session_generation(),
                                       5,
                                       behavior::GlueWindowRole::Follower,
                                       behavior::GlueEventKind::GeometryChanged,
                                       second.target_visible_rect))
                       .kind == behavior::GlueDecisionKind::FeedbackAcknowledged &&
                   coordinator.pending_operation_count() == 0,
               "current feedback still acknowledges after delayed prior feedback");
    }
    {
        behavior::GlueMoveCoordinator coordinator{{2}};
        arm(coordinator);
        start(coordinator);
        const auto first = require_command(
            coordinator.on_event(event(coordinator.session_generation(),
                                       2,
                                       behavior::GlueWindowRole::Leader,
                                       behavior::GlueEventKind::GeometryChanged,
                                       geometry::Rect{10, 0, 110, 100})),
            "capacity setup operation one");
        record_exact(coordinator, first);
        const auto second = require_command(
            coordinator.on_event(event(coordinator.session_generation(),
                                       3,
                                       behavior::GlueWindowRole::Leader,
                                       behavior::GlueEventKind::GeometryChanged,
                                       geometry::Rect{20, 0, 120, 100})),
            "capacity setup operation two");
        record_exact(coordinator, second);
        const auto decision = coordinator.on_event(
            event(coordinator.session_generation(),
                  4,
                  behavior::GlueWindowRole::Leader,
                  behavior::GlueEventKind::GeometryChanged,
                  geometry::Rect{30, 0, 130, 100}));
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::PendingLedgerOverflow,
                     "bounded pending ledger fails closed instead of dropping evidence");
        expect(coordinator.stats().max_pending_depth == 2,
               "pending depth never exceeds configured capacity");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        const auto command = prepare_one_command(coordinator);
        record_exact(coordinator, command);
        constexpr geometry::Rect final_leader{20, 10, 120, 110};
        expect(coordinator
                       .on_event(event(coordinator.session_generation(),
                                       3,
                                       behavior::GlueWindowRole::Leader,
                                       behavior::GlueEventKind::MoveResizeEnded,
                                       final_leader))
                       .kind == behavior::GlueDecisionKind::Completing,
               "END permits an exact operation with missing feedback");
        expect(coordinator
                       .complete({coordinator.session_generation(),
                                  final_leader,
                                  command.target_visible_rect})
                       .kind == behavior::GlueDecisionKind::Completed,
               "exact receipt and final snapshot reconcile missing feedback");
        expect(coordinator.stats().missing_feedback_count == 1 &&
                   coordinator.stats().reconciled_feedback_count == 1,
               "reconciliation is distinguished from event acknowledgement");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        const auto command = prepare_one_command(coordinator);
        record_exact(coordinator, command);
        constexpr geometry::Rect final_leader{20, 10, 120, 110};
        static_cast<void>(coordinator.on_event(
            event(coordinator.session_generation(),
                  3,
                  behavior::GlueWindowRole::Leader,
                  behavior::GlueEventKind::MoveResizeEnded,
                  final_leader)));
        const auto decision = coordinator.complete(
            {coordinator.session_generation(),
             final_leader,
             geometry::Rect{119, 10, 219, 110}});
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::FinalFollowerMismatch,
                     "END final follower mismatch aborts");
    }
    {
        behavior::GlueMoveCoordinator coordinator;
        const auto command = prepare_one_command(coordinator);
        constexpr geometry::Rect final_leader{20, 10, 120, 110};
        static_cast<void>(coordinator.on_event(
            event(coordinator.session_generation(),
                  3,
                  behavior::GlueWindowRole::Leader,
                  behavior::GlueEventKind::MoveResizeEnded,
                  final_leader)));
        const auto decision = coordinator.complete(
            {coordinator.session_generation(),
             final_leader,
             command.target_visible_rect});
        expect_abort(coordinator,
                     decision,
                     behavior::GlueAbortReason::OperationResultMissing,
                     "final snapshot cannot replace a missing native operation result");
    }
}

void test_new_session_generation_clears_state() {
    behavior::GlueMoveCoordinator coordinator;
    const auto command = prepare_one_command(coordinator);
    const auto first_generation = coordinator.session_generation();
    record_exact(coordinator, command);
    static_cast<void>(coordinator.on_event(
        event(first_generation,
              3,
              behavior::GlueWindowRole::Follower,
              behavior::GlueEventKind::GeometryChanged,
              command.target_visible_rect)));
    constexpr geometry::Rect moved_leader{20, 10, 120, 110};
    static_cast<void>(coordinator.on_event(
        event(first_generation,
              4,
              behavior::GlueWindowRole::Leader,
              behavior::GlueEventKind::MoveResizeEnded,
              moved_leader)));
    static_cast<void>(coordinator.complete(
        {first_generation, moved_leader, command.target_visible_rect}));

    expect(arm(coordinator).kind == behavior::GlueDecisionKind::Armed &&
               coordinator.session_generation() == first_generation + 1 &&
               coordinator.pending_operation_count() == 0 &&
               coordinator.stats().leader_start_count == 0 &&
               coordinator.stats().follower_operation_count == 0,
           "new generation starts with fresh snapshots, stats, and pending ledger");

    const auto stale = coordinator.on_event(
        event(first_generation,
              5,
              behavior::GlueWindowRole::Follower,
              behavior::GlueEventKind::GeometryChanged,
              command.target_visible_rect));
    expect_abort(coordinator,
                 stale,
                 behavior::GlueAbortReason::SessionGenerationMismatch,
                 "old-generation late feedback aborts a newly armed session");
}

void test_generated_stress_is_initial_relative_and_non_recursive() {
    behavior::GlueMoveCoordinator coordinator{{8}};
    arm(coordinator);
    start(coordinator);

    behavior::EventSequence sequence = 1;
    behavior::OperationGeneration previous_operation = 0;
    geometry::Rect current_leader = initial_leader;
    geometry::Rect current_follower = initial_follower;
    std::size_t issued = 0;

    for (std::int64_t index = 1; index <= 160; ++index) {
        auto dx = (index * 37) % 181 - 90;
        auto dy = (index * 53) % 151 - 75;
        if (dx == 0 && dy == 0) {
            dx = 1;
        }
        const movement::TranslationDelta delta{dx, dy};
        current_leader = movement::translate_rect(initial_leader, delta);
        const auto expected_follower =
            movement::translate_rect(initial_follower, delta);

        const auto command = require_command(
            coordinator.on_event(event(coordinator.session_generation(),
                                       ++sequence,
                                       behavior::GlueWindowRole::Leader,
                                       behavior::GlueEventKind::GeometryChanged,
                                       current_leader)),
            "generated Leader translation requests a follower operation");
        expect(command.target_visible_rect == expected_follower,
               "every generated target is initial-relative and drift-free");
        expect(command.operation_generation > previous_operation,
               "operation generations are strictly monotonic");
        previous_operation = command.operation_generation;
        ++issued;

        expect(record_exact(coordinator, command).kind ==
                   behavior::GlueDecisionKind::OperationRecorded,
               "generated operation records an exact result");
        const auto feedback = coordinator.on_event(
            event(coordinator.session_generation(),
                  ++sequence,
                  behavior::GlueWindowRole::Follower,
                  behavior::GlueEventKind::GeometryChanged,
                  expected_follower));
        expect(feedback.kind == behavior::GlueDecisionKind::FeedbackAcknowledged &&
                   !feedback.command.has_value(),
               "generated follower feedback never recursively creates an operation");
        current_follower = expected_follower;

        if (index % 13 == 0) {
            const auto duplicate = coordinator.on_event(
                event(coordinator.session_generation(),
                      ++sequence,
                      behavior::GlueWindowRole::Follower,
                      behavior::GlueEventKind::GeometryChanged,
                      current_follower));
            expect(duplicate.kind ==
                       behavior::GlueDecisionKind::DuplicateFeedbackSuppressed &&
                       !duplicate.command.has_value(),
                   "stress duplicate feedback is suppressed without recursion");
        }
        if (index % 17 == 0) {
            expect(coordinator
                           .on_event(event(coordinator.session_generation(),
                                           ++sequence,
                                           behavior::GlueWindowRole::Unrelated,
                                           behavior::GlueEventKind::GeometryChanged,
                                           geometry::Rect{500, 500, 600, 600}))
                           .kind == behavior::GlueDecisionKind::UnrelatedEventIgnored,
                   "stress permits unrelated event insertion");
        }
        if (index % 19 == 0) {
            expect(coordinator
                           .on_event(event(coordinator.session_generation(),
                                           ++sequence,
                                           behavior::GlueWindowRole::Leader,
                                           behavior::GlueEventKind::GeometryChanged,
                                           current_leader))
                           .kind == behavior::GlueDecisionKind::LeaderNoOp,
                   "stress skips duplicated Leader geometry");
        }
    }

    constexpr movement::TranslationDelta final_delta{37, -29};
    current_leader = movement::translate_rect(initial_leader, final_delta);
    const auto final_expected = movement::translate_rect(initial_follower, final_delta);
    const auto final_command = require_command(
        coordinator.on_event(event(coordinator.session_generation(),
                                   ++sequence,
                                   behavior::GlueWindowRole::Leader,
                                   behavior::GlueEventKind::GeometryChanged,
                                   current_leader)),
        "stress final target is issued");
    record_exact(coordinator, final_command);
    static_cast<void>(coordinator.on_event(
        event(coordinator.session_generation(),
              ++sequence,
              behavior::GlueWindowRole::Follower,
              behavior::GlueEventKind::GeometryChanged,
              final_expected)));
    ++issued;

    expect(coordinator
                   .on_event(event(coordinator.session_generation(),
                                   ++sequence,
                                   behavior::GlueWindowRole::Leader,
                                   behavior::GlueEventKind::MoveResizeEnded,
                                   current_leader))
                   .kind == behavior::GlueDecisionKind::Completing,
           "stress END reaches Completing");
    expect(coordinator
                   .complete({coordinator.session_generation(),
                              current_leader,
                              final_expected})
                   .kind == behavior::GlueDecisionKind::Completed,
           "stress completes at the exact final target");
    expect(issued >= 150 && coordinator.stats().follower_operation_count == issued,
           "stress executes more than 100 follower operations");
    expect(coordinator.stats().max_pending_depth == 1 &&
               coordinator.pending_operation_count() == 0 &&
               coordinator.stats().unexpected_feedback_count == 0,
           "stress remains bounded with no recursion or unexpected feedback");
}

[[nodiscard]] geometry::Rect run_translation_path(
    std::span<const movement::TranslationDelta> path) {
    behavior::GlueMoveCoordinator coordinator;
    arm(coordinator);
    start(coordinator);
    behavior::EventSequence sequence = 1;
    geometry::Rect follower = initial_follower;
    geometry::Rect leader = initial_leader;

    for (const auto& delta : path) {
        leader = movement::translate_rect(initial_leader, delta);
        const auto command = require_command(
            coordinator.on_event(event(coordinator.session_generation(),
                                       ++sequence,
                                       behavior::GlueWindowRole::Leader,
                                       behavior::GlueEventKind::GeometryChanged,
                                       leader)),
            "path operation is issued");
        record_exact(coordinator, command);
        static_cast<void>(coordinator.on_event(
            event(coordinator.session_generation(),
                  ++sequence,
                  behavior::GlueWindowRole::Follower,
                  behavior::GlueEventKind::GeometryChanged,
                  command.target_visible_rect)));
        follower = command.target_visible_rect;
    }

    static_cast<void>(coordinator.on_event(
        event(coordinator.session_generation(),
              ++sequence,
              behavior::GlueWindowRole::Leader,
              behavior::GlueEventKind::MoveResizeEnded,
              leader)));
    static_cast<void>(coordinator.complete(
        {coordinator.session_generation(), leader, follower}));
    expect(coordinator.state() == behavior::GlueMoveState::Completed,
           "generated path completes");
    return follower;
}

void test_dropped_intermediate_events_do_not_change_final_target() {
    const std::array dense{
        movement::TranslationDelta{5, 2},
        movement::TranslationDelta{17, -9},
        movement::TranslationDelta{-8, 30},
        movement::TranslationDelta{44, -21},
    };
    const std::array sparse{movement::TranslationDelta{44, -21}};
    const auto dense_final = run_translation_path(dense);
    const auto sparse_final = run_translation_path(sparse);
    expect(dense_final == sparse_final &&
               dense_final == geometry::Rect{144, -21, 244, 79},
           "dropped intermediate Leader events do not change the initial-relative final target");
}

void test_invalid_configuration() {
    expect_throws<std::invalid_argument>(
        [] { [[maybe_unused]] behavior::GlueMoveCoordinator coordinator{{0}}; },
        "zero pending-ledger capacity is rejected");
}

} // namespace

int main() {
    test_happy_path_and_feedback_semantics();
    test_illegal_transitions_and_generation_guards();
    test_topology_and_translation_contracts();
    test_operation_and_external_failure_paths();
    test_feedback_ordering_capacity_and_reconciliation();
    test_new_session_generation_clears_state();
    test_generated_stress_is_initial_relative_and_non_recursive();
    test_dropped_intermediate_events_do_not_change_final_target();
    test_invalid_configuration();

    if (failures != 0) {
        std::cerr << failures << " glue move coordinator test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All glue move coordinator tests passed\n";
    return EXIT_SUCCESS;
}
