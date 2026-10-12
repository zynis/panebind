#pragma once

#include "platform/windows/console/sta_console_line_reader.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace panebind::platform::windows::guest_command {

enum class PipeLineStatus {
    Complete, Disconnected, ClientExited, TimedOut, Quit, OwnerWorkFailed,
    InvalidCommand, ReadFailed, WaitFailed, PeerRejected, InvalidEndpoint,
    WrongThread, Reentrant, CleanupUnconfirmed
};
[[nodiscard]] const char* pipe_line_status_name(PipeLineStatus) noexcept;

struct PipePeerFacts final {
    DWORD expected_pid{}, observed_pid{}, server_session{}, client_session{};
    DWORD server_integrity{}, client_integrity{};
    bool observed{}, same_sid{}, same_session{}, server_elevated{}, client_elevated{};
};

namespace detail {
// Production peer gate, exposed only to test the exact policy without
// changing account tokens or process integrity in the test environment.
[[nodiscard]] bool peer_facts_allowed(const PipePeerFacts&) noexcept;
}

struct PipeLineResult final {
    PipeLineStatus status{PipeLineStatus::InvalidEndpoint};
    std::optional<std::wstring> line;
    PipePeerFacts peer;
    std::uint64_t wait_count{}, pump_count{}, message_dispatch_count{}, byte_count{};
    DWORD owner_thread{}, error{};
    bool connected{}, cleanup_confirmed{};
};

// Guest entry adapter only: the caller must pass its exact Sandbox launch
// guard before constructing this endpoint. This class grants no window/input
// authority. It creates no console, worker, hook, timer or ordinary window.
// One RunId/configuration endpoint accepts one exact, live same-user/session
// process. Commands remain the existing bounded ASCII lines; replies stay in
// the existing evidence log, so a nonreading client cannot block the STA.
class StaGuestCommandPipe final {
public:
    static constexpr std::size_t capacity = 255;
    StaGuestCommandPipe(std::wstring_view run_id, std::wstring_view configuration,
                        DWORD expected_client_pid);
    ~StaGuestCommandPipe();
    StaGuestCommandPipe(const StaGuestCommandPipe&) = delete;
    StaGuestCommandPipe& operator=(const StaGuestCommandPipe&) = delete;

    [[nodiscard]] bool healthy() const noexcept;
    [[nodiscard]] DWORD error() const noexcept;
    [[nodiscard]] const std::wstring& pipe_name() const noexcept;
    [[nodiscard]] PipePeerFacts peer_facts() const noexcept;
    [[nodiscard]] bool cleanup_confirmed() const noexcept;
    [[nodiscard]] PipeLineResult read(
        console_input::StaOwnerWork work = {}, DWORD timeout_ms = INFINITE);
    // Cancel only this endpoint's pending IO. Failure to observe completion is
    // reported; its OVERLAPPED/buffer storage is kept alive until process exit.
    [[nodiscard]] bool close() noexcept;

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace panebind::platform::windows::guest_command
