// Disposable-guest, test-owned integration of the PRODUCT input shield and
// shared Move path. Synthetic input belongs only to this guarded test driver.
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <wtsapi32.h>

#include "core/behavior/move_magnet_session.h"
#include "platform/windows/operations/gesture_input_shield.h"
#include "platform/windows/operations/live_move_writer.h"
#include "platform/windows/operations/move_frame_continuity.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#ifndef PANEBIND_BUILD_SHA
#define PANEBIND_BUILD_SHA "unknown"
#endif

namespace {
namespace b = panebind::core::behavior;
namespace g = panebind::core::geometry;
namespace m = panebind::core::magnet;
namespace op = panebind::platform::windows::operations;

constexpr wchar_t guest_exe[] =
    L"C:\\PaneBindMVP1\\Input\\panebind-owned-shield-validation.exe";
constexpr wchar_t guest_debug_exe[] =
    L"C:\\PaneBindMVP1\\Input\\Debug\\panebind-owned-shield-validation.exe";
constexpr wchar_t guest_marker[] = L"C:\\PaneBindMVP1\\Input\\run-id.txt";
constexpr wchar_t guest_output[] = L"C:\\PaneBindMVP1\\Output\\";
constexpr ULONG_PTR test_tag = 0x50424D56; // Owned SendInput tag; Raw has no tag.
constexpr DWORD shield_deadline_ms = 30000;
constexpr UINT msg_writer_notice = WM_APP + 17;

struct RawSample { POINT cursor{}; std::uint64_t packet{}, tick{}; };

struct State {
    std::string scenario;
    std::uint64_t generation{};
    std::uintptr_t nonce{};
    HANDLE evidence{INVALID_HANDLE_VALUE};
    std::mutex log_mutex;
    std::condition_variable log_cv;
    std::deque<std::string> log_queue;
    bool log_stop{};
    std::atomic<bool> log_ok{true};
    std::uint64_t log_sequence{};
    HANDLE ui_ready{}, ui_gone{}, legacy_down{}, native_start{}, native_end{}, modal_return{};
    HANDLE raw_down{}, raw_up{}, legacy_up{}, isolation_ready{}, isolation_gone{};
    HANDLE hotkey_stop{}, writer_receipt{}, writer_stall_started{}, writer_gone{}, raw_notice{};
    std::atomic<HWND> source{nullptr}, control{nullptr}, overlay{nullptr};
    std::atomic<DWORD> source_thread{0};
    std::atomic<bool> held{false}, retired{false}, armed{false};
    std::atomic<bool> tagged_down{false}, associated_raw_down{false};
    std::atomic<bool> authorized_start{false};
    std::atomic<bool> matching_raw_up{false}, normal_removal_requested{false};
    std::atomic<bool> overlay_destroyed{false}, hotkey_unregistered{false};
    std::atomic<bool> stall_started_held{false}, stall_deadline_removed{false};
    std::atomic<std::uint64_t> raw_sequence{0}, down_watermark{0};
    std::atomic<std::uint64_t> handoff_raw_watermark{0};
    std::atomic<std::uint32_t> native_downs{0}, native_starts{0}, native_ends{0};
    std::atomic<std::uint32_t> raw_downs{0}, raw_ups{0}, legacy_ups{0};
    std::atomic<std::uint32_t> cancel_attempts{0}, native_attempts{0};
    std::atomic<std::uint32_t> writer_failures{0}, control_mouse{0};
    std::atomic<bool> snapped_exact{false}, resource_failure{false};
    std::atomic<bool> trace_click_route{false};
    std::atomic<std::uint32_t> traced_hit_tests{0};
    std::mutex down_mutex, raw_mutex, cancel_mutex;
    bool cancel_claimed{}; // Orders observed Raw UP versus one bounded claim.
    std::optional<POINT> observed_down;
    std::optional<op::MoveFrameGeometry> down_frame;
    std::deque<RawSample> raw_moves;
    POINT start_cursor{}, control_point{}, original_cursor{}, down_point{};
    RECT virtual_rect{}, work_area{};
    HMONITOR monitor{};
    UINT dpi{};
    std::optional<op::MoveFrameGeometry> initial_source, initial_control;
    std::unique_ptr<op::GestureInputShield> shield;
    b::MoveMagnetSession motion;
    op::MoveFrameContinuity continuity;
    std::unique_ptr<op::LiveMoveWriter> writer;
} s;

[[nodiscard]] std::uint64_t qpc() noexcept {
    LARGE_INTEGER tick{};
    QueryPerformanceCounter(&tick);
    return static_cast<std::uint64_t>(tick.QuadPart);
}
[[nodiscard]] std::string boolean(bool value) { return value ? "true" : "false"; }
[[nodiscard]] std::uint64_t hwnd_number(HWND value) noexcept {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(value));
}
[[nodiscard]] std::string rect_json(const g::Rect& value) {
    return "[" + std::to_string(value.left()) + "," + std::to_string(value.top()) +
        "," + std::to_string(value.right()) + "," + std::to_string(value.bottom()) + "]";
}
void record(std::string_view kind, const std::string& fields = {}) noexcept {
    try {
        std::lock_guard lock{s.log_mutex};
        if (s.log_queue.size() >= 2048 || s.log_sequence >= 4096) {
            s.log_ok = false;
            return;
        }
        s.log_queue.push_back("{\"schema\":\"r1c4b-owned-product-shield/v1\",\"sequence\":" +
            std::to_string(++s.log_sequence) + ",\"qpc\":" + std::to_string(qpc()) +
            ",\"type\":\"" + std::string(kind) + "\"" + fields + "}\n");
        s.log_cv.notify_one();
    } catch (...) { s.log_ok = false; }
}
void log_owner() noexcept {
    for (;;) {
        std::deque<std::string> batch;
        {
            std::unique_lock lock{s.log_mutex};
            s.log_cv.wait(lock, [] { return s.log_stop || !s.log_queue.empty(); });
            batch.swap(s.log_queue);
            if (s.log_stop && batch.empty()) break;
        }
        for (const auto& line : batch) {
            DWORD written{};
            if (!WriteFile(s.evidence, line.data(), static_cast<DWORD>(line.size()),
                           &written, nullptr) || written != line.size()) s.log_ok = false;
        }
    }
}
[[nodiscard]] bool wait_for(HANDLE signal, DWORD timeout, std::string_view label) noexcept {
    const bool reached = signal && WaitForSingleObject(signal, timeout) == WAIT_OBJECT_0;
    record("wait", ",\"for\":\"" + std::string(label) +
        "\",\"reached\":" + boolean(reached));
    return reached;
}
[[nodiscard]] bool left_high() noexcept {
    return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
}
[[nodiscard]] HWND root_at(POINT point) noexcept {
    const HWND hit = WindowFromPoint(point);
    return hit ? GetAncestor(hit, GA_ROOT) : nullptr;
}
void record_start_route_snapshot(std::string_view phase) noexcept {
    POINT cursor{};
    const bool cursor_ok = GetCursorPos(&cursor) != FALSE;
    GUITHREADINFO gui{sizeof(gui)};
    SetLastError(0);
    const bool gui_ok = GetGUIThreadInfo(s.source_thread, &gui) != FALSE;
    const DWORD gui_error = gui_ok ? 0 : GetLastError();
    const HWND hit = cursor_ok ? WindowFromPoint(cursor) : nullptr;
    const HWND root = hit ? GetAncestor(hit, GA_ROOT) : nullptr;
    record("native_start_route_snapshot", ",\"phase\":\"" + std::string(phase) +
        "\",\"source_thread\":" + std::to_string(s.source_thread.load()) +
        ",\"gui_ok\":" + boolean(gui_ok) +
        ",\"gui_error\":" + std::to_string(gui_error) +
        ",\"gui_flags\":" + std::to_string(gui.flags) +
        ",\"capture\":" + std::to_string(hwnd_number(gui.hwndCapture)) +
        ",\"move_size\":" + std::to_string(hwnd_number(gui.hwndMoveSize)) +
        ",\"active\":" + std::to_string(hwnd_number(gui.hwndActive)) +
        ",\"foreground\":" + std::to_string(hwnd_number(GetForegroundWindow())) +
        ",\"cursor_ok\":" + boolean(cursor_ok) +
        ",\"cursor\":[" + std::to_string(cursor.x) + "," +
        std::to_string(cursor.y) + "],\"hit\":" + std::to_string(hwnd_number(hit)) +
        ",\"root\":" + std::to_string(hwnd_number(root)) +
        ",\"left_high\":" + boolean(left_high()));
}
[[nodiscard]] bool guest_desktop() noexcept {
    DWORD session{};
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session) || !session) return false;
    HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!input) return false;
    wchar_t a[256]{}, b[256]{};
    DWORD length{};
    const bool same = GetUserObjectInformationW(input, UOI_NAME, a, sizeof(a), &length) &&
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME,
                                  b, sizeof(b), &length) &&
        std::wstring_view(a) == std::wstring_view(b) && std::wstring_view(a) == L"Default";
    CloseDesktop(input);
    if (!same) return false;
    LPWSTR memory{};
    DWORD bytes{};
    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION,
                                     WTSSessionInfoEx, &memory, &bytes)) return false;
    bool active = false;
    if (memory && bytes >= sizeof(WTSINFOEXW)) {
        const auto& info = *reinterpret_cast<const WTSINFOEXW*>(memory);
        active = info.Level == 1 && info.Data.WTSInfoExLevel1.SessionState == WTSActive &&
            info.Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_UNLOCK;
    }
    WTSFreeMemory(memory);
    return active;
}
[[nodiscard]] bool guest_guard(std::wstring_view run_id,
                               std::wstring_view scenario,
                               std::wstring_view log_path) noexcept {
    if (run_id.size() != 32 || !std::all_of(run_id.begin(), run_id.end(), [](wchar_t c) {
            return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f');
        })) return false;
    wchar_t user[256]{};
    DWORD length = static_cast<DWORD>(std::size(user));
    if (!GetUserNameW(user, &length) || std::wstring_view(user) != L"WDAGUtilityAccount" ||
        !guest_desktop()) return false;
    wchar_t exe[MAX_PATH]{};
    const DWORD count = GetModuleFileNameW(nullptr, exe, static_cast<DWORD>(std::size(exe)));
    if (!count || count >= std::size(exe)) return false;
    const bool debug = CompareStringOrdinal(exe, -1, guest_debug_exe, -1, TRUE) == CSTR_EQUAL;
    if (!debug && CompareStringOrdinal(exe, -1, guest_exe, -1, TRUE) != CSTR_EQUAL)
        return false;
    const std::wstring expected = std::wstring(guest_output) + std::wstring(run_id) +
        (debug ? L"-debug-" : L"-release-") + std::wstring(scenario) + L".jsonl";
    if (CompareStringOrdinal(log_path.data(), -1, expected.c_str(), -1, TRUE) != CSTR_EQUAL)
        return false;
    const HANDLE file = CreateFileW(guest_marker, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    std::array<char, 32> marker{};
    DWORD read{};
    const bool valid = GetFileSizeEx(file, &size) &&
        size.QuadPart == static_cast<LONGLONG>(marker.size()) &&
        ReadFile(file, marker.data(), static_cast<DWORD>(marker.size()), &read, nullptr) &&
        read == marker.size() &&
        std::equal(marker.begin(), marker.end(), run_id.begin(), [](char x, wchar_t y) {
            return static_cast<unsigned char>(x) == y;
        });
    CloseHandle(file);
    return valid;
}

[[nodiscard]] bool same_owned(HWND hwnd) noexcept {
    DWORD pid{};
    return hwnd && IsWindow(hwnd) &&
        GetWindowThreadProcessId(hwnd, &pid) == s.source_thread &&
        pid == GetCurrentProcessId() &&
        static_cast<std::uintptr_t>(GetWindowLongPtrW(hwnd, GWLP_USERDATA)) == s.nonce;
}
[[nodiscard]] std::optional<op::MoveFrameGeometry> capture(HWND hwnd) noexcept {
    RECT positioning{}, visible{};
    if (!same_owned(hwnd) || !GetWindowRect(hwnd, &positioning) ||
        FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                     &visible, sizeof(visible)))) return std::nullopt;
    const g::Rect p{positioning.left, positioning.top,
                    positioning.right, positioning.bottom};
    const g::Rect v{visible.left, visible.top, visible.right, visible.bottom};
    if (p.empty() || v.empty()) return std::nullopt;
    return op::MoveFrameGeometry{p, v};
}
[[nodiscard]] bool same_frame(const op::MoveFrameGeometry& a,
                              const op::MoveFrameGeometry& b) noexcept {
    return a.positioning == b.positioning && a.visible == b.visible;
}
[[nodiscard]] bool stable_context() noexcept {
    const HWND source = s.source, control = s.control;
    if (!same_owned(source) || !same_owned(control) || !guest_desktop() ||
        GetForegroundWindow() != source || !s.initial_source || !s.initial_control ||
        !IsWindowVisible(source) || IsIconic(source) || IsZoomed(source) ||
        !IsWindowVisible(control) || IsIconic(control) || IsZoomed(control)) return false;
    GUITHREADINFO gui{sizeof(gui)};
    if (!GetGUIThreadInfo(s.source_thread, &gui) || gui.hwndCapture || gui.hwndMoveSize ||
        gui.hwndMenuOwner || (gui.flags & (GUI_INMENUMODE | GUI_SYSTEMMENUMODE |
                                     GUI_POPUPMENUMODE | GUI_INMOVESIZE))) return false;
    const auto a = capture(source), b = capture(control);
    MONITORINFO info{sizeof(info)};
    const HMONITOR monitor = MonitorFromWindow(source, MONITOR_DEFAULTTONULL);
    return a && b &&
        a->visible.size() == s.initial_source->visible.size() &&
        a->positioning.size() == s.initial_source->positioning.size() &&
        same_frame(*b, *s.initial_control) && monitor == s.monitor &&
        GetMonitorInfoW(monitor, &info) && EqualRect(&info.rcWork, &s.work_area) &&
        GetDpiForWindow(source) == s.dpi && GetDpiForWindow(control) == s.dpi;
}

