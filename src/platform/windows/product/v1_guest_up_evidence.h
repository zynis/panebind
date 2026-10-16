#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
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
// TEST-only admission for delivering the single planned UP to an actual
// current product shield. No HWND/class-name or global capture exemption.
struct CapturedReleaseFacts {
    std::uint64_t generation{},expected_generation{},source{},ready_source{},source_pid{},ready_source_pid{},source_tid{},ready_source_tid{};
    std::uint64_t overlay{},ready_actual_capture{},hit_root{},overlay_pid{},product_pid{},overlay_tid{},source_capture{},owner_capture{};
    bool product_identity{},ready_identity{},ready_attempt_succeeded{},source_gui{},owner_gui{},gui_idle_except_capture{},not_retired{};
};
inline bool captured_release_context_allowed(const CapturedReleaseFacts& f){
    return f.generation&&(!f.expected_generation||f.expected_generation==f.generation)&&
        f.source&&f.source==f.ready_source&&f.source_pid&&f.source_pid==f.ready_source_pid&&
        f.source_tid&&f.source_tid==f.ready_source_tid&&f.overlay&&f.overlay==f.ready_actual_capture&&
        f.product_pid&&f.overlay_pid==f.product_pid&&f.overlay_tid&&
        f.source_capture==f.overlay&&f.owner_capture==f.overlay&&f.product_identity&&f.ready_identity&&
        f.ready_attempt_succeeded&&f.source_gui&&f.owner_gui&&f.gui_idle_except_capture&&f.not_retired;
}
inline bool captured_release_hit_allowed(const CapturedReleaseFacts& f){
    return captured_release_context_allowed(f)&&f.hit_root==f.overlay;
}
// WindowFromPoint is a spatial hit, not the capture delivery target. A source
// promoted to topmost during the gesture may cover this exact shield while
// both associated queues still report this generation's own capture.
inline bool captured_release_route_allowed(const CapturedReleaseFacts& f){
    return captured_release_context_allowed(f)&&(f.hit_root==f.overlay||f.hit_root==f.source);
}
// One immutable parent-to-child START acknowledgement, not a product permit.
// The child must independently recheck actual process/window/GUI facts before
// issuing its one TEST MOVE. No filenames or unknown pointers cross here.
struct ActorStartCaptureReceipt {
    std::uint64_t version{},parent_pid{},parent_birth{},child_pid{},child_birth{},product_birth{},down_qpc{},ready_qpc{};
    CapturedReleaseFacts capture;
    std::array<char,65> product_image_sha256{};
};
inline bool actor_start_capture_receipt_matches(const ActorStartCaptureReceipt& receipt,
    std::uint64_t parent_pid,std::uint64_t parent_birth,std::uint64_t child_pid,std::uint64_t child_birth){
    bool hash_known=receipt.product_image_sha256[64]=='\0';
    for(std::size_t i=0;i<64;++i){const char c=receipt.product_image_sha256[i];
        hash_known=hash_known&&((c>='0'&&c<='9')||(c>='A'&&c<='F'));}
    return receipt.version==1&&parent_pid&&parent_birth&&child_pid&&child_birth&&
        receipt.parent_pid==parent_pid&&receipt.parent_birth==parent_birth&&
        receipt.child_pid==child_pid&&receipt.child_birth==child_birth&&receipt.product_birth&&
        receipt.down_qpc&&receipt.ready_qpc>=receipt.down_qpc&&hash_known&&
        receipt.capture.expected_generation==receipt.capture.generation&&captured_release_route_allowed(receipt.capture);
}
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
    bool boolean(bool& value){
        if(take("true")){value=true;return true;}
        if(take("false")){value=false;return true;}return false;
    }
};
inline std::optional<std::uint64_t> number(std::string_view row,std::string_view key){
    const auto value=unique_value(row,key);if(!value)return {};Cursor cursor{*value};std::uint64_t out{};
    return cursor.number(out)&&cursor.text.empty()?std::optional<std::uint64_t>(out):std::nullopt;
}
inline bool receipt(std::string_view row,const UpEvidenceIdentity& expected,std::string_view type){
    const auto pid=number(row,"\"process_id\":"),generation=number(row,"\"generation\":");
    return row.size()<=16384&&!row.empty()&&row.front()=='{'&&row.back()=='}'&&row.find_first_of("\r\n")==row.npos&&
        expected.process_id&&expected.generation&&expected.send_before_qpc&&expected.source_sha.size()==40&&
        quoted(row,"\"schema\":","panebind-v1/1")&&quoted(row,"\"type\":",type)&&
        quoted(row,"\"source_sha\":",expected.source_sha)&&pid&&*pid==expected.process_id&&generation&&*generation==expected.generation;
}
}
inline bool matching_clean_captured_up(std::string_view legacy,std::string_view gone,const UpEvidenceIdentity& expected){
    using namespace up_evidence_detail;
    const auto qpc=number(legacy,"\"legacy_up_dispatch_qpc\":"),overlay=number(legacy,"\"overlay\":");
    return receipt(legacy,expected,"gesture_input_resource")&&receipt(gone,expected,"gesture_input_resource")&&
        quoted(legacy,"\"fact\":","legacy_left_up")&&unique_value(legacy,"\"actual_legacy_up_accepted\":")=="true"&&
        qpc&&*qpc>=expected.send_before_qpc&&overlay&&*overlay&&number(gone,"\"overlay\":")==overlay&&
        quoted(gone,"\"fact\":","isolation_gone")&&number(gone,"\"removal_reason\":")==0&&
        unique_value(gone,"\"overlay_destroyed\":")=="true"&&unique_value(gone,"\"release_attempted\":")=="true"&&
        unique_value(gone,"\"release_completed\":")=="true"&&unique_value(gone,"\"release_succeeded\":")=="true"&&
        number(gone,"\"capture_after_release\":")==0&&unique_value(gone,"\"detach_attempted\":")=="true"&&
        unique_value(gone,"\"detach_completed\":")=="true"&&unique_value(gone,"\"detach_succeeded\":")=="true";
}
inline bool matching_idle_status(std::string_view row,const UpEvidenceIdentity& expected){
    using namespace up_evidence_detail;
    return row.size()<=16384&&!row.empty()&&row.front()=='{'&&row.back()=='}'&&row.find_first_of("\r\n")==row.npos&&
        quoted(row,"\"schema\":","panebind-v1/1")&&quoted(row,"\"type\":","status")&&
        quoted(row,"\"source_sha\":",expected.source_sha)&&number(row,"\"process_id\":")==expected.process_id&&
        unique_value(row,"\"hook_owner_running\":")=="true"&&unique_value(row,"\"gesture_active\":")=="false"&&
        unique_value(row,"\"native_operation_pending\":")=="false"&&unique_value(row,"\"native_completion_unknown\":")=="false"&&
        unique_value(row,"\"visual_restore_pending\":")=="false";
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
// TEST first-click evidence only: an actual UP may finish before resource
// capture is ready. This narrow pre-capture physical-button rejection is not
// a NormalUp receipt or permission to continue a failed active gesture.
inline bool matching_pre_capture_click_release(std::string_view status,std::string_view failure,
    std::string_view gone,const UpEvidenceIdentity& expected){
    using namespace up_evidence_detail;
    if(!matching_idle_status(status,expected)||unique_value(status,"\"enabled\":")!="true"||
       !receipt(failure,expected,"gesture_input_resource")||!quoted(failure,"\"fact\":","resource_failure")||
       !receipt(gone,expected,"gesture_input_resource")||!quoted(gone,"\"fact\":","isolation_gone")||
       number(failure,"\"native_failure\":")!=4||number(gone,"\"removal_reason\":")!=3||
       unique_value(gone,"\"overlay_destroyed\":")!="true"||unique_value(gone,"\"hotkey_unregistered\":")!="true")return false;
    const auto stage=number(failure,"\"setup_stage\":");if(stage!=2&&stage!=4&&stage!=8)return false;
    for(const auto key:{"\"source\":","\"source_pid\":","\"source_tid\":","\"overlay\":"}){
        const auto value=number(failure,key);if(!value||!*value||number(gone,key)!=value)return false;
    }
    for(const auto row:{failure,gone}){
        for(const auto key:{"\"capture_attempted\":","\"attach_attempted\":","\"release_attempted\":","\"release_completed\":",
            "\"release_succeeded\":","\"detach_attempted\":","\"detach_completed\":","\"detach_succeeded\":"})
            if(unique_value(row,key)!="false")return false;
        if(number(row,"\"capture_actual_after\":")!=0||number(row,"\"capture_after_release\":")!=0)return false;
    }
    constexpr std::string_view key="\"mouse_up_callbacks\":";const auto at=status.find(key);
    if(at==status.npos||status.find(key,at+key.size())!=status.npos)return false;
    Cursor snapshot{status.substr(at+key.size())};std::uint64_t count{},qpc{},generation{};
    return snapshot.take("{\"snapshot_known\":true,\"callback_count\":")&&snapshot.number(count)&&count>0&&
        snapshot.take(",\"last\":{\"hook_observed_qpc\":")&&snapshot.number(qpc)&&qpc>0&&qpc>=expected.send_before_qpc&&
        snapshot.take(",\"button\":1,\"generation_at_entry\":")&&snapshot.number(generation)&&generation==expected.generation&&
        snapshot.take(",\"consumed_before\":true,\"aborted_before\":false,\"matching_branch_taken\":true,\"push_attempted\":false,\"push_succeeded\":false}}}")&&snapshot.text.empty();
}
// ABORT cleanup evidence only. A hook receipt after revocation is not a
// normalized gesture UP, a NormalUp permit, target delivery or idle proof.
// In-flight native completion and geometry must be verified separately.
inline bool matching_abort_cleanup_up(std::string_view status,std::string_view ended,
    std::string_view gone,const UpEvidenceIdentity& expected,std::string_view released={}){
    using namespace up_evidence_detail;
    const bool own_release=unique_value(gone,"\"release_attempted\":")=="true"&&
        unique_value(gone,"\"release_completed\":")=="true"&&unique_value(gone,"\"release_succeeded\":")=="true"&&
        number(gone,"\"capture_after_release\":")==0;
    const auto overlay=number(gone,"\"overlay\":"),owner=number(released,"\"capture_owner_tid\":"),before=number(released,"\"capture_release_before_HWND\":");
    // A typed owner-thread observation can prove abort cleanup without an API
    // release: never infer that observation from default NULL fields in Gone.
    const bool no_own_capture=receipt(released,expected,"gesture_input_resource")&&quoted(released,"\"fact\":","capture_released")&&
        overlay&&*overlay&&number(released,"\"overlay\":")==overlay&&owner&&*owner&&number(gone,"\"capture_owner_tid\":")==owner&&
        before&&*before!=*overlay&&number(released,"\"capture_after_release\":")==before&&
        number(gone,"\"capture_release_before_HWND\":")==before&&number(gone,"\"capture_after_release\":")==before&&
        unique_value(released,"\"capture_owner_observation_known\":")=="true"&&
        unique_value(released,"\"capture_release_after_is_copied_no_call\":")=="true"&&
        number(released,"\"source\":")&&number(released,"\"source\":")!=0&&number(released,"\"source\":")==number(gone,"\"source\":")&&
        number(released,"\"source_pid\":")&&number(released,"\"source_pid\":")!=0&&number(released,"\"source_pid\":")==number(gone,"\"source_pid\":")&&
        number(released,"\"source_tid\":")&&number(released,"\"source_tid\":")!=0&&number(released,"\"source_tid\":")==number(gone,"\"source_tid\":")&&
        unique_value(gone,"\"hotkey_unregistered\":")=="true"&&
        unique_value(released,"\"release_attempted\":")=="false"&&unique_value(released,"\"release_completed\":")=="false"&&
        unique_value(released,"\"release_succeeded\":")=="false"&&unique_value(gone,"\"release_attempted\":")=="false"&&
        unique_value(gone,"\"release_completed\":")=="false"&&unique_value(gone,"\"release_succeeded\":")=="false";
    if(!receipt(ended,expected,"gesture_end")||!receipt(gone,expected,"gesture_input_resource")||
       !quoted(gone,"\"fact\":","isolation_gone")||unique_value(ended,"\"normal_up\":")!="false"||
       unique_value(ended,"\"pending_retired\":")!="true"||!number(ended,"\"end\":")||number(ended,"\"end\":")==0||
       !number(gone,"\"overlay\":")||number(gone,"\"overlay\":")==0||
       !number(gone,"\"removal_reason\":")||number(gone,"\"removal_reason\":")==0||number(gone,"\"removal_reason\":")>5||
       unique_value(gone,"\"overlay_destroyed\":")!="true"||(!own_release&&!no_own_capture)||unique_value(gone,"\"detach_attempted\":")!="true"||
       unique_value(gone,"\"detach_completed\":")!="true"||unique_value(gone,"\"detach_succeeded\":")!="true"||
       status.empty()||status.size()>16384||status.front()!='{'||status.back()!='}'||status.find_first_of("\r\n")!=status.npos||
       !quoted(status,"\"schema\":","panebind-v1/1")||!quoted(status,"\"type\":","status")||
       !quoted(status,"\"source_sha\":",expected.source_sha)||number(status,"\"process_id\":")!=expected.process_id)return false;
    constexpr std::string_view key="\"mouse_up_callbacks\":";const auto at=status.find(key);
    if(at==status.npos||status.find(key,at+key.size())!=status.npos)return false;
    Cursor snapshot{status.substr(at+key.size())};std::uint64_t count{},qpc{},generation{};
    bool consumed{},aborted{},matching{},attempted{},succeeded{};
    const bool observed=snapshot.take("{\"snapshot_known\":true,\"callback_count\":")&&snapshot.number(count)&&count>0&&
        snapshot.take(",\"last\":{\"hook_observed_qpc\":")&&snapshot.number(qpc)&&qpc>0&&qpc>=expected.send_before_qpc&&
        snapshot.take(",\"button\":1,\"generation_at_entry\":")&&snapshot.number(generation)&&generation==expected.generation&&
        snapshot.take(",\"consumed_before\":")&&snapshot.boolean(consumed)&&snapshot.take(",\"aborted_before\":")&&snapshot.boolean(aborted)&&
        snapshot.take(",\"matching_branch_taken\":")&&snapshot.boolean(matching)&&snapshot.take(",\"push_attempted\":")&&snapshot.boolean(attempted)&&
        snapshot.take(",\"push_succeeded\":")&&snapshot.boolean(succeeded)&&snapshot.take("}}}")&&snapshot.text.empty();
    const bool retired_pair=!consumed&&aborted&&!matching&&!attempted&&!succeeded;
    const bool captured_revoke=consumed&&matching&&!attempted&&!succeeded;
    const bool normalized_abort=consumed&&aborted&&matching&&attempted&&succeeded;
    return observed&&(retired_pair||captured_revoke||normalized_abort);
}
inline bool accepted_up_evidence(UpEvidenceMode mode,bool matching_diagnostic_event,
    std::string_view status,const UpEvidenceIdentity& expected,std::string_view legacy={},std::string_view gone={}){
    if(mode==UpEvidenceMode::DiagnosticEvent)return matching_diagnostic_event;
    if(mode==UpEvidenceMode::StatusSnapshot)return matching_status_up_snapshot(status,expected)||
        (matching_idle_status(status,expected)&&matching_clean_captured_up(legacy,gone,expected));
    return false;
}
}
