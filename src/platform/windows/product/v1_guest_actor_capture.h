#pragma once

#include "platform/windows/product/v1_guest_up_evidence.h"
#include <algorithm>
#include <atomic>
#include <string>

namespace panebind::platform::windows::product::guest {

// Two immutable slots in the actor's existing one-shot parent/child mapping.
// The event announces phase, not product START or capture authority. No
// threshold command is accepted without fresh native child-side checks.
enum class ActorCapturePhase : std::uint64_t { Empty,NativeThreshold,ActualStartReady };
struct ActorNativeThresholdCommand {
    std::uint64_t version{},parent_pid{},parent_birth{},child_pid{},child_birth{};
    std::uint64_t product_pid{},product_birth{},source{},source_pid{},source_tid{},generation{},source_key{},down_qpc{};
    std::int64_t anchor_x{},anchor_y{},destination_x{},destination_y{},drag_x{},drag_y{};
    std::array<std::int64_t,4> original_visible{};
    std::array<char,65> product_image_sha256{};
    bool no_up_start_or_end_observed{};
};
struct ActorCaptureHandshake {
    std::atomic<ActorCapturePhase> phase{ActorCapturePhase::Empty};
    ActorNativeThresholdCommand threshold;
    ActorStartCaptureReceipt ready;
};
static_assert(std::atomic<ActorCapturePhase>::is_always_lock_free);
inline bool actor_native_threshold_command_matches(const ActorNativeThresholdCommand& command,
    std::uint64_t parent_pid,std::uint64_t parent_birth,std::uint64_t child_pid,std::uint64_t child_birth,
    std::uint64_t source,std::uint64_t source_tid,std::int64_t anchor_x,std::int64_t anchor_y,std::uint64_t own_down_qpc){
    const auto coordinate=[](std::int64_t value){return value>=std::numeric_limits<std::int32_t>::min()&&
        value<=std::numeric_limits<std::int32_t>::max();};
    if(!coordinate(command.anchor_x)||!coordinate(command.anchor_y)||!coordinate(command.destination_x)||!coordinate(command.destination_y)||
        !std::all_of(command.original_visible.begin(),command.original_visible.end(),coordinate))return false;
    bool hash_known=command.product_image_sha256[64]=='\0';
    for(std::size_t n=0;n<64;++n){const char c=command.product_image_sha256[n];
        hash_known=hash_known&&((c>='0'&&c<='9')||(c>='A'&&c<='F'));}
    const auto displacement=command.destination_x-command.anchor_x;
    return command.version==1&&parent_pid&&parent_birth&&child_pid&&child_birth&&source&&source_tid&&own_down_qpc&&
        command.parent_pid==parent_pid&&command.parent_birth==parent_birth&&command.child_pid==child_pid&&command.child_birth==child_birth&&
        command.product_pid&&command.product_pid!=parent_pid&&command.product_pid!=child_pid&&command.product_birth&&
        command.source==source&&command.source_pid==parent_pid&&command.source_tid==source_tid&&command.generation&&command.source_key&&
        command.down_qpc>=own_down_qpc&&command.anchor_x==anchor_x&&command.anchor_y==anchor_y&&
        command.drag_x>=0&&command.drag_x<=128&&command.drag_y>=0&&command.drag_y<=128&&
        (displacement==command.drag_x+1||displacement==-(command.drag_x+1))&&command.destination_y==anchor_y&&
        command.original_visible[0]<command.original_visible[2]&&command.original_visible[1]<command.original_visible[3]&&
        command.no_up_start_or_end_observed&&hash_known;
}
// Actual END may leave a translated B. Only the product's same-generation
// records may bind that B; observed window geometry is never a new anchor.
inline bool actor_native_handoff_geometry_matches(std::string_view start,std::string_view handoff,
    std::uint64_t product_pid,std::string_view source_sha,std::uint64_t generation,
    const std::array<std::int64_t,4>& original,const std::array<std::int64_t,4>& actual){
    const auto bounded=[](const auto& rectangle){return std::all_of(rectangle.begin(),rectangle.end(),[](auto coordinate){return
        coordinate>=std::numeric_limits<std::int32_t>::min()&&coordinate<=std::numeric_limits<std::int32_t>::max();})&&
        rectangle[0]<rectangle[2]&&rectangle[1]<rectangle[3];};
    if(!bounded(original)||!bounded(actual)||original[2]-original[0]!=actual[2]-actual[0]||original[3]-original[1]!=actual[3]-actual[1]||
        !product_pid||source_sha.size()!=40||!generation)return false;
    const auto identity=[&](std::string_view row){return up_evidence_detail::number(row,"\"process_id\":")==product_pid&&
        up_evidence_detail::number(row,"\"generation\":")==generation&&up_evidence_detail::quoted(row,"\"source_sha\":",source_sha);};
    const auto field=[](std::string_view row,std::string_view key,const auto& rectangle){
        const auto at=row.find(key);if(at==row.npos||row.find(key,at+key.size())!=row.npos)return false;
        const std::string expected=std::string(key)+"["+std::to_string(rectangle[0])+","+std::to_string(rectangle[1])+","+
            std::to_string(rectangle[2])+","+std::to_string(rectangle[3])+"]";
        const auto end=at+expected.size();return row.substr(at,expected.size())==expected&&end<row.size()&&(row[end]==','||row[end]=='}');};
    return identity(start)&&identity(handoff)&&up_evidence_detail::quoted(start,"\"type\":","gesture_start")&&
        up_evidence_detail::quoted(handoff,"\"type\":","native_move_handoff")&&
        up_evidence_detail::unique_value(start,"\"native_first_handoff\":")=="true"&&
        up_evidence_detail::unique_value(handoff,"\"accepted\":")=="true"&&
        up_evidence_detail::quoted(handoff,"\"stage\":","actual_END_capture_pending")&&
        field(start,"\"original\":",original)&&field(handoff,"\"intent_anchor\":",original)&&
        field(start,"\"actual_begin_geometry\":",actual)&&field(handoff,"\"continuity_baseline\":",actual);
}

} // namespace panebind::platform::windows::product::guest
