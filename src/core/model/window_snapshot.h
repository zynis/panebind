#pragma once

#include "core/events/window_event.h"
#include "core/geometry/geometry.h"

#include <cstdint>
#include <optional>
#include <string>

namespace snapweave::core::model {

enum class WindowState {
    Normal,
    Minimized,
    Maximized,
};

struct NormalizedWindowSnapshot {
    events::WindowId id;
    std::uint32_t process_id{};
    std::string application_name;
    std::string title;
    bool visible{};
    WindowState state{WindowState::Normal};
    geometry::Rect positioning_bounds;
    std::optional<geometry::Rect> visible_frame_bounds;
    std::string display_id;
    geometry::Rect display_work_area;
    double display_scale{1.0};
};

} // namespace snapweave::core::model

