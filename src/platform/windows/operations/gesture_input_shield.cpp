#include "platform/windows/operations/gesture_input_shield.h"

#include <dwmapi.h>
#include <windowsx.h>
#include <wtsapi32.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <climits>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace panebind::platform::windows::operations {
namespace {

constexpr DWORD isolation_deadline_ms = 30000;
constexpr BYTE isolation_alpha = 2; // Zero alpha passes mouse hits through.
constexpr int stop_hotkey_id = 0x5043;
constexpr UINT stop_hotkey_key = VK_F11;
constexpr UINT stop_hotkey_modifiers = MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT;

HWND root_at(POINT point) noexcept {
    const HWND hit = WindowFromPoint(point);
    return hit ? GetAncestor(hit, GA_ROOT) : nullptr;
}

bool physical_move_buttons() noexcept {
    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) return false;
    // The product route (plain versus Ctrl) is frozen at native START by the
    // adapter. A modifier pressed later must not reclassify this gesture.
    for (const int key : {VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2}) {
        if ((GetAsyncKeyState(key) & 0x8000) != 0) return false;
    }
    return true;
}

bool interactive_default_desktop() noexcept {
    DWORD session{};
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session) || session == 0)
        return false;
    HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!input) return false;
    wchar_t input_name[256]{}, current_name[256]{};
    DWORD length{};
    const bool same = GetUserObjectInformationW(input, UOI_NAME, input_name,
            sizeof(input_name), &length) &&
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME,
            current_name, sizeof(current_name), &length) &&
        std::wstring_view(input_name) == std::wstring_view(current_name) &&
        std::wstring_view(input_name) == L"Default";
    CloseDesktop(input);
    if (!same) return false;
    LPWSTR data{};
    DWORD bytes{};
    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION,
                                     WTSSessionInfoEx, &data, &bytes)) return false;
    bool active = false;
    if (data && bytes >= sizeof(WTSINFOEXW)) {
        const auto& info = *reinterpret_cast<const WTSINFOEXW*>(data);
        active = info.Level == 1 && info.Data.WTSInfoExLevel1.SessionState == WTSActive &&
            info.Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_UNLOCK;
    }
    WTSFreeMemory(data);
    return active;
}

bool exact_source(const GestureShieldRequest& request) noexcept {
    DWORD actual_pid{};
    if (!request.generation || !request.source || !request.process_id || !request.thread_id ||
        !IsWindow(request.source) || GetAncestor(request.source, GA_ROOT) != request.source ||
        GetWindowThreadProcessId(request.source, &actual_pid) != request.thread_id ||
        actual_pid != request.process_id || !IsWindowVisible(request.source) ||
        IsIconic(request.source) || IsZoomed(request.source)) return false;
    return true;
}

GestureShieldNativeFailure native_plain_move_failure(
    const GestureShieldRequest& request) noexcept {
    if (!exact_source(request)) return GestureShieldNativeFailure::SourceIdentity;
    if (!interactive_default_desktop()) return GestureShieldNativeFailure::InputDesktop;
    if (GetForegroundWindow() != request.source)
        return GestureShieldNativeFailure::Foreground;
    if (!physical_move_buttons()) return GestureShieldNativeFailure::PhysicalButtons;
    GUITHREADINFO gui{sizeof(gui)};
    if (!GetGUIThreadInfo(request.thread_id, &gui))
        return GestureShieldNativeFailure::GuiQuery;
    if (gui.hwndCapture != request.source) return GestureShieldNativeFailure::Capture;
    if (gui.hwndMoveSize != request.source) return GestureShieldNativeFailure::MoveSize;
    if ((gui.flags & (GUI_INMENUMODE | GUI_SYSTEMMENUMODE | GUI_POPUPMENUMODE)) != 0 ||
        gui.hwndMenuOwner) return GestureShieldNativeFailure::MenuMode;
    return GestureShieldNativeFailure::None;
}

bool virtual_desktop_rect(RECT& result) noexcept {
    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    const auto right = static_cast<std::int64_t>(x) + width;
    const auto bottom = static_cast<std::int64_t>(y) + height;
    if (width <= 0 || height <= 0 || right > LONG_MAX || bottom > LONG_MAX)
        return false;
    result = RECT{x, y, static_cast<LONG>(right), static_cast<LONG>(bottom)};
    return true;
}

struct MonitorHitFacts {
    HWND overlay{};
    unsigned count{};
    unsigned hits{};
    bool overflow{};
};

