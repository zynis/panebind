#include "platform/windows/operations/owned_window_operations.h"

#include "core/geometry/checked_arithmetic.h"
#include "platform/windows/operations/owned_window_operations_internal.h"

#include <dwmapi.h>

#include <algorithm>
#include <array>
#include <limits>
#include <mutex>
#include <string_view>
#include <utility>

namespace panebind::platform::windows::operations {
namespace {

constexpr wchar_t kOwnedWindowMarkerProperty[] =
    L"PaneBind.R1B.OwnedWindow.Generation";
constexpr UINT kTranslationFlags =
    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;

detail::MonotonicIdSource token_authority_ids;
detail::MonotonicIdSource operation_batch_ids;

enum class AdapterDiagnosticCode : std::uint64_t {
    InvalidWindow = 1U,
    WrongProcess = 2U,
    WrongThread = 3U,
    WrongClass = 4U,
    MarkerMismatch = 5U,
    InvalidGeometry = 6U,
    InvalidDpi = 7U,
    InvalidMonitor = 8U,
    EmptyBatch = 9U,
    DuplicateToken = 10U,
    StaleToken = 11U,
    WindowChanged = 12U,
    BatchIdExhausted = 13U,
    NotIndependentTopLevel = 14U,
};

[[nodiscard]] OperationDiagnostic adapter_diagnostic(
    const AdapterDiagnosticCode code,
    std::string api,
    std::string detail) {
    return {DiagnosticDomain::Adapter,
            static_cast<std::uint64_t>(code),
            std::move(api),
            std::move(detail)};
}

[[nodiscard]] OperationDiagnostic win32_diagnostic(std::string api,
                                                    const DWORD error,
                                                    std::string detail) {
    return {DiagnosticDomain::Win32,
            static_cast<std::uint64_t>(error),
            std::move(api),
            std::move(detail)};
}

[[nodiscard]] OperationDiagnostic hresult_diagnostic(std::string api,
                                                     const HRESULT error,
                                                     std::string detail) {
    return {DiagnosticDomain::HResult,
            static_cast<std::uint32_t>(error),
            std::move(api),
            std::move(detail)};
}

[[nodiscard]] std::uintptr_t native_key(const HWND window) noexcept {
    return reinterpret_cast<std::uintptr_t>(window);
}

[[nodiscard]] HWND native_window(const std::uintptr_t key) noexcept {
    return reinterpret_cast<HWND>(key);
}

[[nodiscard]] HANDLE generation_marker(const std::uint64_t generation) noexcept {
    if (generation == 0U ||
        generation > std::numeric_limits<std::uintptr_t>::max()) {
        return nullptr;
    }
    return reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(generation));
}

[[nodiscard]] bool has_expected_class(HWND window,
                                      std::optional<OperationDiagnostic>& diagnostic) {
    std::array<wchar_t, 256U> class_name{};
    const int length = GetClassNameW(window,
                                     class_name.data(),
                                     static_cast<int>(class_name.size()));
    if (length == 0) {
        const DWORD error = GetLastError();
        diagnostic = win32_diagnostic("GetClassNameW",
                                      error,
                                      "owned window class query failed");
        return false;
    }

    return std::wstring_view{class_name.data(), static_cast<std::size_t>(length)} ==
           kOwnedWindowClassName;
}

[[nodiscard]] bool has_independent_top_level_provenance(
    HWND window,
    std::optional<OperationDiagnostic>& diagnostic) {
    const HWND root = GetAncestor(window, GA_ROOT);

    SetLastError(ERROR_SUCCESS);
    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    const DWORD style_error = GetLastError();
    if (style == 0 && style_error != ERROR_SUCCESS) {
        diagnostic = win32_diagnostic("GetWindowLongPtrW",
                                      style_error,
                                      "owned window style query failed");
        return false;
    }

    const bool has_child_style =
        (static_cast<ULONG_PTR>(style) & static_cast<ULONG_PTR>(WS_CHILD)) != 0U;
    const bool has_owner = GetWindow(window, GW_OWNER) != nullptr;
    if (!detail::is_independent_top_level_provenance(root == window,
                                                     has_child_style,
                                                     has_owner)) {
        diagnostic = adapter_diagnostic(
            AdapterDiagnosticCode::NotIndependentTopLevel,
            "GetAncestor/GetWindowLongPtrW/GetWindow",
            "capability requires an unowned independent top-level window");
        return false;
    }
    return true;
}

[[nodiscard]] std::optional<core::geometry::Rect>
checked_native_rect(const RECT& rect) noexcept {
    if (rect.right < rect.left || rect.bottom < rect.top) {
        return std::nullopt;
    }
    return core::geometry::Rect{rect.left, rect.top, rect.right, rect.bottom};
}

struct ResolvedWindow {
    HWND window{};
    detail::LedgerEntry ledger_entry;
};

struct ResolveResult {
    OperationStatus status{OperationStatus::StaleToken};
    std::optional<ResolvedWindow> value;
    std::optional<OperationDiagnostic> diagnostic;
};

struct PreparedOperation {
    OwnedWindowToken token;
    HWND window{};
    core::geometry::Rect target_positioning_rect;
};

[[nodiscard]] OperationStatus bridge_operation_status(
    const detail::TranslationBridgeStatus status) noexcept {
    switch (status) {
    case detail::TranslationBridgeStatus::Succeeded:
        return OperationStatus::Succeeded;
    case detail::TranslationBridgeStatus::EmptyGeometry:
        return OperationStatus::EmptyGeometry;
    case detail::TranslationBridgeStatus::ResizeRejected:
        return OperationStatus::ResizeRejected;
    case detail::TranslationBridgeStatus::ArithmeticOverflow:
        return OperationStatus::ArithmeticOverflow;
    case detail::TranslationBridgeStatus::NativeCoordinateOutOfRange:
        return OperationStatus::NativeCoordinateOutOfRange;
    }
    return OperationStatus::ArithmeticOverflow;
}

[[nodiscard]] bool same_rect_size(const core::geometry::Rect& first,
                                  const core::geometry::Rect& second) noexcept {
    const auto first_width = core::geometry::checked_difference(first.right(),
                                                                first.left());
    const auto first_height = core::geometry::checked_difference(first.bottom(),
                                                                 first.top());
    const auto second_width = core::geometry::checked_difference(second.right(),
                                                                 second.left());
    const auto second_height = core::geometry::checked_difference(second.bottom(),
                                                                  second.top());
    return first_width.has_value() && first_height.has_value() &&
           second_width.has_value() && second_height.has_value() &&
           *first_width == *second_width && *first_height == *second_height;
}

void verify_receipt(WindowOperationReceipt& receipt,
                    const OwnedWindowSnapshot& actual) noexcept {
    receipt.actual = actual;
    receipt.visible_target_verified =
        actual.visible_rect == receipt.requested_visible_rect;
    receipt.positioning_target_verified =
        receipt.requested_positioning_rect.has_value() &&
        actual.positioning_rect == *receipt.requested_positioning_rect;
    receipt.size_preserved =
        receipt.before.has_value() &&
        same_rect_size(receipt.before->visible_rect, actual.visible_rect) &&
        same_rect_size(receipt.before->positioning_rect, actual.positioning_rect);
    receipt.dpi_and_monitor_stable =
        receipt.before.has_value() && receipt.before->dpi == actual.dpi &&
        receipt.before->monitor_device_name == actual.monitor_device_name;
}

} // namespace

