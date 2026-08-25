#include "platform/windows/windows_observer.h"

#include "core/events/window_event.h"
#include "platform/windows/text_encoding.h"

#include <dwmapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <deque>
#include <iomanip>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace panebind::platform::windows {
namespace {

constexpr UINT kDrainEventsMessage = WM_APP + 1U;
constexpr UINT kStopObserverMessage = WM_APP + 2U;
constexpr std::size_t kEventQueueCapacity = 4096U;
constexpr DWORD kHookFlags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;
constexpr std::size_t kMaximumTextCharacters = 32768U;

std::atomic<WindowsObserver*> callback_observer{};
std::atomic<DWORD> console_observer_thread_id{};
std::atomic<HANDLE> console_shutdown_complete_event{};
std::atomic<bool> console_close_handler_waiting{};

class UniqueHandle final {
public:
    explicit UniqueHandle(HANDLE handle) noexcept : handle_(handle) {}
    ~UniqueHandle() {
        if (handle_ != nullptr) {
            CloseHandle(handle_);
        }
    }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    [[nodiscard]] HANDLE get() const noexcept { return handle_; }

private:
    HANDLE handle_{};
};

[[nodiscard]] FieldError win32_error(std::string api,
                                     std::string field,
                                     const DWORD code) {
    return {std::move(api), std::move(field), "win32", code};
}

[[nodiscard]] FieldError hresult_error(std::string api,
                                      std::string field,
                                      const HRESULT code) {
    return {std::move(api),
            std::move(field),
            "hresult",
            static_cast<std::uint32_t>(code)};
}

enum class ObserverErrorCode : std::uint64_t {
    UnavailableReturn = 1,
    InvalidWindow = 2,
    IdentityChanged = 3,
};

[[nodiscard]] FieldError observer_error(std::string api,
                                        std::string field,
                                        const ObserverErrorCode code) {
    return {std::move(api),
            std::move(field),
            "observer",
            static_cast<std::uint64_t>(code)};
}

[[nodiscard]] NativeRect native_rect(const RECT& rect) noexcept {
    return {rect.left, rect.top, rect.right, rect.bottom};
}

[[nodiscard]] core::geometry::Rect normalized_rect(const NativeRect& rect) noexcept {
    return {rect.left, rect.top, rect.right, rect.bottom};
}

template <typename NativeHandle>
[[nodiscard]] std::string native_handle_id(const NativeHandle handle) {
    const auto value = reinterpret_cast<std::uintptr_t>(handle);
    std::ostringstream output;
    output << "0x" << std::hex << std::setfill('0')
           << std::setw(static_cast<int>(sizeof(std::uintptr_t) * 2U)) << value;
    return output.str();
}

[[nodiscard]] std::string hex_bits(const std::uint64_t value) {
    std::ostringstream output;
    output << "0x" << std::hex << std::setfill('0') << std::setw(16) << value;
    return output.str();
}

[[nodiscard]] std::string utc_timestamp(
    const std::chrono::system_clock::time_point time_point) {
    const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        time_point.time_since_epoch());
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(microseconds);
    const auto fraction = microseconds - seconds;
    const std::time_t raw_time = static_cast<std::time_t>(seconds.count());
    std::tm utc{};
    gmtime_s(&utc, &raw_time);

    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << '.' << std::setfill('0')
           << std::setw(6) << fraction.count() << 'Z';
    return output.str();
}

[[nodiscard]] std::string_view native_event_name(const DWORD event) noexcept {
    switch (event) {
    case EVENT_SYSTEM_MOVESIZESTART:
        return "EVENT_SYSTEM_MOVESIZESTART";
    case EVENT_OBJECT_LOCATIONCHANGE:
        return "EVENT_OBJECT_LOCATIONCHANGE";
    case EVENT_SYSTEM_MOVESIZEEND:
        return "EVENT_SYSTEM_MOVESIZEEND";
    default:
        return "unknown";
    }
}