void retire(std::string_view reason, b::MoveHandoffEscapeReason escape) noexcept {
    s.armed = false;
    s.retired = true;
    op::MoveRetireFacts facts{};
    if (s.writer) facts = s.writer->retire(s.generation);
    s.continuity.retire(s.generation);
    (void)s.motion.escape(s.generation, escape);
    record("retire", ",\"reason\":\"" + std::string(reason) +
        "\",\"pending_discarded\":" + boolean(facts.pending_discarded) +
        ",\"attempt_started_before_revoke\":" +
        boolean(facts.attempt_started_before_revoke));
}

[[nodiscard]] bool shield_hit(POINT cursor) noexcept {
    const HWND overlay = s.overlay;
    DWORD pid{};
    if (!overlay || !IsWindow(overlay) ||
        GetWindowThreadProcessId(overlay, &pid) == 0 ||
        pid != GetCurrentProcessId() || !IsWindowVisible(overlay) ||
        root_at(cursor) != overlay || root_at(s.control_point) != overlay)
        return false;
    const auto style = GetWindowLongPtrW(overlay, GWL_EXSTYLE);
    BYTE alpha{};
    DWORD flags{};
    return (style & (WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST)) ==
        (WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST) &&
        !(style & WS_EX_TRANSPARENT) &&
        GetLayeredWindowAttributes(overlay, nullptr, &alpha, &flags) &&
        alpha == 2 && flags == LWA_ALPHA;
}

