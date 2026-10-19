#pragma once

// A finite same-Medium notification-area control, not a Shell installer or a
// generic UI automation layer. No input is sent until own icon cleanup and
// actual exact product-icon geometry have both been observed.
std::string tray_readiness_window(HWND window){
    DWORD pid{};const auto tid=window?GetWindowThreadProcessId(window,&pid):0;DWORD session{};
    const bool session_known=pid&&ProcessIdToSessionId(pid,&session);std::array<wchar_t,256> name{};
    const bool class_known=window&&GetClassNameW(window,name.data(),static_cast<int>(name.size()))>0;
    Handle process(pid?OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid):nullptr);
    int rid=-1;bool elevated{};DWORD token_error{};const bool normal=process.value&&medium(process.value,&rid,&elevated,&token_error);
    return "{\"HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(window))+
        ",\"PID\":"+(pid?std::to_string(pid):"null")+",\"TID\":"+(tid?std::to_string(tid):"null")+
        ",\"class\":"+(class_known?quote(utf8(name.data())):"null")+",\"session\":"+(session_known?std::to_string(session):"null")+
        ",\"integrity_rid\":"+(rid>=0?std::to_string(rid):"null")+",\"elevated\":"+(rid>=0?(elevated?"true":"false"):"null")+
        ",\"Medium_non_elevated_observed\":"+(rid>=0?(normal?"true":"false"):"null")+
        ",\"token_query_raw_error\":"+(process.value?std::to_string(token_error):"null")+
        ",\"window_class_is_not_full_desktop_evidence\":true}";
}
struct TrayReadinessProbe {
    HINSTANCE instance{};std::wstring name;HWND window{};DWORD tid{};ATOM atom{};static constexpr UINT uid=0x5042;
    bool add_attempted{},version_attempted{},delete_attempted{},destroy_attempted{},unregister_attempted{},cleanup_called{};
    BOOL added{},versioned{},deleted{},destroyed{},unregistered{};DWORD add_error{},version_error{},delete_error{},destroy_error{},unregister_error{};
    HRESULT lookup{E_PENDING},lookup_after_delete{E_PENDING};RECT area{},after_area{};
    bool window_identity()const noexcept{
        DWORD pid{};std::array<wchar_t,256> actual{};const int count=window?GetClassNameW(window,actual.data(),static_cast<int>(actual.size())):0;
        return window&&IsWindow(window)&&GetWindowThreadProcessId(window,&pid)==tid&&tid==GetCurrentThreadId()&&
            pid==GetCurrentProcessId()&&GetAncestor(window,GA_ROOT)==window&&count>0&&std::wstring_view(actual.data(),static_cast<std::size_t>(count))==name&&
            GetWindowLongPtrW(window,GWLP_HINSTANCE)==reinterpret_cast<LONG_PTR>(instance);
    }
    bool owns()const noexcept{
        return window_identity()&&GetWindowLongPtrW(window,GWLP_USERDATA)==reinterpret_cast<LONG_PTR>(this);
    }
    void cleanup()noexcept{
        if(cleanup_called)return;cleanup_called=true;
        // Before an ADD was attempted, a failed userdata binding must still
        // release the exact newly-created private-class window. Once added,
        // changed userdata is unknown ownership and never licenses deletion.
        if(window_identity()&&(!add_attempted||owns())){
            if(add_attempted&&!delete_attempted){
                NOTIFYICONDATAW data{};data.cbSize=sizeof(data);data.hWnd=window;data.uID=uid;
                delete_attempted=true;SetLastError(0);deleted=Shell_NotifyIconW(NIM_DELETE,&data);delete_error=GetLastError();
                NOTIFYICONIDENTIFIER id{sizeof(id)};id.hWnd=window;id.uID=uid;lookup_after_delete=Shell_NotifyIconGetRect(&id,&after_area);
            }
            destroy_attempted=true;SetLastError(0);destroyed=DestroyWindow(window);destroy_error=destroyed?0:GetLastError();
        }
        if(atom&&(!window||!IsWindow(window))){
            unregister_attempted=true;SetLastError(0);unregistered=UnregisterClassW(name.c_str(),instance);unregister_error=unregistered?0:GetLastError();
        }
    }
    ~TrayReadinessProbe(){cleanup();}
};
bool tray_readiness_same_medium_probe(State& s){
    if(s.suite!="TrayReadinessOnly"||!medium(GetCurrentProcess())||s.left||s.control||!desktop())
        throw std::runtime_error("tray_readiness_exact_released_Medium_guest_required");
    TrayReadinessProbe probe;probe.instance=s.instance;probe.tid=GetCurrentThreadId();probe.name=L"PaneBind.V1.TrayReadiness."+s.run;
    WNDCLASSW wc{};wc.hInstance=s.instance;wc.lpszClassName=probe.name.c_str();wc.lpfnWndProc=DefWindowProcW;
    probe.atom=RegisterClassW(&wc);if(!probe.atom)throw std::runtime_error("tray_readiness_unique_probe_class_not_created_no_reuse");
    probe.window=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,probe.name.c_str(),L"PaneBind guest notification probe",
        WS_POPUP,0,0,1,1,nullptr,nullptr,s.instance,nullptr);
    if(!probe.window)throw std::runtime_error("tray_readiness_exact_probe_window_not_created");
    SetLastError(0);const auto previous=SetWindowLongPtrW(probe.window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(&probe));
    if((!previous&&GetLastError())||!probe.owns())throw std::runtime_error("tray_readiness_probe_window_identity_not_bound");
    NOTIFYICONDATAW data{};data.cbSize=sizeof(data);data.hWnd=probe.window;data.uID=TrayReadinessProbe::uid;
    data.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;data.uCallbackMessage=WM_APP+0x73;data.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    wcscpy_s(data.szTip,L"PaneBind V1 guest tray readiness probe");
    if(!data.hIcon)throw std::runtime_error("tray_readiness_shared_stock_icon_not_available");
    const auto shell_before=tray_readiness_window(GetShellWindow()),taskbar_before=tray_readiness_window(FindWindowW(L"Shell_TrayWnd",nullptr));
    const auto owner=tray_readiness_window(probe.window);const auto foreground_before=GetForegroundWindow();
    probe.add_attempted=true;SetLastError(0);probe.added=Shell_NotifyIconW(NIM_ADD,&data);probe.add_error=GetLastError();
    if(probe.added){
        data.uVersion=NOTIFYICON_VERSION_4;probe.version_attempted=true;SetLastError(0);
        probe.versioned=Shell_NotifyIconW(NIM_SETVERSION,&data);probe.version_error=GetLastError();
    }
    NOTIFYICONIDENTIFIER id{sizeof(id)};id.hWnd=probe.window;id.uID=TrayReadinessProbe::uid;probe.lookup=Shell_NotifyIconGetRect(&id,&probe.area);
    const bool rectangle=probe.lookup==S_OK&&probe.area.left<probe.area.right&&probe.area.top<probe.area.bottom;
    const bool not_activated=GetForegroundWindow()!=probe.window&&!IsWindowVisible(probe.window);
    const auto actual_window=probe.window;probe.cleanup();
    s.log.emit("tray_same_Medium_probe","\"probe_owner\":"+owner+",\"shell\":"+shell_before+",\"taskbar\":"+taskbar_before+
        ",\"foreground_before\":"+std::to_string(reinterpret_cast<std::uintptr_t>(foreground_before))+
        ",\"uid\":"+std::to_string(TrayReadinessProbe::uid)+",\"NIM_ADD_calls\":1,\"NIM_ADD_returned_nonzero\":"+(probe.added?"true":"false")+
        ",\"NIM_ADD_immediate_GetLastError_raw\":"+std::to_string(probe.add_error)+
        ",\"NIM_SETVERSION_calls\":"+(probe.version_attempted?"1":"0")+",\"requested_version\":4,\"NIM_SETVERSION_returned_nonzero\":"+(probe.version_attempted?(probe.versioned?"true":"false"):"null")+
        ",\"NIM_SETVERSION_immediate_GetLastError_raw\":"+(probe.version_attempted?std::to_string(probe.version_error):"null")+
        ",\"NotifyIcon_GetLastError_extended_error_contract\":false,\"GetRect_hresult\":"+std::to_string(static_cast<long>(probe.lookup))+
        ",\"actual_rect\":"+(rectangle?rect({probe.area.left,probe.area.top,probe.area.right,probe.area.bottom}):"null")+
        ",\"probe_did_not_activate\":"+(not_activated?"true":"false")+",\"NIM_DELETE_attempted\":"+(probe.delete_attempted?"true":"false")+
        ",\"NIM_DELETE_returned_nonzero\":"+(probe.deleted?"true":"false")+",\"NIM_DELETE_immediate_GetLastError_raw\":"+std::to_string(probe.delete_error)+
        ",\"GetRect_after_exact_DELETE_hresult\":"+std::to_string(static_cast<long>(probe.lookup_after_delete))+
        ",\"DestroyWindow_attempted\":"+(probe.destroy_attempted?"true":"false")+",\"DestroyWindow_returned\":"+(probe.destroyed?"true":"false")+
        ",\"DestroyWindow_error\":"+std::to_string(probe.destroy_error)+",\"probe_window_gone\":"+(!IsWindow(actual_window)?"true":"false")+
        ",\"UnregisterClass_attempted\":"+(probe.unregister_attempted?"true":"false")+",\"UnregisterClass_returned\":"+(probe.unregistered?"true":"false")+
        ",\"UnregisterClass_error\":"+std::to_string(probe.unregister_error)+",\"shared_stock_icon_destroyed\":false,\"menu_input_sent\":false,"+
        std::string("\"only_exact_probe_HWND_UID_touched\":true,\"full_Shell_or_tray_feature_PASS_inferred\":false"));
    s.log.check("tray_probe_exact_window_class_removed",probe.destroy_attempted&&probe.destroyed&&!IsWindow(actual_window)&&probe.unregister_attempted&&probe.unregistered);
    if(probe.added)s.log.check("tray_probe_added_icon_exact_DELETE_and_lookup",probe.delete_attempted&&probe.deleted&&probe.lookup_after_delete!=S_OK);
    if(!probe.added||!rectangle||!not_activated){
        s.log.emit("tray_readiness_result","\"result\":\"BLOCKED\",\"reason\":\"same_Medium_owned_notification_probe_not_available\",\"menu_input_sent\":false,"+
            std::string("\"Shell_start_or_settings_changes\":false,\"GetLastError_root_cause_inferred\":false,\"product_tray_failure_cause\":\"UNKNOWN\""));
        return false;
    }
    return true;
}
bool owned_tray_readiness_case(State& s){
    settings_guest_scope(s);
    if(s.suite!="TrayReadinessOnly"||s.left||s.control)throw std::runtime_error("tray_readiness_exact_suite_idle_required");
    if(!tray_readiness_same_medium_probe(s))return false;
    namespace evidence=product::guest::up_evidence_detail;
    const auto receipt_identity=[&](std::string_view row,std::string_view type){
        const auto actual_pid=evidence::number(row,"\"process_id\":");
        return s.product_source.size()==40&&row.ends_with('}')&&evidence::quoted(row,"\"schema\":","panebind-v1/1")&&
            evidence::quoted(row,"\"source_sha\":",s.product_source)&&evidence::quoted(row,"\"type\":",type)&&actual_pid&&*actual_pid==s.product_pid;
    };
    const auto app=FindWindowW(L"PaneBind.V1.Application",L"PaneBind");DWORD pid{};const auto tid=GetWindowThreadProcessId(app,&pid);
    TrayGuestOwner owner{app,tid,process_birth(s.process)};
    if(!app||pid!=s.product_pid||!tray_guest_owner(s,owner))throw std::runtime_error("tray_readiness_exact_product_owner_required");
    NOTIFYICONIDENTIFIER id{sizeof(id)};id.hWnd=owner.app;id.uID=1;RECT before{},after{};
    const HRESULT before_lookup=Shell_NotifyIconGetRect(&id,&before);
    const auto mark=read_product(s).size();const UINT taskbar=RegisterWindowMessageW(L"TaskbarCreated");DWORD_PTR response{};
    const bool sent=taskbar&&tray_guest_owner(s,owner)&&SendMessageTimeoutW(owner.app,taskbar,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,1000,&response)!=FALSE;
    // Command return does not flush the asynchronous product logger. Observe
    // this one message's exact receipt while retaining the existing STA pump.
    std::string registration;const bool registration_bound=sent&&wait([&]{
        registration=record(s,mark,"tray_registration");const auto actual_owner=evidence::number(registration,"\"owner_hwnd\":");
        const auto actual_uid=evidence::number(registration,"\"uid\":");
        return tray_guest_owner(s,owner)&&receipt_identity(registration,"tray_registration")&&actual_owner&&
            *actual_owner==reinterpret_cast<std::uintptr_t>(owner.app)&&actual_uid&&*actual_uid==1;
    },1500);
    const HRESULT after_lookup=Shell_NotifyIconGetRect(&id,&after);
    const bool rectangle=after_lookup==S_OK&&after.left<after.right&&after.top<after.bottom;
    s.log.emit("tray_product_once_TaskbarCreated_control","\"actual_product_PID\":"+std::to_string(s.product_pid)+
        ",\"actual_product_birth\":"+std::to_string(owner.birth)+",\"owner_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(owner.app))+
        ",\"uid\":1,\"message_attempt_limit\":1,\"message_sent_to_exact_product_only\":"+(sent?"true":"false")+
        ",\"registration_receipt_wait_limit_ms\":1500,\"exact_current_mark_registration_observed\":"+(registration_bound?"true":"false")+
        ",\"before_GetRect_hresult\":"+std::to_string(static_cast<long>(before_lookup))+",\"after_GetRect_hresult\":"+std::to_string(static_cast<long>(after_lookup))+
        ",\"actual_registration\":"+quote(registration)+",\"actual_post_message_rect\":"+(rectangle?rect({after.left,after.top,after.right,after.bottom}):"null")+
        ",\"existing_icon_reachable_before\":"+(before_lookup==S_OK?"true":"false")+",\"actual_Shell_restart\":false,"+
        std::string("\"NIM_ADD_bool_does_not_alone_gate_existing_icon\":true,\"recreation_PASS_inferred\":false,\"menu_input_sent\":false"));
    if(!sent||!registration_bound||!rectangle||!tray_guest_owner(s,owner)){
        s.log.emit("tray_readiness_result","\"result\":\"BLOCKED\",\"reason\":\"exact_current_product_UID1_not_ready_after_single_simulation\",\"menu_input_sent\":false");
        return false;
    }
    const auto menu_mark=s.log.mark();owned_tray_entry_cases(s,false); // The wrapper already sent its sole TaskbarCreated.
    const auto menu_rows=s.log.since(menu_mark);
    if(menu_rows.find("\"type\":\"tray_entry_result\",\"result\":\"PASS\"")==menu_rows.npos)return false;
    const auto exit_menu=tray_guest_open(s,owner);if(!exit_menu)throw std::runtime_error("tray_exit_icon_not_fresh_exactly_reachable");
    const auto stop_mark=read_product(s).size();tray_guest_choose(s,owner,*exit_menu,505,L"退出");
    const bool stopped=WaitForSingleObject(s.process,5000)==WAIT_OBJECT_0;DWORD code=STILL_ACTIVE;
    const bool code_read=stopped&&GetExitCodeProcess(s.process,&code);const auto shutdown=record(s,stop_mark,"shutdown");
    RECT gone_rect{};const HRESULT icon_after_exit=Shell_NotifyIconGetRect(&id,&gone_rect);GUITHREADINFO source{sizeof(source)};
    const bool native_idle=GetGUIThreadInfo(s.owner,&source)&&!source.hwndCapture&&!source.hwndMoveSize&&!source.hwndMenuOwner;
    s.log.check("tray_actual_Exit_menu_and_resources",stopped&&code_read&&code==0&&receipt_identity(shutdown,"shutdown")&&
        shutdown.find("\"own_hooks_stopped\":true")!=shutdown.npos&&shutdown.find("\"own_overlay_destroyed\":true")!=shutdown.npos&&
        shutdown.find("\"native_worker_exited\":true")!=shutdown.npos&&shutdown.find("\"issued_native_completion_unknown\":false")!=shutdown.npos&&
        shutdown.find("\"visual_recovery_stopped\":true")!=shutdown.npos&&shutdown.find("\"visual_restore_confirmed\":true")!=shutdown.npos&&
        input_resource_stop_observed(shutdown)&&native_idle&&!FindWindowW(L"PaneBind.V1.Application",L"PaneBind")&&icon_after_exit!=S_OK,
        "\"actual_product_exit_code\":"+(code_read?std::to_string(code):"null")+",\"actual_shutdown\":"+quote(shutdown)+
        ",\"GetRect_after_actual_product_Exit_hresult\":"+std::to_string(static_cast<long>(icon_after_exit))+
        ",\"product_icon_deleted_by_driver\":false,\"guest_destruction_used\":false");
    CloseHandle(s.process);s.process=nullptr;
    const HWND h=s.frames[0].hwnd;foreground(s,h);RECT client{};POINT point{};
    if(!own(s,h)||!GetClientRect(h,&client))throw std::runtime_error("tray_stop_client_exact_owned_rect_required");
    point={(client.left+client.right)/2,(client.top+client.bottom)/2};
    if(!ClientToScreen(h,&point))throw std::runtime_error("tray_stop_client_screen_point_unknown");cursor(s,point);DWORD_PTR hit{};
    if(!own(s,h)||GetForegroundWindow()!=h||WindowFromPoint(point)!=h||
        !SendMessageTimeoutW(h,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&hit)||hit!=HTCLIENT)
        throw std::runtime_error("tray_stop_client_exact_actual_hit_changed");
    const auto downs=s.frames[0].client_down[0].load(),ups=s.frames[0].client_up[0].load();
    button(s,true);const bool down=wait([&]{return s.frames[0].client_down[0].load()==downs+1;},1000);
    button(s,false);const bool up=wait([&]{return s.frames[0].client_up[0].load()==ups+1;},1000);
    s.log.check("tray_actual_post_menu_Exit_client_DOWN_UP",down&&up&&own(s,h)&&!s.left,
        "\"actual_client_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
        ",\"DOWN_before\":"+std::to_string(downs)+",\"DOWN_after\":"+std::to_string(s.frames[0].client_down[0].load())+
        ",\"UP_before\":"+std::to_string(ups)+",\"UP_after\":"+std::to_string(s.frames[0].client_up[0].load())+
        ",\"SendInput_return_not_used_as_delivery\":true,\"activation_is_test_setup\":true");
    s.log.emit("tray_readiness_result","\"result\":\"PASS\",\"same_Medium_probe_actual_ADD_rect_DELETE\":true,\"exact_product_actual_menu_and_Exit\":true,"+
        std::string("\"actual_Shell_restart\":\"NOT_RUN\",\"full_desktop_or_whole_version_PASS\":false,\"human_uat\":\"NOT_RUN\""));
    return true;
}
