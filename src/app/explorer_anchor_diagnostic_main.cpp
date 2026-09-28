#include "platform/windows/explorer/explorer_group_session.h"
#include "platform/windows/console/sta_console_line_reader.h"
#include "platform/windows/text_encoding.h"

#include <dwmapi.h>
#include <array>
#include <charconv>
#include <climits>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef PANEBIND_BUILD_SHA
#define PANEBIND_BUILD_SHA "unknown"
#endif

namespace e = panebind::platform::windows::explorer;
namespace w = panebind::platform::windows;

namespace {
constexpr UINT_PTR deadline_timer = 1;
constexpr DWORD hit_test_timeout_ms = 100;

std::string quote(std::string_view value) { return w::json_quote(value); }
std::string quote(std::wstring_view value) {
    const auto converted = w::utf16_to_utf8(value);
    if (!converted.value) throw std::runtime_error("invalid UTF-16");
    return quote(*converted.value);
}
std::string boolean(bool value) { return value ? "true" : "false"; }
std::string hwnd_number(HWND value) {
    return std::to_string(reinterpret_cast<std::uintptr_t>(value));
}
std::string point(POINT value) {
    return "[" + std::to_string(value.x) + "," + std::to_string(value.y) + "]";
}
std::string rect(RECT value) {
    return "[" + std::to_string(value.left) + "," + std::to_string(value.top) +
           "," + std::to_string(value.right) + "," + std::to_string(value.bottom) + "]";
}
std::string rect(const panebind::core::geometry::Rect& value) {
    return "[" + std::to_string(value.left()) + "," + std::to_string(value.top()) +
           "," + std::to_string(value.right()) + "," + std::to_string(value.bottom()) + "]";
}
std::int64_t qpc() noexcept {
    LARGE_INTEGER value{};
    return QueryPerformanceCounter(&value) ? value.QuadPart : 0;
}
bool console(HANDLE handle) {
    DWORD mode{};
    return handle && handle != INVALID_HANDLE_VALUE && GetFileType(handle) == FILE_TYPE_CHAR &&
           GetConsoleMode(handle, &mode);
}
bool print(std::wstring_view text) {
    DWORD written{};
    return WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), text.data(),
                         static_cast<DWORD>(text.size()), &written, nullptr) && written == text.size();
}
std::optional<std::wstring> input_line(w::console_input::StaConsoleLineReader& reader) {
    const auto result = reader.read();
    if (result.status != w::console_input::LineStatus::Complete) return std::nullopt;
    return result.line;
}

class Log final {
public:
    explicit Log(const std::filesystem::path& path)
        : file_(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                            FILE_ATTRIBUTE_NORMAL, nullptr)), healthy_(file_ != INVALID_HANDLE_VALUE) {}
    ~Log() { if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_); }
    bool record(std::string_view type, std::string_view fields = {}) {
        if (!healthy_) return false;
        const std::string line = "{\"schema\":\"r1c4b/mvp1-explorer-anchor-v1\",\"sequence\":" +
                                 std::to_string(++sequence_) + ",\"type\":" + quote(type) +
                                 std::string(fields) + "}\n";
        DWORD written{};
        healthy_ = file_ != INVALID_HANDLE_VALUE &&
                   WriteFile(file_, line.data(), static_cast<DWORD>(line.size()), &written, nullptr) &&
                   written == line.size();
        return healthy_;
    }
    bool healthy() const noexcept { return healthy_; }
private:
    HANDLE file_{INVALID_HANDLE_VALUE};
    std::uint64_t sequence_{};
    bool healthy_{true};
};

struct Geometry final {
    bool positioning{}, visible{};
    RECT positioning_rect{}, visible_rect{};
    UINT dpi{};
};
Geometry geometry(HWND window) noexcept {
    Geometry result;
    result.positioning = GetWindowRect(window, &result.positioning_rect) != FALSE;
    result.visible = DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS,
                                            &result.visible_rect, sizeof(RECT)) == S_OK;
    result.dpi = GetDpiForWindow(window);
    return result;
}
std::string geometry_fields(const Geometry& value) {
    return ",\"positioning\":" +
           (value.positioning ? rect(value.positioning_rect) : "null") +
           ",\"visible\":" + (value.visible ? rect(value.visible_rect) : "null") +
           ",\"dpi\":" + std::to_string(value.dpi);
}
std::string_view hit_kind(bool valid, LRESULT result) noexcept {
    if (!valid) return "UNKNOWN";
    if (result == HTCAPTION) return "CAPTION_MOVE";
    switch (result) {
    case HTLEFT: case HTRIGHT: case HTTOP: case HTBOTTOM:
    case HTTOPLEFT: case HTTOPRIGHT: case HTBOTTOMLEFT: case HTBOTTOMRIGHT:
    case HTGROWBOX:
        return "BORDER_RESIZE";
    default:
        return "OTHER";
    }
}

