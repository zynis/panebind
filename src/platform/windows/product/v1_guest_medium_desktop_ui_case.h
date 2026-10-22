#pragma once

// TEST ONLY, included inside the guarded fixture namespace. This is one
// ordinary-GUI/tray case on a fresh Run desktop, not product target authority
// or an alternative input route. Existing Default guards stay untouched.
bool input_actor_same_user(HANDLE);

struct MediumDesktopUiInputLease {
    static constexpr unsigned in_flight=1,retired_bit=2;
    std::atomic<unsigned> flags{};
    std::atomic<bool> incomplete_pair{};
    HANDLE input_done{CreateEventW(nullptr,TRUE,TRUE,nullptr)};
    ~MediumDesktopUiInputLease(){if(input_done)CloseHandle(input_done);}
    bool valid()const noexcept{return input_done!=nullptr;}
    bool retired()const noexcept{return (flags.load(std::memory_order_acquire)&retired_bit)!=0;}
    bool begin()noexcept{
        if(!input_done)return false;
        // Reset before claiming: retirement can conservatively see an idle
        // unsignalled event, but cannot miss an in-flight native call.
        if(!ResetEvent(input_done))return false;unsigned idle=0;
        return flags.compare_exchange_strong(idle,in_flight,std::memory_order_acq_rel);
    }
    void end()noexcept{flags.fetch_and(~in_flight,std::memory_order_release);if(input_done)SetEvent(input_done);}
    bool retire_and_wait(DWORD budget_ms)noexcept{
        const auto previous=flags.fetch_or(retired_bit,std::memory_order_acq_rel);
        if(previous&in_flight){if(!input_done||WaitForSingleObject(input_done,budget_ms)!=WAIT_OBJECT_0)return false;}
        return !(flags.load(std::memory_order_acquire)&in_flight)&&!incomplete_pair.load(std::memory_order_acquire);
    }
};
struct MediumDesktopUiRequest {fs::path product;std::string source,sha256;};
struct MediumDesktopUiObservation {
    std::vector<std::pair<std::string,std::string>> records;
    bool launch_attempted{},product_started{},product_closed{},completed{},input_attempted{},input_sent{},cleanup_known{};
    std::string reason{"not_started"};
    void emit(std::string type,std::string fields){records.emplace_back(std::move(type),std::move(fields));}
};
struct MediumDesktopUiEvents {HANDLE changed{};DWORD product_pid{};};
thread_local MediumDesktopUiEvents* medium_desktop_ui_events{};
void CALLBACK medium_desktop_ui_event(HWINEVENTHOOK,DWORD,HWND window,LONG,LONG,DWORD,DWORD){
    const auto* owner=medium_desktop_ui_events;DWORD pid{};
    if(owner&&owner->changed&&window&&GetWindowThreadProcessId(window,&pid)&&pid==owner->product_pid)SetEvent(owner->changed);
}

class MediumDesktopUiCase {
    const std::wstring& run_;const HDESK desktop_;const fs::path& product_;const std::string source_,sha_;
    const std::function<bool()>& still_exact_;const std::shared_ptr<MediumDesktopUiInputLease>& lease_;
    const ULONGLONG deadline_;MediumDesktopUiObservation& out_;
    HANDLE process_{},initial_thread_{};DWORD pid_{},session_{};std::uint64_t birth_{};
    HWND app_{},shell_{};DWORD app_tid_{},shell_pid_{};std::uint64_t shell_birth_{};
    MediumDesktopUiEvents events_;HWINEVENTHOOK objects_{},foreground_{};bool com_{},quit_seen_{},quit_attempted_{};

