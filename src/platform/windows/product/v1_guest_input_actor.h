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
// A small completion transport for the SAME real input child. No product
// connection, controller proxy, new input permissions or retry protocol.
struct InputActorDisconnectPacket {
    std::uint64_t version{},parent_pid{},parent_birth{},child_pid{},child_birth{},source{},source_tid{},generation{};
    std::uint64_t down_qpc{},down_inserted{},move_inserted{},total_inserted{};
};
static_assert(sizeof(InputActorDisconnectPacket)==96);
fs::path input_actor_disconnect_packet_path(const fs::path& output,const std::wstring& run,bool debug){
    return output/(run+(debug?L"-debug-input-actor-completion.packet":L"-release-input-actor-completion.packet"));
}
struct InputActorDisconnectFile {
    Handle reader,ready,close_ack;fs::path path;
};
bool input_actor_disconnect_cleanup_up(State&,LayoutFixtures&,std::size_t,std::uint64_t,bool&);
int input_actor_child(int count,LPWSTR* args,bool disconnect=false){
    if(count!=11||std::wstring_view(args[1])!=(disconnect?L"--input-actor-disconnect-once":L"--input-actor-once"))return 78;
    const std::wstring run=args[2];
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
    Handle disconnect_ready,disconnect_ack;fs::path disconnect_packet;
    if(disconnect){
        const auto package=read(input/L"manifest.json",256*1024);const auto files=package.find("\"files\"");
        if(files==package.npos)return 78;const std::string_view identity(package.data(),files);
        // The immutable packager puts guest_suite AFTER the files array. Only
        // the binary identity fields belong to the flat pre-files prefix.
        if(portable_field(identity,"run_id")!=utf8(run)||portable_field(package,"guest_suite")!="DriverDisconnectOnly"||
            portable_field(identity,"driver_source_commit")!=PANEBIND_BUILD_SHA||
            portable_field(identity,debug?"driver_debug_sha256":"driver_release_sha256")!=image_hash)return 78;
        disconnect_packet=input_actor_disconnect_packet_path(output,run,debug);SetLastError(ERROR_SUCCESS);
        if(GetFileAttributesW(disconnect_packet.c_str())!=INVALID_FILE_ATTRIBUTES||GetLastError()!=ERROR_FILE_NOT_FOUND)return 78;
    }
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
    if(disconnect){disconnect_ready.value=OpenEventW(EVENT_MODIFY_STATE,FALSE,(event_name+L".DisconnectReceiptReady").c_str());
        disconnect_ack.value=OpenEventW(SYNCHRONIZE,FALSE,(event_name+L".DisconnectNow").c_str());
        if(!disconnect_ready.value||!disconnect_ack.value)return 78;}
    const auto capture_name=event_name+L".CaptureReceipt";
    Handle capture_mapping(OpenFileMappingW(FILE_MAP_READ,FALSE,capture_name.c_str()));if(!capture_mapping.value)return 78;
    InputActorCaptureView capture_view;capture_view.value=MapViewOfFile(capture_mapping.value,FILE_MAP_READ,0,0,sizeof(product::guest::ActorCaptureHandshake));
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
    LARGE_INTEGER before{},after_down{},before_move{},after{},ack_before{},ack_after{},threshold_before{},threshold_after{};
    QueryPerformanceCounter(&before);const auto before_tick=GetTickCount64();
    const UINT down_inserted=SendInput(1,&packets[0],sizeof(INPUT));const DWORD down_error=down_inserted==1?0:GetLastError();
    QueryPerformanceCounter(&after_down);after=after_down;
    DWORD ack_result=WAIT_FAILED,ack_error{};bool pre_move_context=false,move_attempted=false,cursor_known=false,cursor_exact=false;
    DWORD cursor_error{},move_error{};UINT move_inserted{},cursor_observations{};POINT actual_after{};
    product::guest::ActorStartCaptureReceipt capture_receipt{};bool receipt_identity=false,product_live=false,source_exact=false,
        foreground_exact=false,source_gui_known=false,owner_gui_known=false,cursor_unchanged=false,modifiers_released=false,left_still_down=false,capture_context=false;
    SHORT async_left_value{};bool async_left_queried=false;unsigned own_up_calls{};
    product::guest::ActorNativeThresholdCommand threshold_command{};
    bool threshold_received=false,threshold_identity=false,threshold_context=false,threshold_attempted=false,threshold_skipped_native_started=false,threshold_skipped_actual_ready=false;
    UINT threshold_inserted{},threshold_cursor_observations{};DWORD threshold_error{};POINT threshold_cursor{};bool threshold_cursor_known=false,threshold_cursor_exact=false;
    POINT ready_cursor=from;
    product::guest::CapturedReleaseFacts fresh_capture;
    if(down_inserted==1){
        QueryPerformanceCounter(&ack_before);const std::array<HANDLE,2> waits{start_ack.value,parent.value};
        const auto ack_deadline=GetTickCount64()+2500;
        for(unsigned phase_count=0;phase_count<2;++phase_count){
            const auto now=GetTickCount64();if(now>=ack_deadline){ack_result=WAIT_TIMEOUT;break;}
            ack_result=WaitForMultipleObjects(static_cast<DWORD>(waits.size()),waits.data(),FALSE,static_cast<DWORD>(ack_deadline-now));
            if(ack_result!=WAIT_OBJECT_0){if(ack_result==WAIT_FAILED)ack_error=GetLastError();break;}
            const auto* handshake=static_cast<const product::guest::ActorCaptureHandshake*>(capture_view.value);
            const auto phase=handshake->phase.load(std::memory_order_acquire);
            if(phase==product::guest::ActorCapturePhase::ActualStartReady){capture_receipt=handshake->ready;break;}
            if(phase!=product::guest::ActorCapturePhase::NativeThreshold||threshold_received){ack_result=WAIT_FAILED;ack_error=ERROR_INVALID_DATA;break;}
            threshold_received=true;threshold_command=handshake->threshold;
            threshold_identity=product::guest::actor_native_threshold_command_matches(threshold_command,parent_pid,born,GetCurrentProcessId(),
                process_birth(GetCurrentProcess()),hwnd_value,source_tid,from.x,from.y,static_cast<std::uint64_t>(before.QuadPart));
            Handle threshold_product(threshold_identity&&threshold_command.product_pid<=MAXDWORD?
                OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,static_cast<DWORD>(threshold_command.product_pid)):nullptr);
            std::array<wchar_t,32768> threshold_product_image{};DWORD threshold_image_size=static_cast<DWORD>(threshold_product_image.size());
            const auto expected_product=output/(run+(debug?L"-product-Debug":L"-product-Release"))/L"panebind.exe";
            GUITHREADINFO native{sizeof(native)};POINT current{};const auto current_visible=visible(source);
            const Rect original{static_cast<LONG>(threshold_command.original_visible[0]),static_cast<LONG>(threshold_command.original_visible[1]),
                static_cast<LONG>(threshold_command.original_visible[2]),static_cast<LONG>(threshold_command.original_visible[3])};
            threshold_context=threshold_identity&&threshold_product.value&&WaitForSingleObject(threshold_product.value,0)==WAIT_TIMEOUT&&
                process_birth(threshold_product.value)==threshold_command.product_birth&&medium(threshold_product.value)&&input_actor_same_user(threshold_product.value)&&
                QueryFullProcessImageNameW(threshold_product.value,0,threshold_product_image.data(),&threshold_image_size)&&
                equal(threshold_product_image.data(),expected_product.wstring())&&hash(expected_product)==threshold_command.product_image_sha256.data()&&
                WaitForSingleObject(parent.value,0)==WAIT_TIMEOUT&&process_birth(parent.value)==born&&input_actor_desktop(source_tid,parent_pid)&&
                IsWindow(source)&&GetAncestor(source,GA_ROOT)==source&&GetWindowThreadProcessId(source,&actual_pid)==source_tid&&actual_pid==parent_pid&&
                gui_class(source)==expected_class&&current_visible==original&&GetForegroundWindow()==source&&GetCursorPos(&current)&&
                current.x==from.x&&current.y==from.y&&WindowFromPoint(current)==source&&GetGUIThreadInfo(source_tid,&native)&&
                (!native.hwndCapture||native.hwndCapture==source)&&(!native.hwndMoveSize||native.hwndMoveSize==source)&&!native.hwndMenuOwner&&
                !(native.flags&(GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE))&&
                !(GetAsyncKeyState(VK_CONTROL)&0x8000)&&!(GetAsyncKeyState(VK_SHIFT)&0x8000)&&!(GetAsyncKeyState(VK_MENU)&0x8000)&&
                !(GetAsyncKeyState(VK_LWIN)&0x8000)&&!(GetAsyncKeyState(VK_RWIN)&0x8000)&&
                threshold_command.drag_x==GetSystemMetrics(SM_CXDRAG)&&threshold_command.drag_y==GetSystemMetrics(SM_CYDRAG)&&
                threshold_command.destination_x>=x&&threshold_command.destination_x<static_cast<long long>(x)+w&&
                threshold_command.destination_y>=y&&threshold_command.destination_y<static_cast<long long>(y)+h;
            if(!threshold_context){
                // The owner may have completed the REAL handoff between the
                // threshold signal and these fresh native reads. Accept only
                // its separately published Ready slot, then run the original
                // child-side capture checks; never manufacture that ACK.
                if(threshold_identity&&handshake->phase.load(std::memory_order_acquire)==product::guest::ActorCapturePhase::ActualStartReady){
                    capture_receipt=handshake->ready;threshold_skipped_actual_ready=true;break;
                }
                ack_result=WAIT_FAILED;ack_error=ERROR_INVALID_DATA;break;
            }
            threshold_skipped_native_started=native.hwndMoveSize==source;
            if(!threshold_skipped_native_started){
                if(native.flags&GUI_INMOVESIZE){ack_result=WAIT_FAILED;ack_error=ERROR_INVALID_DATA;break;}
                INPUT threshold{};threshold.type=INPUT_MOUSE;threshold.mi.dwExtraInfo=0x50425631;
                threshold.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
                threshold.mi.dx=static_cast<LONG>(((threshold_command.destination_x-x)*2+1)*65536/(static_cast<long long>(w)*2));
                threshold.mi.dy=static_cast<LONG>(((threshold_command.destination_y-y)*2+1)*65536/(static_cast<long long>(h)*2));
                threshold_attempted=true;QueryPerformanceCounter(&threshold_before);SetLastError(0);
                threshold_inserted=SendInput(1,&threshold,sizeof(threshold));threshold_error=threshold_inserted==1?0:GetLastError();
                QueryPerformanceCounter(&threshold_after);const auto observed_at=GetTickCount64();
                if(threshold_inserted==1&&observed_at<ack_deadline)threshold_cursor_exact=layout_wait_sent([&]{
                    ++threshold_cursor_observations;threshold_cursor_known=GetCursorPos(&threshold_cursor)!=FALSE;
                    return threshold_cursor_known&&threshold_cursor.x==threshold_command.destination_x&&threshold_cursor.y==threshold_command.destination_y;
                },static_cast<DWORD>(std::min<ULONGLONG>(500,ack_deadline-observed_at)));
                if(threshold_inserted!=1||!threshold_cursor_exact){ack_result=WAIT_FAILED;ack_error=threshold_error?threshold_error:ERROR_INVALID_DATA;break;}
                ready_cursor={static_cast<LONG>(threshold_command.destination_x),static_cast<LONG>(threshold_command.destination_y)};
            }
            // The next signal still has to announce the ORIGINAL actual
            // START/END/CaptureReady receipt; this command is never that ACK.
            ack_result=WAIT_FAILED;
        }
        QueryPerformanceCounter(&ack_after);
        if(ack_result==WAIT_OBJECT_0){
            // The event orders one immutable, actual parent START/READY
            // receipt. Its contents alone never authorize the input below.
            receipt_identity=product::guest::actor_start_capture_receipt_matches(capture_receipt,parent_pid,born,GetCurrentProcessId(),process_birth(GetCurrentProcess()))&&
                capture_receipt.capture.source==hwnd_value&&capture_receipt.capture.source_pid==parent_pid&&capture_receipt.capture.source_tid==source_tid&&
                capture_receipt.down_qpc>=static_cast<std::uint64_t>(before.QuadPart)&&capture_receipt.capture.product_pid<=MAXDWORD&&
                (!threshold_received||(capture_receipt.capture.generation==threshold_command.generation&&
                    capture_receipt.capture.product_pid==threshold_command.product_pid&&capture_receipt.product_birth==threshold_command.product_birth&&
                    capture_receipt.down_qpc==threshold_command.down_qpc));
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
            cursor_unchanged=GetCursorPos(&actual)&&actual.x==ready_cursor.x&&actual.y==ready_cursor.y;
            fresh_capture.hit_root=cursor_unchanged?reinterpret_cast<std::uintptr_t>(GetAncestor(WindowFromPoint(actual),GA_ROOT)):0;
            modifiers_released=!(GetAsyncKeyState(VK_CONTROL)&0x8000)&&!(GetAsyncKeyState(VK_SHIFT)&0x8000)&&
                !(GetAsyncKeyState(VK_MENU)&0x8000)&&!(GetAsyncKeyState(VK_LWIN)&0x8000)&&!(GetAsyncKeyState(VK_RWIN)&0x8000);
            async_left_value=GetAsyncKeyState(VK_LBUTTON);async_left_queried=true;left_still_down=(async_left_value&0x8000)!=0;
            capture_context=receipt_identity&&product::guest::captured_release_route_allowed(fresh_capture)&&
                fresh_capture.overlay_tid==capture_receipt.capture.overlay_tid&&input_actor_desktop(static_cast<DWORD>(fresh_capture.overlay_tid),parent_pid);
            // Preserve the actual Async result, including a clear high bit.
            // This actor emits synthetic input, not a physical press, and an
            // independent queue may also receive zero on query failure. Do
            // not convert either reading into an actual UP or a held claim.
            pre_move_context=product::guest::actor_single_move_may_be_sent(down_inserted,move_attempted?1u:0u,own_up_calls,
                receipt_identity,product_live&&source_exact&&foreground_exact&&cursor_unchanged&&modifiers_released&&capture_context);
            if(pre_move_context){
                move_attempted=true;QueryPerformanceCounter(&before_move);
                move_inserted=SendInput(1,&packets[1],sizeof(INPUT));move_error=move_inserted==1?0:GetLastError();QueryPerformanceCounter(&after);
                if(move_inserted==1)cursor_exact=wait([&]{++cursor_observations;cursor_known=GetCursorPos(&actual_after)!=FALSE;
                    cursor_error=cursor_known?0:GetLastError();return cursor_known&&actual_after.x==target.x&&actual_after.y==target.y;},500);
            }
        }
    }
    const UINT inserted=down_inserted+threshold_inserted+move_inserted;const DWORD error=down_error?down_error:threshold_error?threshold_error:move_error;
    bool disconnect_created=false,disconnect_written=false,disconnect_flushed=false,disconnect_publishable=false,disconnect_signaled=false,disconnect_closed=false;
    BOOL disconnect_write_return=FALSE,disconnect_flush_return=FALSE;DWORD disconnect_bytes{};
    DWORD disconnect_create_error{},disconnect_error{},disconnect_flush_error{},disconnect_wait=WAIT_FAILED,disconnect_ready_error{},disconnect_close_error{};
    std::string disconnect_hash;
    if(disconnect){
        const InputActorDisconnectPacket packet{1,parent_pid,born,GetCurrentProcessId(),process_birth(GetCurrentProcess()),hwnd_value,source_tid,
            capture_receipt.capture.generation,static_cast<std::uint64_t>(before.QuadPart),down_inserted,move_inserted,inserted};
        // This exact child is the sole creator/writer. A pre-existing packet
        // is never opened for writing, replaced, repaired or deleted.
        SetLastError(ERROR_SUCCESS);Handle completion(CreateFileW(disconnect_packet.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        disconnect_created=completion.value!=INVALID_HANDLE_VALUE;disconnect_create_error=disconnect_created?ERROR_SUCCESS:GetLastError();
        if(disconnect_created){
            SetLastError(ERROR_SUCCESS);disconnect_write_return=WriteFile(completion.value,&packet,sizeof(packet),&disconnect_bytes,nullptr);
            disconnect_error=disconnect_write_return?ERROR_SUCCESS:GetLastError();
            disconnect_written=disconnect_write_return&&disconnect_bytes==sizeof(packet);
            if(disconnect_written){SetLastError(ERROR_SUCCESS);disconnect_flush_return=FlushFileBuffers(completion.value);
                disconnect_flush_error=disconnect_flush_return?ERROR_SUCCESS:GetLastError();disconnect_flushed=disconnect_flush_return!=FALSE;}
            SetLastError(ERROR_SUCCESS);disconnect_closed=CloseHandle(completion.value)!=FALSE;
            disconnect_close_error=disconnect_closed?ERROR_SUCCESS:GetLastError();if(disconnect_closed)completion.value=nullptr;
        }
        if(disconnect_written&&disconnect_flushed&&disconnect_closed)disconnect_hash=hash(disconnect_packet);
        disconnect_publishable=disconnect_created&&disconnect_written&&disconnect_flushed&&disconnect_closed&&disconnect_hash.size()==64;
        if(disconnect_publishable){SetLastError(ERROR_SUCCESS);disconnect_signaled=SetEvent(disconnect_ready.value)!=FALSE;
            disconnect_ready_error=disconnect_signaled?ERROR_SUCCESS:GetLastError();}
        if(disconnect_signaled){
            // Parent first observes the real exact placement. Its signal is
            // permission to end THIS test child, never product authority.
            const std::array<HANDLE,2> peers{disconnect_ack.value,parent.value};
            disconnect_wait=WaitForMultipleObjects(static_cast<DWORD>(peers.size()),peers.data(),FALSE,5000);
        }
    }
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
        ",\"threshold_command_received\":"+(threshold_received?"true":"false")+",\"threshold_command_identity\":"+(threshold_identity?"true":"false")+
        ",\"threshold_actual_context\":"+(threshold_context?"true":"false")+",\"threshold_command_generation\":"+std::to_string(threshold_command.generation)+
        ",\"threshold_skipped_actual_native_MoveSize\":"+(threshold_skipped_native_started?"true":"false")+
        ",\"threshold_skipped_separately_published_actual_READY\":"+(threshold_skipped_actual_ready?"true":"false")+
        ",\"threshold_MOVE_SendInput_called\":"+(threshold_attempted?"true":"false")+",\"threshold_MOVE_SendInput_return\":"+std::to_string(threshold_inserted)+
        ",\"threshold_MOVE_SendInput_error\":"+std::to_string(threshold_error)+",\"threshold_MOVE_before_qpc\":"+std::to_string(threshold_before.QuadPart)+
        ",\"threshold_MOVE_after_qpc\":"+std::to_string(threshold_after.QuadPart)+",\"threshold_actual_cursor_exact\":"+(threshold_cursor_exact?"true":"false")+
        ",\"threshold_cursor_observations\":"+std::to_string(threshold_cursor_observations)+",\"threshold_cursor_wait_within_original_ack_deadline\":true"+
        ",\"threshold_requested_cursor\":["+std::to_string(threshold_command.destination_x)+","+std::to_string(threshold_command.destination_y)+"]"+
        ",\"START_ready_expected_cursor\":["+std::to_string(ready_cursor.x)+","+std::to_string(ready_cursor.y)+"],\"original_DOWN_anchor_preserved\":true"+
        ",\"parent_no_UP_or_END_observed_before_ack\":"+(capture_receipt.no_up_or_end_observed_before_ack?"true":"false")+
        ",\"actual_source_identity\":"+(source_exact?"true":"false")+",\"actual_foreground_is_source\":"+(foreground_exact?"true":"false")+
        ",\"actual_source_GUI_known\":"+(source_gui_known?"true":"false")+",\"actual_owner_GUI_known\":"+(owner_gui_known?"true":"false")+
        ",\"actual_cursor_unchanged\":"+(cursor_unchanged?"true":"false")+",\"actual_modifiers_released\":"+(modifiers_released?"true":"false")+
        ",\"actual_left_still_down\":null"+
        ",\"GetAsyncKeyState_left_raw_SHORT\":"+(async_left_queried?std::to_string(async_left_value):"null")+
        ",\"GetAsyncKeyState_left_high_bit\":"+(async_left_queried?(left_still_down?"true":"false"):"null")+
        ",\"Async_failure_or_physical_state_distinguished\":false,\"Async_used_as_held_or_UP_proof\":false"+
        ",\"own_DOWN_calls\":1,\"own_MOVE_calls\":"+std::to_string(static_cast<unsigned>(threshold_attempted)+static_cast<unsigned>(move_attempted))+
        ",\"own_threshold_MOVE_calls\":"+(threshold_attempted?"1":"0")+",\"own_post_READY_intent_MOVE_calls\":"+(move_attempted?"1":"0")+",\"own_UP_calls\":"+std::to_string(own_up_calls)+
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
        ",\"after_qpc\":"+std::to_string(after.QuadPart)+",\"UP_sent\":false,\"product_driver_channel\":false,\"automated_guest_driver\":true"+
        (disconnect?",\"disconnect_transport_used\":true,\"completion_transport\":\"fixed96_CREATE_NEW_file_and_existing_events\",\"packet_path\":"+quote(utf8(disconnect_packet.wstring()))+
            ",\"packet_CreateFile_attempted\":true,\"packet_CREATE_NEW_return\":"+(disconnect_created?"true":"false")+
            ",\"packet_CREATE_NEW_error\":"+(disconnect_created?"null":std::to_string(disconnect_create_error))+
            ",\"packet_WriteFile_attempted\":"+(disconnect_created?"true":"false")+
            ",\"packet_WriteFile_return\":"+(disconnect_created?(disconnect_write_return?"true":"false"):"null")+
            ",\"packet_bytes_written\":"+std::to_string(disconnect_bytes)+",\"packet_write_completed\":"+(disconnect_written?"true":"false")+
            ",\"packet_WriteFile_error\":"+(disconnect_created&&!disconnect_write_return?std::to_string(disconnect_error):"null")+
            ",\"packet_FlushFileBuffers_attempted\":"+(disconnect_written?"true":"false")+
            ",\"packet_FlushFileBuffers_return\":"+(disconnect_written?(disconnect_flush_return?"true":"false"):"null")+
            ",\"packet_FlushFileBuffers_error\":"+(disconnect_written&&!disconnect_flush_return?std::to_string(disconnect_flush_error):"null")+
            ",\"packet_CloseHandle_attempted\":"+(disconnect_created?"true":"false")+
            ",\"packet_CloseHandle_return\":"+(disconnect_created?(disconnect_closed?"true":"false"):"null")+
            ",\"packet_CloseHandle_error\":"+(disconnect_created&&!disconnect_closed?std::to_string(disconnect_close_error):"null")+
            ",\"packet_sha256_after_close\":"+(disconnect_hash.empty()?"null":quote(disconnect_hash))+
            ",\"packet_ready_signal_attempted\":"+(disconnect_publishable?"true":"false")+",\"packet_ready_signaled\":"+(disconnect_signaled?"true":"false")+
            ",\"packet_ready_signal_error\":"+(disconnect_publishable&&!disconnect_signaled?std::to_string(disconnect_ready_error):"null")+
            ",\"parent_observed_placement_ack_wait\":"+std::to_string(disconnect_wait)+
            ",\"anonymous_pipe_used\":false,\"handles_inherited\":false,\"packet_retained_as_evidence\":true":
            std::string{})+"}\n";
    DWORD written{};const bool saved=WriteFile(file.value,text.data(),static_cast<DWORD>(text.size()),&written,nullptr)&&written==text.size()&&FlushFileBuffers(file.value);
    return saved&&down_inserted==1&&ack_result==WAIT_OBJECT_0&&pre_move_context&&move_inserted==1&&cursor_exact&&
        (!disconnect||(disconnect_created&&disconnect_written&&disconnect_flushed&&disconnect_closed&&disconnect_hash.size()==64&&disconnect_signaled&&disconnect_wait==WAIT_OBJECT_0))?0:1;
}
void owned_input_actor_exit_transport(State& s,LayoutFixtures& set,DWORD product_pid,std::uint64_t product_birth,bool disconnect){
    namespace v=panebind::core::v1;const HWND source=set.frames[0].hwnd;const auto initial=visible(source);
    if(!set.owns(0)||!initial||s.left||s.control)throw std::runtime_error("I08_actor_initial_exact_source_unavailable");
    const UINT initial_dpi=GetDpiForWindow(source);const HMONITOR initial_monitor=MonitorFromWindow(source,MONITOR_DEFAULTTONULL);
    if(!initial_dpi||!initial_monitor)throw std::runtime_error("I08_actor_initial_monitor_or_DPI_unknown");
    chain_check_product(s,product_pid,product_birth);const auto config=chain_settings(s);
    if(config.magnet_move||config.docking||config.shake||config.drag_transparency)throw std::runtime_error("I08_actor_requires_existing_interruption_configuration");
    if(disconnect&&s.suite!="DriverDisconnectOnly")throw std::runtime_error("I08_disconnect_exact_suite_required");
    const auto receipt=s.output/(s.run+(s.configuration=="Debug"?L"-debug-input-actor.json":L"-release-input-actor.json"));
    if(fs::exists(receipt))throw std::runtime_error("I08_actor_receipt_already_exists");
    s.log.check("I08_actor_exact_foreground",SetForegroundWindow(source)&&wait([&]{return GetForegroundWindow()==source;}));
    const auto point=layout_hit(set,0,HTCAPTION);if(!point)throw std::runtime_error("I08_actor_real_caption_hit_unavailable");
    cursor(s,*point);const auto hover=GetTickCount64();(void)layout_wait_sent([&]{return GetTickCount64()-hover>=100;},100);
    std::array<wchar_t,32768> self{};if(!GetModuleFileNameW(nullptr,self.data(),static_cast<DWORD>(self.size())))throw std::runtime_error("I08_actor_fixture_image_unavailable");
    InputActorDisconnectFile transport;
    if(disconnect){transport.path=input_actor_disconnect_packet_path(s.output,s.run,s.configuration=="Debug");SetLastError(ERROR_SUCCESS);
        const DWORD attributes=GetFileAttributesW(transport.path.c_str());const DWORD error=attributes==INVALID_FILE_ATTRIBUTES?GetLastError():ERROR_SUCCESS;
        s.log.check("I08_disconnect_fresh_completion_packet_path",attributes==INVALID_FILE_ATTRIBUTES&&error==ERROR_FILE_NOT_FOUND,
            "\"packet_path\":"+quote(utf8(transport.path.wstring()))+",\"GetFileAttributes_return\":"+std::to_string(attributes)+
            ",\"GetFileAttributes_error\":"+std::to_string(error)+",\"existing_file_not_overwritten\":true,\"parent_packet_write_calls\":0,"+
            "\"transport_route\":\"fixed96_child_CREATE_NEW_file\",\"runtime_fallback\":false,\"handles_inherited\":false");}
    std::wstring command=L"\""+std::wstring(self.data())+(disconnect?L"\" --input-actor-disconnect-once ":L"\" --input-actor-once ")+s.run+L" "+std::to_wstring(GetCurrentProcessId())+L" "+
        std::to_wstring(process_birth(GetCurrentProcess()))+L" "+std::to_wstring(reinterpret_cast<std::uintptr_t>(source))+L" "+std::to_wstring(set.thread)+L" "+
        std::to_wstring(point->x)+L" "+std::to_wstring(point->y)+L" "+std::to_wstring(point->x-40)+L" "+std::to_wstring(point->y+35);
    std::uint64_t expected_key{};std::string prepared;{std::istringstream rows(product_log_current(s));std::string line;
        while(std::getline(rows,line))if(line.find("\"type\":\"input_preparation\"")!=line.npos&&
            line.find("\"reason\":\"prepared_actual_source_and_hit\"")!=line.npos&&number(line,"\"process_id\":")==product_pid&&
            number(line,"\"root\":")==reinterpret_cast<std::uintptr_t>(source)&&line.find("\"region\":0")!=line.npos){
            expected_key=number(line,"\"key\":");prepared=line;}}
    if(!expected_key)throw std::runtime_error("I08_actor_product_source_key_not_observed");
    const auto offset=read_product(s).size();STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
    const DWORD flags=CREATE_NO_WINDOW|CREATE_SUSPENDED;
    if(!CreateProcessW(self.data(),command.data(),nullptr,nullptr,FALSE,flags,nullptr,s.output.c_str(),&startup,&child))throw std::runtime_error("I08_actor_create_failed");
    Handle child_process(child.hProcess),child_thread(child.hThread);bool test_up=false,cleanup_checked=false,actor_stop_attempted=false,actor_send_count_known=false,actor_resumed=false;
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
    std::string actual_down,actual_start,actual_ready,actual_pending,actual_native_start,actual_native_end,actual_cancel,actual_handoff;Gesture g{*point,0,offset};
    std::optional<Rect> actual_ready_geometry;
    product::guest::ActorStartCaptureReceipt capture_receipt{};
    product::guest::ActorNativeThresholdCommand threshold_command{};bool threshold_published=false;
    std::string packet_before_bytes,packet_before_hash;BY_HANDLE_FILE_INFORMATION packet_before_info{};bool packet_before_known=false;
    std::uint64_t packet_before_down_qpc{};
    try{
        if(disconnect){
            s.log.emit("I08_disconnect_no_inherited_completion_handles",
                "\"actor_PID\":"+std::to_string(child.dwProcessId)+",\"actor_birth\":"+std::to_string(child_birth)+
                ",\"inherited_handle_count\":0,\"actual_input_sender_PID\":"+std::to_string(child.dwProcessId)+
                ",\"source_owner_PID\":"+std::to_string(GetCurrentProcessId())+",\"product_driver_channel\":false,\"anonymous_pipe_used\":false");
        }
        const auto event_name=input_actor_start_event(s.run,GetCurrentProcessId(),process_birth(GetCurrentProcess()),child.dwProcessId);
        SetLastError(0);start_ack.value=CreateEventW(nullptr,FALSE,FALSE,event_name.c_str());const auto event_error=GetLastError();
        s.log.check("I08_actor_fresh_START_event",start_ack.value&&event_error!=ERROR_ALREADY_EXISTS&&child_birth,
            "\"event\":"+quote(utf8(event_name))+",\"CreateEvent_return\":"+(start_ack.value?"true":"false")+
            ",\"CreateEvent_error\":"+std::to_string(event_error)+",\"auto_reset_two_explicit_phases\":true,\"unknown_existing_event_not_signaled\":true");
        if(disconnect){
            SetLastError(ERROR_SUCCESS);transport.ready.value=CreateEventW(nullptr,TRUE,FALSE,(event_name+L".DisconnectReceiptReady").c_str());
            const DWORD ready_error=GetLastError();
            s.log.check("I08_disconnect_fresh_child_receipt_event",transport.ready.value&&ready_error!=ERROR_ALREADY_EXISTS,
                "\"CreateEvent_error\":"+std::to_string(ready_error)+",\"existing_event_not_reused\":true");
            SetLastError(ERROR_SUCCESS);transport.close_ack.value=CreateEventW(nullptr,TRUE,FALSE,(event_name+L".DisconnectNow").c_str());
            const DWORD close_error=GetLastError();
            s.log.check("I08_disconnect_fresh_completion_close_event",transport.close_ack.value&&close_error!=ERROR_ALREADY_EXISTS,
                "\"CreateEvent_error\":"+std::to_string(close_error)+
                ",\"independent_from_native_threshold_and_START_event\":true,\"existing_event_not_reused\":true");
        }
        const auto capture_name=event_name+L".CaptureReceipt";SetLastError(0);
        capture_mapping.value=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,
            static_cast<DWORD>(sizeof(product::guest::ActorCaptureHandshake)),capture_name.c_str());const auto mapping_error=GetLastError();
        if(capture_mapping.value&&mapping_error!=ERROR_ALREADY_EXISTS)
            capture_view.value=MapViewOfFile(capture_mapping.value,FILE_MAP_WRITE,0,0,sizeof(product::guest::ActorCaptureHandshake));
        s.log.check("I08_actor_fresh_capture_receipt",capture_mapping.value&&mapping_error!=ERROR_ALREADY_EXISTS&&capture_view.value,
            "\"mapping\":"+quote(utf8(capture_name))+",\"CreateFileMapping_error\":"+std::to_string(mapping_error)+
            ",\"parent_write_child_read_only\":true,\"unknown_existing_mapping_not_reused\":true");
        std::construct_at(static_cast<product::guest::ActorCaptureHandshake*>(capture_view.value));
        LARGE_INTEGER resume_before{};QueryPerformanceCounter(&resume_before);
        const DWORD resumed=ResumeThread(child_thread.value);const auto resume_error=resumed==static_cast<DWORD>(-1)?GetLastError():0;
        actor_resumed=resumed!=static_cast<DWORD>(-1)&&resumed<=1;
        s.log.check("I08_actor_resume_exact_child",resumed==1,"\"ResumeThread_previous_suspend_count\":"+std::to_string(resumed)+
            ",\"ResumeThread_error\":"+std::to_string(resume_error));
        bool ack_ready=false;
        (void)wait([&]{
            if(WaitForSingleObject(child_process.value,0)!=WAIT_TIMEOUT)return true;
            actual_start=record(s,offset,"gesture_start");g.generation=number(actual_start,"\"generation\":");
            actual_pending=record(s,offset,"native_move_pending");
            const auto pending_generation=number(actual_pending,"\"generation\":");
            if(!g.generation&&!threshold_published&&pending_generation){
                const auto down_qpc=number(actual_pending,"\"down_hook_qpc\":");const auto down_point="\"down_point\":["+std::to_string(point->x)+","+std::to_string(point->y)+"]";
                GUITHREADINFO gui{sizeof(gui)};POINT cursor_now{};
                const bool not_ended=actual_up(s,offset,pending_generation).empty()&&record(s,offset,"gesture_end",pending_generation).empty()&&
                    record(s,offset,"native_move_passthrough_release",pending_generation).empty()&&record(s,offset,"native_move_start",pending_generation).empty()&&
                    record(s,offset,"native_move_end",pending_generation).empty();
                const bool pending_exact=number(actual_pending,"\"source\":")==reinterpret_cast<std::uintptr_t>(source)&&
                    number(actual_pending,"\"source_pid\":")==GetCurrentProcessId()&&number(actual_pending,"\"source_tid\":")==set.thread&&
                    number(actual_pending,"\"source_key\":")==expected_key&&resume_before.QuadPart>0&&down_qpc>=static_cast<std::uint64_t>(resume_before.QuadPart)&&
                    actual_pending.find(down_point)!=actual_pending.npos&&actual_pending.find("\"down_consumed\":false")!=actual_pending.npos&&
                    actual_pending.find("\"write_authority\":false")!=actual_pending.npos;
                const bool source_safe=pending_exact&&not_ended&&desktop()&&set.owns(0)&&visible(source)==initial&&GetForegroundWindow()==source&&
                    GetCursorPos(&cursor_now)&&cursor_now.x==point->x&&cursor_now.y==point->y&&WindowFromPoint(cursor_now)==source&&
                    GetGUIThreadInfo(set.thread,&gui)&&(!gui.hwndCapture||gui.hwndCapture==source)&&!gui.hwndMoveSize&&!gui.hwndMenuOwner&&
                    !(gui.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE))&&
                    s.process&&GetProcessId(s.process)==product_pid&&WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&process_birth(s.process)==product_birth;
                if(source_safe){
                    const LONG drag_x=GetSystemMetrics(SM_CXDRAG),drag_y=GetSystemMetrics(SM_CYDRAG);
                    const long long left=GetSystemMetrics(SM_XVIRTUALSCREEN),right=left+GetSystemMetrics(SM_CXVIRTUALSCREEN);
                    const long long destination=static_cast<long long>(point->x)+drag_x+1<right?
                        static_cast<long long>(point->x)+drag_x+1:static_cast<long long>(point->x)-drag_x-1;
                    const auto product_hash=hash(s.product);
                    if(drag_x<0||drag_x>128||drag_y<0||drag_y>128||destination<left||destination>=right||
                        destination<LONG_MIN||destination>LONG_MAX||product_hash.size()!=64)
                        throw std::runtime_error("I08_actor_threshold_actual_metrics_or_product_hash_invalid");
                    threshold_command.version=1;threshold_command.parent_pid=GetCurrentProcessId();threshold_command.parent_birth=process_birth(GetCurrentProcess());
                    threshold_command.child_pid=child.dwProcessId;threshold_command.child_birth=child_birth;
                    threshold_command.product_pid=product_pid;threshold_command.product_birth=product_birth;
                    threshold_command.source=reinterpret_cast<std::uintptr_t>(source);threshold_command.source_pid=GetCurrentProcessId();threshold_command.source_tid=set.thread;
                    threshold_command.generation=pending_generation;threshold_command.source_key=expected_key;threshold_command.down_qpc=down_qpc;
                    threshold_command.anchor_x=point->x;threshold_command.anchor_y=point->y;threshold_command.destination_x=destination;threshold_command.destination_y=point->y;
                    threshold_command.drag_x=drag_x;threshold_command.drag_y=drag_y;
                    threshold_command.original_visible={initial->l,initial->t,initial->r,initial->b};threshold_command.no_up_start_or_end_observed=not_ended;
                    std::copy(product_hash.begin(),product_hash.end(),threshold_command.product_image_sha256.begin());
                    auto* shared=static_cast<product::guest::ActorCaptureHandshake*>(capture_view.value);shared->threshold=threshold_command;
                    shared->phase.store(product::guest::ActorCapturePhase::NativeThreshold,std::memory_order_release);
                    threshold_published=true;SetLastError(0);const bool signaled=SetEvent(start_ack.value)!=FALSE;const DWORD signal_error=signaled?0:GetLastError();
                    s.log.check("I08_actor_actual_native_pending_threshold_command_once",signaled,
                        "\"generation\":"+std::to_string(pending_generation)+",\"actual_native_pending\":"+quote(actual_pending)+
                        ",\"original_DOWN_anchor\":["+std::to_string(point->x)+","+std::to_string(point->y)+"],\"requested_threshold\":["+std::to_string(destination)+","+std::to_string(point->y)+"]"+
                        ",\"actual_source_capture\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndCapture))+
                        ",\"SetEvent_return\":"+(signaled?"true":"false")+",\"SetEvent_error\":"+std::to_string(signal_error)+
                        ",\"phase\":\"NativeThreshold_not_START_ACK\",\"actual_child_MOVE_not_yet_observed\":true,\"write_authority_inferred\":false");
                }
            }
            actual_down=record(s,offset,"input_button_observed",g.generation);
            actual_ready.clear();{const auto text=product_log_since(s,offset);std::istringstream rows(text);std::string line;
                while(std::getline(rows,line))if(number(line,"\"process_id\":")==product_pid&&number(line,"\"generation\":")==g.generation&&
                    line.find("\"source_sha\":"+quote(s.product_source))!=line.npos&&line.find("\"type\":\"gesture_input_resource\"")!=line.npos&&
                    line.find("\"fact\":\"capture_ready\"")!=line.npos)actual_ready=line;}
            POINT ready_point{};const bool ready_point_known=GetCursorPos(&ready_point)!=FALSE;
            const bool known_cursor=ready_point_known&&((ready_point.x==point->x&&ready_point.y==point->y)||
                (threshold_published&&ready_point.x==threshold_command.destination_x&&ready_point.y==threshold_command.destination_y));
            auto capture=release_capture_facts(s,source,known_cursor?GetAncestor(WindowFromPoint(ready_point),GA_ROOT):nullptr,g.generation);
            const auto down_qpc=number(actual_down,"\"hook_observed_qpc\":"),ready_qpc=number(actual_ready,"\"resource_observed_qpc\":");
            capture.ready_identity=capture.ready_identity&&down_qpc&&ready_qpc>=down_qpc;
            actual_native_start=record(s,offset,"native_move_start",g.generation);actual_native_end=record(s,offset,"native_move_end",g.generation);
            actual_cancel=record(s,offset,"native_move_cancel",g.generation);actual_handoff=record(s,offset,"native_move_handoff",g.generation);
            const bool real_native_end=pending_generation==g.generation&&number(actual_native_start,"\"source\":")==reinterpret_cast<std::uintptr_t>(source)&&
                number(actual_native_start,"\"event\":")==EVENT_SYSTEM_MOVESIZESTART&&number(actual_native_start,"\"event_tid\":")==set.thread&&
                number(actual_native_end,"\"source\":")==reinterpret_cast<std::uintptr_t>(source)&&number(actual_native_end,"\"event\":")==EVENT_SYSTEM_MOVESIZEEND&&
                number(actual_native_end,"\"event_tid\":")==set.thread&&number(actual_native_end,"\"owner_sequence\":")>number(actual_native_start,"\"owner_sequence\":")&&
                actual_cancel.find("\"native_attempted\":true")!=actual_cancel.npos&&actual_cancel.find("\"api_returned_success\":true")!=actual_cancel.npos&&
                actual_handoff.find("\"accepted\":true")!=actual_handoff.npos&&actual_handoff.find("\"stage\":\"actual_END_capture_pending\"")!=actual_handoff.npos;
            const bool no_up_or_end=actual_up(s,offset,g.generation).empty()&&record(s,offset,"gesture_end",g.generation).empty()&&
                record(s,offset,"native_move_passthrough_release",g.generation).empty()&&capture.not_retired;
            actual_ready_geometry=visible(source);
            const bool actual_geometry=actual_ready_geometry&&product::guest::actor_native_handoff_geometry_matches(actual_start,actual_handoff,product_pid,s.product_source,g.generation,
                {initial->l,initial->t,initial->r,initial->b},{actual_ready_geometry->l,actual_ready_geometry->t,actual_ready_geometry->r,actual_ready_geometry->b});
            ack_ready=g.generation&&number(actual_start,"\"process_id\":")==product_pid&&number(actual_start,"\"key\":")==expected_key&&
                actual_start.find("\"source_sha\":"+quote(s.product_source))!=actual_start.npos&&
                actual_down.find("\"source_sha\":"+quote(s.product_source))!=actual_down.npos&&
                actual_start.find("\"resize\":false")!=actual_start.npos&&actual_start.find("\"ctrl\":false")!=actual_start.npos&&
                actual_start.find("\"original\":"+rect(*initial))!=actual_start.npos&&number(actual_down,"\"process_id\":")==product_pid&&
                number(actual_down,"\"source_key\":")==expected_key&&number(actual_down,"\"button\":")==1&&
                actual_down.find("\"kind\":\"DOWN\"")!=actual_down.npos&&actual_down.find("\"source\":\"WH_MOUSE_LL\"")!=actual_down.npos&&
                number(actual_down,"\"hook_observed_qpc\":")&&set.owns(0)&&actual_geometry&&GetDpiForWindow(source)==initial_dpi&&
                MonitorFromWindow(source,MONITOR_DEFAULTTONULL)==initial_monitor&&GetForegroundWindow()==source&&
                no_up_or_end&&real_native_end&&known_cursor&&(!threshold_published||g.generation==threshold_command.generation)&&
                product::guest::captured_release_route_allowed(capture)&&process_birth(s.process)==product_birth;
            if(ack_ready){
                capture_receipt.version=2;capture_receipt.parent_pid=GetCurrentProcessId();capture_receipt.parent_birth=process_birth(GetCurrentProcess());
                capture_receipt.child_pid=child.dwProcessId;capture_receipt.child_birth=child_birth;capture_receipt.product_birth=product_birth;
                capture_receipt.down_qpc=down_qpc;capture_receipt.ready_qpc=ready_qpc;capture_receipt.capture=capture;
                capture_receipt.no_up_or_end_observed_before_ack=no_up_or_end;
            }
            return ack_ready;
        },5000);
        const auto product_hash=ack_ready?hash(s.product):std::string{};
        if(product_hash.size()==64)std::copy(product_hash.begin(),product_hash.end(),capture_receipt.product_image_sha256.begin());
        const bool capture_written=ack_ready&&product::guest::actor_start_capture_receipt_matches(capture_receipt,
            GetCurrentProcessId(),process_birth(GetCurrentProcess()),child.dwProcessId,child_birth);
        if(capture_written){auto* shared=static_cast<product::guest::ActorCaptureHandshake*>(capture_view.value);shared->ready=capture_receipt;
            shared->phase.store(product::guest::ActorCapturePhase::ActualStartReady,std::memory_order_release);}
        const bool ack_called=capture_written&&WaitForSingleObject(child_process.value,0)==WAIT_TIMEOUT;
        const bool ack_sent=ack_called&&SetEvent(start_ack.value)!=FALSE;const auto ack_error=ack_called&&!ack_sent?GetLastError():0;
        const DWORD child_wait_before_ack=WaitForSingleObject(child_process.value,0);DWORD child_exit_before_ack=STILL_ACTIVE;
        BOOL child_exit_code_known=FALSE;DWORD child_exit_query_error{};
        if(child_wait_before_ack==WAIT_OBJECT_0){SetLastError(ERROR_SUCCESS);child_exit_code_known=GetExitCodeProcess(child_process.value,&child_exit_before_ack);
            child_exit_query_error=child_exit_code_known?ERROR_SUCCESS:GetLastError();}
        s.log.check("I08_actor_actual_product_START_ack_once",ack_sent,
            "\"generation\":"+std::to_string(g.generation)+",\"expected_source_key\":"+std::to_string(expected_key)+
            ",\"held_child_wait_before_START_ack\":"+std::to_string(child_wait_before_ack)+
            ",\"child_exit_code_query_attempted\":"+(child_wait_before_ack==WAIT_OBJECT_0?"true":"false")+
            ",\"child_exit_code_query_return\":"+(child_wait_before_ack==WAIT_OBJECT_0?(child_exit_code_known?"true":"false"):"null")+
            ",\"child_exit_code_before_START_ack\":"+(child_exit_code_known?std::to_string(child_exit_before_ack):"null")+
            ",\"child_exit_code_query_error\":"+(child_wait_before_ack==WAIT_OBJECT_0&&!child_exit_code_known?std::to_string(child_exit_query_error):"null")+
            ",\"current_product_PID\":"+std::to_string(product_pid)+",\"actual_DOWN\":"+quote(actual_down)+",\"actual_START\":"+quote(actual_start)+
            ",\"actual_CaptureReady\":"+quote(actual_ready)+",\"immutable_capture_receipt_written\":"+(capture_written?"true":"false")+
            ",\"actual_native_START\":"+quote(actual_native_start)+",\"actual_native_END\":"+quote(actual_native_end)+
            ",\"actual_native_cancel\":"+quote(actual_cancel)+",\"actual_native_handoff\":"+quote(actual_handoff)+
            ",\"original_DOWN_intent_geometry\":"+rect(*initial)+",\"actual_ready_geometry\":"+(actual_ready_geometry?rect(*actual_ready_geometry):"null")+
            ",\"threshold_command_published\":"+(threshold_published?"true":"false")+
            ",\"actual_no_UP_or_END_observed_before_ack\":"+(capture_receipt.no_up_or_end_observed_before_ack?"true":"false")+
            ",\"capture_receipt_product_birth\":"+std::to_string(capture_receipt.product_birth)+",\"capture_receipt_product_sha256\":"+quote(product_hash)+
            ",\"capture_receipt_overlay\":"+std::to_string(capture_receipt.capture.overlay)+",\"capture_receipt_owner_tid\":"+std::to_string(capture_receipt.capture.overlay_tid)+
            ",\"capture_receipt_source_capture\":"+std::to_string(capture_receipt.capture.source_capture)+",\"capture_receipt_owner_capture\":"+std::to_string(capture_receipt.capture.owner_capture)+
            ",\"prepared_source\":"+quote(prepared)+",\"SetEvent_called\":"+(ack_called?"true":"false")+
            ",\"SetEvent_return\":"+(ack_sent?"true":"false")+",\"SetEvent_error\":"+std::to_string(ack_error)+
            ",\"ack_is_test_synchronization_not_product_authority\":true");
        if(disconnect){
            const bool announced=wait([&]{return WaitForSingleObject(transport.ready.value,0)==WAIT_OBJECT_0||
                WaitForSingleObject(child_process.value,0)==WAIT_OBJECT_0;},3500)&&WaitForSingleObject(transport.ready.value,0)==WAIT_OBJECT_0;
            InputActorDisconnectPacket packet{};DWORD bytes{};BOOL read_ok=FALSE;DWORD read_error{};
            bool opened=false,metadata_known=false;DWORD open_error{};LARGE_INTEGER size{};
            // Ready follows the child's real Write/Flush/Close. Retain one
            // read-only handle denying write/delete through the exit check.
            if(announced){SetLastError(ERROR_SUCCESS);transport.reader.value=CreateFileW(transport.path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,
                    OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
                opened=transport.reader.value!=INVALID_HANDLE_VALUE;open_error=opened?ERROR_SUCCESS:GetLastError();
                metadata_known=opened&&GetFileType(transport.reader.value)==FILE_TYPE_DISK&&GetFileInformationByHandle(transport.reader.value,&packet_before_info)&&
                    !(packet_before_info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))&&
                    GetFileSizeEx(transport.reader.value,&size)&&size.QuadPart==sizeof(packet);
                if(metadata_known){SetLastError(ERROR_SUCCESS);read_ok=ReadFile(transport.reader.value,&packet,sizeof(packet),&bytes,nullptr);
                    read_error=read_ok?ERROR_SUCCESS:GetLastError();}
                if(read_ok&&bytes==sizeof(packet)){packet_before_bytes.assign(reinterpret_cast<const char*>(&packet),sizeof(packet));packet_before_hash=hash(transport.path);
                    packet_before_down_qpc=packet.down_qpc;}
            }
            packet_before_known=metadata_known&&read_ok&&bytes==sizeof(packet)&&packet_before_hash.size()==64;
            const bool bound=announced&&packet_before_known&&packet.version==1&&
                packet.parent_pid==GetCurrentProcessId()&&packet.parent_birth==process_birth(GetCurrentProcess())&&
                packet.child_pid==child.dwProcessId&&packet.child_birth==child_birth&&packet.source==reinterpret_cast<std::uintptr_t>(source)&&
                packet.source_tid==set.thread&&packet.generation==g.generation&&packet.down_qpc&&resume_before.QuadPart>0&&
                packet.down_qpc>=static_cast<std::uint64_t>(resume_before.QuadPart)&&capture_receipt.down_qpc>=packet.down_qpc&&
                packet.down_inserted==1&&packet.move_inserted==1&&packet.total_inserted>=2&&packet.total_inserted<=3;
            const Rect expected=translated(*initial,-40,35);std::string exact;
            const bool active_exact=bound&&wait([&]{exact=placement(s,offset,g.generation,expected,true);return
                WaitForSingleObject(child_process.value,0)==WAIT_TIMEOUT&&set.owns(0)&&visible(source)==expected&&
                GetForegroundWindow()==source&&actual_up(s,offset,g.generation).empty()&&record(s,offset,"gesture_end",g.generation).empty()&&
                exact.find("\"authority_after\":true")!=exact.npos;},3000);
            const auto actual=visible(source);
            s.log.check("I08_disconnect_real_input_child_active_exact_before_transport_close",active_exact,
                "\"actor_PID\":"+std::to_string(child.dwProcessId)+",\"actor_birth\":"+std::to_string(child_birth)+
                ",\"packet_ready_observed\":"+(announced?"true":"false")+",\"packet_CreateFile_attempted\":"+(announced?"true":"false")+
                ",\"packet_CreateFile_return\":"+(announced?(opened?"true":"false"):"null")+
                ",\"packet_CreateFile_error\":"+(announced&&!opened?std::to_string(open_error):"null")+
                ",\"packet_regular_nonreparse_exact96\":"+(metadata_known?"true":"false")+
                ",\"packet_ReadFile_attempted\":"+(metadata_known?"true":"false")+",\"packet_ReadFile_return\":"+(metadata_known?(read_ok?"true":"false"):"null")+
                ",\"packet_bytes\":"+std::to_string(bytes)+",\"packet_read_error\":"+(metadata_known&&!read_ok?std::to_string(read_error):"null")+
                ",\"packet_sha256_before_actor_exit\":"+(packet_before_hash.empty()?"null":quote(packet_before_hash))+
                ",\"packet_identity_matches\":"+(bound?"true":"false")+",\"actual_sender_PID\":"+std::to_string(packet.child_pid)+
                ",\"actual_generation\":"+std::to_string(g.generation)+",\"independent_actual_geometry\":"+
                (actual?rect(*actual):"null")+",\"actual_native_receipt\":"+quote(exact)+
                ",\"child_UP_calls\":0,\"product_driver_channel\":false");
            s.log.check("I08_disconnect_actual_exact_Move_ack_then_child_disconnect",SetEvent(transport.close_ack.value)!=FALSE,
                "\"child_input_already_observed\":true,\"ack_not_new_product_authority\":true");
        }
        const bool exited=wait([&]{return WaitForSingleObject(child_process.value,0)==WAIT_OBJECT_0;},5000);DWORD code=STILL_ACTIVE;
        if(!exited)(void)stop_test_actor();
        if(exited)GetExitCodeProcess(child_process.value,&code);const auto child_exit_tick=GetTickCount64();
        if(disconnect){
            std::array<char,sizeof(InputActorDisconnectPacket)> after_bytes{};DWORD bytes{};BOOL read_ok=FALSE;DWORD error{};
            LARGE_INTEGER zero{},size{};BY_HANDLE_FILE_INFORMATION after_info{};bool after_metadata_known=false,rewound=false;
            if(exited&&packet_before_known){after_metadata_known=GetFileInformationByHandle(transport.reader.value,&after_info)&&
                    !(after_info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))&&GetFileSizeEx(transport.reader.value,&size)&&
                    size.QuadPart==sizeof(InputActorDisconnectPacket);
                rewound=SetFilePointerEx(transport.reader.value,zero,nullptr,FILE_BEGIN)!=FALSE;
                if(rewound){SetLastError(ERROR_SUCCESS);read_ok=ReadFile(transport.reader.value,after_bytes.data(),static_cast<DWORD>(after_bytes.size()),&bytes,nullptr);
                    error=read_ok?ERROR_SUCCESS:GetLastError();}}
            const auto after_hash=exited&&after_metadata_known&&read_ok&&bytes==after_bytes.size()?hash(transport.path):std::string{};
            const bool same_file=after_metadata_known&&after_info.dwVolumeSerialNumber==packet_before_info.dwVolumeSerialNumber&&
                after_info.nFileIndexHigh==packet_before_info.nFileIndexHigh&&after_info.nFileIndexLow==packet_before_info.nFileIndexLow;
            const bool immutable=packet_before_known&&same_file&&read_ok&&bytes==after_bytes.size()&&
                std::string(after_bytes.data(),after_bytes.size())==packet_before_bytes&&after_hash==packet_before_hash;
            s.log.check("I08_disconnect_exact_input_actor_exit_and_packet_immutable",exited&&code==0&&child_birth&&
                process_birth(child_process.value)==child_birth&&immutable,
                "\"actor_PID\":"+std::to_string(child.dwProcessId)+",\"actor_birth\":"+std::to_string(child_birth)+
                ",\"held_process_exit_observed\":"+(exited?"true":"false")+",\"exit_code\":"+std::to_string(code)+
                ",\"same_held_packet_file\":"+(same_file?"true":"false")+",\"packet_rewind_return\":"+(rewound?"true":"false")+
                ",\"after_ReadFile_attempted\":"+(rewound?"true":"false")+",\"after_ReadFile_return\":"+(rewound?(read_ok?"true":"false"):"null")+
                ",\"after_bytes\":"+std::to_string(bytes)+",\"after_ReadFile_error\":"+(rewound&&!read_ok?std::to_string(error):"null")+
                ",\"packet_sha256_before_exit\":"+quote(packet_before_hash)+",\"packet_sha256_after_exit\":"+(after_hash.empty()?"null":quote(after_hash))+
                ",\"packet_bytes_and_hash_unchanged\":"+(immutable?"true":"false")+
                ",\"hard_TerminateProcess_scope\":\"NOT_RUN\",\"actor_no_UP_checked_in_following_receipt\":true,\"normal_orderly_UP_disconnect_scope\":\"NOT_RUN\"");
        }
        if(disconnect){
            const bool packet_closed=CloseHandle(transport.reader.value)!=FALSE;
            if(packet_closed)transport.reader.value=nullptr;
            const bool ready_closed=CloseHandle(transport.ready.value)!=FALSE;
            if(ready_closed)transport.ready.value=nullptr;
            const bool close_ack_closed=CloseHandle(transport.close_ack.value)!=FALSE;
            if(close_ack_closed)transport.close_ack.value=nullptr;
            s.log.check("I08_disconnect_parent_transport_handles_closed",packet_closed&&ready_closed&&close_ack_closed,
                "\"packet_reader_CloseHandle_return\":"+std::string(packet_closed?"true":"false")+
                ",\"ready_event_CloseHandle_return\":"+(ready_closed?"true":"false")+
                ",\"close_event_CloseHandle_return\":"+(close_ack_closed?"true":"false")+",\"packet_retained_as_evidence\":true,\"product_resources_not_inferred\":true");
        }
        const auto actor=read(receipt,disconnect?16384:8192);
        const bool actor_identity=actor.ends_with("}\n")&&actor.find("\"UP_sent\":false")!=std::string::npos&&
            actor.find("\"source_sha\":\"" PANEBIND_BUILD_SHA "\"")!=std::string::npos&&actor.find("\"SendInput_return\":")!=std::string::npos&&
            number(actor,"\"parent_pid\":")==GetCurrentProcessId()&&number(actor,"\"pid\":")==child.dwProcessId&&
            number(actor,"\"source_hwnd\":")==reinterpret_cast<std::uintptr_t>(source)&&number(actor,"\"before_qpc\":");
        actor_send_count_known=actor_identity;actor_inserted=actor_identity?number(actor,"\"SendInput_return\":"):0;
        const bool actor_down_inserted=actor_identity&&number(actor,"\"DOWN_SendInput_return\":")==1;
        if(disconnect)s.log.check("I08_disconnect_actual_child_packet_completion_receipt",actor_identity&&
            actor.find("\"packet_CREATE_NEW_return\":true")!=actor.npos&&actor.find("\"packet_WriteFile_return\":true")!=actor.npos&&
            number(actor,"\"packet_bytes_written\":")==sizeof(InputActorDisconnectPacket)&&
            actor.find("\"packet_FlushFileBuffers_return\":true")!=actor.npos&&actor.find("\"packet_CloseHandle_return\":true")!=actor.npos&&
            actor.find("\"packet_ready_signaled\":true")!=actor.npos&&actor.find("\"packet_sha256_after_close\":"+quote(packet_before_hash))!=actor.npos&&
            number(actor,"\"before_qpc\":")==packet_before_down_qpc&&number(actor,"\"own_UP_calls\":")==0,
            "\"actual_child_receipt\":"+quote(actor)+",\"completion_file_and_input_sender_same_child\":true,\"product_driver_notification\":false");
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
        const auto threshold_calls=number(actor,"\"own_threshold_MOVE_calls\":"),threshold_successes=number(actor,"\"threshold_MOVE_SendInput_return\":");
        const bool actual_moves=number(actor,"\"own_post_READY_intent_MOVE_calls\":")==1&&number(actor,"\"MOVE_SendInput_return\":")==1&&
            threshold_calls<=1&&threshold_successes==threshold_calls&&actor_inserted==2+threshold_successes&&
            (!threshold_calls||(actor.find("\"threshold_command_identity\":true")!=actor.npos&&actor.find("\"threshold_actual_context\":true")!=actor.npos&&
                number(actor,"\"threshold_command_generation\":")==g.generation&&actor.find("\"threshold_actual_cursor_exact\":true")!=actor.npos));
        s.log.check("I08_input_actor_exited_without_UP_source_remains",exited&&code==0&&actor_down_inserted&&actual_moves&&new_pair&&s.left&&set.owns(0)&&GetForegroundWindow()==source,
            "\"child_pid\":"+std::to_string(child.dwProcessId)+",\"exit_code\":"+std::to_string(code)+",\"child_exit_tick\":"+std::to_string(child_exit_tick)+
            ",\"source_still_alive\":"+(set.owns(0)?"true":"false")+",\"child_DOWN_insertion_confirmed\":"+
            (actor_down_inserted?"true":"null")+",\"child_SendInput_return\":"+(actor_send_count_known?std::to_string(actor_inserted):"null")+
            ",\"actual_threshold_MOVE_calls\":"+std::to_string(threshold_calls)+",\"actual_threshold_MOVE_insertions\":"+std::to_string(threshold_successes)+
            ",\"actual_post_READY_intent_MOVE_calls\":"+std::to_string(number(actor,"\"own_post_READY_intent_MOVE_calls\":"))+
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
        // A foreground event can precede the GUI's real terminal receipt.
        // Observe that receipt once within a finite bound; never infer End2
        // or a root cause from the foreground change itself.
        if(disconnect&&ended.empty())(void)wait([&]{ended=record(s,offset,"gesture_end",g.generation);return !ended.empty();},1500);
        const auto before_tick=number(actor,"\"before_tick\":"),end_tick=number(ended,"\"tick\":");
        const bool actual_deadline=terminal&&set.owns(0)&&GetForegroundWindow()==source&&s.left&&actual_up(s,offset,g.generation).empty()&&
            number(ended,"\"end\":")==static_cast<std::uint64_t>(v::End::Deadline)&&ended.find("\"normal_up\":false")!=std::string::npos&&
            ended.find("\"pending_retired\":true")!=std::string::npos&&before_tick&&end_tick>=before_tick&&end_tick-before_tick>=30000;
        namespace e=product::guest::up_evidence_detail;
        const bool interrupted_focus=disconnect&&geometry_history_receipt(s,ended,"gesture_end")&&
            e::number(ended,"\"generation\":")==g.generation&&e::number(ended,"\"end\":")==static_cast<std::uint64_t>(v::End::FocusLost)&&
            e::unique_value(ended,"\"normal_up\":")=="false"&&e::unique_value(ended,"\"pending_retired\":")=="true";
        const std::string deadline_fields="\"generation\":"+std::to_string(g.generation)+",\"configured_production_deadline_ms\":30000,\"actor_before_SendInput_tick\":"+
            std::to_string(before_tick)+",\"actual_end_tick\":"+std::to_string(end_tick)+",\"actual_elapsed_ms\":"+
            (end_tick>=before_tick?std::to_string(end_tick-before_tick):"null")+",\"actual_end\":"+quote(ended)+
            ",\"driver_left_outstanding\":"+(s.left?"true":"false")+",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+
            ",\"physical_button_held_not_inferred\":true,\"driver_notification_not_fabricated\":true";
        if(interrupted_focus){
            s.log.failed=true; // The original 30-second assertion still FAILS.
            s.log.emit("case","\"case\":\"I08_input_actor_exit_real_default_30s_Deadline\",\"result\":\"FAIL\","+deadline_fields);
            s.log.emit("I08_disconnect_terminal_classification","\"status\":\"ACTUAL_FOCUS_LOST_DEFAULT_DEADLINE_NOT_OBSERVED\",\"initiator_or_root_cause\":\"UNKNOWN\","
                "\"recovery\":\"NOT_RUN\",\"failure_preserved\":true,\"new_input_authority\":false");
        }else s.log.check("I08_input_actor_exit_real_default_30s_Deadline",actual_deadline,deadline_fields);
        const auto calls=[&]{const auto rows=constraint_generation_placements(s,offset,g.generation);return std::count_if(rows.begin(),rows.end(),[](const auto& row){return row.find("\"native_callsite_claimed\":true")!=std::string::npos;});};
        const auto baseline=calls();const auto barrier=constraint_last_product_sequence(s);
        if(!disconnect)cursor(s,{point->x-11,point->y+47});
        const auto status_offset=read_product(s).size();
        s.log.check(interrupted_focus?"I08_actor_FocusLost_no_new_native_calls":"I08_actor_Deadline_no_new_native_calls",launch(s,L"--status",true)&&wait([&]{return constraint_current_status_after(s,status_offset,barrier);})&&
            calls()==baseline&&baseline>0&&visible(source)==moved);
        // This UP is a test cleanup action after actual termination, not product
        // NormalUp, a replay, or a product recovery feature.
        if(disconnect){cleanup_checked=true;s.log.check("I08_disconnect_single_supervisor_abort_UP_observed",
            input_actor_disconnect_cleanup_up(s,set,offset,g.generation,test_up));}
        else{button(s,false);test_up=true;}
        s.log.check("I08_actor_test_UP_sent_and_native_resources_idle",wait([&]{GUITHREADINFO gui{sizeof(gui)};return !s.left&&
            set.owns(0)&&GetGUIThreadInfo(set.thread,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize;})&&calls()==baseline&&
            record(s,offset,"gesture_end",g.generation).find("\"normal_up\":false")!=std::string::npos,
            "\"actual_matching_product_UP\":"+(actual_up(s,offset,g.generation).empty()?"null":quote(actual_up(s,offset,g.generation)))+
            ",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+",\"UP_delivery_not_inferred\":true,\"not_NormalUp\":true");
        interruption_idle(s,set,true);chain_check_product(s,product_pid,product_birth);interruption_recovery(s,set,g.generation,disconnect);
        chain_check_product(s,product_pid,product_birth);
        s.log.emit("I08_input_actor_exit_scope","\"status\":"+quote(interrupted_focus?"PARTIAL_ACTOR_EXIT_FOCUS_LOST_ABORT_UP_AND_RECOVERY_OBSERVED":"INPUT_ACTOR_EXIT_DEADLINE_AND_RECOVERY_OBSERVED")+
            ",\"default_30s_Deadline_observed\":"+(actual_deadline?"true":"false")+",\"original_failure_preserved\":"+(interrupted_focus?"true":"false")+",\"source_destroyed\":false,\"product_driver_channel\":false,"
            "\"actual_main_fixture_test_UP\":true,\"same_product_new_Move_Resize\":true,\"whole_I08_PASS\":false,\"human_uat\":\"NOT_RUN\""+
            std::string(disconnect?",\"actual_input_child_exit_and_packet_immutability_observed\":true,\"recovery_input_sender\":\"original_supervisor\",\"new_input_actor_Move_Resize\":\"NOT_RUN\"":""));
    }catch(...){
        // The child has only the bounded START acknowledgement/readback waits.
        // Keep desktop exposure bounded
        // even when its evidence/start/deadline fails, never turn cleanup into
        // a functional PASS. No product process or source is terminated here.
        try{const bool actor_gone=stop_test_actor();
            // A new verified single DOWN can survive an ack/MOVE failure or
            // a failed final receipt write. Never infer it from async state.
            const auto failed_actor=read(receipt,disconnect?16384:8192);
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
            if(disconnect&&!cleanup_checked&&!test_up&&actor_down_possible){
                const bool observed=input_actor_disconnect_cleanup_up(s,set,offset,generation,test_up);
                s.log.emit("I08_disconnect_failed_scope_cleanup","\"single_test_UP_attempted\":"+std::string(test_up?"true":"false")+
                    ",\"actual_abort_UP_observed\":"+(observed?"true":"false")+",\"functional_PASS\":false");
            }else if(!disconnect&&!test_up&&actor_down_possible){
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
void owned_input_actor_exit(State& s,LayoutFixtures& set,DWORD product_pid,std::uint64_t product_birth){
    owned_input_actor_exit_transport(s,set,product_pid,product_birth,false);
}
