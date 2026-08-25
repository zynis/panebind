#pragma once

#include "core/model/window_id.h"

#include <utility>

namespace panebind::core::events {

using WindowId = model::WindowId;

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

} // namespace panebind::core::events
