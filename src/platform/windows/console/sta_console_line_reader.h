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
    WrongThread, Reentrant, ModeFailed, ModeRestoreFailed };
[[nodiscard]] const char* line_status_name(LineStatus) noexcept;
struct LineResult final {
    LineStatus status{LineStatus::InvalidConsole};
    std::optional<std::wstring> line;
    std::uint64_t wait_count{},pump_count{},message_dispatch_count{},console_input_event_count{};
    DWORD owner_thread{},error{};
    bool mode_restored{true};
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
[[nodiscard]] LineResult read_with_sta_pump(const InputEndpoint&,DWORD timeout_ms=INFINITE);
[[nodiscard]] bool nowait_api_available() noexcept;

// Original shared console mode is restored on every return/exception. The
// native implementation and deterministic mode tests use this same scope.
template<class Api> class InputModeScope final {
public:
    explicit InputModeScope(Api& api):api_(api) {
        if(!api_.get(saved_))return;
        const DWORD temporary=(saved_|ENABLE_EXTENDED_FLAGS)&
            ~(ENABLE_QUICK_EDIT_MODE|ENABLE_PROCESSED_INPUT|ENABLE_VIRTUAL_TERMINAL_INPUT);
        active_=api_.set(temporary);valid_=active_;
    }
    ~InputModeScope(){static_cast<void>(close());}
    InputModeScope(const InputModeScope&)=delete;
    InputModeScope& operator=(const InputModeScope&)=delete;
    bool valid()const noexcept{return valid_;}
    bool close()noexcept {
        if(active_){
            active_=false;
            DWORD actual{};
            restored_=api_.set(saved_)&&api_.get(actual)&&actual==saved_;
        }
        return restored_;
    }
private:
    Api& api_;DWORD saved_{};bool active_{},valid_{},restored_{true};
};
} // namespace detail

// No COM initialization, worker, hook, timer, code-page change or group access.
// Input/output handles are borrowed and must live through read().
class StaConsoleLineReader final {
public:
    static constexpr std::size_t capacity=255;
    StaConsoleLineReader(HANDLE input,HANDLE output) noexcept
        :input_(input),output_(output),owner_(GetCurrentThreadId()) {}
    [[nodiscard]] LineResult read();
private:
    HANDLE input_{},output_{};DWORD owner_{};
};
} // namespace panebind::platform::windows::console_input
