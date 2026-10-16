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
std::wstring input_actor_start_event(const std::wstring& run,DWORD parent,std::uint64_t birth,DWORD child){
    return L"Local\\PaneBind.V1.InputActor.Start."+run+L"."+std::to_wstring(parent)+L"."+
        std::to_wstring(birth)+L"."+std::to_wstring(child);
}
struct InputActorCaptureView {
    void* value{};
    ~InputActorCaptureView(){if(value)UnmapViewOfFile(value);}
};
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
    const auto event_name=input_actor_start_event(run,parent_pid,born,GetCurrentProcessId());
    Handle start_ack(OpenEventW(SYNCHRONIZE,FALSE,event_name.c_str()));if(!start_ack.value)return 78;
    const auto capture_name=event_name+L".CaptureReceipt";
    Handle capture_mapping(OpenFileMappingW(FILE_MAP_READ,FALSE,capture_name.c_str()));if(!capture_mapping.value)return 78;
    InputActorCaptureView capture_view;capture_view.value=MapViewOfFile(capture_mapping.value,FILE_MAP_READ,0,0,sizeof(product::guest::ActorStartCaptureReceipt));
    if(!capture_view.value)return 78;
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
    // One actual DOWN, then an acknowledgement of the PARENT's observed
    // product START. Never replace it with a delay or replay the press.
    LARGE_INTEGER before{},after_down{},before_move{},after{},ack_before{},ack_after{};
    QueryPerformanceCounter(&before);const auto before_tick=GetTickCount64();
    const UINT down_inserted=SendInput(1,&packets[0],sizeof(INPUT));const DWORD down_error=down_inserted==1?0:GetLastError();
    QueryPerformanceCounter(&after_down);after=after_down;
    DWORD ack_result=WAIT_FAILED,ack_error{};bool pre_move_context=false,move_attempted=false,cursor_known=false,cursor_exact=false;
    DWORD cursor_error{},move_error{};UINT move_inserted{},cursor_observations{};POINT actual_after{};
    product::guest::ActorStartCaptureReceipt capture_receipt{};bool receipt_identity=false,product_live=false,source_exact=false,
        foreground_exact=false,source_gui_known=false,owner_gui_known=false,cursor_unchanged=false,modifiers_released=false,left_still_down=false,capture_context=false;
    product::guest::CapturedReleaseFacts fresh_capture;
    if(down_inserted==1){
        QueryPerformanceCounter(&ack_before);const std::array<HANDLE,2> waits{start_ack.value,parent.value};
        ack_result=WaitForMultipleObjects(static_cast<DWORD>(waits.size()),waits.data(),FALSE,2500);
        if(ack_result==WAIT_FAILED)ack_error=GetLastError();QueryPerformanceCounter(&ack_after);
        if(ack_result==WAIT_OBJECT_0){
            // The event orders one immutable, actual parent START/READY
            // receipt. Its contents alone never authorize the input below.
            MemoryBarrier();capture_receipt=*static_cast<const product::guest::ActorStartCaptureReceipt*>(capture_view.value);
            receipt_identity=product::guest::actor_start_capture_receipt_matches(capture_receipt,parent_pid,born,GetCurrentProcessId(),process_birth(GetCurrentProcess()))&&
                capture_receipt.capture.source==hwnd_value&&capture_receipt.capture.source_pid==parent_pid&&capture_receipt.capture.source_tid==source_tid&&
                capture_receipt.down_qpc>=static_cast<std::uint64_t>(before.QuadPart)&&capture_receipt.capture.product_pid<=MAXDWORD;
            fresh_capture=capture_receipt.capture;
            Handle product_process(receipt_identity?OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,static_cast<DWORD>(fresh_capture.product_pid)):nullptr);
            std::array<wchar_t,32768> product_name{};DWORD product_length=static_cast<DWORD>(product_name.size());
            const auto expected_product=output/(run+(debug?L"-product-Debug":L"-product-Release"))/L"panebind.exe";
            product_live=product_process.value&&WaitForSingleObject(product_process.value,0)==WAIT_TIMEOUT&&
                process_birth(product_process.value)==capture_receipt.product_birth&&medium(product_process.value)&&input_actor_same_user(product_process.value)&&
                QueryFullProcessImageNameW(product_process.value,0,product_name.data(),&product_length)&&equal(product_name.data(),expected_product.wstring())&&
                hash(expected_product)==capture_receipt.product_image_sha256.data();
            source_exact=WaitForSingleObject(parent.value,0)==WAIT_TIMEOUT&&process_birth(parent.value)==born&&
                input_actor_desktop(source_tid,parent_pid)&&IsWindow(source)&&GetAncestor(source,GA_ROOT)==source&&
                GetWindowThreadProcessId(source,&actual_pid)==source_tid&&actual_pid==parent_pid&&gui_class(source)==expected_class;
            const HWND overlay=reinterpret_cast<HWND>(static_cast<std::uintptr_t>(fresh_capture.overlay));DWORD overlay_pid{};
            fresh_capture.overlay_tid=overlay?GetWindowThreadProcessId(overlay,&overlay_pid):0;fresh_capture.overlay_pid=overlay_pid;
            fresh_capture.product_identity=product_live&&IsWindow(overlay)&&GetAncestor(overlay,GA_ROOT)==overlay;
            GUITHREADINFO gui{sizeof(gui)},owner{sizeof(owner)};
            source_gui_known=GetGUIThreadInfo(source_tid,&gui)!=FALSE;
            owner_gui_known=fresh_capture.overlay_tid&&GetGUIThreadInfo(static_cast<DWORD>(fresh_capture.overlay_tid),&owner)!=FALSE;
            fresh_capture.source_gui=source_gui_known;fresh_capture.owner_gui=owner_gui_known;
            fresh_capture.source_capture=reinterpret_cast<std::uintptr_t>(gui.hwndCapture);fresh_capture.owner_capture=reinterpret_cast<std::uintptr_t>(owner.hwndCapture);
            fresh_capture.gui_idle_except_capture=!gui.hwndMoveSize&&!gui.hwndMenuOwner&&!owner.hwndMoveSize&&!owner.hwndMenuOwner&&
                !(gui.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE))&&
                !(owner.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE));
            foreground_exact=GetForegroundWindow()==source;
            cursor_unchanged=GetCursorPos(&actual)&&actual.x==from.x&&actual.y==from.y;
            fresh_capture.hit_root=cursor_unchanged?reinterpret_cast<std::uintptr_t>(GetAncestor(WindowFromPoint(actual),GA_ROOT)):0;
            modifiers_released=!(GetAsyncKeyState(VK_CONTROL)&0x8000)&&!(GetAsyncKeyState(VK_SHIFT)&0x8000)&&
                !(GetAsyncKeyState(VK_MENU)&0x8000)&&!(GetAsyncKeyState(VK_LWIN)&0x8000)&&!(GetAsyncKeyState(VK_RWIN)&0x8000);
            left_still_down=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
            capture_context=receipt_identity&&product::guest::captured_release_route_allowed(fresh_capture)&&
                fresh_capture.overlay_tid==capture_receipt.capture.overlay_tid&&input_actor_desktop(static_cast<DWORD>(fresh_capture.overlay_tid),parent_pid);
            pre_move_context=receipt_identity&&product_live&&source_exact&&foreground_exact&&cursor_unchanged&&modifiers_released&&left_still_down&&capture_context;
            if(pre_move_context){
                move_attempted=true;QueryPerformanceCounter(&before_move);
                move_inserted=SendInput(1,&packets[1],sizeof(INPUT));move_error=move_inserted==1?0:GetLastError();QueryPerformanceCounter(&after);
                if(move_inserted==1)cursor_exact=wait([&]{++cursor_observations;cursor_known=GetCursorPos(&actual_after)!=FALSE;
                    cursor_error=cursor_known?0:GetLastError();return cursor_known&&actual_after.x==target.x&&actual_after.y==target.y;},500);
            }
        }
    }
    const UINT inserted=down_inserted+move_inserted;const DWORD error=down_error?down_error:move_error;
    const auto after_tick=GetTickCount64();
    const std::string text="{\"schema\":\"panebind-v1-test-input-actor/1\",\"source_sha\":\"" PANEBIND_BUILD_SHA "\",\"run_id\":"+quote(utf8(run))+
        ",\"image_sha256\":"+quote(image_hash)+",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"parent_pid\":"+std::to_string(parent_pid)+
        ",\"parent_birth\":"+std::to_string(born)+",\"source_hwnd\":"+std::to_string(hwnd_value)+",\"source_tid\":"+std::to_string(source_tid)+
        ",\"same_SID_session_Medium_guest\":true,\"SendInput_return\":"+std::to_string(inserted)+",\"SendInput_error\":"+std::to_string(error)+
        ",\"DOWN_SendInput_called\":true,\"DOWN_SendInput_return\":"+std::to_string(down_inserted)+",\"DOWN_SendInput_error\":"+std::to_string(down_error)+
        ",\"DOWN_after_qpc\":"+std::to_string(after_down.QuadPart)+",\"START_ack_wait_limit_ms\":2500,\"START_ack_wait_result\":"+std::to_string(ack_result)+
        ",\"START_ack_wait_error\":"+std::to_string(ack_error)+",\"START_ack_before_qpc\":"+std::to_string(ack_before.QuadPart)+
        ",\"START_ack_after_qpc\":"+std::to_string(ack_after.QuadPart)+",\"START_ack_received\":"+(ack_result==WAIT_OBJECT_0?"true":"false")+
        ",\"START_ack_event\":"+quote(utf8(event_name))+",\"child_event_access\":\"SYNCHRONIZE_ONLY\",\"pre_MOVE_exact_context\":"+(pre_move_context?"true":"false")+
        ",\"capture_receipt_access\":\"FILE_MAP_READ_ONLY\",\"capture_receipt_identity\":"+(receipt_identity?"true":"false")+
        ",\"actual_child_birth\":"+std::to_string(process_birth(GetCurrentProcess()))+",\"capture_receipt_child_birth\":"+std::to_string(capture_receipt.child_birth)+
        ",\"capture_receipt_generation\":"+std::to_string(capture_receipt.capture.generation)+",\"capture_receipt_product_PID\":"+std::to_string(capture_receipt.capture.product_pid)+
        ",\"capture_receipt_product_birth\":"+std::to_string(capture_receipt.product_birth)+",\"capture_receipt_ready_qpc\":"+std::to_string(capture_receipt.ready_qpc)+
        ",\"capture_receipt_DOWN_qpc\":"+std::to_string(capture_receipt.down_qpc)+",\"actual_product_live_image_birth_Medium\":"+(product_live?"true":"false")+
        ",\"actual_source_identity\":"+(source_exact?"true":"false")+",\"actual_foreground_is_source\":"+(foreground_exact?"true":"false")+
        ",\"actual_source_GUI_known\":"+(source_gui_known?"true":"false")+",\"actual_owner_GUI_known\":"+(owner_gui_known?"true":"false")+
        ",\"actual_cursor_unchanged\":"+(cursor_unchanged?"true":"false")+",\"actual_modifiers_released\":"+(modifiers_released?"true":"false")+
        ",\"actual_left_still_down\":"+(left_still_down?"true":"false")+
        ",\"actual_hit_root\":"+std::to_string(fresh_capture.hit_root)+",\"actual_overlay\":"+std::to_string(fresh_capture.overlay)+
        ",\"actual_overlay_PID\":"+std::to_string(fresh_capture.overlay_pid)+",\"actual_overlay_TID\":"+std::to_string(fresh_capture.overlay_tid)+
        ",\"actual_source_capture\":"+std::to_string(fresh_capture.source_capture)+",\"actual_owner_capture\":"+std::to_string(fresh_capture.owner_capture)+
        ",\"actual_exact_current_product_capture\":"+(capture_context?"true":"false")+
        ",\"MOVE_SendInput_called\":"+(move_attempted?"true":"false")+",\"MOVE_SendInput_return\":"+std::to_string(move_inserted)+
        ",\"MOVE_SendInput_error\":"+std::to_string(move_error)+",\"MOVE_before_qpc\":"+std::to_string(before_move.QuadPart)+
        ",\"cursor_observations\":"+std::to_string(cursor_observations)+",\"GetCursorPos_succeeded\":"+(cursor_known?"true":"false")+
        ",\"GetCursorPos_error\":"+std::to_string(cursor_error)+",\"requested_cursor\":["+std::to_string(target.x)+","+std::to_string(target.y)+"]"+
        ",\"actual_cursor\":"+(cursor_known?"["+std::to_string(actual_after.x)+","+std::to_string(actual_after.y)+"]":"null")+
        ",\"actual_cursor_exact\":"+(cursor_exact?"true":"false")+",\"native_MOVE_delivery_not_inferred_from_SendInput\":true"+
        ",\"before_tick\":"+std::to_string(before_tick)+",\"after_tick\":"+std::to_string(after_tick)+",\"before_qpc\":"+std::to_string(before.QuadPart)+
        ",\"after_qpc\":"+std::to_string(after.QuadPart)+",\"UP_sent\":false,\"product_driver_channel\":false,\"automated_guest_driver\":true}\n";
    DWORD written{};const bool saved=WriteFile(file.value,text.data(),static_cast<DWORD>(text.size()),&written,nullptr)&&written==text.size()&&FlushFileBuffers(file.value);
    return saved&&down_inserted==1&&ack_result==WAIT_OBJECT_0&&pre_move_context&&move_inserted==1&&cursor_exact?0:1;
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
    std::uint64_t expected_key{};std::string prepared;{std::istringstream rows(product_log_current(s));std::string line;
        while(std::getline(rows,line))if(line.find("\"type\":\"input_preparation\"")!=line.npos&&
            line.find("\"reason\":\"prepared_actual_source_and_hit\"")!=line.npos&&number(line,"\"process_id\":")==product_pid&&
            number(line,"\"root\":")==reinterpret_cast<std::uintptr_t>(source)&&line.find("\"region\":0")!=line.npos){
            expected_key=number(line,"\"key\":");prepared=line;}}
    if(!expected_key)throw std::runtime_error("I08_actor_product_source_key_not_observed");
    const auto offset=read_product(s).size();STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION child{};
    if(!CreateProcessW(self.data(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,s.output.c_str(),&startup,&child))throw std::runtime_error("I08_actor_create_failed");
    Handle child_process(child.hProcess),child_thread(child.hThread);bool test_up=false,actor_stop_attempted=false,actor_send_count_known=false,actor_resumed=false;
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
    Handle start_ack,capture_mapping;InputActorCaptureView capture_view;
    std::string actual_down,actual_start,actual_ready;Gesture g{*point,0,offset};
    product::guest::ActorStartCaptureReceipt capture_receipt{};
    try{
        const auto event_name=input_actor_start_event(s.run,GetCurrentProcessId(),process_birth(GetCurrentProcess()),child.dwProcessId);
        SetLastError(0);start_ack.value=CreateEventW(nullptr,TRUE,FALSE,event_name.c_str());const auto event_error=GetLastError();
        s.log.check("I08_actor_fresh_START_event",start_ack.value&&event_error!=ERROR_ALREADY_EXISTS&&child_birth,
            "\"event\":"+quote(utf8(event_name))+",\"CreateEvent_return\":"+(start_ack.value?"true":"false")+
            ",\"CreateEvent_error\":"+std::to_string(event_error)+",\"unknown_existing_event_not_signaled\":true");
        const auto capture_name=event_name+L".CaptureReceipt";SetLastError(0);
        capture_mapping.value=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,
            static_cast<DWORD>(sizeof(product::guest::ActorStartCaptureReceipt)),capture_name.c_str());const auto mapping_error=GetLastError();
        if(capture_mapping.value&&mapping_error!=ERROR_ALREADY_EXISTS)
            capture_view.value=MapViewOfFile(capture_mapping.value,FILE_MAP_WRITE,0,0,sizeof(product::guest::ActorStartCaptureReceipt));
        s.log.check("I08_actor_fresh_capture_receipt",capture_mapping.value&&mapping_error!=ERROR_ALREADY_EXISTS&&capture_view.value,
            "\"mapping\":"+quote(utf8(capture_name))+",\"CreateFileMapping_error\":"+std::to_string(mapping_error)+
            ",\"parent_write_child_read_only\":true,\"unknown_existing_mapping_not_reused\":true");
        const DWORD resumed=ResumeThread(child_thread.value);const auto resume_error=resumed==static_cast<DWORD>(-1)?GetLastError():0;
        actor_resumed=resumed!=static_cast<DWORD>(-1)&&resumed<=1;
        s.log.check("I08_actor_resume_exact_child",resumed==1,"\"ResumeThread_previous_suspend_count\":"+std::to_string(resumed)+
            ",\"ResumeThread_error\":"+std::to_string(resume_error));
        bool ack_ready=false;
        (void)wait([&]{
            if(WaitForSingleObject(child_process.value,0)!=WAIT_TIMEOUT)return true;
            actual_start=record(s,offset,"gesture_start");g.generation=number(actual_start,"\"generation\":");
            actual_down=record(s,offset,"input_button_observed",g.generation);
            actual_ready.clear();{const auto text=product_log_since(s,offset);std::istringstream rows(text);std::string line;
                while(std::getline(rows,line))if(number(line,"\"process_id\":")==product_pid&&number(line,"\"generation\":")==g.generation&&
                    line.find("\"source_sha\":"+quote(s.product_source))!=line.npos&&line.find("\"type\":\"gesture_input_resource\"")!=line.npos&&
                    line.find("\"fact\":\"capture_ready\"")!=line.npos)actual_ready=line;}
            auto capture=release_capture_facts(s,source,GetAncestor(WindowFromPoint(*point),GA_ROOT),g.generation);
            const auto down_qpc=number(actual_down,"\"hook_observed_qpc\":"),ready_qpc=number(actual_ready,"\"resource_observed_qpc\":");
            capture.ready_identity=capture.ready_identity&&down_qpc&&ready_qpc>=down_qpc;
            ack_ready=g.generation&&number(actual_start,"\"process_id\":")==product_pid&&number(actual_start,"\"key\":")==expected_key&&
                actual_start.find("\"source_sha\":"+quote(s.product_source))!=actual_start.npos&&
                actual_down.find("\"source_sha\":"+quote(s.product_source))!=actual_down.npos&&
                actual_start.find("\"resize\":false")!=actual_start.npos&&actual_start.find("\"ctrl\":false")!=actual_start.npos&&
                actual_start.find("\"original\":"+rect(*initial))!=actual_start.npos&&number(actual_down,"\"process_id\":")==product_pid&&
                number(actual_down,"\"source_key\":")==expected_key&&number(actual_down,"\"button\":")==1&&
                actual_down.find("\"kind\":\"DOWN\"")!=actual_down.npos&&actual_down.find("\"source\":\"WH_MOUSE_LL\"")!=actual_down.npos&&
                number(actual_down,"\"hook_observed_qpc\":")&&set.owns(0)&&visible(source)==initial&&GetForegroundWindow()==source&&
                actual_up(s,offset,g.generation).empty()&&record(s,offset,"gesture_end",g.generation).empty()&&
                product::guest::captured_release_route_allowed(capture)&&process_birth(s.process)==product_birth;
            if(ack_ready){
                capture_receipt.version=1;capture_receipt.parent_pid=GetCurrentProcessId();capture_receipt.parent_birth=process_birth(GetCurrentProcess());
                capture_receipt.child_pid=child.dwProcessId;capture_receipt.child_birth=child_birth;capture_receipt.product_birth=product_birth;
                capture_receipt.down_qpc=down_qpc;capture_receipt.ready_qpc=ready_qpc;capture_receipt.capture=capture;
            }
            return ack_ready;
        },5000);
        const auto product_hash=ack_ready?hash(s.product):std::string{};
        if(product_hash.size()==64)std::copy(product_hash.begin(),product_hash.end(),capture_receipt.product_image_sha256.begin());
        const bool capture_written=ack_ready&&product::guest::actor_start_capture_receipt_matches(capture_receipt,
            GetCurrentProcessId(),process_birth(GetCurrentProcess()),child.dwProcessId,child_birth);
        if(capture_written){*static_cast<product::guest::ActorStartCaptureReceipt*>(capture_view.value)=capture_receipt;MemoryBarrier();}
        const bool ack_called=capture_written&&WaitForSingleObject(child_process.value,0)==WAIT_TIMEOUT;
        const bool ack_sent=ack_called&&SetEvent(start_ack.value)!=FALSE;const auto ack_error=ack_called&&!ack_sent?GetLastError():0;
        s.log.check("I08_actor_actual_product_START_ack_once",ack_sent,
            "\"generation\":"+std::to_string(g.generation)+",\"expected_source_key\":"+std::to_string(expected_key)+
            ",\"current_product_PID\":"+std::to_string(product_pid)+",\"actual_DOWN\":"+quote(actual_down)+",\"actual_START\":"+quote(actual_start)+
            ",\"actual_CaptureReady\":"+quote(actual_ready)+",\"immutable_capture_receipt_written\":"+(capture_written?"true":"false")+
            ",\"capture_receipt_product_birth\":"+std::to_string(capture_receipt.product_birth)+",\"capture_receipt_product_sha256\":"+quote(product_hash)+
            ",\"capture_receipt_overlay\":"+std::to_string(capture_receipt.capture.overlay)+",\"capture_receipt_owner_tid\":"+std::to_string(capture_receipt.capture.overlay_tid)+
            ",\"capture_receipt_source_capture\":"+std::to_string(capture_receipt.capture.source_capture)+",\"capture_receipt_owner_capture\":"+std::to_string(capture_receipt.capture.owner_capture)+
            ",\"prepared_source\":"+quote(prepared)+",\"SetEvent_called\":"+(ack_called?"true":"false")+
            ",\"SetEvent_return\":"+(ack_sent?"true":"false")+",\"SetEvent_error\":"+std::to_string(ack_error)+
            ",\"ack_is_test_synchronization_not_product_authority\":true");
        const bool exited=wait([&]{return WaitForSingleObject(child_process.value,0)==WAIT_OBJECT_0;},5000);DWORD code=STILL_ACTIVE;
        if(!exited)(void)stop_test_actor();
        if(exited)GetExitCodeProcess(child_process.value,&code);const auto child_exit_tick=GetTickCount64();
        const auto actor=read(receipt,8192);
        const bool actor_identity=actor.ends_with("}\n")&&actor.find("\"UP_sent\":false")!=std::string::npos&&
            actor.find("\"source_sha\":\"" PANEBIND_BUILD_SHA "\"")!=std::string::npos&&actor.find("\"SendInput_return\":")!=std::string::npos&&
            number(actor,"\"parent_pid\":")==GetCurrentProcessId()&&number(actor,"\"pid\":")==child.dwProcessId&&
            number(actor,"\"source_hwnd\":")==reinterpret_cast<std::uintptr_t>(source)&&number(actor,"\"before_qpc\":");
        actor_send_count_known=actor_identity;actor_inserted=actor_identity?number(actor,"\"SendInput_return\":"):0;
        const bool actor_down_inserted=actor_identity&&number(actor,"\"DOWN_SendInput_return\":")==1;
        // Import the CHILD's successful-send ledger, not async physical state.
        // START/native receipts below independently prove product observation.
        const auto actor_down_qpc=number(actor,"\"before_qpc\":");
        const bool new_pair=exited&&actor_down_inserted&&child_birth&&process_birth(child_process.value)==child_birth&&
            number(actor,"\"parent_birth\":")==process_birth(GetCurrentProcess())&&number(actor,"\"source_tid\":")==set.thread&&
            actor.find("\"run_id\":"+quote(utf8(s.run)))!=actor.npos&&set.owns(0)&&!s.left&&
            actor_down_qpc>s.left_down_send_qpc&&actor_down_qpc>s.left_up_send_qpc;
        // A verified successful child NEW DOWN starts a new test-send pair.
        // The independent DOWN return remains known if MOVE/ack later fails.
        // Duplicate/unknown receipts cannot clear an attempted UP or grant
        // product authority; only this verified new test-send pair is imported.
        if(new_pair)s.left_up_attempted=false;
        if(actor_identity&&actor_inserted>=1){s.left=true;s.left_down_send_qpc=number(actor,"\"before_qpc\":");}
        s.log.check("I08_input_actor_exited_without_UP_source_remains",exited&&code==0&&actor_down_inserted&&actor_inserted==2&&new_pair&&s.left&&set.owns(0)&&GetForegroundWindow()==source,
            "\"child_pid\":"+std::to_string(child.dwProcessId)+",\"exit_code\":"+std::to_string(code)+",\"child_exit_tick\":"+std::to_string(child_exit_tick)+
            ",\"source_still_alive\":"+(set.owns(0)?"true":"false")+",\"child_DOWN_insertion_confirmed\":"+
            (actor_down_inserted?"true":"null")+",\"child_SendInput_return\":"+(actor_send_count_known?std::to_string(actor_inserted):"null")+
            ",\"driver_left_outstanding\":"+(s.left?"true":"false")+",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+
            ",\"new_verified_actor_send_pair_imported\":"+(new_pair?"true":"false")+",\"product_authority_not_granted_by_send_ledger\":true"+
            ",\"delivery_and_physical_button_held_not_inferred_from_ledger\":true,\"product_driver_channel\":false");
        const Rect moved=translated(*initial,-40,35);std::string native;
        const bool observed_move=wait([&]{native=placement(s,offset,g.generation,moved,true);
            return g.generation&&set.owns(0)&&visible(source)==moved&&actual_up(s,offset,g.generation).empty()&&
                actual_down.find("\"source\":\"WH_MOUSE_LL\"")!=std::string::npos&&actual_down.find("\"kind\":\"DOWN\"")!=std::string::npos&&
                number(actual_down,"\"hook_observed_qpc\":")&&native.find("\"authority_after\":true")!=std::string::npos;});
        s.log.check("I08_actor_actual_START_and_exact_Move",observed_move,
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
        const auto status_offset=read_product(s).size();
        s.log.check("I08_actor_Deadline_no_new_native_calls",launch(s,L"--status",true)&&wait([&]{return constraint_current_status_after(s,status_offset,barrier);})&&
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
        // The child has only the bounded START acknowledgement/readback waits.
        // Keep desktop exposure bounded
        // even when its evidence/start/deadline fails, never turn cleanup into
        // a functional PASS. No product process or source is terminated here.
        try{const bool actor_gone=stop_test_actor();
            // A new verified single DOWN can survive an ack/MOVE failure or
            // a failed final receipt write. Never infer it from async state.
            const auto failed_actor=read(receipt,8192);
            if(!actor_send_count_known&&failed_actor.ends_with("}\n")&&
                number(failed_actor,"\"parent_pid\":")==GetCurrentProcessId()&&number(failed_actor,"\"pid\":")==child.dwProcessId&&
                number(failed_actor,"\"parent_birth\":")==process_birth(GetCurrentProcess())&&
                number(failed_actor,"\"source_hwnd\":")==reinterpret_cast<std::uintptr_t>(source)&&
                failed_actor.find("\"source_sha\":\"" PANEBIND_BUILD_SHA "\"")!=failed_actor.npos){
                actor_send_count_known=true;actor_inserted=number(failed_actor,"\"SendInput_return\":");
                const auto stamp=number(failed_actor,"\"before_qpc\":");
                if(number(failed_actor,"\"DOWN_SendInput_return\":")==1&&!s.left&&stamp>s.left_down_send_qpc&&stamp>s.left_up_send_qpc){
                    s.left_up_attempted=false;s.left=true;s.left_down_send_qpc=stamp;}}
            const auto started=record(s,offset,"gesture_start");const auto generation=number(started,"\"generation\":");
            const auto observed_down=record(s,offset,"input_button_observed",generation);const auto down_qpc=number(observed_down,"\"hook_observed_qpc\":");
            if(generation&&number(started,"\"process_id\":")==product_pid&&number(started,"\"key\":")==expected_key&&
                number(observed_down,"\"process_id\":")==product_pid&&number(observed_down,"\"source_key\":")==expected_key&&
                observed_down.find("\"kind\":\"DOWN\"")!=observed_down.npos&&down_qpc>s.left_down_send_qpc&&down_qpc>s.left_up_send_qpc&&!s.left){
                s.left_up_attempted=false;s.left=true;s.left_down_send_qpc=down_qpc;}
            const bool actor_down_possible=actor_resumed&&(s.left||!actor_send_count_known||actor_inserted>=1);
            if(!test_up&&actor_down_possible){
                // A failed receipt may leave only UNKNOWN partial insertion.
                // This actor's local fence permits ONE guest-only cleanup UP,
                // not a reset/retry of an UP already attempted for this actor.
                if(!s.left&&s.left_up_attempted&&!actor_send_count_known){
                    test_up=true;INPUT release{};release.type=INPUT_MOUSE;release.mi.dwFlags=MOUSEEVENTF_LEFTUP;release.mi.dwExtraInfo=0x50425631;
                    const bool input_desktop=desktop();const UINT inserted=input_desktop?SendInput(1,&release,sizeof(release)):0;
                    const auto error=input_desktop&&inserted!=1?GetLastError():0;
                    s.log.emit("I08_unknown_actor_single_test_UP","\"SendInput_called\":"+std::string(input_desktop?"true":"false")+
                        ",\"SendInput_return\":"+std::to_string(inserted)+",\"error\":"+std::to_string(error)+
                        ",\"actor_local_UP_attempt_retired\":true,\"previous_UP_guard_not_reset\":true,\"delivery_not_inferred\":true");
                }else{test_up=true;button(s,false);}
            }
            s.log.emit("I08_input_actor_failed_test_cleanup","\"test_UP_attempted\":"+std::string(test_up?"true":"false")+
                ",\"actor_resumed\":"+(actor_resumed?"true":"false")+",\"actor_DOWN_possible_or_confirmed\":"+(actor_down_possible?"true":"false")+
                ",\"test_UP_delivery_not_inferred\":true,\"actor_exit_observed\":"+(actor_gone?"true":"false")+",\"child_SendInput_return\":"+
                (actor_send_count_known?std::to_string(actor_inserted):"null")+",\"actual_START_observed\":"+
                (number(started,"\"generation\":")?"true":"false")+",\"input_delivery_not_inferred\":true,\"functional_PASS\":false");}catch(...){}throw;
    }
}
