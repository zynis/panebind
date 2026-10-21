#include "input_service.h"
#include "guest_capture_scope.h"
#include <algorithm>
#include <stdexcept>

#ifndef PANEBIND_BUILD_SHA
#define PANEBIND_BUILD_SHA "unknown"
#endif

namespace panebind::platform::windows::product {
InputService* InputService::instance_{};
namespace {
constexpr UINT_PTR gesture_deadline_timer=1;
constexpr UINT gesture_deadline_ms=30000;
constexpr UINT capture_request_message=WM_APP+44,capture_events_message=WM_APP+45,capture_stop_message=WM_APP+46;
constexpr UINT guest_callback_stop_message=WM_APP+47;
using ShieldEvent=operations::GestureShieldEvent;
using ShieldKind=operations::GestureShieldEventKind;
using Removal=operations::GestureShieldRemovalReason;
std::uint64_t qpc_now(){LARGE_INTEGER q{};return QueryPerformanceCounter(&q)&&q.QuadPart>0?static_cast<std::uint64_t>(q.QuadPart):0;}
bool capture_source_current(const Snapshot& source){
    DWORD pid{};const auto tid=GetWindowThreadProcessId(source.hwnd,&pid);
    if(!source.hwnd||!source.key||!IsWindow(source.hwnd)||GetAncestor(source.hwnd,GA_ROOT)!=source.hwnd||pid!=source.pid||tid!=source.tid)return false;
    const auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return false;
    FILETIME born{},exit{},kernel{},user{};const bool ok=GetProcessTimes(process,&born,&exit,&kernel,&user)!=FALSE;CloseHandle(process);
    return ok&&((static_cast<std::uint64_t>(born.dwHighDateTime)<<32)|born.dwLowDateTime)==source.born;
}
unsigned modifiers(){return ((GetAsyncKeyState(VK_MENU)&0x8000)?1u:0u)|
    ((GetAsyncKeyState(VK_CONTROL)&0x8000)?2u:0u)|((GetAsyncKeyState(VK_SHIFT)&0x8000)?4u:0u)|
    (((GetAsyncKeyState(VK_LWIN)|GetAsyncKeyState(VK_RWIN))&0x8000)?8u:0u);}
int button(WPARAM m,DWORD data){
    if(m==WM_LBUTTONDOWN||m==WM_LBUTTONUP)return 1;
    if(m==WM_RBUTTONDOWN||m==WM_RBUTTONUP)return 2;
    if(m==WM_MBUTTONDOWN||m==WM_MBUTTONUP)return 3;
    if(m==WM_XBUTTONDOWN||m==WM_XBUTTONUP)return HIWORD(data)==XBUTTON1?4:5;
    return 0;
}
bool down(WPARAM m){return m==WM_LBUTTONDOWN||m==WM_RBUTTONDOWN||m==WM_MBUTTONDOWN||m==WM_XBUTTONDOWN;}
bool up(WPARAM m){return m==WM_LBUTTONUP||m==WM_RBUTTONUP||m==WM_MBUTTONUP||m==WM_XBUTTONUP;}
}
struct InputService::CaptureBridge {
    struct Pair {
        InputEvent down;
        operations::GestureShieldConsumedDownRequest request;
        bool native_move{};
        std::atomic<NativeMovePhase> native_phase{NativeMovePhase::AwaitStart};
        std::atomic<std::shared_ptr<const NativeMoveReceipt>> native_start,native_end;
        std::atomic<std::shared_ptr<const Snapshot>> handoff;
        std::atomic<bool> isolation_ready{},cancel_claimed{},raw_up_observed{},resource_requested{};
        std::atomic<bool> down_assigned{};
        std::atomic<bool> up_observed{},aborted{},normal_observed{},ready_authorized{};
        std::atomic<std::shared_ptr<const GestureCaptureContext>> context;
        std::uint64_t deadline{},down_serial{};
        std::shared_ptr<const ShieldEvent> actual_legacy_up;
        // Input-owner-only receipt, separate from strict normal own-release.
        std::shared_ptr<const ShieldEvent> capture_cleanup_observation;
    };
    struct Receipt {std::shared_ptr<const ShieldEvent> event;std::uint64_t qpc{};};
    std::shared_ptr<std::atomic<std::uint64_t>> authority,completion;
    std::shared_ptr<std::atomic<bool>> available;
    std::atomic<std::shared_ptr<Pair>> pair;
    // One deferred old-generation reference, consumed only on the input owner
    // outside callbacks. A busy slot rejects a new candidate instead of making
    // the LL publisher release an old Pair's last reference/heap allocation.
    std::atomic<std::shared_ptr<Pair>> retired_pair;
    std::atomic<DWORD> owner_tid{};
    std::atomic<bool> hotkey_registered{},failed{};
    std::atomic<std::uint64_t> down_serial{};
    std::array<Receipt,64> events;
    std::atomic<unsigned> read{},write{};
    bool active(const std::shared_ptr<Pair>& p)const {
        const auto a=authority->load();
        return p&&p->down.prepared&&!p->up_observed&&!p->aborted&&!failed&&available->load()&&
            (a==p->down.generation||a==(p->down.generation|pending_capture_mask))&&
            p->down_serial==down_serial.load()&&GetTickCount64()<p->deadline;
    }
    bool native_authorized(const operations::GestureShieldRequest& request,bool capture)const {
        const auto p=pair.load();if(!p||!p->native_move||!active(p))return false;
        const auto& actual=p->request.gesture;
        if(request.generation!=actual.generation||request.source!=actual.source||
           request.process_id!=actual.process_id||request.thread_id!=actual.thread_id)return false;
        const auto phase=p->native_phase.load();
        if(capture)return p->cancel_claimed&&p->native_end.load()&&p->handoff.load()&&
            (phase==NativeMovePhase::AwaitCapture||phase==NativeMovePhase::Writing);
        return p->native_start.load()&&!p->native_end.load()&&
            (phase==NativeMovePhase::IsolationPending||phase==NativeMovePhase::AwaitEnd);
    }
    bool authorized(const operations::GestureShieldConsumedDownRequest& request)const {
        const auto p=pair.load();const auto a=authority->load();
        return p&&!p->native_move&&consumed_capture_request_matches_down(request,p->down)&&
            request.source_was_maximized==p->request.source_was_maximized&&request.down_message_time==p->request.down_message_time&&
            request.down_qpc==p->request.down_qpc&&!p->up_observed&&!p->aborted&&!failed&&available->load()&&
            (a==p->down.generation||a==(p->down.generation|pending_capture_mask))&&
            p->down_serial==down_serial.load()&&GetTickCount64()<p->deadline;
    }
    void receive(const ShieldEvent& event){
        const auto p=pair.load();const bool current=p&&p->down.generation==event.generation;
        if(!current&&event.kind==ShieldKind::ResourceFailure){failed=true;*authority=0;*completion=0;*available=false;}
        if(current){
            const auto retire=[&]{auto a=p->down.generation;authority->compare_exchange_strong(a,0);
                a=p->down.generation|pending_capture_mask;authority->compare_exchange_strong(a,0);};
            if(event.kind==ShieldKind::IsolationReady&&p->native_move&&
               event.entry_mode==operations::GestureShieldEntryMode::NativeMoveHandoff&&
               event.gesture.generation==p->down.generation&&event.gesture.source==p->down.prepared->source.hwnd)
                p->isolation_ready=true; // publish BEFORE this receipt can reach the GUI
            if(event.kind==ShieldKind::RawMouse&&(event.raw_button_flags&RI_MOUSE_LEFT_BUTTON_UP)){
                p->raw_up_observed=true;p->up_observed=true;retire();
                if(auto context=p->context.load())context->matching_up_observed.store(true,std::memory_order_release);
            }
            if(event.kind==ShieldKind::LegacyLeftUp&&event.legacy_up_accepted){
                p->up_observed=true;retire();p->normal_observed=true;
                if(auto context=p->context.load())context->normal_up_observed.store(true,std::memory_order_release);
                p->actual_legacy_up=std::make_shared<const ShieldEvent>(event);
            }
            if(event.kind==ShieldKind::CaptureReady){
                if((p->native_move?native_authorized(p->request.gesture,true):authorized(p->request))&&
                   event.overlay&&event.capture.actual_after==event.overlay&&
                   event.capture.owner_thread_id&&event.capture.association.attach_succeeded){
                    auto context=std::make_shared<GestureCaptureContext>();context->generation=event.generation;
                    context->source=p->native_move?*p->handoff.load():p->down.prepared->source;context->overlay=event.overlay;
                    context->owner_tid=event.capture.owner_thread_id;context->owner_pid=GetCurrentProcessId();
                    p->context.store(context);
                    if(arm_exact_capture_generation(*authority,event.generation)){
                        p->ready_authorized=true;if(p->native_move)p->native_phase=NativeMovePhase::Writing;
                    }
                    else p->up_observed=true;
                }
            }
            if(event.kind==ShieldKind::CaptureReleased&&event.capture.release_attempted&&event.capture.release_completed&&
               event.capture.release_succeeded&&event.capture.release_before==event.overlay&&!event.capture.release_after)
                if(auto context=p->context.load())context->own_release_confirmed.store(true,std::memory_order_release);
            if(event.kind==ShieldKind::ThreadDetached&&operations::gesture_shield_association_cleanup_confirmed(event.capture.association,event.generation))
                if(auto context=p->context.load())context->association_detached.store(true,std::memory_order_release);
            if(event.kind==ShieldKind::ContextLost||event.kind==ShieldKind::ResourceFailure||event.kind==ShieldKind::CaptureFailure||
               event.kind==ShieldKind::CaptureLost||event.kind==ShieldKind::ThreadAssociationFailure||
               event.kind==ShieldKind::Deadline||event.kind==ShieldKind::HotkeyStop){
                p->aborted=true;retire();auto g=p->down.generation;completion->compare_exchange_strong(g,0);
            }
        }
        // Raw motion is not an audit stream; preserve button facts and actual
        // lifecycle receipts only. No input/resource thread writes to disk.
        if(event.kind==ShieldKind::RawMouse&&!event.raw_button_flags)return;
        const auto w=write.load(std::memory_order_relaxed),next=(w+1)%static_cast<unsigned>(events.size());
        const auto tid=owner_tid.load();
        if(next==read.load(std::memory_order_acquire)||!tid){failed=true;*authority=0;*completion=0;*available=false;
            throw std::runtime_error("product_capture_receipt_queue_unavailable");}
        events[w]={std::make_shared<const ShieldEvent>(event),qpc_now()};write.store(next,std::memory_order_release);
        if(!PostThreadMessageW(tid,capture_events_message,0,0)){failed=true;*authority=0;*completion=0;*available=false;
            throw std::runtime_error("product_capture_receipt_post_failed");}
    }
};
struct InputService::PreparedBundle {
    std::shared_ptr<const Prepared> prepared;
    std::shared_ptr<CaptureBridge::Pair> native_pair;
};
void InputService::prepare(std::shared_ptr<const Prepared> value){
    if(!value){prepared_.store({});return;}
    try{
        auto bundle=std::make_shared<PreparedBundle>();bundle->prepared=std::move(value);
        const auto& p=*bundle->prepared;
        if(native_geometry_entry(capture_route_enabled_,p,1,1,0,route_down(p,1,1,0))||
           native_geometry_entry(capture_route_enabled_,p,1,1,2,route_down(p,1,1,2)))
            bundle->native_pair=std::make_shared<CaptureBridge::Pair>();
        prepared_.store(std::move(bundle));
    }catch(...){
        prepared_.store({});*available_=false;healthy_=false;
        PostMessageW(owner_,catalog_message,1,0);
    }
}
bool InputService::native_move_pending(std::uint64_t generation)const {
    const auto bridge=capture_bridge_;if(!bridge)return false;const auto p=bridge->pair.load();
    return p&&p->native_move&&p->down.generation==generation&&bridge->active(p);
}
bool InputService::native_move_in_progress()const {
    const auto bridge=capture_bridge_;if(!bridge)return false;const auto p=bridge->pair.load();
    return p&&p->native_move&&(bridge->active(p)||p->resource_requested.load());
}
bool InputService::request_native_move_isolation(std::uint64_t generation,const std::shared_ptr<const NativeMoveReceipt>& receipt){
    const auto bridge=capture_bridge_;if(!bridge||!shield_)return false;const auto p=bridge->pair.load();
    if(!p||!p->native_move||p->down.generation!=generation||!bridge->active(p)||!receipt||
       p->native_start.load()!=receipt||p->native_end.load()||
       !native_move_receipt_matches(*receipt,*p->down.prepared,generation,p->down.native_message_time,EVENT_SYSTEM_MOVESIZESTART))return false;
    auto phase=NativeMovePhase::AwaitStart;
    if(!p->native_phase.compare_exchange_strong(phase,NativeMovePhase::IsolationPending))return false;
    p->resource_requested=true;
    if(shield_->request_isolation(p->request.gesture))return true;
    p->resource_requested=false;p->aborted=true;p->native_phase=NativeMovePhase::Retired;revoke_generation(generation);return false;
}
bool InputService::claim_native_move_cancel(std::uint64_t generation){
    const auto bridge=capture_bridge_;if(!bridge)return false;const auto p=bridge->pair.load();
    if(!p||!p->native_move||p->down.generation!=generation||!bridge->active(p)||!p->isolation_ready||p->native_end.load())return false;
    auto phase=NativeMovePhase::IsolationPending;
    if(!p->native_phase.compare_exchange_strong(phase,NativeMovePhase::AwaitEnd))return false;
    bool claimed=false;return p->cancel_claimed.compare_exchange_strong(claimed,true)&&bridge->active(p);
}
bool InputService::request_native_move_capture(std::uint64_t generation,const std::shared_ptr<const NativeMoveReceipt>& receipt,const Snapshot& actual){
    const auto bridge=capture_bridge_;if(!bridge||!shield_)return false;const auto p=bridge->pair.load();
    if(!p||!p->native_move||p->down.generation!=generation||!bridge->active(p)||!p->cancel_claimed||!receipt||
       p->native_end.load()!=receipt||!p->native_start.load()||!native_move_end_follows(*p->native_start.load(),*receipt)||
       !native_move_receipt_matches(*receipt,*p->down.prepared,generation,p->down.native_message_time,EVENT_SYSTEM_MOVESIZEEND)||
       !native_geometry_handoff_compatible(*p->down.prepared,actual))return false;
    auto phase=NativeMovePhase::AwaitEnd;
    if(!p->native_phase.compare_exchange_strong(phase,NativeMovePhase::AwaitCapture))return false;
    p->handoff.store(std::make_shared<const Snapshot>(actual));
    if(bridge->active(p)&&shield_->request_capture_after_native_end(p->request.gesture))return true;
    p->aborted=true;revoke_generation(generation);return false;
}
GestureCaptureLease InputService::capture_context(std::uint64_t generation)const {
    const auto bridge=capture_bridge_;if(!bridge)return {};const auto pair=bridge->pair.load();
    return pair&&pair->down.generation==generation?pair->context.load():GestureCaptureLease{};
}
void InputService::retire_capture_pair(std::uint64_t generation,core::v1::End reason){
    const auto bridge=capture_bridge_;if(!bridge)return;const auto pair=bridge->pair.load();
    if(pair&&(!generation||pair->down.generation==generation)){
        pair->aborted=true;const auto tid=bridge->owner_tid.load();
        if(tid)PostThreadMessageW(tid,WM_APP+2,static_cast<WPARAM>(pair->down.generation),static_cast<LPARAM>(reason));
    }
}
void InputService::request_capture_stop(Removal reason){
    if(!capture_pair_||!capture_bridge_||!shield_)return;const auto p=capture_bridge_->pair.load();if(!p)return;
    if(reason!=Removal::NormalUp)p->aborted=true;
    // abort() is also reachable from LL callbacks: never enter the shield's
    // command mutex there. The owner handles this nonblocking notification.
    if(!PostThreadMessageW(thread_id_,capture_stop_message,static_cast<WPARAM>(p->down.generation),static_cast<LPARAM>(reason))){
        capture_bridge_->failed=true;*available_=false;healthy_=false;
    }
}
bool InputService::begin_capture(const InputEvent& event){
    if(!capture_bridge_||!shield_||!event.prepared||!capture_source_current(event.prepared->source)||
       authorized_->load()!=(event.generation|pending_capture_mask)||
       !guest::capture_interactive_medium()||GetForegroundWindow()!=event.prepared->source.hwnd)return false;
    if(event.kind==InputKind::NativeMovePending){
        const auto p=capture_bridge_->pair.load();
        return p&&p->native_move&&p->down.generation==event.generation&&capture_bridge_->active(p)&&
            SetTimer(receiver_,gesture_deadline_timer,gesture_deadline_ms,nullptr);
    }
    if(event.kind!=InputKind::Down||!consumed_down_||native_move_||!capture_pair_)return false;
    auto p=std::make_shared<CaptureBridge::Pair>();p->down=event;p->down_serial=capture_bridge_->down_serial.load();
    p->deadline=event.tick+gesture_deadline_ms;
    p->request.gesture={event.generation,event.prepared->source.hwnd,event.prepared->source.pid,event.prepared->source.tid,gesture_deadline_ms};
    p->request.down_point=event.point;p->request.down_message_time=event.native_message_time;p->request.down_qpc=event.hook_observed_qpc;
    p->request.source_was_maximized=event.prepared->source.maximized;
    if(!consumed_capture_request_matches_down(p->request,event))return false;
    p->request.external_stop_owner_pid=GetCurrentProcessId();p->request.external_stop_owner_tid=thread_id_;
    p->request.external_stop_hotkey_id=910;capture_bridge_->pair.store(p);
    return shield_->request_capture_for_consumed_down(p->request);
}
void InputService::drain_capture_events(){
    if(!capture_bridge_)return;auto& bridge=*capture_bridge_;
    if(bridge.failed){healthy_=false;*available_=false;}
    for(unsigned budget=0;budget<64;++budget){const auto r=bridge.read.load(std::memory_order_relaxed);
        if(r==bridge.write.load(std::memory_order_acquire))break;auto receipt=std::move(bridge.events[r]);
        bridge.read.store((r+1)%static_cast<unsigned>(bridge.events.size()),std::memory_order_release);
        InputEvent resource;resource.kind=InputKind::Resource;resource.origin=InputOrigin::ShieldResource;
        resource.generation=receipt.event->generation;resource.tick=GetTickCount64();resource.resource_observed_qpc=receipt.qpc;
        resource.native_message_time=receipt.event->message_time;resource.resource_event=receipt.event;push(std::move(resource));
        const auto p=bridge.pair.load();if(!p||p->down.generation!=receipt.event->generation)continue;
        const auto& actual=*receipt.event;
        if(actual.kind==ShieldKind::IsolationReady)p->isolation_ready=true;
        if(actual.kind==ShieldKind::CaptureReleased)p->capture_cleanup_observation=receipt.event;
        if(actual.kind==ShieldKind::CaptureReady){
            // This is the historical READY receipt, not a fresh regrant. A
            // real UP may already have revoked issue permission while this
            // owner was dispatching earlier messages. Preserve DOWN→UP order
            // without turning allowed() back on or inventing late movement.
            if(p->ready_authorized&&p->context.load()&&!p->aborted){
                // Duplicate READY is a receipt, not a second GUI DOWN. An
                // actual UP may already have retired motion; the historical
                // first DOWN can still be published once for terminal order.
                if(captured_move_.may_queue_down(actual.generation)){
                    auto down=p->down;down.kind=InputKind::Down;down.handoff_snapshot=p->handoff.load();
                    down.native_move_receipt=p->native_end.load();const bool queued=push(std::move(down));
                    if(queued&&captured_move_.mark_down_enqueued(actual.generation)){
                        // The LL owner may see a real MOVE after READY arms g
                        // but before this historical DOWN is queued. Native
                        // pre-END movement was never retained in this slot.
                        const auto sample=captured_move_.take_after_down(actual.generation,
                            bridge.active(p)&&allowed(actual.generation));
                        if(sample){auto move=p->down;move.kind=InputKind::Move;move.point=sample->point;
                            move.tick=sample->tick;move.hook_observed_qpc=sample->hook_qpc;
                            move.native_message_time=sample->native_message_time;push(std::move(move));}
                    }else captured_move_.retire();
                }
            }
            // An early UP is not NormalUp and never permits late re-arming.
            else{captured_move_.retire();request_capture_stop(Removal::ContextLost);}
        }
        if(p->aborted&&!aborted_)abort(actual.kind==ShieldKind::Deadline?core::v1::End::Deadline:core::v1::End::SourceLost);
        if(p->native_move&&!p->aborted&&p->raw_up_observed&&p->normal_observed&&
           actual.kind!=ShieldKind::IsolationGone&&shield_)
            (void)shield_->request_remove(actual.generation,Removal::NormalUp);
        if(actual.kind!=ShieldKind::IsolationGone)continue;
        captured_move_.retire();
        const auto context=p->context.load();const bool strict_clean=actual.overlay_destroyed&&actual.hotkey_unregistered&&
            (!actual.capture.attempted||(actual.capture.release_completed&&actual.capture.release_succeeded&&!actual.capture.release_after))&&
            operations::gesture_shield_association_cleanup_confirmed(actual.capture.association,actual.generation);
        // An abort may already have lost capture before cleanup. The resource
        // must not release foreign capture, and a real no-own observation plus
        // completed detach/destruction is not a failed ReleaseCapture call.
        const bool clean=strict_clean||captured_abort_cleanup_confirmed(context.get(),
            p->capture_cleanup_observation.get(),actual,p->aborted.load());
        const bool actual_normal=p->normal_observed&&p->actual_legacy_up&&context&&
            (!p->native_move||p->raw_up_observed)&&
            actual.removal_reason==Removal::NormalUp&&context->normal_up_observed&&context->own_release_confirmed&&context->association_detached&&
            p->ready_authorized;
        const bool normal=captured_release_can_complete(clean,actual_normal,p->aborted,
            bridge.down_serial.load()==p->down_serial,!stop_requested_&&!paused_&&available_->load());
        if(normal){
            auto e=p->down;e.kind=InputKind::Up;e.end=core::v1::End::NormalUp;e.origin=InputOrigin::ShieldLegacy;
            e.point=p->actual_legacy_up->message_point;e.native_message_time=p->actual_legacy_up->message_time;
            e.hook_observed_qpc=0;e.resource_observed_qpc=p->actual_legacy_up->legacy_up_dispatch_qpc;e.tick=GetTickCount64();
            *completion_=p->down.generation;
            // A GUI stop/revoke can race this owner's release dispatch. Its
            // monotonic per-pair abort cannot be undone by a quick resume.
            if(p->aborted||!available_->load()||paused_||stop_requested_){
                auto generation=p->down.generation;completion_->compare_exchange_strong(generation,0);abort(core::v1::End::SourceLost);
            }else push(std::move(e));
        }else if(!aborted_)abort(core::v1::End::SourceLost);
        if(native_move_resources_can_retire(actual.kind,clean)){
            p->resource_requested=false;if(p->native_move)p->native_phase=NativeMovePhase::Retired;
        }else {healthy_=false;*available_=false;}
        KillTimer(receiver_,gesture_deadline_timer);consumed_down_=false;native_move_=false;capture_pair_=false;active_prepared_.reset();prepared_.store({});
        if(!prepare_pending_.exchange(true))PostMessageW(owner_,prepare_message,0,0);
    }
}
bool InputService::start(HWND owner,bool diagnostic){
    if(thread_.joinable())return healthy_;
    if(!IsWindow(owner))return false;
    button_diagnostic_=diagnostic;
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,"hook timing counters must not hide a lock");
    LARGE_INTEGER frequency{};counter_frequency_=QueryPerformanceFrequency(&frequency)&&frequency.QuadPart>0?
        static_cast<std::uint64_t>(frequency.QuadPart):0;
    paused_=false;healthy_=false;stop_requested_=false;prepare_pending_=false;
    read_=0;write_=0;consumed_down_=false;native_move_=false;aborted_=false;click_candidate_={};
    captured_move_.retire();
    owner_=owner;ready_=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!ready_)return false;
    try{thread_=std::thread([this]{run();});}catch(...){CloseHandle(ready_);ready_=nullptr;return false;}
    const auto result=WaitForSingleObject(ready_,5000);
    // Keep the initialization event alive until the worker is done using it,
    // including the timeout path. Closing first races the worker's SetEvent.
    if(result!=WAIT_OBJECT_0||!healthy_)stop();
    CloseHandle(ready_);ready_=nullptr;
    const bool started=result==WAIT_OBJECT_0&&healthy_;
    if(started)input_owner_start_serial_.fetch_add(1,std::memory_order_release);
    return started;
}
void InputService::stop(){
    // The one-shot wait callback never waits on this GUI thread. Cancel/join
    // it before closing the std::thread HANDLE or destroying its this/HWND.
    if(guest_callback_wait_){
        if(guest_callback_wait_context_)guest_callback_wait_context_->owner=nullptr;
        guest_callback_wait_unregister_attempted_=true;SetLastError(0);
        guest_callback_wait_unregister_return_=UnregisterWaitEx(guest_callback_wait_,INVALID_HANDLE_VALUE)!=FALSE;
        guest_callback_wait_unregister_error_=guest_callback_wait_unregister_return_?0:GetLastError();
        if(guest_callback_wait_context_){
            guest_callback_wait_signaled_=guest_callback_wait_context_->signaled.load();
            guest_callback_wait_notified_=guest_callback_wait_context_->notified.load();
            if(guest_callback_wait_unregister_return_)guest_callback_wait_context_.reset();
            else (void)guest_callback_wait_context_.release(); // one bounded failed-wait context; inert owner, no dangling this
        }
        guest_callback_wait_=nullptr;
    }
    retire_capture_pair(0,core::v1::End::Exit);
    *completion_=0;*available_=false;*authorized_=0;paused_=true;stop_requested_=true;
    if(thread_.joinable()){const auto id=thread_id_.load();if(id)PostThreadMessageW(id,WM_QUIT,0,0);thread_.join();}
    prepared_.store({});guest_callback_wait_thread_=nullptr;
}
void InputService::pause(bool value){if(value)retire_capture_pair(0,core::v1::End::Paused);*completion_=0;paused_=value;*available_=!value&&healthy_;const auto retired=authorized_->exchange(0);
    auto generation=retired&~pending_capture_mask;
    if(!generation&&capture_bridge_)if(const auto pair=capture_bridge_->pair.load())generation=pair->down.generation;
    const auto id=thread_id_.load();if(value&&id&&generation)PostThreadMessageW(id,WM_APP+1,static_cast<WPARAM>(generation),0);}
