#pragma once

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace panebind::platform::windows::product::guest {
// One bounded TEST command, not an extensible control protocol or authority.
// Both sender and receiver independently require the exact guest proof.
struct GuestQueueFailureRequest {std::string run;std::uint64_t generation{};std::uintptr_t source{};};
inline std::optional<GuestQueueFailureRequest> parse_guest_queue_failure(std::string_view text){
    constexpr std::string_view prefix="queue-failure ";
    if(!text.starts_with(prefix)||text.size()>126)return {};
    text.remove_prefix(prefix.size());const auto first=text.find(' ');
    if(first!=32)return {};const auto run=text.substr(0,first);
    if(!std::all_of(run.begin(),run.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))return {};
    text.remove_prefix(first+1);const auto second=text.find(' ');
    if(second==text.npos||!second)return {};const auto generation=text.substr(0,second),source=text.substr(second+1);
    GuestQueueFailureRequest request{std::string(run)};
    const auto g=std::from_chars(generation.data(),generation.data()+generation.size(),request.generation);
    const auto h=std::from_chars(source.data(),source.data()+source.size(),request.source);
    if(g.ec!=std::errc{}||g.ptr!=generation.data()+generation.size()||!request.generation||
       h.ec!=std::errc{}||h.ptr!=source.data()+source.size()||!request.source)return {};
    return request;
}
} // namespace panebind::platform::windows::product::guest
