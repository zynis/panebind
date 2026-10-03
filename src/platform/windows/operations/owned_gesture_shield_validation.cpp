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
constexpr UINT msg_source_pause = WM_APP + 18;
constexpr DWORD source_pause_ms = 5000;
constexpr DWORD short_deadline_ms = 3000;

struct RawSample { POINT cursor{}; std::uint64_t packet{}, tick{}; };

struct State {
    std::string scenario;
    std::atomic<std::uint64_t> generation{0};
    std::uintptr_t nonce{};
    HANDLE evidence{INVALID_HANDLE_VALUE};
    std::mutex log_mutex;
    std::condition_variable log_cv;
    std::deque<std::string> log_queue;
    bool log_stop{};
    std::atomic<bool> log_ok{true};
    std::uint64_t log_sequence{};
    HANDLE ui_ready{}, ui_gone{}, legacy_down{}, native_start{}, native_end{}, modal_return{};
    HANDLE bootstrap_down{}, bootstrap_up{};
    HANDLE raw_down{}, raw_up{}, legacy_up{}, isolation_ready{}, isolation_gone{};
    HANDLE capture_ready{}, capture_failure{}, capture_lost{}, capture_released{};
    HANDLE hotkey_stop{}, writer_receipt{}, writer_stall_started{}, writer_gone{}, raw_notice{};
    HANDLE source_pause_entered{}, source_pause_resumed{}, source_pause_release{};
    std::atomic<HWND> source{nullptr}, control{nullptr}, overlay{nullptr};
    std::atomic<DWORD> source_thread{0};
    std::atomic<bool> held{false}, retired{false}, armed{false};
    std::atomic<bool> bootstrap_phase{true};
    std::atomic<bool> tagged_down{false}, associated_raw_down{false};
    std::atomic<bool> authorized_start{false};
    std::atomic<bool> authorized_end{false}, capture_authority{false};
    std::atomic<bool> capture_established{false}, own_capture_released{false};
    std::atomic<bool> association_established{false}, association_detached{false};
    std::atomic<std::uint32_t> association_attempts{0}, detach_attempts{0};
    std::atomic<DWORD> shield_thread{0};
    std::atomic<std::uint32_t> capture_attempts{0};
    std::atomic<bool> matching_raw_up{false}, normal_removal_requested{false};
    std::atomic<bool> overlay_destroyed{false}, hotkey_unregistered{false};
    std::atomic<bool> stall_started_held{false}, stall_deadline_removed{false};
    std::atomic<std::uint64_t> source_pause_begin_tick{0}, source_pause_resume_tick{0};
    std::atomic<std::uint64_t> isolation_gone_tick{0};
    std::atomic<std::uint64_t> isolation_ready_tick{0};
    std::uint32_t completed_gestures{};
    std::atomic<std::uint64_t> raw_sequence{0}, down_watermark{0};
    std::atomic<std::uint64_t> handoff_raw_watermark{0};
    std::atomic<std::uint32_t> native_downs{0}, native_starts{0}, native_ends{0};
    std::atomic<std::uint32_t> client_downs{0}, client_ups{0};
    std::atomic<std::uint32_t> raw_downs{0}, raw_ups{0}, legacy_ups{0};
    std::atomic<std::uint32_t> cancel_attempts{0}, native_attempts{0};
    std::atomic<std::uint32_t> writer_failures{0}, control_mouse{0};
    std::atomic<bool> snapped_exact{false}, resource_failure{false};
    std::atomic<bool> trace_click_route{false};
    std::atomic<std::uint32_t> traced_hit_tests{0};
    std::atomic<std::uint32_t> traced_legacy_moves{0};
    std::mutex down_mutex, raw_mutex, cancel_mutex;
    std::mutex diagnostic_frame_mutex;
    std::optional<op::MoveFrameGeometry> last_confirmed_frame;
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
    std::atomic<std::shared_ptr<b::MoveMagnetSession>> motion{
        std::make_shared<b::MoveMagnetSession>()};
    std::atomic<std::shared_ptr<op::MoveFrameContinuity>> continuity{
        std::make_shared<op::MoveFrameContinuity>()};
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
[[nodiscard]] std::string frame_json(const std::optional<op::MoveFrameGeometry>& frame) {
    return frame ? "{\"positioning\":" + rect_json(frame->positioning) +
        ",\"visible\":" + rect_json(frame->visible) + "}" : "null";
}
[[nodiscard]] std::string shield_windowpos_sample_json(
    const op::GestureShieldWindowPosSample& value) {
    return "{\"window\":" + std::to_string(hwnd_number(value.window)) +
        ",\"insert_after\":" +
        std::to_string(reinterpret_cast<std::intptr_t>(value.insert_after)) +
        ",\"x\":" + std::to_string(value.x) +
        ",\"y\":" + std::to_string(value.y) +
        ",\"width\":" + std::to_string(value.width) +
        ",\"height\":" + std::to_string(value.height) +
        ",\"flags\":" + std::to_string(value.flags) + "}";
}
[[nodiscard]] std::string shield_windowpos_json(
    const op::GestureShieldWindowPosFacts& value) {
    return "{\"count\":" + std::to_string(value.count) +
        ",\"first\":" + shield_windowpos_sample_json(value.first) +
        ",\"last\":" + shield_windowpos_sample_json(value.last) + "}";
}
[[nodiscard]] std::string shield_placement_json(
    const op::GestureShieldPlacementFacts& value) {
    return "{\"attempted\":" + boolean(value.attempted) +
        ",\"window\":" + std::to_string(hwnd_number(value.window)) +
        ",\"insert_after\":" +
        std::to_string(reinterpret_cast<std::intptr_t>(value.insert_after)) +
        ",\"x\":" + std::to_string(value.x) +
        ",\"y\":" + std::to_string(value.y) +
        ",\"width\":" + std::to_string(value.width) +
        ",\"height\":" + std::to_string(value.height) +
        ",\"flags\":" + std::to_string(value.flags) +
        ",\"succeeded\":" + boolean(value.succeeded) +
        ",\"win32_error\":" + std::to_string(value.win32_error) +
        ",\"after_exstyle\":" + std::to_string(value.after_exstyle) +
        ",\"changing_count_before\":" +
        std::to_string(value.changing_count_before) +
        ",\"changing_count_after\":" +
        std::to_string(value.changing_count_after) +
        ",\"changed_count_before\":" +
        std::to_string(value.changed_count_before) +
        ",\"changed_count_after\":" +
        std::to_string(value.changed_count_after) + "}";
}
[[nodiscard]] std::string shield_setup_json(const op::GestureShieldEvent& event) {
    return ",\"created_exstyle\":" + std::to_string(event.created_exstyle) +
        ",\"initial_placement\":" + shield_placement_json(event.initial_placement) +
        ",\"retry_placement\":" + shield_placement_json(event.retry_placement) +
        ",\"windowpos_changing\":" +
        shield_windowpos_json(event.windowpos_changing) +
        ",\"windowpos_changed\":" +
        shield_windowpos_json(event.windowpos_changed);
}
[[nodiscard]] std::string shield_gui_json(const op::GestureShieldGuiFacts& facts) {
    return "{\"thread_id\":" + std::to_string(facts.thread_id) +
        ",\"available\":" + boolean(facts.available) +
        ",\"capture\":" + std::to_string(hwnd_number(facts.capture)) +
        ",\"move_size\":" + std::to_string(hwnd_number(facts.move_size)) +
        ",\"menu_owner\":" + std::to_string(hwnd_number(facts.menu_owner)) +
        ",\"flags\":" + std::to_string(facts.flags) +
        ",\"active\":" + std::to_string(hwnd_number(facts.active)) +
        ",\"focus\":" + std::to_string(hwnd_number(facts.focus)) + "}";
}
[[nodiscard]] std::string shield_association_json(
    const op::GestureShieldAssociationFacts& facts) {
    return "{\"generation\":" + std::to_string(facts.generation) +
        ",\"owner_thread_id\":" + std::to_string(facts.owner_thread_id) +
        ",\"source_thread_id\":" + std::to_string(facts.source_thread_id) +
        ",\"source_process_id\":" + std::to_string(facts.source_process_id) +
        ",\"source\":" + std::to_string(hwnd_number(facts.source)) +
        ",\"desktop_verified\":" + boolean(facts.desktop_verified) +
        ",\"owner_session_id\":" + std::to_string(facts.owner_session_id) +
        ",\"source_session_id\":" + std::to_string(facts.source_session_id) +
        ",\"owner_desktop_query\":" + boolean(facts.owner_desktop_query) +
        ",\"source_desktop_query\":" + boolean(facts.source_desktop_query) +
        ",\"input_desktop_query\":" + boolean(facts.input_desktop_query) +
        ",\"owner_desktop_input\":" + boolean(facts.owner_desktop_input) +
        ",\"source_desktop_input\":" + boolean(facts.source_desktop_input) +
        ",\"input_desktop_active\":" + boolean(facts.input_desktop_active) +
        ",\"attach_attempted\":" + boolean(facts.attach_attempted) +
        ",\"attach_completed\":" + boolean(facts.attach_completed) +
        ",\"attach_succeeded\":" + (facts.attach_completed ?
            boolean(facts.attach_succeeded) : std::string{"null"}) +
        ",\"attach_error\":" + (facts.attach_completed ?
            std::to_string(facts.attach_error) : std::string{"null"}) +
        ",\"attach_before_qpc\":" + std::to_string(facts.attach_before_qpc) +
        ",\"attach_after_qpc\":" + std::to_string(facts.attach_after_qpc) +
        ",\"foreground_before\":" + std::to_string(hwnd_number(facts.foreground_before)) +
        ",\"foreground_after\":" + std::to_string(hwnd_number(facts.foreground_after)) +
        ",\"owner_before\":" + shield_gui_json(facts.owner_before) +
        ",\"source_before\":" + shield_gui_json(facts.source_before) +
        ",\"owner_after\":" + shield_gui_json(facts.owner_after) +
        ",\"source_after\":" + shield_gui_json(facts.source_after) +
        ",\"detach_attempted\":" + boolean(facts.detach_attempted) +
        ",\"detach_completed\":" + boolean(facts.detach_completed) +
        ",\"detach_succeeded\":" + (facts.detach_completed ?
            boolean(facts.detach_succeeded) : std::string{"null"}) +
        ",\"detach_error\":" + (facts.detach_completed ?
            std::to_string(facts.detach_error) : std::string{"null"}) +
        ",\"detach_before_qpc\":" + std::to_string(facts.detach_before_qpc) +
        ",\"detach_after_qpc\":" + std::to_string(facts.detach_after_qpc) +
        ",\"foreground_before_detach\":" +
            std::to_string(hwnd_number(facts.foreground_before_detach)) +
        ",\"foreground_after_detach\":" +
            std::to_string(hwnd_number(facts.foreground_after_detach)) +
        ",\"owner_before_detach\":" + shield_gui_json(facts.owner_before_detach) +
        ",\"source_before_detach\":" + shield_gui_json(facts.source_before_detach) +
        ",\"owner_after_detach\":" + shield_gui_json(facts.owner_after_detach) +
        ",\"source_after_detach\":" + shield_gui_json(facts.source_after_detach) + "}";
}
[[nodiscard]] std::string shield_capture_json(const op::GestureShieldCaptureFacts& facts) {
    return ",\"owner_thread_id\":" + std::to_string(facts.owner_thread_id) +
        ",\"attempted\":" + boolean(facts.attempted) +
        ",\"previous_capture\":" + std::to_string(hwnd_number(facts.previous)) +
        ",\"actual_after\":" + std::to_string(hwnd_number(facts.actual_after)) +
        ",\"foreground_before\":" + std::to_string(hwnd_number(facts.foreground_before)) +
        ",\"foreground_after\":" + std::to_string(hwnd_number(facts.foreground_after)) +
        ",\"source_before\":" + shield_gui_json(facts.source_before) +
        ",\"source_after\":" + shield_gui_json(facts.source_after) +
        ",\"foreground_before_gui\":" + shield_gui_json(facts.foreground_before_gui) +
        ",\"foreground_after_gui\":" + shield_gui_json(facts.foreground_after_gui) +
        ",\"owner_before\":" + shield_gui_json(facts.owner_before) +
        ",\"owner_after\":" + shield_gui_json(facts.owner_after) +
        ",\"failure\":" + std::to_string(static_cast<int>(facts.failure)) +
        ",\"release_attempted\":" + boolean(facts.release_attempted) +
        ",\"release_completed\":" + boolean(facts.release_completed) +
        ",\"release_succeeded\":" + (facts.release_completed ?
            boolean(facts.release_succeeded) : std::string{"null"}) +
        ",\"release_before\":" + std::to_string(hwnd_number(facts.release_before)) +
        ",\"release_after\":" + std::to_string(hwnd_number(facts.release_after)) +
        ",\"release_error\":" + (facts.release_completed ?
            std::to_string(facts.release_error) : std::string{"null"}) +
        ",\"release_before_qpc\":" + std::to_string(facts.release_before_qpc) +
        ",\"release_after_qpc\":" + std::to_string(facts.release_after_qpc) +
        ",\"capture_changed_to\":" + std::to_string(hwnd_number(facts.capture_changed_to)) +
        ",\"own_release_message\":" + boolean(facts.own_release_message) +
        ",\"association\":" + shield_association_json(facts.association);
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
void record_release_route_snapshot(std::string_view reason,
                                   std::string_view phase) noexcept {
    POINT cursor{};
    const bool cursor_ok = GetCursorPos(&cursor) != FALSE;
    GUITHREADINFO gui{sizeof(gui)};
    SetLastError(0);
    const bool gui_ok = GetGUIThreadInfo(s.source_thread, &gui) != FALSE;
    const DWORD gui_error = gui_ok ? 0 : GetLastError();
    const HWND hit = cursor_ok ? WindowFromPoint(cursor) : nullptr;
    const HWND root = hit ? GetAncestor(hit, GA_ROOT) : nullptr;
    record("test_up_route_snapshot", ",\"reason\":\"" + std::string(reason) +
        "\",\"phase\":\"" + std::string(phase) +
        "\",\"source\":" + std::to_string(hwnd_number(s.source)) +
        ",\"source_thread\":" + std::to_string(s.source_thread.load()) +
        ",\"gui_ok\":" + boolean(gui_ok) +
        ",\"gui_error\":" + std::to_string(gui_error) +
        ",\"gui_flags\":" + std::to_string(gui.flags) +
        ",\"capture\":" + std::to_string(hwnd_number(gui.hwndCapture)) +
        ",\"move_size\":" + std::to_string(hwnd_number(gui.hwndMoveSize)) +
        ",\"foreground\":" +
            std::to_string(hwnd_number(GetForegroundWindow())) +
        ",\"cursor_ok\":" + boolean(cursor_ok) +
        ",\"cursor\":[" + std::to_string(cursor.x) + "," +
            std::to_string(cursor.y) + "],\"hit\":" +
            std::to_string(hwnd_number(hit)) +
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
[[nodiscard]] bool stable_context(std::string_view* rejection = nullptr) noexcept {
    auto reject = [rejection](std::string_view why) {
        if (rejection) *rejection = why;
        return false;
    };
    const HWND source = s.source, control = s.control;
    if (!same_owned(source) || !same_owned(control)) return reject("owned_identity");
    if (!guest_desktop()) return reject("interactive_desktop");
    if (GetForegroundWindow() != source) return reject("foreground");
    if (!s.initial_source || !s.initial_control ||
        !IsWindowVisible(source) || IsIconic(source) || IsZoomed(source) ||
        !IsWindowVisible(control) || IsIconic(control) || IsZoomed(control))
        return reject("window_state_or_baseline");
    GUITHREADINFO gui{sizeof(gui)};
    if (!GetGUIThreadInfo(s.source_thread, &gui)) return reject("source_gui_query");
    // Attached input queues can expose this gesture's exact self-owned shield
    // capture in the source snapshot. Never accept a different capture HWND.
    const HWND own_overlay = s.overlay;
    const bool capture_is_own = gui.hwndCapture == own_overlay &&
        op::gesture_shield_exact_capture(own_overlay, s.shield_thread);
    if ((gui.hwndCapture && !capture_is_own) || gui.hwndMoveSize ||
        gui.hwndMenuOwner || (gui.flags & (GUI_INMENUMODE | GUI_SYSTEMMENUMODE |
                                     GUI_POPUPMENUMODE | GUI_INMOVESIZE)))
        return reject("capture_or_modal_state");
    const auto a = capture(source), b = capture(control);
    MONITORINFO info{sizeof(info)};
    const HMONITOR monitor = MonitorFromWindow(source, MONITOR_DEFAULTTONULL);
    if (!a || !b) return reject("actual_geometry_query");
    if (a->visible.size() != s.initial_source->visible.size() ||
        a->positioning.size() != s.initial_source->positioning.size())
        return reject("source_size");
    if (!same_frame(*b, *s.initial_control)) return reject("peer_geometry");
    if (monitor != s.monitor || !GetMonitorInfoW(monitor, &info) ||
        !EqualRect(&info.rcWork, &s.work_area)) return reject("monitor_or_work_area");
    if (GetDpiForWindow(source) != s.dpi || GetDpiForWindow(control) != s.dpi)
        return reject("dpi");
    if (rejection) *rejection = "none";
    return true;
}
[[nodiscard]] bool owned_capture_live() noexcept {
    return s.capture_established &&
        op::gesture_shield_exact_capture(s.overlay, s.shield_thread);
}

void retire(std::string_view reason, b::MoveHandoffEscapeReason escape) noexcept {
    s.armed = false;
    s.retired = true;
    s.capture_authority = false;
    op::MoveRetireFacts facts{};
    if (s.writer) facts = s.writer->retire(s.generation);
    s.continuity.load()->retire(s.generation);
    (void)s.motion.load()->escape(s.generation, escape);
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
    const auto generation = s.generation.load();
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
                s.capture_authority = false;
                if (s.writer) (void)s.writer->retire(generation);
                s.continuity.load()->retire(generation);
                if (s.scenario != "legacy-control")
                    (void)s.motion.load()->raw_up_observed(generation, true);
                if (s.scenario != "legacy-control" &&
                    WaitForSingleObject(s.legacy_up, 0) == WAIT_OBJECT_0 && s.shield) {
                    (void)s.motion.load()->legacy_up_delivery_observed(generation, true);
                    if (s.motion.load()->removal_allowed(generation) &&
                        !s.normal_removal_requested.exchange(true))
                        (void)s.shield->request_remove(generation,
                            op::GestureShieldRemovalReason::NormalUp);
                }
            }
            record("raw_up", ",\"packet\":" + std::to_string(event.raw_sequence) +
                ",\"generation_at_receiver\":" + std::to_string(event.generation) +
                ",\"owned_down_matched\":" + boolean(matched) +
                ",\"own_hit\":" + boolean(own_hit) +
                ",\"message_time\":" + std::to_string(event.message_time) +
                ",\"message_point\":[" + std::to_string(event.message_point.x) +
                "," + std::to_string(event.message_point.y) + "]" +
                ",\"message_point_root\":" +
                    std::to_string(hwnd_number(event.message_point_root)) +
                ",\"cursor_available\":" + boolean(event.cursor_available) +
                ",\"cursor\":[" + std::to_string(event.cursor_now.x) + "," +
                    std::to_string(event.cursor_now.y) + "]" +
                ",\"cursor_root\":" + std::to_string(hwnd_number(
                    event.cursor_available ? root_at(event.cursor_now) : nullptr)) +
                ",\"observed_foreground\":" +
                    std::to_string(hwnd_number(event.observed_foreground)) +
                ",\"foreground_gui_available\":" +
                    boolean(event.foreground_gui_available) +
                ",\"foreground_capture\":" +
                    std::to_string(hwnd_number(event.foreground_capture)) +
                ",\"foreground_move_size\":" +
                    std::to_string(hwnd_number(event.foreground_move_size)) +
                ",\"foreground_gui_flags\":" +
                    std::to_string(event.foreground_gui_flags) +
                ",\"overlay\":" + std::to_string(hwnd_number(s.overlay)) +
                ",\"left_high_at_receiver\":" + boolean(left_high()));
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
            (void)s.motion.load()->legacy_up_delivery_observed(generation, true);
            if (s.motion.load()->removal_allowed(generation) &&
                !s.normal_removal_requested.exchange(true))
                (void)s.shield->request_remove(generation,
                    op::GestureShieldRemovalReason::NormalUp);
        }
        break;
    case op::GestureShieldEventKind::OverlayMouseRoute:
        record("overlay_mouse_route", ",\"generation\":" +
            std::to_string(event.generation) + ",\"overlay\":" +
            std::to_string(hwnd_number(event.overlay)) +
            ",\"message\":" + std::to_string(event.route_message) +
            ",\"wparam\":" + std::to_string(event.route_wparam) +
            ",\"capture\":" +
                std::to_string(hwnd_number(event.route_capture)) +
            ",\"message_time\":" + std::to_string(event.message_time) +
            ",\"message_point\":[" +
                std::to_string(event.message_point.x) + "," +
                std::to_string(event.message_point.y) + "]" +
            ",\"cursor_available\":" + boolean(event.cursor_available) +
            ",\"cursor\":[" + std::to_string(event.cursor_now.x) + "," +
                std::to_string(event.cursor_now.y) + "]");
        break;
    case op::GestureShieldEventKind::IsolationReady:
        s.isolation_ready_tick = GetTickCount64();
        s.overlay = event.overlay;
        record("isolation_ready", ",\"overlay\":" +
            std::to_string(hwnd_number(event.overlay)) +
            ",\"generation\":" + std::to_string(event.generation) +
            ",\"isolation_timeout_ms\":" + std::to_string(event.isolation_timeout_ms) +
            ",\"initial_exstyle\":" + std::to_string(event.initial_exstyle) +
            ",\"topmost_retry_attempted\":" +
            boolean(event.topmost_retry_attempted) +
            ",\"observed_exstyle\":" + std::to_string(event.observed_exstyle) +
            shield_setup_json(event));
        SetEvent(s.isolation_ready);
        break;
    case op::GestureShieldEventKind::IsolationGone:
        s.isolation_gone_tick = GetTickCount64();
        s.capture_established = false;
        s.overlay = nullptr;
        s.overlay_destroyed = event.overlay_destroyed;
        s.hotkey_unregistered = event.hotkey_unregistered;
        if (event.removal_reason == op::GestureShieldRemovalReason::Deadline)
            s.stall_deadline_removed = true;
        if (event.removal_reason == op::GestureShieldRemovalReason::NormalUp) {
            (void)s.motion.load()->isolation_removed(generation);
        } else {
            retire("shield_removed", b::MoveHandoffEscapeReason::ContextLost);
        }
        record("isolation_gone", ",\"generation\":" +
            std::to_string(event.generation) + ",\"reason\":" +
            std::to_string(static_cast<int>(event.removal_reason)) +
            ",\"isolation_timeout_ms\":" + std::to_string(event.isolation_timeout_ms) +
            ",\"overlay_destroyed\":" + boolean(event.overlay_destroyed) +
            ",\"hotkey_unregistered\":" + boolean(event.hotkey_unregistered));
        SetEvent(s.isolation_gone);
        break;
    case op::GestureShieldEventKind::CaptureReady:
        s.shield_thread = event.capture.owner_thread_id;
        if (event.capture.attempted) ++s.capture_attempts;
        s.capture_established = event.generation == generation &&
            event.overlay == s.overlay && event.capture.actual_after == event.overlay &&
            event.capture.failure == op::GestureShieldCaptureFailure::None;
        record("capture_ready", ",\"generation\":" + std::to_string(event.generation) +
            ",\"overlay\":" + std::to_string(hwnd_number(event.overlay)) +
            ",\"native_ends_observed\":" + std::to_string(s.native_ends.load()) +
            ",\"left_high\":" + boolean(left_high()) +
            ",\"retired\":" + boolean(s.retired) + shield_capture_json(event.capture));
        SetEvent(s.capture_ready);
        break;
    case op::GestureShieldEventKind::ThreadAssociated:
        if (event.capture.association.attach_attempted) ++s.association_attempts;
        s.association_established =
            op::gesture_shield_association_owned_pair(event.capture.association, generation) &&
            event.capture.association.source == s.source &&
            event.capture.association.source_process_id == GetCurrentProcessId() &&
            event.capture.association.source_thread_id == s.source_thread &&
            event.capture.association.desktop_verified;
        record("thread_associated", ",\"generation\":" + std::to_string(event.generation) +
            ",\"native_ends_observed\":" + std::to_string(s.native_ends.load()) +
            ",\"normal_up_observed\":" + boolean(s.matching_raw_up) +
            ",\"actual_pair_accepted\":" + boolean(s.association_established) +
            shield_capture_json(event.capture));
        break;
    case op::GestureShieldEventKind::ThreadAssociationAttempt:
        record("thread_association_attempt", ",\"generation\":" +
            std::to_string(event.generation) + shield_capture_json(event.capture));
        break;
    case op::GestureShieldEventKind::CaptureReleaseAttempt:
        record("capture_release_attempt", ",\"generation\":" +
            std::to_string(event.generation) + shield_capture_json(event.capture));
        break;
    case op::GestureShieldEventKind::ThreadDetached:
        if (event.capture.association.detach_attempted) ++s.detach_attempts;
        s.association_detached = s.association_established &&
            op::gesture_shield_association_cleanup_confirmed(
                event.capture.association, generation);
        record("thread_detached", ",\"generation\":" + std::to_string(event.generation) +
            ",\"actual_pair_detached\":" + boolean(s.association_detached) +
            shield_capture_json(event.capture));
        break;
    case op::GestureShieldEventKind::ThreadAssociationFailure:
        if (event.capture.association.attach_attempted &&
            !event.capture.association.attach_succeeded) ++s.association_attempts;
        retire("thread_association_failure", b::MoveHandoffEscapeReason::ContextLost);
        record("thread_association_failure", ",\"generation\":" +
            std::to_string(event.generation) + shield_capture_json(event.capture));
        break;
    case op::GestureShieldEventKind::CaptureFailure:
        if (event.capture.attempted) ++s.capture_attempts;
        retire("capture_establishment_failed", b::MoveHandoffEscapeReason::ContextLost);
        record("capture_failure", ",\"generation\":" + std::to_string(event.generation) +
            ",\"overlay\":" + std::to_string(hwnd_number(event.overlay)) +
            shield_capture_json(event.capture));
        SetEvent(s.capture_failure);
        break;
    case op::GestureShieldEventKind::CaptureLost:
        s.capture_established = false;
        retire("unexpected_capture_lost", b::MoveHandoffEscapeReason::ContextLost);
        record("capture_lost", ",\"generation\":" + std::to_string(event.generation) +
            ",\"overlay\":" + std::to_string(hwnd_number(event.overlay)) +
            shield_capture_json(event.capture));
        SetEvent(s.capture_lost);
        break;
    case op::GestureShieldEventKind::CaptureReleased:
        s.capture_established = false;
        s.own_capture_released = event.generation == generation &&
            event.capture.release_attempted && event.capture.release_succeeded &&
            event.capture.release_before == event.overlay && !event.capture.release_after;
        record("capture_released", ",\"generation\":" + std::to_string(event.generation) +
            ",\"overlay\":" + std::to_string(hwnd_number(event.overlay)) +
            shield_capture_json(event.capture));
        SetEvent(s.capture_released);
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
            boolean(left_high()) + ",\"isolation_timeout_ms\":" +
            std::to_string(event.isolation_timeout_ms));
        break;
    case op::GestureShieldEventKind::ContextLost:
    case op::GestureShieldEventKind::ResourceFailure:
        if (event.kind == op::GestureShieldEventKind::ResourceFailure)
            s.resource_failure = true;
        retire("resource_or_context", b::MoveHandoffEscapeReason::ContextLost);
        record("resource_or_context", ",\"kind\":" +
            std::to_string(static_cast<int>(event.kind)) +
            ",\"error\":" + std::to_string(event.win32_error) +
            ",\"setup_stage\":" +
            std::to_string(static_cast<int>(event.setup_stage)) +
            ",\"native_failure\":" +
            std::to_string(static_cast<int>(event.native_failure)) +
            ",\"readback_failure\":" +
            std::to_string(static_cast<int>(event.readback_failure)) +
            ",\"initial_exstyle\":" + std::to_string(event.initial_exstyle) +
            ",\"topmost_retry_attempted\":" +
            boolean(event.topmost_retry_attempted) +
            ",\"observed_exstyle\":" +
            std::to_string(event.observed_exstyle) + shield_setup_json(event) +
            (event.capture.owner_thread_id ? shield_capture_json(event.capture) : std::string{}));
        break;
    }
}

