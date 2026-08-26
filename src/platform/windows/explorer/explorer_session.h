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

struct ExplorerProvisioningFacts {
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
    bool window_disappeared{};
    bool token_retired{};
    std::optional<ExplorerDiagnostic> diagnostic;

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
