#include "platform/windows/operations/window_translation.h"

#include "core/geometry/checked_arithmetic.h"

#include <limits>

namespace panebind::platform::windows::operations::window_translation {

TranslationPreparationResult prepare_visible_translation(
    const core::geometry::Rect& current_positioning_rect,
    const core::geometry::Rect& current_visible_rect,
    const core::geometry::Rect& target_visible_rect) noexcept {
    const auto positioning_width = core::geometry::checked_difference(
        current_positioning_rect.right(), current_positioning_rect.left());
    const auto positioning_height = core::geometry::checked_difference(
        current_positioning_rect.bottom(), current_positioning_rect.top());
    const auto current_visible_width = core::geometry::checked_difference(
        current_visible_rect.right(), current_visible_rect.left());
    const auto current_visible_height = core::geometry::checked_difference(
        current_visible_rect.bottom(), current_visible_rect.top());
    const auto target_visible_width = core::geometry::checked_difference(
        target_visible_rect.right(), target_visible_rect.left());
    const auto target_visible_height = core::geometry::checked_difference(
        target_visible_rect.bottom(), target_visible_rect.top());

    if (!positioning_width.has_value() || !positioning_height.has_value() ||
        !current_visible_width.has_value() || !current_visible_height.has_value() ||
        !target_visible_width.has_value() || !target_visible_height.has_value()) {
        return {TranslationPreparationStatus::ArithmeticOverflow,
                0,
                0,
                std::nullopt};
    }
    if (*positioning_width == 0 || *positioning_height == 0 ||
        *current_visible_width == 0 || *current_visible_height == 0 ||
        *target_visible_width == 0 || *target_visible_height == 0) {
        return {TranslationPreparationStatus::EmptyGeometry, 0, 0, std::nullopt};
    }
    if (*current_visible_width != *target_visible_width ||
        *current_visible_height != *target_visible_height) {
        return {TranslationPreparationStatus::ResizeRejected, 0, 0, std::nullopt};
    }

    const auto left_delta = core::geometry::checked_difference(
        target_visible_rect.left(), current_visible_rect.left());
    const auto right_delta = core::geometry::checked_difference(
        target_visible_rect.right(), current_visible_rect.right());
    const auto top_delta = core::geometry::checked_difference(
        target_visible_rect.top(), current_visible_rect.top());
    const auto bottom_delta = core::geometry::checked_difference(
        target_visible_rect.bottom(), current_visible_rect.bottom());
    if (!left_delta.has_value() || !right_delta.has_value() ||
        !top_delta.has_value() || !bottom_delta.has_value()) {
        return {TranslationPreparationStatus::ArithmeticOverflow,
                0,
                0,
                std::nullopt};
    }
    if (*left_delta != *right_delta || *top_delta != *bottom_delta) {
        return {TranslationPreparationStatus::ResizeRejected, 0, 0, std::nullopt};
    }

    const auto target_left = core::geometry::checked_add(
        current_positioning_rect.left(), *left_delta);
    const auto target_top = core::geometry::checked_add(
        current_positioning_rect.top(), *top_delta);
    const auto target_right = core::geometry::checked_add(
        current_positioning_rect.right(), *left_delta);
    const auto target_bottom = core::geometry::checked_add(
        current_positioning_rect.bottom(), *top_delta);
    if (!target_left.has_value() || !target_top.has_value() ||
        !target_right.has_value() || !target_bottom.has_value()) {
        return {TranslationPreparationStatus::ArithmeticOverflow,
                0,
                0,
                std::nullopt};
    }

    constexpr auto native_minimum = std::numeric_limits<int>::min();
    constexpr auto native_maximum = std::numeric_limits<int>::max();
    const auto native_representable = [](const core::geometry::Coordinate value) {
        return value >= native_minimum && value <= native_maximum;
    };
    if (!native_representable(*target_left) ||
        !native_representable(*target_top) ||
        !native_representable(*target_right) ||
        !native_representable(*target_bottom)) {
        return {TranslationPreparationStatus::NativeCoordinateOutOfRange,
                0,
                0,
                std::nullopt};
    }

    return {TranslationPreparationStatus::Succeeded,
            *left_delta,
            *top_delta,
            core::geometry::Rect{*target_left,
                                 *target_top,
                                 *target_right,
                                 *target_bottom}};
}

} // namespace panebind::platform::windows::operations::window_translation
