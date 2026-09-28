#include "core/behavior/cursor_move_magnet_intent.h"

#include <array>
#include <iostream>
#include <limits>

namespace behavior = panebind::core::behavior;
namespace geometry = panebind::core::geometry;
namespace magnet = panebind::core::magnet;
using geometry::Point;
using geometry::Rect;
using panebind::core::model::WindowId;

namespace {
int checks{}, failures{};
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL " << name << '\n';
    }
}
magnet::MagnetTarget target(const char* id, Rect rect) {
    return {WindowId{id}, rect, false};
}
}

int main() {
    const std::array targets{
        target("A", {0, 0, 120, 180}),
        target("C", {20, 180, 300, 300}),
    };
    const Point anchor{500, 500};

    {
        behavior::CursorMoveMagnetIntent intent;
        check(intent.start(anchor, {150, 20, 250, 160}, targets, 1000, 1000), "start frozen pair");
        const auto first = intent.sample({476, 516}, 2000);
        check(first && first->free_visible == Rect{126, 36, 226, 176} &&
              first->target_visible == Rect{120, 40, 220, 180} && first->snapped(),
              "full displacement from DOWN anchor and XY snap");
        check(intent.counters().solver_calls == 1, "one solver on first cursor quantum");
        check(!intent.sample({476, 516}, 2100) && intent.active() &&
              intent.last_status() == "duplicate_cursor" && intent.counters().solver_calls == 1,
              "duplicate cursor is not native raw reassertion");
        const auto next = intent.sample({480, 512}, 3100);
        check(next && next->free_visible == Rect{130, 32, 230, 172} &&
              next->target_visible == Rect{120, 40, 220, 180} && intent.counters().solver_calls == 2,
              "next free intent remains anchor-based, not corrected-based");
        const auto back = intent.sample(anchor, 4100);
        check(back && back->free_visible == Rect{150, 20, 250, 160} && intent.active(),
              "return to initial cursor is not a native raw reassertion");
        intent.release();
        check(!intent.active() && !intent.sample({475, 515}, 5100) &&
              intent.counters().solver_calls == 3, "UP retires all further targets");
    }
    {
        behavior::CursorMoveMagnetIntent intent;
        check(intent.start(anchor, {150, 20, 250, 160}, targets, 1000, 1000), "free start");
        const auto first = intent.sample({550, 500}, 2000);
        const auto next = intent.sample({560, 500}, 3000);
        check(first && first->free_visible == Rect{200, 20, 300, 160} &&
              first->target_visible == first->free_visible && !first->snapped(),
              "no candidate still emits free target");
        check(next && next->free_visible == Rect{210, 20, 310, 160} &&
              next->target_visible == next->free_visible && !next->snapped() &&
              intent.counters().solver_calls == 2, "continuous unsnapped movement");
    }
    {
        behavior::CursorMoveMagnetIntent intent;
        const std::array one{target("A", {0, 0, 120, 180})};
        check(intent.start(anchor, {128, 20, 228, 160}, one, 1000, 1000), "hysteresis start");
        const auto enter = intent.sample({502, 500}, 2000); // 10 px gap
        const auto hold13 = intent.sample({505, 500}, 3000); // 13 px gap
        const auto hold15 = intent.sample({507, 500}, 4000); // 15 px gap
        const auto hold16 = intent.sample({508, 500}, 5000); // 16 px gap
        const auto release17 = intent.sample({509, 500}, 6000); // 17 px gap
        check(enter && enter->snapped() && enter->target_visible.left() == 120,
              "10 px attraction acquires");
        check(hold13 && hold13->snapped() && hold13->target_visible.left() == 120 &&
              hold15 && hold15->snapped() && hold15->target_visible.left() == 120 &&
              hold16 && hold16->snapped() && hold16->target_visible.left() == 120,
              "16 px preference retains through boundary");
        check(release17 && !release17->snapped() &&
              release17->free_visible.left() == 137 &&
              release17->target_visible == release17->free_visible,
              "17 px detach continues free movement");
        check(intent.counters().solver_calls == 5, "one solver per distinct cursor quantum");
    }
    {
        behavior::CursorMoveMagnetIntent intent;
        const std::array one{target("A", {0, 0, 120, 180})};
        check(intent.start(anchor, {128, 20, 228, 160}, one, 1000, 1000), "speed start");
        const auto enter = intent.sample({501, 500}, 2000);
        const auto fast = intent.sample({507, 500}, 2001);
        const auto unlatched = intent.sample({508, 500}, 3001);
        const auto reacquire = intent.sample({501, 500}, 4001);
        check(enter && enter->snapped(), "speed fixture acquired");
        check(fast && fast->motion == magnet::MotionState::Suppressed &&
              !fast->snapped() && fast->target_visible == fast->free_visible &&
              fast->free_visible.left() == 135,
              "fast motion suppresses magnet, not free movement");
        check(unlatched && !unlatched->snapped() && unlatched->free_visible.left() == 136,
              "fast motion released the latch");
        check(reacquire && reacquire->snapped() && reacquire->target_visible.left() == 120,
              "slower re-entry reacquires");
        check(intent.counters().solver_calls == 4, "speed path still one solver per quantum");
    }
    {
        behavior::CursorMoveMagnetIntent intent;
        std::array one{target("A", {0, 0, 120, 180})};
        check(intent.start(anchor, {128, 20, 228, 160}, one, 1000, 1000), "snapshot start");
        one[0].rect = {1000, 0, 1120, 180};
        const auto out = intent.sample({501, 500}, 2000);
        check(out && out->snapped() && out->target_visible.left() == 120,
              "target rect is frozen at start");
    }
    {
        behavior::CursorMoveMagnetIntent intent;
        const std::array one{target("A", {0, 0, 120, 180})};
        const auto max = std::numeric_limits<geometry::Coordinate>::max();
        const auto min = std::numeric_limits<geometry::Coordinate>::min();
        check(!intent.start(anchor, {min, 0, max, 100}, one, 1000, 1000) &&
              !intent.active(), "unrepresentable initial extent rejected");
        check(intent.start(anchor, {max - 20, 0, max - 10, 10}, one, 1000, 1000),
              "near-boundary start");
        check(!intent.sample({530, 500}, 2000) && !intent.active() &&
              intent.last_status() == "unrepresentable_free_geometry" &&
              intent.counters().solver_calls == 0,
              "translated coordinate overflow retires before solver");
        check(intent.start({min, 500}, {128, 20, 228, 160}, one, 3000, 1000),
              "wide anchor start");
        check(!intent.sample({max, 500}, 4000) && !intent.active() &&
              intent.last_status() == "unrepresentable_cursor_displacement",
              "cursor delta overflow retires");
        check(intent.start(anchor, {128, 20, 228, 160}, one, 5000,
                           std::numeric_limits<std::uint64_t>::max()),
              "near-boundary motion start");
        check(!intent.sample({502, 500}, 5001) && !intent.active() &&
              intent.last_status() == "invalid_motion_sample" &&
              intent.counters().solver_calls == 1,
              "velocity multiplication overflow retires after one solver");
    }
    {
        behavior::CursorMoveMagnetIntent intent;
        const std::array one{target("A", {0, 0, 120, 180})};
        check(intent.start(anchor, {128, 20, 228, 160}, one, 1000, 1000), "order start");
        check(!intent.sample({501, 500}, 1000) && !intent.active() &&
              intent.last_status() == "invalid_tick_order" &&
              intent.counters().solver_calls == 0, "same-tick changed cursor retires");
        check(!intent.sample({502, 500}, 2000), "invalid session emits no later target");
    }
    {
        behavior::CursorMoveMagnetIntent intent;
        const std::array duplicate{target("A", {0, 0, 120, 180}), target("A", {200, 0, 300, 180})};
        const std::array screen{magnet::MagnetTarget{WindowId{"screen"}, {0, 0, 1000, 800}, true}};
        const std::array three{target("A", {0, 0, 120, 180}), target("B", {200, 0, 300, 180}),
                               target("C", {400, 0, 500, 180})};
        check(!intent.start(anchor, {128, 20, 228, 160}, duplicate, 1000, 1000),
              "duplicate frozen target rejected");
        check(!intent.start(anchor, {128, 20, 228, 160}, screen, 1000, 1000),
              "screen edge not authorized");
        check(!intent.start(anchor, {128, 20, 228, 160}, three, 1000, 1000),
              "more than two targets rejected");
        check(!intent.start(anchor, {128, 20, 228, 160}, std::span<const magnet::MagnetTarget>{}, 1000, 0),
              "zero frequency rejected");
    }

    std::cout << "cursor-move-magnet-intent checks=" << checks << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
