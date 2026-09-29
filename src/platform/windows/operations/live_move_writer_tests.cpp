#include "platform/windows/operations/live_move_writer.h"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace o = panebind::platform::windows::operations;
using panebind::core::geometry::Rect;

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
            [&](const auto&) { return true; },
            [&](const auto&, const Rect& target, const Rect& positioning) {
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
            [&](const auto&) { return true; },
            [&](const auto&, const Rect& target, const Rect& positioning) {
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
            [&](const auto&) { return false; }, // no fresh authority
            [&](const auto&, const Rect&, const Rect&) {
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
            [&](const auto&) { return true; },
            [&](const auto&, const Rect& target, const Rect& positioning) {
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
            [&](const auto&) { return true; },
            [&](const auto&, const Rect& target, const Rect& positioning) {
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
            [&](const auto&) { return true; },
            [&](const auto&, const Rect& target, const Rect& positioning) {
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
            [&](const auto&) { return true; },
            [&](const auto&, const Rect&, const Rect&) -> o::MoveNativePlacement {
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
            [&](const auto&) { return true; },
            [&](const auto&, const Rect& target, const Rect& positioning) {
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
            [&](const auto&) { return true; },
            [&](const auto&, const Rect& target, const Rect& positioning) {
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

    std::cout << "live-move-writer checks=" << checks
              << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
