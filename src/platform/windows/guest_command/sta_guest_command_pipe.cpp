#include "platform/windows/guest_command/sta_guest_command_pipe.h"

#include <algorithm>
#include <array>
#include <vector>

namespace panebind::platform::windows::guest_command {
namespace {
bool run_id_valid(std::wstring_view value) noexcept {
    return value.size() == 32 && std::all_of(value.begin(), value.end(), [](wchar_t c) {
        return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f');
    });
}
struct TokenFacts final {
    std::vector<unsigned char> user;
    DWORD integrity{};
    bool elevated{}, observed{};
};
TokenFacts token_facts(HANDLE process) {
    TokenFacts facts;
    HANDLE token{};
    if (!OpenProcessToken(process, TOKEN_QUERY, &token)) return facts;
    struct Guard { HANDLE value; ~Guard() { CloseHandle(value); } } guard{token};
    DWORD size{};
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    if (size == 0) return facts;
    facts.user.resize(size);
    if (!GetTokenInformation(token, TokenUser, facts.user.data(), size, &size)) return facts;
    GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &size);
    if (size == 0) return facts;
    std::vector<unsigned char> integrity(size);
    if (!GetTokenInformation(token, TokenIntegrityLevel, integrity.data(), size, &size)) return facts;
    const auto sid = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(integrity.data())->Label.Sid;
    if (!IsValidSid(sid)) return facts;
    const auto count = *GetSidSubAuthorityCount(sid);
    if (count == 0) return facts;
    facts.integrity = *GetSidSubAuthority(sid, count - 1);
    TOKEN_ELEVATION elevation{};
    if (!GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) return facts;
    facts.elevated = elevation.TokenIsElevated != 0;
    facts.observed = true;
    return facts;
}
bool disconnected_error(DWORD error) noexcept {
    return error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED ||
           error == ERROR_NO_DATA;
}
} // namespace

bool detail::peer_facts_allowed(const PipePeerFacts& facts) noexcept {
    const bool integrity_allowed = facts.server_integrity == SECURITY_MANDATORY_MEDIUM_RID ||
        facts.server_integrity == SECURITY_MANDATORY_HIGH_RID;
    return facts.observed && facts.expected_pid != 0 && facts.expected_pid == facts.observed_pid &&
        facts.same_sid && facts.same_session && integrity_allowed &&
        facts.server_integrity == facts.client_integrity &&
        facts.server_elevated == facts.client_elevated;
}

struct StaGuestCommandPipe::State final {
    HANDLE pipe{INVALID_HANDLE_VALUE}, event{}, client{};
    OVERLAPPED operation{};
    char byte{};
    DWORD owner{GetCurrentThreadId()}, error{};
    PipePeerFacts peer;
    std::wstring name;
    bool pending{}, connected{}, retired{}, reading{}, cleanup{true};

