#include "platform/windows/explorer/explorer_session_internal.h"

#include "platform/windows/operations/window_translation.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>
#include <vector>

namespace {

namespace explorer = panebind::platform::windows::explorer;
namespace detail = panebind::platform::windows::explorer::detail;
namespace geometry = panebind::core::geometry;
namespace translation =
    panebind::platform::windows::operations::window_translation;

int failures = 0;

void expect(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] explorer::FilesystemLocationIdentity location_id(
    const std::uint64_t volume,
    const std::byte seed) {
    explorer::FilesystemLocationIdentity identity;
    identity.volume_serial_number = volume;
    identity.file_id.fill(seed);
    return identity;
}

[[nodiscard]] explorer::OpaqueLocationFingerprint opaque_location(
    const std::byte seed) {
    explorer::OpaqueLocationFingerprint fingerprint{};
    fingerprint.fill(seed);
    return fingerprint;
}

[[nodiscard]] detail::InventoryFingerprint fingerprint(
    const detail::NativeWindowKey key,
    const explorer::FilesystemLocationIdentity& location) {
    return {key,
            1U,
            {{explorer::ShellLocationStatus::Filesystem,
              explorer::ShellLocationSource::FileUrl,
              location,
              opaque_location(location.file_id.front())}}};
}

[[nodiscard]] detail::InventoryFingerprint opaque_fingerprint(
    const detail::NativeWindowKey key,
    const explorer::ShellLocationStatus status,
    const explorer::OpaqueLocationFingerprint& location) {
    return {key,
            1U,
            {{status,
              explorer::ShellLocationSource::FileUrl,
              std::nullopt,
              location}}};
}

[[nodiscard]] explorer::ExplorerWindowToken issued_token(
    detail::ExplorerTokenLedger& ledger,
    const detail::NativeWindowKey key) {
    auto result = ledger.issue(key, 100U, 200U);
    if (!result.token.has_value()) {
        std::abort();
    }
    return *result.token;
}

[[nodiscard]] detail::EligibilityModelFacts eligible_facts() {
    return {
        true,  // window_exists
        true,  // process_alive
        true,  // process_id_stable
        true,  // thread_id_stable
        true,  // image_matches
        true,  // class_allowed
        true,  // root_is_self
        false, // child_style
        false, // has_owner
        true,  // visible
        false, // cloaked
        false, // minimized
        false, // maximized
        true,  // current_virtual_desktop
        true,  // security_query_succeeded
        true,  // same_user
        true,  // same_session
        true,  // same_integrity
        true,  // medium_integrity
        false, // elevated
        false, // ui_access
        false, // app_container
        true,  // shell_entry_unique
        true,  // location_exact
        true,  // geometry_available
        true,  // dpi_context_supported
        true,  // monitor_available
        true,  // monitor_stable
        true,  // dpi_stable
    };
}

void test_token_is_explorer_specific_and_opaque() {
    static_assert(
        !std::is_default_constructible_v<explorer::ExplorerWindowToken>);
    static_assert(!std::is_constructible_v<explorer::ExplorerWindowToken,
                                           HWND>);
    static_assert(!std::is_constructible_v<explorer::ExplorerWindowToken,
                                           std::uintptr_t>);
    static_assert(
        !std::is_convertible_v<explorer::ExplorerWindowToken, HWND>);
    static_assert(
        !std::is_convertible_v<HWND, explorer::ExplorerWindowToken>);
    expect(true, "ExplorerWindowToken has no public native-handle boundary");
}

void test_monotonic_source_does_not_wrap() {
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    detail::MonotonicIdSource source{maximum - 1U};
    expect(source.issue() == maximum - 1U,
           "monotonic source issues its last safe identifier");
    expect(source.issue() == 0U && source.issue() == 0U,
           "monotonic source remains exhausted instead of wrapping");
}

void test_token_authority_generation_stale_and_cross_session() {
    detail::ExplorerTokenLedger first{10U, 20U, 1U, 1U};
    detail::ExplorerTokenLedger second{10U, 21U, 1U, 1U};
    const auto first_token = issued_token(first, 0x100U);
    const auto second_token = issued_token(second, 0x200U);

    expect(first_token.logical_id() == second_token.logical_id() &&
               first_token.generation() == second_token.generation(),
           "test reproduces equal local token values across sessions");
    expect(first_token != second_token,
           "private session authority prevents cross-session aliasing");
    expect(first.contains(first_token) && second.contains(second_token),
           "each ledger resolves its own Explorer capability");
    expect(!first.contains(second_token) && !second.contains(first_token),
           "foreign Explorer capability is rejected by both ledgers");

    expect(first.retire_native(0x100U),
           "retire invalidates the live Explorer capability");
    expect(!first.contains(first_token), "retired token is stale");
    const auto replacement = issued_token(first, 0x100U);
    expect(replacement != first_token &&
               replacement.generation() > first_token.generation(),
           "native-key reuse receives a fresh generation");
    expect(!first.contains(first_token) && first.contains(replacement),
           "native-key reuse cannot revive an old token");

    detail::ExplorerTokenLedger invalid_authority{0U, 1U, 1U, 1U};
    expect(invalid_authority.issue(0x300U, 1U, 1U).status ==
               detail::ExplorerLedgerIssueStatus::AuthorityExhausted,
           "zero controller authority blocks issuance");
    detail::ExplorerTokenLedger exhausted{
        1U, 2U, std::numeric_limits<std::uint64_t>::max(), 1U};
    expect(exhausted.issue(0x300U, 1U, 1U).status ==
               detail::ExplorerLedgerIssueStatus::GenerationExhausted,
           "identifier exhaustion blocks issuance without wrapping");
}

void test_single_primary_apply_guard() {
    detail::SinglePrimaryApplyGuard guard;
    expect(!guard.consumed(),
           "primary apply allowance starts unconsumed");
    expect(guard.try_consume() && guard.consumed(),
           "first native primary apply consumes the session allowance");
    expect(!guard.try_consume(),
           "second primary native apply is rejected deterministically");
    expect(detail::evaluate_primary_apply_gate(true, false) ==
               explorer::ExplorerEligibilityReason::Eligible &&
               detail::evaluate_primary_apply_gate(true, true) ==
                   explorer::ExplorerEligibilityReason::OperationLimitReached,
           "live token gets exactly one primary operation allowance");
    expect(detail::evaluate_primary_apply_gate(false, true) ==
               explorer::ExplorerEligibilityReason::StaleToken,
           "close-retired token is stale before the consumed-limit check");

    expect(detail::evaluate_restore_gate(true, false, false, false, false) ==
               explorer::ExplorerEligibilityReason::OperationSequenceViolation,
           "restore cannot run before a primary native attempt");
    expect(detail::evaluate_restore_gate(true, true, true, false, false) ==
               explorer::ExplorerEligibilityReason::OperationSequenceViolation,
           "restore requires a reliable post-attempt recapture");
    expect(detail::evaluate_restore_gate(true, true, true, true, false) ==
               explorer::ExplorerEligibilityReason::Eligible,
           "one independent restore is allowed after a recaptured primary attempt");
    expect(detail::evaluate_restore_gate(true, true, true, true, true) ==
               explorer::ExplorerEligibilityReason::OperationLimitReached,
           "a second restore is rejected before native apply");
    expect(detail::evaluate_restore_gate(false, true, true, true, true) ==
               explorer::ExplorerEligibilityReason::StaleToken,
           "close-retired token remains stale before restore sequence checks");
}

void test_shell_creation_and_handle_readiness_are_distinct() {
    expect(detail::map_shell_creation_stage(
               explorer::ShellAutomationStage::CreateBrowserWindow) ==
               explorer::ExplorerEligibilityReason::ShellWindowCreationFailed,
           "CoCreateInstance failure remains a creation blocker");
    expect(detail::map_shell_creation_stage(
               explorer::ShellAutomationStage::ReadWindowHandle) ==
               explorer::ExplorerEligibilityReason::ShellWindowHandleMissing,
           "get_HWND timeout is reported as frame readiness, not creation");
    expect(detail::map_shell_creation_stage(
               explorer::ShellAutomationStage::AwaitWindowHandle) ==
               explorer::ExplorerEligibilityReason::ShellWindowHandleMissing,
           "STA readiness wait failure cannot trigger a creation fallback");
    expect(detail::map_shell_creation_stage(
               explorer::ShellAutomationStage::ApartmentValidation) ==
               explorer::ExplorerEligibilityReason::ComApartmentUnavailable,
           "apartment validation remains distinguishable from Shell creation");

    const auto epoch = std::chrono::steady_clock::time_point{};
    expect(detail::readiness_deadline_active(
               epoch + std::chrono::milliseconds{1}, epoch),
           "readiness permits a stage only while now is strictly before deadline");
    expect(!detail::readiness_deadline_active(epoch, epoch) &&
               !detail::readiness_deadline_active(
                   epoch, epoch + std::chrono::milliseconds{1}),
           "equal or expired absolute deadlines block before the next side effect");
    expect(detail::readiness_acceptance_allowed(
               true, epoch + std::chrono::milliseconds{1}, epoch),
           "stable readiness strictly before deadline may be accepted");
    expect(!detail::readiness_acceptance_allowed(
               false, epoch + std::chrono::milliseconds{1}, epoch) &&
               !detail::readiness_acceptance_allowed(true, epoch, epoch) &&
               !detail::readiness_acceptance_allowed(
                   true, epoch, epoch + std::chrono::milliseconds{1}),
           "unstable, equal-deadline, and post-deadline samples cannot be accepted");
}

void test_candidate_set_delta_and_exact_location() {
    const auto first_location = location_id(1U, std::byte{0x11});
    const auto second_location = location_id(1U, std::byte{0x22});
    const auto target_location = location_id(2U, std::byte{0x33});
    const detail::InventoryModel baseline{
        true,
        {fingerprint(0x100U, first_location),
         fingerprint(0x200U, second_location)}};
    const detail::InventoryModel post{
        true,
        {fingerprint(0x100U, first_location),
         fingerprint(0x200U, second_location),
         fingerprint(0x300U, target_location)}};

    const auto valid = detail::evaluate_candidate_inventory(
        baseline, post, 0x300U, target_location);
    expect(valid.reason == explorer::ExplorerEligibilityReason::Eligible &&
               valid.new_candidate_count == 1U &&
               valid.baseline_unchanged && valid.exact_unique_location,
           "one retained post-baseline HWND at the exact location is eligible");

    expect(detail::evaluate_candidate_inventory(
               baseline, post, 0x100U, target_location)
               .reason == explorer::ExplorerEligibilityReason::PreexistingWindow,
           "preexisting HWND can never be promoted to a capability");

    expect(detail::evaluate_candidate_inventory(
               baseline, baseline, 0x300U, target_location)
               .reason ==
               explorer::ExplorerEligibilityReason::ReusedExistingWindow,
           "no post-baseline HWND is an explicit reuse failure");

    auto ambiguous = post;
    ambiguous.windows.push_back(fingerprint(0x400U, target_location));
    expect(detail::evaluate_candidate_inventory(
               baseline, ambiguous, 0x300U, target_location)
               .reason == explorer::ExplorerEligibilityReason::AmbiguousCandidate,
           "multiple new HWNDs are rejected without guessing");

    expect(detail::evaluate_candidate_inventory(
               baseline, post, 0x400U, target_location)
               .reason ==
               explorer::ExplorerEligibilityReason::ReusedExistingWindow,
           "the sole new HWND must equal the exact retained Shell object HWND");

    auto changed_baseline = post;
    changed_baseline.windows[0].locations[0].filesystem_location =
        target_location;
    expect(detail::evaluate_candidate_inventory(
               baseline, changed_baseline, 0x300U, target_location)
               .reason == explorer::ExplorerEligibilityReason::BaselineChanged,
           "preexisting-window fingerprint changes block isolation");

    auto wrong_location = post;
    wrong_location.windows.back() = fingerprint(0x300U, first_location);
    expect(detail::evaluate_candidate_inventory(
               baseline, wrong_location, 0x300U, target_location)
               .reason == explorer::ExplorerEligibilityReason::LocationMismatch,
           "new HWND at another directory is rejected");

    auto pending = post;
    pending.windows.back().locations = {{
        explorer::ShellLocationStatus::NavigationPending,
        explorer::ShellLocationSource::None,
        std::nullopt,
        std::nullopt}};
    expect(detail::evaluate_candidate_inventory(
               baseline, pending, 0x300U, target_location)
               .reason == explorer::ExplorerEligibilityReason::LocationNotReady,
           "pending navigation is retryable evidence, not a guessed match");

    auto multiple_entries = post;
    multiple_entries.windows.back().shell_entry_count = 2U;
    expect(detail::evaluate_candidate_inventory(
               baseline, multiple_entries, 0x300U, target_location)
               .reason == explorer::ExplorerEligibilityReason::AmbiguousCandidate,
           "multiple Shell entries sharing a frame are rejected as tab ambiguity");

    auto incomplete = post;
    incomplete.complete = false;
    expect(detail::evaluate_candidate_inventory(
               baseline, incomplete, 0x300U, target_location)
               .reason == explorer::ExplorerEligibilityReason::InventoryUnavailable,
           "incomplete inventory cannot issue a capability");

    auto unsupported_baseline = baseline;
    unsupported_baseline.windows.front().locations = {{
        explorer::ShellLocationStatus::Unsupported,
        explorer::ShellLocationSource::None,
        std::nullopt,
        std::nullopt}};
    expect(detail::evaluate_candidate_inventory(unsupported_baseline,
                                                post,
                                                0x300U,
                                                target_location)
               .reason == explorer::ExplorerEligibilityReason::InventoryUnavailable,
           "a baseline entry without an opaque LocationURL fingerprint blocks isolation");

    auto file_identity_only_baseline = baseline;
    file_identity_only_baseline.windows.front()
        .locations.front()
        .opaque_location.reset();
    expect(detail::evaluate_candidate_inventory(file_identity_only_baseline,
                                                post,
                                                0x300U,
                                                target_location)
               .reason == explorer::ExplorerEligibilityReason::InventoryUnavailable,
           "FILE_ID_INFO cannot replace the mandatory opaque baseline URL fingerprint");

    const auto first_opaque = opaque_location(std::byte{0x44});
    const auto second_opaque = opaque_location(std::byte{0x55});
    const detail::InventoryModel opaque_baseline{
        true,
        {opaque_fingerprint(0x100U,
                            explorer::ShellLocationStatus::OpenFailed,
                            first_opaque)}};
    const detail::InventoryModel stable_opaque_post{
        true,
        {opaque_fingerprint(0x100U,
                            explorer::ShellLocationStatus::OpenFailed,
                            first_opaque),
         fingerprint(0x300U, target_location)}};
    expect(detail::evaluate_candidate_inventory(opaque_baseline,
                                                stable_opaque_post,
                                                0x300U,
                                                target_location)
               .reason == explorer::ExplorerEligibilityReason::Eligible,
           "a stable opaque LocationURL fingerprint preserves an inaccessible baseline entry");

    auto changed_opaque_post = stable_opaque_post;
    changed_opaque_post.windows.front() = opaque_fingerprint(
        0x100U,
        explorer::ShellLocationStatus::OpenFailed,
        second_opaque);
    expect(detail::evaluate_candidate_inventory(opaque_baseline,
                                                changed_opaque_post,
                                                0x300U,
                                                target_location)
               .reason == explorer::ExplorerEligibilityReason::BaselineChanged,
           "equal status with a changed opaque LocationURL fingerprint blocks isolation");

    auto changed_source_post = stable_opaque_post;
    changed_source_post.windows.front().locations.front().source =
        explorer::ShellLocationSource::AbsolutePath;
    expect(detail::evaluate_candidate_inventory(opaque_baseline,
                                                changed_source_post,
                                                0x300U,
                                                target_location)
               .reason == explorer::ExplorerEligibilityReason::BaselineChanged,
           "a changed Shell location source remains part of the baseline fingerprint");

    const detail::InventoryModel associated_baseline{
        true,
        {{0x100U,
          2U,
          {{explorer::ShellLocationStatus::OpenFailed,
            explorer::ShellLocationSource::FileUrl,
            std::nullopt,
            first_opaque},
           {explorer::ShellLocationStatus::Unsupported,
            explorer::ShellLocationSource::None,
            std::nullopt,
            second_opaque}}}}};
    const detail::InventoryModel reassociated_post{
        true,
        {{0x100U,
          2U,
          {{explorer::ShellLocationStatus::OpenFailed,
            explorer::ShellLocationSource::FileUrl,
            std::nullopt,
            second_opaque},
           {explorer::ShellLocationStatus::Unsupported,
            explorer::ShellLocationSource::None,
            std::nullopt,
            first_opaque}}},
         fingerprint(0x300U, target_location)}};
    expect(detail::evaluate_candidate_inventory(associated_baseline,
                                                reassociated_post,
                                                0x300U,
                                                target_location)
               .reason == explorer::ExplorerEligibilityReason::BaselineChanged,
           "status and opaque fingerprint remain associated per Shell entry");
}

void test_allowlist_reason_model() {
    auto facts = eligible_facts();
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::Eligible,
           "complete Explorer allowlist facts are eligible");