class Probe final {
public:
    Probe(Log& log, const std::array<e::detail::GroupMemberBinding, 3>& members) noexcept
        : log_(log), members_(members), owner_(GetCurrentThreadId()) {}
    Probe(const Probe&) = delete;
    Probe& operator=(const Probe&) = delete;
    ~Probe() { stop(); }

    bool start(DWORD duration_ms) {
        if (active_ || GetCurrentThreadId() != owner_) return false;
        WNDCLASSW cls{};
        cls.lpfnWndProc = &window_proc;
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpszClassName = L"PaneBindExplorerAnchorReadOnlyReceiver";
        if (!RegisterClassW(&cls)) return false;
        receiver_ = CreateWindowExW(0, cls.lpszClassName, L"", 0, 0, 0, 0, 0,
                                    HWND_MESSAGE, nullptr, cls.hInstance, this);
        if (!receiver_) return false;
        RAWINPUTDEVICE device{1, 2, RIDEV_INPUTSINK, receiver_};
        if (!RegisterRawInputDevices(&device, 1, sizeof(device))) return false;
        raw_registered_ = true;
        RAWINPUTDEVICE registered[4]{};
        UINT count = 4;
        const auto found = GetRegisteredRawInputDevices(registered, &count, sizeof(RAWINPUTDEVICE));
        if (found != 1 || registered[0].usUsagePage != 1 || registered[0].usUsage != 2 ||
            registered[0].dwFlags != RIDEV_INPUTSINK || registered[0].hwndTarget != receiver_) return false;
        active_ = this;
        for (std::size_t i = 0; i < members_.size(); ++i) {
            bool repeated = false;
            for (std::size_t j = 0; j < i; ++j) repeated |= members_[j].process_id == members_[i].process_id;
            if (repeated) continue;
            const auto hook = SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND,
                nullptr, &win_event, members_[i].process_id, 0,
                WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
            if (!hook) return false;
            hooks_.push_back(hook);
        }
        if (!SetTimer(receiver_, deadline_timer, duration_ms, nullptr)) return false;
        timer_ = true;
        return log_.record("probe_ready", ",\"receiver\":" + hwnd_number(receiver_) +
            ",\"receiver_tid\":" + std::to_string(owner_) + ",\"raw_registration\":true" +
            ",\"hook_count\":" + std::to_string(hooks_.size()) +
            ",\"deadline_ms\":" + std::to_string(duration_ms));
    }
    bool run() {
        MSG message{};
        while (true) {
            const auto result = GetMessageW(&message, nullptr, 0, 0);
            if (result <= 0) {
                if (result < 0) healthy_ = false;
                break;
            }
            if (message.message == WM_INPUT) {
                dispatch_ = message;
                dispatch_valid_ = true;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
            dispatch_valid_ = false;
            if (!log_.healthy()) { healthy_ = false; break; }
        }
        return healthy_ && log_.healthy();
    }
    bool stop() noexcept {
        if (GetCurrentThreadId() != owner_) return false;
        if (timer_) { KillTimer(receiver_, deadline_timer); timer_ = false; }
        bool unhooked = true;
        for (const auto hook : hooks_) unhooked &= UnhookWinEvent(hook) != FALSE;
        hooks_.clear();
        if (active_ == this) active_ = nullptr;
        bool removed = true;
        if (raw_registered_) {
            RAWINPUTDEVICE device{1, 2, RIDEV_REMOVE, nullptr};
            removed = RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
            raw_registered_ = false;
        }
        bool destroyed = true;
        if (receiver_) { destroyed = DestroyWindow(receiver_) != FALSE; receiver_ = nullptr; }
        healthy_ &= unhooked && removed && destroyed;
        return healthy_;
    }
    std::string summary_fields() const {
        return ",\"raw_down\":" + std::to_string(raw_down_) +
               ",\"raw_up\":" + std::to_string(raw_up_) +
               ",\"native_start\":" + std::to_string(native_start_) +
               ",\"native_end\":" + std::to_string(native_end_) +
               ",\"online_move_candidate\":" + std::to_string(candidates_) +
               ",\"unrelated_raw_down\":" + std::to_string(unrelated_down_) +
               ",\"unrelated_raw_up\":" + std::to_string(unrelated_up_) +
               ",\"ambiguous_raw_down\":" + std::to_string(ambiguous_down_) +
               ",\"raw_error\":" + std::to_string(raw_error_) +
               ",\"health\":" + boolean(healthy_ && log_.healthy());
    }
private:
    struct Down final {
        std::uint64_t sequence{};
        std::size_t member{3};
        DWORD message_time{};
        bool exact_hit{}, caption{}, resize{}, pre_native{}, start_seen{}, end_seen{};
    };
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }
        auto* self = reinterpret_cast<Probe*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self && message == WM_INPUT) {
            try { self->on_raw(wparam, lparam); }
            catch (...) { self->healthy_ = false; PostQuitMessage(2); }
            return DefWindowProcW(hwnd, message, wparam, lparam);
        }
        if (self && message == WM_TIMER && wparam == deadline_timer) {
            self->log_.record("deadline", ",\"qpc\":" + std::to_string(qpc()));
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
    static void CALLBACK win_event(HWINEVENTHOOK, DWORD event, HWND window,
                                   LONG object, LONG child, DWORD thread, DWORD time) {
        if (!active_) return;
        try { active_->on_win_event(event, window, object, child, thread, time); }
        catch (...) { active_->healthy_ = false; PostQuitMessage(2); }
    }
    std::size_t member(HWND window) const noexcept {
        for (std::size_t i = 0; i < members_.size(); ++i) if (members_[i].window == window) return i;
        return members_.size();
    }
    bool identity(std::size_t index) const noexcept {
        if (index >= members_.size()) return false;
        const auto& binding = members_[index];
        DWORD pid{};
        return IsWindow(binding.window) && GetWindowThreadProcessId(binding.window, &pid) == binding.thread_id &&
               pid == binding.process_id && GetAncestor(binding.window, GA_ROOT) == binding.window &&
               GetWindow(binding.window, GW_OWNER) == nullptr;
    }
    void on_raw(WPARAM wparam, LPARAM lparam) {
        alignas(RAWINPUT) std::array<std::byte, sizeof(RAWINPUT)> bytes{};
        UINT size = static_cast<UINT>(bytes.size());
        SetLastError(0);
        const auto copied = GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT,
                                             bytes.data(), &size, sizeof(RAWINPUTHEADER));
        if (copied == static_cast<UINT>(-1) || copied != size || size < sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE)) {
            ++raw_error_;
            log_.record("raw_error", ",\"error\":" + std::to_string(GetLastError()) +
                                        ",\"reported_size\":" + std::to_string(size));
            healthy_ = false;
            return;
        }
        const auto* raw = reinterpret_cast<const RAWINPUT*>(bytes.data());
        if (raw->header.dwType != RIM_TYPEMOUSE) return;
        const auto buttons = raw->data.mouse.usButtonFlags;
        if (buttons & RI_MOUSE_LEFT_BUTTON_DOWN) on_down(*raw, wparam, lparam);
        if (buttons & RI_MOUSE_LEFT_BUTTON_UP) on_up(*raw, wparam, lparam);
    }
    void on_down(const RAWINPUT& raw, WPARAM wparam, LPARAM lparam) {
        if (down_ || suppress_until_up_) {
            ++ambiguous_down_;
            down_.reset();
            suppress_until_up_ = true;
            log_.record("raw_down_ambiguous", ",\"count\":" + std::to_string(ambiguous_down_));
            return;
        }
        const bool message_available = dispatch_valid_ && dispatch_.message == WM_INPUT &&
                                       dispatch_.hwnd == receiver_ && dispatch_.wParam == wparam &&
                                       dispatch_.lParam == lparam;
        const POINT message_point = message_available ? dispatch_.pt : POINT{};
        const DWORD message_time = message_available ? dispatch_.time : 0;
        const auto observed = qpc();
        // Do not inspect/log arbitrary pre-existing windows on unrelated DOWNs.
        const HWND foreground_window = GetForegroundWindow();
        const auto foreground_member = member(foreground_window);
        if (!message_available || foreground_member >= members_.size() || !identity(foreground_member)) {
            ++unrelated_down_;
            down_.reset();
            return;
        }
        const HWND hit = WindowFromPoint(message_point);
        const HWND hit_root = hit ? GetAncestor(hit, GA_ROOT) : nullptr;
        const auto index = member(hit_root);
        const bool exact = index < members_.size() && identity(index);
        if (!exact || index != foreground_member) {
            ++unrelated_down_;
            down_.reset();
            return;
        }
        const auto sequence = ++raw_down_;
        GUITHREADINFO gui{sizeof(gui)};
        const bool gui_ok = exact && GetGUIThreadInfo(members_[index].thread_id, &gui);
        const bool pre_native = gui_ok && !(gui.flags & GUI_INMOVESIZE) &&
                                !gui.hwndMoveSize && !gui.hwndCapture;
        bool hit_test_ok = false;
        LRESULT hit_test{};
        DWORD hit_error{};
        // WM_NCHITTEST stores signed 16-bit screen coordinates in LPARAM.
        if (exact && message_point.x >= SHRT_MIN && message_point.x <= SHRT_MAX &&
            message_point.y >= SHRT_MIN && message_point.y <= SHRT_MAX) {
            DWORD_PTR result{};
            SetLastError(0);
            hit_test_ok = SendMessageTimeoutW(hit_root, WM_NCHITTEST, 0,
                MAKELPARAM(static_cast<SHORT>(message_point.x), static_cast<SHORT>(message_point.y)),
                SMTO_ABORTIFHUNG | SMTO_BLOCK, hit_test_timeout_ms, &result) != 0;
            hit_error = hit_test_ok ? 0 : GetLastError();
            hit_test = static_cast<LRESULT>(result);
        }
        const auto initial = exact ? geometry(hit_root) : Geometry{};
        const auto classified = hit_kind(hit_test_ok, hit_test);
        down_ = Down{sequence, index, message_time, exact && message_available,
                     classified == "CAPTION_MOVE", classified == "BORDER_RESIZE",
                     pre_native, false, false};
        const bool foreground = foreground_window == hit_root;
        const auto fields = ",\"raw_down_sequence\":" + std::to_string(sequence) +
            ",\"qpc\":" + std::to_string(observed) +
            ",\"input_code\":" + std::to_string(GET_RAWINPUT_CODE_WPARAM(wparam)) +
            ",\"device_present\":" + boolean(raw.header.hDevice != nullptr) +
            ",\"message_pt_available\":" + boolean(message_available) +
            ",\"message_pt\":" + (message_available ? point(message_point) : "null") +
            ",\"message_time\":" + (message_available ? std::to_string(message_time) : "null") +
            ",\"hit_hwnd\":" + hwnd_number(hit) +
            ",\"hit_root\":" + hwnd_number(hit_root) +
            ",\"member\":" + (exact ? std::to_string(index) : "null") +
            ",\"exact_identity\":" + boolean(exact) +
            ",\"foreground_exact\":" + boolean(foreground) +
            ",\"hit_test_ok\":" + boolean(hit_test_ok) +
            ",\"hit_test\":" + (hit_test_ok ? std::to_string(hit_test) : "null") +
            ",\"down_hit_kind\":" + quote(classified) +
            ",\"hit_test_error\":" + std::to_string(hit_error) +
            ",\"gui_query\":" + boolean(gui_ok) +
            ",\"gui_flags\":" + std::to_string(gui.flags) +
            ",\"gui_move_size\":" + hwnd_number(gui.hwndMoveSize) +
            ",\"gui_capture\":" + hwnd_number(gui.hwndCapture) +
            ",\"pre_native_gui\":" + boolean(pre_native) +
            ",\"raw_left_held_sample\":" + boolean((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) +
            geometry_fields(initial);
        log_.record("raw_down", fields);
    }
    void on_up(const RAWINPUT& raw, WPARAM wparam, LPARAM lparam) {
        if (suppress_until_up_) {
            suppress_until_up_ = false;
            ++unrelated_up_;
            return;
        }
        if (!down_) { ++unrelated_up_; return; }
        const auto observed = qpc();
        ++raw_up_;
        const bool message_available = dispatch_valid_ && dispatch_.message == WM_INPUT &&
                                       dispatch_.hwnd == receiver_ && dispatch_.wParam == wparam &&
                                       dispatch_.lParam == lparam;
        log_.record("raw_up", ",\"raw_up_sequence\":" + std::to_string(raw_up_) +
            ",\"qpc\":" + std::to_string(observed) +
            ",\"message_pt\":" + (message_available ? point(dispatch_.pt) : "null") +
            ",\"raw_down_sequence\":" + (down_ ? std::to_string(down_->sequence) : "null") +
            ",\"native_start_seen\":" + boolean(down_->start_seen) +
            ",\"native_end_seen\":" + boolean(down_->end_seen) +
            ",\"device_present\":" + boolean(raw.header.hDevice != nullptr) +
            ",\"input_code\":" + std::to_string(GET_RAWINPUT_CODE_WPARAM(wparam)));
        down_.reset();
    }
    void on_win_event(DWORD event, HWND window, LONG object, LONG child, DWORD thread, DWORD time) {
        if (GetCurrentThreadId() != owner_ || object != OBJID_WINDOW || child != CHILDID_SELF) return;
        const auto index = member(window);
        if (index >= members_.size()) return;
        if (event != EVENT_SYSTEM_MOVESIZESTART && event != EVENT_SYSTEM_MOVESIZEEND) return;
        const bool exact = identity(index) && thread == members_[index].thread_id;
        if (event == EVENT_SYSTEM_MOVESIZESTART) ++native_start_;
        else ++native_end_;
        GUITHREADINFO gui{sizeof(gui)};
        const bool gui_ok = exact && GetGUIThreadInfo(thread, &gui);
        const auto current = exact ? geometry(window) : Geometry{};
        POINT cursor{};
        const bool cursor_ok = GetCursorPos(&cursor) != FALSE;
        const bool down_before = down_.has_value();
        const bool same_down = down_before && down_->member == index && down_->exact_hit;
        const bool ordered_time = same_down && static_cast<std::int32_t>(time - down_->message_time) >= 0;
        const bool held = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool foreground = GetForegroundWindow() == window;
        // A candidate is diagnostic only. It is not permission to cancel or write.
        const bool candidate = event == EVENT_SYSTEM_MOVESIZESTART && exact && same_down &&
            ordered_time && !down_->start_seen && down_->pre_native && down_->caption &&
            held && !ctrl && foreground && gui_ok &&
            (gui.flags & GUI_INMOVESIZE) && gui.hwndMoveSize == window;
        const std::string_view down_class = !same_down ? "UNKNOWN" :
            down_->caption ? "CAPTION_MOVE" : down_->resize ? "BORDER_RESIZE" : "OTHER";
        if (candidate) ++candidates_;
        const bool repeated_start = event == EVENT_SYSTEM_MOVESIZESTART && same_down && down_->start_seen;
        if (event == EVENT_SYSTEM_MOVESIZESTART && same_down) down_->start_seen = true;
        if (event == EVENT_SYSTEM_MOVESIZEEND && same_down) down_->end_seen = true;
        log_.record(event == EVENT_SYSTEM_MOVESIZESTART ? "native_start" : "native_end",
            ",\"member\":" + std::to_string(index) +
            ",\"hwnd\":" + hwnd_number(window) +
            ",\"pid\":" + std::to_string(members_[index].process_id) +
            ",\"tid\":" + std::to_string(thread) +
            ",\"capability_generation\":" + std::to_string(members_[index].capability_generation) +
            ",\"native_time\":" + std::to_string(time) +
            ",\"callback_qpc\":" + std::to_string(qpc()) +
            ",\"exact_identity\":" + boolean(exact) +
            ",\"foreground_exact\":" + boolean(foreground) +
            ",\"cursor_sample_at_callback\":" + (cursor_ok ? point(cursor) : "null") +
            ",\"gui_query\":" + boolean(gui_ok) +
            ",\"gui_flags\":" + std::to_string(gui.flags) +
            ",\"gui_move_size\":" + hwnd_number(gui.hwndMoveSize) +
            ",\"gui_capture\":" + hwnd_number(gui.hwndCapture) +
            ",\"raw_down_sequence\":" + (same_down ? std::to_string(down_->sequence) : "null") +
            ",\"down_before_callback\":" + boolean(down_before) +
            ",\"native_time_after_down\":" + boolean(ordered_time) +
            ",\"left_held\":" + boolean(held) +
            ",\"ctrl\":" + boolean(ctrl) +
            ",\"down_hit_caption\":" + boolean(same_down && down_->caption) +
            ",\"down_hit_kind\":" + quote(down_class) +
            ",\"down_pre_native_gui\":" + boolean(same_down && down_->pre_native) +
            ",\"repeated_start_for_down\":" + boolean(repeated_start) +
            ",\"read_only_move_candidate\":" + boolean(candidate) +
            geometry_fields(current));
        if (!exact) healthy_ = false;
    }
    Log& log_;
    std::array<e::detail::GroupMemberBinding, 3> members_;
    DWORD owner_{};
    HWND receiver_{};
    std::vector<HWINEVENTHOOK> hooks_;
    MSG dispatch_{};
    std::optional<Down> down_;
    std::uint64_t raw_down_{}, raw_up_{}, unrelated_down_{}, unrelated_up_{}, ambiguous_down_{},
                  native_start_{}, native_end_{}, candidates_{}, raw_error_{};
    bool dispatch_valid_{}, raw_registered_{}, timer_{}, healthy_{true}, suppress_until_up_{};
    static inline Probe* active_{};
};

