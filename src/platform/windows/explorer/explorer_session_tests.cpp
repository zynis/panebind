#include "platform/windows/explorer/explorer_session_internal.h"

#include "platform/windows/operations/window_translation.h"

#include <algorithm>
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
    auto result = ledger.issue(key, 100U, 200U, 42, 7U);
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

[[nodiscard]] detail::RegistrationCorrelationContext correlation_context(
    const explorer::FilesystemLocationIdentity& target_location) {
    return {
        7U,      // subscription_generation
        0x300U,  // automation_window
        0x300U,  // live_eligibility_window
        target_location,
        {0x100U, 0x200U},
    };
}

[[nodiscard]] detail::RegistrationWitness matching_registration(
    const explorer::FilesystemLocationIdentity& target_location,
    const std::int32_t cookie = 42,
    const std::uint64_t sequence = 1U) {
    return {
        sequence,
        7U,
        cookie,
        true,
        true,
        0x300U,
        target_location,
        false,
    };
}

[[nodiscard]] detail::LeaseCleanupFacts authorized_cleanup_facts() {
    return {
        true, // retained_automation_object_available
        true, // lease_belongs_to_current_session
        true, // created_by_current_cocreate
        true, // subscription_generation_matches
        true, // canonical_identity_available
        true, // canonical_identity_matches
        false, // canonical_identity_ambiguous
        true, // window_not_preexisting
        true, // window_identity_matches
        detail::LeaseNavigationDisposition::ExactExpectedTarget,
    };
}

