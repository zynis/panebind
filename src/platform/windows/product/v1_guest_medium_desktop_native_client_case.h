#pragma once

// TEST ONLY. Included after MediumDesktopUiInputLease and the native
// desktop/window facts. The default no-input suite and product path remain
// separate from this exact guest-client input case. The controller owns activation and normal
// Terminal close; this function never activates a window or releases capture.
struct MediumTerminalClientObservation {
    std::vector<std::pair<std::string,std::string>> records;
    bool input_attempted{},input_inserted{},mouse_pair_inserted{},focus_observed{};
    bool command_inserted{},enter_inserted{},exact_nonce_observed{},completed{},cleanup_known{};
    std::string reason{"not_started"};
    void emit(std::string type,std::string fields){records.emplace_back(std::move(type),std::move(fields));}
};
struct MediumTerminalClientEvents {HANDLE changed{};DWORD pid{};};
thread_local MediumTerminalClientEvents* medium_terminal_client_events{};
void CALLBACK medium_terminal_client_event(HWINEVENTHOOK,DWORD,HWND window,LONG,LONG,DWORD,DWORD){
    const auto* owner=medium_terminal_client_events;DWORD pid{};
    if(owner&&window&&GetWindowThreadProcessId(window,&pid)&&pid==owner->pid&&owner->changed)SetEvent(owner->changed);
}
// One exact nonce-file notification owner, at most four completed native
// notifications. No fixed file polling. Pending OVERLAPPED/buffer memory is
// never freed just because CancelIoEx returned TRUE.
struct MediumTerminalClientFileWatch {
    HANDLE directory{INVALID_HANDLE_VALUE},event{};OVERLAPPED operation{};
    alignas(DWORD) std::array<BYTE,2048> buffer{};bool pending{},unknown{};
    bool cancel_attempted{},cancel_return{},close_directory_return{true},close_event_return{true};
    DWORD cancel_error{},cancel_wait{WAIT_FAILED},open_error{},arm_error{};
    bool arm_return{};unsigned completions{};
    bool open(const fs::path& parent){
        directory=CreateFileW(parent.c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
            nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,nullptr);
        if(directory==INVALID_HANDLE_VALUE){open_error=GetLastError();return false;}
        event=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!event){open_error=GetLastError();return false;}return arm();
    }
    bool arm()noexcept{
        if(pending||!event||directory==INVALID_HANDLE_VALUE||completions>=4)return false;
        operation={};operation.hEvent=event;if(!ResetEvent(event))return false;SetLastError(ERROR_SUCCESS);
        arm_return=ReadDirectoryChangesW(directory,buffer.data(),static_cast<DWORD>(buffer.size()),FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE,nullptr,&operation,nullptr)!=FALSE;
        arm_error=arm_return?0:GetLastError();pending=arm_return||arm_error==ERROR_IO_PENDING;return pending;
    }
    bool completed(DWORD& bytes,DWORD& error)noexcept{
        if(!pending){error=ERROR_INVALID_FUNCTION;return false;}SetLastError(ERROR_SUCCESS);
        const bool returned=GetOverlappedResult(directory,&operation,&bytes,FALSE)!=FALSE;
        error=returned?0:GetLastError();if(returned||error!=ERROR_IO_INCOMPLETE){pending=false;++completions;}
        return returned;
    }
    bool stop()noexcept{
        if(pending){
            SetLastError(ERROR_SUCCESS);cancel_attempted=true;
            cancel_return=CancelIoEx(directory,&operation)!=FALSE;cancel_error=cancel_return?0:GetLastError();
            cancel_wait=WaitForSingleObject(event,200);DWORD bytes{},error{};
            if(cancel_wait==WAIT_OBJECT_0)completed(bytes,error);
            if(pending){unknown=true;return false;}
        }
        if(directory!=INVALID_HANDLE_VALUE){const auto actual=directory;directory=INVALID_HANDLE_VALUE;
            close_directory_return=CloseHandle(actual)!=FALSE;unknown=unknown||!close_directory_return;}
        if(event){const auto actual=event;event=nullptr;close_event_return=CloseHandle(actual)!=FALSE;unknown=unknown||!close_event_return;}
        return !unknown;
    }
};
struct MediumTerminalClientResources {
    MediumTerminalClientEvents& events;HWINEVENTHOOK& hook;IVirtualDesktopManager*& desktops;
    std::unique_ptr<MediumTerminalClientFileWatch> file_watch;
    bool com_owned{},unhook_attempted{},unhook_ok{true},event_closed{true},unknown{},watch_pending_unknown{};
    bool watch_cancel_attempted{},watch_cancel_return{},watch_closed{};
    DWORD unhook_error{},watch_cancel_error{},watch_cancel_wait{WAIT_FAILED};
    void finish()noexcept{
        if(hook){unhook_attempted=true;const auto actual=hook;hook=nullptr;SetLastError(ERROR_SUCCESS);
            unhook_ok=UnhookWinEvent(actual)!=FALSE;unhook_error=unhook_ok?0:GetLastError();unknown=unknown||!unhook_ok;}
        if(medium_terminal_client_events==&events)medium_terminal_client_events=nullptr;
        if(file_watch){
            watch_closed=file_watch->stop();watch_pending_unknown=file_watch->pending;
            watch_cancel_attempted=file_watch->cancel_attempted;watch_cancel_return=file_watch->cancel_return;
            watch_cancel_error=file_watch->cancel_error;watch_cancel_wait=file_watch->cancel_wait;
            if(!watch_closed){unknown=true;
                // Preserve the actual native I/O buffer/OVERLAPPED and its
                // handles until this dedicated guest process exits. No UAF,
                // background polling or false cleanup-known claim.
                if(file_watch->pending)file_watch.release();
            }
        }
        if(events.changed){const auto actual=events.changed;events.changed=nullptr;event_closed=CloseHandle(actual)!=FALSE;unknown=unknown||!event_closed;}
        if(desktops){auto* actual=desktops;desktops=nullptr;actual->Release();}
        if(com_owned){com_owned=false;CoUninitialize();}
    }
    ~MediumTerminalClientResources()noexcept{finish();}
};

