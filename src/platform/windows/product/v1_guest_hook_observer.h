// TEST ONLY. A short-lived, passive head-of-chain observation around one
// already-planned guest UP. It never supplies product authority or NormalUp.
// Included only by the guarded guest fixture, not by the product executable.
class GuestUpHookObservation {
    struct FixedMessage {
        std::atomic<std::uint64_t> count{},entry_qpc{},return_qpc{};
        std::atomic<bool> in_callnext{};
    };
    struct Facts {
        HANDLE ready{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
        HANDLE done{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
        std::atomic<DWORD> tid{},install_error{},unhook_error{},stamp{},flags{};
        std::atomic<bool> stopping{},installed{},unhooked{},returned{};
        std::atomic<std::uint64_t> count{},qpc{},extra{};
        std::atomic<LONG_PTR> downstream{};
        std::array<FixedMessage,3> fixed;
        std::atomic<bool> pump_entered{};
        ~Facts(){if(ready)CloseHandle(ready);if(done)CloseHandle(done);}
    };
    std::shared_ptr<Facts> facts_{std::make_shared<Facts>()};
    static inline thread_local Facts* current_{};
    static LRESULT CALLBACK callback(int code,WPARAM message,LPARAM packet){
        auto* facts=current_;
        if(code<0||!facts)return CallNextHookEx(nullptr,code,message,packet);
        const unsigned index=message==WM_LBUTTONDOWN?0:message==WM_MOUSEMOVE?1:message==WM_LBUTTONUP?2:3;
        LARGE_INTEGER entered{};if(index<3){QueryPerformanceCounter(&entered);auto& fixed=facts->fixed[index];
            ++fixed.count;fixed.entry_qpc=entered.QuadPart>0?static_cast<std::uint64_t>(entered.QuadPart):0;fixed.return_qpc=0;fixed.in_callnext=true;}
        const bool left_up=message==WM_LBUTTONUP;
        if(left_up){
            const auto& value=*reinterpret_cast<const MSLLHOOKSTRUCT*>(packet);
            LARGE_INTEGER now{};QueryPerformanceCounter(&now);
            facts->qpc=now.QuadPart>0?static_cast<std::uint64_t>(now.QuadPart):0;
            facts->stamp=value.time;facts->flags=value.flags;facts->extra=value.dwExtraInfo;
            facts->returned=false;++facts->count;
        }
        // Preserve the complete downstream result. No filtering, retry,
        // geometry, window query, logging or allocation in this callback.
        const auto result=CallNextHookEx(nullptr,code,message,packet);
        if(index<3){LARGE_INTEGER returned{};QueryPerformanceCounter(&returned);auto& fixed=facts->fixed[index];
            fixed.return_qpc=returned.QuadPart>0?static_cast<std::uint64_t>(returned.QuadPart):0;fixed.in_callnext=false;}
        if(left_up){facts->downstream=result;facts->returned=true;}
        return result;
    }
public:
    bool start(){
        const auto state=facts_;if(!state->ready||!state->done)return false;
        try{std::thread([state]{
            MSG message{};PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE);
            state->tid=GetCurrentThreadId();current_=state.get();SetLastError(0);
            const HHOOK hook=SetWindowsHookExW(WH_MOUSE_LL,callback,GetModuleHandleW(nullptr),0);
            state->install_error=hook?0:GetLastError();state->installed=hook!=nullptr;SetEvent(state->ready);
            if(hook&&!state->stopping){state->pump_entered=true;while(GetMessageW(&message,nullptr,0,0)>0){
                TranslateMessage(&message);DispatchMessageW(&message);
            }}
            if(hook){SetLastError(0);state->unhooked=UnhookWindowsHookEx(hook)!=FALSE;
                state->unhook_error=state->unhooked?0:GetLastError();}
            current_=nullptr;SetEvent(state->done);
        }).detach();}catch(...){return false;}
        return WaitForSingleObject(state->ready,2500)==WAIT_OBJECT_0&&state->installed;
    }
    bool stop(){
        const auto state=facts_;state->stopping=true;const auto tid=state->tid.load();
        if(tid)PostThreadMessageW(tid,WM_QUIT,0,0);
        return state->done&&WaitForSingleObject(state->done,2500)==WAIT_OBJECT_0&&state->unhooked;
    }
    ~GuestUpHookObservation(){stop();}
    std::string json()const{
        const auto& f=*facts_;const auto count=f.count.load();
        const bool finished=f.done&&WaitForSingleObject(f.done,0)==WAIT_OBJECT_0;
        std::string fixed="[";for(unsigned index=0;index<3;++index){if(index)fixed+=',';const auto& value=f.fixed[index];
            fixed+="{\"native_message\":"+std::to_string(index==0?WM_LBUTTONDOWN:index==1?WM_MOUSEMOVE:WM_LBUTTONUP)+
                ",\"callback_count\":"+std::to_string(value.count.load())+",\"last_entry_qpc\":"+std::to_string(value.entry_qpc.load())+
                ",\"last_CallNext_return_qpc\":"+std::to_string(value.return_qpc.load())+",\"currently_in_CallNext\":"+(value.in_callnext?"true":"false")+"}";}fixed+=']';
        return "{\"snapshot_coherent\":"+std::string(finished?"true":"false")+
            ",\"observation_final\":"+(finished?"true":"false")+
            ",\"installed\":"+(f.installed?"true":"false")+
            ",\"install_error\":"+std::to_string(f.install_error.load())+
            ",\"actual_hook_thread\":"+std::to_string(f.tid.load())+",\"GetMessage_loop_entered\":"+(f.pump_entered?"true":"false")+
            ",\"fixed_mouse_message_observations\":"+fixed+
            ",\"actual_left_UP_callbacks\":"+std::to_string(count)+
            ",\"last_qpc\":"+(count?std::to_string(f.qpc.load()):"null")+
            ",\"last_native_time\":"+(count?std::to_string(f.stamp.load()):"null")+
            ",\"last_flags\":"+(count?std::to_string(f.flags.load()):"null")+
            ",\"last_extra_information\":"+(count?std::to_string(f.extra.load()):"null")+
            ",\"CallNext_returned\":"+(count?(f.returned?"true":"false"):"null")+
            ",\"CallNext_result\":"+(count&&f.returned?std::to_string(f.downstream.load()):"null")+
            ",\"unhooked\":"+(f.unhooked?"true":"false")+
            ",\"unhook_error\":"+std::to_string(f.unhook_error.load())+
            ",\"scope\":\"passive_guest_test_only_no_product_receipt\"}";
    }
};

// TEST ONLY. Four one-use slots correspond to the four rapid owned routes.
// Each hook has its own callback address and process-lifetime retained facts:
// a callback still returning after Unhook cannot touch a destroyed stack or a
// later route's record. No DLL, global thread id, message mutation or logging.
class GuestOwnedQueueUpObservation {
    struct MessageFacts {
        std::atomic<std::uintptr_t> hwnd{},wparam{};
        std::atomic<UINT> message{};
        std::atomic<std::intptr_t> lparam{};
        std::atomic<DWORD> time{};
        std::atomic<LONG> x{},y{};
        void store(const MSG& value){hwnd=reinterpret_cast<std::uintptr_t>(value.hwnd);message=value.message;
            wparam=value.wParam;lparam=value.lParam;time=value.time;x=value.pt.x;y=value.pt.y;}
        std::string json()const{return "{\"hwnd\":"+std::to_string(hwnd.load())+",\"message\":"+std::to_string(message.load())+
            ",\"wParam\":"+std::to_string(wparam.load())+",\"lParam\":"+std::to_string(lparam.load())+",\"time\":"+std::to_string(time.load())+
            ",\"pt\":["+std::to_string(x.load())+","+std::to_string(y.load())+"]}";}
    };
    struct Record {
        MessageFacts before,after;
        std::atomic<std::uint64_t> entry_qpc{},return_qpc{};
        std::atomic<std::intptr_t> extra{},downstream{};
        std::atomic<bool> returned{},unchanged{};
    };
    struct Facts {
        HWND source{};DWORD owner_tid{};
        std::atomic<bool> installed{},unhook_attempted{},unhooked{};
        std::atomic<DWORD> install_error{},unhook_error{};
        std::atomic<unsigned> count{},active_callbacks{};
        std::array<Record,2> records;
    };
    static inline std::array<std::shared_ptr<Facts>,4> retained_;
    static inline std::array<std::atomic<Facts*>,4> published_{};
    static inline std::atomic<unsigned> next_slot_{};
    std::shared_ptr<Facts> facts_{std::make_shared<Facts>()};
    HHOOK hook_{};
    template<unsigned Slot> static LRESULT CALLBACK callback(int code,WPARAM removed,LPARAM parameter){
        auto* facts=published_[Slot].load(std::memory_order_acquire);
        if(code!=HC_ACTION||removed!=PM_REMOVE||!parameter||!facts)return CallNextHookEx(nullptr,code,removed,parameter);
        const auto& message=*reinterpret_cast<const MSG*>(parameter);
        if(message.message!=WM_LBUTTONUP&&message.message!=WM_NCLBUTTONUP)return CallNextHookEx(nullptr,code,removed,parameter);
        ++facts->active_callbacks;const auto index=facts->count.fetch_add(1);Record* record=index<facts->records.size()?&facts->records[index]:nullptr;
        const MSG before=message;
        if(record){LARGE_INTEGER now{};QueryPerformanceCounter(&now);record->before.store(before);
            record->entry_qpc=now.QuadPart>0?static_cast<std::uint64_t>(now.QuadPart):0;record->extra=GetMessageExtraInfo();}
        const auto result=CallNextHookEx(nullptr,code,removed,parameter);
        if(record){const MSG after=message;LARGE_INTEGER now{};QueryPerformanceCounter(&now);record->after.store(after);
            record->unchanged=before.hwnd==after.hwnd&&before.message==after.message&&before.wParam==after.wParam&&before.lParam==after.lParam&&
                before.time==after.time&&before.pt.x==after.pt.x&&before.pt.y==after.pt.y;
            record->downstream=result;record->return_qpc=now.QuadPart>0?static_cast<std::uint64_t>(now.QuadPart):0;record->returned=true;}
        --facts->active_callbacks;return result;
    }
public:
    bool start(HWND source,DWORD owner_tid){
        DWORD pid{};if(!source||!owner_tid||GetWindowThreadProcessId(source,&pid)!=owner_tid||pid!=GetCurrentProcessId()||
            GetAncestor(source,GA_ROOT)!=source||hook_||facts_->installed)return false;
        const auto slot=next_slot_.fetch_add(1);if(slot>=retained_.size())return false;
        facts_->source=source;facts_->owner_tid=owner_tid;retained_[slot]=facts_;published_[slot].store(facts_.get(),std::memory_order_release);
        const std::array<HOOKPROC,4> callbacks{callback<0>,callback<1>,callback<2>,callback<3>};SetLastError(0);
        hook_=SetWindowsHookExW(WH_GETMESSAGE,callbacks[slot],nullptr,owner_tid);
        facts_->install_error=hook_?0:GetLastError();facts_->installed=hook_!=nullptr;return hook_!=nullptr;
    }
    bool stop(){
        if(facts_->unhook_attempted)return facts_->unhooked;
        if(!hook_)return false;
        facts_->unhook_attempted=true;SetLastError(0);facts_->unhooked=UnhookWindowsHookEx(hook_)!=FALSE;
        facts_->unhook_error=facts_->unhooked?0:GetLastError();hook_=nullptr;return facts_->unhooked;
    }
    ~GuestOwnedQueueUpObservation(){stop();}
    // A second native-only TEST observation, never product NormalUp. Require
    // the exact source queue's unchanged, removed, marked UP to return before
    // this source's real modal END. Missing marker/queue facts stay UNKNOWN.
    std::uint64_t exact_source_up_qpc(HWND source,DWORD owner_tid,std::uint64_t send_before,
        DWORD down_stamp,POINT destination,std::uint64_t native_end_qpc)const{
        const auto& f=*facts_;if(!f.installed||f.source!=source||f.owner_tid!=owner_tid||
            !source||!owner_tid||!send_before||!native_end_qpc||f.count!=1||f.active_callbacks!=0)return 0;
        const auto& r=f.records[0];const auto entered=r.entry_qpc.load(),returned=r.return_qpc.load();
        const auto message=r.before.message.load();
        if(!r.returned||!r.unchanged||entered<send_before||returned<entered||native_end_qpc<=returned||
            r.extra!=0x50425631||r.before.hwnd!=reinterpret_cast<std::uintptr_t>(source)||
            (message!=WM_LBUTTONUP&&message!=WM_NCLBUTTONUP)||
            static_cast<LONG>(r.before.time.load()-down_stamp)<0||
            r.before.x!=destination.x||r.before.y!=destination.y||f.count!=1||f.active_callbacks!=0)return 0;
        return entered;
    }
    std::string json()const{
        const auto& f=*facts_;const auto count=f.count.load();std::string records="[";
        for(unsigned index=0;index<std::min<unsigned>(count,static_cast<unsigned>(f.records.size()));++index){if(index)records+=',';const auto& r=f.records[index];
            records+="{\"MSG_before\":"+r.before.json()+",\"MSG_after\":"+r.after.json()+",\"callback_qpc\":"+std::to_string(r.entry_qpc.load())+
                ",\"CallNext_return_qpc\":"+std::to_string(r.return_qpc.load())+",\"GetMessageExtraInfo_raw\":"+std::to_string(r.extra.load())+
                ",\"CallNext_returned\":"+(r.returned?"true":"false")+",\"CallNext_result\":"+std::to_string(r.downstream.load())+
                ",\"documented_MSG_fields_unchanged\":"+(r.unchanged?"true":"false")+"}";}records+=']';
        return "{\"scope\":\"exact_own_process_nonzero_source_thread_PM_REMOVE_UP_observation_only\",\"source\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(f.source))+",\"owner_tid\":"+std::to_string(f.owner_tid)+
            ",\"installed\":"+(f.installed?"true":"false")+",\"install_error\":"+std::to_string(f.install_error.load())+
            ",\"actual_queue_UP_count\":"+std::to_string(count)+",\"record_overflow\":"+(count>f.records.size()?"true":"false")+
            ",\"no_callback_active_at_read\":"+(f.active_callbacks==0?"true":"false")+
            ",\"snapshot_semantics\":\"bounded_atomic_fields_not_an_inflight_callback_barrier\",\"unhook_attempted\":"+(f.unhook_attempted?"true":"false")+
            ",\"unhooked\":"+(f.unhooked?"true":"false")+",\"unhook_error\":"+std::to_string(f.unhook_error.load())+
            ",\"records\":"+records+",\"marker_preservation_or_NormalUp_inferred\":false}";
    }
};
