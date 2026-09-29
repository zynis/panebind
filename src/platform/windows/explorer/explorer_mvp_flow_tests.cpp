#include "core/behavior/move_magnet_session.h"
#include "platform/windows/explorer/explorer_mvp_attribution.h"
#include "platform/windows/explorer/explorer_group_event_source.h"
#include "platform/windows/explorer/explorer_mvp_end_authority.h"
#include "platform/windows/operations/live_move_writer.h"
#include "platform/windows/operations/move_frame_continuity.h"

#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

// Offline composition contract only. No HWND, Raw Input, Shell capability,
// native END, overlay hit, or SetWindowPos is manufactured or exercised here.
// The Explorer adapter must supply those independent facts in the guest.
namespace b = panebind::core::behavior;
namespace e = panebind::platform::windows::explorer;
namespace g = panebind::core::geometry;
namespace m = panebind::core::magnet;
namespace o = panebind::platform::windows::operations;

namespace {
unsigned checks{};
void check(bool value, std::string_view description) {
    ++checks;
    if (!value) throw std::runtime_error{std::string(description)};
}

constexpr g::Point anchor{500, 500};
constexpr g::Rect initial{128, 20, 228, 160};
constexpr g::Rect positioning{118, 10, 238, 170};
const std::array target{m::MagnetTarget{
    panebind::core::model::WindowId{"peer"}, {0, 0, 120, 180}, false}};

e::MvpSourceBinding binding(std::size_t member) {
    return {member, 100 + member, 200 + member, 300 + member,
            0x1000 + member, static_cast<std::uint32_t>(400 + member),
            static_cast<std::uint32_t>(500 + member)};
}

e::MvpRawDownFacts down(std::uint64_t generation, std::size_t member) {
    e::MvpRawDownFacts result;
    result.generation = generation;
    result.packet_sequence = 10 + generation;
    result.receiver_watermark = result.packet_sequence - 1;
    result.message_time = 100;
    result.source = binding(member);
    result.message_point = anchor;
    result.hit_root = result.foreground_root = result.source.hwnd;
    result.initial_visible = initial;
    result.initial_positioning = positioning;
    result.hit = e::MvpDownHit::Caption;
    result.raw_left_down = result.packet_valid = result.receiver_healthy = true;
    result.hit_test_succeeded = result.pre_native_gui_clear = true;
    result.exact_consent_capture = result.input_desktop_valid = true;
    return result;
}

e::MvpNativeStartFacts start(std::uint64_t generation, std::size_t member) {
    e::MvpNativeStartFacts result;
    result.generation = generation;
    result.event_sequence = 100 + generation;
    result.native_time = 101;
    result.source = binding(member);
    result.event_hwnd = result.foreground_root = result.gui_move_size_hwnd = result.source.hwnd;
    result.ctrl = e::MvpCtrlState::Up;
    result.exact_win_event = result.object_window_self = result.event_stream_healthy = true;
    result.exact_consent_capture = result.gui_in_move_size = result.physical_left_held = true;
    result.input_desktop_valid = result.other_modifiers_up = true;
    return result;
}

e::MvpNativeEndFacts end(std::uint64_t generation, std::size_t member) {
    return {generation, 200 + generation, binding(member), binding(member).hwnd,
            true, true, true};
}

e::MvpEndAuthorityFacts fresh_end_facts() {
    e::MvpEndAuthorityFacts result;
    result.exact_handoff_context = result.gui_after_end = true;
    result.source_foreground = result.physical_left_held = true;
    result.cursor_available = result.overlay_valid = true;
    result.input_desktop_active = true;
    return result;
}

void end_authority_and_raw_watermark() {
    // This receipt predicate is called by the real GroupEventSource queue
    // inspection, and this END predicate is called by the product adapter.
    check(!e::product_move_receipt_conflicts(0, e::GroupEventKind::Location, 0),
          "source LOCATION is not a new lifecycle or peer conflict");
    check(e::product_move_receipt_conflicts(0, e::GroupEventKind::Location, 1),
          "another member LOCATION conflicts with source handoff");
    check(e::product_move_receipt_conflicts(0, e::GroupEventKind::Start, 0) &&
          e::product_move_receipt_conflicts(0, e::GroupEventKind::End, 0) &&
          e::product_move_receipt_conflicts(0, e::GroupEventKind::Destroy, 0),
          "any newly queued lifecycle conflicts with handoff");
    std::array<e::GroupEventReceipt, 2> end_batch{};
    end_batch[0].member_index = 0;
    end_batch[0].kind = e::GroupEventKind::Location;
    end_batch[0].sequence = 30;
    end_batch[1].member_index = 0;
    end_batch[1].kind = e::GroupEventKind::End;
    end_batch[1].sequence = 31;
    check(!e::product_move_handoff_batch_conflicts(0, 31, end_batch),
          "already-drained matching END and source LOCATION permit handoff");
    check(e::product_move_handoff_batch_conflicts(0, 99, end_batch),
          "missing matching END cannot grant handoff");
    end_batch[0].member_index = 1;
    check(e::product_move_handoff_batch_conflicts(0, 31, end_batch),
          "peer LOCATION in the drained END batch still conflicts");
    end_batch[0].member_index = 0;
    end_batch[0].kind = e::GroupEventKind::Start;
    check(e::product_move_handoff_batch_conflicts(0, 31, end_batch),
          "another lifecycle receipt in the drained batch conflicts");
    end_batch[0].kind = e::GroupEventKind::End;
    check(e::product_move_handoff_batch_conflicts(0, 31, end_batch),
          "only the exact matching END is exempted");

    auto facts = fresh_end_facts();
    check(e::mvp_end_authority_ready(facts, false, false),
          "processed matching END and no pending conflict admit handoff");
    check(!e::mvp_end_authority_ready(facts, true, false),
          "conflict in the processed END batch forbids handoff");
    check(!e::mvp_end_authority_ready(facts, false, true),
          "unprocessed group batch or pending receipt forbids handoff");
    facts = fresh_end_facts();
    facts.raw_up_seen = true;
    check(!e::mvp_end_authority_ready(facts, false, false),
          "physical UP still revokes after END");
    facts = fresh_end_facts();
    facts.exact_handoff_context = false;
    check(!e::mvp_end_authority_ready(facts, false, false),
          "changed identity or geometry forbids handoff");
    facts = fresh_end_facts();
    facts.overlay_valid = false;
    check(!e::mvp_end_authority_ready(facts, false, false),
          "lost input isolation forbids handoff");

    check(!e::mvp_raw_continuation_after_handoff(19, 10, 20) &&
          !e::mvp_raw_continuation_after_handoff(20, 10, 20) &&
          e::mvp_raw_continuation_after_handoff(21, 10, 20),
          "pre-END drained Raw packets stay below the handoff watermark");
}

void route_and_permission() {
    e::ExplorerMvpAttribution missing;
    auto invalid = down(1, 0);
    invalid.exact_consent_capture = false;
    check(!missing.observe_down(invalid), "missing exact consent rejects DOWN");
    check(missing.observe_start(start(1, 0)).route == e::MvpGestureRoute::Reject,
          "START cannot repair missing consent");

    e::ExplorerMvpAttribution ctrl;
    check(ctrl.observe_down(down(2, 1)), "Ctrl DOWN accepted");
    auto ctrl_start = start(2, 1);
    ctrl_start.ctrl = e::MvpCtrlState::Down;
    check(ctrl.observe_start(ctrl_start).route == e::MvpGestureRoute::CtrlMove,
          "Ctrl START selects native Glue route");

    e::ExplorerMvpAttribution resize;
    auto border = down(3, 2);
    border.hit = e::MvpDownHit::Resize;
    check(resize.observe_down(border), "Resize DOWN accepted");
    check(resize.observe_start(start(3, 2)).route == e::MvpGestureRoute::NativeResize,
          "Resize START stays native");

    b::MoveMagnetSession not_plain;
    check(!not_plain.begin(2, anchor, initial, target, 1000, 1000,
                           true, true, ctrl.route() == e::MvpGestureRoute::PlainMoveCandidate),
          "Ctrl route cannot arm source writer");
    check(!not_plain.begin(3, anchor, initial, target, 1000, 1000,
                           true, true, resize.route() == e::MvpGestureRoute::PlainMoveCandidate),
          "Resize route cannot arm source writer");
}

// The in-memory frame is not a substitute for the Explorer bridge. These
// callbacks use mutable identity/context, actual frame and retirement facts,
// rather than an always-true idealized adapter.
struct OfflineGesture {
    explicit OfflineGesture(std::uint64_t generation, std::size_t member)
        : generation(generation), member(member) {}

