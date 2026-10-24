#pragma once

// TEST ONLY: the exact Run blank neighbour has an independent UI owner.
// That owner performs no evidence-file I/O, input driving or product writing.
class MediumDesktopMagnetTarget {
    struct State {
        std::wstring name,run;HDESK desktop{};HWND source{};std::atomic<HWND> hwnd{};
        DWORD parent{},owner{},pid{},session{};std::uint64_t birth{};UINT dpi{};HMONITOR monitor{};
        Rect initial{},work{};std::optional<Rect> positioning;
        HINSTANCE instance{GetModuleHandleW(nullptr)};
        HANDLE stop{CreateEventW(nullptr,TRUE,FALSE,nullptr)},ready{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
        std::atomic<bool> stopping{},facts_ready{},worker_cleanup_known{};
        bool desktop_open_attempted{},desktop_bound{},com_attempted{},com_owned{},registered{},created{},placed{},setup_known{};
        bool destroy_attempted{},destroyed{true},unregister_attempted{},unregistered{true},com_exited{};
        DWORD desktop_error{},bind_error{},register_error{},create_error{},placement_error{},wait_error{};
        HRESULT com_result{E_UNEXPECTED},vdm_create{E_UNEXPECTED},vdm_query{E_UNEXPECTED};BOOL vdm_current{};
        const char* setup_reason{"worker_not_started"};
        std::mutex retention_mutex;std::shared_ptr<State> retained_on_cleanup_unknown;
        bool close_handles()noexcept{
            bool known=true;
            if(desktop){const auto h=desktop;desktop=nullptr;if(!CloseDesktop(h))known=false;}
            if(stop){const auto h=stop;stop=nullptr;if(!CloseHandle(h))known=false;}
            if(ready){const auto h=ready;ready=nullptr;if(!CloseHandle(h))known=false;}
            return known;
        }
        ~State(){close_handles();}
    };
    std::shared_ptr<State> state_;std::thread thread_;HDESK parent_desktop_{};
    struct CoupledGeometry {Rect before,after,before_native,after_native;};
    std::optional<CoupledGeometry> coupled_;
    MediumDesktopProductGeometryObservation* output_{};bool thread_started_{},finish_attempted_{},finish_known_{};
    static constexpr ACCESS_MASK desktop_access=DESKTOP_READOBJECTS|DESKTOP_CREATEWINDOW|
        DESKTOP_CREATEMENU|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE;
    static void retain_unknown(const std::shared_ptr<State>& state)noexcept{
        try{std::lock_guard lock(state->retention_mutex);state->retained_on_cleanup_unknown=state;}catch(...){}
    }
    static LRESULT CALLBACK procedure(HWND h,UINT m,WPARAM w,LPARAM l){
        if(m==WM_NCCREATE){const auto* c=reinterpret_cast<const CREATESTRUCTW*>(l);
            SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(c->lpCreateParams));}
        // Ordinary frame/track-limit semantics. Never dereference the userdata
        // token, activate on show, log, or consume/replay TEST input here.
        return DefWindowProcW(h,m,w,l);
    }
    static bool exact_state(const State& s,HDESK held,const CoupledGeometry* coupled=nullptr){
        const HWND h=s.hwnd.load(std::memory_order_acquire);
        if(!h||!s.owner||s.stopping.load(std::memory_order_acquire)||GetCurrentProcessId()!=s.pid||
            process_birth(GetCurrentProcess())!=s.birth||!medium(GetCurrentProcess())||!input_actor_same_user(GetCurrentProcess())||
            !equal(medium_desktop_object_name(GetThreadDesktop(GetCurrentThreadId())),L"PaneBindV1_"+s.run)||
            !equal(medium_desktop_object_name(GetThreadDesktop(s.owner)),L"PaneBindV1_"+s.run)||
            !equal(medium_desktop_object_name(held),L"PaneBindV1_"+s.run))return false;
        DWORD session{},pid{};if(!ProcessIdToSessionId(s.pid,&session)||session!=s.session||
            !IsWindow(h)||GetWindowThreadProcessId(h,&pid)!=s.owner||pid!=s.pid||GetAncestor(h,GA_ROOT)!=h||
            GetWindow(h,GW_OWNER)||GetWindowLongPtrW(h,GWLP_USERDATA)!=reinterpret_cast<LONG_PTR>(&s)||
            gui_class(h)!=s.name||!IsWindowVisible(h)||IsIconic(h)||GetDpiForWindow(h)!=s.dpi||
            MonitorFromWindow(h,MONITOR_DEFAULTTONULL)!=s.monitor)return false;
        const auto a=MediumDesktopWindows::read(held),b=MediumDesktopWindows::read(held);
        if(!a.complete||!b.complete||std::find(a.windows.begin(),a.windows.end(),h)==a.windows.end()||
            std::find(b.windows.begin(),b.windows.end(),h)==b.windows.end())return false;
        DWORD cloak{};RECT p{};const auto frame=visible(h);MONITORINFO monitor{sizeof(monitor)};
        if(!GetWindowRect(h,&p)||!frame)return false;
        const Rect native{p.left,p.top,p.right,p.bottom};
        const bool geometry=coupled?((*frame==coupled->before&&native==coupled->before_native)||
            (*frame==coupled->after&&native==coupled->after_native)):
            frame->width()==s.initial.width()&&frame->height()==s.initial.height();
        return geometry&&
            DwmGetWindowAttribute(h,DWMWA_CLOAKED,&cloak,sizeof(cloak))==S_OK&&!cloak&&
            GetMonitorInfoW(s.monitor,&monitor)&&Rect{monitor.rcWork.left,monitor.rcWork.top,monitor.rcWork.right,monitor.rcWork.bottom}==s.work;
    }
    static void owner(const std::shared_ptr<State>& s)noexcept{
        s->owner=GetCurrentThreadId();IVirtualDesktopManager* manager{};
        const auto running=[&]{return !s->stopping.load(std::memory_order_acquire);};
        const auto require=[&](bool fact,const char* why){if(!fact){s->setup_reason=why;throw std::runtime_error(why);}};
        try{
            require(running(),"stop_before_owner_setup");
            // No GUI or COM operation precedes the exact desktop bind.
            s->desktop_open_attempted=true;SetLastError(ERROR_SUCCESS);
            s->desktop=OpenDesktopW((L"PaneBindV1_"+s->run).c_str(),0,FALSE,desktop_access);
            s->desktop_error=s->desktop?0:GetLastError();require(s->desktop!=nullptr,"owner_OpenDesktop_failed");
            require(running()&&equal(medium_desktop_object_name(s->desktop),L"PaneBindV1_"+s->run),"owner_exact_Run_desktop_unconfirmed");
            SetLastError(ERROR_SUCCESS);s->desktop_bound=SetThreadDesktop(s->desktop)!=FALSE;s->bind_error=s->desktop_bound?0:GetLastError();
            require(s->desktop_bound,"owner_SetThreadDesktop_failed");
            require(running()&&medium(GetCurrentProcess())&&input_actor_same_user(GetCurrentProcess())&&
                equal(medium_desktop_object_name(GetProcessWindowStation()),L"WinSta0")&&
                equal(medium_desktop_object_name(GetThreadDesktop(s->owner)),L"PaneBindV1_"+s->run)&&
                AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2),
                "owner_Medium_Run_or_PMV2_unconfirmed");
            s->com_attempted=true;s->com_result=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);s->com_owned=SUCCEEDED(s->com_result);
            require(s->com_owned&&running(),"owner_STA_initialize_or_stop");
            const auto before=MediumDesktopWindows::read(s->desktop);WNDCLASSEXW old{sizeof(old)};SetLastError(ERROR_SUCCESS);
            const bool exists=GetClassInfoExW(s->instance,s->name.c_str(),&old)!=FALSE;const auto query_error=GetLastError();
            require(before.complete&&!exists&&query_error==ERROR_CLASS_DOES_NOT_EXIST&&GetForegroundWindow()==s->source&&running(),
                "owner_new_class_baseline_or_foreground_unconfirmed");
            WNDCLASSEXW wc{sizeof(wc)};wc.hInstance=s->instance;wc.lpfnWndProc=procedure;wc.lpszClassName=s->name.c_str();
            wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
            SetLastError(ERROR_SUCCESS);s->registered=RegisterClassExW(&wc)!=0;s->register_error=s->registered?0:GetLastError();
            require(s->registered&&running(),"owner_RegisterClass_failed_or_stop");
            SetLastError(ERROR_SUCCESS);const HWND h=CreateWindowExW(WS_EX_NOACTIVATE,s->name.c_str(),L"PaneBind exact Run blank target",WS_OVERLAPPEDWINDOW,
                s->initial.l,s->initial.t,s->initial.width(),s->initial.height(),nullptr,nullptr,s->instance,s.get());
            s->create_error=h?0:GetLastError();s->created=h!=nullptr;s->hwnd.store(h,std::memory_order_release);
            require(h&&running(),"owner_CreateWindow_failed_or_stop");
            require(std::find(before.windows.begin(),before.windows.end(),h)==before.windows.end(),"owner_baseline_identity_collision");
            ShowWindow(h,SW_SHOWNOACTIVATE);RECT p{};const auto first=visible(h);
            require(first&&GetWindowRect(h,&p)&&GetForegroundWindow()==s->source&&running(),"owner_frame_or_foreground_unknown");
            const LONG left=s->initial.l-(first->l-p.left),top=s->initial.t-(first->t-p.top);
            const LONG width=s->initial.width()+(p.right-p.left-first->width()),height=s->initial.height()+(p.bottom-p.top-first->height());
            SetLastError(ERROR_SUCCESS);s->placed=SetWindowPos(h,nullptr,left,top,width,height,SWP_NOACTIVATE|SWP_NOZORDER)!=FALSE;
            s->placement_error=s->placed?0:GetLastError();require(s->placed&&running(),"owner_setup_placement_failed_or_stop");
            s->vdm_create=CoCreateInstance(CLSID_VirtualDesktopManager,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager));
            if(manager&&running())s->vdm_query=manager->IsWindowOnCurrentVirtualDesktop(h,&s->vdm_current);
            if(manager){manager->Release();manager=nullptr;}
            if(GetWindowRect(h,&p))s->positioning=Rect{p.left,p.top,p.right,p.bottom};
            require(s->positioning&&s->vdm_create==S_OK&&s->vdm_query==S_OK&&s->vdm_current&&
                exact_state(*s,s->desktop)&&visible(h)==s->initial&&GetForegroundWindow()==s->source&&running(),
                "owner_actual_frame_VDM_or_foreground_unconfirmed");
            s->setup_known=true;s->setup_reason="independent_UI_owner_ready";
            s->facts_ready.store(true,std::memory_order_release);SetEvent(s->ready);
            for(;;){
                const auto waited=MsgWaitForMultipleObjectsEx(1,&s->stop,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
                if(waited==WAIT_OBJECT_0)break;
                if(waited!=WAIT_OBJECT_0+1){s->wait_error=waited==WAIT_FAILED?GetLastError():ERROR_INVALID_FUNCTION;break;}
                MSG message{};unsigned budget=256;
                while(budget--&&running()&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
                    if(message.message==WM_QUIT){s->wait_error=ERROR_OPERATION_ABORTED;s->stopping=true;break;}
                    TranslateMessage(&message);DispatchMessageW(&message);
                }
                if(!running())break;
            }
        }catch(...){if(!s->facts_ready.load(std::memory_order_acquire))s->setup_known=false;}
        if(manager)manager->Release();
        const HWND h=s->hwnd.load(std::memory_order_acquire);
        if(h){
            s->destroy_attempted=true;DWORD pid{};
            s->destroyed=GetWindowThreadProcessId(h,&pid)==s->owner&&pid==s->pid&&
                GetWindowLongPtrW(h,GWLP_USERDATA)==reinterpret_cast<LONG_PTR>(s.get())&&DestroyWindow(h)&&!IsWindow(h);
            if(s->destroyed)s->hwnd.store(nullptr,std::memory_order_release);
        }
        if(s->registered){s->unregister_attempted=true;s->unregistered=s->destroyed&&UnregisterClassW(s->name.c_str(),s->instance)!=FALSE;}
        if(s->com_owned){CoUninitialize();s->com_owned=false;s->com_exited=true;}
        const bool cleaned=s->destroyed&&s->unregistered&&s->wait_error==0;
        s->worker_cleanup_known.store(cleaned,std::memory_order_release);
        // Preserve state on an unconfirmed owner cleanup. No dangling callback
        // state, kill, endless join or false cleanup PASS in this failed guest.
        if(!cleaned)retain_unknown(s);
        if(!s->facts_ready.exchange(true,std::memory_order_acq_rel))SetEvent(s->ready);
    }
