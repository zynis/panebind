#pragma once

// Synthetic test-driver policy only. This header has no native input authority
// and must never be included by a product target.
namespace panebind::test::foreground {

struct ActivationProof {
    bool own_identity{};
    bool desktop_ready{};
    bool same_integrity{};
    bool visible{};
    bool temporary_topmost{};
    bool point_root_matches{};
    bool client_hit{};
    bool foreign_capture_clear{};
    bool modifiers_clear{};
    bool button_state_matches{};
};

// The special activation path must establish every proof before it can emit a
// click. The native driver is responsible for collecting fresh observations.
inline const char* activation_guard(const ActivationProof& proof) {
    if (!proof.own_identity || !proof.same_integrity)
        return "BLOCKED_BY_ACTIVATION_IDENTITY";
    if (!proof.desktop_ready)
        return "BLOCKED_BY_INTERACTIVE_DESKTOP";
    if (!proof.visible || !proof.temporary_topmost)
        return "BLOCKED_BY_ACTIVATION_VISIBILITY";
    if (!proof.point_root_matches || !proof.client_hit)
        return "BLOCKED_BY_ACTIVATION_HIT_TEST";
    if (!proof.foreign_capture_clear)
        return "BLOCKED_BY_FOREIGN_INPUT_CAPTURE";
    if (!proof.modifiers_clear || !proof.button_state_matches)
        return "BLOCKED_BY_INPUT_INTERFERENCE";
    return "none";
}

// An already verified direct foreground transfer needs no synthetic click and
// therefore no activation-point or temporary-topmost proof. Normal drag input
// remains subject to its separate native foreground/input fences.
inline bool direct_ready(bool api_result, bool foreground_matches,
                         const ActivationProof& proof) {
    return api_result && foreground_matches && proof.own_identity &&
           proof.desktop_ready && proof.same_integrity && proof.visible;
}

struct ActivationCompletion {
    bool move_success{};
    bool down_success{};
    bool up_success{};
    bool activation_event_seen{};
    bool final_foreground_matches{};
    bool topmost_restored{};
};

inline const char* completion_guard(const ActivationCompletion& completion) {
    if (!completion.move_success || !completion.down_success ||
        !completion.up_success)
        return "BLOCKED_BY_SENDINPUT";
    if (!completion.activation_event_seen ||
        !completion.final_foreground_matches)
        return "BLOCKED_BY_ACTIVATION_FAILURE";
    if (!completion.topmost_restored)
        return "BLOCKED_BY_ACTIVATION_VISIBILITY";
    return "none";
}

} // namespace panebind::test::foreground
