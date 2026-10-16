// TEST ONLY. A short-lived, passive head-of-chain observation around one
// already-planned guest UP. It never supplies product authority or NormalUp.
// Included only by the guarded guest fixture, not by the product executable.
class GuestUpHookObservation {
    struct Facts {
        HANDLE ready{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
        HANDLE done{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
        std::atomic<DWORD> tid{},install_error{},unhook_error{},stamp{},flags{};
        std::atomic<bool> stopping{},installed{},unhooked{},returned{};
        std::atomic<std::uint64_t> count{},qpc{},extra{};
        std::atomic<LONG_PTR> downstream{};
        ~Facts(){if(ready)CloseHandle(ready);if(done)CloseHandle(done);}
    };
    std::shared_ptr<Facts> facts_{std::make_shared<Facts>()};
    static inline thread_local Facts* current_{};
    static LRESULT CALLBACK callback(int code,WPARAM message,LPARAM packet){
        auto* facts=current_;
        if(code<0||!facts)return CallNextHookEx(nullptr,code,message,packet);
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
            if(hook&&!state->stopping)while(GetMessageW(&message,nullptr,0,0)>0){
                TranslateMessage(&message);DispatchMessageW(&message);
            }
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
        return "{\"snapshot_coherent\":"+std::string(finished?"true":"false")+
            ",\"observation_final\":"+(finished?"true":"false")+
            ",\"installed\":"+(f.installed?"true":"false")+
            ",\"install_error\":"+std::to_string(f.install_error.load())+
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
