#pragma once

#include "core/geometry/geometry.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace panebind::platform::windows::explorer {

namespace detail {
class ExplorerTokenLedger;
class ExplorerSessionDiagnostics;
}

class ExplorerTestSession;
class ExplorerWindowOperations;
struct ExplorerProvisionResult;

// This token is deliberately application- and fixture-specific. It represents
// only the single Explorer window isolated by an ExplorerTestSession; it is not
// a generic third-party-window capability and has no native-handle conversion.
class ExplorerWindowToken final {
public:
    ExplorerWindowToken() = delete;

    [[nodiscard]] std::uint64_t logical_id() const noexcept {
        return logical_id_;
    }

    [[nodiscard]] std::uint64_t generation() const noexcept {
        return generation_;
    }

    friend bool operator==(const ExplorerWindowToken&,
                           const ExplorerWindowToken&) = default;

private:
    ExplorerWindowToken(std::uint64_t controller_authority,
                        std::uint64_t session_authority,
                        std::uint64_t logical_id,
                        std::uint64_t generation) noexcept
        : controller_authority_(controller_authority),
          session_authority_(session_authority),
          logical_id_(logical_id),
          generation_(generation) {}

    std::uint64_t controller_authority_{};
    std::uint64_t session_authority_{};
    std::uint64_t logical_id_{};
    std::uint64_t generation_{};

    friend class detail::ExplorerTokenLedger;
    friend class ExplorerTestSession;
    friend class ExplorerWindowOperations;
    friend class detail::ExplorerSessionDiagnostics;
};

enum class ExplorerEligibilityReason {
    Eligible,
    InvalidTargetDirectory,
    TargetDirectoryNotEmpty,
    ComApartmentUnavailable,
    InventoryUnavailable,
    InventoryUnstable,
    BaselineWindowIdentityUnavailable,
    ShellEventSubscriptionUnavailable,
    BrowserEventSubscriptionUnavailable,
    ShellEventStreamInvalid,
    RegistrationNotObserved,
    RegistrationResolutionFailed,
    RegistrationRevoked,
    CanonicalIdentityMismatch,
    SubscriptionGenerationMismatch,
    ShellWindowCreationFailed,
    ShellWindowHandleMissing,
    PreexistingWindow,
    ReusedExistingWindow,
    AmbiguousCandidate,
    BaselineChanged,
    LocationNotReady,
    LocationMismatch,
    WindowDestroyed,
    ProcessOpenFailed,
    ProcessExited,
    WrongProcess,
    WrongThread,
    WrongImage,
    WrongClass,
    NotTopLevel,
    ChildWindow,
    OwnedWindow,
    Invisible,
    Cloaked,
    Minimized,
    Maximized,
    WrongVirtualDesktop,
    SecurityQueryFailed,
    UserMismatch,
    SessionMismatch,
    IntegrityMismatch,
    Elevated,
    UiAccess,
    AppContainer,
    GeometryCaptureFailed,
    DpiContextMismatch,
    MonitorUnavailable,
    MonitorChanged,
    DpiChanged,
    UnsafeDelta,
    StaleToken,
    OperationLimitReached,
    OperationSequenceViolation,
    AuthorityExhausted,
    GenerationExhausted,
    EmptyGeometry,
    ResizeRejected,
    ArithmeticOverflow,
    NativeCoordinateOutOfRange,
    NativeApplyFailed,
    PostVerificationFailed,
    TargetInvalidated,
    SafeCleanupNotPerformed,
};

enum class ExplorerOperationStage {
    BaselineInventory,
    ShellWindowCreation,
    Navigation,
    CandidateResolution,
    Eligibility,
    Preflight,
    NativeApply,
    PostVerification,
    Restore,
    Cleanup,
};

enum class ExplorerDiagnosticDomain {
    Win32,
    HResult,
    Shell,
    Adapter,
};