    std::wstring name(HANDLE object)const{
        std::array<wchar_t,128> text{};DWORD size{};
        return object&&GetUserObjectInformationW(object,UOI_NAME,text.data(),sizeof(text),&size)?std::wstring(text.data()):std::wstring{};
    }
    std::wstring type(HWND window)const{
        std::array<wchar_t,128> text{};const int size=GetClassNameW(window,text.data(),static_cast<int>(text.size()));
        return size>0?std::wstring(text.data(),size):std::wstring{};
    }
    bool input_context(bool after_retire=false)const{
        if(!lease_||!lease_->valid()||(!after_retire&&lease_->retired())||GetTickCount64()>=deadline_||
           !medium(GetCurrentProcess())||!equal(name(GetProcessWindowStation()),L"WinSta0")||
           !equal(name(GetThreadDesktop(GetCurrentThreadId())),L"PaneBindV1_"+run_)||
           !equal(name(desktop_),L"PaneBindV1_"+run_)||!still_exact_||!still_exact_())return false;
        const HDESK input=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);if(!input)return false;
        BOOL active{};DWORD size{};const bool ok=equal(name(input),L"PaneBindV1_"+run_)&&
            GetUserObjectInformationW(input,UOI_IO,&active,sizeof(active),&size)&&active;
        const bool closed=CloseDesktop(input)!=FALSE;return ok&&closed;
    }
    bool process_exact()const{
        if(!process_||!pid_||!birth_||GetProcessId(process_)!=pid_||WaitForSingleObject(process_,0)!=WAIT_TIMEOUT||
           process_birth(process_)!=birth_||!medium(process_)||!input_actor_same_user(process_))return false;
        DWORD session{};std::array<wchar_t,32768> image{};DWORD size=static_cast<DWORD>(image.size());
        return ProcessIdToSessionId(pid_,&session)&&session==session_&&
            QueryFullProcessImageNameW(process_,0,image.data(),&size)&&size&&size<image.size()&&
            equal(std::wstring(image.data(),size),product_.wstring());
    }
    bool member(HWND window)const{
        if(!window||!IsWindow(window)||!equal(name(desktop_),L"PaneBindV1_"+run_))return false;
        struct Search {HWND wanted{};unsigned seen{},count{};bool overflow{};} search{window};
        SetLastError(ERROR_SUCCESS);const bool complete=EnumDesktopWindows(desktop_,[](HWND h,LPARAM value)->BOOL{
            auto& state=*reinterpret_cast<Search*>(value);if(++state.count>256){state.overflow=true;return FALSE;}
            if(h==state.wanted)++state.seen;return TRUE;
        },reinterpret_cast<LPARAM>(&search))!=FALSE;
        return complete&&!search.overflow&&search.seen==1&&equal(name(desktop_),L"PaneBindV1_"+run_);
    }
    bool app_exact()const{
        DWORD pid{};return process_exact()&&app_&&app_tid_&&member(app_)&&type(app_)==L"PaneBind.V1.Application"&&
            GetWindowThreadProcessId(app_,&pid)==app_tid_&&pid==pid_&&GetAncestor(app_,GA_ROOT)==app_;
    }
    bool product_window(HWND window,bool visible=true)const{
        DWORD pid{};return app_exact()&&window&&GetWindowThreadProcessId(window,&pid)==app_tid_&&pid==pid_&&
            (!visible||IsWindowVisible(window))&&member(GetAncestor(window,GA_ROOT));
    }
    DWORD remaining(DWORD phase)const{
        const auto now=GetTickCount64();return now<deadline_?static_cast<DWORD>((std::min<ULONGLONG>)(phase,deadline_-now)):0;
    }
    bool wait_for(const std::function<bool()>& predicate,DWORD phase_ms){
        const auto phase_end=GetTickCount64()+remaining(phase_ms);
        for(;;){
            if(quit_seen_||!input_context())return false;
            if(predicate())return true;
            const auto now=GetTickCount64();if(now>=phase_end)return false;
            ResetEvent(events_.changed);
            const DWORD waited=MsgWaitForMultipleObjectsEx(1,&events_.changed,static_cast<DWORD>(phase_end-now),QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(waited==WAIT_FAILED)return false;
            if(waited==WAIT_OBJECT_0+1){
                MSG message{};unsigned messages=256;while(messages--&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
                    if(message.message==WM_QUIT){quit_seen_=true;return false;}TranslateMessage(&message);DispatchMessageW(&message);
                }
            }
            if(waited==WAIT_TIMEOUT)return input_context()&&predicate();
        }
    }
    bool no_buttons()const{
        for(const int key:{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2,VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN})
            if(GetAsyncKeyState(key)&0x8000)return false;return true;
    }
    void send(INPUT* packets,UINT count,const std::function<bool()>& target,const char* purpose,bool pair){
        if(!input_context()||!target()||!lease_->begin())throw std::runtime_error("Medium_UI_input_fresh_guard_or_lease_rejected");
        struct End {MediumDesktopUiInputLease& lease;~End(){lease.end();}} end{*lease_};
        if(!input_context()||!target())throw std::runtime_error("Medium_UI_input_changed_after_lease_claim");
        if(pair)lease_->incomplete_pair.store(true,std::memory_order_release);
        for(UINT n=0;n<count;++n){if(packets[n].type==INPUT_MOUSE)packets[n].mi.dwExtraInfo=0x50425631;
            else packets[n].ki.dwExtraInfo=0x50425631;}
        const auto begin=GetTickCount64();out_.input_attempted=true;SetLastError(ERROR_SUCCESS);const auto inserted=SendInput(count,packets,sizeof(INPUT));
        const auto error=inserted==count?0:GetLastError();out_.input_sent=out_.input_sent||inserted>0;
        out_.emit("medium_desktop_ui_input","\"purpose\":"+quote(purpose)+",\"packet_count\":"+std::to_string(count)+
            ",\"SendInput_return\":"+std::to_string(inserted)+",\"error_on_short_insert\":"+std::to_string(error)+
            ",\"before_tick\":"+std::to_string(begin)+",\"after_tick\":"+std::to_string(GetTickCount64())+
            ",\"automated_guest_driver\":true,\"API_insert_is_not_message_delivery_proof\":true,\"Default_input_helpers_used\":false");
        if(pair&&(inserted==count||inserted==0))lease_->incomplete_pair.store(false,std::memory_order_release);
        // A short insert is a TEST input failure. Release only this one known
        // pair while its in-flight lease still excludes return. Never inject
        // into Default or invent product NormalUp evidence.
        if(pair&&inserted==1&&input_context(true)&&target()){
            SetLastError(ERROR_SUCCESS);const UINT released=SendInput(1,&packets[count-1],sizeof(INPUT));const auto release_error=released==1?0:GetLastError();
            if(released==1)lease_->incomplete_pair.store(false,std::memory_order_release);
            out_.emit("medium_desktop_ui_short_insert_cleanup","\"actual_own_pair_UP_attempted\":true,\"SendInput_return\":"+
                std::to_string(released)+",\"error_on_failure\":"+std::to_string(release_error)+",\"NormalUp_PASS_inferred\":false");
        }
        if(inserted!=count)throw std::runtime_error("Medium_UI_SendInput_short_insert");
    }
    void cursor(POINT point,const std::function<bool()>& target){
        const int left=GetSystemMetrics(SM_XVIRTUALSCREEN),top=GetSystemMetrics(SM_YVIRTUALSCREEN),width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if(width<2||height<2||point.x<left||point.y<top||point.x>=left+width||point.y>=top+height)throw std::runtime_error("Medium_UI_cursor_out_of_virtual_screen");
        INPUT packet{};packet.type=INPUT_MOUSE;packet.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
        packet.mi.dx=static_cast<LONG>(((static_cast<long long>(point.x-left)*2+1)*65536)/(static_cast<long long>(width)*2));
        packet.mi.dy=static_cast<LONG>(((static_cast<long long>(point.y-top)*2+1)*65536)/(static_cast<long long>(height)*2));
        send(&packet,1,target,"exact_UI_cursor",false);POINT actual{};
        if(!wait_for([&]{return target()&&GetCursorPos(&actual)&&actual.x==point.x&&actual.y==point.y;},200))
            throw std::runtime_error("Medium_UI_actual_cursor_not_exact");
        out_.emit("medium_desktop_ui_cursor","\"requested\":["+std::to_string(point.x)+","+std::to_string(point.y)+
            "],\"actual\":["+std::to_string(actual.x)+","+std::to_string(actual.y)+"],\"actual_GetCursorPos_readback\":true");
    }
    void click(bool right,POINT point,const std::function<bool()>& target){
        cursor(point,target);if(!no_buttons())throw std::runtime_error("Medium_UI_unexpected_pressed_button_or_modifier");
        INPUT pair[2]{};for(auto& packet:pair)packet.type=INPUT_MOUSE;
        pair[0].mi.dwFlags=right?MOUSEEVENTF_RIGHTDOWN:MOUSEEVENTF_LEFTDOWN;
        pair[1].mi.dwFlags=right?MOUSEEVENTF_RIGHTUP:MOUSEEVENTF_LEFTUP;
        send(pair,2,target,right?"exact_product_icon_RIGHTDOWN_RIGHTUP":"exact_product_menu_LEFTDOWN_LEFTUP",true);
    }
    HWND settings()const{
        struct Search {const MediumDesktopUiCase* self{};HWND found{};unsigned count{};} state{this};
        const bool enumerated=EnumThreadWindows(app_tid_,[](HWND h,LPARAM value)->BOOL{auto& state=*reinterpret_cast<Search*>(value);
            if(state.self->product_window(h)&&state.self->type(h)==L"PaneBindV1SettingsWindow"&&GetWindow(h,GW_OWNER)==state.self->app_){state.found=h;++state.count;}return TRUE;
        },reinterpret_cast<LPARAM>(&state))!=FALSE;return enumerated&&state.count==1?state.found:nullptr;
    }
    void escape_settings(HWND window,const char* phase){
        const auto target=[&]{GUITHREADINFO gui{sizeof(gui)};return product_window(window)&&settings()==window&&GetForegroundWindow()==window&&
            GetGUIThreadInfo(app_tid_,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner&&gui.hwndFocus&&
            (gui.hwndFocus==window||IsChild(window,gui.hwndFocus));};
        if(!target()||!no_buttons())throw std::runtime_error("Medium_UI_settings_keyboard_target_unknown");
        INPUT pair[2]{};for(auto& packet:pair){packet.type=INPUT_KEYBOARD;packet.ki.wVk=VK_ESCAPE;}pair[1].ki.dwFlags=KEYEVENTF_KEYUP;
        send(pair,2,target,phase,true);
        if(!wait_for([&]{return app_exact()&&!IsWindow(window)&&!settings();},1200))throw std::runtime_error("Medium_UI_settings_real_Escape_not_closed");
        out_.emit("medium_desktop_ui_settings_Escape","\"phase\":"+quote(phase)+",\"actual_settings_HWND\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(window))+",\"window_destroy_observed\":true,\"WM_CLOSE_or_WM_COMMAND_injected\":false");
    }
    struct Menu {HWND window{};HMENU handle{};};
    bool menu_exact(const Menu& menu)const{
        GUITHREADINFO gui{sizeof(gui)};MENUBARINFO bar{sizeof(bar)};
        return product_window(menu.window)&&type(menu.window)==L"#32768"&&GetForegroundWindow()==app_&&
            GetGUIThreadInfo(app_tid_,&gui)&&gui.hwndMenuOwner==app_&&(gui.flags&GUI_INMENUMODE)&&
            GetMenuBarInfo(menu.window,OBJID_CLIENT,0,&bar)&&bar.hMenu==menu.handle;
    }
    Menu open_menu(){
        NOTIFYICONIDENTIFIER icon{sizeof(icon)};icon.hWnd=app_;icon.uID=1;RECT area{};
        const HRESULT queried=Shell_NotifyIconGetRect(&icon,&area);
        const bool nonempty=queried==S_OK&&area.left<area.right&&area.top<area.bottom;
        out_.emit("medium_desktop_ui_product_icon","\"actual_product_PID\":"+std::to_string(pid_)+",\"owner_HWND\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(app_))+",\"UID\":1,\"GetRect_hresult\":"+
            std::to_string(static_cast<long>(queried))+",\"actual_rect\":"+(nonempty?rect({area.left,area.top,area.right,area.bottom}):"null"));
        if(!nonempty)throw std::runtime_error("Medium_UI_exact_product_UID1_icon_not_reachable");
        const POINT point{area.left+(area.right-area.left)/2,area.top+(area.bottom-area.top)/2};
        const auto target=[&]{
            RECT fresh{};DWORD hit_pid{};const HWND hit=WindowFromPoint(point),root=GetAncestor(hit,GA_ROOT);
            Handle shell_process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,shell_pid_));
            return app_exact()&&still_exact_()&&GetShellWindow()==shell_&&shell_process.value&&medium(shell_process.value)&&
                input_actor_same_user(shell_process.value)&&process_birth(shell_process.value)==shell_birth_&&
                WaitForSingleObject(shell_process.value,0)==WAIT_TIMEOUT&&Shell_NotifyIconGetRect(&icon,&fresh)==S_OK&&
                EqualRect(&fresh,&area)&&hit&&GetWindowThreadProcessId(hit,&hit_pid)&&hit_pid==shell_pid_&&member(root)&&
                (type(root)==L"Shell_TrayWnd"||type(root)==L"NotifyIconOverflowWindow"||type(root)==L"TopLevelWindowForOverflowXamlIsland");
        };
        click(true,point,target);Menu found;
        const bool opened=wait_for([&]{
            struct Search {const MediumDesktopUiCase* self{};Menu found;unsigned count{};} state{this};
            const bool enumerated=EnumThreadWindows(app_tid_,[](HWND h,LPARAM value)->BOOL{auto& state=*reinterpret_cast<Search*>(value);MENUBARINFO bar{sizeof(bar)};
                if(state.self->type(h)==L"#32768"&&GetMenuBarInfo(h,OBJID_CLIENT,0,&bar)){
                    const Menu candidate{h,bar.hMenu};if(state.self->menu_exact(candidate)){state.found=candidate;++state.count;}}return TRUE;
            },reinterpret_cast<LPARAM>(&state))!=FALSE;found=state.found;return enumerated&&state.count==1;
        },1500);
        if(!opened)throw std::runtime_error("Medium_UI_real_RIGHTUP_did_not_open_exact_product_menu");
        out_.emit("medium_desktop_ui_actual_menu","\"menu_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(found.window))+
            ",\"product_PID_TID_and_current_GUI_menu_owner_confirmed\":true,\"fake_tray_callback_sent\":false");return found;
    }
    void choose(const Menu& menu,UINT wanted,const wchar_t* prefix){
        if(!menu_exact(menu))throw std::runtime_error("Medium_UI_product_menu_identity_lost");
        const int count=GetMenuItemCount(menu.handle);if(count<7||count>12)throw std::runtime_error("Medium_UI_menu_count_unexpected");
        RECT area{};UINT position{};std::wstring label;unsigned matches{};
        for(int n=0;n<count;++n){MENUITEMINFOW info{sizeof(info)};std::array<wchar_t,256> text{};
            info.fMask=MIIM_ID|MIIM_STRING|MIIM_STATE|MIIM_FTYPE;info.dwTypeData=text.data();info.cch=static_cast<UINT>(text.size());
            if(!GetMenuItemInfoW(menu.handle,static_cast<UINT>(n),TRUE,&info)||info.cch>=text.size())throw std::runtime_error("Medium_UI_menu_label_read_failed");
            if(info.wID!=wanted)continue;
            if((info.fType&MFT_SEPARATOR)||(info.fState&(MFS_DISABLED|MFS_GRAYED))||!std::wstring_view(text.data(),info.cch).starts_with(prefix)||
               !GetMenuItemRect(nullptr,menu.handle,static_cast<UINT>(n),&area)||area.left>=area.right||area.top>=area.bottom)
                throw std::runtime_error("Medium_UI_exact_Chinese_menu_item_not_available");
            position=static_cast<UINT>(n);label.assign(text.data(),info.cch);++matches;
        }
        if(matches!=1)throw std::runtime_error("Medium_UI_menu_item_not_unique");
        const POINT point{area.left+(area.right-area.left)/2,area.top+(area.bottom-area.top)/2};
        const auto target=[&]{RECT fresh{};return menu_exact(menu)&&WindowFromPoint(point)==menu.window&&
            GetMenuItemRect(nullptr,menu.handle,position,&fresh)&&EqualRect(&area,&fresh);};
        click(false,point,target);
        out_.emit("medium_desktop_ui_menu_choice","\"actual_item_id\":"+std::to_string(wanted)+",\"actual_Chinese_label\":"+quote(utf8(label))+
            ",\"actual_screen_rect\":"+rect({area.left,area.top,area.right,area.bottom})+",\"WM_COMMAND_injected\":false");
    }
    bool close_command(){
        // This public same-EXE command sends no synthetic input. It remains
        // available after the UI deadline or an independent Default return;
        // the original exact Run-owned process/window/path remain mandatory.
        const fs::path output=L"C:\\PaneBindV1\\Output";
        if(quit_attempted_||!process_exact()||!app_exact()||!medium(GetCurrentProcess())||
           !equal(name(GetThreadDesktop(GetCurrentThreadId())),L"PaneBindV1_"+run_)||
           !equal(name(desktop_),L"PaneBindV1_"+run_)||hash(product_)!=sha_||
           !marker(output/L"panebind-v1-output.marker",run_)||
           !marker(fs::path(L"C:\\PaneBindV1\\Input")/L"panebind-v1-input.marker",run_))return false;
        quit_attempted_=true;
        STARTUPINFOW startup{sizeof(startup)};std::wstring desktop=L"WinSta0\\PaneBindV1_"+run_;startup.lpDesktop=desktop.data();
        PROCESS_INFORMATION info{};auto line=L"\""+product_.wstring()+L"\" --quit";
        SetLastError(ERROR_SUCCESS);const bool created=CreateProcessW(product_.c_str(),line.data(),nullptr,nullptr,FALSE,0,nullptr,product_.parent_path().c_str(),&startup,&info)!=FALSE;
        const DWORD error=created?0:GetLastError();DWORD exited=WAIT_FAILED,code=STILL_ACTIVE;
        if(created){exited=WaitForSingleObject(info.hProcess,1000);if(exited==WAIT_OBJECT_0)GetExitCodeProcess(info.hProcess,&code);
            CloseHandle(info.hThread);CloseHandle(info.hProcess);}
        out_.emit("medium_desktop_ui_failure_quit","\"public_same_EXE_quit_command_create\":"+std::string(created?"true":"false")+
            ",\"create_error\":"+std::to_string(error)+",\"command_wait_result\":"+std::to_string(exited)+
            ",\"command_exit_code\":"+(code!=STILL_ACTIVE?std::to_string(code):"null")+
            ",\"command_wait_budget_ms\":1000,\"synthetic_input_sent\":false,\"forced_process_termination\":false");
        return created&&exited==WAIT_OBJECT_0&&code==0;
    }
    void failure_close()noexcept{
        try{
            if(process_exact())(void)close_command();
            const bool exited=process_&&WaitForSingleObject(process_,1000)==WAIT_OBJECT_0;DWORD code=STILL_ACTIVE;
            if(exited&&GetExitCodeProcess(process_,&code)&&code==0){
                NOTIFYICONIDENTIFIER icon{sizeof(icon)};icon.hWnd=app_;icon.uID=1;RECT area{};
                const auto lookup=app_?Shell_NotifyIconGetRect(&icon,&area):E_PENDING;
                out_.product_closed=app_&&!IsWindow(app_)&&lookup!=S_OK;
                out_.emit("medium_desktop_ui_failure_cleanup_observation","\"actual_process_signalled\":true,\"actual_exit_code\":0,"
                    "\"app_identity_previously_observed\":"+std::string(app_?"true":"false")+
                    ",\"GetRect_after_public_quit_hresult\":"+std::to_string(static_cast<long>(lookup))+
                    ",\"actual_product_closed_known\":"+(out_.product_closed?"true":"false")+",\"failed_case_promoted_to_PASS\":false");
            }
        }catch(...){out_.cleanup_known=false;}
    }
