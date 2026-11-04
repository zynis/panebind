#pragma once

// TEST ONLY. Classifies observed facts before the disposable-guest HC lease
// may change configuration. A named candidate is not proof of exact recovery.
// This header deliberately has no Win32 calls or platform-state sampling.
namespace guest_high_contrast_preflight {
enum class Decision {
    RejectSnapshot,
    RejectEmptyScheme,
    AlreadyEnabledReadOnly,
    NamedSchemeCandidate,
};
struct Facts {
    bool snapshot_known{},scheme_known{},scheme_retained{},scheme_borrowed{};
    bool available{},no_theme_change{},enabled{},scheme_empty{};
};
constexpr Decision classify(const Facts& facts) noexcept {
    if(!facts.snapshot_known||!facts.scheme_known||!facts.scheme_retained||facts.scheme_borrowed||
       !facts.available||facts.no_theme_change)return Decision::RejectSnapshot;
    if(facts.enabled)return Decision::AlreadyEnabledReadOnly;
    if(facts.scheme_empty)return Decision::RejectEmptyScheme;
    return Decision::NamedSchemeCandidate;
}
constexpr const char* reason(Decision decision) noexcept {
    switch(decision){
    case Decision::RejectSnapshot:return "actual_HC_availability_scheme_or_safe_restore_unknown";
    case Decision::RejectEmptyScheme:return "original_empty_HC_scheme_exact_restore_not_proven";
    case Decision::AlreadyEnabledReadOnly:return "already_ON_read_only_no_SET";
    case Decision::NamedSchemeCandidate:return "named_scheme_candidate_exact_restore_still_required";
    }
    return "unknown_HC_preflight_rejected";
}
}
