#pragma once

#include "core/geometry/geometry.h"

#include <limits>
#include <optional>

namespace panebind::core::geometry {

[[nodiscard]] constexpr std::optional<Distance>
checked_difference(Coordinate minuend, Coordinate subtrahend) noexcept {
    constexpr auto minimum = std::numeric_limits<Distance>::min();
    constexpr auto maximum = std::numeric_limits<Distance>::max();

    if ((subtrahend > 0 && minuend < minimum + subtrahend) ||
        (subtrahend < 0 && minuend > maximum + subtrahend)) {
        return std::nullopt;
    }

    return minuend - subtrahend;
}

[[nodiscard]] constexpr std::optional<Coordinate>
checked_add(Coordinate value, Distance delta) noexcept {
    constexpr auto minimum = std::numeric_limits<Coordinate>::min();
    constexpr auto maximum = std::numeric_limits<Coordinate>::max();

    if ((delta > 0 && value > maximum - delta) ||
        (delta < 0 && value < minimum - delta)) {
        return std::nullopt;
    }

    return value + delta;
}

// Compare a signed magnitude without negating INT64_MIN.
[[nodiscard]] constexpr bool magnitude_within(Distance value,
                                              Distance tolerance) noexcept {
    if (tolerance < 0) {
        return false;
    }

    return value >= -tolerance && value <= tolerance;
}

// An unrepresentable difference has magnitude greater than INT64_MAX and is
// therefore outside every valid non-negative Distance tolerance.
[[nodiscard]] constexpr bool magnitude_within(Coordinate first,
                                              Coordinate second,
                                              Distance tolerance) noexcept {
    const auto difference = checked_difference(first, second);
    return difference.has_value() && magnitude_within(*difference, tolerance);
}

} // namespace panebind::core::geometry