BOOL CALLBACK count_monitor_hits(HMONITOR, HDC, LPRECT bounds, LPARAM param) noexcept {
    auto& facts = *reinterpret_cast<MonitorHitFacts*>(param);
    if (++facts.count > 16) {
        facts.overflow = true;
        return FALSE;
    }
    const POINT center{bounds->left + (bounds->right - bounds->left) / 2,
                       bounds->top + (bounds->bottom - bounds->top) / 2};
    if (root_at(center) == facts.overlay) ++facts.hits;
    return TRUE;
}

// Raw registration is process-wide per mouse TLC. Never replace an existing
// receiver (including a different PaneBind component) or remove its successor.
enum class MouseRegistration { Absent, Ours, Other, QueryFailed };

MouseRegistration mouse_registration(HWND receiver) noexcept {
    UINT count{};
    if (GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE)) != 0)
        return MouseRegistration::QueryFailed;
    if (count == 0) return MouseRegistration::Absent;
    std::array<RAWINPUTDEVICE, 64> devices{};
    if (count > devices.size()) return MouseRegistration::QueryFailed;
    if (GetRegisteredRawInputDevices(devices.data(), &count,
                                     sizeof(RAWINPUTDEVICE)) == UINT_MAX)
        return MouseRegistration::QueryFailed;
    for (const auto& device : devices) {
        if (device.usUsagePage == 0x01 && device.usUsage == 0x02) {
            return device.hwndTarget == receiver && device.dwFlags == RIDEV_INPUTSINK ?
                MouseRegistration::Ours : MouseRegistration::Other;
        }
    }
    return MouseRegistration::Absent;
}

} // namespace

struct GestureInputShield::Impl {
    struct SetupDiagnostics {
        GestureShieldSetupStage stage{GestureShieldSetupStage::None};
        GestureShieldNativeFailure native{GestureShieldNativeFailure::None};
        GestureShieldReadbackFailure readback{GestureShieldReadbackFailure::None};
        DWORD error{};
    };

    explicit Impl(GestureShieldCallbacks value) : callbacks(std::move(value)) {
        stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        command_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        ready_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        done_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        const auto suffix = std::to_wstring(GetCurrentProcessId()) + L"-" +
            std::to_wstring(reinterpret_cast<std::uintptr_t>(this));
        receiver_class_name = L"PaneBindGestureRawReceiver-" + suffix;
        overlay_class_name = L"PaneBindGestureInputShield-" + suffix;
    }

    ~Impl() {
        for (const HANDLE handle : {stop_event, command_event, ready_event, done_event}) {
            if (handle) CloseHandle(handle);
        }
    }

    enum class Phase { Idle, Pending, Active, Failed };
    struct Removal {
        std::uint64_t generation{};
        GestureShieldRemovalReason reason{};
    };

    GestureShieldCallbacks callbacks;
    HANDLE stop_event{};
    HANDLE command_event{};
    HANDLE ready_event{};
    HANDLE done_event{};
    std::wstring receiver_class_name;
    std::wstring overlay_class_name;
    std::mutex command_mutex;
    Phase phase{Phase::Idle};
    std::optional<GestureShieldRequest> pending_start;
    std::optional<GestureShieldRequest> current;
    std::optional<Removal> pending_remove;
    std::atomic<bool> started{false};
    std::atomic<bool> worker_launched{false};
    std::atomic<bool> ready{false};
    std::atomic<bool> stopping{false};
    std::atomic<bool> callback_failed{false};
    std::atomic<bool> context_lost{false};
    std::atomic<bool> overlay_destroyed{true};
    std::atomic<bool> receiver_destroyed{true};
    std::atomic<bool> raw_registration_removed{true};
    std::atomic<bool> hotkey_unregistered{true};
    std::atomic<bool> winevent_unhooked{true};
    std::atomic<bool> classes_unregistered{true};
    DWORD thread_id{};
    HWND receiver{};
    HWND overlay{};
    HWINEVENTHOOK foreground_hook{};
    HWINEVENTHOOK desktop_hook{};
    bool receiver_class_registered{};
    bool overlay_class_registered{};
    bool raw_registered{};
    bool hotkey_registered{};
    std::uint64_t raw_sequence{};
    POINT message_point{};
    DWORD message_time{};
    ULONGLONG deadline_tick{};

    void emit(GestureShieldEvent event) noexcept {
        if (!callbacks.on_event) return;
        try {
            callbacks.on_event(event);
        } catch (...) {
            callback_failed = true;
            stopping = true;
            SetEvent(stop_event);
        }
    }