    facts = eligible_facts();
    facts.image_matches = false;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::WrongImage,
           "canonical explorer image mismatch is explicit");
    facts = eligible_facts();
    facts.class_allowed = false;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::WrongClass,
           "class allowlist mismatch is explicit");
    facts = eligible_facts();
    facts.root_is_self = false;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::NotTopLevel,
           "non-root target is rejected");
    facts = eligible_facts();
    facts.child_style = true;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::ChildWindow,
           "WS_CHILD target is rejected");
    facts = eligible_facts();
    facts.has_owner = true;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::OwnedWindow,
           "owned target is rejected");
    facts = eligible_facts();
    facts.visible = false;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::Invisible,
           "invisible target is rejected");
    facts = eligible_facts();
    facts.cloaked = true;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::Cloaked,
           "DWM-cloaked target is rejected");
    facts = eligible_facts();
    facts.minimized = true;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::Minimized,
           "minimized target is rejected");
    facts = eligible_facts();
    facts.maximized = true;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::Maximized,
           "maximized target is rejected");
    facts = eligible_facts();
    facts.current_virtual_desktop = false;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::WrongVirtualDesktop,
           "off-desktop target is rejected");
    facts = eligible_facts();
    facts.same_user = false;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::UserMismatch,
           "different user token is rejected");
    facts = eligible_facts();
    facts.same_integrity = false;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::IntegrityMismatch,
           "integrity mismatch is rejected");
    facts = eligible_facts();
    facts.elevated = true;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::Elevated,
           "elevated target/controller boundary is unsupported");
    facts = eligible_facts();
    facts.ui_access = true;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::UiAccess,
           "UIAccess target is rejected");
    facts = eligible_facts();
    facts.app_container = true;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::AppContainer,
           "AppContainer target is rejected");
    facts = eligible_facts();
    facts.location_exact = false;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::LocationMismatch,
           "live shell-location mismatch invalidates the target");
    facts = eligible_facts();
    facts.monitor_stable = false;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::MonitorChanged,
           "monitor change invalidates the operation");
    facts = eligible_facts();
    facts.dpi_stable = false;
    expect(detail::evaluate_eligibility_model(facts) ==
               explorer::ExplorerEligibilityReason::DpiChanged,
           "DPI change invalidates the operation");
}