int observe(Log& log, std::string_view sha, DWORD duration_ms) {
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    log.record("startup", ",\"implementation_sha\":" + quote(sha) +
        ",\"evidence_kind\":\"read_only_explorer_anchor_diagnostic\"" +
        ",\"owner_tid\":" + std::to_string(GetCurrentThreadId()) +
        ",\"qpc_frequency\":" + std::to_string(frequency.QuadPart) +
        ",\"native_cancel_attempted\":false,\"window_write_attempted\":false");
    const auto fail = [&](std::string_view reason) {
        log.record("shutdown", ",\"result\":\"BLOCKED\",\"reason\":" + quote(reason) +
                               ",\"native_cancel_attempted\":false,\"window_write_attempted\":false");
        return 2;
    };
    w::console_input::StaConsoleLineReader reader{
        GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE)};
    e::ExplorerGroupSession::OwnedMembers sessions;
    for (std::size_t i = 0; i < sessions.size(); ++i) {
        GUID guid{};
        wchar_t nonce[64]{};
        if (CoCreateGuid(&guid) != S_OK || !StringFromGUID2(guid, nonce, 64)) return fail("nonce_failed");
        const auto directory = std::filesystem::temp_directory_path() /
            (std::wstring{L"PaneBind-MVP1-Anchor-"} + static_cast<wchar_t>(L'A' + i) + L"-" + nonce);
        if (!std::filesystem::create_directory(directory)) return fail("nonce_directory_failed");
        auto begin = e::ExplorerConsentProvisioning::begin(directory);
        if (!begin.succeeded()) return fail("target_baseline_failed");
        const auto prompt = begin.provisioning->record_target_prompt();
        if (!prompt.succeeded()) return fail("target_prompt_failed");
        log.record("target_prompt", ",\"member\":" + std::to_string(i) +
            ",\"nonce\":" + quote(nonce) +
            ",\"baseline_generation\":" + std::to_string(begin.facts.generations.baseline_generation) +
            ",\"prompt_generation\":" + std::to_string(prompt.generation));
        print(L"\r\n请新建一扇 Explorer 窗口并打开此临时目录（不要复用旧窗）：\r\n" +
              directory.native() + L"\r\n完成后回控制台按 Enter；输入其它内容取消。\r\n");
        const auto answer = input_line(reader);
        if (!answer || !answer->empty()) return fail("target_confirmation_declined");
        auto confirmed = begin.provisioning->confirm_user_target();
        if (!confirmed.succeeded()) {
            log.record("target_rejected", ",\"member\":" + std::to_string(i) +
                ",\"reason\":" + std::to_string(static_cast<int>(confirmed.reason)));
            return fail("target_confirmation_failed");
        }
        const auto& facts = confirmed.facts;
        log.record("target_confirmed", ",\"member\":" + std::to_string(i) +
            ",\"baseline_excluded\":" + boolean(facts.baseline_exclusion_complete) +
            ",\"unique\":" + boolean(facts.unique_new_target) +
            ",\"exact_location\":" + boolean(facts.exact_target_location) +
            ",\"token_issued\":" + boolean(facts.token_issued) +
            ",\"capability_generation\":" + std::to_string(facts.generations.token_generation));
        sessions[i] = std::move(confirmed.session);
    }
    print(L"\r\n只读归因诊断：仅观察这三扇新窗的鼠标 DOWN、原生 START/END 和几何。"
          L"不取消拖动，不建立隔离窗，不移动窗口。输入 Y 后开始有界观察；其它输入取消。\r\n");
    const auto consent = input_line(reader);
    if (!consent || (*consent != L"Y" && *consent != L"y")) return fail("read_only_observation_declined");
    auto group = e::ExplorerGroupSession::create(std::move(sessions));
    if (!group) return fail("group_binding_failed");
    for (std::size_t i = 0; i < 3; ++i) {
        const auto& binding = group->bindings()[i];
        const auto& snap = group->binding_snapshots()[i];
        log.record("binding", ",\"member\":" + std::to_string(i) +
            ",\"group_generation\":" + std::to_string(group->generation()) +
            ",\"window_id\":" + std::to_string(binding.window_id) +
            ",\"capability_generation\":" + std::to_string(binding.capability_generation) +
            ",\"consent_generation\":" + std::to_string(binding.consent_generation) +
            ",\"hwnd\":" + hwnd_number(binding.window) +
            ",\"pid\":" + std::to_string(binding.process_id) +
            ",\"tid\":" + std::to_string(binding.thread_id) +
            ",\"positioning\":" + rect(snap.positioning_rect) +
            ",\"visible\":" + rect(snap.visible_rect) +
            ",\"dpi\":" + std::to_string(snap.dpi) +
            ",\"monitor\":" + quote(snap.monitor_device_name));
    }
    Probe probe{log, group->bindings()};
    if (!probe.start(duration_ms)) {
        const bool cleaned = probe.stop();
        log.record("probe_cleanup", ",\"success\":" + boolean(cleaned));
        return fail("receiver_or_hook_start_failed");
    }
    print(L"\r\n先单击并确认目标新窗已成为前台，再做普通标题栏 Move 与边框 Resize；到期自动结束。"
          L"本诊断不会接管输入。\r\n");
    const bool pumped = probe.run();
    const bool stopped = probe.stop();
    const bool complete = pumped && stopped && log.healthy();
    log.record("summary", probe.summary_fields());
    log.record("shutdown", ",\"result\":" + quote(complete ? "OBSERVED_NOT_AUTHORIZED" : "BLOCKED") +
                            ",\"hook_and_raw_removed\":" + boolean(stopped));
    return complete && log.healthy() ? 0 : 2;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring_view(argv[1]) == L"--build-identity") {
        std::cout << "{\"implementation_sha\":" << quote(PANEBIND_BUILD_SHA) << "}\n";
        return 0;
    }
    if (argc != 8 || std::wstring_view(argv[1]) != L"--read-only-consent-diagnostic" ||
        std::wstring_view(argv[2]) != L"--evidence-log" ||
        std::wstring_view(argv[4]) != L"--implementation-sha" ||
        std::wstring_view(argv[6]) != L"--observe-ms") return 2;
    const auto sha = w::utf16_to_utf8(argv[5]);
    if (!sha.value || *sha.value != PANEBIND_BUILD_SHA || sha.value->size() != 40 ||
        !console(GetStdHandle(STD_INPUT_HANDLE)) || !console(GetStdHandle(STD_OUTPUT_HANDLE))) return 2;
    DWORD duration{};
    const auto duration_text = w::utf16_to_utf8(argv[7]);
    if (!duration_text.value) return 2;
    const auto parsed = std::from_chars(duration_text.value->data(),
                                        duration_text.value->data() + duration_text.value->size(), duration);
    if (parsed.ec != std::errc{} || parsed.ptr != duration_text.value->data() + duration_text.value->size() ||
        duration < 5000 || duration > 60000) return 2;
    try {
        Log log{argv[3]};
        if (!log.healthy()) return 2;
        return observe(log, *sha.value, duration);
    } catch (const std::exception&) {
        std::cerr << "Explorer anchor diagnostic failed; preserve incomplete evidence\n";
        return 2;
    }
}