void on_shield_event(const op::GestureShieldEvent& event) noexcept {
    const auto generation = s.generation;
    switch (event.kind) {
    case op::GestureShieldEventKind::RawMouse: {
        s.raw_sequence.store(event.raw_sequence, std::memory_order_release);
        if (event.raw_button_flags & RI_MOUSE_LEFT_BUTTON_DOWN) {
            ++s.raw_downs;
            if (s.held && event.raw_sequence > s.down_watermark &&
                event.observed_foreground == s.source &&
                event.message_point_root == s.source) {
                s.associated_raw_down = true;
                SetEvent(s.raw_down);
            }
            record("raw_down", ",\"packet\":" +
                std::to_string(event.raw_sequence) + ",\"foreground\":" +
                std::to_string(hwnd_number(event.observed_foreground)) +
                ",\"point_root\":" +
                std::to_string(hwnd_number(event.message_point_root)) +
                ",\"associated\":" + boolean(s.associated_raw_down));
        }
        if (event.raw_button_flags & RI_MOUSE_LEFT_BUTTON_UP) {
            ++s.raw_ups;
            const bool own_hit = event.message_point_root == s.source ||
                (s.overlay && event.message_point_root == s.overlay);
            const bool matched = s.held && s.tagged_down &&
                s.associated_raw_down && event.raw_sequence > s.down_watermark &&
                event.observed_foreground == s.source && own_hit &&
                same_owned(s.source);
            if (matched) {
                {
                    std::lock_guard lock{s.cancel_mutex};
                    s.matching_raw_up = true;
                }
                s.armed = false;
                s.retired = true;
                if (s.writer) (void)s.writer->retire(generation);
                s.continuity.retire(generation);
                (void)s.motion.raw_up_observed(generation, true);
                if (WaitForSingleObject(s.legacy_up, 0) == WAIT_OBJECT_0 && s.shield) {
                    (void)s.motion.legacy_up_delivery_observed(generation, true);
                    if (s.motion.removal_allowed(generation) &&
                        !s.normal_removal_requested.exchange(true))
                        (void)s.shield->request_remove(generation,
                            op::GestureShieldRemovalReason::NormalUp);
                }
            }
            record("raw_up", ",\"packet\":" + std::to_string(event.raw_sequence) +
                ",\"generation_at_receiver\":" + std::to_string(event.generation) +
                ",\"owned_down_matched\":" + boolean(matched) +
                ",\"own_hit\":" + boolean(own_hit));
            SetEvent(s.raw_up);
        } else if (event.cursor_available && event.generation == generation &&
                   s.armed && !s.retired && s.associated_raw_down &&
                   event.raw_sequence > s.down_watermark) {
            {
                std::lock_guard lock{s.raw_mutex};
                if (s.raw_moves.size() < 64)
                    s.raw_moves.push_back({event.cursor_now, event.raw_sequence, qpc()});
                else s.resource_failure = true;
                SetEvent(s.raw_notice);
            }
        }
        break;
    }
    case op::GestureShieldEventKind::LegacyLeftUp:
        ++s.legacy_ups;
        record("legacy_up", ",\"overlay\":" +
            std::to_string(hwnd_number(event.overlay)));
        SetEvent(s.legacy_up);
        if (s.matching_raw_up && s.shield) {
            (void)s.motion.legacy_up_delivery_observed(generation, true);
            if (s.motion.removal_allowed(generation) &&
                !s.normal_removal_requested.exchange(true))
                (void)s.shield->request_remove(generation,
                    op::GestureShieldRemovalReason::NormalUp);
        }
        break;
    case op::GestureShieldEventKind::IsolationReady:
        s.overlay = event.overlay;
        record("isolation_ready", ",\"overlay\":" +
            std::to_string(hwnd_number(event.overlay)) +
            ",\"generation\":" + std::to_string(event.generation));
        SetEvent(s.isolation_ready);
        break;
    case op::GestureShieldEventKind::IsolationGone:
        s.overlay = nullptr;
        s.overlay_destroyed = event.overlay_destroyed;
        s.hotkey_unregistered = event.hotkey_unregistered;
        if (event.removal_reason == op::GestureShieldRemovalReason::Deadline)
            s.stall_deadline_removed = true;
        if (event.removal_reason == op::GestureShieldRemovalReason::NormalUp) {
            (void)s.motion.isolation_removed(generation);
        } else {
            retire("shield_removed", b::MoveHandoffEscapeReason::ContextLost);
        }
        record("isolation_gone", ",\"generation\":" +
            std::to_string(event.generation) + ",\"reason\":" +
            std::to_string(static_cast<int>(event.removal_reason)) +
            ",\"overlay_destroyed\":" + boolean(event.overlay_destroyed) +
            ",\"hotkey_unregistered\":" + boolean(event.hotkey_unregistered));
        SetEvent(s.isolation_gone);
        break;
    case op::GestureShieldEventKind::HotkeyStop:
        retire("actual_hotkey", b::MoveHandoffEscapeReason::ExplicitStop);
        record("hotkey_stop", ",\"generation\":" +
            std::to_string(event.generation) + ",\"left_high\":" +
            boolean(left_high()));
        SetEvent(s.hotkey_stop);
        break;
    case op::GestureShieldEventKind::Deadline:
        retire("actual_deadline", b::MoveHandoffEscapeReason::Deadline);
        record("deadline", ",\"generation\":" +
            std::to_string(event.generation) + ",\"left_high\":" +
            boolean(left_high()));
        break;
    case op::GestureShieldEventKind::ContextLost:
    case op::GestureShieldEventKind::ResourceFailure:
        if (event.kind == op::GestureShieldEventKind::ResourceFailure)
            s.resource_failure = true;
        retire("resource_or_context", b::MoveHandoffEscapeReason::ContextLost);
        record("resource_or_context", ",\"kind\":" +
            std::to_string(static_cast<int>(event.kind)) +
            ",\"error\":" + std::to_string(event.win32_error));
        break;
    }
}

