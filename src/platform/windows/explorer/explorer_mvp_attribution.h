#pragma once

#include "core/geometry/geometry.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace panebind::platform::windows::explorer {

// These identifiers must come from a separately consented three-frame seal.
// Raw Input itself does not contain a target HWND or a Shell capability.
struct MvpSourceBinding final {
    std::size_t member{3};
    std::uint64_t window_id{}, capability_generation{}, consent_generation{};
    std::uintptr_t hwnd{};
    std::uint32_t process_id{}, thread_id{};

    friend bool operator==(const MvpSourceBinding&, const MvpSourceBinding&) = default;
};

enum class MvpDownHit { Unknown, Caption, Resize, Other };
enum class MvpCtrlState { Unknown, Up, Down };
enum class MvpGestureRoute { Reject, PlainMoveCandidate, CtrlMove, NativeResize };

// A receiver supplies actual packet/MSG, hit-test, foreground and initial
// geometry observations. The exact-root facts must not be synthesized from a
// fixed test trajectory, a Shell launch command or a guessed foreground HWND.
struct MvpRawDownFacts final {
    std::uint64_t generation{}, packet_sequence{}, receiver_watermark{};
    std::uint32_t message_time{};
    MvpSourceBinding source;
    std::optional<core::geometry::Point> message_point;
    std::uintptr_t hit_root{}, foreground_root{};
    core::geometry::Rect initial_visible, initial_positioning;
    MvpDownHit hit{MvpDownHit::Unknown};
    bool raw_left_down{}, raw_left_up{}, packet_valid{}, receiver_healthy{};
    bool hit_test_succeeded{}, pre_native_gui_clear{};
    bool exact_consent_capture{}, input_desktop_valid{};
};

// One real out-of-context START receipt, not a simulated START or a native
// operation permit. In particular, a matching time/root cannot prove that
// Explorer received the Raw DOWN: the two queues have no common gesture ID.
struct MvpNativeStartFacts final {
    std::uint64_t generation{}, event_sequence{};
    std::uint32_t native_time{};
    MvpSourceBinding source;
    std::uintptr_t event_hwnd{}, foreground_root{}, gui_move_size_hwnd{};
    MvpCtrlState ctrl{MvpCtrlState::Unknown};
    bool exact_win_event{}, object_window_self{}, event_stream_healthy{};
    bool exact_consent_capture{}, gui_in_move_size{}, physical_left_held{};
    bool input_desktop_valid{}, other_modifiers_up{};
};

struct MvpNativeEndFacts final {
    std::uint64_t generation{}, event_sequence{};
    MvpSourceBinding source;
    std::uintptr_t event_hwnd{};
    bool exact_win_event{}, object_window_self{}, event_stream_healthy{};
};

struct MvpGestureAttribution final {
    MvpGestureRoute route{MvpGestureRoute::Reject};
    std::uint64_t generation{}, down_packet_sequence{}, start_event_sequence{};
    MvpSourceBinding source;
    core::geometry::Point original_cursor;
    core::geometry::Rect initial_visible, initial_positioning;

    // A successful classification is still only a candidate. It deliberately
    // contains no cancel/write permit or mutable HWND selector.
};

// One instance belongs to one gesture. Make a fresh instance for every next
// DOWN/generation so a retired anchor or UP cannot leak into a later gesture.
class ExplorerMvpAttribution final {
public:
    [[nodiscard]] bool observe_down(const MvpRawDownFacts& facts) noexcept;
    [[nodiscard]] MvpGestureAttribution observe_start(const MvpNativeStartFacts& facts) noexcept;
    [[nodiscard]] bool observe_end(const MvpNativeEndFacts& facts) noexcept;
    [[nodiscard]] bool observe_raw_up(std::uint64_t generation,
                                      std::uint64_t packet_sequence) noexcept;
    void retire() noexcept;

    [[nodiscard]] bool raw_up_seen() const noexcept { return raw_up_; }
    [[nodiscard]] bool matching_end_seen() const noexcept { return end_seen_; }
    [[nodiscard]] MvpGestureRoute route() const noexcept {
        return raw_up_ || retired_ ? MvpGestureRoute::Reject : decision_.route;
    }

private:
    std::optional<MvpRawDownFacts> down_;
    MvpGestureAttribution decision_;
    bool start_seen_{}, end_seen_{}, raw_up_{}, retired_{};
};

} // namespace panebind::platform::windows::explorer
