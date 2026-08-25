#pragma once

#include "core/geometry/checked_arithmetic.h"
#include "core/geometry/geometry.h"

#include <optional>
#include <stdexcept>

namespace panebind::core::movement {

struct TranslationDelta {
    geometry::Distance dx{};
    geometry::Distance dy{};

    friend constexpr bool operator==(const TranslationDelta&,
                                     const TranslationDelta&) = default;
};

enum class GeometryChangeKind {
    Unchanged,
    Translation,
    ResizeOrMixed,
};

struct GeometryChange {
    GeometryChangeKind kind{GeometryChangeKind::Unchanged};
    std::optional<TranslationDelta> delta;

    friend constexpr bool operator==(const GeometryChange&,
                                     const GeometryChange&) = default;
};

namespace detail {

[[nodiscard]] inline geometry::Distance checked_extent(
    geometry::Coordinate maximum,
    geometry::Coordinate minimum) {
    const auto extent = geometry::checked_difference(maximum, minimum);
    if (!extent.has_value()) {
        throw std::overflow_error{"rectangle extent is not representable"};
    }

    return *extent;
}

[[nodiscard]] inline geometry::Coordinate checked_translate_coordinate(
    geometry::Coordinate coordinate,
    geometry::Distance delta) {
    const auto translated = geometry::checked_add(coordinate, delta);
    if (!translated.has_value()) {
        throw std::overflow_error{"translated rectangle coordinate is not representable"};
    }

    return *translated;
}

} // namespace detail

[[nodiscard]] inline GeometryChange classify_geometry_change(
    const geometry::Rect& before,
    const geometry::Rect& after) {
    if (before == after) {
        return {GeometryChangeKind::Unchanged, std::nullopt};
    }

    const auto before_width = detail::checked_extent(before.right(), before.left());
    const auto before_height = detail::checked_extent(before.bottom(), before.top());
    const auto after_width = detail::checked_extent(after.right(), after.left());
    const auto after_height = detail::checked_extent(after.bottom(), after.top());

    if (before_width != after_width || before_height != after_height) {
        return {GeometryChangeKind::ResizeOrMixed, std::nullopt};
    }

    const auto dx = geometry::checked_difference(after.left(), before.left());
    const auto dy = geometry::checked_difference(after.top(), before.top());
    const auto right_dx = geometry::checked_difference(after.right(), before.right());
    const auto bottom_dy = geometry::checked_difference(after.bottom(), before.bottom());
    if (!dx.has_value() || !dy.has_value() || !right_dx.has_value() ||
        !bottom_dy.has_value()) {
        throw std::overflow_error{"translation delta is not representable"};
    }
    if (*dx != *right_dx || *dy != *bottom_dy) {
        throw std::logic_error{"equal-size rectangles do not share one translation delta"};
    }

    return {GeometryChangeKind::Translation, TranslationDelta{*dx, *dy}};
}

[[nodiscard]] inline geometry::Rect translate_rect(
    const geometry::Rect& rect,
    const TranslationDelta& delta) {
    const auto left = detail::checked_translate_coordinate(rect.left(), delta.dx);
    const auto top = detail::checked_translate_coordinate(rect.top(), delta.dy);
    const auto right = detail::checked_translate_coordinate(rect.right(), delta.dx);
    const auto bottom = detail::checked_translate_coordinate(rect.bottom(), delta.dy);

    return {left, top, right, bottom};
}

} // namespace panebind::core::movement
