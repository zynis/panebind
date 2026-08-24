#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace snapweave::core::events {

class WindowId final {
public:
    WindowId() = delete;

    explicit WindowId(std::string identifier) : value_(std::move(identifier)) {
        if (value_.empty()) {
            throw std::invalid_argument{"WindowId must not be empty"};
        }
    }

    [[nodiscard]] const std::string& value() const noexcept { return value_; }

    friend bool operator==(const WindowId&, const WindowId&) = default;

private:
    std::string value_;
};

enum class WindowEventType {
    MoveResizeStarted,
    GeometryChanged,
    MoveResizeEnded,
};

struct WindowEvent {
    WindowEvent(WindowEventType event_type, WindowId id)
        : type(event_type), window_id(std::move(id)) {}

    WindowEventType type;
    WindowId window_id;
};

} // namespace snapweave::core::events