namespace detail {

MonotonicIdSource::MonotonicIdSource(const std::uint64_t first) noexcept
    : next_(first) {}

std::uint64_t MonotonicIdSource::issue() noexcept {
    auto current = next_.load(std::memory_order_relaxed);
    for (;;) {
        if (current == 0U ||
            current == std::numeric_limits<std::uint64_t>::max()) {
            return 0U;
        }
        if (next_.compare_exchange_weak(current,
                                        current + 1U,
                                        std::memory_order_relaxed,
                                        std::memory_order_relaxed)) {
            return current;
        }
    }
}

TokenLedger::TokenLedger() noexcept
    : authority_id_(token_authority_ids.issue()) {}

TokenLedger::TokenLedger(const std::uint64_t next_logical_id,
                         const std::uint64_t next_generation) noexcept
    : authority_id_(token_authority_ids.issue()),
      next_logical_id_(next_logical_id),
      next_generation_(next_generation) {}

LedgerIssueResult TokenLedger::issue(const std::uintptr_t native_key_value,
                                     const std::uint32_t process_id,
                                     const std::uint32_t creator_thread_id) {
    if (native_key_value == 0U) {
        return {LedgerIssueStatus::InvalidNativeKey, std::nullopt};
    }
    if (logical_id_by_native_key_.contains(native_key_value)) {
        return {LedgerIssueStatus::DuplicateNativeKey, std::nullopt};
    }
    if (authority_id_ == 0U) {
        return {LedgerIssueStatus::AuthorityExhausted, std::nullopt};
    }
    if (next_logical_id_ == 0U ||
        next_logical_id_ == std::numeric_limits<std::uint64_t>::max() ||
        next_generation_ == 0U ||
        next_generation_ == std::numeric_limits<std::uint64_t>::max()) {
        return {LedgerIssueStatus::GenerationExhausted, std::nullopt};
    }

    const LedgerEntry entry{next_logical_id_,
                            next_generation_,
                            native_key_value,
                            process_id,
                            creator_thread_id};
    active_by_logical_id_.emplace(entry.logical_id, entry);
    logical_id_by_native_key_.emplace(entry.native_key, entry.logical_id);
    ++next_logical_id_;
    ++next_generation_;
    return {LedgerIssueStatus::Succeeded,
            OwnedWindowToken{authority_id_,
                             entry.logical_id,
                             entry.generation}};
}

bool TokenLedger::retire_native(const std::uintptr_t native_key_value) noexcept {
    const auto native_found = logical_id_by_native_key_.find(native_key_value);
    if (native_found == logical_id_by_native_key_.end()) {
        return false;
    }

    active_by_logical_id_.erase(native_found->second);
    logical_id_by_native_key_.erase(native_found);
    return true;
}

std::optional<LedgerEntry>
TokenLedger::resolve(const OwnedWindowToken& token) const noexcept {
    if (token.authority_id_ != authority_id_) {
        return std::nullopt;
    }
    const auto found = active_by_logical_id_.find(token.logical_id_);
    if (found == active_by_logical_id_.end() ||
        found->second.generation != token.generation_) {
        return std::nullopt;
    }
    return found->second;
}

std::optional<LedgerEntry>
TokenLedger::find_native(const std::uintptr_t native_key_value) const noexcept {
    const auto native_found = logical_id_by_native_key_.find(native_key_value);
    if (native_found == logical_id_by_native_key_.end()) {
        return std::nullopt;
    }
    const auto entry_found = active_by_logical_id_.find(native_found->second);
    if (entry_found == active_by_logical_id_.end()) {
        return std::nullopt;
    }
    return entry_found->second;
}

std::vector<LedgerEntry> TokenLedger::active_entries() const {
    std::vector<LedgerEntry> result;
    result.reserve(active_by_logical_id_.size());
    for (const auto& [logical_id, entry] : active_by_logical_id_) {
        static_cast<void>(logical_id);
        result.push_back(entry);
    }
    return result;
}

BatchTokenValidation validate_batch_tokens(
    const std::span<const OwnedWindowToken> tokens,
    const TokenLedger& ledger) noexcept {
    if (tokens.empty()) {
        return BatchTokenValidation::Empty;
    }

    for (std::size_t first = 0U; first < tokens.size(); ++first) {
        for (std::size_t second = first + 1U; second < tokens.size(); ++second) {
            if (tokens[first] == tokens[second]) {
                return BatchTokenValidation::Duplicate;
            }
        }
    }

    for (const auto& token : tokens) {
        if (!ledger.resolve(token).has_value()) {
            return BatchTokenValidation::Unknown;
        }
    }
    return BatchTokenValidation::Succeeded;
}

bool is_independent_top_level_provenance(const bool root_is_self,
                                         const bool has_child_style,
                                         const bool has_owner) noexcept {
    return root_is_self && !has_child_style && !has_owner;
}

} // namespace detail