POINT InputService::latest()const{auto p=point_.load();return {static_cast<LONG>(p>>32),static_cast<LONG>(p&0xffffffffu)};}
bool InputService::arm_guest_queue_failure(std::string_view run_id,std::uint64_t generation,HWND source,std::string& why){
    return arm_guest_input_failure(run_id,generation,source,false,why);
}
bool InputService::arm_guest_input_failure(std::string_view run_id,std::uint64_t generation,HWND source,bool callback_owner_stop,std::string& why){
    // Called only by the normal GUI pump, never WM_COPYDATA's borrowed buffer
    // or a hook/resource callback. No ordinary product admission is a fallback.
    const auto reject=[&](const char* reason){why=reason;return false;};
    auto& state=callback_owner_stop?guest_callback_stop_state_:guest_queue_failure_state_;
    auto& permit=callback_owner_stop?guest_callback_stop_permit_:guest_queue_failure_permit_;
    if(state.load()!=0)return reject("one_process_lifetime_permit_already_used_or_armed");
    const auto scope=guest::detect_capture_scope(PANEBIND_BUILD_SHA);
    if(!scope.enabled||scope.run_id!=run_id||!generation||!source)
        return reject("exact_Medium_disposable_Run_product_proof_required");
    const std::filesystem::path input=L"C:\\PaneBindV1\\Input";
    const auto manifest=guest::capture_read(input/L"manifest.json");const auto files=manifest.find("\"files\"");
    if(files==manifest.npos||!(callback_owner_stop?guest::capture_manifest_suite(manifest,"CallbackOwnerStopOnly"):
         guest::capture_manifest_suite(manifest,"QueueFailureOnly")))
        return reject(callback_owner_stop?"exact_callback_owner_stop_suite_required":"exact_queue_failure_suite_required");
    const auto pair=capture_bridge_?capture_bridge_->pair.load():nullptr;
    const auto context=pair?pair->context.load():GestureCaptureLease{};
    if(!pair||!context||!pair->down.prepared||!capture_bridge_->active(pair)||!pair->ready_authorized||
       !allowed(generation)||!healthy_||paused_||pair->down.generation!=generation||context->generation!=generation||
       pair->down.prepared->source.hwnd!=source||context->source.hwnd!=source||
       pair->up_observed||pair->aborted||context->matching_up_observed||
       (pair->native_move&&pair->native_phase!=NativeMovePhase::Writing)||
       !Catalog::same(pair->down.prepared->source,context->source)||GetForegroundWindow()!=source||
       !operations::gesture_shield_exact_capture(context->overlay,context->owner_tid))
        return reject("actual_current_active_CaptureReady_source_required");
    const auto& identity=context->source;
    const std::wstring wrun(run_id.begin(),run_id.end());std::array<wchar_t,256> cls{};
    if(!capture_source_current(identity)||identity.pid==GetCurrentProcessId()||
       !GetClassNameW(source,cls.data(),static_cast<int>(cls.size()))||
       std::wstring_view(cls.data())!=L"PaneBind.V1.OwnedFixture."+wrun||
       identity.window_class!=cls.data())return reject("exact_new_Run_owned_fixture_root_required");
    struct Owned {HANDLE value{};~Owned(){if(value)CloseHandle(value);}} process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,identity.pid)};
    if(!process.value||WaitForSingleObject(process.value,0)!=WAIT_TIMEOUT)
        return reject("fixture_process_not_live");
    Owned token,own_token;
    if(!OpenProcessToken(process.value,TOKEN_QUERY,&token.value)||!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&own_token.value))
        return reject("fixture_token_unknown");
    alignas(TOKEN_USER) std::array<BYTE,512> user{},own_user{};
    alignas(TOKEN_MANDATORY_LABEL) std::array<BYTE,512> label{};
    DWORD bytes{},session{},own_session{},ui{},app{};TOKEN_ELEVATION elevation{};
    if(!GetTokenInformation(token.value,TokenUser,user.data(),static_cast<DWORD>(user.size()),&bytes)||
       !GetTokenInformation(own_token.value,TokenUser,own_user.data(),static_cast<DWORD>(own_user.size()),&bytes)||
       !IsValidSid(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid)||
       !IsValidSid(reinterpret_cast<TOKEN_USER*>(own_user.data())->User.Sid)||
       !EqualSid(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid,reinterpret_cast<TOKEN_USER*>(own_user.data())->User.Sid)||
       !GetTokenInformation(token.value,TokenIntegrityLevel,label.data(),static_cast<DWORD>(label.size()),&bytes)||
       !GetTokenInformation(token.value,TokenElevation,&elevation,sizeof(elevation),&bytes)||elevation.TokenIsElevated||
       !GetTokenInformation(token.value,TokenSessionId,&session,sizeof(session),&bytes)||!session||
       !ProcessIdToSessionId(GetCurrentProcessId(),&own_session)||session!=own_session||
       !GetTokenInformation(token.value,TokenUIAccess,&ui,sizeof(ui),&bytes)||ui||
       !GetTokenInformation(token.value,TokenIsAppContainer,&app,sizeof(app),&bytes)||app)
        return reject("fixture_same_SID_session_nonElevated_security_required");
    const auto sid=reinterpret_cast<TOKEN_MANDATORY_LABEL*>(label.data())->Label.Sid;
    if(!IsValidSid(sid)||!*GetSidSubAuthorityCount(sid)||
       *GetSidSubAuthority(sid,*GetSidSubAuthorityCount(sid)-1)!=SECURITY_MANDATORY_MEDIUM_RID)
        return reject("fixture_exact_Medium_required");
    std::array<wchar_t,128> desktop{};DWORD needed{};
    if(!GetUserObjectInformationW(GetThreadDesktop(identity.tid),UOI_NAME,desktop.data(),sizeof(desktop),&needed)||
       std::wstring_view(desktop.data())!=L"Default")return reject("fixture_actual_Default_thread_desktop_required");
    std::array<wchar_t,32768> image{};DWORD length=static_cast<DWORD>(image.size());
    if(!QueryFullProcessImageNameW(process.value,0,image.data(),&length)||!length||length>=image.size())
        return reject("fixture_actual_image_unknown");
    const std::filesystem::path actual=image.data();
    const bool debug=guest::same_exact_directory(actual.parent_path(),input/L"Debug");
    if((!debug&&!guest::same_exact_directory(actual.parent_path(),input))||
       actual.filename()!=L"panebind-v1-guest-fixture.exe"||_wcsicmp(identity.exe.c_str(),actual.c_str())||
       !guest::capture_plain_directory(actual.parent_path()))return reject("fixture_image_outside_exact_Input_configuration");
    const auto attrs=GetFileAttributesW(actual.c_str());std::error_code ec;
    if(attrs==INVALID_FILE_ATTRIBUTES||(attrs&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
       std::filesystem::canonical(actual,ec)!=actual||ec)return reject("fixture_image_not_plain_exact_file");
    const auto driver_hash=guest::capture_hash(actual);
    if(driver_hash.empty()||driver_hash!=guest::capture_field(std::string_view(manifest.data(),files),debug?"driver_debug_sha256":"driver_release_sha256"))
        return reject("fixture_actual_driver_manifest_hash_mismatch");
    // Re-read actual lifetime/context after all expensive proof, before the
    // immutable permit is published. A real UP during hashing cannot arm it.
    if(!capture_source_current(identity)||GetForegroundWindow()!=source||!guest::capture_interactive_medium()||
       !capture_bridge_->active(pair)||pair!=capture_bridge_->pair.load()||!allowed(generation)||
       pair->up_observed||pair->aborted||context->matching_up_observed||
       !operations::gesture_shield_exact_capture(context->overlay,context->owner_tid))
        return reject("source_or_generation_revoked_during_proof");
    unsigned empty=0;if(!state.compare_exchange_strong(empty,1))
        return reject("one_process_lifetime_permit_already_used_or_armed");
    try{permit={true,generation,identity,std::string(run_id),driver_hash};}
    catch(...){why="immutable_permit_publication_failed";return false;}
    state.store(2,std::memory_order_release);why="exact_guest_one_shot_armed_not_yet_triggered";return true;
}
bool InputService::request_guest_callback_owner_stop(std::string_view run_id,std::uint64_t generation,HWND source,std::string& why){
    if(!arm_guest_input_failure(run_id,generation,source,true,why))return false;
    if(!thread_.joinable()||!thread_id_.load()){why="actual_input_owner_handle_unavailable";return false;}
    guest_callback_wait_thread_=thread_.native_handle();guest_callback_wait_context_=std::make_unique<GuestCallbackWaitContext>();
    guest_callback_wait_context_->owner=owner_;
    SetLastError(0);if(!RegisterWaitForSingleObject(&guest_callback_wait_,guest_callback_wait_thread_,guest_callback_owner_exited,
        guest_callback_wait_context_.get(),INFINITE,WT_EXECUTEONLYONCE)){why="one_shot_thread_wait_registration_failed";
        guest_callback_wait_register_error_=GetLastError();guest_callback_wait_thread_=nullptr;guest_callback_wait_context_.reset();return false;}
    guest_callback_wait_registered_=true;SetLastError(0);
    const bool posted=PostThreadMessageW(thread_id_.load(),guest_callback_stop_message,static_cast<WPARAM>(generation),0)!=FALSE;
    guest_callback_stop_post_error_=posted?0:GetLastError();guest_callback_stop_posted_=posted;
    why=posted?"exact_guest_callback_owner_stop_posted":"callback_owner_stop_PostThreadMessage_failed";return posted;
}
VOID CALLBACK InputService::guest_callback_owner_exited(PVOID value,BOOLEAN timed_out){
    auto* context=static_cast<GuestCallbackWaitContext*>(value);if(!context||timed_out)return;
    context->signaled.store(true,std::memory_order_release);
    if(const auto owner=context->owner.load())context->notified=PostMessageW(owner,catalog_message,4,0)!=FALSE;
}
bool InputService::guest_callback_owner_stop_clean()const{
    if(!guest_callback_stop_triggered_.load())return true;
    if(!guest_callback_stop_exit_ready_.load(std::memory_order_acquire)||!guest_callback_gui_exit_observed_||
       (guest_callback_wait_unregister_attempted_&&!guest_callback_wait_unregister_return_))return false;
    const auto& f=guest_callback_stop_observation_;
    if(!f.unhook_attempted||!f.unhook_return||!f.hotkey_attempted||!f.hotkey_return||!f.raw_attempted||!f.raw_return||
       !f.receiver_attempted||!f.receiver_return||!f.shield_stop_clean)return false;
    for(std::size_t i=0;i<f.event_attempted.size();++i)if(!f.event_attempted[i]||!f.event_return[i])return false;
    return true;
}
std::string InputService::guest_callback_owner_stop_json(){
    const auto state=guest_callback_stop_state_.load(std::memory_order_acquire);
    const bool complete=guest_callback_stop_exit_ready_.load(std::memory_order_acquire);
    if(guest_callback_wait_context_){guest_callback_wait_signaled_=guest_callback_wait_context_->signaled.load();
        guest_callback_wait_notified_=guest_callback_wait_context_->notified.load();}
    if(complete&&guest_callback_wait_signaled_.load(std::memory_order_acquire)&&!guest_callback_gui_exit_observed_&&guest_callback_wait_thread_){
        SetLastError(0);guest_callback_gui_wait_result_=WaitForSingleObject(guest_callback_wait_thread_,0);
        guest_callback_gui_wait_error_=guest_callback_gui_wait_result_==WAIT_FAILED?GetLastError():0;
        DWORD code=STILL_ACTIVE;bool known=false;
        if(guest_callback_gui_wait_result_==WAIT_OBJECT_0){guest_callback_gui_exit_query_attempted_=true;SetLastError(0);
            known=GetExitCodeThread(guest_callback_wait_thread_,&code)!=FALSE;guest_callback_gui_exit_query_return_=known;
            guest_callback_gui_exit_query_error_=known?0:GetLastError();}
        guest_callback_gui_exit_code_=known?code:STILL_ACTIVE;
        guest_callback_gui_exit_observed_=guest_callback_gui_wait_result_==WAIT_OBJECT_0&&known&&code!=STILL_ACTIVE;
    }
    std::string out="\"stop_request_posted\":"+std::string(guest_callback_stop_posted_.load()?"true":"false")+
        ",\"stop_post_error\":"+std::to_string(guest_callback_stop_post_error_.load())+
        ",\"callback_stop_permit_armed\":"+(state==2?"true":"false")+",\"callback_stop_permit_consumed\":"+(state==3?"true":"false")+
        ",\"owner_stop_triggered\":"+(guest_callback_stop_triggered_.load()?"true":"false")+
        ",\"owner_stop_rejection_code\":"+std::to_string(guest_callback_stop_rejection_.load())+
        ",\"actual_owner_exit_recorded\":"+(complete?"true":"false")+
        ",\"thread_wait_registered\":"+(guest_callback_wait_registered_?"true":"false")+
        ",\"thread_wait_register_error\":"+std::to_string(guest_callback_wait_register_error_)+
        ",\"thread_object_signaled\":"+(guest_callback_wait_signaled_.load()?"true":"false")+
        ",\"thread_wait_result\":"+std::to_string(guest_callback_gui_wait_result_)+
        ",\"thread_wait_error\":"+std::to_string(guest_callback_gui_wait_error_)+
        ",\"thread_exit_code\":"+std::to_string(guest_callback_gui_exit_code_)+
        ",\"thread_exit_query_attempted\":"+(guest_callback_gui_exit_query_attempted_?"true":"false")+
        ",\"thread_exit_query_return\":"+(guest_callback_gui_exit_query_return_?"true":"false")+
        ",\"thread_exit_query_error\":"+std::to_string(guest_callback_gui_exit_query_error_)+
        ",\"thread_exit_observed\":"+(guest_callback_gui_exit_observed_?"true":"false")+
        ",\"thread_wait_unregister_attempted\":"+(guest_callback_wait_unregister_attempted_?"true":"false")+
        ",\"thread_wait_unregister_return\":"+(guest_callback_wait_unregister_return_?"true":"false")+
        ",\"thread_wait_unregister_error\":"+std::to_string(guest_callback_wait_unregister_error_)+
        ",\"origin\":\"guest_injected_callback_owner_stop\",\"OS_silent_hook_removal_proven\":false"+
        ",\"current_healthy\":"+(healthy_.load()?"true":"false")+",\"current_available\":"+(available_->load()?"true":"false")+
        ",\"current_authorized_generation\":"+std::to_string(authorized_->load())+",\"current_completion_generation\":"+std::to_string(completion_->load())+
        ",\"current_input_owner_tid\":"+std::to_string(thread_id_.load())+",\"actual_input_owner_start_serial\":"+std::to_string(input_owner_start_serial_.load());
    if(state>=2){const auto& p=guest_callback_stop_permit_;out+=",\"fault_run_id\":\""+p.run_id+"\",\"fault_generation\":"+std::to_string(p.generation)+
        ",\"fault_source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(p.source.hwnd))+",\"fault_source_key\":"+std::to_string(p.source.key)+
        ",\"fault_source_pid\":"+std::to_string(p.source.pid)+",\"fault_source_tid\":"+std::to_string(p.source.tid)+",\"fault_source_birth\":"+std::to_string(p.source.born)+
        ",\"fault_driver_sha256\":\""+p.driver_sha256+"\"";}
    if(guest_callback_stop_triggered_.load())out+=",\"callback_stop_trigger_qpc\":"+std::to_string(guest_callback_stop_observation_.trigger_qpc);
    if(complete){const auto& f=guest_callback_stop_observation_;out+=",\"actual_stopped_owner_tid\":"+std::to_string(f.owner_tid)+
        ",\"owner_exit_record_qpc\":"+std::to_string(f.exit_record_qpc)+",\"mouse_hook_HWND_not_claimed\":true,\"mouse_hook_handle\":"+std::to_string(f.hook)+
        ",\"mouse_unhook_attempted\":"+(f.unhook_attempted?"true":"false")+",\"mouse_unhook_return\":"+(f.unhook_return?"true":"false")+
        ",\"mouse_unhook_error\":"+std::to_string(f.unhook_error)+",\"mouse_unhook_before_qpc\":"+std::to_string(f.unhook_before_qpc)+
        ",\"mouse_unhook_after_qpc\":"+std::to_string(f.unhook_after_qpc)+
        ",\"hotkey_unregister_return\":"+(f.hotkey_return?"true":"false")+",\"hotkey_unregister_error\":"+std::to_string(f.hotkey_error)+
        ",\"raw_unregister_return\":"+(f.raw_return?"true":"false")+",\"raw_unregister_error\":"+std::to_string(f.raw_error)+
        ",\"receiver_destroy_return\":"+(f.receiver_return?"true":"false")+",\"receiver_destroy_error\":"+std::to_string(f.receiver_error)+
        ",\"shield_stop_clean\":"+(f.shield_stop_clean?"true":"false")+",\"event_unhook\":[";
        for(std::size_t i=0;i<f.event_attempted.size();++i){if(i)out+=',';out+="{\"index\":"+std::to_string(i)+",\"attempted\":"+(f.event_attempted[i]?"true":"false")+
            ",\"native_return\":"+(f.event_return[i]?"true":"false")+",\"error\":"+std::to_string(f.event_error[i])+"}";}out+=']';}
    out+=",\"callback_stop_cleanup_confirmed\":"+std::string(guest_callback_stop_triggered_.load()&&guest_callback_owner_stop_clean()?"true":"false");return out;
}
std::string InputService::guest_queue_failure_json()const{
    const auto state=guest_queue_failure_state_.load(std::memory_order_acquire);
    const bool fired=guest_queue_failure_observation_ready_.load(std::memory_order_acquire);
    std::string fields="\"permit_armed\":"+std::string(state==2?"true":"false")+",\"permit_consumed\":"+(state==3?"true":"false")+
        ",\"failure_fired\":"+(fired?"true":"false")+",\"scope\":\"exact_disposable_guest_owned_only\",\"natural_queue_full_proven\":false,\"OS_silent_hook_removal_proven\":false";
    fields+=",\"current_healthy\":"+std::string(healthy_.load()?"true":"false")+",\"current_available\":"+(available_->load()?"true":"false")+
        ",\"current_authorized_generation\":"+std::to_string(authorized_->load())+",\"current_completion_generation\":"+std::to_string(completion_->load())+
        ",\"current_input_owner_tid\":"+std::to_string(thread_id_.load())+",\"actual_input_owner_start_serial\":"+std::to_string(input_owner_start_serial_.load(std::memory_order_acquire))+
        ",\"health_fields_not_OS_hook_presence_query\":true";
    if(state>=2){const auto& p=guest_queue_failure_permit_;fields+=",\"fault_run_id\":\""+p.run_id+"\",\"fault_generation\":"+std::to_string(p.generation)+
        ",\"fault_source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(p.source.hwnd))+",\"fault_source_key\":"+std::to_string(p.source.key)+
        ",\"fault_source_pid\":"+std::to_string(p.source.pid)+",\"fault_source_tid\":"+std::to_string(p.source.tid)+",\"fault_source_birth\":"+std::to_string(p.source.born)+
        ",\"fault_driver_sha256\":\""+p.driver_sha256+"\"";}
    if(fired){const auto& f=guest_queue_failure_observation_;fields+=",\"origin\":\"guest_injected_failure\",\"kind\":\"Move\",\"trigger_qpc\":"+
        std::to_string(f.qpc)+",\"actual_full\":"+(f.actual_full?"true":"false")+",\"read_index\":"+std::to_string(f.read)+
        ",\"write_index\":"+std::to_string(f.write)+",\"next_index\":"+std::to_string(f.next)+",\"same_production_full_failure_branch\":true";}
    return fields;
}
bool InputService::push(InputEvent e){
    const auto w=write_.load(std::memory_order_relaxed);
    const auto next=(w+1)%static_cast<unsigned>(queue_.size());
    const auto r=read_.load(std::memory_order_acquire);const bool actual_full=next==r;bool injected=false;std::uint64_t injected_qpc{};
    if(guest_queue_failure_state_.load(std::memory_order_acquire)==2){
        const auto pair=capture_bridge_?capture_bridge_->pair.load():nullptr;
        const auto context=pair?pair->context.load():GestureCaptureLease{};
        if(guest_queue_failure_matches(guest_queue_failure_permit_,e,context?&context->source:nullptr,authorized_->load(),
            healthy_.load(),available_->load(),paused_.load(),pair&&capture_bridge_->active(pair),
            pair&&pair->ready_authorized&&context, !pair||pair->up_observed||pair->aborted||(context&&context->matching_up_observed))){
            injected_qpc=qpc_now();unsigned armed=2;auto authority=e.generation;
            if(injected_qpc&&guest_queue_failure_state_.compare_exchange_strong(armed,3)&&authorized_->compare_exchange_strong(authority,0))
                injected=!pair->up_observed&&!pair->aborted&&!context->matching_up_observed;
        }
    }
    if(actual_full||injected){*completion_=0;*available_=false;*authorized_=0;healthy_=false;
        if((consumed_down_||native_move_)&&!aborted_){aborted_=true;active_end_=core::v1::End::QueueFailure;}
        request_capture_stop(Removal::ContextLost);
        if(injected){const auto& source=guest_queue_failure_permit_.source;
            guest_queue_failure_observation_={e.generation,injected_qpc,source.hwnd,source.pid,source.tid,r,w,static_cast<unsigned>(next),actual_full};
            guest_queue_failure_observation_ready_.store(true,std::memory_order_release);}
        PostMessageW(owner_,catalog_message,1,0);return false;}
    queue_[w]=std::move(e);write_.store(static_cast<unsigned>(next),std::memory_order_release);
    if(!PostMessageW(owner_,input_message,0,0)){*completion_=0;*available_=false;*authorized_=0;healthy_=false;
        if((consumed_down_||native_move_)&&!aborted_){aborted_=true;active_end_=core::v1::End::QueueFailure;}
        request_capture_stop(Removal::ContextLost);
        return false;}return true;
}
bool InputService::pop(InputEvent& e){const auto r=read_.load(std::memory_order_relaxed);
    if(r==write_.load(std::memory_order_acquire))return false;e=std::move(queue_[r]);
    read_.store((r+1)%static_cast<unsigned>(queue_.size()),std::memory_order_release);return true;}