[[nodiscard]] std::optional<core::events::WindowEventType> normalized_event_type(
    const DWORD event) noexcept {
    switch (event) {
    case EVENT_SYSTEM_MOVESIZESTART:
        return core::events::WindowEventType::MoveResizeStarted;
    case EVENT_OBJECT_LOCATIONCHANGE:
        return core::events::WindowEventType::GeometryChanged;
    case EVENT_SYSTEM_MOVESIZEEND:
        return core::events::WindowEventType::MoveResizeEnded;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] std::string_view normalized_event_name(
    const core::events::WindowEventType event) noexcept {
    switch (event) {
    case core::events::WindowEventType::MoveResizeStarted:
        return "MoveResizeStarted";
    case core::events::WindowEventType::GeometryChanged:
        return "GeometryChanged";
    case core::events::WindowEventType::MoveResizeEnded:
        return "MoveResizeEnded";
    }
    return "Unknown";
}

[[nodiscard]] std::string dpi_awareness_name(const DPI_AWARENESS_CONTEXT context) {
    if (context == nullptr) {
        return "unavailable";
    }
    if (AreDpiAwarenessContextsEqual(context,
                                     DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        return "per_monitor_aware_v2";
    }
    if (AreDpiAwarenessContextsEqual(context, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
        return "per_monitor_aware";
    }
    if (AreDpiAwarenessContextsEqual(context, DPI_AWARENESS_CONTEXT_SYSTEM_AWARE)) {
        return "system_aware";
    }
#ifdef DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED
    if (AreDpiAwarenessContextsEqual(context, DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED)) {
        return "unaware_gdi_scaled";
    }
#endif
    if (AreDpiAwarenessContextsEqual(context, DPI_AWARENESS_CONTEXT_UNAWARE)) {
        return "unaware";
    }
    return "unknown";
}

void append_rect(std::ostringstream& output, const NativeRect& rect) {
    output << "{\"left\":" << rect.left << ",\"top\":" << rect.top
           << ",\"right\":" << rect.right << ",\"bottom\":" << rect.bottom
           << '}';
}

template <typename Value, typename WriteValue>
void append_optional(std::ostringstream& output,
                     const std::optional<Value>& value,
                     WriteValue write_value) {
    if (value.has_value()) {
        write_value(*value);
    } else {
        output << "null";
    }
}

void append_field_errors(std::ostringstream& output,
                         const std::vector<FieldError>& errors) {
    output << '[';
    bool first = true;
    for (const auto& error : errors) {
        if (!first) {
            output << ',';
        }
        first = false;
        output << "{\"api\":" << json_quote(error.api)
               << ",\"field\":" << json_quote(error.field)
               << ",\"domain\":" << json_quote(error.domain)
               << ",\"code\":" << error.code << '}';
    }
    output << ']';
}

void append_normalized_rect(std::ostringstream& output,
                            const std::optional<core::geometry::Rect>& rect) {
    append_optional(output, rect, [&output](const core::geometry::Rect& value) {
        output << "{\"left\":" << value.left() << ",\"top\":" << value.top()
               << ",\"right\":" << value.right() << ",\"bottom\":"
               << value.bottom() << '}';
    });
}

[[nodiscard]] std::string_view window_state_name(
    const core::model::WindowState state) noexcept {
    switch (state) {
    case core::model::WindowState::Normal:
        return "normal";
    case core::model::WindowState::Minimized:
        return "minimized";
    case core::model::WindowState::Maximized:
        return "maximized";
    }
    return "unknown";
}

void append_normalized_snapshot(
    std::ostringstream& output,
    const core::model::NormalizedWindowSnapshot& snapshot) {
    output << "{\"window_id\":" << json_quote(snapshot.id.value())
           << ",\"process_id\":" << snapshot.process_id.value()
           << ",\"process_name\":";
    append_optional(output, snapshot.process_name, [&output](const std::string& value) {
        output << json_quote(value);
    });
    output << ",\"title\":";
    append_optional(output, snapshot.title, [&output](const std::string& value) {
        output << json_quote(value);
    });
    output << ",\"visible\":";
    append_optional(output, snapshot.visible, [&output](const bool value) {
        output << (value ? "true" : "false");
    });
    output << ",\"state\":";
    append_optional(output, snapshot.state, [&output](const core::model::WindowState value) {
        output << json_quote(window_state_name(value));
    });
    output << ",\"positioning_bounds\":";
    append_normalized_rect(output, snapshot.positioning_bounds);
    output << ",\"visible_frame_bounds\":";
    append_normalized_rect(output, snapshot.visible_frame_bounds);
    output << ",\"display_id\":";
    append_optional(output, snapshot.display_id, [&output](const std::string& value) {
        output << json_quote(value);
    });
    output << ",\"display_work_area\":";
    append_normalized_rect(output, snapshot.display_work_area);
    output << ",\"effective_window_scale\":";
    append_optional(output, snapshot.effective_window_scale, [&output](const double value) {
        output << value;
    });
    output << '}';
}

void append_windows_snapshot(std::ostringstream& output,
                             const WindowsWindowSnapshot& snapshot) {
    output << "{\"native_window_id\":" << json_quote(native_handle_id(snapshot.window))
           << ",\"pid\":" << snapshot.process_id
           << ",\"window_thread_id\":" << snapshot.window_thread_id
           << ",\"process_path\":";
    append_optional(output, snapshot.process_path, [&output](const std::string& value) {
        output << json_quote(value);
    });
    output << ",\"process_name\":";
    append_optional(output, snapshot.process_name, [&output](const std::string& value) {
        output << json_quote(value);
    });
    output << ",\"title\":";
    append_optional(output, snapshot.title, [&output](const std::string& value) {
        output << json_quote(value);
    });
    output << ",\"class_name\":";
    append_optional(output, snapshot.class_name, [&output](const std::string& value) {
        output << json_quote(value);
    });
    output << ",\"style\":";
    append_optional(output, snapshot.style, [&output](const std::uint64_t value) {
        output << json_quote(hex_bits(value));
    });
    output << ",\"extended_style\":";
    append_optional(output,
                    snapshot.extended_style,
                    [&output](const std::uint64_t value) {
                        output << json_quote(hex_bits(value));
                    });
    output << ",\"visible\":" << (snapshot.visible ? "true" : "false")
           << ",\"minimized\":" << (snapshot.minimized ? "true" : "false")
           << ",\"maximized\":" << (snapshot.maximized ? "true" : "false")
           << ",\"cloak_mask\":";
    append_optional(output, snapshot.cloak_mask, [&output](const DWORD value) {
        output << value;
    });
    output << ",\"positioning_bounds\":";
    append_optional(output, snapshot.positioning_bounds, [&output](const NativeRect& value) {
        append_rect(output, value);
    });
    output << ",\"visible_frame_bounds\":";
    append_optional(output,
                    snapshot.visible_frame_bounds,
                    [&output](const NativeRect& value) { append_rect(output, value); });
    output << ",\"coordinate_space\":\"virtual_screen_exclusive_edges\""
           << ",\"monitor_selection_policy\":\"nearest\""
           << ",\"native_monitor_id\":";
    if (snapshot.monitor != nullptr) {
        output << json_quote(native_handle_id(snapshot.monitor));
    } else {
        output << "null";
    }
    output << ",\"monitor_device_name\":";
    append_optional(output,
                    snapshot.monitor_device_name,
                    [&output](const std::string& value) { output << json_quote(value); });
    output << ",\"monitor_bounds\":";
    append_optional(output, snapshot.monitor_bounds, [&output](const NativeRect& value) {
        append_rect(output, value);
    });
    output << ",\"monitor_work_area\":";
    append_optional(output, snapshot.monitor_work_area, [&output](const NativeRect& value) {
        append_rect(output, value);
    });
    output << ",\"monitor_primary\":";
    append_optional(output, snapshot.monitor_primary, [&output](const bool value) {
        output << (value ? "true" : "false");
    });
    output << ",\"window_dpi\":";
    append_optional(output, snapshot.window_dpi, [&output](const UINT value) {
        output << value;
    });
    output << ",\"window_dpi_awareness\":";
    append_optional(output,
                    snapshot.window_dpi_awareness,
                    [&output](const std::string& value) { output << json_quote(value); });
    output << ",\"dpi_geometry_context_valid\":"
           << (snapshot.dpi_geometry_context_valid ? "true" : "false")
           << ",\"identity_valid\":" << (snapshot.identity_valid ? "true" : "false")
           << '}';
}

void append_utf8(std::optional<std::string>& target,
                 const std::wstring_view source,
                 std::vector<FieldError>& errors,
                 const std::string_view api,
                 const std::string_view field) {
    auto result = utf16_to_utf8(source);
    if (result.value.has_value()) {
        target = std::move(result.value);
    } else {
        errors.push_back(win32_error(std::string(api),
                                     std::string(field),
                                     result.error_code));
    }
}

[[nodiscard]] BOOL WINAPI console_control_handler(const DWORD control_type) {
    switch (control_type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT: {
        const DWORD thread_id = console_observer_thread_id.load(std::memory_order_acquire);
        return thread_id != 0U && PostThreadMessageW(thread_id, kStopObserverMessage, 0, 0);
    }
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT: {
        console_close_handler_waiting.store(true, std::memory_order_release);
        const DWORD thread_id = console_observer_thread_id.load(std::memory_order_acquire);
        const HANDLE completion =
            console_shutdown_complete_event.load(std::memory_order_acquire);
        if (thread_id == 0U || completion == nullptr ||
            !PostThreadMessageW(thread_id, kStopObserverMessage, 0, 0)) {
            console_close_handler_waiting.store(false, std::memory_order_release);
            return FALSE;
        }

        const DWORD wait_result = WaitForSingleObject(completion, 4000U);
        console_close_handler_waiting.store(false, std::memory_order_release);
        return wait_result == WAIT_OBJECT_0;
    }
    default:
        return FALSE;
    }
}

} // namespace

core::model::NormalizedWindowSnapshot
to_normalized_window_snapshot(const WindowsWindowSnapshot& snapshot) {
    core::model::NormalizedWindowSnapshot normalized{
        core::events::WindowId{"win32:" + native_handle_id(snapshot.window)},
        core::model::ProcessId{snapshot.process_id}};
    normalized.process_name = snapshot.process_name;
    normalized.title = snapshot.title;
    normalized.visible = snapshot.visible;
    normalized.state = snapshot.minimized
                           ? core::model::WindowState::Minimized
                           : (snapshot.maximized ? core::model::WindowState::Maximized
                                                 : core::model::WindowState::Normal);
    // Raw native rectangles remain logged even if the manifest/thread context
    // verification fails, but DPI-sensitive geometry is then not promoted to
    // the platform-neutral model as a valid comparable coordinate space.
    if (snapshot.dpi_geometry_context_valid && !snapshot.minimized) {
        if (snapshot.positioning_bounds.has_value()) {
            normalized.positioning_bounds = normalized_rect(*snapshot.positioning_bounds);
        }
        if (snapshot.visible_frame_bounds.has_value()) {
            normalized.visible_frame_bounds = normalized_rect(*snapshot.visible_frame_bounds);
        }
    }
    normalized.display_id = snapshot.monitor_device_name;
    if (snapshot.dpi_geometry_context_valid && snapshot.monitor_work_area.has_value()) {
        normalized.display_work_area = normalized_rect(*snapshot.monitor_work_area);
    }
    if (snapshot.window_dpi.has_value()) {
        normalized.effective_window_scale = static_cast<double>(*snapshot.window_dpi) / 96.0;
    }
    return normalized;
}

WindowsObserver::WindowsObserver(std::ostream& output)
    : output_(output), own_process_id_(GetCurrentProcessId()) {}

WindowsObserver::~WindowsObserver() {
    if ((move_size_hook_ != nullptr || location_hook_ != nullptr) &&
        GetCurrentThreadId() == owner_thread_id_) {
        static_cast<void>(uninstall_hooks());
    }
    if (callback_target_active_) {
        callback_observer.store(nullptr, std::memory_order_release);
        callback_target_active_ = false;
    }
}

std::uint64_t WindowsObserver::next_sequence() noexcept {
    return next_sequence_.fetch_add(1U, std::memory_order_relaxed);
}

bool WindowsObserver::verify_thread_dpi_context() {
    const DPI_AWARENESS_CONTEXT context = GetThreadDpiAwarenessContext();
    thread_dpi_awareness_ = dpi_awareness_name(context);
    return context != nullptr &&
           AreDpiAwarenessContextsEqual(context,
                                        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

bool WindowsObserver::initialize_owner_thread() {
    owner_thread_id_ = GetCurrentThreadId();

    // Force creation of the thread message queue before callbacks can post work.
    MSG message{};
    static_cast<void>(PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE));

    thread_dpi_context_valid_ = verify_thread_dpi_context();
    emit_diagnostic("thread_dpi_context",
                    thread_dpi_context_valid_ ? "verified_per_monitor_v2"
                                              : "not_per_monitor_v2");
    return !output_failed_;
}

void CALLBACK WindowsObserver::win_event_callback(const HWINEVENTHOOK hook,
                                                   const DWORD event,
                                                   const HWND window,
                                                   const LONG object_id,
                                                   const LONG child_id,
                                                   const DWORD event_thread_id,
                                                   const DWORD event_time_ms) {
    WindowsObserver* observer = callback_observer.load(std::memory_order_acquire);
    if (observer != nullptr) {
        observer->receive_win_event(hook,
                                    event,
                                    window,
                                    object_id,
                                    child_id,
                                    event_thread_id,
                                    event_time_ms);
    }
}

void WindowsObserver::receive_win_event(const HWINEVENTHOOK hook,
                                        const DWORD event,
                                        const HWND window,
                                        const LONG object_id,
                                        const LONG child_id,
                                        const DWORD event_thread_id,
                                        const DWORD event_time_ms) noexcept {
    DWORD callback_process_id = 0;
    DWORD callback_window_thread_id = 0;
    bool callback_root_matches = false;
    if (window != nullptr) {
        callback_window_thread_id =
            GetWindowThreadProcessId(window, &callback_process_id);
        if (callback_window_thread_id == 0U) {
            callback_process_id = 0;
        }
        callback_root_matches = GetAncestor(window, GA_ROOT) == window;
    }

    RawWinEvent receipt{hook,
                        event,
                        window,
                        object_id,
                        child_id,
                        event_thread_id,
                        event_time_ms,
                        callback_window_thread_id,
                        callback_process_id,
                        callback_root_matches,
                        next_sequence(),
                        std::chrono::system_clock::now()};

    try {
        std::lock_guard lock(queue_mutex_);
        if (event_queue_.size() < kEventQueueCapacity) {
            event_queue_.push_back(receipt);
        } else {
            ++dropped_event_count_;
            if (first_dropped_sequence_ == 0U) {
                first_dropped_sequence_ = receipt.sequence;
            }
            last_dropped_sequence_ = receipt.sequence;
        }
    } catch (...) {
        std::lock_guard lock(queue_mutex_);
        ++dropped_event_count_;
        if (first_dropped_sequence_ == 0U) {
            first_dropped_sequence_ = receipt.sequence;
        }
        last_dropped_sequence_ = receipt.sequence;
    }

    const bool should_notify =
        !queue_notification_pending_.exchange(true, std::memory_order_acq_rel);
    if (should_notify &&
        !PostThreadMessageW(owner_thread_id_, kDrainEventsMessage, 0, 0)) {
        const DWORD error = GetLastError();
        queue_notification_pending_.store(false, std::memory_order_release);
        try {
            std::lock_guard lock(queue_mutex_);
            ++post_message_failure_count_;
            last_post_message_error_ = error;
        } catch (...) {
            // No exception may cross a WinEvent callback boundary. The queued
            // receipt (when any) is still drained during orderly shutdown.
        }
    }
}

bool WindowsObserver::install_hooks() {
    WindowsObserver* expected = nullptr;
    if (!callback_observer.compare_exchange_strong(expected,
                                                   this,
                                                   std::memory_order_acq_rel)) {
        emit_diagnostic("hook_registration", "observer_already_active");
        return false;
    }
    callback_target_active_ = true;

    SetLastError(ERROR_SUCCESS);
    move_size_hook_ = SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART,
                                     EVENT_SYSTEM_MOVESIZEEND,
                                     nullptr,
                                     &WindowsObserver::win_event_callback,
                                     0,
                                     0,
                                     kHookFlags);
    if (move_size_hook_ == nullptr) {
        emit_diagnostic("hook_registration",
                        "move_size_hook_failed",
                        {win32_error("SetWinEventHook",
                                     "move_size_hook",
                                     GetLastError())});
        callback_observer.store(nullptr, std::memory_order_release);
        callback_target_active_ = false;
        return false;
    }

    SetLastError(ERROR_SUCCESS);
    location_hook_ = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE,
                                    EVENT_OBJECT_LOCATIONCHANGE,
                                    nullptr,
                                    &WindowsObserver::win_event_callback,
                                    0,
                                    0,
                                    kHookFlags);
    if (location_hook_ == nullptr) {
        const DWORD registration_error = GetLastError();
        emit_diagnostic("hook_registration",
                        "location_hook_failed",
                        {win32_error("SetWinEventHook",
                                     "location_hook",
                                     registration_error)});
        if (!UnhookWinEvent(move_size_hook_)) {
            emit_diagnostic("hook_cleanup",
                            "move_size_unhook_failed",
                            {win32_error("UnhookWinEvent",
                                         "move_size_hook",
                                         GetLastError())});
        }
        move_size_hook_ = nullptr;
        callback_observer.store(nullptr, std::memory_order_release);
        callback_target_active_ = false;
        return false;
    }

    emit_diagnostic("hook_registration", "complete");
    return true;
}

bool WindowsObserver::uninstall_hooks() {
    bool succeeded = true;
    std::vector<FieldError> errors;

    if (location_hook_ != nullptr) {
        if (!UnhookWinEvent(location_hook_)) {
            succeeded = false;
            errors.push_back(win32_error("UnhookWinEvent",
                                         "location_hook",
                                         GetLastError()));
        }
        location_hook_ = nullptr;
    }
    if (move_size_hook_ != nullptr) {
        if (!UnhookWinEvent(move_size_hook_)) {
            succeeded = false;
            errors.push_back(win32_error("UnhookWinEvent",
                                         "move_size_hook",
                                         GetLastError()));
        }
        move_size_hook_ = nullptr;
    }

    callback_observer.store(nullptr, std::memory_order_release);
    callback_target_active_ = false;
    emit_diagnostic("hook_shutdown", succeeded ? "complete" : "incomplete", errors);
    return succeeded;
}

BOOL CALLBACK WindowsObserver::enum_windows_callback(const HWND window,
                                                      const LPARAM context) {
    auto* observer = reinterpret_cast<WindowsObserver*>(context);
    try {
        observer->emit_census_snapshot(window);
        if (observer->output_failed_) {
            SetLastError(ERROR_WRITE_FAULT);
            return FALSE;
        }
        return TRUE;
    } catch (...) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
}

bool WindowsObserver::enumerate_windows() {
    SetLastError(ERROR_SUCCESS);
    if (EnumWindows(&WindowsObserver::enum_windows_callback,
                    reinterpret_cast<LPARAM>(this))) {
        emit_diagnostic("initial_census", "complete");
        return !output_failed_;
    }

    emit_diagnostic("initial_census",
                    "failed",
                    {win32_error("EnumWindows", "initial_census", GetLastError())});
    return false;
}

WindowsObserver::SnapshotResult WindowsObserver::inspect_window(
    const HWND window,
    const std::optional<DWORD> expected_process_id,
    const std::optional<DWORD> expected_window_thread_id) {

    SnapshotResult result;
    if (window == nullptr) {
        result.disposition = "null_window";
        return result;
    }
    if (!IsWindow(window)) {
        result.disposition = "invalid_or_destroyed_window";
        result.field_errors.push_back(observer_error("IsWindow",
                                                     "window_identity",
                                                     ObserverErrorCode::InvalidWindow));
        return result;
    }

    SetLastError(ERROR_SUCCESS);
    const HWND root = GetAncestor(window, GA_ROOT);
    if (root == nullptr) {
        result.disposition = "root_unavailable";
        result.field_errors.push_back(observer_error("GetAncestor",
                                                     "root_window",
                                                     ObserverErrorCode::UnavailableReturn));
        return result;
    }
    if (root != window) {
        result.disposition = "child_or_nonroot";
        return result;
    }

    DWORD process_id = 0;
    SetLastError(ERROR_SUCCESS);
    const DWORD window_thread_id = GetWindowThreadProcessId(window, &process_id);
    if (window_thread_id == 0U || process_id == 0U) {
        result.disposition = "pid_unavailable";
        result.field_errors.push_back(observer_error(
            "GetWindowThreadProcessId",
            "process_id",
            ObserverErrorCode::UnavailableReturn));
        return result;
    }
    if ((expected_process_id.has_value() && process_id != *expected_process_id) ||
        (expected_window_thread_id.has_value() &&
         window_thread_id != *expected_window_thread_id)) {
        result.disposition = "identity_changed_since_receipt";
        result.field_errors.push_back(observer_error(
            "GetWindowThreadProcessId",
            "receipt_identity_match",
            ObserverErrorCode::IdentityChanged));
        return result;
    }
    if (process_id == own_process_id_) {
        result.disposition = "own_process";
        return result;
    }

    result.snapshot = capture_snapshot(window, process_id, window_thread_id);
    result.field_errors = result.snapshot->field_errors;
    if (!result.snapshot->identity_valid) {
        result.disposition = "identity_changed_during_snapshot";
        return result;
    }

    result.observed_snapshot = true;
    if (!result.snapshot->visible) {
        result.disposition = "not_visible";
        return result;
    }
    if (!result.snapshot->cloak_mask.has_value()) {
        result.disposition = "cloak_state_unavailable";
        return result;
    }
    if (*result.snapshot->cloak_mask != 0U) {
        result.disposition = "cloaked";
        return result;
    }
    if (!result.snapshot->extended_style.has_value()) {
        result.disposition = "extended_style_unavailable";
        return result;
    }

    const std::uint64_t extended_style = *result.snapshot->extended_style;
    const bool tool_window = (extended_style & WS_EX_TOOLWINDOW) != 0U;
    const bool app_window = (extended_style & WS_EX_APPWINDOW) != 0U;
    if (tool_window && !app_window) {
        result.disposition = "tool_window_without_appwindow";
        return result;
    }

    result.default_candidate = true;
    result.disposition = "accepted";
    return result;
}

WindowsWindowSnapshot WindowsObserver::capture_snapshot(const HWND window,
                                                        const DWORD process_id,
                                                        const DWORD window_thread_id) {
    WindowsWindowSnapshot snapshot;
    snapshot.window = window;
    snapshot.process_id = process_id;
    snapshot.window_thread_id = window_thread_id;
    snapshot.captured_at = std::chrono::system_clock::now();
    snapshot.dpi_geometry_context_valid = thread_dpi_context_valid_;

    SetLastError(ERROR_SUCCESS);
    UniqueHandle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                     FALSE,
                                     process_id)};
    if (process.get() == nullptr) {
        snapshot.field_errors.push_back(
            win32_error("OpenProcess", "process_path", GetLastError()));
    } else {
        DWORD capacity = 1024U;
        while (capacity <= kMaximumTextCharacters) {
            std::vector<wchar_t> path_buffer(capacity);
            DWORD character_count = capacity;
            SetLastError(ERROR_SUCCESS);
            if (QueryFullProcessImageNameW(process.get(),
                                           0,
                                           path_buffer.data(),
                                           &character_count)) {
                append_utf8(snapshot.process_path,
                            std::wstring_view{path_buffer.data(), character_count},
                            snapshot.field_errors,
                            "QueryFullProcessImageNameW",
                            "process_path");
                if (snapshot.process_path.has_value()) {
                    const std::size_t separator = snapshot.process_path->find_last_of("\\/");
                    snapshot.process_name = separator == std::string::npos
                                                ? *snapshot.process_path
                                                : snapshot.process_path->substr(separator + 1U);
                }
                break;
            }

            const DWORD error = GetLastError();
            if (error == ERROR_INSUFFICIENT_BUFFER &&
                capacity < kMaximumTextCharacters) {
                capacity = std::min<DWORD>(capacity * 2U,
                                           static_cast<DWORD>(kMaximumTextCharacters));
                continue;
            }
            snapshot.field_errors.push_back(win32_error(
                "QueryFullProcessImageNameW", "process_path", error));
            break;
        }
    }

    std::vector<wchar_t> title_buffer(kMaximumTextCharacters);
    SetLastError(ERROR_SUCCESS);
    const int title_length = GetWindowTextW(window,
                                            title_buffer.data(),
                                            static_cast<int>(title_buffer.size()));
    const DWORD title_error = GetLastError();
    if (title_length > 0) {
        append_utf8(snapshot.title,
                    std::wstring_view{title_buffer.data(),
                                      static_cast<std::size_t>(title_length)},
                    snapshot.field_errors,
                    "GetWindowTextW",
                    "title");
        if (static_cast<std::size_t>(title_length) == title_buffer.size() - 1U) {
            snapshot.field_errors.push_back(win32_error(
                "GetWindowTextW", "title_truncated", ERROR_INSUFFICIENT_BUFFER));
        }
    } else if (title_error == ERROR_SUCCESS) {
        snapshot.title = std::string{};
    } else {
        snapshot.field_errors.push_back(
            win32_error("GetWindowTextW", "title", title_error));
    }

    std::array<wchar_t, 1024U> class_buffer{};
    SetLastError(ERROR_SUCCESS);
    const int class_length = GetClassNameW(window,
                                           class_buffer.data(),
                                           static_cast<int>(class_buffer.size()));
    if (class_length > 0) {
        append_utf8(snapshot.class_name,
                    std::wstring_view{class_buffer.data(),
                                      static_cast<std::size_t>(class_length)},
                    snapshot.field_errors,
                    "GetClassNameW",
                    "class_name");
        if (static_cast<std::size_t>(class_length) == class_buffer.size() - 1U) {
            snapshot.field_errors.push_back(win32_error(
                "GetClassNameW", "class_name_truncated", ERROR_INSUFFICIENT_BUFFER));
        }
    } else {
        snapshot.field_errors.push_back(
            win32_error("GetClassNameW", "class_name", GetLastError()));
    }

    SetLastError(ERROR_SUCCESS);
    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    const DWORD style_error = GetLastError();
    if (style != 0 || style_error == ERROR_SUCCESS) {
        snapshot.style = static_cast<std::uint64_t>(static_cast<ULONG_PTR>(style));
    } else {
        snapshot.field_errors.push_back(
            win32_error("GetWindowLongPtrW", "style", style_error));
    }

    SetLastError(ERROR_SUCCESS);
    const LONG_PTR extended_style = GetWindowLongPtrW(window, GWL_EXSTYLE);
    const DWORD extended_style_error = GetLastError();
    if (extended_style != 0 || extended_style_error == ERROR_SUCCESS) {
        snapshot.extended_style =
            static_cast<std::uint64_t>(static_cast<ULONG_PTR>(extended_style));
    } else {
        snapshot.field_errors.push_back(win32_error(
            "GetWindowLongPtrW", "extended_style", extended_style_error));
    }

    snapshot.visible = IsWindowVisible(window) != FALSE;
    snapshot.minimized = IsIconic(window) != FALSE;
    snapshot.maximized = IsZoomed(window) != FALSE;

    DWORD cloak_mask = 0;
    HRESULT result = DwmGetWindowAttribute(window,
                                           DWMWA_CLOAKED,
                                           &cloak_mask,
                                           sizeof(cloak_mask));
    if (SUCCEEDED(result)) {
        snapshot.cloak_mask = cloak_mask;
    } else {
        snapshot.field_errors.push_back(
            hresult_error("DwmGetWindowAttribute", "cloak_mask", result));
    }

    RECT positioning_bounds{};
    SetLastError(ERROR_SUCCESS);
    if (GetWindowRect(window, &positioning_bounds)) {
        snapshot.positioning_bounds = native_rect(positioning_bounds);
    } else {
        snapshot.field_errors.push_back(
            win32_error("GetWindowRect", "positioning_bounds", GetLastError()));
    }

    RECT visible_frame_bounds{};
    result = DwmGetWindowAttribute(window,
                                   DWMWA_EXTENDED_FRAME_BOUNDS,
                                   &visible_frame_bounds,
                                   sizeof(visible_frame_bounds));
    if (SUCCEEDED(result)) {
        snapshot.visible_frame_bounds = native_rect(visible_frame_bounds);
    } else {
        snapshot.field_errors.push_back(hresult_error(
            "DwmGetWindowAttribute", "visible_frame_bounds", result));
    }

    SetLastError(ERROR_SUCCESS);
    const UINT dpi = GetDpiForWindow(window);
    if (dpi != 0U) {
        snapshot.window_dpi = dpi;
    } else {
        snapshot.field_errors.push_back(observer_error(
            "GetDpiForWindow",
            "window_dpi",
            ObserverErrorCode::UnavailableReturn));
    }

    SetLastError(ERROR_SUCCESS);
    const DPI_AWARENESS_CONTEXT awareness = GetWindowDpiAwarenessContext(window);
    if (awareness != nullptr) {
        snapshot.window_dpi_awareness = dpi_awareness_name(awareness);
    } else {
        snapshot.field_errors.push_back(observer_error(
            "GetWindowDpiAwarenessContext",
            "window_dpi_awareness",
            ObserverErrorCode::UnavailableReturn));
    }

    snapshot.monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    if (snapshot.monitor != nullptr) {
        MONITORINFOEXW monitor_info{};
        monitor_info.cbSize = sizeof(monitor_info);
        SetLastError(ERROR_SUCCESS);
        if (GetMonitorInfoW(snapshot.monitor, &monitor_info)) {
            snapshot.monitor_bounds = native_rect(monitor_info.rcMonitor);
            snapshot.monitor_work_area = native_rect(monitor_info.rcWork);
            snapshot.monitor_primary = (monitor_info.dwFlags & MONITORINFOF_PRIMARY) != 0U;
            const std::size_t device_length =
                std::char_traits<wchar_t>::length(monitor_info.szDevice);
            append_utf8(snapshot.monitor_device_name,
                        std::wstring_view{monitor_info.szDevice, device_length},
                        snapshot.field_errors,
                        "GetMonitorInfoW",
                        "monitor_device_name");
        } else {
            snapshot.field_errors.push_back(
                win32_error("GetMonitorInfoW", "monitor_metadata", GetLastError()));
        }
    } else {
        snapshot.field_errors.push_back(observer_error(
            "MonitorFromWindow",
            "monitor",
            ObserverErrorCode::UnavailableReturn));
    }

    DWORD final_process_id = 0;
    SetLastError(ERROR_SUCCESS);
    const DWORD final_thread_id =
        GetWindowThreadProcessId(window, &final_process_id);
    if (final_thread_id == 0U || final_process_id == 0U) {
        snapshot.field_errors.push_back(observer_error(
            "GetWindowThreadProcessId",
            "identity_recheck",
            ObserverErrorCode::UnavailableReturn));
    } else if (final_process_id != process_id || final_thread_id != window_thread_id) {
        snapshot.field_errors.push_back(observer_error(
            "GetWindowThreadProcessId",
            "identity_recheck",
            ObserverErrorCode::IdentityChanged));
    } else {
        snapshot.identity_valid = true;
    }

    return snapshot;
}