    ~State() {
        // Only destroyed once there is no outstanding kernel reference to the
        // operation/buffer. An unconfirmed cancelled State is retained instead.
        if (pipe != INVALID_HANDLE_VALUE) CloseHandle(pipe);
        if (event) CloseHandle(event);
        if (client) CloseHandle(client);
    }
    [[nodiscard]] bool client_exited() const noexcept {
        return client && WaitForSingleObject(client, 0) == WAIT_OBJECT_0;
    }
    bool observe_peer() {
        ULONG actual{};
        if (!GetNamedPipeClientProcessId(pipe, &actual)) { error = GetLastError(); return false; }
        peer.observed_pid = actual;
        if (actual != peer.expected_pid || client_exited()) { error = ERROR_ACCESS_DENIED; return false; }
        const auto caller = token_facts(GetCurrentProcess());
        const auto target = token_facts(client);
        DWORD own_session{}, client_session{};
        if (!caller.observed || !target.observed ||
            !ProcessIdToSessionId(GetCurrentProcessId(), &own_session) ||
            !ProcessIdToSessionId(actual, &client_session)) {
            error = ERROR_ACCESS_DENIED; return false;
        }
        const auto own_sid = reinterpret_cast<const TOKEN_USER*>(caller.user.data())->User.Sid;
        const auto client_sid = reinterpret_cast<const TOKEN_USER*>(target.user.data())->User.Sid;
        peer.server_session = own_session; peer.client_session = client_session;
        peer.same_session = own_session == client_session;
        peer.same_sid = IsValidSid(own_sid) && IsValidSid(client_sid) && EqualSid(own_sid, client_sid);
        peer.server_integrity = caller.integrity; peer.client_integrity = target.integrity;
        peer.server_elevated = caller.elevated; peer.client_elevated = target.elevated;
        peer.observed = true;
        if (!detail::peer_facts_allowed(peer)) { error = ERROR_ACCESS_DENIED; return false; }
        return true;
    }
    void prepare_operation() noexcept {
        ResetEvent(event);
        operation = {};
        operation.hEvent = event;
    }
    bool cancel() noexcept {
        if (!pending) return true;
        // Request cancellation, then observe its *actual* completion without
        // an unbounded GetOverlappedResult(TRUE). No buffer is recycled early.
        CancelIoEx(pipe, &operation);
        const auto deadline = GetTickCount64() + 250;
        for (;;) {
            DWORD bytes{};
            if (GetOverlappedResult(pipe, &operation, &bytes, FALSE) ||
                GetLastError() != ERROR_IO_INCOMPLETE) {
                pending = false; cleanup = true; return true;
            }
            const auto now = GetTickCount64();
            if (now >= deadline) { cleanup = false; return false; }
            const DWORD remaining = static_cast<DWORD>(deadline - now);
            const auto wake = MsgWaitForMultipleObjectsEx(1, &event, remaining,
                QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (wake == WAIT_FAILED || wake == WAIT_TIMEOUT) {
                cleanup = false; return false;
            }
            // Keep nonqueued COM calls and owner messages serviceable even
            // during the short cancellation-completion bound.
            for (unsigned i = 0; i < 8; ++i) {
                MSG message{};
                if (!PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) break;
                if (message.message == WM_QUIT) { PostQuitMessage(static_cast<int>(message.wParam)); break; }
                TranslateMessage(&message); DispatchMessageW(&message);
            }
        }
    }
};

const char* pipe_line_status_name(PipeLineStatus status) noexcept {
    switch (status) {
    case PipeLineStatus::Complete: return "complete";
    case PipeLineStatus::Disconnected: return "command_pipe_disconnected";
    case PipeLineStatus::ClientExited: return "command_driver_exited";
    case PipeLineStatus::TimedOut: return "timed_out";
    case PipeLineStatus::Quit: return "wm_quit";
    case PipeLineStatus::OwnerWorkFailed: return "owner_work_failed";
    case PipeLineStatus::InvalidCommand: return "invalid_or_oversized_command";
    case PipeLineStatus::ReadFailed: return "command_pipe_read_failed";
    case PipeLineStatus::WaitFailed: return "command_pipe_wait_failed";
    case PipeLineStatus::PeerRejected: return "command_pipe_peer_rejected";
    case PipeLineStatus::InvalidEndpoint: return "invalid_command_endpoint";
    case PipeLineStatus::WrongThread: return "wrong_thread";
    case PipeLineStatus::Reentrant: return "reentrant_read";
    case PipeLineStatus::CleanupUnconfirmed: return "command_io_cleanup_unconfirmed";
    }
    return "unknown";
}

StaGuestCommandPipe::StaGuestCommandPipe(std::wstring_view run_id,
    std::wstring_view configuration, DWORD expected_client_pid) : state_(std::make_unique<State>()) {
    auto& s = *state_;
    s.peer.expected_pid = expected_client_pid;
    if (!run_id_valid(run_id) || (configuration != L"debug" && configuration != L"release") ||
        expected_client_pid == 0 || expected_client_pid == GetCurrentProcessId()) {
        s.error = ERROR_INVALID_PARAMETER; return;
    }
    s.name = L"\\\\.\\pipe\\PaneBindMVP1-" + std::wstring(run_id) + L"-" + std::wstring(configuration);
    s.client = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, expected_client_pid);
    if (!s.client) { s.error = GetLastError(); return; }
    if (s.client_exited()) { s.error = ERROR_PROCESS_ABORTED; return; }
    s.event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!s.event) { s.error = GetLastError(); return; }
    s.pipe = CreateNamedPipeW(s.name.c_str(), PIPE_ACCESS_INBOUND |
        FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, 0, static_cast<DWORD>(capacity + 2), 0, nullptr);
    if (s.pipe == INVALID_HANDLE_VALUE) s.error = GetLastError();
}
StaGuestCommandPipe::~StaGuestCommandPipe() {
    if (!close()) {
        // Windows may still reference the OVERLAPPED and byte. Keep these
        // exact endpoint resources, not a reusable/live authority, until exit.
        (void)state_.release();
    }
}
bool StaGuestCommandPipe::healthy() const noexcept {
    return state_ && state_->error == 0 && !state_->retired && state_->pipe != INVALID_HANDLE_VALUE;
}
DWORD StaGuestCommandPipe::error() const noexcept { return state_ ? state_->error : ERROR_INVALID_HANDLE; }
const std::wstring& StaGuestCommandPipe::pipe_name() const noexcept { return state_->name; }
PipePeerFacts StaGuestCommandPipe::peer_facts() const noexcept { return state_ ? state_->peer : PipePeerFacts{}; }
bool StaGuestCommandPipe::cleanup_confirmed() const noexcept { return state_ && state_->cleanup && !state_->pending; }
bool StaGuestCommandPipe::close() noexcept {
    if (!state_) return true;
    auto& s = *state_; s.retired = true;
    if (!s.cancel()) return false;
    if (s.pipe != INVALID_HANDLE_VALUE) { CloseHandle(s.pipe); s.pipe = INVALID_HANDLE_VALUE; }
    if (s.event) { CloseHandle(s.event); s.event = nullptr; }
    if (s.client) { CloseHandle(s.client); s.client = nullptr; }
    return true;
}