struct OwnedWindowRegistry::Impl final {
    mutable std::mutex mutex;
    detail::TokenLedger ledger;

    [[nodiscard]] std::uint64_t issue_operation_batch_id() noexcept {
        return operation_batch_ids.issue();
    }

    [[nodiscard]] detail::BatchTokenValidation validate_tokens(
        const std::span<const OwnedWindowToken> tokens) const noexcept {
        std::lock_guard lock{mutex};
        return detail::validate_batch_tokens(tokens, ledger);
    }

    void retire_if_current(const detail::LedgerEntry& entry) noexcept {
        std::lock_guard lock{mutex};
        const auto current = ledger.find_native(entry.native_key);
        if (current.has_value() && current->logical_id == entry.logical_id &&
            current->generation == entry.generation) {
            static_cast<void>(ledger.retire_native(entry.native_key));
        }
    }

    [[nodiscard]] ResolveResult resolve(const OwnedWindowToken& token) {
        std::optional<detail::LedgerEntry> entry;
        {
            std::lock_guard lock{mutex};
            entry = ledger.resolve(token);
        }
        if (!entry.has_value()) {
            return {OperationStatus::StaleToken,
                    std::nullopt,
                    adapter_diagnostic(AdapterDiagnosticCode::StaleToken,
                                       "OwnedWindowRegistry",
                                       "token is not active in the owned-window ledger")};
        }

        if (GetCurrentThreadId() != entry->creator_thread_id) {
            return {OperationStatus::WrongThread,
                    std::nullopt,
                    adapter_diagnostic(AdapterDiagnosticCode::WrongThread,
                                       "GetCurrentThreadId",
                                       "owned-window operation must run on the creator thread")};
        }

        HWND window = native_window(entry->native_key);
        if (!IsWindow(window)) {
            retire_if_current(*entry);
            return {OperationStatus::InvalidWindow,
                    std::nullopt,
                    adapter_diagnostic(AdapterDiagnosticCode::InvalidWindow,
                                       "IsWindow",
                                       "registered HWND no longer identifies a window")};
        }

        DWORD process_id = 0U;
        const DWORD thread_id = GetWindowThreadProcessId(window, &process_id);
        if (thread_id == 0U) {
            const DWORD error = GetLastError();
            retire_if_current(*entry);
            return {OperationStatus::InvalidWindow,
                    std::nullopt,
                    win32_diagnostic("GetWindowThreadProcessId",
                                     error,
                                     "registered HWND identity query failed")};
        }
        if (process_id != entry->process_id ||
            process_id != GetCurrentProcessId()) {
            retire_if_current(*entry);
            return {OperationStatus::WrongProcess,
                    std::nullopt,
                    adapter_diagnostic(AdapterDiagnosticCode::WrongProcess,
                                       "GetWindowThreadProcessId",
                                       "registered HWND no longer belongs to this process")};
        }
        if (thread_id != entry->creator_thread_id) {
            retire_if_current(*entry);
            return {OperationStatus::WrongThread,
                    std::nullopt,
                    adapter_diagnostic(AdapterDiagnosticCode::WrongThread,
                                       "GetWindowThreadProcessId",
                                       "registered HWND creator thread changed")};
        }

        std::optional<OperationDiagnostic> class_diagnostic;
        if (!has_expected_class(window, class_diagnostic)) {
            retire_if_current(*entry);
            if (!class_diagnostic.has_value()) {
                class_diagnostic = adapter_diagnostic(
                    AdapterDiagnosticCode::WrongClass,
                    "GetClassNameW",
                    "registered HWND does not use the private harness class");
            }
            return {OperationStatus::WrongWindowClass,
                    std::nullopt,
                    std::move(class_diagnostic)};
        }

        std::optional<OperationDiagnostic> topology_diagnostic;
        if (!has_independent_top_level_provenance(window, topology_diagnostic)) {
            retire_if_current(*entry);
            return {OperationStatus::NotIndependentTopLevel,
                    std::nullopt,
                    std::move(topology_diagnostic)};
        }

        if (GetPropW(window, kOwnedWindowMarkerProperty) !=
            generation_marker(entry->generation)) {
            retire_if_current(*entry);
            return {OperationStatus::MarkerFailure,
                    std::nullopt,
                    adapter_diagnostic(AdapterDiagnosticCode::MarkerMismatch,
                                       "GetPropW",
                                       "owned-window generation marker is missing or changed")};
        }

        return {OperationStatus::Succeeded,
                ResolvedWindow{window, *entry},
                std::nullopt};
    }
};

