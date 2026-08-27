#pragma once

#include "platform/windows/explorer/explorer_session.h"
#include "platform/windows/explorer/explorer_shell_inventory.h"
#include "platform/windows/operations/window_translation.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace panebind::platform::windows::explorer::detail {

using NativeWindowKey = std::uintptr_t;

class MonotonicIdSource final {
public:
    explicit MonotonicIdSource(std::uint64_t first = 1U) noexcept;

    MonotonicIdSource(const MonotonicIdSource&) = delete;
    MonotonicIdSource& operator=(const MonotonicIdSource&) = delete;

    [[nodiscard]] std::uint64_t issue() noexcept;

private:
    std::atomic<std::uint64_t> next_;
};

struct ExplorerLedgerEntry {
    std::uint64_t logical_id{};
    std::uint64_t generation{};
    NativeWindowKey native_key{};
    std::uint32_t process_id{};
    std::uint32_t thread_id{};
    std::int32_t registration_cookie{};
    std::uint64_t subscription_generation{};
};

enum class ExplorerLedgerIssueStatus {
    Succeeded,
    InvalidNativeKey,
    DuplicateNativeKey,
    InvalidRegistrationAuthority,
    AuthorityExhausted,
    GenerationExhausted,
};

struct ExplorerLedgerIssueResult {
    ExplorerLedgerIssueStatus status{
        ExplorerLedgerIssueStatus::InvalidNativeKey};
    std::optional<ExplorerWindowToken> token;
};

class ExplorerTokenLedger final {
public:
    ExplorerTokenLedger() noexcept;
    ExplorerTokenLedger(std::uint64_t controller_authority,
                        std::uint64_t session_authority,
                        std::uint64_t next_logical_id,
                        std::uint64_t next_generation) noexcept;

    [[nodiscard]] ExplorerLedgerIssueResult issue(
        NativeWindowKey native_key,
        std::uint32_t process_id,
        std::uint32_t thread_id,
        std::int32_t registration_cookie,
        std::uint64_t subscription_generation);
    [[nodiscard]] bool retire_native(NativeWindowKey native_key) noexcept;
    [[nodiscard]] std::optional<ExplorerLedgerEntry> resolve(
        const ExplorerWindowToken& token) const noexcept;
    [[nodiscard]] bool contains(
        const ExplorerWindowToken& token) const noexcept;
    [[nodiscard]] std::uint64_t session_authority() const noexcept {
        return session_authority_;
    }

private:
    std::uint64_t controller_authority_{};
    std::uint64_t session_authority_{};
    std::uint64_t next_logical_id_{1U};
    std::uint64_t next_generation_{1U};
    std::map<std::uint64_t, ExplorerLedgerEntry> active_by_logical_id_;
    std::map<NativeWindowKey, std::uint64_t> logical_id_by_native_key_;
};

class SinglePrimaryApplyGuard final {
public:
    [[nodiscard]] bool try_consume() noexcept {
        if (consumed_) {
            return false;
        }
        consumed_ = true;
        return true;
    }

    [[nodiscard]] bool consumed() const noexcept { return consumed_; }

private:
    bool consumed_{};
};

[[nodiscard]] ExplorerEligibilityReason evaluate_primary_apply_gate(
    bool token_active,
    bool allowance_consumed) noexcept;

[[nodiscard]] ExplorerEligibilityReason evaluate_restore_gate(
    bool token_active,
    bool primary_attempted,
    bool primary_before_available,
    bool primary_actual_available,
    bool restore_allowance_consumed) noexcept;

// Keeps creation failure and delayed frame readiness distinguishable without
// exposing a raw Shell automation object or inventing a fallback target.
[[nodiscard]] ExplorerEligibilityReason map_shell_creation_stage(
    ShellAutomationStage stage) noexcept;

[[nodiscard]] constexpr bool readiness_deadline_active(
    const std::chrono::steady_clock::time_point deadline,
    const std::chrono::steady_clock::time_point now) noexcept {
    return now < deadline;
}

[[nodiscard]] constexpr bool readiness_acceptance_allowed(
    const bool stable,
    const std::chrono::steady_clock::time_point deadline,
    const std::chrono::steady_clock::time_point now) noexcept {
    return stable && readiness_deadline_active(deadline, now);
}

