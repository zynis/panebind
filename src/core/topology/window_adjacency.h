#pragma once

#include "core/geometry/checked_arithmetic.h"
#include "core/geometry/geometry.h"
#include "core/model/window_id.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace panebind::core::topology {

struct WindowGeometry {
    model::WindowId id;
    geometry::Rect visible_rect;

    friend bool operator==(const WindowGeometry&, const WindowGeometry&) = default;
};

struct AdjacencyOptions {
    geometry::Distance edge_tolerance{};

    friend constexpr bool operator==(const AdjacencyOptions&,
                                     const AdjacencyOptions&) = default;
};

struct AdjacencyRelation {
    model::WindowId first;
    model::WindowId second;
    geometry::Edge first_edge;
    geometry::Edge second_edge;
    geometry::Distance signed_gap;
    geometry::Distance orthogonal_overlap;

    friend bool operator==(const AdjacencyRelation&, const AdjacencyRelation&) = default;
};

namespace detail {

[[nodiscard]] inline bool id_less(const model::WindowId& first,
                                  const model::WindowId& second) noexcept {
    return first.value() < second.value();
}

[[nodiscard]] constexpr bool rect_is_empty(const geometry::Rect& rect) noexcept {
    return rect.left() == rect.right() || rect.top() == rect.bottom();
}

[[nodiscard]] inline geometry::Distance positive_interval_overlap(
    geometry::Coordinate first_min,
    geometry::Coordinate first_max,
    geometry::Coordinate second_min,
    geometry::Coordinate second_max) {
    const auto lower = std::max(first_min, second_min);
    const auto upper = std::min(first_max, second_max);
    if (upper <= lower) {
        return 0;
    }

    const auto overlap = geometry::checked_difference(upper, lower);
    if (!overlap.has_value()) {
        throw std::overflow_error{"orthogonal overlap is not representable"};
    }

    return *overlap;
}

} // namespace detail

[[nodiscard]] inline std::optional<AdjacencyRelation>
find_adjacency(const WindowGeometry& first_input,
               const WindowGeometry& second_input,
               const AdjacencyOptions& options) {
    if (options.edge_tolerance < 0) {
        throw std::invalid_argument{"edge tolerance must not be negative"};
    }
    if (first_input.id == second_input.id) {
        throw std::invalid_argument{"adjacency requires distinct WindowId values"};
    }
    if (detail::rect_is_empty(first_input.visible_rect) ||
        detail::rect_is_empty(second_input.visible_rect)) {
        return std::nullopt;
    }

    const auto* first = &first_input;
    const auto* second = &second_input;
    if (detail::id_less(second->id, first->id)) {
        std::swap(first, second);
    }

    std::optional<AdjacencyRelation> candidate;
    bool multiple_candidates = false;

    const auto consider = [&](geometry::Edge first_edge,
                              geometry::Edge second_edge,
                              geometry::Coordinate gap_minuend,
                              geometry::Coordinate gap_subtrahend,
                              geometry::Distance orthogonal_overlap) {
        if (orthogonal_overlap <= 0 ||
            !geometry::magnitude_within(gap_minuend,
                                        gap_subtrahend,
                                        options.edge_tolerance)) {
            return;
        }

        const auto signed_gap =
            geometry::checked_difference(gap_minuend, gap_subtrahend);
        if (!signed_gap.has_value()) {
            return;
        }

        AdjacencyRelation relation{first->id,
                                   second->id,
                                   first_edge,
                                   second_edge,
                                   *signed_gap,
                                   orthogonal_overlap};
        if (candidate.has_value()) {
            multiple_candidates = true;
            return;
        }

        candidate = std::move(relation);
    };

    const auto vertical_overlap = detail::positive_interval_overlap(
        first->visible_rect.top(),
        first->visible_rect.bottom(),
        second->visible_rect.top(),
        second->visible_rect.bottom());
    const auto horizontal_overlap = detail::positive_interval_overlap(
        first->visible_rect.left(),
        first->visible_rect.right(),
        second->visible_rect.left(),
        second->visible_rect.right());

    // first.Right <-> second.Left: second.left - first.right
    consider(geometry::Edge::Right,
             geometry::Edge::Left,
             second->visible_rect.left(),
             first->visible_rect.right(),
             vertical_overlap);
    // first.Left <-> second.Right: first.left - second.right
    consider(geometry::Edge::Left,
             geometry::Edge::Right,
             first->visible_rect.left(),
             second->visible_rect.right(),
             vertical_overlap);
    // first.Bottom <-> second.Top: second.top - first.bottom
    consider(geometry::Edge::Bottom,
             geometry::Edge::Top,
             second->visible_rect.top(),
             first->visible_rect.bottom(),
             horizontal_overlap);
    // first.Top <-> second.Bottom: first.top - second.bottom
    consider(geometry::Edge::Top,
             geometry::Edge::Bottom,
             first->visible_rect.top(),
             second->visible_rect.bottom(),
             horizontal_overlap);

    return multiple_candidates ? std::nullopt : candidate;
}