namespace {

[[nodiscard]] CaptureResult capture_native_owned_window(HWND window) {
    CaptureResult result;
    result.stage = OperationStage::Preflight;

    RECT positioning{};
    if (!GetWindowRect(window, &positioning)) {
        const DWORD error = GetLastError();
        result.status = OperationStatus::GeometryCaptureFailed;
        result.diagnostic = win32_diagnostic("GetWindowRect",
                                             error,
                                             "positioning rectangle query failed");
        return result;
    }

    RECT visible{};
    const HRESULT visible_result = DwmGetWindowAttribute(
        window,
        DWMWA_EXTENDED_FRAME_BOUNDS,
        &visible,
        sizeof(visible));
    if (FAILED(visible_result)) {
        result.status = OperationStatus::GeometryCaptureFailed;
        result.diagnostic = hresult_diagnostic(
            "DwmGetWindowAttribute",
            visible_result,
            "extended visible frame query failed");
        return result;
    }

    const auto positioning_rect = checked_native_rect(positioning);
    const auto visible_rect = checked_native_rect(visible);
    if (!positioning_rect.has_value() || !visible_rect.has_value()) {
        result.status = OperationStatus::GeometryCaptureFailed;
        result.diagnostic = adapter_diagnostic(
            AdapterDiagnosticCode::InvalidGeometry,
            "OwnedWindowOperations",
            "native API returned an inverted rectangle");
        return result;
    }

    const DPI_AWARENESS_CONTEXT dpi_context =
        GetWindowDpiAwarenessContext(window);
    if (dpi_context == nullptr ||
        !AreDpiAwarenessContextsEqual(
            dpi_context, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        result.status = OperationStatus::DpiContextMismatch;
        result.diagnostic = adapter_diagnostic(
            AdapterDiagnosticCode::InvalidDpi,
            "GetWindowDpiAwarenessContext",
            "owned harness window is not PerMonitorV2");
        return result;
    }

    const UINT dpi = GetDpiForWindow(window);
    if (dpi == 0U) {
        result.status = OperationStatus::GeometryCaptureFailed;
        result.diagnostic = adapter_diagnostic(AdapterDiagnosticCode::InvalidDpi,
                                               "GetDpiForWindow",
                                               "window DPI query returned zero");
        return result;
    }

    const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    if (monitor == nullptr) {
        result.status = OperationStatus::GeometryCaptureFailed;
        result.diagnostic = adapter_diagnostic(AdapterDiagnosticCode::InvalidMonitor,
                                               "MonitorFromWindow",
                                               "window monitor query returned null");
        return result;
    }

    MONITORINFOEXW monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!GetMonitorInfoW(monitor, &monitor_info)) {
        const DWORD error = GetLastError();
        result.status = OperationStatus::GeometryCaptureFailed;
        result.diagnostic = win32_diagnostic("GetMonitorInfoW",
                                             error,
                                             "monitor geometry query failed");
        return result;
    }

    const auto monitor_rect = checked_native_rect(monitor_info.rcMonitor);
    const auto work_area = checked_native_rect(monitor_info.rcWork);
    if (!monitor_rect.has_value() || !work_area.has_value()) {
        result.status = OperationStatus::GeometryCaptureFailed;
        result.diagnostic = adapter_diagnostic(
            AdapterDiagnosticCode::InvalidMonitor,
            "GetMonitorInfoW",
            "monitor API returned an inverted rectangle");
        return result;
    }

    result.status = OperationStatus::Succeeded;
    result.snapshot = OwnedWindowSnapshot{*positioning_rect,
                                          *visible_rect,
                                          dpi,
                                          monitor_info.szDevice,
                                          *monitor_rect,
                                          *work_area};
    return result;
}

void recapture_operation_windows(OwnedWindowOperations& operations,
                                 OperationResult& result,
                                 const bool after_native_failure) {
    result.verified_count = 0U;
    result.recaptured_after_native_failure = after_native_failure;
    for (auto& receipt : result.windows) {
        const auto capture = operations.capture(receipt.token);
        if (!capture.succeeded()) {
            continue;
        }
        verify_receipt(receipt, *capture.snapshot);
        if (receipt.visible_target_verified &&
            receipt.positioning_target_verified && receipt.size_preserved &&
            receipt.dpi_and_monitor_stable) {
            ++result.verified_count;
        }
    }
}

} // namespace

