#pragma once

#include "core/geometry/geometry.h"
#include "platform/windows/companion/companion_protocol.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace panebind::platform::windows::companion {

namespace detail {
class CompanionTokenLedger;
}

class CompanionSession;
class CompanionWindowOperations;
struct CompanionLaunchResult;

class CompanionWindowToken final {
public:
    CompanionWindowToken() = delete;

    [[nodiscard]] protocol::SessionMarker session_authority() const noexcept {
        return session_authority_;
    }

    [[nodiscard]] protocol::LogicalWindowId logical_id() const noexcept {
        return logical_id_;
    }

    [[nodiscard]] std::uint64_t generation() const noexcept {
        return generation_;
    }

    friend bool operator==(const CompanionWindowToken&,
                           const CompanionWindowToken&) = default;

private:
    CompanionWindowToken(std::uint64_t controller_authority,
                         protocol::SessionMarker session_authority,
                         protocol::LogicalWindowId logical_id,
                         std::uint64_t generation) noexcept
        : controller_authority_(controller_authority),
          session_authority_(session_authority),
          logical_id_(logical_id),
          generation_(generation) {}

    std::uint64_t controller_authority_{};
    protocol::SessionMarker session_authority_{};
    protocol::LogicalWindowId logical_id_{};
    std::uint64_t generation_{};

    friend class detail::CompanionTokenLedger;
    friend class CompanionSession;
    friend class CompanionWindowOperations;
};

enum class CompanionDiagnosticDomain {
    Win32,
    HResult,
    Protocol,
    Adapter,
};

struct CompanionDiagnostic {
    CompanionDiagnosticDomain domain{CompanionDiagnosticDomain::Adapter};
    std::uint64_t code{};
    std::string api;
    std::string detail;
};

struct ProcessSecurityFacts {
    std::uint32_t integrity_rid{};
    bool ui_access{};
    bool app_container{};

    friend bool operator==(const ProcessSecurityFacts&,
                           const ProcessSecurityFacts&) = default;
};

enum class CompanionLaunchStatus {
    Succeeded,
    InvalidTargetPath,
    AuthorityGenerationFailed,
    PipeCreationFailed,
    HandleRestrictionFailed,
    ProcessCreationFailed,
    SecurityQueryFailed,
    SecurityMismatch,
    HandshakeTimeout,
    HandshakeProtocolFailure,
    HandshakeRejected,
    TargetExited,
};

struct CompanionLaunchFacts {
    std::uint32_t controller_process_id{};
    std::uint32_t controller_thread_id{};
    std::uint32_t target_process_id{};
    std::uint32_t target_ui_thread_id{};
    protocol::SessionMarker session_authority{};
    ProcessSecurityFacts controller_security;
    ProcessSecurityFacts target_security;
};

enum class CompanionCommandStatus {
    Succeeded,
    InvalidRequest,
    StaleToken,
    SessionExited,
    SessionPoisoned,
    ProtocolFailure,
    Timeout,
    TargetRejected,
    NativeFailure,
};

struct CompanionCommandResult {
    CompanionCommandStatus status{CompanionCommandStatus::InvalidRequest};
    protocol::CommandStatus target_status{protocol::CommandStatus::InvalidFrame};
    std::uint64_t request_id{};
    std::optional<CompanionDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return status == CompanionCommandStatus::Succeeded;
    }
};

struct CompanionEvidenceResult {
    CompanionCommandStatus status{CompanionCommandStatus::InvalidRequest};
    std::uint64_t request_id{};
    std::optional<protocol::EvidencePayload> evidence;
    std::optional<CompanionDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return status == CompanionCommandStatus::Succeeded &&
               evidence.has_value();
    }
};

struct CompanionShutdownResult {
    CompanionCommandStatus status{CompanionCommandStatus::InvalidRequest};
    bool graceful_request_acknowledged{};
    bool process_signaled{};
    bool terminate_fallback_used{};
    std::optional<std::uint32_t> exit_code;
    std::optional<CompanionDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return status == CompanionCommandStatus::Succeeded && process_signaled;
    }
};

class CompanionSession final {
public:
    ~CompanionSession();

    CompanionSession(const CompanionSession&) = delete;
    CompanionSession& operator=(const CompanionSession&) = delete;
    CompanionSession(CompanionSession&&) = delete;
    CompanionSession& operator=(CompanionSession&&) = delete;

    [[nodiscard]] static CompanionLaunchResult launch(
        const std::filesystem::path& fully_qualified_target_path,
        std::chrono::milliseconds handshake_timeout =
            std::chrono::seconds{5});

    [[nodiscard]] const CompanionLaunchFacts& launch_facts() const noexcept;
    [[nodiscard]] bool is_alive() noexcept;
    [[nodiscard]] std::vector<CompanionWindowToken> tokens() const;
    [[nodiscard]] std::optional<CompanionWindowToken> token(
        protocol::LogicalWindowId logical_id) const;
    [[nodiscard]] bool contains(const CompanionWindowToken& token) noexcept;