[[nodiscard]] detail::ProvisioningAttemptSummary passing_attempt(
    const std::size_t attempt_index) {
    return {
        attempt_index,
        true, // baseline_exclusion_complete
        true, // registration_subscription_active
        true, // exactly_one_target
        true, // target_not_preexisting
        true, // matching_cookie_unique
        true, // canonical_identity_matches
        true, // three_way_window_matches
        true, // exact_target_location
        true, // full_eligibility_passed
        true, // safe_cleanup_succeeded
        true, // no_attributable_orphan
        true, // existing_windows_untouched
        true, // translation_not_attempted
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
    expect(invalid_authority.issue(0x300U, 1U, 1U, 42, 7U).status ==
               detail::ExplorerLedgerIssueStatus::AuthorityExhausted,
           "zero controller authority blocks issuance");
    detail::ExplorerTokenLedger exhausted{
        1U, 2U, std::numeric_limits<std::uint64_t>::max(), 1U};
    expect(exhausted.issue(0x300U, 1U, 1U, 42, 7U).status ==
               detail::ExplorerLedgerIssueStatus::GenerationExhausted,
           "identifier exhaustion blocks issuance without wrapping");
    detail::ExplorerTokenLedger invalid_registration{1U, 2U, 1U, 1U};
    expect(invalid_registration.issue(0x300U, 1U, 1U, 0, 7U).status ==
                   detail::ExplorerLedgerIssueStatus::
                       InvalidRegistrationAuthority &&
               invalid_registration.issue(0x300U, 1U, 1U, 42, 0U).status ==
                   detail::ExplorerLedgerIssueStatus::
                       InvalidRegistrationAuthority,
           "cookie and subscription generation are mandatory token authority");
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

void test_baseline_exclusion_uses_only_reliable_hwnd_identity() {
    const std::array entries{
        detail::BaselineEntryModel{
            0x100U, true, detail::BaselineLocationDisposition::Valid},
        detail::BaselineEntryModel{
            0x200U, true, detail::BaselineLocationDisposition::Empty},
        detail::BaselineEntryModel{
            0x300U, true, detail::BaselineLocationDisposition::Inaccessible},
        detail::BaselineEntryModel{
            0x200U, true, detail::BaselineLocationDisposition::Empty},
    };
    const std::array<detail::NativeWindowKey, 3U> prior{
        0x900U, 0x900U, 0U};
    const auto complete =
        detail::evaluate_baseline_exclusion(entries, prior);
    expect(complete.complete() &&
               complete.total_entry_count == 4U &&
               complete.reliable_entry_count == 4U &&
               complete.reliable_hwnd_count == 3U,
           "valid, empty, and inaccessible baseline entries remain complete when every HWND is reliable");
    expect(complete.valid_location_count == 1U &&
               complete.empty_location_count == 2U &&
               complete.inaccessible_location_count == 1U,
           "baseline location dispositions remain explicit diagnostic counts");
    expect(complete.forbidden_preexisting_hwnds ==
               std::vector<detail::NativeWindowKey>{
                   0x100U, 0x200U, 0x300U, 0x900U},
           "forbidden baseline HWNDs are non-zero, deduplicated, and include prior exclusions");

    const auto target_location = location_id(4U, std::byte{0x44});
    auto positive_context = correlation_context(target_location);
    positive_context.automation_window = 0x400U;
    positive_context.live_eligibility_window = 0x400U;
    positive_context.forbidden_preexisting_hwnds =
        complete.forbidden_preexisting_hwnds;
    auto positive_witness = matching_registration(target_location);
    positive_witness.cookie_resolved_window = 0x400U;
    const std::array positive_witnesses{positive_witness};
    expect(detail::evaluate_registration_correlation(
               positive_context, positive_witnesses)
               .eligible(),
           "opaque baseline locations do not weaken or block an independently proven new target");

    const std::array next_entries{
        detail::BaselineEntryModel{
            0x400U, true, detail::BaselineLocationDisposition::Valid}};
    const auto accumulated = detail::evaluate_baseline_exclusion(
        next_entries, complete.forbidden_preexisting_hwnds);
    expect(accumulated.complete() &&
               accumulated.forbidden_preexisting_hwnds ==
                   std::vector<detail::NativeWindowKey>{
                       0x100U, 0x200U, 0x300U, 0x400U, 0x900U},
           "a later capture can only add to the session's permanent forbidden HWND set");

    const std::array unreliable_entries{
        detail::BaselineEntryModel{
            0x500U, false, detail::BaselineLocationDisposition::Empty},
        detail::BaselineEntryModel{
            0U, true, detail::BaselineLocationDisposition::Inaccessible},
        detail::BaselineEntryModel{
            0x600U, true, detail::BaselineLocationDisposition::Valid},
    };
    const auto blocked = detail::evaluate_baseline_exclusion(
        unreliable_entries, accumulated.forbidden_preexisting_hwnds);
    expect(!blocked.complete() && blocked.reliable_entry_count == 1U &&
               blocked.reliable_hwnd_count == 1U,
           "a non-zero but unreliable HWND and a reliable zero HWND each block baseline completeness");
    expect(std::find(blocked.forbidden_preexisting_hwnds.begin(),
                     blocked.forbidden_preexisting_hwnds.end(),
                     0x600U) != blocked.forbidden_preexisting_hwnds.end() &&
               std::find(blocked.forbidden_preexisting_hwnds.begin(),
                         blocked.forbidden_preexisting_hwnds.end(),
                         0x500U) == blocked.forbidden_preexisting_hwnds.end(),
           "a blocked capture still conservatively retains every independently reliable HWND without guessing unreliable ones");
}

void test_shell_receipts_are_monotonic_and_fail_closed_on_exhaustion() {
    detail::ShellReceiptSequence sequence;
    const auto registered = sequence.issue(
        detail::ShellRegistrationEventKind::WindowRegistered, 9U, -17);
    const auto revoked = sequence.issue(
        detail::ShellRegistrationEventKind::WindowRevoked, 9U, -17);
    expect(registered.status == detail::ShellReceiptIssueStatus::Issued &&
               registered.receipt.has_value() &&
               registered.receipt->sequence == 1U &&
               registered.receipt->subscription_generation == 9U &&
               registered.receipt->cookie == -17 &&
               registered.receipt->kind ==
                   detail::ShellRegistrationEventKind::WindowRegistered,
           "registration receipt preserves signed LONG cookie semantics and subscription generation");
    expect(revoked.status == detail::ShellReceiptIssueStatus::Issued &&
               revoked.receipt.has_value() &&
               revoked.receipt->sequence == 2U &&
               revoked.receipt->kind ==
                   detail::ShellRegistrationEventKind::WindowRevoked,
           "registered and revoked callbacks receive one local monotonic sequence");

    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    detail::ShellReceiptSequence exhausted{maximum - 1U};
    const auto last_safe = exhausted.issue(
        detail::ShellRegistrationEventKind::WindowRegistered, 1U, 1);
    const auto first_failure = exhausted.issue(
        detail::ShellRegistrationEventKind::WindowRevoked, 1U, 1);
    const auto persistent_failure = exhausted.issue(
        detail::ShellRegistrationEventKind::WindowRegistered, 1U, 2);
    expect(last_safe.status == detail::ShellReceiptIssueStatus::Issued &&
               last_safe.receipt.has_value() &&
               last_safe.receipt->sequence == maximum - 1U,
           "receipt sequence issues its last safe value without wrapping");
    expect(first_failure.status ==
                   detail::ShellReceiptIssueStatus::SequenceExhausted &&
               !first_failure.receipt.has_value() &&
               persistent_failure.status ==
                   detail::ShellReceiptIssueStatus::SequenceExhausted &&
               !persistent_failure.receipt.has_value(),
           "receipt sequence exhaustion is permanent and cannot fabricate a zero or wrapped receipt");
}

void test_registration_correlation_requires_positive_authority() {
    const auto target = location_id(7U, std::byte{0x77});
    const auto context = correlation_context(target);

    const auto none = detail::evaluate_registration_correlation(
        context, std::span<const detail::RegistrationWitness>{});
    expect(none.status ==
                   detail::RegistrationCorrelationStatus::NoRegistration &&
               !none.eligible(),
           "no WindowRegistered receipt cannot establish provisioning authority");

    auto unrelated = matching_registration(target);
    unrelated.canonical_com_identity_matches = false;
    unrelated.cookie_resolved_window = 0x310U;
    const std::array unrelated_only{unrelated};
    const auto wrong_com = detail::evaluate_registration_correlation(
        context, unrelated_only);
    expect(wrong_com.status == detail::RegistrationCorrelationStatus::
                                   UnrelatedRegistrationsOnly &&
               wrong_com.unrelated_registration_count == 1U &&
               !wrong_com.eligible(),
           "a new HWND from the wrong canonical COM object is unrelated and cannot mint authority");

    auto shared_window_wrong_com = matching_registration(target);
    shared_window_wrong_com.canonical_com_identity_matches = false;
    shared_window_wrong_com.receipt_sequence = 2U;
    const std::array shared_window_wrong_com_only{shared_window_wrong_com};
    const auto shared_window_conflict =
        detail::evaluate_registration_correlation(
            context, shared_window_wrong_com_only);
    expect(shared_window_conflict.status ==
                   detail::RegistrationCorrelationStatus::
                       SharedWindowIdentityConflict &&
               shared_window_conflict.shared_window_identity_conflict_count ==
                   1U &&
               shared_window_conflict.unrelated_registration_count == 0U &&
               !shared_window_conflict.eligible(),
           "wrong canonical COM identity sharing the target HWND is an explicit multi-tab/frame conflict, not unrelated noise");
    const std::array match_plus_shared_window_conflict{
        matching_registration(target, 42, 1U), shared_window_wrong_com};
    expect(detail::evaluate_registration_correlation(
               context, match_plus_shared_window_conflict)
               .status == detail::RegistrationCorrelationStatus::
                              SharedWindowIdentityConflict,
           "one valid cookie cannot override another COM object registered on the same target frame");

    const std::array one_match{matching_registration(target, 42)};
    const auto eligible = detail::evaluate_registration_correlation(
        context, one_match);
    expect(eligible.eligible() && eligible.matching_cookie_count == 1U &&
               eligible.matching_cookie == 42,
           "one resolved same-object cookie with exact HWND and FILE_ID_INFO is eligible");

    auto unrelated_second = unrelated;
    unrelated_second.cookie = 11;
    unrelated_second.receipt_sequence = 2U;
    auto unrelated_third = unrelated;
    unrelated_third.cookie = 12;
    unrelated_third.receipt_sequence = 3U;
    const std::array mixed{
        unrelated_second, matching_registration(target, 42, 4U),
        unrelated_third};
    const auto mixed_result = detail::evaluate_registration_correlation(
        context, mixed);
    expect(mixed_result.eligible() &&
               mixed_result.unrelated_registration_count == 2U &&
               mixed_result.matching_cookie_count == 1U,
           "multiple resolved unrelated registrations are ignored when exactly one cookie matches the lease object");

    const std::array two_matches{
        matching_registration(target, 42, 1U),
        matching_registration(target, 43, 2U)};
    const auto ambiguous = detail::evaluate_registration_correlation(
        context, two_matches);
    expect(ambiguous.status == detail::RegistrationCorrelationStatus::
                                   AmbiguousMatchingRegistrations &&
               ambiguous.matching_cookie_count == 2U &&
               !ambiguous.eligible(),
           "two distinct cookies matching the same created object are ambiguous");
    const std::array three_matches{
        matching_registration(target, 42, 1U),
        matching_registration(target, 43, 2U),
        matching_registration(target, 44, 3U)};
    expect(detail::evaluate_registration_correlation(context, three_matches)
                   .matching_cookie_count == 3U,
           "ambiguous evidence reports the exact distinct matching-cookie count");
    const std::array duplicate_receipt{
        matching_registration(target, 42, 1U),
        matching_registration(target, 42, 2U)};
    expect(detail::evaluate_registration_correlation(
               context, duplicate_receipt)
               .eligible(),
           "duplicate receipts for one cookie do not invent a second registered window");

    auto preexisting_context = context;
    preexisting_context.forbidden_preexisting_hwnds.push_back(0x300U);
    const auto preexisting = detail::evaluate_registration_correlation(
        preexisting_context, one_match);
    expect(preexisting.status ==
               detail::RegistrationCorrelationStatus::PreexistingWindow,
           "same COM identity can never override permanent baseline HWND exclusion");

    auto wrong_cookie_hwnd = matching_registration(target);
    wrong_cookie_hwnd.cookie_resolved_window = 0x301U;
    const std::array wrong_cookie_hwnd_only{wrong_cookie_hwnd};
    expect(detail::evaluate_registration_correlation(
               context, wrong_cookie_hwnd_only)
               .status == detail::RegistrationCorrelationStatus::
                              ThreeWayWindowMismatch,
           "automation and cookie-resolved HWND mismatch blocks three-way authority");

    auto wrong_live_context = context;
    wrong_live_context.live_eligibility_window = 0x302U;
    expect(detail::evaluate_registration_correlation(
               wrong_live_context, one_match)
               .status == detail::RegistrationCorrelationStatus::
                              ThreeWayWindowMismatch,
           "automation and live-eligibility HWND mismatch blocks three-way authority");

    auto wrong_location = matching_registration(
        location_id(7U, std::byte{0x78}));
    const std::array wrong_location_only{wrong_location};
    expect(detail::evaluate_registration_correlation(
               context, wrong_location_only)
               .status == detail::RegistrationCorrelationStatus::
                              TargetLocationMismatch,
           "same object at a different FILE_ID_INFO is rejected");
    auto unavailable_location = matching_registration(target);
    unavailable_location.live_filesystem_location.reset();
    const std::array unavailable_location_only{unavailable_location};
    expect(detail::evaluate_registration_correlation(
               context, unavailable_location_only)
               .status == detail::RegistrationCorrelationStatus::
                              TargetLocationMismatch,
           "empty or inaccessible target location cannot satisfy positive attribution");

    auto revoked = matching_registration(target);
    revoked.revoked_before_issuance = true;
    const std::array revoked_only{revoked};
    expect(detail::evaluate_registration_correlation(context, revoked_only)
               .status == detail::RegistrationCorrelationStatus::
                              MatchingRegistrationRevoked,
           "WindowRevoked before token issuance makes the matching witness stale");

    auto stale_generation = matching_registration(target);
    stale_generation.subscription_generation = 6U;
    const std::array stale_generation_only{stale_generation};
    expect(detail::evaluate_registration_correlation(
               context, stale_generation_only)
               .status == detail::RegistrationCorrelationStatus::
                              SubscriptionGenerationMismatch,
           "receipt from another subscription generation is rejected");

    auto unresolved = matching_registration(target);
    unresolved.cookie_resolved = false;
    unresolved.cookie_resolved_window = 0U;
    const std::array unresolved_only{unresolved};
    expect(detail::evaluate_registration_correlation(context, unresolved_only)
               .status == detail::RegistrationCorrelationStatus::
                              UnresolvedRegistration,
           "FindWindowSW resolution failure cannot be guessed unrelated");
    const std::array matching_plus_unresolved{
        matching_registration(target, 42, 1U), unresolved};
    expect(detail::evaluate_registration_correlation(
               context, matching_plus_unresolved)
               .status == detail::RegistrationCorrelationStatus::
                              UnresolvedRegistration,
           "an unresolved concurrent registration blocks uniqueness even beside one apparent match");
}

void test_cleanup_authority_stays_on_the_exact_provisioning_lease() {
    auto facts = authorized_cleanup_facts();
    expect(detail::evaluate_lease_cleanup_authorization(facts) ==
               detail::LeaseCleanupAuthorization::Authorized,
           "exact session-owned CoCreate lease at the nonce target may call Quit");

    facts.navigation =
        detail::LeaseNavigationDisposition::PendingExpectedTarget;
    expect(detail::evaluate_lease_cleanup_authorization(facts) ==
               detail::LeaseCleanupAuthorization::Authorized,
           "a known pending navigation to the expected target retains lease cleanup authority");
    facts.navigation = detail::LeaseNavigationDisposition::NotStarted;
    expect(detail::evaluate_lease_cleanup_authorization(facts) ==
               detail::LeaseCleanupAuthorization::Authorized,
           "a retained object created by this CoCreate may be cleaned before navigation starts");

    facts = authorized_cleanup_facts();
    facts.retained_automation_object_available = false;
    expect(detail::evaluate_lease_cleanup_authorization(facts) ==
               detail::LeaseCleanupAuthorization::RetainedObjectUnavailable,
           "cleanup cannot fall back from a missing lease object to a raw HWND");
    facts = authorized_cleanup_facts();
    facts.lease_belongs_to_current_session = false;
    expect(detail::evaluate_lease_cleanup_authorization(facts) ==
               detail::LeaseCleanupAuthorization::WrongSession,
           "foreign session lease is rejected");
    facts = authorized_cleanup_facts();
    facts.created_by_current_cocreate = false;
    expect(detail::evaluate_lease_cleanup_authorization(facts) ==
               detail::LeaseCleanupAuthorization::WrongCreationAuthority,
           "an object not created by this CoCreate has no cleanup authority");
    facts = authorized_cleanup_facts();
    facts.subscription_generation_matches = false;
    expect(detail::evaluate_lease_cleanup_authorization(facts) ==
               detail::LeaseCleanupAuthorization::
                   SubscriptionGenerationMismatch,
           "stale subscription generation cannot authorize cleanup");
    facts = authorized_cleanup_facts();
    facts.canonical_identity_available = false;
    expect(detail::evaluate_lease_cleanup_authorization(facts) ==
               detail::LeaseCleanupAuthorization::CanonicalIdentityUnavailable,
           "missing canonical IUnknown identity blocks cleanup");
    facts = authorized_cleanup_facts();
    facts.canonical_identity_matches = false;
    expect(detail::evaluate_lease_cleanup_authorization(facts) ==
               detail::LeaseCleanupAuthorization::CanonicalIdentityMismatch,
           "cleanup lease identity mismatch never permits unsafe Quit");
    facts = authorized_cleanup_facts();
    facts.canonical_identity_ambiguous = true;
    expect(detail::evaluate_lease_cleanup_authorization(facts) ==
               detail::LeaseCleanupAuthorization::CanonicalIdentityAmbiguous,
           "ambiguous canonical identity blocks cleanup");
    facts = authorized_cleanup_facts();
    facts.window_not_preexisting = false;
    expect(detail::evaluate_lease_cleanup_authorization(facts) ==
               detail::LeaseCleanupAuthorization::PreexistingWindow,
           "a permanent baseline HWND exclusion also blocks exact-object Quit");
    facts = authorized_cleanup_facts();
    facts.window_identity_matches = false;
    expect(detail::evaluate_lease_cleanup_authorization(facts) ==
               detail::LeaseCleanupAuthorization::WindowIdentityMismatch,
           "cookie and lease HWND mismatch cannot authorize cleanup");
    facts = authorized_cleanup_facts();
    facts.navigation = detail::LeaseNavigationDisposition::LeftExpectedTarget;
    expect(detail::evaluate_lease_cleanup_authorization(facts) ==
               detail::LeaseCleanupAuthorization::NavigationUnsafe,
           "a lease observed away from the nonce target cannot close that object");
    facts.navigation = detail::LeaseNavigationDisposition::Unknown;
    expect(detail::evaluate_lease_cleanup_authorization(facts) ==
               detail::LeaseCleanupAuthorization::NavigationUnsafe,
           "unknown navigation history fails closed");
}

void test_public_cleanup_completion_requires_quit_lifecycle_and_no_orphan() {
    explorer::ExplorerProvisioningCleanupFacts facts;
    facts.cleanup_authorized = true;
    facts.native_quit_attempted = true;
    facts.native_quit_succeeded = true;
    facts.exact_hwnd_invalidated = true;
    facts.orphan_attribution_known = true;
    facts.browser_events_unadvised = true;
    facts.shell_events_unadvised = true;
    facts.browser_event_lifecycle_clean = true;
    facts.shell_event_lifecycle_clean = true;
    expect(facts.completed(),
           "exact Quit plus invalidation and both Unadvises completes cleanup");

    facts.orphan_attribution_known = false;
    expect(!facts.completed(),
           "unknown orphan attribution cannot become cleanup PASS");
    facts.orphan_attribution_known = true;

    facts.native_quit_succeeded = false;
    expect(!facts.completed(), "a failed Quit cannot become cleanup PASS");
    facts.native_quit_succeeded = true;
    facts.attributable_orphan = true;
    expect(!facts.completed(), "an attributable live frame blocks cleanup PASS");
    facts.attributable_orphan = false;
    facts.browser_events_unadvised = false;
    expect(!facts.completed(), "browser sink retirement is mandatory");
    facts.browser_events_unadvised = true;
    facts.shell_events_unadvised = false;
    expect(!facts.completed(), "Shell sink retirement is mandatory");
    facts.shell_events_unadvised = true;
    facts.browser_event_lifecycle_clean = false;
    expect(!facts.completed(), "browser sink lifecycle evidence must be clean");
    facts.browser_event_lifecycle_clean = true;
    facts.shell_event_lifecycle_clean = false;
    expect(!facts.completed(), "Shell sink lifecycle evidence must be clean");
}

void test_browser_navigation_history_fails_closed_without_urls() {
    expect(!detail::browser_navigation_history_ambiguous(0U, std::nullopt) &&
               !detail::browser_navigation_history_ambiguous(1U,
                                                               std::nullopt),
           "zero or one pre-issuance same-object completion is an optional readiness hint");
    expect(detail::browser_navigation_history_ambiguous(2U, std::nullopt),
           "multiple pre-issuance same-object completions make URL-free history ambiguous");
    expect(!detail::browser_navigation_history_ambiguous(1U, 1U),
           "the frozen issuance completion count remains valid while unchanged");
    expect(detail::browser_navigation_history_ambiguous(2U, 1U) &&
               detail::browser_navigation_history_ambiguous(1U, 0U),
           "any post-issuance matching completion increment fails closed");
}

void test_fixed_three_attempt_provisioning_stability_gate() {
    constexpr std::array requirements{
        &detail::ProvisioningAttemptSummary::baseline_exclusion_complete,
        &detail::ProvisioningAttemptSummary::registration_subscription_active,
        &detail::ProvisioningAttemptSummary::exactly_one_target,
        &detail::ProvisioningAttemptSummary::target_not_preexisting,
        &detail::ProvisioningAttemptSummary::matching_cookie_unique,
        &detail::ProvisioningAttemptSummary::canonical_identity_matches,
        &detail::ProvisioningAttemptSummary::three_way_window_matches,
        &detail::ProvisioningAttemptSummary::exact_target_location,
        &detail::ProvisioningAttemptSummary::full_eligibility_passed,
        &detail::ProvisioningAttemptSummary::safe_cleanup_succeeded,
        &detail::ProvisioningAttemptSummary::no_attributable_orphan,
        &detail::ProvisioningAttemptSummary::existing_windows_untouched,
        &detail::ProvisioningAttemptSummary::translation_not_attempted,
    };
    for (const auto requirement : requirements) {
        auto failed = passing_attempt(1U);
        failed.*requirement = false;
        expect(!detail::provisioning_attempt_passed(failed),
               "every provision-only safety fact is mandatory");
    }

    const std::array three_passes{
        passing_attempt(1U), passing_attempt(2U), passing_attempt(3U)};
    const auto passed =
        detail::evaluate_provisioning_stability(three_passes);
    expect(passed.status ==
                   detail::ProvisioningStabilityGateStatus::Pass &&
               passed.passing_attempt_count == 3U &&
               passed.attempt_sequence_valid &&
               !passed.fixed_attempt_count_violated,
           "exactly three ordered provision-only passes open the stability gate");

    const auto not_started = detail::evaluate_provisioning_stability(
        std::span<const detail::ProvisioningAttemptSummary>{});
    expect(not_started.status ==
               detail::ProvisioningStabilityGateStatus::NotRun,
           "zero provisioning attempts leave the gate not run");
    const std::array pass_prefix{
        passing_attempt(1U), passing_attempt(2U)};
    const auto incomplete =
        detail::evaluate_provisioning_stability(pass_prefix);
    expect(incomplete.status ==
                   detail::ProvisioningStabilityGateStatus::NotRun &&
               incomplete.attempt_sequence_valid,
           "an ordered all-pass prefix shorter than three is incomplete, not pass");

    auto second_failure = passing_attempt(2U);
    second_failure.safe_cleanup_succeeded = false;
    const std::array failed_attempts{
        passing_attempt(1U), second_failure, passing_attempt(3U)};
    const auto blocked =
        detail::evaluate_provisioning_stability(failed_attempts);
    expect(blocked.status ==
                   detail::ProvisioningStabilityGateStatus::Blocked &&
               blocked.passing_attempt_count == 2U &&
               blocked.first_failing_attempt == 2U,
           "any one of the fixed three failures blocks without retry-until-pass semantics");

    auto first_failure = passing_attempt(1U);
    first_failure.exact_target_location = false;
    const std::array stopped_after_failure{first_failure};
    expect(detail::evaluate_provisioning_stability(stopped_after_failure)
               .status == detail::ProvisioningStabilityGateStatus::Blocked,
           "an observed failure blocks immediately even when later attempts are not run");

    const std::array too_many{
        passing_attempt(1U), passing_attempt(2U), passing_attempt(3U),
        passing_attempt(4U)};
    const auto retried = detail::evaluate_provisioning_stability(too_many);
    expect(retried.status ==
                   detail::ProvisioningStabilityGateStatus::Blocked &&
               retried.fixed_attempt_count_violated,
           "a fourth attempt violates the fixed gate instead of refreshing evidence");

    const std::array duplicate_index{
        passing_attempt(1U), passing_attempt(1U), passing_attempt(3U)};
    const auto duplicate =
        detail::evaluate_provisioning_stability(duplicate_index);
    expect(duplicate.status ==
                   detail::ProvisioningStabilityGateStatus::Blocked &&
               !duplicate.attempt_sequence_valid,
           "duplicate attempt indices cannot masquerade as three independent runs");

    const std::array out_of_order{
        passing_attempt(2U), passing_attempt(1U), passing_attempt(3U)};
    const auto reordered =
        detail::evaluate_provisioning_stability(out_of_order);
    expect(reordered.status ==
                   detail::ProvisioningStabilityGateStatus::Blocked &&
               !reordered.attempt_sequence_valid,
           "out-of-order attempt indices are rejected; the required sequence is exactly 1, 2, 3");
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
    test_baseline_exclusion_uses_only_reliable_hwnd_identity();
    test_shell_receipts_are_monotonic_and_fail_closed_on_exhaustion();
    test_registration_correlation_requires_positive_authority();
    test_cleanup_authority_stays_on_the_exact_provisioning_lease();
    test_public_cleanup_completion_requires_quit_lifecycle_and_no_orphan();
    test_browser_navigation_history_fails_closed_without_urls();
    test_fixed_three_attempt_provisioning_stability_gate();
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
