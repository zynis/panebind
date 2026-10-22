#pragma once

// TEST ONLY. Included after the existing MediumDesktop* facts and
// fixture helpers. Not a product adapter or an input route.
// The package-bound caller already passed real guest/manifest/activation
// guards. Its actual manifest digest is independently reread below.
// Existing asset layout is NOT Input\Apps\Terminal:
//   Input\Terminal-test.zip
//   C:\PaneBindV1\Terminal\<run>\<configuration>\terminal-1.25.2733.0\WindowsTerminal.exe
struct MediumTerminalNativeRequest {
    std::string configuration{"Release"},manifest_sha256;
};
struct MediumTerminalNativeObservation {
    std::vector<std::pair<std::string,std::string>> records;
    bool launch_attempted{},launched{},root_ready{},close_attempted{},root_gone{};
    bool hooks_stopped{},local_handles_closed{},product_absence_known{},completed{};
    std::string reason{"not_started"};
    void emit(std::string type,std::string fields){records.emplace_back(std::move(type),std::move(fields));}
};
struct MediumTerminalNativeEvents {
    HANDLE changed{};DWORD pid{};HWND root{};unsigned notifications{};bool overflow{};
};
thread_local MediumTerminalNativeEvents* medium_terminal_native_events{};
void CALLBACK medium_terminal_native_event(HWINEVENTHOOK,DWORD,HWND window,LONG object,LONG child,DWORD,DWORD){
    auto* state=medium_terminal_native_events;if(!state||!window||object!=OBJID_WINDOW||child)return;
    DWORD pid{};GetWindowThreadProcessId(window,&pid);
    // A destroyed root may no longer yield its PID. Only a previously bound
    // exact root can wake the owner then; no new window is adopted.
    if((!state->pid||pid!=state->pid)&&window!=state->root)return;
    if(state->notifications<64)++state->notifications;else state->overflow=true;
    if(state->changed)SetEvent(state->changed);
}

// No-allocation fallback: read/hash/emit may throw (including allocation
// failures), so tail-only cleanup would leave a callback's TLS dangling.
// The owner thread does all cleanup. It never terminates an application.
struct MediumTerminalNativeResources {
    HWINEVENTHOOK& hook;HANDLE& process;HANDLE& thread;MediumTerminalNativeEvents& events;
    bool hook_attempted{},process_attempted{},thread_attempted{},event_attempted{};
    bool hook_ok{true},process_ok{true},thread_ok{true},event_ok{true},unknown{},fallback_used{};
    DWORD hook_error{};
    void finish()noexcept{
        if(hook){
            hook_attempted=true;const auto actual=hook;hook=nullptr;
            SetLastError(ERROR_SUCCESS);hook_ok=UnhookWinEvent(actual)!=FALSE;
            hook_error=hook_ok?0:GetLastError();unknown=unknown||!hook_ok;
        }
        // Unhook failure remains UNKNOWN, not a licence for API retries.
        // A later same-thread callback finds NULL, never dead stack state.
        if(medium_terminal_native_events==&events)medium_terminal_native_events=nullptr;
        if(process){process_attempted=true;const auto actual=process;process=nullptr;
            process_ok=CloseHandle(actual)!=FALSE;unknown=unknown||!process_ok;}
        if(thread){thread_attempted=true;const auto actual=thread;thread=nullptr;
            thread_ok=CloseHandle(actual)!=FALSE;unknown=unknown||!thread_ok;}
        if(events.changed){event_attempted=true;const auto actual=events.changed;events.changed=nullptr;
            event_ok=CloseHandle(actual)!=FALSE;unknown=unknown||!event_ok;}
    }
    ~MediumTerminalNativeResources()noexcept{
        if(hook||process||thread||events.changed||medium_terminal_native_events==&events){
            fallback_used=true;finish();
        }
    }
};

