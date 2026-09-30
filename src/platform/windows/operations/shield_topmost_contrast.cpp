// Disposable-guest-only, self-owned Z-order contrast. No input or product HWNDs.
#include <windows.h>
#include <wtsapi32.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#ifndef PANEBIND_BUILD_SHA
#define PANEBIND_BUILD_SHA "unknown"
#endif

namespace {

constexpr wchar_t guest_exe[] =
    L"C:\\PaneBindMVP1\\Input\\panebind-shield-topmost-contrast.exe";
constexpr wchar_t input_marker[] = L"C:\\PaneBindMVP1\\Input\\run-id.txt";
constexpr wchar_t output_marker[] = L"C:\\PaneBindMVP1\\Output\\run-id.txt";
constexpr wchar_t output_root[] = L"C:\\PaneBindMVP1\\Output\\";
constexpr wchar_t window_class[] = L"PaneBindGuestTopmostContrastWindow";
constexpr UINT place_flags = SWP_NOACTIVATE | SWP_SHOWWINDOW;
constexpr UINT retry_flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;

std::uint64_t number(HWND window) noexcept {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(window));
}

const char* json_bool(bool value) noexcept { return value ? "true" : "false"; }

bool marker_matches(const wchar_t* path, std::wstring_view run_id) noexcept {
    const HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    std::array<char, 32> bytes{};
    DWORD read{};
    const bool valid = GetFileSizeEx(file, &size) && size.QuadPart == 32 &&
        ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) &&
        read == bytes.size() &&
        std::equal(bytes.begin(), bytes.end(), run_id.begin(),
            [](char actual, wchar_t expected) {
                return static_cast<unsigned char>(actual) == expected;
            });
    CloseHandle(file);
    return valid;
}

bool active_guest_desktop() noexcept {
    DWORD session{};
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session) || !session) return false;
    HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!input) return false;
    wchar_t input_name[256]{}, thread_name[256]{};
    DWORD length{};
    const bool desktop = GetUserObjectInformationW(input, UOI_NAME, input_name,
            sizeof(input_name), &length) &&
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME,
            thread_name, sizeof(thread_name), &length) &&
        std::wstring_view(input_name) == L"Default" &&
        std::wstring_view(thread_name) == std::wstring_view(input_name);
    CloseDesktop(input);
    if (!desktop) return false;
    LPWSTR memory{};
    DWORD bytes{};
    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION,
                                     WTSSessionInfoEx, &memory, &bytes)) return false;
    bool active = false;
    if (memory && bytes >= sizeof(WTSINFOEXW)) {
        const auto& info = *reinterpret_cast<const WTSINFOEXW*>(memory);
        active = info.Level == 1 &&
            info.Data.WTSInfoExLevel1.SessionState == WTSActive &&
            info.Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_UNLOCK;
    }
    WTSFreeMemory(memory);
    return active;
}

bool guest_guard(std::wstring_view run_id, std::wstring_view evidence) noexcept {
    if (run_id.size() != 32 || !std::all_of(run_id.begin(), run_id.end(), [](wchar_t ch) {
            return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f');
        })) return false;
    wchar_t user[256]{};
    DWORD length = static_cast<DWORD>(std::size(user));
    if (!GetUserNameW(user, &length) || std::wstring_view(user) != L"WDAGUtilityAccount" ||
        !active_guest_desktop()) return false;
    wchar_t image[MAX_PATH]{};
    const DWORD image_length = GetModuleFileNameW(nullptr, image,
        static_cast<DWORD>(std::size(image)));
    if (!image_length || image_length >= std::size(image) ||
        CompareStringOrdinal(image, -1, guest_exe, -1, TRUE) != CSTR_EQUAL)
        return false;
    const std::wstring expected = std::wstring(output_root) + std::wstring(run_id) +
        L"-topmost-contrast.jsonl";
    return CompareStringOrdinal(evidence.data(), -1, expected.c_str(), -1, TRUE) == CSTR_EQUAL &&
        marker_matches(input_marker, run_id) && marker_matches(output_marker, run_id);
}

struct PositionMessage {
    UINT kind{};
    HWND target{}, insert_after{};
    int x{}, y{}, width{}, height{};
    UINT flags{};
};

struct WindowFacts {
    static constexpr std::size_t max_messages = 16;
    const char* label{};
    DWORD requested_exstyle{};
    HWND window{};
    int requested_x{}, requested_y{};
    DWORD create_error{}, alpha_error{}, place_error{}, retry_error{};
    bool alpha_called{}, alpha_result{}, place_result{}, retry_called{}, retry_result{};
    LONG_PTR style_after_create{}, style_after_place{}, style_after_retry{};
    HWND foreground_before{}, foreground_after{};
    DWORD owner_thread{}, owner_process{};
    bool destroy_result{}, gone{};
    std::array<PositionMessage, max_messages> messages{};
    std::size_t message_count{};
    bool message_overflow{};
};