struct ExplorerDiagnostic {
    ExplorerDiagnosticDomain domain{ExplorerDiagnosticDomain::Adapter};
    std::uint64_t code{};
    std::string api;
    std::string detail;
    // Populated only for diagnostics originating at the Shell automation
    // boundary. This is a fixed, path-free label derived directly from
    // ShellAutomationStage; free-form detail is never emitted as evidence.
    std::string shell_stage;
};

struct ExplorerProcessSecurityFacts {
    std::uint32_t integrity_rid{};
    std::uint32_t session_id{};
    bool elevated{};
    bool ui_access{};
    bool app_container{};

    friend bool operator==(const ExplorerProcessSecurityFacts&,
                           const ExplorerProcessSecurityFacts&) = default;
};

struct ExplorerWindowSnapshot {
    core::geometry::Rect positioning_rect;
    core::geometry::Rect visible_rect;
    std::uint32_t process_id{};
    std::uint32_t thread_id{};
    std::wstring process_image_path;
    std::wstring window_class;
    std::uint32_t dpi{};
    std::wstring monitor_device_name;
    core::geometry::Rect monitor_rect;
    core::geometry::Rect monitor_work_area;
    ExplorerProcessSecurityFacts controller_security;
    ExplorerProcessSecurityFacts target_security;
    bool root_top_level{};
    bool visible{};
    bool cloaked{};
    bool minimized{};
    bool maximized{};
    bool on_current_virtual_desktop{};
    bool exact_test_location{};

    friend bool operator==(const ExplorerWindowSnapshot&,
                           const ExplorerWindowSnapshot&) = default;
};

struct ExplorerProvisioningCleanupFacts {
    bool cleanup_authorized{};
    bool native_quit_attempted{};
    bool native_quit_succeeded{};
    bool matching_registration_revoked{};
    bool exact_hwnd_invalidated{};
    bool orphan_attribution_known{};
    bool attributable_orphan{};
    bool browser_events_unadvised{};
    bool shell_events_unadvised{};
    bool browser_event_lifecycle_clean{};
    bool shell_event_lifecycle_clean{};

    [[nodiscard]] bool completed() const noexcept {
        return cleanup_authorized && native_quit_attempted &&
               native_quit_succeeded && browser_events_unadvised &&
               shell_events_unadvised && browser_event_lifecycle_clean &&
               shell_event_lifecycle_clean &&
               orphan_attribution_known &&
               (matching_registration_revoked || exact_hwnd_invalidated) &&
               !attributable_orphan;
    }
};

struct ExplorerProvisioningFacts {
    std::size_t baseline_total_shell_entries{};
    std::size_t baseline_reliable_shell_entries{};
    std::size_t baseline_reliable_unique_hwnd_count{};
    std::size_t forbidden_preexisting_hwnd_count{};
    std::size_t baseline_valid_location_count{};
    std::size_t baseline_empty_location_count{};
    std::size_t baseline_inaccessible_location_count{};
    bool baseline_exclusion_complete{};

    bool shell_subscription_advised{};
    std::uint64_t shell_subscription_generation{};
    std::uint64_t shell_callback_count{};
    std::uint64_t shell_registered_event_count{};
    std::uint64_t shell_revoked_event_count{};
    std::uint64_t shell_malformed_event_count{};
    std::uint64_t shell_overflow_event_count{};
    std::uint64_t shell_wrong_thread_event_count{};
    std::uint64_t shell_post_retirement_event_count{};
    bool shell_subscription_generation_mismatch{};
    bool shell_cookie_lifecycle_ambiguous{};

    bool browser_subscription_advised{};
    std::uint64_t browser_navigate_complete_count{};
    std::uint64_t browser_matching_navigate_complete_count{};
    std::uint64_t issued_matching_navigate_complete_count{};
    std::uint64_t browser_unrelated_navigate_complete_count{};
    std::uint64_t browser_identity_query_failure_count{};
    std::uint64_t browser_quit_event_count{};
    std::uint64_t browser_malformed_event_count{};
    std::uint64_t browser_overflow_event_count{};
    std::uint64_t browser_wrong_thread_event_count{};
    std::uint64_t browser_post_retirement_event_count{};
    bool browser_navigation_history_ambiguous{};