LRESULT CALLBACK owned_wndproc(HWND hwnd, UINT message, WPARAM wparam,
                               LPARAM lparam) noexcept {
    switch (message) {
    case WM_NCHITTEST:
        if (hwnd == s.source && s.trace_click_route) {
            const auto extra = GetMessageExtraInfo();
            const LRESULT result = DefWindowProcW(hwnd, message, wparam, lparam);
            if (s.traced_hit_tests.fetch_add(1) < 8)
                record("native_route_nchittest", ",\"point\":[" +
                    std::to_string(GET_X_LPARAM(lparam)) + "," +
                    std::to_string(GET_Y_LPARAM(lparam)) +
                    "],\"at_down_point\":" + boolean(
                        GET_X_LPARAM(lparam) == s.down_point.x &&
                        GET_Y_LPARAM(lparam) == s.down_point.y) +
                    ",\"result\":" + std::to_string(result) +
                    ",\"extra_info\":" +
                    std::to_string(static_cast<std::uintptr_t>(extra)));
            return result;
        }
        break;
    case WM_MOUSEACTIVATE:
        if (hwnd == s.source && s.trace_click_route) {
            const auto extra = GetMessageExtraInfo();
            const LRESULT result = DefWindowProcW(hwnd, message, wparam, lparam);
            record("native_route_mouseactivate", ",\"hit_code\":" +
                std::to_string(LOWORD(lparam)) + ",\"mouse_message\":" +
                std::to_string(HIWORD(lparam)) + ",\"result\":" +
                std::to_string(result) + ",\"extra_info\":" +
                std::to_string(static_cast<std::uintptr_t>(extra)));
            return result;
        }
        break;
    case WM_NCLBUTTONDOWN:
        if (hwnd == s.source)
            record("native_route_nclbuttondown", ",\"hit_code\":" +
                std::to_string(wparam) + ",\"extra_info\":" +
                std::to_string(static_cast<std::uintptr_t>(GetMessageExtraInfo())));
        if (hwnd == s.source && wparam == HTCAPTION) {
            ++s.native_downs;
            s.tagged_down = GetMessageExtraInfo() == test_tag;
            const POINT down{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            const auto frame = capture(hwnd);
            {
                std::lock_guard lock{s.down_mutex};
                s.observed_down = down;
                s.down_frame = frame;
            }
            record("owned_native_down", ",\"tagged\":" + boolean(s.tagged_down) +
                ",\"frame_captured\":" + boolean(frame.has_value()));
            SetEvent(s.legacy_down);
            const LRESULT result = DefWindowProcW(hwnd, message, wparam, lparam);
            SetEvent(s.modal_return);
            return result;
        }
        if (hwnd == s.source) SetEvent(s.legacy_down);
        break;
    case WM_ENTERSIZEMOVE:
        if (hwnd == s.source) {
            ++s.native_starts;
            record("owned_native_start", ",\"left_high\":" + boolean(left_high()));
            SetEvent(s.native_start);
        }
        break;
    case WM_EXITSIZEMOVE:
        if (hwnd == s.source) {
            ++s.native_ends;
            record("owned_native_end", ",\"left_high\":" + boolean(left_high()));
            SetEvent(s.native_end);
        }
        break;
    case WM_LBUTTONDOWN:
        if (hwnd == s.source) {
            record("native_route_lbuttondown", ",\"extra_info\":" +
                std::to_string(static_cast<std::uintptr_t>(GetMessageExtraInfo())));
            SetEvent(s.legacy_down);
        }
        [[fallthrough]];
    case WM_LBUTTONUP:
        if (hwnd == s.control) ++s.control_mouse;
        break;
    case WM_SYSCOMMAND:
        if (hwnd == s.source && (wparam & 0xFFF0) == SC_MOVE)
            record("native_route_syscommand_move", ",\"command\":" +
                std::to_string(wparam) + ",\"extra_info\":" +
                std::to_string(static_cast<std::uintptr_t>(GetMessageExtraInfo())));
        break;
    case WM_DESTROY:
        if (hwnd == s.source) PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

void ui_owner() noexcept {
    s.source_thread = GetCurrentThreadId();
    const wchar_t class_name[] = L"PaneBindOwnedProductShieldValidation";
    WNDCLASSW cls{};
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = class_name;
    cls.lpfnWndProc = owned_wndproc;
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&cls)) { SetEvent(s.ui_ready); SetEvent(s.ui_gone); return; }
    MONITORINFO monitor{sizeof(monitor)};
    const HMONITOR active = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    if (!GetMonitorInfoW(active, &monitor)) {
        UnregisterClassW(class_name, cls.hInstance);
        SetEvent(s.ui_ready); SetEvent(s.ui_gone); return;
    }
    const RECT area = monitor.rcWork;
    if (area.right - area.left < 880 || area.bottom - area.top < 420) {
        UnregisterClassW(class_name, cls.hInstance);
        SetEvent(s.ui_ready); SetEvent(s.ui_gone); return;
    }
    const int x = area.left + 55, y = area.top + 70;
    const HWND source = CreateWindowExW(0, class_name, L"PaneBind owned source",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, x, y, 310, 210,
        nullptr, nullptr, cls.hInstance, nullptr);
    const HWND control = CreateWindowExW(0, class_name, L"PaneBind owned control",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, x + 480, y, 310, 210,
        nullptr, nullptr, cls.hInstance, nullptr);
    if (source) SetWindowLongPtrW(source, GWLP_USERDATA, static_cast<LONG_PTR>(s.nonce));
    if (control) SetWindowLongPtrW(control, GWLP_USERDATA, static_cast<LONG_PTR>(s.nonce));
    s.source = source;
    s.control = control;
    if (source && control) {
        ShowWindow(control, SW_SHOWNOACTIVATE);
        ShowWindow(source, SW_SHOW);
        (void)SetForegroundWindow(source);
    }
    SetEvent(s.ui_ready);
    MSG message{};
    while (source && control && GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (source && IsWindow(source)) DestroyWindow(source);
    if (control && IsWindow(control)) DestroyWindow(control);
    s.source = nullptr;
    s.control = nullptr;
    UnregisterClassW(class_name, cls.hInstance);
    SetEvent(s.ui_gone);
}

struct WriteFacts {
    bool stable{};
    bool active{};
    bool hit{};
    op::MoveSampleDecision decision{op::MoveSampleDecision::Reject};
};

[[nodiscard]] WriteFacts write_facts(POINT cursor) noexcept {
    // Version before native capture: a completed own write can make this
    // sample stale, never evidence of an external translation.
    const auto version = s.continuity.snapshot_version(s.generation);
    const bool context = stable_context();
    const auto source_now = context ? capture(s.source) : std::nullopt;
    const auto observed = source_now ? s.continuity.observe(s.generation,
        *source_now, version) : op::MoveFrameObservation::NotArmed;
    const auto decision = op::classify_move_sample(
        observed, context && source_now.has_value());
    const bool stable = decision == op::MoveSampleDecision::Process;
    const bool active = stable && s.armed && !s.retired && !s.matching_raw_up &&
        left_high() && s.tagged_down && s.associated_raw_down &&
        s.native_starts == 1 && s.native_ends == 1 && s.cancel_attempts == 1 &&
        !s.motion.retired(s.generation);
    return {stable, active, shield_hit(cursor), decision};
}

[[nodiscard]] bool work_area_contains(const g::Rect& rect) noexcept {
    return rect.left() >= s.work_area.left && rect.top() >= s.work_area.top &&
        rect.right() <= s.work_area.right && rect.bottom() <= s.work_area.bottom;
}

[[nodiscard]] op::MoveWriteCallbacks owned_writer_callbacks() {
    return {
        [] { return capture(s.source); },
        [](const op::MoveFrameGeometry& before) {
            POINT cursor{};
            if (!GetCursorPos(&cursor)) return op::MovePreflightVerdict::ContextInvalid;
            const auto facts = write_facts(cursor);
            const auto now = capture(s.source);
            return op::classify_move_preflight({
                facts.stable && now && same_frame(*now, before),
                facts.active && facts.hit,
                s.matching_raw_up});
        },
        [](const op::MoveFrameGeometry& before, const g::Rect& target,
           const g::Rect& expected, std::uint64_t quantum) {
            if (!work_area_contains(target) || expected.left() < INT_MIN ||
                expected.left() > INT_MAX || expected.top() < INT_MIN ||
                expected.top() > INT_MAX) return op::MoveNativePlacement{};
            if (s.scenario == "writer-stall") {
                // Guest-only fault injection inside the actual shared writer
                // placement callback while LEFT is still physically held.
                s.stall_started_held = left_high();
                record("writer_stall_begin", ",\"left_high\":" +
                    boolean(s.stall_started_held));
                SetEvent(s.writer_stall_started);
                Sleep(shield_deadline_ms + 1000);
                record("writer_stall_end", ",\"isolation_gone\":" +
                    boolean(WaitForSingleObject(s.isolation_gone, 0) == WAIT_OBJECT_0));
            }
            POINT cursor{};
            if (!GetCursorPos(&cursor)) return op::MoveNativePlacement{};
            const auto facts = write_facts(cursor);
            const auto now = capture(s.source);
            if (!facts.stable || !facts.active || !facts.hit || !now ||
                !same_frame(*now, before) || !s.writer ||
                !s.continuity.begin_attempt(s.generation, quantum, before,
                                             {expected, target}))
                return op::MoveNativePlacement{};
            // No other potentially blocking validation after this claim.
            if (!s.writer->begin_native_attempt(s.generation)) {
                (void)s.continuity.abort_unissued(s.generation, quantum);
                return op::MoveNativePlacement{};
            }
            SetLastError(0);
            const bool success = SetWindowPos(s.source, nullptr,
                static_cast<int>(expected.left()), static_cast<int>(expected.top()),
                0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
            const DWORD error = success ? 0 : GetLastError();
            record("native_place", ",\"quantum\":" + std::to_string(quantum) +
                ",\"target\":" + rect_json(target) +
                ",\"attempted\":true,\"succeeded\":" + boolean(success) +
                ",\"error\":" + std::to_string(error));
            return op::MoveNativePlacement{true, success, true};
        },
        [](const op::MoveFrameGeometry&,
           const op::MoveFrameGeometry& after) {
            const auto version = s.continuity.snapshot_version(s.generation);
            const auto now = capture(s.source);
            const bool context = stable_context();
            const auto observed = now && context ? s.continuity.observe(
                s.generation, *now, version) : op::MoveFrameObservation::NotArmed;
            const bool continuous = observed == op::MoveFrameObservation::Expected ||
                observed == op::MoveFrameObservation::OwnInFlight;
            return op::move_post_context_valid({same_owned(s.source),
                same_owned(s.control), context && continuous,
                now && same_frame(*now, after)});
        }
    };
}

void writer_owner() noexcept {
    try {
        for (;;) {
            const auto receipt = s.writer->wait_and_execute();
            if (!receipt) break;
            const bool committed = receipt->native_attempted &&
                s.continuity.finish_attempt(s.generation, receipt->quantum,
                    receipt->native_attempted, receipt->exact, receipt->after);
            if (!receipt->native_attempted)
                (void)s.continuity.abort_unissued(s.generation, receipt->quantum);
            if (receipt->native_attempted) ++s.native_attempts;
            const bool exact = receipt->exact &&
                (!receipt->native_attempted || committed);
            const bool snapped = receipt->snapped && receipt->native_attempted &&
                exact && receipt->after &&
                receipt->after->visible == receipt->target_visible;
            if (snapped) s.snapped_exact = true;
            if (receipt->exact || receipt->native_attempted) {
                const b::MoveWritePlan plan{receipt->generation, receipt->quantum,
                    {}, receipt->target_visible, receipt->snapped,
                    m::MotionState::BelowThreshold, receipt->reason};
                (void)s.motion.write_result(plan, exact,
                    receipt->after ? receipt->after->visible : g::Rect{});
            }
            const bool failed = op::move_write_receipt_failed(*receipt, committed);
            if (failed) ++s.writer_failures;
            record("writer_receipt", ",\"quantum\":" +
                std::to_string(receipt->quantum) +
                ",\"native_attempted\":" + boolean(receipt->native_attempted) +
                ",\"native_succeeded\":" +
                (receipt->native_attempted ? boolean(receipt->native_succeeded) : "null") +
                ",\"exact\":" + boolean(receipt->exact) +
                ",\"snapped\":" + boolean(receipt->snapped) +
                ",\"continuity_committed\":" + boolean(committed) +
                ",\"failed\":" + boolean(failed) +
                ",\"reason\":\"" + std::string(receipt->reason) + "\"");
            SetEvent(s.writer_receipt);
            if (failed) retire("writer_receipt_failure", b::MoveHandoffEscapeReason::NativeFailure);
        }
    } catch (...) {
        ++s.writer_failures;
        retire("writer_exception", b::MoveHandoffEscapeReason::NativeFailure);
        SetEvent(s.writer_receipt);
    }
    SetEvent(s.writer_gone);
}

[[nodiscard]] bool process_raw_continuations() noexcept {
    std::deque<RawSample> batch;
    {
        std::lock_guard lock{s.raw_mutex};
        batch.swap(s.raw_moves);
    }
    bool offered = false;
    for (const auto& sample : batch) {
        if (!s.armed || s.retired || s.matching_raw_up) break;
        if (sample.packet <= s.handoff_raw_watermark) {
            record("pre_handoff_raw_discarded", ",\"packet\":" +
                std::to_string(sample.packet));
            continue;
        }
        const auto facts = write_facts(sample.cursor);
        if (facts.decision == op::MoveSampleDecision::DropStale) {
            record("raw_snapshot_stale", ",\"packet\":" +
                std::to_string(sample.packet));
            continue;
        }
        if (!facts.active || !facts.hit ||
            !s.motion.sample_cursor(s.generation,
                {sample.cursor.x, sample.cursor.y}, sample.tick,
                facts.active, facts.stable, facts.hit)) {
            if (!s.motion.retired(s.generation))
                retire("cursor_sample_invalid", b::MoveHandoffEscapeReason::ContextLost);
            continue;
        }
        const auto plan = s.motion.take_pending(s.generation,
            facts.active, facts.stable, facts.hit);
        if (!plan) continue;
        offered |= s.writer && s.writer->offer({plan->generation, plan->quantum,
                                                plan->target_visible, plan->snapped});
        record("cursor_quantum", ",\"packet\":" +
            std::to_string(sample.packet) + ",\"quantum\":" +
            std::to_string(plan->quantum) + ",\"target\":" +
            rect_json(plan->target_visible) + ",\"snapped\":" +
            boolean(plan->snapped));
    }
    return offered;
}

[[nodiscard]] bool virtual_desktop_rect(RECT& value) noexcept {
    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    const auto right = static_cast<std::int64_t>(x) + width;
    const auto bottom = static_cast<std::int64_t>(y) + height;
    if (width <= 1 || height <= 1 || right > INT_MAX || bottom > INT_MAX)
        return false;
    value = RECT{x, y, static_cast<LONG>(right), static_cast<LONG>(bottom)};
    return true;
}

[[nodiscard]] bool send_mouse(DWORD flags, POINT destination) noexcept {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    input.mi.dwExtraInfo = test_tag;
    if (flags & MOUSEEVENTF_ABSOLUTE) {
        const auto width = s.virtual_rect.right - s.virtual_rect.left;
        const auto height = s.virtual_rect.bottom - s.virtual_rect.top;
        if (width <= 1 || height <= 1) return false;
        input.mi.dx = static_cast<LONG>(
            (static_cast<std::int64_t>(destination.x - s.virtual_rect.left) * 65535) /
            (width - 1));
        input.mi.dy = static_cast<LONG>(
            (static_cast<std::int64_t>(destination.y - s.virtual_rect.top) * 65535) /
            (height - 1));
    }
    if (flags & MOUSEEVENTF_LEFTDOWN)
        s.down_watermark.store(s.raw_sequence.load(std::memory_order_acquire),
                               std::memory_order_release);
    const bool sent = SendInput(1, &input, sizeof(input)) == 1;
    record("test_sendinput", ",\"flags\":" + std::to_string(flags) +
        ",\"sent\":" + boolean(sent));
    return sent;
}

[[nodiscard]] bool move_cursor(POINT target) noexcept {
    if (!send_mouse(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE |
                    MOUSEEVENTF_VIRTUALDESK, target)) return false;
    Sleep(30); // One guest-driver dispatch allowance, never product polling.
    POINT actual{};
    const bool exact = GetCursorPos(&actual) &&
        actual.x == target.x && actual.y == target.y;
    record("test_cursor_readback", ",\"requested\":[" +
        std::to_string(target.x) + "," + std::to_string(target.y) +
        "],\"actual\":[" + std::to_string(actual.x) + "," +
        std::to_string(actual.y) + "],\"exact\":" + boolean(exact));
    return exact;
}

[[nodiscard]] bool send_stop_chord() noexcept {
    constexpr std::array<WORD, 6> keys{
        VK_CONTROL, VK_SHIFT, VK_F11, VK_F11, VK_SHIFT, VK_CONTROL};
    std::array<INPUT, keys.size()> inputs{};
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        inputs[i].type = INPUT_KEYBOARD;
        inputs[i].ki.wVk = keys[i];
        inputs[i].ki.dwFlags = i >= 3 ? KEYEVENTF_KEYUP : 0;
        inputs[i].ki.dwExtraInfo = test_tag;
    }
    const UINT sent = SendInput(static_cast<UINT>(inputs.size()),
                                inputs.data(), sizeof(INPUT));
    // Even a partially delivered batch cannot leave the stop chord held.
    constexpr std::array<WORD, 3> release{VK_F11, VK_SHIFT, VK_CONTROL};
    std::array<INPUT, release.size()> cleanup{};
    for (std::size_t i = 0; i < cleanup.size(); ++i) {
        cleanup[i].type = INPUT_KEYBOARD;
        cleanup[i].ki.wVk = release[i];
        cleanup[i].ki.dwFlags = KEYEVENTF_KEYUP;
        cleanup[i].ki.dwExtraInfo = test_tag;
    }
    const UINT released = SendInput(static_cast<UINT>(cleanup.size()),
                                    cleanup.data(), sizeof(INPUT));
    record("test_stop_chord", ",\"sent\":" + std::to_string(sent) +
        ",\"released\":" + std::to_string(released));
    return sent == inputs.size() && released == cleanup.size();
}

[[nodiscard]] bool release_owned_left(std::string_view reason) noexcept {
    if (!s.held && !left_high()) return true;
    const HWND source = s.source;
    POINT cursor{};
    GUITHREADINFO gui{sizeof(gui)};
    const bool context = same_owned(source) && guest_desktop() &&
        GetCursorPos(&cursor) && GetGUIThreadInfo(s.source_thread, &gui);
    const HWND hit = context ? root_at(cursor) : nullptr;
    const bool own_route = context && GetForegroundWindow() == source &&
        (hit == source || shield_hit(cursor)) &&
        (!gui.hwndCapture || gui.hwndCapture == source) &&
        (!gui.hwndMoveSize || gui.hwndMoveSize == source) &&
        !gui.hwndMenuOwner && !(gui.flags & (GUI_INMENUMODE |
            GUI_SYSTEMMENUMODE | GUI_POPUPMENUMODE)) && left_high();
    record("cleanup_up_route", ",\"reason\":\"" + std::string(reason) +
        "\",\"hit\":" + std::to_string(hwnd_number(hit)) +
        ",\"foreground\":" +
        std::to_string(hwnd_number(GetForegroundWindow())) +
        ",\"exact_owned_route\":" + boolean(own_route));
    if (!own_route) return false; // Guest failure, never inject into a foreign root.
    if (!send_mouse(MOUSEEVENTF_LEFTUP, cursor)) return false;
    const bool observed = wait_for(s.raw_up, 1500, "actual_raw_cleanup_up");
    Sleep(30); // Test-only async key readback, not an input event source.
    const bool released = !left_high();
    if (released) s.held = false; // AFTER the Raw callback associated this UP.
    record("cleanup_up_result", ",\"raw_observed\":" + boolean(observed) +
        ",\"left_released\":" + boolean(released));
    return observed && s.matching_raw_up && released;
}

[[nodiscard]] bool find_caption_point(HWND source, POINT& result) noexcept {
    RECT rect{};
    if (!GetWindowRect(source, &rect)) return false;
    const int x = rect.left + (rect.right - rect.left) / 3;
    for (int delta : {10, 15, 20, 25, 30, 35, 40}) {
        const POINT point{x, rect.top + delta};
        if (root_at(point) != source) continue;
        const LRESULT hit = SendMessageW(source, WM_NCHITTEST, 0,
            MAKELPARAM(static_cast<SHORT>(point.x), static_cast<SHORT>(point.y)));
        if (hit == HTCAPTION) { result = point; return true; }
    }
    return false;
}

[[nodiscard]] bool prepare_owned() noexcept {
    if (!wait_for(s.ui_ready, 2000, "owned_ui_ready") ||
        !same_owned(s.source) || !same_owned(s.control) ||
        !virtual_desktop_rect(s.virtual_rect) || left_high() ||
        GetForegroundWindow() != s.source ||
        !GetCursorPos(&s.original_cursor)) return false;
    s.initial_source = capture(s.source);
    s.initial_control = capture(s.control);
    s.monitor = MonitorFromWindow(s.source, MONITOR_DEFAULTTONULL);
    MONITORINFO info{sizeof(info)};
    s.dpi = GetDpiForWindow(s.source);
    if (!s.initial_source || !s.initial_control || !s.monitor || !s.dpi ||
        !GetMonitorInfoW(s.monitor, &info) ||
        MonitorFromWindow(s.control, MONITOR_DEFAULTTONULL) != s.monitor ||
        GetDpiForWindow(s.control) != s.dpi ||
        !find_caption_point(s.source, s.down_point)) return false;
    s.work_area = info.rcWork;
    const auto& target = s.initial_control->visible;
    s.control_point = POINT{
        static_cast<LONG>(target.left() + target.width() / 2),
        static_cast<LONG>(target.top() + target.height() / 2)};
    record("owned_baseline", ",\"source_visible\":" +
        rect_json(s.initial_source->visible) + ",\"control_visible\":" +
        rect_json(s.initial_control->visible) + ",\"dpi\":" +
        std::to_string(s.dpi));
    return move_cursor(s.down_point) && root_at(s.down_point) == s.source;
}

[[nodiscard]] bool begin_native_owned_move() noexcept {
    if (!prepare_owned()) return false;
    record_start_route_snapshot("before_down");
    s.traced_hit_tests = 0;
    s.trace_click_route = true;
    s.held = true;
    if (!send_mouse(MOUSEEVENTF_LEFTDOWN, s.down_point)) {
        s.trace_click_route = false;
        return false;
    }
    if (!wait_for(s.legacy_down, 1500, "actual_legacy_down") ||
        s.native_downs != 1 || !s.tagged_down) {
        s.trace_click_route = false;
        record_start_route_snapshot("down_route_invalid");
        return false;
    }
    s.start_cursor = POINT{s.down_point.x + 22, s.down_point.y + 18};
    const bool moved = move_cursor(s.start_cursor);
    const bool started = moved && wait_for(s.native_start, 2000, "actual_native_start");
    s.trace_click_route = false;
    if (!started) {
        if (moved) record_start_route_snapshot("start_timeout");
        return false;
    }
    if (
        !wait_for(s.raw_down, 1500, "actual_raw_down") ||
        !left_high() || s.native_downs != 1 || s.native_starts != 1 ||
        !s.tagged_down || !s.associated_raw_down ||
        (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_MENU) & 0x8000) != 0) return false;
    GUITHREADINFO gui{sizeof(gui)};
    std::optional<POINT> down;
    std::optional<op::MoveFrameGeometry> frame;
    {
        std::lock_guard lock{s.down_mutex};
        down = s.observed_down;
        frame = s.down_frame;
    }
    const bool exact = down && frame && down->x == s.down_point.x &&
        down->y == s.down_point.y &&
        same_frame(*frame, *s.initial_source) &&
        GetGUIThreadInfo(s.source_thread, &gui) &&
        gui.hwndCapture == s.source && gui.hwndMoveSize == s.source &&
        GetForegroundWindow() == s.source;
    if (!exact) return false;
    s.authorized_start = true;
    LARGE_INTEGER frequency{};
    const std::array targets{m::MagnetTarget{
        panebind::core::model::WindowId{"owned-control"},
        s.initial_control->visible, false}};
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 ||
        !s.motion.begin(s.generation, {down->x, down->y}, frame->visible,
            targets, qpc(), static_cast<std::uint64_t>(frequency.QuadPart),
            same_owned(s.source), true, true)) return false;
    record("owned_move_begin", ",\"generation\":" +
        std::to_string(s.generation) + ",\"original_down\":[" +
        std::to_string(down->x) + "," + std::to_string(down->y) +
        "],\"actual_start\":true,\"owned_only_attribution\":true");
    return true;
}