OwnedWindowRegistry::OwnedWindowRegistry() : impl_(std::make_unique<Impl>()) {}

OwnedWindowRegistry::~OwnedWindowRegistry() {
    std::vector<detail::LedgerEntry> entries;
    {
        std::lock_guard lock{impl_->mutex};
        entries = impl_->ledger.active_entries();
    }

    // Defensive marker cleanup only. The registry never destroys a window.
    // Normal harness teardown invalidates each entry from WM_NCDESTROY first.
    for (const auto& entry : entries) {
        HWND window = native_window(entry.native_key);
        if (!IsWindow(window)) {
            continue;
        }
        DWORD process_id = 0U;
        const DWORD thread_id = GetWindowThreadProcessId(window, &process_id);
        std::optional<OperationDiagnostic> ignored_class_diagnostic;
        if (thread_id == entry.creator_thread_id &&
            process_id == entry.process_id &&
            process_id == GetCurrentProcessId() &&
            has_expected_class(window, ignored_class_diagnostic) &&
            GetPropW(window, kOwnedWindowMarkerProperty) ==
                generation_marker(entry.generation)) {
            static_cast<void>(RemovePropW(window, kOwnedWindowMarkerProperty));
        }
    }
}

RegistrationResult OwnedWindowRegistry::register_window(HWND window) {
    if (window == nullptr || !IsWindow(window)) {
        return {OperationStatus::InvalidWindow,
                std::nullopt,
                adapter_diagnostic(AdapterDiagnosticCode::InvalidWindow,
                                   "IsWindow",
                                   "capability issuance requires an existing HWND")};
    }

    DWORD process_id = 0U;
    const DWORD creator_thread_id = GetWindowThreadProcessId(window, &process_id);
    if (creator_thread_id == 0U) {
        const DWORD error = GetLastError();
        return {OperationStatus::InvalidWindow,
                std::nullopt,
                win32_diagnostic("GetWindowThreadProcessId",
                                 error,
                                 "capability issuance identity query failed")};
    }
    if (process_id != GetCurrentProcessId()) {
        return {OperationStatus::WrongProcess,
                std::nullopt,
                adapter_diagnostic(AdapterDiagnosticCode::WrongProcess,
                                   "GetWindowThreadProcessId",
                                   "capability issuance rejects non-PaneBind processes")};
    }
    if (creator_thread_id != GetCurrentThreadId()) {
        return {OperationStatus::WrongThread,
                std::nullopt,
                adapter_diagnostic(AdapterDiagnosticCode::WrongThread,
                                   "GetWindowThreadProcessId",
                                   "register_window must run on the HWND creator thread")};
    }

    std::optional<OperationDiagnostic> class_diagnostic;
    if (!has_expected_class(window, class_diagnostic)) {
        if (!class_diagnostic.has_value()) {
            class_diagnostic = adapter_diagnostic(
                AdapterDiagnosticCode::WrongClass,
                "GetClassNameW",
                "capability issuance requires PaneBind.R1B.OwnedWindow");
        }
        return {OperationStatus::WrongWindowClass,
                std::nullopt,
                std::move(class_diagnostic)};
    }

    std::optional<OperationDiagnostic> topology_diagnostic;
    if (!has_independent_top_level_provenance(window, topology_diagnostic)) {
        return {OperationStatus::NotIndependentTopLevel,
                std::nullopt,
                std::move(topology_diagnostic)};
    }

    if (GetPropW(window, kOwnedWindowMarkerProperty) != nullptr) {
        return {OperationStatus::MarkerConflict,
                std::nullopt,
                adapter_diagnostic(AdapterDiagnosticCode::MarkerMismatch,
                                   "GetPropW",
                                   "private generation marker already exists")};
    }

    detail::LedgerIssueResult issue;
    {
        std::lock_guard lock{impl_->mutex};
        issue = impl_->ledger.issue(native_key(window),
                                    process_id,
                                    creator_thread_id);
    }
    switch (issue.status) {
    case detail::LedgerIssueStatus::InvalidNativeKey:
        return {OperationStatus::InvalidWindow, std::nullopt, std::nullopt};
    case detail::LedgerIssueStatus::DuplicateNativeKey:
        return {OperationStatus::MarkerConflict,
                std::nullopt,
                adapter_diagnostic(AdapterDiagnosticCode::MarkerMismatch,
                                   "OwnedWindowRegistry",
                                   "HWND is already registered")};
    case detail::LedgerIssueStatus::AuthorityExhausted:
        return {OperationStatus::GenerationExhausted,
                std::nullopt,
                adapter_diagnostic(AdapterDiagnosticCode::BatchIdExhausted,
                                   "OwnedWindowRegistry",
                                   "process token-authority space is exhausted")};
    case detail::LedgerIssueStatus::GenerationExhausted:
        return {OperationStatus::GenerationExhausted,
                std::nullopt,
                adapter_diagnostic(AdapterDiagnosticCode::BatchIdExhausted,
                                   "OwnedWindowRegistry",
                                   "logical token generation space is exhausted")};
    case detail::LedgerIssueStatus::Succeeded:
        break;
    }

    const HANDLE marker = generation_marker(issue.token->generation());
    if (marker == nullptr) {
        std::lock_guard lock{impl_->mutex};
        static_cast<void>(impl_->ledger.retire_native(native_key(window)));
        return {OperationStatus::GenerationExhausted,
                std::nullopt,
                adapter_diagnostic(AdapterDiagnosticCode::BatchIdExhausted,
                                   "OwnedWindowRegistry",
                                   "generation is not representable as a property marker")};
    }
    if (!SetPropW(window, kOwnedWindowMarkerProperty, marker)) {
        const DWORD error = GetLastError();
        std::lock_guard lock{impl_->mutex};
        static_cast<void>(impl_->ledger.retire_native(native_key(window)));
        return {OperationStatus::MarkerFailure,
                std::nullopt,
                win32_diagnostic("SetPropW",
                                 error,
                                 "private generation marker installation failed")};
    }
    if (GetPropW(window, kOwnedWindowMarkerProperty) != marker) {
        static_cast<void>(RemovePropW(window, kOwnedWindowMarkerProperty));
        std::lock_guard lock{impl_->mutex};
        static_cast<void>(impl_->ledger.retire_native(native_key(window)));
        return {OperationStatus::MarkerFailure,
                std::nullopt,
                adapter_diagnostic(AdapterDiagnosticCode::MarkerMismatch,
                                   "GetPropW",
                                   "private generation marker verification failed")};
    }

    return {OperationStatus::Succeeded, issue.token, std::nullopt};
}