[[nodiscard]] bool operation_snapshot_matches_expected(
    const ExplorerWindowSnapshot& expected,
    const ExplorerWindowSnapshot& current) noexcept;

enum class BaselineLocationDisposition {
    Valid,
    Empty,
    Inaccessible,
};

struct BaselineEntryModel {
    NativeWindowKey native_key{};
    bool native_key_reliable{};
    BaselineLocationDisposition location{
        BaselineLocationDisposition::Inaccessible};
};

enum class BaselineExclusionStatus {
    Complete,
    Blocked,
};

struct BaselineExclusionEvaluation {
    BaselineExclusionStatus status{BaselineExclusionStatus::Blocked};
    std::size_t total_entry_count{};
    std::size_t reliable_entry_count{};
    std::size_t reliable_hwnd_count{};
    std::size_t valid_location_count{};
    std::size_t empty_location_count{};
    std::size_t inaccessible_location_count{};
    std::vector<NativeWindowKey> forbidden_preexisting_hwnds;

    [[nodiscard]] bool complete() const noexcept {
        return status == BaselineExclusionStatus::Complete;
    }
};

// A location is only a diagnostic fact for a preexisting entry. Completeness
// depends solely on every entry providing a reliable, non-zero HWND. Passing a
// prior forbidden set models the session invariant that an excluded numeric
// HWND is never removed, even if Windows later reuses that value.
[[nodiscard]] BaselineExclusionEvaluation evaluate_baseline_exclusion(
    std::span<const BaselineEntryModel> entries,
    std::span<const NativeWindowKey> previously_forbidden = {});

struct InventoryLocationFingerprint {
    ShellLocationStatus status{ShellLocationStatus::Unavailable};
    ShellLocationSource source{ShellLocationSource::None};
    std::optional<FilesystemLocationIdentity> filesystem_location;
    std::optional<OpaqueLocationFingerprint> opaque_location;

    friend bool operator==(const InventoryLocationFingerprint&,
                           const InventoryLocationFingerprint&) = default;
};

struct InventoryFingerprint {
    NativeWindowKey native_key{};
    std::size_t shell_entry_count{};
    // Keep each status, file identity, and opaque LocationURL digest together.
    // Sorting these compound records makes Shell enumeration order irrelevant
    // without losing which witnesses belonged to which entry.
    std::vector<InventoryLocationFingerprint> locations;

    friend bool operator==(const InventoryFingerprint&,
                           const InventoryFingerprint&) = default;
};

struct InventoryModel {
    bool complete{};
    std::vector<InventoryFingerprint> windows;

    friend bool operator==(const InventoryModel&,
                           const InventoryModel&) = default;
};

struct CandidateEvaluation {
    ExplorerEligibilityReason reason{
        ExplorerEligibilityReason::InventoryUnavailable};
    std::size_t new_candidate_count{};
    bool baseline_unchanged{};
    bool exact_unique_location{};
};

[[nodiscard]] CandidateEvaluation evaluate_candidate_inventory(
    const InventoryModel& baseline,
    const InventoryModel& post_navigation,
    NativeWindowKey retained_window,
    const FilesystemLocationIdentity& target_location) noexcept;

enum class ShellRegistrationEventKind {
    WindowRegistered,
    WindowRevoked,
};

struct ShellRegistrationReceipt {
    std::uint64_t sequence{};
    std::uint64_t subscription_generation{};
    std::int32_t cookie{};
    ShellRegistrationEventKind kind{
        ShellRegistrationEventKind::WindowRegistered};
};

enum class ShellReceiptIssueStatus {
    Issued,
    SequenceExhausted,
};

struct ShellReceiptIssueResult {
    ShellReceiptIssueStatus status{ShellReceiptIssueStatus::SequenceExhausted};
    std::optional<ShellRegistrationReceipt> receipt;
};

// Provides the small monotonic receipt capture needed by a COM event sink.
// It has no native side effects and permanently fails closed on exhaustion.
class ShellReceiptSequence final {
public:
    explicit ShellReceiptSequence(std::uint64_t first_sequence = 1U) noexcept;

