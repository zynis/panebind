#pragma once

// TEST ONLY. One finite, no-console cmd startup/output control before the
// existing NativeClientOnly Terminal case. This is not a client-input oracle,
// product authority, or an all-variables-equal ConPTY comparison.
struct MediumDirectCmdProbeObservation {
    std::vector<std::pair<std::string,std::string>> records;
    bool setup_complete{},launch_attempted{},created{},output_exact{},cleanup_known{},may_continue{};
    std::string reason{"not_started"};
    void emit(std::string type,std::string fields){records.emplace_back(std::move(type),std::move(fields));}
};
struct MediumDirectCmdProbeHandles {
    HANDLE process{},thread{},child_token{},caller_token{};
    bool process_close_attempted{},thread_close_attempted{},token_close_attempted{};
    bool process_closed{true},thread_closed{true},tokens_closed{true};
    void finish()noexcept{
        if(child_token){const auto actual=child_token;child_token=nullptr;token_close_attempted=true;tokens_closed=CloseHandle(actual)!=FALSE&&tokens_closed;}
        if(caller_token){const auto actual=caller_token;caller_token=nullptr;token_close_attempted=true;tokens_closed=CloseHandle(actual)!=FALSE&&tokens_closed;}
        if(thread){const auto actual=thread;thread=nullptr;thread_close_attempted=true;thread_closed=CloseHandle(actual)!=FALSE;}
        if(process){const auto actual=process;process=nullptr;process_close_attempted=true;process_closed=CloseHandle(actual)!=FALSE;}
    }
    ~MediumDirectCmdProbeHandles()noexcept{finish();}
};
MediumDirectCmdProbeObservation medium_direct_cmd_probe(
    const std::wstring& run,const fs::path& output,const fs::path& working_directory,
    const std::wstring& full_desktop,const std::string& manifest_sha256,ULONGLONG caller_deadline,
    const std::function<bool()>& active,const std::function<bool(const char*)>& product_absent,bool& quit_seen){
    MediumDirectCmdProbeObservation out;MediumDirectCmdProbeHandles owned;
    const auto script=output/(run+L"-direct-cmd-bootstrap.cmd"),result=output/(run+L"-direct-cmd-probe.txt");
    const fs::path cmd=L"C:\\Windows\\System32\\cmd.exe",input=L"C:\\PaneBindV1\\Input";
    const auto expected="PaneBind-direct-cmd-probe-"+utf8(run)+"\r\n";
    MediumHeldProcessExitObservation exit_observation;DWORD pid{},tid{},create_error{};
    std::uint64_t birth{};bool files_closed{true},identity_contradiction{},wait_observed{},post_launch_context{};
    const auto regular=[](const fs::path& path){const auto a=GetFileAttributesW(path.c_str());
        return a!=INVALID_FILE_ATTRIBUTES&&!(a&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));};
    try{
        const auto manifest=read(input/L"manifest.json",256*1024);const auto files=manifest.find("\"files\"");
        const auto identity=files==manifest.npos?std::string{}:manifest.substr(0,files);
        std::array<wchar_t,128> account{};DWORD account_size=static_cast<DWORD>(account.size());
        if(run.size()!=32||run.find_first_not_of(L"0123456789abcdef")!=std::wstring::npos||quit_seen||
           !active||!active()||!product_absent||!product_absent("before_direct_cmd_probe_setup")||
           !medium(GetCurrentProcess())||!GetUserNameW(account.data(),&account_size)||!equal(account.data(),L"WDAGUtilityAccount")||
           !equal(full_desktop,L"WinSta0\\PaneBindV1_"+run)||!equal(output.wstring(),L"C:\\PaneBindV1\\Output")||
           !plain_directory(output)||!plain_directory(working_directory)||!regular(cmd)||
           !marker(input/L"panebind-v1-input.marker",run)||!marker(output/L"panebind-v1-output.marker",run)||
           manifest_sha256.size()!=64||hash(input/L"manifest.json")!=manifest_sha256||
           portable_field(identity,"run_id")!=utf8(run)||portable_field(identity,"driver_source_commit")!=PANEBIND_BUILD_SHA||
           portable_field(manifest,"guest_suite")!="MediumDesktopNativeClientOnly")
            throw std::runtime_error("direct_cmd_exact_guest_Run_source_manifest_context_required");
        const auto body="@echo off\r\nfor %%F in (\""+utf8(result.wstring())+
            "\") do if not \"%%~zF\"==\"0\" exit /b 19\r\n>\""+utf8(result.wstring())+
            "\" echo PaneBind-direct-cmd-probe-"+utf8(run)+"\r\nif errorlevel 1 exit /b 20\r\nexit /b 0\r\n";
        const auto create_once=[&](const fs::path& path,const std::string& contents){
            if(!active()||quit_seen)throw std::runtime_error("direct_cmd_Run_revoked_before_CREATE_NEW");
            SetLastError(ERROR_SUCCESS);Handle file(CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,
                CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
            const bool opened=file.value&&file.value!=INVALID_HANDLE_VALUE;const DWORD open_error=opened?0:GetLastError();
            DWORD written{},write_error{},flush_error{},close_error{};bool wrote{},flushed{},closed{};
            if(opened){
                if(!contents.empty()){SetLastError(ERROR_SUCCESS);wrote=WriteFile(file.value,contents.data(),static_cast<DWORD>(contents.size()),&written,nullptr)!=FALSE;
                    write_error=wrote?0:GetLastError();}
                SetLastError(ERROR_SUCCESS);flushed=FlushFileBuffers(file.value)!=FALSE;flush_error=flushed?0:GetLastError();
                const auto actual=file.value;file.value=nullptr;SetLastError(ERROR_SUCCESS);closed=CloseHandle(actual)!=FALSE;close_error=closed?0:GetLastError();
                files_closed=files_closed&&closed;
            }
            out.emit("medium_direct_cmd_probe_file","\"path\":"+quote(utf8(path.wstring()))+
                ",\"CreateFile_CREATE_NEW_attempted\":true,\"CreateFile_return\":"+(opened?"true":"false")+
                ",\"create_error\":"+std::to_string(open_error)+",\"WriteFile_attempted\":"+(opened&&!contents.empty()?"true":"false")+
                ",\"WriteFile_return\":"+(opened&&!contents.empty()?(wrote?"true":"false"):"null")+
                ",\"write_error\":"+(opened&&!contents.empty()?std::to_string(write_error):"null")+",\"bytes_written\":"+std::to_string(written)+
                ",\"FlushFileBuffers_return\":"+(opened?(flushed?"true":"false"):"null")+",\"flush_error\":"+(opened?std::to_string(flush_error):"null")+
                ",\"CloseHandle_return\":"+(opened?(closed?"true":"false"):"null")+",\"close_error\":"+(opened?std::to_string(close_error):"null")+
                ",\"empty_output_reservation_only\":"+(contents.empty()?"true":"false")+",\"Terminal_ready_or_client_nonce_written\":false");
            if(!opened||(!contents.empty()&&(!wrote||written!=contents.size()))||!flushed||!closed||
               !regular(path)||fs::file_size(path)!=contents.size()||read(path,1024)!=contents)
                throw std::runtime_error("direct_cmd_CREATE_NEW_file_write_or_readback_unconfirmed");
        };
        create_once(script,body);create_once(result,{});out.setup_complete=true;
        const auto cmd_hash=hash(cmd),script_hash=hash(script);
        if(cmd_hash.size()!=64||script_hash.size()!=64||!active()||quit_seen||
           !product_absent("immediately_before_direct_cmd_CreateProcessW"))
            throw std::runtime_error("direct_cmd_identity_or_product_absence_lost_before_launch");
        std::wstring command=L"\""+cmd.wstring()+L"\" /d /q /c \"\""+script.wstring()+L"\"\"";
        STARTUPINFOW startup{sizeof(startup)};startup.lpDesktop=const_cast<wchar_t*>(full_desktop.c_str());PROCESS_INFORMATION info{};
        const auto before=GetTickCount64();SetLastError(ERROR_SUCCESS);out.launch_attempted=true;
        out.created=CreateProcessW(cmd.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,
            working_directory.c_str(),&startup,&info)!=FALSE;
        create_error=out.created?0:GetLastError();const auto after=GetTickCount64();
        if(out.created){owned.process=info.hProcess;owned.thread=info.hThread;pid=info.dwProcessId;tid=info.dwThreadId;birth=process_birth(owned.process);}
        post_launch_context=!quit_seen&&active()&&product_absent("immediately_after_direct_cmd_CreateProcessW");
        out.emit("medium_direct_cmd_probe_launch","\"CreateProcessW_attempted\":true,\"native_return\":"+(out.created?"true":"false")+
            ",\"error\":"+std::to_string(create_error)+",\"lpApplicationName\":"+quote(utf8(cmd.wstring()))+",\"command\":"+quote(utf8(command))+
            ",\"cmd_sha256\":"+quote(cmd_hash)+",\"script_sha256\":"+quote(script_hash)+",\"lpDesktop\":"+quote(utf8(full_desktop))+
            ",\"working_directory\":"+quote(utf8(working_directory.wstring()))+",\"creation_flags\":"+std::to_string(CREATE_NO_WINDOW)+
            ",\"handle_inheritance\":false,\"environment\":\"caller_inherited\",\"alternate_token_API_used\":false,"+
            "\"caller_process_Medium_rechecked\":true,\"PSEUDOCONSOLE_attribute_used\":false,\"finite_slash_c_not_Terminal_slash_k\":true,"+
            "\"PID\":"+std::to_string(pid)+",\"TID\":"+std::to_string(tid)+",\"birth\":"+std::to_string(birth)+
            ",\"before_tick\":"+std::to_string(before)+",\"after_tick\":"+std::to_string(after)+
            ",\"native_call_completed_after_caller_deadline\":"+(after>=caller_deadline?"true":"false")+
            ",\"immediate_post_launch_context_and_product_absence_rechecked\":"+(post_launch_context?"true":"false")+
            ",\"native_return_is_not_output_or_Terminal_PASS\":true,\"client_input_sent\":false");
        if(out.created){
            int rid=-1;bool elevated{};DWORD child_session{},caller_session{};
            const bool child_session_known=ProcessIdToSessionId(pid,&child_session)!=FALSE;
            const bool caller_session_known=ProcessIdToSessionId(GetCurrentProcessId(),&caller_session)!=FALSE;
            const auto actual_image=medium_desktop_image(owned.process);
            SetLastError(ERROR_SUCCESS);const HDESK child_desktop=GetThreadDesktop(tid);
            const DWORD child_desktop_error=child_desktop?0:GetLastError();
            const auto actual_desktop=child_desktop?medium_desktop_object_name(child_desktop):std::wstring{};
            const bool desktop_known=!actual_desktop.empty();
            alignas(TOKEN_USER) std::array<BYTE,512> child_user{},caller_user{};
            alignas(TOKEN_MANDATORY_LABEL) std::array<BYTE,512> integrity{};DWORD bytes{};TOKEN_ELEVATION elevation{};
            SetLastError(ERROR_SUCCESS);const bool child_open=OpenProcessToken(owned.process,TOKEN_QUERY,&owned.child_token)!=FALSE;
            const DWORD child_open_error=child_open?0:GetLastError();
            SetLastError(ERROR_SUCCESS);const bool caller_open=OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&owned.caller_token)!=FALSE;
            const DWORD caller_open_error=caller_open?0:GetLastError();
            SetLastError(ERROR_SUCCESS);const bool integrity_return=child_open&&GetTokenInformation(owned.child_token,TokenIntegrityLevel,
                integrity.data(),static_cast<DWORD>(integrity.size()),&bytes)!=FALSE;
            const DWORD integrity_error=integrity_return?0:GetLastError();
            SetLastError(ERROR_SUCCESS);const bool elevation_return=child_open&&GetTokenInformation(owned.child_token,TokenElevation,&elevation,sizeof(elevation),&bytes)!=FALSE;
            const DWORD elevation_error=elevation_return?0:GetLastError();
            const auto integrity_sid=integrity_return?reinterpret_cast<TOKEN_MANDATORY_LABEL*>(integrity.data())->Label.Sid:nullptr;
            const bool token_known=integrity_return&&elevation_return&&integrity_sid&&IsValidSid(integrity_sid)&&*GetSidSubAuthorityCount(integrity_sid);
            if(token_known){rid=static_cast<int>(*GetSidSubAuthority(integrity_sid,*GetSidSubAuthorityCount(integrity_sid)-1U));elevated=elevation.TokenIsElevated!=0;}
            const bool actual_medium=token_known&&rid==SECURITY_MANDATORY_MEDIUM_RID&&!elevated;
            SetLastError(ERROR_SUCCESS);const bool child_user_return=child_open&&GetTokenInformation(owned.child_token,TokenUser,child_user.data(),static_cast<DWORD>(child_user.size()),&bytes)!=FALSE;
            const DWORD child_user_error=child_user_return?0:GetLastError();
            const bool child_sid_known=child_user_return&&
                IsValidSid(reinterpret_cast<TOKEN_USER*>(child_user.data())->User.Sid);
            SetLastError(ERROR_SUCCESS);const bool caller_user_return=caller_open&&GetTokenInformation(owned.caller_token,TokenUser,caller_user.data(),static_cast<DWORD>(caller_user.size()),&bytes)!=FALSE;
            const DWORD caller_user_error=caller_user_return?0:GetLastError();
            const bool caller_sid_known=caller_user_return&&
                IsValidSid(reinterpret_cast<TOKEN_USER*>(caller_user.data())->User.Sid);
            const bool sid_known=child_sid_known&&caller_sid_known;
            const bool same_sid=sid_known&&EqualSid(reinterpret_cast<TOKEN_USER*>(child_user.data())->User.Sid,reinterpret_cast<TOKEN_USER*>(caller_user.data())->User.Sid);
            const bool session_known=child_session_known&&caller_session_known,same_session=session_known&&child_session==caller_session;
            identity_contradiction=(token_known&&!actual_medium)||(sid_known&&!same_sid)||(session_known&&!same_session)||
                (!actual_image.empty()&&!equal(actual_image,cmd.wstring()))||(desktop_known&&!equal(actual_desktop,L"PaneBindV1_"+run));
            out.emit("medium_direct_cmd_probe_identity","\"PID\":"+std::to_string(pid)+",\"birth\":"+std::to_string(birth)+
                ",\"held_PID_birth_match\":"+(GetProcessId(owned.process)==pid&&birth&&process_birth(owned.process)==birth?"true":"false")+
                ",\"actual_image\":"+(actual_image.empty()?"null":quote(utf8(actual_image)))+
                ",\"GetThreadDesktop_return\":"+(child_desktop?"true":"false")+",\"GetThreadDesktop_error\":"+std::to_string(child_desktop_error)+
                ",\"actual_desktop_name\":"+(desktop_known?quote(utf8(actual_desktop)):"null")+
                ",\"actual_Run_desktop_name_matches\":"+(desktop_known?(equal(actual_desktop,L"PaneBindV1_"+run)?"true":"false"):"null")+
                ",\"child_OpenProcessToken_return\":"+(child_open?"true":"false")+",\"child_OpenProcessToken_error\":"+std::to_string(child_open_error)+
                ",\"caller_OpenProcessToken_return\":"+(caller_open?"true":"false")+",\"caller_OpenProcessToken_error\":"+std::to_string(caller_open_error)+
                ",\"integrity_query_attempted\":"+(child_open?"true":"false")+",\"integrity_query_return\":"+(child_open?(integrity_return?"true":"false"):"null")+
                ",\"integrity_query_error\":"+(child_open?std::to_string(integrity_error):"null")+
                ",\"elevation_query_attempted\":"+(child_open?"true":"false")+",\"elevation_query_return\":"+(child_open?(elevation_return?"true":"false"):"null")+
                ",\"elevation_query_error\":"+(child_open?std::to_string(elevation_error):"null")+
                ",\"token_integrity_and_elevation_known\":"+(token_known?"true":"false")+
                ",\"actual_integrity_RID\":"+(token_known?std::to_string(rid):"null")+",\"actual_elevated\":"+(token_known?(elevated?"true":"false"):"null")+
                ",\"actual_Medium_nonElevated\":"+(token_known?(actual_medium?"true":"false"):"null")+
                ",\"same_SID_known\":"+(sid_known?"true":"false")+",\"actual_same_SID\":"+(sid_known?(same_sid?"true":"false"):"null")+
                ",\"child_TokenUser_attempted\":"+(child_open?"true":"false")+",\"child_TokenUser_error\":"+(child_open?std::to_string(child_user_error):"null")+
                ",\"caller_TokenUser_attempted\":"+(caller_open?"true":"false")+",\"caller_TokenUser_error\":"+(caller_open?std::to_string(caller_user_error):"null")+
                ",\"session_known\":"+(session_known?"true":"false")+",\"actual_session\":"+(child_session_known?std::to_string(child_session):"null")+
                ",\"actual_same_session\":"+(session_known?(same_session?"true":"false"):"null")+
                ",\"known_identity_contradiction\":"+(identity_contradiction?"true":"false")+",\"UNKNOWN_facts_not_rewritten_true\":true");
            exit_observation=medium_observe_held_process_exit(owned.process,pid,birth,caller_deadline,quit_seen,1500);wait_observed=true;
            out.emit("medium_direct_cmd_probe_exit","\"observation\":"+exit_observation.json());
        }
        out.reason=!out.created?"direct_cmd_CreateProcessW_failed":identity_contradiction?"direct_cmd_known_identity_contradiction":
            !exit_observation.signalled?"direct_cmd_exit_not_observed_within_existing_budget":
            !exit_observation.exit_known?"direct_cmd_exit_code_UNKNOWN":exit_observation.exit_code?"direct_cmd_nonzero_exit":"direct_cmd_normal_exit_observed";
    }catch(const std::exception& failure){out.reason=failure.what();}
    catch(...){out.reason="direct_cmd_probe_unknown_exception";}
    if(owned.process&&!wait_observed){
        exit_observation=medium_observe_held_process_exit(owned.process,pid,birth,GetTickCount64(),quit_seen,0);
        out.emit("medium_direct_cmd_probe_exit","\"exception_cleanup_nonblocking_only\":true,\"observation\":"+exit_observation.json());
    }
    if(out.setup_complete){
        const bool output_plain=regular(result);std::error_code size_error;const auto size=output_plain?fs::file_size(result,size_error):0;
        const bool readable=output_plain&&!size_error&&size<=256;const auto actual=readable?read(result,256):std::string{};
        out.output_exact=readable&&actual==expected;
        if(out.reason=="direct_cmd_normal_exit_observed")out.reason=out.output_exact?
            "direct_cmd_normal_exit_and_exact_output_observed":"direct_cmd_normal_exit_without_exact_output";
        out.emit("medium_direct_cmd_probe_output","\"file\":"+quote(utf8(result.wstring()))+",\"actual_plain_file\":"+(output_plain?"true":"false")+
            ",\"bytes\":"+(output_plain&&!size_error?std::to_string(size):"null")+",\"bounded_read_attempted\":"+(readable?"true":"false")+
            ",\"actual_exact_probe_output\":"+(out.output_exact?"true":"false")+",\"sha256\":"+(readable?quote(hash(result)):"null")+
            ",\"Terminal_ShellReady_satisfied\":false,\"client_nonce_satisfied\":false,\"client_input_sent\":false");
    }
    owned.finish();
    out.cleanup_known=files_closed&&owned.process_closed&&owned.thread_closed&&owned.tokens_closed&&(!out.created||exit_observation.signalled);
    out.may_continue=out.setup_complete&&out.launch_attempted&&post_launch_context&&out.cleanup_known&&!identity_contradiction&&!quit_seen&&active&&active();
    out.emit("medium_direct_cmd_probe_summary","\"reason\":"+quote(out.reason)+",\"launch_attempted\":"+(out.launch_attempted?"true":"false")+
        ",\"created\":"+(out.created?"true":"false")+",\"actual_exact_probe_output\":"+(out.output_exact?"true":"false")+
        ",\"held_process_signalled\":"+(out.created?(exit_observation.signalled?"true":"false"):"null")+
        ",\"normal_exit_and_exact_output_observed\":"+(out.created&&exit_observation.signalled&&exit_observation.exit_known&&
            exit_observation.exit_code==0&&out.output_exact?"true":"false")+
        ",\"process_handle_CloseHandle_attempted\":"+(owned.process_close_attempted?"true":"false")+
        ",\"process_handle_closed\":"+(owned.process_closed?"true":"false")+",\"thread_handle_CloseHandle_attempted\":"+(owned.thread_close_attempted?"true":"false")+
        ",\"thread_handle_closed\":"+(owned.thread_closed?"true":"false")+",\"cleanup_known\":"+(out.cleanup_known?"true":"false")+
        ",\"token_handle_CloseHandle_attempted\":"+(owned.token_close_attempted?"true":"false")+",\"token_handles_closed\":"+(owned.tokens_closed?"true":"false")+
        ",\"may_continue_existing_Terminal_case_not_input_authority\":"+(out.may_continue?"true":"false")+
        ",\"terminated_or_replayed_input\":false,\"all_descendants_exit_inferred\":false,\"Terminal_or_product_PASS\":false");
    return out;
}
