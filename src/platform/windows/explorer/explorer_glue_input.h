#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>

namespace panebind::platform::windows::explorer {

struct CtrlSample final {
    // Available means sampled, not that Win32 distinguished UP from failure.
    bool available{};
    bool ctrl{};
    bool left{};
    bool right{};
};

[[nodiscard]] constexpr bool ctrl_high_bit(const std::int16_t value) noexcept {
    return value < 0;
}

// Three sequential observations, NOT one atomic keyboard snapshot. Aggregate
// VK_CONTROL is the authority; side-specific values are diagnostic only.
template <class ReadKey>
[[nodiscard]] CtrlSample sample_ctrl_with(ReadKey&& read) noexcept {
    return {true, ctrl_high_bit(read(VK_CONTROL)),
            ctrl_high_bit(read(VK_LCONTROL)), ctrl_high_bit(read(VK_RCONTROL))};
}

[[nodiscard]] inline CtrlSample sample_ctrl() noexcept {
    return sample_ctrl_with([](const int key) noexcept {
        return static_cast<std::int16_t>(GetAsyncKeyState(key));
    });
}

[[nodiscard]] inline std::int64_t glue_qpc_now() noexcept {
    LARGE_INTEGER value{};
    return QueryPerformanceCounter(&value) ? value.QuadPart : 0;
}

[[nodiscard]] inline std::int64_t glue_qpc_frequency() noexcept {
    LARGE_INTEGER value{};
    return QueryPerformanceFrequency(&value) ? value.QuadPart : 0;
}

} // namespace panebind::platform::windows::explorer
