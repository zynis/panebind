#pragma once

#include "core/geometry/geometry.h"

#include <optional>

namespace panebind::platform::windows::operations::window_translation {

// Capability-neutral preparation shared by the owned-window and companion
// adapters. It deliberately knows nothing about HWND, tokens, or native apply.
enum class TranslationPreparationStatus {
    Succeeded,
    EmptyGeometry,
    ResizeRejected,
    ArithmeticOverflow,
    NativeCoordinateOutOfRange,
};

struct TranslationPreparationResult {
    TranslationPreparationStatus status{
        TranslationPreparationStatus::ArithmeticOverflow};
    core::geometry::Distance dx{};
    core::geometry::Distance dy{};
    std::optional<core::geometry::Rect> target_positioning_rect;
};

[[nodiscard]] TranslationPreparationResult prepare_visible_translation(
    const core::geometry::Rect& current_positioning_rect,
    const core::geometry::Rect& current_visible_rect,
    const core::geometry::Rect& target_visible_rect) noexcept;

} // namespace panebind::platform::windows::operations::window_translation