    // These fixture commands never accept an HWND. SetStep only arms
    // target-side evidence and the deterministic uncooperative-window mode.
    [[nodiscard]] CompanionCommandResult set_step(
        std::uint64_t step_id,
        std::uint32_t uncooperative_window_mask = 0U,
        std::int32_t uncooperative_offset_x = 0);
    [[nodiscard]] CompanionCommandResult destroy_window(
        const CompanionWindowToken& token);
    [[nodiscard]] CompanionEvidenceResult query_evidence();
    [[nodiscard]] CompanionShutdownResult shutdown(
        std::chrono::milliseconds graceful_timeout =
            std::chrono::seconds{2},
        std::chrono::milliseconds terminate_timeout =
            std::chrono::seconds{2});

private:
    struct Impl;
    explicit CompanionSession(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;

    friend class CompanionWindowOperations;
};

struct CompanionLaunchResult {
    CompanionLaunchStatus status{CompanionLaunchStatus::InvalidTargetPath};
    std::unique_ptr<CompanionSession> session;
    std::optional<CompanionDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return status == CompanionLaunchStatus::Succeeded && session != nullptr;
    }
};

enum class CompanionOperationStage {
    Preflight,
    TargetArm,
    SingleApply,
    BeginBatch,
    DeferBatch,
    EndBatch,
    PostVerification,
};

enum class CompanionOperationStatus {
    Succeeded,
    InvalidRequest,
    DuplicateToken,
    StaleToken,
    SessionExited,
    SessionPoisoned,
    InvalidWindow,
    WrongProcess,
    WrongThread,
    WrongWindowClass,
    NotIndependentTopLevel,
    MarkerFailure,
    GeometryCaptureFailed,
    DpiContextMismatch,
    EmptyGeometry,
    ResizeRejected,
    ArithmeticOverflow,
    NativeCoordinateOutOfRange,
    TargetArmFailed,
    SingleApplyFailed,
    BeginBatchFailed,
    DeferBatchFailed,
    EndBatchFailed,
    PostVerificationFailed,
    WindowInvalidatedDuringApply,
};

struct CompanionWindowSnapshot {
    core::geometry::Rect positioning_rect;
    core::geometry::Rect visible_rect;
    std::uint32_t dpi{};
    std::wstring monitor_device_name;
    core::geometry::Rect monitor_rect;
    core::geometry::Rect monitor_work_area;
    std::uint32_t target_process_id{};
    std::uint32_t target_thread_id{};

    friend bool operator==(const CompanionWindowSnapshot&,
                           const CompanionWindowSnapshot&) = default;
};

struct CompanionCaptureResult {
    CompanionOperationStatus status{CompanionOperationStatus::InvalidRequest};
    CompanionOperationStage stage{CompanionOperationStage::Preflight};
    std::optional<CompanionWindowSnapshot> snapshot;
    std::optional<CompanionDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return status == CompanionOperationStatus::Succeeded &&
               snapshot.has_value();
    }
};

struct CompanionWindowTranslationRequest {
    CompanionWindowToken token;
    core::geometry::Rect target_visible_rect;
};

struct CompanionApplyOptions {
    std::uint32_t uncooperative_window_mask{};
    std::int32_t uncooperative_offset_x{};
};

struct CompanionWindowOperationReceipt {
    CompanionWindowToken token;
    protocol::SessionMarker session_authority{};
    std::uint32_t target_process_id{};
    std::uint32_t target_thread_id{};
    core::geometry::Rect requested_visible_rect;
    std::optional<core::geometry::Rect> requested_positioning_rect;
    std::optional<CompanionWindowSnapshot> before;
    std::optional<CompanionWindowSnapshot> actual;
    bool visible_target_verified{};
    bool positioning_target_verified{};
    bool size_preserved{};
    bool dpi_and_monitor_stable{};
};

struct CompanionOperationResult {
    std::uint64_t operation_batch_id{};
    CompanionOperationStatus status{CompanionOperationStatus::InvalidRequest};
    CompanionOperationStage stage{CompanionOperationStage::Preflight};
    std::size_t requested_count{};
    std::size_t deferred_count{};
    std::size_t verified_count{};
    bool native_apply_attempted{};
    bool native_outcome_known{true};
    bool recaptured_after_native_failure{};
    std::vector<CompanionWindowOperationReceipt> windows;
    std::optional<CompanionDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return status == CompanionOperationStatus::Succeeded;
    }
};

class CompanionWindowOperations final {
public:
    explicit CompanionWindowOperations(CompanionSession& session) noexcept;

    [[nodiscard]] CompanionCaptureResult capture(
        const CompanionWindowToken& token);
    [[nodiscard]] CompanionOperationResult apply_one(
        const CompanionWindowTranslationRequest& request,
        CompanionApplyOptions options = {});
    [[nodiscard]] CompanionOperationResult apply(
        std::span<const CompanionWindowTranslationRequest> requests,
        CompanionApplyOptions options = {});

private:
    CompanionSession* session_{};
};

} // namespace panebind::platform::windows::companion