void WindowsObserver::emit_event(const RawWinEvent& event) {
    SnapshotResult inspection;
    const auto normalized_type = normalized_event_type(event.event);

    if (!normalized_type.has_value()) {
        inspection.disposition = "unsupported_native_event";
    } else if (event.event == EVENT_OBJECT_LOCATIONCHANGE &&
               (event.object_id != OBJID_WINDOW || event.child_id != CHILDID_SELF)) {
        inspection.disposition = "location_object_or_child_mismatch";
    } else if (event.window == nullptr) {
        inspection.disposition = "null_window";
    } else if (!event.callback_root_matches) {
        inspection.disposition = "child_nonroot_or_destroyed_at_receipt";
    } else if (event.callback_process_id == 0U ||
               event.callback_window_thread_id == 0U) {
        inspection.disposition = "identity_unavailable_at_receipt";
    } else {
        inspection = inspect_window(event.window,
                                    event.callback_process_id,
                                    event.callback_window_thread_id);
    }

    std::ostringstream record;
    record << "{\"schema_version\":1,\"record_kind\":\"event\""
           << ",\"observer_sequence\":" << event.sequence
           << ",\"origin\":\"winevent\""
           << ",\"native_hook_id\":" << json_quote(native_handle_id(event.hook))
           << ",\"native_event\":{\"id\":" << event.event
           << ",\"name\":" << json_quote(native_event_name(event.event)) << '}'
           << ",\"native_window_id\":";
    if (event.window != nullptr) {
        record << json_quote(native_handle_id(event.window));
    } else {
        record << "null";
    }
    record << ",\"pid\":";
    if (inspection.snapshot.has_value()) {
        record << inspection.snapshot->process_id;
    } else {
        record << "null";
    }
    record << ",\"callback_pid\":" << event.callback_process_id
           << ",\"callback_window_thread_id\":"
           << event.callback_window_thread_id
           << ",\"callback_root_matches\":"
           << (event.callback_root_matches ? "true" : "false");
    record << ",\"native_object_id\":" << event.object_id
           << ",\"native_child_id\":" << event.child_id
           << ",\"native_event_thread_id\":" << event.event_thread_id
           << ",\"native_event_time_ms\":"
           << static_cast<std::uint64_t>(event.native_event_time_ms)
           << ",\"received_at\":" << json_quote(utc_timestamp(event.received_at))
           << ",\"snapshot_captured_at\":";
    if (inspection.snapshot.has_value()) {
        record << json_quote(utc_timestamp(inspection.snapshot->captured_at));
    } else {
        record << "null";
    }
    record << ",\"disposition\":" << json_quote(inspection.disposition)
           << ",\"observed_snapshot\":"
           << (inspection.observed_snapshot ? "true" : "false")
           << ",\"default_candidate\":"
           << (inspection.default_candidate ? "true" : "false");
    if (inspection.default_candidate && normalized_type.has_value()) {
        const core::events::WindowEvent normalized_event{
            *normalized_type,
            core::events::WindowId{"win32:" + native_handle_id(event.window)}};
        record << ",\"normalized_event\":"
               << json_quote(normalized_event_name(normalized_event.type));
    }
    record << ",\"snapshot\":";
    if (inspection.snapshot.has_value()) {
        append_windows_snapshot(record, *inspection.snapshot);
    } else {
        record << "null";
    }
    if (inspection.default_candidate && inspection.snapshot.has_value()) {
        record << ",\"normalized_snapshot\":";
        const auto normalized = to_normalized_window_snapshot(*inspection.snapshot);
        append_normalized_snapshot(record, normalized);
    }
    record << ",\"field_errors\":";
    append_field_errors(record, inspection.field_errors);
    record << '}';

    write_json_line(record.str());
}

