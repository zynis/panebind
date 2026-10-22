#pragma once

// TEST ONLY, inside the guarded fixture's unnamed namespace. The original
// environment case has no product/input; the separate exact UI case is bounded
// and never relaxes the product's Default-desktop input eligibility.
// The caller retains its original 195-access desktop and exact child identity.
struct MediumDesktopActivationInput {
    bool opened{},name_known{},io_known{},receiving{},closed{};
    DWORD open_error{},name_error{},io_error{},close_error{};std::wstring name;
    static MediumDesktopActivationInput read(){
        MediumDesktopActivationInput out;SetLastError(ERROR_SUCCESS);
        const HDESK input=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
        out.opened=input!=nullptr;out.open_error=input?0:GetLastError();if(!input)return out;
        std::array<wchar_t,128> value{};DWORD bytes{};BOOL active{};
        SetLastError(ERROR_SUCCESS);out.name_known=GetUserObjectInformationW(input,UOI_NAME,value.data(),sizeof(value),&bytes)!=FALSE;
        out.name_error=out.name_known?0:GetLastError();if(out.name_known)out.name=value.data();
        SetLastError(ERROR_SUCCESS);out.io_known=GetUserObjectInformationW(input,UOI_IO,&active,sizeof(active),&bytes)!=FALSE;
        out.io_error=out.io_known?0:GetLastError();out.receiving=out.io_known&&active!=FALSE;
        SetLastError(ERROR_SUCCESS);out.closed=CloseDesktop(input)!=FALSE;out.close_error=out.closed?0:GetLastError();return out;
    }
    bool exact(const std::wstring& expected)const{return opened&&name_known&&io_known&&receiving&&closed&&equal(name,expected);}
    std::string json()const{return "{\"opened\":"+std::string(opened?"true":"false")+",\"open_error\":"+std::to_string(open_error)+
        ",\"name_known\":"+(name_known?"true":"false")+",\"name\":"+quote(utf8(name))+",\"name_error\":"+std::to_string(name_error)+
        ",\"UOI_IO_known\":"+(io_known?"true":"false")+",\"UOI_IO\":"+(io_known?(receiving?"true":"false"):"null")+
        ",\"UOI_IO_error\":"+std::to_string(io_error)+",\"handle_closed\":"+(closed?"true":"false")+",\"close_error\":"+std::to_string(close_error)+"}";}
};
struct MediumDesktopActivationObservation {
    std::vector<std::pair<std::string,std::string>> records;
    bool attempted{},entry_completed{},entry_return{},active_verified{},post_activation_ready{};
    bool tray_api_attempted{},tray_api_ready{},tray_probe_cleanup_known{true};
    bool ui_requested{},ui_started{},ui_completed{},ui_cleanup_known{true},ui_product_closed{};
    bool default_return_verified{},return_owner_exited{},opened_handles_closed{},cleanup_known{},deadline_or_completion_unknown{};
    std::string reason{"not_started"},ui_reason{"not_requested"};
    void emit(std::string type,std::string fields){records.emplace_back(std::move(type),std::move(fields));}
};
std::wstring medium_desktop_activation_name(HANDLE object){
    std::array<wchar_t,128> name{};DWORD bytes{};
    return object&&GetUserObjectInformationW(object,UOI_NAME,name.data(),sizeof(name),&bytes)?std::wstring(name.data()):std::wstring{};
}
struct MediumDesktopActivationReturn {
    HDESK target{},original{};HANDLE armed{},return_request{},entry_done{};
    std::wstring target_name;ULONGLONG began{};
    DWORD deadline_ms{5000},outer_ms{10000};
    std::shared_ptr<MediumDesktopUiInputLease> ui_input;
    std::atomic<bool> deadline{},entry_wait_unknown{},return_verified{};
    std::atomic<unsigned> entry_gate{},return_attempt{};
    // Only this owner writes these rows; caller reads after real thread exit.
    std::vector<std::pair<std::string,std::string>> owner_records;
    ~MediumDesktopActivationReturn(){
        if(target)CloseDesktop(target);if(original)CloseDesktop(original);
        if(armed)CloseHandle(armed);if(return_request)CloseHandle(return_request);if(entry_done)CloseHandle(entry_done);
    }
};
void medium_desktop_activation_return_once(const std::shared_ptr<MediumDesktopActivationReturn>& state,
    std::vector<std::pair<std::string,std::string>>& records,const char* actor){
    // Retire new UI sends before changing input desktops. A paired SendInput
    // already issued is not cancellable; wait once for its actual completion.
    if(state->ui_input){
        const bool quiescent=state->ui_input->retire_and_wait(1000);
        records.emplace_back("medium_desktop_UI_input_retired","\"actor\":"+quote(actor)+
            ",\"retired\":true,\"in_flight_completed\":"+(quiescent?"true":"false")+
            ",\"wait_limit_ms\":1000,\"issued_input_cancelled\":false");
        if(!quiescent){state->entry_wait_unknown.store(true);return;}
    }
    const auto before=MediumDesktopActivationInput::read();const auto begin=GetTickCount64();
    // Never displace an unknown/secure/third desktop. Already Default is an
    // observation, not an invented SwitchDesktop success.
    if(before.exact(L"Default")){
        state->return_verified.store(true);records.emplace_back("medium_desktop_return_observation",
            "\"actor\":"+quote(actor)+",\"before\":"+before.json()+",\"SwitchDesktop_attempted\":false,\"already_Default\":true");return;
    }
    if(!before.exact(state->target_name)){
        records.emplace_back("medium_desktop_return_observation","\"actor\":"+quote(actor)+",\"before\":"+before.json()+
            ",\"SwitchDesktop_attempted\":false,\"reason\":\"input_not_exact_own_Run_no_desktop_displaced\"");return;
    }
    unsigned unused=0;if(!state->return_attempt.compare_exchange_strong(unused,1)){
        records.emplace_back("medium_desktop_return_observation","\"actor\":"+quote(actor)+",\"before\":"+before.json()+
            ",\"SwitchDesktop_attempted\":false,\"reason\":\"one_actual_return_attempt_already_claimed_no_retry\"");return;
    }
    SetLastError(ERROR_SUCCESS);const BOOL returned=SwitchDesktop(state->original);const DWORD raw_error=GetLastError();
    const auto ended=GetTickCount64();const auto after=MediumDesktopActivationInput::read();
    state->return_verified.store(returned!=FALSE&&after.exact(L"Default"));state->return_attempt.store(2);
    records.emplace_back("medium_desktop_return_actual","\"actor\":"+quote(actor)+",\"before\":"+before.json()+
        ",\"SwitchDesktop_attempted\":true,\"native_return\":"+(returned?"true":"false")+",\"raw_last_error\":"+
        std::to_string(raw_error)+",\"last_error_not_general_root_cause\":true,\"before_tick\":"+std::to_string(begin)+
        ",\"after_tick\":"+std::to_string(ended)+",\"after\":"+after.json()+",\"native_call_cancelled\":false");
}
DWORD WINAPI medium_desktop_activation_return_owner(void* parameter){
    std::unique_ptr<std::shared_ptr<MediumDesktopActivationReturn>> holder(
        static_cast<std::shared_ptr<MediumDesktopActivationReturn>*>(parameter));const auto state=*holder;
    try{
        const bool armed=SetEvent(state->armed)!=FALSE;
        state->owner_records.emplace_back("medium_desktop_return_owner_armed","\"SetEvent_return\":"+
            std::string(armed?"true":"false")+",\"owner_TID\":"+std::to_string(GetCurrentThreadId())+
            ",\"one_shot_deadline_ms\":"+std::to_string(state->deadline_ms)+
            ",\"native_completion_outer_budget_ms\":"+std::to_string(state->outer_ms)+",\"polling\":false");
        const auto now=GetTickCount64();const DWORD budget=now-state->began<state->deadline_ms?static_cast<DWORD>(state->deadline_ms-(now-state->began)):0;
        const DWORD requested=WaitForSingleObject(state->return_request,budget);
        if(requested!=WAIT_OBJECT_0){state->deadline.store(true);unsigned pending=0;state->entry_gate.compare_exchange_strong(pending,2);}
        state->owner_records.emplace_back("medium_desktop_return_owner_wake","\"wait_result\":"+std::to_string(requested)+
            ",\"explicit_request\":"+(requested==WAIT_OBJECT_0?"true":"false")+",\"one_shot_deadline_reached\":"+
            (requested==WAIT_TIMEOUT?"true":"false")+",\"native_entry_completion_not_inferred\":true");
        // An in-flight entry can switch after an early Default observation.
        // Wait once for real completion, within the total ten-second budget.
        const auto before_done=GetTickCount64();const DWORD left=before_done-state->began<state->outer_ms?
            static_cast<DWORD>(state->outer_ms-(before_done-state->began)):0;
        const DWORD completed=WaitForSingleObject(state->entry_done,left);
        if(completed!=WAIT_OBJECT_0){
            state->entry_wait_unknown.store(true);state->owner_records.emplace_back("medium_desktop_return_completion_unknown",
                "\"entry_done_wait_result\":"+std::to_string(completed)+",\"resources_stopped\":false,\"native_call_cancelled\":false,"
                "\"late_entry_retains_shared_context\":true,\"additional_wait_or_retry\":false");return 78;
        }
        medium_desktop_activation_return_once(state,state->owner_records,"independent_return_owner");return state->return_verified.load()?0:78;
    }catch(...){
        state->entry_wait_unknown.store(true);state->deadline.store(true);
        unsigned pending=0;state->entry_gate.compare_exchange_strong(pending,2);
        return 78;
    }
}
// Reuse the same exact private-class icon resource as TrayReadinessOnly. This
// is a native notification-area precondition, not product/menu/input evidence.
// The independent return owner is already armed and is never waiting for us.
void medium_desktop_active_tray_probe(MediumDesktopActivationObservation& out,
    const std::shared_ptr<MediumDesktopActivationReturn>& state,const std::function<bool()>& still_exact){
    out.tray_probe_cleanup_known=false; // Never infer cleanup if observation itself exits unexpectedly.
    TrayReadinessProbe probe;probe.instance=GetModuleHandleW(nullptr);probe.tid=GetCurrentThreadId();
    probe.name=L"PaneBind.V1.ActiveMediumTray."+state->target_name;
    const auto began=GetTickCount64();const auto foreground_before=GetForegroundWindow();
    DWORD register_error{},create_error{},userdata_error{};bool class_attempted{},window_attempted{},userdata_bound{},not_activated{};
    std::string reason="not_started";std::string input_before="null",input_after="null";
    const auto exact_active=[&]{
        if(state->deadline.load()||state->entry_wait_unknown.load()||state->return_attempt.load()!=0||
           GetTickCount64()-state->began>=state->deadline_ms||!medium(GetCurrentProcess())||
           !equal(medium_desktop_activation_name(GetThreadDesktop(GetCurrentThreadId())),state->target_name))return false;
        const auto input=MediumDesktopActivationInput::read();
        return input.exact(state->target_name)&&still_exact();
    };
    try{
        const auto input=MediumDesktopActivationInput::read();input_before=input.json();
        if(!input.exact(state->target_name)||!exact_active())throw std::runtime_error("actual_exact_active_Run_or_remaining_deadline_not_confirmed");
        WNDCLASSW wc{};wc.hInstance=probe.instance;wc.lpszClassName=probe.name.c_str();wc.lpfnWndProc=DefWindowProcW;
        class_attempted=true;SetLastError(ERROR_SUCCESS);probe.atom=RegisterClassW(&wc);register_error=probe.atom?0:GetLastError();
        if(!probe.atom)throw std::runtime_error("fresh_private_class_not_created_no_existing_class_reused");
        window_attempted=true;SetLastError(ERROR_SUCCESS);
        probe.window=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,probe.name.c_str(),L"PaneBind guest active-desktop tray probe",
            WS_POPUP,0,0,1,1,nullptr,nullptr,probe.instance,nullptr);
        create_error=probe.window?0:GetLastError();if(!probe.window)throw std::runtime_error("private_probe_window_not_created");
        SetLastError(ERROR_SUCCESS);const auto previous=SetWindowLongPtrW(probe.window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(&probe));
        userdata_error=GetLastError();userdata_bound=(previous||!userdata_error)&&probe.owns();
        if(!userdata_bound)throw std::runtime_error("exact_probe_window_userdata_identity_not_bound");
        NOTIFYICONDATAW data{};data.cbSize=sizeof(data);data.hWnd=probe.window;data.uID=TrayReadinessProbe::uid;
        data.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;data.uCallbackMessage=WM_APP+0x73;data.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
        wcscpy_s(data.szTip,L"PaneBind V1 active Medium guest tray probe");
        if(!data.hIcon)throw std::runtime_error("shared_stock_icon_not_obtained");
        if(!probe.owns()||!exact_active())throw std::runtime_error("exact_active_Run_expired_before_ADD");
        out.tray_api_attempted=true;probe.add_attempted=true;const auto add_begin=GetTickCount64();
        SetLastError(ERROR_SUCCESS);probe.added=Shell_NotifyIconW(NIM_ADD,&data);probe.add_error=GetLastError();
        out.emit("medium_desktop_tray_actual_ADD","\"owner_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(probe.window))+
            ",\"owner_PID\":"+std::to_string(GetCurrentProcessId())+",\"owner_TID\":"+std::to_string(probe.tid)+
            ",\"uid\":"+std::to_string(TrayReadinessProbe::uid)+",\"native_return\":"+(probe.added?"true":"false")+
            ",\"immediate_GetLastError_raw\":"+std::to_string(probe.add_error)+",\"begin_tick\":"+std::to_string(add_begin)+
            ",\"end_tick\":"+std::to_string(GetTickCount64())+",\"raw_error_not_root_cause\":true");
        if(!probe.added)throw std::runtime_error("actual_owned_NIM_ADD_returned_false");
        if(!probe.owns()||!exact_active())throw std::runtime_error("exact_active_Run_expired_before_SETVERSION");
        data.uVersion=NOTIFYICON_VERSION_4;probe.version_attempted=true;const auto version_begin=GetTickCount64();
        SetLastError(ERROR_SUCCESS);probe.versioned=Shell_NotifyIconW(NIM_SETVERSION,&data);probe.version_error=GetLastError();
        out.emit("medium_desktop_tray_actual_SETVERSION","\"requested_version\":4,\"native_return\":"+
            std::string(probe.versioned?"true":"false")+",\"immediate_GetLastError_raw\":"+std::to_string(probe.version_error)+
            ",\"begin_tick\":"+std::to_string(version_begin)+",\"end_tick\":"+std::to_string(GetTickCount64())+",\"raw_error_not_root_cause\":true");
        if(!probe.versioned)throw std::runtime_error("actual_owned_NIM_SETVERSION_returned_false");
        if(!probe.owns()||!exact_active())throw std::runtime_error("exact_active_Run_expired_before_GetRect");
        NOTIFYICONIDENTIFIER id{sizeof(id)};id.hWnd=probe.window;id.uID=TrayReadinessProbe::uid;
        const auto lookup_begin=GetTickCount64();probe.lookup=Shell_NotifyIconGetRect(&id,&probe.area);
        const bool rectangle=probe.lookup==S_OK&&probe.area.left<probe.area.right&&probe.area.top<probe.area.bottom;
        not_activated=probe.owns()&&!IsWindowVisible(probe.window)&&GetForegroundWindow()!=probe.window;
        const auto after=MediumDesktopActivationInput::read();input_after=after.json();
        const bool active_after=after.exact(state->target_name)&&exact_active();
        out.emit("medium_desktop_tray_actual_GetRect","\"HRESULT\":"+std::to_string(static_cast<long>(probe.lookup))+
            ",\"actual_rect\":"+(rectangle?rect({probe.area.left,probe.area.top,probe.area.right,probe.area.bottom}):"null")+
            ",\"begin_tick\":"+std::to_string(lookup_begin)+",\"end_tick\":"+std::to_string(GetTickCount64())+
            ",\"probe_hidden_and_not_foreground\":"+(not_activated?"true":"false")+",\"actual_active_Run_after\":"+(active_after?"true":"false"));
        out.tray_api_ready=rectangle&&not_activated&&active_after;
        reason=out.tray_api_ready?"own_Medium_icon_actual_ADD_version4_rect_observed_only":"actual_icon_geometry_activation_or_active_Run_not_confirmed";
    }catch(const std::exception& error){reason=error.what();}
    const HWND actual_window=probe.window;const auto cleanup_begin=GetTickCount64();probe.cleanup();
    const bool window_gone=!actual_window||!IsWindow(actual_window);
    out.tray_probe_cleanup_known=(!window_attempted||!actual_window||(probe.destroy_attempted&&probe.destroyed&&window_gone))&&
        (!probe.atom||(probe.unregister_attempted&&probe.unregistered))&&
        (!probe.added||(probe.delete_attempted&&probe.deleted&&probe.lookup_after_delete!=S_OK));
    out.emit("medium_desktop_tray_actual_cleanup","\"NIM_DELETE_attempted\":"+std::string(probe.delete_attempted?"true":"false")+
        ",\"NIM_DELETE_native_return\":"+(probe.delete_attempted?(probe.deleted?"true":"false"):"null")+
        ",\"NIM_DELETE_immediate_GetLastError_raw\":"+(probe.delete_attempted?std::to_string(probe.delete_error):"null")+
        ",\"GetRect_after_DELETE_HRESULT\":"+(probe.delete_attempted?std::to_string(static_cast<long>(probe.lookup_after_delete)):"null")+
        ",\"DestroyWindow_attempted\":"+(probe.destroy_attempted?"true":"false")+",\"DestroyWindow_native_return\":"+(probe.destroy_attempted?(probe.destroyed?"true":"false"):"null")+
        ",\"DestroyWindow_error\":"+(probe.destroy_attempted?std::to_string(probe.destroy_error):"null")+",\"actual_window_gone\":"+(window_gone?"true":"false")+
        ",\"UnregisterClass_attempted\":"+(probe.unregister_attempted?"true":"false")+",\"UnregisterClass_native_return\":"+(probe.unregister_attempted?(probe.unregistered?"true":"false"):"null")+
        ",\"UnregisterClass_error\":"+(probe.unregister_attempted?std::to_string(probe.unregister_error):"null")+
        ",\"begin_tick\":"+std::to_string(cleanup_begin)+",\"end_tick\":"+std::to_string(GetTickCount64())+
        ",\"cleanup_known\":"+(out.tray_probe_cleanup_known?"true":"false")+",\"only_exact_own_HWND_uid_class_touched\":true,\"stock_icon_destroyed\":false");
    out.emit("medium_desktop_tray_API_summary","\"result\":"+quote(out.tray_api_ready&&out.tray_probe_cleanup_known?"OWN_API_PRECONDITION_ONLY":"BLOCKED_OR_UNKNOWN")+
        ",\"reason\":"+quote(reason)+",\"Run_desktop\":"+quote(utf8(state->target_name))+
        ",\"class_registration_attempted\":"+(class_attempted?"true":"false")+",\"class_registration_error\":"+std::to_string(register_error)+
        ",\"window_creation_attempted\":"+(window_attempted?"true":"false")+",\"window_creation_error\":"+std::to_string(create_error)+
        ",\"userdata_identity_bound\":"+(userdata_bound?"true":"false")+",\"userdata_raw_error\":"+std::to_string(userdata_error)+
        ",\"foreground_before\":"+std::to_string(reinterpret_cast<std::uintptr_t>(foreground_before))+
        ",\"foreground_after\":"+std::to_string(reinterpret_cast<std::uintptr_t>(GetForegroundWindow()))+
        ",\"input_before\":"+input_before+",\"input_after\":"+input_after+",\"begin_tick\":"+std::to_string(began)+
        ",\"end_tick\":"+std::to_string(GetTickCount64())+",\"independent_return_deadline_ms\":"+std::to_string(state->deadline_ms)+",\"callback_did_not_gate_independent_return\":true,"
        "\"synthetic_input_sent\":false,\"product_started\":false,\"menu_interaction\":\"NOT_RUN\",\"B27_PASS_inferred\":false");
}
MediumDesktopActivationObservation medium_desktop_activate_and_return(HDESK created,const std::wstring& exact_run_name,
    const std::function<bool()>& still_exact,const MediumDesktopUiRequest* ui_request=nullptr){
    MediumDesktopActivationObservation out;const std::wstring prefix=L"PaneBindV1_";
    const auto run=exact_run_name.starts_with(prefix)?exact_run_name.substr(prefix.size()):std::wstring{};
    std::array<wchar_t,128> user{};DWORD size=static_cast<DWORD>(user.size()),session{};
    const bool guarded=run.size()==32&&std::all_of(run.begin(),run.end(),[](wchar_t c){return(c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');})&&
        created&&equal(medium_desktop_activation_name(created),exact_run_name)&&
        equal(medium_desktop_activation_name(GetThreadDesktop(GetCurrentThreadId())),exact_run_name)&&
        equal(medium_desktop_activation_name(GetProcessWindowStation()),L"WinSta0")&&GetUserNameW(user.data(),&size)&&
        equal(user.data(),L"WDAGUtilityAccount")&&medium(GetCurrentProcess())&&
        ProcessIdToSessionId(GetCurrentProcessId(),&session)&&session&&
        marker(fs::path(L"C:\\PaneBindV1\\Input\\panebind-v1-input.marker"),run)&&
        marker(fs::path(L"C:\\PaneBindV1\\Output\\panebind-v1-output.marker"),run);
    if(!guarded||!still_exact){out.reason="exact_original_guest_Run_desktop_or_readiness_callback_missing";return out;}
    const auto original_input=MediumDesktopActivationInput::read();
    if(!original_input.exact(L"Default")){out.reason="actual_original_Default_input_not_verified";
        out.emit("medium_desktop_activation_preflight","\"input\":"+original_input.json()+",\"SwitchDesktop_attempted\":false");return out;}
    const auto state=std::make_shared<MediumDesktopActivationReturn>();state->target_name=exact_run_name;
    out.ui_requested=ui_request!=nullptr;
    if(ui_request){state->deadline_ms=30000;state->outer_ms=35000;
        state->ui_input=std::make_shared<MediumDesktopUiInputLease>();}
    constexpr ACCESS_MASK access=DESKTOP_READOBJECTS|DESKTOP_SWITCHDESKTOP;
    SetLastError(ERROR_SUCCESS);state->target=OpenDesktopW(exact_run_name.c_str(),0,FALSE,access);
    const DWORD target_error=state->target?0:GetLastError();
    SetLastError(ERROR_SUCCESS);state->original=OpenDesktopW(L"Default",0,FALSE,access);
    const DWORD original_error=state->original?0:GetLastError();
    const auto target_name=medium_desktop_activation_name(state->target),default_name=medium_desktop_activation_name(state->original);
    out.emit("medium_desktop_activation_handles","\"requested_access\":257,\"original_created_access_unchanged\":195,"
        "\"target_open_return\":"+std::string(state->target?"true":"false")+",\"target_error\":"+std::to_string(target_error)+
        ",\"actual_target_name\":"+quote(utf8(target_name))+",\"Default_open_return\":"+(state->original?"true":"false")+
        ",\"Default_error\":"+std::to_string(original_error)+",\"actual_Default_name\":"+quote(utf8(default_name))+
        ",\"DACL_label_token_or_account_changed\":false");
    if(!state->target||!state->original||!equal(target_name,exact_run_name)||!equal(default_name,L"Default")){
        out.reason="legal_switch_handles_not_obtained_no_access_control_changes";return out;}
    state->armed=CreateEventW(nullptr,TRUE,FALSE,nullptr);state->return_request=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    state->entry_done=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!state->armed||!state->return_request||!state->entry_done){out.reason="independent_return_events_not_created";return out;}
    auto* argument=new std::shared_ptr<MediumDesktopActivationReturn>(state);state->began=GetTickCount64();
    SetLastError(ERROR_SUCCESS);Handle owner(CreateThread(nullptr,0,medium_desktop_activation_return_owner,argument,0,nullptr));
    const DWORD create_error=owner.value?0:GetLastError();if(!owner.value){delete argument;out.reason="independent_return_owner_not_created";
        out.emit("medium_desktop_activation_refused","\"CreateThread_return\":false,\"win32_error\":"+std::to_string(create_error));return out;}
    const DWORD armed=WaitForSingleObject(state->armed,1000);bool ready=false;
    try{
        if(armed==WAIT_OBJECT_0)ready=still_exact();
        const auto input_before=MediumDesktopActivationInput::read();
        const DWORD owner_before_entry=WaitForSingleObject(owner.value,0);
        const bool independent_owner_live=owner_before_entry==WAIT_TIMEOUT&&!state->entry_wait_unknown.load();
        const bool within_deadline=GetTickCount64()-state->began<state->deadline_ms&&!state->deadline.load();
        out.emit("medium_desktop_activation_readiness","\"return_owner_armed_wait_result\":"+std::to_string(armed)+
            ",\"actual_caller_Shell_Tray_readiness\":"+(ready?"true":"false")+",\"input_before\":"+input_before.json()+
            ",\"return_owner_before_entry_wait_result\":"+std::to_string(owner_before_entry)+
            ",\"independent_return_owner_live\":"+(independent_owner_live?"true":"false")+
            ",\"before_deadline\":"+(within_deadline?"true":"false")+",\"readiness_is_not_product_authority\":true");
        unsigned pending=0;
        if(armed==WAIT_OBJECT_0&&ready&&independent_owner_live&&input_before.exact(L"Default")&&within_deadline&&
           state->entry_gate.compare_exchange_strong(pending,1)){
            out.attempted=true;const auto begin=GetTickCount64();SetLastError(ERROR_SUCCESS);
            const BOOL result=SwitchDesktop(state->target);const DWORD raw_error=GetLastError();const auto ended=GetTickCount64();
            out.entry_completed=true;out.entry_return=result!=FALSE;SetEvent(state->entry_done);
            const auto actual=MediumDesktopActivationInput::read();out.active_verified=out.entry_return&&actual.exact(exact_run_name);
            if(out.active_verified)out.post_activation_ready=still_exact();
            out.emit("medium_desktop_activation_actual","\"SwitchDesktop_attempted\":true,\"native_return\":"+
                std::string(result?"true":"false")+",\"raw_last_error\":"+std::to_string(raw_error)+
                ",\"last_error_not_general_root_cause\":true,\"before_tick\":"+std::to_string(begin)+",\"after_tick\":"+
                std::to_string(ended)+",\"actual_input\":"+actual.json()+",\"post_activation_exact_Shell_Tray_ready\":"+
                (out.post_activation_ready?"true":"false")+",\"product_started\":false,\"synthetic_input_sent\":false");
            if(out.active_verified&&out.post_activation_ready)medium_desktop_active_tray_probe(out,state,still_exact);
            if(ui_request&&out.tray_api_ready&&out.tray_probe_cleanup_known&&!state->deadline.load()){
                out.ui_cleanup_known=false;
                const auto elapsed=GetTickCount64()-state->began;
                const DWORD remaining=elapsed<state->deadline_ms?static_cast<DWORD>(state->deadline_ms-elapsed):0;
                const auto ui=medium_desktop_ui_case(run,created,ui_request->product,ui_request->source,
                    ui_request->sha256,std::min<DWORD>(20000,remaining),still_exact,state->ui_input);
                out.ui_started=ui.product_started;out.ui_completed=ui.completed;
                out.ui_cleanup_known=ui.cleanup_known;out.ui_product_closed=ui.product_closed;
                out.ui_reason=ui.reason;
                out.records.insert(out.records.end(),ui.records.begin(),ui.records.end());
            }else if(ui_request)out.ui_reason="actual_owned_tray_API_or_remaining_active_deadline_not_ready_no_UI_started";
        }else out.reason="actual_readiness_Default_input_or_armed_deadline_rejected";
    }catch(...){out.reason="readiness_or_actual_observation_exception";}
    // Signaled only after the real entry API returned (or was never issued).
    SetEvent(state->entry_done);SetEvent(state->return_request);
    bool late_return_checked=false;
    if(state->entry_wait_unknown.load()){
        late_return_checked=true;medium_desktop_activation_return_once(state,out.records,"late_entry_completion_no_native_retry");
    }
    const DWORD exited=WaitForSingleObject(owner.value,2000);out.return_owner_exited=exited==WAIT_OBJECT_0;
    if(out.return_owner_exited&&state->entry_wait_unknown.load()&&!late_return_checked)
        medium_desktop_activation_return_once(state,out.records,"completed_owner_unknown_no_native_retry");
    out.deadline_or_completion_unknown=state->deadline.load()||state->entry_wait_unknown.load()||!out.return_owner_exited;
    if(out.return_owner_exited)out.records.insert(out.records.end(),state->owner_records.begin(),state->owner_records.end());
    out.default_return_verified=state->return_verified.load();
    const bool owner_handle_closed=CloseHandle(owner.value)!=FALSE;owner.value=nullptr;
    if(out.return_owner_exited){
        const bool target_closed=CloseDesktop(state->target)!=FALSE;state->target=nullptr;
        const bool original_closed=CloseDesktop(state->original)!=FALSE;state->original=nullptr;
        const bool armed_closed=CloseHandle(state->armed)!=FALSE;state->armed=nullptr;
        const bool requested_closed=CloseHandle(state->return_request)!=FALSE;state->return_request=nullptr;
        const bool done_closed=CloseHandle(state->entry_done)!=FALSE;state->entry_done=nullptr;
        const bool ui_event_attempted=state->ui_input&&state->ui_input->input_done;
        const bool ui_event_closed=!ui_event_attempted||CloseHandle(state->ui_input->input_done)!=FALSE;
        if(ui_event_attempted&&ui_event_closed)state->ui_input->input_done=nullptr;
        out.opened_handles_closed=target_closed&&original_closed&&armed_closed&&requested_closed&&done_closed&&owner_handle_closed&&ui_event_closed;
        out.emit("medium_desktop_activation_local_handles","\"target_CloseDesktop_return\":"+std::string(target_closed?"true":"false")+
            ",\"Default_CloseDesktop_return\":"+(original_closed?"true":"false")+",\"armed_CloseHandle_return\":"+(armed_closed?"true":"false")+
            ",\"request_CloseHandle_return\":"+(requested_closed?"true":"false")+",\"entry_done_CloseHandle_return\":"+(done_closed?"true":"false")+
            ",\"owner_CloseHandle_return\":"+(owner_handle_closed?"true":"false")+
            ",\"UI_input_event_CloseHandle_attempted\":"+(ui_event_attempted?"true":"false")+
            ",\"UI_input_event_CloseHandle_return\":"+(ui_event_attempted?(ui_event_closed?"true":"false"):"null")+
            ",\"original_created_handle_closed_by_helper\":false");
    }
    out.cleanup_known=out.opened_handles_closed&&out.default_return_verified&&out.return_owner_exited&&!out.deadline_or_completion_unknown&&out.tray_probe_cleanup_known&&out.ui_cleanup_known;
    const bool complete=out.attempted&&out.entry_completed&&out.entry_return&&out.active_verified&&out.post_activation_ready&&
        out.default_return_verified&&out.return_owner_exited&&out.cleanup_known&&!out.deadline_or_completion_unknown;
    if(complete)out.reason=ui_request?(out.ui_completed?"actual_ordinary_UI_subchain_and_independent_Default_return_observed":out.ui_reason):
        "actual_own_desktop_activation_and_independent_Default_return_observed_only";
    else if(out.reason=="not_started")out.reason="activation_or_Default_return_unconfirmed_no_followup_environment_interaction";
    out.emit("medium_desktop_activation_summary","\"result\":"+quote(complete?"ACTIVATED_AND_RETURNED_ONLY":"UNKNOWN_OR_BLOCKED")+
        ",\"reason\":"+quote(out.reason)+",\"actual_switch_return\":"+(out.entry_return?"true":"false")+
        ",\"actual_active_readback\":"+(out.active_verified?"true":"false")+",\"actual_Default_restored\":"+(out.default_return_verified?"true":"false")+
        ",\"return_owner_actual_exit_wait_result\":"+std::to_string(exited)+",\"opened_handles_closed\":"+(out.opened_handles_closed?"true":"false")+
        ",\"cleanup_known\":"+(out.cleanup_known?"true":"false")+
        ",\"own_tray_API_attempted\":"+(out.tray_api_attempted?"true":"false")+",\"own_tray_API_ready\":"+(out.tray_api_ready?"true":"false")+
        ",\"own_tray_probe_cleanup_known\":"+(out.tray_probe_cleanup_known?"true":"false")+
        ",\"deadline_or_completion_unknown\":"+(out.deadline_or_completion_unknown?"true":"false")+
        ",\"UI_requested\":"+(out.ui_requested?"true":"false")+",\"UI_product_started\":"+(out.ui_started?"true":"false")+
        ",\"UI_completed\":"+(out.ui_completed?"true":"false")+",\"UI_product_closed\":"+(out.ui_product_closed?"true":"false")+
        ",\"UI_reason\":"+quote(out.ui_reason)+
        ",\"product_enhancement_result\":\"NOT_RUN\",\"desktop_object_destroyed\":\"UNKNOWN\","
        "\"guest_disposal_is_not_product_cleanup_PASS\":true");return out;
}