    ShellReceiptSequence(const ShellReceiptSequence&) = delete;
    ShellReceiptSequence& operator=(const ShellReceiptSequence&) = delete;

    [[nodiscard]] ShellReceiptIssueResult issue(
        ShellRegistrationEventKind kind,
        std::uint64_t subscription_generation,
        std::int32_t cookie) noexcept;

private:
    MonotonicIdSource sequence_;
};

struct RegistrationWitness {
    std::uint64_t receipt_sequence{};
    std::uint64_t subscription_generation{};
    std::int32_t cookie{};
    bool cookie_resolved{};
    bool canonical_com_identity_matches{};
    NativeWindowKey cookie_resolved_window{};
    std::optional<FilesystemLocationIdentity> live_filesystem_location;
    bool revoked_before_issuance{};
};

struct RegistrationCorrelationContext {
    std::uint64_t subscription_generation{};
    NativeWindowKey automation_window{};
    NativeWindowKey live_eligibility_window{};
    FilesystemLocationIdentity expected_target_location;
    std::vector<NativeWindowKey> forbidden_preexisting_hwnds;
};

enum class RegistrationCorrelationStatus {
    NoRegistration,
    UnrelatedRegistrationsOnly,
    UnresolvedRegistration,
    SubscriptionGenerationMismatch,
    SharedWindowIdentityConflict,
    MatchingRegistrationRevoked,
    PreexistingWindow,
    ThreeWayWindowMismatch,
    TargetLocationMismatch,
    AmbiguousMatchingRegistrations,
    Eligible,
};

struct RegistrationCorrelationEvaluation {
    RegistrationCorrelationStatus status{
        RegistrationCorrelationStatus::NoRegistration};
    std::size_t observed_registration_count{};
    std::size_t unrelated_registration_count{};
    std::size_t unresolved_registration_count{};
    std::size_t shared_window_identity_conflict_count{};
    std::size_t matching_cookie_count{};
    std::optional<std::int32_t> matching_cookie;

    [[nodiscard]] bool eligible() const noexcept {
        return status == RegistrationCorrelationStatus::Eligible &&
               matching_cookie_count == 1U && matching_cookie.has_value();
    }
};

// Only a resolved registration for the same canonical COM object may become
// authority. The target's exact FILE_ID_INFO remains a hard gate, independent
// of baseline entries whose locations may be opaque.
[[nodiscard]] RegistrationCorrelationEvaluation
evaluate_registration_correlation(
    const RegistrationCorrelationContext& context,
    std::span<const RegistrationWitness> witnesses);

enum class LeaseNavigationDisposition {
    NotStarted,
    PendingExpectedTarget,
    ExactExpectedTarget,
    LeftExpectedTarget,
    Unknown,
};

struct LeaseCleanupFacts {
    bool retained_automation_object_available{};
    bool lease_belongs_to_current_session{};
    bool created_by_current_cocreate{};
    bool subscription_generation_matches{};
    bool canonical_identity_available{};
    bool canonical_identity_matches{};
    bool canonical_identity_ambiguous{};
    bool window_not_preexisting{};
    bool window_identity_matches{};
    LeaseNavigationDisposition navigation{
        LeaseNavigationDisposition::Unknown};
};

enum class LeaseCleanupAuthorization {
    Authorized,
    RetainedObjectUnavailable,
    WrongSession,
    WrongCreationAuthority,
    SubscriptionGenerationMismatch,
    CanonicalIdentityUnavailable,
    CanonicalIdentityMismatch,
    CanonicalIdentityAmbiguous,
    PreexistingWindow,
    WindowIdentityMismatch,
    NavigationUnsafe,
};

[[nodiscard]] LeaseCleanupAuthorization
evaluate_lease_cleanup_authorization(
    const LeaseCleanupFacts& facts) noexcept;

// The browser sink intentionally does not retain or trust URLs. Before token
// issuance, at most one same-object NavigateComplete2 is an admissible
// readiness hint. After issuance, any increase from the frozen counter makes
// navigation history unknowable and permanently fails closed.
[[nodiscard]] bool browser_navigation_history_ambiguous(
    std::uint64_t current_matching_navigate_complete_count,
    std::optional<std::uint64_t> issued_baseline_count) noexcept;

