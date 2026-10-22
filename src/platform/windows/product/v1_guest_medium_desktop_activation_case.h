#pragma once

// TEST ONLY, inside the guarded fixture's unnamed namespace. No product/input.
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
    bool default_return_verified{},return_owner_exited{},opened_handles_closed{},cleanup_known{},deadline_or_completion_unknown{};
    std::string reason{"not_started"};
    void emit(std::string type,std::string fields){records.emplace_back(std::move(type),std::move(fields));}
};
std::wstring medium_desktop_activation_name(HANDLE object){
    std::array<wchar_t,128> name{};DWORD bytes{};
    return object&&GetUserObjectInformationW(object,UOI_NAME,name.data(),sizeof(name),&bytes)?std::wstring(name.data()):std::wstring{};
}
struct MediumDesktopActivationReturn {
    HDESK target{},original{};HANDLE armed{},return_request{},entry_done{};
    std::wstring target_name;ULONGLONG began{};
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
            ",\"one_shot_deadline_ms\":5000,\"native_completion_outer_budget_ms\":10000,\"polling\":false");
        const auto now=GetTickCount64();const DWORD budget=now-state->began<5000?static_cast<DWORD>(5000-(now-state->began)):0;
        const DWORD requested=WaitForSingleObject(state->return_request,budget);
        if(requested!=WAIT_OBJECT_0){state->deadline.store(true);unsigned pending=0;state->entry_gate.compare_exchange_strong(pending,2);}
        state->owner_records.emplace_back("medium_desktop_return_owner_wake","\"wait_result\":"+std::to_string(requested)+
            ",\"explicit_request\":"+(requested==WAIT_OBJECT_0?"true":"false")+",\"one_shot_deadline_reached\":"+
            (requested==WAIT_TIMEOUT?"true":"false")+",\"native_entry_completion_not_inferred\":true");
        // An in-flight entry can switch after an early Default observation.
        // Wait once for real completion, within the total ten-second budget.
        const auto before_done=GetTickCount64();const DWORD left=before_done-state->began<10000?
            static_cast<DWORD>(10000-(before_done-state->began)):0;
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
MediumDesktopActivationObservation medium_desktop_activate_and_return(HDESK created,const std::wstring& exact_run_name,
    const std::function<bool()>& still_exact){
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
        const bool within_deadline=GetTickCount64()-state->began<5000&&!state->deadline.load();
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
        out.opened_handles_closed=target_closed&&original_closed&&armed_closed&&requested_closed&&done_closed&&owner_handle_closed;
        out.emit("medium_desktop_activation_local_handles","\"target_CloseDesktop_return\":"+std::string(target_closed?"true":"false")+
            ",\"Default_CloseDesktop_return\":"+(original_closed?"true":"false")+",\"armed_CloseHandle_return\":"+(armed_closed?"true":"false")+
            ",\"request_CloseHandle_return\":"+(requested_closed?"true":"false")+",\"entry_done_CloseHandle_return\":"+(done_closed?"true":"false")+
            ",\"owner_CloseHandle_return\":"+(owner_handle_closed?"true":"false")+",\"original_created_handle_closed_by_helper\":false");
    }
    out.cleanup_known=out.opened_handles_closed&&out.default_return_verified&&out.return_owner_exited&&!out.deadline_or_completion_unknown;
    const bool complete=out.attempted&&out.entry_completed&&out.entry_return&&out.active_verified&&out.post_activation_ready&&
        out.default_return_verified&&out.return_owner_exited&&out.cleanup_known&&!out.deadline_or_completion_unknown;
    if(complete)out.reason="actual_own_desktop_activation_and_independent_Default_return_observed_only";
    else if(out.reason=="not_started")out.reason="activation_or_Default_return_unconfirmed_no_followup_environment_interaction";
    out.emit("medium_desktop_activation_summary","\"result\":"+quote(complete?"ACTIVATED_AND_RETURNED_ONLY":"UNKNOWN_OR_BLOCKED")+
        ",\"reason\":"+quote(out.reason)+",\"actual_switch_return\":"+(out.entry_return?"true":"false")+
        ",\"actual_active_readback\":"+(out.active_verified?"true":"false")+",\"actual_Default_restored\":"+(out.default_return_verified?"true":"false")+
        ",\"return_owner_actual_exit_wait_result\":"+std::to_string(exited)+",\"opened_handles_closed\":"+(out.opened_handles_closed?"true":"false")+
        ",\"cleanup_known\":"+(out.cleanup_known?"true":"false")+
        ",\"deadline_or_completion_unknown\":"+(out.deadline_or_completion_unknown?"true":"false")+
        ",\"product_result\":\"NOT_RUN\",\"Tray_interaction\":\"NOT_RUN\",\"desktop_object_destroyed\":\"UNKNOWN\","
        "\"guest_disposal_is_not_product_cleanup_PASS\":true");return out;
}
