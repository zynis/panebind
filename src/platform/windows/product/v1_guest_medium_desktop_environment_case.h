#pragma once

// TEST ONLY. One fresh desktop and one same-token Explorer observation.
// No product, synthetic input, input-desktop switch, token or ACL changes.
bool input_actor_same_user(HANDLE);

std::wstring medium_desktop_object_name(HANDLE object){
    std::array<wchar_t,128> name{};DWORD size{};
    if(!object||!GetUserObjectInformationW(object,UOI_NAME,name.data(),static_cast<DWORD>(sizeof(name)),&size))return {};
    return name.data();
}
std::wstring medium_desktop_image(HANDLE process){
    std::array<wchar_t,32768> image{};DWORD size=static_cast<DWORD>(image.size());
    if(!process||!QueryFullProcessImageNameW(process,0,image.data(),&size)||!size||size>=image.size())return {};
    return {image.data(),size};
}
struct MediumDesktopDefaultShell {
    HWND hwnd{};DWORD pid{},tid{};std::uint64_t birth{};std::wstring image;bool known{};
    static MediumDesktopDefaultShell read(){
        MediumDesktopDefaultShell out;out.hwnd=GetShellWindow();
        out.tid=GetWindowThreadProcessId(out.hwnd,&out.pid);
        Handle process(out.pid?OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,out.pid):nullptr);
        if(process.value){out.birth=process_birth(process.value);out.image=medium_desktop_image(process.value);
            DWORD pid{};const auto tid=GetWindowThreadProcessId(out.hwnd,&pid);
            out.known=out.hwnd&&out.pid&&out.tid&&out.birth&&!out.image.empty()&&
                WaitForSingleObject(process.value,0)==WAIT_TIMEOUT&&pid==out.pid&&tid==out.tid&&
                GetShellWindow()==out.hwnd&&equal(medium_desktop_object_name(GetThreadDesktop(out.tid)),L"Default");}
        return out;
    }
    bool same(const MediumDesktopDefaultShell& other)const{
        return known&&other.known&&hwnd==other.hwnd&&pid==other.pid&&tid==other.tid&&birth==other.birth&&equal(image,other.image);
    }
    std::string json()const{return "{\"known\":"+std::string(known?"true":"false")+",\"HWND\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(hwnd))+",\"PID\":"+std::to_string(pid)+",\"TID\":"+
        std::to_string(tid)+",\"birth\":"+std::to_string(birth)+",\"image\":"+quote(utf8(image))+"}";}
};
struct MediumDesktopWindows {
    std::vector<HWND> windows;bool complete{};DWORD error{};
    static MediumDesktopWindows read(HDESK target){
        MediumDesktopWindows out;out.windows.reserve(256);SetLastError(ERROR_SUCCESS);
        out.complete=EnumDesktopWindows(target,[](HWND h,LPARAM context)->BOOL{
            auto& rows=*reinterpret_cast<std::vector<HWND>*>(context);
            if(rows.size()>=256){SetLastError(ERROR_MORE_DATA);return FALSE;}rows.push_back(h);return TRUE;
        },reinterpret_cast<LPARAM>(&out.windows))!=FALSE;
        out.error=out.complete?0:GetLastError();return out;
    }
};
struct MediumDesktopWindowFact {
    HWND hwnd{};DWORD pid{},tid{};std::wstring type,desktop_name;RECT rectangle{};
    bool known{},root{},visible{},exact_child{};
    HDESK thread_desktop{};bool thread_query_attempted{},name_query_attempted{},name_query_return{};
    DWORD thread_query_error{},name_query_error{};
    static MediumDesktopWindowFact read(HWND h,DWORD child,HANDLE process,std::uint64_t birth,const std::wstring& desktop_name){
        MediumDesktopWindowFact out;out.hwnd=h;out.tid=GetWindowThreadProcessId(h,&out.pid);
        std::array<wchar_t,128> type{};const bool class_known=GetClassNameW(h,type.data(),static_cast<int>(type.size()))>0;
        out.type=type.data();out.thread_query_attempted=out.tid!=0;
        if(out.thread_query_attempted){SetLastError(ERROR_SUCCESS);out.thread_desktop=GetThreadDesktop(out.tid);
            out.thread_query_error=out.thread_desktop?0:GetLastError();}
        if(out.thread_desktop){std::array<wchar_t,128> name{};DWORD size{};out.name_query_attempted=true;
            SetLastError(ERROR_SUCCESS);out.name_query_return=GetUserObjectInformationW(out.thread_desktop,UOI_NAME,
                name.data(),static_cast<DWORD>(sizeof(name)),&size)!=FALSE;
            out.name_query_error=out.name_query_return?0:GetLastError();
            if(out.name_query_return)out.desktop_name=name.data();}
        out.root=GetAncestor(h,GA_ROOT)==h;out.visible=IsWindowVisible(h)!=FALSE;
        DWORD after_pid{};const auto after_tid=GetWindowThreadProcessId(h,&after_pid);
        out.known=h&&out.pid&&out.tid&&class_known&&!out.desktop_name.empty()&&GetWindowRect(h,&out.rectangle)&&
            IsWindow(h)&&after_pid==out.pid&&after_tid==out.tid;
        out.exact_child=out.known&&out.root&&out.pid==child&&process&&GetProcessId(process)==child&&birth&&
            process_birth(process)==birth&&WaitForSingleObject(process,0)==WAIT_TIMEOUT&&equal(out.desktop_name,desktop_name);
        return out;
    }
    bool same(const MediumDesktopWindowFact& other)const{
        return known&&other.known&&hwnd==other.hwnd&&pid==other.pid&&tid==other.tid&&type==other.type&&
            desktop_name==other.desktop_name&&root==other.root&&visible==other.visible&&exact_child==other.exact_child&&
            EqualRect(&rectangle,&other.rectangle);
    }
    std::string json()const{return "{\"HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hwnd))+
        ",\"PID\":"+std::to_string(pid)+",\"TID\":"+std::to_string(tid)+",\"class\":"+quote(utf8(type))+
        ",\"thread_desktop\":"+quote(utf8(desktop_name))+",\"known\":"+(known?"true":"false")+
        ",\"GetThreadDesktop_attempted\":"+(thread_query_attempted?"true":"false")+
        ",\"GetThreadDesktop_return\":"+(thread_query_attempted?std::to_string(reinterpret_cast<std::uintptr_t>(thread_desktop)):"null")+
        ",\"GetThreadDesktop_error\":"+(thread_query_attempted?std::to_string(thread_query_error):"null")+
        ",\"GetUserObjectInformation_name_attempted\":"+(name_query_attempted?"true":"false")+
        ",\"GetUserObjectInformation_name_return\":"+(name_query_attempted?(name_query_return?"true":"false"):"null")+
        ",\"GetUserObjectInformation_name_error\":"+(name_query_attempted?std::to_string(name_query_error):"null")+
        ",\"root\":"+(root?"true":"false")+",\"visible\":"+(visible?"true":"false")+
        ",\"exact_returned_child\":"+(exact_child?"true":"false")+",\"positioning_rect\":["+
        std::to_string(rectangle.left)+","+std::to_string(rectangle.top)+","+std::to_string(rectangle.right)+","+
        std::to_string(rectangle.bottom)+"]}";}
};
struct MediumDesktopEnvironmentObservation {
    std::vector<std::pair<std::string,std::string>> records;
    bool enumerated_ready{},handle_closed{},thread_restored{},event_hook_stopped{true};std::string reason{"not_started"};
    void emit(std::string type,std::string fields){records.emplace_back(std::move(type),std::move(fields));}
};
struct MediumDesktopWindowEvents {
    struct Fact {DWORD event{},pid{},tid{},native_time{};HWND hwnd{};};
    std::array<Fact,64> facts{};std::size_t stored{};std::uint64_t relevant{};bool overflow{};
    DWORD child_pid{};HWINEVENTHOOK hook{};
};
thread_local MediumDesktopWindowEvents* medium_desktop_window_events{};
void CALLBACK medium_desktop_window_event(HWINEVENTHOOK hook,DWORD event,HWND hwnd,LONG object,LONG child,
    DWORD event_thread,DWORD native_time){
    auto* context=medium_desktop_window_events;
    if(!context||context->hook!=hook||!context->child_pid||!hwnd||object!=OBJID_WINDOW||child!=0)return;
    DWORD pid{};const auto tid=GetWindowThreadProcessId(hwnd,&pid);
    if(!tid||pid!=context->child_pid)return;
    if(context->relevant!=UINT64_MAX)++context->relevant;
    if(context->stored<context->facts.size())context->facts[context->stored++]={event,pid,event_thread,native_time,hwnd};
    else context->overflow=true;
}
void medium_desktop_environment_probe(const std::wstring& run,const fs::path& executable,MediumDesktopEnvironmentObservation& out){
    // A fresh hook/window-free TEST thread connects to the new object. The
    // fixture STA and its original windows remain on Default throughout.
    const HDESK original=GetThreadDesktop(GetCurrentThreadId());HDESK created{};HANDLE child{},child_thread{};
    const auto name=L"PaneBindV1_"+run;const auto full_name=L"WinSta0\\"+name;
    MediumDesktopWindowEvents events;HWINEVENTHOOK event_hook{};
    try{
        if(!equal(medium_desktop_object_name(original),L"Default")||!medium(GetCurrentProcess()))
            throw std::runtime_error("fresh_observer_thread_not_original_Default_Medium");
        struct Existing {const std::wstring* name{};unsigned count{},matches{};} existing{&name};
        SetLastError(ERROR_SUCCESS);
        const bool enumerated=EnumDesktopsW(GetProcessWindowStation(),[](LPWSTR actual,LPARAM context)->BOOL{
            auto& e=*reinterpret_cast<Existing*>(context);if(++e.count>128){SetLastError(ERROR_MORE_DATA);return FALSE;}
            if(equal(actual,*e.name))++e.matches;return TRUE;
        },reinterpret_cast<LPARAM>(&existing))!=FALSE;
        const DWORD enum_error=enumerated?0:GetLastError();
        SetLastError(ERROR_SUCCESS);const HDESK before=OpenDesktopW(name.c_str(),0,FALSE,DESKTOP_READOBJECTS);
        const DWORD open_error=before?0:GetLastError();const bool before_closed=!before||CloseDesktop(before)!=FALSE;
        out.emit("medium_desktop_fresh_name","\"desktop_name\":"+quote(utf8(name))+
            ",\"EnumDesktops_return\":"+(enumerated?"true":"false")+",\"EnumDesktops_error\":"+std::to_string(enum_error)+
            ",\"visited\":"+std::to_string(existing.count)+",\"matching_existing_names\":"+std::to_string(existing.matches)+
            ",\"OpenDesktop_returned_existing\":"+(before?"true":"false")+",\"OpenDesktop_error\":"+std::to_string(open_error)+
            ",\"preexisting_handle_closed\":"+(before_closed?"true":"false")+",\"baseline_is_not_atomic_CREATE_NEW\":true");
        if(!enumerated||existing.matches||before||open_error!=ERROR_FILE_NOT_FOUND||!before_closed)
            throw std::runtime_error("Run_desktop_name_preexisting_or_freshness_unknown");
        constexpr ACCESS_MASK access=DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE;
        SetLastError(ERROR_SUCCESS);created=CreateDesktopW(name.c_str(),nullptr,nullptr,0,access,nullptr);
        const DWORD create_error=created?0:GetLastError();
        const auto created_name=created?medium_desktop_object_name(created):std::wstring{};
        out.emit("medium_desktop_create","\"desktop_name\":"+quote(utf8(name))+",\"CreateDesktop_return\":"+
            (created?"true":"false")+",\"win32_error\":"+std::to_string(create_error)+",\"requested_access\":"+std::to_string(access)+
            ",\"actual_created_object_name\":"+quote(utf8(created_name))+
            ",\"flags\":0,\"security_attributes_NULL\":true,\"security_descriptor_inherited_unmodified\":true,\"input_desktop_switched\":false");
        if(!created||!equal(created_name,name))throw std::runtime_error("CreateDesktop_failed_or_exact_Run_object_unverified_inherited_security_not_changed");
        const auto baseline=MediumDesktopWindows::read(created);
        // A new desktop was actually observed returning FALSE/error0 without
        // invoking the callback. Do not relabel that as a complete empty
        // inventory, or reject this authorized child creation as a Shell
        // capability failure before trying it. Fresh name/object identity is
        // independent; no baseline window is adopted or controlled. Readiness
        // below still requires complete fresh enumeration and exact new child.
        const bool unknown_zero_baseline=!baseline.complete&&baseline.error==ERROR_SUCCESS&&baseline.windows.empty();
        out.emit("medium_desktop_window_baseline","\"enumeration_complete\":"+std::string(baseline.complete?"true":"false")+
            ",\"win32_error\":"+std::to_string(baseline.error)+",\"window_count\":"+std::to_string(baseline.windows.size())+
            ",\"zero_callback_error0_inventory_unknown\":"+(unknown_zero_baseline?"true":"false")+
            ",\"baseline_is_not_Shell_readiness_or_window_authority\":true");
        if(!baseline.windows.empty()||(!baseline.complete&&!unknown_zero_baseline))
            throw std::runtime_error("fresh_desktop_has_observed_windows_or_enumeration_error");
        SetLastError(ERROR_SUCCESS);const bool attached=SetThreadDesktop(created)!=FALSE;const DWORD attach_error=attached?0:GetLastError();
        out.emit("medium_desktop_observer_connection","\"SetThreadDesktop_return\":"+std::string(attached?"true":"false")+
            ",\"win32_error\":"+std::to_string(attach_error)+",\"actual_thread_desktop\":"+
            quote(utf8(medium_desktop_object_name(GetThreadDesktop(GetCurrentThreadId()))))+
            ",\"only_fresh_TEST_thread\":true,\"SwitchDesktop_attempted\":false");
        if(!attached||!equal(medium_desktop_object_name(GetThreadDesktop(GetCurrentThreadId())),name))
            throw std::runtime_error("new_desktop_observer_connection_not_verified");
        // This hook observes only this new desktop. Its callback cannot grant
        // window authority or grow records; the existing exact reader remains
        // the sole readiness decision. Install before child creation so its
        // first window events are not lost during startup.
        medium_desktop_window_events=&events;
        SetLastError(ERROR_SUCCESS);event_hook=SetWinEventHook(EVENT_OBJECT_CREATE,EVENT_OBJECT_SHOW,nullptr,
            medium_desktop_window_event,0,0,WINEVENT_OUTOFCONTEXT);events.hook=event_hook;
        const DWORD hook_error=event_hook?0:GetLastError();out.event_hook_stopped=event_hook==nullptr;
        out.emit("medium_desktop_readiness_hook","\"SetWinEventHook_attempted\":true,\"native_return\":"+
            std::string(event_hook?"true":"false")+",\"win32_error\":"+std::to_string(hook_error)+
            ",\"owner_TID\":"+std::to_string(GetCurrentThreadId())+",\"desktop_name\":"+quote(utf8(name))+
            ",\"event_min\":"+std::to_string(EVENT_OBJECT_CREATE)+",\"event_max\":"+std::to_string(EVENT_OBJECT_SHOW)+
            ",\"out_of_context\":true,\"observation_only\":true,\"callback_capacity\":64");
        if(!event_hook)throw std::runtime_error("new_desktop_readiness_event_hook_failed");
        const auto observation_begin=GetTickCount64();const auto observation_deadline=observation_begin+10000;
        STARTUPINFOW startup{};startup.cb=sizeof(startup);auto requested_desktop=full_name;startup.lpDesktop=requested_desktop.data();
        PROCESS_INFORMATION info{};auto command=L"\""+executable.wstring()+L"\"";
        SetLastError(ERROR_SUCCESS);const bool launched=CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,0,
            nullptr,executable.parent_path().c_str(),&startup,&info)!=FALSE;
        const DWORD launch_error=launched?0:GetLastError();child=info.hProcess;child_thread=info.hThread;
        const auto birth=launched?process_birth(child):0;const auto image=launched?medium_desktop_image(child):std::wstring{};
        DWORD session{},own_session{};int rid=-1;bool elevated{};DWORD token_error{};
        const bool normal=launched&&medium(child,&rid,&elevated,&token_error);
        const bool same_sid=launched&&input_actor_same_user(child);
        const bool same_session=launched&&ProcessIdToSessionId(info.dwProcessId,&session)&&
            ProcessIdToSessionId(GetCurrentProcessId(),&own_session)&&session&&session==own_session;
        const bool child_identity=launched&&info.dwProcessId&&info.dwThreadId&&birth&&normal&&same_sid&&same_session&&
            equal(image,executable.wstring())&&GetProcessId(child)==info.dwProcessId;
        out.emit("medium_desktop_Explorer_create","\"CreateProcessW_return\":"+std::string(launched?"true":"false")+
            ",\"win32_error\":"+std::to_string(launch_error)+",\"lpDesktop\":"+quote(utf8(full_name))+
            ",\"PID\":"+std::to_string(info.dwProcessId)+",\"primary_TID\":"+std::to_string(info.dwThreadId)+
            ",\"birth\":"+std::to_string(birth)+",\"image\":"+quote(utf8(image))+",\"integrity_rid\":"+std::to_string(rid)+
            ",\"elevated\":"+(rid>=0?(elevated?"true":"false"):"null")+",\"token_query_error\":"+std::to_string(token_error)+
            ",\"same_SID\":"+(same_sid?"true":"false")+",\"same_interactive_session\":"+(same_session?"true":"false")+
            ",\"session\":"+std::to_string(session)+",\"exact_child_identity\":"+(child_identity?"true":"false")+
            ",\"same_existing_Medium_token_CreateProcessW\":true,\"token_or_account_modified\":false,\"handle_inheritance\":false");
        if(!child_identity)throw std::runtime_error("new_Explorer_process_identity_not_verified");
        events.child_pid=info.dwProcessId;
        const auto before_idle=GetTickCount64();
        const DWORD idle_budget=before_idle<observation_deadline?static_cast<DWORD>(observation_deadline-before_idle):0;
        SetLastError(ERROR_SUCCESS);const auto idle=WaitForInputIdle(child,idle_budget);const DWORD idle_error=idle==WAIT_FAILED?GetLastError():0;
        out.emit("medium_desktop_single_readiness_wait","\"WaitForInputIdle_return\":"+std::to_string(idle)+
            ",\"win32_error\":"+std::to_string(idle_error)+",\"timeout_ms\":"+std::to_string(idle_budget)+
            ",\"calls\":1,\"total_observation_deadline_ms\":10000,\"idle_is_not_Shell_readiness\":true");
        unsigned event_samples{},wait_calls{};std::uint64_t sampled_events{};bool child_exited{},quit{};
        const auto observe=[&](const char* stage){
            const auto sample_begin=GetTickCount64();const HWND shell_before=GetShellWindow();
            const auto first=MediumDesktopWindows::read(created);
            std::vector<MediumDesktopWindowFact> facts;facts.reserve(first.windows.size());
            for(const HWND h:first.windows)facts.push_back(MediumDesktopWindowFact::read(h,info.dwProcessId,child,birth,name));
            const auto second=MediumDesktopWindows::read(created);const HWND shell_after=GetShellWindow();
            bool stable=first.complete&&second.complete&&first.windows==second.windows&&shell_before&&shell_before==shell_after;
            unsigned exact_shell{},exact_tray{};std::string rows="[";
            for(std::size_t n=0;n<facts.size();++n){const auto fresh=MediumDesktopWindowFact::read(facts[n].hwnd,info.dwProcessId,child,birth,name);
                stable=facts[n].same(fresh)&&stable;if(n)rows+=',';rows+=fresh.json();
                if(fresh.exact_child&&fresh.visible&&fresh.hwnd==shell_after)++exact_shell;
                if(fresh.exact_child&&fresh.visible&&fresh.type==L"Shell_TrayWnd")++exact_tray;}
            rows+=']';
            const bool identity_still=WaitForSingleObject(child,0)==WAIT_TIMEOUT&&process_birth(child)==birth&&
                medium(child)&&input_actor_same_user(child)&&equal(medium_desktop_image(child),executable.wstring());
            out.enumerated_ready=stable&&identity_still&&exact_shell==1&&exact_tray==1;
            out.emit("medium_desktop_actual_window_observation","\"stage\":"+quote(stage)+
                ",\"sample_begin_tick\":"+std::to_string(sample_begin)+",\"sample_end_tick\":"+std::to_string(GetTickCount64())+
                ",\"child_event_samples\":"+std::to_string(event_samples)+",\"relevant_notifications\":"+std::to_string(events.relevant)+
                ",\"first_enumeration_complete\":"+std::string(first.complete?"true":"false")+
                ",\"first_error\":"+std::to_string(first.error)+",\"second_enumeration_complete\":"+(second.complete?"true":"false")+
                ",\"second_error\":"+std::to_string(second.error)+",\"same_local_snapshots\":"+(stable?"true":"false")+
                ",\"GetShellWindow_before\":"+std::to_string(reinterpret_cast<std::uintptr_t>(shell_before))+
                ",\"GetShellWindow_after\":"+std::to_string(reinterpret_cast<std::uintptr_t>(shell_after))+
                ",\"exact_live_child_identity_still_valid\":"+(identity_still?"true":"false")+
                ",\"exact_visible_Shell_roots\":"+std::to_string(exact_shell)+",\"exact_visible_Tray_roots\":"+std::to_string(exact_tray)+
                ",\"windows\":"+rows+",\"unknown_or_Default_broker_windows_adopted\":false,\"window_text_read\":false");
        };
        observe("initial");sampled_events=events.relevant;
        while(!out.enumerated_ready){
            const auto now=GetTickCount64();if(now>=observation_deadline){observe("deadline_final");break;}
            ++wait_calls;SetLastError(ERROR_SUCCESS);
            const auto waited=MsgWaitForMultipleObjectsEx(1,&child,static_cast<DWORD>(observation_deadline-now),QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(waited==WAIT_OBJECT_0){child_exited=true;break;}
            if(waited==WAIT_TIMEOUT){observe("deadline_final");break;}
            if(waited!=WAIT_OBJECT_0+1){const DWORD error=waited==WAIT_FAILED?GetLastError():0;
                out.emit("medium_desktop_readiness_wait_failure","\"native_return\":"+std::to_string(waited)+
                    ",\"win32_error\":"+std::to_string(error));throw std::runtime_error("new_desktop_readiness_event_wait_failed");}
            MSG message{};unsigned dispatched{};
            while(dispatched<256&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
                ++dispatched;if(message.message==WM_QUIT){quit=true;break;}
                TranslateMessage(&message);DispatchMessageW(&message);if(GetTickCount64()>=observation_deadline)break;
            }
            if(quit)break;
            // Coalesce a message batch to one actual observation; cap retained
            // samples while still pumping and allowing the final deadline read.
            if(events.relevant!=sampled_events&&event_samples<16){sampled_events=events.relevant;
                ++event_samples;observe("child_window_event");}
        }
        const auto observation_end=GetTickCount64();
        out.emit("medium_desktop_readiness_interval","\"begin_tick\":"+std::to_string(observation_begin)+
            ",\"end_tick\":"+std::to_string(observation_end)+",\"elapsed_ms\":"+std::to_string(observation_end-observation_begin)+
            ",\"total_deadline_ms\":10000,\"deadline_reached\":"+(observation_end>=observation_deadline?"true":"false")+
            ",\"event_wait_calls\":"+std::to_string(wait_calls)+",\"child_event_samples\":"+std::to_string(event_samples)+
            ",\"event_sample_capacity\":16,\"child_exit_observed\":"+(child_exited?"true":"false")+
            ",\"WM_QUIT_observed\":"+(quit?"true":"false")+",\"buffered_emit_tick_is_not_native_call_tick\":true");
        out.reason=out.enumerated_ready?"exact_Medium_child_Shell_and_Tray_enumerated_only":
            child_exited?"exact_Medium_child_exited_before_Shell_readiness":
            quit?"readiness_observer_received_WM_QUIT":"deadline_no_exact_visible_child_Shell_and_Tray";
    }catch(const std::exception& error){out.reason=error.what();}
    if(event_hook){SetLastError(ERROR_SUCCESS);out.event_hook_stopped=UnhookWinEvent(event_hook)!=FALSE;
        const DWORD error=out.event_hook_stopped?0:GetLastError();
        out.emit("medium_desktop_readiness_unhook","\"UnhookWinEvent_attempted\":true,\"native_return\":"+
            std::string(out.event_hook_stopped?"true":"false")+",\"win32_error\":"+std::to_string(error)+
            ",\"owner_TID\":"+std::to_string(GetCurrentThreadId())+",\"before_thread_desktop_restore\":true");}
    // Callback is delivered on this very thread. Keep its stack context alive
    // through actual unhook, then clear TLS even if native cleanup failed; never
    // claim restored readiness on an unconfirmed hook unload.
    medium_desktop_window_events=nullptr;
    std::string event_rows="[";for(std::size_t n=0;n<events.stored;++n){const auto& fact=events.facts[n];if(n)event_rows+=',';
        event_rows+="{\"event\":"+std::to_string(fact.event)+",\"HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(fact.hwnd))+
            ",\"PID\":"+std::to_string(fact.pid)+",\"event_TID\":"+std::to_string(fact.tid)+",\"native_event_time\":"+std::to_string(fact.native_time)+"}";}
    event_rows+=']';out.emit("medium_desktop_readiness_events","\"relevant_notifications\":"+std::to_string(events.relevant)+
        ",\"retained\":"+std::to_string(events.stored)+",\"overflow\":"+(events.overflow?"true":"false")+
        ",\"capacity\":64,\"events\":"+event_rows+",\"events_are_not_window_authority\":true");
    if(!out.event_hook_stopped){out.enumerated_ready=false;out.reason="new_desktop_readiness_hook_unload_unconfirmed";}
    if(child){DWORD exit=STILL_ACTIVE;const bool exit_known=GetExitCodeProcess(child,&exit)!=FALSE;
        const auto waited=WaitForSingleObject(child,0);const bool alive=waited==WAIT_TIMEOUT;
        out.emit("medium_desktop_child_retention","\"actual_wait_result\":"+std::to_string(waited)+
            ",\"exit_code\":"+(exit_known?std::to_string(exit):"null")+",\"actual_process_still_alive\":"+(alive?"true":"false")+
            ",\"WM_CLOSE_or_TerminateProcess_attempted\":false,\"process_exit_inferred_from_HANDLE_close\":false,\"live_child_left_for_exact_guest_job_or_disposal\":"+(alive?"true":"false")+
            ",\"job_or_guest_disposal_is_not_product_cleanup_PASS\":true");}
    if(child_thread)CloseHandle(child_thread);if(child)CloseHandle(child);
    const bool restore_attempted=out.event_hook_stopped;
    SetLastError(ERROR_SUCCESS);out.thread_restored=restore_attempted&&SetThreadDesktop(original)!=FALSE;
    const DWORD restore_error=restore_attempted&&!out.thread_restored?GetLastError():0;
    out.thread_restored=out.thread_restored&&equal(medium_desktop_object_name(GetThreadDesktop(GetCurrentThreadId())),L"Default");
    const bool close_attempted=created!=nullptr;
    SetLastError(ERROR_SUCCESS);out.handle_closed=!created||CloseDesktop(created)!=FALSE;
    const DWORD close_error=out.handle_closed?0:GetLastError();
    out.emit("medium_desktop_handle_cleanup","\"observer_thread_Default_restored\":"+std::string(out.thread_restored?"true":"false")+
        ",\"SetThreadDesktop_restore_attempted\":"+(restore_attempted?"true":"false")+
        ",\"SetThreadDesktop_restore_error\":"+(restore_attempted?std::to_string(restore_error):"null")+
        ",\"readiness_hook_stopped\":"+(out.event_hook_stopped?"true":"false")+",\"CloseDesktop_attempted\":"+(close_attempted?"true":"false")+
        ",\"CloseDesktop_handle_return\":"+(close_attempted?(out.handle_closed?"true":"false"):"null")+
        ",\"CloseDesktop_error\":"+(close_attempted?std::to_string(close_error):"null")+",\"desktop_object_destroyed\":\"UNKNOWN\",\"process_cleanup_PASS_inferred\":false");
}
void medium_desktop_environment_flow(State& s){
    std::array<wchar_t,128> user{};DWORD user_size=static_cast<DWORD>(user.size());
    if(s.suite!="MediumDesktopEnvironmentOnly"||!desktop()||!GetUserNameW(user.data(),&user_size)||
       !equal(user.data(),L"WDAGUtilityAccount")||!medium(GetCurrentProcess())||s.process||s.product_pid||
       !equal(medium_desktop_object_name(GetProcessWindowStation()),L"WinSta0")||
       !equal(s.output.wstring(),L"C:\\PaneBindV1\\Output")||!marker(s.output/L"panebind-v1-output.marker",s.run)||
       !marker(fs::path(L"C:\\PaneBindV1\\Input")/L"panebind-v1-input.marker",s.run)||
       s.run.size()!=32||!std::all_of(s.run.begin(),s.run.end(),[](wchar_t c){return(c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');})||
       !std::all_of(s.frames.begin(),s.frames.end(),[&](const auto& f){return own(s,f.hwnd);}))
        throw std::runtime_error("medium_desktop_environment_requires_original_exact_Default_Medium_Run_guest");
    const auto before=MediumDesktopDefaultShell::read();
    std::array<wchar_t,32768> windows{};const auto length=GetWindowsDirectoryW(windows.data(),static_cast<UINT>(windows.size()));
    const fs::path executable=fs::path(windows.data())/L"explorer.exe";
    const auto attributes=GetFileAttributesW(executable.c_str());
    s.log.emit("medium_desktop_environment_scope","\"run_id\":"+quote(utf8(s.run))+",\"Default_Shell_before\":"+before.json()+
        ",\"one_fresh_Run_desktop\":true,\"product_create_requests\":0,\"synthetic_input_requests\":0,\"SwitchDesktop_attempted\":false,\"Default_Shell_control_attempted\":false,\"host_mutation_attempted\":false");
    if(!before.known||!length||length>=windows.size()||attributes==INVALID_FILE_ATTRIBUTES||
       (attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))){
        s.log.emit("medium_desktop_environment_summary","\"result\":\"BLOCKED\",\"reason\":\"Default_Shell_or_exact_Windows_Explorer_image_unknown\",\"product_result\":\"NOT_RUN\"");s.result=78;return;}
    MediumDesktopEnvironmentObservation observation;
    std::thread probe([&]{medium_desktop_environment_probe(s.run,executable,observation);});probe.join();
    for(const auto& [type,fields]:observation.records)s.log.emit(type,fields);
    const auto after=MediumDesktopDefaultShell::read();const bool original_unchanged=before.same(after)&&desktop();
    const bool ready=observation.enumerated_ready&&observation.event_hook_stopped&&observation.thread_restored&&observation.handle_closed&&original_unchanged;
    s.log.emit("medium_desktop_environment_summary","\"result\":"+quote(ready?"READY_ENUMERATED_ONLY":"BLOCKED")+
        ",\"reason\":"+quote(observation.reason)+",\"Default_Shell_after\":"+after.json()+
        ",\"original_Default_input_and_Shell_identity_unchanged\":"+(original_unchanged?"true":"false")+
        ",\"product_result\":\"NOT_RUN\",\"input_desktop_switch_and_interaction\":\"NOT_RUN\",\"ordinary_full_desktop_acceptance\":\"NOT_PROVEN\",\"guest_disposal_is_not_product_cleanup_PASS\":true");
    s.result=ready?0:78;
}