public:
    Rect initial{};Rect work{};std::optional<Rect> positioning;
    MediumDesktopMagnetTarget()=default;
    MediumDesktopMagnetTarget(const MediumDesktopMagnetTarget&)=delete;
    ~MediumDesktopMagnetTarget(){finish();}
    HWND window()const noexcept{return state_?state_->hwnd.load(std::memory_order_acquire):nullptr;}
    bool exact()const{
        return state_&&state_->facts_ready.load(std::memory_order_acquire)&&state_->setup_known&&
            GetCurrentThreadId()==state_->parent&&exact_state(*state_,parent_desktop_,coupled_?&*coupled_:nullptr);
    }
    bool begin_shared_right_resize(Rect before,Rect before_native,LONG delta){
        // Parent TEST-only scope: this one declared shared edge has two states,
        // not arbitrary accepted geometry or a new product baseline.
        // Rearming only follows a locked singleton, never an unresolved pair.
        if((coupled_&&(coupled_->before!=coupled_->after||coupled_->before_native!=coupled_->after_native))||
            !exact()||visible(window())!=std::optional<Rect>{before}||delta<=0||
            static_cast<std::int64_t>(before.l)+delta>=before.r||static_cast<std::int64_t>(before_native.l)+delta>=before_native.r)return false;
        RECT actual{};if(!GetWindowRect(window(),&actual)||Rect{actual.left,actual.top,actual.right,actual.bottom}!=before_native)return false;
        if(coupled_&&(coupled_->before!=before||coupled_->before_native!=before_native))return false;
        const Rect after{before.l+delta,before.t,before.r,before.b},native{before_native.l+delta,before_native.t,before_native.r,before_native.b};
        if(after.width()<=0||native.width()<=0||after.l<work.l||after.t<work.t||after.r>work.r||after.b>work.b)return false;
        coupled_=CoupledGeometry{before,after,before_native,native};return exact();
    }
    bool finish_shared_right_resize(bool accepted){
        if(!coupled_||!exact())return false;
        const auto wanted=accepted?coupled_->after:coupled_->before;
        const auto native=accepted?coupled_->after_native:coupled_->before_native;
        RECT actual{};if(visible(window())!=std::optional<Rect>{wanted}||!GetWindowRect(window(),&actual)||
            Rect{actual.left,actual.top,actual.right,actual.bottom}!=native)return false;
        // Lock the measured outcome to the already declared singleton.
        coupled_=CoupledGeometry{wanted,wanted,native,native};return exact();
    }
    void create(const std::wstring& run,HDESK desktop,DWORD session,HWND source,Rect desired,UINT dpi,HMONITOR monitor,
        Rect actual_work,MediumDesktopProductGeometryObservation& out){
        if(state_||thread_.joinable())throw std::runtime_error("geometry_blank_target_already_created");
        if(run.size()!=32||run.find_first_not_of(L"0123456789abcdef")!=run.npos||!desktop||!source||!session||!dpi||!monitor||
            !medium(GetCurrentProcess())||GetForegroundWindow()!=source||
            !equal(medium_desktop_object_name(desktop),L"PaneBindV1_"+run)||
            !equal(medium_desktop_object_name(GetThreadDesktop(GetCurrentThreadId())),L"PaneBindV1_"+run))
            throw std::runtime_error("geometry_blank_target_parent_exact_Run_unconfirmed");
        state_=std::make_shared<State>();auto& s=*state_;parent_desktop_=desktop;output_=&out;
        s.run=run;s.name=L"PaneBind.V1.MediumMagnetTarget."+run;s.parent=GetCurrentThreadId();s.pid=GetCurrentProcessId();
        s.birth=process_birth(GetCurrentProcess());s.session=session;s.source=source;s.dpi=dpi;s.monitor=monitor;s.initial=desired;s.work=actual_work;
        initial=desired;work=actual_work;
        if(!s.stop||!s.ready||!s.birth)throw std::runtime_error("geometry_blank_target_owner_events_or_birth_unavailable");
        thread_=std::thread([state=state_]{owner(state);});thread_started_=true;
        const auto waited=WaitForSingleObject(s.ready,2000);
        const bool ready=waited==WAIT_OBJECT_0&&s.facts_ready.load(std::memory_order_acquire);
        out.emit("medium_product_geometry_blank_target_owner","\"independent_UI_thread\":true,\"parent_TID\":"+std::to_string(s.parent)+
            ",\"ready_wait_return\":"+std::to_string(waited)+",\"ready_wait_cap_ms\":2000,\"ready_facts_observed\":"+(ready?"true":"false")+
            ",\"owner_TID\":"+(ready?std::to_string(s.owner):"null")+",\"desktop_requested_access\":"+std::to_string(desktop_access)+
            ",\"OpenDesktop_attempted\":"+(ready?(s.desktop_open_attempted?"true":"false"):"null")+
            ",\"OpenDesktop_error\":"+(ready?std::to_string(s.desktop_error):"null")+",\"SetThreadDesktop_return\":"+(ready?(s.desktop_bound?"true":"false"):"null")+
            ",\"SetThreadDesktop_error\":"+(ready?std::to_string(s.bind_error):"null")+",\"STA_initialize_attempted\":"+(ready?(s.com_attempted?"true":"false"):"null")+
            ",\"STA_hresult\":"+(ready&&s.com_attempted?std::to_string(static_cast<long>(s.com_result)):"null")+
            ",\"worker_disk_IO\":false,\"new_input_or_product_writer\":false");
        if(!ready)throw std::runtime_error("geometry_blank_target_owner_ready_not_observed");
        positioning=s.positioning;const bool actual=s.setup_known&&exact();const auto h=window();
        const auto frame=h?visible(h):std::optional<Rect>{};
        out.emit("medium_product_geometry_blank_target","\"HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
            ",\"PID\":"+std::to_string(s.pid)+",\"TID\":"+std::to_string(s.owner)+",\"birth\":"+std::to_string(s.birth)+
            ",\"class\":"+quote(utf8(s.name))+",\"RegisterClass_return\":"+(s.registered?"true":"false")+
            ",\"register_error\":"+std::to_string(s.register_error)+",\"CreateWindow_return\":"+(s.created?"true":"false")+",\"create_error\":"+std::to_string(s.create_error)+
            ",\"fixture_SetWindowPos_return\":"+(s.placed?"true":"false")+",\"placement_error\":"+std::to_string(s.placement_error)+
            ",\"requested_visible\":"+rect(desired)+",\"actual_visible\":"+(frame?rect(*frame):"null")+
            ",\"actual_positioning\":"+(positioning?rect(*positioning):"null")+",\"actual_DPI\":"+(h?std::to_string(GetDpiForWindow(h)):"null")+
            ",\"actual_exact_identity_membership\":"+(actual?"true":"false")+",\"VDM_create_hresult\":"+std::to_string(static_cast<long>(s.vdm_create))+
            ",\"VDM_query_hresult\":"+std::to_string(static_cast<long>(s.vdm_query))+",\"VDM_current\":"+(s.vdm_query==S_OK?(s.vdm_current?"true":"false"):"null")+
            ",\"foreground_unchanged\":"+(GetForegroundWindow()==source?"true":"false")+",\"owner_setup_reason\":"+quote(s.setup_reason)+",\"setup_is_not_product_write\":true");
        if(!actual||!positioning||frame!=std::optional<Rect>{desired}||GetForegroundWindow()!=source)
            throw std::runtime_error("geometry_blank_target_independent_owner_setup_unconfirmed");
    }
    bool finish()noexcept{
        if(finish_attempted_)return finish_known_;finish_attempted_=true;
        if(!state_)return finish_known_=true;
        try{
            state_->stopping.store(true,std::memory_order_release);
            const bool signalled=state_->stop&&SetEvent(state_->stop)!=FALSE;
            DWORD waited=WAIT_OBJECT_0;const bool wait_attempted=thread_.joinable();
            if(thread_.joinable()){
                waited=WaitForSingleObject(thread_.native_handle(),1000);
                if(waited!=WAIT_OBJECT_0){retain_unknown(state_);thread_.detach();}
                else thread_.join();
            }
            const bool exited=waited==WAIT_OBJECT_0;
            const bool cleaned=exited&&(!thread_started_||state_->worker_cleanup_known.load(std::memory_order_acquire));
            const bool handles_closed=exited&&state_->close_handles();
            finish_known_=signalled&&exited&&cleaned&&handles_closed;
            if(output_)output_->emit("medium_product_geometry_blank_target_owner_cleanup",
                "\"stop_event_set\":"+std::string(signalled?"true":"false")+",\"thread_started\":"+(thread_started_?"true":"false")+
                ",\"thread_wait_attempted\":"+(wait_attempted?"true":"false")+",\"thread_wait_return\":"+(wait_attempted?std::to_string(waited):"null")+
                ",\"thread_wait_cap_ms\":1000,\"actual_thread_exited\":"+(thread_started_?(exited?"true":"false"):"null")+
                ",\"owner_DestroyWindow_attempted\":"+(exited?(state_->destroy_attempted?"true":"false"):"null")+
                ",\"owner_window_destroyed\":"+(exited&&state_->destroy_attempted?(state_->destroyed?"true":"false"):"null")+
                ",\"owner_UnregisterClass_attempted\":"+(exited?(state_->unregister_attempted?"true":"false"):"null")+
                ",\"owner_class_unregistered\":"+(exited&&state_->unregister_attempted?(state_->unregistered?"true":"false"):"null")+
                ",\"owner_COM_uninitialized\":"+(exited&&SUCCEEDED(state_->com_result)?(state_->com_exited?"true":"false"):"null")+
                ",\"owned_handles_closed\":"+(exited?(handles_closed?"true":"false"):"null")+
                ",\"cleanup_known\":"+(finish_known_?"true":"false")+",\"forced_termination\":false,\"guest_disposal_is_not_cleanup_PASS\":true");
            return finish_known_;
        }catch(...){
            if(thread_.joinable()){try{retain_unknown(state_);thread_.detach();}catch(...){}}
            return false;
        }
    }
};
