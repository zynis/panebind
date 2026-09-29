// Explicitly opted-in, test-owned input-isolation experiment. Never linked into
// the PaneBind product. Synthetic input is permitted only after exact owned-hit
// checks; this executable cannot grant Explorer write authority.
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <wtsapi32.h>
#include "core/behavior/move_magnet_session.h"
#include "platform/windows/operations/live_move_writer.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
namespace behavior = panebind::core::behavior;
namespace geometry = panebind::core::geometry;
namespace magnet = panebind::core::magnet;
namespace operations = panebind::platform::windows::operations;
constexpr UINT create_overlay_message = WM_APP + 1;
constexpr int stop_hotkey_id = 0x5042;
constexpr UINT stop_hotkey_key = VK_F11;
constexpr UINT stop_hotkey_modifiers = MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT;
constexpr DWORD max_overlay_ms = 30000;
// This starts only after the overlay is gone. It outlives the driver's
// bounded overlay wait (2 s), Raw UP wait (1.5 s), and scheduling margin;
// expiration means failed observation, never a fabricated UP.
constexpr DWORD raw_cleanup_observation_ms = 6000;
constexpr BYTE overlay_alpha = 2; // Nonzero: alpha zero would pass mouse input through.
constexpr ULONG_PTR test_input_tag = 0x50424F56; // Test-only PBOV marker; Raw packets have no such tag.
constexpr wchar_t guest_probe_path[] = L"C:\\PaneBindMVP1\\Input\\panebind-owned-overlay-probe.exe";
constexpr wchar_t guest_run_id_path[] = L"C:\\PaneBindMVP1\\Input\\run-id.txt";
constexpr wchar_t guest_output_path[] = L"C:\\PaneBindMVP1\\Output\\";
constexpr std::size_t log_capacity = 2048, log_line_capacity = 1536;
struct LogSlot {
    std::atomic<bool> ready{false};
    DWORD length{};
    char line[log_line_capacity]{};
};

struct State {
    HANDLE log = INVALID_HANDLE_VALUE;
    HANDLE stop = nullptr, ui_ready = nullptr, receiver_ready = nullptr;
    HANDLE native_start = nullptr, native_end = nullptr, native_loop_return = nullptr;
    HANDLE overlay_ready = nullptr;
    HANDLE overlay_gone = nullptr, receiver_gone = nullptr, raw_down = nullptr, raw_up = nullptr, legacy_up = nullptr;
    HANDLE ui_gone = nullptr, overlay_arm = nullptr;
    HANDLE writer_gone = nullptr, writer_receipt = nullptr, writer_stall_started = nullptr;
    HANDLE log_notice = nullptr, log_stop = nullptr, logger_gone = nullptr;
    std::array<LogSlot, log_capacity> log_slots{};
    std::atomic<std::uint64_t> sequence{0};
    std::atomic<bool> log_ok{true}, overlay_ok{false}, receiver_ok{false};
    std::atomic<bool> overlay_destroyed{false}, receiver_destroyed{false};
    std::atomic<bool> overlay_created{false}, receiver_created{false};
    std::atomic<bool> registration_removed{false}, hotkey_removed{false};
    std::atomic<bool> hotkey_registered{false}, hotkey_received{false};
    std::atomic<bool> hotkey_during_held_native{false};
    std::atomic<bool> held{false}, retired{false};
    std::atomic<bool> writer_armed{false};
    std::atomic<bool> writer_stall_held_at_start{false};
    std::atomic<HWND> source{nullptr}, control{nullptr}, receiver{nullptr}, overlay{nullptr};
    std::atomic<DWORD> source_tid{0}, overlay_tid{0};
    std::atomic<std::uint32_t> native_downs{0}, native_starts{0}, native_ends{0};
    std::atomic<std::uint32_t> cancel_attempts{0}, control_mouse{0}, overlay_mouse{0};
    std::uint32_t control_baseline{}; // Driver only, frozen before cancel.
    std::atomic<std::uint32_t> raw_downs{0}, raw_ups{0}, overlay_ups{0};
    std::atomic<bool> core_raw_up_accepted{false}, normal_removal_triggered{false};
    std::atomic<std::uint32_t> writer_offers{0}, writer_receipts{0}, writer_native_attempts{0};
    std::atomic<std::uint32_t> writer_exact{0}, writer_failures{0}, writer_snapped_offers{0};
    std::atomic<bool> writer_snapped_exact{false};
    std::atomic<std::uint64_t> writer_snapped_exact_quantum{0};
    std::mutex down_mutex;
    std::optional<POINT> actual_down_point;
    std::optional<operations::MoveFrameGeometry> actual_down_frame;
    std::mutex receipt_mutex;
    std::optional<operations::MoveWriteReceipt> snapped_exact_receipt;
    std::mutex cancel_mutex; // Orders an observed Raw UP against the one bounded cancel.
    std::atomic<std::uint64_t> raw_packet_watermark{0}, test_down_watermark{0};
    std::atomic<std::uint64_t> test_down_issue_qpc{0};
    std::atomic<std::uint64_t> candidate_raw_down{0}, candidate_raw_up{0};
    std::atomic<bool> tagged_native_down{false};
    std::atomic<std::uint64_t> original_down_qpc{0}, overlay_ready_qpc{0};
    std::atomic<std::uint64_t> cancel_qpc{0};
    RECT virtual_rect{};
    RECT initial_work_area{};
    POINT initial_cursor{}, source_down{}, control_point{};
    std::optional<operations::MoveFrameGeometry> initial_source, initial_control;
    HMONITOR initial_monitor{};
    UINT initial_dpi{};
    HWND initial_foreground{}, initial_focus{}, initial_capture{};
    DWORD deadline_ms = max_overlay_ms;
    bool inject_overlay_failure = false;
    bool inject_writer_stall = false;
    std::string scenario;
    std::uintptr_t run_nonce{};
} state;
behavior::MoveMagnetSession move_session;
std::unique_ptr<operations::LiveMoveWriter> live_writer;
thread_local POINT received_msg_point{};
thread_local DWORD received_msg_time{};