class WindowAdjacencyGraph final {
public:
    [[nodiscard]] static WindowAdjacencyGraph
    build(std::span<const WindowGeometry> input, const AdjacencyOptions& options) {
        if (options.edge_tolerance < 0) {
            throw std::invalid_argument{"edge tolerance must not be negative"};
        }

        std::vector<WindowGeometry> windows{input.begin(), input.end()};
        std::sort(windows.begin(), windows.end(), [](const auto& first, const auto& second) {
            return detail::id_less(first.id, second.id);
        });

        for (std::size_t index = 1; index < windows.size(); ++index) {
            if (windows[index - 1].id == windows[index].id) {
                throw std::invalid_argument{"WindowId values must be unique within a graph"};
            }
        }

        std::vector<AdjacencyRelation> relations;
        for (std::size_t first = 0; first < windows.size(); ++first) {
            for (std::size_t second = first + 1; second < windows.size(); ++second) {
                auto relation = find_adjacency(windows[first], windows[second], options);
                if (relation.has_value()) {
                    relations.push_back(std::move(*relation));
                }
            }
        }

        return WindowAdjacencyGraph{std::move(windows), std::move(relations)};
    }

    [[nodiscard]] std::span<const WindowGeometry> windows() const noexcept {
        return {windows_.data(), windows_.size()};
    }

    [[nodiscard]] std::span<const AdjacencyRelation> relations() const noexcept {
        return {relations_.data(), relations_.size()};
    }

    [[nodiscard]] std::vector<model::WindowId>
    connected_component(const model::WindowId& start) const {
        const auto start_index = index_of(start);
        if (!start_index.has_value()) {
            throw std::invalid_argument{"connected-component start WindowId is unknown"};
        }

        std::vector<bool> visited(windows_.size(), false);
        std::vector<std::size_t> pending{*start_index};
        visited[*start_index] = true;

        for (std::size_t cursor = 0; cursor < pending.size(); ++cursor) {
            const auto& current = windows_[pending[cursor]].id;
            for (const auto& relation : relations_) {
                const model::WindowId* neighbor = nullptr;
                if (relation.first == current) {
                    neighbor = &relation.second;
                } else if (relation.second == current) {
                    neighbor = &relation.first;
                }

                if (neighbor == nullptr) {
                    continue;
                }

                const auto neighbor_index = index_of(*neighbor);
                if (!neighbor_index.has_value()) {
                    throw std::logic_error{"adjacency relation references an unknown node"};
                }
                if (!visited[*neighbor_index]) {
                    visited[*neighbor_index] = true;
                    pending.push_back(*neighbor_index);
                }
            }
        }

        std::vector<model::WindowId> component;
        component.reserve(pending.size());
        for (std::size_t index = 0; index < windows_.size(); ++index) {
            if (visited[index]) {
                component.push_back(windows_[index].id);
            }
        }
        return component;
    }

private:
    WindowAdjacencyGraph(std::vector<WindowGeometry> windows,
                         std::vector<AdjacencyRelation> relations)
        : windows_(std::move(windows)), relations_(std::move(relations)) {}

    [[nodiscard]] std::optional<std::size_t>
    index_of(const model::WindowId& id) const noexcept {
        const auto found = std::lower_bound(
            windows_.begin(), windows_.end(), id, [](const auto& window, const auto& sought) {
                return detail::id_less(window.id, sought);
            });
        if (found == windows_.end() || !(found->id == id)) {
            return std::nullopt;
        }

        return static_cast<std::size_t>(std::distance(windows_.begin(), found));
    }

    std::vector<WindowGeometry> windows_;
    std::vector<AdjacencyRelation> relations_;
};

} // namespace panebind::core::topology