public:
    MediumDesktopUiCase(const std::wstring& run,HDESK desktop,const fs::path& product,std::string_view source,std::string_view sha,
        DWORD budget,const std::function<bool()>& still_exact,const std::shared_ptr<MediumDesktopUiInputLease>& lease,MediumDesktopUiObservation& out)
        :run_(run),desktop_(desktop),product_(product),source_(source),sha_(sha),still_exact_(still_exact),lease_(lease),
         deadline_(GetTickCount64()+(std::min<DWORD>)(budget,20000)),out_(out){}
    ~MediumDesktopUiCase(){
        // Preserve the already-published launch flags even if a reporting
        // allocation throws. This is local handle cleanup, never evidence
        // that a live product or its notification icon has stopped.
        if(objects_)UnhookWinEvent(objects_);if(foreground_)UnhookWinEvent(foreground_);
        if(medium_desktop_ui_events==&events_)medium_desktop_ui_events=nullptr;
        if(events_.changed)CloseHandle(events_.changed);
        if(initial_thread_)CloseHandle(initial_thread_);if(process_)CloseHandle(process_);
        if(com_)CoUninitialize();
    }
    void execute(){
        const HRESULT apartment=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);com_=SUCCEEDED(apartment);
        out_.emit("medium_desktop_ui_scope","\"run_id\":"+quote(utf8(run_))+",\"product_source\":"+quote(source_)+
            ",\"product_sha256\":"+quote(sha_)+",\"ordinary_no_arguments\":true,\"test_only_UI_case\":true,\"automated_guest_driver\":true,"
            "\"product_Default_input_guards_unchanged\":true,\"product_enhancement_PASS_inferred\":false,\"CoInitializeEx_hresult\":"+std::to_string(static_cast<long>(apartment)));
        try{
            const auto parent=product_.parent_path();const fs::path output=L"C:\\PaneBindV1\\Output";
            const bool path=equal(parent.wstring(),(output/(run_+L"-product-Debug")).wstring())||equal(parent.wstring(),(output/(run_+L"-product-Release")).wstring());
            std::array<wchar_t,128> user{};DWORD length=static_cast<DWORD>(user.size());
            if(!com_||!path||run_.size()!=32||source_.size()!=40||sha_.size()!=64||!marker(output/L"panebind-v1-output.marker",run_)||
               !marker(fs::path(L"C:\\PaneBindV1\\Input")/L"panebind-v1-input.marker",run_)||
               !plain_directory(parent)||!GetUserNameW(user.data(),&length)||!equal(user.data(),L"WDAGUtilityAccount")||
               !ProcessIdToSessionId(GetCurrentProcessId(),&session_)||!session_||!input_context()||!no_buttons()||
               hash(product_)!=sha_||!read(parent/L"portable.flag",128).starts_with("PaneBind V1 portable test candidate"))
                throw std::runtime_error("Medium_UI_exact_guest_path_source_or_active_context_not_verified");
            SetLastError(ERROR_SUCCESS);Handle mutex(OpenMutexW(SYNCHRONIZE,FALSE,L"Local\\PaneBind.V1.SingleInstance"));
            const DWORD mutex_error=mutex.value?0:GetLastError();
            out_.emit("medium_desktop_ui_single_instance_preflight","\"OpenMutex_return\":"+std::string(mutex.value?"true":"false")+
                ",\"win32_error\":"+std::to_string(mutex_error)+",\"session_shared_mutex_modified\":false,\"other_desktop_product_touched\":false");
            if(mutex.value||mutex_error!=ERROR_FILE_NOT_FOUND)throw std::runtime_error("Medium_UI_other_product_or_mutex_state_unknown_no_launch");
            shell_=GetShellWindow();GetWindowThreadProcessId(shell_,&shell_pid_);Handle shell_process(shell_pid_?OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,shell_pid_):nullptr);
            shell_birth_=shell_process.value?process_birth(shell_process.value):0;
            if(!shell_||!shell_birth_||!shell_process.value||!medium(shell_process.value)||!input_actor_same_user(shell_process.value))
                throw std::runtime_error("Medium_UI_exact_new_Medium_Shell_identity_unavailable");
            events_.changed=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!events_.changed)throw std::runtime_error("Medium_UI_native_change_event_not_created");
            medium_desktop_ui_events=&events_;
            objects_=SetWinEventHook(EVENT_OBJECT_CREATE,EVENT_OBJECT_HIDE,nullptr,medium_desktop_ui_event,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
            foreground_=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,medium_desktop_ui_event,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
            if(!objects_||!foreground_)throw std::runtime_error("Medium_UI_bounded_native_observer_not_available");
            STARTUPINFOW startup{sizeof(startup)};std::wstring desktop=L"WinSta0\\PaneBindV1_"+run_;startup.lpDesktop=desktop.data();
            PROCESS_INFORMATION info{};auto line=L"\""+product_.wstring()+L"\"";out_.launch_attempted=true;
            SetLastError(ERROR_SUCCESS);const bool created=CreateProcessW(product_.c_str(),line.data(),nullptr,nullptr,FALSE,0,nullptr,parent.c_str(),&startup,&info)!=FALSE;
            const auto error=created?0:GetLastError();process_=info.hProcess;initial_thread_=info.hThread;pid_=info.dwProcessId;
            birth_=created?process_birth(process_):0;events_.product_pid=pid_;out_.product_started=created;
            out_.emit("medium_desktop_ui_ordinary_launch","\"CreateProcessW_attempted\":true,\"native_return\":"+std::string(created?"true":"false")+
                ",\"error\":"+std::to_string(error)+",\"PID\":"+std::to_string(pid_)+",\"birth\":"+std::to_string(birth_)+
                ",\"lpDesktop\":"+quote(utf8(desktop))+",\"arguments\":[],\"product_is_not_a_diagnostic_or_driver_entry\":true");
            if(!created||!process_exact())throw std::runtime_error("Medium_UI_ordinary_product_process_identity_failed");
            const bool gui=wait_for([&]{
                HWND candidate=FindWindowW(L"PaneBind.V1.Application",L"PaneBind");DWORD pid{};const auto tid=GetWindowThreadProcessId(candidate,&pid);
                if(pid!=pid_||!tid)return false;app_=candidate;app_tid_=tid;
                const HWND window=settings();return app_exact()&&window&&GetForegroundWindow()==window;
            },6000);
            if(!gui)throw std::runtime_error("Medium_UI_noarg_visible_exact_settings_not_observed");
            const HWND first=settings();RECT geometry{};
            if(!GetWindowRect(first,&geometry)||geometry.left>=geometry.right||geometry.top>=geometry.bottom)
                throw std::runtime_error("Medium_UI_actual_settings_geometry_unavailable");
            out_.emit("medium_desktop_ui_ordinary_GUI","\"actual_product_PID\":"+std::to_string(pid_)+",\"birth\":"+std::to_string(birth_)+
                ",\"Medium_non_elevated_same_SID_session_image\":true,\"actual_app_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(app_))+
                ",\"actual_settings_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(first))+
                ",\"actual_settings_rect\":"+rect({geometry.left,geometry.top,geometry.right,geometry.bottom})+
                ",\"actual_foreground_is_settings\":true,\"current_held_Run_desktop_membership_confirmed\":true");
            escape_settings(first,"ordinary_noarg_settings_Escape");
            const auto menu=open_menu();choose(menu,501,L"设置");HWND reopened{};
            if(!wait_for([&]{reopened=settings();return reopened&&GetForegroundWindow()==reopened;},2000))
                throw std::runtime_error("Medium_UI_real_tray_Settings_not_opened");
            out_.emit("medium_desktop_ui_tray_settings","\"actual_settings_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(reopened))+
                ",\"same_product_PID_birth\":true,\"actual_tray_RIGHTUP_and_menu_LEFTUP\":true,\"private_settings_command_used\":false");
            escape_settings(reopened,"tray_opened_settings_Escape");
            const auto exit=open_menu();choose(exit,505,L"退出");
            if(!wait_for([&]{return WaitForSingleObject(process_,0)==WAIT_OBJECT_0;},3500))throw std::runtime_error("Medium_UI_real_tray_Exit_not_completed");
            DWORD code=STILL_ACTIVE;const bool read_code=GetExitCodeProcess(process_,&code)!=FALSE;
            NOTIFYICONIDENTIFIER icon{sizeof(icon)};icon.hWnd=app_;icon.uID=1;RECT gone{};const auto lookup=Shell_NotifyIconGetRect(&icon,&gone);
            out_.product_closed=read_code&&code==0&&!IsWindow(app_)&&lookup!=S_OK;
            out_.emit("medium_desktop_ui_tray_exit","\"actual_product_process_signalled\":true,\"exit_code\":"+(read_code?std::to_string(code):"null")+
                ",\"actual_app_HWND_gone\":"+(!IsWindow(app_)?"true":"false")+",\"GetRect_after_Exit_hresult\":"+std::to_string(static_cast<long>(lookup))+
                ",\"forced_termination_or_guest_disposal_used\":false");
            if(!out_.product_closed)throw std::runtime_error("Medium_UI_actual_menu_exit_or_icon_cleanup_unconfirmed");
            out_.completed=true;out_.reason="ordinary_GUI_real_tray_Settings_and_Exit_only";
        }catch(const std::exception& error){out_.reason=error.what();failure_close();}
        catch(...){out_.reason="Medium_UI_unexpected_exception_actual_launch_flags_retained";failure_close();}
        const auto product_log=process_?read(product_.parent_path()/L"panebind.log.jsonl"):std::string{};
        for(const auto* wanted:{"startup","input_route","shutdown"}){
            std::istringstream lines(product_log);std::string line,last;
            while(std::getline(lines,line))if(line.find("\"source_sha\":"+quote(source_))!=line.npos&&
                line.find("\"type\":"+quote(wanted))!=line.npos&&number(line,"\"process_id\":")==pid_)last=line;
            out_.emit("medium_desktop_ui_product_receipt","\"receipt_type\":"+quote(wanted)+",\"actual_PID\":"+
                std::to_string(pid_)+",\"receipt\":"+(last.empty()?"null":quote(last))+",\"missing_is_unknown\":true");
        }
        const bool objects_stopped=!objects_||UnhookWinEvent(objects_)!=FALSE;
        const bool foreground_stopped=!foreground_||UnhookWinEvent(foreground_)!=FALSE;
        objects_=nullptr;foreground_=nullptr;
        medium_desktop_ui_events=nullptr;
        const bool event_closed=!events_.changed||CloseHandle(events_.changed)!=FALSE;events_.changed=nullptr;
        const bool thread_closed=!initial_thread_||CloseHandle(initial_thread_)!=FALSE;initial_thread_=nullptr;
        const bool process_handle_closed=!process_||CloseHandle(process_)!=FALSE;process_=nullptr;
        if(com_){CoUninitialize();com_=false;}
        out_.cleanup_known=objects_stopped&&foreground_stopped&&event_closed&&thread_closed&&process_handle_closed&&
            (!out_.product_started||out_.product_closed)&&lease_&&!lease_->incomplete_pair.load()&&!(lease_->flags.load()&MediumDesktopUiInputLease::in_flight);
        out_.emit("medium_desktop_ui_summary","\"result\":"+quote(out_.completed&&out_.cleanup_known?"ORDINARY_GUI_TRAY_SETTINGS_EXIT_ONLY":"FAIL_OR_BLOCKED")+
            ",\"reason\":"+quote(out_.reason)+",\"product_started\":"+(out_.product_started?"true":"false")+
            ",\"product_closed\":"+(out_.product_closed?"true":"false")+",\"input_attempted\":"+(out_.input_attempted?"true":"false")+
            ",\"input_sent\":"+(out_.input_sent?"true":"false")+
            ",\"WinEvent_objects_unhook_return\":"+(objects_stopped?"true":"false")+",\"WinEvent_foreground_unhook_return\":"+(foreground_stopped?"true":"false")+
            ",\"input_pair_incomplete\":"+(lease_&&lease_->incomplete_pair.load()?"true":"false")+",\"local_cleanup_known\":"+(out_.cleanup_known?"true":"false")+
            ",\"product_Move_Resize_Ctrl\":\"NOT_RUN\",\"resume_enhancement\":\"NOT_RUN_Default_guard_unchanged\",\"help_diagnostic_this_case\":\"NOT_RUN\","
            "\"TaskbarCreated_or_real_Shell_restart\":\"NOT_RUN\",\"whole_M01_B27_B28_PASS_inferred\":false,\"human_UAT\":\"NOT_RUN\"");
    }
};
MediumDesktopUiObservation medium_desktop_ui_case(const std::wstring& run,HDESK desktop,const fs::path& product,
    std::string_view source,std::string_view sha256,DWORD budget_ms,const std::function<bool()>& still_exact,
    const std::shared_ptr<MediumDesktopUiInputLease>& lease){
    MediumDesktopUiObservation out;
    try{MediumDesktopUiCase test(run,desktop,product,source,sha256,budget_ms,still_exact,lease,out);test.execute();}
    catch(...){out.completed=false;out.cleanup_known=false;out.reason="UI_reporting_or_unexpected_exception_launch_flags_preserved";}
    return out;
}