[[nodiscard]] bool request_real_isolation() noexcept {
    const HWND source = s.source;
    const bool queued = s.shield && s.shield->request_isolation({
        s.generation, source, GetCurrentProcessId(), s.source_thread});
    record("isolation_request", ",\"queued\":" + boolean(queued) +
        ",\"generation\":" + std::to_string(s.generation));
    return queued;
}

[[nodiscard]] bool actual_isolation_ready() noexcept {
    if (!wait_for(s.isolation_ready, 2000, "product_shield_isolation_ready"))
        return false;
    POINT cursor{};
    GUITHREADINFO gui{sizeof(gui)};
    const bool actual = GetCursorPos(&cursor) && shield_hit(cursor) &&
        guest_desktop() && GetForegroundWindow() == s.source && left_high() &&
        GetGUIThreadInfo(s.source_thread, &gui) &&
        gui.hwndCapture == s.source && gui.hwndMoveSize == s.source;
    record("isolation_post_readback", ",\"actual_hit_and_context\":" +
        boolean(actual));
    return actual && s.motion.isolation_ready(s.generation, true);
}

[[nodiscard]] bool bounded_native_cancel_and_handoff() noexcept {
    // Request queueing and API return are not native END evidence. The owned
    // WndProc must observe WM_EXITSIZEMOVE and its modal call must return.
    POINT cursor{};
    GUITHREADINFO gui{sizeof(gui)};
    const bool preflight = !s.matching_raw_up && s.armed == false &&
        s.native_starts == 1 && s.native_ends == 0 &&
        s.cancel_attempts == 0 && s.associated_raw_down && s.tagged_down &&
        same_owned(s.source) && left_high() && guest_desktop() &&
        GetForegroundWindow() == s.source && GetCursorPos(&cursor) &&
        shield_hit(cursor) && GetGUIThreadInfo(s.source_thread, &gui) &&
        gui.hwndCapture == s.source && gui.hwndMoveSize == s.source &&
        s.motion.may_cancel(s.generation);
    if (!preflight) return false;
    {
        // Only the claim is serialized with actual Raw UP observation. The
        // resource callback never waits for the bounded native API itself.
        std::lock_guard lock{s.cancel_mutex};
        if (s.matching_raw_up || s.retired ||
            !s.motion.cancel_issued(s.generation)) return false;
        s.cancel_claimed = true;
    }
    record("cancel_permission_claim", ",\"claimed\":true,\"api_entered\":false");
    DWORD_PTR ignored{};
    ++s.cancel_attempts;
    SetLastError(0);
    const bool returned = SendMessageTimeoutW(s.source, WM_CANCELMODE, 0, 0,
        SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 1000, &ignored) != 0;
    const DWORD error = returned ? 0 : GetLastError();
    record("cancel_api_result", ",\"api_entered\":true,\"returned\":" + boolean(returned) +
        ",\"error\":" + std::to_string(error));
    if (!returned || !wait_for(s.native_end, 1500, "actual_native_end") ||
        !wait_for(s.modal_return, 1500, "native_modal_return") ||
        !same_owned(s.source) || s.native_ends != 1 ||
        s.matching_raw_up || !left_high() ||
        !s.motion.native_end_observed(s.generation, true)) return false;
    const auto handoff = capture(s.source);
    const bool fresh = handoff && stable_context() &&
        s.continuity.arm(s.generation, *handoff);
    record("handoff", ",\"actual_end\":true,\"fresh\":" +
        boolean(fresh) + ",\"handoff_visible\":" +
        (handoff ? rect_json(handoff->visible) : "null"));
    if (!fresh) return false;
    {
        std::lock_guard lock{s.raw_mutex};
        s.raw_moves.clear(); // END-before-continuation watermark.
        ResetEvent(s.raw_notice);
        s.handoff_raw_watermark.store(s.raw_sequence.load(std::memory_order_acquire),
                                      std::memory_order_release);
    }
    record("handoff_raw_watermark", ",\"last_pre_handoff_packet\":" +
        std::to_string(s.handoff_raw_watermark.load()));
    s.armed = true;
    return true;
}

