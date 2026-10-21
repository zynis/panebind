#pragma once
#include "core/geometry/geometry.h"
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace panebind::core::v1 {
using geometry::Rect;
using geometry::Point;
using Key = std::uint64_t;
struct Edges { bool left{}, top{}, right{}, bottom{}; };
struct Limits {
    std::int64_t min_width{64}, min_height{48};
    std::int64_t max_width{2147483647}, max_height{2147483647};
    std::int64_t grid_width{1}, grid_height{1};
};
struct Item { Key id{}; Rect visible; Limits limits; };
struct Placement { Key id{}; Rect visible; };
struct Plan {
    bool accepted{};
    std::string reason;
    std::vector<Placement> placements;
};
enum class Action {
    None, Minimize, Maximize, Restore, Center, Lower,
    MoveLeft, MoveRight, MoveUp, MoveDown, WidthLess, WidthMore, HeightLess, HeightMore,
    NextDisplay, ToggleTopmost, OpacityDown, OpacityUp,
    DockLeft, DockRight, DockTop, DockBottom, DockTopLeft, DockTopRight, DockBottomLeft, DockBottomRight,
    StretchLeft, StretchRight, StretchTop, StretchBottom, StretchHorizontal, StretchVertical, StretchAll,
    Undo, Pause, Settings, Exit
};
enum class End { NormalUp, Escape, FocusLost, SourceLost, Paused, SessionLost, QueueFailure, Deadline, Exit, InputUnavailable };
} // namespace panebind::core::v1