std::uint64_t qpc() noexcept {
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return static_cast<std::uint64_t>(now.QuadPart);
}
std::uint64_t number(HWND window) noexcept {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(window));
}
std::string json_bool(bool value) { return value ? "true" : "false"; }
std::string point(POINT value) {
    return "[" + std::to_string(value.x) + "," + std::to_string(value.y) + "]";
}
std::string rect(const RECT& value) {
    return "[" + std::to_string(value.left) + "," + std::to_string(value.top) +
        "," + std::to_string(value.right) + "," + std::to_string(value.bottom) + "]";
}
std::string rect(const geometry::Rect& value) {
    return "[" + std::to_string(value.left()) + "," + std::to_string(value.top()) +
        "," + std::to_string(value.right()) + "," + std::to_string(value.bottom()) + "]";
}
void log(std::string_view type, const std::string& fields = {}) noexcept {
    try {
        const auto index = state.sequence.fetch_add(1, std::memory_order_relaxed);
        if (index >= log_capacity) { state.log_ok = false; return; }
        const auto line = "{\"schema\":\"r1c4b-mvp1-owned-overlay/v1\",\"sequence\":" +
            std::to_string(index + 1) + ",\"type\":\"" + std::string(type) +
            "\",\"qpc\":" + std::to_string(qpc()) + fields + "}\n";
        if (line.size() >= log_line_capacity) { state.log_ok = false; return; }
        auto& slot = state.log_slots[index];
        std::memcpy(slot.line, line.data(), line.size());
        slot.length = static_cast<DWORD>(line.size());
        slot.ready.store(true, std::memory_order_release);
        if (state.log_notice) SetEvent(state.log_notice);
    } catch (...) { state.log_ok = false; }
}
void log_owner() noexcept {
    std::uint64_t next = 0;
    const HANDLE signals[]{state.log_notice, state.log_stop};
    for (;;) {
        while (next < std::min<std::uint64_t>(state.sequence.load(), log_capacity) &&
               state.log_slots[next].ready.load(std::memory_order_acquire)) {
            const auto& slot = state.log_slots[next];
            DWORD written{};
            if (!WriteFile(state.log, slot.line, slot.length, &written, nullptr) ||
                written != slot.length) state.log_ok = false;
            ++next;
        }
        if (WaitForSingleObject(state.log_stop, 0) == WAIT_OBJECT_0) {
            if (next < state.sequence.load()) state.log_ok = false;
            break;
        }
        const DWORD wait = WaitForMultipleObjects(2, signals, FALSE, INFINITE);
        if (wait != WAIT_OBJECT_0 && wait != WAIT_OBJECT_0 + 1) {
            state.log_ok = false;
            break;
        }
    }
    SetEvent(state.logger_gone);
}
bool same_source(HWND window) noexcept {
    DWORD pid{};
    return window && window == state.source && IsWindow(window) &&
        GetWindowThreadProcessId(window, &pid) == state.source_tid &&
        pid == GetCurrentProcessId() &&
        static_cast<std::uintptr_t>(GetWindowLongPtrW(window, GWLP_USERDATA)) == state.run_nonce;
}
bool same_control(HWND window) noexcept {
    DWORD pid{};
    return window && window == state.control && IsWindow(window) &&
        GetWindowThreadProcessId(window, &pid) == state.source_tid &&
        pid == GetCurrentProcessId() &&
        static_cast<std::uintptr_t>(GetWindowLongPtrW(window, GWLP_USERDATA)) == state.run_nonce;
}
std::optional<operations::MoveFrameGeometry> capture_frame(HWND window) noexcept {
    RECT positioning{}, visible{};
    if (!window || !GetWindowRect(window, &positioning) ||
        FAILED(DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS,
                                     &visible, sizeof(visible)))) return std::nullopt;
    const geometry::Rect p{positioning.left, positioning.top,
                           positioning.right, positioning.bottom};
    const geometry::Rect v{visible.left, visible.top, visible.right, visible.bottom};
    if (p.empty() || v.empty()) return std::nullopt;
    return operations::MoveFrameGeometry{p, v};
}
bool same_overlay(HWND window) noexcept {
    DWORD pid{};
    return window && window == state.overlay && IsWindow(window) &&
        GetWindowThreadProcessId(window, &pid) == state.overlay_tid &&
        pid == GetCurrentProcessId() &&
        static_cast<std::uintptr_t>(GetWindowLongPtrW(window, GWLP_USERDATA)) == state.run_nonce;
}
HWND root_at(POINT position) noexcept {
    const HWND found = WindowFromPoint(position);
    return found ? GetAncestor(found, GA_ROOT) : nullptr;
}
bool left_down() noexcept { return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0; }
bool input_clean() noexcept {
    for (const int key : {VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2,
             VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN}) {
        if ((GetAsyncKeyState(key) & 0x8000) != 0) return false;
    }
    return true;
}
bool interactive_default_desktop() noexcept {
    HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!input) return false;
    wchar_t input_name[256]{}, current_name[256]{};
    DWORD length{};
    const bool same = GetUserObjectInformationW(input, UOI_NAME, input_name,
            sizeof(input_name), &length) &&
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME,
            current_name, sizeof(current_name), &length) &&
        std::wstring_view(input_name) == current_name &&
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
struct OwnedWriteFacts {
    bool fresh_authority{};
    bool actual_stable{};
    bool isolation_hits{};
};
OwnedWriteFacts owned_write_facts(POINT cursor) noexcept {
    const HWND source = state.source.load(), control = state.control.load();
    const HWND overlay = state.overlay.load();
    const bool source_ok = same_source(source), control_ok = same_control(control);
    GUITHREADINFO gui{sizeof(gui)};
    const bool gui_ok = source_ok && GetGUIThreadInfo(state.source_tid, &gui);
    const bool clean_native = gui_ok && !gui.hwndCapture && !gui.hwndMoveSize &&
        !gui.hwndMenuOwner && !(gui.flags & (GUI_INMENUMODE | GUI_SYSTEMMENUMODE |
                                            GUI_POPUPMENUMODE | GUI_INMOVESIZE));
    const bool active = state.writer_armed.load(std::memory_order_acquire) &&
        !state.retired.load(std::memory_order_acquire) &&
        WaitForSingleObject(state.stop, 0) != WAIT_OBJECT_0;
    const bool authority = active && source_ok && control_ok &&
        interactive_default_desktop() && GetForegroundWindow() == source &&
        left_down() && input_clean() && state.tagged_native_down &&
        state.candidate_raw_down != 0 && state.native_starts == 1 &&
        state.native_ends == 1 && state.cancel_attempts == 1 &&
        state.overlay_ready_qpc != 0 &&
        state.overlay_ready_qpc < state.cancel_qpc && clean_native;
    const bool isolation = same_overlay(overlay) && IsWindowVisible(overlay) &&
        root_at(cursor) == overlay && root_at(state.control_point) == overlay;
    bool stable = false;
    if (source_ok && control_ok && state.initial_source && state.initial_control) {
        const auto source_now = capture_frame(source);
        const auto control_now = capture_frame(control);
        MONITORINFO monitor_info{sizeof(monitor_info)};
        const HMONITOR monitor = MonitorFromWindow(source, MONITOR_DEFAULTTONULL);
        stable = source_now && control_now &&
            source_now->visible.size() == state.initial_source->visible.size() &&
            source_now->positioning.size() == state.initial_source->positioning.size() &&
            control_now->positioning == state.initial_control->positioning &&
            control_now->visible == state.initial_control->visible &&
            monitor && monitor == state.initial_monitor &&
            GetMonitorInfoW(monitor, &monitor_info) &&
            EqualRect(&monitor_info.rcWork, &state.initial_work_area) &&
             GetDpiForWindow(source) == state.initial_dpi &&
             !IsIconic(source) && !IsZoomed(source) &&
             IsWindowVisible(control) && !IsIconic(control) && !IsZoomed(control);
    }
    return {authority, stable, isolation};
}
void retire_live_move(behavior::MoveHandoffEscapeReason reason,
                      const char* source) noexcept {
    state.writer_armed.store(false, std::memory_order_release);
    state.retired = true;
    operations::MoveRetireFacts facts{};
    if (live_writer)
        facts = live_writer->retire(static_cast<std::uint64_t>(state.run_nonce));
    if (state.run_nonce)
        (void)move_session.escape(static_cast<std::uint64_t>(state.run_nonce), reason);
    log("owned_move_retire", ",\"source\":\"" + std::string(source) +
        "\",\"pending_discarded\":" + json_bool(facts.pending_discarded) +
        ",\"dispatch_reserved\":" + json_bool(facts.dispatch_reserved) +
        ",\"attempt_started_before_revoke\":" +
        json_bool(facts.attempt_started_before_revoke) +
        ",\"reserved_quantum\":" + std::to_string(facts.reserved_quantum));
}
std::optional<operations::MoveFrameGeometry> capture_owned_source() noexcept {
    const HWND source = state.source.load();
    if (!same_source(source) || !interactive_default_desktop()) return std::nullopt;
    auto result = capture_frame(source);
    return same_source(source) ? result : std::nullopt;
}
bool same_frame(const operations::MoveFrameGeometry& a,
                const operations::MoveFrameGeometry& b) noexcept {
    return a.positioning == b.positioning && a.visible == b.visible;
}
bool owned_target_in_work_area(const geometry::Rect& target) noexcept {
    const RECT& work = state.initial_work_area;
    return target.left() >= work.left && target.top() >= work.top &&
        target.right() <= work.right && target.bottom() <= work.bottom;
}
operations::MoveWriteCallbacks owned_move_callbacks() {
    const auto generation = static_cast<std::uint64_t>(state.run_nonce);
    return {
        [] { return capture_owned_source(); },
        [](const operations::MoveFrameGeometry& before) {
            POINT cursor{};
            if (!GetCursorPos(&cursor)) return false;
            const auto facts = owned_write_facts(cursor);
            const auto source_now = capture_owned_source();
            return facts.fresh_authority && facts.actual_stable && facts.isolation_hits &&
                source_now && same_frame(*source_now, before);
        },
        [generation](const operations::MoveFrameGeometry& before,
                     const geometry::Rect& target,
                     const geometry::Rect& expected_positioning) {
            POINT cursor{};
            if (!owned_target_in_work_area(target))
                return operations::MoveNativePlacement{};
            if (state.inject_writer_stall) {
                // Guest-only fault injection inside the actual shared writer
                // callback, before its final native-call permission check.
                // This is not a claim that SetWindowPos itself was blocked.
                state.writer_stall_held_at_start = left_down();
                log("owned_writer_stall_begin", ",\"left_high\":" + json_bool(left_down()) +
                    ",\"overlay_present\":" + json_bool(same_overlay(state.overlay)));
                SetEvent(state.writer_stall_started);
                Sleep(state.deadline_ms + 500);
                log("owned_writer_stall_end", ",\"overlay_gone\":" + json_bool(
                    WaitForSingleObject(state.overlay_gone, 0) == WAIT_OBJECT_0) +
                    ",\"left_high\":" + json_bool(left_down()));
            }
            if (!GetCursorPos(&cursor)) return operations::MoveNativePlacement{};
            const auto facts = owned_write_facts(cursor);
            const auto source_now = capture_owned_source();
            if (!facts.fresh_authority || !facts.actual_stable || !facts.isolation_hits ||
                !source_now || !same_frame(*source_now, before) ||
                !live_writer || move_session.retired(generation))
                return operations::MoveNativePlacement{};
            if (expected_positioning.left() < INT_MIN ||
                expected_positioning.left() > INT_MAX ||
                expected_positioning.top() < INT_MIN ||
                expected_positioning.top() > INT_MAX)
                return operations::MoveNativePlacement{};
            const HWND source = state.source.load();
            // The shared writer has already bridged visible -> positioning
            // once. This owned capability callback issues exactly one native
            // Move-only placement; it never grants an arbitrary HWND route.
            SetLastError(0);
            if (!live_writer->begin_native_attempt(generation))
                return operations::MoveNativePlacement{};
            const bool applied = SetWindowPos(source, nullptr,
                static_cast<int>(expected_positioning.left()),
                static_cast<int>(expected_positioning.top()), 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
            const DWORD error = applied ? 0 : GetLastError();
            log("owned_move_native_place", ",\"target_visible\":" + rect(target) +
                ",\"expected_positioning\":" + rect(expected_positioning) +
                ",\"succeeded\":" + json_bool(applied) +
                ",\"error\":" + std::to_string(error));
            return operations::MoveNativePlacement{true, applied, true};
        },
        [](const operations::MoveFrameGeometry&,
           const operations::MoveFrameGeometry& after) {
            POINT cursor{};
            if (!GetCursorPos(&cursor)) return false;
            const auto facts = owned_write_facts(cursor);
            const auto source_now = capture_owned_source();
            return facts.fresh_authority && facts.actual_stable && facts.isolation_hits &&
                source_now && same_frame(*source_now, after);
        }
    };
}
void writer_owner() noexcept {
    try {
        for (;;) {
            const auto receipt = live_writer->wait_and_execute();
            if (!receipt) break;
            ++state.writer_receipts;
            if (receipt->native_attempted) ++state.writer_native_attempts;
            if (receipt->exact) ++state.writer_exact;
            const bool snap_receipt = receipt->generation == state.run_nonce &&
                receipt->snapped && receipt->exact && receipt->native_attempted &&
                receipt->post_context_exact && receipt->after &&
                receipt->after->visible == receipt->target_visible;
            if (snap_receipt) {
                {
                    std::lock_guard lock{state.receipt_mutex};
                    state.snapped_exact_receipt = *receipt;
                }
                state.writer_snapped_exact_quantum = receipt->quantum;
                state.writer_snapped_exact = true;
            }
            const bool benign_no_write = receipt->reason == "superseded_before_native" ||
                receipt->reason == "retired_before_native";
            const behavior::MoveWritePlan plan{receipt->generation, receipt->quantum,
                {}, receipt->target_visible, false, magnet::MotionState::BelowThreshold,
                receipt->reason};
            behavior::MoveWriteCompletion completion{};
            if (receipt->exact || receipt->native_attempted)
                completion = move_session.write_result(plan, receipt->exact,
                    receipt->after ? receipt->after->visible : geometry::Rect{});
            log("owned_move_write_receipt", ",\"generation\":" +
                std::to_string(receipt->generation) + ",\"quantum\":" +
                std::to_string(receipt->quantum) + ",\"target_visible\":" +
                rect(receipt->target_visible) + ",\"dispatch_reserved\":" +
                json_bool(receipt->dispatch_reserved) + ",\"native_attempted\":" +
                json_bool(receipt->native_attempted) + ",\"native_succeeded\":" +
                json_bool(receipt->native_succeeded) + ",\"exact\":" +
                json_bool(receipt->exact) + ",\"geometry_exact\":" +
                json_bool(receipt->geometry_exact) + ",\"snapped\":" +
                json_bool(receipt->snapped) + ",\"snap_receipt\":" +
                json_bool(snap_receipt) + ",\"post_context_exact\":" +
                json_bool(receipt->post_context_exact) + ",\"attempt_start_order\":" +
                std::to_string(static_cast<int>(receipt->attempt_start_order)) +
                ",\"retired_during_dispatch\":" +
                json_bool(receipt->retired_during_dispatch) + ",\"completion_recognized\":" +
                json_bool(completion.recognized) + ",\"completion_after_retirement\":" +
                json_bool(completion.after_retirement) + ",\"reason\":\"" +
                std::string(receipt->reason) + "\"");
            SetEvent(state.writer_receipt);
            if (!receipt->exact && !benign_no_write) {
                ++state.writer_failures;
                retire_live_move(behavior::MoveHandoffEscapeReason::NativeFailure,
                                 "writer_failure");
                SetEvent(state.stop);
            }
        }
    } catch (...) {
        ++state.writer_failures;
        retire_live_move(behavior::MoveHandoffEscapeReason::NativeFailure,
                         "writer_exception");
        SetEvent(state.stop);
    }
    SetEvent(state.writer_gone);
}
bool sandbox_run_authorized(std::wstring_view run_id, std::wstring_view scenario,
                            const wchar_t* evidence_log) {
    // This is an accidental-host-run guard, not a security boundary against a
    // deliberately forged guest. The host launches this exact read-only input
    // package into a fresh Windows Sandbox interactive WDAGUtilityAccount.
    if (run_id.size() != 32 || !std::all_of(run_id.begin(), run_id.end(), [](wchar_t ch) {
            return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f');
        })) return false;
    wchar_t user[256]{};
    DWORD user_length = static_cast<DWORD>(std::size(user));
    if (!GetUserNameW(user, &user_length) || std::wstring_view(user) != L"WDAGUtilityAccount")
        return false;
    DWORD session{};
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session) || session == 0 ||
        !interactive_default_desktop()) return false;
    wchar_t executable[MAX_PATH]{};
    const DWORD executable_length = GetModuleFileNameW(nullptr, executable,
        static_cast<DWORD>(std::size(executable)));
    if (executable_length == 0 || executable_length >= std::size(executable) ||
        CompareStringOrdinal(executable, -1, guest_probe_path, -1, TRUE) != CSTR_EQUAL)
        return false;
    const std::wstring expected_log = std::wstring(guest_output_path) +
        std::wstring(run_id) + L"-" + std::wstring(scenario) + L".jsonl";
    if (CompareStringOrdinal(evidence_log, -1, expected_log.c_str(), -1, TRUE) != CSTR_EQUAL)
        return false;
    const HANDLE marker = CreateFileW(guest_run_id_path, GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (marker == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    std::array<char, 32> bytes{};
    DWORD read{};
    const bool valid = GetFileSizeEx(marker, &size) &&
        size.QuadPart == static_cast<LONGLONG>(bytes.size()) &&
        ReadFile(marker, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) &&
        read == static_cast<DWORD>(bytes.size()) &&
        std::equal(bytes.begin(), bytes.end(), run_id.begin(), [](char lhs, wchar_t rhs) {
            return static_cast<unsigned char>(lhs) == rhs;
        });
    CloseHandle(marker);
    return valid;
}
bool virtual_desktop(RECT& result) noexcept {
    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN), y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN), height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    const auto right = static_cast<std::int64_t>(x) + width;
    const auto bottom = static_cast<std::int64_t>(y) + height;
    if (width <= 0 || height <= 0 || right > INT_MAX || bottom > INT_MAX) return false;
    result = RECT{x, y, static_cast<LONG>(right), static_cast<LONG>(bottom)};
    return true;
}
struct MonitorHits {
    HWND overlay{};
    std::uint32_t count{};
    std::uint32_t hit{};
    bool overflow{};
};
BOOL CALLBACK monitor_hit_callback(HMONITOR, HDC, LPRECT bounds, LPARAM context) noexcept {
    auto& facts = *reinterpret_cast<MonitorHits*>(context);
    if (++facts.count > 16) { facts.overflow = true; return FALSE; }
    const POINT center{bounds->left + (bounds->right - bounds->left) / 2,
                       bounds->top + (bounds->bottom - bounds->top) / 2};
    if (root_at(center) == facts.overlay) ++facts.hit;
    return TRUE;
}
bool raw_registration(HWND receiver) noexcept {
    RAWINPUTDEVICE device{0x01, 0x02, RIDEV_INPUTSINK, receiver};
    if (!RegisterRawInputDevices(&device, 1, sizeof(device))) return false;
    UINT count{};
    if (GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE)) != 0 || count == 0)
        return false;
    std::vector<RAWINPUTDEVICE> registrations(count);
    if (GetRegisteredRawInputDevices(registrations.data(), &count, sizeof(RAWINPUTDEVICE)) == UINT_MAX)
        return false;
    return std::any_of(registrations.begin(), registrations.end(), [receiver](const auto& item) {
        return item.usUsagePage == 0x01 && item.usUsage == 0x02 &&
            item.dwFlags == RIDEV_INPUTSINK && item.hwndTarget == receiver;
    });
}
bool remove_raw_registration() noexcept {
    RAWINPUTDEVICE device{0x01, 0x02, RIDEV_REMOVE, nullptr};
    const bool removed = RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
    UINT count{};
    bool absent = GetRegisteredRawInputDevices(nullptr, &count,
        sizeof(RAWINPUTDEVICE)) == 0;
    if (absent && count) {
        std::vector<RAWINPUTDEVICE> registrations(count);
        absent = GetRegisteredRawInputDevices(registrations.data(), &count,
            sizeof(RAWINPUTDEVICE)) != UINT_MAX &&
            std::none_of(registrations.begin(), registrations.end(), [](const auto& item) {
                return item.usUsagePage == 0x01 && item.usUsage == 0x02;
            });
    }
    state.registration_removed = removed && absent;
    log("raw_registration_removed", ",\"api_success\":" + json_bool(removed) +
        ",\"readback_absent\":" + json_bool(absent));
    return state.registration_removed;
}

