#pragma once

#include "core/v1/types.h"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>

namespace panebind::core::v1 {

// Owned by the serialized production dispatcher, not the native callback.
// A retired generation can never rearm, and a late event cannot terminate a
// newer generation. Platform identity/permission remains an adapter concern.
class GestureEpoch {
public:
    bool begin(std::uint64_t id) noexcept {
        if (active_ || id == 0 || id <= generation_) return false;
        generation_ = id;
        active_ = true;
        return true;
    }
    bool motion(std::uint64_t id) const noexcept { return active_ && id == generation_; }
    bool finish(std::uint64_t id, End why) noexcept {
        if (!motion(id)) return false;
        active_ = false;
        last_end_ = why;
        return true;
    }
    bool active() const noexcept { return active_; }
    std::uint64_t generation() const noexcept { return generation_; }
    // Meaningful after a successful finish. Exit is the cold-start sentinel.
    End last_end() const noexcept { return last_end_; }
private:
    std::uint64_t generation_{};
    bool active_{};
    End last_end_{End::Exit};
};

namespace gesture_detail {
inline std::optional<std::int64_t> add(std::int64_t a, std::int64_t b) noexcept {
    constexpr auto lo = std::numeric_limits<std::int64_t>::min();
    constexpr auto hi = std::numeric_limits<std::int64_t>::max();
    if ((b > 0 && a > hi - b) || (b < 0 && a < lo - b)) return std::nullopt;
    return a + b;
}
inline std::optional<std::int64_t> subtract(std::int64_t a, std::int64_t b) noexcept {
    constexpr auto lo = std::numeric_limits<std::int64_t>::min();
    constexpr auto hi = std::numeric_limits<std::int64_t>::max();
    if ((b > 0 && a < lo + b) || (b < 0 && a > hi + b)) return std::nullopt;
    return a - b;
}
inline bool representable(const Rect& rect) noexcept {
    // Do not call Rect::width/height before proving the span representable.
    return rect.left() < rect.right() && rect.top() < rect.bottom() &&
        subtract(rect.right(), rect.left()).has_value() &&
        subtract(rect.bottom(), rect.top()).has_value();
}
inline std::uint64_t distance(std::int64_t a, std::int64_t b) noexcept {
    // Unsigned subtraction represents even the complete signed-domain span
    // without signed overflow. It is used only for positive threshold tests.
    return a >= b ? static_cast<std::uint64_t>(a) - static_cast<std::uint64_t>(b) :
                    static_cast<std::uint64_t>(b) - static_cast<std::uint64_t>(a);
}
} // namespace gesture_detail

// Absolute proposal from the original DOWN anchors. No accumulated snap
// corrections enter this function. Empty edges mean Move; opposing active
// edges on one axis are ambiguous and rejected instead of normalizing flips.
inline std::optional<Rect> intent(Rect original, Point down, Point now, Edges edges) noexcept {
    using namespace gesture_detail;
    if (!representable(original) || (edges.left && edges.right) || (edges.top && edges.bottom)) return std::nullopt;
    const auto dx = subtract(now.x, down.x), dy = subtract(now.y, down.y);
    if (!dx || !dy) return std::nullopt;
    const bool move = !edges.left && !edges.right && !edges.top && !edges.bottom;
    const auto left = add(original.left(), move || edges.left ? *dx : 0);
    const auto right = add(original.right(), move || edges.right ? *dx : 0);
    const auto top = add(original.top(), move || edges.top ? *dy : 0);
    const auto bottom = add(original.bottom(), move || edges.bottom ? *dy : 0);
    if (!left || !right || !top || !bottom || *left >= *right || *top >= *bottom) return std::nullopt;
    const Rect result{*left, *top, *right, *bottom};
    return representable(result) ? std::optional{result} : std::nullopt;
}

// Original PaneBind detector: amplitude-qualified alternating turns on either
// axis, counted only within the configured time window. There is no sampling
// timer. The input adapter supplies real cursor events only during valid Move.
// At most 20 turns per axis are retained; no history grows with event rate.
class ShakeDetector {
public:
    bool sample(Point cursor, std::uint64_t ms, int amplitude,
                unsigned reversals, std::uint64_t window_ms) noexcept {
        if (triggered_) return false;
        if (amplitude <= 0 || reversals < 2 || reversals > 20 || window_ms == 0 || window_ms > 10000) {
            reset(); return false;
        }
        if (!initialized_ || ms < last_ms_ || amplitude != amplitude_ ||
            reversals != required_ || window_ms != window_ms_) {
            reset();
            initialized_ = true;
            axes_[0].extreme = cursor.x;
            axes_[1].extreme = cursor.y;
            amplitude_ = amplitude; required_ = reversals; window_ms_ = window_ms;
            last_ms_ = ms;
            return false;
        }
        last_ms_ = ms;
        const bool x = turn(axes_[0], cursor.x, ms);
        const bool y = turn(axes_[1], cursor.y, ms);
        if (x || y) { triggered_ = true; return true; }
        return false;
    }
    void reset() noexcept { *this = ShakeDetector{}; }
private:
    struct Axis {
        std::int64_t extreme{};
        int direction{};
        std::array<std::uint64_t, 20> turns{};
        unsigned count{};
    };
    bool turn(Axis& axis, std::int64_t value, std::uint64_t ms) noexcept {
        using gesture_detail::distance;
        unsigned keep{};
        for (unsigned i = 0; i < axis.count; ++i)
            if (ms - axis.turns[i] <= window_ms_) axis.turns[keep++] = axis.turns[i];
        axis.count = keep;
        if (axis.direction == 0) {
            if (distance(value, axis.extreme) >= static_cast<std::uint64_t>(amplitude_)) {
                axis.direction = value > axis.extreme ? 1 : -1;
                axis.extreme = value;
            }
            return false;
        }
        if ((axis.direction > 0 && value >= axis.extreme) ||
            (axis.direction < 0 && value <= axis.extreme)) {
            axis.extreme = value;
            return false;
        }
        if (distance(value, axis.extreme) < static_cast<std::uint64_t>(amplitude_)) return false;
        axis.direction = -axis.direction;
        axis.extreme = value;
        if (axis.count == axis.turns.size()) {
            for (unsigned i = 1; i < axis.count; ++i) axis.turns[i - 1] = axis.turns[i];
            --axis.count;
        }
        axis.turns[axis.count++] = ms;
        return axis.count >= required_;
    }
    std::array<Axis, 2> axes_{};
    std::uint64_t last_ms_{}, window_ms_{};
    int amplitude_{};
    unsigned required_{};
    bool initialized_{}, triggered_{};
};

} // namespace panebind::core::v1
