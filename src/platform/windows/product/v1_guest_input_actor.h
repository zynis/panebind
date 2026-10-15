#pragma once

// One-shot TEST input producer, not a product connection or a production
// feature. Include after guard/helpers and interruption cases, inside the
// fixture namespace. Its own guarded entry never runs the ordinary fixture.
std::uint64_t input_actor_unsigned(const wchar_t* text){
    const std::wstring value(text);if(value.empty()||value.size()>20||!std::all_of(value.begin(),value.end(),[](wchar_t c){return c>=L'0'&&c<=L'9';}))
        throw std::runtime_error("input_actor_invalid_unsigned");std::size_t end{};const auto result=std::stoull(value,&end);
    if(end!=value.size())throw std::runtime_error("input_actor_invalid_unsigned");return result;
}
LONG input_actor_coordinate(const wchar_t* text){
    const std::wstring value(text);if(value.empty()||value.size()>12)throw std::runtime_error("input_actor_invalid_coordinate");
    std::size_t end{};const auto result=std::stoll(value,&end);
    if(end!=value.size()||result<LONG_MIN||result>LONG_MAX)throw std::runtime_error("input_actor_coordinate_out_of_range");return static_cast<LONG>(result);
}
bool input_actor_same_user(HANDLE parent){
    HANDLE a{},b{};if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&a))return false;Handle own_token(a);
    if(!OpenProcessToken(parent,TOKEN_QUERY,&b))return false;Handle parent_token(b);
    alignas(TOKEN_USER) std::array<BYTE,512> own{},other{};DWORD size{};
    return GetTokenInformation(a,TokenUser,own.data(),static_cast<DWORD>(own.size()),&size)&&
        GetTokenInformation(b,TokenUser,other.data(),static_cast<DWORD>(other.size()),&size)&&
        EqualSid(reinterpret_cast<TOKEN_USER*>(own.data())->User.Sid,reinterpret_cast<TOKEN_USER*>(other.data())->User.Sid);
}
DWORD input_actor_parent_pid(){
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));if(snapshot.value==INVALID_HANDLE_VALUE)return 0;
    PROCESSENTRY32W entry{sizeof(entry)};if(!Process32FirstW(snapshot.value,&entry))return 0;
    do{if(entry.th32ProcessID==GetCurrentProcessId())return entry.th32ParentProcessID;}while(Process32NextW(snapshot.value,&entry));return 0;
}
bool input_actor_desktop(DWORD source_tid,DWORD parent_pid){
    if(!desktop())return false;DWORD own_session{},parent_session{};
    if(!ProcessIdToSessionId(GetCurrentProcessId(),&own_session)||!own_session||!ProcessIdToSessionId(parent_pid,&parent_session)||own_session!=parent_session)return false;
    std::array<wchar_t,128> station{},source_name{},own_name{};DWORD size{};
    const auto source_desktop=GetThreadDesktop(source_tid);const auto own_desktop=GetThreadDesktop(GetCurrentThreadId());
    if(!source_desktop||!GetUserObjectInformationW(GetProcessWindowStation(),UOI_NAME,station.data(),static_cast<DWORD>(sizeof(station)),&size)||!equal(station.data(),L"WinSta0")||
        !GetUserObjectInformationW(source_desktop,UOI_NAME,source_name.data(),static_cast<DWORD>(sizeof(source_name)),&size)||
        !GetUserObjectInformationW(own_desktop,UOI_NAME,own_name.data(),static_cast<DWORD>(sizeof(own_name)),&size)||
        !equal(source_name.data(),L"Default")||!equal(source_name.data(),own_name.data()))return false;
    LPWSTR raw{};DWORD bytes{};const bool queried=WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,own_session,WTSConnectState,&raw,&bytes)!=FALSE;
    const bool active=queried&&bytes>=sizeof(WTS_CONNECTSTATE_CLASS)&&*reinterpret_cast<WTS_CONNECTSTATE_CLASS*>(raw)==WTSActive;
    if(raw)WTSFreeMemory(raw);return active;
}
int input_actor_child(int count,LPWSTR* args){
    if(count!=11)return 78;const std::wstring run=args[2];
    if(run.size()!=32||!std::all_of(run.begin(),run.end(),[](wchar_t c){return(c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');}))return 78;
    std::array<wchar_t,128> username{};DWORD length=static_cast<DWORD>(username.size());
    if(!GetUserNameW(username.data(),&length)||!equal(username.data(),L"WDAGUtilityAccount"))return 78;
    const fs::path input=L"C:\\PaneBindV1\\Input",output=L"C:\\PaneBindV1\\Output";
    if(!plain_directory(input)||!plain_directory(output)||!marker(input/L"panebind-v1-input.marker",run)||!marker(output/L"panebind-v1-output.marker",run))return 78;
    if(!medium(GetCurrentProcess()))return 78;
    const auto parent_value=input_actor_unsigned(args[3]),born=input_actor_unsigned(args[4]),hwnd_value=input_actor_unsigned(args[5]),tid_value=input_actor_unsigned(args[6]);
    if(!parent_value||parent_value>MAXDWORD||!tid_value||tid_value>MAXDWORD||!hwnd_value||hwnd_value>UINTPTR_MAX||!born)return 78;
    const auto parent_pid=static_cast<DWORD>(parent_value),source_tid=static_cast<DWORD>(tid_value);const HWND source=reinterpret_cast<HWND>(static_cast<std::uintptr_t>(hwnd_value));
    if(input_actor_parent_pid()!=parent_pid)return 78;Handle parent(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,parent_pid));
    if(!parent.value||WaitForSingleObject(parent.value,0)!=WAIT_TIMEOUT||process_birth(parent.value)!=born||!medium(parent.value)||!input_actor_same_user(parent.value))return 78;
    DWORD actual_pid{};const std::wstring expected_class=L"PaneBind.V1.OwnedFixture."+run;
    if(!IsWindow(source)||GetAncestor(source,GA_ROOT)!=source||GetWindowThreadProcessId(source,&actual_pid)!=source_tid||actual_pid!=parent_pid||gui_class(source)!=expected_class||
        !input_actor_desktop(source_tid,parent_pid))return 78;
    std::array<wchar_t,32768> self_name{},parent_name{};DWORD parent_length=static_cast<DWORD>(parent_name.size());
    const auto self_length=GetModuleFileNameW(nullptr,self_name.data(),static_cast<DWORD>(self_name.size()));
    if(!self_length||self_length>=self_name.size()||!QueryFullProcessImageNameW(parent.value,0,parent_name.data(),&parent_length)||!equal(self_name.data(),parent_name.data()))return 78;
    std::error_code ec;const auto executable=fs::canonical(self_name.data(),ec);if(ec||!equal(executable.wstring(),self_name.data()))return 78;
    const bool debug=equal(executable.wstring(),(input/L"Debug"/L"panebind-v1-guest-fixture.exe").wstring());
    if(!debug&&!equal(executable.wstring(),(input/L"panebind-v1-guest-fixture.exe").wstring()))return 78;
    if(debug&&!plain_directory(input/L"Debug"))return 78;const auto image_hash=hash(executable);
    if(image_hash.empty()||image_hash!=hash(parent_name.data()))return 78;
    const POINT from{input_actor_coordinate(args[7]),input_actor_coordinate(args[8])},target{input_actor_coordinate(args[9]),input_actor_coordinate(args[10])};
    if(static_cast<long long>(target.x)-from.x!=-40||static_cast<long long>(target.y)-from.y!=35)return 78;
    const bool dpi_set=SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)!=FALSE;
    if(!dpi_set&&!AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))return 78;
    POINT actual{};DWORD_PTR hit{};if(!GetCursorPos(&actual)||actual.x!=from.x||actual.y!=from.y||GetForegroundWindow()!=source||WindowFromPoint(from)!=source||
        (GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_CONTROL)&0x8000)||(GetAsyncKeyState(VK_SHIFT)&0x8000)||
        (GetAsyncKeyState(VK_MENU)&0x8000)||(GetAsyncKeyState(VK_LWIN)&0x8000)||(GetAsyncKeyState(VK_RWIN)&0x8000)||
        !SendMessageTimeoutW(source,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(from.x),static_cast<short>(from.y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&hit)||hit!=HTCAPTION)return 78;
    const int x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN),w=GetSystemMetrics(SM_CXVIRTUALSCREEN),h=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if(w<2||h<2||target.x<x||target.y<y||static_cast<long long>(target.x)>=static_cast<long long>(x)+w||static_cast<long long>(target.y)>=static_cast<long long>(y)+h)return 78;
    const fs::path receipt=output/(run+(debug?L"-debug-input-actor.json":L"-release-input-actor.json"));
    Handle file(CreateFileW(receipt.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));if(file.value==INVALID_HANDLE_VALUE)return 78;
    std::array<INPUT,2> packets{};for(auto& p:packets){p.type=INPUT_MOUSE;p.mi.dwExtraInfo=0x50425631;}
    packets[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;packets[1].mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
    packets[1].mi.dx=static_cast<LONG>(((static_cast<long long>(target.x-x)*2+1)*65536)/(static_cast<long long>(w)*2));
    packets[1].mi.dy=static_cast<LONG>(((static_cast<long long>(target.y-y)*2+1)*65536)/(static_cast<long long>(h)*2));
    // The mapped-output CREATE_NEW may have waited on IO. It is not a lease
    // on the earlier foreground/hit facts; recheck immediately before input.
    if(WaitForSingleObject(parent.value,0)!=WAIT_TIMEOUT||process_birth(parent.value)!=born||!desktop()||
        GetWindowThreadProcessId(source,&actual_pid)!=source_tid||actual_pid!=parent_pid||GetAncestor(source,GA_ROOT)!=source||gui_class(source)!=expected_class||
        !GetCursorPos(&actual)||actual.x!=from.x||actual.y!=from.y||WindowFromPoint(from)!=source||GetForegroundWindow()!=source||
        (GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_CONTROL)&0x8000)||(GetAsyncKeyState(VK_SHIFT)&0x8000)||
        (GetAsyncKeyState(VK_MENU)&0x8000)||(GetAsyncKeyState(VK_LWIN)&0x8000)||(GetAsyncKeyState(VK_RWIN)&0x8000))return 78;
    LARGE_INTEGER before{},after{};QueryPerformanceCounter(&before);const auto before_tick=GetTickCount64();
    const UINT inserted=SendInput(static_cast<UINT>(packets.size()),packets.data(),sizeof(INPUT));const DWORD error=inserted==packets.size()?0:GetLastError();
    const auto after_tick=GetTickCount64();QueryPerformanceCounter(&after);
    const std::string text="{\"schema\":\"panebind-v1-test-input-actor/1\",\"source_sha\":\"" PANEBIND_BUILD_SHA "\",\"run_id\":"+quote(utf8(run))+
        ",\"image_sha256\":"+quote(image_hash)+",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"parent_pid\":"+std::to_string(parent_pid)+
        ",\"parent_birth\":"+std::to_string(born)+",\"source_hwnd\":"+std::to_string(hwnd_value)+",\"source_tid\":"+std::to_string(source_tid)+
        ",\"same_SID_session_Medium_guest\":true,\"SendInput_return\":"+std::to_string(inserted)+",\"SendInput_error\":"+std::to_string(error)+
        ",\"before_tick\":"+std::to_string(before_tick)+",\"after_tick\":"+std::to_string(after_tick)+",\"before_qpc\":"+std::to_string(before.QuadPart)+
        ",\"after_qpc\":"+std::to_string(after.QuadPart)+",\"UP_sent\":false,\"product_driver_channel\":false,\"automated_guest_driver\":true}\n";
    DWORD written{};const bool saved=WriteFile(file.value,text.data(),static_cast<DWORD>(text.size()),&written,nullptr)&&written==text.size()&&FlushFileBuffers(file.value);
    return saved&&inserted==packets.size()?0:1;
}
void owned_input_actor_exit(State& s,LayoutFixtures& set,DWORD product_pid,std::uint64_t product_birth){
    namespace v=panebind::core::v1;const HWND source=set.frames[0].hwnd;const auto initial=visible(source);
    if(!set.owns(0)||!initial||s.left||s.control)throw std::runtime_error("I08_actor_initial_exact_source_unavailable");
    chain_check_product(s,product_pid,product_birth);const auto config=chain_settings(s);
    if(config.magnet_move||config.docking||config.shake||config.drag_transparency)throw std::runtime_error("I08_actor_requires_existing_interruption_configuration");
    const auto receipt=s.output/(s.run+(s.configuration=="Debug"?L"-debug-input-actor.json":L"-release-input-actor.json"));
    if(fs::exists(receipt))throw std::runtime_error("I08_actor_receipt_already_exists");
    s.log.check("I08_actor_exact_foreground",SetForegroundWindow(source)&&wait([&]{return GetForegroundWindow()==source;}));
    const auto point=layout_hit(set,0,HTCAPTION);if(!point)throw std::runtime_error("I08_actor_real_caption_hit_unavailable");
    cursor(s,*point);const auto hover=GetTickCount64();(void)layout_wait_sent([&]{return GetTickCount64()-hover>=100;},100);
    std::array<wchar_t,32768> self{};if(!GetModuleFileNameW(nullptr,self.data(),static_cast<DWORD>(self.size())))throw std::runtime_error("I08_actor_fixture_image_unavailable");
    std::wstring command=L"\""+std::wstring(self.data())+L"\" --input-actor-once "+s.run+L" "+std::to_wstring(GetCurrentProcessId())+L" "+
        std::to_wstring(process_birth(GetCurrentProcess()))+L" "+std::to_wstring(reinterpret_cast<std::uintptr_t>(source))+L" "+std::to_wstring(set.thread)+L" "+
        std::to_wstring(point->x)+L" "+std::to_wstring(point->y)+L" "+std::to_wstring(point->x-40)+L" "+std::to_wstring(point->y+35);
    const auto offset=read(s.product_log).size();STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION child{};
    if(!CreateProcessW(self.data(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,s.output.c_str(),&startup,&child))throw std::runtime_error("I08_actor_create_failed");
    Handle child_process(child.hProcess),child_thread(child.hThread);bool test_up=false,actor_stop_attempted=false,actor_send_count_known=false;
    std::uint64_t actor_inserted{};
    const auto child_birth=process_birth(child_process.value);
    const auto stop_test_actor=[&]{
        if(WaitForSingleObject(child_process.value,0)==WAIT_OBJECT_0)return true;
        if(actor_stop_attempted)return false;actor_stop_attempted=true;
        const bool exact=child_birth&&process_birth(child_process.value)==child_birth&&child.dwProcessId!=product_pid&&child.dwProcessId!=GetCurrentProcessId();
        const bool requested=exact&&TerminateProcess(child_process.value,ERROR_TIMEOUT)!=FALSE;
        const bool gone=wait([&]{return WaitForSingleObject(child_process.value,0)==WAIT_OBJECT_0;},1000);
        s.log.emit("I08_forced_test_actor_cleanup","\"actor_pid\":"+std::to_string(child.dwProcessId)+",\"actor_birth\":"+std::to_string(child_birth)+
            ",\"retained_exact_child_handle\":"+(exact?"true":"false")+",\"TerminateProcess_requested\":"+(requested?"true":"false")+
            ",\"actor_exit_observed\":"+(gone?"true":"false")+",\"product_or_source_terminated\":false,\"functional_PASS\":false");return gone;
    };
    try{
        const bool exited=wait([&]{return WaitForSingleObject(child_process.value,0)==WAIT_OBJECT_0;},5000);DWORD code=STILL_ACTIVE;
        if(!exited)(void)stop_test_actor();
        if(exited)GetExitCodeProcess(child_process.value,&code);const auto child_exit_tick=GetTickCount64();
        const auto actor=read(receipt,8192);
        const bool actor_identity=actor.ends_with("}\n")&&actor.find("\"UP_sent\":false")!=std::string::npos&&
            actor.find("\"source_sha\":\"" PANEBIND_BUILD_SHA "\"")!=std::string::npos&&actor.find("\"SendInput_return\":")!=std::string::npos&&
            number(actor,"\"parent_pid\":")==GetCurrentProcessId()&&number(actor,"\"pid\":")==child.dwProcessId&&
            number(actor,"\"source_hwnd\":")==reinterpret_cast<std::uintptr_t>(source)&&number(actor,"\"before_qpc\":");
        actor_send_count_known=actor_identity;actor_inserted=actor_identity?number(actor,"\"SendInput_return\":"):0;
        const bool actor_down_inserted=actor_identity&&actor_inserted==2;
        // Import the CHILD's successful-send ledger, not async physical state.
        // START/native receipts below independently prove product observation.
        if(actor_identity&&actor_inserted>=1){s.left=true;s.left_down_send_qpc=number(actor,"\"before_qpc\":");}
        s.log.check("I08_input_actor_exited_without_UP_source_remains",exited&&code==0&&actor_down_inserted&&s.left&&set.owns(0)&&GetForegroundWindow()==source,
            "\"child_pid\":"+std::to_string(child.dwProcessId)+",\"exit_code\":"+std::to_string(code)+",\"child_exit_tick\":"+std::to_string(child_exit_tick)+
            ",\"source_still_alive\":"+(set.owns(0)?"true":"false")+",\"child_DOWN_insertion_confirmed\":"+
            (actor_down_inserted?"true":"null")+",\"child_SendInput_return\":"+(actor_send_count_known?std::to_string(actor_inserted):"null")+
            ",\"driver_left_outstanding\":"+(s.left?"true":"false")+",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+
            ",\"delivery_and_physical_button_held_not_inferred_from_ledger\":true,\"product_driver_channel\":false");
        Gesture g{*point,0,offset};const Rect moved=translated(*initial,-40,35);std::string actual_down,native;
        s.log.check("I08_actor_actual_START_and_exact_Move",wait([&]{g.generation=number(record(s,offset,"gesture_start"),"\"generation\":");
            actual_down=record(s,offset,"input_button_observed",g.generation);native=placement(s,offset,g.generation,moved,true);
            return g.generation&&set.owns(0)&&visible(source)==moved&&actual_up(s,offset,g.generation).empty()&&
                actual_down.find("\"source\":\"WH_MOUSE_LL\"")!=std::string::npos&&actual_down.find("\"kind\":\"DOWN\"")!=std::string::npos&&
                number(actual_down,"\"hook_observed_qpc\":")&&native.find("\"authority_after\":true")!=std::string::npos;}),
            "\"actual_DOWN\":"+quote(actual_down)+",\"actual_native_receipt\":"+quote(native));
        std::string ended;const bool terminal=wait([&]{ended=record(s,offset,"gesture_end",g.generation);
            return !ended.empty()||!set.owns(0)||GetForegroundWindow()!=source||!actual_up(s,offset,g.generation).empty();},35000);
        const auto before_tick=number(actor,"\"before_tick\":"),end_tick=number(ended,"\"tick\":");
        s.log.check("I08_input_actor_exit_real_default_30s_Deadline",terminal&&set.owns(0)&&GetForegroundWindow()==source&&s.left&&actual_up(s,offset,g.generation).empty()&&
            number(ended,"\"end\":")==static_cast<std::uint64_t>(v::End::Deadline)&&ended.find("\"normal_up\":false")!=std::string::npos&&
            ended.find("\"pending_retired\":true")!=std::string::npos&&before_tick&&end_tick>=before_tick&&end_tick-before_tick>=30000,
            "\"generation\":"+std::to_string(g.generation)+",\"configured_production_deadline_ms\":30000,\"actor_before_SendInput_tick\":"+
            std::to_string(before_tick)+",\"actual_end_tick\":"+std::to_string(end_tick)+",\"actual_elapsed_ms\":"+
            (end_tick>=before_tick?std::to_string(end_tick-before_tick):"null")+",\"actual_end\":"+quote(ended)+
            ",\"driver_left_outstanding\":"+(s.left?"true":"false")+",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+
            ",\"physical_button_held_not_inferred\":true,\"driver_notification_not_fabricated\":true");
        const auto calls=[&]{const auto rows=constraint_generation_placements(s,offset,g.generation);return std::count_if(rows.begin(),rows.end(),[](const auto& row){return row.find("\"native_callsite_claimed\":true")!=std::string::npos;});};
        const auto baseline=calls();const auto barrier=constraint_last_product_sequence(s);cursor(s,{point->x-11,point->y+47});
        s.log.check("I08_actor_Deadline_no_new_native_calls",launch(s,L"--status",true)&&wait([&]{return number(record(s,0,"status"),"\"sequence\":")>barrier;})&&
            calls()==baseline&&baseline>0&&visible(source)==moved);
        // This UP is a test cleanup action after actual Deadline, not product
        // NormalUp, a replay, or a product recovery feature.
        button(s,false);test_up=true;
        s.log.check("I08_actor_test_UP_sent_and_native_resources_idle",wait([&]{GUITHREADINFO gui{sizeof(gui)};return !s.left&&
            set.owns(0)&&GetGUIThreadInfo(set.thread,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize;})&&calls()==baseline&&
            record(s,offset,"gesture_end",g.generation).find("\"normal_up\":false")!=std::string::npos,
            "\"actual_matching_product_UP\":"+(actual_up(s,offset,g.generation).empty()?"null":quote(actual_up(s,offset,g.generation)))+
            ",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+",\"UP_delivery_not_inferred\":true,\"not_NormalUp\":true");
        interruption_idle(s,set,true);chain_check_product(s,product_pid,product_birth);interruption_recovery(s,set,g.generation);
        chain_check_product(s,product_pid,product_birth);
        s.log.emit("I08_input_actor_exit_scope","\"status\":\"INPUT_ACTOR_EXIT_DEADLINE_AND_RECOVERY_OBSERVED\",\"source_destroyed\":false,\"product_driver_channel\":false," 
            "\"actual_main_fixture_test_UP\":true,\"same_product_new_Move_Resize\":true,\"whole_I08_PASS\":false,\"human_uat\":\"NOT_RUN\"");
    }catch(...){
        // The child is one-shot with no waits. Keep desktop exposure bounded
        // even when its evidence/start/deadline fails, never turn cleanup into
        // a functional PASS. No product process or source is terminated here.
        try{const bool actor_gone=stop_test_actor();
            // A real START may survive a missing/failed receipt write. Keep
            // that observation separate from an unknown child API return.
            const auto started=record(s,offset,"gesture_start");if(number(started,"\"generation\":"))s.left=true;
            if(!test_up){button(s,false);test_up=true;}
            s.log.emit("I08_input_actor_failed_test_cleanup","\"test_UP_sent\":"+std::string(test_up?"true":"false")+
                ",\"actor_exit_observed\":"+(actor_gone?"true":"false")+",\"child_SendInput_return\":"+
                (actor_send_count_known?std::to_string(actor_inserted):"null")+",\"actual_START_observed\":"+
                (number(started,"\"generation\":")?"true":"false")+",\"input_delivery_not_inferred\":true,\"functional_PASS\":false");}catch(...){}throw;
    }
}
