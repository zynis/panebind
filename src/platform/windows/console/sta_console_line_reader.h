#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace panebind::platform::windows::console_input {
enum class LineStatus { Complete, Aborted, Quit, TimedOut, InvalidConsole,
    ApiUnavailable, WaitFailed, ReadFailed, EchoFailed, TooLong, InvalidUnicode,
    WrongThread, Reentrant, ModeQueryFailed, OwnerWorkFailed };
[[nodiscard]] const char* line_status_name(LineStatus) noexcept;
struct LineResult final {
    LineStatus status{LineStatus::InvalidConsole};
    std::optional<std::wstring> line;
    std::uint64_t wait_count{},pump_count{},message_dispatch_count{},console_input_event_count{};
    DWORD owner_thread{},error{};
    DWORD input_mode_before{},input_mode_after{};
    bool modes_observed{},mode_changed{};
};
struct StaOwnerWork {
    void* context{};
    bool (*run)(void*) noexcept{};
};

namespace detail {
enum class RecordRead { Empty, Record, Failed };
// Narrow test seam: neither callback can select/control a window or access a
// group through this interface. Production read MUST be the NOWAIT API.
struct InputEndpoint final {
    HANDLE ready{};
    void* context{};
    RecordRead (*read_nowait)(void*,INPUT_RECORD&) noexcept{};
    bool (*echo)(void*,std::wstring_view,std::wstring_view,bool) noexcept{};
};
[[nodiscard]] LineResult read_with_sta_pump(const InputEndpoint&,DWORD timeout_ms=INFINITE,StaOwnerWork work={});
[[nodiscard]] bool nowait_api_available() noexcept;

} // namespace detail

// No COM initialization, worker, hook, timer or code-page change. Optional
// caller-owned work runs between bounded message quanta; the reader itself has
// no window-operation authority. Default callers retain the original behavior.
// Host input modes/selection/shortcuts are never set or restored by this reader.
// Input/output handles are borrowed and must live through read().
class StaConsoleLineReader final {
public:
    static constexpr std::size_t capacity=255;
    StaConsoleLineReader(HANDLE input,HANDLE output) noexcept
        :input_(input),output_(output),owner_(GetCurrentThreadId()) {}
    [[nodiscard]] LineResult read(StaOwnerWork work={});
private:
    HANDLE input_{},output_{};DWORD owner_{};
};
} // namespace panebind::platform::windows::console_input
