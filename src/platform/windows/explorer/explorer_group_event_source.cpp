#include "platform/windows/explorer/explorer_group_event_source.h"
#include <algorithm>
#include <limits>

namespace panebind::platform::windows::explorer {
namespace {
std::atomic<ExplorerGroupEventSource*> group_source{};
std::atomic<bool> group_hook_poison{};
constexpr DWORD flags=WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS;
}
ExplorerGroupEventSource::ExplorerGroupEventSource(const std::array<detail::GroupMemberBinding,3>& members)
    :members_(members),owner_(GetCurrentThreadId()) {}
ExplorerGroupEventSource::~ExplorerGroupEventSource() { static_cast<void>(stop()); }
bool ExplorerGroupEventSource::start() {
    if(running_ || GetCurrentThreadId()!=owner_ || group_hook_poison.load()) return false;
    for(std::size_t i=0;i<3;++i) {
        const auto& m=members_[i]; DWORD pid{};
        if(!m.window_id || !m.capability_generation || !m.consent_generation ||
           GetWindowThreadProcessId(m.window,&pid)!=m.thread_id || pid!=m.process_id ||
           !IsWindow(m.window) || GetAncestor(m.window,GA_ROOT)!=m.window) return false;
        for(std::size_t j=0;j<i;++j) if(m.window==members_[j].window || m.window_id==members_[j].window_id) return false;
    }
    ExplorerGroupEventSource* absent{};
    if(!group_source.compare_exchange_strong(absent,this)) return false;
    MSG message{}; PeekMessageW(&message,nullptr,WM_USER,WM_USER,PM_NOREMOVE);
    running_=true;
    for(std::size_t i=0;i<3;++i) {
        const DWORD pid=members_[i].process_id;
        bool already=false;
        for(std::size_t j=0;j<i;++j) already|=members_[j].process_id==pid;
        if(already)continue;
        for(const auto range:std::array<std::pair<DWORD,DWORD>,3>{{
            {EVENT_SYSTEM_MOVESIZESTART,EVENT_SYSTEM_MOVESIZEEND},
            {EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_LOCATIONCHANGE},
            {EVENT_OBJECT_DESTROY,EVENT_OBJECT_DESTROY}}}) {
            auto hook=SetWinEventHook(range.first,range.second,nullptr,&callback,pid,0,flags);
            if(!hook) {poisoned_=true;static_cast<void>(stop());return false;}
            hooks_[hook_count_++]={hook,pid,range.first,range.second};
        }
    }
    return true;
}
bool ExplorerGroupEventSource::stop() noexcept {
    if(GetCurrentThreadId()!=owner_) {poisoned_=true;return false;}
    bool success=true;
    for(std::size_t i=0;i<hook_count_;++i) {
#if defined(PANEBIND_EXPLORER_GROUP_TESTING)
        if(synthetic_)continue;
#endif
        if(hooks_[i].value && !UnhookWinEvent(hooks_[i].value)) {success=false;group_hook_poison=true;}
    }
    hook_count_=0; running_=false;
    auto* expected=this; group_source.compare_exchange_strong(expected,nullptr);
    if(!success)poisoned_=true;
    return success;
}
void CALLBACK ExplorerGroupEventSource::callback(HWINEVENTHOOK hook,DWORD event,HWND window,
    LONG object,LONG child,DWORD thread,DWORD time) noexcept {
    auto* source=group_source.load();
    if(source)source->receive(hook,event,window,object,child,thread,time);
}
void ExplorerGroupEventSource::receive(HWINEVENTHOOK hook,DWORD event,HWND window,
    LONG object,LONG child,DWORD thread,DWORD time) noexcept {
    if(GetCurrentThreadId()!=owner_) {poisoned_=true;return;}
    if(!running_ || poisoned_)return;
    if(in_callback_) {poisoned_=true;return;}
    in_callback_=true;
    struct Guard {bool& value;~Guard(){value=false;}} guard{in_callback_};
    if(object!=OBJID_WINDOW || child!=CHILDID_SELF) {++ignored_;return;}
    if(event!=EVENT_SYSTEM_MOVESIZESTART && event!=EVENT_SYSTEM_MOVESIZEEND &&
       event!=EVENT_OBJECT_LOCATIONCHANGE && event!=EVENT_OBJECT_DESTROY) {++ignored_;return;}
    std::size_t index=3;
    for(std::size_t i=0;i<3;++i)if(members_[i].window==window){index=i;break;}
    if(index==3){++ignored_;return;}
    const auto& binding=members_[index];
    bool hook_matches=false;
    for(std::size_t i=0;i<hook_count_;++i) if(hooks_[i].value==hook &&
        hooks_[i].pid==binding.process_id && event>=hooks_[i].first && event<=hooks_[i].last)
        hook_matches=true;
    if(!hook_matches || thread!=binding.thread_id) {poisoned_=true;return;}
    if(size_==queue_.size() || sequence_==std::numeric_limits<std::uint64_t>::max()) {
        ++overflow_;poisoned_=true;return;
    }
    const auto kind=event==EVENT_SYSTEM_MOVESIZESTART?GroupEventKind::Start:
        event==EVENT_SYSTEM_MOVESIZEEND?GroupEventKind::End:
        event==EVENT_OBJECT_DESTROY?GroupEventKind::Destroy:GroupEventKind::Location;
    CtrlSample ctrl;
    if(kind==GroupEventKind::Start) {
#if defined(PANEBIND_EXPLORER_GROUP_TESTING)
        ctrl=synthetic_?synthetic_ctrl_:sample_ctrl();
#else
        ctrl=sample_ctrl();
#endif
    }
    queue_[(head_+size_)%queue_.size()]={index,binding.window_id,binding.capability_generation,
        window,kind,++sequence_,thread,time,glue_qpc_now(),ctrl};
    ++size_;max_depth_=std::max(max_depth_,size_);
    if(!notified_) {
        bool posted=false;
#if defined(PANEBIND_EXPLORER_GROUP_TESTING)
        if(synthetic_)posted=true;
        else
#endif
        posted=PostThreadMessageW(owner_,wake_message,reinterpret_cast<WPARAM>(this),0)!=FALSE;
        if(!posted){++post_failure_;poisoned_=true;}else notified_=true;
    }
}
std::vector<GroupEventReceipt> ExplorerGroupEventSource::drain() {
    if(GetCurrentThreadId()!=owner_){poisoned_=true;return {};}
    std::vector<GroupEventReceipt> result;result.reserve(size_);
    const auto count=size_;
    for(std::size_t i=0;i<count;++i) {
        const auto receipt=queue_[head_]; head_=(head_+1)%queue_.size();--size_;
        const auto& m=members_[receipt.member_index];
        if(receipt.kind!=GroupEventKind::Destroy) {
            DWORD pid{};
            if(!IsWindow(m.window) || GetWindowThreadProcessId(m.window,&pid)!=m.thread_id ||
               pid!=m.process_id || GetAncestor(m.window,GA_ROOT)!=m.window)poisoned_=true;
        }
        result.push_back(receipt);
    }
    notified_=false;
    return result;
}
bool ExplorerGroupEventSource::lifecycle_pending(std::optional<std::size_t> active_member) const noexcept {
    if(GetCurrentThreadId()!=owner_ || poisoned_)return true;
    for(std::size_t i=0;i<size_;++i) {
        const auto& receipt=queue_[(head_+i)%queue_.size()];
        const auto kind=receipt.kind;
        if(kind==GroupEventKind::End && active_member && receipt.member_index==*active_member)continue;
        if(kind!=GroupEventKind::Location)return true;
    }
    return false;
}
GroupEventFacts ExplorerGroupEventSource::facts() const noexcept {
    return {sequence_,ignored_,overflow_,post_failure_,max_depth_,poisoned_.load(),running_};
}
bool ExplorerGroupEventSource::magnet_conflict_pending(std::size_t source) const noexcept {
    if(GetCurrentThreadId()!=owner_||poisoned_||source>=3)return true;
    for(std::size_t i=0;i<size_;++i){const auto& r=queue_[(head_+i)%queue_.size()];
        if(r.kind!=GroupEventKind::Location||r.member_index!=source)return true;
    }
    return false;
}
} // namespace panebind::platform::windows::explorer