void test_safe_delta_selection_and_work_area_containment() {
    explorer::ExplorerWindowSnapshot snapshot;
    snapshot.visible_rect = geometry::Rect{100, 100, 500, 400};
    snapshot.positioning_rect = geometry::Rect{90, 90, 510, 410};
    snapshot.monitor_work_area = geometry::Rect{0, 0, 1000, 800};

    const auto normal = explorer::select_safe_test_delta(snapshot);
    expect(normal.succeeded() &&
               normal.delta == explorer::ExplorerTranslationDelta{80, 50} &&
               normal.target_visible_rect ==
                   geometry::Rect{180, 150, 580, 450},
           "safe-delta policy deterministically selects the first contained target");

    snapshot.visible_rect = geometry::Rect{600, 450, 980, 780};
    const auto near_edge = explorer::select_safe_test_delta(snapshot);
    expect(near_edge.succeeded() &&
               near_edge.delta ==
                   explorer::ExplorerTranslationDelta{-80, -50},
           "safe-delta policy backtracks when positive movement leaves work area");

    snapshot.visible_rect = geometry::Rect{0, 0, 1000, 800};
    const auto no_space = explorer::select_safe_test_delta(snapshot);
    expect(no_space.reason == explorer::ExplorerEligibilityReason::UnsafeDelta,
           "no fully-contained pure translation blocks the fixture");

    snapshot.visible_rect = geometry::Rect{100, 100, 500, 400};
    constexpr auto maximum = std::numeric_limits<geometry::Coordinate>::max();
    const std::array overflowing{
        explorer::ExplorerTranslationDelta{maximum, 0}};
    expect(detail::select_safe_delta_from_candidates(snapshot, overflowing)
               .reason == explorer::ExplorerEligibilityReason::UnsafeDelta,
           "overflowing safe-delta candidate is rejected without wraparound");

    expect(detail::rect_is_contained(geometry::Rect{0, 0, 10, 10},
                                     geometry::Rect{0, 0, 10, 10}) &&
               !detail::rect_is_contained(geometry::Rect{-1, 0, 10, 10},
                                          geometry::Rect{0, 0, 10, 10}),
           "work-area containment is inclusive at the boundary and rejects spill");
}