void WindowsObserver::emit_census_snapshot(const HWND window) {
    const std::uint64_t sequence = next_sequence();
    const auto received_at = std::chrono::system_clock::now();
    const SnapshotResult inspection =
        inspect_window(window, std::nullopt, std::nullopt);

    std::ostringstream record;
    record << "{\"schema_version\":1,\"record_kind\":\"census_snapshot\""
           << ",\"observer_sequence\":" << sequence
           << ",\"origin\":\"initial_census\""
           << ",\"native_window_id\":" << json_quote(native_handle_id(window))
           << ",\"pid\":";
    if (inspection.snapshot.has_value()) {
        record << inspection.snapshot->process_id;
    } else {
        record << "null";
    }
    record << ",\"received_at\":" << json_quote(utc_timestamp(received_at))
           << ",\"snapshot_captured_at\":";
    if (inspection.snapshot.has_value()) {
        record << json_quote(utc_timestamp(inspection.snapshot->captured_at));
    } else {
        record << "null";
    }
    record << ",\"disposition\":" << json_quote(inspection.disposition)
           << ",\"observed_snapshot\":"
           << (inspection.observed_snapshot ? "true" : "false")
           << ",\"default_candidate\":"
           << (inspection.default_candidate ? "true" : "false")
           << ",\"snapshot\":";
    if (inspection.snapshot.has_value()) {
        append_windows_snapshot(record, *inspection.snapshot);
    } else {
        record << "null";
    }
    if (inspection.default_candidate && inspection.snapshot.has_value()) {
        record << ",\"normalized_snapshot\":";
        const auto normalized = to_normalized_window_snapshot(*inspection.snapshot);
        append_normalized_snapshot(record, normalized);
    }
    record << ",\"field_errors\":";
    append_field_errors(record, inspection.field_errors);
    record << '}';

    write_json_line(record.str());
}

