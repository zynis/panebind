#include "platform/windows/explorer/explorer_group_session.h"
#include "core/geometry/checked_arithmetic.h"
#include "platform/windows/explorer/explorer_glue_quantum.h"
#include <algorithm>

namespace panebind::platform::windows::explorer {
namespace b=core::behavior;
namespace t=core::topology;
namespace v=core::movement;
using core::geometry::Rect;
namespace {
bool contained(const Rect& r,const Rect& area) noexcept {
    return r.left()>=area.left() && r.right()<=area.right() && r.top()>=area.top() && r.bottom()<=area.bottom();
}
bool tick_after(DWORD later,DWORD earlier) noexcept {
    const auto delta=static_cast<std::uint32_t>(later-earlier);
    return delta!=0 && delta<0x80000000U;
}
struct Registration {
    ExplorerGroupSession* session;
    const b::FollowerMoveBatchCommand* batch;
    GroupOperationRecord* record;
};
}
GroupLayoutReadiness group_layout_readiness(const detail::GroupSnapshots& s) noexcept {
    GroupLayoutReadiness result;
    try {
        for(const auto& m:s) if(m.dpi!=s[0].dpi || m.monitor_device_name!=s[0].monitor_device_name ||
            m.monitor_work_area!=s[0].monitor_work_area || !m.dpi) {
            result.reason="monitor_or_dpi_mismatch";return result;
        }
        const auto area=s[0].monitor_work_area;
        std::array<std::int64_t,3> widths{},heights{};
        for(std::size_t i=0;i<3;++i) {
            widths[i]=v::detail::checked_extent(s[i].visible_rect.right(),s[i].visible_rect.left());
            heights[i]=v::detail::checked_extent(s[i].visible_rect.bottom(),s[i].visible_rect.top());
            if(widths[i]<=0 || heights[i]<=0)return result;
        }
        // B and C must meet only at a corner. Unequal dimensions may extend
        // away from that corner, but B cannot extend below A into C.
        if(heights[1]>heights[0] && widths[2]>widths[0]) {
            result.reason="B_C_overlap_for_l_shape";return result;
        }
        const auto a=v::translate_rect(s[0].visible_rect,
            {v::detail::checked_extent(area.left(),s[0].visible_rect.left()),
             v::detail::checked_extent(area.top(),s[0].visible_rect.top())});
        const auto br=v::translate_rect(s[1].visible_rect,
            {v::detail::checked_extent(a.right(),s[1].visible_rect.left()),
             v::detail::checked_extent(a.top(),s[1].visible_rect.top())});
        const auto cr=v::translate_rect(s[2].visible_rect,
            {v::detail::checked_extent(a.left(),s[2].visible_rect.left()),
             v::detail::checked_extent(a.bottom(),s[2].visible_rect.top())});
        result.targets={a,br,cr};
        const auto right=std::max({a.right(),br.right(),cr.right()});
        const auto bottom=std::max({a.bottom(),br.bottom(),cr.bottom()});
        result.width_deficit=std::max<std::int64_t>(0,v::detail::checked_extent(right,area.right()));
        result.height_deficit=std::max<std::int64_t>(0,v::detail::checked_extent(bottom,area.bottom()));
        if(!contained(a,area)||!contained(br,area)||!contained(cr,area)) {result.reason="work_area_deficit";return result;}
        const std::array<t::WindowGeometry,3> nodes{{{core::model::WindowId{"A"},a},
            {core::model::WindowId{"B"},br},{core::model::WindowId{"C"},cr}}};
        const auto graph=t::WindowAdjacencyGraph::build(nodes,{});
        if(graph.relations().size()!=2 || t::find_adjacency(nodes[1],nodes[2],{})) {
            result.reason="required_edges_mismatch";return result;
        }
        result.ready=true;result.reason="ready";
    }catch(const std::exception&){result.reason="arithmetic_overflow";}
    return result;
}
ExplorerGroupSession::ExplorerGroupSession(OwnedMembers members):owner_(GetCurrentThreadId()),members_(std::move(members)) {}
detail::GroupSessions ExplorerGroupSession::sessions() const noexcept {return {members_[0].get(),members_[1].get(),members_[2].get()};}
std::unique_ptr<ExplorerGroupSession> ExplorerGroupSession::create(OwnedMembers members) {
    auto result=std::unique_ptr<ExplorerGroupSession>(new ExplorerGroupSession(std::move(members)));
    result->desktop_=std::make_unique<ExplorerVirtualDesktopManager>();
    result->seal_=detail::ExplorerGroupBridge::bind(result->sessions(),*result->desktop_,result->original_);
    if(!result->seal_)return nullptr;
    result->current_=result->original_;
    for(const auto& m:result->seal_->members())
        result->logical_members_.push_back({core::model::WindowId{std::to_string(m.window_id)},m.capability_generation});
    result->model_=std::make_unique<b::GlueGroupMoveCoordinator>(result->logical_members_,result->seal_->generation(),3072);
    result->source_=std::unique_ptr<ExplorerGroupEventSource>(new ExplorerGroupEventSource(result->seal_->members()));
    result->operations_.reserve(1538);result->quanta_.reserve(4096);result->receipts_.reserve(16384);result->feedback_.reserve(16384);
    return result;
}
ExplorerGroupSession::~ExplorerGroupSession() {
    if(GetCurrentThreadId()!=owner_) {
        // Preserve owner-affine hooks/COM and their callback target rather than
        // releasing from a foreign apartment. Public contract is owner-only.
        static_cast<void>(source_.release());static_cast<void>(desktop_.release());
        for(auto& member:members_)static_cast<void>(member.release());
        return;
    }
    if(source_)static_cast<void>(source_->stop());
    if(seal_)detail::ExplorerGroupBridge::retire(*seal_,sessions());
}
bool ExplorerGroupSession::healthy() const noexcept {
    return GetCurrentThreadId()==owner_ && !poisoned_ && seal_ && (!source_||source_->healthy());
}
void ExplorerGroupSession::poison(std::string_view reason) noexcept {
    poisoned_=true;reason_=reason;
    if(model_)model_->abort(reason);
    if(source_)static_cast<void>(source_->stop());
    if(seal_)detail::ExplorerGroupBridge::retire(*seal_,sessions());
    active_member_.reset();
}
std::vector<t::WindowGeometry> ExplorerGroupSession::geometry(const detail::GroupSnapshots& snapshots) const {
    std::vector<t::WindowGeometry> result;
    for(std::size_t i=0;i<3;++i)result.push_back({logical_members_[i].id,snapshots[i].visible_rect});
    return result;
}
GroupLayoutReadiness ExplorerGroupSession::readiness() const {return group_layout_readiness(original_);}
bool ExplorerGroupSession::register_pending(void* raw) noexcept {
    auto& context=*static_cast<Registration*>(raw);
    auto& s=*context.session;
    if(!s.healthy() || (s.source_ && s.source_->lifecycle_pending(s.active_member_)) ||
       !detail::ExplorerGroupBridge::receipts_healthy(*s.seal_,s.sessions()))return false;
    context.record->watermark=s.source_?s.source_->watermark():0;
    context.record->pre_native_tick=GetTickCount();
    try {
        return !context.batch || s.model_->register_batch(*context.batch,context.record->watermark);
    }catch(...){return false;}
}
bool ExplorerGroupSession::apply_targets(const std::array<std::optional<Rect>,3>& targets,
    const detail::GroupSnapshots& expected,GroupOperationRecord& record,const b::FollowerMoveBatchCommand* batch) {
    if(!healthy() || operations_.size()>=1538){poison("operation_capacity");return false;}
    Registration registration{this,batch,&record};
    record.receipt=detail::ExplorerGroupBridge::apply(*seal_,sessions(),expected,targets,
        ++native_generation_,&register_pending,&registration);
    operations_.push_back(record);
    if(!record.receipt.all_postverify){poison(record.receipt.reason);return false;}
    for(std::size_t i=0;i<3;++i)current_[i]=*record.receipt.actual[i];
    return true;
}
bool ExplorerGroupSession::setup() {
    if(!healthy()||setup_done_)return false;
    const auto ready=readiness();
    if(!ready.ready){reason_=ready.reason;return false;}
    std::array<std::optional<Rect>,3> targets;
    for(std::size_t i=0;i<3;++i)targets[i]=ready.targets[i];
    GroupOperationRecord record;record.group=generation();
    if(!apply_targets(targets,original_,record,nullptr))return false;
    if(!source_->start()){poison("hook_install_failed");return false;}
    setup_done_=true;return true;
}
bool ExplorerGroupSession::attribute(const GroupEventReceipt& r,const detail::GroupSnapshots& live) {
    if(feedback_.size()>=16384){poison("feedback_capacity");return false;}
    GroupFeedbackRecord record{r.member_index,model_->gesture_generation(),0,r.sequence,"unattributed"};
    record.observed_visible=live[r.member_index].visible_rect;
    if(!tick_after(r.native_time,start_native_time_)) {
        record.result="stale_or_start_tick_ambiguous";feedback_.push_back(record);return true;
    }
    const auto& member=logical_members_[r.member_index];
    const b::GroupPendingExpectation* candidate{};
    for(const auto& p:model_->pending()) {
        if(p.member==member && p.expected==live[r.member_index].visible_rect &&
           r.sequence>p.registration_watermark && p.exact_native_result && !p.feedback_observed) {
            if(candidate){poison("ambiguous_batch_attribution");return false;}
            candidate=&p;
        }
    }
    if(candidate) {
        const auto operation=std::find_if(operations_.begin(),operations_.end(),[&](const auto& op){
            return op.gesture==candidate->gesture_generation && op.batch==candidate->batch_generation;});
        if(operation==operations_.end()){poison("missing_native_proof");return false;}
        record.batch=candidate->batch_generation;
        if(!tick_after(r.native_time,operation->pre_native_tick))record.result="pre_native_or_tick_ambiguous";
        else {
            const auto result=model_->feedback(member,generation(),candidate->gesture_generation,
                candidate->batch_generation,r.sequence,live[r.member_index].visible_rect);
            if(result!=b::GroupFeedbackResult::Acknowledged){poison("feedback_rejected");return false;}
            record.result="acknowledged";
        }
    } else {
        if(live[r.member_index].visible_rect!=current_[r.member_index].visible_rect){poison("unexpected_member_geometry");return false;}
        record.result="exact_duplicate_or_noop";
    }
    feedback_.push_back(record);return true;
}
bool ExplorerGroupSession::move_batch(const GroupEventReceipt& r,const detail::GroupSnapshots& live,std::int64_t owner_qpc) {
    auto batch=model_->plan(logical_members_[r.member_index],r.sequence,live[r.member_index].visible_rect);
    if(!batch) {
        if(model_->state()==b::GlueGroupState::GroupPoisoned){poison(model_->reason());return false;}
        return true;
    }
    std::array<std::optional<Rect>,3> targets;
    for(const auto& target:batch->followers)for(std::size_t i=0;i<3;++i)
        if(logical_members_[i].id==target.id)targets[i]=target.target_visible_rect;
    GroupOperationRecord record;
    record.group=generation();record.gesture=batch->gesture_generation;record.batch=batch->batch_generation;
    record.source_sequence=r.sequence;record.source_member=r.member_index;record.receipt_qpc=r.callback_qpc;record.owner_qpc=owner_qpc;
    record.source_visible=live[r.member_index].visible_rect;
    if(!apply_targets(targets,live,record,&*batch))return false;
    std::vector<v::PlannedTranslation> actual;
    for(const auto& target:batch->followers)for(std::size_t i=0;i<3;++i)
        if(logical_members_[i].id==target.id)actual.push_back({target.id,current_[i].visible_rect});
    if(!model_->postverify(*batch,true,actual)){poison(model_->reason());return false;}
    ++gestures_.back().batches;
    return true;
}
bool ExplorerGroupSession::quantum(std::span<const GroupEventReceipt> events) {
    if(events.empty())return true;
    if(quanta_.size()>=4096 || receipts_.size()+events.size()>16384){poison("evidence_capacity");return false;}
    const auto owner_qpc=glue_qpc_now();
    GroupQuantumRecord q{quanta_.size()+1,model_->gesture_generation(),events.front().sequence,events.back().sequence,events.size(),0,owner_qpc,0};
    receipts_.insert(receipts_.end(),events.begin(),events.end());
    const auto capture=detail::ExplorerGroupBridge::capture(*seal_,sessions());
    if(!capture){poison("member_validation_failed");return false;}
    const auto& live=*capture;
    std::optional<GroupEventReceipt> sample,end;
    for(const auto& r:events) {
        if(r.kind==GroupEventKind::Destroy){poison("member_destroyed");return false;}
        if(r.kind==GroupEventKind::Start) {
            if(active_member_ || plain_member_){poison("concurrent_member_start");return false;}
            if(!r.ctrl.available || !r.ctrl.ctrl) {plain_member_=r.member_index;continue;}
            try {
                if(v::classify_geometry_change(current_[r.member_index].visible_rect,
                    live[r.member_index].visible_rect).kind==v::GeometryChangeKind::ResizeOrMixed ||
                   v::classify_geometry_change(current_[r.member_index].positioning_rect,
                    live[r.member_index].positioning_rect).kind==v::GeometryChangeKind::ResizeOrMixed) {
                    poison("resize_or_mixed_before_start_drain");return false;
                }
            }catch(const std::exception&){poison("start_geometry_overflow");return false;}
            if(!model_->start(logical_members_[r.member_index],true,r.sequence,geometry(live),{},3)) {
                poison(model_->reason());return false;
            }
            active_member_=r.member_index;start_native_time_=r.native_time;
            current_=live;
            GroupGestureRecord g;
            g.group=generation();g.gesture=model_->gesture_generation();g.source_member=r.member_index;
            g.start_sequence=r.sequence;g.starts=1;g.callback_ctrl=r.ctrl;g.owner_ctrl=sample_ctrl();
            g.callback_qpc=r.callback_qpc;g.decision_qpc=glue_qpc_now();g.initial=live;
            gestures_.push_back(g);
            detail::ExplorerGroupBridge::active(*seal_,sessions(),true);
        } else if(plain_member_) {
            if(r.kind==GroupEventKind::End) {
                if(r.member_index!=*plain_member_){poison("unexpected_plain_end");return false;}
                plain_member_.reset();current_=live;
            }
        } else if(active_member_) {
            if(r.kind==GroupEventKind::End) {
                if(r.member_index!=*active_member_){poison("follower_end");return false;}
                end=r;++gestures_.back().ends;
            } else if(r.member_index==*active_member_) {
                ++gestures_.back().locations;
                if(r.sequence<=gestures_.back().start_sequence || !tick_after(r.native_time,start_native_time_))continue;
                if(sample)++q.coalesced;
                sample=r;
            } else if(!attribute(r,live))return false;
        }
    }
    if(active_member_) {
        // A follower changed independently even with no delivered receipt.
        for(std::size_t i=0;i<3;++i)if(i!=*active_member_ && live[i].visible_rect!=current_[i].visible_rect) {
            poison("unexpected_follower_geometry");return false;
        }
        if(end)sample=end; // exact final sample is explicitly END-sourced
        if(sample && !move_batch(*sample,live,owner_qpc))return false;
        if(end) {
            auto final=detail::ExplorerGroupBridge::capture(*seal_,sessions());
            if(!final || !model_->finish(logical_members_[*active_member_],end->sequence,geometry(*final))) {
                poison(final?model_->reason():"end_validation_failed");return false;
            }
            auto& g=gestures_.back();g.end_sequence=end->sequence;g.final=*final;
            g.exact=true;g.pending_empty=model_->pending().empty();g.roles_cleared=!model_->gesture_leader();
            g.group_ready=model_->state()==b::GlueGroupState::GroupReady;
            current_=*final;active_member_.reset();detail::ExplorerGroupBridge::active(*seal_,sessions(),false);
        }
    }
    q.gesture=model_->gesture_generation();q.end_qpc=glue_qpc_now();quanta_.push_back(q);
    return true;
}
bool ExplorerGroupSession::run_gesture(std::size_t expected_member,std::chrono::seconds timeout) {
    if(!healthy() || !setup_done_ || restored_ || expected_member>=3 || active_member_)return false;
    const auto completed=gestures_.size();
    const auto deadline=std::chrono::steady_clock::now()+timeout;
    while(healthy() && std::chrono::steady_clock::now()<deadline) {
        // Event-driven wait with a finite overall UAT deadline, not input polling.
        MSG msg{};
        detail::pump_glue_message_quantum([&] {
            if(!PeekMessageW(&msg,nullptr,0,0,PM_REMOVE))return false;
            if(msg.message==WM_QUIT){poison("owner_quit");return false;}
            if(msg.message!=ExplorerGroupEventSource::wake_message){TranslateMessage(&msg);DispatchMessageW(&msg);}
            return true;
        },[&]{return !healthy() || source_->size_!=0;});
        if(!healthy())return false;
        if(!source_->healthy()){poison("event_stream_failed");return false;}
        const auto events=source_->drain();
        if(!events.empty() && !quantum(events))return false;
        if(gestures_.size()>completed && gestures_.back().exact) {
            if(gestures_.back().source_member!=expected_member){poison("uat_gesture_order_mismatch");return false;}
            return true;
        }
        if(source_->facts().accepted>receipts_.size())continue;
        const auto left=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
        if(left<=0)break;
        if(MsgWaitForMultipleObjectsEx(0,nullptr,static_cast<DWORD>(left),QS_ALLINPUT,MWMO_INPUTAVAILABLE)==WAIT_FAILED) {
            poison("owner_wait_failed");return false;
        }
    }
    poison("gesture_timeout");return false;
}
bool ExplorerGroupSession::restore() {
    if(!healthy() || !setup_done_ || restored_ || active_member_ || gestures_.size()!=3)return false;
    if(!source_->stop()){poison("unhook_failed");return false;}
    const auto live=detail::ExplorerGroupBridge::capture(*seal_,sessions());
    if(!live){poison("restore_validation_failed");return false;}
    std::array<std::optional<Rect>,3> targets;
    for(std::size_t i=0;i<3;++i)targets[i]=original_[i].visible_rect;
    GroupOperationRecord record;record.group=generation();
    if(!apply_targets(targets,*live,record,nullptr))return false;
    restored_=current_==original_;
    if(!restored_)poison("restore_not_exact");
    return restored_;
}
} // namespace panebind::platform::windows::explorer