void test_shared_bridge_and_translation_failures() {
    const geometry::Rect positioning{90, 90, 510, 410};
    const geometry::Rect visible{100, 100, 500, 400};
    const auto translated = translation::prepare_visible_translation(
        positioning, visible, geometry::Rect{180, 150, 580, 450});
    expect(translated.status ==
               translation::TranslationPreparationStatus::Succeeded &&
               translated.dx == 80 && translated.dy == 50 &&
               translated.target_positioning_rect ==
                   geometry::Rect{170, 140, 590, 460},
           "Explorer adapter reuses visible-to-positioning translation semantics");

    const auto resize = translation::prepare_visible_translation(
        positioning, visible, geometry::Rect{180, 150, 581, 450});
    expect(detail::map_translation_status(resize.status) ==
               explorer::ExplorerEligibilityReason::ResizeRejected,
           "Explorer translation rejects resize instead of widening scope");

    constexpr auto minimum = std::numeric_limits<geometry::Coordinate>::min();
    constexpr auto maximum = std::numeric_limits<geometry::Coordinate>::max();
    const auto overflow = translation::prepare_visible_translation(
        positioning,
        geometry::Rect{minimum, 0, minimum + 10, 10},
        geometry::Rect{maximum - 10, 0, maximum, 10});
    expect(detail::map_translation_status(overflow.status) ==
               explorer::ExplorerEligibilityReason::ArithmeticOverflow,
           "Explorer translation propagates checked arithmetic failure");
}