bool OwnedWindowRegistry::invalidate_window(HWND window) noexcept {
    if (window == nullptr) {
        return false;
    }

    bool retired = false;
    {
        std::lock_guard lock{impl_->mutex};
        retired = impl_->ledger.retire_native(native_key(window));
    }
    if (retired) {
        static_cast<void>(RemovePropW(window, kOwnedWindowMarkerProperty));
    }
    return retired;
}

bool OwnedWindowRegistry::contains(const OwnedWindowToken& token) const noexcept {
    std::lock_guard lock{impl_->mutex};
    return impl_->ledger.resolve(token).has_value();
}

OwnedWindowOperations::OwnedWindowOperations(OwnedWindowRegistry& registry) noexcept
    : registry_(&registry) {}

CaptureResult OwnedWindowOperations::capture(const OwnedWindowToken& token) {
    auto resolved = registry_->impl_->resolve(token);
    if (!resolved.value.has_value()) {
        return {resolved.status,
                OperationStage::Preflight,
                std::nullopt,
                std::move(resolved.diagnostic)};
    }

    CaptureResult result = capture_native_owned_window(resolved.value->window);
    if (!result.succeeded()) {
        return result;
    }

    auto confirmed = registry_->impl_->resolve(token);
    if (!confirmed.value.has_value() ||
        confirmed.value->window != resolved.value->window) {
        return {OperationStatus::WindowInvalidatedDuringApply,
                OperationStage::Preflight,
                std::nullopt,
                confirmed.diagnostic.has_value()
                    ? std::move(confirmed.diagnostic)
                    : std::optional<OperationDiagnostic>{adapter_diagnostic(
                          AdapterDiagnosticCode::WindowChanged,
                          "OwnedWindowRegistry",
                          "owned HWND changed during capture")}};
    }
    return result;
}

