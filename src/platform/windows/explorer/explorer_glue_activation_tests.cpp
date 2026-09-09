#include "platform/windows/explorer/explorer_glue_activation.h"
#include "core/behavior/glue_move_coordinator.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace e = panebind::platform::windows::explorer;
namespace b = panebind::core::behavior;
namespace g = panebind::core::geometry;
namespace m = panebind::core::model;
namespace t = panebind::core::topology;
namespace {
int failures{};
void check(bool ok, const char* message) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
constexpr e::CtrlSample down{true, true, true, false};
constexpr e::CtrlSample up{true, false, false, false};
e::ExplorerGlueActivationPair pair() { return {11, 12, 1, 101, 2, 102, 201, 202, 1}; }
e::ExplorerGlueEvent start(e::CtrlSample sample = down) {
    return {e::ExplorerGlueEventKind::MoveResizeStarted,
        e::ExplorerGlueWindowRole::Leader, 1, 101, 1, 0, 0, 100, sample};
}
void test_samples() {
    check(!e::ctrl_high_bit(1) && !e::ctrl_high_bit(0) &&
        e::ctrl_high_bit(std::numeric_limits<std::int16_t>::min()) &&
        e::ctrl_high_bit(-1), "only high bit, never unreliable low bit");
    for (const bool left : {false, true}) for (const bool right : {false, true}) {
        int calls{};
        const auto sampled = e::sample_ctrl_with([&](int key) noexcept -> std::int16_t {
            ++calls;
            const bool pressed = key == VK_CONTROL ? left || right :
                key == VK_LCONTROL ? left : right;
            return pressed ? std::numeric_limits<std::int16_t>::min() : 1;
        });
        check(calls == 3 && sampled.available && sampled.ctrl == (left || right) &&
            sampled.left == left && sampled.right == right, "left/right/both/neither sampling");
        for (const auto owner : {down, up}) {
            e::ExplorerGlueActivationController controller(pair());
            const auto result = controller.evaluate_start(start(sampled), owner, pair(), true, 110, 120);
            check(result.activated == sampled.ctrl && result.owner_ctrl.ctrl == owner.ctrl,
                "callback authority preserved when owner differs");
        }
    }
    auto non_atomic = down;
    non_atomic.left = false;
    e::ExplorerGlueActivationController controller(pair());
    check(controller.evaluate_start(start(non_atomic), up, pair(), true, 110, 120).activated,
        "three reads not atomic: aggregate authoritative, sides diagnostic");
}
void test_rejections_and_generations() {
    using Reason = e::ExplorerGlueActivationReason;
    auto rejected = [](e::ExplorerGlueEvent event, e::ExplorerGlueActivationPair current,
                        bool live, Reason expected) {
        e::ExplorerGlueActivationController controller(pair());
        const auto result = controller.evaluate_start(event, down, current, live, 110, 120);
        check(!result.activated && result.reason == expected && !controller.is_active(),
            "invalid START cannot authorize native operation");
    };
    auto event = start(); event.role = e::ExplorerGlueWindowRole::Follower; event.window_id = 2;
    rejected(event, pair(), true, Reason::WrongWindow);
    event = start(); event.window_id = 3;
    rejected(event, pair(), true, Reason::WrongWindow);
    event = start(); event.capability_generation++;
    rejected(event, pair(), true, Reason::WrongTargetGeneration);
    auto stale = pair(); stale.authority_generation++;
    rejected(start(), stale, true, Reason::StalePairAuthority);
    stale = pair(); stale.follower_consent_generation++;
    rejected(start(), stale, true, Reason::StalePairAuthority);
    rejected(start(), pair(), false, Reason::StalePairAuthority);
    event = start(); event.ctrl_callback.available = false;
    rejected(event, pair(), true, Reason::MissingCtrlSample);
    event = start(); event.callback_qpc = 111;
    rejected(event, pair(), true, Reason::InvalidTiming);
    event = start(); event.receipt_sequence = 0;
    rejected(event, pair(), true, Reason::InvalidStart);
    e::ExplorerGlueActivationController controller(pair());
    const auto active = controller.evaluate_start(start(), down, pair(), true, 110, 120);
    check(active.activated && active.activation_generation != 0 &&
        controller.matches_activation(active.activation_generation, pair()), "unique bound activation");
    check(controller.stamp_last_decision_complete(121) &&
        controller.attempts().front().decision_qpc == 121,
        "live completion stamp follows policy evaluation");
    check(!controller.matches_activation(active.activation_generation + 1, pair()) &&
        !controller.matches_activation(active.activation_generation, stale), "operation generation/pair mismatch");
    event = start(); event.receipt_sequence = 2;
    const auto duplicate = controller.evaluate_start(event, down, pair(), true, 110, 120);
    check(duplicate.reason == Reason::DuplicateStart && controller.poisoned() &&
        !controller.is_active() && duplicate.attempt_generation == 2, "double START fails closed");
    for (std::size_t i = 2; i < e::ExplorerGlueActivationController::capacity + 1; ++i) {
        static_cast<void>(controller.evaluate_start(event, down, pair(), true, 110, 120));
    }
    check(controller.overflowed() && controller.attempts().size() ==
        e::ExplorerGlueActivationController::capacity, "bounded attempts, explicit overflow poison");
    e::ExplorerGlueActivationController invalid_completion(pair());
    static_cast<void>(invalid_completion.evaluate_start(start(), down, pair(), true, 110, 120));
    check(!invalid_completion.stamp_last_decision_complete(119) &&
        !invalid_completion.is_active(), "nonmonotonic decision completion fails closed");
}

// Deterministic interaction -> UNMODIFIED Core -> owned native operation sink.
// The negative path starts with a ready owned pair; no setup/restore writes are
// needed. It is not an Explorer eligibility/UAT simulation.
void test_owned_fixture(bool ctrl_at_start, bool press_later, bool resize, bool profiling = false) {
    auto profile = profiling ? std::make_unique<e::ExplorerGlueProfiler>() : nullptr;
    const HWND follower = CreateWindowExW(0, L"STATIC", L"PaneBind owned activation test",
        WS_POPUP, 100, 0, 100, 100, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    check(follower != nullptr, "owned fixture window created, never shown");
    if (!follower) return;
    RECT before{}; GetWindowRect(follower, &before);
    const std::array windows{t::WindowGeometry{m::WindowId{"leader"}, {0, 0, 100, 100}},
        t::WindowGeometry{m::WindowId{"follower"}, {100, 0, 200, 100}}};
    b::GlueMoveCoordinator core;
    const auto graph = t::WindowAdjacencyGraph::build(windows, {});
    check(core.arm(graph, m::WindowId{"leader"}, m::WindowId{"follower"}).kind ==
        b::GlueDecisionKind::Armed, "frozen existing Core topology");
    e::ExplorerGlueActivationController controller(pair());
    const auto activation = controller.evaluate_start(start(ctrl_at_start ? down : up),
        press_later ? down : up, pair(), true, 110, 120);
    std::size_t requested{}, native_calls{};
    std::uint64_t sequence = 1;
    g::Rect leader{0, 0, 100, 100};
    g::Rect follower_rect{100, 0, 200, 100};
    if (activation.activated) {
        check(core.on_event({1, sequence, b::GlueWindowRole::Leader,
            b::GlueEventKind::MoveResizeStarted, leader}).kind == b::GlueDecisionKind::Activated,
            "one callback activation enters existing Core");
    }
    for (int i = 1; i <= 12; ++i) {
        e::GlueProfileScope measured(profile.get(), e::GlueProfileStage::EventPolicy);
        leader = {i, i, 100 + i + (resize ? 1 : 0), 100 + i};
        ++sequence;
        // Later modifier changes do not evaluate START or create activation.
        if (!controller.matches_activation(activation.activation_generation, pair())) continue;
        auto decision = core.on_event({1, sequence, b::GlueWindowRole::Leader,
            b::GlueEventKind::GeometryChanged, leader});
        if (resize) {
            check(decision.abort_reason == b::GlueAbortReason::ResizeOrMixed && !decision.command,
                "Ctrl Resize fails closed before any follower write");
            break;
        }
        if (decision.command) {
            ++requested;
            check(controller.matches_activation(activation.activation_generation, pair()),
                "activation checked immediately before owned native call");
            ++native_calls;
            check(SetWindowPos(follower, nullptr, 100 + i, i, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE, "owned apply exact");
            follower_rect = decision.command->target_visible_rect;
            static_cast<void>(core.on_operation_result({1, decision.command->operation_generation,
                m::WindowId{"follower"}, b::FollowerOperationOutcome::Exact, follower_rect}));
            const auto feedback = core.on_event({1, ++sequence, b::GlueWindowRole::Follower,
                b::GlueEventKind::GeometryChanged, follower_rect});
            check(!feedback.command && feedback.kind == b::GlueDecisionKind::FeedbackAcknowledged,
                "existing feedback suppression no recursion");
        }
    }
    if (activation.activated && !resize) {
        static_cast<void>(core.on_event({1, ++sequence, b::GlueWindowRole::Leader,
            b::GlueEventKind::MoveResizeEnded, leader}));
        check(core.complete({1, leader, follower_rect}).kind == b::GlueDecisionKind::Completed,
            "matching END completes existing runtime");
    }
    controller.finish();
    check(!controller.is_active() && controller.attempts().size() == 1,
        "one activation attempt across entire session, ends once");
    RECT after{}; GetWindowRect(follower, &after);
    if (!ctrl_at_start || resize) {
        check(requested == 0 && native_calls == 0 && EqualRect(&before, &after),
            "no Ctrl/late Ctrl/Resize: zero requests, zero native writes, unchanged owned Follower");
    } else {
        check(requested == 12 && native_calls == 12 && core.stats().suppressed_feedback_count == 12,
            "Ctrl START latched: progressive moves and exact suppression");
    }
    DestroyWindow(follower); // Only this fixture's own never-shown HWND.
    check(!profile || profile->valid(), "profiling ON leaves all existing activation/native/feedback outcomes unchanged");
}
}

int main() {
    test_samples();
    test_rejections_and_generations();
    test_owned_fixture(false, false, false);
    test_owned_fixture(false, true, false);
    test_owned_fixture(true, false, false);
    test_owned_fixture(true, false, true);
    test_owned_fixture(false, false, false, true);
    test_owned_fixture(false, true, false, true);
    test_owned_fixture(true, false, false, true);
    test_owned_fixture(true, false, true, true);
    std::cout << "Ctrl activation tests " << (failures ? "FAIL" : "PASS") << '\n';
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