LRESULT CALLBACK source_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    try {
        switch (message) {
        case WM_NCLBUTTONDOWN:
            if (wparam == HTCAPTION) {
                ++state.native_downs;
                state.original_down_qpc = qpc();
                state.tagged_native_down = GetMessageExtraInfo() == test_input_tag;
                const POINT down{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                const auto down_frame = capture_frame(window);
                {
                    std::lock_guard lock{state.down_mutex};
                    state.actual_down_point = down;
                    state.actual_down_frame = down_frame;
                }
                log("owned_nclbuttondown", ",\"hwnd\":" + std::to_string(number(window)) +
                    ",\"hit\":" + std::to_string(wparam) + ",\"point\":" + point(down) +
                    ",\"frame_captured\":" + json_bool(down_frame.has_value()) +
                    ",\"left_high\":" + json_bool(left_down()) +
                    ",\"test_tag\":" + json_bool(state.tagged_native_down));
                const LRESULT native = DefWindowProcW(window, message, wparam, lparam);
                log("owned_native_loop_return", ",\"native_end_count\":" +
                    std::to_string(state.native_ends.load()));
                SetEvent(state.native_loop_return);
                return native;
            }
            break;
        case WM_ENTERSIZEMOVE: {
            ++state.native_starts;
            GUITHREADINFO gui{sizeof(gui)};
            const bool queried = GetGUIThreadInfo(state.source_tid, &gui) != FALSE;
            log("owned_native_start", ",\"hwnd\":" + std::to_string(number(window)) +
                ",\"left_high\":" + json_bool(left_down()) +
                ",\"gui_queried\":" + json_bool(queried) +
                ",\"gui_capture\":" + std::to_string(number(gui.hwndCapture)) +
                ",\"gui_move_size\":" + std::to_string(number(gui.hwndMoveSize)));
            SetEvent(state.native_start);
            break;
        }
        case WM_CANCELMODE:
            log("owned_cancel_received", ",\"source\":" + std::to_string(number(window)) +
                ",\"overlay_exists\":" + json_bool(same_overlay(state.overlay)) +
                ",\"overlay_ready_qpc\":" + std::to_string(state.overlay_ready_qpc.load()));
            break;
        case WM_EXITSIZEMOVE:
            ++state.native_ends;
            log("owned_native_end", ",\"hwnd\":" + std::to_string(number(window)));
            SetEvent(state.native_end);
            break;
        case WM_MOUSEMOVE: case WM_LBUTTONDOWN: case WM_LBUTTONUP:
            log("source_legacy_mouse", ",\"message\":" + std::to_string(message));
            break;
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default: break;
        }
    } catch (...) { state.log_ok = false; }
    return DefWindowProcW(window, message, wparam, lparam);
}
LRESULT CALLBACK control_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    if (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP) {
        ++state.control_mouse;
        log("control_legacy_mouse", ",\"message\":" + std::to_string(message));
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
LRESULT CALLBACK overlay_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    switch (message) {
    case WM_NCHITTEST: return HTCLIENT;
    case WM_MOUSEACTIVATE:
        log("overlay_mouseactivate", ",\"result\":3");
        return MA_NOACTIVATE;
    case WM_MOUSEMOVE: case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        ++state.overlay_mouse;
        if (message == WM_LBUTTONUP) {
            ++state.overlay_ups;
            const bool accepted = move_session.legacy_up_delivery_observed(
                static_cast<std::uint64_t>(state.run_nonce),
                message == WM_LBUTTONUP && same_overlay(window));
            log("owned_legacy_up_delivery", ",\"core_accepted\":" + json_bool(accepted));
            SetEvent(state.legacy_up);
            if (accepted && move_session.removal_allowed(
                    static_cast<std::uint64_t>(state.run_nonce))) {
                state.normal_removal_triggered = true;
                log("owned_normal_removal_ready", ",\"last_observation\":\"legacy_up\"");
                SetEvent(state.stop);
            }
        }
        log("overlay_legacy_mouse", ",\"message\":" + std::to_string(message) +
            ",\"foreground\":" + std::to_string(number(GetForegroundWindow())));
        return 0;
    case WM_ACTIVATE: case WM_SETFOCUS:
        if (message == WM_SETFOCUS || LOWORD(wparam) != WA_INACTIVE) {
            state.overlay_ok = false;
            log("overlay_unexpected_activation", ",\"message\":" + std::to_string(message));
            retire_live_move(behavior::MoveHandoffEscapeReason::ContextLost,
                             "overlay_activation");
            SetEvent(state.stop);
        }
        break;
    default: break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
LRESULT CALLBACK receiver_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    if (message == WM_INPUT) {
        UINT size{};
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, nullptr, &size,
                            sizeof(RAWINPUTHEADER)) == 0 && size >= sizeof(RAWINPUT) && size <= 4096) {
            std::vector<std::byte> buffer(size);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, buffer.data(), &size,
                                sizeof(RAWINPUTHEADER)) == size) {
                const auto* input = reinterpret_cast<const RAWINPUT*>(buffer.data());
                if (input->header.dwType == RIM_TYPEMOUSE) {
                    const auto packet = ++state.raw_packet_watermark;
                    const auto flags = input->data.mouse.usButtonFlags;
                    if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) {
                        ++state.raw_downs;
                        const bool candidate = state.test_down_issue_qpc != 0 &&
                            packet > state.test_down_watermark &&
                            qpc() >= state.test_down_issue_qpc &&
                            received_msg_point.x == state.source_down.x &&
                            received_msg_point.y == state.source_down.y;
                        if (candidate && state.candidate_raw_down == 0) {
                            state.candidate_raw_down = packet;
                            SetEvent(state.raw_down);
                        }
                        log("raw_left_down", ",\"button_flags\":" + std::to_string(flags) +
                            ",\"packet\":" + std::to_string(packet) +
                            ",\"test_down_watermark\":" + std::to_string(state.test_down_watermark.load()) +
                            ",\"candidate_for_test_down\":" + json_bool(candidate) +
                            ",\"msg_point\":" + point(received_msg_point) +
                            ",\"msg_time\":" + std::to_string(received_msg_time) +
                            ",\"cursor_now\":" + [&] { POINT p{}; GetCursorPos(&p); return point(p); }());
                    }
                    if (flags & RI_MOUSE_LEFT_BUTTON_UP) {
                        std::lock_guard cancel_lock{state.cancel_mutex};
                        ++state.raw_ups;
                        state.retired = true;
                        state.writer_armed.store(false, std::memory_order_release);
                        operations::MoveRetireFacts retired_writer{};
                        if (live_writer) retired_writer = live_writer->retire(
                            static_cast<std::uint64_t>(state.run_nonce));
                        const bool associated = state.candidate_raw_down != 0 &&
                            packet > state.candidate_raw_down &&
                            state.raw_ups == 1;
                        if (associated) state.candidate_raw_up = packet;
                        const bool core_up = move_session.raw_up_observed(
                            static_cast<std::uint64_t>(state.run_nonce), associated);
                        state.core_raw_up_accepted = core_up;
                        log("raw_left_up", ",\"button_flags\":" + std::to_string(flags) +
                            ",\"packet\":" + std::to_string(packet) +
                            ",\"associated_with_candidate_down\":" + json_bool(associated) +
                            ",\"writer_retired\":" + json_bool(retired_writer.retired_now) +
                            ",\"pending_discarded\":" + json_bool(retired_writer.pending_discarded) +
                            ",\"dispatch_reserved\":" + json_bool(retired_writer.dispatch_reserved) +
                            ",\"attempt_started_before_revoke\":" +
                            json_bool(retired_writer.attempt_started_before_revoke) +
                            ",\"reserved_quantum\":" +
                            std::to_string(retired_writer.reserved_quantum) +
                            ",\"core_up_accepted\":" + json_bool(core_up));
                        SetEvent(state.raw_up);
                        if (core_up && move_session.removal_allowed(
                                static_cast<std::uint64_t>(state.run_nonce))) {
                            state.normal_removal_triggered = true;
                            log("owned_normal_removal_ready", ",\"last_observation\":\"raw_up\"");
                            SetEvent(state.stop);
                        } else if (!associated || !core_up) {
                            // An unrelated/invalid UP is not normal delivery
                            // proof. Recover the desktop immediately instead
                            // of leaving the opaque shield until its deadline.
                            retire_live_move(behavior::MoveHandoffEscapeReason::ContextLost,
                                             "unmatched_raw_up");
                            SetEvent(state.stop);
                        }
                    }
                    if (state.writer_armed.load(std::memory_order_acquire) &&
                        !state.retired.load(std::memory_order_acquire) &&
                        (input->data.mouse.lLastX != 0 || input->data.mouse.lLastY != 0)) {
                        POINT cursor{};
                        if (!GetCursorPos(&cursor)) {
                            retire_live_move(behavior::MoveHandoffEscapeReason::ContextLost,
                                             "raw_cursor_read_failed");
                            SetEvent(state.stop);
                        } else {
                            const auto facts = owned_write_facts(cursor);
                            const auto generation = static_cast<std::uint64_t>(state.run_nonce);
                            const bool sampled = move_session.sample_cursor(generation,
                                {cursor.x, cursor.y}, qpc(), facts.fresh_authority,
                                facts.actual_stable, facts.isolation_hits);
                            const auto plan = sampled ? move_session.take_pending(generation,
                                facts.fresh_authority, facts.actual_stable,
                                facts.isolation_hits) : std::nullopt;
                            if (plan) {
                                const bool offered = live_writer && live_writer->offer(
                                    {plan->generation, plan->quantum, plan->target_visible,
                                     plan->snapped});
                                log("owned_move_offer", ",\"packet\":" +
                                    std::to_string(packet) + ",\"quantum\":" +
                                    std::to_string(plan->quantum) + ",\"msg_point\":" +
                                    point(received_msg_point) + ",\"cursor\":" +
                                    point(cursor) + ",\"free_visible\":" +
                                    rect(plan->free_visible) + ",\"target_visible\":" +
                                    rect(plan->target_visible) + ",\"snapped\":" +
                                    json_bool(plan->snapped) + ",\"offered\":" +
                                    json_bool(offered));
                                if (offered) {
                                    ++state.writer_offers;
                                    if (plan->snapped) ++state.writer_snapped_offers;
                                }
                                else if (!move_session.retired(generation)) {
                                    retire_live_move(behavior::MoveHandoffEscapeReason::NativeFailure,
                                                     "writer_offer_rejected");
                                    SetEvent(state.stop);
                                }
                            } else if (move_session.retired(generation)) {
                                retire_live_move(behavior::MoveHandoffEscapeReason::ContextLost,
                                                 "cursor_plan_retired");
                                SetEvent(state.stop);
                            }
                        }
                    }
                }
            }
        }
        if (GET_RAWINPUT_CODE_WPARAM(wparam) == RIM_INPUT)
            return DefWindowProcW(window, message, wparam, lparam);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

