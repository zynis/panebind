#pragma once

#include "core/model/window_id.h"
#include "core/movement/translation.h"
#include "core/topology/window_adjacency.h"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <vector>

namespace panebind::core::movement {

struct PlannedTranslation {
    model::WindowId id;
    geometry::Rect target_visible_rect;

    friend bool operator==(const PlannedTranslation&,
                           const PlannedTranslation&) = default;
};

class TranslationSession final {
public:
    TranslationSession(const topology::WindowAdjacencyGraph& graph,
                       const model::WindowId& leader)
        : leader_(leader) {
        const auto component = graph.connected_component(leader_);
        const auto windows = graph.windows();
        initial_component_.reserve(component.size());

        bool leader_found = false;
        for (const auto& id : component) {
            const auto found = std::lower_bound(
                windows.begin(),
                windows.end(),
                id,
                [](const topology::WindowGeometry& window,
                   const model::WindowId& sought) {
                    return window.id.value() < sought.value();
                });
            if (found == windows.end() || !(found->id == id)) {
                throw std::logic_error{"connected component references an unknown window"};
            }

            initial_component_.push_back(*found);
            if (found->id == leader_) {
                initial_leader_visible_rect_ = found->visible_rect;
                leader_found = true;
            }
        }

        if (!leader_found) {
            throw std::logic_error{"connected component does not contain its leader"};
        }
    }

    [[nodiscard]] std::optional<std::vector<PlannedTranslation>>
    plan(const geometry::Rect& current_leader_visible) const {
        const auto change = classify_geometry_change(initial_leader_visible_rect_,
                                                     current_leader_visible);
        if (change.kind == GeometryChangeKind::ResizeOrMixed) {
            return std::nullopt;
        }

        const TranslationDelta delta = change.delta.value_or(TranslationDelta{});
        std::vector<PlannedTranslation> result;
        result.reserve(initial_component_.size() - 1);

        for (const auto& window : initial_component_) {
            if (window.id == leader_) {
                continue;
            }

            const auto target = change.kind == GeometryChangeKind::Unchanged
                                    ? window.visible_rect
                                    : translate_rect(window.visible_rect, delta);
            result.push_back({window.id, target});
        }

        return result;
    }

private:
    model::WindowId leader_;
    geometry::Rect initial_leader_visible_rect_;
    std::vector<topology::WindowGeometry> initial_component_;
};

} // namespace panebind::core::movement
