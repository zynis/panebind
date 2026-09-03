#include "platform/windows/explorer/explorer_glue_event_source.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace explorer = panebind::platform::windows::explorer;
using TestAccess = explorer::detail::ExplorerGlueEventSourceTestAccess;

int failures = 0;

void expect(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

LRESULT CALLBACK test_window_proc(const HWND window,
                                  const UINT message,
                                  const WPARAM wparam,
                                  const LPARAM lparam) {
    return DefWindowProcW(window, message, wparam, lparam);
}

class TestWindows final {
public:
    TestWindows() {
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.lpfnWndProc = test_window_proc;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.lpszClassName = class_name;
        atom_ = RegisterClassExW(&window_class);
        if (atom_ == 0U && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            std::abort();
        }

        leader_ = make_window(nullptr, WS_OVERLAPPED);
        follower_ = make_window(nullptr, WS_OVERLAPPED);
        unrelated_ = make_window(nullptr, WS_OVERLAPPED);
        child_ = make_window(leader_, WS_CHILD);
        if (leader_ == nullptr || follower_ == nullptr ||
            unrelated_ == nullptr || child_ == nullptr) {
            std::abort();
        }
    }

    ~TestWindows() {
        if (child_ != nullptr) {
            DestroyWindow(child_);
        }
        if (unrelated_ != nullptr) {
            DestroyWindow(unrelated_);
        }
        if (follower_ != nullptr) {
            DestroyWindow(follower_);
        }
        if (leader_ != nullptr) {
            DestroyWindow(leader_);
        }
        if (atom_ != 0U) {
            UnregisterClassW(class_name, GetModuleHandleW(nullptr));
        }
    }

    TestWindows(const TestWindows&) = delete;
    TestWindows& operator=(const TestWindows&) = delete;

    [[nodiscard]] HWND leader() const noexcept { return leader_; }
    [[nodiscard]] HWND follower() const noexcept { return follower_; }
    [[nodiscard]] HWND unrelated() const noexcept { return unrelated_; }
    [[nodiscard]] HWND child() const noexcept { return child_; }
    [[nodiscard]] bool destroy_leader() noexcept {
        const HWND window = std::exchange(leader_, nullptr);
        child_ = nullptr;
        return window != nullptr && DestroyWindow(window) != FALSE;
    }
    [[nodiscard]] bool destroy_unrelated() noexcept {
        const HWND window = std::exchange(unrelated_, nullptr);
        return window != nullptr && DestroyWindow(window) != FALSE;
    }

private:
    [[nodiscard]] static HWND make_window(const HWND parent,
                                          const DWORD style) {
        return CreateWindowExW(0U,
                               class_name,
                               L"",
                               style,
                               0,
                               0,
                               200,
                               120,
                               parent,
                               nullptr,
                               GetModuleHandleW(nullptr),
                               nullptr);
    }

    static constexpr const wchar_t* class_name =
        L"PaneBind.R1C2B.EventSourceTest";
    ATOM atom_{};
    HWND leader_{};
    HWND follower_{};
    HWND unrelated_{};
    HWND child_{};
};

[[nodiscard]] explorer::detail::ExplorerGlueEventSourceTestBinding binding(
    const HWND window,
    const explorer::ExplorerGlueWindowRole role,
    const std::uint64_t window_id,
    const std::uint64_t generation) {
    DWORD process_id = 0U;
    const DWORD thread_id = GetWindowThreadProcessId(window, &process_id);
    return {window, process_id, thread_id, window_id, generation, role};
}

[[nodiscard]] explorer::detail::ExplorerGlueSyntheticWinEvent event(
    const DWORD kind,
    const HWND window,
    const DWORD event_time = 0U) {
    DWORD process_id = 0U;
    const DWORD thread_id = GetWindowThreadProcessId(window, &process_id);
    static_cast<void>(process_id);
    return {kind,
            window,
            OBJID_WINDOW,
            CHILDID_SELF,
            thread_id,
            event_time};
}

void test_filter_and_lifecycle(const TestWindows& windows) {
    auto source = TestAccess::create(
        binding(windows.leader(),
                explorer::ExplorerGlueWindowRole::Leader,
                11U,
                101U),
        binding(windows.follower(),
                explorer::ExplorerGlueWindowRole::Follower,
                22U,
                202U),
        16U);

    TestAccess::enqueue(
        *source,
        event(EVENT_SYSTEM_MOVESIZESTART, windows.leader(), 10U));
    TestAccess::enqueue(
        *source,
        event(EVENT_OBJECT_LOCATIONCHANGE, windows.leader(), 11U));
    TestAccess::enqueue(
        *source,
        event(EVENT_OBJECT_LOCATIONCHANGE, windows.follower(), 12U));
    TestAccess::enqueue(
        *source,
        event(EVENT_SYSTEM_MOVESIZEEND, windows.leader(), 13U));

    auto wrong_object =
        event(EVENT_OBJECT_LOCATIONCHANGE, windows.follower(), 14U);
    wrong_object.object_id = OBJID_CLIENT;
    TestAccess::enqueue(*source, wrong_object);
    TestAccess::enqueue(
        *source,
        event(EVENT_OBJECT_LOCATIONCHANGE, windows.unrelated(), 15U));
    TestAccess::enqueue(*source, event(EVENT_OBJECT_SHOW, windows.leader(), 16U));

    const auto result = TestAccess::drain(*source);
    expect(result.events.size() == 4U,
           "only exact target lifecycle/location receipts are emitted");
    if (result.events.size() == 4U) {
        expect(result.events[0].kind ==
                   explorer::ExplorerGlueEventKind::MoveResizeStarted,
               "START is normalized separately");
        expect(result.events[1].kind ==
                   explorer::ExplorerGlueEventKind::GeometryChanged,
               "leader LOCATION is normalized");
        expect(result.events[2].kind ==
                   explorer::ExplorerGlueEventKind::GeometryChanged,
               "follower LOCATION is normalized");
        expect(result.events[3].kind ==
                   explorer::ExplorerGlueEventKind::MoveResizeEnded,
               "END is normalized separately");
        expect(result.events[0].role ==
                   explorer::ExplorerGlueWindowRole::Leader &&
                   result.events[2].role ==
                       explorer::ExplorerGlueWindowRole::Follower,
               "role comes from the frozen binding");
        expect(result.events[0].window_id == 11U &&
                   result.events[2].window_id == 22U &&
                   result.events[0].capability_generation == 101U &&
                   result.events[2].capability_generation == 202U,
               "logical identity and capability generation are preserved");
        expect(result.events[0].receipt_sequence == 1U &&
                   result.events[1].receipt_sequence == 2U &&
                   result.events[2].receipt_sequence == 3U &&
                   result.events[3].receipt_sequence == 4U,
               "accepted lifecycle is ordered by monotonic receipt sequence");
    }
    expect(result.facts.ignored_object_child_count == 1U,
           "non-window object receipts are ignored");
    expect(result.facts.ignored_other_window_count == 1U,
           "other HWND receipts are ignored");
    expect(result.facts.ignored_event_count == 1U,
           "unsupported events are ignored");
    expect(result.facts.poison ==
               explorer::ExplorerGlueEventSourcePoison::None,
           "ignored noise does not poison the source");
    expect(result.facts.unique_target_process_count == 1U,
           "same-process targets produce one process filter");
    expect(result.facts.lifecycle_hook_count == 1U &&
               result.facts.location_hook_count == 1U &&
               result.facts.destroy_hook_count == 1U,
           "each unique target PID has separate lifecycle, location, and destroy slots");
    expect(result.facts.hook_flags ==
               (WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS),
           "the live hook flags are fixed and auditable");
}

void test_queue_overflow_is_poison(const TestWindows& windows) {
    auto source = TestAccess::create(
        binding(windows.leader(),
                explorer::ExplorerGlueWindowRole::Leader,
                31U,
                301U),
        binding(windows.follower(),
                explorer::ExplorerGlueWindowRole::Follower,
                32U,
                302U),
        2U);
    TestAccess::enqueue(
        *source,
        event(EVENT_SYSTEM_MOVESIZESTART, windows.leader()));
    TestAccess::enqueue(
        *source,
        event(EVENT_OBJECT_LOCATIONCHANGE, windows.leader()));
    TestAccess::enqueue(
        *source,
        event(EVENT_OBJECT_LOCATIONCHANGE, windows.leader()));

    const auto before_drain = TestAccess::facts(*source);
    expect(before_drain.poison ==
               explorer::ExplorerGlueEventSourcePoison::QueueOverflow,
           "queue overflow poisons instead of dropping and continuing");
    expect(before_drain.queue_depth == 2U &&
               before_drain.max_queue_depth == 2U &&
               before_drain.overflow_count == 1U &&
               before_drain.latest_receipt_sequence == 3U,
           "bounded queue facts preserve overflow evidence");

    const auto result = TestAccess::drain(*source);
    expect(result.events.size() == 2U,
           "receipts queued before overflow remain diagnosable");
    TestAccess::enqueue(
        *source,
        event(EVENT_SYSTEM_MOVESIZEEND, windows.leader()));
    expect(TestAccess::facts(*source).post_poison_count == 1U,
           "events after poison cannot mutate the active queue");
}

void test_notification_failure_is_poison(const TestWindows& windows) {
    auto source = TestAccess::create(
        binding(windows.leader(),
                explorer::ExplorerGlueWindowRole::Leader,
                41U,
                401U),
        binding(windows.follower(),
                explorer::ExplorerGlueWindowRole::Follower,
                42U,
                402U),
        4U,
        false);
    TestAccess::enqueue(
        *source,
        event(EVENT_SYSTEM_MOVESIZESTART, windows.leader()));

    const auto facts = TestAccess::facts(*source);
    expect(facts.poison ==
               explorer::ExplorerGlueEventSourcePoison::NotificationFailure,
           "owner notification failure poisons immediately");
    expect(facts.notification_failure_count == 1U && facts.queue_depth == 1U,
           "notification failure retains the undelivered receipt evidence");
}

void test_process_slots_and_hook_receipt_validation(const TestWindows& windows) {
    auto second_process = binding(windows.follower(),
                                  explorer::ExplorerGlueWindowRole::Follower,
                                  46U,
                                  406U);
    ++second_process.process_id;
    auto two_process_source = TestAccess::create(
        binding(windows.leader(),
                explorer::ExplorerGlueWindowRole::Leader,
                45U,
                405U),
        second_process,
        4U);
    const auto slot_facts = TestAccess::facts(*two_process_source);
    expect(slot_facts.unique_target_process_count == 2U &&
               slot_facts.lifecycle_hook_count == 2U &&
               slot_facts.location_hook_count == 2U &&
               slot_facts.destroy_hook_count == 2U,
           "each unique target PID receives exactly three distinct hook slots");

    auto source = TestAccess::create(
        binding(windows.leader(),
                explorer::ExplorerGlueWindowRole::Leader,
                47U,
                407U),
        binding(windows.follower(),
                explorer::ExplorerGlueWindowRole::Follower,
                48U,
                408U),
        4U);
    auto mismatched_hook =
        event(EVENT_OBJECT_LOCATIONCHANGE, windows.leader());
    mismatched_hook.matching_hook_slot = false;
    TestAccess::enqueue(*source, mismatched_hook);
    const auto result = TestAccess::drain(*source);
    expect(result.events.empty() &&
               result.facts.poison ==
                   explorer::ExplorerGlueEventSourcePoison::HookReceiptMismatch,
           "an exact HWND receipt must originate from its process-filtered hook slot");
}

void test_identity_and_root_revalidation(const TestWindows& windows) {
    auto wrong_process = binding(windows.leader(),
                                 explorer::ExplorerGlueWindowRole::Leader,
                                 51U,
                                 501U);
    ++wrong_process.process_id;
    auto source = TestAccess::create(
        wrong_process,
        binding(windows.follower(),
                explorer::ExplorerGlueWindowRole::Follower,
                52U,
                502U),
        4U);
    TestAccess::enqueue(
        *source,
        event(EVENT_SYSTEM_MOVESIZESTART, windows.leader()));
    const auto identity_result = TestAccess::drain(*source);
    expect(identity_result.events.empty() &&
               identity_result.facts.poison ==
                   explorer::ExplorerGlueEventSourcePoison::TargetIdentityMismatch,
           "exact HWND with PID/TID drift poisons");

    auto child_source = TestAccess::create(
        binding(windows.child(),
                explorer::ExplorerGlueWindowRole::Leader,
                53U,
                503U),
        binding(windows.follower(),
                explorer::ExplorerGlueWindowRole::Follower,
                54U,
                504U),
        4U);
    TestAccess::enqueue(
        *child_source,
        event(EVENT_SYSTEM_MOVESIZESTART, windows.child()));
    const auto root_result = TestAccess::drain(*child_source);
    expect(root_result.events.empty() &&
               root_result.facts.poison ==
                   explorer::ExplorerGlueEventSourcePoison::TargetNotRoot,
           "an exact target that is no longer root poisons");

    auto wrong_event_thread = TestAccess::create(
        binding(windows.leader(),
                explorer::ExplorerGlueWindowRole::Leader,
                55U,
                505U),
        binding(windows.follower(),
                explorer::ExplorerGlueWindowRole::Follower,
                56U,
                506U),
        4U);
    auto mismatched_receipt =
        event(EVENT_SYSTEM_MOVESIZESTART, windows.leader());
    ++mismatched_receipt.event_thread;
    TestAccess::enqueue(*wrong_event_thread, mismatched_receipt);
    const auto event_thread_result = TestAccess::drain(*wrong_event_thread);
    expect(event_thread_result.events.size() == 1U &&
               event_thread_result.events.front().native_event_thread ==
                   mismatched_receipt.event_thread &&
               event_thread_result.facts.poison ==
                   explorer::ExplorerGlueEventSourcePoison::None,
           "WinEvent producer TID is retained as auxiliary evidence, not sole authority");
}

void test_target_destroy_and_unrelated_destroy() {
    {
        TestWindows windows;
        auto source = TestAccess::create(
            binding(windows.leader(),
                    explorer::ExplorerGlueWindowRole::Leader,
                    57U,
                    507U),
            binding(windows.follower(),
                    explorer::ExplorerGlueWindowRole::Follower,
                    58U,
                    508U),
            4U);

        TestAccess::enqueue(
            *source,
            event(EVENT_OBJECT_DESTROY, windows.leader(), 77U));
        expect(windows.destroy_leader(),
               "test leader is destroyed before owner drain");
        const auto target_result = TestAccess::drain(*source);
        expect(target_result.events.size() == 1U &&
                   target_result.events.front().kind ==
                       explorer::ExplorerGlueEventKind::TargetDestroyed &&
                   target_result.events.front().role ==
                       explorer::ExplorerGlueWindowRole::Leader &&
                   target_result.facts.poison ==
                       explorer::ExplorerGlueEventSourcePoison::None,
               "exact target destroy is emitted without querying a dead HWND");
    }

    {
        TestWindows windows;
        auto unrelated_source = TestAccess::create(
            binding(windows.leader(),
                    explorer::ExplorerGlueWindowRole::Leader,
                    59U,
                    509U),
            binding(windows.follower(),
                    explorer::ExplorerGlueWindowRole::Follower,
                    60U,
                    510U),
            4U);
        TestAccess::enqueue(
            *unrelated_source,
            event(EVENT_OBJECT_DESTROY, windows.unrelated(), 78U));
        expect(windows.destroy_unrelated(),
               "test unrelated window is destroyed before owner drain");
        const auto unrelated_result = TestAccess::drain(*unrelated_source);
        expect(unrelated_result.events.empty() &&
                   unrelated_result.facts.ignored_other_window_count == 1U &&
                   unrelated_result.facts.poison ==
                       explorer::ExplorerGlueEventSourcePoison::None,
               "unrelated destroy is ignored without changing source state");
    }
}

void test_thread_reentrancy_and_unhook_poison(const TestWindows& windows) {
    auto source = TestAccess::create(
        binding(windows.leader(),
                explorer::ExplorerGlueWindowRole::Leader,
                61U,
                601U),
        binding(windows.follower(),
                explorer::ExplorerGlueWindowRole::Follower,
                62U,
                602U),
        4U);
    std::thread wrong_thread([&source]() {
        static_cast<void>(TestAccess::drain(*source));
    });
    wrong_thread.join();
    auto facts = TestAccess::facts(*source);
    expect(facts.poison ==
               explorer::ExplorerGlueEventSourcePoison::WrongOwnerThread &&
               facts.wrong_thread_count == 1U,
           "wrong-thread state mutation is explicit poison");

    auto reentrant = TestAccess::create(
        binding(windows.leader(),
                explorer::ExplorerGlueWindowRole::Leader,
                63U,
                603U),
        binding(windows.follower(),
                explorer::ExplorerGlueWindowRole::Follower,
                64U,
                604U),
        4U);
    TestAccess::simulate_reentrant_drain(*reentrant);
    facts = TestAccess::facts(*reentrant);
    expect(facts.poison ==
               explorer::ExplorerGlueEventSourcePoison::ReentrantDrain &&
               facts.reentrant_count == 1U,
           "reentrant owner drain is explicit poison");

    auto unhook_failure = TestAccess::create(
        binding(windows.leader(),
                explorer::ExplorerGlueWindowRole::Leader,
                65U,
                605U),
        binding(windows.follower(),
                explorer::ExplorerGlueWindowRole::Follower,
                66U,
                606U),
        4U);
    TestAccess::stop(*unhook_failure, true);
    facts = TestAccess::facts(*unhook_failure);
    expect(facts.poison ==
               explorer::ExplorerGlueEventSourcePoison::HookUnhookFailure &&
               !facts.running,
           "unhook failure poisons a stopped source");
}

void test_invalid_binding_and_post_stop_event(const TestWindows& windows) {
    auto invalid = binding(windows.leader(),
                           explorer::ExplorerGlueWindowRole::Leader,
                           71U,
                           701U);
    invalid.window = nullptr;
    auto invalid_source = TestAccess::create(
        invalid,
        binding(windows.follower(),
                explorer::ExplorerGlueWindowRole::Follower,
                72U,
                702U),
        4U);
    expect(TestAccess::facts(*invalid_source).poison ==
               explorer::ExplorerGlueEventSourcePoison::InvalidBinding,
           "invalid authority bindings fail closed");

    auto stopped = TestAccess::create(
        binding(windows.leader(),
                explorer::ExplorerGlueWindowRole::Leader,
                73U,
                703U),
        binding(windows.follower(),
                explorer::ExplorerGlueWindowRole::Follower,
                74U,
                704U),
        4U);
    TestAccess::stop(*stopped);
    TestAccess::enqueue(
        *stopped,
        event(EVENT_SYSTEM_MOVESIZESTART, windows.leader()));
    const auto facts = TestAccess::facts(*stopped);
    expect(facts.poison == explorer::ExplorerGlueEventSourcePoison::None &&
               !facts.running && facts.post_poison_count == 1U &&
               facts.queue_depth == 0U,
           "events after an orderly stop are discarded without rearming");
}

} // namespace

int main() {
    const TestWindows windows;
    test_filter_and_lifecycle(windows);
    test_queue_overflow_is_poison(windows);
    test_notification_failure_is_poison(windows);
    test_process_slots_and_hook_receipt_validation(windows);
    test_identity_and_root_revalidation(windows);
    test_target_destroy_and_unrelated_destroy();
    test_thread_reentrancy_and_unhook_poison(windows);
    test_invalid_binding_and_post_stop_event(windows);

    if (failures != 0) {
        std::cerr << failures << " explorer glue event-source test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Explorer glue event-source tests passed\n";
    return EXIT_SUCCESS;
}