[[nodiscard]] bool run_normal_or_stall() noexcept {
    if (!bounded_native_cancel_and_handoff()) return false;
    if (s.scenario == "writer-stall") {
        const POINT continuation{s.start_cursor.x + 8, s.start_cursor.y + 7};
        if (!move_cursor(continuation) ||
            !wait_for(s.raw_notice, 2000, "raw_continuation_for_stall") ||
            !process_raw_continuations() ||
            !wait_for(s.writer_stall_started, 2000, "actual_writer_callback_stall") ||
            !s.stall_started_held || !left_high()) return false;
        const bool gone = wait_for(s.isolation_gone, shield_deadline_ms + 2000,
                                   "product_shield_independent_deadline");
        const bool held_at_removal = left_high();
        const bool released = gone && held_at_removal &&
            release_owned_left("writer_stall_after_deadline");
        const bool receipt = wait_for(s.writer_receipt, 4000,
                                      "writer_after_deadline_receipt");
        record("writer_stall_verdict", ",\"deadline_removal\":" +
            boolean(s.stall_deadline_removed) +
            ",\"overlay_destroyed\":" + boolean(s.overlay_destroyed) +
            ",\"held_at_callback\":" + boolean(s.stall_started_held) +
            ",\"held_at_removal\":" + boolean(held_at_removal) +
            ",\"cleanup_raw_up\":" + boolean(released) +
            ",\"native_attempts\":" + std::to_string(s.native_attempts.load()));
        return gone && released && receipt && s.stall_deadline_removed &&
            s.overlay_destroyed && s.hotkey_unregistered &&
            s.native_attempts == 0 && s.retired;
    }
    // Only this test driver uses a fixed trajectory. Product session uses the
    // observed original DOWN anchor and arbitrary event-driven cursor quanta.
    const auto& from = s.initial_source->visible;
    const auto& to = s.initial_control->visible;
    const auto dx = to.left() - from.right() - 5;
    const auto dy = to.top() - from.top() + 4;
    const auto x = static_cast<std::int64_t>(s.down_point.x) + dx;
    const auto y = static_cast<std::int64_t>(s.down_point.y) + dy;
    if (x < INT_MIN || x > INT_MAX || y < INT_MIN || y > INT_MAX) return false;
    const POINT snap{static_cast<LONG>(x), static_cast<LONG>(y)};
    Sleep(150); // Test input separation, not a resident cursor sampler.
    if (!move_cursor(snap) ||
        !wait_for(s.raw_notice, 2000, "raw_snap_continuation") ||
        !process_raw_continuations() ||
        !wait_for(s.writer_receipt, 2000, "shared_writer_receipt")) return false;
    if (!s.snapped_exact) {
        Sleep(150);
        const POINT reacquire{snap.x + 1, snap.y};
        if (!move_cursor(reacquire) ||
            !wait_for(s.raw_notice, 2000, "raw_reacquire_continuation") ||
            !process_raw_continuations() ||
            !wait_for(s.writer_receipt, 2000, "shared_writer_reacquire_receipt"))
            return false;
    }
    if (!s.snapped_exact || s.native_attempts == 0 || s.writer_failures != 0 ||
        !shield_hit(snap) || !release_owned_left("normal_raw_up")) return false;
    const bool legacy = wait_for(s.legacy_up, 1500, "actual_overlay_legacy_up");
    const bool gone = wait_for(s.isolation_gone, 2000, "normal_shield_removal");
    const bool no_control_delivery = s.control_mouse == 0;
    record("normal_verdict", ",\"snapped_exact\":" + boolean(s.snapped_exact) +
        ",\"native_attempts\":" + std::to_string(s.native_attempts.load()) +
        ",\"raw_downs\":" + std::to_string(s.raw_downs.load()) +
        ",\"raw_ups\":" + std::to_string(s.raw_ups.load()) +
        ",\"legacy_up\":" + boolean(legacy) +
        ",\"normal_removal\":" + boolean(s.normal_removal_requested) +
        ",\"control_mouse_zero\":" + boolean(no_control_delivery));
    return legacy && gone && no_control_delivery && s.normal_removal_requested &&
        s.overlay_destroyed && s.hotkey_unregistered && s.retired;
}

