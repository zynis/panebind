#pragma once

#include "core/geometry/magnet_constraint_solver.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace panebind::core::behavior {

// Cursor intent is independent of native LOCATION receipts and window-writing
// authority. The adapter must establish and retire that authority separately.
struct CursorMoveTarget {
    geometry::Rect free_visible;
    geometry::Rect target_visible;
    std::optional<magnet::MagnetProposal> proposal;
    magnet::MotionState motion{magnet::MotionState::BelowThreshold};
    std::string_view reason;

    [[nodiscard]] bool snapped() const noexcept { return proposal.has_value(); }
};

struct CursorMoveCounters {
    std::size_t solver_calls{}, duplicate_cursor{};
};

// Pure, bounded Move target selection. One changed cursor position is one owner
// quantum and calls the existing solver once. Even without a Magnet proposal,
// the full anchor-based free translation remains the target. This class neither
// interprets native feedback nor grants permission to place a window.
class CursorMoveMagnetIntent final {
public:
    [[nodiscard]] bool start(geometry::Point anchor, geometry::Rect initial_visible,
                             std::span<const magnet::MagnetTarget> frozen_targets,
                             std::uint64_t start_qpc, std::uint64_t frequency) {
        if (active_ || !start_qpc || !frequency ||
            !magnet::detail::valid_rect(initial_visible) || frozen_targets.size() > 2) {
            last_status_ = "invalid_start";
            return false;
        }
        for (std::size_t i = 0; i < frozen_targets.size(); ++i) {
            if (frozen_targets[i].screen || !magnet::detail::valid_rect(frozen_targets[i].rect)) {
                last_status_ = "invalid_target_set";
                return false;
            }
            for (std::size_t j = 0; j < i; ++j) {
                if (frozen_targets[i].id == frozen_targets[j].id) {
                    last_status_ = "invalid_target_set";
                    return false;
                }
            }
        }
        targets_.assign(frozen_targets.begin(), frozen_targets.end());
        anchor_ = last_cursor_ = anchor;
        initial_ = last_free_ = initial_visible;
        last_qpc_ = start_qpc;
        frequency_ = frequency;
        preferred_x_.reset();
        preferred_y_.reset();
        counters_ = {};
        active_ = true;
        last_status_ = "started";
        return true;
    }

    [[nodiscard]] std::optional<CursorMoveTarget> sample(geometry::Point cursor,
                                                          std::uint64_t qpc) {
        if (!active_) return std::nullopt;
        if (!qpc || qpc < last_qpc_ || (qpc == last_qpc_ && cursor != last_cursor_)) {
            retire("invalid_tick_order");
            return std::nullopt;
        }
        if (cursor == last_cursor_) {
            last_qpc_ = qpc;
            ++counters_.duplicate_cursor;
            last_status_ = "duplicate_cursor";
            return std::nullopt;
        }
        const auto dx = geometry::checked_difference(cursor.x, anchor_.x);
        const auto dy = geometry::checked_difference(cursor.y, anchor_.y);
        if (!dx || !dy) {
            retire("unrepresentable_cursor_displacement");
            return std::nullopt;
        }
        geometry::Rect free;
        try {
            free = movement::translate_rect(initial_, {*dx, *dy});
        } catch (const std::overflow_error&) {
            retire("unrepresentable_free_geometry");
            return std::nullopt;
        }
        if (!magnet::detail::valid_rect(free)) {
            retire("unrepresentable_free_geometry");
            return std::nullopt;
        }

        magnet::MagnetOptions options;
        options.screen_edges = false;
        options.max_targets = 2;
        const magnet::MotionSample motion{last_free_, last_qpc_, qpc, frequency_, speed_limit};
        const magnet::MagnetInput input{initial_, free, magnet::Interaction::Move, {},
                                        targets_, options, motion, preferred_x_, preferred_y_};
        ++counters_.solver_calls;
        auto solved = magnet::MagnetConstraintSolver::solve(input);
        if (solved.motion == magnet::MotionState::InvalidSample ||
            (solved.motion == magnet::MotionState::BelowThreshold && !solved.proposal &&
             solved.reason != "no_candidate" && solved.reason != "unstable_candidate_pair" &&
             solved.reason != "incompatible_combined_geometry")) {
            retire(solved.motion == magnet::MotionState::InvalidSample ?
                   "invalid_motion_sample" : solved.reason);
            return std::nullopt;
        }

        // Both speed suppression and absence/release of candidates continue
        // free Move. Never compare this free rect with native corrected feedback.
        if (solved.motion != magnet::MotionState::BelowThreshold || !solved.proposal) {
            preferred_x_.reset();
            preferred_y_.reset();
        } else {
            update_preference(preferred_x_, solved.proposal->selected_x);
            update_preference(preferred_y_, solved.proposal->selected_y);
        }
        last_cursor_ = cursor;
        last_free_ = free;
        last_qpc_ = qpc;
        last_status_ = solved.reason;
        const auto target = solved.proposal ? solved.proposal->corrected : free;
        return CursorMoveTarget{free, target, std::move(solved.proposal), solved.motion, solved.reason};
    }

    void release() noexcept { retire("released"); }
    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] std::string_view last_status() const noexcept { return last_status_; }
    [[nodiscard]] const CursorMoveCounters& counters() const noexcept { return counters_; }

    static constexpr geometry::Distance speed_limit = 2000;
    static constexpr geometry::Distance release_distance = 16;

private:
    static void update_preference(std::optional<magnet::MagnetAxisPreference>& old,
                                  const std::optional<magnet::SatisfiedConstraint>& selected) {
        if (!selected) {
            old.reset();
            return;
        }
        if (old && old->target == selected->target && old->kind == selected->kind &&
            old->moving_edge == selected->moving_edge && old->target_edge == selected->target_edge) {
            return;
        }
        old = magnet::MagnetAxisPreference{selected->target, selected->kind,
                                           selected->moving_edge, selected->target_edge,
                                           release_distance};
    }
    void retire(std::string_view reason) noexcept {
        active_ = false;
        preferred_x_.reset();
        preferred_y_.reset();
        last_status_ = reason;
    }

    geometry::Point anchor_, last_cursor_;
    geometry::Rect initial_, last_free_;
    std::uint64_t last_qpc_{}, frequency_{};
    std::vector<magnet::MagnetTarget> targets_;
    std::optional<magnet::MagnetAxisPreference> preferred_x_, preferred_y_;
    CursorMoveCounters counters_;
    std::string_view last_status_{"inactive"};
    bool active_{};
};

} // namespace panebind::core::behavior
