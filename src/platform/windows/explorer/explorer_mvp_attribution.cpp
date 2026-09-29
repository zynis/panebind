#include "platform/windows/explorer/explorer_mvp_attribution.h"

namespace panebind::platform::windows::explorer {
namespace {
bool valid_binding(const MvpSourceBinding& source) noexcept {
    return source.member < 3 && source.window_id && source.capability_generation &&
        source.consent_generation && source.hwnd && source.process_id && source.thread_id;
}

// Tick stamps share the Win32 32-bit time domain. An ambiguous half-range or
// earlier START is rejected; timestamp ordering is only one candidate fact.
bool tick_at_or_after(std::uint32_t later, std::uint32_t earlier) noexcept {
    return static_cast<std::uint32_t>(later - earlier) < 0x80000000U;
}
} // namespace

bool ExplorerMvpAttribution::observe_down(const MvpRawDownFacts& facts) noexcept {
    if (down_ || retired_) {
        retired_ = true;
        return false;
    }
    if (!facts.generation || !facts.packet_sequence ||
        facts.packet_sequence <= facts.receiver_watermark || !valid_binding(facts.source) ||
        !facts.packet_valid || !facts.receiver_healthy || !facts.raw_left_down ||
        facts.raw_left_up || !facts.message_point || !facts.hit_test_succeeded ||
        !facts.exact_consent_capture || !facts.input_desktop_valid ||
        !facts.pre_native_gui_clear || facts.hit_root != facts.source.hwnd ||
        facts.foreground_root != facts.source.hwnd ||
        facts.hit == MvpDownHit::Unknown || facts.hit == MvpDownHit::Other ||
        facts.initial_visible.empty() || facts.initial_positioning.empty()) {
        retired_ = true;
        return false;
    }
    down_ = facts;
    return true;
}

MvpGestureAttribution ExplorerMvpAttribution::observe_start(
    const MvpNativeStartFacts& facts) noexcept {
    if (start_seen_) {
        retired_ = true;
        decision_ = {};
        return {};
    }
    if (!down_ || raw_up_ || retired_ || !facts.generation ||
        facts.generation != down_->generation || facts.source != down_->source ||
        !facts.event_sequence || !tick_at_or_after(facts.native_time, down_->message_time) ||
        !facts.exact_win_event || !facts.object_window_self ||
        !facts.event_stream_healthy || !facts.exact_consent_capture ||
        !facts.gui_in_move_size || !facts.physical_left_held ||
        !facts.input_desktop_valid || !facts.other_modifiers_up ||
        facts.event_hwnd != facts.source.hwnd ||
        facts.foreground_root != facts.source.hwnd ||
        facts.gui_move_size_hwnd != facts.source.hwnd ||
        facts.ctrl == MvpCtrlState::Unknown) {
        retired_ = true;
        return {};
    }

    start_seen_ = true;
    decision_.generation = facts.generation;
    decision_.down_packet_sequence = down_->packet_sequence;
    decision_.start_event_sequence = facts.event_sequence;
    decision_.source = facts.source;
    decision_.original_cursor = *down_->message_point;
    decision_.initial_visible = down_->initial_visible;
    decision_.initial_positioning = down_->initial_positioning;
    decision_.route = down_->hit == MvpDownHit::Resize ? MvpGestureRoute::NativeResize :
        facts.ctrl == MvpCtrlState::Down ? MvpGestureRoute::CtrlMove :
        MvpGestureRoute::PlainMoveCandidate;
    return decision_;
}

bool ExplorerMvpAttribution::observe_end(const MvpNativeEndFacts& facts) noexcept {
    if (!start_seen_ || end_seen_ || retired_ || decision_.route == MvpGestureRoute::Reject ||
        facts.generation != decision_.generation || facts.source != decision_.source ||
        facts.event_sequence <= decision_.start_event_sequence ||
        !facts.exact_win_event || !facts.object_window_self ||
        !facts.event_stream_healthy || facts.event_hwnd != facts.source.hwnd) return false;
    end_seen_ = true;
    return true;
}

bool ExplorerMvpAttribution::observe_raw_up(std::uint64_t generation,
                                             std::uint64_t packet_sequence) noexcept {
    if (!down_ || raw_up_ || retired_ || generation != down_->generation ||
        packet_sequence <= down_->packet_sequence) return false;
    raw_up_ = true;
    return true;
}

void ExplorerMvpAttribution::retire() noexcept {
    retired_ = true;
}

} // namespace panebind::platform::windows::explorer