    std::size_t registered_cookie_count{};
    std::size_t revoked_cookie_count{};
    std::size_t unrelated_registration_count{};
    std::size_t unresolved_registration_count{};
    std::size_t shared_window_identity_conflict_count{};
    std::size_t matching_registration_count{};
    std::optional<std::int32_t> matching_cookie;
    bool matching_cookie_find_window_resolved{};
    bool canonical_iunknown_identity_matches{};
    bool lease_hwnd_resolved{};
    bool cookie_hwnd_resolved{};
    bool live_eligibility_hwnd_resolved{};
    bool lease_cookie_live_hwnd_match{};
    bool target_hwnd_preexisting{};
    bool exact_target_location{};
    bool token_issued{};
    ExplorerProvisioningCleanupFacts cleanup;

    // Compatibility fields retained for the existing evidence harness. New
    // recovery evidence must use the structured fields above; no post-launch
    // global inventory delta is used as authority.
    std::size_t preexisting_window_count{};
    std::size_t post_navigation_window_count{};
    std::size_t new_candidate_count{};
    bool retained_window_was_new_before_navigation{};
    bool baseline_facts_unchanged{};
    bool exact_unique_test_location{};
    ExplorerWindowSnapshot initial_snapshot;
};

struct ExplorerCaptureResult {
    ExplorerEligibilityReason reason{
        ExplorerEligibilityReason::TargetInvalidated};
    ExplorerOperationStage stage{ExplorerOperationStage::Preflight};
    std::optional<ExplorerWindowSnapshot> snapshot;
    std::optional<ExplorerDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return reason == ExplorerEligibilityReason::Eligible &&
               snapshot.has_value();
    }
};

struct ExplorerTranslationDelta {
    core::geometry::Distance dx{};
    core::geometry::Distance dy{};

    friend bool operator==(const ExplorerTranslationDelta&,
                           const ExplorerTranslationDelta&) = default;
};

struct ExplorerSafeDeltaResult {
    ExplorerEligibilityReason reason{ExplorerEligibilityReason::UnsafeDelta};
    std::optional<ExplorerTranslationDelta> delta;
    std::optional<core::geometry::Rect> target_visible_rect;

    [[nodiscard]] bool succeeded() const noexcept {
        return reason == ExplorerEligibilityReason::Eligible &&
               delta.has_value() && target_visible_rect.has_value();
    }
};

struct ExplorerOperationReceipt {
    ExplorerWindowToken token;
    std::optional<ExplorerWindowSnapshot> before;
    core::geometry::Rect requested_visible_rect;
    std::optional<core::geometry::Rect> requested_positioning_rect;
    std::optional<ExplorerWindowSnapshot> actual;
    bool visible_target_verified{};
    bool positioning_target_verified{};
    bool size_preserved{};
    bool identity_stable{};
    bool location_stable{};
    bool monitor_and_dpi_stable{};
};

struct ExplorerOperationResult {
    std::uint64_t operation_id{};
    ExplorerEligibilityReason reason{
        ExplorerEligibilityReason::TargetInvalidated};
    ExplorerOperationStage stage{ExplorerOperationStage::Preflight};
    bool native_apply_attempted{};
    bool native_outcome_known{true};
    bool cleanup_operation{};
    std::optional<ExplorerOperationReceipt> receipt;
    std::optional<ExplorerDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return reason == ExplorerEligibilityReason::Eligible;
    }
};