    std::uint64_t generation;
    std::size_t member;
    e::ExplorerMvpAttribution attribution;
    e::MvpGestureAttribution route;
    b::MoveMagnetSession motion;
    o::MoveFrameContinuity continuity;
    o::MoveFrameGeometry actual{positioning, initial};
    std::unique_ptr<o::LiveMoveWriter> writer;
    bool exact_identity{true}, participant_context{true}, physical_left{true};
    bool isolated{true}, actual_end{};
    unsigned placements{};

    void begin_candidate() {
        check(attribution.observe_down(down(generation, member)), "fresh DOWN");
        route = attribution.observe_start(start(generation, member));
        check(route.route == e::MvpGestureRoute::PlainMoveCandidate,
              "plain candidate, not native permit");
        check(motion.begin(generation, route.original_cursor, route.initial_visible,
                           target, 1000, 1000, exact_identity, physical_left, true),
              "shared Move session begins with original DOWN anchor");
        check(motion.isolation_ready(generation, isolated) && motion.may_cancel(generation) &&
              motion.cancel_issued(generation), "independent isolation/cancel gate");
        check(!motion.sample_cursor(generation, {502, 500}, 2000,
                                    true, true, isolated), "cancel is not END");
    }

    void handoff() {
        check(attribution.observe_end(end(generation, member)), "matching real END fact");
        actual_end = attribution.matching_end_seen();
        check(actual_end && !attribution.raw_up_seen(), "END is not physical UP");
        check(motion.native_end_observed(generation, actual_end), "END opens handoff");
        check(continuity.arm(generation, actual), "actual handoff geometry baseline");
        writer = std::make_unique<o::LiveMoveWriter>(generation, o::MoveWriteCallbacks{
            [this]() -> std::optional<o::MoveFrameGeometry> {
                if (!exact_identity || !participant_context) return std::nullopt;
                return actual;
            },
            [this](const o::MoveFrameGeometry& before) {
                const auto version = continuity.snapshot_version(generation);
                const auto observation = continuity.observe(generation, actual, version);
                const bool context = exact_identity && participant_context &&
                    before.positioning == actual.positioning &&
                    before.visible == actual.visible &&
                    o::classify_move_sample(observation, true) == o::MoveSampleDecision::Process;
                return o::classify_move_preflight({
                    context, actual_end && physical_left && isolated &&
                        !attribution.raw_up_seen(), attribution.raw_up_seen()});
            },
            [this](const o::MoveFrameGeometry& before, const g::Rect& visible,
                   const g::Rect& target_positioning, std::uint64_t quantum) {
                const o::MoveFrameGeometry target_frame{target_positioning, visible};
                if (!continuity.begin_attempt(generation, quantum, before, target_frame))
                    return o::MoveNativePlacement{};
                if (!writer->begin_native_attempt(generation)) {
                    (void)continuity.abort_unissued(generation, quantum);
                    return o::MoveNativePlacement{};
                }
                ++placements;
                actual = target_frame;
                return o::MoveNativePlacement{true, true};
            },
            [this](const o::MoveFrameGeometry&, const o::MoveFrameGeometry& after) {
                const auto version = continuity.snapshot_version(generation);
                const auto observed = continuity.observe(generation, actual, version);
                return o::move_post_context_valid({
                    exact_identity, participant_context,
                    observed == o::MoveFrameObservation::OwnInFlight,
                    after.positioning == actual.positioning && after.visible == actual.visible});
            }});
        check(writer->ready(), "one shared writer armed after END");
    }

