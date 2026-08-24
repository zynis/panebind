#pragma once

#include <cstdint>
#include <string>

namespace snapweave::core::events {

struct WindowId {
    std::string value;

    friend bool operator==(const WindowId&, const WindowId&) = default;
};

enum class WindowEventType {
    MoveResizeStarted,
    GeometryChanged,
    MoveResizeEnded,
};

struct WindowEvent {
    WindowEventType type{};
    WindowId window_id;
    std::uint64_t source_timestamp_ms{};
};

} // namespace snapweave::core::events