void WindowsObserver::write_json_line(const std::string_view record) noexcept {
    if (output_failed_) {
        return;
    }

    try {
        output_.write(record.data(), static_cast<std::streamsize>(record.size()));
        output_.put('\n');
        output_.flush();
        if (!output_) {
            output_failed_ = true;
        }
    } catch (...) {
        output_failed_ = true;
    }
}

void WindowsObserver::emit_diagnostic(const std::string_view name,
                                      const std::string_view disposition,
                                      const std::vector<FieldError>& errors) {
    std::ostringstream record;
    record << "{\"schema_version\":1,\"record_kind\":\"diagnostic\""
           << ",\"observer_sequence\":" << next_sequence()
           << ",\"origin\":\"observer\""
           << ",\"received_at\":"
           << json_quote(utc_timestamp(std::chrono::system_clock::now()))
           << ",\"diagnostic\":" << json_quote(name)
           << ",\"disposition\":" << json_quote(disposition)
           << ",\"observer_process_id\":" << own_process_id_
           << ",\"owner_thread_id\":" << owner_thread_id_
           << ",\"thread_dpi_awareness\":" << json_quote(thread_dpi_awareness_)
           << ",\"dpi_geometry_context_valid\":"
           << (thread_dpi_context_valid_ ? "true" : "false")
           << ",\"field_errors\":";
    append_field_errors(record, errors);
    record << '}';
    write_json_line(record.str());
}

