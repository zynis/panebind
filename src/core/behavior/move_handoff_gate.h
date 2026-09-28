#pragma once

#include <cstdint>
namespace panebind::core::behavior {

enum class MoveHandoffEscapeReason {
    None, EarlyUp, SetupFailure, ExplicitStop, ContextLost, ParentExited,
    Deadline, LegacyUpMissing, NativeFailure
};

// Ordering gate only. The adapter must independently prove every native fact
// supplied here, including source identity, DOWN attribution, actual overlay
// hit, matching native END and fresh per-quantum write authority. This class
// never grants native-window capability by itself.
class MoveHandoffGate final {
public:
    [[nodiscard]] bool begin(std::uint64_t gesture, bool exact_source,
                             bool attributed_down_held, bool plain_titlebar_move) noexcept {
        if (begun_ || !gesture || !exact_source || !attributed_down_held ||
            !plain_titlebar_move) return false;
        begun_ = true;
        gesture_ = gesture;
        return true;
    }

    [[nodiscard]] bool isolation_ready(std::uint64_t gesture,
                                       bool actual_hit_and_context_proven) noexcept {
        if (!current(gesture) || isolated_ || cancel_ || native_end_ || raw_up_ ||
            !actual_hit_and_context_proven) return false;
        isolated_ = true;
        return true;
    }

    [[nodiscard]] bool may_cancel(std::uint64_t gesture) const noexcept {
        return current(gesture) && isolated_ && !cancel_ && !native_end_ && !raw_up_;
    }

    [[nodiscard]] bool cancel_issued(std::uint64_t gesture) noexcept {
        if (!may_cancel(gesture)) return false;
        cancel_ = true;
        return true;
    }

    [[nodiscard]] bool native_end_observed(std::uint64_t gesture,
                                           bool matching_end) noexcept {
        if (!current(gesture) || native_end_ || !matching_end) return false;
        native_end_ = true;
        return true;
    }

    // A true result is only an ordering permit for this one owner quantum.
    // The adapter must redo all authority/context/geometry checks before its
    // one native placement; never latch the booleans across quanta.
    [[nodiscard]] bool may_write(std::uint64_t gesture, bool fresh_authority,
                                 bool actual_stable, bool isolation_still_hits) const noexcept {
        return current(gesture) && isolated_ && cancel_ && native_end_ && !raw_up_ &&
            fresh_authority && actual_stable && isolation_still_hits;
    }

    [[nodiscard]] bool raw_up_observed(std::uint64_t gesture,
                                       bool matching_physical_up) noexcept {
        if (!current(gesture) || raw_up_ || !matching_physical_up) return false;
        raw_up_ = true; // revokes every later write, including queued motion
        return true;
    }

    // Delivery proof must come from an actual message/observation, not from
    // Raw UP or a presumed order between two message queues.
    [[nodiscard]] bool legacy_up_delivery_observed(std::uint64_t gesture,
                                                    bool actual_delivery_proven) noexcept {
        if (!current(gesture) || !isolated_ || legacy_up_ || !actual_delivery_proven)
            return false;
        legacy_up_ = true;
        return true;
    }

    [[nodiscard]] bool normal_removal_allowed(std::uint64_t gesture) const noexcept {
        return current(gesture) && isolated_ && raw_up_ && legacy_up_;
    }

    // Explicit stop, context loss, parent exit, initialization failure or a
    // one-shot deadline prioritizes desktop recovery. Missing legacy-UP proof
    // remains an evidence failure/UNKNOWN, not a normal acceptance.
    [[nodiscard]] bool escape(std::uint64_t gesture, MoveHandoffEscapeReason reason) noexcept {
        if (!current(gesture) || reason == MoveHandoffEscapeReason::None) return false;
        escaped_ = true;
        escape_reason_ = reason;
        return true;
    }

    [[nodiscard]] bool removal_allowed(std::uint64_t gesture) const noexcept {
        return same_gesture(gesture) && isolated_ &&
            (normal_removal_allowed(gesture) || escaped_);
    }

    [[nodiscard]] bool isolation_removed(std::uint64_t gesture) noexcept {
        if (!removal_allowed(gesture)) return false;
        removed_ = true;
        return true;
    }

    [[nodiscard]] bool begun() const noexcept { return begun_; }
    [[nodiscard]] bool isolated() const noexcept { return isolated_; }
    [[nodiscard]] bool cancel_sent() const noexcept { return cancel_; }
    [[nodiscard]] bool end_seen() const noexcept { return native_end_; }
    [[nodiscard]] bool raw_up_seen() const noexcept { return raw_up_; }
    [[nodiscard]] bool legacy_up_seen() const noexcept { return legacy_up_; }
    [[nodiscard]] bool escaped() const noexcept { return escaped_; }
    [[nodiscard]] bool removed() const noexcept { return removed_; }
    [[nodiscard]] MoveHandoffEscapeReason escape_reason() const noexcept { return escape_reason_; }

private:
    [[nodiscard]] bool same_gesture(std::uint64_t gesture) const noexcept {
        return begun_ && gesture_ == gesture && !removed_;
    }
    [[nodiscard]] bool current(std::uint64_t gesture) const noexcept {
        return same_gesture(gesture) && !escaped_;
    }

    std::uint64_t gesture_{};
    bool begun_{}, isolated_{}, cancel_{}, native_end_{}, raw_up_{}, legacy_up_{};
    bool escaped_{}, removed_{};
    MoveHandoffEscapeReason escape_reason_{MoveHandoffEscapeReason::None};
};

} // namespace panebind::core::behavior