struct ProvisioningAttemptSummary {
    std::size_t attempt_index{};
    bool baseline_exclusion_complete{};
    bool registration_subscription_active{};
    bool exactly_one_target{};
    bool target_not_preexisting{};
    bool matching_cookie_unique{};
    bool canonical_identity_matches{};
    bool three_way_window_matches{};
    bool exact_target_location{};
    bool full_eligibility_passed{};
    bool safe_cleanup_succeeded{};
    bool no_attributable_orphan{};
    bool existing_windows_untouched{};
    bool translation_not_attempted{};
};

enum class ProvisioningStabilityGateStatus {
    NotRun,
    Pass,
    Blocked,
};

struct ProvisioningStabilityEvaluation {
    ProvisioningStabilityGateStatus status{
        ProvisioningStabilityGateStatus::NotRun};
    std::size_t attempt_count{};
    std::size_t passing_attempt_count{};
    std::optional<std::size_t> first_failing_attempt;
    bool fixed_attempt_count_violated{};
    bool attempt_sequence_valid{true};
};

[[nodiscard]] bool provisioning_attempt_passed(
    const ProvisioningAttemptSummary& attempt) noexcept;

// Three is a fixed gate, not a retry budget. A recorded failure blocks
// immediately; all-pass prefixes shorter than three remain NOT RUN, while any
// attempt beyond the fixed three is a protocol violation and blocks.
[[nodiscard]] ProvisioningStabilityEvaluation
evaluate_provisioning_stability(
    std::span<const ProvisioningAttemptSummary> attempts) noexcept;

struct EligibilityModelFacts {
    bool window_exists{};
    bool process_alive{};
    bool process_id_stable{};
    bool thread_id_stable{};
    bool image_matches{};
    bool class_allowed{};
    bool root_is_self{};
    bool child_style{};
    bool has_owner{};
    bool visible{};
    bool cloaked{};
    bool minimized{};
    bool maximized{};
    bool current_virtual_desktop{};
    bool security_query_succeeded{};
    bool same_user{};
    bool same_session{};
    bool same_integrity{};
    bool medium_integrity{};
    bool elevated{};
    bool ui_access{};
    bool app_container{};
    bool shell_entry_unique{};
    bool location_exact{};
    bool geometry_available{};
    bool dpi_context_supported{};
    bool monitor_available{};
    bool monitor_stable{};
    bool dpi_stable{};
};

[[nodiscard]] ExplorerEligibilityReason evaluate_eligibility_model(
    const EligibilityModelFacts& facts) noexcept;

[[nodiscard]] bool rect_is_contained(
    const core::geometry::Rect& inner,
    const core::geometry::Rect& outer) noexcept;

[[nodiscard]] ExplorerSafeDeltaResult select_safe_delta_from_candidates(
    const ExplorerWindowSnapshot& snapshot,
    std::span<const ExplorerTranslationDelta> candidates) noexcept;

struct PostVerificationModel {
    bool token_identity_stable{};
    bool process_identity_stable{};
    bool thread_identity_stable{};
    bool image_and_class_stable{};
    bool location_stable{};
    bool security_stable{};
    bool monitor_stable{};
    bool dpi_stable{};
    bool size_preserved{};
    bool visible_target_matches{};
    bool positioning_target_matches{};
};

[[nodiscard]] ExplorerEligibilityReason evaluate_post_verification(
    const PostVerificationModel& verification) noexcept;

[[nodiscard]] ExplorerEligibilityReason map_translation_status(
    operations::window_translation::TranslationPreparationStatus status)
    noexcept;

struct ExplorerDiagnosticNativeIdentity {
    NativeWindowKey native_key{};
    std::uint32_t process_id{};
    std::uint32_t thread_id{};
};

// Fixture-only read path for correlating session evidence with observer JSONL.
// It is intentionally absent from explorer_session.h and cannot authorize an
// operation; all mutations still require ExplorerWindowToken.
class ExplorerSessionDiagnostics final {
public:
    [[nodiscard]] static ExplorerDiagnosticNativeIdentity read(
        const ExplorerTestSession& session) noexcept;
};

} // namespace panebind::platform::windows::explorer::detail