void WindowsObserver::emit_overflow_diagnostic() {
    std::uint64_t dropped_count = 0;
    std::uint64_t first_dropped = 0;
    std::uint64_t last_dropped = 0;
    std::uint64_t post_failure_count = 0;
    DWORD post_error = ERROR_SUCCESS;
    {
        std::lock_guard lock(queue_mutex_);
        dropped_count = std::exchange(dropped_event_count_, 0U);
        first_dropped = std::exchange(first_dropped_sequence_, 0U);
        last_dropped = std::exchange(last_dropped_sequence_, 0U);
        post_failure_count = std::exchange(post_message_failure_count_, 0U);
        post_error = std::exchange(last_post_message_error_, ERROR_SUCCESS);
    }

    if (dropped_count != 0U) {
        std::ostringstream record;
        record << "{\"schema_version\":1,\"record_kind\":\"diagnostic\""
               << ",\"observer_sequence\":" << next_sequence()
               << ",\"origin\":\"observer\""
               << ",\"received_at\":"
               << json_quote(utc_timestamp(std::chrono::system_clock::now()))
               << ",\"diagnostic\":\"event_queue_overflow\""
               << ",\"disposition\":\"events_dropped\""
               << ",\"dropped_event_count\":" << dropped_count
               << ",\"first_dropped_sequence\":" << first_dropped
               << ",\"last_dropped_sequence\":" << last_dropped
               << ",\"queue_capacity\":" << kEventQueueCapacity
               << ",\"field_errors\":[]}";
        write_json_line(record.str());
    }

    if (post_failure_count != 0U) {
        std::ostringstream record;
        record << "{\"schema_version\":1,\"record_kind\":\"diagnostic\""
               << ",\"observer_sequence\":" << next_sequence()
               << ",\"origin\":\"observer\""
               << ",\"received_at\":"
               << json_quote(utc_timestamp(std::chrono::system_clock::now()))
               << ",\"diagnostic\":\"queue_notification_failure\""
               << ",\"disposition\":\"queued_receipts_may_be_delayed\""
               << ",\"post_message_failure_count\":" << post_failure_count
               << ",\"field_errors\":";
        append_field_errors(record,
                            {win32_error("PostThreadMessageW",
                                         "event_queue_notification",
                                         post_error)});
        record << '}';
        write_json_line(record.str());
    }
}

