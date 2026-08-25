#pragma once

#include "core/geometry/geometry.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace panebind::platform::windows::operations {

namespace detail {
class TokenLedger;
}

inline constexpr wchar_t kOwnedWindowClassName[] =
    L"PaneBind.R1B.OwnedWindow";

class OwnedWindowToken final {
public:
    OwnedWindowToken() = delete;

    [[nodiscard]] std::uint64_t logical_id() const noexcept {
        return logical_id_;
    }

    [[nodiscard]] std::uint64_t generation() const noexcept {
        return generation_;
    }

    friend bool operator==(const OwnedWindowToken&,
                           const OwnedWindowToken&) = default;

private:
    OwnedWindowToken(std::uint64_t authority_id,
                     std::uint64_t logical_id,
                     std::uint64_t generation) noexcept
        : authority_id_(authority_id),
          logical_id_(logical_id),
          generation_(generation) {}

    std::uint64_t authority_id_{};
    std::uint64_t logical_id_{};
    std::uint64_t generation_{};

    friend class detail::TokenLedger;
    friend class OwnedWindowRegistry;
    friend class OwnedWindowOperations;
};

enum class OperationStage {
    None,
    Registration,
    Preflight,
    SingleApply,
    BeginBatch,
    DeferBatch,
    EndBatch,
    PostVerification,
};

enum class OperationStatus {
    Succeeded,
    InvalidRequest,
    DuplicateToken,
    StaleToken,
    InvalidWindow,
    WrongProcess,
    WrongThread,
    WrongWindowClass,
    NotIndependentTopLevel,
    MarkerConflict,
    MarkerFailure,
    GenerationExhausted,
    GeometryCaptureFailed,
    DpiContextMismatch,
    EmptyGeometry,
    ResizeRejected,
    ArithmeticOverflow,
    NativeCoordinateOutOfRange,
    SingleApplyFailed,
    BeginBatchFailed,
    DeferBatchFailed,
    EndBatchFailed,
    PostVerificationFailed,
    WindowInvalidatedDuringApply,
};

enum class DiagnosticDomain {
    Win32,
    HResult,
    Adapter,
};

struct OperationDiagnostic {
    DiagnosticDomain domain{DiagnosticDomain::Adapter};
    std::uint64_t code{};
    std::string api;
    std::string detail;
};

struct RegistrationResult {
    OperationStatus status{OperationStatus::InvalidRequest};
    std::optional<OwnedWindowToken> token;
    std::optional<OperationDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return status == OperationStatus::Succeeded && token.has_value();
    }
};

struct OwnedWindowSnapshot {
    core::geometry::Rect positioning_rect;
    core::geometry::Rect visible_rect;
    std::uint32_t dpi{};
    std::wstring monitor_device_name;
    core::geometry::Rect monitor_rect;
    core::geometry::Rect monitor_work_area;

    friend bool operator==(const OwnedWindowSnapshot&,
                           const OwnedWindowSnapshot&) = default;
};

struct CaptureResult {
    OperationStatus status{OperationStatus::InvalidRequest};
    OperationStage stage{OperationStage::Preflight};
    std::optional<OwnedWindowSnapshot> snapshot;
    std::optional<OperationDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return status == OperationStatus::Succeeded && snapshot.has_value();
    }
};

struct WindowTranslationRequest {
    OwnedWindowToken token;
    core::geometry::Rect target_visible_rect;
};

struct WindowOperationReceipt {
    OwnedWindowToken token;
    core::geometry::Rect requested_visible_rect;
    std::optional<core::geometry::Rect> requested_positioning_rect;
    std::optional<OwnedWindowSnapshot> before;
    std::optional<OwnedWindowSnapshot> actual;
    bool visible_target_verified{};
    bool positioning_target_verified{};
    bool size_preserved{};
    bool dpi_and_monitor_stable{};
};

struct OperationResult {
    std::uint64_t operation_batch_id{};
    OperationStatus status{OperationStatus::InvalidRequest};
    OperationStage stage{OperationStage::Preflight};
    std::size_t requested_count{};
    std::size_t deferred_count{};
    std::size_t verified_count{};
    bool native_apply_attempted{};
    bool native_outcome_known{true};
    bool recaptured_after_native_failure{};
    std::vector<WindowOperationReceipt> windows;
    std::optional<OperationDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return status == OperationStatus::Succeeded;
    }
};

// register_window is the sole raw-HWND capability-issuance boundary. The
// fixed private class, independent-top-level shape, PID/TID, private property,
// and token authority form process-local architecture provenance; they are not
// a sandbox against malicious code already executing inside this process. The
// resulting token has no native conversion. Call invalidate_window(hwnd) at
// WM_NCDESTROY entry so every previously issued token is permanently stale.
class OwnedWindowRegistry final {
public:
    OwnedWindowRegistry();
    ~OwnedWindowRegistry();

    OwnedWindowRegistry(const OwnedWindowRegistry&) = delete;
    OwnedWindowRegistry& operator=(const OwnedWindowRegistry&) = delete;
    OwnedWindowRegistry(OwnedWindowRegistry&&) = delete;
    OwnedWindowRegistry& operator=(OwnedWindowRegistry&&) = delete;

    [[nodiscard]] RegistrationResult register_window(HWND window);
    [[nodiscard]] bool invalidate_window(HWND window) noexcept;
    [[nodiscard]] bool contains(const OwnedWindowToken& token) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    friend class OwnedWindowOperations;
};

// capture/apply deliberately accept only issued capabilities, never HWND.
class OwnedWindowOperations final {
public:
    explicit OwnedWindowOperations(OwnedWindowRegistry& registry) noexcept;

    [[nodiscard]] CaptureResult capture(const OwnedWindowToken& token);
    [[nodiscard]] OperationResult apply_one(
        const WindowTranslationRequest& request);
    [[nodiscard]] OperationResult apply(
        std::span<const WindowTranslationRequest> requests);

private:
    OwnedWindowRegistry* registry_{};
};

} // namespace panebind::platform::windows::operations
