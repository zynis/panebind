#include "platform/windows/operations/live_move_writer.h"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace o = panebind::platform::windows::operations;
using panebind::core::geometry::Rect;

constexpr auto allowed = o::MovePreflightVerdict::Allowed;
constexpr auto context_invalid = o::MovePreflightVerdict::ContextInvalid;

int main() {
    int checks = 0;
    int failures = 0;
    const auto check = [&](bool value) {
        ++checks;
        if (!value) ++failures;
    };
    const o::MoveFrameGeometry initial{{90, 90, 310, 210},
                                       {100, 100, 300, 200}};
    const Rect one{120, 130, 320, 230};
    const Rect two{150, 160, 350, 260};

    {
        // This fake adapter stands in for a capability-bound native adapter;
        // it verifies the writer's scheduling and exact geometry contract.
        auto frame = initial;
        int placements = 0;
        o::LiveMoveWriter writer{7, {
            [&] { return std::optional{frame}; },
            [&](const auto&) { return allowed; },
            [&](const auto&, const Rect& target, const Rect& positioning, std::uint64_t) {
                if (!writer.begin_native_attempt(7)) return o::MoveNativePlacement{};
                ++placements;
                frame = {positioning, target};
                return o::MoveNativePlacement{true, true};
            },
            [&](const auto&, const auto&) { return true; }}};
        check(writer.ready());
        check(!writer.try_execute()); // an extra posted work notice cannot block UI
        check(writer.offer({7, 1, one, true}));
        const auto first = writer.try_execute();
        check(first && first->exact && first->dispatch_reserved &&
              first->native_attempted && first->native_succeeded);
        check(first && first->expected_positioning == Rect{110, 120, 330, 240});
        check(first && first->snapped);
        check(placements == 1);
        check(!writer.try_execute());
        check(!writer.offer({7, 1, two})); // one placement per quantum
        check(!writer.offer({6, 2, two})); // stale generation
        check(writer.offer({7, 2, two}));
        const auto second = writer.wait_and_execute();
        check(second && second->exact && placements == 2);
        check(writer.offer({7, 3, one}));
        check(writer.offer({7, 4, two})); // latest pending wins
        const auto latest = writer.wait_and_execute();
        check(latest && latest->quantum == 4 && latest->exact &&
              latest->target_visible == two &&
              !latest->dispatch_reserved && placements == 2);
        check(!writer.retire(6).retired_now);
        check(writer.retire(7).retired_now);
        check(!writer.offer({7, 5, one}));
        check(!writer.wait_and_execute());
    }
    {
        auto frame = initial;
        int placements = 0;
        o::LiveMoveWriter writer{8, {
            [&] { return std::optional{frame}; },
            [&](const auto&) { return allowed; },
            [&](const auto&, const Rect& target, const Rect& positioning, std::uint64_t) {
                if (!writer.begin_native_attempt(8)) return o::MoveNativePlacement{};
                ++placements; frame = {positioning, target};
                return o::MoveNativePlacement{true, true};
            },
            [&](const auto&, const auto&) { return true; }}};
        check(writer.offer({8, 1, one}));
        const auto retired = writer.retire(8); // Raw UP wins before issue
        check(retired.retired_now && retired.pending_discarded &&
              !retired.dispatch_reserved);
        check(!writer.wait_and_execute() && placements == 0);
    }
    {
        auto frame = initial;
        int placements = 0;
        o::LiveMoveWriter writer{9, {
            [&] { return std::optional{frame}; },
            [&](const auto&) { return context_invalid; }, // no fresh authority
            [&](const auto&, const Rect&, const Rect&, std::uint64_t) {
                ++placements; return o::MoveNativePlacement{true, true};
            },
            [&](const auto&, const auto&) { return true; }}};
        check(writer.offer({9, 1, one}));
        const auto rejected = writer.wait_and_execute();
        check(rejected && !rejected->dispatch_reserved && !rejected->exact &&
              rejected->reason == "fresh_authority_rejected" && placements == 0);
        check(!writer.ready() && !writer.offer({9, 2, two}));
    }
    {
        auto frame = initial;
        o::LiveMoveWriter writer{10, {
            [&] { return std::optional{frame}; },
            [&](const auto&) { return allowed; },
            [&](const auto&, const Rect& target, const Rect& positioning, std::uint64_t) {
                if (!writer.begin_native_attempt(10)) return o::MoveNativePlacement{};
                // A native receipt claiming success is insufficient if the
                // visible frame differs on the exact post-capture.
                frame = {positioning, Rect{target.left() + 1,
                                           target.top(),
                                           target.right() + 1,
                                           target.bottom()}};
                return o::MoveNativePlacement{true, true};
            },
            [&](const auto&, const auto&) { return true; }}};
        check(writer.offer({10, 1, one}));
        const auto mismatch = writer.wait_and_execute();
        check(mismatch && mismatch->native_attempted && !mismatch->exact &&
              mismatch->reason == "exact_postverify_failed");
        check(!writer.ready() && !writer.offer({10, 2, two}));
    }
    {
        auto frame = initial;
        std::atomic<int> placements{};
        std::promise<void> entered;
        std::promise<void> release;
        const auto entered_future = entered.get_future();
        const auto release_future = release.get_future();
        o::LiveMoveWriter writer{11, {
            [&] { return std::optional{frame}; },
            [&](const auto&) { return allowed; },
            [&](const auto&, const Rect& target, const Rect& positioning, std::uint64_t) {
                if (!writer.begin_native_attempt(11)) return o::MoveNativePlacement{};
                ++placements;
                entered.set_value();
                release_future.wait();
                frame = {positioning, target};
                return o::MoveNativePlacement{true, true};
            },
            [&](const auto&, const auto&) { return true; }}};
        check(writer.offer({11, 1, one}));
        std::optional<o::MoveWriteReceipt> receipt;
        std::thread worker{[&] { receipt = writer.wait_and_execute(); }};
        const auto entered_native =
            entered_future.wait_for(std::chrono::seconds{3}) ==
            std::future_status::ready;
        check(entered_native);
        const auto retired = writer.retire(11);
        check(retired.retired_now &&
              (!entered_native ||
               (retired.dispatch_reserved && retired.reserved_quantum == 1 &&
                retired.attempt_started_before_revoke)));
        check(!writer.offer({11, 2, two}));
        release.set_value();
        worker.join();
        check(receipt && receipt->dispatch_reserved && receipt->exact &&
              receipt->retired_during_dispatch &&
              receipt->attempt_start_order ==
                  o::MoveAttemptStartOrder::ClaimedBeforeRevoke &&
              placements == 1);
        check(!writer.wait_and_execute());
    }

    {
        // UP after reservation but before the adapter's final active check:
        // no claim that a native API was entered, and no placement occurs.
        auto frame = initial;
        std::atomic<int> placements{};
        std::promise<void> entered;
        std::promise<void> release;
        const auto entered_future = entered.get_future();
        const auto release_future = release.get_future();
        o::LiveMoveWriter writer{12, {
            [&] { return std::optional{frame}; },
            [&](const auto&) { return allowed; },
            [&](const auto&, const Rect& target, const Rect& positioning, std::uint64_t) {
                entered.set_value();
                release_future.wait();
                if (!writer.begin_native_attempt(12)) return o::MoveNativePlacement{};
                ++placements;
                frame = {positioning, target};
                return o::MoveNativePlacement{true, true};
            },
            [&](const auto&, const auto&) { return true; }}};
        check(writer.offer({12, 1, one}));
        std::optional<o::MoveWriteReceipt> receipt;
        std::thread worker{[&] { receipt = writer.wait_and_execute(); }};
        const auto reserved =
            entered_future.wait_for(std::chrono::seconds{3}) ==
            std::future_status::ready;
        check(reserved);
        const auto retired = writer.retire(12);
        check(retired.retired_now &&
              (!reserved ||
               (retired.dispatch_reserved && !retired.attempt_started_before_revoke)));
        release.set_value();
        worker.join();
        check(receipt && receipt->dispatch_reserved &&
              !receipt->native_attempted && !receipt->exact &&
              receipt->attempt_start_order ==
                  o::MoveAttemptStartOrder::RejectedAfterRevoke &&
              receipt->reason == "retired_before_native" &&
              placements == 0);
    }
    {
        auto frame = initial;
        o::LiveMoveWriter writer{13, {
            [&] { return std::optional{frame}; },
            [&](const auto&) { return allowed; },
            [&](const auto&, const Rect&, const Rect&, std::uint64_t) -> o::MoveNativePlacement {
                if (!writer.begin_native_attempt(13)) return {};
                throw std::runtime_error{"test native exception"};
            },
            [&](const auto&, const auto&) { return true; }}};
        check(writer.offer({13, 1, one}));
        const auto failed = writer.wait_and_execute();
        check(failed && failed->dispatch_reserved &&
              !failed->native_outcome_known && !failed->exact &&
              failed->reason == "native_callback_exception");
        check(!writer.ready());
    }
    {
        auto frame = initial;
        o::LiveMoveWriter writer{14, {
            [&] { return std::optional{frame}; },
            [&](const auto&) { return allowed; },
            [&](const auto&, const Rect& target, const Rect& positioning, std::uint64_t) {
                frame = {positioning, target};
                return o::MoveNativePlacement{true, true};
            },
            [&](const auto&, const auto&) { return true; }}};
        check(writer.offer({14, 1, one}));
        const auto invalid = writer.wait_and_execute();
        check(invalid && !invalid->exact &&
              invalid->attempt_start_order ==
                  o::MoveAttemptStartOrder::NotStarted &&
              invalid->reason == "native_attempt_without_gate");
        check(!writer.ready());
    }
    {
        auto frame = initial;
        o::LiveMoveWriter writer{15, {
            [&] { return std::optional{frame}; },
            [&](const auto&) { return allowed; },
            [&](const auto&, const Rect& target, const Rect& positioning, std::uint64_t) {
                if (!writer.begin_native_attempt(15)) return o::MoveNativePlacement{};
                frame = {positioning, target};
                return o::MoveNativePlacement{true, true};
            },
            [&](const auto&, const auto&) { return false; }}};
        check(writer.offer({15, 1, one}));
        const auto rejected = writer.wait_and_execute();
        check(rejected && rejected->native_attempted &&
              rejected->after && rejected->after->visible == one &&
              rejected->geometry_exact && !rejected->post_context_exact &&
              !rejected->exact &&
              rejected->reason == "post_context_rejected");
        check(!writer.ready());
    }
    {
        // Regression A: a matching Raw UP retires the writer while capture
        // is in flight. The adapter rejects a *new* native call, but no
        // native failure occurred and the final gate must not be claimed.
        auto frame = initial;
        bool matching_raw_up = false;
        bool context_valid = true;
        int placements = 0;
        o::LiveMoveWriter writer{16, {
            [&] {
                matching_raw_up = true;
                (void)writer.retire(16);
                return std::optional{frame};
            },
            [&](const auto&) {
                return o::classify_move_preflight(
                    {context_valid, !matching_raw_up, matching_raw_up});
            },
            [&](const auto&, const Rect&, const Rect&, std::uint64_t) {
                ++placements;
                return o::MoveNativePlacement{true, true};
            },
            [&](const auto&, const auto&) {
                return o::move_post_context_valid(
                    {context_valid, true, true, true});
            }}};
        check(writer.offer({16, 1, one}));
        const auto up_before_native = writer.wait_and_execute();
        check(up_before_native && !up_before_native->native_attempted &&
               !up_before_native->dispatch_reserved && placements == 0 &&
               up_before_native->reason == "normal_up_before_native");
        check(up_before_native &&
              !o::move_write_receipt_failed(*up_before_native, false));
        check(!writer.offer({16, 2, two}));
    }
    {
        // Regression B: the native call succeeds, then matching Raw UP
        // retires this generation before exact geometry post-verification.
        // The already-issued result must be verified, not re-authorized.
        auto frame = initial;
        bool matching_raw_up = false;
        bool context_valid = true;
        int placements = 0;
        o::LiveMoveWriter writer{17, {
            [&] { return std::optional{frame}; },
            [&](const auto&) {
                return o::classify_move_preflight(
                    {context_valid, !matching_raw_up, matching_raw_up});
            },
            [&](const auto&, const Rect& target, const Rect& positioning, std::uint64_t) {
                if (!writer.begin_native_attempt(17)) return o::MoveNativePlacement{};
                ++placements;
                frame = {positioning, target};
                matching_raw_up = true;
                (void)writer.retire(17);
                return o::MoveNativePlacement{true, true};
            },
            [&](const auto&, const auto& after) {
                return o::move_post_context_valid({
                    context_valid, true, true,
                    after.visible == one && after.positioning ==
                        Rect{110, 120, 330, 240}});
            }}};
        check(writer.offer({17, 1, one}));
        const auto up_after_native = writer.wait_and_execute();
        check(up_after_native && up_after_native->native_attempted &&
               up_after_native->geometry_exact && up_after_native->exact &&
               up_after_native->retired_during_dispatch && placements == 1);
        check(up_after_native &&
              !o::move_write_receipt_failed(*up_after_native, true) &&
              o::move_write_receipt_failed(*up_after_native, false));
        check(!writer.offer({17, 2, two}));
    }
    {
        // A matching UP does not hide independent identity/context loss.
        auto frame = initial;
        int placements = 0;
        o::LiveMoveWriter writer{18, {
            [&] {
                (void)writer.retire(18);
                return std::optional{frame};
            },
            [&](const auto&) {
                return o::classify_move_preflight({false, false, true});
            },
            [&](const auto&, const Rect&, const Rect&, std::uint64_t) {
                ++placements;
                return o::MoveNativePlacement{true, true};
            },
            [&](const auto&, const auto&) { return true; }}};
        check(writer.offer({18, 1, one}));
        const auto invalid = writer.wait_and_execute();
        check(invalid && !invalid->native_attempted &&
               invalid->reason == "fresh_authority_rejected" &&
               placements == 0 && !writer.ready());
        check(invalid && o::move_write_receipt_failed(*invalid, false));
    }
    {
        // After native entry, UP does not excuse an invalid post-context.
        auto frame = initial;
        o::LiveMoveWriter writer{19, {
            [&] { return std::optional{frame}; },
            [&](const auto&) { return allowed; },
            [&](const auto&, const Rect& target, const Rect& positioning, std::uint64_t) {
                if (!writer.begin_native_attempt(19)) return o::MoveNativePlacement{};
                frame = {positioning, target};
                (void)writer.retire(19);
                return o::MoveNativePlacement{true, true};
            },
            [&](const auto&, const auto&) {
                return o::move_post_context_valid({true, true, false, true});
            }}};
        check(writer.offer({19, 1, one}));
        const auto invalid = writer.wait_and_execute();
        check(invalid && invalid->native_attempted && invalid->geometry_exact &&
               !invalid->post_context_exact && !invalid->exact &&
               invalid->reason == "post_context_rejected" && !writer.ready());
        check(invalid && o::move_write_receipt_failed(*invalid, false));
    }

    std::cout << "live-move-writer checks=" << checks
              << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