void record_owned_mouse_route(HWND hwnd, UINT message, WPARAM wparam,
                              LPARAM lparam) noexcept {
    if (hwnd != s.source && hwnd != s.control) return;
    if (message == WM_MOUSEMOVE &&
        (!s.held || s.traced_legacy_moves.fetch_add(1) >= 4)) return;
    record("owned_mouse_route", ",\"generation\":" +
        std::to_string(s.generation) + ",\"hwnd\":" +
        std::to_string(hwnd_number(hwnd)) + ",\"role\":\"" +
        (hwnd == s.source ? "source" : "control") +
        "\",\"message\":" + std::to_string(message) +
        ",\"wparam\":" +
            std::to_string(static_cast<std::uintptr_t>(wparam)) +
        ",\"lparam\":" +
            std::to_string(static_cast<std::uintptr_t>(lparam)) +
        ",\"thread_capture\":" +
            std::to_string(hwnd_number(GetCapture())) +
        ",\"foreground\":" +
            std::to_string(hwnd_number(GetForegroundWindow())) +
        ",\"extra_info\":" + std::to_string(
            static_cast<std::uintptr_t>(GetMessageExtraInfo())));
}

LRESULT CALLBACK owned_wndproc(HWND hwnd, UINT message, WPARAM wparam,
                               LPARAM lparam) noexcept {
    if (s.bootstrap_phase && hwnd == s.source &&
        (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP)) {
        // A real guest-only activation click before Raw registration is not
        // this test gesture's DOWN/UP and never grants product authority.
        const bool tagged = GetMessageExtraInfo() == test_tag;
        record("bootstrap_client_message", ",\"message\":" + std::to_string(message) +
            ",\"source\":" + std::to_string(hwnd_number(hwnd)) +
            ",\"tagged\":" + boolean(tagged) + ",\"foreground\":" +
            std::to_string(hwnd_number(GetForegroundWindow())));
        if (tagged) SetEvent(message == WM_LBUTTONDOWN ? s.bootstrap_down : s.bootstrap_up);
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
    switch (message) {
    case msg_source_pause:
        if (hwnd == s.source && s.scenario == "source-pause" &&
            s.capture_established && s.authorized_end && s.native_ends == 1 &&
            !s.matching_raw_up && left_high()) {
            // Test-owned UI thread only: no SuspendThread, Explorer thread
            // manipulation or unrecoverable wait. The real resource thread
            // remains associated with this deliberately non-pumping queue.
            s.source_pause_begin_tick = GetTickCount64();
            record("source_pause_begin", ",\"generation\":" +
                std::to_string(s.generation) + ",\"source_thread\":" +
                std::to_string(GetCurrentThreadId()) + ",\"tick_ms\":" +
                std::to_string(s.source_pause_begin_tick.load()) +
                ",\"pause_bound_ms\":" + std::to_string(source_pause_ms) +
                ",\"left_high\":" + boolean(left_high()));
            SetEvent(s.source_pause_entered);
            const DWORD result = WaitForSingleObject(s.source_pause_release, source_pause_ms);
            s.source_pause_resume_tick = GetTickCount64();
            record("source_pause_resume", ",\"generation\":" +
                std::to_string(s.generation) + ",\"tick_ms\":" +
                std::to_string(s.source_pause_resume_tick.load()) +
                ",\"wait_result\":" + std::to_string(result) +
                ",\"actor\":\"bounded_owned_test_ui_wait\"");
            SetEvent(s.source_pause_resumed);
            return 0;
        }
        return 0;
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
            if (s.scenario == "legacy-control") {
                ++s.client_downs;
                s.tagged_down = GetMessageExtraInfo() == test_tag;
                record("legacy_control_client_down", ",\"tagged\":" + boolean(s.tagged_down) +
                    ",\"source\":" + std::to_string(hwnd_number(hwnd)));
            }
            SetEvent(s.legacy_down);
        }
        [[fallthrough]];
    case WM_LBUTTONUP:
        if (message == WM_LBUTTONUP)
            record_owned_mouse_route(hwnd, message, wparam, lparam);
        if (message == WM_LBUTTONUP && hwnd == s.source && s.scenario == "legacy-control") {
            ++s.client_ups;
            record("legacy_control_client_up", ",\"source\":" +
                std::to_string(hwnd_number(hwnd)) + ",\"extra_info\":" +
                std::to_string(static_cast<std::uintptr_t>(GetMessageExtraInfo())));
            SetEvent(s.legacy_up);
        }
        if (hwnd == s.control) ++s.control_mouse;
        break;
    case WM_NCLBUTTONUP:
    case WM_CAPTURECHANGED:
    case WM_MOUSEMOVE:
        record_owned_mouse_route(hwnd, message, wparam, lparam);
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
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, x, y, 420, 210,
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
    bool context{}, armed{}, retired{}, raw_up{}, exact_capture{}, left_held{};
    bool associated_down{}, native_counts{}, model_retired{};
    std::string_view context_rejection{"none"};
    std::uint64_t captured_version{}, version_after_observation{};
    op::MoveFrameObservation observation{op::MoveFrameObservation::NotArmed};
    std::optional<op::MoveFrameGeometry> actual, last_confirmed;
    HWND cursor_hit{}, control_hit{};
};

[[nodiscard]] WriteFacts write_facts(POINT cursor) noexcept {
    // Version before native capture: a completed own write can make this
    // sample stale, never evidence of an external translation.
    WriteFacts facts{};
    const auto generation = s.generation.load();
    const auto continuity = s.continuity.load();
    const auto version = continuity->snapshot_version(generation);
    const bool context = stable_context(&facts.context_rejection);
    const auto source_now = context ? capture(s.source) : std::nullopt;
    const auto observed = source_now ? continuity->observe(generation,
        *source_now, version) : op::MoveFrameObservation::NotArmed;
    const auto decision = op::classify_move_sample(
        observed, context && source_now.has_value());
    const bool stable = decision == op::MoveSampleDecision::Process;
    facts.context = context;
    facts.armed = s.armed; facts.retired = s.retired; facts.raw_up = s.matching_raw_up;
    facts.exact_capture = owned_capture_live(); facts.left_held = left_high();
    facts.associated_down = s.tagged_down && s.associated_raw_down;
    facts.native_counts = s.native_starts == 1 && s.native_ends == 1 && s.cancel_attempts == 1;
    facts.model_retired = s.motion.load()->retired(generation);
    facts.stable = stable;
    facts.active = stable && facts.armed && !facts.retired && !facts.raw_up &&
        facts.exact_capture && facts.left_held && facts.associated_down &&
        facts.native_counts && !facts.model_retired;
    facts.hit = shield_hit(cursor); facts.decision = decision;
    facts.cursor_hit = root_at(cursor); facts.control_hit = root_at(s.control_point);
    facts.captured_version = version;
    facts.version_after_observation = continuity->snapshot_version(generation);
    facts.observation = observed; facts.actual = source_now;
    {
        std::lock_guard lock{s.diagnostic_frame_mutex};
        facts.last_confirmed = s.last_confirmed_frame;
    }
    return facts;
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
                // Remain inside the REAL shared placement callback until the
                // independent resource owner has removed its shield, or one
                // finite test timeout expires. Early ContextLost is evidence
                // of interruption, not a reason to keep the writer asleep
                // after cleanup or to relabel this Deadline-only case PASS.
                const DWORD wait_result = WaitForSingleObject(s.isolation_gone,
                    shield_deadline_ms + 1000);
                record("writer_stall_end", ",\"isolation_gone\":" +
                    boolean(wait_result == WAIT_OBJECT_0) +
                    ",\"resource_wait_result\":" + std::to_string(wait_result) +
                    ",\"wait_timeout_ms\":" + std::to_string(shield_deadline_ms + 1000));
            }
            POINT cursor{};
            if (!GetCursorPos(&cursor)) return op::MoveNativePlacement{};
            const auto facts = write_facts(cursor);
            const auto now = capture(s.source);
            if (!facts.stable || !facts.active || !facts.hit || !now ||
                !same_frame(*now, before) || !s.writer ||
                !s.continuity.load()->begin_attempt(s.generation, quantum, before,
                                             {expected, target}))
                return op::MoveNativePlacement{};
            // No other potentially blocking validation after this claim.
            if (!s.writer->begin_native_attempt(s.generation)) {
                (void)s.continuity.load()->abort_unissued(s.generation, quantum);
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
            const auto version = s.continuity.load()->snapshot_version(s.generation);
            const auto now = capture(s.source);
            const bool context = stable_context();
            const auto observed = now && context ? s.continuity.load()->observe(
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
                s.continuity.load()->finish_attempt(s.generation, receipt->quantum,
                    receipt->native_attempted, receipt->exact, receipt->after);
            if (!receipt->native_attempted)
                (void)s.continuity.load()->abort_unissued(s.generation, receipt->quantum);
            if (committed && receipt->after) {
                std::lock_guard lock{s.diagnostic_frame_mutex};
                s.last_confirmed_frame = receipt->after;
            }
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
                (void)s.motion.load()->write_result(plan, exact,
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

[[nodiscard]] bool process_raw_continuations(
    std::optional<POINT> expected_cursor = std::nullopt,
    std::uint64_t after_packet = 0) noexcept {
    std::deque<RawSample> batch;
    {
        std::lock_guard lock{s.raw_mutex};
        batch.swap(s.raw_moves);
        // Consume this batch's queue notice under the producer's same short
        // lock. A later WM_INPUT enqueues its sample AND sets a fresh notice;
        // an old duplicate/empty notice cannot acknowledge the next test move.
        ResetEvent(s.raw_notice);
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
        const bool facts_processable = facts.active && facts.hit;
        const auto motion = s.motion.load();
        const bool sample_created = facts_processable &&
            motion->sample_cursor(s.generation,
                {sample.cursor.x, sample.cursor.y}, sample.tick,
                facts.active, facts.stable, facts.hit);
        const bool model_retired_after = motion->retired(s.generation);
        const auto dispatch = op::classify_move_cursor_dispatch(
            facts_processable, sample_created, model_retired_after);
        record("cursor_dispatch", ",\"generation\":" +
            std::to_string(s.generation.load()) + ",\"packet\":" +
            std::to_string(sample.packet) + ",\"cursor\":[" +
            std::to_string(sample.cursor.x) + "," + std::to_string(sample.cursor.y) +
            "],\"stable\":" + boolean(facts.stable) +
            ",\"active\":" + boolean(facts.active) + ",\"hit\":" + boolean(facts.hit) +
            ",\"static_context\":" + boolean(facts.context) +
            ",\"context_rejection\":\"" + std::string(facts.context_rejection) +
            "\",\"captured_version\":" + std::to_string(facts.captured_version) +
            ",\"version_after_observation\":" + std::to_string(facts.version_after_observation) +
            ",\"observation\":" + std::to_string(static_cast<int>(facts.observation)) +
            ",\"actual_frame\":" + frame_json(facts.actual) +
            ",\"last_confirmed_frame_diagnostic_only\":" + frame_json(facts.last_confirmed) +
            ",\"armed\":" + boolean(facts.armed) + ",\"retired\":" + boolean(facts.retired) +
            ",\"raw_up_observed\":" + boolean(facts.raw_up) +
            ",\"exact_capture\":" + boolean(facts.exact_capture) +
            ",\"left_held\":" + boolean(facts.left_held) +
            ",\"associated_down\":" + boolean(facts.associated_down) +
            ",\"native_counts_valid\":" + boolean(facts.native_counts) +
            ",\"cursor_hit\":" + std::to_string(hwnd_number(facts.cursor_hit)) +
            ",\"control_hit\":" + std::to_string(hwnd_number(facts.control_hit)) +
            ",\"model_sample_called\":" + boolean(facts_processable) +
            ",\"model_sample_created\":" + boolean(sample_created) +
            ",\"model_retired_before\":" + boolean(facts.model_retired) +
            ",\"model_retired_after\":" + boolean(model_retired_after) +
            ",\"dispatch\":" + std::to_string(static_cast<int>(dispatch)));
        if (dispatch == op::MoveCursorDispatch::Retire) {
            // A matched UP can race the sampled facts. Its already-observed
            // retirement must not be changed into an abnormal context escape.
            if (!s.matching_raw_up)
                retire(facts_processable ? "cursor_model_retired" : "cursor_facts_rejected",
                    facts_processable ? b::MoveHandoffEscapeReason::NativeFailure :
                                        b::MoveHandoffEscapeReason::ContextLost);
            continue;
        }
        if (dispatch == op::MoveCursorDispatch::NoNewPlan) continue;
        const auto plan = s.motion.load()->take_pending(s.generation,
            facts.active, facts.stable, facts.hit);
        if (!plan) continue;
        const bool accepted = s.writer && s.writer->offer({plan->generation, plan->quantum,
                                                          plan->target_visible, plan->snapped});
        const bool expected_sample = !expected_cursor ||
            (sample.packet > after_packet && sample.cursor.x == expected_cursor->x &&
             sample.cursor.y == expected_cursor->y);
        offered |= accepted && expected_sample;
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

[[nodiscard]] bool drive_cursor_continuation(POINT target,
                                            std::string_view label) noexcept {
    // One test input, then at most two event notifications within one 2 s
    // budget. This is not cursor polling or repeated SendInput. A queued
    // duplicate from an earlier move is legal but cannot stand in for the
    // requested new cursor observation or a product writer receipt.
    const auto before_packet = s.raw_sequence.load();
    if (!move_cursor(target)) return false;
    const auto deadline = GetTickCount64() + 2000;
    for (unsigned notification = 0; notification != 2; ++notification) {
        const auto now = GetTickCount64();
        if (now >= deadline || !wait_for(s.raw_notice,
                static_cast<DWORD>(deadline - now), label)) return false;
        if (process_raw_continuations(target, before_packet)) return true;
        if (s.retired || s.matching_raw_up) return false;
    }
    return false;
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
        (!gui.hwndCapture || gui.hwndCapture == source ||
            (gui.hwndCapture == s.overlay && owned_capture_live())) &&
        (!gui.hwndMoveSize || gui.hwndMoveSize == source) &&
        !gui.hwndMenuOwner && !(gui.flags & (GUI_INMENUMODE |
            GUI_SYSTEMMENUMODE | GUI_POPUPMENUMODE)) && left_high();
    record("cleanup_up_route", ",\"reason\":\"" + std::string(reason) +
        "\",\"hit\":" + std::to_string(hwnd_number(hit)) +
        ",\"foreground\":" +
        std::to_string(hwnd_number(GetForegroundWindow())) +
        ",\"exact_owned_route\":" + boolean(own_route));
    if (!own_route) return false; // Guest failure, never inject into a foreign root.
    record_release_route_snapshot(reason, "before_sendinput");
    // One genuine UP transition, never a manufactured legacy callback. This
    // owned normal-driver contrast includes the CURRENT absolute cursor point
    // in the same INPUT; control/abort cases retain the original UP-only form.
    const bool same_point_move = reason == "normal_raw_up";
    const DWORD up_flags = MOUSEEVENTF_LEFTUP | (same_point_move ?
        (MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK) : 0);
    record("test_up_form", ",\"form\":\"" + std::string(same_point_move ?
        "same_point_move_plus_single_up" : "up_only") + "\",\"up_transition_count\":1");
    if (!send_mouse(up_flags, cursor)) return false;
    const bool observed = wait_for(s.raw_up, 1500, "actual_raw_cleanup_up");
    record_release_route_snapshot(reason, "after_raw_wait");
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
    for (int offset : {72, 90, 108}) {
        for (int delta : {10, 15, 20, 25, 30, 35, 40}) {
            const POINT point{rect.left + offset, rect.top + delta};
            if (root_at(point) != source) continue;
            const LPARAM location = MAKELPARAM(static_cast<SHORT>(point.x),
                                               static_cast<SHORT>(point.y));
            LRESULT dwm_hit{};
            const bool dwm_handled = DwmDefWindowProc(source, WM_NCHITTEST,
                0, location, &dwm_hit) != FALSE;
            if (dwm_handled && dwm_hit != HTCAPTION) continue;
            const LRESULT hit = SendMessageW(source, WM_NCHITTEST, 0, location);
            if (hit == HTCAPTION) {
                result = point;
                record("owned_caption_point", ",\"point\":[" +
                    std::to_string(point.x) + "," + std::to_string(point.y) +
                    "],\"dwm_handled\":" + boolean(dwm_handled) +
                    ",\"dwm_hit\":" + std::to_string(dwm_hit));
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] bool bootstrap_owned_foreground() noexcept {
    if (!wait_for(s.ui_ready, 2000, "owned_ui_ready_before_raw_registration") ||
        !same_owned(s.source) || !same_owned(s.control) || !guest_desktop() ||
        !virtual_desktop_rect(s.virtual_rect) || left_high()) return false;
    if (GetForegroundWindow() == s.source) {
        s.bootstrap_phase = false;
        record("owned_foreground_bootstrap", ",\"already_foreground\":true,\"test_click\":false");
        return true;
    }
    RECT client{};
    if (!GetClientRect(s.source, &client)) return false;
    POINT point{client.left + (client.right - client.left) / 2,
                client.top + (client.bottom - client.top) / 2};
    if (!ClientToScreen(s.source, &point) || !move_cursor(point)) return false;
    const auto clear_gui = [](DWORD thread) noexcept {
        GUITHREADINFO gui{sizeof(gui)};
        return thread && GetGUIThreadInfo(thread, &gui) && !gui.hwndCapture &&
            !gui.hwndMoveSize && !gui.hwndMenuOwner &&
            !(gui.flags & (GUI_INMENUMODE | GUI_SYSTEMMENUMODE |
                          GUI_POPUPMENUMODE | GUI_INMOVESIZE));
    };
    const auto fresh_route = [&]() noexcept {
        const HWND foreground = GetForegroundWindow();
        const DWORD foreground_thread = foreground ?
            GetWindowThreadProcessId(foreground, nullptr) : 0;
        DWORD_PTR hit{};
        POINT actual_cursor{};
        return same_owned(s.source) && guest_desktop() &&
            GetCursorPos(&actual_cursor) && actual_cursor.x == point.x &&
            actual_cursor.y == point.y && root_at(actual_cursor) == s.source &&
            IsWindowVisible(s.source) && !IsIconic(s.source) && !IsZoomed(s.source) &&
            clear_gui(s.source_thread) && clear_gui(foreground_thread) &&
            (GetAsyncKeyState(VK_RBUTTON) & 0x8000) == 0 &&
            (GetAsyncKeyState(VK_MBUTTON) & 0x8000) == 0 &&
            (GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0 &&
            (GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0 &&
            (GetAsyncKeyState(VK_MENU) & 0x8000) == 0 &&
            SendMessageTimeoutW(s.source, WM_NCHITTEST, 0,
                MAKELPARAM(static_cast<SHORT>(point.x), static_cast<SHORT>(point.y)),
                SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 500, &hit) && hit == HTCLIENT;
    };
    if (!fresh_route() || left_high()) return false;
    record("owned_foreground_bootstrap", ",\"already_foreground\":false,\"test_click\":true,"
        "\"raw_receiver_started\":false,\"exact_owned_client\":true");
    const bool sent_down = send_mouse(MOUSEEVENTF_LEFTDOWN, point);
    const bool actual_down = sent_down && wait_for(s.bootstrap_down, 1500,
                                                  "actual_bootstrap_client_down");
    // Even failure cleanup is bound to the fresh owned client route. This
    // test-only click never releases or takes another window's capture.
    const bool sent_up = sent_down && fresh_route() &&
        send_mouse(MOUSEEVENTF_LEFTUP, point);
    const bool actual_up = sent_up && wait_for(s.bootstrap_up, 1500,
                                              "actual_bootstrap_client_up");
    Sleep(30); // Bounded guest driver key-state readback, not product polling.
    const bool activated = actual_down && actual_up && !left_high() &&
        same_owned(s.source) && GetForegroundWindow() == s.source && fresh_route();
    record("owned_foreground_bootstrap_result", ",\"actual_down\":" + boolean(actual_down) +
        ",\"actual_up\":" + boolean(actual_up) + ",\"foreground_owned\":" + boolean(activated) +
        ",\"left_high\":" + boolean(left_high()) + ",\"raw_receiver_started\":false");
    if (activated) s.bootstrap_phase = false;
    return activated;
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
        !s.motion.load()->begin(s.generation, {down->x, down->y}, frame->visible,
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
    // A shorter finite TEST request exercises the SAME product timeout path.
    // Product/default requests remain 30 s; this is not a latency SLA.
    const DWORD timeout_ms = (s.scenario == "writer-stall" ||
        s.scenario == "source-pause") ? short_deadline_ms : shield_deadline_ms;
    const bool queued = s.shield && s.shield->request_isolation({
        s.generation, source, GetCurrentProcessId(), s.source_thread, timeout_ms});
    record("isolation_request", ",\"queued\":" + boolean(queued) +
        ",\"generation\":" + std::to_string(s.generation) +
        ",\"isolation_timeout_ms\":" + std::to_string(timeout_ms));
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
    return actual && s.motion.load()->isolation_ready(s.generation, true);
}

[[nodiscard]] bool request_capture_and_arm_writer() noexcept {
    const bool queued = s.shield && s.shield->request_capture_after_native_end({
        s.generation, s.source, GetCurrentProcessId(), s.source_thread});
    record("capture_request", ",\"queued\":" + boolean(queued) +
        ",\"actual_end_published\":" + boolean(s.authorized_end) +
        ",\"authority_published\":" + boolean(s.capture_authority) +
        ",\"normal_up_observed\":" + boolean(s.matching_raw_up));
    if (!queued || !wait_for(s.capture_ready, 2000, "actual_product_capture_ready") ||
        !s.association_established || s.association_detached ||
        s.retired || s.matching_raw_up || !left_high() || !owned_capture_live()) return false;
    const auto handoff = capture(s.source);
    const bool fresh = handoff && stable_context() &&
        s.continuity.load()->arm(s.generation, *handoff);
    record("capture_handoff", ",\"fresh\":" + boolean(fresh) +
        ",\"actual_capture\":" + boolean(owned_capture_live()) +
        ",\"handoff_visible\":" + (handoff ? rect_json(handoff->visible) : "null"));
    if (!fresh || s.retired || s.matching_raw_up) return false;
    {
        std::lock_guard lock{s.diagnostic_frame_mutex};
        s.last_confirmed_frame = handoff;
    }
    {
        std::lock_guard lock{s.raw_mutex};
        s.raw_moves.clear();
        ResetEvent(s.raw_notice);
        s.handoff_raw_watermark.store(s.raw_sequence.load(std::memory_order_acquire),
                                      std::memory_order_release);
    }
    record("handoff_raw_watermark", ",\"last_pre_handoff_packet\":" +
        std::to_string(s.handoff_raw_watermark.load()));
    s.armed = true;
    // A matched UP racing this publication keeps writer permissions revoked.
    if (s.retired || s.matching_raw_up) { s.armed = false; return false; }
    return true;
}

[[nodiscard]] bool bounded_native_cancel_and_handoff(bool acquire_capture = true) noexcept {
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
        s.motion.load()->may_cancel(s.generation);
    if (!preflight) return false;
    {
        // Only the claim is serialized with actual Raw UP observation. The
        // resource callback never waits for the bounded native API itself.
        std::lock_guard lock{s.cancel_mutex};
        if (s.matching_raw_up || s.retired ||
            !s.motion.load()->cancel_issued(s.generation)) return false;
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
        !s.motion.load()->native_end_observed(s.generation, true)) return false;
    const auto handoff = capture(s.source);
    const bool fresh = handoff && stable_context();
    record("handoff", ",\"actual_end\":true,\"fresh\":" +
        boolean(fresh) + ",\"handoff_visible\":" +
        (handoff ? rect_json(handoff->visible) : "null"));
    if (!fresh) return false;
    s.authorized_end = true;
    if (s.retired || s.matching_raw_up) return false;
    s.capture_authority = true;
    if (s.retired || s.matching_raw_up) { s.capture_authority = false; return false; }
    return !acquire_capture || request_capture_and_arm_writer();
}

[[nodiscard]] bool run_capture_abort() noexcept {
    const bool before_capture = s.scenario == "capture-early-up" ||
        s.scenario == "capture-fail";
    if (!bounded_native_cancel_and_handoff(!before_capture)) return false;
    if (s.scenario == "capture-early-up") {
        if (!release_owned_left("actual_up_after_end_before_capture")) return false;
        // No capture request after the actual UP. Missing legacy delivery is
        // not repaired or relabelled NormalUp: this is a bounded abort case.
        const bool already_gone = WaitForSingleObject(s.isolation_gone, 0) == WAIT_OBJECT_0;
        if (!already_gone)
            (void)s.shield->request_remove(s.generation,
                op::GestureShieldRemovalReason::ExplicitStop);
        const bool gone = wait_for(s.isolation_gone, 2000, "post_end_early_up_abort");
        record("capture_early_up_verdict", ",\"actual_end\":" +
            boolean(s.native_ends == 1) + ",\"actual_raw_up\":" + boolean(s.matching_raw_up) +
            ",\"capture_attempts\":" + std::to_string(s.capture_attempts.load()) +
            ",\"normal_removal\":" + boolean(s.normal_removal_requested));
        return gone && s.retired && s.overlay_destroyed && s.hotkey_unregistered &&
            s.capture_attempts == 0 && s.native_attempts == 0;
    }
    if (s.scenario == "capture-fail") {
        // Test-owned authority is deliberately withdrawn after the real END.
        // The PRODUCT resource must observe and report its authorization
        // rejection. This is not evidence of SetCapture API failure.
        s.capture_authority = false;
        const bool queued = s.shield->request_capture_after_native_end({
            s.generation, s.source, GetCurrentProcessId(), s.source_thread});
        record("test_capture_authority_withdrawn", ",\"actual_end\":true,\"queued\":" +
            boolean(queued) + ",\"fault_actor\":\"owned_test_adapter\"");
        const bool failed = queued && wait_for(s.capture_failure, 2000,
                                               "actual_capture_authorization_rejection");
        const bool gone = failed && wait_for(s.isolation_gone, 2000, "capture_failure_abort");
        const bool released = gone && release_owned_left("capture_failure_test_cleanup");
        record("capture_failure_verdict", ",\"authorization_rejection\":" + boolean(failed) +
            ",\"api_attempts\":" + std::to_string(s.capture_attempts.load()) +
            ",\"normal_removal\":" + boolean(s.normal_removal_requested));
        return released && s.retired && s.overlay_destroyed && s.hotkey_unregistered &&
            s.capture_attempts == 0 && s.native_attempts == 0 &&
            !s.normal_removal_requested;
    }
    if (!owned_capture_live() || !left_high()) return false;
    if (s.scenario == "capture-stop") {
        if (!send_stop_chord() || !wait_for(s.hotkey_stop, 1500,
                "actual_post_end_capture_hotkey") ||
            !wait_for(s.isolation_gone, 2000, "post_end_hotkey_shield_removal")) return false;
        const bool held_at_removal = left_high();
        const bool released = held_at_removal &&
            release_owned_left("post_end_stop_test_cleanup");
        record("post_end_stop_verdict", ",\"actual_hotkey\":true,\"held_at_removal\":" +
            boolean(held_at_removal) + ",\"capture_released\":" + boolean(s.own_capture_released) +
            ",\"normal_removal\":" + boolean(s.normal_removal_requested));
        return released && s.own_capture_released && s.retired &&
            s.association_detached && s.overlay_destroyed && s.hotkey_unregistered &&
            s.native_attempts == 0 &&
            !s.normal_removal_requested;
    }
    // Bounded test-only fault, delivered to the current exact OWN overlay.
    // Its unchanged DefWindowProc processes real WM_CANCELMODE/ReleaseCapture
    // on its owning thread, producing actual WM_CAPTURECHANGED. Neither this
    // driver nor the product takes/releases another thread's capture HWND.
    const HWND overlay = s.overlay;
    const DWORD owner_thread = s.shield_thread;
    DWORD overlay_pid{};
    GUITHREADINFO before{sizeof(before)};
    const bool exact_overlay = overlay && IsWindow(overlay) &&
        GetWindowThreadProcessId(overlay, &overlay_pid) == owner_thread &&
        overlay_pid == GetCurrentProcessId() && s.authorized_end &&
        s.native_ends == 1 && s.cancel_attempts == 1 && !s.retired &&
        !s.matching_raw_up && left_high() && guest_desktop() &&
        GetForegroundWindow() == s.source &&
        GetGUIThreadInfo(owner_thread, &before) && before.hwndCapture == overlay &&
        !before.hwndMoveSize && !before.hwndMenuOwner &&
        !(before.flags & (GUI_INMENUMODE | GUI_SYSTEMMENUMODE |
                          GUI_POPUPMENUMODE | GUI_INMOVESIZE));
    record("test_owned_shield_cancel_capture_loss_permission", ",\"generation\":" +
        std::to_string(s.generation) + ",\"overlay\":" + std::to_string(hwnd_number(overlay)) +
        ",\"owner_thread\":" + std::to_string(owner_thread) +
        ",\"actual_owner_capture_before\":" + std::to_string(hwnd_number(before.hwndCapture)) +
        ",\"exact_overlay\":" + boolean(exact_overlay) + ",\"api_entered\":false");
    if (!exact_overlay) return false;
    DWORD_PTR cancel_result{};
    SetLastError(0);
    const bool returned = SendMessageTimeoutW(overlay, WM_CANCELMODE, 0, 0,
        SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 1000, &cancel_result) != 0;
    const DWORD error = returned ? 0 : GetLastError();
    GUITHREADINFO after{sizeof(after)};
    const bool after_available = GetGUIThreadInfo(owner_thread, &after) != FALSE;
    record("test_owned_shield_cancel_capture_loss", ",\"generation\":" +
        std::to_string(s.generation) + ",\"overlay\":" + std::to_string(hwnd_number(overlay)) +
        ",\"message\":" + std::to_string(WM_CANCELMODE) +
        ",\"api_entered\":true,\"returned\":" + boolean(returned) +
        ",\"message_result\":" + std::to_string(cancel_result) +
        ",\"error\":" + std::to_string(error) + ",\"owner_gui_after_available\":" +
        boolean(after_available) + ",\"actual_owner_capture_after\":" +
        (after_available ? std::to_string(hwnd_number(after.hwndCapture)) : "null") +
        ",\"source_cancel_attempts\":" + std::to_string(s.cancel_attempts.load()) +
        ",\"actor\":\"guest_test_driver_exact_owned_overlay\"");
    const bool lost = returned && after_available && !after.hwndCapture &&
        wait_for(s.capture_lost, 1500, "actual_capture_lost");
    const bool gone = lost && wait_for(s.isolation_gone, 2000, "capture_loss_abort");
    const bool held_at_removal = left_high();
    const bool raw_cleanup = gone && held_at_removal &&
        release_owned_left("capture_loss_test_cleanup");
    record("capture_loss_verdict", ",\"actual_capture_lost\":" + boolean(lost) +
        ",\"held_at_removal\":" + boolean(held_at_removal) +
        ",\"source_cancel_attempts\":" + std::to_string(s.cancel_attempts.load()) +
        ",\"normal_removal\":" + boolean(s.normal_removal_requested));
    return raw_cleanup && s.retired && s.overlay_destroyed && s.hotkey_unregistered &&
        s.association_detached &&
        s.native_attempts == 0 && !s.normal_removal_requested && !s.own_capture_released;
}

[[nodiscard]] bool run_normal_or_stall() noexcept {
    if (!bounded_native_cancel_and_handoff()) return false;
    if (s.scenario == "source-pause") {
        if (!s.association_established || s.association_detached ||
            !owned_capture_live() || !left_high() ||
            !PostMessageW(s.source, msg_source_pause, 0, 0) ||
            !wait_for(s.source_pause_entered, 1000, "actual_owned_source_pause_entered"))
            return false;
        // Allow the source's own five-second finite wait to recover even when
        // the attached resource queue cannot execute cleanup by its 3 s
        // deadline. A later successful cleanup is not independent-exit PASS.
        const bool gone = wait_for(s.isolation_gone, source_pause_ms + 2000,
                                   "associated_source_pause_resource_exit");
        const auto gone_tick = s.isolation_gone_tick.load();
        const auto resumed_tick_at_gone = s.source_pause_resume_tick.load();
        const auto began_tick = s.source_pause_begin_tick.load();
        const bool independent_exit = gone && gone_tick > began_tick &&
            (resumed_tick_at_gone == 0 || gone_tick < resumed_tick_at_gone);
        const bool resumed = wait_for(s.source_pause_resumed, source_pause_ms + 1000,
                                     "bounded_owned_source_resume");
        const auto resumed_tick = s.source_pause_resume_tick.load();
        const bool held_at_removal = left_high();
        const bool released = gone && resumed && held_at_removal &&
            release_owned_left("source_pause_test_cleanup");
        record("source_pause_verdict", ",\"source_pause_begin_tick_ms\":" +
            std::to_string(began_tick) + ",\"source_resume_tick_ms\":" +
            std::to_string(resumed_tick) + ",\"isolation_gone_tick_ms\":" +
            std::to_string(gone_tick) + ",\"isolation_ready_tick_ms\":" +
            std::to_string(s.isolation_ready_tick.load()) +
            ",\"requested_deadline_ms\":" + std::to_string(short_deadline_ms) +
            ",\"source_pause_bound_ms\":" + std::to_string(source_pause_ms) +
            ",\"independent_exit_before_source_resume\":" + boolean(independent_exit) +
            ",\"actual_deadline_removal\":" + boolean(s.stall_deadline_removed) +
            ",\"actual_association_detached\":" + boolean(s.association_detached) +
            ",\"cleanup_raw_up\":" + boolean(released) +
            ",\"normal_removal\":" + boolean(s.normal_removal_requested));
        return independent_exit && resumed && released && s.stall_deadline_removed &&
            s.association_detached && s.own_capture_released && s.overlay_destroyed &&
            s.hotkey_unregistered && s.retired && s.native_attempts == 0 &&
            !s.normal_removal_requested;
    }
    if (s.scenario == "writer-stall") {
        const POINT continuation{s.start_cursor.x + 8, s.start_cursor.y + 7};
        if (!drive_cursor_continuation(continuation, "raw_continuation_for_stall") ||
            !wait_for(s.writer_stall_started, 2000, "actual_writer_callback_stall") ||
            !s.stall_started_held || !left_high()) return false;
        const bool gone = wait_for(s.isolation_gone, short_deadline_ms + 2000,
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
            ",\"actual_association_detached\":" + boolean(s.association_detached) +
            ",\"requested_deadline_ms\":" + std::to_string(short_deadline_ms) +
            ",\"cleanup_raw_up\":" + boolean(released) +
            ",\"native_attempts\":" + std::to_string(s.native_attempts.load()));
        return gone && released && receipt && s.stall_deadline_removed &&
            s.overlay_destroyed && s.hotkey_unregistered && s.own_capture_released &&
            s.association_detached &&
            s.native_attempts == 0 && s.retired;
    }
    // Only this test driver uses a fixed trajectory. Product session uses the
    // observed original DOWN anchor and arbitrary event-driven cursor quanta.
    const auto& from = s.initial_source->visible;
    const auto& to = s.initial_control->visible;
    if (s.completed_gestures != 0) {
        // The first gesture leaves the source already snapped. Repeating
        // only that same target correctly yields `already_exact`, not a new
        // native attempt. Exercise a real free placement before returning to
        // snap, still computed from THIS gesture's original DOWN/window anchor.
        const g::Rect free_target{from.left() - 48, from.top() + 32,
                                  from.right() - 48, from.bottom() + 32};
        const POINT free_cursor{s.down_point.x - 48, s.down_point.y + 32};
        if (!work_area_contains(free_target)) return false;
        Sleep(150); // Guest driver input spacing, not product cursor polling.
        const auto attempts_before = s.native_attempts.load();
        const bool followed = drive_cursor_continuation(free_cursor,
            "followup_raw_free_continuation") &&
            wait_for(s.writer_receipt, 2000, "followup_actual_free_writer_receipt");
        const auto actual_free = followed ? capture(s.source) : std::nullopt;
        const bool free_exact = actual_free && actual_free->visible == free_target &&
            s.native_attempts > attempts_before && s.writer_failures == 0 &&
            !s.snapped_exact && !s.retired && owned_capture_live();
        record("followup_free_placement_verdict", ",\"generation\":" +
            std::to_string(s.generation.load()) + ",\"target\":" +
            rect_json(free_target) + ",\"actual_visible\":" +
            (actual_free ? rect_json(actual_free->visible) : "null") +
            ",\"actual_native_attempt_added\":" +
            boolean(s.native_attempts > attempts_before) +
            ",\"exact_free_before_resnap\":" + boolean(free_exact));
        if (!free_exact) return false;
    }
    const auto dx = to.left() - from.right() - 5;
    const auto dy = to.top() - from.top() + 4;
    const auto x = static_cast<std::int64_t>(s.down_point.x) + dx;
    const auto y = static_cast<std::int64_t>(s.down_point.y) + dy;
    if (x < INT_MIN || x > INT_MAX || y < INT_MIN || y > INT_MAX) return false;
    const POINT snap{static_cast<LONG>(x), static_cast<LONG>(y)};
    Sleep(150); // Test input separation, not a resident cursor sampler.
    if (!drive_cursor_continuation(snap, "raw_snap_continuation") ||
        !wait_for(s.writer_receipt, 2000, "shared_writer_receipt")) return false;
    if (!s.snapped_exact) {
        Sleep(150);
        const POINT reacquire{snap.x + 1, snap.y};
        if (!drive_cursor_continuation(reacquire, "raw_reacquire_continuation") ||
            !wait_for(s.writer_receipt, 2000, "shared_writer_reacquire_receipt"))
            return false;
    }
    if (!s.snapped_exact || s.native_attempts == 0 || s.writer_failures != 0 ||
        !shield_hit(snap) || !release_owned_left("normal_raw_up")) return false;
    const bool legacy = wait_for(s.legacy_up, 1500, "actual_overlay_legacy_up");
    const bool gone = wait_for(s.isolation_gone, 2000, "normal_shield_removal");
    const bool no_control_delivery = s.control_mouse == 0;
    record("normal_verdict", ",\"snapped_exact\":" + boolean(s.snapped_exact) +
        ",\"generation\":" + std::to_string(s.generation.load()) +
        ",\"gesture_index\":" + std::to_string(s.completed_gestures + 1) +
        ",\"native_attempts\":" + std::to_string(s.native_attempts.load()) +
        ",\"raw_downs\":" + std::to_string(s.raw_downs.load()) +
        ",\"raw_ups\":" + std::to_string(s.raw_ups.load()) +
        ",\"legacy_up\":" + boolean(legacy) +
        ",\"normal_removal\":" + boolean(s.normal_removal_requested) +
        ",\"association_established\":" + boolean(s.association_established) +
        ",\"association_detached\":" + boolean(s.association_detached) +
        ",\"control_mouse_zero\":" + boolean(no_control_delivery));
    return legacy && gone && no_control_delivery && s.normal_removal_requested &&
        s.capture_attempts == 1 && s.own_capture_released && s.association_detached &&
        s.association_attempts == 1 && s.detach_attempts == 1 &&
        s.raw_downs == 1 && s.raw_ups == 1 && s.legacy_ups == 1 &&
        s.native_downs == 1 && s.native_starts == 1 && s.native_ends == 1 &&
        s.cancel_attempts == 1 &&
        s.overlay_destroyed && s.hotkey_unregistered && s.retired;
}

[[nodiscard]] bool drive_one() noexcept {
    if (s.scenario == "legacy-control") {
        if (!prepare_owned()) return false;
        RECT client{};
        if (!GetClientRect(s.source, &client)) return false;
        POINT point{client.left + (client.right - client.left) / 2,
                    client.top + (client.bottom - client.top) / 2};
        if (!ClientToScreen(s.source, &point) || !move_cursor(point) ||
            root_at(point) != s.source || GetForegroundWindow() != s.source) return false;
        DWORD_PTR hit{};
        const bool client_hit = SendMessageTimeoutW(s.source, WM_NCHITTEST, 0,
            MAKELPARAM(static_cast<SHORT>(point.x), static_cast<SHORT>(point.y)),
            SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 500, &hit) && hit == HTCLIENT;
        record("legacy_control_route", ",\"point\":[" + std::to_string(point.x) + "," +
            std::to_string(point.y) + "],\"actual_client_hit\":" + boolean(client_hit) +
            ",\"purpose\":\"guest_legacy_delivery_control_not_product_pass\"");
        if (!client_hit) return false;
        s.held = true;
        if (!send_mouse(MOUSEEVENTF_LEFTDOWN, point) ||
            !wait_for(s.legacy_down, 1500, "actual_control_client_down") ||
            !wait_for(s.raw_down, 1500, "actual_control_raw_down") ||
            !s.tagged_down || s.client_downs != 1 || !s.associated_raw_down) return false;
        const bool released = release_owned_left("legacy_control_actual_up");
        const bool legacy = wait_for(s.legacy_up, 1500, "actual_control_client_up");
        record("legacy_control_verdict", ",\"actual_raw_up\":" + boolean(s.matching_raw_up) +
            ",\"actual_client_up\":" + boolean(legacy) +
            ",\"client_downs\":" + std::to_string(s.client_downs.load()) +
            ",\"client_ups\":" + std::to_string(s.client_ups.load()) +
            ",\"normal_removal\":" + boolean(s.normal_removal_requested) +
            ",\"purpose\":\"guest_input_delivery_control_only\"");
        return released && legacy && s.client_ups == 1 && s.capture_attempts == 0 &&
            s.cancel_attempts == 0 && s.native_attempts == 0 && !s.overlay &&
            !s.normal_removal_requested;
    }
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
    if (s.scenario == "capture-stop" || s.scenario == "capture-early-up" ||
        s.scenario == "capture-fail" || s.scenario == "capture-lost")
        return run_capture_abort();
    return run_normal_or_stall();
}

[[nodiscard]] bool next_owned_gesture(std::thread& writer_thread) noexcept {
    // The same product shield, receiver, UI threads and exact HWNDs survive
    // this boundary. Only one-gesture model/writer state is replaced, after
    // genuine NormalUp removal, release AND detachment have been observed.
    const auto previous_generation = s.generation.load();
    GUITHREADINFO source_gui{sizeof(source_gui)}, resource_gui{sizeof(resource_gui)};
    const bool queue_clean = s.shield_thread &&
        GetGUIThreadInfo(s.source_thread, &source_gui) &&
        GetGUIThreadInfo(s.shield_thread, &resource_gui) &&
        !source_gui.hwndCapture && !source_gui.hwndMoveSize &&
        !resource_gui.hwndCapture && !resource_gui.hwndMoveSize;
    const bool modifiers_clear = (GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0 &&
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0 &&
        (GetAsyncKeyState(VK_MENU) & 0x8000) == 0;
    const bool duplicate_remove = s.shield->request_remove(previous_generation,
        op::GestureShieldRemovalReason::NormalUp);
    const bool ready = s.completed_gestures == 1 && s.normal_removal_requested &&
        s.matching_raw_up && s.legacy_ups == 1 && s.own_capture_released &&
        s.association_established && s.association_detached &&
        s.association_attempts == 1 && s.detach_attempts == 1 &&
        !s.overlay && s.overlay_destroyed && s.hotkey_unregistered &&
        !left_high() && modifiers_clear && queue_clean && !duplicate_remove &&
        same_owned(s.source) && same_owned(s.control);
    record("between_gestures_actual_readback", ",\"previous_generation\":" +
        std::to_string(previous_generation) + ",\"source_capture\":" +
        std::to_string(hwnd_number(source_gui.hwndCapture)) +
        ",\"resource_capture\":" + std::to_string(hwnd_number(resource_gui.hwndCapture)) +
        ",\"modifiers_clear\":" + boolean(modifiers_clear) +
        ",\"left_high\":" + boolean(left_high()) +
        ",\"duplicate_remove_queued\":" + boolean(duplicate_remove) +
        ",\"ready_for_same_resource_followup\":" + boolean(ready));
    if (!ready || !wait_for(s.writer_gone, 2000, "first_gesture_writer_retired"))
        return false;
    if (writer_thread.joinable()) writer_thread.join();

    // Raw may continue observing unpressed cursor movement. It has no active
    // generation, no held DOWN and no armed writer during this reset. Atomic
    // shared ownership keeps even a late diagnostic snapshot alive; no HWND
    // operation occurs under a lock that could block UP or shield removal.
    s.held = false;
    s.armed = false;
    s.authorized_start = false;
    s.authorized_end = false;
    s.capture_authority = false;
    s.retired = true;
    s.motion.store(std::make_shared<b::MoveMagnetSession>());
    s.continuity.store(std::make_shared<op::MoveFrameContinuity>());
    {
        std::lock_guard lock{s.diagnostic_frame_mutex};
        s.last_confirmed_frame.reset();
    }
    const auto new_generation = previous_generation + 1;
    if (!new_generation) return false;
    s.generation = new_generation;
    s.tagged_down = false;
    s.associated_raw_down = false;
    s.capture_established = false;
    s.own_capture_released = false;
    s.association_established = false;
    s.association_detached = false;
    s.matching_raw_up = false;
    s.normal_removal_requested = false;
    s.overlay_destroyed = false;
    s.hotkey_unregistered = false;
    s.stall_started_held = false;
    s.stall_deadline_removed = false;
    s.native_downs = 0; s.native_starts = 0; s.native_ends = 0;
    s.raw_downs = 0; s.raw_ups = 0; s.legacy_ups = 0;
    s.cancel_attempts = 0; s.native_attempts = 0;
    s.capture_attempts = 0; s.association_attempts = 0; s.detach_attempts = 0;
    s.writer_failures = 0; s.control_mouse = 0;
    s.snapped_exact = false; s.resource_failure = false;
    s.isolation_ready_tick = 0; s.isolation_gone_tick = 0;
    {
        std::lock_guard lock{s.down_mutex};
        s.observed_down.reset(); s.down_frame.reset();
    }
    {
        std::lock_guard lock{s.raw_mutex};
        s.raw_moves.clear();
    }
    {
        std::lock_guard lock{s.cancel_mutex};
        s.cancel_claimed = false;
    }
    for (HANDLE event : {s.legacy_down, s.native_start, s.native_end, s.modal_return,
            s.raw_down, s.raw_up, s.legacy_up, s.isolation_ready, s.isolation_gone,
            s.capture_ready, s.capture_failure, s.capture_lost, s.capture_released,
            s.hotkey_stop, s.writer_receipt, s.writer_stall_started, s.writer_gone,
            s.raw_notice}) ResetEvent(event);
    s.writer = std::make_unique<op::LiveMoveWriter>(new_generation,
                                                   owned_writer_callbacks());
    s.retired = false;
    if (!s.writer->ready()) return false;
    writer_thread = std::thread(writer_owner);
    record("next_gesture_created", ",\"generation\":" + std::to_string(new_generation) +
        ",\"same_shield_and_ui_resources\":true,\"new_original_down_required\":true");
    return true;
}

[[nodiscard]] bool drive(std::thread& writer_thread) noexcept {
    const bool first = drive_one();
    if (!first) return false;
    ++s.completed_gestures;
    if (s.scenario != "normal-repeat") return true;
    if (!next_owned_gesture(writer_thread)) return false;
    const bool second = drive_one();
    if (second) ++s.completed_gestures;
    record("continuous_followup_verdict", ",\"completed_gestures\":" +
        std::to_string(s.completed_gestures) + ",\"same_process_and_resource_threads\":true" +
        ",\"accepted\":" + boolean(second && s.completed_gestures == 2));
    return second && s.completed_gestures == 2;
}

[[nodiscard]] bool create_signals() noexcept {
    auto make = [](bool manual = true) { return CreateEventW(nullptr, manual, FALSE, nullptr); };
    s.ui_ready = make();
    s.ui_gone = make();
    s.bootstrap_down = make();
    s.bootstrap_up = make();
    s.legacy_down = make();
    s.native_start = make();
    s.native_end = make();
    s.modal_return = make();
    s.raw_down = make();
    s.raw_up = make();
    s.legacy_up = make();
    s.isolation_ready = make();
    s.isolation_gone = make();
    s.capture_ready = make();
    s.capture_failure = make();
    s.capture_lost = make();
    s.capture_released = make();
    s.hotkey_stop = make();
    s.writer_receipt = make(false);
    s.writer_stall_started = make();
    s.writer_gone = make();
    s.raw_notice = make(false);
    s.source_pause_entered = make();
    s.source_pause_resumed = make();
    s.source_pause_release = make();
    return s.ui_ready && s.ui_gone && s.bootstrap_down && s.bootstrap_up &&
        s.legacy_down && s.native_start && s.native_end &&
        s.modal_return && s.raw_down && s.raw_up && s.legacy_up &&
        s.isolation_ready && s.isolation_gone && s.hotkey_stop &&
        s.capture_ready && s.capture_failure && s.capture_lost && s.capture_released &&
        s.writer_receipt && s.writer_stall_started && s.writer_gone &&
        s.raw_notice && s.source_pause_entered && s.source_pause_resumed &&
        s.source_pause_release;
}

void close_signals() noexcept {
    for (HANDLE handle : {s.ui_ready, s.ui_gone, s.legacy_down, s.native_start, s.native_end,
            s.bootstrap_down, s.bootstrap_up,
            s.modal_return, s.raw_down, s.raw_up, s.legacy_up,
            s.isolation_ready, s.isolation_gone, s.hotkey_stop,
            s.capture_ready, s.capture_failure, s.capture_lost, s.capture_released,
            s.writer_receipt, s.writer_stall_started, s.writer_gone, s.raw_notice,
            s.source_pause_entered, s.source_pause_resumed, s.source_pause_release}) {
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
    if (scenario != L"legacy-control" && scenario != L"normal" &&
        scenario != L"normal-repeat" && scenario != L"source-pause" && scenario != L"early-up" &&
        scenario != L"setup-fail" && scenario != L"stop" &&
        scenario != L"capture-stop" && scenario != L"capture-early-up" &&
        scenario != L"capture-fail" && scenario != L"capture-lost" &&
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
    if (scenario == L"legacy-control") s.scenario = "legacy-control";
    else if (scenario == L"normal") s.scenario = "normal";
    else if (scenario == L"normal-repeat") s.scenario = "normal-repeat";
    else if (scenario == L"source-pause") s.scenario = "source-pause";
    else if (scenario == L"early-up") s.scenario = "early-up";
    else if (scenario == L"setup-fail") s.scenario = "setup-fail";
    else if (scenario == L"stop") s.scenario = "stop";
    else if (scenario == L"capture-stop") s.scenario = "capture-stop";
    else if (scenario == L"capture-early-up") s.scenario = "capture-early-up";
    else if (scenario == L"capture-fail") s.scenario = "capture-fail";
    else if (scenario == L"capture-lost") s.scenario = "capture-lost";
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
        on_shield_event,
        [](const op::GestureShieldRequest& request) {
            // These are published REAL owned END facts. The resource itself
            // additionally checks source/foreground/owner GUI state and UP.
            return request.generation == s.generation && request.source == s.source &&
                request.process_id == GetCurrentProcessId() &&
                request.thread_id == s.source_thread && s.authorized_start &&
                s.authorized_end && s.capture_authority && s.native_ends == 1 &&
                s.cancel_attempts == 1 && s.tagged_down && s.associated_raw_down &&
                !s.matching_raw_up && !s.retired && !s.armed && left_high();
        }
    });
    std::thread writer_thread;
    std::thread ui_thread{ui_owner};
    const bool bootstrapped = bootstrap_owned_foreground();
    const bool initialized = bootstrapped && s.writer->ready() && s.shield->start();
    record("resource_start", ",\"initialized\":" + json_bool(initialized) +
        ",\"foreground_bootstrap_completed\":" + json_bool(bootstrapped));
    bool result = false;
    if (initialized) {
        writer_thread = std::thread(writer_owner);
        result = drive(writer_thread);
    }
    // Test-only cleanup is attempted while the self-owned overlay still
    // exists; never manufacture UP toward a foreign root after shield removal.
    const bool cleanup_released = initialized ? release_owned_left("final_guarded_cleanup") :
        !left_high();
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
    const bool input_counts = initialized && s.raw_downs == 1 && s.raw_ups == 1 &&
        s.tagged_down && s.associated_raw_down && s.matching_raw_up;
    const bool actual_counts = input_counts && (s.scenario == "legacy-control" ?
        (s.client_downs == 1 && s.client_ups == 1 && s.native_downs == 0 &&
         s.native_starts == 0 && s.native_ends == 0 && s.cancel_attempts == 0 &&
         s.native_attempts == 0 && s.capture_attempts == 0 && !s.overlay) :
        (s.native_downs == 1 && s.native_starts == 1 && s.native_ends == 1));
    const bool resource_verdict = s.scenario == "setup-fail" ? s.resource_failure.load() :
        !s.resource_failure.load();
    const bool accepted = result && cleanup_released && actual_counts &&
        (s.scenario != "normal-repeat" || s.completed_gestures == 2) &&
        shield_facts.clean() && writer_gone && ui_gone &&
        s.writer_failures == 0 && resource_verdict && s.log_ok && !left_high();
    record("shutdown", ",\"scenario_pass\":" + json_bool(result) +
        ",\"accepted\":" + json_bool(accepted) +
        ",\"shield_clean\":" + json_bool(shield_facts.clean()) +
        ",\"receiver_destroyed\":" + json_bool(shield_facts.receiver_destroyed) +
        ",\"raw_removed\":" + json_bool(shield_facts.raw_registration_removed) +
        ",\"capture_cleanup_complete\":" + json_bool(shield_facts.capture_released) +
        ",\"association_cleanup_complete\":" + json_bool(shield_facts.association_detached) +
        ",\"completed_gestures\":" + std::to_string(s.completed_gestures) +
        ",\"hotkey_unregistered\":" + json_bool(shield_facts.hotkey_unregistered) +
        ",\"owned_ui_gone\":" + json_bool(ui_gone) +
        ",\"writer_gone\":" + json_bool(writer_gone) +
        ",\"actual_counts\":" + json_bool(actual_counts) +
        ",\"control_only\":" + json_bool(s.scenario == "legacy-control") +
        ",\"resource_failure\":" + json_bool(s.resource_failure) +
        ",\"resource_verdict\":" + json_bool(resource_verdict) +
        ",\"native_attempts\":" + std::to_string(s.native_attempts.load()) +
        ",\"writer_failures\":" + std::to_string(s.writer_failures.load()) +
        ",\"capture_attempts\":" + std::to_string(s.capture_attempts.load()) +
        ",\"own_capture_released\":" + json_bool(s.own_capture_released) +
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
