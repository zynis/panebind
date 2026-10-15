#include "input_service.h"
#include <algorithm>

namespace panebind::platform::windows::product {
InputService* InputService::instance_{};
namespace {
constexpr UINT_PTR gesture_deadline_timer=1;
constexpr UINT gesture_deadline_ms=30000;
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
bool InputService::start(HWND owner){
    if(thread_.joinable())return healthy_;
    if(!IsWindow(owner))return false;
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,"hook timing counters must not hide a lock");
    LARGE_INTEGER frequency{};counter_frequency_=QueryPerformanceFrequency(&frequency)&&frequency.QuadPart>0?
        static_cast<std::uint64_t>(frequency.QuadPart):0;
    paused_=false;healthy_=false;stop_requested_=false;prepare_pending_=false;
    read_=0;write_=0;consumed_down_=false;aborted_=false;click_candidate_={};
    owner_=owner;ready_=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!ready_)return false;
    try{thread_=std::thread([this]{run();});}catch(...){CloseHandle(ready_);ready_=nullptr;return false;}
    const auto result=WaitForSingleObject(ready_,5000);
    // Keep the initialization event alive until the worker is done using it,
    // including the timeout path. Closing first races the worker's SetEvent.
    if(result!=WAIT_OBJECT_0||!healthy_)stop();
    CloseHandle(ready_);ready_=nullptr;
    return result==WAIT_OBJECT_0&&healthy_;
}
void InputService::stop(){
    *completion_=0;*available_=false;*authorized_=0;paused_=true;stop_requested_=true;
    if(thread_.joinable()){const auto id=thread_id_.load();if(id)PostThreadMessageW(id,WM_QUIT,0,0);thread_.join();}
    prepared_.store({});
}
void InputService::pause(bool value){*completion_=0;paused_=value;*available_=!value&&healthy_;const auto retired=authorized_->exchange(0);
    const auto id=thread_id_.load();if(value&&id)PostThreadMessageW(id,WM_APP+1,static_cast<WPARAM>(retired),0);}
POINT InputService::latest()const{auto p=point_.load();return {static_cast<LONG>(p>>32),static_cast<LONG>(p&0xffffffffu)};}
bool InputService::push(InputEvent e){
    const auto w=write_.load(std::memory_order_relaxed);
    const auto next=(w+1)%static_cast<unsigned>(queue_.size());
    if(next==read_.load(std::memory_order_acquire)){*completion_=0;*available_=false;*authorized_=0;healthy_=false;
        if(consumed_down_&&!aborted_){aborted_=true;active_end_=core::v1::End::QueueFailure;}
        PostMessageW(owner_,catalog_message,1,0);return false;}
    queue_[w]=std::move(e);write_.store(static_cast<unsigned>(next),std::memory_order_release);
    if(!PostMessageW(owner_,input_message,0,0)){*completion_=0;*available_=false;*authorized_=0;healthy_=false;
        if(consumed_down_&&!aborted_){aborted_=true;active_end_=core::v1::End::QueueFailure;}
        return false;}return true;
}
bool InputService::pop(InputEvent& e){const auto r=read_.load(std::memory_order_relaxed);
    if(r==write_.load(std::memory_order_acquire))return false;e=std::move(queue_[r]);
    read_.store((r+1)%static_cast<unsigned>(queue_.size()),std::memory_order_release);return true;}
