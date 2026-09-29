#include "core/behavior/move_magnet_session.h"

#include <array>
#include <iostream>

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

constexpr std::uint64_t generation = 41;
constexpr Point anchor{500, 500};
constexpr Rect initial{128, 20, 228, 160};
const std::array one_target{magnet::MagnetTarget{WindowId{"A"}, {0, 0, 120, 180}, false}};

bool begin_and_handoff(behavior::MoveMagnetSession& session,
                       std::span<const magnet::MagnetTarget> targets = one_target) {
    return session.begin(generation, anchor, initial, targets, 1000, 1000,
                         true, true, true) &&
           session.isolation_ready(generation, true) &&
           session.may_cancel(generation) &&
           session.cancel_issued(generation) &&
           session.native_end_observed(generation, true);
}
}

int main() {
    {
        behavior::MoveMagnetSession session;
        check(!session.begin(generation, anchor, initial, one_target, 1000, 1000,
                             false, true, true), "unknown source cannot begin");
        // The adapter classifies these mutually exclusive native routes and
        // supplies plain_titlebar_move=false for each non-magnet route.
        check(!session.begin(generation, anchor, initial, one_target, 1000, 1000,
                             true, true, false), "Ctrl Move stays on native Glue route");
        check(!session.begin(generation, anchor, initial, one_target, 1000, 1000,
                             true, true, false), "Resize stays on native route");
        check(session.begin(generation, anchor, initial, one_target, 1000, 1000,
                            true, true, true), "exact plain Move begins");
        check(!session.sample_cursor(generation, {502, 500}, 2000, true, true, true) &&
              !session.take_pending(generation, true, true, true),
              "no plan before actual isolation cancel and END");
        check(!session.isolation_ready(generation, false) &&
              session.isolation_ready(generation, true), "hit proof required");
        check(session.cancel_issued(generation) &&
              !session.sample_cursor(generation, {502, 500}, 2000, true, true, true),
              "cancel alone does not make writer ready");
        check(!session.native_end_observed(generation, false) &&
              session.native_end_observed(generation, true), "real matching END required");
        check(session.sample_cursor(generation, {502, 500}, 2000, true, true, true),
              "first post-END cursor quantum accepted");
        const auto enter = session.take_pending(generation, true, true, true);
        check(enter && enter->generation == generation && enter->quantum == 1 &&
              enter->free_visible == Rect{130, 20, 230, 160} &&
              enter->target_visible == Rect{120, 20, 220, 160} && enter->snapped,
              "anchor-based snapped target, one whole target");
        check(!session.take_pending(generation, true, true, true) &&
              !session.sample_cursor(generation, {502, 500}, 2100, true, true, true),
              "one take per quantum and duplicate cursor cannot reissue");
        check(enter && session.write_result(*enter, true, enter->target_visible).exact,
              "exact native result can be postverified");
        check(!session.write_result(*enter, true, enter->target_visible).recognized,
              "duplicate native receipt rejected");
    }
    {
        behavior::MoveMagnetSession session;
        check(begin_and_handoff(session), "hysteresis fixture handoff");
        const auto sample = [&](Point cursor, std::uint64_t qpc) {
            check(session.sample_cursor(generation, cursor, qpc, true, true, true),
                  "changed cursor planned");
            return session.take_pending(generation, true, true, true);
        };
        const auto enter = sample({502, 500}, 2000);
        const auto hold = sample({508, 500}, 3000);
        const auto detach = sample({509, 500}, 4000);
        const auto resnap = sample({501, 500}, 5000);
        check(enter && enter->snapped && enter->target_visible.left() == 120 &&
              hold && hold->snapped && hold->target_visible.left() == 120,
              "attract and hold through 16 px gap");
        check(detach && !detach->snapped &&
              detach->target_visible == detach->free_visible &&
              detach->target_visible.left() == 137,
              "release beyond 16 px continues free follow");
        check(resnap && resnap->snapped && resnap->target_visible.left() == 120 &&
              resnap->free_visible.left() == 129,
              "re-entry resnaps from original anchor, not corrected geometry");
        check(enter && hold && detach && resnap &&
              enter->quantum == 1 && hold->quantum == 2 &&
              detach->quantum == 3 && resnap->quantum == 4,
              "exactly one plan per changed cursor quantum");
    }
    {
        behavior::MoveMagnetSession session;
        check(begin_and_handoff(session, {}), "free-follow fixture handoff");
        check(session.sample_cursor(generation, {550, 500}, 2000, true, true, true),
              "no candidate still plans movement");
        const auto free = session.take_pending(generation, true, true, true);
        check(free && !free->snapped && free->free_visible == Rect{178, 20, 278, 160} &&
              free->target_visible == free->free_visible,
              "free target is the sole placement, not a correction after a write");
    }
    {
        behavior::MoveMagnetSession session;
        check(begin_and_handoff(session), "fast-motion fixture handoff");
        check(session.sample_cursor(generation, {501, 500}, 2000, true, true, true),
              "slow entry planned");
        const auto enter = session.take_pending(generation, true, true, true);
        check(enter && enter->snapped, "slow entry acquires snap");
        check(session.sample_cursor(generation, {507, 500}, 2001, true, true, true),
              "fast cursor still planned");
        const auto fast = session.take_pending(generation, true, true, true);
        check(fast && !fast->snapped &&
              fast->motion == magnet::MotionState::Suppressed &&
              fast->target_visible == fast->free_visible &&
              fast->target_visible.left() == 135,
              "speed suppression does not suppress free movement");
    }
    {
        behavior::MoveMagnetSession session;
        check(begin_and_handoff(session), "pending-UP fixture handoff");
        check(session.sample_cursor(generation, {502, 500}, 2000, true, true, true),
              "pending target before Raw UP");
        check(!session.raw_up_observed(generation + 1, true) &&
              session.raw_up_observed(generation, true), "only matching physical UP retires");
        check(session.retired(generation) &&
              !session.take_pending(generation, true, true, true) &&
              !session.sample_cursor(generation, {503, 500}, 3000, true, true, true),
              "Raw UP revokes pending and forbids new plans");
        check(!session.legacy_up_delivery_observed(generation, false) &&
              session.legacy_up_delivery_observed(generation, true) &&
              session.removal_allowed(generation) &&
              session.isolation_removed(generation),
              "actual legacy delivery is separate from Raw UP");
    }
    {
        behavior::MoveMagnetSession session;
        check(session.begin(generation, anchor, initial, one_target, 1000, 1000,
                            true, true, true), "early-UP fixture begins");
        check(session.raw_up_observed(generation, true) &&
              !session.isolation_ready(generation, true) &&
              !session.may_cancel(generation) &&
              !session.sample_cursor(generation, {502, 500}, 2000, true, true, true),
              "Raw UP before setup prevents cancel and all placement plans");
    }
    {
        behavior::MoveMagnetSession session;
        check(begin_and_handoff(session), "in-flight fixture handoff");
        check(session.sample_cursor(generation, {502, 500}, 2000, true, true, true),
              "in-flight plan made");
        const auto in_flight = session.take_pending(generation, true, true, true);
        check(in_flight && session.raw_up_observed(generation, true),
              "Raw UP after plan taken");
        const auto completion = in_flight ?
            session.write_result(*in_flight, true, in_flight->target_visible) :
            behavior::MoveWriteCompletion{};
        check(completion.recognized && completion.exact &&
              completion.after_retirement && session.retired(generation),
              "in-flight completion is recorded without reviving writer");
    }
    {
        behavior::MoveMagnetSession session;
        check(begin_and_handoff(session), "stale-generation fixture handoff");
        check(!session.sample_cursor(generation - 1, {502, 500}, 2000, true, true, true) &&
              !session.take_pending(generation - 1, true, true, true) &&
              !session.raw_up_observed(generation - 1, true) &&
              !session.retired(generation), "old generation cannot plan or revoke current one");
        check(session.sample_cursor(generation, {502, 500}, 2000, true, true, true) &&
              session.take_pending(generation, true, true, true).has_value(),
              "current generation remains usable after stale input");
    }
    {
        behavior::MoveMagnetSession session;
        check(begin_and_handoff(session), "stop fixture handoff");
        check(session.sample_cursor(generation, {502, 500}, 2000, true, true, true),
              "plan before stop");
        check(session.escape(generation, behavior::MoveHandoffEscapeReason::ExplicitStop) &&
              session.retired(generation) &&
              !session.take_pending(generation, true, true, true) &&
              !session.sample_cursor(generation, {503, 500}, 3000, true, true, true),
              "stop clears plan and forbids further write target");
        check(session.removal_allowed(generation) && session.isolation_removed(generation),
              "escape can remove own isolation without invented UP");
    }
    {
        behavior::MoveMagnetSession session;
        check(begin_and_handoff(session), "authority fixture handoff");
        check(!session.sample_cursor(generation, {502, 500}, 2000, false, true, true) &&
              session.retired(generation) &&
              !session.take_pending(generation, true, true, true),
              "lost fresh authority retires rather than latching old permission");
    }
    {
        behavior::MoveMagnetSession session;
        check(begin_and_handoff(session), "late-context fixture handoff");
        check(session.sample_cursor(generation, {502, 500}, 2000, true, true, true),
              "target formed before take-time context loss");
        check(!session.take_pending(generation, true, true, false) &&
              session.retired(generation) &&
              !session.take_pending(generation, true, true, true),
              "lost overlay hit at take revokes pending target permanently");
    }
    {
        behavior::MoveMagnetSession session;
        check(begin_and_handoff(session), "nonexact fixture handoff");
        check(session.sample_cursor(generation, {502, 500}, 2000, true, true, true),
              "nonexact plan made");
        const auto plan = session.take_pending(generation, true, true, true);
        const auto result = plan ? session.write_result(*plan, true, plan->free_visible) :
                                   behavior::MoveWriteCompletion{};
        check(result.recognized && !result.exact && session.retired(generation) &&
              !session.sample_cursor(generation, {503, 500}, 3000, true, true, true),
              "nonexact postverify retires writer");
    }

    std::cout << "move-magnet-session checks=" << checks << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
