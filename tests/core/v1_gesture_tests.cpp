#include "core/v1/gesture.h"

#include <iostream>
#include <limits>

namespace v1 = panebind::core::v1;
namespace {
int checks{}, failures{};
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
}
int main() {
    v1::GestureEpoch epoch;
    check(!epoch.active() && !epoch.motion(0) && !epoch.begin(0), "cold state cannot accept zero generation");
    check(epoch.begin(1) && epoch.active() && epoch.generation() == 1 && epoch.motion(1), "first generation active");
    check(!epoch.begin(2) && !epoch.motion(2) && !epoch.finish(2, v1::End::NormalUp) && epoch.active(), "active owner cannot be replaced or ended by foreign event");
    check(epoch.finish(1, v1::End::Escape) && !epoch.active() && epoch.last_end() == v1::End::Escape, "abort retires epoch with actual reason");
    check(!epoch.motion(1) && !epoch.finish(1, v1::End::NormalUp) && !epoch.begin(1) && epoch.last_end() == v1::End::Escape,
          "retired epoch cannot revive or become normal by late UP");
    check(epoch.begin(2) && !epoch.motion(1) && !epoch.finish(1, v1::End::SourceLost) && epoch.motion(2), "new epoch isolated from old motion and destroy");
    check(epoch.finish(2, v1::End::NormalUp) && !epoch.motion(2) && epoch.last_end() == v1::End::NormalUp, "normal UP retires pending motion");
    check(epoch.begin(std::numeric_limits<std::uint64_t>::max()) && epoch.finish(std::numeric_limits<std::uint64_t>::max(), v1::End::Exit) && !epoch.begin(1),
          "generation overflow never wraps and reauthorizes old gesture");
    for (const auto reason : {v1::End::FocusLost, v1::End::SourceLost, v1::End::Paused,
                             v1::End::SessionLost, v1::End::QueueFailure, v1::End::Deadline}) {
        v1::GestureEpoch item;
        check(item.begin(4) && item.finish(4, reason) && !item.motion(4) && item.last_end() == reason,
              "each abnormal termination prevents subsequent work");
    }

    const v1::Rect original{100, 100, 500, 400};
    check(v1::intent(original, {300, 200}, {175, 260}, {}) == v1::Rect{-25, 160, 375, 460}, "G02 fixed anchor move independent expected");
    check(v1::intent(original, {300, 200}, {340, 180}, {}) == v1::Rect{140, 80, 540, 380}, "second move still based on original not last correction");
    check(v1::intent(original, {500, 200}, {594, 310}, {false, false, true, false}) == v1::Rect{100, 100, 594, 400}, "right resize changes active edge only");
    check(v1::intent(original, {100, 100}, {70, 60}, {true, true, false, false}) == v1::Rect{70, 60, 500, 400}, "top left corner fixed opposing edges");
    check(v1::intent(original, {500, 400}, {550, 450}, {false, false, true, true}) == v1::Rect{100, 100, 550, 450}, "bottom right corner resize");
    check(!v1::intent(original, {100, 100}, {500, 100}, {true, false, false, false}), "zero width rejected before Rect normalization");
    check(!v1::intent(original, {100, 100}, {550, 100}, {true, false, false, false}), "crossing active edge rejected not silently flipped");
    check(!v1::intent(original, {}, {}, {true, false, true, false}), "opposing active edges ambiguous rejected");
    check(!v1::intent({}, {}, {}, {}), "empty original rejected");
    constexpr auto lo = std::numeric_limits<std::int64_t>::min();
    constexpr auto hi = std::numeric_limits<std::int64_t>::max();
    check(!v1::intent(original, {lo, 0}, {hi, 0}, {}), "cursor delta overflow rejected");
    check(!v1::intent({hi - 10, 0, hi, 10}, {}, {1, 0}, {}), "placement addition overflow rejected");
    check(!v1::intent({lo, 0, hi, 10}, {}, {}, {}), "unrepresentable original span rejected without signed overflow");
    check(v1::intent({lo, 0, lo + 10, 10}, {}, {1, 0}, {}) == v1::Rect{lo + 1, 0, lo + 11, 10}, "valid signed domain minimum accepted");
    check(!v1::intent({0, 0, hi, 10}, {}, {-1, 0}, {true, false, false, false}), "expanded span overflow rejected");

    v1::ShakeDetector shake;
    check(!shake.sample({0, 0}, 0, 24, 3, 700), "shake starts unarmed");
    check(!shake.sample({30, 0}, 50, 24, 3, 700), "first substantial direction is not reversal");
    check(!shake.sample({0, 0}, 100, 24, 3, 700), "single reversal not shake");
    check(!shake.sample({30, 0}, 150, 24, 3, 700), "two reversals below configured three");
    check(shake.sample({0, 0}, 200, 24, 3, 700), "three amplitude qualified reversals trigger within window");
    check(!shake.sample({30, 0}, 250, 24, 3, 700) && !shake.sample({0, 0}, 300, 24, 3, 700), "once per gesture prevents repeated property toggle");
    shake.reset();
    bool slow_trigger{};
    for (unsigned i = 0; i < 20; ++i)
        slow_trigger |= shake.sample({(i % 2) * 30, 0}, i * 1000, 24, 3, 700);
    check(!slow_trigger, "slow alternating movement not shake");
    shake.reset();
    bool jitter_trigger{};
    for (unsigned i = 0; i < 100; ++i)
        jitter_trigger |= shake.sample({static_cast<int>(i % 2) * 10, 0}, i * 5, 24, 3, 700);
    check(!jitter_trigger, "fast sub-amplitude jitter never accumulates false reversals");
    shake.reset();
    check(!shake.sample({0, 0}, 0, 24, 2, 700) && !shake.sample({0, 30}, 50, 24, 2, 700) &&
          !shake.sample({0, 0}, 100, 24, 2, 700) && shake.sample({0, 30}, 150, 24, 2, 700), "vertical axis trigger independent of horizontal");
    shake.reset();
    check(!shake.sample({0, 0}, 100, 24, 2, 700) && !shake.sample({30, 0}, 150, 24, 2, 700) &&
          !shake.sample({0, 0}, 10, 24, 2, 700) && !shake.sample({30, 0}, 20, 24, 2, 700), "backward timestamp reanchors without invented turns");
    check(!shake.sample({0, 0}, 30, 0, 2, 700) && !shake.sample({30, 0}, 40, 24, 1, 700) &&
          !shake.sample({0, 0}, 50, 24, 99, 700) && !shake.sample({30, 0}, 60, 24, 3, 0), "invalid configuration cannot arm shake");
    shake.reset();
    check(!shake.sample({lo, 0}, 0, 24, 2, 700) && !shake.sample({hi, 0}, 10, 24, 2, 700) &&
          !shake.sample({lo, 0}, 20, 24, 2, 700) && shake.sample({hi, 0}, 30, 24, 2, 700), "amplitude comparisons do not overflow signed coordinate span");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