BOOL WINAPI console_control(DWORD control) noexcept {
    if (control != CTRL_C_EVENT && control != CTRL_BREAK_EVENT && control != CTRL_CLOSE_EVENT)
        return FALSE;
    retire_live_move(behavior::MoveHandoffEscapeReason::ExplicitStop,
                     "console_control");
    if (state.stop) SetEvent(state.stop);
    log("keyboard_console_stop", ",\"control\":" + std::to_string(control));
    return TRUE;
}
void ui_owner() noexcept {
    state.source_tid = GetCurrentThreadId();
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW source_class{};
    source_class.hInstance = instance;
    source_class.lpfnWndProc = source_procedure;
    source_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    source_class.lpszClassName = L"PaneBindMVP1OwnedOverlaySource";
    WNDCLASSW control_class = source_class;
    control_class.lpfnWndProc = control_procedure;
    control_class.lpszClassName = L"PaneBindMVP1OwnedOverlayControl";
    RECT work{};
    const bool work_ok = SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0) != FALSE;
    const bool classes = RegisterClassW(&source_class) && RegisterClassW(&control_class);
    if (!work_ok || !classes || work.right - work.left < 1040 || work.bottom - work.top < 600) {
        log("owned_setup_failed", ",\"work\":" + rect(work));
        SetEvent(state.ui_ready);
        SetEvent(state.ui_gone);
        return;
    }
    const int x = work.left + 90, y = work.top + 110;
    const HWND source = CreateWindowExW(0, source_class.lpszClassName,
        L"PaneBind owned overlay test source", WS_OVERLAPPEDWINDOW, x, y, 430, 280,
        nullptr, nullptr, instance, nullptr);
    const HWND control = CreateWindowExW(0, control_class.lpszClassName,
        L"PaneBind owned overlay test control", WS_OVERLAPPEDWINDOW,
        x + 510, y + 30, 260, 240, nullptr, nullptr, instance, nullptr);
    state.source = source;
    state.control = control;
    if (!source || !control) {
        log("owned_setup_failed", ",\"window_create\":false");
        if (source) DestroyWindow(source);
        if (control) DestroyWindow(control);
        SetEvent(state.ui_ready);
        SetEvent(state.ui_gone);
        return;
    }
    SetLastError(0);
    SetWindowLongPtrW(source, GWLP_USERDATA, static_cast<LONG_PTR>(state.run_nonce));
    const DWORD source_nonce_error = GetLastError();
    SetLastError(0);
    SetWindowLongPtrW(control, GWLP_USERDATA, static_cast<LONG_PTR>(state.run_nonce));
    if (source_nonce_error || GetLastError() || !same_source(source) || !same_control(control)) {
        log("owned_setup_failed", ",\"window_nonce\":false");
        DestroyWindow(control);
        DestroyWindow(source);
        SetEvent(state.ui_ready);
        SetEvent(state.ui_gone);
        return;
    }
    state.source_down = POINT{x + 155, y + 16};
    state.control_point = POINT{x + 620, y + 135};
    ShowWindow(control, SW_SHOWNOACTIVATE);
    ShowWindow(source, SW_SHOW);
    state.initial_foreground = GetForegroundWindow();
    GUITHREADINFO info{sizeof(info)};
    if (GetGUIThreadInfo(state.source_tid, &info)) {
        state.initial_focus = info.hwndFocus;
        state.initial_capture = info.hwndCapture;
    }
    log("owned_ready", ",\"source\":" + std::to_string(number(source)) +
        ",\"control\":" + std::to_string(number(control)) +
        ",\"down\":" + point(state.source_down) +
        ",\"control_point\":" + point(state.control_point));
    SetEvent(state.ui_ready);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (IsWindow(control)) DestroyWindow(control);
    if (IsWindow(source)) DestroyWindow(source);
    SetEvent(state.ui_gone);
}

