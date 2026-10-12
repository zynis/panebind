#include "platform/windows/guest_command/sta_guest_command_pipe.h"

#include <objbase.h>
#include <array>
#include <iostream>
#include <string>
#include <vector>

namespace pipe_input = panebind::platform::windows::guest_command;
namespace {
int failures{};
void check(bool condition, const char* label) {
    if (!condition) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
struct Handle final {
    HANDLE value{};
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    void close() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); value = nullptr; }
};
std::wstring nonce() {
    GUID id{};
    if (CoCreateGuid(&id) != S_OK) return {};
    const auto* bytes = reinterpret_cast<const unsigned char*>(&id);
    constexpr wchar_t hex[] = L"0123456789abcdef";
    std::wstring result;
    for (std::size_t i = 0; i < sizeof(id); ++i) {
        result.push_back(hex[bytes[i] >> 4]); result.push_back(hex[bytes[i] & 15]);
    }
    return result;
}
std::wstring endpoint(const std::wstring& run) { return L"\\\\.\\pipe\\PaneBindMVP1-" + run + L"-debug"; }
std::wstring ready_name(const std::wstring& run) { return L"Local\\PaneBindMVP1.CommandTest-" + run; }
struct Owner final { DWORD thread{GetCurrentThreadId()}; unsigned messages{}, work{}; bool fail{}; };
LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_NCCREATE) {
        const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
    }
    auto* state = reinterpret_cast<Owner*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_APP + 61 && state) {
        ++state->messages;
        if (wp > 1) PostMessageW(window, message, wp - 1, 0);
        return 0;
    }
    return DefWindowProcW(window, message, wp, lp);
}
bool work(void* context) noexcept {
    auto& owner = *static_cast<Owner*>(context);
    ++owner.work;
    return !owner.fail && owner.thread == GetCurrentThreadId();
}
bool absent_console() {
    DWORD mode{};
    return GetConsoleWindow() == nullptr &&
        !GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) &&
        !GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode);
}
int server_child(const std::wstring& run, DWORD parent, const std::wstring& mode) {
    if (GetConsoleWindow() != nullptr) return 201;
    DWORD observed_mode{};
    if (GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &observed_mode)) return 202;
    if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &observed_mode)) return 203;
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 21;
    struct Com { ~Com() { CoUninitialize(); } } com;
    Owner owner;
    WNDCLASSW cls{}; cls.lpfnWndProc = &procedure; cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"PaneBind.GuestCommand.Test.MessageOnly";
    if (!RegisterClassW(&cls)) return 22;
    const auto window = CreateWindowExW(0, cls.lpszClassName, L"private STA message-only fixture", 0,
        0, 0, 0, 0, HWND_MESSAGE, nullptr, cls.hInstance, &owner);
    if (!window) return 23;
    struct Window { HWND value; ~Window() { DestroyWindow(value); } } guard{window};
    pipe_input::StaGuestCommandPipe reader{run, L"debug", parent};
    if (!reader.healthy()) return 1000 + static_cast<int>(reader.error());
    Handle ready{OpenEventW(EVENT_MODIFY_STATE, FALSE, ready_name(run).c_str())};
    if (!ready.value || !SetEvent(ready.value)) return 25;
    // No synthetic keyboard/mouse input or foreign HWND. These are messages
    // to the child's exact private message-only window, proving STA progress.
    if (!PostMessageW(window, WM_APP + 61, 25, 0)) return 26;
    if (mode == L"timeout") {
        const auto result = reader.read({&owner, &work}, 120);
        return result.status == pipe_input::PipeLineStatus::TimedOut &&
            owner.messages == 25 && owner.work > 0 && result.cleanup_confirmed && reader.close() ? 0 : 27;
    }
    if (mode == L"owner_stop") owner.fail = true;
    const auto first = reader.read({&owner, &work}, 5000);
    if (mode == L"disconnect") {
        return first.status == pipe_input::PipeLineStatus::Disconnected && !first.line &&
            first.cleanup_confirmed && reader.close() ? 0 : 28;
    }
    if (mode == L"owner_stop") {
        return first.status == pipe_input::PipeLineStatus::OwnerWorkFailed && !first.line &&
            first.cleanup_confirmed && reader.close() ? 0 : 29;
    }
    if (mode == L"oversized" || mode == L"non_ascii") {
        return first.status == pipe_input::PipeLineStatus::InvalidCommand && !first.line &&
            first.cleanup_confirmed && reader.close() ? 0 : 30;
    }
    if (first.status != pipe_input::PipeLineStatus::Complete || first.line != L"" ||
        !first.peer.observed || !first.peer.same_sid || !first.peer.same_session ||
        first.peer.observed_pid != parent || first.owner_thread != GetCurrentThreadId() ||
        owner.messages != 25 || owner.work == 0) return 31;
    for (const auto expected : std::array<std::wstring_view, 3>{L"Y", L"S", L"Q"}) {
        const auto result = reader.read({&owner, &work}, 5000);
        if (result.status != pipe_input::PipeLineStatus::Complete || result.line != expected ||
            !result.cleanup_confirmed || result.pump_count == 0) return 32;
    }
    return absent_console() && reader.close() && reader.cleanup_confirmed() && reader.close() ? 0 : 33;
}
struct Process final {
    HANDLE value{}; DWORD pid{};
    ~Process() { if (value) CloseHandle(value); }
};
bool start(const std::wstring& arguments, Process& process) {
    std::array<wchar_t, 32768> executable{};
    const DWORD count = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (count == 0 || count == executable.size()) return false;
    auto command = L"\"" + std::wstring(executable.data(), count) + L"\" " + arguments;
    // CREATE_NO_WINDOW alone can leave copied standard console handles. This
    // fixture explicitly supplies its own NUL and an exact inheritance list;
    // no parent console handle or unrelated inheritable handle is borrowed.
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    Handle null_device{CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr)};
    if (null_device.value == INVALID_HANDLE_VALUE) return false;
    SIZE_T attribute_size{};
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
    if (attribute_size == 0) return false;
    std::vector<unsigned char> storage(attribute_size);
    const auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_size)) return false;
    struct Attributes { LPPROC_THREAD_ATTRIBUTE_LIST value; ~Attributes() { DeleteProcThreadAttributeList(value); } } attribute_guard{attributes};
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        &null_device.value, sizeof(null_device.value), nullptr, nullptr)) return false;
    STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = startup.StartupInfo.hStdOutput = startup.StartupInfo.hStdError = null_device.value;
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION created{};
    if (!CreateProcessW(executable.data(), command.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo, &created)) return false;
    CloseHandle(created.hThread); process.value = created.hProcess; process.pid = created.dwProcessId;
    return true;
}
bool write(HANDLE pipe, const std::string& bytes) {
    DWORD written{};
    return WriteFile(pipe, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size();
}
bool finish(Process& child, DWORD& code) {
    if (WaitForSingleObject(child.value, 8000) != WAIT_OBJECT_0) return false;
    return GetExitCodeProcess(child.value, &code) != FALSE;
}
void scenario(const std::wstring& mode) {
    const auto run = nonce(); check(run.size() == 32, "random private endpoint");
    Handle ready{CreateEventW(nullptr, TRUE, FALSE, ready_name(run).c_str())};
    check(ready.value != nullptr, "private ready event");
    Process child;
    const auto arguments = L"--no-console-server " + run + L" " + std::to_wstring(GetCurrentProcessId()) + L" " + mode;
    if (!start(arguments, child)) { check(false, "CREATE_NO_WINDOW child start"); return; }
    if (WaitForSingleObject(ready.value, 5000) != WAIT_OBJECT_0) {
        check(false, "child endpoint ready"); DWORD code{}; (void)finish(child, code);
        std::wcout << L"no-console pipe scenario=" << mode << L" before-ready exit=" << code << L'\n';
        return;
    }
    Handle client;
    if (mode != L"timeout" && mode != L"owner_stop") {
        client.value = CreateFileW(endpoint(run).c_str(), GENERIC_WRITE, 0, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        check(client.value != INVALID_HANDLE_VALUE, "exact pipe connect");
        if (client.value != INVALID_HANDLE_VALUE) {
            if (mode == L"disconnect") {
                check(write(client.value, "S"), "partial command delivered"); client.close();
            } else if (mode == L"oversized") {
                check(write(client.value, std::string(256, 'S') + "\n"), "oversized command delivered");
            } else if (mode == L"non_ascii") {
                check(write(client.value, std::string(1, static_cast<char>(0x80)) + "\n"), "non-ASCII delivered");
            } else {
                check(write(client.value, "\nY\r\nS\nQ\n"), "existing confirmation/status/stop lines delivered");
            }
        }
    }
    DWORD code{};
    check(finish(child, code), "child bounded exit");
    std::wcout << L"no-console pipe scenario=" << mode << L" exit=" << code << L'\n';
    check(code == 0, "actual child pipe/pump/lifecycle contract");
}
int client_child(const std::wstring& run) {
    if (GetConsoleWindow() != nullptr) return 401;
    DWORD observed_mode{};
    if (GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &observed_mode)) return 402;
    if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &observed_mode)) return 403;
    Handle ready{OpenEventW(SYNCHRONIZE, FALSE, ready_name(run).c_str())};
    if (!ready.value || WaitForSingleObject(ready.value, 5000) != WAIT_OBJECT_0) return 41;
    Handle pipe{CreateFileW(endpoint(run).c_str(), GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (pipe.value == INVALID_HANDLE_VALUE || !write(pipe.value, "Y\n")) return 42;
    return 0;
}
void exited_client() {
    const auto run = nonce();
    Handle ready{CreateEventW(nullptr, TRUE, FALSE, ready_name(run).c_str())};
    Process child;
    if (!start(L"--no-console-client " + run, child)) { check(false, "exit fixture child start"); return; }
    pipe_input::StaGuestCommandPipe reader{run, L"debug", child.pid};
    check(reader.healthy() && SetEvent(ready.value), "exit fixture endpoint ready");
    DWORD code{};
    check(finish(child, code) && code == 0, "real client exited with command buffered");
    std::wcout << L"no-console exiting client exit=" << code << L'\n';
    const auto result = reader.read({}, 1000);
    check(result.status == pipe_input::PipeLineStatus::ClientExited && !result.line,
        "driver exit outranks buffered consent");
    check(reader.close() && reader.cleanup_confirmed(), "exit endpoint close confirmed");
}
void peer_policy() {
    pipe_input::PipePeerFacts facts;
    facts.observed = true; facts.expected_pid = facts.observed_pid = 42;
    facts.same_sid = facts.same_session = true;
    facts.server_integrity = facts.client_integrity = SECURITY_MANDATORY_MEDIUM_RID;
    check(pipe_input::detail::peer_facts_allowed(facts), "exact same-user/session Medium peer");
    facts.server_integrity = facts.client_integrity = SECURITY_MANDATORY_HIGH_RID;
    facts.server_elevated = facts.client_elevated = true;
    check(pipe_input::detail::peer_facts_allowed(facts), "exact same-user/session High peer");
    facts.client_integrity = SECURITY_MANDATORY_MEDIUM_RID;
    check(!pipe_input::detail::peer_facts_allowed(facts), "mixed integrity rejected");
    facts.server_integrity = facts.client_integrity = SECURITY_MANDATORY_SYSTEM_RID;
    check(!pipe_input::detail::peer_facts_allowed(facts), "System peer rejected");
    facts.server_integrity = facts.client_integrity = 0;
    check(!pipe_input::detail::peer_facts_allowed(facts), "unknown integrity rejected");
    facts.server_integrity = facts.client_integrity = SECURITY_MANDATORY_HIGH_RID;
    facts.client_elevated = false;
    check(!pipe_input::detail::peer_facts_allowed(facts), "mixed elevation rejected");
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc == 5 && std::wstring_view(argv[1]) == L"--no-console-server")
        return server_child(argv[2], static_cast<DWORD>(std::stoul(argv[3])), argv[4]);
    if (argc == 3 && std::wstring_view(argv[1]) == L"--no-console-client") return client_child(argv[2]);
    for (const auto mode : {L"normal", L"disconnect", L"timeout", L"owner_stop", L"oversized", L"non_ascii"}) scenario(mode);
    exited_client();
    peer_policy();
    pipe_input::StaGuestCommandPipe invalid{L"bad", L"debug", GetCurrentProcessId()};
    check(!invalid.healthy(), "exact endpoint parameters required");
    std::cout << "guest command pipe failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