struct ExplorerCleanupResult {
    ExplorerEligibilityReason reason{
        ExplorerEligibilityReason::SafeCleanupNotPerformed};
    bool native_close_attempted{};
    bool native_close_succeeded{};
    bool window_disappeared{};
    bool token_retired{};
    std::optional<ExplorerDiagnostic> diagnostic;
    bool matching_registration_revoked{};
    bool exact_hwnd_invalidated{};
    bool orphan_attribution_known{};
    bool attributable_orphan{};
    bool browser_events_unadvised{};
    bool shell_events_unadvised{};
    bool browser_event_lifecycle_clean{};
    bool shell_event_lifecycle_clean{};

    [[nodiscard]] bool succeeded() const noexcept {
        return reason == ExplorerEligibilityReason::Eligible &&
               window_disappeared && token_retired;
    }
};

// The session owns an STA Shell automation object. Provisioning, every method,
// and normal destruction must occur on the provisioning thread. A misplaced
// destruction is fail-safe (no cross-apartment COM call), but intentionally
// leaks the retained proxy because releasing it on the wrong STA is unsafe.
class ExplorerTestSession final {
public:
    ~ExplorerTestSession();

    ExplorerTestSession(const ExplorerTestSession&) = delete;
    ExplorerTestSession& operator=(const ExplorerTestSession&) = delete;
    ExplorerTestSession(ExplorerTestSession&&) = delete;
    ExplorerTestSession& operator=(ExplorerTestSession&&) = delete;

    [[nodiscard]] static ExplorerProvisionResult provision(
        const std::filesystem::path& unique_empty_test_directory,
        std::chrono::milliseconds readiness_timeout =
            std::chrono::seconds{8});

    [[nodiscard]] const ExplorerProvisioningFacts& provisioning_facts()
        const noexcept;
    [[nodiscard]] const ExplorerWindowToken& token() const noexcept;
    [[nodiscard]] bool contains(const ExplorerWindowToken& token) noexcept;

    // Closing is optional and fail-closed. It acts only through the exact
    // retained Shell automation object after another complete live eligibility
    // check. Destruction never broadcasts WM_CLOSE or terminates explorer.exe.
    [[nodiscard]] ExplorerCleanupResult close_test_window(
        const ExplorerWindowToken& token,
        std::chrono::milliseconds disappearance_timeout =
            std::chrono::seconds{3});

private:
    struct Impl;
    explicit ExplorerTestSession(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;

    friend class ExplorerWindowOperations;
    friend class detail::ExplorerSessionDiagnostics;
};

struct ExplorerProvisionResult {
    ExplorerEligibilityReason reason{
        ExplorerEligibilityReason::InvalidTargetDirectory};
    std::unique_ptr<ExplorerTestSession> session;
    std::optional<ExplorerDiagnostic> diagnostic;
    // Always populated as far as the attempt progressed, including failures.
    ExplorerProvisioningFacts facts;
    ExplorerProvisioningCleanupFacts cleanup;

    [[nodiscard]] bool succeeded() const noexcept {
        return reason == ExplorerEligibilityReason::Eligible &&
               session != nullptr;
    }
};

class ExplorerWindowOperations final {
public:
    explicit ExplorerWindowOperations(ExplorerTestSession& session) noexcept;

    [[nodiscard]] ExplorerCaptureResult capture(
        const ExplorerWindowToken& token);
    [[nodiscard]] ExplorerOperationResult apply_single(
        const ExplorerWindowToken& token,
        const core::geometry::Rect& target_visible_rect);
    [[nodiscard]] ExplorerOperationResult restore(
        const ExplorerWindowToken& token);

private:
    ExplorerTestSession* session_{};
};

// Deterministic test-fixture policy, not a product movement default. The first
// candidate that keeps the complete visible frame inside the original monitor
// work area is selected.
[[nodiscard]] ExplorerSafeDeltaResult select_safe_test_delta(
    const ExplorerWindowSnapshot& snapshot) noexcept;

} // namespace panebind::platform::windows::explorer