void InputService::abort(core::v1::End end,std::optional<InputAbortObservation> observation){
    captured_move_.retire();
    *completion_=0;*authorized_=0;
    if((consumed_down_||native_move_)&&!aborted_){aborted_=true;active_end_=end;
        InputEvent event{InputKind::Abort,latest(),generation_,GetTickCount64(),modifiers_,button_,1,end,active_prepared_,active_action_};
        event.abort_observation=observation;push(std::move(event));}
    if(capture_bridge_)if(auto p=capture_bridge_->pair.load();p&&p->down.generation==generation_){
        p->aborted=true;if(p->native_move)p->native_phase=NativeMovePhase::Retired;
        if(p->native_move&&!p->resource_requested){
            KillTimer(receiver_,gesture_deadline_timer);native_move_=false;capture_pair_=false;
            active_prepared_.reset();prepared_.store({});
            if(!prepare_pending_.exchange(true))PostMessageW(owner_,prepare_message,0,0);
        }
    }
    request_capture_stop(end==core::v1::End::Deadline?Removal::Deadline:
        (end==core::v1::End::Exit?Removal::Shutdown:(end==core::v1::End::Paused?Removal::ExplicitStop:Removal::ContextLost)));
    // Retain the consumed-button pair until the actual matching UP; no synthetic
    // UP, foreign capture or foreground recovery. Other buttons always pass.
}
LRESULT CALLBACK InputService::mouse(int code,WPARAM message,LPARAM param){
    auto* self=instance_;if(code<0||!self)return CallNextHookEx(nullptr,code,message,param);
    LARGE_INTEGER before{},after{};QueryPerformanceCounter(&before);
    const auto observed_qpc=before.QuadPart>0?static_cast<std::uint64_t>(before.QuadPart):0;
    const auto& packet=*reinterpret_cast<MSLLHOOKSTRUCT*>(param);
    const auto tick=GetTickCount64();
    const bool observed_up=up(message);MouseUpCallbackSnapshot up_observation;
    std::optional<ButtonAdmission> admission;
    const auto admission_state=[&]()->ButtonAdmissionState{
        return {self->generation_,self->authorized_->load(),self->completion_->load(),
            self->consumed_down_,self->native_move_,self->capture_pair_,self->paused_.load(),
            self->healthy_.load(),self->available_->load(),self->aborted_,self->stop_requested_.load()};
    };
    if(self->button_diagnostic_&&(down(message)||observed_up)){
        auto& facts=admission.emplace();facts.sequence=++self->button_admission_sequence_;
        facts.hook_qpc=observed_qpc;facts.tick=tick;facts.message=static_cast<UINT>(message);
        facts.native_time=packet.time;facts.flags=packet.flags;facts.mouse_data=packet.mouseData;
        facts.point=packet.pt;facts.button=button(message,packet.mouseData);facts.down=down(message);
        facts.before=admission_state();facts.active_prepared=button_prepared_facts(self->active_prepared_.get());
    }
    // Called only in the original short-circuit positions below. A missing
    // foreground sample means that branch did not query it, not NULL/allowed.
    const auto observed_foreground=[&]{const auto actual=GetForegroundWindow();
        if(admission){admission->foreground_read=true;admission->foreground=actual;}return actual;};
    if(observed_up){
        up_observation.callback_count=++self->up_callback_count_;up_observation.hook_observed_qpc=observed_qpc;
        up_observation.button=button(message,packet.mouseData);up_observation.generation_at_entry=self->generation_;
        up_observation.consumed_before=self->consumed_down_;up_observation.aborted_before=self->aborted_;
    }
    // Any newly observed physical/test DOWN retires an earlier UP's pending
    // discrete dock commit, even if this new click passes through natively.
    if(down(message)){
        self->captured_move_.retire();
        *self->completion_=0;
        if(self->capture_bridge_)self->capture_bridge_->down_serial.fetch_add(1);
        if(self->capture_pair_){
            // A second actual DOWN cannot inherit or complete the older pair.
            *self->authorized_=0;if(auto p=self->capture_bridge_->pair.load())p->aborted=true;
            PostThreadMessageW(self->thread_id_,WM_APP+2,static_cast<WPARAM>(self->generation_),static_cast<LPARAM>(core::v1::End::SourceLost));
        }
        if(!self->capture_pair_&&retire_revoked_pair_for_down(self->consumed_down_,self->authorized_->load(),
            self->button_,button(message,packet.mouseData),self->active_down_stamp_,packet.time)){
            // The old write generation stays revoked. Retire its input pair
            // BEFORE considering current preparation, so an unprepared/native
            // new DOWN cannot have its real UP stolen by the aborted pair.
            KillTimer(self->receiver_,gesture_deadline_timer);self->consumed_down_=false;
            self->active_prepared_.reset();self->active_action_=core::v1::Action::None;
            self->click_candidate_={};
        }
        // An overlapping press is not a completed first click. Never revive a
        // candidate by returning to its anchor after a drag or another DOWN.
        if(self->click_candidate_.pressing)self->click_candidate_={};
    }
    if(message==WM_MOUSEMOVE)self->click_candidate_.motion(packet.pt,self->drag_x_,self->drag_y_);
    if(up(message)&&native_event_is_current(packet.time,self->last_down_))
        self->click_candidate_.release(button(message,packet.mouseData));
    self->point_=(static_cast<std::uint64_t>(static_cast<std::uint32_t>(packet.pt.x))<<32)|static_cast<std::uint32_t>(packet.pt.y);
    if((self->consumed_down_||self->native_move_)&&!self->aborted_&&self->stop_requested_)self->abort(core::v1::End::Exit);
    else if((self->consumed_down_||self->native_move_)&&!self->aborted_&&self->paused_)self->abort(core::v1::End::Paused);
    if((self->consumed_down_||self->native_move_)&&!self->aborted_&&tick>=self->deadline_)self->abort(core::v1::End::Deadline);
    bool consume=false;
    if(message==WM_MOUSEMOVE){
        if(self->consumed_down_||self->native_move_){
            if(self->capture_pair_&&!self->captured_move_.direct_move_allowed(self->generation_)){
                if(!self->aborted_&&self->active_prepared_)
                    self->captured_move_.observe(self->generation_,{packet.pt,tick,observed_qpc,packet.time},
                        self->allowed(self->generation_));
            }else if(self->allowed(self->generation_)){
                const auto& p=self->active_prepared_;
                if(!p||(!self->capture_pair_&&GetForegroundWindow()!=p->source.hwnd))self->abort(core::v1::End::FocusLost);
                else self->push({InputKind::Move,packet.pt,self->generation_,tick,self->modifiers_,self->button_,1,
                    self->active_end_,p,self->active_action_,observed_qpc});
            }
            // Observe motion, but let Windows update/route the real cursor.
            // A consumed DOWN did not start native Move/Resize. Native-first
            // leaves every early MOVE to Windows and publishes motion only
            // after real END and CaptureReady. Filtering every MOVE can
            // suppress the system cursor itself; it is not needed to prevent
            // a second native geometry writer. The consumed button's actual
            // UP is still paired below, with the existing finite deadline.
            consume=false;
        }
        // Waiting for a revoked pair's UP must not freeze hover preparation.
        // The GUI still refuses to prepare while its gesture is active; this
        // event-driven request grants no write authority and starts no timer.
        if(motion_requests_preparation(self->consumed_down_||self->native_move_,self->authorized_->load())&&
            !self->prepare_pending_.exchange(true))PostMessageW(self->owner_,prepare_message,0,0);
    }else if(down(message)&&!self->consumed_down_&&!self->native_move_&&!self->paused_&&self->healthy_&&self->available_->load()){
        const auto bundle=self->prepared_.load();const auto p=bundle?bundle->prepared:nullptr;
        const auto mods=modifiers();const auto b=button(message,packet.mouseData);
        if(admission){auto& facts=*admission;facts.down_branch_entered=true;facts.cache_read=true;
            facts.prepared=button_prepared_facts(p.get());facts.modifiers=mods;
            facts.candidate_eligible=self->click_candidate_.eligible;facts.candidate_pressing=self->click_candidate_.pressing;
            facts.candidate_anchor=self->click_candidate_.anchor;facts.previous_down_time=self->last_down_;
            facts.previous_region=self->last_region_;facts.previous_button=self->last_button_;
            facts.previous_modifiers=self->last_modifiers_;facts.previous_source_key=self->last_source_key_;
            facts.double_click_time=self->double_click_time_;}
        // Incomplete/stale preparation passes through. Never hit-test/COM/wait in
        // this callback, and never turn an inactive-window click into activation.
        // This exact-point preparation is invalidated/rebuilt by native
        // geometry/lifecycle/foreground events, not an arbitrary hover timeout.
        // The GUI still revalidates identity and geometry before ANY write.
        if(p&&p->region>=0&&p->point.x==packet.pt.x&&p->point.y==packet.pt.y&&
            observed_foreground()==p->source.hwnd){
            if(admission)admission->prepared_gate_passed=true;
            const auto difference_x=static_cast<std::int64_t>(packet.pt.x)-self->click_candidate_.anchor.x;
            const auto difference_y=static_cast<std::int64_t>(packet.pt.y)-self->click_candidate_.anchor.y;
            const bool double_click=self->click_candidate_.eligible&&
                static_cast<DWORD>(packet.time-self->last_down_)<=self->double_click_time_&&
                difference_x>=-self->double_click_x_&&difference_x<=self->double_click_x_&&
                difference_y>=-self->double_click_y_&&difference_y<=self->double_click_y_&&
                self->last_region_==p->region&&self->last_button_==b&&self->last_modifiers_==mods&&
                self->last_source_key_==p->source.key;
            self->last_down_=packet.time;self->last_region_=p->region;
            self->last_button_=b;self->last_modifiers_=mods;self->last_source_key_=p->source.key;
            self->click_candidate_.begin(packet.pt,b,!double_click);
            const int clicks=double_click?2:1;
            const auto route=route_down(*p,b,clicks,mods);
            if(admission){admission->route_evaluated=true;admission->route=route.route;
                admission->action=route.action;admission->clicks=static_cast<unsigned>(clicks);}
            if(route.route!=DownRoute::Pass&&self->next_generation_<(pending_capture_mask-1)){
                bool routed=false;
                self->generation_=++self->next_generation_;
                self->capture_pair_=self->capture_route_enabled_&&route.route==DownRoute::Gesture&&b==1;
                self->native_move_=native_geometry_entry(self->capture_route_enabled_,*p,b,clicks,mods,route);
                self->captured_move_.begin(self->generation_,self->capture_pair_,!self->native_move_);
                *self->authorized_=self->generation_|(self->capture_pair_?pending_capture_mask:0);
                self->consumed_down_=!self->native_move_;self->aborted_=false;self->button_=b;self->modifiers_=mods;
                self->active_down_stamp_=packet.time;
                self->active_prepared_=p;self->active_action_=route.action;self->active_end_=core::v1::End::NormalUp;
                self->deadline_=tick+gesture_deadline_ms;
                if(self->capture_pair_){
                    InputEvent e{InputKind::Down,packet.pt,self->generation_,tick,mods,b,clicks,core::v1::End::NormalUp,p,route.action,observed_qpc};
                    e.native_message_time=packet.time;
                    bool pair_prepared=true;
                    if(self->native_move_){
                        e.kind=InputKind::NativeMovePending;
                        const auto pair=bundle->native_pair;bool unassigned=false;
                        pair_prepared=pair&&!self->capture_bridge_->retired_pair.load()&&
                            pair->down_assigned.compare_exchange_strong(unassigned,true);
                        if(pair_prepared){
                            // Allocated with this exact immutable Prepared on
                            // the GUI owner. The LL callback only fills/publishes
                            // this once; it does not allocate a Pair or wait.
                            pair->native_move=true;pair->down=e;
                            pair->down_serial=self->capture_bridge_->down_serial.load();pair->deadline=self->deadline_;
                            pair->request.gesture={e.generation,p->source.hwnd,p->source.pid,p->source.tid,gesture_deadline_ms};
                            pair->request.gesture.external_stop={GetCurrentProcessId(),self->thread_id_.load(),910};
                            auto previous=self->capture_bridge_->pair.exchange(pair);
                            if(previous)self->capture_bridge_->retired_pair.store(std::move(previous));
                        }
                    }
                    const bool published=pair_prepared&&(!self->native_move_||self->push(e));
                    self->capture_pending_down_=std::move(e);
                    const bool posted=published&&PostThreadMessageW(self->thread_id_,capture_request_message,static_cast<WPARAM>(self->generation_),0)!=FALSE;
                    routed=posted;
                    consume=posted&&!self->native_move_;
                }else if(SetTimer(self->receiver_,gesture_deadline_timer,gesture_deadline_ms,nullptr))
                    routed=consume=self->push({route.route==DownRoute::Mapped?InputKind::Mapped:InputKind::Down,
                        packet.pt,self->generation_,tick,mods,b,clicks,core::v1::End::NormalUp,p,route.action,observed_qpc});
                if(!routed){self->captured_move_.retire();self->consumed_down_=false;self->native_move_=false;self->capture_pair_=false;self->capture_pending_down_.reset();*self->authorized_=0;self->active_prepared_.reset();
                    KillTimer(self->receiver_,gesture_deadline_timer);}
                if(admission)admission->routed=routed;
            }
        }else self->click_candidate_={};
    }else if(up(message)&&((self->capture_pair_&&(self->consumed_down_||self->native_move_)&&button(message,packet.mouseData)==self->button_)||
        button_up_matches_current_pair(self->consumed_down_,self->button_,
        button(message,packet.mouseData),self->active_down_stamp_,packet.time))){
        up_observation.matching_branch_taken=true;
        self->captured_move_.retire();
        if(self->capture_pair_){
            // Same-owner actual UP revokes immediately, including coarse-time
            // ambiguity. It passes through to capture; only queued legacy UP
            // can later establish NormalUp. No native query or fake UP here.
            *self->authorized_=0;
            if(auto p=self->capture_bridge_->pair.load();p&&p->down.generation==self->generation_){
                if(auto context=p->context.load())context->matching_up_observed.store(true,std::memory_order_release);
                p->up_observed=true;
                if(p->native_move&&!p->ready_authorized){
                    p->aborted=true;p->native_phase=NativeMovePhase::Retired;
                    auto released=p->down;released.kind=InputKind::NativeMoveReleased;released.point=packet.pt;
                    released.native_message_time=packet.time;released.hook_observed_qpc=observed_qpc;
                    // This actual native UP grants no normalized NormalUp or
                    // late writer permission. A short native drag stays native.
                    const bool release_pushed=self->push(std::move(released));
                    if(admission){admission->native_release_push_attempted=true;admission->native_release_push_succeeded=release_pushed;}
                    if(p->resource_requested)self->abort(core::v1::End::SourceLost);
                    else {KillTimer(self->receiver_,gesture_deadline_timer);self->native_move_=false;
                        self->capture_pair_=false;self->active_prepared_.reset();self->prepared_.store({});
                        if(!self->prepare_pending_.exchange(true))PostMessageW(self->owner_,prepare_message,0,0);}
                }
            }
        }else{
        const auto& prepared=self->active_prepared_;DWORD source_pid{};
        const DWORD source_tid=prepared?GetWindowThreadProcessId(prepared->source.hwnd,&source_pid):0;
        const bool source_current=prepared&&self->generation_&&self->authorized_->load()==self->generation_&&
            prepared->source.key&&prepared->source.hwnd&&IsWindow(prepared->source.hwnd)&&
            GetAncestor(prepared->source.hwnd,GA_ROOT)==prepared->source.hwnd&&
            source_tid==prepared->source.tid&&source_pid==prepared->source.pid;
        self->active_end_=matching_up_end(self->aborted_,self->active_end_,source_current,
            prepared&&observed_foreground()==prepared->source.hwnd);
        if(self->active_end_!=core::v1::End::NormalUp)self->aborted_=true;
        *self->completion_=!self->aborted_&&self->active_end_==core::v1::End::NormalUp?self->generation_:0;
        *self->authorized_=0;consume=true;KillTimer(self->receiver_,gesture_deadline_timer);
        up_observation.push_attempted=true;
        up_observation.push_succeeded=self->push({InputKind::Up,packet.pt,self->generation_,tick,self->modifiers_,self->button_,1,
            self->active_end_,self->active_prepared_,self->active_action_,observed_qpc});
        self->consumed_down_=false;self->active_prepared_.reset();
        if(!self->prepare_pending_.exchange(true))PostMessageW(self->owner_,prepare_message,0,0);
        }
    }
    if(observed_up)self->up_observation_.publish(up_observation);
    if(admission){auto& facts=*admission;facts.after=admission_state();facts.consume=consume;
        facts.matching_up=up_observation.matching_branch_taken;facts.up_push_attempted=up_observation.push_attempted;
        facts.up_push_succeeded=up_observation.push_succeeded;
        if(self->button_admissions_.push(true,facts)&&!PostMessageW(self->owner_,input_message,0,0))
            self->button_admission_wake_failures_.fetch_add(1,std::memory_order_relaxed);
    }
    QueryPerformanceCounter(&after);
    const auto finished_qpc=after.QuadPart>0?static_cast<std::uint64_t>(after.QuadPart):0;
    self->callback_timing_.observe_qpc(observed_qpc,finished_qpc,self->counter_frequency_);
    if(observed_qpc&&finished_qpc>=observed_qpc){
        const auto duration=finished_qpc-observed_qpc;
        // This hook owner is the sole writer; no unbounded CAS retry belongs
        // in the callback. Downstream CallNextHookEx time is excluded.
        if(duration>self->max_ticks_.load(std::memory_order_relaxed))self->max_ticks_.store(duration,std::memory_order_relaxed);
    }
    return consume?1:CallNextHookEx(nullptr,code,message,param);
}
void CALLBACK InputService::event(HWINEVENTHOOK,DWORD ev,HWND hwnd,LONG object,LONG child,DWORD event_tid,DWORD stamp){
    auto* s=instance_;if(!s)return;
    if(s->native_move_&&s->capture_bridge_&&s->active_prepared_&&
       (ev==EVENT_SYSTEM_MOVESIZESTART||ev==EVENT_SYSTEM_MOVESIZEEND)){
        const auto pair=s->capture_bridge_->pair.load();
        NativeMoveReceipt receipt{s->generation_,++s->native_receipt_sequence_,ev,event_tid,stamp,hwnd,object,child};
        if(pair&&pair->native_move&&native_move_receipt_matches(receipt,*s->active_prepared_,s->generation_,s->active_down_stamp_,ev)){
            try{
            auto actual=std::make_shared<const NativeMoveReceipt>(receipt);bool accepted=false;
            if(ev==EVENT_SYSTEM_MOVESIZESTART){
                std::shared_ptr<const NativeMoveReceipt> absent;
                accepted=pair->native_start.compare_exchange_strong(absent,actual);
            }else if(const auto start=pair->native_start.load();start&&native_move_end_follows(*start,receipt)){
                std::shared_ptr<const NativeMoveReceipt> absent;
                accepted=pair->native_end.compare_exchange_strong(absent,actual);
            }
            if(accepted){auto e=pair->down;e.kind=ev==EVENT_SYSTEM_MOVESIZESTART?InputKind::NativeMoveStart:InputKind::NativeMoveEnd;
                e.origin=InputOrigin::NativeMoveEvent;e.native_move_receipt=std::move(actual);e.resource_observed_qpc=qpc_now();
                s->push(std::move(e));}
            else s->abort(core::v1::End::SourceLost); // duplicate/mismatched lifecycle is not another cancel permit
            }catch(...){
                // No exception may escape this Win32 ABI. Do not call a
                // potentially allocating notification path from the catch.
                *s->authorized_=0;*s->completion_=0;*s->available_=false;s->healthy_=false;
                pair->aborted=true;pair->native_phase=NativeMovePhase::Retired;s->capture_bridge_->failed=true;
                PostThreadMessageW(s->thread_id_,WM_APP+2,static_cast<WPARAM>(s->generation_),static_cast<LPARAM>(core::v1::End::QueueFailure));
                PostThreadMessageW(s->thread_id_,capture_stop_message,static_cast<WPARAM>(s->generation_),static_cast<LPARAM>(Removal::ContextLost));
                PostMessageW(s->owner_,catalog_message,1,0);
            }
        }
    }
    if((s->consumed_down_||s->native_move_)&&native_event_is_current(stamp,s->active_down_stamp_)){const auto& p=s->active_prepared_;
        const auto abort_focus_lost=[&](InputAbortOrigin origin){
            std::optional<InputAbortObservation> observation;
            if(!s->aborted_&&p){
                // The existing predicate has already decided to abort. Revoke
                // before optional observations: GUI queries must not extend a
                // known-invalid generation's native-write admission window.
                s->captured_move_.retire();*s->completion_=0;*s->authorized_=0;
                observation.emplace();auto& facts=*observation;
                facts.origin=origin;facts.trigger_event=ev;facts.trigger_hwnd=hwnd;
                facts.native_time=stamp;facts.observed_qpc=qpc_now();facts.foreground=GetForegroundWindow();
                DWORD pid{};facts.source_identity_current=GetWindowThreadProcessId(p->source.hwnd,&pid)==p->source.tid&&pid==p->source.pid;
                if(facts.source_identity_current){
                    GUITHREADINFO gui{sizeof(gui)};facts.source_gui_attempted=true;SetLastError(ERROR_SUCCESS);
                    facts.source_gui_known=GetGUIThreadInfo(p->source.tid,&gui)!=FALSE;
                    facts.source_gui_error=facts.source_gui_known?ERROR_SUCCESS:GetLastError();
                    if(facts.source_gui_known){facts.source_capture=gui.hwndCapture;facts.source_move_size=gui.hwndMoveSize;
                        facts.source_menu_owner=gui.hwndMenuOwner;facts.source_gui_flags=gui.flags;}
                }
                const auto capture=s->capture_context(s->generation_);
                if(capture&&capture->generation==s->generation_&&capture->owner_pid==GetCurrentProcessId()&&
                   capture->owner_tid&&Catalog::same(capture->source,p->source)&&
                   GetWindowThreadProcessId(capture->overlay,&pid)==capture->owner_tid&&pid==capture->owner_pid){
                    GUITHREADINFO gui{sizeof(gui)};facts.owner_thread_id=capture->owner_tid;facts.owner_gui_attempted=true;SetLastError(ERROR_SUCCESS);
                    facts.owner_gui_known=GetGUIThreadInfo(capture->owner_tid,&gui)!=FALSE;
                    facts.owner_gui_error=facts.owner_gui_known?ERROR_SUCCESS:GetLastError();
                    if(facts.owner_gui_known)facts.owner_capture=gui.hwndCapture;
                }
            }
            s->abort(core::v1::End::FocusLost,observation);
        };
        if(ev==EVENT_SYSTEM_FOREGROUND&&p&&hwnd!=p->source.hwnd&&GetForegroundWindow()!=p->source.hwnd)
            abort_focus_lost(InputAbortOrigin::WinEventForeground);
        if(ev==EVENT_OBJECT_DESTROY&&p&&hwnd==p->source.hwnd&&object==OBJID_WINDOW&&child==CHILDID_SELF)s->abort(core::v1::End::SourceLost);
        // This production route never takes capture. An observed new capture
        // is an invalidating native interaction, not permission to reclaim it.
        const auto lease=s->capture_context(s->generation_);
        const bool own_capture=lease&&hwnd==lease->overlay;
        const auto pair=s->capture_bridge_?s->capture_bridge_->pair.load():nullptr;
        const bool native_source_capture=s->native_move_&&pair&&pair->native_move&&!pair->native_end.load()&&
            p&&hwnd==p->source.hwnd;
        if(ev==EVENT_SYSTEM_CAPTURESTART&&p&&hwnd&&!own_capture&&
            !native_source_capture&&
            (GetWindowThreadProcessId(hwnd,nullptr)==p->source.tid||GetAncestor(hwnd,GA_ROOT)==p->source.hwnd))
            abort_focus_lost(InputAbortOrigin::WinEventCaptureStart);
    }
    if((object==OBJID_WINDOW&&child==CHILDID_SELF)||ev==EVENT_SYSTEM_FOREGROUND)PostMessageW(s->owner_,catalog_message,0,0);
}
LRESULT CALLBACK InputService::receiver(HWND w,UINT m,WPARAM wp,LPARAM lp){
    if(m==WM_TIMER&&wp==gesture_deadline_timer&&instance_){
        // A previously queued timer message cannot terminate a newer gesture.
        if((instance_->consumed_down_||instance_->native_move_)&&GetTickCount64()>=instance_->deadline_){
            KillTimer(w,gesture_deadline_timer);instance_->abort(core::v1::End::Deadline);
            if(!instance_->capture_pair_){instance_->consumed_down_=false;instance_->native_move_=false;instance_->active_prepared_.reset();}
            instance_->prepared_.store({});}
        else if(!instance_->consumed_down_&&!instance_->native_move_)KillTimer(w,gesture_deadline_timer);
        return 0;
    }
    if(m==WM_INPUT&&instance_){RAWINPUT raw{};UINT size=sizeof(raw);
        if(GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),RID_INPUT,&raw,&size,sizeof(RAWINPUTHEADER))!=static_cast<UINT>(-1)&&
            raw.header.dwType==RIM_TYPEKEYBOARD&&raw.data.keyboard.VKey==VK_ESCAPE&&!(raw.data.keyboard.Flags&RI_KEY_BREAK))
            instance_->abort(core::v1::End::Escape);
    }
    return DefWindowProcW(w,m,wp,lp);
}
void InputService::run(){
    bool injected_owner_stop=false;
    instance_=this;MSG msg{};PeekMessageW(&msg,nullptr,0,0,PM_NOREMOVE);thread_id_=GetCurrentThreadId();
    double_click_time_=GetDoubleClickTime();double_click_x_=std::max(1,GetSystemMetrics(SM_CXDOUBLECLK)/2);
    double_click_y_=std::max(1,GetSystemMetrics(SM_CYDOUBLECLK)/2);
    // Unlike the full double-click rectangle dimensions, SM_CX/YDRAG are the
    // distances allowed on either side of the DOWN point. Widen before taking
    // the magnitude so unusual signed metrics cannot overflow in the hook.
    const auto drag_x=static_cast<std::int64_t>(GetSystemMetrics(SM_CXDRAG));
    const auto drag_y=static_cast<std::int64_t>(GetSystemMetrics(SM_CYDRAG));
    drag_x_=drag_x<0?-drag_x:drag_x;drag_y_=drag_y<0?-drag_y:drag_y;
    WNDCLASSW wc{};wc.lpfnWndProc=receiver;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"PaneBind.V1.Input";
    const auto atom=RegisterClassW(&wc);receiver_=CreateWindowW(wc.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,wc.hInstance,nullptr);
    RAWINPUTDEVICE key{1,6,RIDEV_INPUTSINK,receiver_};
    const bool raw_ok=receiver_&&RegisterRawInputDevices(&key,1,sizeof(key));
    hook_=SetWindowsHookExW(WH_MOUSE_LL,mouse,wc.hInstance,0);
    const std::array<DWORD,6> ev{EVENT_SYSTEM_FOREGROUND,EVENT_OBJECT_DESTROY,EVENT_OBJECT_SHOW,EVENT_OBJECT_LOCATIONCHANGE,EVENT_SYSTEM_CAPTURESTART,
        EVENT_SYSTEM_MOVESIZESTART};
    // The adjacent START/END pair shares ONE out-of-context hook's sequential
    // ordering guarantee. This does not invent order with LL/Raw/other hooks.
    bool event_ok=true;for(std::size_t i=0;i<ev.size();++i){const auto last=ev[i]==EVENT_SYSTEM_MOVESIZESTART?EVENT_SYSTEM_MOVESIZEEND:ev[i];
        events_[i]=SetWinEventHook(ev[i],last,nullptr,event,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);event_ok=event_ok&&events_[i];}
    const bool stop_ok=RegisterHotKey(nullptr,910,MOD_CONTROL|MOD_SHIFT|MOD_NOREPEAT,VK_F11);
    const auto resource_route=guest::detect_resource_route(PANEBIND_BUILD_SHA);
    // Resource availability is not source authority. Normal product gestures
    // still require enabled, fresh Catalog/Prepared identity and per-generation
    // native/capture receipts. The exact guest contract never falls through
    // to ordinary production admission when its markers or hashes are invalid.
    capture_route_enabled_=resource_route.enabled();capture_route_reason_=resource_route.reason;
    capture_route_is_guest_=resource_route.kind==guest::ResourceRouteKind::ExactGuest;
    capture_route_mode_=capture_route_is_guest_?"exact_guest":
        resource_route.kind==guest::ResourceRouteKind::ProductionMedium?"production_Medium":"unavailable";
    bool shield_ok=true;
    if(capture_route_enabled_&&stop_ok){
        capture_bridge_=std::make_shared<CaptureBridge>();const auto bridge=capture_bridge_;
        bridge->authority=authorized_;bridge->completion=completion_;bridge->available=available_;
        bridge->owner_tid=GetCurrentThreadId();bridge->hotkey_registered=true;
        operations::GestureShieldCallbacks callbacks;
        callbacks.environment_allowed=[resource_route]{return resource_route.enabled()&&guest::capture_interactive_medium();};
        callbacks.consumed_down_authorized=[bridge](const auto& request){return bridge->authorized(request);};
        callbacks.gesture_authorized=[bridge](const auto& request){return bridge->native_authorized(request,false);};
        callbacks.capture_authorized=[bridge](const auto& request){return bridge->native_authorized(request,true);};
        callbacks.native_stop_owner_registered=[bridge](const auto& request){
            return bridge->hotkey_registered&&request.external_stop.process_id==GetCurrentProcessId()&&
                request.external_stop.thread_id==bridge->owner_tid.load()&&request.external_stop.hotkey_id==910;
        };
        callbacks.external_stop_owner_registered=[bridge](const auto& request){
            return bridge->hotkey_registered&&request.external_stop_owner_pid==GetCurrentProcessId()&&
                request.external_stop_owner_tid==bridge->owner_tid.load()&&request.external_stop_hotkey_id==910;
        };
        callbacks.on_event=[bridge](const auto& event){bridge->receive(event);};
        shield_=std::make_unique<operations::GestureInputShield>(std::move(callbacks));shield_ok=shield_->start();
        if(!shield_ok)capture_route_reason_="selected_input_shield_initialization_failed";
    }
    // A failed environment/guest proof must leave native input untouched, not
    // silently re-enter the superseded consumed-DOWN route without resources.
    healthy_=hook_&&raw_ok&&event_ok&&stop_ok&&shield_ok&&capture_route_enabled_;*available_=healthy_&&!paused_;SetEvent(ready_);
    if(healthy_&&!stop_requested_)while(GetMessageW(&msg,nullptr,0,0)>0){
        if(msg.message==guest_callback_stop_message){
            // Explicit TEST request, not a forged queue failure/OS hook loss.
            // All expensive Run/token/hash proof ran on the GUI. Recheck this
            // still-active generation and actual source before retiring it.
            const auto pair=capture_bridge_?capture_bridge_->pair.load():nullptr;
            const auto context=pair?pair->context.load():GestureCaptureLease{};
            const auto g=static_cast<std::uint64_t>(msg.wParam);
            if(guest_callback_stop_state_.load(std::memory_order_acquire)!=2||
               !guest_callback_owner_stop_matches(guest_callback_stop_permit_,context?&context->source:nullptr,g,authorized_->load(),
                healthy_.load(),available_->load(),paused_.load(),pair&&capture_bridge_->active(pair),pair&&pair->ready_authorized&&context,
                !pair||pair->up_observed||pair->aborted||(context&&context->matching_up_observed))||
               !context||!capture_source_current(context->source)||GetForegroundWindow()!=context->source.hwnd||
               !operations::gesture_shield_exact_capture(context->overlay,context->owner_tid)){
                guest_callback_stop_rejection_=1;PostMessageW(owner_,catalog_message,3,0);continue;
            }
            auto authority=g;unsigned armed=2;
            if(!guest_callback_stop_state_.compare_exchange_strong(armed,3)||!authorized_->compare_exchange_strong(authority,0)){
                guest_callback_stop_rejection_=2;PostMessageW(owner_,catalog_message,3,0);continue;
            }
            auto& facts=guest_callback_stop_observation_;facts.generation=g;facts.owner_tid=GetCurrentThreadId();facts.trigger_qpc=qpc_now();
            *completion_=0;*available_=false;healthy_=false;aborted_=true;active_end_=core::v1::End::InputUnavailable;
            captured_move_.retire();pair->aborted=true;guest_callback_stop_triggered_.store(true,std::memory_order_release);
            PostMessageW(owner_,catalog_message,3,0);injected_owner_stop=true;break;
        }
        else if(msg.message==WM_HOTKEY&&msg.wParam==910&&HIWORD(msg.lParam)==VK_F11&&
           (LOWORD(msg.lParam)&(MOD_CONTROL|MOD_SHIFT|MOD_ALT|MOD_WIN))==(MOD_CONTROL|MOD_SHIFT)){
            ++emergency_hotkey_messages_;*available_=false;paused_=true;abort(core::v1::End::Paused);PostMessageW(owner_,catalog_message,2,0);}
        else if(msg.message==WM_APP+1&&msg.wParam&&(consumed_down_||native_move_)&&msg.wParam==generation_)abort(core::v1::End::Paused);
        else if(msg.message==WM_APP+2&&msg.wParam&&(consumed_down_||native_move_)&&msg.wParam==generation_)abort(static_cast<core::v1::End>(msg.lParam));
        else if(msg.message==capture_request_message){
            // Native publication retained the previous generation here. The
            // potentially final shared reference is retired outside the LL ABI.
            if(capture_bridge_)capture_bridge_->retired_pair.exchange({});
            if(capture_pending_down_&&capture_pending_down_->generation==msg.wParam){
                const auto pending=std::move(*capture_pending_down_);capture_pending_down_.reset();
                if(!begin_capture(pending)){
                    abort(core::v1::End::SourceLost);consumed_down_=false;native_move_=false;capture_pair_=false;active_prepared_.reset();prepared_.store({});
                    if(!prepare_pending_.exchange(true))PostMessageW(owner_,prepare_message,0,0);
                }
            }
        }
        else if(msg.message==capture_events_message)drain_capture_events();
        else if(msg.message==capture_stop_message){
            if(shield_)(void)shield_->request_remove(static_cast<std::uint64_t>(msg.wParam),static_cast<Removal>(msg.lParam));
        }
        else{TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    *completion_=0;*available_=false;*authorized_=0;healthy_=false;KillTimer(receiver_,gesture_deadline_timer);
    if(capture_bridge_){if(auto p=capture_bridge_->pair.load())p->aborted=true;}
    if(shield_){capture_stop_facts_=shield_->stop();drain_capture_events();shield_.reset();}
    if(capture_bridge_){capture_bridge_->owner_tid=0;capture_bridge_->hotkey_registered=false;}
    auto& stop_facts=guest_callback_stop_observation_;
    if(injected_owner_stop)stop_facts.shield_stop_clean=capture_stop_facts_.clean();
    if(stop_ok){SetLastError(0);const bool returned=UnregisterHotKey(nullptr,910)!=FALSE;
        if(injected_owner_stop){stop_facts.hotkey_attempted=true;stop_facts.hotkey_return=returned;stop_facts.hotkey_error=returned?0:GetLastError();}}
    if(hook_){if(injected_owner_stop){stop_facts.hook=reinterpret_cast<std::uintptr_t>(hook_);stop_facts.unhook_attempted=true;stop_facts.unhook_before_qpc=qpc_now();}
        SetLastError(0);const bool returned=UnhookWindowsHookEx(hook_)!=FALSE;
        if(injected_owner_stop){stop_facts.unhook_error=returned?0:GetLastError();stop_facts.unhook_return=returned;stop_facts.unhook_after_qpc=qpc_now();}}
    hook_=nullptr;
    for(std::size_t i=0;i<events_.size();++i)if(events_[i]){SetLastError(0);const bool returned=UnhookWinEvent(events_[i])!=FALSE;
        if(injected_owner_stop){stop_facts.event_attempted[i]=true;stop_facts.event_return[i]=returned;stop_facts.event_error[i]=returned?0:GetLastError();}events_[i]=nullptr;}
    if(raw_ok){key.dwFlags=RIDEV_REMOVE;key.hwndTarget=nullptr;SetLastError(0);const bool returned=RegisterRawInputDevices(&key,1,sizeof(key))!=FALSE;
        if(injected_owner_stop){stop_facts.raw_attempted=true;stop_facts.raw_return=returned;stop_facts.raw_error=returned?0:GetLastError();}}
    if(receiver_){SetLastError(0);const bool returned=DestroyWindow(receiver_)!=FALSE;
        if(injected_owner_stop){stop_facts.receiver_attempted=true;stop_facts.receiver_return=returned;stop_facts.receiver_error=returned?0:GetLastError();}}
    receiver_=nullptr;if(atom)UnregisterClassW(wc.lpszClassName,wc.hInstance);instance_=nullptr;
    active_prepared_.reset();consumed_down_=false;native_move_=false;thread_id_=0;
    if(injected_owner_stop){stop_facts.exit_record_qpc=qpc_now();guest_callback_stop_exit_ready_.store(true,std::memory_order_release);}
}
}