void test_postverification_identity_and_geometry_model() {
    detail::PostVerificationModel valid{
        true, true, true, true, true, true, true, true, true, true, true};
    expect(detail::evaluate_post_verification(valid) ==
               explorer::ExplorerEligibilityReason::Eligible,
           "exact identity and geometry postcondition succeeds");

    auto changed_identity = valid;
    changed_identity.process_identity_stable = false;
    expect(detail::evaluate_post_verification(changed_identity) ==
               explorer::ExplorerEligibilityReason::TargetInvalidated,
           "process identity change invalidates postverification");
    auto changed_location = valid;
    changed_location.location_stable = false;
    expect(detail::evaluate_post_verification(changed_location) ==
               explorer::ExplorerEligibilityReason::LocationMismatch,
           "navigation race invalidates postverification");
    auto adjusted_geometry = valid;
    adjusted_geometry.visible_target_matches = false;
    expect(detail::evaluate_post_verification(adjusted_geometry) ==
               explorer::ExplorerEligibilityReason::PostVerificationFailed,
           "application-adjusted geometry is not silently accepted");
    auto resized = valid;
    resized.size_preserved = false;
    expect(detail::evaluate_post_verification(resized) ==
               explorer::ExplorerEligibilityReason::PostVerificationFailed,
           "postverification rejects resize even after native success");

    explorer::ExplorerWindowSnapshot expected;
    expected.visible_rect = geometry::Rect{100, 100, 500, 400};
    expected.positioning_rect = geometry::Rect{90, 90, 510, 410};
    auto user_moved = expected;
    user_moved.visible_rect = geometry::Rect{110, 100, 510, 400};
    user_moved.positioning_rect = geometry::Rect{100, 90, 520, 410};
    expect(detail::operation_snapshot_matches_expected(expected, expected) &&
               !detail::operation_snapshot_matches_expected(expected,
                                                             user_moved),
           "issuance-to-apply and primary-to-restore geometry competition is detected");
}

} // namespace

int main() {
    test_token_is_explorer_specific_and_opaque();
    test_monotonic_source_does_not_wrap();
    test_token_authority_generation_stale_and_cross_session();
    test_single_primary_apply_guard();
    test_shell_creation_and_handle_readiness_are_distinct();
    test_candidate_set_delta_and_exact_location();
    test_allowlist_reason_model();
    test_safe_delta_selection_and_work_area_containment();
    test_shared_bridge_and_translation_failures();
    test_postverification_identity_and_geometry_model();

    if (failures != 0) {
        std::cerr << failures << " explorer session test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Explorer session tests passed\n";
    return EXIT_SUCCESS;
}