[[nodiscard]] bool drive() noexcept {
    if (!begin_native_owned_move()) return false;
    bool conflict_registered = false;
    if (s.scenario == "setup-fail") {
        MSG ignored{};
        PeekMessageW(&ignored, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
        conflict_registered = RegisterHotKey(nullptr, 0x5044,
            MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, VK_F11) != FALSE;
        record("setup_fail_hotkey_collision", ",\"registered\":" +
            boolean(conflict_registered));
        if (!conflict_registered) return false;
    }
    const bool requested = request_real_isolation();
    if (!requested) {
        if (conflict_registered) UnregisterHotKey(nullptr, 0x5044);
        return false;
    }
    if (s.scenario == "setup-fail") {
        const bool gone = wait_for(s.isolation_gone, 2000,
                                   "actual_setup_failure_shield_removal");
        const bool unregistered = UnregisterHotKey(nullptr, 0x5044) != FALSE;
        const bool released = gone && release_owned_left("setup_failure_cleanup");
        const bool native_end = released &&
            wait_for(s.native_end, 1500, "native_end_after_setup_failure");
        record("setup_failure_verdict", ",\"resource_failure\":" +
            boolean(s.resource_failure) + ",\"overlay_destroyed\":" +
            boolean(s.overlay_destroyed) + ",\"collision_unregistered\":" +
            boolean(unregistered) + ",\"cancel_attempts\":" +
            std::to_string(s.cancel_attempts.load()));
        return gone && released && native_end && unregistered &&
            s.resource_failure && s.overlay_destroyed &&
            s.cancel_attempts == 0 && s.native_attempts == 0;
    }
    if (!actual_isolation_ready()) return false;
    if (s.scenario == "early-up") {
        if (!release_owned_left("early_up_before_cancel")) return false;
        retire("early_up", b::MoveHandoffEscapeReason::EarlyUp);
        // If actual Raw and legacy UP already completed normal removal, a
        // second request correctly returns false. The observed removal event,
        // not request_remove's queue result, is the teardown witness.
        const bool already_gone =
            WaitForSingleObject(s.isolation_gone, 0) == WAIT_OBJECT_0;
        const bool requested_remove = already_gone ? false :
            s.shield->request_remove(s.generation,
                op::GestureShieldRemovalReason::ExplicitStop);
        const bool gone = wait_for(s.isolation_gone, 2000,
                                   "early_up_shield_removal");
        const bool end = wait_for(s.native_end, 1500, "early_up_native_end");
        record("early_up_verdict", ",\"actual_raw_up\":" +
            boolean(s.matching_raw_up) + ",\"cancel_attempts\":" +
            std::to_string(s.cancel_attempts.load()) +
            ",\"already_gone\":" + boolean(already_gone) +
            ",\"removal_requested\":" + boolean(requested_remove));
        return gone && s.overlay_destroyed && s.hotkey_unregistered &&
            end && s.matching_raw_up &&
            s.cancel_attempts == 0 && s.native_attempts == 0;
    }
    if (s.scenario == "stop") {
        if (!left_high() || s.native_ends != 0 || !send_stop_chord() ||
            !wait_for(s.hotkey_stop, 1500, "real_product_hotkey_stop") ||
            !wait_for(s.isolation_gone, 2000, "hotkey_shield_removal")) return false;
        const bool held_at_removal = left_high();
        const bool released = held_at_removal &&
            release_owned_left("stop_cleanup_after_overlay");
        const bool end = released && wait_for(s.native_end, 1500,
                                              "native_end_after_hotkey_stop");
        record("stop_verdict", ",\"held_at_removal\":" +
            boolean(held_at_removal) + ",\"actual_cleanup_raw_up\":" +
            boolean(s.matching_raw_up) + ",\"hotkey_unregistered\":" +
            boolean(s.hotkey_unregistered));
        return released && end && held_at_removal && s.overlay_destroyed &&
            s.hotkey_unregistered && s.cancel_attempts == 0 && s.native_attempts == 0;
    }
    return run_normal_or_stall();
}

[[nodiscard]] bool create_signals() noexcept {
    auto make = [](bool manual = true) { return CreateEventW(nullptr, manual, FALSE, nullptr); };
    s.ui_ready = make();
    s.ui_gone = make();
    s.legacy_down = make();
    s.native_start = make();
    s.native_end = make();
    s.modal_return = make();
    s.raw_down = make();
    s.raw_up = make();
    s.legacy_up = make();
    s.isolation_ready = make();
    s.isolation_gone = make();
    s.hotkey_stop = make();
    s.writer_receipt = make(false);
    s.writer_stall_started = make();
    s.writer_gone = make();
    s.raw_notice = make(false);
    return s.ui_ready && s.ui_gone && s.legacy_down && s.native_start && s.native_end &&
        s.modal_return && s.raw_down && s.raw_up && s.legacy_up &&
        s.isolation_ready && s.isolation_gone && s.hotkey_stop &&
        s.writer_receipt && s.writer_stall_started && s.writer_gone &&
        s.raw_notice;
}

void close_signals() noexcept {
    for (HANDLE handle : {s.ui_ready, s.ui_gone, s.legacy_down, s.native_start, s.native_end,
            s.modal_return, s.raw_down, s.raw_up, s.legacy_up,
            s.isolation_ready, s.isolation_gone, s.hotkey_stop,
            s.writer_receipt, s.writer_stall_started, s.writer_gone, s.raw_notice}) {
        if (handle) CloseHandle(handle);
    }
}

} // namespace