OperationResult OwnedWindowOperations::apply_one(
    const WindowTranslationRequest& request) {
    OperationResult result;
    result.requested_count = 1U;

    result.operation_batch_id = registry_->impl_->issue_operation_batch_id();
    if (result.operation_batch_id == 0U) {
        result.status = OperationStatus::GenerationExhausted;
        result.diagnostic = adapter_diagnostic(
            AdapterDiagnosticCode::BatchIdExhausted,
            "OwnedWindowOperations",
            "operation batch identifier space is exhausted");
        return result;
    }
    result.windows.push_back(
        WindowOperationReceipt{request.token, request.target_visible_rect});

    const std::array tokens{request.token};
    const auto token_validation = registry_->impl_->validate_tokens(tokens);
    if (token_validation != detail::BatchTokenValidation::Succeeded) {
        result.status = OperationStatus::StaleToken;
        result.diagnostic = adapter_diagnostic(AdapterDiagnosticCode::StaleToken,
                                               "OwnedWindowOperations",
                                               "single translation token is stale");
        return result;
    }

    auto resolved = registry_->impl_->resolve(request.token);
    if (!resolved.value.has_value()) {
        result.status = resolved.status;
        result.diagnostic = std::move(resolved.diagnostic);
        return result;
    }

    auto captured = capture_native_owned_window(resolved.value->window);
    if (!captured.succeeded()) {
        result.status = captured.status;
        result.diagnostic = std::move(captured.diagnostic);
        return result;
    }

    const auto bridge = detail::bridge_visible_translation(
        captured.snapshot->positioning_rect,
        captured.snapshot->visible_rect,
        request.target_visible_rect);
    if (bridge.status != detail::TranslationBridgeStatus::Succeeded) {
        result.status = bridge_operation_status(bridge.status);
        result.diagnostic = adapter_diagnostic(
            AdapterDiagnosticCode::InvalidGeometry,
            "bridge_visible_translation",
            "visible target is not a representable pure translation");
        return result;
    }

    auto confirmed = registry_->impl_->resolve(request.token);
    if (!confirmed.value.has_value() ||
        confirmed.value->window != resolved.value->window) {
        result.status = OperationStatus::WindowInvalidatedDuringApply;
        result.diagnostic = confirmed.diagnostic.has_value()
                                ? std::move(confirmed.diagnostic)
                                : std::optional<OperationDiagnostic>{
                                      adapter_diagnostic(
                                          AdapterDiagnosticCode::WindowChanged,
                                          "OwnedWindowRegistry",
                                          "owned HWND changed during single preflight")};
        return result;
    }

    result.windows.front().before = *captured.snapshot;
    result.windows.front().requested_positioning_rect =
        *bridge.target_positioning_rect;

    const auto& target = *bridge.target_positioning_rect;
    result.stage = OperationStage::SingleApply;
    result.native_apply_attempted = true;
    if (!SetWindowPos(resolved.value->window,
                      nullptr,
                      static_cast<int>(target.left()),
                      static_cast<int>(target.top()),
                      0,
                      0,
                      kTranslationFlags)) {
        const DWORD error = GetLastError();
        result.status = OperationStatus::SingleApplyFailed;
        result.native_outcome_known = false;
        result.diagnostic = win32_diagnostic(
            "SetWindowPos",
            error,
            "single native apply failed; no rollback is assumed");
        recapture_operation_windows(*this, result, true);
        return result;
    }

    result.stage = OperationStage::PostVerification;
    recapture_operation_windows(*this, result, false);
    if (!result.windows.front().actual.has_value()) {
        result.status = OperationStatus::WindowInvalidatedDuringApply;
        result.diagnostic = adapter_diagnostic(
            AdapterDiagnosticCode::WindowChanged,
            "OwnedWindowOperations",
            "owned window could not be captured after single apply");
        return result;
    }
    if (result.verified_count != 1U) {
        result.status = OperationStatus::PostVerificationFailed;
        result.diagnostic = adapter_diagnostic(
            AdapterDiagnosticCode::InvalidGeometry,
            "OwnedWindowOperations",
            "single requested and actual geometry or context differ");
        return result;
    }

    result.status = OperationStatus::Succeeded;
    return result;
}