MediumTerminalNativeObservation medium_terminal_native_case(
    const std::wstring& run,HDESK created,const MediumTerminalNativeRequest& request,
    const std::function<bool()>& exact_medium_Shell_Tray,ULONGLONG caller_deadline_tick){
    MediumTerminalNativeObservation out;
    const auto name=L"PaneBindV1_"+run,full_name=L"WinSta0\\"+name;
    const fs::path input=L"C:\\PaneBindV1\\Input",output=L"C:\\PaneBindV1\\Output";
    const auto root=fs::path(L"C:\\PaneBindV1\\Terminal")/run;
    const auto configuration=root/std::wstring(request.configuration.begin(),request.configuration.end());
    const auto directory=configuration/L"terminal-1.25.2733.0",image=directory/L"WindowsTerminal.exe";
    constexpr const char* zip_pin="BF3EF2012F6C44D8340A4C58125ACC9498D19B580F9890DC043CDF831852E796";
    constexpr const char* exe_pin="1CBE52914F7437A4F064327383F1B4C45FE2EB8E92D4F8DAE0AD7EFCB13A20A1";
    MediumTerminalNativeEvents events;HWINEVENTHOOK hook{};HANDLE process{},thread{};
    DWORD pid{},session{},window_tid{};std::uint64_t birth{};HWND window{};bool quit_seen{};
    MediumTerminalNativeResources resources{hook,process,thread,events};
    MediumDesktopWindows accepted_first,accepted_second;
    const auto regular=[](const fs::path& path){const auto a=GetFileAttributesW(path.c_str());
        return a!=INVALID_FILE_ATTRIBUTES&&!(a&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));};
    const auto active=[&]{
        if(GetTickCount64()>=caller_deadline_tick||!created||!medium(GetCurrentProcess())||
           !equal(medium_desktop_object_name(GetProcessWindowStation()),L"WinSta0")||
           !equal(medium_desktop_object_name(GetThreadDesktop(GetCurrentThreadId())),name)||
           !equal(medium_desktop_object_name(created),name)||!exact_medium_Shell_Tray||!exact_medium_Shell_Tray())return false;
        const HDESK current=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);if(!current)return false;
        BOOL receiving{};DWORD size{};const bool good=equal(medium_desktop_object_name(current),name)&&
            GetUserObjectInformationW(current,UOI_IO,&receiving,sizeof(receiving),&size)&&receiving;
        const bool closed=CloseDesktop(current)!=FALSE;return good&&closed;
    };
    const auto absent=[&](const char* stage){
        SetLastError(ERROR_SUCCESS);const HANDLE mutex=OpenMutexW(SYNCHRONIZE,FALSE,L"Local\\PaneBind.V1.SingleInstance");
        const DWORD mutex_error=mutex?0:GetLastError();const bool closed=!mutex||CloseHandle(mutex)!=FALSE;
        // Access denial is UNKNOWN, never "no product". This session mutex
        // also catches a product on Default rather than only this desktop.
        const bool mutex_absent=!mutex&&mutex_error==ERROR_FILE_NOT_FOUND;
        Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));PROCESSENTRY32W item{sizeof(item)};
        unsigned count{},matches{};bool complete{};DWORD error{};
        if(snapshot.value!=INVALID_HANDLE_VALUE){BOOL more=Process32FirstW(snapshot.value,&item);
            if(!more)error=GetLastError();
            while(more){if(++count>4096)break;if(equal(item.szExeFile,L"panebind.exe"))++matches;
                more=Process32NextW(snapshot.value,&item);if(!more){error=GetLastError();complete=error==ERROR_NO_MORE_FILES;}}
        }else error=GetLastError();
        const HWND app=FindWindowW(L"PaneBind.V1.Application",L"PaneBind");
        const bool good=mutex_absent&&closed&&complete&&!matches&&!app;
        out.emit("medium_terminal_product_absence","\"stage\":"+quote(stage)+",\"session_mutex_OpenMutex_return\":"+
            (mutex?"true":"false")+",\"OpenMutex_error\":"+std::to_string(mutex_error)+",\"opened_mutex_closed\":"+(closed?"true":"false")+
            ",\"process_snapshot_complete\":"+(complete?"true":"false")+",\"snapshot_error\":"+std::to_string(error)+
            ",\"candidate_named_processes\":"+std::to_string(matches)+",\"current_desktop_product_root\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(app))+",\"observed_absent\":"+(good?"true":"false")+
            ",\"scope\":\"session_mutex_candidate_basename_and_current_desktop_root_not_all_renamed_processes\"");return good;
    };
    const auto exact_process=[&]{DWORD actual_session{};return process&&pid&&birth&&GetProcessId(process)==pid&&
        process_birth(process)==birth&&WaitForSingleObject(process,0)==WAIT_TIMEOUT&&medium(process)&&
        input_actor_same_user(process)&&ProcessIdToSessionId(pid,&actual_session)&&actual_session==session&&
        equal(medium_desktop_image(process),image.wstring());};
    const auto root_facts=[&](HWND candidate,const MediumDesktopWindows& first,const MediumDesktopWindows& second){
        if(!exact_process()||!first.complete||!second.complete||!equal(medium_desktop_object_name(created),name)||
           std::find(first.windows.begin(),first.windows.end(),candidate)==first.windows.end()||
           std::find(second.windows.begin(),second.windows.end(),candidate)==second.windows.end())return false;
        const auto a=MediumDesktopWindowFact::read(candidate,pid,process,birth,name);
        const auto b=MediumDesktopWindowFact::read(candidate,pid,process,birth,name);
        return a.type==L"CASCADIA_HOSTING_WINDOW_CLASS"&&a.readonly_same(b)&&(!window||candidate==window)&&
            (!window_tid||a.tid==window_tid);
    };
    const auto exact_window=[&]{const auto first=MediumDesktopWindows::read(created),second=MediumDesktopWindows::read(created);
        return window&&root_facts(window,first,second);};
    const auto ready=[&]{
        if(!active()||!exact_process())return false;
        const auto first=MediumDesktopWindows::read(created),second=MediumDesktopWindows::read(created);
        HWND candidate{};unsigned matches{};
        for(const HWND h:first.windows)if(root_facts(h,first,second)){candidate=h;++matches;}
        if(matches!=1)return false;accepted_first=first;accepted_second=second;
        window=candidate;DWORD ignored{};window_tid=GetWindowThreadProcessId(window,&ignored);
        events.root=window;return true;
    };
    const auto wait_for=[&](const std::function<bool()>& predicate,ULONGLONG deadline){
        for(;;){
            if(quit_seen||!active()||events.overflow)return false;
            // Reset before observing: an event racing the observation remains
            // signalled. No fixed-interval window enumeration or sleep.
            if(!ResetEvent(events.changed))return false;
            if(predicate())return true;
            const auto now=GetTickCount64();if(now>=deadline||now>=caller_deadline_tick)return false;
            const DWORD timeout=static_cast<DWORD>((std::min)(deadline,caller_deadline_tick)-now);
            // A child exit can end readiness immediately; do not wait out a
            // full deadline after an actual C0000142/native process failure.
            const HANDLE handles[2]{events.changed,process};const DWORD count=process?2:1;
            const auto result=MsgWaitForMultipleObjectsEx(count,handles,timeout,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(result==WAIT_FAILED)return false;
            if(process&&result==WAIT_OBJECT_0+1)return active()&&predicate();
            if(result==WAIT_OBJECT_0+count){MSG message{};unsigned budget=256;
                while(budget--&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
                    if(message.message==WM_QUIT){quit_seen=true;return false;}TranslateMessage(&message);DispatchMessageW(&message);}}
            if(result==WAIT_TIMEOUT)return active()&&predicate();
        }
    };
    try{
        if(run.size()!=32||run.find_first_not_of(L"0123456789abcdef")!=std::wstring::npos||
           (request.configuration!="Release"&&request.configuration!="Debug")||medium_terminal_native_events||
           !active()||!marker(input/L"panebind-v1-input.marker",run)||!marker(output/L"panebind-v1-output.marker",run)||
           !ProcessIdToSessionId(GetCurrentProcessId(),&session)||!session)
            throw std::runtime_error("exact_active_Run_Medium_guest_context_required");
        const auto manifest_hash=hash(input/L"manifest.json");
        const bool manifest_bound=request.manifest_sha256.size()==64&&
            request.manifest_sha256.find_first_not_of("0123456789ABCDEF")==std::string::npos&&manifest_hash==request.manifest_sha256;
        const bool prepared=manifest_bound&&regular(input/L"Terminal-test.zip")&&
            fs::file_size(input/L"Terminal-test.zip")==12127272&&hash(input/L"Terminal-test.zip")==zip_pin&&
            plain_directory(root.parent_path())&&plain_directory(root)&&plain_directory(configuration)&&plain_directory(directory)&&
            regular(image)&&hash(image)==exe_pin&&regular(directory/L".portable")&&fs::file_size(directory/L".portable")==0&&
            marker(directory/L"panebind-v1-test-application.marker",run);
        out.emit("medium_terminal_asset","\"actual_manifest_sha256\":"+quote(manifest_hash)+",\"expected_manifest_sha256\":"+quote(request.manifest_sha256)+
            ",\"manifest_digest_matches_package_bound_caller\":"+(manifest_bound?"true":"false")+",\"ZIP_sha256_pin\":"+quote(zip_pin)+
            ",\"EXE_sha256_pin\":"+quote(exe_pin)+",\"version\":\"1.25.2733.0\",\"actual_exe\":"+quote(utf8(image.wstring()))+
            ",\"prepared\":"+(prepared?"true":"false")+",\"Appx_registration_or_default_terminal_change\":false");
        if(!prepared||!absent("before_launch"))throw std::runtime_error("Terminal_asset_or_true_product_absence_unconfirmed");
        const auto baseline=MediumDesktopWindows::read(created);
        if(!baseline.complete)throw std::runtime_error("new_root_baseline_enumeration_UNKNOWN");
        events.changed=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!events.changed)throw std::runtime_error("own_event_create_failed");
        medium_terminal_native_events=&events;SetLastError(ERROR_SUCCESS);
        hook=SetWinEventHook(EVENT_OBJECT_CREATE,EVENT_OBJECT_HIDE,nullptr,medium_terminal_native_event,0,0,WINEVENT_OUTOFCONTEXT);
        const DWORD hook_error=hook?0:GetLastError();
        out.emit("medium_terminal_hook","\"SetWinEventHook_return\":"+std::string(hook?"true":"false")+",\"error\":"+std::to_string(hook_error)+
            ",\"observer_only\":true,\"callback_capacity\":64,\"disk_IO_on_callback\":false");
        if(!hook)throw std::runtime_error("own_window_event_hook_unavailable");
        const auto begin=GetTickCount64(),ready_deadline=(std::min)(begin+10000,caller_deadline_tick);
        if(!active()||!absent("immediately_before_CreateProcessW"))throw std::runtime_error("context_or_product_absence_lost_before_launch");
        std::wstring command=L"\""+image.wstring()+L"\" --window new new-tab --title \"PaneBind-"+run+
            L"-Terminal\" --suppressApplicationTitle \"C:\\Windows\\System32\\cmd.exe\" /d /q /k";
        STARTUPINFOW startup{sizeof(startup)};startup.lpDesktop=const_cast<wchar_t*>(full_name.c_str());PROCESS_INFORMATION info{};
        const auto native_begin=GetTickCount64();SetLastError(ERROR_SUCCESS);out.launch_attempted=true;
        out.launched=CreateProcessW(image.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NEW_PROCESS_GROUP,nullptr,directory.c_str(),&startup,&info)!=FALSE;
        const DWORD error=out.launched?0:GetLastError();const auto native_end=GetTickCount64();
        if(out.launched){process=info.hProcess;thread=info.hThread;pid=info.dwProcessId;birth=process_birth(process);events.pid=pid;}
        out.emit("medium_terminal_launch","\"CreateProcessW_attempted\":true,\"native_return\":"+std::string(out.launched?"true":"false")+
            ",\"error\":"+std::to_string(error)+",\"lpDesktop\":"+quote(utf8(full_name))+",\"PID\":"+std::to_string(pid)+
            ",\"birth\":"+std::to_string(birth)+",\"command\":"+quote(utf8(command))+",\"working_directory\":"+quote(utf8(directory.wstring()))+
            ",\"same_existing_Medium_token\":true,\"handle_inheritance\":false,\"before_tick\":"+std::to_string(native_begin)+
            ",\"after_tick\":"+std::to_string(native_end)+",\"native_call_completed_after_caller_deadline\":"+
            (native_end>=caller_deadline_tick?"true":"false")+",\"launch_is_not_root_or_client_PASS\":true");
        if(!out.launched)throw std::runtime_error("actual_Terminal_CreateProcessW_failed");
        out.root_ready=wait_for([&]{if(!ready())return false;return std::find(baseline.windows.begin(),baseline.windows.end(),window)==baseline.windows.end();},ready_deadline);
        DWORD exit{};const bool exit_known=GetExitCodeProcess(process,&exit)!=FALSE;
        out.emit("medium_terminal_root_ready","\"result\":"+quote(out.root_ready?"READY_ONLY":"UNKNOWN_OR_BLOCKED")+
            ",\"root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(window))+",\"TID\":"+std::to_string(window_tid)+
            ",\"exact_live_process\":"+(exact_process()?"true":"false")+",\"actual_exit_code\":"+(exit_known?std::to_string(exit):"null")+
            ",\"event_notifications\":"+std::to_string(events.notifications)+",\"event_overflow\":"+(events.overflow?"true":"false")+
            ",\"elapsed_since_before_launch_ms\":"+std::to_string(GetTickCount64()-begin)+",\"ready_budget_ms\":10000,\"client_input_sent\":false");
        if(!out.root_ready)throw std::runtime_error("no_unique_new_exact_modern_Terminal_root_within_context_budget");
        // Preserve actual independent observations, not only a permission bit.
        int actual_rid=-1;bool elevated{};DWORD token_error{},actual_session{};
        const bool token_medium=medium(process,&actual_rid,&elevated,&token_error);
        const bool same_sid=input_actor_same_user(process),session_known=ProcessIdToSessionId(pid,&actual_session)!=FALSE;
        out.emit("medium_terminal_exact_root", "\"first_actual\":"+MediumDesktopWindowFact::read(window,pid,process,birth,name).json()+
            ",\"second_actual\":"+MediumDesktopWindowFact::read(window,pid,process,birth,name).json()+
            ",\"held_created_desktop\":"+std::to_string(reinterpret_cast<std::uintptr_t>(created))+
            ",\"actual_held_desktop_name\":"+quote(utf8(medium_desktop_object_name(created)))+
            ",\"first_enumeration_complete\":"+(accepted_first.complete?"true":"false")+
            ",\"first_enumeration_error\":"+std::to_string(accepted_first.error)+
            ",\"first_inventory_count\":"+std::to_string(accepted_first.windows.size())+
            ",\"second_enumeration_complete\":"+(accepted_second.complete?"true":"false")+
            ",\"second_enumeration_error\":"+std::to_string(accepted_second.error)+
            ",\"second_inventory_count\":"+std::to_string(accepted_second.windows.size())+
            ",\"root_in_first_inventory\":"+(std::find(accepted_first.windows.begin(),accepted_first.windows.end(),window)!=accepted_first.windows.end()?"true":"false")+
            ",\"root_in_second_inventory\":"+(std::find(accepted_second.windows.begin(),accepted_second.windows.end(),window)!=accepted_second.windows.end()?"true":"false")+
            ",\"actual_image\":"+quote(utf8(medium_desktop_image(process)))+
            ",\"integrity_rid\":"+(actual_rid<0?"null":std::to_string(actual_rid))+
            ",\"elevated\":"+(actual_rid<0?"null":elevated?"true":"false")+
            ",\"token_query_error\":"+std::to_string(token_error)+",\"actual_Medium\":"+(token_medium?"true":"false")+
            ",\"same_SID\":"+(same_sid?"true":"false")+",\"actual_session\":"+(session_known?std::to_string(actual_session):"null")+
            ",\"same_interactive_session\":"+(session_known?(actual_session==session?"true":"false"):"null")+
            ",\"identity_basis\":\"held_exact_child_PID_birth_image_Medium_same_SID_session_and_two_complete_exact_HDESK_memberships\"");
        if(!active()||!exact_window()||!absent("before_WM_NULL"))throw std::runtime_error("exact_context_lost_before_response_check");
        DWORD_PTR response{};SetLastError(ERROR_SUCCESS);
        const bool responded=SendMessageTimeoutW(window,WM_NULL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&response)!=FALSE;
        const DWORD response_error=responded?0:GetLastError();
        out.emit("medium_terminal_response","\"WM_NULL_attempted\":true,\"native_return\":"+std::string(responded?"true":"false")+
            ",\"error\":"+std::to_string(response_error)+",\"timeout_ms\":100,\"client_or_content_semantics_inferred\":false");
        // A failed response does not bypass normal-close identity validation.
        if(!active()||!exact_window()||!absent("before_normal_close"))throw std::runtime_error("no_fresh_close_authority");
        out.close_attempted=true;SetLastError(ERROR_SUCCESS);const bool posted=PostMessageW(window,WM_CLOSE,0,0)!=FALSE;
        const DWORD post_error=posted?0:GetLastError();
        if(posted)out.root_gone=wait_for([&]{if(!IsWindow(window))return true;DWORD actual{};
            const auto tid=GetWindowThreadProcessId(window,&actual);return actual!=pid||tid!=window_tid;},(std::min)(GetTickCount64()+3000,caller_deadline_tick));
        DWORD after_exit{};const bool after_exit_known=GetExitCodeProcess(process,&after_exit)!=FALSE;
        out.emit("medium_terminal_normal_close","\"WM_CLOSE_attempts\":1,\"PostMessage_return\":"+std::string(posted?"true":"false")+
            ",\"error\":"+std::to_string(post_error)+",\"actual_bound_root_gone\":"+(out.root_gone?"true":"false")+
            ",\"process_wait_result\":"+std::to_string(WaitForSingleObject(process,0))+
            ",\"process_exit_code\":"+(after_exit_known?std::to_string(after_exit):"null")+
            ",\"all_descendants_exit_inferred\":false,\"force_terminate_attempted\":false,\"client_input_sent\":false");
        out.product_absence_known=absent("after_normal_close");
        out.completed=responded&&posted&&out.root_gone&&out.product_absence_known&&active();
        out.reason=out.completed?"new_exact_Terminal_root_response_and_normal_close_only":
            !responded?"actual_WM_NULL_unresponsive_normal_close_separate":"normal_close_or_final_context_unconfirmed";
    }catch(const std::exception& error){out.reason=error.what();}
    resources.finish();
    out.hooks_stopped=resources.hook_ok;
    out.local_handles_closed=resources.process_ok&&resources.thread_ok&&resources.event_ok;
    out.completed=out.completed&&out.hooks_stopped&&out.local_handles_closed&&!resources.unknown;
    out.emit("medium_terminal_unhook","\"attempted\":"+std::string(resources.hook_attempted?"true":"false")+
        ",\"native_return\":"+(resources.hook_attempted?(resources.hook_ok?"true":"false"):"null")+
        ",\"error\":"+(resources.hook_attempted?std::to_string(resources.hook_error):"null"));
    out.emit("medium_terminal_summary","\"result\":"+quote(out.completed?"PASS_START_RESPONSE_NORMAL_CLOSE_ONLY":"UNKNOWN_OR_BLOCKED")+
        ",\"reason\":"+quote(out.reason)+",\"own_hook_stopped\":"+(out.hooks_stopped?"true":"false")+
        ",\"process_CloseHandle_attempted\":"+(resources.process_attempted?"true":"false")+
        ",\"process_handle_CloseHandle_return\":"+(resources.process_attempted?(resources.process_ok?"true":"false"):"null")+
        ",\"thread_CloseHandle_attempted\":"+(resources.thread_attempted?"true":"false")+
        ",\"thread_handle_CloseHandle_return\":"+(resources.thread_attempted?(resources.thread_ok?"true":"false"):"null")+
        ",\"event_CloseHandle_attempted\":"+(resources.event_attempted?"true":"false")+
        ",\"event_CloseHandle_return\":"+(resources.event_attempted?(resources.event_ok?"true":"false"):"null")+
        ",\"native_cleanup_unknown\":"+(resources.unknown?"true":"false")+
        ",\"product_started\":false,\"client_input_sent\":false,"+
        "\"product_enhancement_result\":\"NOT_RUN\",\"normal_root_close_is_not_all_process_cleanup\":true,"+
        "\"guest_disposal_is_not_cleanup_PASS\":true,\"human_UAT\":\"NOT_RUN\"");
    return out;
}