    void failure(std::uint64_t generation, DWORD error,
                 const SetupDiagnostics* setup = nullptr) noexcept {
        GestureShieldEvent event{};
        event.kind = GestureShieldEventKind::ResourceFailure;
        event.generation = generation;
        event.overlay = overlay;
        event.win32_error = setup ? setup->error :
            (error ? error : ERROR_INVALID_STATE);
        if (setup) {
            event.setup_stage = setup->stage;
            event.native_failure = setup->native;
            event.readback_failure = setup->readback;
        }
        emit(event);
    }

    [[nodiscard]] bool active_identity() noexcept {
        return current && exact_source(*current) && interactive_default_desktop() &&
            GetForegroundWindow() == current->source;
    }

    void request_context_removal() noexcept {
        if (!current) return;
        context_lost = true;
        SetEvent(command_event);
    }

    static LRESULT CALLBACK receiver_procedure(HWND window, UINT message,
                                                WPARAM wparam, LPARAM lparam) noexcept {
        if (message == WM_NCCREATE) {
            const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(window, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
        }
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (!self || message != WM_INPUT) return DefWindowProcW(window, message, wparam, lparam);
        UINT size{};
        bool valid = GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT,
            nullptr, &size, sizeof(RAWINPUTHEADER)) == 0 &&
            size >= sizeof(RAWINPUT) && size <= 4096;
        alignas(RAWINPUT) std::array<std::byte, 4096> buffer{};
        if (valid) {
            const UINT expected = size;
            valid = GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT,
                buffer.data(), &size, sizeof(RAWINPUTHEADER)) == expected;
        }
        if (valid) {
            const auto* raw = reinterpret_cast<const RAWINPUT*>(buffer.data());
            if (raw->header.dwType == RIM_TYPEMOUSE) {
                GestureShieldEvent event{};
                event.kind = GestureShieldEventKind::RawMouse;
                event.generation = self->current ? self->current->generation : 0;
                event.raw_sequence = ++self->raw_sequence;
                event.message_point = self->message_point;
                event.message_time = self->message_time;
                event.observed_foreground = GetForegroundWindow();
                event.message_point_root = root_at(self->message_point);
                if (event.observed_foreground) {
                    event.foreground_positioning_available =
                        GetWindowRect(event.observed_foreground,
                                      &event.foreground_positioning) != FALSE;
                    if (!event.foreground_positioning_available)
                        event.foreground_positioning = RECT{};
                    event.foreground_visible_available = SUCCEEDED(DwmGetWindowAttribute(
                        event.observed_foreground, DWMWA_EXTENDED_FRAME_BOUNDS,
                        &event.foreground_visible, sizeof(event.foreground_visible)));
                    if (!event.foreground_visible_available)
                        event.foreground_visible = RECT{};
                    event.foreground_thread_id = GetWindowThreadProcessId(
                        event.observed_foreground, &event.foreground_process_id);
                    GUITHREADINFO gui{sizeof(gui)};
                    event.foreground_gui_available = event.foreground_thread_id &&
                        GetGUIThreadInfo(event.foreground_thread_id, &gui) != FALSE;
                    if (event.foreground_gui_available) {
                        event.foreground_capture = gui.hwndCapture;
                        event.foreground_move_size = gui.hwndMoveSize;
                        event.foreground_gui_flags = gui.flags;
                    }
                }
                event.cursor_available = GetCursorPos(&event.cursor_now) != FALSE;
                event.raw_button_flags = raw->data.mouse.usButtonFlags;
                event.raw_delta_x = raw->data.mouse.lLastX;
                event.raw_delta_y = raw->data.mouse.lLastY;
                self->emit(event);
                if (self->current && !self->active_identity())
                    self->request_context_removal();
            }
        } else {
            self->failure(self->current ? self->current->generation : 0, GetLastError());
            self->request_context_removal();
        }
        // Foreground Raw Input must reach DefWindowProc for system cleanup.
        if (GET_RAWINPUT_CODE_WPARAM(wparam) == RIM_INPUT)
            return DefWindowProcW(window, message, wparam, lparam);
        return 0;
    }

    static LRESULT CALLBACK overlay_procedure(HWND window, UINT message,
                                               WPARAM wparam, LPARAM lparam) noexcept {
        if (message == WM_NCCREATE) {
            const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(window, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
        }
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (!self) return DefWindowProcW(window, message, wparam, lparam);
        if (message == WM_NCHITTEST) return HTCLIENT;
        if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
        if (message == WM_LBUTTONUP) {
            GestureShieldEvent event{};
            event.kind = GestureShieldEventKind::LegacyLeftUp;
            event.generation = self->current ? self->current->generation : 0;
            event.overlay = window;
            event.message_point = self->message_point;
            event.message_time = self->message_time;
            event.cursor_available = GetCursorPos(&event.cursor_now) != FALSE;
            self->emit(event);
            return 0;
        }
        if (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN) return 0;
        if (message == WM_SETFOCUS ||
            (message == WM_ACTIVATE && LOWORD(wparam) != WA_INACTIVE)) {
            self->request_context_removal();
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

    static void CALLBACK win_event_procedure(HWINEVENTHOOK, DWORD event, HWND,
                                              LONG, LONG, DWORD, DWORD) noexcept {
        // Out-of-context WinEvents are delivered on their registering thread.
        if (auto* self = current_on_thread; self && self->current &&
            (event == EVENT_SYSTEM_DESKTOPSWITCH ||
             (event == EVENT_SYSTEM_FOREGROUND && !self->active_identity()))) {
            self->request_context_removal();
        }
    }

    [[nodiscard]] bool create_classes() noexcept {
        WNDCLASSW receiver_class{};
        receiver_class.hInstance = GetModuleHandleW(nullptr);
        receiver_class.lpfnWndProc = receiver_procedure;
        receiver_class.lpszClassName = receiver_class_name.c_str();
        receiver_class_registered = RegisterClassW(&receiver_class) != 0;
        if (!receiver_class_registered) return false;
        classes_unregistered = false;
        WNDCLASSW overlay_class{};
        overlay_class.hInstance = receiver_class.hInstance;
        overlay_class.lpfnWndProc = overlay_procedure;
        overlay_class.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        overlay_class.lpszClassName = overlay_class_name.c_str();
        overlay_class_registered = RegisterClassW(&overlay_class) != 0;
        return overlay_class_registered;
    }

    [[nodiscard]] bool register_receiver() noexcept {
        if (mouse_registration(nullptr) != MouseRegistration::Absent) return false;
        receiver = CreateWindowExW(0, receiver_class_name.c_str(), L"", 0,
            0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), this);
        receiver_destroyed = receiver == nullptr;
        if (!receiver) return false;
        RAWINPUTDEVICE device{0x01, 0x02, RIDEV_INPUTSINK, receiver};
        if (!RegisterRawInputDevices(&device, 1, sizeof(device))) return false;
        raw_registered = true;
        raw_registration_removed = false;
        return mouse_registration(receiver) == MouseRegistration::Ours;
    }

    [[nodiscard]] bool register_context_events() noexcept {
        foreground_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND,
            EVENT_SYSTEM_FOREGROUND, nullptr, win_event_procedure, 0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        if (foreground_hook) winevent_unhooked = false;
        desktop_hook = SetWinEventHook(EVENT_SYSTEM_DESKTOPSWITCH,
            EVENT_SYSTEM_DESKTOPSWITCH, nullptr, win_event_procedure, 0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        if (desktop_hook) winevent_unhooked = false;
        return foreground_hook && desktop_hook;
    }

    [[nodiscard]] GestureShieldReadbackFailure overlay_readback(
        const GestureShieldRequest& request, const RECT& requested,
        HWND foreground_before, SetupDiagnostics& diagnostic) noexcept {
        if (!overlay || !IsWindow(overlay))
            return GestureShieldReadbackFailure::OverlayIdentity;
        DWORD pid{};
        const bool identity = GetWindowThreadProcessId(overlay, &pid) == thread_id &&
            pid == GetCurrentProcessId();
        if (!identity) return GestureShieldReadbackFailure::OverlayIdentity;
        RECT actual{};
        SetLastError(0);
        if (!GetWindowRect(overlay, &actual)) {
            diagnostic.error = GetLastError();
            return GestureShieldReadbackFailure::Rectangle;
        }
        if (!EqualRect(&actual, &requested))
            return GestureShieldReadbackFailure::Rectangle;
        const auto style = GetWindowLongPtrW(overlay, GWL_EXSTYLE);
        const bool style_exact = (style & (WS_EX_LAYERED | WS_EX_NOACTIVATE |
            WS_EX_TOPMOST)) == (WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST) &&
            (style & WS_EX_TRANSPARENT) == 0;
        if (!style_exact) return GestureShieldReadbackFailure::Style;
        BYTE actual_alpha{};
        DWORD alpha_flags{};
        SetLastError(0);
        if (!GetLayeredWindowAttributes(overlay, nullptr, &actual_alpha, &alpha_flags)) {
            diagnostic.error = GetLastError();
            return GestureShieldReadbackFailure::Alpha;
        }
        if (actual_alpha != isolation_alpha || alpha_flags != LWA_ALPHA)
            return GestureShieldReadbackFailure::Alpha;
        if (!IsWindowVisible(overlay)) return GestureShieldReadbackFailure::Visibility;
        POINT cursor{};
        SetLastError(0);
        if (!GetCursorPos(&cursor)) {
            diagnostic.error = GetLastError();
            return GestureShieldReadbackFailure::CursorHit;
        }
        if (root_at(cursor) != overlay)
            return GestureShieldReadbackFailure::CursorHit;
        MonitorHitFacts hits{overlay};
        SetLastError(0);
        const bool enumerated = EnumDisplayMonitors(nullptr, nullptr,
            count_monitor_hits, reinterpret_cast<LPARAM>(&hits)) != FALSE;
        if (!enumerated) diagnostic.error = GetLastError();
        if (!enumerated || hits.overflow || hits.count == 0 || hits.count != hits.hits)
            return GestureShieldReadbackFailure::MonitorHit;
        if (GetForegroundWindow() != foreground_before ||
            foreground_before != request.source)
            return GestureShieldReadbackFailure::Foreground;
        diagnostic.native = native_plain_move_failure(request);
        if (diagnostic.native != GestureShieldNativeFailure::None)
            return GestureShieldReadbackFailure::NativeLive;
        return GestureShieldReadbackFailure::None;
    }

    [[nodiscard]] bool create_overlay(const GestureShieldRequest& request,
                                      SetupDiagnostics& diagnostic) noexcept {
        deadline_tick = GetTickCount64() + isolation_deadline_ms;
        RECT virtual_rect{};
        diagnostic.stage = GestureShieldSetupStage::VirtualScreen;
        if (!virtual_desktop_rect(virtual_rect)) return false;
        diagnostic.stage = GestureShieldSetupStage::PreLive;
        diagnostic.native = native_plain_move_failure(request);
        if (diagnostic.native != GestureShieldNativeFailure::None) return false;
        bool authorized = false;
        diagnostic.stage = GestureShieldSetupStage::Authorization;
        try { authorized = callbacks.gesture_authorized(request); }
        catch (...) { return false; }
        if (!authorized) return false;
        diagnostic.stage = GestureShieldSetupStage::PostLive;
        diagnostic.native = native_plain_move_failure(request);
        if (diagnostic.native != GestureShieldNativeFailure::None) return false;
        const HWND foreground_before = GetForegroundWindow();
        diagnostic.stage = GestureShieldSetupStage::CreateWindow;
        SetLastError(0);
        overlay = CreateWindowExW(WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            overlay_class_name.c_str(), L"PaneBind gesture input shield", WS_POPUP,
            virtual_rect.left, virtual_rect.top,
            virtual_rect.right - virtual_rect.left,
            virtual_rect.bottom - virtual_rect.top, nullptr, nullptr,
            GetModuleHandleW(nullptr), this);
        overlay_destroyed = overlay == nullptr;
        if (!overlay) { diagnostic.error = GetLastError(); return false; }
        diagnostic.stage = GestureShieldSetupStage::SetAlpha;
        SetLastError(0);
        if (!SetLayeredWindowAttributes(overlay, 0, isolation_alpha, LWA_ALPHA)) {
            diagnostic.error = GetLastError();
            return false;
        }
        diagnostic.stage = GestureShieldSetupStage::PlaceWindow;
        SetLastError(0);
        if (!SetWindowPos(overlay, HWND_TOPMOST,
            virtual_rect.left, virtual_rect.top,
            virtual_rect.right - virtual_rect.left,
            virtual_rect.bottom - virtual_rect.top,
            SWP_NOACTIVATE | SWP_SHOWWINDOW)) {
            diagnostic.error = GetLastError();
            return false;
        }
        diagnostic.stage = GestureShieldSetupStage::Readback;
        diagnostic.readback = overlay_readback(request, virtual_rect,
                                               foreground_before, diagnostic);
        if (diagnostic.readback != GestureShieldReadbackFailure::None) return false;
        diagnostic.stage = GestureShieldSetupStage::RegisterHotkey;
        SetLastError(0);
        hotkey_registered = RegisterHotKey(nullptr, stop_hotkey_id,
            stop_hotkey_modifiers, stop_hotkey_key) != FALSE;
        hotkey_unregistered = !hotkey_registered;
        if (!hotkey_registered) { diagnostic.error = GetLastError(); return false; }
        diagnostic.stage = GestureShieldSetupStage::Deadline;
        if (GetTickCount64() >= deadline_tick) return false;
        diagnostic.stage = GestureShieldSetupStage::None;
        return true;
    }

    [[nodiscard]] bool remove_overlay(GestureShieldRemovalReason reason) noexcept {
        const auto generation = current ? current->generation : 0;
        const HWND old_overlay = overlay;
        const bool unregistered = !hotkey_registered ||
            UnregisterHotKey(nullptr, stop_hotkey_id) != FALSE;
        hotkey_unregistered = unregistered;
        if (unregistered) hotkey_registered = false;
        const bool destroyed = !overlay ||
            (DestroyWindow(overlay) != FALSE && !IsWindow(overlay));
        overlay_destroyed = destroyed;
        if (destroyed) overlay = nullptr;
        deadline_tick = 0;
        GestureShieldEvent event{};
        event.kind = GestureShieldEventKind::IsolationGone;
        event.generation = generation;
        event.overlay = old_overlay;
        event.removal_reason = reason;
        event.overlay_destroyed = destroyed;
        event.hotkey_unregistered = unregistered;
        {
            std::lock_guard lock{command_mutex};
            phase = destroyed && unregistered ? Phase::Idle : Phase::Failed;
            if (phase == Phase::Idle) current.reset();
        }
        emit(event);
        if (!destroyed || !unregistered)
            failure(generation, GetLastError());
        return destroyed && unregistered;
    }

    void process_commands() noexcept {
        ResetEvent(command_event);
        if (context_lost.exchange(false) && current) {
            GestureShieldEvent event{};
            event.kind = GestureShieldEventKind::ContextLost;
            event.generation = current->generation;
            event.overlay = overlay;
            emit(event);
            (void)remove_overlay(GestureShieldRemovalReason::ContextLost);
        }
        std::optional<Removal> removal;
        std::optional<GestureShieldRequest> start;
        {
            std::lock_guard lock{command_mutex};
            removal = std::exchange(pending_remove, std::nullopt);
            if (removal && pending_start &&
                pending_start->generation == removal->generation) {
                current = *pending_start;
                pending_start.reset();
            }
            if (!removal) start = std::exchange(pending_start, std::nullopt);
        }
        if (removal) {
            if (current && current->generation == removal->generation)
                (void)remove_overlay(removal->reason);
            // A new gesture may have been queued by IsolationGone's callback.
            // Do not strand it behind the old generation's removal command.
            {
                std::lock_guard lock{command_mutex};
                if (pending_start) SetEvent(command_event);
            }
            return;
        }
        if (!start || stopping || WaitForSingleObject(stop_event, 0) == WAIT_OBJECT_0)
            return;
        {
            std::lock_guard lock{command_mutex};
            current = *start;
        }
        SetupDiagnostics setup{};
        const bool created = create_overlay(*start, setup);
        const bool expired = deadline_tick && GetTickCount64() >= deadline_tick;
        const bool lost = context_lost.exchange(false);
        bool revoked = expired || lost || stopping ||
            WaitForSingleObject(stop_event, 0) == WAIT_OBJECT_0;
        {
            std::lock_guard lock{command_mutex};
            revoked = revoked || (pending_remove &&
                pending_remove->generation == start->generation);
            if (created && !revoked) phase = Phase::Active;
        }
        if (created && !revoked) {
            GestureShieldEvent event{};
            event.kind = GestureShieldEventKind::IsolationReady;
            event.generation = start->generation;
            event.overlay = overlay;
            emit(event);
        } else {
            if (expired) {
                GestureShieldEvent event{};
                event.kind = GestureShieldEventKind::Deadline;
                event.generation = start->generation;
                event.overlay = overlay;
                emit(event);
            }
            if (lost) {
                GestureShieldEvent event{};
                event.kind = GestureShieldEventKind::ContextLost;
                event.generation = start->generation;
                event.overlay = overlay;
                emit(event);
            }
            if (!created) failure(start->generation, setup.error, &setup);
            (void)remove_overlay(expired ? GestureShieldRemovalReason::Deadline :
                (lost ? GestureShieldRemovalReason::ContextLost :
                (created ? GestureShieldRemovalReason::ExplicitStop :
                           GestureShieldRemovalReason::SetupFailure)));
            std::lock_guard lock{command_mutex};
            if (pending_remove && pending_remove->generation == start->generation)
                pending_remove.reset();
        }
    }

    void cleanup() noexcept {
        if (overlay || current)
            (void)remove_overlay(GestureShieldRemovalReason::Shutdown);
        bool hooks_removed = true;
        if (desktop_hook) {
            if (!UnhookWinEvent(desktop_hook)) {
                hooks_removed = false;
                failure(0, GetLastError());
            }
            desktop_hook = nullptr;
        }
        if (foreground_hook) {
            if (!UnhookWinEvent(foreground_hook)) {
                hooks_removed = false;
                failure(0, GetLastError());
            }
            foreground_hook = nullptr;
        }
        winevent_unhooked = hooks_removed;
        bool registration_removed = !raw_registered;
        if (raw_registered) {
            const auto registration = mouse_registration(receiver);
            if (registration == MouseRegistration::Ours) {
                RAWINPUTDEVICE device{0x01, 0x02, RIDEV_REMOVE, nullptr};
                registration_removed = RegisterRawInputDevices(&device, 1,
                    sizeof(device)) != FALSE &&
                    mouse_registration(receiver) == MouseRegistration::Absent;
            } else {
                // A different receiver now owns the process registration.
                // Do not remove another component's input observation.
                registration_removed = false;
            }
        }
        raw_registration_removed = registration_removed;
        if (!registration_removed) failure(0, GetLastError());
        const bool receiver_gone = !receiver ||
            (DestroyWindow(receiver) != FALSE && !IsWindow(receiver));
        receiver_destroyed = receiver_gone;
        if (!receiver_gone) failure(0, GetLastError());
        receiver = nullptr;
        bool classes_removed = true;
        if (overlay_class_registered &&
            !UnregisterClassW(overlay_class_name.c_str(), GetModuleHandleW(nullptr)))
            classes_removed = false;
        if (receiver_class_registered &&
            !UnregisterClassW(receiver_class_name.c_str(), GetModuleHandleW(nullptr)))
            classes_removed = false;
        classes_unregistered = classes_removed;
        if (!classes_removed) failure(0, GetLastError());
    }

    void run() noexcept {
        thread_id = GetCurrentThreadId();
        current_on_thread = this;
        MSG message{};
        PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
        ready = create_classes() && register_receiver() && register_context_events();
        if (!ready) failure(0, GetLastError());
        SetEvent(ready_event);
        if (ready) {
            const HANDLE signals[]{stop_event, command_event};
            for (;;) {
                if (WaitForSingleObject(stop_event, 0) == WAIT_OBJECT_0) break;
                const ULONGLONG now = GetTickCount64();
                const DWORD remaining = deadline_tick ?
                    static_cast<DWORD>(std::min<ULONGLONG>(isolation_deadline_ms,
                        deadline_tick > now ? deadline_tick - now : 0)) : INFINITE;
                const DWORD result = MsgWaitForMultipleObjectsEx(2, signals,
                    remaining, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                if (result == WAIT_OBJECT_0) break;
                if (result == WAIT_OBJECT_0 + 1) {
                    process_commands();
                    continue;
                }
                if (result == WAIT_TIMEOUT) {
                    GestureShieldEvent event{};
                    event.kind = GestureShieldEventKind::Deadline;
                    event.generation = current ? current->generation : 0;
                    event.overlay = overlay;
                    emit(event);
                    if (current) (void)remove_overlay(GestureShieldRemovalReason::Deadline);
                    continue;
                }
                if (result != WAIT_OBJECT_0 + 2) {
                    failure(current ? current->generation : 0, GetLastError());
                    break;
                }
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        stopping = true;
                        SetEvent(stop_event);
                        break;
                    }
                    if (message.message == WM_HOTKEY &&
                        message.wParam == stop_hotkey_id && hotkey_registered &&
                        HIWORD(message.lParam) == stop_hotkey_key &&
                        (LOWORD(message.lParam) & (MOD_CONTROL | MOD_SHIFT |
                                                      MOD_ALT | MOD_WIN)) ==
                            (MOD_CONTROL | MOD_SHIFT)) {
                        GestureShieldEvent event{};
                        event.kind = GestureShieldEventKind::HotkeyStop;
                        event.generation = current ? current->generation : 0;
                        event.overlay = overlay;
                        emit(event);
                        if (current)
                            (void)remove_overlay(GestureShieldRemovalReason::ExplicitStop);
                        continue;
                    }
                    message_point = message.pt;
                    message_time = message.time;
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                    if (WaitForSingleObject(stop_event, 0) == WAIT_OBJECT_0 ||
                        WaitForSingleObject(command_event, 0) == WAIT_OBJECT_0 ||
                        (deadline_tick && GetTickCount64() >= deadline_tick))
                        break;
                }
            }
        }
        cleanup();
        current_on_thread = nullptr;
        SetEvent(done_event);
    }

    static thread_local Impl* current_on_thread;
};

thread_local GestureInputShield::Impl* GestureInputShield::Impl::current_on_thread = nullptr;

GestureInputShield::GestureInputShield(GestureShieldCallbacks callbacks)
    : impl_(std::make_shared<Impl>(std::move(callbacks))) {}

GestureInputShield::~GestureInputShield() {
    (void)stop();
}

bool GestureInputShield::start() noexcept {
    const auto self = impl_;
    if (!self || self->started.exchange(true) || !self->stop_event ||
        !self->command_event || !self->ready_event || !self->done_event ||
        !self->callbacks.environment_allowed ||
        !self->callbacks.gesture_authorized || !self->callbacks.on_event)
        return false;
    bool allowed = false;
    try { allowed = self->callbacks.environment_allowed(); }
    catch (...) { return false; }
    if (!allowed) return false;
    try {
        worker_ = std::thread([self] { self->run(); });
        self->worker_launched = true;
    }
    catch (...) { return false; }
    if (WaitForSingleObject(self->ready_event, 2000) != WAIT_OBJECT_0 || !self->ready) {
        (void)stop();
        return false;
    }
    return true;
}

bool GestureInputShield::request_isolation(GestureShieldRequest request) noexcept {
    const auto self = impl_;
    if (!self || !self->ready || self->stopping || !exact_source(request)) return false;
    std::lock_guard lock{self->command_mutex};
    if (self->phase != Impl::Phase::Idle || self->pending_start || self->current)
        return false;
    self->phase = Impl::Phase::Pending;
    self->pending_start = request;
    if (!SetEvent(self->command_event)) {
        self->pending_start.reset();
        self->phase = Impl::Phase::Idle;
        return false;
    }
    return true;
}

bool GestureInputShield::request_remove(std::uint64_t generation,
                                         GestureShieldRemovalReason reason) noexcept {
    const auto self = impl_;
    if (!self || !self->ready || self->stopping || !generation) return false;
    std::lock_guard lock{self->command_mutex};
    const bool matches = (self->pending_start &&
        self->pending_start->generation == generation) ||
        (self->current && self->current->generation == generation);
    if (!matches || self->phase == Impl::Phase::Idle ||
        self->phase == Impl::Phase::Failed) return false;
    if (!self->pending_remove) self->pending_remove = Impl::Removal{generation, reason};
    return SetEvent(self->command_event) != FALSE;
}

GestureShieldStopFacts GestureInputShield::stop(DWORD timeout_ms) noexcept {
    GestureShieldStopFacts facts{};
    const auto self = impl_;
    if (!self) return facts;
    self->stopping = true;
    if (self->stop_event) SetEvent(self->stop_event);
    if (!self->worker_launched) {
        facts.worker_exited = true;
        facts.overlay_destroyed = self->overlay_destroyed;
        facts.receiver_destroyed = self->receiver_destroyed;
        facts.raw_registration_removed = self->raw_registration_removed;
        facts.hotkey_unregistered = self->hotkey_unregistered;
        facts.winevent_unhooked = self->winevent_unhooked;
        facts.classes_unregistered = self->classes_unregistered;
        facts.callback_delivery_ok = !self->callback_failed;
        return facts;
    }
    if (worker_.joinable() && std::this_thread::get_id() == worker_.get_id())
        return facts; // A callback may request stop, but may not join itself.
    const bool exited = self->done_event &&
        WaitForSingleObject(self->done_event, (std::min)(timeout_ms, 7000UL)) == WAIT_OBJECT_0;
    if (worker_.joinable()) {
        if (exited && std::this_thread::get_id() != worker_.get_id())
            worker_.join();
        else
            worker_.detach(); // The shared Impl remains alive; cleanup is NOT certified.
    }
    facts.worker_exited = exited;
    facts.overlay_destroyed = self->overlay_destroyed;
    facts.receiver_destroyed = self->receiver_destroyed;
    facts.raw_registration_removed = self->raw_registration_removed;
    facts.hotkey_unregistered = self->hotkey_unregistered;
    facts.winevent_unhooked = self->winevent_unhooked;
    facts.classes_unregistered = self->classes_unregistered;
    facts.callback_delivery_ok = !self->callback_failed;
    return facts;
}

} // namespace panebind::platform::windows::operations