    b::MoveWritePlan plan(g::Point cursor, std::uint64_t qpc) {
        const auto version = continuity.snapshot_version(generation);
        const auto observation = continuity.observe(generation, actual, version);
        check(o::classify_move_sample(observation, exact_identity && participant_context) ==
                  o::MoveSampleDecision::Process, "fresh frame passes continuity");
        check(motion.sample_cursor(generation, cursor, qpc,
                                    exact_identity && physical_left && actual_end,
                                    true, isolated), "changed cursor makes one plan");
        auto pending = motion.take_pending(generation, exact_identity && physical_left,
                                           true, isolated);
        check(pending.has_value(), "one pending target consumed");
        return *pending;
    }

    o::MoveWriteReceipt execute(const b::MoveWritePlan& plan) {
        check(writer->offer({plan.generation, plan.quantum, plan.target_visible,
                             plan.snapped}), "one writer offer");
        auto receipt = writer->try_execute();
        check(receipt.has_value(), "owner-side nonblocking writer dispatch");
        const bool committed = continuity.finish_attempt(generation, receipt->quantum,
            receipt->native_attempted, receipt->exact, receipt->after);
        const auto result = motion.write_result(plan, receipt->exact,
                                                receipt->after ? receipt->after->visible : g::Rect{});
        check(receipt->exact && committed && result.recognized && result.exact &&
              !o::move_write_receipt_failed(*receipt, committed),
              "exact single placement commits shared geometry");
        return *receipt;
    }