bool create_overlay() noexcept {
    if (state.inject_overlay_failure) {
        log("overlay_setup_injected_failure");
        return false;
    }
    RECT virtual_rect{};
    if (!virtual_desktop(virtual_rect)) { log("overlay_virtual_rect_invalid"); return false; }
    state.virtual_rect = virtual_rect;
    const auto instance = GetModuleHandleW(nullptr);
    const HWND overlay = CreateWindowExW(WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        L"PaneBindMVP1OwnedOverlay", L"PaneBind owned gesture input isolation",
        WS_POPUP, virtual_rect.left, virtual_rect.top,
        virtual_rect.right - virtual_rect.left, virtual_rect.bottom - virtual_rect.top,
        nullptr, nullptr, instance, nullptr);
    state.overlay = overlay;
    state.overlay_created = overlay != nullptr;
    if (overlay) {
        SetLastError(0);
        SetWindowLongPtrW(overlay, GWLP_USERDATA, static_cast<LONG_PTR>(state.run_nonce));
        if (GetLastError()) { log("overlay_nonce_failed"); return false; }
    }
    const bool alpha = overlay && SetLayeredWindowAttributes(overlay, 0, overlay_alpha, LWA_ALPHA);
    const HWND foreground_before = GetForegroundWindow();
    const bool placed = alpha && SetWindowPos(overlay, HWND_TOPMOST,
        virtual_rect.left, virtual_rect.top, virtual_rect.right - virtual_rect.left,
        virtual_rect.bottom - virtual_rect.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    RECT actual{};
    const bool rect_ok = placed && GetWindowRect(overlay, &actual) && EqualRect(&actual, &virtual_rect);
    const auto exstyle = overlay ? GetWindowLongPtrW(overlay, GWL_EXSTYLE) : 0;
    const bool style_ok = (exstyle & (WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST)) ==
        (WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST) && !(exstyle & WS_EX_TRANSPARENT);
    BYTE actual_alpha{};
    DWORD alpha_flags{};
    const bool alpha_ok = overlay && GetLayeredWindowAttributes(overlay, nullptr,
        &actual_alpha, &alpha_flags) && actual_alpha == overlay_alpha &&
        alpha_flags == LWA_ALPHA;
    const HWND source = state.source.load();
    const bool focus_ok = GetForegroundWindow() == foreground_before && foreground_before == source;
    const bool hit = root_at(state.control_point) == overlay;
    MonitorHits monitors{overlay};
    const bool enumerated = EnumDisplayMonitors(nullptr, nullptr, monitor_hit_callback,
        reinterpret_cast<LPARAM>(&monitors)) != FALSE;
    const bool monitor_hits = enumerated && !monitors.overflow && monitors.count > 0 &&
        monitors.count == monitors.hit;
    const bool exact = same_overlay(overlay);
    const bool result = rect_ok && style_ok && alpha_ok && focus_ok && hit && monitor_hits &&
        exact && IsWindowVisible(overlay);
    state.overlay_ok = result;
    state.overlay_ready_qpc = result ? qpc() : 0;
    log("overlay_setup", ",\"requested_rect\":" + rect(virtual_rect) +
        ",\"actual_rect\":" + rect(actual) +
        ",\"alpha\":" + std::to_string(overlay_alpha) +
        ",\"actual_alpha\":" + std::to_string(actual_alpha) +
        ",\"alpha_ok\":" + json_bool(alpha_ok) +
        ",\"overlay\":" + std::to_string(number(overlay)) +
        ",\"style_ok\":" + json_bool(style_ok) +
        ",\"foreground_before\":" + std::to_string(number(foreground_before)) +
        ",\"foreground_after\":" + std::to_string(number(GetForegroundWindow())) +
        ",\"control_point_hit\":" + std::to_string(number(root_at(state.control_point))) +
        ",\"actual_hit\":" + json_bool(hit) +
        ",\"monitor_count\":" + std::to_string(monitors.count) +
        ",\"monitor_center_hits\":" + std::to_string(monitors.hit) +
        ",\"monitor_sample_ready\":" + json_bool(monitor_hits) +
        ",\"ready\":" + json_bool(result));
    return result;
}
void overlay_owner() noexcept {
    state.overlay_tid = GetCurrentThreadId();
    MSG prime{};
    PeekMessageW(&prime, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW overlay_class{};
    overlay_class.hInstance = instance;
    overlay_class.lpfnWndProc = overlay_procedure;
    overlay_class.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    overlay_class.lpszClassName = L"PaneBindMVP1OwnedOverlay";
    WNDCLASSW receiver_class = overlay_class;
    receiver_class.lpfnWndProc = receiver_procedure;
    receiver_class.lpszClassName = L"PaneBindMVP1OwnedRawReceiver";
    if (!RegisterClassW(&overlay_class) || !RegisterClassW(&receiver_class)) {
        log("receiver_setup_failed", ",\"class_registration\":false");
        SetEvent(state.receiver_ready);
        SetEvent(state.overlay_gone);
        SetEvent(state.receiver_gone);
        return;
    }
    const HWND receiver = CreateWindowExW(0, receiver_class.lpszClassName, L"", 0,
        0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
    state.receiver = receiver;
    state.receiver_created = receiver != nullptr;
    state.receiver_ok = receiver && raw_registration(receiver);
    log("receiver_setup", ",\"receiver\":" + std::to_string(number(receiver)) +
        ",\"registered\":" + json_bool(state.receiver_ok));
    SetEvent(state.receiver_ready);
    if (!state.receiver_ok) {
        state.registration_removed = remove_raw_registration();
        state.receiver_destroyed = !receiver || (DestroyWindow(receiver) && !IsWindow(receiver));
        state.overlay_destroyed = true;
        state.hotkey_removed = true;
        SetEvent(state.overlay_gone);
        SetEvent(state.receiver_gone);
        return;
    }

    bool created = false;
    ULONGLONG end_tick = 0;
    const HANDLE events[]{state.stop};
    for (;;) {
        const DWORD remaining = created ? static_cast<DWORD>(std::min<ULONGLONG>(
            state.deadline_ms, end_tick > GetTickCount64() ? end_tick - GetTickCount64() : 0)) : INFINITE;
        const DWORD wait = MsgWaitForMultipleObjectsEx(1, events, remaining, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (wait == WAIT_OBJECT_0) { log("overlay_stop_event"); break; }
        if (wait == WAIT_TIMEOUT) {
            retire_live_move(behavior::MoveHandoffEscapeReason::Deadline,
                             "overlay_deadline");
            log("overlay_deadline", ",\"max_ms\":" + std::to_string(state.deadline_ms));
            break;
        }
        if (wait != WAIT_OBJECT_0 + 1) {
            retire_live_move(behavior::MoveHandoffEscapeReason::ContextLost,
                             "overlay_wait_failed");
            log("overlay_wait_failed", ",\"result\":" + std::to_string(wait));
            break;
        }
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) { SetEvent(state.stop); break; }
            if (message.message == create_overlay_message && !created) {
                const bool ready = create_overlay();
                created = true;
                end_tick = GetTickCount64() + state.deadline_ms;
                const bool hotkey = ready && RegisterHotKey(nullptr, stop_hotkey_id,
                    stop_hotkey_modifiers, stop_hotkey_key);
                state.hotkey_registered = hotkey;
                if (!hotkey && ready) {
                    log("overlay_hotkey_failed", ",\"error\":" + std::to_string(GetLastError()));
                    state.overlay_ok = false;
                }
                log("overlay_hotkey", ",\"chord\":\"Ctrl+Shift+F11\",\"registered\":" +
                    json_bool(hotkey));
                SetEvent(state.overlay_ready);
            } else if (message.message == WM_HOTKEY && message.wParam == stop_hotkey_id) {
                const bool matches = state.hotkey_registered &&
                    HIWORD(message.lParam) == stop_hotkey_key &&
                    (LOWORD(message.lParam) & (MOD_CONTROL | MOD_SHIFT | MOD_ALT | MOD_WIN)) ==
                        (MOD_CONTROL | MOD_SHIFT);
                log("overlay_hotkey_message", ",\"chord\":\"Ctrl+Shift+F11\",\"matches\":" +
                    json_bool(matches) + ",\"key\":" + std::to_string(HIWORD(message.lParam)) +
                    ",\"modifiers\":" + std::to_string(LOWORD(message.lParam)));
                if (matches) {
                    GUITHREADINFO gui{sizeof(gui)};
                    const HWND source = state.source.load();
                    const bool gui_ok = GetGUIThreadInfo(state.source_tid, &gui) != FALSE;
                    const bool held_native = left_down() && same_source(source) &&
                        same_overlay(state.overlay.load()) && state.native_starts == 1 &&
                        state.native_ends == 0 && gui_ok &&
                        gui.hwndCapture == source && gui.hwndMoveSize == source;
                    state.hotkey_received = true;
                    state.hotkey_during_held_native = held_native;
                    retire_live_move(behavior::MoveHandoffEscapeReason::ExplicitStop,
                                     "keyboard_hotkey");
                    log("overlay_keyboard_stop", ",\"hotkey\":\"Ctrl+Shift+F11\",\"via_wm_hotkey\":true" +
                        std::string(",\"left_high\":") + json_bool(left_down()) +
                        ",\"native_ends\":" + std::to_string(state.native_ends.load()) +
                        ",\"capture\":" + std::to_string(number(gui.hwndCapture)) +
                        ",\"move_size\":" + std::to_string(number(gui.hwndMoveSize)) +
                        ",\"during_held_native\":" + json_bool(held_native));
                    SetEvent(state.stop);
                }
            } else {
                received_msg_point = message.pt;
                received_msg_time = message.time;
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
    }
    state.retired = true;
    const bool hotkey_removed = !state.hotkey_registered ||
        UnregisterHotKey(nullptr, stop_hotkey_id) != FALSE;
    state.hotkey_removed = hotkey_removed;
    log("overlay_hotkey_unregistered", ",\"was_registered\":" +
        json_bool(state.hotkey_registered) + ",\"removed\":" + json_bool(hotkey_removed) +
        ",\"wm_hotkey_received\":" + json_bool(state.hotkey_received));
    const HWND overlay = state.overlay.load();
    bool destroyed = true;
    if (overlay) destroyed = DestroyWindow(overlay) && !IsWindow(overlay);
    state.overlay_destroyed = destroyed;
    if (destroyed) state.overlay = nullptr;
    log("overlay_destroy", ",\"target\":" + std::to_string(number(overlay)) +
        ",\"destroyed\":" + json_bool(destroyed) +
        ",\"hotkey_removed\":" + json_bool(hotkey_removed) +
        ",\"raw_ups\":" + std::to_string(state.raw_ups.load()) +
        ",\"legacy_ups\":" + std::to_string(state.overlay_ups.load()));
    const bool core_removed = destroyed && move_session.isolation_removed(
        static_cast<std::uint64_t>(state.run_nonce));
    log("owned_move_isolation_removed", ",\"actual_destroyed\":" +
        json_bool(destroyed) + ",\"core_accepted\":" + json_bool(core_removed));
    // The screen-covering overlay must disappear promptly. Its message-only
    // Raw receiver is a separate lifetime: a stop can still require the
    // test driver's guarded LEFTUP after overlay_gone. Keep pumping WM_INPUT
    // for one bounded observation window, never keep the overlay for this.
    if (destroyed) SetEvent(state.overlay_gone);
    else log("overlay_destroy_failed");
    const ULONGLONG observation_end = GetTickCount64() + raw_cleanup_observation_ms;
    bool raw_observed = WaitForSingleObject(state.raw_up, 0) == WAIT_OBJECT_0;
    bool observation_error = false;
    while (!raw_observed) {
        const ULONGLONG now = GetTickCount64();
        if (now >= observation_end) break;
        const DWORD remaining = static_cast<DWORD>(observation_end - now);
        const HANDLE observation[]{state.raw_up};
        const DWORD wait = MsgWaitForMultipleObjectsEx(1, observation, remaining,
            QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (wait == WAIT_OBJECT_0) { raw_observed = true; break; }
        if (wait == WAIT_TIMEOUT) break;
        if (wait != WAIT_OBJECT_0 + 1) { observation_error = true; break; }
        MSG message{};
        while (GetTickCount64() < observation_end &&
               PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) { observation_error = true; break; }
            received_msg_point = message.pt;
            received_msg_time = message.time;
            TranslateMessage(&message);
            DispatchMessageW(&message);
            if (WaitForSingleObject(state.raw_up, 0) == WAIT_OBJECT_0) {
                raw_observed = true;
                break;
            }
        }
        if (observation_error) break;
    }
    log("receiver_observation_end", ",\"overlay_gone\":" + json_bool(destroyed) +
        ",\"raw_up_event\":" +
        json_bool(raw_observed) + ",\"timed_out\":" +
        json_bool(!raw_observed && !observation_error) + ",\"wait_error\":" +
        json_bool(observation_error) + ",\"raw_ups\":" +
        std::to_string(state.raw_ups.load()));
    state.registration_removed = remove_raw_registration();
    state.receiver_destroyed = !receiver || (DestroyWindow(receiver) && !IsWindow(receiver));
    log("receiver_destroy", ",\"target\":" + std::to_string(number(receiver)) +
        ",\"destroyed\":" + json_bool(state.receiver_destroyed));
    SetEvent(state.receiver_gone);
}
void overlay_watchdog() noexcept {
    const HANDLE triggers[]{state.overlay_arm, state.stop};
    const DWORD armed = WaitForMultipleObjects(2, triggers, FALSE, INFINITE);
    if (armed != WAIT_OBJECT_0) return;
    // Arm before posting the creation message, not after CreateWindowEx or
    // SetWindowPos returns. A blocked geometry owner cannot postpone this.
    if (WaitForSingleObject(state.overlay_gone, state.deadline_ms) == WAIT_OBJECT_0) return;
    retire_live_move(behavior::MoveHandoffEscapeReason::Deadline,
                     "independent_watchdog_deadline");
    SetEvent(state.stop);
    log("independent_watchdog_deadline", ",\"max_ms\":" + std::to_string(state.deadline_ms));
    if (WaitForSingleObject(state.overlay_gone, 500) == WAIT_OBJECT_0) return;
    // Test-only last resort: destroy all windows owned by this disposable
    // process if its overlay UI thread is trapped inside native creation or
    // dispatch. No product process or foreign window is terminated here.
    TerminateProcess(GetCurrentProcess(), 71);
}

bool send_mouse(DWORD flags, POINT destination) noexcept {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    input.mi.dwExtraInfo = test_input_tag;
    if ((flags & MOUSEEVENTF_ABSOLUTE) != 0) {
        const auto width = state.virtual_rect.right - state.virtual_rect.left;
        const auto height = state.virtual_rect.bottom - state.virtual_rect.top;
        if (width <= 1 || height <= 1) return false;
        input.mi.dx = static_cast<LONG>((static_cast<std::int64_t>(destination.x - state.virtual_rect.left) * 65535) / (width - 1));
        input.mi.dy = static_cast<LONG>((static_cast<std::int64_t>(destination.y - state.virtual_rect.top) * 65535) / (height - 1));
    }
    if (flags & MOUSEEVENTF_LEFTDOWN) {
        state.test_down_watermark = state.raw_packet_watermark.load();
        state.test_down_issue_qpc = qpc();
    }
    const bool sent = SendInput(1, &input, sizeof(input)) == 1;
    log("owned_test_sendinput", ",\"flags\":" + std::to_string(flags) +
        ",\"destination\":" + point(destination) + ",\"sent\":" + json_bool(sent));
    return sent;
}
bool move_cursor(POINT destination) noexcept {
    if (!send_mouse(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK, destination))
        return false;
    // Test driver only: one bounded dispatch allowance, then a single actual
    // cursor readback. Never send DOWN or UP based only on SendInput's return.
    Sleep(30);
    POINT actual{};
    const bool exact = GetCursorPos(&actual) && actual.x == destination.x && actual.y == destination.y;
    log("owned_test_cursor_readback", ",\"requested\":" + point(destination) +
        ",\"actual\":" + point(actual) + ",\"exact\":" + json_bool(exact));
    return exact;
}
bool send_test_stop_hotkey() noexcept {
    // Test driver only. SendInput is not part of the product input path. The
    // scenario passes only after the overlay thread receives the real WM_HOTKEY.
    // A partial batch could leave Ctrl/Shift/F11 down. Always issue a separate
    // test-only key-up batch, including when the first batch fails or succeeds.
    constexpr std::array<WORD, 6> keys{
        VK_CONTROL, VK_SHIFT, VK_F11, VK_F11, VK_SHIFT, VK_CONTROL};
    std::array<INPUT, keys.size()> inputs{};
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        inputs[i].type = INPUT_KEYBOARD;
        inputs[i].ki.wVk = keys[i];
        inputs[i].ki.dwFlags = i >= 3 ? KEYEVENTF_KEYUP : 0;
        inputs[i].ki.dwExtraInfo = test_input_tag;
    }
    const UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    constexpr std::array<WORD, 3> release_keys{VK_F11, VK_SHIFT, VK_CONTROL};
    std::array<INPUT, release_keys.size()> releases{};
    for (std::size_t i = 0; i < releases.size(); ++i) {
        releases[i].type = INPUT_KEYBOARD;
        releases[i].ki.wVk = release_keys[i];
        releases[i].ki.dwFlags = KEYEVENTF_KEYUP;
        releases[i].ki.dwExtraInfo = test_input_tag;
    }
    const UINT cleanup_sent = SendInput(static_cast<UINT>(releases.size()),
        releases.data(), sizeof(INPUT));
    Sleep(30); // One bounded test-driver dispatch allowance, not a polling loop.
    const bool keys_up = (GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0 &&
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0 &&
        (GetAsyncKeyState(VK_F11) & 0x8000) == 0;
    log("owned_test_keyboard_hotkey", ",\"chord\":\"Ctrl+Shift+F11\",\"requested\":" +
        std::to_string(inputs.size()) + ",\"sent\":" + std::to_string(sent) +
        ",\"cleanup_requested\":" + std::to_string(releases.size()) +
        ",\"cleanup_sent\":" + std::to_string(cleanup_sent) +
        ",\"keys_up\":" + json_bool(keys_up));
    return sent == inputs.size() && cleanup_sent == releases.size() && keys_up;
}
bool wait_event(HANDLE event, DWORD ms, std::string_view name) noexcept {
    const bool passed = WaitForSingleObject(event, ms) == WAIT_OBJECT_0;
    log("wait", ",\"for\":\"" + std::string(name) + "\",\"passed\":" + json_bool(passed));
    return passed;
}
bool release_test_owned_left(const char* reason) noexcept {
    if (!state.held && !left_down()) return true;
    const HWND source = state.source.load();
    POINT cursor{};
    GUITHREADINFO gui{sizeof(gui)};
    const HWND foreground_before = GetForegroundWindow();
    const bool context = same_source(source) && interactive_default_desktop() &&
        GetCursorPos(&cursor) && GetGUIThreadInfo(state.source_tid, &gui);
    const HWND root = context ? root_at(cursor) : nullptr;
    const bool own_capture = context && (!gui.hwndCapture || gui.hwndCapture == source);
    const bool own_move = context && (!gui.hwndMoveSize || gui.hwndMoveSize == source);
    const bool clean_gui = context && !gui.hwndMenuOwner &&
        !(gui.flags & (GUI_INMENUMODE | GUI_SYSTEMMENUMODE | GUI_POPUPMENUMODE));
    const bool source_foreground = foreground_before == source;
    const bool normal_route = source_foreground &&
        (root == source || same_overlay(root));
    // A failed test gesture can lose foreground while its own overlay remains
    // hit-test opaque. Only cleanup (never writer authority) may use this
    // stricter foreign-foreground route. GetGUIThreadInfo(0) targets the
    // foreground thread; activation transitions fail closed on unstable HWND.
    GUITHREADINFO foreground_gui{sizeof(foreground_gui)};
    const HWND overlay = state.overlay.load();
    const auto exstyle = same_overlay(overlay) ? GetWindowLongPtrW(overlay, GWL_EXSTYLE) : 0;
    BYTE alpha{};
    DWORD alpha_flags{};
    const bool overlay_opaque_to_hit = same_overlay(overlay) && root == overlay &&
        IsWindowVisible(overlay) &&
        (exstyle & (WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST)) ==
            (WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST) &&
        !(exstyle & WS_EX_TRANSPARENT) &&
        GetLayeredWindowAttributes(overlay, nullptr, &alpha, &alpha_flags) &&
        alpha == overlay_alpha && alpha_flags == LWA_ALPHA;
    const bool foreign_gui_clear = context && foreground_before &&
        GetGUIThreadInfo(0, &foreground_gui) &&
        foreground_gui.hwndActive == foreground_before &&
        !foreground_gui.hwndCapture &&
        !foreground_gui.hwndMoveSize && !foreground_gui.hwndMenuOwner &&
        !(foreground_gui.flags & (GUI_INMENUMODE | GUI_SYSTEMMENUMODE |
                                  GUI_POPUPMENUMODE | GUI_INMOVESIZE));
    const bool failure_only = std::string_view(reason) == "final_failure_cleanup";
    const bool foreign_cleanup_route = failure_only && !source_foreground &&
        foreground_before != overlay &&
        overlay_opaque_to_hit && foreign_gui_clear;
    const bool stable = GetForegroundWindow() == foreground_before &&
        root_at(cursor) == root && interactive_default_desktop();
    const bool route = context && own_capture && own_move && clean_gui &&
        (normal_route || foreign_cleanup_route) && stable && left_down();
    log("owned_cleanup_route", ",\"reason\":\"" + std::string(reason) +
        "\",\"cursor\":" + point(cursor) +
        ",\"root\":" + std::to_string(number(root)) +
        ",\"foreground\":" + std::to_string(number(foreground_before)) +
        ",\"source_foreground\":" + json_bool(source_foreground) +
        ",\"foreign_foreground_clear\":" + json_bool(foreign_gui_clear) +
        ",\"overlay_opaque_hit\":" + json_bool(overlay_opaque_to_hit) +
        ",\"source_context\":" + json_bool(context) +
        ",\"capture\":" + std::to_string(number(gui.hwndCapture)) +
        ",\"move_size\":" + std::to_string(number(gui.hwndMoveSize)) +
        ",\"exact_own_route\":" + json_bool(route));
    if (!route) return false; // Never inject UP into an unproven foreign root.
    if (!send_mouse(MOUSEEVENTF_LEFTUP, cursor)) return false;
    const bool raw = wait_event(state.raw_up, 1500, "cleanup_raw_up");
    Sleep(30); // One bounded async-state readback allowance, test driver only.
    const bool released = !left_down();
    state.held = !released;
    log("owned_cleanup_release", ",\"reason\":\"" + std::string(reason) +
        "\",\"raw_up\":" + json_bool(raw) +
        ",\"left_high\":" + json_bool(left_down()) +
        ",\"released\":" + json_bool(released));
    return raw && released;
}

int drive() noexcept {
    if (!wait_event(state.ui_ready, 2000, "ui_ready") ||
        !wait_event(state.receiver_ready, 2000, "receiver_ready") ||
        !same_source(state.source) || !state.receiver_ok || !virtual_desktop(state.virtual_rect) ||
        !input_clean() || left_down()) return 2;
    const HWND source = state.source.load();
    if (!GetCursorPos(&state.initial_cursor)) return 2;
    const HWND control = state.control.load();
    state.initial_source = same_source(source) ? capture_frame(source) : std::nullopt;
    state.initial_control = same_control(control) ? capture_frame(control) : std::nullopt;
    MONITORINFO initial_monitor_info{sizeof(initial_monitor_info)};
    state.initial_monitor = MonitorFromWindow(source, MONITOR_DEFAULTTONULL);
    state.initial_dpi = GetDpiForWindow(source);
    if (!state.initial_source || !state.initial_control || !state.initial_monitor ||
        !state.initial_dpi || !GetMonitorInfoW(state.initial_monitor, &initial_monitor_info) ||
        MonitorFromWindow(control, MONITOR_DEFAULTTONULL) != state.initial_monitor ||
        GetDpiForWindow(control) != state.initial_dpi ||
        !IsWindowVisible(control) || IsIconic(control) || IsZoomed(control)) {
        log("owned_initial_geometry_failed");
        return 2;
    }
    state.initial_work_area = initial_monitor_info.rcWork;
    log("owned_initial_geometry", ",\"source_visible\":" +
        rect(state.initial_source->visible) + ",\"control_visible\":" +
        rect(state.initial_control->visible) + ",\"dpi\":" +
        std::to_string(state.initial_dpi));
    if (GetForegroundWindow() != source || root_at(state.source_down) != source) {
        log("owned_start_preflight_failed", ",\"foreground\":" +
            std::to_string(number(GetForegroundWindow())) + ",\"root\":" +
            std::to_string(number(root_at(state.source_down))));
        return 2;
    }
    const LRESULT hit = SendMessageW(source, WM_NCHITTEST, 0,
        MAKELPARAM(state.source_down.x, state.source_down.y));
    if (hit != HTCAPTION) { log("owned_caption_hit_failed", ",\"hit\":" + std::to_string(hit)); return 2; }
    log("owned_start_preflight", ",\"source\":" + std::to_string(number(source)) +
        ",\"down\":" + point(state.source_down) + ",\"hit\":" + std::to_string(hit));
    if (!move_cursor(state.source_down) || root_at(state.source_down) != source ||
        !send_mouse(MOUSEEVENTF_LEFTDOWN, state.source_down)) return 2;
    state.held = true;
    POINT moved{state.source_down.x + 22, state.source_down.y + 18};
    if (!move_cursor(moved) || !wait_event(state.native_start, 2000, "native_start") ||
        state.native_downs != 1 || state.native_starts != 1 || !left_down()) return 2;
    GUITHREADINFO at_start{sizeof(at_start)};
    if (!GetGUIThreadInfo(state.source_tid, &at_start) ||
        at_start.hwndCapture != source || at_start.hwndMoveSize != source) {
        log("pre_overlay_native_authority_failed", ",\"capture\":" +
            std::to_string(number(at_start.hwndCapture)) + ",\"move_size\":" +
            std::to_string(number(at_start.hwndMoveSize)));
        return 2;
    }
    if (!wait_event(state.raw_down, 1500, "associated_raw_down")) return 2;
    std::optional<POINT> actual_down_point;
    std::optional<operations::MoveFrameGeometry> actual_down_frame;
    {
        std::lock_guard lock{state.down_mutex};
        actual_down_point = state.actual_down_point;
        actual_down_frame = state.actual_down_frame;
    }
    const bool original_anchor_exact = actual_down_point && actual_down_frame &&
        actual_down_point->x == state.source_down.x &&
        actual_down_point->y == state.source_down.y &&
        actual_down_frame->positioning == state.initial_source->positioning &&
        actual_down_frame->visible == state.initial_source->visible;
    log("owned_original_anchor", ",\"observed\":" +
        json_bool(actual_down_point.has_value()) + ",\"point\":" +
        point(actual_down_point.value_or(POINT{})) + ",\"frame_exact\":" +
        json_bool(original_anchor_exact));
    if (!original_anchor_exact) return 2;
    LARGE_INTEGER frequency{};
    const std::array frozen_targets{magnet::MagnetTarget{
        panebind::core::model::WindowId{"owned-control"},
        state.initial_control->visible, false}};
    const bool exact_down = original_anchor_exact && state.tagged_native_down && state.candidate_raw_down != 0 &&
        state.native_downs == 1 && state.raw_downs == 1 && same_source(source);
    const bool plain_move = hit == HTCAPTION && input_clean() && left_down() &&
        at_start.hwndMoveSize == source;
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 ||
        !move_session.begin(static_cast<std::uint64_t>(state.run_nonce),
            {actual_down_point->x, actual_down_point->y},
            actual_down_frame->visible, frozen_targets,
            state.original_down_qpc.load(),
            static_cast<std::uint64_t>(frequency.QuadPart),
            same_source(source), exact_down && left_down(), plain_move)) {
        log("owned_move_session_begin_failed", ",\"exact_down\":" + json_bool(exact_down) +
            ",\"plain_move\":" + json_bool(plain_move));
        return 2;
    }
    log("owned_move_session_begin", ",\"generation\":" +
        std::to_string(state.run_nonce) + ",\"anchor\":" + point(*actual_down_point) +
        ",\"initial_visible\":" + rect(actual_down_frame->visible));
    SetEvent(state.overlay_arm);
    if (!PostThreadMessageW(state.overlay_tid, create_overlay_message, 0, 0) ||
        !wait_event(state.overlay_ready, 2000, "overlay_ready")) return 2;
    if (!state.overlay_ok) {
        retire_live_move(behavior::MoveHandoffEscapeReason::SetupFailure,
                         "overlay_setup_failed");
        if (state.scenario == "setup-fail" && release_test_owned_left("setup_failure")) {
            const bool ended = wait_event(state.native_end, 1500, "setup_failure_native_end");
            SetEvent(state.stop);
            const bool gone = wait_event(state.overlay_gone, 2000, "setup_failure_overlay_gone");
            log("setup_failure_verdict", std::string(",\"cancel_attempts\":0,\"source_writes\":0") +
                ",\"native_end\":" + json_bool(ended) + ",\"overlay_gone\":" + json_bool(gone));
            return ended && gone && state.cancel_attempts == 0 ? 0 : 2;
        }
        return 2;
    }
    const HWND overlay = state.overlay.load();
    GUITHREADINFO before{sizeof(before)};
    const bool before_ok = GetGUIThreadInfo(state.source_tid, &before) != FALSE;
    log("pre_cancel", ",\"overlay\":" + std::to_string(number(overlay)) +
        ",\"overlay_hit\":" + json_bool(root_at(state.control_point) == overlay) +
        ",\"foreground\":" + std::to_string(number(GetForegroundWindow())) +
        ",\"gui_ok\":" + json_bool(before_ok) +
        ",\"capture\":" + std::to_string(number(before.hwndCapture)) +
        ",\"move_size\":" + std::to_string(number(before.hwndMoveSize)));
    state.control_baseline = state.control_mouse.load();
    log("control_legacy_baseline", ",\"count\":" + std::to_string(state.control_baseline));
    POINT pre_cancel_cursor{};
    const bool actual_isolation = same_overlay(overlay) &&
        GetCursorPos(&pre_cancel_cursor) && root_at(pre_cancel_cursor) == overlay &&
        root_at(state.control_point) == overlay && interactive_default_desktop() &&
        GetForegroundWindow() == source;
    if (!actual_isolation ||
        GetForegroundWindow() != source || !before_ok || !left_down() ||
        before.hwndCapture != at_start.hwndCapture ||
        before.hwndMoveSize != at_start.hwndMoveSize ||
        before.hwndFocus != at_start.hwndFocus) return 2;
    if (!move_session.isolation_ready(static_cast<std::uint64_t>(state.run_nonce),
                                      actual_isolation)) return 2;
    if (state.scenario == "early-up") {
        if (!release_test_owned_left("early_up")) return 2;
        // The actual Raw UP must reach the core before a test-only escape
        // retires any remaining overlay resource. No cancel was issued.
        const bool core_raw_up = state.core_raw_up_accepted;
        retire_live_move(behavior::MoveHandoffEscapeReason::EarlyUp,
                         "early_up_before_cancel");
        const bool ended = wait_event(state.native_end, 1500, "early_native_end");
        SetEvent(state.stop);
        log("early_up_verdict", ",\"native_end\":" + json_bool(ended) +
            ",\"core_raw_up\":" + json_bool(core_raw_up) +
            ",\"cancel_attempts\":" + std::to_string(state.cancel_attempts.load()));
        return ended && core_raw_up && state.cancel_attempts == 0 ? 0 : 2;
    }
    if (state.scenario == "stop") {
        // Exercise the escape while the original test-owned native Move and
        // synthetic LEFT are still active. Do not manufacture a legacy UP on
        // the overlay: its removal precedes the guarded test-only LEFTUP.
        if (!state.hotkey_registered || !left_down() || state.native_ends != 0 ||
            !send_test_stop_hotkey() ||
            !wait_event(state.stop, 1500, "keyboard_hotkey_stop") ||
            !state.hotkey_received || !state.hotkey_during_held_native) return 2;
        const bool gone = wait_event(state.overlay_gone, 2000, "keyboard_stop_overlay_gone");
        const bool released = gone && release_test_owned_left("keyboard_stop_after_overlay");
        const bool ended = released && wait_event(state.native_end, 1500, "keyboard_stop_native_end");
        log("keyboard_stop_verdict", ",\"wm_hotkey_received\":" +
            json_bool(state.hotkey_received) + ",\"unregistered\":" +
            json_bool(state.hotkey_removed) + ",\"overlay_gone\":" +
            json_bool(gone) + ",\"during_held_native\":" +
            json_bool(state.hotkey_during_held_native) + ",\"test_left_released\":" +
            json_bool(released) + ",\"native_end\":" + json_bool(ended) +
            ",\"overlay_legacy_ups\":" + std::to_string(state.overlay_ups.load()));
        return gone && released && ended && state.hotkey_received &&
            state.hotkey_during_held_native && state.hotkey_removed &&
            state.cancel_attempts == 0 ? 0 : 2;
    }
    // The overlay must be ready before the only permitted native cancel.
    const auto generation = static_cast<std::uint64_t>(state.run_nonce);
    bool cancel_started = false, cancel = false;
    DWORD cancel_error = 0;
    std::uint32_t raw_ups_at_cancel = 0;
    {
        // The Raw receiver takes the same short-lived gate before recording
        // UP. An already observed UP therefore wins before this call; a
        // cancel claim that wins is an in-flight bounded native operation.
        std::lock_guard cancel_lock{state.cancel_mutex};
        POINT cursor{};
        const bool still_live = !state.retired && state.raw_ups == 0 &&
            WaitForSingleObject(state.stop, 0) != WAIT_OBJECT_0 &&
            left_down() && input_clean() && same_source(source) &&
            same_overlay(state.overlay.load()) &&
            GetCursorPos(&cursor) && root_at(cursor) == state.overlay.load() &&
            interactive_default_desktop() && GetForegroundWindow() == source;
        if (still_live && move_session.may_cancel(generation) &&
            move_session.cancel_issued(generation)) {
            DWORD_PTR ignored{};
            raw_ups_at_cancel = state.raw_ups;
            ++state.cancel_attempts;
            state.cancel_qpc = qpc();
            cancel_started = true;
            cancel = SendMessageTimeoutW(source, WM_CANCELMODE, 0, 0,
                SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 1000, &ignored) != 0;
            cancel_error = cancel ? 0 : GetLastError();
        }
    }
    if (!cancel_started) {
        retire_live_move(behavior::MoveHandoffEscapeReason::NativeFailure,
                         "cancel_preflight_rejected");
        SetEvent(state.stop);
        return 2;
    }
    log("cancel_result", ",\"sent\":true,\"returned\":" + json_bool(cancel) +
        ",\"error\":" + std::to_string(cancel_error) +
        ",\"raw_ups_at_claim\":" + std::to_string(raw_ups_at_cancel) +
        ",\"claim_precedes_observed_up\":true" +
        ",\"overlay_ready_before_cancel\":" +
        json_bool(state.overlay_ready_qpc != 0 && state.overlay_ready_qpc < state.cancel_qpc));
    const bool end_received = cancel && wait_event(state.native_end, 1500, "native_end");
    const bool modal_returned = end_received &&
        wait_event(state.native_loop_return, 1500, "native_loop_return");
    const bool matching_native_end = modal_returned && state.native_ends == 1 &&
        state.native_starts == 1 && state.cancel_attempts == 1 && same_source(source);
    if (!cancel || !end_received || !modal_returned ||
        !left_down() || !matching_native_end ||
        !move_session.native_end_observed(generation, matching_native_end)) {
        retire_live_move(behavior::MoveHandoffEscapeReason::NativeFailure,
                         "native_end_barrier_failed");
        return 2;
    }
    state.writer_armed.store(true, std::memory_order_release);
    log("owned_move_writer_armed", ",\"generation\":" +
        std::to_string(generation) + ",\"native_end_count\":" +
        std::to_string(state.native_ends.load()));
    if (state.scenario == "writer-stall") {
        // Test-only: block the real shared writer callback while LEFT remains
        // held. The independent overlay owner must hit its one-shot deadline,
        // remove the shield, and leave the Raw receiver long enough for the
        // guarded test cleanup UP. SetWindowPos is not claimed to have blocked.
        POINT continuation{moved.x + 8, moved.y + 7};
        if (!move_cursor(continuation) ||
            !wait_event(state.writer_stall_started, 1500, "writer_stall_started")) return 2;
        const bool gone = wait_event(state.overlay_gone, state.deadline_ms + 1000,
                                     "writer_stall_overlay_gone");
        const bool held_at_gone = left_down();
        const bool released = gone && held_at_gone &&
            release_test_owned_left("writer_stall_after_overlay");
        const bool receipt = wait_event(state.writer_receipt, state.deadline_ms + 1000,
                                        "writer_stall_receipt");
        log("writer_stall_deadline_verdict", ",\"overlay_gone\":" + json_bool(gone) +
            ",\"held_at_stall_start\":" + json_bool(state.writer_stall_held_at_start) +
            ",\"held_at_overlay_gone\":" + json_bool(held_at_gone) +
            ",\"cleanup_up_observed\":" + json_bool(released) +
            ",\"writer_receipt\":" + json_bool(receipt) +
            ",\"native_attempts\":" +
            std::to_string(state.writer_native_attempts.load()) +
            ",\"real_shared_writer\":true,\"native_call_blocked\":false");
        return gone && state.writer_stall_held_at_start && held_at_gone &&
            released && receipt && state.writer_native_attempts == 0 && state.retired ? 0 : 2;
    }
    GUITHREADINFO after{sizeof(after)};
    const bool after_ok = GetGUIThreadInfo(state.source_tid, &after) != FALSE;
    log("post_end", ",\"gui_ok\":" + json_bool(after_ok) +
        ",\"capture\":" + std::to_string(number(after.hwndCapture)) +
        ",\"move_size\":" + std::to_string(number(after.hwndMoveSize)) +
        ",\"overlay_hit\":" + json_bool(root_at(state.control_point) == overlay));
    if (!after_ok || after.hwndCapture || after.hwndMoveSize ||
        root_at(state.control_point) != overlay) return 2;
    // This fixed cursor choice belongs only to the guest test driver. The
    // shared session uses the original DOWN/window anchor and accepts any
    // event-driven cursor stream; it has no knowledge of this trajectory.
    const auto& initial = state.initial_source->visible;
    const auto& target = state.initial_control->visible;
    const std::int64_t dx = target.left() - initial.right() - 5;
    const std::int64_t dy = target.top() - initial.top() + 4;
    const std::int64_t test_x = static_cast<std::int64_t>(state.source_down.x) + dx;
    const std::int64_t test_y = static_cast<std::int64_t>(state.source_down.y) + dy;
    if (test_x < INT_MIN || test_x >= INT_MAX || test_y < INT_MIN || test_y > INT_MAX)
        return 2;
    const POINT snap_cursor{static_cast<LONG>(test_x), static_cast<LONG>(test_y)};
    Sleep(100); // Test-only bounded motion separation; not a product sampler.
    if (!move_cursor(snap_cursor) ||
        !wait_event(state.writer_receipt, 2000, "first_move_write_receipt")) return 2;
    if (!state.writer_snapped_exact) {
        const POINT reacquire{snap_cursor.x + 1, snap_cursor.y};
        Sleep(100);
        if (!move_cursor(reacquire) ||
            !wait_event(state.writer_receipt, 2000, "reacquire_move_write_receipt")) return 2;
    }
    std::optional<operations::MoveWriteReceipt> snap_receipt;
    {
        std::lock_guard lock{state.receipt_mutex};
        snap_receipt = state.snapped_exact_receipt;
    }
    const bool snap_receipt_evidence = snap_receipt && snap_receipt->snapped &&
        snap_receipt->generation == state.run_nonce &&
        snap_receipt->quantum == state.writer_snapped_exact_quantum &&
        snap_receipt->native_attempted && snap_receipt->native_succeeded &&
        snap_receipt->exact && snap_receipt->post_context_exact &&
        snap_receipt->after &&
        snap_receipt->after->visible == snap_receipt->target_visible;
    if (!snap_receipt_evidence || state.writer_native_attempts == 0 ||
        state.writer_failures != 0) return 2;
    if (!move_cursor(state.control_point) || root_at(state.control_point) != overlay ||
        !release_test_owned_left("normal")) return 2;
    const bool legacy = wait_event(state.legacy_up, 1500, "legacy_up");
    const bool raw_associated = state.tagged_native_down &&
        state.candidate_raw_down > state.test_down_watermark &&
        state.candidate_raw_up > state.candidate_raw_down &&
        state.raw_downs == 1 && state.raw_ups == 1;
    const bool clean = !left_down() && state.retired &&
        state.control_mouse == state.control_baseline;
    log("normal_verdict", ",\"raw_up_associated\":" + json_bool(raw_associated) +
        ",\"test_down_watermark\":" + std::to_string(state.test_down_watermark.load()) +
        ",\"candidate_raw_down\":" + std::to_string(state.candidate_raw_down.load()) +
        ",\"candidate_raw_up\":" + std::to_string(state.candidate_raw_up.load()) +
        ",\"tagged_native_down\":" + json_bool(state.tagged_native_down) +
        ",\"legacy_up\":" + json_bool(legacy) +
        ",\"writer_retired\":" + json_bool(state.retired) +
        ",\"writer_offers\":" + std::to_string(state.writer_offers.load()) +
        ",\"writer_native_attempts\":" +
        std::to_string(state.writer_native_attempts.load()) +
        ",\"writer_snapped_exact\":" + json_bool(state.writer_snapped_exact) +
        ",\"snap_receipt_evidence\":" + json_bool(snap_receipt_evidence) +
        ",\"snapped_quantum\":" +
        std::to_string(state.writer_snapped_exact_quantum.load()) +
        ",\"control_legacy_before\":" + std::to_string(state.control_baseline) +
        ",\"control_legacy_after\":" + std::to_string(state.control_mouse.load()) +
        ",\"left_up\":" + json_bool(!left_down()) +
        ",\"clean\":" + json_bool(clean));
    if (!raw_associated || !legacy || !clean) return 2;
    // No driver-triggered stop here: the real Raw and overlay legacy UP
    // callbacks must independently establish normal removal and wake the
    // overlay owner. A deadline/failure wake is not a normal PASS.
    return wait_event(state.overlay_gone, 2000, "normal_overlay_gone") &&
        state.normal_removal_triggered ? 0 : 2;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 8 || std::wstring_view(argv[1]) != L"--run-owned-overlay-probe" ||
        std::wstring_view(argv[2]) != L"--scenario" ||
        std::wstring_view(argv[4]) != L"--evidence-log" ||
        std::wstring_view(argv[6]) != L"--sandbox-run-id") return 64;
    const std::wstring_view scenario(argv[3]);
    if (scenario != L"normal" && scenario != L"early-up" && scenario != L"stop" &&
        scenario != L"setup-fail" && scenario != L"writer-stall") return 64;
    // Any pre-overlay input-recovery failure is disposable only inside the
    // explicitly provisioned guest. Fail closed before opening evidence or
    // creating GUI/input on the ordinary host or an unrecognized guest run.
    if (!sandbox_run_authorized(argv[7], scenario, argv[5])) {
        constexpr char not_ready[] = "NOT_READY: owned input probe requires its exact interactive Windows Sandbox package; no GUI or input was started.\n";
        DWORD notice_written{};
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), not_ready,
                  static_cast<DWORD>(sizeof(not_ready) - 1), &notice_written, nullptr);
        return 78;
    }
    if (scenario == L"normal") state.scenario = "normal";
    else if (scenario == L"early-up") state.scenario = "early-up";
    else if (scenario == L"stop") state.scenario = "stop";
    else if (scenario == L"setup-fail") state.scenario = "setup-fail";
    else state.scenario = "writer-stall";
    state.inject_overlay_failure = scenario == L"setup-fail";
    if (scenario == L"writer-stall") {
        state.deadline_ms = 3000;
        state.inject_writer_stall = true;
    }
    state.run_nonce = static_cast<std::uintptr_t>(qpc() ^
        (static_cast<std::uint64_t>(GetCurrentProcessId()) << 32));
    if (state.run_nonce == 0) state.run_nonce = 1;
    state.log = CreateFileW(argv[5], GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (state.log == INVALID_HANDLE_VALUE) return 65;
    state.stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.ui_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.receiver_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.native_start = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.native_end = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.native_loop_return = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.overlay_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.overlay_gone = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.receiver_gone = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.raw_down = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.raw_up = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.legacy_up = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.ui_gone = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.overlay_arm = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.writer_gone = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.writer_receipt = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    state.writer_stall_started = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.log_notice = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    state.log_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state.logger_gone = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    const bool dpi_ready = SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) ||
        AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),
                                     DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!state.stop || !state.ui_ready || !state.receiver_ready || !state.native_start ||
        !state.native_end || !state.native_loop_return || !state.overlay_ready || !state.overlay_gone ||
        !state.receiver_gone || !state.raw_down || !state.raw_up || !state.legacy_up ||
        !state.ui_gone || !state.overlay_arm || !state.writer_gone ||
        !state.writer_receipt || !state.writer_stall_started ||
        !state.log_notice || !state.log_stop || !state.logger_gone ||
        !dpi_ready) {
        log("initialization_failed");
        return 66;
    }
    live_writer = std::make_unique<operations::LiveMoveWriter>(
        static_cast<std::uint64_t>(state.run_nonce), owned_move_callbacks());
    if (!live_writer->ready()) {
        log("writer_initialization_failed");
        return 66;
    }
    std::thread logger_thread(log_owner);
    log("startup", ",\"scenario\":\"" + state.scenario +
        "\",\"deadline_ms\":" + std::to_string(state.deadline_ms) +
        ",\"raw_cleanup_observation_ms\":" + std::to_string(raw_cleanup_observation_ms) +
        ",\"alpha\":" + std::to_string(overlay_alpha) +
        ",\"test_only\":true");
    SetConsoleCtrlHandler(console_control, TRUE);
    std::thread writer_thread(writer_owner);
    std::thread source_thread(ui_owner);
    std::thread overlay_thread(overlay_owner);
    std::thread watchdog_thread(overlay_watchdog);
    const int result = drive();
    state.retired = true;
    state.writer_armed.store(false, std::memory_order_release);
    if (!move_session.retired(static_cast<std::uint64_t>(state.run_nonce)))
        retire_live_move(behavior::MoveHandoffEscapeReason::NativeFailure,
                         "final_failure_cleanup");
    else (void)live_writer->retire(static_cast<std::uint64_t>(state.run_nonce));
    // Failure cleanup keeps an already-ready owned overlay alive until the
    // synthetic button is safely retired. It never injects toward a foreign
    // root. Normal/early-up paths have already retired the button.
    const bool cleanup_released = release_test_owned_left("final_failure_cleanup");
    SetEvent(state.stop);
    const bool overlay_gone = wait_event(state.overlay_gone, 3000, "final_overlay_gone");
    const bool receiver_gone = wait_event(state.receiver_gone, 7000, "final_receiver_gone");
    if (!receiver_gone) {
        // Never turn a bounded Raw observation into an unbounded join. This
        // executable is restricted to the disposable guest; ending only this
        // test process also releases any of its remaining owned windows.
        log("receiver_teardown_timeout", ",\"overlay_gone\":" + json_bool(overlay_gone));
        SetEvent(state.log_stop);
        SetEvent(state.log_notice);
        WaitForSingleObject(state.logger_gone, 1000);
        TerminateProcess(GetCurrentProcess(), 74);
        ExitProcess(74);
    }
    const bool writer_gone = wait_event(state.writer_gone, 3000, "final_writer_gone");
    if (!writer_gone) {
        log("writer_teardown_timeout", ",\"overlay_gone\":" + json_bool(overlay_gone));
        SetEvent(state.log_stop);
        SetEvent(state.log_notice);
        WaitForSingleObject(state.logger_gone, 1000);
        TerminateProcess(GetCurrentProcess(), 75);
        ExitProcess(75);
    }
    const HWND source = state.source.load();
    log("owned_cleanup_input", ",\"cleanup_released\":" + json_bool(cleanup_released) +
        ",\"held_recorded\":" + json_bool(state.held) +
        ",\"left_high\":" + json_bool(left_down()));
    if (source && same_source(source)) PostMessageW(source, WM_CLOSE, 0, 0);
    const bool ui_gone = wait_event(state.ui_gone, 3000, "final_ui_gone");
    if (state.held || left_down()) Sleep(30);
    const bool input_released = !state.held && !left_down();
    log("final_input_state", ",\"held_recorded\":" + json_bool(state.held) +
        ",\"left_high\":" + json_bool(left_down()) +
        ",\"released\":" + json_bool(input_released));
    if (overlay_thread.joinable()) overlay_thread.join();
    if (watchdog_thread.joinable()) watchdog_thread.join();
    if (writer_thread.joinable()) writer_thread.join();
    if (source_thread.joinable()) {
        if (ui_gone) source_thread.join();
        else source_thread.detach(); // Process exit destroys only our fixture.
    }
    log("shutdown", ",\"result\":" + std::to_string(result) +
        ",\"overlay_gone\":" + json_bool(overlay_gone) +
        ",\"receiver_gone\":" + json_bool(receiver_gone) +
        ",\"writer_gone\":" + json_bool(writer_gone) +
        ",\"overlay_created\":" + json_bool(state.overlay_created) +
        ",\"overlay_destroyed\":" + json_bool(state.overlay_destroyed) +
        ",\"receiver_created\":" + json_bool(state.receiver_created) +
        ",\"receiver_destroyed\":" + json_bool(state.receiver_destroyed) +
        ",\"registration_removed\":" + json_bool(state.registration_removed) +
        ",\"hotkey_registered\":" + json_bool(state.hotkey_registered) +
        ",\"wm_hotkey_received\":" + json_bool(state.hotkey_received) +
        ",\"hotkey_during_held_native\":" + json_bool(state.hotkey_during_held_native) +
        ",\"hotkey_removed\":" + json_bool(state.hotkey_removed) +
        ",\"ui_gone\":" + json_bool(ui_gone) +
        ",\"input_released\":" + json_bool(input_released) +
        ",\"cancel_attempts\":" + std::to_string(state.cancel_attempts.load()) +
        ",\"raw_downs\":" + std::to_string(state.raw_downs.load()) +
        ",\"raw_ups\":" + std::to_string(state.raw_ups.load()) +
        ",\"writer_offers\":" + std::to_string(state.writer_offers.load()) +
        ",\"writer_receipts\":" + std::to_string(state.writer_receipts.load()) +
        ",\"writer_native_attempts\":" + std::to_string(state.writer_native_attempts.load()) +
        ",\"writer_snapped_exact\":" + json_bool(state.writer_snapped_exact) +
        ",\"writer_failures\":" + std::to_string(state.writer_failures.load()) +
        ",\"legacy_ups\":" + std::to_string(state.overlay_ups.load()) +
        ",\"control_legacy_count\":" + std::to_string(state.control_mouse.load()) +
        ",\"log_ok\":" + json_bool(state.log_ok));
    SetConsoleCtrlHandler(console_control, FALSE);
    SetEvent(state.log_stop);
    SetEvent(state.log_notice);
    const bool logger_gone = WaitForSingleObject(state.logger_gone, 2000) == WAIT_OBJECT_0;
    if (!logger_gone) {
        // The evidence sink itself may be stuck. At this point the overlay is
        // already gone; terminate only this disposable test process.
        TerminateProcess(GetCurrentProcess(), 72);
    }
    if (logger_thread.joinable()) logger_thread.join();
    CloseHandle(state.log);
    for (const HANDLE event : {state.stop, state.ui_ready, state.receiver_ready, state.native_start,
             state.native_end, state.native_loop_return, state.overlay_ready, state.overlay_gone,
             state.receiver_gone, state.raw_down, state.raw_up, state.legacy_up,
             state.ui_gone, state.overlay_arm, state.writer_gone, state.writer_receipt,
             state.writer_stall_started, state.log_notice,
             state.log_stop, state.logger_gone}) CloseHandle(event);
    const bool input_counts = state.raw_downs == 1 && state.raw_ups == 1 &&
        state.native_downs == 1 && state.native_starts == 1 && state.native_ends == 1 &&
        state.tagged_native_down && state.candidate_raw_down > state.test_down_watermark &&
        state.candidate_raw_up > state.candidate_raw_down;
    const bool overlay_absent = !state.overlay_created ? state.overlay == nullptr :
        state.overlay_destroyed && state.overlay == nullptr;
    const bool teardown = overlay_gone && receiver_gone && writer_gone && overlay_absent &&
        state.receiver_created && state.receiver_destroyed &&
        state.registration_removed && state.hotkey_removed;
    return result == 0 && cleanup_released && teardown && ui_gone &&
        input_released && input_counts && state.writer_failures == 0 &&
        state.log_ok ? 0 : 2;
}
