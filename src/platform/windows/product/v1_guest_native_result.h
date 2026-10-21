#pragma once

#include <string_view>

namespace panebind::platform::windows::product::guest {

// TEST-only result composition, not window authority or a native observation.
// In particular, no bound root means close was NOT_RUN, never close PASS.
enum class NativeClientVerdict { Pass, Fail, Blocked, NotRun };
struct NativeClientCompletionFacts {
    bool root_bound{};
    bool client_attempted{};
    bool client_passed{};
    bool source_idle{};
    bool normal_close_confirmed{};
    bool cleanup_error_free{};
    bool input_clear{};
};
struct NativeClientCompletion {
    NativeClientVerdict client{NativeClientVerdict::NotRun};
    NativeClientVerdict normal_close{NativeClientVerdict::NotRun};
    NativeClientVerdict overall{NativeClientVerdict::Fail};
};
constexpr NativeClientCompletion native_client_completion(NativeClientCompletionFacts facts){
    const auto client=!facts.root_bound?NativeClientVerdict::Blocked:!facts.client_attempted?NativeClientVerdict::NotRun:
        facts.client_passed?NativeClientVerdict::Pass:NativeClientVerdict::Fail;
    const auto close=!facts.root_bound?NativeClientVerdict::NotRun:
        facts.normal_close_confirmed?NativeClientVerdict::Pass:NativeClientVerdict::Fail;
    const bool clean=facts.source_idle&&facts.cleanup_error_free&&facts.input_clear;
    const auto overall=client==NativeClientVerdict::Pass&&close==NativeClientVerdict::Pass&&clean?
        NativeClientVerdict::Pass:client==NativeClientVerdict::Blocked&&clean?
        NativeClientVerdict::Blocked:NativeClientVerdict::Fail;
    return {client,close,overall};
}
constexpr std::string_view native_client_verdict_name(NativeClientVerdict value){
    switch(value){
    case NativeClientVerdict::Pass:return "PASS";
    case NativeClientVerdict::Fail:return "FAIL";
    case NativeClientVerdict::Blocked:return "BLOCKED";
    case NativeClientVerdict::NotRun:return "NOT_RUN";
    }
    return "UNKNOWN";
}

} // namespace panebind::platform::windows::product::guest