MediumTerminalClientObservation medium_terminal_client_case(
    const std::wstring& run,HDESK created,HWND source,DWORD pid,DWORD tid,
    HANDLE held_process,std::uint64_t birth,const fs::path& nonce_output,
    const std::function<bool()>& active,const std::function<bool()>& exact_window,
    const std::shared_ptr<MediumDesktopUiInputLease>& lease,ULONGLONG deadline_tick){
    MediumTerminalClientObservation out;const auto desktop_name=L"PaneBindV1_"+run;
    const auto expected_output=fs::path(L"C:\\PaneBindV1\\Output")/(run+L"-terminal-client.txt");
    const auto nonce=L"PaneBind-terminal-client-"+run;
    fs::path actual_nonce=nonce_output;
    MediumTerminalClientEvents events{nullptr,pid};HWINEVENTHOOK hook{};IVirtualDesktopManager* desktops{};
    MediumTerminalClientResources resources{events,hook,desktops};bool quit_seen{};DWORD session{};
    const auto remaining=[&](DWORD cap){const auto now=GetTickCount64();return now<deadline_tick?
        static_cast<DWORD>((std::min<ULONGLONG>)(cap,deadline_tick-now)):0;};
    const auto context=[&]{
        if(!remaining(1)||!lease||!lease->valid()||lease->retired()||!active||!exact_window||!active()||!exact_window()||
           !created||!source||!pid||!tid||!held_process||!birth||GetProcessId(held_process)!=pid||
           process_birth(held_process)!=birth||WaitForSingleObject(held_process,0)!=WAIT_TIMEOUT||
           !medium(GetCurrentProcess())||!medium(held_process)||!input_actor_same_user(held_process)||
           !equal(medium_desktop_object_name(created),desktop_name)||
           !equal(medium_desktop_object_name(GetProcessWindowStation()),L"WinSta0")||
           !equal(medium_desktop_object_name(GetThreadDesktop(GetCurrentThreadId())),desktop_name))return false;
        DWORD actual_pid{},actual_session{};const auto actual_tid=GetWindowThreadProcessId(source,&actual_pid);
        if(actual_pid!=pid||actual_tid!=tid||GetAncestor(source,GA_ROOT)!=source||
           !ProcessIdToSessionId(pid,&actual_session)||actual_session!=session)return false;
        const HDESK input=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);if(!input)return false;
        BOOL receiving{};DWORD bytes{};const bool good=equal(medium_desktop_object_name(input),desktop_name)&&
            GetUserObjectInformationW(input,UOI_IO,&receiving,sizeof(receiving),&bytes)&&receiving;
        const bool closed=CloseDesktop(input)!=FALSE;if(!good||!closed)return false;
        SetLastError(ERROR_SUCCESS);const HANDLE mutex=OpenMutexW(SYNCHRONIZE,FALSE,L"Local\\PaneBind.V1.SingleInstance");
        const DWORD error=mutex?0:GetLastError();const bool mutex_closed=!mutex||CloseHandle(mutex)!=FALSE;
        return !mutex&&error==ERROR_FILE_NOT_FOUND&&mutex_closed&&!FindWindowW(L"PaneBind.V1.Application",L"PaneBind");
    };
    const auto no_buttons=[]{
        for(const int key:{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2,VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN,VK_RETURN,VK_ESCAPE})
            if(GetAsyncKeyState(key)&0x8000)return false;return true;
    };
    const auto client_hit=[&](POINT point){
        if(!context())return false;const HWND hit=WindowFromPoint(point);DWORD actual_pid{};
        const auto actual_tid=GetWindowThreadProcessId(hit,&actual_pid);
        return hit&&actual_tid&&actual_pid==pid&&GetAncestor(hit,GA_ROOT)==source;
    };
    const auto foreground_focus=[&](GUITHREADINFO* observed=nullptr,bool* query_known=nullptr,DWORD* query_error=nullptr){
        if(query_known)*query_known=false;if(query_error)*query_error=0;
        if(!context()||GetForegroundWindow()!=source)return false;
        GUITHREADINFO gui{sizeof(gui)};SetLastError(ERROR_SUCCESS);const bool queried=GetGUIThreadInfo(0,&gui)!=FALSE;
        const DWORD error=queried?0:GetLastError();if(query_known)*query_known=queried;if(query_error)*query_error=error;
        if(observed)*observed=gui;if(!queried||gui.hwndActive!=source||!gui.hwndFocus||gui.hwndCapture||
            gui.hwndMoveSize||gui.hwndMenuOwner||(gui.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE)))return false;
        DWORD actual_pid{};const auto actual_tid=GetWindowThreadProcessId(gui.hwndFocus,&actual_pid);
        return actual_tid&&actual_pid==pid&&GetAncestor(gui.hwndFocus,GA_ROOT)==source&&GetForegroundWindow()==source;
    };
    const auto wait_for=[&](const std::function<bool()>& predicate,DWORD cap_ms){
        const auto end=GetTickCount64()+remaining(cap_ms);
        for(;;){
            if(quit_seen||!context())return false;
            if(!ResetEvent(events.changed))return false;
            if(predicate())return true;
            const auto now=GetTickCount64();if(now>=end)return false;
            const bool watch_active=resources.file_watch&&resources.file_watch->pending;
            HANDLE handles[3]{events.changed,held_process,watch_active?resources.file_watch->event:nullptr};
            const DWORD count=watch_active?3:2;
            const auto waited=MsgWaitForMultipleObjectsEx(count,handles,static_cast<DWORD>(end-now),QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(waited==WAIT_FAILED||waited==WAIT_OBJECT_0+1)return false;
            if(watch_active&&waited==WAIT_OBJECT_0+2){
                DWORD bytes{},error{};const bool completed=resources.file_watch->completed(bytes,error);
                out.emit("medium_terminal_client_file_notification","\"GetOverlappedResult_return\":"+std::string(completed?"true":"false")+
                    ",\"error\":"+std::to_string(error)+",\"bytes\":"+std::to_string(bytes)+
                    ",\"native_completions\":"+std::to_string(resources.file_watch->completions)+
                    ",\"file_names_or_other_file_contents_collected\":false");
                if(!completed)return false;
                if(context()&&predicate())return true;
                if(!context()||!resources.file_watch->arm())return false;
            }
            if(waited==WAIT_OBJECT_0+count){MSG message{};unsigned budget=256;
                while(budget--&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
                    if(message.message==WM_QUIT){quit_seen=true;return false;}TranslateMessage(&message);DispatchMessageW(&message);}}
            if(waited==WAIT_TIMEOUT)return context()&&predicate();
        }
    };
    const auto send=[&](INPUT* packets,UINT count,const std::function<bool()>& target,const char* purpose,bool pairs){
        if(!count||count>1024||!context()||!target()||!lease->begin())throw std::runtime_error("client_input_context_target_or_lease_rejected");
        struct End {MediumDesktopUiInputLease& lease;~End(){lease.end();}} end{*lease};
        if(!context()||!target()||!context())throw std::runtime_error("client_input_retired_or_context_changed_after_claim");
        if(pairs)lease->incomplete_pair.store(true,std::memory_order_release);
        for(UINT index=0;index<count;++index){if(packets[index].type==INPUT_MOUSE)packets[index].mi.dwExtraInfo=0x50425631;
            else packets[index].ki.dwExtraInfo=0x50425631;}
        LARGE_INTEGER before{},after{};const bool before_known=QueryPerformanceCounter(&before)!=FALSE;
        out.input_attempted=true;SetLastError(ERROR_SUCCESS);const auto inserted=SendInput(count,packets,sizeof(INPUT));
        const DWORD error=inserted==count?0:GetLastError();const bool after_known=QueryPerformanceCounter(&after)!=FALSE;
        out.input_inserted=out.input_inserted||inserted>0;
        // Packet arrays contain complete adjacent DOWN/UP pairs only. An odd
        // short insert leaves an UNKNOWN held pair; never replay DOWN or
        // manufacture a product UP. The caller retires/disposes this guest.
        if(pairs&&inserted<=count&&(inserted%2)==0)lease->incomplete_pair.store(false,std::memory_order_release);
        if(inserted!=count)lease->flags.fetch_or(MediumDesktopUiInputLease::retired_bit,std::memory_order_release);
        out.emit("medium_terminal_client_input","\"purpose\":"+quote(purpose)+",\"packet_count\":"+std::to_string(count)+
            ",\"SendInput_return\":"+std::to_string(inserted)+",\"short_insert_error\":"+std::to_string(error)+
            ",\"before_qpc\":"+(before_known?std::to_string(before.QuadPart):"null")+
            ",\"after_qpc\":"+(after_known?std::to_string(after.QuadPart):"null")+
            ",\"lease_retired_after_call\":"+(lease->retired()?"true":"false")+
            ",\"incomplete_pair_unknown\":"+(lease->incomplete_pair.load(std::memory_order_acquire)?"true":"false")+
            ",\"in_flight_call_cancelled\":false,\"API_insert_is_not_window_delivery_or_product_NormalUp\":true");
        if(inserted!=count)throw std::runtime_error("client_short_insert_no_replay_or_unverified_UP_cleanup");
    };
    try{
        if(run.size()!=32||run.find_first_not_of(L"0123456789abcdef")!=std::wstring::npos||
           medium_terminal_client_events||!ProcessIdToSessionId(GetCurrentProcessId(),&session)||!session||
           !product::guest::same_exact_directory(nonce_output.parent_path(),expected_output.parent_path())||
           nonce_output.filename()!=expected_output.filename()||!plain_directory(nonce_output.parent_path())||
           !marker(nonce_output.parent_path()/L"panebind-v1-output.marker",run)||
           !marker(L"C:\\PaneBindV1\\Input\\panebind-v1-input.marker",run)||!context()||!no_buttons())
            throw std::runtime_error("exact_active_Run_client_guest_scope_required");
        std::error_code file_error;const bool existed=fs::exists(nonce_output,file_error);
        out.emit("medium_terminal_client_output_precondition","\"file\":"+quote(utf8(nonce_output.wstring()))+
            ",\"exists_before_input\":"+(existed?"true":"false")+",\"existence_query_error\":"+std::to_string(file_error.value())+
            ",\"same_exact_Run_output_path\":true,\"product_started\":false");
        if(existed||file_error)throw std::runtime_error("new_Run_nonce_output_required_existing_file_not_overwritten");

        // Both real GUI observations coexist with foreign-thread desktop
        // UNKNOWN. Owner GTI failure is not converted into "idle/no capture".
        const auto gui_observation=[&](DWORD requested,const char* mode){
            GUITHREADINFO gui{sizeof(gui)};SetLastError(ERROR_SUCCESS);
            const bool returned=GetGUIThreadInfo(requested,&gui)!=FALSE;const DWORD error=returned?0:GetLastError();
            const HWND foreground=GetForegroundWindow();DWORD foreground_pid{};
            const auto foreground_tid=GetWindowThreadProcessId(foreground,&foreground_pid);
            out.emit("medium_terminal_client_GUI_observation","\"mode\":"+quote(mode)+",\"requested_TID\":"+std::to_string(requested)+
                ",\"GetGUIThreadInfo_return\":"+(returned?"true":"false")+",\"error\":"+std::to_string(error)+
                ",\"flags\":"+(returned?std::to_string(gui.flags):"null")+
                ",\"active\":"+(returned?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndActive)):"null")+
                ",\"focus\":"+(returned?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndFocus)):"null")+
                ",\"capture\":"+(returned?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndCapture)):"null")+
                ",\"move_size\":"+(returned?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndMoveSize)):"null")+
                ",\"menu_owner\":"+(returned?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndMenuOwner)):"null")+
                ",\"in_move_size\":"+(returned?((gui.flags&GUI_INMOVESIZE)?"true":"false"):"null")+
                ",\"in_menu_mode\":"+(returned?((gui.flags&GUI_INMENUMODE)?"true":"false"):"null")+
                ",\"actual_foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(foreground))+
                ",\"foreground_PID\":"+std::to_string(foreground_pid)+",\"foreground_TID\":"+std::to_string(foreground_tid));
        };
        gui_observation(tid,"source_owner_thread");gui_observation(0,"actual_foreground_thread");
        SetLastError(ERROR_SUCCESS);const HDESK foreign=GetThreadDesktop(tid);const DWORD foreign_error=foreign?0:GetLastError();
        std::array<wchar_t,128> foreign_name{};DWORD foreign_bytes{};bool name_return{};DWORD name_error{};
        if(foreign){SetLastError(ERROR_SUCCESS);name_return=GetUserObjectInformationW(foreign,UOI_NAME,foreign_name.data(),
            static_cast<DWORD>(sizeof(foreign_name)),&foreign_bytes)!=FALSE;name_error=name_return?0:GetLastError();}
        out.emit("medium_terminal_client_foreign_desktop","\"GetThreadDesktop_return\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(foreign))+",\"GetThreadDesktop_error\":"+std::to_string(foreign_error)+
            ",\"name_query_attempted\":"+(foreign?"true":"false")+",\"name_query_return\":"+(foreign?(name_return?"true":"false"):"null")+
            ",\"name_query_error\":"+(foreign?std::to_string(name_error):"null")+
            ",\"actual_name\":"+(name_return?quote(utf8(foreign_name.data())):"null")+
            ",\"foreign_query_UNKNOWN_does_not_erase_exact_held_desktop_membership\":true,\"borrowed_handle_closed\":false");

        // Read-only Task View information is separate from Win32 HDESK/input
        // membership, and never grants product or test input authority.
        const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        resources.com_owned=SUCCEEDED(initialized);
        HRESULT created_manager=E_UNEXPECTED,queried=E_UNEXPECTED;BOOL current{};bool desktop_query_attempted{};
        if(resources.com_owned)created_manager=CoCreateInstance(CLSID_VirtualDesktopManager,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&desktops));
        if(SUCCEEDED(created_manager)&&desktops&&context()){desktop_query_attempted=true;queried=desktops->IsWindowOnCurrentVirtualDesktop(source,&current);}
        out.emit("medium_terminal_client_virtual_desktop_readonly","\"CoInitializeEx_STA_hresult\":"+std::to_string(static_cast<std::uint32_t>(initialized))+
            ",\"CoCreateInstance_attempted\":"+(resources.com_owned?"true":"false")+
            ",\"CoCreateInstance_hresult\":"+(resources.com_owned?std::to_string(static_cast<std::uint32_t>(created_manager)):"null")+
            ",\"IsWindowOnCurrentVirtualDesktop_attempted\":"+(desktop_query_attempted?"true":"false")+
            ",\"query_hresult\":"+(desktop_query_attempted?std::to_string(static_cast<std::uint32_t>(queried)):"null")+
            ",\"actual_on_current_virtual_desktop\":"+(queried==S_OK?(current?"true":"false"):"null")+
            ",\"readonly_not_input_or_product_authority\":true");
        events.changed=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!events.changed)throw std::runtime_error("client_own_event_create_failed");
        medium_terminal_client_events=&events;SetLastError(ERROR_SUCCESS);
        hook=SetWinEventHook(EVENT_OBJECT_FOCUS,EVENT_OBJECT_FOCUS,nullptr,medium_terminal_client_event,pid,0,WINEVENT_OUTOFCONTEXT);
        const DWORD hook_error=hook?0:GetLastError();
        out.emit("medium_terminal_client_observer","\"SetWinEventHook_return\":"+std::string(hook?"true":"false")+
            ",\"error\":"+std::to_string(hook_error)+",\"observation_only\":true,\"callback_disk_IO\":false");
        if(!hook)throw std::runtime_error("client_focus_observer_unavailable");
        // Arm one actual OVERLAPPED directory notification before typing.
        // A failed mapped-folder setup may choose one already-owned local
        // Terminal Run folder before the sole command; never resend a command.
        resources.file_watch=std::make_unique<MediumTerminalClientFileWatch>();
        const bool mapped_watch=resources.file_watch->open(actual_nonce.parent_path());
        out.emit("medium_terminal_client_file_watch","\"path\":"+quote(utf8(actual_nonce.parent_path().wstring()))+
            ",\"opened_and_armed\":"+(mapped_watch?"true":"false")+",\"open_error\":"+std::to_string(resources.file_watch->open_error)+
            ",\"ReadDirectoryChangesW_return\":"+(resources.file_watch->arm_return?"true":"false")+
            ",\"read_error\":"+std::to_string(resources.file_watch->arm_error)+",\"overlapped_event\":true,\"max_completed_notifications\":4");
        if(!mapped_watch){
            if(!resources.file_watch->stop())throw std::runtime_error("mapped_file_watch_cleanup_UNKNOWN_no_command");
            const auto actual_image=fs::path(medium_desktop_image(held_process));const auto local=actual_image.parent_path();
            const auto release=fs::path(L"C:\\PaneBindV1\\Terminal")/run/L"Release"/L"terminal-1.25.2733.0";
            const auto debug=fs::path(L"C:\\PaneBindV1\\Terminal")/run/L"Debug"/L"terminal-1.25.2733.0";
            if((!product::guest::same_exact_directory(local,release)&&!product::guest::same_exact_directory(local,debug))||
               !plain_directory(local)||!equal(actual_image.filename().wstring(),L"WindowsTerminal.exe")||
               !marker(local/L"panebind-v1-test-application.marker",run))
                throw std::runtime_error("owned_Run_local_nonce_directory_unconfirmed_no_command");
            actual_nonce=local/expected_output.filename();file_error.clear();
            if(fs::exists(actual_nonce,file_error)||file_error)throw std::runtime_error("new_local_nonce_file_required_no_overwrite");
            resources.file_watch=std::make_unique<MediumTerminalClientFileWatch>();
            const bool local_watch=resources.file_watch->open(local);
            out.emit("medium_terminal_client_file_watch_local_fallback","\"actual_nonce_path\":"+quote(utf8(actual_nonce.wstring()))+
                ",\"opened_and_armed\":"+(local_watch?"true":"false")+",\"open_error\":"+std::to_string(resources.file_watch->open_error)+
                ",\"ReadDirectoryChangesW_return\":"+(resources.file_watch->arm_return?"true":"false")+
                ",\"read_error\":"+std::to_string(resources.file_watch->arm_error)+
                ",\"actual_owned_local_marker_checked\":true,\"earlier_command_sent\":false");
            if(!local_watch)throw std::runtime_error("actual_local_nonce_notification_setup_failed_no_command");
        }
        RECT client{},source_position{};POINT point{};SetLastError(ERROR_SUCCESS);
        const bool source_position_read=GetWindowRect(source,&source_position)!=FALSE;
        const DWORD source_position_error=source_position_read?0:GetLastError();SetLastError(ERROR_SUCCESS);
        const bool client_read=GetClientRect(source,&client)!=FALSE;const DWORD client_error=client_read?0:GetLastError();
        if(client_read){point.x=client.left+(client.right-client.left)/2;point.y=client.top+(client.bottom-client.top)/2;}
        const bool convert_attempted=client_read&&client.left<client.right&&client.top<client.bottom;SetLastError(ERROR_SUCCESS);
        const bool point_ready=convert_attempted&&ClientToScreen(source,&point)!=FALSE;
        const DWORD convert_error=point_ready?0:GetLastError();
        const auto query_dpi_context=GetThreadDpiAwarenessContext(),source_dpi_context=GetWindowDpiAwarenessContext(source);
        const auto source_dpi=GetDpiForWindow(source);
        const bool query_pmv2=query_dpi_context&&AreDpiAwarenessContextsEqual(query_dpi_context,DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        POINT roundtrip=point;SetLastError(ERROR_SUCCESS);
        const bool roundtrip_attempted=point_ready,roundtrip_return=roundtrip_attempted&&ScreenToClient(source,&roundtrip)!=FALSE;
        const DWORD roundtrip_error=roundtrip_attempted&&!roundtrip_return?GetLastError():0;
        const bool roundtrip_exact=roundtrip_return&&roundtrip.x==client.left+(client.right-client.left)/2&&
            roundtrip.y==client.top+(client.bottom-client.top)/2;
        const HWND actual_hit=point_ready?WindowFromPoint(point):nullptr;DWORD hit_pid{};const auto hit_tid=GetWindowThreadProcessId(actual_hit,&hit_pid);
        DWORD_PTR hit_result{};bool nc_attempted{},nc_return{};DWORD nc_error{};
        if(point_ready&&point.x>=-32768&&point.x<=32767&&point.y>=-32768&&point.y<=32767&&client_hit(point)){
            nc_attempted=true;SetLastError(ERROR_SUCCESS);nc_return=SendMessageTimeoutW(source,WM_NCHITTEST,0,
                MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&hit_result)!=FALSE;
            nc_error=nc_return?0:GetLastError();
        }
        out.emit("medium_terminal_client_hit","\"GetClientRect_return\":"+std::string(client_read?"true":"false")+
            ",\"GetClientRect_error\":"+std::to_string(client_error)+",\"client_rect\":"+
            (client_read?"["+std::to_string(client.left)+","+std::to_string(client.top)+","+std::to_string(client.right)+","+std::to_string(client.bottom)+"]":"null")+
            ",\"source_GetWindowRect_return\":"+(source_position_read?"true":"false")+
            ",\"source_GetWindowRect_error\":"+std::to_string(source_position_error)+",\"actual_source_positioning_rect\":"+
            (source_position_read?rect({source_position.left,source_position.top,source_position.right,source_position.bottom}):"null")+
            ",\"ClientToScreen_attempted\":"+(convert_attempted?"true":"false")+
            ",\"ClientToScreen_return\":"+(convert_attempted?(point_ready?"true":"false"):"null")+
            ",\"ClientToScreen_error\":"+(convert_attempted?std::to_string(convert_error):"null")+
            ",\"screen_point\":"+(point_ready?"["+std::to_string(point.x)+","+std::to_string(point.y)+"]":"null")+",\"WindowFromPoint\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(actual_hit))+",\"hit_PID\":"+std::to_string(hit_pid)+",\"hit_TID\":"+std::to_string(hit_tid)+
            ",\"WM_NCHITTEST_attempted\":"+(nc_attempted?"true":"false")+",\"native_return\":"+(nc_attempted?(nc_return?"true":"false"):"null")+
            ",\"native_result\":"+(nc_return?std::to_string(hit_result):"null")+",\"error\":"+(nc_attempted?std::to_string(nc_error):"null")+
            ",\"query_thread_DPI_context\":"+(query_dpi_context?std::to_string(reinterpret_cast<std::intptr_t>(query_dpi_context)):"null")+
            ",\"query_thread_PMV2\":"+(query_pmv2?"true":"false")+",\"source_window_DPI_context\":"+
            (source_dpi_context?std::to_string(reinterpret_cast<std::intptr_t>(source_dpi_context)):"null")+
            ",\"source_window_DPI\":"+(source_dpi?std::to_string(source_dpi):"null")+
            ",\"ScreenToClient_attempted\":"+(roundtrip_attempted?"true":"false")+
            ",\"ScreenToClient_return\":"+(roundtrip_attempted?(roundtrip_return?"true":"false"):"null")+
            ",\"ScreenToClient_error\":"+(roundtrip_attempted?std::to_string(roundtrip_error):"null")+
            ",\"actual_client_roundtrip\":"+(roundtrip_return?"["+std::to_string(roundtrip.x)+","+std::to_string(roundtrip.y)+"]":"null")+
            ",\"client_roundtrip_exact\":"+(roundtrip_exact?"true":"false")+
            ",\"external_root_hit_is_observation_not_keyboard_authority\":true,\"mouse_client_click\":\"NOT_RUN\",\"mouse_input_sent\":false");
        if(!source_position_read||!point_ready||!roundtrip_exact||!query_pmv2||!source_dpi_context||!source_dpi)
            throw std::runtime_error("actual_source_client_geometry_or_DPI_roundtrip_unconfirmed_no_keyboard_input");

        // Terminal's root procedure can return HTCAPTION for its real XAML
        // client space (upstream8795). This independent keyboard subcase does
        // not relabel that result, simulate a mouse click, or claim that the
        // content works. It uses only the already-existing exact foreground
        // focus; the sole content oracle remains the actual new nonce file.
        GUITHREADINFO initial_gui{sizeof(initial_gui)};bool initial_gui_known{};DWORD initial_gui_error{};
        const bool initial_focus=foreground_focus(&initial_gui,&initial_gui_known,&initial_gui_error);
        const HWND focused=initial_gui_known?initial_gui.hwndFocus:nullptr;DWORD focus_pid{};
        const DWORD focus_tid=focused?GetWindowThreadProcessId(focused,&focus_pid):0;
        std::array<wchar_t,256> focus_class{};RECT focus_position{};SetLastError(ERROR_SUCCESS);
        const int focus_class_length=focused?GetClassNameW(focused,focus_class.data(),static_cast<int>(focus_class.size())):0;
        const DWORD focus_class_error=focus_class_length?0:GetLastError();SetLastError(ERROR_SUCCESS);
        const bool focus_position_read=focused&&GetWindowRect(focused,&focus_position)!=FALSE;
        const DWORD focus_position_error=focus_position_read?0:GetLastError();
        const bool focus_identity=initial_focus&&focused&&focus_tid&&focus_pid==pid&&IsWindow(focused)&&
            GetAncestor(focused,GA_ROOT)==source&&focus_class_length>0&&focus_class_length<static_cast<int>(focus_class.size())-1&&
            focus_position_read&&focus_position.left<focus_position.right&&focus_position.top<focus_position.bottom;
        out.emit("medium_terminal_client_existing_keyboard_focus","\"foreground_GTI_known\":"+std::string(initial_gui_known?"true":"false")+
            ",\"foreground_GTI_error\":"+std::to_string(initial_gui_error)+",\"actual_foreground\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(GetForegroundWindow()))+",\"actual_focus\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(focused))+",\"focus_PID\":"+std::to_string(focus_pid)+
            ",\"focus_TID\":"+std::to_string(focus_tid)+",\"focus_class\":"+(focus_class_length?quote(utf8(focus_class.data())):"null")+
            ",\"focus_class_query_attempted\":"+(focused?"true":"false")+
            ",\"focus_class_error\":"+(focused?std::to_string(focus_class_error):"null")+
            ",\"focus_GetWindowRect_attempted\":"+(focused?"true":"false")+
            ",\"focus_GetWindowRect_return\":"+(focused?(focus_position_read?"true":"false"):"null")+
            ",\"focus_GetWindowRect_error\":"+(focused?std::to_string(focus_position_error):"null")+",\"actual_focus_rect\":"+
            (focus_position_read?rect({focus_position.left,focus_position.top,focus_position.right,focus_position.bottom}):"null")+
            ",\"exact_existing_focus_identity\":"+(focus_identity?"true":"false")+
            ",\"SetForegroundWindow_or_SetFocus_called\":false,\"mouse_client_click\":\"NOT_RUN\",\"content_ready_inferred\":false");
        if(!focus_identity||!no_buttons())throw std::runtime_error("actual_existing_exact_foreground_focus_unconfirmed_no_keyboard_input");
        const auto fresh_keyboard_focus=[&]{
            if(!context()||!no_buttons())return false;
            GUITHREADINFO gui{sizeof(gui)};bool known{};DWORD error{};
            const bool focused_now=foreground_focus(&gui,&known,&error);
            DWORD actual_pid{};const DWORD actual_tid=GetWindowThreadProcessId(focused,&actual_pid);
            std::array<wchar_t,256> actual_class{};RECT actual_position{},actual_client{},actual_focus{},after_position{},after_client{},after_focus{};
            const int count=GetClassNameW(focused,actual_class.data(),static_cast<int>(actual_class.size()));
            const bool identity=focused_now&&known&&gui.hwndFocus==focused&&actual_tid==focus_tid&&actual_pid==pid&&
                IsWindow(focused)&&GetAncestor(focused,GA_ROOT)==source&&count==focus_class_length&&
                std::wstring_view(actual_class.data(),static_cast<std::size_t>(count))==
                    std::wstring_view(focus_class.data(),static_cast<std::size_t>(focus_class_length));
            const bool geometry=GetWindowRect(source,&actual_position)&&GetClientRect(source,&actual_client)&&GetWindowRect(focused,&actual_focus)&&
                EqualRect(&actual_position,&source_position)&&EqualRect(&actual_client,&client)&&EqualRect(&actual_focus,&focus_position);
            POINT mapped{client.left+(client.right-client.left)/2,client.top+(client.bottom-client.top)/2};
            const bool screen=geometry&&ClientToScreen(source,&mapped)&&mapped.x==point.x&&mapped.y==point.y;
            const bool mapped_back=screen&&ScreenToClient(source,&mapped)&&mapped.x==roundtrip.x&&mapped.y==roundtrip.y;
            const bool dpi=AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),query_dpi_context)&&
                AreDpiAwarenessContextsEqual(GetWindowDpiAwarenessContext(source),source_dpi_context)&&GetDpiForWindow(source)==source_dpi;
            const bool stable=GetWindowRect(source,&after_position)&&GetClientRect(source,&after_client)&&GetWindowRect(focused,&after_focus)&&
                EqualRect(&after_position,&source_position)&&EqualRect(&after_client,&client)&&EqualRect(&after_focus,&focus_position);
            const bool good=identity&&geometry&&mapped_back&&dpi&&stable&&context()&&no_buttons();
            out.emit("medium_terminal_client_fresh_keyboard_guard","\"actual_existing_focus_identity_matches\":"+std::string(identity?"true":"false")+
                ",\"actual_source_client_and_focus_rects_match\":"+(geometry&&stable?"true":"false")+
                ",\"actual_client_screen_roundtrip_matches\":"+(mapped_back?"true":"false")+",\"actual_DPI_context_matches\":"+(dpi?"true":"false")+
                ",\"foreground_GTI_error\":"+std::to_string(error)+",\"accepted_keyboard_target\":"+(good?"true":"false")+
                ",\"root_WM_NCHITTEST_used_as_keyboard_authority\":false,\"mouse_client_click\":\"NOT_RUN\"");
            return good;
        };
        out.focus_observed=fresh_keyboard_focus();
        if(!out.focus_observed)throw std::runtime_error("existing_keyboard_focus_or_fresh_geometry_changed_no_input");
        const std::wstring command=L"if not exist \""+actual_nonce.wstring()+L"\" (echo "+nonce+L">\""+actual_nonce.wstring()+L"\")";
        if(command.size()>400)throw std::runtime_error("fixed_nonce_command_unexpected_length");
        std::vector<INPUT> text;text.reserve(command.size()*2);
        for(const wchar_t value:command){INPUT down{};down.type=INPUT_KEYBOARD;down.ki.wScan=value;down.ki.dwFlags=KEYEVENTF_UNICODE;
            text.push_back(down);down.ki.dwFlags|=KEYEVENTF_KEYUP;text.push_back(down);}
        send(text.data(),static_cast<UINT>(text.size()),fresh_keyboard_focus,"fixed_Run_nonce_cmd_Unicode_pairs",true);
        out.command_inserted=true;
        file_error.clear();if(fs::exists(actual_nonce,file_error)||file_error)throw std::runtime_error("nonce_file_appeared_before_Enter_no_overwrite");
        INPUT enter[2]{};for(auto& packet:enter){packet.type=INPUT_KEYBOARD;packet.ki.wVk=VK_RETURN;}
        enter[1].ki.dwFlags=KEYEVENTF_KEYUP;
        send(enter,2,fresh_keyboard_focus,"fixed_nonce_cmd_Enter_DOWN_UP",true);out.enter_inserted=true;
        const auto exact_nonce=[&]{
            const auto attributes=GetFileAttributesW(actual_nonce.c_str());
            if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))return false;
            std::error_code error;const auto size=fs::file_size(actual_nonce,error);if(error||size>256)return false;
            auto bytes=read(actual_nonce,256);if(bytes.ends_with("\r\n"))bytes.resize(bytes.size()-2);else if(bytes.ends_with('\n'))bytes.pop_back();
            return bytes==utf8(nonce);
        };
        // No file polling: real directory completion/message/process events,
        // at most four completions and a bounded final exact read.
        out.exact_nonce_observed=wait_for(exact_nonce,3000);
        out.emit("medium_terminal_client_nonce","\"file\":"+quote(utf8(actual_nonce.wstring()))+
            ",\"expected_nonce\":"+quote(utf8(nonce))+",\"actual_exact_nonce_observed\":"+(out.exact_nonce_observed?"true":"false")+
            ",\"actual_normalized_nonce_value\":"+(out.exact_nonce_observed?quote(utf8(nonce)):"null")+
            ",\"actual_file_sha256\":"+(out.exact_nonce_observed?quote(hash(actual_nonce)):"null")+
            ",\"command_inserted\":"+(out.command_inserted?"true":"false")+",\"Enter_inserted\":"+(out.enter_inserted?"true":"false")+
            ",\"API_success_alone_is_not_client_semantic_PASS\":true,\"normal_close_this_function\":false");
        out.completed=out.exact_nonce_observed&&context()&&!lease->incomplete_pair.load(std::memory_order_acquire);
        out.reason=out.completed?"actual_Terminal_client_nonce_receipt_only":"actual_nonce_or_final_context_not_confirmed";
    }catch(const std::exception& error){
        out.reason=error.what();if(lease)lease->flags.fetch_or(MediumDesktopUiInputLease::retired_bit,std::memory_order_release);
    }
    resources.finish();out.cleanup_known=!resources.unknown;
    out.completed=out.completed&&out.cleanup_known;
    out.emit("medium_terminal_client_summary","\"result\":"+quote(out.completed?"PASS_EXACT_CLIENT_NONCE_ONLY":"UNKNOWN_OR_BLOCKED")+
        ",\"reason\":"+quote(out.reason)+",\"own_UnhookWinEvent_attempted\":"+(resources.unhook_attempted?"true":"false")+
        ",\"own_UnhookWinEvent_return\":"+(resources.unhook_attempted?(resources.unhook_ok?"true":"false"):"null")+
        ",\"own_UnhookWinEvent_error\":"+(resources.unhook_attempted?std::to_string(resources.unhook_error):"null")+
        ",\"local_observer_cleanup_known\":"+(out.cleanup_known?"true":"false")+
        ",\"directory_CancelIoEx_attempted\":"+(resources.watch_cancel_attempted?"true":"false")+
        ",\"directory_CancelIoEx_return\":"+(resources.watch_cancel_attempted?(resources.watch_cancel_return?"true":"false"):"null")+
        ",\"directory_CancelIoEx_error\":"+(resources.watch_cancel_attempted?std::to_string(resources.watch_cancel_error):"null")+
        ",\"directory_cancel_completion_wait\":"+(resources.watch_cancel_attempted?std::to_string(resources.watch_cancel_wait):"null")+
        ",\"directory_native_pending_unknown\":"+(resources.watch_pending_unknown?"true":"false")+
        ",\"shared_input_pair_unknown\":"+(lease&&lease->incomplete_pair.load(std::memory_order_acquire)?"true":"false")+
        ",\"input_subcase\":\"existing_foreground_keyboard_nonce_only\",\"mouse_pair_inserted\":false,\"mouse_client_click\":\"NOT_RUN\",\"mouse_input_sent\":false"+
        ",\"product_started\":false,\"product_enhancement_result\":\"NOT_RUN\",\"normal_close_owned_by_outer_native_case\":true,"+
        "\"guest_disposal_is_not_cleanup_PASS\":true,\"human_UAT\":\"NOT_RUN\"");
    return out;
}