void InputService::abort(core::v1::End end){
    *completion_=0;*authorized_=0;
    if(consumed_down_&&!aborted_){aborted_=true;active_end_=end;
        push({InputKind::Abort,latest(),generation_,GetTickCount64(),modifiers_,button_,1,end,active_prepared_,active_action_});}
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
    if(observed_up){
        up_observation.callback_count=++self->up_callback_count_;up_observation.hook_observed_qpc=observed_qpc;
        up_observation.button=button(message,packet.mouseData);up_observation.generation_at_entry=self->generation_;
        up_observation.consumed_before=self->consumed_down_;up_observation.aborted_before=self->aborted_;
    }
    // Any newly observed physical/test DOWN retires an earlier UP's pending
    // discrete dock commit, even if this new click passes through natively.
    if(down(message)){
        *self->completion_=0;
        if(retire_revoked_pair_for_down(self->consumed_down_,self->authorized_->load(),
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
    if(self->consumed_down_&&!self->aborted_&&self->stop_requested_)self->abort(core::v1::End::Exit);
    else if(self->consumed_down_&&!self->aborted_&&self->paused_)self->abort(core::v1::End::Paused);
    if(self->consumed_down_&&!self->aborted_&&tick>=self->deadline_)self->abort(core::v1::End::Deadline);
    bool consume=false;
    if(message==WM_MOUSEMOVE){
        if(self->consumed_down_){
            if(self->allowed(self->generation_)){
                const auto& p=self->active_prepared_;
                if(!p||GetForegroundWindow()!=p->source.hwnd)self->abort(core::v1::End::FocusLost);
                else self->push({InputKind::Move,packet.pt,self->generation_,tick,self->modifiers_,self->button_,1,
                    self->active_end_,p,self->active_action_,observed_qpc});
            }
            // Observe motion, but let Windows update/route the real cursor.
            // The matching DOWN was already consumed, so it did not start a
            // native nonclient Move/Resize loop. Filtering every MOVE can
            // suppress the system cursor itself; it is not needed to prevent
            // a second native geometry writer. The consumed button's actual
            // UP is still paired below, with the existing finite deadline.
            consume=false;
        }
        // Waiting for a revoked pair's UP must not freeze hover preparation.
        // The GUI still refuses to prepare while its gesture is active; this
        // event-driven request grants no write authority and starts no timer.
        if(motion_requests_preparation(self->consumed_down_,self->authorized_->load())&&
            !self->prepare_pending_.exchange(true))PostMessageW(self->owner_,prepare_message,0,0);
    }else if(down(message)&&!self->consumed_down_&&!self->paused_&&self->healthy_){
        auto p=self->prepared_.load();const auto mods=modifiers();const auto b=button(message,packet.mouseData);
        // Incomplete/stale preparation passes through. Never hit-test/COM/wait in
        // this callback, and never turn an inactive-window click into activation.
        // This exact-point preparation is invalidated/rebuilt by native
        // geometry/lifecycle/foreground events, not an arbitrary hover timeout.
        // The GUI still revalidates identity and geometry before ANY write.
        if(p&&p->region>=0&&p->point.x==packet.pt.x&&p->point.y==packet.pt.y&&
            GetForegroundWindow()==p->source.hwnd){
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
            if(route.route!=DownRoute::Pass&&self->next_generation_!=std::numeric_limits<std::uint64_t>::max()){
                self->generation_=++self->next_generation_;*self->authorized_=self->generation_;
                self->consumed_down_=true;self->aborted_=false;self->button_=b;self->modifiers_=mods;
                self->active_down_stamp_=packet.time;
                self->active_prepared_=p;self->active_action_=route.action;self->active_end_=core::v1::End::NormalUp;
                self->deadline_=tick+gesture_deadline_ms;
                if(SetTimer(self->receiver_,gesture_deadline_timer,gesture_deadline_ms,nullptr))
                    consume=self->push({route.route==DownRoute::Mapped?InputKind::Mapped:InputKind::Down,
                        packet.pt,self->generation_,tick,mods,b,clicks,core::v1::End::NormalUp,p,route.action,observed_qpc});
                if(!consume){self->consumed_down_=false;*self->authorized_=0;self->active_prepared_.reset();
                    KillTimer(self->receiver_,gesture_deadline_timer);}
            }
        }else self->click_candidate_={};
    }else if(up(message)&&button_up_matches_current_pair(self->consumed_down_,self->button_,
        button(message,packet.mouseData),self->active_down_stamp_,packet.time)){
        up_observation.matching_branch_taken=true;
        const auto& prepared=self->active_prepared_;DWORD source_pid{};
        const DWORD source_tid=prepared?GetWindowThreadProcessId(prepared->source.hwnd,&source_pid):0;
        const bool source_current=prepared&&self->generation_&&self->authorized_->load()==self->generation_&&
            prepared->source.key&&prepared->source.hwnd&&IsWindow(prepared->source.hwnd)&&
            GetAncestor(prepared->source.hwnd,GA_ROOT)==prepared->source.hwnd&&
            source_tid==prepared->source.tid&&source_pid==prepared->source.pid;
        self->active_end_=matching_up_end(self->aborted_,self->active_end_,source_current,
            prepared&&GetForegroundWindow()==prepared->source.hwnd);
        if(self->active_end_!=core::v1::End::NormalUp)self->aborted_=true;
        *self->completion_=!self->aborted_&&self->active_end_==core::v1::End::NormalUp?self->generation_:0;
        *self->authorized_=0;consume=true;KillTimer(self->receiver_,gesture_deadline_timer);
        up_observation.push_attempted=true;
        up_observation.push_succeeded=self->push({InputKind::Up,packet.pt,self->generation_,tick,self->modifiers_,self->button_,1,
            self->active_end_,self->active_prepared_,self->active_action_,observed_qpc});
        self->consumed_down_=false;self->active_prepared_.reset();
        if(!self->prepare_pending_.exchange(true))PostMessageW(self->owner_,prepare_message,0,0);
    }
    if(observed_up)self->up_observation_.publish(up_observation);
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
void CALLBACK InputService::event(HWINEVENTHOOK,DWORD ev,HWND hwnd,LONG object,LONG child,DWORD,DWORD stamp){
    auto* s=instance_;if(!s)return;
    if(s->consumed_down_&&native_event_is_current(stamp,s->active_down_stamp_)){const auto& p=s->active_prepared_;
        if(ev==EVENT_SYSTEM_FOREGROUND&&p&&hwnd!=p->source.hwnd&&GetForegroundWindow()!=p->source.hwnd)s->abort(core::v1::End::FocusLost);
        if(ev==EVENT_OBJECT_DESTROY&&p&&hwnd==p->source.hwnd&&object==OBJID_WINDOW&&child==CHILDID_SELF)s->abort(core::v1::End::SourceLost);
        // This production route never takes capture. An observed new capture
        // is an invalidating native interaction, not permission to reclaim it.
        if(ev==EVENT_SYSTEM_CAPTURESTART&&p&&hwnd&&
            (GetWindowThreadProcessId(hwnd,nullptr)==p->source.tid||GetAncestor(hwnd,GA_ROOT)==p->source.hwnd))
            s->abort(core::v1::End::FocusLost);
    }
    if((object==OBJID_WINDOW&&child==CHILDID_SELF)||ev==EVENT_SYSTEM_FOREGROUND)PostMessageW(s->owner_,catalog_message,0,0);
}
LRESULT CALLBACK InputService::receiver(HWND w,UINT m,WPARAM wp,LPARAM lp){
    if(m==WM_TIMER&&wp==gesture_deadline_timer&&instance_){
        // A previously queued timer message cannot terminate a newer gesture.
        if(instance_->consumed_down_&&GetTickCount64()>=instance_->deadline_){
            KillTimer(w,gesture_deadline_timer);instance_->abort(core::v1::End::Deadline);
            instance_->consumed_down_=false;instance_->active_prepared_.reset();
            instance_->prepared_.store({});}
        else if(!instance_->consumed_down_)KillTimer(w,gesture_deadline_timer);
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
    const std::array<DWORD,5> ev{EVENT_SYSTEM_FOREGROUND,EVENT_OBJECT_DESTROY,EVENT_OBJECT_SHOW,EVENT_OBJECT_LOCATIONCHANGE,EVENT_SYSTEM_CAPTURESTART};
    bool event_ok=true;for(std::size_t i=0;i<ev.size();++i){events_[i]=SetWinEventHook(ev[i],ev[i],nullptr,event,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);event_ok=event_ok&&events_[i];}
    const bool stop_ok=RegisterHotKey(nullptr,910,MOD_CONTROL|MOD_SHIFT|MOD_NOREPEAT,VK_F11);
    healthy_=hook_&&raw_ok&&event_ok&&stop_ok;*available_=healthy_&&!paused_;SetEvent(ready_);
    if(healthy_&&!stop_requested_)while(GetMessageW(&msg,nullptr,0,0)>0){
        if(msg.message==WM_HOTKEY&&msg.wParam==910){++emergency_hotkey_messages_;*available_=false;paused_=true;abort(core::v1::End::Paused);PostMessageW(owner_,catalog_message,2,0);}
        else if(msg.message==WM_APP+1&&msg.wParam&&consumed_down_&&msg.wParam==generation_)abort(core::v1::End::Paused);
        else if(msg.message==WM_APP+2&&msg.wParam&&consumed_down_&&msg.wParam==generation_)abort(static_cast<core::v1::End>(msg.lParam));
        else{TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    *completion_=0;*available_=false;*authorized_=0;healthy_=false;KillTimer(receiver_,gesture_deadline_timer);
    if(stop_ok)UnregisterHotKey(nullptr,910);if(hook_)UnhookWindowsHookEx(hook_);hook_=nullptr;
    for(auto h:events_)if(h)UnhookWinEvent(h);
    if(raw_ok){key.dwFlags=RIDEV_REMOVE;key.hwndTarget=nullptr;RegisterRawInputDevices(&key,1,sizeof(key));}
    if(receiver_)DestroyWindow(receiver_);receiver_=nullptr;if(atom)UnregisterClassW(wc.lpszClassName,wc.hInstance);instance_=nullptr;
    active_prepared_.reset();consumed_down_=false;thread_id_=0;
}
}
