#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

namespace snapweave::core::geometry {

using Coordinate = std::int64_t;
using Distance = std::int64_t;

struct Point {
    Coordinate x{};
    Coordinate y{};

    friend constexpr bool operator==(const Point&, const Point&) = default;
};

struct Size {
    Distance width{};
    Distance height{};

    friend constexpr bool operator==(const Size&, const Size&) = default;
};

class Rect final {
public:
    constexpr Rect() = default;

    constexpr Rect(Coordinate first_x,
                   Coordinate first_y,
                   Coordinate second_x,
                   Coordinate second_y) noexcept
        : left_(std::min(first_x, second_x)),
          top_(std::min(first_y, second_y)),
          right_(std::max(first_x, second_x)),
          bottom_(std::max(first_y, second_y)) {}

    [[nodiscard]] constexpr Coordinate left() const noexcept { return left_; }
    [[nodiscard]] constexpr Coordinate top() const noexcept { return top_; }
    [[nodiscard]] constexpr Coordinate right() const noexcept { return right_; }
    [[nodiscard]] constexpr Coordinate bottom() const noexcept { return bottom_; }

    [[nodiscard]] constexpr Distance width() const noexcept { return right_ - left_; }
    [[nodiscard]] constexpr Distance height() const noexcept { return bottom_ - top_; }
    [[nodiscard]] constexpr Size size() const noexcept { return {width(), height()}; }
    [[nodiscard]] constexpr bool empty() const noexcept {
        return width() == 0 || height() == 0;
    }

    friend constexpr bool operator==(const Rect&, const Rect&) = default;

private:
    Coordinate left_{};
    Coordinate top_{};
    Coordinate right_{};
    Coordinate bottom_{};
};

enum class Edge {
    Left,
    Top,
    Right,
    Bottom,
};

[[nodiscard]] constexpr Distance absolute_distance(Coordinate first,
                                                   Coordinate second) noexcept {
    return first >= second ? first - second : second - first;
}

[[nodiscard]] constexpr Coordinate edge_coordinate(const Rect& rect, Edge edge) noexcept {
    switch (edge) {
    case Edge::Left:
        return rect.left();
    case Edge::Top:
        return rect.top();
    case Edge::Right:
        return rect.right();
    case Edge::Bottom:
        return rect.bottom();
    }

    return 0;
}

[[nodiscard]] constexpr Distance edge_distance(const Rect& first,
                                                Edge first_edge,
                                                const Rect& second,
                                                Edge second_edge) noexcept {
    return absolute_distance(edge_coordinate(first, first_edge),
                             edge_coordinate(second, second_edge));
}

[[nodiscard]] constexpr Distance interval_overlap(Coordinate first_min,
                                                  Coordinate first_max,
                                                  Coordinate second_min,
                                                  Coordinate second_max) noexcept {
    const auto lower = std::max(std::min(first_min, first_max),
                                std::min(second_min, second_max));
    const auto upper = std::min(std::max(first_min, first_max),
                                std::max(second_min, second_max));
    return std::max<Distance>(0, upper - lower);
}

[[nodiscard]] constexpr Distance horizontal_overlap(const Rect& first,
                                                    const Rect& second) noexcept {
    return interval_overlap(first.left(), first.right(), second.left(), second.right());
}

[[nodiscard]] constexpr Distance vertical_overlap(const Rect& first,
                                                  const Rect& second) noexcept {
    return interval_overlap(first.top(), first.bottom(), second.top(), second.bottom());
}

[[nodiscard]] constexpr bool overlaps(const Rect& first, const Rect& second) noexcept {
    return horizontal_overlap(first, second) > 0 && vertical_overlap(first, second) > 0;
}

[[nodiscard]] constexpr std::optional<Rect> intersection(const Rect& first,
                                                        const Rect& second) noexcept {
    const auto left = std::max(first.left(), second.left());
    const auto top = std::max(first.top(), second.top());
    const auto right = std::min(first.right(), second.right());
    const auto bottom = std::min(first.bottom(), second.bottom());

    if (right <= left || bottom <= top) {
        return std::nullopt;
    }

    return Rect{left, top, right, bottom};
}

[[nodiscard]] constexpr bool within_tolerance(Distance distance,
                                              Distance tolerance) noexcept {
    return distance >= 0 && tolerance >= 0 && distance <= tolerance;
}

} // namespace snapweave::core::geometry