void WindowsObserver::drain_event_queue() {
    queue_notification_pending_.store(false, std::memory_order_release);
    for (;;) {
        std::deque<RawWinEvent> batch;
        {
            std::lock_guard lock(queue_mutex_);
            if (event_queue_.empty()) {
                break;
            }
            batch.swap(event_queue_);
        }

        for (const auto& event : batch) {
            emit_event(event);
        }
    }
    emit_overflow_diagnostic();
}

int WindowsObserver::enumerate_only() {
    if (!initialize_owner_thread()) {
        return 1;
    }
    return enumerate_windows() && !output_failed_ ? 0 : 1;
}

int WindowsObserver::observe(const std::optional<std::chrono::seconds> duration) {
    if (!initialize_owner_thread()) {
        return 1;
    }
    if (!install_hooks()) {
        return 1;
    }

    bool succeeded = true;
    bool control_handler_installed = false;
    HANDLE shutdown_complete_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (shutdown_complete_event == nullptr) {
        emit_diagnostic("console_shutdown_event",
                        "creation_failed",
                        {win32_error("CreateEventW",
                                     "shutdown_complete_event",
                                     GetLastError())});
        succeeded = false;
    } else {
        console_shutdown_complete_event.store(shutdown_complete_event,
                                              std::memory_order_release);
        console_observer_thread_id.store(owner_thread_id_, std::memory_order_release);
        if (SetConsoleCtrlHandler(&console_control_handler, TRUE)) {
            control_handler_installed = true;
        } else {
            emit_diagnostic("console_control_handler",
                            "registration_failed",
                            {win32_error("SetConsoleCtrlHandler",
                                         "shutdown_handler",
                                         GetLastError())});
            succeeded = false;
        }
    }

    if (!enumerate_windows()) {
        succeeded = false;
    }
    drain_event_queue();
    if (output_failed_) {
        succeeded = false;
    }

    UINT_PTR timer_id = 0;
    if (duration.has_value() && succeeded) {
        const auto seconds = duration->count();
        const auto maximum_seconds =
            static_cast<std::int64_t>(USER_TIMER_MAXIMUM / 1000U);
        if (seconds <= 0 || seconds > maximum_seconds) {
            emit_diagnostic("observation_timer", "duration_out_of_range");
            succeeded = false;
        } else {
            const auto milliseconds = static_cast<UINT>(seconds * 1000);
            timer_id = SetTimer(nullptr, 0, milliseconds, nullptr);
            if (timer_id == 0U) {
                emit_diagnostic("observation_timer",
                                "registration_failed",
                                {win32_error("SetTimer",
                                             "observation_duration",
                                             GetLastError())});
                succeeded = false;
            }
        }
    }

    if (succeeded) {
        bool stop_requested = false;
        while (!stop_requested) {
            MSG message{};
            const BOOL result = GetMessageW(&message, nullptr, 0, 0);
            if (result == -1) {
                emit_diagnostic("message_loop",
                                "get_message_failed",
                                {win32_error("GetMessageW",
                                             "owner_message_loop",
                                             GetLastError())});
                succeeded = false;
                break;
            }
            if (result == 0) {
                break;
            }

            if (message.message == kDrainEventsMessage) {
                drain_event_queue();
                if (output_failed_) {
                    succeeded = false;
                    stop_requested = true;
                }
            } else if (message.message == kStopObserverMessage ||
                       (timer_id != 0U && message.message == WM_TIMER &&
                        message.wParam == timer_id)) {
                stop_requested = true;
            } else {
                static_cast<void>(TranslateMessage(&message));
                static_cast<void>(DispatchMessageW(&message));
            }
        }
    }

    if (timer_id != 0U) {
        if (!KillTimer(nullptr, timer_id)) {
            emit_diagnostic("observation_timer", "cleanup_failed");
            succeeded = false;
        }
    }

    if (!uninstall_hooks()) {
        succeeded = false;
    }
    drain_event_queue();
    if (control_handler_installed &&
        !SetConsoleCtrlHandler(&console_control_handler, FALSE)) {
        emit_diagnostic("console_control_handler",
                        "cleanup_failed",
                        {win32_error("SetConsoleCtrlHandler",
                                     "shutdown_handler",
                                     GetLastError())});
        succeeded = false;
    }
    emit_diagnostic("observer_shutdown", succeeded ? "complete" : "incomplete");

    if (shutdown_complete_event != nullptr) {
        static_cast<void>(SetEvent(shutdown_complete_event));
    }
    console_observer_thread_id.store(0U, std::memory_order_release);
    console_shutdown_complete_event.store(nullptr, std::memory_order_release);
    if (shutdown_complete_event != nullptr &&
        !console_close_handler_waiting.load(std::memory_order_acquire)) {
        CloseHandle(shutdown_complete_event);
    }
    return succeeded && !output_failed_ ? 0 : 1;
}

} // namespace panebind::platform::windows
