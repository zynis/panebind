#include "platform/windows/operations/move_frame_continuity.h"
#include "core/behavior/move_magnet_session.h"

#include <iostream>
#include <optional>

namespace o = panebind::platform::windows::operations;
using panebind::core::geometry::Rect;

namespace {
o::MoveFrameObservation observe_now(o::MoveFrameContinuity& continuity,
                                    std::uint64_t generation,
                                    const o::MoveFrameGeometry& actual) {
    const auto version = continuity.snapshot_version(generation);
    return continuity.observe(generation, actual, version);
}
}

int main() {
    int checks = 0;
    int failures = 0;
    const auto check = [&](bool value) {
        ++checks;
        if (!value) ++failures;
    };
    const o::MoveFrameGeometry initial{{90, 90, 310, 210},
                                       {100, 100, 300, 200}};
    const o::MoveFrameGeometry first{{110, 120, 330, 240},
                                     {120, 130, 320, 230}};
    const o::MoveFrameGeometry second{{140, 150, 360, 270},
                                      {150, 160, 350, 260}};
    const o::MoveFrameGeometry external{{115, 120, 335, 240},
                                        {125, 130, 325, 230}};

    {
        // Original failure: an external translation preserves both sizes.
        // The next write must not overwrite that unrelated placement.
        check(external.positioning.size() == first.positioning.size());
        check(external.visible.size() == first.visible.size());
        o::MoveFrameContinuity continuity;
        check(continuity.arm(7, initial));
        check(continuity.begin_attempt(7, 1, initial, first));
        check(observe_now(continuity, 7, {first.positioning, initial.visible}) ==
              o::MoveFrameObservation::OwnInFlight);
        check(observe_now(continuity, 7, {initial.positioning, first.visible}) ==
              o::MoveFrameObservation::OwnInFlight);
        check(continuity.finish_attempt(7, 1, true, true, first));
        check(observe_now(continuity, 7, first) == o::MoveFrameObservation::Expected);
        check(observe_now(continuity, 7, external) ==
              o::MoveFrameObservation::ExternalChange);
        check(!continuity.begin_attempt(7, 2, external, second));
        check(!continuity.begin_attempt(7, 2, first, second));
        check(!continuity.may_write(7));
    }
    {
        // Even when no receiver saw the intervening translation, writer
        // preflight compares the captured frame with the confirmed baseline.
        o::MoveFrameContinuity continuity;
        check(continuity.arm(8, initial));
        check(continuity.begin_attempt(8, 1, initial, first));
        check(continuity.finish_attempt(8, 1, true, true, first));
        check(!continuity.begin_attempt(8, 2, external, second));
        check(!continuity.may_write(8));
    }
    {
        // Normal own placement and its feedback advance the validation
        // baseline, without changing any original DOWN intent anchor.
        o::MoveFrameContinuity continuity;
        continuity.retire(0); // no gesture exists yet
        check(observe_now(continuity, 9, initial) == o::MoveFrameObservation::NotArmed);
        check(continuity.arm(9, initial));
        check(continuity.begin_attempt(9, 1, initial, first));
        check(observe_now(continuity, 9, first) == o::MoveFrameObservation::OwnInFlight);
        check(continuity.finish_attempt(9, 1, true, true, first));
        check(observe_now(continuity, 9, first) == o::MoveFrameObservation::Expected);
        check(continuity.begin_attempt(9, 2, first, second));
        check(continuity.finish_attempt(9, 2, true, true, second));
        check(observe_now(continuity, 9, second) == o::MoveFrameObservation::Expected);
        check(continuity.may_write(9));
    }
    {
        // Red reproduction: receiver captures A, then our writer commits B
        // before the receiver can classify its old snapshot. No external
        // window movement occurred, so the old sample must not poison the
        // next fresh B observation or the next ordinary write.
        o::MoveFrameContinuity continuity;
        check(continuity.arm(90, initial));
        panebind::core::behavior::MoveMagnetSession session;
        check(session.begin(90, {500, 500}, initial.visible, {}, 1000, 1000,
                            true, true, true));
        check(session.isolation_ready(90, true) && session.may_cancel(90) &&
              session.cancel_issued(90) && session.native_end_observed(90, true));
        const auto version_before_capture = continuity.snapshot_version(90);
        const auto receiver_captured_before_write = initial;
        check(continuity.begin_attempt(90, 1, initial, first));
        check(continuity.finish_attempt(90, 1, true, true, first));
        check(continuity.snapshot_version(90) != version_before_capture);
        const auto stale = continuity.observe(90, receiver_captured_before_write,
                                              version_before_capture);
        check(stale == o::MoveFrameObservation::Stale);
        const auto stale_decision = o::classify_move_sample(stale, true);
        check(stale_decision == o::MoveSampleDecision::DropStale);
        // This is the owned Raw receiver's result split: DropStale must not
        // call sample_cursor(..., actual_stable=false), which would retire.
        bool forwarded_old_sample = false;
        if (stale_decision == o::MoveSampleDecision::Process)
            forwarded_old_sample = session.sample_cursor(
                90, {501, 500}, 2000, true, true, true);
        check(!forwarded_old_sample && !session.retired(90));
        const auto fresh = observe_now(continuity, 90, first);
        check(fresh == o::MoveFrameObservation::Expected);
        check(o::classify_move_sample(fresh, true) ==
              o::MoveSampleDecision::Process);
        check(session.sample_cursor(90, {502, 500}, 2000, true, true, true));
        check(session.take_pending(90, true, true, true).has_value() &&
              !session.retired(90));
        check(continuity.begin_attempt(90, 2, first, second));
    }
    {
        // A fresh observation of the old A after our confirmed A->B write
        // is an external B->A translation, not a stale sample exemption.
        o::MoveFrameContinuity continuity;
        check(continuity.arm(91, initial));
        check(continuity.begin_attempt(91, 1, initial, first));
        check(continuity.finish_attempt(91, 1, true, true, first));
        const auto version_before_fresh_capture = continuity.snapshot_version(91);
        const auto external_return_to_a = initial;
        check(continuity.observe(91, external_return_to_a,
                                 version_before_fresh_capture) ==
              o::MoveFrameObservation::ExternalChange);
        check(o::classify_move_sample(o::MoveFrameObservation::ExternalChange, true) ==
              o::MoveSampleDecision::Reject);
        check(o::classify_move_sample(o::MoveFrameObservation::Stale, false) ==
              o::MoveSampleDecision::Reject);
        check(o::classify_move_sample(o::MoveFrameObservation::OwnInFlight, true) ==
              o::MoveSampleDecision::Process);
        check(!continuity.may_write(91));
        check(!continuity.begin_attempt(91, 2, initial, second));
    }
    {
        // An unissued request cannot become a new baseline. Normal UP may
        // retire while a claimed native call completes; it never re-arms.
        o::MoveFrameContinuity continuity;
        check(continuity.arm(10, initial));
        check(continuity.begin_attempt(10, 1, initial, first));
        check(continuity.abort_unissued(10, 1));
        check(observe_now(continuity, 10, initial) == o::MoveFrameObservation::Expected);
        check(continuity.begin_attempt(10, 2, initial, first));
        const auto version_before_up = continuity.snapshot_version(10);
        continuity.retire(10);
        check(continuity.snapshot_version(10) == version_before_up);
        check(continuity.finish_attempt(10, 2, true, true, first));
        check(observe_now(continuity, 10, first) == o::MoveFrameObservation::Expected);
        check(!continuity.may_write(10));
        check(!continuity.begin_attempt(10, 3, first, second));
    }
    {
        // A non-exact native result and an unexpected in-flight frame are
        // fail-closed, even if later observations return to an old position.
        o::MoveFrameContinuity continuity;
        check(continuity.arm(11, initial));
        check(continuity.begin_attempt(11, 1, initial, first));
        check(observe_now(continuity, 11, external) ==
              o::MoveFrameObservation::ExternalChange);
        check(!continuity.finish_attempt(11, 1, true, true, first));
        check(observe_now(continuity, 11, initial) ==
              o::MoveFrameObservation::ExternalChange);
        check(!continuity.may_write(11));
    }
    {
        o::MoveFrameContinuity continuity;
        check(continuity.arm(12, initial));
        check(continuity.begin_attempt(12, 1, initial, first));
        check(!continuity.finish_attempt(12, 1, true, false, external));
        check(!continuity.may_write(12));
        check(observe_now(continuity, 12, initial) ==
              o::MoveFrameObservation::ExternalChange);
        check(observe_now(continuity, 13, initial) == o::MoveFrameObservation::NotArmed);
    }
    {
        // Exercise the same shared writer + continuity decisions used by the
        // owned adapter, not a fake fresh-authority callback that always
        // returns true. Two own writes advance the baseline; a same-size
        // external translation then prevents a third native placement.
        auto actual = initial;
        o::MoveFrameContinuity continuity;
        check(continuity.arm(20, actual));
        int placements = 0;
        o::LiveMoveWriter writer{20, {
            [&] { return std::optional{actual}; },
            [&](const o::MoveFrameGeometry& before) {
                const auto observed = observe_now(continuity, 20, actual);
                const bool exact_before = before.positioning == actual.positioning &&
                    before.visible == actual.visible;
                return observed == o::MoveFrameObservation::Expected && exact_before ?
                    o::MovePreflightVerdict::Allowed :
                    o::MovePreflightVerdict::ContextInvalid;
            },
            [&](const o::MoveFrameGeometry& before, const Rect& target,
                const Rect& positioning, std::uint64_t quantum) {
                const o::MoveFrameGeometry target_frame{positioning, target};
                if (!continuity.begin_attempt(20, quantum, before, target_frame))
                    return o::MoveNativePlacement{};
                if (!writer.begin_native_attempt(20)) {
                    (void)continuity.abort_unissued(20, quantum);
                    return o::MoveNativePlacement{};
                }
                ++placements;
                actual = target_frame;
                return o::MoveNativePlacement{true, true};
            },
            [&](const auto&, const o::MoveFrameGeometry& after) {
                return observe_now(continuity, 20, actual) ==
                           o::MoveFrameObservation::OwnInFlight &&
                    actual.positioning == after.positioning &&
                    actual.visible == after.visible;
            }}};
        check(writer.offer({20, 1, first.visible}));
        const auto first_receipt = writer.wait_and_execute();
        check(first_receipt && first_receipt->exact && placements == 1);
        check(first_receipt && continuity.finish_attempt(20, 1,
            first_receipt->native_attempted, first_receipt->exact,
            first_receipt->after));
        check(writer.offer({20, 2, second.visible}));
        const auto second_receipt = writer.wait_and_execute();
        check(second_receipt && second_receipt->exact && placements == 2);
        check(second_receipt && continuity.finish_attempt(20, 2,
            second_receipt->native_attempted, second_receipt->exact,
            second_receipt->after));
        actual = external;
        check(writer.offer({20, 3, initial.visible}));
        const auto rejected = writer.wait_and_execute();
        check(rejected && !rejected->native_attempted && !rejected->exact &&
            rejected->reason == "fresh_authority_rejected");
        check(placements == 2 && actual.positioning == external.positioning &&
            actual.visible == external.visible);
        check(!writer.offer({20, 4, first.visible}));
    }
    {
        // UP during capture: the bound geometry remains valid, but the
        // gesture has been revoked before any native attempt.
        auto actual = initial;
        o::MoveFrameContinuity continuity;
        check(continuity.arm(21, actual));
        bool matching_up = false;
        int placements = 0;
        o::LiveMoveWriter writer{21, {
            [&] {
                matching_up = true;
                continuity.retire(21);
                (void)writer.retire(21);
                return std::optional{actual};
            },
            [&](const o::MoveFrameGeometry& before) {
                const auto observed = observe_now(continuity, 21, actual);
                return o::classify_move_preflight({
                    observed == o::MoveFrameObservation::Expected &&
                        before.positioning == actual.positioning &&
                        before.visible == actual.visible,
                    continuity.may_write(21), matching_up});
            },
            [&](const auto&, const Rect&, const Rect&, std::uint64_t) {
                ++placements;
                return o::MoveNativePlacement{true, true};
            },
            [&](const auto&, const auto&) { return true; }}};
        check(writer.offer({21, 1, first.visible}));
        const auto receipt = writer.wait_and_execute();
        check(receipt && receipt->reason == "normal_up_before_native" &&
            !receipt->native_attempted && placements == 0 &&
            !o::move_write_receipt_failed(*receipt, false));
        check(!continuity.may_write(21) && !writer.offer({21, 2, second.visible}));
    }
    {
        // UP after a native placement: post-context validates the issued
        // result, then continuity records it without reviving permission.
        auto actual = initial;
        o::MoveFrameContinuity continuity;
        check(continuity.arm(22, actual));
        bool matching_up = false;
        int placements = 0;
        o::LiveMoveWriter writer{22, {
            [&] { return std::optional{actual}; },
            [&](const o::MoveFrameGeometry& before) {
                const auto observed = observe_now(continuity, 22, actual);
                return o::classify_move_preflight({
                    observed == o::MoveFrameObservation::Expected &&
                        before.positioning == actual.positioning &&
                        before.visible == actual.visible,
                    continuity.may_write(22), matching_up});
            },
            [&](const o::MoveFrameGeometry& before, const Rect& target,
                const Rect& positioning, std::uint64_t quantum) {
                const o::MoveFrameGeometry target_frame{positioning, target};
                if (!continuity.begin_attempt(22, quantum, before, target_frame) ||
                    !writer.begin_native_attempt(22)) return o::MoveNativePlacement{};
                ++placements;
                actual = target_frame;
                matching_up = true;
                continuity.retire(22);
                (void)writer.retire(22);
                return o::MoveNativePlacement{true, true};
            },
            [&](const auto&, const o::MoveFrameGeometry& after) {
                return o::move_post_context_valid({
                    true, true,
                    observe_now(continuity, 22, actual) ==
                        o::MoveFrameObservation::OwnInFlight,
                    actual.positioning == after.positioning &&
                        actual.visible == after.visible});
            }}};
        check(writer.offer({22, 1, first.visible}));
        const auto receipt = writer.wait_and_execute();
        const bool committed = receipt && continuity.finish_attempt(22, 1,
            receipt->native_attempted, receipt->exact, receipt->after);
        check(receipt && receipt->native_attempted && receipt->exact &&
            receipt->retired_during_dispatch && placements == 1 && committed &&
            !o::move_write_receipt_failed(*receipt, committed));
        check(!continuity.may_write(22) &&
            !writer.offer({22, 2, second.visible}));
    }

    std::cout << "move frame continuity: " << checks << " checks, "
              << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