LRESULT CALLBACK window_proc(HWND window, UINT message,
                             WPARAM wparam, LPARAM lparam) noexcept {
    if (message == WM_NCCREATE) {
        const auto* created = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(created->lpCreateParams));
    }
    auto* facts = reinterpret_cast<WindowFacts*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (facts && (message == WM_WINDOWPOSCHANGING || message == WM_WINDOWPOSCHANGED)) {
        const auto* position = reinterpret_cast<const WINDOWPOS*>(lparam);
        if (position) {
            if (facts->message_count < facts->messages.size()) {
                facts->messages[facts->message_count++] = PositionMessage{
                    message, position->hwnd, position->hwndInsertAfter,
                    position->x, position->y, position->cx, position->cy,
                    position->flags};
            } else {
                facts->message_overflow = true;
            }
        }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

void capture(WindowFacts& facts, bool layered, int x, int y) noexcept {
    facts.label = layered ? "layered_alpha2" : "plain";
    facts.requested_exstyle = WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST |
        (layered ? WS_EX_LAYERED : 0);
    facts.requested_x = x;
    facts.requested_y = y;
    facts.foreground_before = GetForegroundWindow();
    SetLastError(0);
    facts.window = CreateWindowExW(facts.requested_exstyle, window_class, L"",
        WS_POPUP, x, y, 96, 64, nullptr, nullptr, GetModuleHandleW(nullptr), &facts);
    facts.create_error = facts.window ? 0 : GetLastError();
    if (!facts.window) {
        facts.foreground_after = GetForegroundWindow();
        return;
    }
    facts.owner_thread = GetWindowThreadProcessId(facts.window, &facts.owner_process);
    facts.style_after_create = GetWindowLongPtrW(facts.window, GWL_EXSTYLE);
    if (layered) {
        facts.alpha_called = true;
        SetLastError(0);
        facts.alpha_result = SetLayeredWindowAttributes(facts.window, 0, 2, LWA_ALPHA) != FALSE;
        facts.alpha_error = facts.alpha_result ? 0 : GetLastError();
    }
    SetLastError(0);
    facts.place_result = SetWindowPos(facts.window, HWND_TOPMOST,
        x, y, 96, 64, place_flags) != FALSE;
    facts.place_error = facts.place_result ? 0 : GetLastError();
    facts.style_after_place = GetWindowLongPtrW(facts.window, GWL_EXSTYLE);
    if (facts.place_result && (facts.style_after_place & WS_EX_TOPMOST) == 0) {
        facts.retry_called = true;
        SetLastError(0);
        facts.retry_result = SetWindowPos(facts.window, HWND_TOPMOST,
            0, 0, 0, 0, retry_flags) != FALSE;
        facts.retry_error = facts.retry_result ? 0 : GetLastError();
        facts.style_after_retry = GetWindowLongPtrW(facts.window, GWL_EXSTYLE);
    }
    facts.foreground_after = GetForegroundWindow();
    facts.destroy_result = DestroyWindow(facts.window) != FALSE;
    facts.gone = !IsWindow(facts.window);
}

std::string position_json(const PositionMessage& value) {
    std::ostringstream out;
    out << "{\"message\":" << value.kind << ",\"hwnd\":" << number(value.target)
        << ",\"insert_after\":" << number(value.insert_after)
        << ",\"x\":" << value.x << ",\"y\":" << value.y
        << ",\"width\":" << value.width << ",\"height\":" << value.height
        << ",\"flags\":" << value.flags << '}';
    return out.str();
}

std::string facts_json(std::uint32_t sequence, const WindowFacts& facts) {
    std::ostringstream out;
    out << "{\"schema\":\"r1c4b-shield-topmost-contrast/v1\",\"sequence\":"
        << sequence << ",\"type\":\"window\",\"case\":\"" << facts.label
        << "\",\"requested_exstyle\":" << facts.requested_exstyle
        << ",\"hwnd\":" << number(facts.window)
        << ",\"owner_tid\":" << facts.owner_thread
        << ",\"owner_pid\":" << facts.owner_process
        << ",\"create_error\":" << facts.create_error
        << ",\"style_after_create\":" << facts.style_after_create
        << ",\"alpha_called\":" << json_bool(facts.alpha_called)
        << ",\"alpha_result\":" << json_bool(facts.alpha_result)
        << ",\"alpha_error\":" << facts.alpha_error
        << ",\"place_insert_after\":-1,\"place_x\":" << facts.requested_x
        << ",\"place_y\":" << facts.requested_y
        << ",\"place_width\":96,\"place_height\":64"
        << ",\"place_flags\":" << place_flags
        << ",\"place_result\":" << json_bool(facts.place_result)
        << ",\"place_error\":" << facts.place_error
        << ",\"style_after_place\":" << facts.style_after_place
        << ",\"retry_called\":" << json_bool(facts.retry_called)
        << ",\"retry_insert_after\":-1,\"retry_flags\":" << retry_flags
        << ",\"retry_result\":" << json_bool(facts.retry_result)
        << ",\"retry_error\":" << facts.retry_error
        << ",\"style_after_retry\":" << facts.style_after_retry
        << ",\"foreground_before\":" << number(facts.foreground_before)
        << ",\"foreground_after\":" << number(facts.foreground_after)
        << ",\"destroy_result\":" << json_bool(facts.destroy_result)
        << ",\"gone\":" << json_bool(facts.gone)
        << ",\"windowpos_overflow\":" << json_bool(facts.message_overflow)
        << ",\"windowpos\":[";
    for (std::size_t i = 0; i < facts.message_count; ++i) {
        if (i) out << ',';
        out << position_json(facts.messages[i]);
    }
    out << "]}\n";
    return out.str();
}

bool write_line(HANDLE file, const std::string& line) noexcept {
    DWORD written{};
    return WriteFile(file, line.data(), static_cast<DWORD>(line.size()),
                     &written, nullptr) && written == line.size();
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring_view(argv[1]) == L"--build-identity") {
        std::cout << "{\"implementation_sha\":\"" << PANEBIND_BUILD_SHA << "\"}\n";
        return 0;
    }
    if (argc != 6 || std::wstring_view(argv[1]) != L"--run-topmost-contrast" ||
        std::wstring_view(argv[2]) != L"--sandbox-run-id" ||
        std::wstring_view(argv[4]) != L"--evidence-log") return 64;
    const std::wstring_view run_id(argv[3]), evidence(argv[5]);
    if (!guest_guard(run_id, evidence)) {
        std::cerr << "NOT_READY: exact interactive disposable guest and markers required.\n";
        return 78;
    }
    const HANDLE file = CreateFileW(argv[5], GENERIC_WRITE, FILE_SHARE_READ,
        nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return 65;
    const bool dpi = SetProcessDpiAwarenessContext(
            DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) ||
        AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),
                                     DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW klass{};
    klass.lpfnWndProc = window_proc;
    klass.hInstance = GetModuleHandleW(nullptr);
    klass.lpszClassName = window_class;
    const ATOM registered = dpi ? RegisterClassW(&klass) : 0;
    WindowFacts plain{}, layered{};
    if (registered) {
        const int origin_x = GetSystemMetrics(SM_XVIRTUALSCREEN) + 16;
        const int origin_y = GetSystemMetrics(SM_YVIRTUALSCREEN) + 16;
        capture(plain, false, origin_x, origin_y);
        capture(layered, true, origin_x + 112, origin_y);
    }
    const bool unregistered = registered && UnregisterClassW(window_class,
        GetModuleHandleW(nullptr)) != FALSE;
    std::string ascii_run_id;
    ascii_run_id.reserve(run_id.size());
    for (wchar_t ch : run_id) ascii_run_id.push_back(static_cast<char>(ch));
    std::ostringstream startup;
    startup << "{\"schema\":\"r1c4b-shield-topmost-contrast/v1\",\"sequence\":1"
        << ",\"type\":\"startup\",\"run_id\":\"" << ascii_run_id
        << "\",\"implementation_sha\":\"" << PANEBIND_BUILD_SHA
        << "\",\"guest_test_only\":true,\"no_input\":true,\"no_native_move\":true"
        << ",\"dpi_ready\":" << json_bool(dpi)
        << ",\"class_registered\":" << json_bool(registered != 0) << "}\n";
    const bool logged = write_line(file, startup.str()) &&
        (!registered || (write_line(file, facts_json(2, plain)) &&
                         write_line(file, facts_json(3, layered))));
    const std::string shutdown = std::string(
        "{\"schema\":\"r1c4b-shield-topmost-contrast/v1\",\"sequence\":") +
        (registered ? "4" : "2") + ",\"type\":\"shutdown\"" +
        ",\"class_unregistered\":" + json_bool(unregistered) +
        ",\"plain_gone\":" + json_bool(!plain.window || plain.gone) +
        ",\"layered_gone\":" + json_bool(!layered.window || layered.gone) + "}\n";
    const bool complete = logged && write_line(file, shutdown) && FlushFileBuffers(file);
    CloseHandle(file);
    // Capturing facts is not a shield PASS; failure to create/destroy is still
    // represented in evidence, with a nonzero exit for incomplete cleanup.
    return complete && registered && unregistered && plain.window && layered.window &&
        plain.gone && layered.gone ? 0 : 2;
}
