#include "platform/windows/explorer/explorer_mvp_attribution.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace e = panebind::platform::windows::explorer;
namespace g = panebind::core::geometry;

namespace {
unsigned checks{};
void check(bool value, std::string_view name) {
    ++checks;
    if (!value) throw std::runtime_error{std::string{name}};
}

e::MvpSourceBinding binding() {
    return {1, 71, 73, 79, 0x1234, 101, 103};
}
e::MvpRawDownFacts down(std::uint64_t generation = 11) {
    e::MvpRawDownFacts result;
    result.generation = generation;
    result.packet_sequence = 9;
    result.receiver_watermark = 8;
    result.message_time = 100;
    result.source = binding();
    result.message_point = g::Point{120, 130};
    result.hit_root = result.foreground_root = result.source.hwnd;
    result.initial_visible = {100, 100, 300, 300};
    result.initial_positioning = {95, 95, 305, 305};
    result.hit = e::MvpDownHit::Caption;
    result.raw_left_down = result.packet_valid = result.receiver_healthy = true;
    result.hit_test_succeeded = result.pre_native_gui_clear = true;
    result.exact_consent_capture = result.input_desktop_valid = true;
    return result;
}
e::MvpNativeStartFacts start(std::uint64_t generation = 11) {
    e::MvpNativeStartFacts result;
    result.generation = generation;
    result.event_sequence = 17;
    result.native_time = 101;
    result.source = binding();
    result.event_hwnd = result.foreground_root = result.gui_move_size_hwnd = result.source.hwnd;
    result.ctrl = e::MvpCtrlState::Up;
    result.exact_win_event = result.object_window_self = result.event_stream_healthy = true;
    result.exact_consent_capture = result.gui_in_move_size = result.physical_left_held = true;
    result.input_desktop_valid = result.other_modifiers_up = true;
    return result;
}
e::MvpNativeEndFacts end(std::uint64_t generation = 11) {
    return {generation, 18, binding(), binding().hwnd, true, true, true};
}

void classification_and_frozen_down_anchor() {
    e::ExplorerMvpAttribution gesture;
    auto first_down = down();
    check(gesture.observe_down(first_down), "exact actual DOWN accepted as candidate");
    first_down.message_point = g::Point{999, 999};
    first_down.initial_visible = {0, 0, 10, 10};
    const auto decision = gesture.observe_start(start());
    check(decision.route == e::MvpGestureRoute::PlainMoveCandidate, "ordinary Move candidate");
    check(decision.original_cursor == g::Point{120, 130}, "original DOWN cursor frozen");
    check(decision.initial_visible == g::Rect{100, 100, 300, 300}, "original DOWN frame frozen");
    check(decision.source == binding() && decision.generation == 11, "exact generation and binding");
    check(gesture.observe_end(end()), "actual matching END witnessed separately");
    check(gesture.matching_end_seen() && !gesture.raw_up_seen(), "END is not physical UP");
    check(gesture.observe_raw_up(11, 10), "later Raw UP observed");
    check(gesture.raw_up_seen(), "UP stored as separate revocation fact");
    check(gesture.route() == e::MvpGestureRoute::Reject,
          "UP revokes current route without rewriting historical decision");
}

void exclusive_routes() {
    e::ExplorerMvpAttribution ctrl;
    check(ctrl.observe_down(down()), "Ctrl DOWN accepted");
    auto ctrl_start = start();
    ctrl_start.ctrl = e::MvpCtrlState::Down;
    check(ctrl.observe_start(ctrl_start).route == e::MvpGestureRoute::CtrlMove,
          "Ctrl at START chooses native Glue route, not magnet");

    e::ExplorerMvpAttribution resize;
    auto resize_down = down();
    resize_down.hit = e::MvpDownHit::Resize;
    check(resize.observe_down(resize_down), "border DOWN accepted");
    check(resize.observe_start(start()).route == e::MvpGestureRoute::NativeResize,
          "ordinary Resize remains native");

    e::ExplorerMvpAttribution ctrl_resize;
    check(ctrl_resize.observe_down(resize_down), "Ctrl border DOWN accepted");
    check(ctrl_resize.observe_start(ctrl_start).route == e::MvpGestureRoute::NativeResize,
          "Ctrl Resize does not enable source magnet or follower writer");
}

void missing_permission_and_ambiguous_identity_fail_closed() {
    auto no_consent = down();
    no_consent.exact_consent_capture = false;
    e::ExplorerMvpAttribution missing;
    check(!missing.observe_down(no_consent), "missing exact consent rejects DOWN");
    check(missing.observe_start(start()).route == e::MvpGestureRoute::Reject,
          "rejected DOWN cannot become START authority");

    auto wrong_hit = down();
    wrong_hit.hit_root = 0x9876;
    e::ExplorerMvpAttribution hit;
    check(!hit.observe_down(wrong_hit), "foreign hit root rejects DOWN");

    e::ExplorerMvpAttribution wrong_start;
    check(wrong_start.observe_down(down()), "candidate before mismatched START");
    auto foreign = start();
    foreign.source.process_id++;
    check(wrong_start.observe_start(foreign).route == e::MvpGestureRoute::Reject,
          "START with changed exact binding rejects");
    check(wrong_start.observe_start(start()).route == e::MvpGestureRoute::Reject,
          "failed attribution cannot be repaired by later START");

    e::ExplorerMvpAttribution unknown_ctrl;
    check(unknown_ctrl.observe_down(down()), "candidate before unknown Ctrl");
    auto unknown = start();
    unknown.ctrl = e::MvpCtrlState::Unknown;
    check(unknown_ctrl.observe_start(unknown).route == e::MvpGestureRoute::Reject,
          "unknown Ctrl cannot select plain Move");

    e::ExplorerMvpAttribution stale_time;
    check(stale_time.observe_down(down()), "candidate before stale START");
    auto early = start();
    early.native_time = 99;
    check(stale_time.observe_start(early).route == e::MvpGestureRoute::Reject,
          "START before DOWN time rejects");
}

void duplicates_up_before_end_and_next_generation() {
    e::ExplorerMvpAttribution duplicate;
    check(duplicate.observe_down(down()), "first DOWN");
    check(!duplicate.observe_down(down()), "second DOWN is ambiguous");
    check(duplicate.observe_start(start()).route == e::MvpGestureRoute::Reject,
          "ambiguous DOWN cannot route");

    e::ExplorerMvpAttribution repeated_start;
    check(repeated_start.observe_down(down()), "DOWN before repeated START");
    check(repeated_start.observe_start(start()).route == e::MvpGestureRoute::PlainMoveCandidate,
          "first START candidate");
    check(repeated_start.observe_start(start()).route == e::MvpGestureRoute::Reject,
          "duplicate START retires candidate");
    check(repeated_start.route() == e::MvpGestureRoute::Reject,
          "duplicate START does not leave stale route active");

    e::ExplorerMvpAttribution early_up;
    check(early_up.observe_down(down()), "DOWN before early UP");
    check(early_up.observe_raw_up(11, 10), "real early UP");
    check(early_up.observe_start(start()).route == e::MvpGestureRoute::Reject,
          "UP before START prevents takeover route");

    e::ExplorerMvpAttribution up_before_end;
    check(up_before_end.observe_down(down()), "DOWN before START/UP/END");
    check(up_before_end.observe_start(start()).route == e::MvpGestureRoute::PlainMoveCandidate,
          "route before UP");
    check(up_before_end.observe_raw_up(11, 10), "UP before END");
    check(up_before_end.observe_end(end()), "END is still recorded after UP");
    check(up_before_end.raw_up_seen() && up_before_end.matching_end_seen(),
          "END did not clear UP revocation fact");
    check(up_before_end.route() == e::MvpGestureRoute::Reject,
          "END after UP cannot reactivate ordinary Move route");

    e::ExplorerMvpAttribution next;
    check(next.observe_down(down(12)), "next generation has independent DOWN");
    check(next.observe_start(start(12)).route == e::MvpGestureRoute::PlainMoveCandidate,
          "next generation is not polluted by prior UP");
    check(!next.observe_end(end(11)), "prior-generation END cannot finish next gesture");
    check(next.observe_end(end(12)), "new matching END accepted");
}
} // namespace

int main() {
    try {
        classification_and_frozen_down_anchor();
        exclusive_routes();
        missing_permission_and_ambiguous_identity_fail_closed();
        duplicates_up_before_end_and_next_generation();
        std::cout << "Explorer MVP attribution: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Explorer MVP attribution check " << checks << ": " << error.what() << '\n';
        return 1;
    }
}
