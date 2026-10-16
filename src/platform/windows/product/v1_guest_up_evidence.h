#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace panebind::platform::windows::product::guest {

// A fixture contract, not a product input source. Ordinary portable launches
// intentionally do not emit diagnostic input_button_observed records.
enum class UpEvidenceMode { DiagnosticEvent, StatusSnapshot };
struct UpEvidenceIdentity {
    std::uint64_t process_id{};
    std::string_view source_sha;
    std::uint64_t generation{}, send_before_qpc{};
};
namespace up_evidence_detail {
// Read only the product's bounded, canonical status serialization. Ambiguous
// fields and changed/missing snapshot structure fail closed; this is not a
// general JSON reader or a fallback to a fixture hook/Raw observation.
inline std::optional<std::string_view> unique_value(std::string_view row,std::string_view key){
    const auto at=row.find(key);
    if(at==row.npos||row.find(key,at+key.size())!=row.npos)return {};
    const auto begin=at+key.size(),end=row.find_first_of(",}",begin);
    if(end==row.npos)return {};
    return row.substr(begin,end-begin);
}
inline bool quoted(std::string_view row,std::string_view key,std::string_view expected){
    const auto value=unique_value(row,key);
    return value&&value->size()==expected.size()+2&&value->front()=='"'&&value->back()=='"'&&
        value->substr(1,value->size()-2)==expected;
}
struct Cursor {
    std::string_view text;
    bool take(std::string_view expected){
        if(!text.starts_with(expected))return false;
        text.remove_prefix(expected.size());return true;
    }
    bool number(std::uint64_t& value){
        value=0;std::size_t size{};
        while(size<text.size()&&text[size]>='0'&&text[size]<='9'){
            const auto digit=static_cast<unsigned>(text[size]-'0');
            if(value>(std::numeric_limits<std::uint64_t>::max()-digit)/10)return false;
            value=value*10+digit;++size;
        }
        if(!size||(size>1&&text.front()=='0'))return false;
        text.remove_prefix(size);return true;
    }
};
}
inline bool matching_status_up_snapshot(std::string_view row,const UpEvidenceIdentity& expected){
    using namespace up_evidence_detail;
    if(row.empty()||row.size()>16384||row.front()!='{'||row.back()!='}'||
       row.find_first_of("\r\n")!=row.npos||!expected.process_id||expected.source_sha.size()!=40||
       !expected.generation||!expected.send_before_qpc||
       !quoted(row,"\"schema\":","panebind-v1/1")||!quoted(row,"\"type\":","status")||
       !quoted(row,"\"source_sha\":",expected.source_sha))return false;
    const auto pid=unique_value(row,"\"process_id\":");if(!pid)return false;
    Cursor process{*pid};std::uint64_t actual_pid{};
    if(!process.number(actual_pid)||!process.text.empty()||actual_pid!=expected.process_id)return false;
    constexpr std::string_view key="\"mouse_up_callbacks\":";const auto at=row.find(key);
    if(at==row.npos||row.find(key,at+key.size())!=row.npos)return false;
    Cursor snapshot{row.substr(at+key.size())};std::uint64_t count{},qpc{},generation{};
    return snapshot.take("{\"snapshot_known\":true,\"callback_count\":")&&snapshot.number(count)&&count>0&&
        snapshot.take(",\"last\":{\"hook_observed_qpc\":")&&snapshot.number(qpc)&&qpc>0&&qpc>=expected.send_before_qpc&&
        snapshot.take(",\"button\":1,\"generation_at_entry\":")&&snapshot.number(generation)&&generation==expected.generation&&
        snapshot.take(",\"consumed_before\":true,\"aborted_before\":false,\"matching_branch_taken\":true,\"push_attempted\":true,\"push_succeeded\":true}}}")&&
        snapshot.text.empty();
}
inline bool accepted_up_evidence(UpEvidenceMode mode,bool matching_diagnostic_event,
    std::string_view status,const UpEvidenceIdentity& expected){
    if(mode==UpEvidenceMode::DiagnosticEvent)return matching_diagnostic_event;
    if(mode==UpEvidenceMode::StatusSnapshot)return matching_status_up_snapshot(status,expected);
    return false;
}
}
