#pragma once

// Pure test-only diagnostics: no Win32 API or product dependency.
// Eligibility facts alone never execute input; native integration is separate.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace panebind::test::abort {
enum class PredicateState { NotEvaluated, Unknown, Pass, Fail };
constexpr std::string_view state_name(PredicateState s) noexcept {
    switch(s){
    case PredicateState::NotEvaluated:return "NOT_EVALUATED";
    case PredicateState::Unknown:return "UNKNOWN";
    case PredicateState::Pass:return "PASS";
    case PredicateState::Fail:return "FAIL";
    }
    return "UNKNOWN";
}
enum class FencePredicate : std::size_t {
    OwnerRunning, LogHealthy, DesktopReady, SourceIdentity, Foreground,
    NoForeignCaptureTransfer, CursorQuery, CursorPosition, LeftState,
    OtherInputClear, GuiQuery, CancelledGuiSafe, InputRootOwned, Capture,
    Count
};
constexpr std::string_view predicate_name(FencePredicate p) noexcept {
    switch(p){
    case FencePredicate::OwnerRunning:return "OWNER_RUNNING";
    case FencePredicate::LogHealthy:return "LOG_HEALTHY";
    case FencePredicate::DesktopReady:return "DESKTOP_READY";
    case FencePredicate::SourceIdentity:return "SOURCE_IDENTITY";
    case FencePredicate::Foreground:return "FOREGROUND";
    case FencePredicate::NoForeignCaptureTransfer:return "NO_FOREIGN_CAPTURE_TRANSFER";
    case FencePredicate::CursorQuery:return "CURSOR_QUERY";
    case FencePredicate::CursorPosition:return "CURSOR_POSITION";
    case FencePredicate::LeftState:return "LEFT_STATE";
    case FencePredicate::OtherInputClear:return "OTHER_INPUT_CLEAR";
    case FencePredicate::GuiQuery:return "GUI_QUERY";
    case FencePredicate::CancelledGuiSafe:return "CANCELLED_GUI_SAFE";
    case FencePredicate::InputRootOwned:return "INPUT_ROOT_OWNED";
    case FencePredicate::Capture:return "CAPTURE";
    case FencePredicate::Count:break;
    }
    return "NONE";
}
struct FenceFailureSnapshot {
    static constexpr std::size_t count=static_cast<std::size_t>(FencePredicate::Count);
    std::array<PredicateState,count> predicates{};
    std::array<bool,count> required{};
    constexpr FenceFailureSnapshot() noexcept {
        // The base predicates are mandatory even for an empty/partial record.
        // Conditional GUI/root/capture branches are selected by the caller.
        for(std::size_t i=0;i<=static_cast<std::size_t>(FencePredicate::GuiQuery);++i)required[i]=true;
    }
    constexpr void set(FencePredicate p,PredicateState value,bool needed=true) noexcept {
        const auto i=static_cast<std::size_t>(p);predicates[i]=value;required[i]=needed;
    }
};
struct FenceResult {
    bool passed{true};
    FencePredicate first{FencePredicate::Count};
    std::array<FencePredicate,FenceFailureSnapshot::count> failed{};
    std::size_t failed_count{};
};
constexpr FenceResult classify_fence(const FenceFailureSnapshot& s) noexcept {
    FenceResult r;
    for(std::size_t i=0;i<s.count;++i){
        if(!s.required[i])continue;
        if(s.predicates[i]!=PredicateState::Pass){
            r.passed=false;
            if(r.first==FencePredicate::Count)r.first=static_cast<FencePredicate>(i);
            // Unknown or unevaluated facts are never fabricated as false.
            if(s.predicates[i]==PredicateState::Fail)r.failed[r.failed_count++]=static_cast<FencePredicate>(i);
        }
    }
    return r;
}
constexpr std::string_view fence_reason(FencePredicate p) noexcept {
    switch(p){
    case FencePredicate::OwnerRunning:return "owner_message_wait_failed";
    case FencePredicate::LogHealthy:return "evidence_capture_failed";
    case FencePredicate::DesktopReady:return "BLOCKED_BY_INTERACTIVE_DESKTOP";
    case FencePredicate::SourceIdentity:case FencePredicate::Foreground:return "BLOCKED_BY_FOREGROUND";
    case FencePredicate::NoForeignCaptureTransfer:case FencePredicate::CancelledGuiSafe:return "BLOCKED_BY_FOREIGN_INPUT_CAPTURE";
    case FencePredicate::GuiQuery:return "BLOCKED_BY_GUI_STATE";
    case FencePredicate::InputRootOwned:return "BLOCKED_BY_INPUT_HIT_AUTHORITY";
    case FencePredicate::Count:return "none";
    default:return "BLOCKED_BY_INPUT_INTERFERENCE";
    }
}
constexpr std::string_view fence_subreason(FencePredicate p) noexcept {
    switch(p){
    case FencePredicate::CursorQuery:return "CURSOR_QUERY_FAILED";
    case FencePredicate::CursorPosition:return "CURSOR_DEVIATION";
    case FencePredicate::LeftState:return "LEFT_STATE_MISMATCH";
    case FencePredicate::OtherInputClear:return "OTHER_INPUT_ACTIVE";
    case FencePredicate::Capture:return "CAPTURE_MISMATCH";
    default:return predicate_name(p);
    }
}
struct PendingDownLedger {
    std::uint64_t down_input_sequence{},raw_down_receiver_sequence{},native_down_sequence{},native_enter_sequence{};
    std::int64_t down_start_qpc{},down_return_qpc{},raw_down_receiver_qpc{},native_down_qpc{},native_enter_qpc{};
    std::uint32_t receiver_watermark{};
    bool down_sent{},raw_down_matches{},native_down_matches{},native_enter_matches{},matching_up_seen{};
    std::uint64_t non_test_button_transitions{},unmatched_button_transitions{};
    bool up_command_sent{};
    constexpr bool pending_owned() const noexcept {
        return down_sent&&down_input_sequence&&down_start_qpc>0&&down_return_qpc>=down_start_qpc&&
            raw_down_matches&&raw_down_receiver_sequence>receiver_watermark&&raw_down_receiver_qpc>=down_start_qpc&&
            native_down_matches&&native_down_sequence&&native_down_qpc>=down_start_qpc&&
            native_enter_matches&&native_enter_sequence&&native_enter_qpc>=native_down_qpc&&
            !matching_up_seen&&!up_command_sent&&!non_test_button_transitions&&!unmatched_button_transitions;
    }
};
struct AbortAuthority {
    PendingDownLedger ledger;
    bool fixture_scope_active{},own_identity{},desktop_ready{},source_visible{},foreground_matches{};
    bool gui_query_succeeded{},capture_is_source{},move_size_is_source{},expected_native_mode{},menu_clear{};
    bool cursor_available{},root_is_source{},left_down{},other_input_clear{},receiver_healthy{},log_healthy{};
    bool foreign_capture_transferred{},writer_quiescent{},acceptance_retired{},input_mapping_supported{},api_boundary_stable{};
    constexpr bool eligible() const noexcept {
        // Planned cursor is deliberately absent: movement deviation alone does
        // not authorize continuation and does not destroy exact DOWN ownership.
        return ledger.pending_owned()&&fixture_scope_active&&own_identity&&desktop_ready&&source_visible&&
            foreground_matches&&gui_query_succeeded&&capture_is_source&&move_size_is_source&&expected_native_mode&&menu_clear&&
            cursor_available&&root_is_source&&left_down&&other_input_clear&&receiver_healthy&&log_healthy&&
            !foreign_capture_transferred&&writer_quiescent&&acceptance_retired&&input_mapping_supported&&api_boundary_stable;
    }
};
struct AbortCompletion {
    bool attempted{},sent{},raw_up_matches{},native_exit_observed{},winevent_end_matches{};
    bool final_context_reliable{},final_capture_clear{},final_mode_clear{},final_left_up{};
    bool ledger_settled{},writer_quiescent{},acceptance_retired{},no_pending_work{},cleanup_scope{};
    constexpr std::string_view result() const noexcept {
        if(!attempted)return "SKIPPED_NO_AUTHORITY";
        if(!sent)return "FAILED";
        return raw_up_matches&&native_exit_observed&&winevent_end_matches&&final_context_reliable&&
            final_capture_clear&&final_mode_clear&&final_left_up&&ledger_settled&&writer_quiescent&&
            acceptance_retired&&no_pending_work&&cleanup_scope?"PASS":"UNCONFIRMED";
    }
};
} // namespace panebind::test::abort