PipeLineResult StaGuestCommandPipe::read(console_input::StaOwnerWork work, DWORD timeout_ms) {
    PipeLineResult result;
    auto& s = *state_;
    result.owner_thread = GetCurrentThreadId();
    const auto finish = [&](PipeLineStatus status, DWORD error = ERROR_SUCCESS) {
        if (status != PipeLineStatus::Complete) {
            s.retired = true;
            if (!s.cancel()) status = PipeLineStatus::CleanupUnconfirmed;
        }
        result.status = status; result.error = error; result.connected = s.connected;
        result.peer = s.peer; result.cleanup_confirmed = s.cleanup && !s.pending;
        return result;
    };
    if (result.owner_thread != s.owner) return finish(PipeLineStatus::WrongThread);
    if (s.reading) return finish(PipeLineStatus::Reentrant);
    if (!healthy()) return finish(PipeLineStatus::InvalidEndpoint, s.error);
    s.reading = true;
    struct ReadingGuard { bool& value; ~ReadingGuard() { value = false; } } reading{s.reading};
    const auto started = GetTickCount64();
    std::wstring line;
    bool carriage{};
    const auto pump = [&]() -> bool {
        ++result.pump_count;
        for (unsigned i = 0; i < (work.run ? 8U : 64U); ++i) {
            MSG message{};
            if (!PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) break;
            if (message.message == WM_QUIT) {
                PostQuitMessage(static_cast<int>(message.wParam)); result.status = PipeLineStatus::Quit; return false;
            }
            TranslateMessage(&message); DispatchMessageW(&message);
            ++result.message_dispatch_count;
        }
        if (work.run && !work.run(work.context)) { result.status = PipeLineStatus::OwnerWorkFailed; return false; }
        return true;
    };
    const auto peer_still_live = [&]() -> std::optional<PipeLineStatus> {
        if (s.client_exited()) return PipeLineStatus::ClientExited;
        if (s.connected && !PeekNamedPipe(s.pipe, nullptr, 0, nullptr, nullptr, nullptr)) {
            result.error = GetLastError();
            return disconnected_error(result.error) ? PipeLineStatus::Disconnected : PipeLineStatus::ReadFailed;
        }
        return std::nullopt;
    };
    for (;;) {
        if (const auto lost = peer_still_live()) return finish(*lost, result.error);
        s.prepare_operation();
        DWORD bytes{};
        const bool connecting = !s.connected;
        const BOOL immediate = connecting ? ConnectNamedPipe(s.pipe, &s.operation) :
            ReadFile(s.pipe, &s.byte, 1, &bytes, &s.operation);
        DWORD operation_error = immediate ? ERROR_SUCCESS : GetLastError();
        if (connecting && operation_error == ERROR_PIPE_CONNECTED) operation_error = ERROR_SUCCESS;
        if (operation_error == ERROR_IO_PENDING) s.pending = true;
        else if (operation_error != ERROR_SUCCESS)
            return finish(disconnected_error(operation_error) ? PipeLineStatus::Disconnected : PipeLineStatus::ReadFailed,
                          operation_error);
        // Pump on both immediate and event completions; a buffered command
        // cannot starve COM, Raw UP, WinEvent or the shared session quantum.
        if (!pump()) return finish(result.status);
        while (s.pending) {
            DWORD remaining = INFINITE;
            if (timeout_ms != INFINITE) {
                const auto elapsed = GetTickCount64() - started;
                if (elapsed >= timeout_ms) return finish(PipeLineStatus::TimedOut);
                remaining = timeout_ms - static_cast<DWORD>(elapsed);
            }
            const std::array handles{s.client, s.event};
            ++result.wait_count;
            const auto wake = MsgWaitForMultipleObjectsEx(static_cast<DWORD>(handles.size()), handles.data(),
                remaining, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (wake == WAIT_FAILED) return finish(PipeLineStatus::WaitFailed, GetLastError());
            if (wake == WAIT_TIMEOUT) return finish(PipeLineStatus::TimedOut);
            if (wake == WAIT_OBJECT_0) return finish(PipeLineStatus::ClientExited);
            if (wake != WAIT_OBJECT_0 + 1 && wake != WAIT_OBJECT_0 + handles.size())
                return finish(PipeLineStatus::WaitFailed, ERROR_INVALID_DATA);
            if (!pump()) return finish(result.status);
            if (const auto lost = peer_still_live()) return finish(*lost, result.error);
            if (wake == WAIT_OBJECT_0 + 1) {
                if (!GetOverlappedResult(s.pipe, &s.operation, &bytes, FALSE)) {
                    const auto error = GetLastError();
                    if (error == ERROR_IO_INCOMPLETE) continue;
                    s.pending = false;
                    return finish(disconnected_error(error) ? PipeLineStatus::Disconnected : PipeLineStatus::ReadFailed, error);
                }
                s.pending = false;
            }
        }
        if (const auto lost = peer_still_live()) return finish(*lost, result.error);
        if (connecting) {
            s.connected = true;
            if (!s.observe_peer()) return finish(PipeLineStatus::PeerRejected, s.error);
            continue;
        }
        if (bytes != 1) return finish(PipeLineStatus::Disconnected);
        ++result.byte_count;
        const auto byte = static_cast<unsigned char>(s.byte);
        if (byte == '\n') { result.line = std::move(line); return finish(PipeLineStatus::Complete); }
        if (carriage) return finish(PipeLineStatus::InvalidCommand, ERROR_INVALID_DATA);
        if (byte == '\r') { carriage = true; continue; }
        if (byte < 32 || byte > 126 || line.size() == capacity)
            return finish(PipeLineStatus::InvalidCommand, ERROR_INVALID_DATA);
        line.push_back(static_cast<wchar_t>(byte));
    }
}
} // namespace panebind::platform::windows::guest_command
