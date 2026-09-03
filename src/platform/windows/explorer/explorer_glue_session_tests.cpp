#include "platform/windows/explorer/explorer_glue_session.h"
#include "platform/windows/explorer/explorer_glue_session_internal.h"

#include <cstdlib>
#include <iostream>

namespace {

namespace explorer = panebind::platform::windows::explorer;
namespace detail = panebind::platform::windows::explorer::detail;
namespace geometry = panebind::core::geometry;

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

} // namespace

int main() {
    test_target_consent_prefix_is_not_c2a_move_consent();
    test_pair_authority_precedence();
    test_feedback_receipt_must_follow_native_registration();
    test_start_geometry_accepts_queued_translation_only();
    test_horizontal_layout_is_exact_and_size_preserving();
    test_vertical_fallback_and_unsafe_layout();

    if (failures != 0) {
        std::cerr << failures << " Explorer Glue session test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Explorer Glue session tests passed\n";
    return EXIT_SUCCESS;
}
