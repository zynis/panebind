#include "platform/windows/operations/test_foreground_bootstrap_model.h"

#include <initializer_list>
#include <iostream>
#include <string_view>

namespace f = panebind::test::foreground;

int main() {
    int checks{}, failures{};
    const auto check = [&](bool result, std::string_view name) {
        ++checks;
        if (!result) {
            ++failures;
            std::cerr << "failed: " << name << '\n';
        }
    };
    const auto matches = [](const char* actual, std::string_view expected) {
        return std::string_view{actual} == expected;
    };
    const f::ActivationProof complete_proof{
        true, true, true, true, true, true, true, true, true, true};
    const f::ActivationCompletion complete_result{
        true, true, true, true, true, true};

    // A-H are synthetic policy cases, not observations of Windows or SendInput.
    // This counter models the caller's guard-before-click contract; it never
    // sends native input, and a rejected proof must leave it at zero.
    const auto attempt_click = [&](const f::ActivationProof& proof,
                                   int& synthetic_clicks) {
        const auto* reason = f::activation_guard(proof);
        if (!matches(reason, "none")) return reason;
        ++synthetic_clicks;
        return reason;
    };

    // A: direct success is possible without any click-specific proof.
    auto direct_proof = complete_proof;
    direct_proof.temporary_topmost = false;
    direct_proof.point_root_matches = false;
    direct_proof.client_hit = false;
    int clicks{};
    check(f::direct_ready(true, true, direct_proof), "A direct success");
    check(clicks == 0, "A no synthetic activation click");
    check(!f::direct_ready(false, true, direct_proof),
          "A denied API cannot claim direct success");
    check(!f::direct_ready(true, false, direct_proof),
          "A API success requires fresh foreground match");

    // B: denial routes to a fully proven activation click, then an event and
    // fresh foreground observation must authorize continuation.
    check(!f::direct_ready(false, false, complete_proof), "B direct denied");
    check(matches(attempt_click(complete_proof, clicks), "none") && clicks == 1,
          "B verified synthetic click");
    check(matches(f::completion_guard(complete_result), "none"),
          "B activation completion accepted");

    // C-E: hit testing and foreign GUI state reject before emitting a click.
    for (const auto member : {&f::ActivationProof::point_root_matches,
                              &f::ActivationProof::client_hit,
                              &f::ActivationProof::foreign_capture_clear}) {
        auto proof = complete_proof;
        proof.*member = false;
        clicks = 0;
        const auto* expected = member == &f::ActivationProof::foreign_capture_clear
                                   ? "BLOCKED_BY_FOREIGN_INPUT_CAPTURE"
                                   : "BLOCKED_BY_ACTIVATION_HIT_TEST";
        check(matches(attempt_click(proof, clicks), expected),
              "C-E reject unsafe activation proof");
        check(clicks == 0, "C-E zero click on rejected proof");
    }

    // F: any missing/partial SendInput stage is a failure, even when an
    // unrelated activation event and matching foreground are also present.
    for (const auto member : {&f::ActivationCompletion::move_success,
                              &f::ActivationCompletion::down_success,
                              &f::ActivationCompletion::up_success}) {
        auto result = complete_result;
        result.*member = false;
        check(matches(f::completion_guard(result), "BLOCKED_BY_SENDINPUT"),
              "F failed or partial injected stage blocks");
    }

    auto result = complete_result;
    result.activation_event_seen = false;
    check(matches(f::completion_guard(result), "BLOCKED_BY_ACTIVATION_FAILURE"),
          "G no activation event");
    result = complete_result;
    result.final_foreground_matches = false;
    check(matches(f::completion_guard(result), "BLOCKED_BY_ACTIVATION_FAILURE"),
          "H event does not replace fresh foreground proof");
    result = complete_result;
    result.topmost_restored = false;
    check(matches(f::completion_guard(result), "BLOCKED_BY_ACTIVATION_VISIBILITY"),
          "temporary topmost must be restored");

    struct GuardCase {
        bool f::ActivationProof::*member;
        const char* reason;
    };
    const GuardCase guard_cases[]{
        {&f::ActivationProof::own_identity, "BLOCKED_BY_ACTIVATION_IDENTITY"},
        {&f::ActivationProof::desktop_ready, "BLOCKED_BY_INTERACTIVE_DESKTOP"},
        {&f::ActivationProof::same_integrity, "BLOCKED_BY_ACTIVATION_IDENTITY"},
        {&f::ActivationProof::visible, "BLOCKED_BY_ACTIVATION_VISIBILITY"},
        {&f::ActivationProof::temporary_topmost, "BLOCKED_BY_ACTIVATION_VISIBILITY"},
        {&f::ActivationProof::point_root_matches, "BLOCKED_BY_ACTIVATION_HIT_TEST"},
        {&f::ActivationProof::client_hit, "BLOCKED_BY_ACTIVATION_HIT_TEST"},
        {&f::ActivationProof::foreign_capture_clear, "BLOCKED_BY_FOREIGN_INPUT_CAPTURE"},
        {&f::ActivationProof::modifiers_clear, "BLOCKED_BY_INPUT_INTERFERENCE"},
        {&f::ActivationProof::button_state_matches, "BLOCKED_BY_INPUT_INTERFERENCE"},
    };
    for (const auto& test : guard_cases) {
        auto proof = complete_proof;
        proof.*(test.member) = false;
        clicks = 0;
        check(matches(attempt_click(proof, clicks), test.reason), test.reason);
        check(clicks == 0, "every missing proof prevents a synthetic click");
    }
    for (const auto member : {&f::ActivationProof::own_identity,
                              &f::ActivationProof::desktop_ready,
                              &f::ActivationProof::same_integrity,
                              &f::ActivationProof::visible}) {
        auto proof = complete_proof;
        proof.*member = false;
        check(!f::direct_ready(true, true, proof),
              "direct success requires identity/desktop/integrity/visibility");
    }

    check(matches(f::activation_guard({}), "BLOCKED_BY_ACTIVATION_IDENTITY"),
          "default proof fails closed");
    check(matches(f::completion_guard({}), "BLOCKED_BY_SENDINPUT"),
          "default completion fails closed");
    result = complete_result;
    result.down_success = result.activation_event_seen =
        result.final_foreground_matches = result.topmost_restored = false;
    check(matches(f::completion_guard(result), "BLOCKED_BY_SENDINPUT"),
          "failed injection retains failure priority");

    std::cout << "foreground-bootstrap synthetic_only=true checks=" << checks
              << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
