#pragma once

// TEST-only, exact disposable guest observations. A direct shell receipt is
// not Terminal client input. A document title is not an actual browser click.
void guest_native_shell_control(State& s,const ApplicationTarget& target){
    const auto expected=fs::path(L"C:\\PaneBindV1\\Applications")/s.run/
        (std::wstring(s.configuration.begin(),s.configuration.end())+L"-Terminal");
    if(target.kind!="Terminal"||s.suite!="NativeClientsOnly"||s.left||s.control||
       !medium(GetCurrentProcess())||!desktop()||!plain_directory(target.directory)||
       !product::guest::same_exact_directory(target.directory,expected)||
       !product::guest::same_exact_directory(s.output,L"C:\\PaneBindV1\\Output")||
       !marker(L"C:\\PaneBindV1\\Input\\panebind-v1-input.marker",s.run)||
       !marker(s.output/L"panebind-v1-output.marker",s.run)||
       !native_clients_product_absent(s,"Terminal.direct_shell.before"))
        throw std::runtime_error("native_shell_control_exact_guest_scope_required");
    std::array<wchar_t,MAX_PATH> system{};
    if(!GetSystemDirectoryW(system.data(),static_cast<UINT>(system.size())))
        throw std::runtime_error("native_shell_control_system_directory_unknown");
    const auto image=fs::path(system.data())/L"cmd.exe";
    const auto name=s.run+L"-"+std::wstring(s.configuration.begin(),s.configuration.end())+L"-native-shell-receipt.txt";
    const auto receipt=s.output/name;
    if(fs::exists(receipt))throw std::runtime_error("native_shell_control_new_receipt_required");
    const auto nonce=L"PaneBind-native-shell-"+s.run;
    std::wstring command=L"\""+image.wstring()+L"\" /d /q /c echo "+nonce+L">\""+receipt.wstring()+L"\"";
    STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION info{};
    SetLastError(ERROR_SUCCESS);
    const bool launched=CreateProcessW(image.c_str(),command.data(),nullptr,nullptr,FALSE,
        CREATE_NO_WINDOW,nullptr,target.directory.c_str(),&startup,&info)!=FALSE;
    const DWORD launch_error=launched?ERROR_SUCCESS:GetLastError();
    Handle process(launched?info.hProcess:nullptr),thread(launched?info.hThread:nullptr);
    const auto born=launched?process_birth(process.value):0;
    int rid=-1;bool elevated{};DWORD token_error{},own_session{},child_session{};
    const bool actual_medium=launched&&medium(process.value,&rid,&elevated,&token_error);
    const bool session_known=launched&&ProcessIdToSessionId(GetCurrentProcessId(),&own_session)&&
        ProcessIdToSessionId(info.dwProcessId,&child_session);
    bool sid_known=false,same_sid=false;
    if(launched){
        HANDLE own_raw{},child_raw{};
        const bool own_open=OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&own_raw)!=FALSE;
        const bool child_open=OpenProcessToken(process.value,TOKEN_QUERY,&child_raw)!=FALSE;
        Handle own_token(own_raw),child_token(child_raw);
        alignas(TOKEN_USER) std::array<BYTE,512> own_user{},child_user{};DWORD own_size{},child_size{};
        sid_known=own_open&&child_open&&GetTokenInformation(own_token.value,TokenUser,own_user.data(),
            static_cast<DWORD>(own_user.size()),&own_size)&&GetTokenInformation(child_token.value,TokenUser,
            child_user.data(),static_cast<DWORD>(child_user.size()),&child_size);
        if(sid_known){const auto a=reinterpret_cast<const TOKEN_USER*>(own_user.data())->User.Sid;
            const auto b=reinterpret_cast<const TOKEN_USER*>(child_user.data())->User.Sid;
            sid_known=IsValidSid(a)&&IsValidSid(b);same_sid=sid_known&&EqualSid(a,b);}
    }
    const bool exited=launched&&wait([&]{return WaitForSingleObject(process.value,0)==WAIT_OBJECT_0;},5000);
    DWORD exit{};const bool exit_known=launched&&GetExitCodeProcess(process.value,&exit)!=FALSE;
    std::string bytes=read(receipt,256);
    while(!bytes.empty()&&(bytes.back()=='\r'||bytes.back()=='\n'))bytes.pop_back();
    const bool exact_receipt=bytes==utf8(nonce);
    const bool identity=actual_medium&&sid_known&&same_sid&&session_known&&own_session&&own_session==child_session&&born;
    const bool passed=launched&&identity&&exited&&exit_known&&exit==0&&exact_receipt;
    s.log.emit("native_shell_direct_control","\"application\":\"Terminal\",\"result\":"+quote(passed?"PASS":"FAIL")+
        ",\"actual_executable\":"+quote(utf8(image.wstring()))+",\"actual_working_directory\":"+quote(utf8(target.directory.wstring()))+
        ",\"command\":"+quote(utf8(command))+",\"CreateProcessW_succeeded\":"+(launched?"true":"false")+
        ",\"CreateProcessW_error\":"+std::to_string(launch_error)+",\"pid\":"+std::to_string(launched?info.dwProcessId:0)+
        ",\"birth\":"+std::to_string(born)+",\"integrity_rid\":"+(rid<0?"null":std::to_string(rid))+
        ",\"elevated\":"+(rid<0?"null":elevated?"true":"false")+",\"token_query_error\":"+std::to_string(token_error)+
        ",\"actual_Medium\":"+(actual_medium?"true":"false")+",\"same_SID_known\":"+(sid_known?"true":"false")+
        ",\"same_SID\":"+(sid_known?(same_sid?"true":"false"):"null")+",\"session_query_known\":"+(session_known?"true":"false")+
        ",\"same_interactive_session\":"+(session_known?(own_session&&own_session==child_session?"true":"false"):"null")+
        ",\"process_exit_observed\":"+(exited?"true":"false")+",\"exit_code\":"+(exit_known?std::to_string(exit):"null")+
        ",\"exact_receipt\":"+(exact_receipt?"true":"false")+",\"receipt_file\":"+quote(utf8(name))+
        ",\"receipt_sha256\":"+quote(hash(receipt))+",\"wait_bound_ms\":5000,\"creation_flags\":\"CREATE_NO_WINDOW\""+
        ",\"same_current_token_and_Terminal_working_directory\":true,\"environment_or_token_modified\":false"+
        ",\"console_mode_is_not_pseudoconsole\":true,\"product_started\":false,\"client_input_sent\":false"+
        ",\"Terminal_client_PASS_inferred\":false,\"unique_failure_cause_inferred\":false");
    if(launched&&!exited)throw std::runtime_error("native_shell_control_child_exit_unconfirmed_no_dependent_input");
    if(!native_clients_product_absent(s,"Terminal.direct_shell.after"))
        throw std::runtime_error("native_shell_control_product_absence_lost");
}

