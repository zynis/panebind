#include "platform/windows/explorer/explorer_live_magnet_session.h"
#include <limits>
namespace panebind::platform::windows::explorer {
namespace b=core::behavior;
namespace {
// Normalize a recent 32-bit provider stamp against its 64-bit system epoch.
// Reject future/ambiguous (> half range) stamps, not a suppression time window.
std::optional<std::uint64_t> provider_tick(DWORD tick) noexcept {
    const auto now=GetTickCount64();const auto age=static_cast<DWORD>(now)-tick;
    if(age>=0x80000000U||now<age)return {};return now-age;
}
struct PendingRegistration {ExplorerLiveMagnetSession* session;LiveMagnetOperation* operation;};
}
ExplorerLiveMagnetSession::ExplorerLiveMagnetSession(std::unique_ptr<ExplorerGroupSession> group)
    :group_(std::move(group)),audit_(std::make_unique<ConsentValidationAudit>()) {
    audit_->vdm_enabled=true;current_=group_->binding_snapshots();
    gestures_.reserve(64);operations_.reserve(1024);samples_.reserve(8192);quanta_.reserve(8192);receipts_.reserve(32768);
}
std::unique_ptr<ExplorerLiveMagnetSession> ExplorerLiveMagnetSession::create(ExplorerGroupSession::OwnedMembers members,Consent consent) {
    if(consent!=Consent::Confirmed)return {};
    auto group=ExplorerGroupSession::create(std::move(members));if(!group)return {};
    auto live=std::unique_ptr<ExplorerLiveMagnetSession>(new ExplorerLiveMagnetSession(std::move(group)));
    ConsentValidationAuditScope audit{live->audit_.get()};
    if(!detail::ExplorerGroupBridge::enable_live_magnet(*live->group_->seal_,live->group_->sessions())||!live->group_->source_->start())return {};
    return live;
}
ExplorerLiveMagnetSession::~ExplorerLiveMagnetSession()=default;
bool ExplorerLiveMagnetSession::healthy() const noexcept {return !failed_&&group_&&group_->healthy()&&!audit_->overflow;}
bool ExplorerLiveMagnetSession::stop(std::string_view reason) {
    failed_=true;reason_=reason;model_.abort(reason);
    if(active_source_&&!gestures_.empty()){
        auto& g=gestures_.back();g.route=model_.route();g.edges=model_.edges();g.counters=model_.counters();g.reason=reason;
    }
    group_->poison(reason);return false;
}
std::vector<b::MagnetMember> ExplorerLiveMagnetSession::members(const detail::GroupSnapshots& snapshots) const {
    std::vector<b::MagnetMember> result;result.reserve(3);
    for(std::size_t i=0;i<3;++i){const auto& binding=group_->bindings()[i];result.push_back({core::model::WindowId{std::to_string(binding.window_id)},binding.capability_generation,binding.consent_generation,snapshots[i].visible_rect});}
    return result;
}
bool ExplorerLiveMagnetSession::pump() noexcept {
    if(!healthy())return false;
    if(processing_)return stop("live_owner_reentrancy");
    if(!group_->source_->facts().running)return true;
    processing_=true;struct Guard{bool& value;~Guard(){value=false;}}guard{processing_};
    try {
        const auto events=group_->source_->drain();
        if(!group_->source_->healthy())return stop("live_event_stream_failed");
        if(events.empty())return true;
        return process(events);
    }catch(const std::exception&){return stop("live_processing_exception");}
}
bool ExplorerLiveMagnetSession::register_pending(void* raw) noexcept {
    auto& registration=*static_cast<PendingRegistration*>(raw);auto& s=*registration.session;auto& op=*registration.operation;
    if(!s.healthy()||!s.active_source_||*s.active_source_!=op.command.source||s.model_.ctrl()||
       s.group_->source_->magnet_conflict_pending(op.command.source))return false;
    op.watermark=s.group_->source_->watermark();op.provider_tick=GetTickCount64();
    try{return s.model_.register_pending(op.command,op.watermark,op.provider_tick);}catch(...){return false;}
}
bool ExplorerLiveMagnetSession::process(std::span<const GroupEventReceipt> events) {
    if(receipts_.size()+events.size()>32768||quanta_.size()>=8192||samples_.size()>=8192)return stop("live_evidence_capacity");
    std::optional<GroupEventReceipt> start,end,sample;
    for(const auto& r:events){
        if(r.sequence!=last_sequence_+1||r.member_index>=3)return stop("live_receipt_sequence");last_sequence_=r.sequence;
        const auto& binding=group_->bindings()[r.member_index];
        if(r.window_id!=binding.window_id||r.capability_generation!=binding.capability_generation||r.native_source!=binding.window||r.native_thread!=binding.thread_id)return stop("live_receipt_identity");
        if(r.kind==GroupEventKind::Destroy)return stop("live_member_destroyed");
        if(r.kind==GroupEventKind::Start){if(start||active_source_)return stop("live_concurrent_start");start=r;}
        if(r.kind==GroupEventKind::End){if(end)return stop("live_multiple_end");end=r;}
    }
    receipts_.insert(receipts_.end(),events.begin(),events.end());
    const auto owner=glue_qpc_now();const auto phase=start?ConsentValidationPhase::Start:active_source_?ConsentValidationPhase::Active:ConsentValidationPhase::Setup;
    ConsentValidationAuditScope audit{audit_.get()};ConsentValidationPhaseScope phase_scope{phase};
    const auto active_index=static_cast<std::size_t>(ConsentValidationPhase::Active);
    const auto inventory_before=audit_->inventory_calls[active_index],manager_before=audit_->manager_creates[active_index];
    last_capture_=detail::ExplorerGroupBridge::capture(*group_->seal_,group_->sessions());
    if(!last_capture_)return stop("live_capture_failed");
    auto live=*last_capture_;
    if(start){
        if(gestures_.size()>=64||!start->ctrl.available)return stop("live_start_capacity_or_input");
        active_source_=start->member_index;
        if(!model_.start(group_->generation(),++generation_,*active_source_,start->ctrl.ctrl,start->sequence,
                         static_cast<std::uint64_t>(start->callback_qpc),static_cast<std::uint64_t>(glue_qpc_frequency()),members(live)))return stop("live_model_start_failed");
        LiveMagnetGesture g;g.generation=generation_;g.start_receipt=start->sequence;g.source=*active_source_;g.ctrl=start->ctrl.ctrl;g.initial=live;gestures_.push_back(g);
        detail::ExplorerGroupBridge::active(*group_->seal_,group_->sessions(),true);
    }
    LiveMagnetQuantum quantum{quanta_.size()+1,events.front().sequence,events.back().sequence,generation_,events.size(),0,0,0,owner,0,0,0};
    if(!active_source_){
        if(end)return stop("live_orphan_end");current_=group_->current_=live;
        quantum.end_qpc=glue_qpc_now();quanta_.push_back(quantum);return true;
    }
    auto& g=gestures_.back();g.raw_receipts+=events.size();
    if(end&&end->member_index!=*active_source_)return stop("live_wrong_member_end");
    for(const auto& r:events){
        if(r.kind!=GroupEventKind::Location||r.sequence<=g.start_receipt)continue;
        if(!g.ctrl&&r.member_index!=*active_source_&&(!end||r.sequence<=end->sequence))return stop("unexpected_target_location");
        if(r.member_index==*active_source_){if(sample)++quantum.coalesced;sample=r;}
    }
    if(!model_.validate_members(members(live)))return stop(model_.reason());
    if(end)sample=end;
    const auto before=model_.counters();
    if(sample&&sample->sequence>g.start_receipt){
        const auto stamp=provider_tick(sample->native_time);if(!stamp)return stop("invalid_provider_timestamp");
        const auto command=model_.sample(sample->sequence,*stamp,static_cast<std::uint64_t>(sample->callback_qpc),live[*active_source_].visible_rect,sample->kind==GroupEventKind::Location);
        samples_.push_back({generation_,sample->sequence,*stamp,model_.last_feedback_operation(),*active_source_,live[*active_source_].visible_rect,model_.last_sample_result(),sample->callback_qpc,owner});
        if(model_.route()==b::MagnetRoute::CtrlResizeUnsupported)return stop("ctrl_resize_not_implemented");
        if(model_.route()==b::MagnetRoute::Aborted)return stop(model_.reason());
        if(command){
            if(operations_.size()>=1024)return stop("live_operation_capacity");
            LiveMagnetOperation operation{*command,*sample,0,0,owner,{}};
            PendingRegistration registration{this,&operation};
            operation.native=detail::ExplorerGroupBridge::apply_magnet(*group_->seal_,group_->sessions(),live,*command,&register_pending,&registration);
            operations_.push_back(operation);
            if(!operation.native.native_attempted&&(operation.native.reason=="raw_sample_superseded"||operation.native.reason=="outside_supported_work_area")) {
                model_.discard_superseded_proposal();
            } else {
                if(!operation.native.actual||!model_.postverify(command->operation,operation.native.native_success,
                    (*operation.native.actual)[*active_source_].visible_rect,operation.native.exact))return stop(operation.native.reason);
                live=*operation.native.actual;
            }
        }
    }
    if(g.ctrl){
        if(!accepted_){if(model_.route()==b::MagnetRoute::GlueMove)return stop("topology_not_accepted");}
        else if(!group_->quantum(events,&live)) {
            if(group_->reason()=="resize_or_mixed_before_start_drain"&&group_->operations().empty())return stop("ctrl_resize_not_implemented");
            return stop(group_->reason());
        }
    }
    quantum.solver_calls=model_.counters().solver_calls-before.solver_calls;
    quantum.corrections=model_.counters().corrections-before.corrections;
    if(quantum.solver_calls>1||quantum.corrections>1)return stop("live_quantum_operation_storm");
    if(end){
        if(g.ctrl&&accepted_)live=group_->current_;
        else {
            last_capture_=detail::ExplorerGroupBridge::capture(*group_->seal_,group_->sessions());
            if(!last_capture_)return stop("live_end_capture_failed");live=*last_capture_;
        }
        if(!model_.finish(end->sequence,members(live)))return stop(model_.reason());
        g.end_receipt=end->sequence;g.final=live;g.completed=true;g.topology=group_layout_readiness(live);
        g.route=model_.route();g.edges=model_.edges();g.counters=model_.counters();g.reason=model_.reason();
        current_=group_->current_=live;active_source_.reset();detail::ExplorerGroupBridge::active(*group_->seal_,group_->sessions(),false);
    }
    quantum.inventory_active=audit_->inventory_calls[active_index]-inventory_before;
    quantum.manager_creates_active=audit_->manager_creates[active_index]-manager_before;
    quantum.end_qpc=glue_qpc_now();quanta_.push_back(quantum);
    if(quantum.inventory_active||quantum.manager_creates_active||audit_->overflow)return stop("live_active_performance_gate");
    return true;
}
bool ExplorerLiveMagnetSession::accept_topology() {
    if(!healthy()||active_source_||accepted_||!pump()||active_source_)return false;
    ConsentValidationAuditScope audit{audit_.get()};ConsentValidationPhaseScope phase{ConsentValidationPhase::Setup};
    last_capture_=detail::ExplorerGroupBridge::capture(*group_->seal_,group_->sessions());
    if(!last_capture_)return stop("accept_capture_failed");
    if(group_->source_->lifecycle_pending()||!group_layout_readiness(*last_capture_).ready)return false;
    accepted_=*last_capture_;current_=*accepted_;group_->original_=group_->current_=*accepted_;group_->setup_done_=true;
    return true;
}
bool ExplorerLiveMagnetSession::restore() {
    if(!healthy()||!accepted_||active_source_||!pump()||active_source_)return false;
    ConsentValidationAuditScope audit{audit_.get()};ConsentValidationPhaseScope phase{ConsentValidationPhase::Restore};
    if(!group_->restore())return stop(group_->reason());
    current_=group_->current_;return current_==*accepted_;
}
}