    void retire_up(std::uint64_t packet_sequence) {
        check(attribution.observe_raw_up(generation, packet_sequence), "matching Raw UP");
        physical_left = false;
        check(motion.raw_up_observed(generation, true), "UP revokes motion");
        continuity.retire(generation);
        check(writer->retire(generation).retired_now, "UP revokes writer");
    }
};

void admitted_end_reaches_shared_writer() {
    OfflineGesture accepted{20, 0};
    accepted.begin_candidate();
    check(e::mvp_end_authority_ready(fresh_end_facts(), false, false),
          "the adapter's fresh END gate admits the normal handoff");
    accepted.handoff();
    check(e::mvp_raw_continuation_after_handoff(41, accepted.generation + 10, 40),
          "later Raw cursor packet may continue the accepted handoff");
    const auto plan = accepted.plan({502, 500}, 2000);
    accepted.execute(plan);
    check(accepted.placements == 1,
          "admitted END and later Raw continuation reach the shared writer");
}

void complete_then_retire_and_switch() {
    OfflineGesture first{11, 0};
    first.begin_candidate();
    first.handoff();
    const auto captured_version = first.continuity.snapshot_version(11);
    const auto before_own_write = first.actual;
    const auto one = first.plan({502, 500}, 2000);
    check(one.snapped && one.target_visible.left() == 120,
          "shared solver selects one snapped target");
    const auto receipt = first.execute(one);
    check(first.placements == 1 && receipt.native_attempted &&
          !first.writer->offer({11, one.quantum, one.target_visible, one.snapped}),
          "one native placement maximum per quantum");
    check(first.continuity.observe(11, before_own_write, captured_version) ==
              o::MoveFrameObservation::Stale &&
          o::classify_move_sample(o::MoveFrameObservation::Stale, true) ==
              o::MoveSampleDecision::DropStale,
          "old capture across own write is discarded, not authority");
    check(!first.motion.retired(11), "stale sample did not poison gesture");

    const auto pending = first.plan({503, 500}, 3000);
    check(first.writer->offer({11, pending.quantum, pending.target_visible, pending.snapped}),
          "pending second quantum");
    first.retire_up(22);
    check(!first.writer->try_execute() && first.placements == 1 &&
          !first.motion.sample_cursor(11, {504, 500}, 4000, true, true, true) &&
          !first.writer->offer({11, pending.quantum + 1, pending.target_visible, false}),
          "UP clears pending and forbids new placement");

    OfflineGesture next{12, 2};
    next.begin_candidate();
    check(!next.attribution.observe_end(end(11, 0)),
          "old generation/source END cannot finish switched source");
    next.handoff();
    check(next.route.source.member == 2 && next.route.generation == 12,
          "fresh generation switches authorized source");
    const auto next_plan = next.plan({502, 500}, 2000);
    next.execute(next_plan);
    check(next.placements == 1 && first.placements == 1,
          "new writer independent of retired old writer");
    const auto stop_plan = next.plan({503, 500}, 3000);
    check(next.writer->offer({12, stop_plan.quantum, stop_plan.target_visible,
                              stop_plan.snapped}), "pending before stop");
    check(next.motion.escape(12, b::MoveHandoffEscapeReason::ExplicitStop),
          "explicit stop retires session");
    next.continuity.retire(12);
    check(next.writer->retire(12).pending_discarded &&
          !next.writer->try_execute() && next.placements == 1 &&
          !next.motion.take_pending(12, true, true, true),
          "stop revokes pending and native writer");
}

void participant_context_loss_rejects_placement() {
    OfflineGesture lost{13, 1};
    lost.begin_candidate();
    lost.handoff();
    const auto plan = lost.plan({502, 500}, 2000);
    lost.participant_context = false; // e.g. another bound frame changed identity
    check(lost.writer->offer({13, plan.quantum, plan.target_visible, plan.snapped}),
          "request was offered before context loss");
    const auto receipt = lost.writer->try_execute();
    check(receipt && receipt->reason == "capture_failed" &&
          !receipt->native_attempted && lost.placements == 0,
          "fresh participant context loss cannot reach native placement");
    check(!lost.writer->offer({13, plan.quantum + 1, plan.target_visible, false}),
          "failed writer cannot be revived by later request");
}
} // namespace

int main() {
    try {
        route_and_permission();
        end_authority_and_raw_watermark();
        admitted_end_reaches_shared_writer();
        complete_then_retire_and_switch();
        participant_context_loss_rejects_placement();
        std::cout << "Explorer MVP offline flow: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Explorer MVP offline flow check " << checks << ": "
                  << error.what() << '\n';
        return 1;
    }
}