OperationResult OwnedWindowOperations::apply(
    const std::span<const WindowTranslationRequest> requests) {
    OperationResult result;
    result.requested_count = requests.size();

    result.operation_batch_id = registry_->impl_->issue_operation_batch_id();
    if (result.operation_batch_id == 0U) {
        result.status = OperationStatus::GenerationExhausted;
        result.diagnostic = adapter_diagnostic(
            AdapterDiagnosticCode::BatchIdExhausted,
            "OwnedWindowOperations",
            "operation batch identifier space is exhausted");
        return result;
    }

    result.windows.reserve(requests.size());
    std::vector<OwnedWindowToken> tokens;
    tokens.reserve(requests.size());
    for (const auto& request : requests) {
        result.windows.push_back(
            WindowOperationReceipt{request.token, request.target_visible_rect});
        tokens.push_back(request.token);
    }

    const auto token_validation = registry_->impl_->validate_tokens(tokens);
    switch (token_validation) {
    case detail::BatchTokenValidation::Empty:
        result.status = OperationStatus::InvalidRequest;
        result.diagnostic = adapter_diagnostic(AdapterDiagnosticCode::EmptyBatch,
                                               "OwnedWindowOperations",
                                               "translation batch must not be empty");
        return result;
    case detail::BatchTokenValidation::Duplicate:
        result.status = OperationStatus::DuplicateToken;
        result.diagnostic = adapter_diagnostic(
            AdapterDiagnosticCode::DuplicateToken,
            "OwnedWindowOperations",
            "translation batch contains a duplicate capability");
        return result;
    case detail::BatchTokenValidation::Unknown:
        result.status = OperationStatus::StaleToken;
        result.diagnostic = adapter_diagnostic(AdapterDiagnosticCode::StaleToken,
                                               "OwnedWindowOperations",
                                               "translation batch contains a stale token");
        return result;
    case detail::BatchTokenValidation::Succeeded:
        break;
    }

    if (requests.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        result.status = OperationStatus::InvalidRequest;
        result.diagnostic = adapter_diagnostic(
            AdapterDiagnosticCode::EmptyBatch,
            "BeginDeferWindowPos",
            "translation batch count is not representable as int");
        return result;
    }

    std::vector<PreparedOperation> prepared;
    prepared.reserve(requests.size());
    for (std::size_t index = 0U; index < requests.size(); ++index) {
        const auto& request = requests[index];
        auto resolved = registry_->impl_->resolve(request.token);
        if (!resolved.value.has_value()) {
            result.status = resolved.status;
            result.diagnostic = std::move(resolved.diagnostic);
            return result;
        }

        auto captured = capture_native_owned_window(resolved.value->window);
        if (!captured.succeeded()) {
            result.status = captured.status;
            result.diagnostic = std::move(captured.diagnostic);
            return result;
        }

        const auto bridge = detail::bridge_visible_translation(
            captured.snapshot->positioning_rect,
            captured.snapshot->visible_rect,
            request.target_visible_rect);
        if (bridge.status != detail::TranslationBridgeStatus::Succeeded) {
            result.status = bridge_operation_status(bridge.status);
            result.diagnostic = adapter_diagnostic(
                AdapterDiagnosticCode::InvalidGeometry,
                "bridge_visible_translation",
                "visible target is not a representable pure translation");
            return result;
        }

        auto confirmed = registry_->impl_->resolve(request.token);
        if (!confirmed.value.has_value() ||
            confirmed.value->window != resolved.value->window) {
            result.status = OperationStatus::WindowInvalidatedDuringApply;
            result.diagnostic = confirmed.diagnostic.has_value()
                                    ? std::move(confirmed.diagnostic)
                                    : std::optional<OperationDiagnostic>{
                                          adapter_diagnostic(
                                              AdapterDiagnosticCode::WindowChanged,
                                              "OwnedWindowRegistry",
                                              "owned HWND changed during preflight")};
            return result;
        }

        result.windows[index].before = *captured.snapshot;
        result.windows[index].requested_positioning_rect =
            *bridge.target_positioning_rect;
        prepared.push_back(PreparedOperation{request.token,
                                             resolved.value->window,
                                             *bridge.target_positioning_rect});
    }

    const HWND shared_parent = GetAncestor(prepared.front().window, GA_PARENT);
    if (std::any_of(prepared.begin(), prepared.end(), [&](const auto& operation) {
            return GetAncestor(operation.window, GA_PARENT) != shared_parent;
        })) {
        result.status = OperationStatus::InvalidRequest;
        result.diagnostic = adapter_diagnostic(
            AdapterDiagnosticCode::WindowChanged,
            "GetAncestor",
            "DeferWindowPos batch members must share one native parent");
        return result;
    }

    // Revalidate the complete member set after every capture/bridge succeeds
    // and immediately before creating the native batch.
    for (const auto& operation : prepared) {
        auto resolved = registry_->impl_->resolve(operation.token);
        if (!resolved.value.has_value() ||
            resolved.value->window != operation.window) {
            result.status = OperationStatus::WindowInvalidatedDuringApply;
            result.diagnostic = resolved.diagnostic.has_value()
                                    ? std::move(resolved.diagnostic)
                                    : std::optional<OperationDiagnostic>{
                                          adapter_diagnostic(
                                              AdapterDiagnosticCode::WindowChanged,
                                              "OwnedWindowRegistry",
                                              "owned HWND changed before native batch")};
            return result;
        }
    }

    HDWP deferred = BeginDeferWindowPos(static_cast<int>(prepared.size()));
    if (deferred == nullptr) {
        const DWORD error = GetLastError();
        result.status = OperationStatus::BeginBatchFailed;
        result.stage = OperationStage::BeginBatch;
        result.diagnostic = win32_diagnostic("BeginDeferWindowPos",
                                             error,
                                             "native batch allocation failed");
        recapture_operation_windows(*this, result, true);
        return result;
    }

    for (const auto& operation : prepared) {
        const auto& target = operation.target_positioning_rect;
        HDWP updated = DeferWindowPos(deferred,
                                     operation.window,
                                     nullptr,
                                     static_cast<int>(target.left()),
                                     static_cast<int>(target.top()),
                                     0,
                                     0,
                                     kTranslationFlags);
        if (updated == nullptr) {
            const DWORD error = GetLastError();
            // The documented contract requires abandoning this chain. Do not
            // call EndDeferWindowPos, including from cleanup code.
            deferred = nullptr;
            result.status = OperationStatus::DeferBatchFailed;
            result.stage = OperationStage::DeferBatch;
            result.diagnostic = win32_diagnostic("DeferWindowPos",
                                                 error,
                                                 "native defer chain failed and was abandoned");
            recapture_operation_windows(*this, result, true);
            return result;
        }
        deferred = updated;
        ++result.deferred_count;
    }

    result.native_apply_attempted = true;
    if (!EndDeferWindowPos(deferred)) {
        const DWORD error = GetLastError();
        result.status = OperationStatus::EndBatchFailed;
        result.stage = OperationStage::EndBatch;
        result.native_outcome_known = false;
        result.diagnostic = win32_diagnostic(
            "EndDeferWindowPos",
            error,
            "native batch failed; no rollback or atomicity is assumed");
        recapture_operation_windows(*this, result, true);
        return result;
    }

    result.stage = OperationStage::PostVerification;
    recapture_operation_windows(*this, result, false);
    const bool all_captured = std::all_of(
        result.windows.begin(), result.windows.end(), [](const auto& receipt) {
            return receipt.actual.has_value();
        });
    if (!all_captured) {
        result.status = OperationStatus::WindowInvalidatedDuringApply;
        result.diagnostic = adapter_diagnostic(
            AdapterDiagnosticCode::WindowChanged,
            "OwnedWindowOperations",
            "one or more owned windows could not be captured after apply");
        return result;
    }
    if (result.verified_count != result.requested_count) {
        result.status = OperationStatus::PostVerificationFailed;
        result.diagnostic = adapter_diagnostic(
            AdapterDiagnosticCode::InvalidGeometry,
            "OwnedWindowOperations",
            "requested and actual geometry or DPI/monitor context differ");
        return result;
    }

    result.status = OperationStatus::Succeeded;
    return result;
}

} // namespace panebind::platform::windows::operations
