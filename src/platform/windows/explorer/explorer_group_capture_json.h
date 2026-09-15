#pragma once
#include "platform/windows/explorer/explorer_group_capture.h"
#include <sstream>
namespace panebind::platform::windows::explorer::detail {
// Only fixed tokens and numbers. ExplorerDiagnostic api/detail/paths never
// enter this representation. Sanitize even injected test-boundary labels.
inline std::string_view capture_safe_label(std::string_view value) noexcept {
    if(value.empty()||value.size()>80)return "redacted_non_label";
    for(const char c:value)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return "redacted_non_label";
    return value;
}
inline std::string group_capture_json(const GroupCaptureResult& c) {
    std::ostringstream s;s<<std::boolalpha;
    const auto maybe=[&](std::optional<bool> b){if(b)s<<*b;else s<<"null";};
    s<<"{\"succeeded\":"<<c.succeeded()<<",\"recoverable\":"<<c.recoverable()
     <<",\"fatal\":"<<(c.disposition==GroupCaptureDisposition::Fatal)<<",\"failed_member_index\":";
    if(c.failed_member_index)s<<*c.failed_member_index;else s<<"null";
    s<<",\"failure_stage\":\""<<group_capture_stage_name(c.failure_stage)<<"\",\"eligibility_reason\":";
    if(c.eligibility_reason)s<<'"'<<group_capture_eligibility_name(*c.eligibility_reason)<<'"';else s<<"null";
    s<<",\"eligibility_code\":";if(c.eligibility_reason)s<<static_cast<int>(*c.eligibility_reason);else s<<"null";
    s<<",\"diagnostic\":";
    if(c.diagnostic)s<<"{\"domain\":"<<static_cast<int>(c.diagnostic->domain)<<",\"code\":"<<c.diagnostic->code<<'}';else s<<"null";
    s<<",\"glue_validation_invalidation\":\""<<capture_safe_label(c.glue_validation_invalidation)
     <<"\",\"reason\":\""<<capture_safe_label(c.reason)<<"\",\"members\":[";
    for(std::size_t i=0;i<3;++i){if(i)s<<',';const auto& o=c.observations[i];
        s<<"{\"member\":"<<i<<",\"browser_observed\":"<<o.browser_observed<<",\"canonical_identity_matches\":";maybe(o.canonical_identity_matches);
        s<<",\"anchor_hwnd_matches\":";maybe(o.anchor_hwnd_matches);s<<",\"location_exact\":";maybe(o.location_exact);
        s<<",\"navigation_epoch\":";if(o.navigation_epoch)s<<*o.navigation_epoch;else s<<"null";
        s<<",\"browser_stream_reason\":\""<<capture_safe_label(o.browser_stream_reason)<<"\",\"browser\":";
        if(!o.browser_observed)s<<"null";
        else {const auto& b=o.browser;
            s<<"{\"callback_sequence\":"<<b.callback_sequence<<",\"latest_sequence\":"<<b.latest_sequence
             <<",\"navigate_complete_count\":"<<b.navigate_complete_count<<",\"matching_navigate_complete_count\":"<<b.matching_navigate_complete_count
             <<",\"unrelated_navigate_complete_count\":"<<b.unrelated_navigate_complete_count<<",\"identity_query_failure_count\":"<<b.identity_query_failure_count
             <<",\"quit_count\":"<<b.quit_count<<",\"malformed_count\":"<<b.malformed_count<<",\"overflow_count\":"<<b.overflow_count
             <<",\"wrong_thread_count\":"<<b.wrong_thread_count<<",\"post_retirement_count\":"<<b.post_retirement_count
             <<",\"geometry_event_count\":"<<b.geometry_event_count<<",\"last_geometry_dispid\":"<<b.last_geometry_dispid
             <<",\"last_malformed_dispid\":"<<b.last_malformed_dispid<<",\"latest_activity_was_quit\":"<<b.latest_activity_was_quit
             <<",\"accepting\":"<<b.accepting<<",\"subscribed\":"<<b.subscribed<<",\"unadvised\":"<<b.unadvised
             <<",\"subscription_diagnostic\":"<<b.subscription_diagnostic<<'}';
        }s<<'}';
    }s<<"]}";return s.str();
}
}
