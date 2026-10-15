#pragma once

#include "core/geometry/checked_arithmetic.h"
#include <array>

namespace panebind::core::v1 {

// Native adapters supply raw (not normalized) rectangle edges. A successful
// caption hit query does not overrule independently observed system buttons.
struct CaptionButtonFacts {
    geometry::Point point;
    std::array<geometry::Coordinate, 4> window{};
    std::array<geometry::Coordinate, 4> relative_buttons{};
    bool window_known{}, buttons_known{}, comparable_coordinates{};
    bool visible{}, standard_buttons_possible{};
};
enum class CaptionHit { Caption, SystemButton, Unknown };

[[nodiscard]] constexpr CaptionHit caption_hit(const CaptionButtonFacts& facts) noexcept {
    const auto& w = facts.window;
    if (!facts.visible || !facts.window_known || !facts.comparable_coordinates ||
        w[0] >= w[2] || w[1] >= w[3] || facts.point.x < w[0] ||
        facts.point.x >= w[2] || facts.point.y < w[1] || facts.point.y >= w[3])
        return CaptionHit::Unknown;
    const auto width = geometry::checked_difference(w[2], w[0]);
    const auto height = geometry::checked_difference(w[3], w[1]);
    if (!width || !height) return CaptionHit::Unknown;

    const auto& b = facts.relative_buttons;
    const bool valid = facts.buttons_known && b[0] >= 0 && b[1] >= 0 &&
        b[0] < b[2] && b[1] < b[3] && b[2] <= *width && b[3] <= *height;
    if (!valid) return facts.standard_buttons_possible ? CaptionHit::Unknown : CaptionHit::Caption;
    const auto left = geometry::checked_add(w[0], b[0]);
    const auto top = geometry::checked_add(w[1], b[1]);
    const auto right = geometry::checked_add(w[0], b[2]);
    const auto bottom = geometry::checked_add(w[1], b[3]);
    if (!left || !top || !right || !bottom) return CaptionHit::Unknown;
    return facts.point.x >= *left && facts.point.x < *right &&
        facts.point.y >= *top && facts.point.y < *bottom ?
        CaptionHit::SystemButton : CaptionHit::Caption;
}

[[nodiscard]] constexpr const char* caption_hit_name(CaptionHit result) noexcept {
    switch (result) {
    case CaptionHit::Caption: return "caption_not_in_confirmed_system_button_area";
    case CaptionHit::SystemButton: return "actual_DWM_system_caption_button_area";
    case CaptionHit::Unknown: return "system_caption_button_exclusion_unknown";
    }
    return "system_caption_button_exclusion_unknown";
}
} // namespace panebind::core::v1
