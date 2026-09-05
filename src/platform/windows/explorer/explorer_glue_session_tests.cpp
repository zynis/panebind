#include "platform/windows/explorer/explorer_glue_session.h"
#include "platform/windows/explorer/explorer_glue_session_internal.h"
#include "platform/windows/explorer/explorer_glue_quantum.h"
#include "core/behavior/glue_move_coordinator.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <vector>

namespace {

namespace explorer = panebind::platform::windows::explorer;
namespace detail = panebind::platform::windows::explorer::detail;
namespace geometry = panebind::core::geometry;
namespace behavior = panebind::core::behavior;
namespace model = panebind::core::model;
namespace topology = panebind::core::topology;

int failures = 0;

void expect(const bool condition, const char* const message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

[[nodiscard]] explorer::ExplorerWindowSnapshot snapshot(
    const geometry::Rect visible,
    const geometry::Rect work_area,
    const wchar_t* const monitor = L"DISPLAY1",
    const std::uint32_t dpi = 192U) {
    explorer::ExplorerWindowSnapshot result;
    result.visible_rect = visible;
    result.positioning_rect = visible;
    result.monitor_device_name = monitor;
    result.monitor_rect = work_area;
    result.monitor_work_area = work_area;
    result.dpi = dpi;
    return result;
}

[[nodiscard]] detail::ExplorerGlueTargetConsentPrefixFacts valid_prefix() {
    detail::ExplorerGlueTargetConsentPrefixFacts facts;
    facts.authority_kind = explorer::ExplorerAuthorityKind::UserConsent;
    facts.generations.baseline_generation = 1U;
    facts.generations.target_prompt_generation = 2U;
    facts.generations.target_confirmation_generation = 3U;
    facts.generations.eligibility_generation = 4U;
    facts.generations.token_generation = 5U;
    facts.baseline_exclusion_complete = true;
    facts.unique_new_target = true;
    facts.exact_target_location = true;
    facts.token_issued = true;
    facts.token_active = true;
    return facts;
}

[[nodiscard]] detail::ExplorerGluePairInspection ready_inspection(
    const explorer::ExplorerWindowSnapshot& leader,
    const explorer::ExplorerWindowSnapshot& follower) {
    detail::ExplorerGluePairInspection result;
    result.reason = explorer::ExplorerGlueReason::Eligible;
    result.leader_snapshot = leader;
    result.follower_snapshot = follower;
    result.leader_target_consent_prefix_valid = true;
    result.follower_target_consent_prefix_valid = true;
    result.follower_baseline_excluded_leader = true;
    result.pair_distinct = true;
    result.same_monitor =
        leader.monitor_device_name == follower.monitor_device_name &&
        leader.monitor_rect == follower.monitor_rect &&
        leader.monitor_work_area == follower.monitor_work_area;
    result.same_dpi = leader.dpi == follower.dpi;
    result.same_monitor_and_dpi = result.same_monitor && result.same_dpi;
    if (!result.same_monitor_and_dpi) {
        result.reason = explorer::ExplorerGlueReason::MonitorOrDpiMismatch;
    }
    return result;
}

void expect_preview_has_no_effects(
    const explorer::ExplorerGlueLayoutReadinessResult& result,
    const char* const message) {
    expect(!result.glue_authority_bound && !result.glue_authority_consumed &&
               !result.native_apply_attempted && !result.event_source_armed &&
               !result.temporary_peer_exception_retained,
           message);
}

void test_target_consent_prefix_is_not_c2a_move_consent() {
    auto facts = valid_prefix();
    expect(detail::evaluate_target_consent_prefix(facts),
           "five-stage target-consent prefix is valid for Glue pairing");

    facts.generations.move_prompt_generation = 6U;
    expect(!detail::evaluate_target_consent_prefix(facts),
           "an R1-C2A move prompt cannot be upgraded into Glue authority");
    facts = valid_prefix();
    facts.move_authorized = true;
    expect(!detail::evaluate_target_consent_prefix(facts),
           "an already move-authorized target is rejected");
    facts = valid_prefix();
    facts.primary_authority_consumed = true;
    expect(!detail::evaluate_target_consent_prefix(facts),
           "a consumed one-shot target is rejected");
    facts = valid_prefix();
    facts.token_active = false;
    expect(!detail::evaluate_target_consent_prefix(facts),
           "a stale target token is rejected");
    facts = valid_prefix();
    facts.generations.eligibility_generation = 3U;
    expect(!detail::evaluate_target_consent_prefix(facts),
           "non-increasing target generations are rejected");
}

void test_pair_authority_precedence() {
    detail::ExplorerGluePairAuthorityFacts facts{
        true, true, true, true, true, true, true, true};
    expect(detail::evaluate_pair_authority(facts) ==
               explorer::ExplorerGlueReason::Eligible,
           "two exact target prefixes form an eligible pair");
    facts.owner_thread_matches = false;
    expect(detail::evaluate_pair_authority(facts) ==
               explorer::ExplorerGlueReason::WrongOwnerThread,
           "wrong owner thread fails first");
    facts = {true, true, true, true, true, true, false, true};
    expect(detail::evaluate_pair_authority(facts) ==
               explorer::ExplorerGlueReason::FollowerBaselineMissingLeader,
           "Follower must permanently exclude Leader from its baseline");
    facts = {true, true, true, true, false, true, true, true};
    expect(detail::evaluate_pair_authority(facts) ==
               explorer::ExplorerGlueReason::PairNotDistinct,
           "same target cannot fill both roles");
    facts = {true, true, true, true, true, true, true, false};
    expect(detail::evaluate_pair_authority(facts) ==
               explorer::ExplorerGlueReason::MonitorOrDpiMismatch,
           "mixed monitor/DPI pair is rejected");
}

void test_feedback_receipt_must_follow_native_registration() {
    expect(!detail::follower_feedback_after_registration(0U, 5U) &&
               !detail::follower_feedback_after_registration(5U, 5U) &&
               !detail::follower_feedback_after_registration(4U, 5U) &&
               detail::follower_feedback_after_registration(6U, 5U),
           "only a receipt sequenced after pending registration can self-acknowledge");

    expect(detail::follower_receipt_precedes_new_native_apply(
               12U, true, 10U, true, true, false),
           "delayed exact feedback for an earlier completed apply may precede the next apply");
    expect(!detail::follower_receipt_precedes_new_native_apply(
               10U, true, 10U, true, true, false),
           "a receipt at the earlier registration watermark is not attributable");
    expect(!detail::follower_receipt_precedes_new_native_apply(
               12U, true, 10U, false, false, false),
           "geometry for an operation not yet applied cannot authorize feedback");
    expect(!detail::follower_receipt_precedes_new_native_apply(
               12U, false, 0U, false, false, false),
           "unmatched Follower geometry aborts before a new native apply");
    expect(detail::follower_receipt_precedes_new_native_apply(
               12U, false, 0U, false, false, true),
           "the exact acknowledged current geometry remains a legal duplicate");
}

void test_start_geometry_accepts_queued_translation_only() {
    const auto armed = snapshot({100, 100, 400, 300}, {0, 0, 1200, 800});
    expect(detail::evaluate_leader_start_geometry(armed, armed) ==
               detail::ExplorerGlueLeaderStartGeometry::ExactLayout,
           "START alone accepts the exact armed layout");

    auto translated = armed;
    translated.visible_rect = {125, 112, 425, 312};
    translated.positioning_rect = translated.visible_rect;
    expect(detail::evaluate_leader_start_geometry(armed, translated) ==
               detail::ExplorerGlueLeaderStartGeometry::TranslatedSinceReceipt,
           "queued START plus LOCATION accepts one same-size translation");

    auto later_translation = armed;
    later_translation.visible_rect = {180, 145, 480, 345};
    later_translation.positioning_rect = later_translation.visible_rect;
    expect(detail::evaluate_leader_start_geometry(armed, later_translation) ==
               detail::ExplorerGlueLeaderStartGeometry::TranslatedSinceReceipt,
           "queued START plus multiple LOCATION receipts accepts the final total delta");

    auto resized = armed;
    resized.visible_rect = {100, 100, 450, 300};
    resized.positioning_rect = resized.visible_rect;
    expect(detail::evaluate_leader_start_geometry(armed, resized) ==
               detail::ExplorerGlueLeaderStartGeometry::ResizeOrMixed,
           "queued START plus ResizeOrMixed geometry aborts");

    auto inconsistent_frame = translated;
    inconsistent_frame.positioning_rect = armed.positioning_rect;
    expect(detail::evaluate_leader_start_geometry(armed, inconsistent_frame) ==
               detail::ExplorerGlueLeaderStartGeometry::TargetInvalidated,
           "visible and positioning deltas must remain identical");

    auto changed_monitor = translated;
    changed_monitor.monitor_device_name = L"DISPLAY2";
    expect(detail::evaluate_leader_start_geometry(armed, changed_monitor) ==
               detail::ExplorerGlueLeaderStartGeometry::TargetInvalidated,
           "START never accepts monitor baseline drift");
}

void test_horizontal_layout_is_exact_and_size_preserving() {
    const auto leader = snapshot({50, 60, 350, 260}, {0, 0, 1000, 800});
    const auto follower = snapshot({400, 70, 650, 320}, {0, 0, 1000, 800});
    const auto plan = explorer::plan_explorer_glue_test_layout(leader, follower);
    expect(plan.has_value() &&
               plan->orientation ==
                   explorer::ExplorerGlueLayoutOrientation::Horizontal,
           "horizontal layout is preferred when both widths fit");
    if (!plan.has_value()) {
        return;
    }
    expect(plan->leader_target_visible.right() ==
               plan->follower_target_visible.left(),
           "horizontal layout has exact zero-gap adjacency");
    expect(plan->leader_target_visible.width() == leader.visible_rect.width() &&
               plan->leader_target_visible.height() ==
                   leader.visible_rect.height() &&
               plan->follower_target_visible.width() ==
                   follower.visible_rect.width() &&
               plan->follower_target_visible.height() ==
                   follower.visible_rect.height(),
           "fixture layout never resizes either window");
    expect(plan->leader_target_visible.left() >= 0 &&
               plan->follower_target_visible.right() <= 1000 &&
               plan->leader_target_visible.top() >= 0 &&
               plan->follower_target_visible.bottom() <= 800,
           "horizontal pair remains inside the exact work area");
}

void test_vertical_fallback_and_unsafe_layout() {
    const auto leader = snapshot({0, 0, 400, 300}, {0, 0, 500, 800});
    const auto follower = snapshot({0, 0, 400, 300}, {0, 0, 500, 800});
    const auto vertical =
        explorer::plan_explorer_glue_test_layout(leader, follower);
    expect(vertical.has_value() &&
               vertical->orientation ==
                   explorer::ExplorerGlueLayoutOrientation::Vertical &&
               vertical->leader_target_visible.bottom() ==
                   vertical->follower_target_visible.top(),
           "vertical zero-gap layout is the deterministic fallback");

    const auto too_tall = snapshot({0, 0, 400, 500}, {0, 0, 500, 800});
    expect(!explorer::plan_explorer_glue_test_layout(too_tall, too_tall)
                .has_value(),
           "layout blocks rather than resizing when neither orientation fits");

    auto other_monitor = follower;
    other_monitor.monitor_device_name = L"DISPLAY2";
    expect(!explorer::plan_explorer_glue_test_layout(leader, other_monitor)
                .has_value(),
           "layout rejects different monitors");
    auto other_dpi = follower;
    other_dpi.dpi = 144U;
    expect(!explorer::plan_explorer_glue_test_layout(leader, other_dpi)
                .has_value(),
           "layout rejects different DPI");
}

void test_layout_readiness_horizontal_and_structured_dimensions() {
    const auto leader = snapshot({50, 60, 350, 260}, {0, 0, 1000, 800});
    const auto follower = snapshot({400, 70, 650, 320}, {0, 0, 1000, 800});
    const auto preview = detail::evaluate_layout_readiness_preview(
        ready_inspection(leader, follower));

    expect(preview.succeeded() && preview.same_monitor && preview.same_dpi &&
               preview.readiness.has_value() && preview.layout.has_value(),
           "horizontal readiness retains live-compatible pair facts");
    if (!preview.readiness.has_value()) {
        return;
    }
    const auto& readiness = *preview.readiness;
    expect(readiness.leader_visible_size == geometry::Size{300, 200} &&
               readiness.follower_visible_size == geometry::Size{250, 250} &&
               readiness.work_area_size == geometry::Size{1000, 800},
           "readiness reports both current visible sizes and the work area");
    expect(readiness.horizontal.required == geometry::Size{550, 250} &&
               readiness.horizontal.available == geometry::Size{1000, 800} &&
               readiness.horizontal.fits,
           "horizontal readiness reports required and available width/height");
    expect(readiness.vertical.required == geometry::Size{300, 450} &&
               readiness.vertical.available == geometry::Size{1000, 800} &&
               readiness.vertical.fits,
           "vertical readiness is reported even when horizontal is preferred");
    expect(readiness.orientation ==
                   explorer::ExplorerGlueLayoutOrientation::Horizontal &&
               preview.layout->orientation ==
                   explorer::ExplorerGlueLayoutOrientation::Horizontal,
           "preview and planner share horizontal-first orientation");
    expect_preview_has_no_effects(
        preview, "horizontal preview never binds, consumes, applies, or hooks");
}

void test_layout_readiness_vertical_and_neither() {
    const auto leader = snapshot({0, 0, 400, 300}, {0, 0, 500, 800});
    const auto follower = snapshot({0, 0, 400, 300}, {0, 0, 500, 800});
    const auto vertical = detail::evaluate_layout_readiness_preview(
        ready_inspection(leader, follower));
    expect(vertical.succeeded() && vertical.readiness.has_value() &&
               !vertical.readiness->horizontal.fits &&
               vertical.readiness->vertical.fits &&
               vertical.readiness->horizontal.required ==
                   geometry::Size{800, 300} &&
               vertical.readiness->vertical.required ==
                   geometry::Size{400, 600} &&
               vertical.readiness->orientation ==
                   explorer::ExplorerGlueLayoutOrientation::Vertical &&
               vertical.layout->orientation ==
                   explorer::ExplorerGlueLayoutOrientation::Vertical,
           "preview exposes vertical fallback with both constraints");

    const auto too_tall = snapshot({0, 0, 400, 500}, {0, 0, 500, 800});
    const auto neither = detail::evaluate_layout_readiness_preview(
        ready_inspection(too_tall, too_tall));
    expect(neither.reason == explorer::ExplorerGlueReason::UnsafeLayout &&
               neither.readiness.has_value() && !neither.layout.has_value() &&
               !neither.readiness->horizontal.fits &&
               !neither.readiness->vertical.fits &&
               !neither.readiness->orientation.has_value() &&
               neither.readiness->horizontal.required ==
                   geometry::Size{800, 500} &&
               neither.readiness->vertical.required ==
                   geometry::Size{400, 1000},
           "preview explains both constraints when neither orientation fits");
    expect_preview_has_no_effects(
        neither, "unsafe preview never binds, consumes, applies, or hooks");
}

void test_layout_readiness_monitor_dpi_and_invalidation() {
    const auto leader = snapshot({0, 0, 300, 200}, {0, 0, 1000, 800});

    const auto other_monitor =
        snapshot({0, 0, 300, 200}, {1000, 0, 2000, 800}, L"DISPLAY2");
    const auto monitor = detail::evaluate_layout_readiness_preview(
        ready_inspection(leader, other_monitor));
    expect(monitor.reason ==
                   explorer::ExplorerGlueReason::MonitorOrDpiMismatch &&
               !monitor.same_monitor && monitor.same_dpi &&
               monitor.leader_snapshot.has_value() &&
               monitor.follower_snapshot.has_value() &&
               !monitor.readiness.has_value(),
           "preview distinguishes monitor mismatch from matching DPI");

    const auto other_dpi =
        snapshot({0, 0, 300, 200}, {0, 0, 1000, 800}, L"DISPLAY1", 144U);
    const auto dpi = detail::evaluate_layout_readiness_preview(
        ready_inspection(leader, other_dpi));
    expect(dpi.reason == explorer::ExplorerGlueReason::MonitorOrDpiMismatch &&
               dpi.same_monitor && !dpi.same_dpi &&
               dpi.leader_snapshot.has_value() &&
               dpi.follower_snapshot.has_value() &&
               !dpi.readiness.has_value(),
           "preview distinguishes DPI mismatch from matching monitor");

    detail::ExplorerGluePairInspection invalidated;
    invalidated.reason = explorer::ExplorerGlueReason::TargetChanged;
    invalidated.leader_target_consent_prefix_valid = true;
    invalidated.follower_target_consent_prefix_valid = true;
    invalidated.follower_baseline_excluded_leader = true;
    invalidated.pair_distinct = true;
    const auto invalid = detail::evaluate_layout_readiness_preview(invalidated);
    expect(invalid.reason == explorer::ExplorerGlueReason::TargetChanged &&
               !invalid.succeeded() && !invalid.readiness.has_value() &&
               !invalid.layout.has_value(),
           "invalidated live model never manufactures readiness");
    expect_preview_has_no_effects(
        monitor, "monitor mismatch preview remains non-consuming");
    expect_preview_has_no_effects(dpi, "DPI mismatch preview remains non-consuming");
    expect_preview_has_no_effects(invalid,
                                  "invalidated preview remains non-consuming");
}

void test_layout_readiness_is_repeatable_and_too_large_can_become_ready() {
    const auto too_large = snapshot({0, 0, 600, 500}, {0, 0, 1000, 800});
    const auto inspection = ready_inspection(too_large, too_large);
    const auto first = detail::evaluate_layout_readiness_preview(inspection);
    const auto repeated = detail::evaluate_layout_readiness_preview(inspection);
    expect(first.reason == explorer::ExplorerGlueReason::UnsafeLayout &&
               repeated.reason == first.reason &&
               repeated.readiness == first.readiness &&
               repeated.layout == first.layout,
           "the same readiness input is repeatable and non-consuming");
    expect(inspection.reason == explorer::ExplorerGlueReason::Eligible &&
               inspection.leader_snapshot.has_value() &&
               inspection.follower_snapshot.has_value(),
           "preview leaves caller-owned model input available for another call");

    const auto smaller = snapshot({0, 0, 400, 300}, {0, 0, 1000, 800});
    const auto after_user_resize = detail::evaluate_layout_readiness_preview(
        ready_inspection(smaller, smaller));
    expect(after_user_resize.succeeded() &&
               after_user_resize.readiness->horizontal.fits &&
               after_user_resize.readiness->orientation ==
                   explorer::ExplorerGlueLayoutOrientation::Horizontal,
           "a later non-consuming preview observes too-large targets now fitting");
    expect_preview_has_no_effects(first, "first preview has no native side effect");
    expect_preview_has_no_effects(repeated,
                                  "repeated preview has no native side effect");
    expect_preview_has_no_effects(
        after_user_resize, "ready preview still has no native side effect");

    using PreviewFunction = explorer::ExplorerGlueLayoutReadinessResult (*)(
        explorer::ExplorerTestSession&, explorer::ExplorerTestSession&);
    [[maybe_unused]] PreviewFunction public_preview =
        &explorer::ExplorerGlueConsent::preview_layout_readiness;
}

void test_layout_readiness_rejects_retained_side_effect_state() {
    const auto leader = snapshot({0, 0, 300, 200}, {0, 0, 1000, 800});
    auto native_attempt = ready_inspection(leader, leader);
    native_attempt.native_apply_attempted = true;
    const auto native =
        detail::evaluate_layout_readiness_preview(native_attempt);
    expect(native.reason == explorer::ExplorerGlueReason::AuthorityConsumed &&
               native.native_apply_attempted && !native.succeeded() &&
               !native.layout.has_value(),
           "preview rejects any observed native operation side effect");

    auto retained_peer = ready_inspection(leader, leader);
    retained_peer.temporary_peer_exception_retained = true;
    const auto peer = detail::evaluate_layout_readiness_preview(retained_peer);
    expect(peer.reason == explorer::ExplorerGlueReason::AuthorityConsumed &&
               peer.temporary_peer_exception_retained && !peer.succeeded() &&
               !peer.layout.has_value(),
           "preview rejects a retained temporary peer exception");
}

// Synthetic normalized receipts exercise the production selection policy and
// real R1-A/Glue coordinator. Exact operation results below are fixture inputs,
// never native Explorer observations or event-time geometry claims.
class QuantumFixture final {
public:
    QuantumFixture() {
        const std::array windows{
            topology::WindowGeometry{model::WindowId{"leader"}, initial_leader},
            topology::WindowGeometry{model::WindowId{"follower"}, initial_follower}};
        const auto graph = topology::WindowAdjacencyGraph::build(windows, {});
        expect(coordinator.arm(graph, model::WindowId{"leader"},
                               model::WindowId{"follower"}).kind ==
                   behavior::GlueDecisionKind::Armed,
               "quantum fixture arms the actual frozen two-window coordinator");
    }

    [[nodiscard]] explorer::ExplorerGlueEvent receipt(
        const explorer::ExplorerGlueEventKind kind,
        const explorer::ExplorerGlueWindowRole role =
            explorer::ExplorerGlueWindowRole::Leader) {
        return {kind, role,
                role == explorer::ExplorerGlueWindowRole::Leader ? 11U : 22U,
                1U, next_sequence++, 0U, 0U};
    }

    void start() {
        const std::array events{
            receipt(explorer::ExplorerGlueEventKind::MoveResizeStarted)};
        process(events, initial_leader);
    }

    void locations(const geometry::Rect live_leader,
                   const std::size_t count = 3U) {
        std::vector<explorer::ExplorerGlueEvent> events;
        for (std::size_t index = 0U; index < count; ++index) {
            events.push_back(
                receipt(explorer::ExplorerGlueEventKind::GeometryChanged));
        }
        process(events, live_leader);
    }

    void feedback() {
        const std::array events{receipt(
            explorer::ExplorerGlueEventKind::GeometryChanged,
            explorer::ExplorerGlueWindowRole::Follower)};
        const auto previous_requests = commands.size();
        process(events, last_leader);
        expect(commands.size() == previous_requests,
               "synthetic exact follower feedback never requests another move");
    }

    void finish() {
        const std::array events{
            receipt(explorer::ExplorerGlueEventKind::MoveResizeEnded)};
        process(events, last_leader);
    }

    void process(const std::span<const explorer::ExplorerGlueEvent> events,
                 const geometry::Rect live_leader) {
        ++quantum_count;
        const auto selected = detail::select_glue_quantum_events(events);
        const auto leader_locations = std::count_if(
            events.begin(), events.end(), [](const auto& event) {
                return event.kind == explorer::ExplorerGlueEventKind::GeometryChanged &&
                       event.role == explorer::ExplorerGlueWindowRole::Leader;
            });
        raw_leader_locations += static_cast<std::size_t>(leader_locations);
        if (leader_locations != 0) {
            samples.push_back(live_leader);
        }
        last_leader = live_leader;
        for (std::size_t index = 0U; index < events.size(); ++index) {
            if (!selected[index]) {
                ++coalesced_receipts;
                continue;
            }
            const auto& event = events[index];
            if (event.kind == explorer::ExplorerGlueEventKind::TargetDestroyed) {
                static_cast<void>(coordinator.report_target_invalidated(
                    coordinator.session_generation()));
                break;
            }
            const auto role = event.role == explorer::ExplorerGlueWindowRole::Leader
                                  ? behavior::GlueWindowRole::Leader
                                  : behavior::GlueWindowRole::Follower;
            const auto kind = event.kind == explorer::ExplorerGlueEventKind::MoveResizeStarted
                                  ? behavior::GlueEventKind::MoveResizeStarted
                              : event.kind == explorer::ExplorerGlueEventKind::MoveResizeEnded
                                  ? behavior::GlueEventKind::MoveResizeEnded
                                  : behavior::GlueEventKind::GeometryChanged;
            const auto decision = coordinator.on_event(
                {coordinator.session_generation(), event.receipt_sequence,
                 role, kind,
                 kind == behavior::GlueEventKind::MoveResizeStarted
                     ? std::nullopt
                     : std::optional<geometry::Rect>{
                           role == behavior::GlueWindowRole::Leader
                               ? live_leader : follower}});
            decisions.push_back(decision.kind);
            if (decision.command.has_value()) {
                expect(role == behavior::GlueWindowRole::Leader &&
                           kind == behavior::GlueEventKind::GeometryChanged,
                       "only selected Leader LOCATION requests follower motion");
                commands.push_back(*decision.command);
                operation_quanta.push_back(quantum_count);
                follower = decision.command->target_visible_rect;
                const auto& command = *decision.command;
                expect(coordinator.on_operation_result(
                           {command.session_generation,
                            command.operation_generation, command.follower_id,
                            behavior::FollowerOperationOutcome::Exact,
                            follower}).kind ==
                           behavior::GlueDecisionKind::OperationRecorded,
                       "every fixture operation has an exact postverify result");
            }
            if (coordinator.state() == behavior::GlueMoveState::Aborted) {
                break;
            }
            if (decision.kind == behavior::GlueDecisionKind::Completing) {
                expect(coordinator.complete(
                           {coordinator.session_generation(), live_leader,
                            follower}).kind == behavior::GlueDecisionKind::Completed,
                       "END reconciles the exact final total-delta target");
            }
        }
    }

    static constexpr geometry::Rect initial_leader{0, 0, 100, 100};
    static constexpr geometry::Rect initial_follower{100, 0, 200, 100};
    behavior::GlueMoveCoordinator coordinator;
    geometry::Rect follower{initial_follower};
    geometry::Rect last_leader{initial_leader};
    std::uint64_t next_sequence{1U};
    std::size_t quantum_count{};
    std::size_t raw_leader_locations{};
    std::size_t coalesced_receipts{};
    std::vector<geometry::Rect> samples;
    std::vector<behavior::FollowerMoveCommand> commands;
    std::vector<std::size_t> operation_quanta;
    std::vector<behavior::GlueDecisionKind> decisions;
};

void test_one_quantum_coalesces_without_claiming_historical_samples() {
    QuantumFixture fixture;
    using Kind = explorer::ExplorerGlueEventKind;
    const std::array events{
        fixture.receipt(Kind::MoveResizeStarted),
        fixture.receipt(Kind::GeometryChanged),
        fixture.receipt(Kind::GeometryChanged),
        fixture.receipt(Kind::GeometryChanged),
        fixture.receipt(Kind::MoveResizeEnded)};
    expect(detail::select_glue_quantum_events(events) ==
               std::vector<bool>{true, false, false, true, true},
           "START and END survive while only the latest LOCATION is selected");
    fixture.process(events, {40, 25, 140, 125});
    expect(fixture.raw_leader_locations == 3U &&
               fixture.samples.size() == 1U &&
               fixture.coalesced_receipts == 2U &&
               fixture.commands.size() == 1U &&
               fixture.commands.front().source_leader_sequence == 4U &&
               fixture.follower == geometry::Rect{140, 25, 240, 125} &&
               fixture.coordinator.state() == behavior::GlueMoveState::Completed,
           "one quantum yields one current sample and one final catch-up, not three historical observations");
}

void test_multiple_wakes_preserve_progressive_total_delta() {
    QuantumFixture fixture;
    fixture.start();
    const std::array samples{
        geometry::Rect{10, 5, 110, 105},
        geometry::Rect{25, -7, 125, 93},
        geometry::Rect{55, 18, 155, 118}};
    for (const auto& sample : samples) {
        fixture.locations(sample);
    }
    expect(fixture.raw_leader_locations == 9U && fixture.samples.size() == 3U &&
               fixture.commands.size() == 3U &&
               fixture.operation_quanta == std::vector<std::size_t>{2U, 3U, 4U},
           "separate wakes produce distinct samples and multiple active requests");
    if (fixture.commands.size() == 3U) {
        for (std::size_t index = 0U; index < samples.size(); ++index) {
            const auto& command = fixture.commands[index];
            expect(command.target_visible_rect == geometry::Rect{
                       samples[index].left() + 100, samples[index].top(),
                       samples[index].right() + 100, samples[index].bottom()} &&
                       command.operation_generation == index + 1U,
                   "every requested target uses initial follower plus total Leader delta");
        }
    }
    fixture.finish();
    expect(fixture.coordinator.state() == behavior::GlueMoveState::Completed &&
               fixture.coordinator.stats().missing_feedback_count == 3U,
           "multiple exact operations may all reconcile without follower events");
}

void test_same_sample_next_quantum_is_noop() {
    QuantumFixture fixture;
    fixture.start();
    fixture.locations({30, 12, 130, 112});
    fixture.locations({30, 12, 130, 112});
    expect(fixture.samples.size() == 2U && fixture.commands.size() == 1U &&
               fixture.coordinator.stats().follower_noop_count == 1U,
           "a repeated processing sample does not cause a second operation");
    fixture.locations({60, 24, 160, 124});
    expect(fixture.commands.size() == 2U,
           "the next meaningful sample after a no-op still requests motion");
    fixture.finish();
}

void test_end_quantum_keeps_final_translation_and_lifecycle_barriers() {
    QuantumFixture fixture;
    fixture.start();
    fixture.locations({10, 2, 110, 102});
    using Kind = explorer::ExplorerGlueEventKind;
    const std::array final_events{
        fixture.receipt(Kind::GeometryChanged),
        fixture.receipt(Kind::GeometryChanged),
        fixture.receipt(Kind::MoveResizeEnded)};
    fixture.process(final_events, {80, 35, 180, 135});
    expect(fixture.commands.size() == 2U &&
               fixture.commands.back().source_leader_sequence ==
                   final_events[1].receipt_sequence &&
               fixture.decisions.back() == behavior::GlueDecisionKind::Completing &&
               fixture.follower == geometry::Rect{180, 35, 280, 135},
           "last meaningful LOCATION is applied before same-quantum END reconciliation");

    const std::array barrier_events{
        explorer::ExplorerGlueEvent{Kind::GeometryChanged},
        explorer::ExplorerGlueEvent{Kind::MoveResizeEnded},
        explorer::ExplorerGlueEvent{Kind::GeometryChanged},
        explorer::ExplorerGlueEvent{Kind::TargetDestroyed},
        explorer::ExplorerGlueEvent{Kind::GeometryChanged}};
    expect(detail::select_glue_quantum_events(barrier_events) ==
               std::vector<bool>{true, true, true, true, true},
           "coalescing never removes lifecycle or destroy barriers");
}

void test_quantum_resize_or_mixed_still_aborts() {
    QuantumFixture fixture;
    fixture.start();
    fixture.locations({10, 5, 110, 105});
    fixture.locations({25, 10, 140, 110});
    expect(fixture.coordinator.state() == behavior::GlueMoveState::Aborted &&
               fixture.coordinator.abort_reason() ==
                   behavior::GlueAbortReason::ResizeOrMixed &&
               fixture.commands.size() == 1U,
           "a later quantum with mixed move/resize aborts without another request");
}

void test_multiple_quantum_feedback_and_missing_reconcile() {
    QuantumFixture fixture;
    fixture.start();
    fixture.locations({10, 0, 110, 100});
    fixture.feedback();
    fixture.locations({20, 5, 120, 105});
    fixture.locations({30, 10, 130, 110});
    expect(fixture.coordinator.pending_operation_count() == 2U,
           "operation one is acknowledged while operations two and three await feedback");
    fixture.finish();
    const auto& stats = fixture.coordinator.stats();
    expect(fixture.commands.size() == 3U &&
               stats.suppressed_feedback_count == 1U &&
               stats.missing_feedback_count == 2U &&
               stats.reconciled_feedback_count == 2U &&
               stats.unexpected_feedback_count == 0U &&
               fixture.coordinator.pending_operation_count() == 0U &&
               fixture.coordinator.state() == behavior::GlueMoveState::Completed,
           "mixed observed/missing exact feedback reconciles without recursive motion");
}

void test_120_receipts_across_quanta_without_drift_or_recursion() {
    QuantumFixture fixture;
    fixture.start();
    for (std::size_t index = 1U; index <= 24U; ++index) {
        const auto delta = static_cast<geometry::Coordinate>(index * 3U);
        fixture.locations({delta, -delta, delta + 100, 100 - delta}, 5U);
        if (index % 4U == 0U) {
            fixture.feedback();
            fixture.feedback();
        }
    }
    fixture.finish();
    const auto& stats = fixture.coordinator.stats();
    expect(fixture.raw_leader_locations == 120U &&
               fixture.samples.size() == 24U &&
               fixture.coalesced_receipts == 96U &&
               fixture.commands.size() == 24U &&
               fixture.follower == geometry::Rect{172, -72, 272, 28} &&
               stats.duplicate_feedback_count == 6U &&
               stats.missing_feedback_count == 18U &&
               stats.reconciled_feedback_count == 18U &&
               stats.max_pending_depth <= 32U &&
               stats.unexpected_feedback_count == 0U &&
               fixture.coordinator.state() == behavior::GlueMoveState::Completed,
           "120 raw receipts preserve 24 processing samples with bounded ledger, no drift, no recursion, and exact final geometry");
}

} // namespace

int main() {
    test_target_consent_prefix_is_not_c2a_move_consent();
    test_pair_authority_precedence();
    test_feedback_receipt_must_follow_native_registration();
    test_start_geometry_accepts_queued_translation_only();
    test_horizontal_layout_is_exact_and_size_preserving();
    test_vertical_fallback_and_unsafe_layout();
    test_layout_readiness_horizontal_and_structured_dimensions();
    test_layout_readiness_vertical_and_neither();
    test_layout_readiness_monitor_dpi_and_invalidation();
    test_layout_readiness_is_repeatable_and_too_large_can_become_ready();
    test_layout_readiness_rejects_retained_side_effect_state();
    test_one_quantum_coalesces_without_claiming_historical_samples();
    test_multiple_wakes_preserve_progressive_total_delta();
    test_same_sample_next_quantum_is_noop();
    test_end_quantum_keeps_final_translation_and_lifecycle_barriers();
    test_quantum_resize_or_mixed_still_aborts();
    test_multiple_quantum_feedback_and_missing_reconcile();
    test_120_receipts_across_quanta_without_drift_or_recursion();

    if (failures != 0) {
        std::cerr << failures << " Explorer Glue session test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Explorer Glue session tests passed\n";
    return EXIT_SUCCESS;
}