[[nodiscard]] static std::string json_bool(bool value) {
    return value ? "true" : "false";
}

int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring_view(argv[1]) == L"--build-identity") {
        // Read-only on any host; no evidence, GUI, Raw registration or input.
        std::cout << "{\"implementation_sha\":\"" << PANEBIND_BUILD_SHA << "\"}\n";
        return 0;
    }
    if (argc != 8 || std::wstring_view(argv[1]) != L"--run-owned-shield-validation" ||
        std::wstring_view(argv[2]) != L"--scenario" ||
        std::wstring_view(argv[4]) != L"--evidence-log" ||
        std::wstring_view(argv[6]) != L"--sandbox-run-id") return 64;
    const std::wstring_view scenario(argv[3]);
    if (scenario != L"normal" && scenario != L"early-up" &&
        scenario != L"setup-fail" && scenario != L"stop" &&
        scenario != L"writer-stall") return 64;
    if (!guest_guard(argv[7], scenario, argv[5])) {
        constexpr char denied[] =
            "NOT_READY: exact interactive disposable Windows Sandbox package required; no GUI or input started.\n";
        DWORD written{};
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), denied,
                  static_cast<DWORD>(sizeof(denied) - 1), &written, nullptr);
        return 78;
    }
    // These are the only allowed ASCII literals; no lossy wide conversion.
    if (scenario == L"normal") s.scenario = "normal";
    else if (scenario == L"early-up") s.scenario = "early-up";
    else if (scenario == L"setup-fail") s.scenario = "setup-fail";
    else if (scenario == L"stop") s.scenario = "stop";
    else s.scenario = "writer-stall";
    s.generation = qpc() ^ (static_cast<std::uint64_t>(GetCurrentProcessId()) << 32);
    if (!s.generation) s.generation = 1;
    s.nonce = static_cast<std::uintptr_t>(s.generation);
    s.evidence = CreateFileW(argv[5], GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (s.evidence == INVALID_HANDLE_VALUE) return 65;
    const bool dpi = SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) ||
        AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),
                                     DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!dpi || !create_signals()) {
        close_signals();
        CloseHandle(s.evidence);
        return 66;
    }
    std::thread logger{log_owner};
    record("startup", ",\"scenario\":\"" + s.scenario +
        "\",\"implementation_sha\":\"" + PANEBIND_BUILD_SHA +
        "\",\"generation\":" + std::to_string(s.generation) +
        ",\"guest_test_only\":true,\"shield_is_product\":true");
    const std::wstring run_id(argv[7]);
    const std::wstring scenario_w(argv[3]);
    const std::wstring log_path(argv[5]);
    s.writer = std::make_unique<op::LiveMoveWriter>(s.generation,
                                                     owned_writer_callbacks());
    s.shield = std::make_unique<op::GestureInputShield>(op::GestureShieldCallbacks{
        [run_id, scenario_w, log_path] {
            return guest_guard(run_id, scenario_w, log_path);
        },
        [](const op::GestureShieldRequest& request) {
            // Published owned facts only. This is not Explorer attribution.
            return request.generation == s.generation &&
                request.source == s.source &&
                request.process_id == GetCurrentProcessId() &&
                request.thread_id == s.source_thread &&
                s.authorized_start && s.tagged_down && s.associated_raw_down &&
                s.native_downs == 1 && s.native_starts == 1 && s.native_ends == 0 &&
                s.raw_ups == 0 && !s.retired && left_high() &&
                same_owned(s.source) && GetForegroundWindow() == s.source;
        },
        on_shield_event
    });
    const bool initialized = s.writer->ready() && s.shield->start();
    record("resource_start", ",\"initialized\":" + json_bool(initialized));
    std::thread writer_thread;
    std::thread ui_thread;
    bool result = false;
    if (initialized) {
        writer_thread = std::thread(writer_owner);
        ui_thread = std::thread(ui_owner);
        result = drive();
    }
    // Test-only cleanup is attempted while the self-owned overlay still
    // exists; never manufacture UP toward a foreign root after shield removal.
    const bool cleanup_released = release_owned_left("final_guarded_cleanup");
    retire("final_shutdown", b::MoveHandoffEscapeReason::ExplicitStop);
    if (s.shield && s.overlay)
        (void)s.shield->request_remove(s.generation,
            op::GestureShieldRemovalReason::Shutdown);
    const auto shield_facts = s.shield->stop();
    const bool writer_gone = !writer_thread.joinable() ||
        wait_for(s.writer_gone, 4000, "writer_thread_gone");
    if (writer_thread.joinable() && writer_gone) writer_thread.join();
    if (same_owned(s.source)) PostMessageW(s.source, WM_CLOSE, 0, 0);
    const bool ui_gone = !ui_thread.joinable() ||
        wait_for(s.ui_gone, 3000, "owned_ui_gone");
    if (ui_thread.joinable() && ui_gone) ui_thread.join();
    const bool actual_counts = initialized && s.native_downs == 1 &&
        s.native_starts == 1 && s.native_ends == 1 &&
        s.raw_downs == 1 && s.raw_ups == 1 && s.tagged_down &&
        s.associated_raw_down && s.matching_raw_up;
    const bool resource_verdict = s.scenario == "setup-fail" ? s.resource_failure.load() :
        !s.resource_failure.load();
    const bool accepted = result && cleanup_released && actual_counts &&
        shield_facts.clean() && writer_gone && ui_gone &&
        s.writer_failures == 0 && resource_verdict && s.log_ok && !left_high();
    record("shutdown", ",\"scenario_pass\":" + json_bool(result) +
        ",\"accepted\":" + json_bool(accepted) +
        ",\"shield_clean\":" + json_bool(shield_facts.clean()) +
        ",\"receiver_destroyed\":" + json_bool(shield_facts.receiver_destroyed) +
        ",\"raw_removed\":" + json_bool(shield_facts.raw_registration_removed) +
        ",\"hotkey_unregistered\":" + json_bool(shield_facts.hotkey_unregistered) +
        ",\"owned_ui_gone\":" + json_bool(ui_gone) +
        ",\"writer_gone\":" + json_bool(writer_gone) +
        ",\"actual_counts\":" + json_bool(actual_counts) +
        ",\"resource_failure\":" + json_bool(s.resource_failure) +
        ",\"resource_verdict\":" + json_bool(resource_verdict) +
        ",\"native_attempts\":" + std::to_string(s.native_attempts.load()) +
        ",\"writer_failures\":" + std::to_string(s.writer_failures.load()) +
        ",\"left_high\":" + json_bool(left_high()));
    {
        std::lock_guard lock{s.log_mutex};
        s.log_stop = true;
    }
    s.log_cv.notify_one();
    if (logger.joinable()) logger.join();
    CloseHandle(s.evidence);
    close_signals();
    if (!writer_gone || !ui_gone || !shield_facts.clean()) {
        // Only this disposable test process is ended. The guest runner must
        // still classify input cleanup as FAIL/UNKNOWN and destroy its guest.
        ExitProcess(74);
    }
    return accepted ? 0 : 2;
}