bool guest_native_application_ready(State& s,ApplicationTarget& target){
    if((target.kind!="Terminal"&&target.kind!="Edge")||!target.accepted||!target.identity||
       !native_clients_product_absent(s,target.kind+".initialization.before"))
        throw std::runtime_error("native_application_ready_exact_bound_scope_required");
    const HWND root=target.identity->hwnd;std::array<wchar_t,1024> title{};
    const auto exact=[&]{std::optional<product::Snapshot> fresh;return application_fresh(s,target,root,&fresh)&&
        fresh&&product::Catalog::same(*target.identity,*fresh)&&test_root_stamp(s,*target.identity);};
    const bool document_ready=target.kind!="Edge"||wait([&]{return exact()&&
        GetWindowTextW(root,title.data(),static_cast<int>(title.size()))>0&&
        std::wstring(title.data()).find(L"PaneBind blank")!=std::wstring::npos;},5000);
    const bool same=exact();DWORD_PTR response{};bool returned=false;DWORD error{};
    if(same){SetLastError(ERROR_SUCCESS);returned=SendMessageTimeoutW(root,WM_NULL,0,0,
        SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&response)!=FALSE;error=returned?ERROR_SUCCESS:GetLastError();
        GetWindowTextW(root,title.data(),static_cast<int>(title.size()));}
    s.log.emit("native_application_initialization","\"application\":"+quote(target.kind)+
        ",\"same_exact_bound_root\":"+(same?"true":"false")+",\"root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(root))+
        ",\"root_pid\":"+std::to_string(target.identity->pid)+",\"root_tid\":"+std::to_string(target.identity->tid)+
        ",\"root_birth\":"+std::to_string(target.identity->born)+",\"actual_root_title\":"+(same?quote(utf8(title.data())):"null")+
        ",\"offline_document_title_observed\":"+(target.kind=="Edge"?(document_ready?"true":"false"):"null")+
        ",\"offline_document_wait_bound_ms\":"+(target.kind=="Edge"?"5000":"0")+
        ",\"WM_NULL_attempted\":"+(same?"true":"false")+",\"WM_NULL_returned\":"+(same?(returned?"true":"false"):"null")+
        ",\"WM_NULL_error\":"+(same?std::to_string(error):"null")+",\"WM_NULL_timeout_ms\":100"+
        ",\"product_started\":false,\"client_input_sent\":false,\"focus_or_capture_requested\":false"+
        ",\"client_semantic_PASS_inferred\":false,\"unique_failure_cause_inferred\":false");
    // Reads only held, birth-verified launch-tree processes and their new
    // roots. No discovered child is adopted as input or closing authority.
    application_binding_facts(s,target);
    if(!native_clients_product_absent(s,target.kind+".initialization.after"))
        throw std::runtime_error("native_application_ready_product_absence_lost");
    return same&&document_ready;
}
