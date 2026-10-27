#pragma once

// Include inside v1_guest_fixture.cpp's unnamed namespace, after gui_* and
// cleanup(). These cases use actual input in the exact guarded disposable
// guest. They are deliberately not a product settings API or host test.

void settings_guest_scope(State& s){
    std::array<wchar_t,128> user{};DWORD count=static_cast<DWORD>(user.size());
    const auto suffix=s.configuration=="Debug"?L"Debug":s.configuration=="Release"?L"Release":L"";
    const auto local=fs::path(L"C:\\PaneBindV1\\ExportFailure")/s.run/suffix;
    const bool exact_run_copy=s.product==s.output/(s.run+L"-product-"+suffix)/L"panebind.exe";
    const bool exact_local_copy=s.diagnostic_export_failure_local&&s.product==local/L"panebind.exe"&&
        plain_directory(local)&&marker(local/L"export-failure.marker",s.run)&&
        !hash(s.input_product).empty()&&hash(s.product)==hash(s.input_product);
    if(!desktop()||!GetUserNameW(user.data(),&count)||!equal(user.data(),L"WDAGUtilityAccount")||
       s.run.size()!=32||!std::all_of(s.run.begin(),s.run.end(),[](wchar_t c){return(c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');})||
       !equal(s.output.wstring(),L"C:\\PaneBindV1\\Output")||!plain_directory(s.output)||
       !marker(s.output/L"panebind-v1-output.marker",s.run)||
       (!exact_run_copy&&!exact_local_copy)||
       !plain_directory(s.product.parent_path())||!s.process||!medium(s.process)||
       WaitForSingleObject(s.process,0)!=WAIT_TIMEOUT)
        throw std::runtime_error("settings_cases_exact_Medium_disposable_guest_required");
}
std::wstring settings_guest_text(State& s,const SettingsGui& gui,HWND control){
    const auto length=gui_read(s,gui,control,WM_GETTEXTLENGTH);
    if(length>panebind::core::v1::settings_detail::max_file_bytes)throw std::runtime_error("settings_control_text_over_bound");
    std::wstring value(static_cast<std::size_t>(length)+1,L'\0');
    const auto copied=gui_read(s,gui,control,WM_GETTEXT,value.size(),reinterpret_cast<LPARAM>(value.data()));
    if(copied>length)throw std::runtime_error("settings_control_text_changed_during_read");value.resize(copied);return value;
}
void settings_guest_unicode(State& s,std::wstring_view text,const std::function<bool()>& exact){
    if(text.size()>panebind::core::v1::settings_detail::max_file_bytes)throw std::runtime_error("settings_test_text_over_bound");
    for(const auto c:text){
        if(!desktop()||!s.process||WaitForSingleObject(s.process,0)!=WAIT_TIMEOUT||GetProcessId(s.process)!=s.product_pid||!exact())
            throw std::runtime_error("settings_test_text_target_changed");
        // The paired release is sent even if the DOWN changes focus. It is
        // test input, never evidence that the receiving application got it.
        for(const bool up:{false,true}){INPUT packet{};packet.type=INPUT_KEYBOARD;packet.ki.wScan=c;
            packet.ki.dwFlags=KEYEVENTF_UNICODE|(up?KEYEVENTF_KEYUP:0);packet.ki.dwExtraInfo=0x50425631;
            if(!desktop()||SendInput(1,&packet,sizeof(packet))!=1)throw std::runtime_error("settings_test_unicode_SendInput_failed");}
    }
}
void settings_guest_replace(State& s,const SettingsGui& gui,HWND edit,std::wstring_view text){
    gui_click(s,gui,edit);gui_require_input(s,gui);key(s,VK_CONTROL,true);gui_press(s,gui,'A');key(s,VK_CONTROL,false);
    const auto exact=[&]{const auto style=GetWindowLongPtrW(edit,GWL_STYLE);
        return gui_identity(s,gui,edit)&&GetForegroundWindow()==gui.root&&gui_focus(gui)==edit&&
            gui_class(edit)==L"Edit"&&IsWindowVisible(edit)&&IsWindowEnabled(edit)&&
            (style&ES_MULTILINE)&&(style&ES_WANTRETURN)&&!(style&ES_READONLY);};
    // A multiline EDIT's native Enter supplies one hard CRLF. Do not inject
    // Unicode CR and LF as two separate user operations and then assume the
    // resulting control text has that same representation.
    for(std::size_t begin=0;begin<text.size();){const auto end=text.find(L"\r\n",begin);
        const auto line=text.substr(begin,end==std::wstring_view::npos?text.size()-begin:end-begin);
        if(line.find_first_of(L"\r\n")!=std::wstring_view::npos)throw std::runtime_error("settings_test_noncanonical_line_break");
        settings_guest_unicode(s,line,exact);if(end==std::wstring_view::npos)break;
        if(!exact())throw std::runtime_error("settings_test_line_break_target_changed");gui_press(s,gui,VK_RETURN);begin=end+2;
    }
    std::wstring observed;const bool readback=wait([&]{gui_require_input(s,gui);
        if(!exact())throw std::runtime_error("settings_test_readback_target_changed");
        observed=settings_guest_text(s,gui,edit);return observed==text;});
    const auto line_facts=[](std::wstring_view value){std::size_t pairs{};
        for(std::size_t n=0;n+1<value.size();++n)if(value[n]==L'\r'&&value[n+1]==L'\n')++pairs;
        return "{\"characters\":"+std::to_string(value.size())+",\"CR\":"+std::to_string(std::count(value.begin(),value.end(),L'\r'))+
            ",\"LF\":"+std::to_string(std::count(value.begin(),value.end(),L'\n'))+",\"CRLF_pairs\":"+std::to_string(pairs)+
            ",\"ends_in_line_break\":"+(!value.empty()&&(value.back()==L'\r'||value.back()==L'\n')?"true":"false")+"}";};
    s.log.check("settings_actual_editor_input",readback,
        "\"input_kind\":\"actual_Unicode_keys_native_Enter_between_lines\",\"expected_shape\":"+line_facts(text)+
        ",\"actual_shape\":"+line_facts(observed)+",\"content_logged\":false,\"input_resent\":false");
}
// Buttons which open a modal cannot use gui_click's postcondition that focus
// remains in the settings root. Validate the same actual hit, then inspect the
// resulting modal instead of injecting a command notification.
void settings_guest_activate(State& s,const SettingsGui& gui,HWND control){
    gui_require_input(s,gui);RECT r{};
    if(!gui_identity(s,gui,control)||gui_class(control)!=L"Button"||!IsWindowVisible(control)||!IsWindowEnabled(control)||
       !GetWindowRect(control,&r)||s.left)throw std::runtime_error("settings_modal_button_not_exact");
    const POINT point{r.left+(r.right-r.left)/2,r.top+(r.bottom-r.top)/2};cursor(s,point);
    const auto exact_hit=[&]{
        RECT current{};POINT actual_cursor{};
        return !s.left&&gui_identity(s,gui,control)&&gui_class(control)==L"Button"&&
            IsWindowVisible(control)&&IsWindowEnabled(control)&&GetForegroundWindow()==gui.root&&
            gui_identity(s,gui,gui_focus(gui))&&GetWindowRect(control,&current)&&EqualRect(&r,&current)&&
            GetCursorPos(&actual_cursor)&&actual_cursor.x==point.x&&actual_cursor.y==point.y&&
            PtInRect(&current,point)&&WindowFromPoint(point)==control;
    };
    const bool hit_ready=exact_hit();
    s.log.check("settings_modal_button_actual_ready_hit",hit_ready,
        "\"root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.root))+
        ",\"control\":"+std::to_string(reinterpret_cast<std::uintptr_t>(control))+
        ",\"control_rect\":"+rect({r.left,r.top,r.right,r.bottom})+
        ",\"point\":["+std::to_string(point.x)+","+std::to_string(point.y)+
        "],\"fresh_exact_target_geometry_cursor_hit\":"+(hit_ready?"true":"false")+",\"single_DOWN_UP_not_yet_sent\":true");
    // Recheck after diagnostic I/O immediately before the one input pair.
    // A changed layout/hit fails rather than clicking stale coordinates.
    if(!exact_hit())throw std::runtime_error("settings_modal_button_actual_hit_or_geometry_changed");
    button(s,true);button(s,false);
}
struct SettingsGuestModal {HWND root{};DWORD tid{};};
bool settings_guest_modal_identity(State& s,const SettingsGui& gui,const SettingsGuestModal& modal){
    DWORD pid{};if(!modal.root||GetWindowThreadProcessId(modal.root,&pid)!=gui.tid||pid!=s.product_pid||
        modal.tid!=gui.tid||gui_class(modal.root)!=L"#32770"||!IsWindowVisible(modal.root)||
        !s.process||WaitForSingleObject(s.process,0)!=WAIT_TIMEOUT)return false;
    HWND owner=GetWindow(modal.root,GW_OWNER);unsigned limit=16;
    while(owner&&owner!=gui.root&&limit--)owner=GetWindow(owner,GW_OWNER);
    return owner==gui.root;
}
SettingsGuestModal settings_guest_modal(State& s,const SettingsGui& gui,std::wstring_view title){
    SettingsGuestModal found;
    s.log.check("settings_exact_native_modal",wait([&]{
        struct Search {State* state;const SettingsGui* gui;std::wstring_view title;SettingsGuestModal value;unsigned count{};} query{&s,&gui,title};
        EnumWindows([](HWND h,LPARAM data)->BOOL{auto& q=*reinterpret_cast<Search*>(data);DWORD pid{};const auto tid=GetWindowThreadProcessId(h,&pid);
            if(pid!=q.state->product_pid||tid!=q.gui->tid||gui_class(h)!=L"#32770"||!IsWindowVisible(h))return TRUE;
            std::array<wchar_t,256> caption{};const auto n=GetWindowTextW(h,caption.data(),static_cast<int>(caption.size()));
            SettingsGuestModal candidate{h,tid};if(std::wstring_view(caption.data(),n)==q.title&&settings_guest_modal_identity(*q.state,*q.gui,candidate)){q.value=candidate;++q.count;}return TRUE;
        },reinterpret_cast<LPARAM>(&query));found=query.value;return query.count==1&&GetForegroundWindow()==found.root;
    }),"\"title\":"+quote(utf8(std::wstring(title)))+",\"same_actual_product_PID_TID\":true");return found;
}
void settings_guest_modal_press(State& s,const SettingsGui& gui,const SettingsGuestModal& modal,WORD code){
    GUITHREADINFO info{sizeof(info)};
    if(!settings_guest_modal_identity(s,gui,modal)||GetForegroundWindow()!=modal.root||!GetGUIThreadInfo(modal.tid,&info)||
       !info.hwndFocus||(info.hwndFocus!=modal.root&&!IsChild(modal.root,info.hwndFocus)))
        throw std::runtime_error("settings_modal_keyboard_target_changed");
    key(s,code,true);key(s,code,false);
}
void settings_guest_acknowledge(State& s,const SettingsGui& gui,std::wstring_view body_prefix,std::wstring_view title=L"PaneBind"){
    const auto modal=settings_guest_modal(s,gui,title);bool body=false;
    struct Search {HWND root;std::wstring_view prefix;bool found{};} query{modal.root,body_prefix};
    EnumChildWindows(modal.root,[](HWND h,LPARAM data)->BOOL{auto& q=*reinterpret_cast<Search*>(data);if(gui_class(h)!=L"Static")return TRUE;
        std::array<wchar_t,1024> value{};DWORD_PTR copied{};
        if(SendMessageTimeoutW(h,WM_GETTEXT,value.size(),reinterpret_cast<LPARAM>(value.data()),SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&copied)&&
           copied<value.size()&&std::wstring_view(value.data(),copied).starts_with(q.prefix))q.found=true;return TRUE;
    },reinterpret_cast<LPARAM>(&query));body=query.found;
    s.log.check("settings_expected_modal_body",body,"\"expected_prefix\":"+quote(utf8(std::wstring(body_prefix))));
    settings_guest_modal_press(s,gui,modal,VK_RETURN);
    s.log.check("settings_actual_Enter_dismisses_modal",wait([&]{return !IsWindow(modal.root)&&GetForegroundWindow()==gui.root;}));
}
HWND settings_guest_filename(State& s,const SettingsGui& gui,const SettingsGuestModal& modal){
    const auto editable=[&](HWND h){DWORD pid{};return settings_guest_modal_identity(s,gui,modal)&&GetForegroundWindow()==modal.root&&
        h&&IsChild(modal.root,h)&&GetWindowThreadProcessId(h,&pid)==modal.tid&&pid==s.product_pid&&gui_class(h)==L"Edit"&&
        IsWindowVisible(h)&&IsWindowEnabled(h)&&!(GetWindowLongPtrW(h,GWL_STYLE)&ES_READONLY);};
    HWND found{};unsigned candidates{},visited{};
    const bool ready=wait([&]{
        if(!settings_guest_modal_identity(s,gui,modal)||GetForegroundWindow()!=modal.root)return false;
        struct Search {HWND modal,found{};DWORD pid,tid;unsigned candidates{},visited{};} query{modal.root,nullptr,s.product_pid,modal.tid};
        EnumChildWindows(modal.root,[](HWND h,LPARAM data)->BOOL{auto& q=*reinterpret_cast<Search*>(data);if(++q.visited>128)return FALSE;
            DWORD pid{};if(GetWindowThreadProcessId(h,&pid)!=q.tid||pid!=q.pid||gui_class(h)!=L"Edit"||!IsChild(q.modal,h)||
                !IsWindowVisible(h)||!IsWindowEnabled(h)||(GetWindowLongPtrW(h,GWL_STYLE)&ES_READONLY))return TRUE;
            bool filename=GetDlgCtrlID(h)==1152;HWND parent=GetParent(h);unsigned limit=8;
            while(parent&&parent!=q.modal&&limit--){if(GetDlgCtrlID(parent)==1148){filename=true;break;}parent=GetParent(parent);}
            if(filename){q.found=h;++q.candidates;}return TRUE;
        },reinterpret_cast<LPARAM>(&query));
        found=query.found;candidates=query.candidates;visited=query.visited;
        return visited<=128&&candidates==1&&editable(found);
    },1500);
    bool used_alt_n=false;
    if(!ready){
        GUITHREADINFO context{sizeof(context)};
        if(!settings_guest_modal_identity(s,gui,modal)||GetForegroundWindow()!=modal.root||!GetGUIThreadInfo(modal.tid,&context)||
           !context.hwndFocus||!IsChild(modal.root,context.hwndFocus))throw std::runtime_error("native_filename_shortcut_modal_context_invalid");
        for(const int code:std::array{VK_CONTROL,VK_MENU,VK_SHIFT,VK_LWIN,VK_RWIN})
            if(GetAsyncKeyState(code)&0x8000)throw std::runtime_error("native_filename_shortcut_modifiers_not_released");
        bool alt_down=false;
        try{alt_down=true;key(s,VK_MENU,true);settings_guest_modal_press(s,gui,modal,'N');key(s,VK_MENU,false);alt_down=false;}
        catch(...){if(alt_down)try{key(s,VK_MENU,false);}catch(...){}throw;}
        used_alt_n=true;
        if(!wait([&]{GUITHREADINFO actual{sizeof(actual)};if(!GetGUIThreadInfo(modal.tid,&actual)||!editable(actual.hwndFocus))return false;
            found=actual.hwndFocus;return true;},1500)){
            struct Snapshot {std::string rows{"["};unsigned visited{};HWND modal;} snapshot{"[",0,modal.root};
            EnumChildWindows(modal.root,[](HWND h,LPARAM data)->BOOL{auto& q=*reinterpret_cast<Snapshot*>(data);if(q.visited++>=128)return FALSE;
                DWORD pid{};const auto tid=GetWindowThreadProcessId(h,&pid);if(q.rows.size()>1)q.rows+=',';
                q.rows+="{\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+",\"class\":"+quote(utf8(gui_class(h)))+
                    ",\"id\":"+std::to_string(GetDlgCtrlID(h))+",\"style\":"+std::to_string(GetWindowLongPtrW(h,GWL_STYLE))+
                    ",\"pid\":"+std::to_string(pid)+",\"tid\":"+std::to_string(tid)+",\"visible\":"+(IsWindowVisible(h)?"true":"false")+
                    ",\"enabled\":"+(IsWindowEnabled(h)?"true":"false")+",\"ancestors\":[";
                HWND parent=GetParent(h);for(unsigned n=0;parent&&n<8;++n){if(n)q.rows+=',';q.rows+=std::to_string(reinterpret_cast<std::uintptr_t>(parent));
                    if(parent==q.modal)break;parent=GetParent(parent);}q.rows+="]}";return TRUE;
            },reinterpret_cast<LPARAM>(&snapshot));snapshot.rows+=']';
            s.log.emit("settings_native_filename_unavailable","\"documented_candidate_count\":"+std::to_string(candidates)+
                ",\"child_snapshot_cap\":128,\"children\":"+snapshot.rows+",\"control_text_recorded\":false,\"actual_Alt_N_sent\":true");
            throw std::runtime_error("native_filename_actual_shortcut_focus_not_editable");
        }
    }
    if(!editable(found))throw std::runtime_error("native_filename_edit_identity_changed");
    s.log.emit("settings_native_filename_identified","\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(found))+
        ",\"actual_Alt_N_used\":"+(used_alt_n?"true":"false")+",\"bounded_readiness_wait\":true,\"exact_modal_PID_TID\":true");
    return found;
}
void settings_guest_file_path(State& s,const SettingsGui& gui,bool save,const fs::path& path){
    settings_guest_scope(s);
    if(!path.is_absolute()||path.parent_path()!=s.output||path.extension()!=L".conf"||!path.filename().wstring().starts_with(s.run+L"-"))
        throw std::runtime_error("settings_native_dialog_path_not_run_output");
    settings_guest_activate(s,gui,GetDlgItem(gui.root,save?113:112));
    const auto modal=settings_guest_modal(s,gui,save?L"导出当前设置草稿":L"导入 PaneBind 配置");
    try{
    const auto edit=settings_guest_filename(s,gui,modal);RECT r{};if(!GetWindowRect(edit,&r))throw std::runtime_error("native_filename_edit_bounds_missing");
    const POINT point{r.left+(r.right-r.left)/2,r.top+(r.bottom-r.top)/2};cursor(s,point);
    if(WindowFromPoint(point)!=edit)throw std::runtime_error("native_filename_edit_actual_hit_mismatch");button(s,true);button(s,false);
    s.log.check("settings_native_filename_actual_focus",wait([&]{GUITHREADINFO info{sizeof(info)};return settings_guest_modal_identity(s,gui,modal)&&
        GetForegroundWindow()==modal.root&&GetGUIThreadInfo(modal.tid,&info)&&info.hwndFocus==edit;},1500));
    key(s,VK_CONTROL,true);settings_guest_modal_press(s,gui,modal,'A');key(s,VK_CONTROL,false);
    settings_guest_unicode(s,path.wstring(),[&]{GUITHREADINFO info{sizeof(info)};return settings_guest_modal_identity(s,gui,modal)&&
        GetForegroundWindow()==modal.root&&GetGUIThreadInfo(modal.tid,&info)&&info.hwndFocus==edit;});
    const auto expected=path.wstring();std::wstring actual(expected.size()+2,L'\0');
    unsigned reads{};DWORD read_error{};DWORD_PTR copied{};bool text_read=false;
    const bool exact_text=wait([&]{
        GUITHREADINFO info{sizeof(info)};DWORD pid{};
        if(!settings_guest_modal_identity(s,gui,modal)||GetForegroundWindow()!=modal.root||!GetGUIThreadInfo(modal.tid,&info)||
           info.hwndFocus!=edit||GetWindowThreadProcessId(edit,&pid)!=modal.tid||pid!=s.product_pid||!IsChild(modal.root,edit)||
           gui_class(edit)!=L"Edit"||!IsWindowVisible(edit)||!IsWindowEnabled(edit)||(GetWindowLongPtrW(edit,GWL_STYLE)&ES_READONLY))
            throw std::runtime_error("native_filename_readback_exact_context_lost");
        std::fill(actual.begin(),actual.end(),L'\0');copied=0;++reads;SetLastError(0);
        text_read=SendMessageTimeoutW(edit,WM_GETTEXT,actual.size(),reinterpret_cast<LPARAM>(actual.data()),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&copied)!=FALSE;
        read_error=text_read?0:GetLastError();
        return text_read&&copied<actual.size()&&std::wstring_view(actual.data(),copied)==expected;
    },1500);
    s.log.check("settings_native_filename_actual_keys",exact_text,"\"new_run_output_path\":"+quote(utf8(path.wstring()))+
        ",\"bounded_readback_wait_ms\":1500,\"read_attempts\":"+std::to_string(reads)+",\"last_read_succeeded\":"+(text_read?"true":"false")+
        ",\"last_read_error\":"+std::to_string(read_error)+",\"observed_copied_characters\":"+std::to_string(copied)+
        ",\"expected_characters\":"+std::to_string(expected.size())+",\"input_resent\":false,\"actual_text_recorded\":false");
    settings_guest_modal_press(s,gui,modal,VK_RETURN);
    settings_guest_acknowledge(s,gui,save?L"已导出经过校验的设置草稿":L"已导入到草稿");
    }catch(const std::exception& failure){
        bool attempted=false,closed=false;std::string cleanup_error;
        if(settings_guest_modal_identity(s,gui,modal)&&GetForegroundWindow()==modal.root){
            try{if(s.control)key(s,VK_CONTROL,false);if(s.left)button(s,false);
                attempted=true;settings_guest_modal_press(s,gui,modal,VK_ESCAPE);
                closed=wait([&]{return !IsWindow(modal.root)&&gui_identity(s,gui,gui.tab)&&GetForegroundWindow()==gui.root;},1500);
            }catch(const std::exception& error){cleanup_error=error.what();}
        }
        s.log.emit("settings_native_file_modal_failure_cleanup","\"original_failure\":"+quote(failure.what())+
            ",\"exact_original_modal\":"+std::to_string(reinterpret_cast<std::uintptr_t>(modal.root))+
            ",\"actual_Escape_attempted\":"+(attempted?"true":"false")+",\"original_modal_closed_returned_to_own_settings\":"+(closed?"true":"false")+
            ",\"cleanup_error\":"+quote(cleanup_error)+",\"original_failure_preserved\":true,\"functional_PASS_inferred\":false");
        throw;
    }
}
struct SettingsGuestRunValue {bool exists{};DWORD type{};std::wstring text;};
SettingsGuestRunValue settings_guest_run_value(){
    HKEY key{};const auto opened=RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&key);
    if(opened==ERROR_FILE_NOT_FOUND)return {};if(opened!=ERROR_SUCCESS)throw std::runtime_error("settings_HKCU_Run_query_denied");
    struct Close {HKEY key;~Close(){RegCloseKey(key);}} close{key};DWORD type{},bytes{};
    const auto sized=RegQueryValueExW(key,L"PaneBind",nullptr,&type,nullptr,&bytes);if(sized==ERROR_FILE_NOT_FOUND)return {};
    if(sized!=ERROR_SUCCESS||type!=REG_SZ||bytes<sizeof(wchar_t)||bytes>65536||bytes%sizeof(wchar_t))throw std::runtime_error("settings_existing_Run_value_unknown");
    std::wstring value(bytes/sizeof(wchar_t),L'\1');DWORD actual=bytes;
    if(RegQueryValueExW(key,L"PaneBind",nullptr,&type,reinterpret_cast<BYTE*>(value.data()),&actual)!=ERROR_SUCCESS||actual!=bytes||type!=REG_SZ||
       value.back()!=L'\0'||value.find(L'\0')!=value.size()-1)throw std::runtime_error("settings_Run_value_not_exact_terminated_REG_SZ");
    value.pop_back();return {true,type,std::move(value)};
}
panebind::core::v1::Settings settings_guest_configuration(State& s){
    auto parsed=panebind::core::v1::parse(read(s.product.parent_path()/L"settings.ini"));
    if(!parsed.value)throw std::runtime_error("settings_case_actual_configuration_invalid");return *parsed.value;
}
std::string settings_guest_single_instance(State& s,DWORD expected_pid,std::uint64_t expected_birth,std::string_view stage){
    const auto exact_process=[&]{return desktop()&&expected_pid&&expected_birth&&s.process&&s.product_pid==expected_pid&&
        GetProcessId(s.process)==expected_pid&&WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&medium(s.process)&&process_birth(s.process)==expected_birth;};
    const auto roots=[&]{
        struct Search {DWORD pid;unsigned count{};bool exact{true};} query{expected_pid};
        const bool enumerated=EnumWindows([](HWND window,LPARAM data)->BOOL{auto& q=*reinterpret_cast<Search*>(data);
            if(gui_class(window)!=L"PaneBind.V1.Application")return TRUE;
            DWORD pid{};++q.count;q.exact=q.exact&&GetAncestor(window,GA_ROOT)==window&&GetWindowThreadProcessId(window,&pid)&&pid==q.pid;
            return q.count<=16;
        },reinterpret_cast<LPARAM>(&query))!=FALSE;
        return enumerated&&query.count==1&&query.exact;
    };
    if(!exact_process()||!roots())throw std::runtime_error("single_instance_exact_held_product_or_unique_root_not_confirmed");
    const auto mark=read_product(s).size();DWORD command_exit=STILL_ACTIVE;const bool requested=launch(s,L"--status",true,&command_exit);std::string status;
    const bool observed=requested&&wait([&]{status=record(s,mark,"status");return number(status,"\"process_id\":")==expected_pid;});
    const bool source_matches=s.product_source.size()==40&&status.find("\"source_sha\":"+quote(s.product_source))!=status.npos;
    const bool hook_owner_running=status.find("\"hook_owner_running\":true")!=status.npos;
    const bool native_idle=status.find("\"gesture_active\":false")!=status.npos&&
        status.find("\"native_operation_pending\":false")!=status.npos&&status.find("\"native_completion_unknown\":false")!=status.npos&&
        status.find("\"visual_restore_pending\":false")!=status.npos;
    s.log.check(std::string(stage)+"_single_exact_behavior_instance",observed&&command_exit==0&&source_matches&&hook_owner_running&&native_idle&&exact_process()&&roots(),
        "\"expected_PID\":"+std::to_string(expected_pid)+",\"expected_birth\":"+std::to_string(expected_birth)+
        ",\"fresh_status_command_exit\":"+std::to_string(command_exit)+",\"fresh_actual_status\":"+quote(status)+
        ",\"actual_source_sha_matches_product_identity\":"+(source_matches?"true":"false")+
        ",\"actual_hook_owner_running_from_status_only\":"+(hook_owner_running?"true":"false")+
        ",\"actual_native_idle_confirmed\":"+(native_idle?"true":"false")+",\"global_hook_count_not_inferred\":true,\"unknown_roots_not_controlled\":true");
    return status;
}
void settings_guest_startup(State& s){
    settings_guest_scope(s);const auto original=settings_guest_configuration(s);const auto expected=L"\""+s.product.wstring()+L"\" --startup";
    if(original.startup||settings_guest_run_value().exists)throw std::runtime_error("startup_test_requires_absent_Run_value_no_unknown_overwrite");
    const auto stop_verified=[&](std::string_view stage){
        const auto pid=s.product_pid;const auto birth=process_birth(s.process);const auto mark=read_product(s).size();const auto driver_mark=s.log.mark();
        cleanup(s);const auto shutdown=record(s,mark,"shutdown");std::string stopped,recovery;bool forced=false;
        std::istringstream rows(s.log.since(driver_mark));std::string row;
        while(std::getline(rows,row))if(row.ends_with('}')){forced=forced||row.find("\"own_product_forced_termination\":true")!=row.npos;
            if(row.find("\"type\":\"product_stop\"")!=row.npos)stopped=row;
            if(row.find("\"type\":\"post_stop_actual_client_click\"")!=row.npos)recovery=row;}
        s.log.check(std::string(stage)+"_normal_stop_and_actual_input",pid&&birth&&!s.process&&!forced&&number(shutdown,"\"process_id\":")==pid&&
            stopped.find("\"quit_command_completed\":true")!=stopped.npos&&stopped.find("\"process_exit_observed\":true")!=stopped.npos&&
            stopped.find("\"product_exit_code\":0")!=stopped.npos&&shutdown.find("\"own_hooks_stopped\":true")!=shutdown.npos&&
            shutdown.find("\"own_overlay_destroyed\":true")!=shutdown.npos&&shutdown.find("\"native_worker_exited\":true")!=shutdown.npos&&
            shutdown.find("\"issued_native_completion_unknown\":false")!=shutdown.npos&&input_resource_stop_observed(shutdown)&&recovery.find("\"actual_down_up_delivered\":true")!=recovery.npos,
            "\"actual_shutdown\":"+quote(shutdown)+",\"actual_post_stop_input\":"+quote(recovery));
    };
    try{for(const bool enabled:{true,false}){const auto gui=gui_open(s);gui_page_select(s,gui,0);
            const auto checkbox=gui_find(s,gui,L"Button",L"用户登录后启动");gui_click(s,gui,checkbox);
            s.log.check("startup_actual_checkbox",wait([&]{return (gui_read(s,gui,checkbox,BM_GETCHECK)==BST_CHECKED)==enabled;}));gui_save_enter(s,gui);
            const auto config=settings_guest_configuration(s);const auto entry=settings_guest_run_value();auto wanted=original;wanted.startup=enabled;wanted.generation=config.generation;
            s.log.check("startup_GUI_save_actual_Run_value",config==wanted&&(enabled?(entry.exists&&entry.type==REG_SZ&&entry.text==expected):!entry.exists),
                "\"enabled\":"+std::string(enabled?"true":"false")+",\"exact_current_product_exe\":true,\"actual_HKCU\":true");
            if(enabled){
                const auto paused_gui=gui_open(s);gui_page_select(s,paused_gui,0);const auto toggle=gui_find(s,paused_gui,L"Button",L"启用 PaneBind");
                if(gui_read(s,paused_gui,toggle,BM_GETCHECK)!=BST_UNCHECKED)gui_click(s,paused_gui,toggle);
                // Focus can already be on this checkbox before its queued UP
                // toggles BS_AUTOCHECKBOX. Observe the result of that ONE
                // actual click; never click again or save an old state.
                s.log.check("startup_actual_GUI_saved_disabled_checkbox",wait([&]{return gui_read(s,paused_gui,toggle,BM_GETCHECK)==BST_UNCHECKED;}));gui_save_enter(s,paused_gui);
                const auto saved=settings_guest_configuration(s);auto disabled=original;disabled.startup=true;disabled.enabled=false;disabled.generation=saved.generation;
                const auto run_value=settings_guest_run_value();const auto saved_bytes=read(s.product.parent_path()/L"settings.ini");
                s.log.check("startup_saved_disabled_and_exact_Run_command",saved==disabled&&run_value.exists&&run_value.type==REG_SZ&&run_value.text==expected);
                const auto prior_pid=s.product_pid;const auto prior_birth=process_birth(s.process);stop_verified("startup_before_simulated_login");
                const auto start_mark=read_product(s).size();s.log.check("startup_simulated_login_exact_stored_command",launch(s,L"--startup",false),
                    "\"stored_command\":"+quote(utf8(run_value.text))+",\"arguments\":\"--startup\",\"real_Windows_logon\":false");
                s.log.check("startup_new_actual_process_identity",wait([&]{return number(record(s,start_mark,"startup"),"\"process_id\":")==s.product_pid;})&&
                    s.process&&(s.product_pid!=prior_pid||process_birth(s.process)!=prior_birth)&&read(s.product.parent_path()/L"settings.ini")==saved_bytes);
                const auto startup_pid=s.product_pid;const auto startup_birth=process_birth(s.process);
                const auto status=settings_guest_single_instance(s,startup_pid,startup_birth,"simulated_login");
                struct SettingsCount {DWORD pid;unsigned count{};} count{startup_pid};
                const bool enumerated=EnumWindows([](HWND window,LPARAM data)->BOOL{auto& q=*reinterpret_cast<SettingsCount*>(data);DWORD pid{};
                    if(gui_class(window)==L"PaneBindV1SettingsWindow"&&GetWindowThreadProcessId(window,&pid)&&pid==q.pid)++q.count;return TRUE;
                },reinterpret_cast<LPARAM>(&count))!=FALSE;
                s.log.check("startup_saved_disabled_no_automatic_settings",status.find("\"enabled\":false")!=status.npos&&
                    status.find("\"gesture_active\":false")!=status.npos&&enumerated&&count.count==0&&settings_guest_configuration(s)==saved,
                    "\"fresh_status\":"+quote(status)+",\"actual_product_settings_window_count\":"+std::to_string(count.count)+",\"real_Windows_logon\":false");
                DWORD secondary_exit=STILL_ACTIVE;const bool secondary=launch(s,L"",true,&secondary_exit);SettingsGui secondary_gui;
                s.log.check("startup_ordinary_secondary_command_exits_zero",secondary&&secondary_exit==0,"\"arguments\":\"\",\"secondary_exit\":"+std::to_string(secondary_exit));
                const bool opened=wait([&]{struct Search {State* s;SettingsGui gui;unsigned count{};} query{&s};
                    EnumWindows([](HWND window,LPARAM data)->BOOL{auto& q=*reinterpret_cast<Search*>(data);DWORD pid{};const auto tid=GetWindowThreadProcessId(window,&pid);
                        if(pid==q.s->product_pid&&gui_class(window)==L"PaneBindV1SettingsWindow"&&IsWindowVisible(window)){
                            SettingsGui candidate{window,GetDlgItem(window,100),tid};if(candidate.tab&&gui_identity(*q.s,candidate,candidate.tab)){q.gui=candidate;++q.count;}}return TRUE;
                    },reinterpret_cast<LPARAM>(&query));secondary_gui=query.gui;return query.count==1&&GetForegroundWindow()==secondary_gui.root&&gui_identity(s,secondary_gui,gui_focus(secondary_gui));});
                const auto after_secondary=settings_guest_single_instance(s,startup_pid,startup_birth,"startup_ordinary_secondary");
                s.log.check("startup_ordinary_secondary_opens_same_main_settings",opened&&after_secondary.find("\"enabled\":false")!=after_secondary.npos&&
                    settings_guest_configuration(s)==saved,"\"main_PID_unchanged\":true,\"main_birth_unchanged\":true,\"settings_opened_by_no_argument_launch_only\":true");
                gui_page_select(s,secondary_gui,0);const auto original_enabled=gui_find(s,secondary_gui,L"Button",L"启用 PaneBind");
                if((gui_read(s,secondary_gui,original_enabled,BM_GETCHECK)==BST_CHECKED)!=original.enabled)gui_click(s,secondary_gui,original_enabled);
                gui_save_enter(s,secondary_gui);const auto restored=settings_guest_configuration(s);auto resumed=original;resumed.startup=true;resumed.generation=restored.generation;
                s.log.check("startup_original_enabled_restored_actual_GUI",restored==resumed);
            }
        }
        // --startup is intentionally not --diagnostic. Restore the old test
        // harness launch mode only AFTER the ordinary secondary-entry proof,
        // so later receipt-based cases do not silently lose detailed logging.
        const auto final=settings_guest_configuration(s);auto wanted=original;wanted.generation=final.generation;
        s.log.check("startup_original_options_and_Run_removal",final==wanted&&!settings_guest_run_value().exists);
        stop_verified("startup_after_simulated_login");const auto mark=read_product(s).size();
        s.log.check("startup_restore_diagnostic_test_setup",launch(s,L"--diagnostic",false));
        s.log.check("startup_diagnostic_test_setup_current_instance",wait([&]{return number(record(s,mark,"startup"),"\"process_id\":")==s.product_pid;})&&settings_guest_configuration(s)==final);
        settings_guest_single_instance(s,s.product_pid,process_birth(s.process),"startup_restored_test_setup");
        s.log.emit("startup_simulated_login_result","\"result\":\"PASS\",\"actual_HKCU_add_remove\":true,\"saved_disabled_restored_on_startup\":true,"
            "\"ordinary_secondary_same_behavior_owner\":true,\"real_Windows_logon\":false,\"global_hook_count_not_inferred\":true,\"diagnostic_restart_is_test_setup\":true");
    }catch(...){try{const auto value=settings_guest_run_value();if(value.exists&&value.text==expected){HKEY key{};
                const auto opened=RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_SET_VALUE,&key);
                const auto result=opened==ERROR_SUCCESS?RegDeleteValueW(key,L"PaneBind"):opened;if(opened==ERROR_SUCCESS)RegCloseKey(key);
                s.log.emit("test_cleanup","\"resource\":\"exact_test_created_startup_value\",\"deleted\":"+std::string(result==ERROR_SUCCESS?"true":"false")+",\"product_GUI_PASS_inferred\":false");}}catch(...){}throw;}
}
void settings_guest_effect_layout(State& s){
    settings_guest_scope(s);
    if(s.left||s.control||std::any_of(s.explorer.begin(),s.explorer.end(),[](const auto& target){return static_cast<bool>(target);})||
        std::any_of(s.applications.begin(),s.applications.end(),[](const auto& target){return static_cast<bool>(target);}))
        throw std::runtime_error("settings_effect_requires_released_exact_original_owned_roots");
    for(const auto& frame:s.frames)if(!own(s,frame.hwnd)||GetAncestor(frame.hwnd,GA_ROOT)!=frame.hwnd)
        throw std::runtime_error("settings_effect_original_owned_root_identity_changed");
    // Give the standard caption real space at 150% DPI. Only the source width
    // and both targets' X coordinates change: the 20 px gap, +14/+3 intent,
    // 6 px/3 px residual, and vertical alignment oracle remain independent.
    constexpr std::array<Rect,3> layout{{{120,80,520,440},{540,80,800,260},{540,260,800,440}}};
    s.log.emit("test_setup","\"purpose\":\"settings_effect_owned_caption_space\",\"source_visible_width\":400,\"initial_horizontal_gap\":20,\"product_placement_claimed\":false");
    for(std::size_t n=0;n<3;++n){
        const HWND h=s.frames[n].hwnd;const bool was_visible=IsWindowVisible(h)!=FALSE,was_iconic=IsIconic(h)!=FALSE,was_zoomed=IsZoomed(h)!=FALSE;
        const auto before=visible(h);bool restore_attempted=false,restore_returned=false,show_attempted=false,show_returned=false;
        if(was_iconic||was_zoomed){
            if(!own(s,h))throw std::runtime_error("settings_effect_identity_changed_before_owned_restore");
            restore_attempted=true;restore_returned=ShowWindowAsync(h,SW_RESTORE)!=FALSE;
        }
        if(!IsWindowVisible(h)){
            if(!own(s,h))throw std::runtime_error("settings_effect_identity_changed_before_owned_show");
            show_attempted=true;show_returned=ShowWindowAsync(h,SW_SHOWNOACTIVATE)!=FALSE;
        }
        const bool ready=(!restore_attempted||restore_returned)&&(!show_attempted||show_returned)&&wait([&]{return own(s,h)&&
            GetAncestor(h,GA_ROOT)==h&&IsWindowVisible(h)&&IsWindowEnabled(h)&&!IsIconic(h)&&!IsZoomed(h);},1000);
        const bool positioned=ready&&fixture_rect(h,layout[n]);const auto after=visible(h);
        const auto monitor=MonitorFromWindow(h,MONITOR_DEFAULTTONULL);const auto dpi=GetDpiForWindow(h);
        const bool native_ready=positioned&&own(s,h)&&GetAncestor(h,GA_ROOT)==h&&IsWindowVisible(h)&&IsWindowEnabled(h)&&
            !IsIconic(h)&&!IsZoomed(h)&&after==layout[n]&&monitor&&dpi&&
            monitor==MonitorFromWindow(s.frames[0].hwnd,MONITOR_DEFAULTTONULL)&&dpi==GetDpiForWindow(s.frames[0].hwnd);
        s.log.check("settings_effect_exact_owned_setup",native_ready,
            "\"test_setup\":true,\"member\":"+std::to_string(n)+",\"HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
            ",\"before_visible\":"+(was_visible?"true":"false")+",\"before_iconic\":"+(was_iconic?"true":"false")+
            ",\"before_zoomed\":"+(was_zoomed?"true":"false")+",\"before_visible_rect\":"+(before?rect(*before):"null")+
            ",\"owned_restore_attempted\":"+(restore_attempted?"true":"false")+",\"ShowWindowAsync_restore_return\":"+(restore_attempted?(restore_returned?"true":"false"):"null")+
            ",\"owned_show_attempted\":"+(show_attempted?"true":"false")+",\"ShowWindowAsync_show_return\":"+(show_attempted?(show_returned?"true":"false"):"null")+
            ",\"after_visible\":"+(IsWindowVisible(h)?"true":"false")+",\"after_iconic\":"+(IsIconic(h)?"true":"false")+
            ",\"after_zoomed\":"+(IsZoomed(h)?"true":"false")+",\"after_enabled\":"+(IsWindowEnabled(h)?"true":"false")+
            ",\"expected_visible\":"+rect(layout[n])+",\"actual_visible_rect\":"+(after?rect(*after):"null")+
            ",\"actual_monitor\":"+std::to_string(reinterpret_cast<std::uintptr_t>(monitor))+",\"actual_DPI\":"+std::to_string(dpi)+
            ",\"product_admission_or_snap_PASS_inferred\":false");
    }
    GUITHREADINFO gui{sizeof(gui)};const bool gui_read=GetGUIThreadInfo(s.owner,&gui)!=FALSE;
    const bool roots_ready=std::all_of(s.frames.begin(),s.frames.end(),[&](const auto& frame){return own(s,frame.hwnd)&&
        GetAncestor(frame.hwnd,GA_ROOT)==frame.hwnd&&IsWindowVisible(frame.hwnd)&&IsWindowEnabled(frame.hwnd)&&!IsIconic(frame.hwnd)&&!IsZoomed(frame.hwnd);});
    s.log.check("settings_effect_owned_roots_ready_before_gesture",roots_ready&&gui_read&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner&&
        !s.left&&!s.control,"\"test_setup_only\":true,\"all_three_original_owned_visible_noniconic\":"+(roots_ready?std::string("true"):std::string("false"))+
        ",\"source_GUI_known\":"+(gui_read?"true":"false")+",\"capture\":"+(gui_read?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndCapture)):"null")+
        ",\"move_size\":"+(gui_read?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndMoveSize)):"null")+
        ",\"menu_owner\":"+(gui_read?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndMenuOwner)):"null")+
        ",\"actual_product_DOWN_admission_still_required\":true");
}
void settings_guest_move_effect(State& s,bool snap,std::string_view name){
    settings_guest_effect_layout(s);const auto g=down(s,0);const Rect expected=snap?Rect{140,80,540,440}:Rect{134,83,534,443};
    phase(s,0,g,14,3,expected,std::string(name),true);up(s,0,g,expected);
}
void settings_guest_native_move(State& s,LONG requested_dx,LONG dy,std::string_view stage){
    const HWND h=s.frames[0].hwnd;const std::string label(stage);foreground(s,h);
    const auto before=visible(h);const auto anchor=hit_point(h,HTCAPTION);RECT positioning{},client{};
    if(!before||!anchor||!GetWindowRect(h,&positioning)||!GetClientRect(h,&client))
        throw std::runtime_error("settings_native_Move_original_geometry_unknown");
    cursor(s,*anchor);Sleep(100);
    GUITHREADINFO initial{sizeof(initial)};RECT fresh{};DWORD_PTR region{};
    const bool queried=SendMessageTimeoutW(h,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(anchor->x),static_cast<short>(anchor->y)),
        SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&region)!=FALSE;
    const bool positioning_known=GetWindowRect(h,&fresh)!=FALSE,gui_known=GetGUIThreadInfo(s.owner,&initial)!=FALSE;
    const auto caption=native_caption_observation(h,*anchor);
    const auto actual_visible=visible(h);
    const bool fresh_context=own(s,h)&&GetForegroundWindow()==h&&WindowFromPoint(*anchor)==h&&queried&&
        static_cast<int>(region)==HTCAPTION&&caption.result()==v1::CaptionHit::Caption&&!IsIconic(h)&&!IsZoomed(h)&&client.left<client.right&&client.top<client.bottom&&
        actual_visible==before&&positioning_known&&EqualRect(&positioning,&fresh)&&gui_known&&
        !initial.hwndCapture&&!initial.hwndMoveSize&&!initial.hwndMenuOwner;
    s.log.check(label+"_fresh_native_caption_before_DOWN",fresh_context,
        "\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+",\"source_iconic\":"+(IsIconic(h)?"true":"false")+
        ",\"actual_visible\":"+(actual_visible?rect(*actual_visible):"null")+",\"actual_positioning\":"+
        (positioning_known?rect({fresh.left,fresh.top,fresh.right,fresh.bottom}):"null")+",\"source_GUI_known\":"+(gui_known?"true":"false")+
        ",\"actual_region\":"+(queried?std::to_string(region):"null")+",\"actual_foreground\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(GetForegroundWindow()))+",\"point\":["+std::to_string(anchor->x)+","+std::to_string(anchor->y)+"]"+
        ",\"selected_caption_facts\":"+native_caption_json(caption));
    const LONG drag_x=GetSystemMetrics(SM_CXDRAG),drag_y=GetSystemMetrics(SM_CYDRAG);
    if(drag_x<0||drag_y<0||drag_x>128||drag_y>128||requested_dx<=0)
        throw std::runtime_error("settings_native_Move_drag_metrics_out_of_test_bounds");
    const LONG trigger_dx=drag_x+1,dx=std::max(requested_dx,trigger_dx+1);const auto expected=translated(*before,dx,dy);
    const auto mark=read_product(s).size();const auto starts=s.frames[0].starts.load(),ends=s.frames[0].ends.load();
    const auto native_before=native_move_boundary_json(s.frames[0]);
    bool up_attempted=false;
    try{
        button(s,true);
        // Supply an actual displacement before waiting for entry. The prior
        // driver waited for START without ever sending its planned movement;
        // this does not presume which Windows internal path needs a threshold.
        cursor(s,{anchor->x+trigger_dx,anchor->y});
        const bool started=wait([&]{return s.frames[0].starts.load()==starts+1;});
        const auto native_after_start=native_move_boundary_json(s.frames[0]);
        s.log.check(label+"_actual_native_START",started,
            "\"motion_before_START_wait\":true,\"system_drag_metrics\":["+std::to_string(drag_x)+","+std::to_string(drag_y)+"]"+
            ",\"source_native_messages_before\":"+native_before+",\"source_native_messages_after_START_wait\":"+native_after_start);
        cursor(s,{anchor->x+dx,anchor->y+dy});
        s.log.check(label+"_actual_native_geometry",wait([&]{return visible(h)==expected;}),"\"expected\":"+rect(expected)+
            ",\"actual\":"+(visible(h)?rect(*visible(h)):"null"));
        up_attempted=true;button(s,false);
        s.log.check(label+"_native_END_no_product_writer",wait([&]{return s.frames[0].ends.load()==ends+1;})&&
            record(s,mark,"gesture_start").empty()&&record(s,mark,"placement").empty()&&visible(h)==expected,
            "\"ordinary_native_Move_preserved\":true,\"actual_START_and_END_required\":true");
    }catch(...){
        bool cleanup_up=false;if(s.left&&!up_attempted){up_attempted=true;try{button(s,false);cleanup_up=true;}catch(...){};}
        GUITHREADINFO observed{sizeof(observed)};const bool idle=wait([&]{return own(s,h)&&GetGUIThreadInfo(s.owner,&observed)&&
            !observed.hwndCapture&&!observed.hwndMoveSize&&!observed.hwndMenuOwner;},1500);
        RECT actual{},actual_client{};const bool rect_known=GetWindowRect(h,&actual)!=FALSE,client_known=GetClientRect(h,&actual_client)!=FALSE;
        const auto native_after_cleanup=native_move_boundary_json(s.frames[0]);
        s.log.emit("settings_native_Move_failure_cleanup","\"stage\":"+quote(stage)+",\"cleanup_UP_inserted\":"+(cleanup_up?"true":"false")+
            ",\"UP_attempted\":"+(up_attempted?"true":"false")+",\"driver_left_outstanding\":"+(s.left?"true":"false")+
            ",\"exact_source_GUI_idle\":"+(idle?"true":"false")+",\"source_iconic\":"+(IsIconic(h)?"true":"false")+
            ",\"source_zoomed\":"+(IsZoomed(h)?"true":"false")+",\"actual_positioning\":"+(rect_known?rect({actual.left,actual.top,actual.right,actual.bottom}):"null")+
            ",\"actual_client\":"+(client_known?rect({actual_client.left,actual_client.top,actual_client.right,actual_client.bottom}):"null")+
            ",\"actual_START_delta\":"+std::to_string(s.frames[0].starts.load()-starts)+",\"actual_END_delta\":"+std::to_string(s.frames[0].ends.load()-ends)+
            ",\"source_native_messages_before\":"+native_before+",\"source_native_messages_after_cleanup\":"+native_after_cleanup+
            ",\"original_failure_preserved\":true,\"cleanup_send_not_NormalUp_or_delivery_PASS\":true");
        throw;
    }
}
void settings_guest_exclusion(State& s,void(*while_excluded)(State&)){
    namespace v=panebind::core::v1;const auto original=settings_guest_configuration(s);
    settings_guest_effect_layout(s);
    foreground(s,s.frames[0].hwnd);const auto point=hit_point(s.frames[0].hwnd,HTCAPTION);if(!point)throw std::runtime_error("rule_exact_source_caption_missing");cursor(s,*point);Sleep(100);
    auto gui=gui_open(s);gui_page_select(s,gui,4);
    // Exercise cancellation from the same multiline EDIT that receives focus
    // after Add Rule. This is a real guest key, not a close/command message.
    gui_click(s,gui,gui_number(s,gui,L"rule.编号"));gui_press(s,gui,VK_ESCAPE);
    s.log.check("rule_multiline_actual_Escape_cancels_without_save",wait([&]{return !IsWindow(gui.root);})&&
        settings_guest_configuration(s)==original,"\"input_kind\":\"actual_Escape_from_rules_EDIT\"");
    gui=gui_open(s);gui_page_select(s,gui,4);const auto restricted=gui_find(s,gui,L"Button",L"仅限制为该窗口类名");
    if(gui_read(s,gui,restricted,BM_GETCHECK)!=BST_CHECKED)gui_click(s,gui,restricted);
    s.log.check("rule_actual_class_checkbox",wait([&]{return gui_read(s,gui,restricted,BM_GETCHECK)==BST_CHECKED;}));
    settings_guest_activate(s,gui,GetDlgItem(gui_page(s,gui),115));settings_guest_acknowledge(s,gui,L"已将重新核验的应用加入排除草稿");gui_save_enter(s,gui);
    const auto excluded=settings_guest_configuration(s);std::wstring fixture_exe(32768,L'\0');
    const auto exe_chars=GetModuleFileNameW(nullptr,fixture_exe.data(),static_cast<DWORD>(fixture_exe.size()));
    if(!exe_chars||exe_chars>=fixture_exe.size())throw std::runtime_error("rule_actual_fixture_executable_unknown");fixture_exe.resize(exe_chars);
    const bool exact_rule=excluded.rules.size()==original.rules.size()+1&&std::any_of(excluded.rules.begin(),excluded.rules.end(),[&](const auto& rule){
        return v::settings_detail::ascii_fold(rule.exe)==v::settings_detail::ascii_fold(utf8(fixture_exe))&&
            rule.excluded&&rule.window_class==utf8(s.class_name)&&rule.feature_mask==0xffffffffU;});
    s.log.check("rule_actual_GUI_added_exact_source_class",exact_rule);
    settings_guest_effect_layout(s);settings_guest_native_move(s,36,21,"excluded_Move");
    if(while_excluded)while_excluded(s);
    std::string rules;for(const auto row:v::settings_detail::split(v::serialize(original),'\n'))if(row.starts_with("rule_count=")||row.starts_with("rule.")){
        if(!rules.empty())rules+="\r\n";rules+=row;}
    gui=gui_open(s);gui_page_select(s,gui,4);std::wstring wide_rules(rules.begin(),rules.end());
    settings_guest_replace(s,gui,gui_number(s,gui,L"rule.编号"),wide_rules);gui_save_enter(s,gui);
    s.log.check("rule_actual_GUI_removed_exclusion",settings_guest_configuration(s).rules==original.rules);settings_guest_move_effect(s,true,"rule_unexcluded_next_actual_Move");
}
void settings_guest_exclusion(State& s){settings_guest_exclusion(s,nullptr);}
void settings_guest_attraction(State& s,int value){
    const auto gui=gui_open(s);gui_page_select(s,gui,1);gui_edit_number(s,gui,gui_number(s,gui,L"吸引距离"),static_cast<unsigned>(value));gui_save_enter(s,gui);
    s.log.check("settings_actual_attraction_saved",settings_guest_configuration(s).attraction_dip==value);
}
void settings_guest_fault_diagnostic(State& s,const SettingsGui& gui,std::string_view actual_failure);
[[maybe_unused]] void settings_guest_save_failure(State& s){
    // TEST ONLY: a reversible attribute fault on this exact Run's portable
    // settings file. No DACL, host directory, product API or registry mutation.
    settings_guest_scope(s);const auto original=settings_guest_configuration(s);
    const auto path=s.product.parent_path()/L"settings.ini";const auto bytes=read(path);const auto original_hash=hash(path);
    if(!original.enabled||!original.magnet_move||original.attraction_dip<6||bytes.empty()||original_hash.empty()||s.left||s.control||s.explorer[0]||s.applications[0])
        throw std::runtime_error("save_failure_exact_enabled_owned_baseline_required");
    const auto attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_READONLY)))
        throw std::runtime_error("save_failure_existing_file_not_plain_writable_owned_input");
    // Share DELETE as well as READ/WRITE: this identity observation must NOT
    // turn the intended READONLY fault into an accidental sharing violation.
    Handle held(CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    BY_HANDLE_FILE_INFORMATION baseline{};
    if(held.value==INVALID_HANDLE_VALUE||!GetFileInformationByHandle(held.value,&baseline)||baseline.nNumberOfLinks!=1||
       (baseline.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
       baseline.nFileSizeHigh||baseline.nFileSizeLow!=bytes.size())throw std::runtime_error("save_failure_owned_file_identity_unknown");
    const auto protected_attributes=(attributes&~FILE_ATTRIBUTE_NORMAL)|FILE_ATTRIBUTE_READONLY;
    const auto exact_file=[&]{
        settings_guest_scope(s);
        Handle current(CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
            nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));BY_HANDLE_FILE_INFORMATION actual{};
        return current.value!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(current.value,&actual)&&actual.nNumberOfLinks==1&&
            !(actual.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&
            actual.dwVolumeSerialNumber==baseline.dwVolumeSerialNumber&&actual.nFileIndexHigh==baseline.nFileIndexHigh&&
            actual.nFileIndexLow==baseline.nFileIndexLow&&actual.ftCreationTime.dwHighDateTime==baseline.ftCreationTime.dwHighDateTime&&
            actual.ftCreationTime.dwLowDateTime==baseline.ftCreationTime.dwLowDateTime&&read(path)==bytes&&hash(path)==original_hash;
    };
    bool protection_may_exist=false;
    const auto restore=[&]{
        if(!protection_may_exist)return true;
        const bool exact=exact_file();const auto actual=GetFileAttributesW(path.c_str());
        bool called=false,returned=false;DWORD error{};
        if(exact&&actual==protected_attributes){called=true;SetLastError(0);returned=SetFileAttributesW(path.c_str(),attributes)!=FALSE;error=returned?0:GetLastError();}
        const bool confirmed=exact&&(actual==attributes||(returned&&GetFileAttributesW(path.c_str())==attributes))&&exact_file();
        s.log.emit("settings_save_fault_attribute_cleanup","\"fault_kind\":\"FILE_ATTRIBUTE_READONLY\",\"exact_file_identity_and_original_bytes\":"+
            std::string(exact?"true":"false")+",\"SetFileAttributes_called\":"+(called?"true":"false")+",\"SetFileAttributes_returned\":"+
            (called?(returned?"true":"false"):"null")+",\"error\":"+std::to_string(error)+",\"original_attributes_restored\":"+(confirmed?"true":"false")+
            ",\"unknown_file_or_attributes_not_modified\":true,\"DACL_changed\":false");
        if(confirmed)protection_may_exist=false;return confirmed;
    };
    std::array<Rect,3> before{};for(std::size_t n=0;n<before.size();++n){const auto actual=visible(s.frames[n].hwnd);
        if(!own(s,s.frames[n].hwnd)||!actual)throw std::runtime_error("save_failure_original_owned_geometry_unknown");before[n]=*actual;}
    try{
        const auto gui=gui_open(s);gui_page_select(s,gui,1);const auto edit=gui_number(s,gui,L"吸引距离");gui_edit_number(s,gui,edit,0);
        if(!exact_file()||GetFileAttributesW(path.c_str())!=attributes)throw std::runtime_error("save_failure_file_changed_before_attribute_fault");
        protection_may_exist=true;SetLastError(0);const bool protected_set=SetFileAttributesW(path.c_str(),protected_attributes)!=FALSE;
        const DWORD protection_error=protected_set?0:GetLastError();
        s.log.check("settings_save_readonly_fault_applied",protected_set&&exact_file()&&GetFileAttributesW(path.c_str())==protected_attributes,
            "\"fault_kind\":\"FILE_ATTRIBUTE_READONLY\",\"original_sha256\":"+quote(original_hash)+",\"original_attributes\":"+
            std::to_string(attributes)+",\"protected_attributes\":"+std::to_string(protected_attributes)+",\"SetFileAttributes_error\":"+
            std::to_string(protection_error)+",\"DACL_permission_experiment\":false,\"save_failure_not_inferred\":true");
        const auto mark=read_product(s).size();settings_guest_activate(s,gui,GetDlgItem(gui.root,IDOK));
        settings_guest_acknowledge(s,gui,L"保存或应用失败，原有效配置应保持不变。",L"PaneBind：设置未更改");
        const auto notice=record(s,mark,"notice");
        s.log.check("settings_actual_save_rejected_original_file_and_draft_retained",notice.find("无法原子保存配置，原配置未覆盖")!=notice.npos&&
            exact_file()&&settings_guest_configuration(s)==original&&gui_identity(s,gui,edit)&&gui_text(s,gui,edit)==L"0",
            "\"fault_kind\":\"FILE_ATTRIBUTE_READONLY\",\"actual_product_failure_notice\":"+quote(notice)+
            ",\"ReplaceFile_native_error\":null,\"ReplaceFile_native_error_observed\":false,\"original_sha256\":"+quote(original_hash)+
            ",\"draft_still_open\":true,\"generation_unchanged\":true,\"whole_M11F05_PASS_inferred\":false");
        // Export beside, not into, the READONLY settings file. The failure
        // must be real and already acknowledged; no diagnostic input is seeded.
        settings_guest_fault_diagnostic(s,gui,"无法原子保存配置，原配置未覆盖。");
        s.log.check("settings_save_fault_restored_before_continuation",restore());
        // Settings is modeless. Exercise the old live policy without saving or
        // cancelling the failed draft; re-opening brings this same draft back.
        settings_guest_move_effect(s,true,"save_rejected_old_live_magnet_policy");
        const auto resumed=gui_open(s);gui_page_select(s,resumed,1);
        s.log.check("settings_failed_draft_survives_old_policy_operation",resumed.root==gui.root&&gui_text(s,resumed,gui_number(s,resumed,L"吸引距离"))==L"0"&&
            read(path)==bytes&&settings_guest_configuration(s)==original);
        gui_save_enter(s,resumed);const auto saved=settings_guest_configuration(s);auto wanted=original;wanted.attraction_dip=0;wanted.generation=saved.generation;
        s.log.check("settings_after_fault_same_draft_save_succeeds",saved==wanted&&saved.generation>original.generation&&GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES&&
            !(GetFileAttributesW(path.c_str())&FILE_ATTRIBUTE_READONLY),"\"actual_saved_sha256\":"+quote(hash(path)));
        const auto saved_bytes=read(path),saved_hash=hash(path);const auto old_pid=s.product_pid;const auto old_birth=process_birth(s.process);
        const auto stop_mark=read_product(s).size();const auto driver_mark=s.log.mark();cleanup(s);
        const auto shutdown=record(s,stop_mark,"shutdown");std::string input_recovery;bool forced=false;
        std::istringstream rows(s.log.since(driver_mark));std::string row;
        while(std::getline(rows,row))if(row.ends_with('}')){forced=forced||row.find("\"own_product_forced_termination\":true")!=row.npos;
            if(row.find("\"type\":\"post_stop_actual_client_click\"")!=row.npos)input_recovery=row;}
        s.log.check("settings_save_failure_recovery_normal_stop_before_restart",!s.process&&!forced&&number(shutdown,"\"process_id\":")==old_pid&&
            shutdown.find("\"own_hooks_stopped\":true")!=shutdown.npos&&shutdown.find("\"own_overlay_destroyed\":true")!=shutdown.npos&&
            shutdown.find("\"native_worker_exited\":true")!=shutdown.npos&&shutdown.find("\"issued_native_completion_unknown\":false")!=shutdown.npos&&
            input_recovery.find("\"actual_down_up_delivered\":true")!=input_recovery.npos,
            "\"actual_stop_receipt\":"+quote(shutdown)+",\"actual_post_stop_input\":"+quote(input_recovery));
        const auto start_mark=read_product(s).size();s.log.check("settings_save_failure_same_EXE_restart",launch(s,L"--diagnostic",false));
        s.log.check("settings_save_failure_actual_restart_identity",wait([&]{return !record(s,start_mark,"startup").empty();})&&s.process&&medium(s.process)&&
            (s.product_pid!=old_pid||process_birth(s.process)!=old_birth)&&read(path)==saved_bytes&&hash(path)==saved_hash);
        const auto restarted=gui_open(s);gui_page_select(s,restarted,1);
        s.log.check("settings_recovered_save_actual_restart_keeps_new_value",gui_text(s,restarted,gui_number(s,restarted,L"吸引距离"))==L"0"&&settings_guest_configuration(s)==saved);
        gui_press(s,restarted,VK_ESCAPE);s.log.check("settings_save_failure_restart_draft_cancel",wait([&]{return !IsWindow(restarted.root);}));
        settings_guest_move_effect(s,false,"save_recovery_restart_zero_attraction_live_effect");settings_guest_attraction(s,original.attraction_dip);
        const auto restored=settings_guest_configuration(s);auto expected=original;expected.generation=restored.generation;
        s.log.check("settings_save_failure_original_options_restored",restored==expected);
        for(std::size_t n=0;n<before.size();++n)s.log.check("settings_save_failure_original_owned_layout_restored",own(s,s.frames[n].hwnd)&&fixture_rect(s.frames[n].hwnd,before[n]));
        s.log.emit("settings_save_failure_result","\"result\":\"PASS\",\"fault_kind\":\"FILE_ATTRIBUTE_READONLY\",\"actual_Chinese_failure_modal\":true,"
            "\"old_bytes_and_live_policy_retained\":true,\"same_draft_later_saved_and_restart_verified\":true,\"DACL_test\":false,\"complete_M11F05_not_inferred\":true");
    }catch(...){
        bool restored=false;std::string error;try{restored=restore();}catch(const std::exception& failure){error=failure.what();}
        s.log.emit("settings_save_failure_exception_cleanup","\"attribute_cleanup_confirmed\":"+std::string(restored?"true":"false")+
            ",\"cleanup_error\":"+quote(error)+",\"original_failure_preserved\":true,\"unknown_protection_not_removed\":true,\"functional_PASS_inferred\":false");throw;
    }
}
// Reuse the existing actual-command/geometry helpers also used by the mouse
// mapping cases. Their definitions follow this header in the same fixture TU.
Rect chain_work(HWND h);
void chain_hotkey_exact(LayoutFixtures& set,WORD code,Rect expected,const char* label);

void settings_guest_mapping_conflict(State& s){
    namespace v=panebind::core::v1;
    settings_guest_scope(s);const auto original=settings_guest_configuration(s);
    auto baseline=v::default_settings();baseline.generation=original.generation;
    if(original!=baseline||s.left||s.control||s.explorer[0]||s.applications[0])
        throw std::runtime_error("mapping_conflict_requires_restored_defaults_and_exact_owned_only");
    constexpr int registration=0x50b1;constexpr WORD test_key=VK_F10;
    if(std::any_of(original.hotkeys.begin(),original.hotkeys.end(),[](const auto& binding){return binding.modifiers==3&&binding.key==test_key;})||
       std::none_of(original.hotkeys.begin(),original.hotkeys.end(),[](const auto& binding){return binding.modifiers==3&&binding.key=='C'&&binding.action==v::Action::Center;}))
        throw std::runtime_error("mapping_conflict_test_key_or_old_center_mapping_unavailable");
    for(const int code:std::array{VK_CONTROL,VK_MENU,VK_SHIFT,VK_LWIN,VK_RWIN,VK_F10})
        if(GetAsyncKeyState(code)&0x8000)throw std::runtime_error("mapping_conflict_initial_key_not_released");
    const auto path=s.product.parent_path()/L"settings.ini";const auto bytes=read(path),original_hash=hash(path);
    if(bytes.empty()||original_hash.empty())throw std::runtime_error("mapping_original_settings_identity_unavailable");
    SetLastError(0);bool registered=RegisterHotKey(nullptr,registration,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,test_key)!=FALSE;
    const DWORD register_error=registered?0:GetLastError();
    const auto release=[&]{
        if(!registered)return true;
        SetLastError(0);const bool returned=UnregisterHotKey(nullptr,registration)!=FALSE;const DWORD error=returned?0:GetLastError();
        if(returned)registered=false;
        s.log.emit("settings_mapping_test_conflict_removal","\"actual_UnregisterHotKey_return\":"+std::string(returned?"true":"false")+
            ",\"error\":"+std::to_string(error)+",\"registration_id\":"+std::to_string(registration)+
            ",\"owner_thread\":"+std::to_string(GetCurrentThreadId())+",\"only_successfully_created_registration\":true");
        return returned;
    };
    const auto fragment=[](const v::Settings& config,std::string_view prefix,std::string_view count){
        std::wstring result;const auto serialized=v::serialize(config);
        for(const auto row:v::settings_detail::split(serialized,'\n'))if(row.starts_with(prefix)||row.starts_with(count)){
            for(const char c:row){if(static_cast<unsigned char>(c)>=128)throw std::runtime_error("mapping_fragment_not_ASCII");result+=static_cast<wchar_t>(c);}result+=L"\r\n";
        }if(result.empty()||result.size()>16384)throw std::runtime_error("mapping_fragment_over_existing_bound");return result;
    };
    const auto editor=[&](const SettingsGui& gui,std::wstring_view prefix,bool readonly){
        HWND found{};unsigned count{};
        for(HWND h=GetWindow(gui_page(s,gui),GW_CHILD);h;h=GetWindow(h,GW_HWNDNEXT)){
            if(gui_class(h)!=L"Edit"||!gui_identity(s,gui,h))continue;
            const auto text=settings_guest_text(s,gui,h);const auto style=GetWindowLongPtrW(h,GWL_STYLE);
            if(text.starts_with(prefix)&&(style&ES_MULTILINE)&&((style&ES_READONLY)!=0)==readonly){found=h;++count;}
        }if(count!=1)throw std::runtime_error("mapping_exact_unique_public_editor_not_found");return found;
    };
    const auto legend=[&](const SettingsGui& gui){const auto h=editor(gui,L"动作标识 → 中文行为：",true);return settings_guest_text(s,gui,h);};
    bool configuration_changed=false;
    try{
        s.log.emit("settings_mapping_test_conflict_registration","\"actual_RegisterHotKey_return\":"+std::string(registered?"true":"false")+
            ",\"error\":"+std::to_string(register_error)+",\"registration_id\":"+std::to_string(registration)+
            ",\"owner_thread\":"+std::to_string(GetCurrentThreadId())+",\"modifiers\":3,\"key\":121,\"foreign_registration_touched\":false");
        if(!registered){s.log.failed=true;s.log.emit("settings_mapping_conflict_result","\"result\":\"BLOCKED\",\"reason\":\"test_could_not_reserve_one_unused_nonreserved_hotkey\",\"no_takeover_or_retry\":true");return;}
        LayoutFixtures set{s};set.create();const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<900||work.height()<650)throw std::runtime_error("mapping_conflict_actual_work_area_too_small");
        const Rect initial{work.l+120,work.t+110,work.l+600,work.t+430};
        set.setup({initial},{{120,100,work.width(),work.height()}});
        const Rect centered{work.l+(work.width()-initial.width())/2,work.t+(work.height()-initial.height())/2,
            work.l+(work.width()-initial.width())/2+initial.width(),work.t+(work.height()-initial.height())/2+initial.height()};
        auto wanted=original;wanted.hotkeys.push_back({3,test_key,v::Action::MoveRight});
        const auto requested_fragment=fragment(wanted,"hotkey.","hotkey_count=");
        auto gui=gui_open(s);gui_page_select(s,gui,3);const HWND hotkey_edit=editor(gui,L"hotkey_count=",false);
        const auto public_legend=legend(gui);
        s.log.check("settings_mapping_actual_help_matches_action_identifiers",public_legend.find(L"Center → 居中\r\n")!=public_legend.npos&&
            public_legend.find(L"MoveRight → 向右微移\r\n")!=public_legend.npos,
            "\"actual_readonly_public_legend\":true,\"actions_checked\":[\"Center\",\"MoveRight\"],\"tray_help_claimed\":false");
        settings_guest_replace(s,gui,hotkey_edit,requested_fragment);
        const auto mark=read_product(s).size();settings_guest_activate(s,gui,GetDlgItem(gui.root,IDOK));
        settings_guest_acknowledge(s,gui,L"保存或应用失败，原有效配置应保持不变。",L"PaneBind：设置未更改");
        const auto notice=record(s,mark,"notice");
        s.log.check("settings_mapping_real_registration_conflict_preserves_old_snapshot",notice.find("快捷键注册冲突")!=notice.npos&&
            read(path)==bytes&&hash(path)==original_hash&&settings_guest_configuration(s)==original&&
            gui_identity(s,gui,hotkey_edit)&&settings_guest_text(s,gui,hotkey_edit)==requested_fragment,
            "\"actual_product_notice\":"+quote(notice)+",\"original_sha256\":"+quote(original_hash)+
            ",\"actual_test_registration_held\":true,\"draft_retained\":true,\"generation_unchanged\":true");
        chain_hotkey_exact(set,'C',centered,"settings_mapping_old_registered_Center_survives_rejected_save");
        s.log.check("settings_mapping_conflict_owned_registration_released",release());
        gui=gui_open(s);gui_page_select(s,gui,3);
        s.log.check("settings_mapping_same_failed_draft_after_old_command",settings_guest_text(s,gui,editor(gui,L"hotkey_count=",false))==requested_fragment&&read(path)==bytes);
        gui_save_enter(s,gui);const auto saved=settings_guest_configuration(s);wanted.generation=saved.generation;configuration_changed=true;
        s.log.check("settings_mapping_same_draft_after_release_saved",saved==wanted&&saved.generation>original.generation);
        const UINT dpi=GetDpiForWindow(set.frames[0].hwnd);if(!dpi)throw std::runtime_error("mapping_actual_source_DPI_unknown");
        const LONG step=MulDiv(original.step_dip,static_cast<int>(dpi),96);
        chain_hotkey_exact(set,test_key,translated(centered,step,0),"settings_mapping_new_registered_MoveRight_matches_editor_and_help");
        gui=gui_open(s);gui_page_select(s,gui,3);gui_click(s,gui,GetDlgItem(gui.root,110));
        s.log.check("settings_mapping_actual_page_defaults_and_help_consistent",settings_guest_text(s,gui,editor(gui,L"hotkey_count=",false))==
            fragment(baseline,"hotkey.","hotkey_count=")&&settings_guest_text(s,gui,editor(gui,L"mouse_count=",false))==
            fragment(baseline,"mouse.","mouse_count=")&&gui_text(s,gui,gui_number(s,gui,L"键盘微移"))==std::to_wstring(baseline.step_dip)&&
            legend(gui)==public_legend&&settings_guest_configuration(s)==saved,
            "\"actual_button_id\":110,\"actual_mouse_click\":true,\"not_saved_until_real_Enter\":true");
        gui_save_enter(s,gui);const auto restored=settings_guest_configuration(s);auto expected=original;expected.generation=restored.generation;
        s.log.check("settings_mapping_original_options_restored_after_page_defaults",restored==expected&&restored.generation>saved.generation);
        configuration_changed=false;set.setup({initial},{{120,100,work.width(),work.height()}});
        chain_hotkey_exact(set,'C',centered,"settings_mapping_default_Center_still_executes_actual_command");
        s.log.check("settings_mapping_exact_owned_windows_closed",set.close());
        s.log.emit("settings_mapping_conflict_result","\"result\":\"PASS\",\"real_RegisterHotKey_conflict_and_Chinese_notice\":true,"
            "\"old_mapping_and_actual_command_preserved\":true,\"own_registration_released\":true,\"same_draft_new_mapping_executed\":true,"
            "\"actual_page_defaults_and_public_action_help\":true,\"product_EXE_unchanged\":true,\"human_uat\":\"NOT_RUN\"");
    }catch(...){
        bool removed=false;try{removed=release();}catch(...){}
        s.log.emit("settings_mapping_conflict_exception_cleanup","\"own_registration_absent_confirmed\":"+std::string(removed?"true":"false")+
            ",\"configuration_may_have_changed\":"+(configuration_changed?"true":"false")+
            ",\"independent_phase_real_GUI_restore_still_required\":true,\"original_failure_preserved\":true");throw;
    }
}
void settings_guest_import_export(State& s){
    // An independent non-drag settings check; original five-button mappings
    // keep their existing separate driver and are not rerun by this function.
    settings_guest_mapping_conflict(s);
    namespace v=panebind::core::v1;const auto original=settings_guest_configuration(s);const auto config_path=s.product.parent_path()/L"settings.ini";const auto bytes=read(config_path);
    const auto suffix=s.configuration=="Debug"?L"debug":L"release";const auto exported=s.output/(s.run+L"-"+suffix+L"-settings-export.conf");
    if(fs::exists(exported))throw std::runtime_error("settings_export_requires_new_RunId_path");
    auto gui=gui_open(s);gui_page_select(s,gui,1);gui_edit_number(s,gui,gui_number(s,gui,L"吸引距离"),0);
    settings_guest_file_path(s,gui,true,exported);const auto actual=v::parse(read(exported));auto expected=original;expected.attraction_dip=0;
    s.log.check("settings_native_export_actual_parsed_draft",actual.value&&*actual.value==expected&&read(config_path)==bytes,
        "\"file\":"+quote(utf8(exported.filename().wstring()))+",\"sha256\":"+quote(hash(exported))+",\"runtime_unchanged\":true");
    gui_press(s,gui,VK_ESCAPE);s.log.check("settings_export_then_cancel_runtime_unchanged",wait([&]{return !IsWindow(gui.root);})&&read(config_path)==bytes);
    for(const bool save:{false,true}){gui=gui_open(s);settings_guest_file_path(s,gui,false,exported);gui_page_select(s,gui,1);
        s.log.check("settings_native_import_actual_draft",gui_text(s,gui,gui_number(s,gui,L"吸引距离"))==L"0"&&read(config_path)==bytes);
        if(save){gui_save_enter(s,gui);const auto imported=settings_guest_configuration(s);expected.generation=imported.generation;
            s.log.check("settings_import_GUI_save_applies",imported==expected&&imported.generation>original.generation);settings_guest_move_effect(s,false,"imported_zero_attraction_next_actual_Move");}
        else{gui_press(s,gui,VK_ESCAPE);s.log.check("settings_native_import_cancel_no_runtime_write",wait([&]{return !IsWindow(gui.root);})&&read(config_path)==bytes);}}
    settings_guest_attraction(s,original.attraction_dip);
}
std::vector<fs::path> settings_guest_diagnostic_inventory(State& s){
    settings_guest_scope(s);std::vector<fs::path> files;
    for(const auto& entry:fs::directory_iterator(s.product.parent_path())){
        const auto name=entry.path().filename().wstring();
        // atomic_save's diagnostic-<tick>.txt.tmp.<pid>.<tick> can disappear
        // during rename. Only the exact final filename is a result candidate.
        if(name.starts_with(L"diagnostic-")&&name.size()>=16&&name.ends_with(L".txt")&&
            std::all_of(name.begin()+11,name.end()-4,[](wchar_t c){return c>=L'0'&&c<=L'9';})){
            files.push_back(entry.path());if(files.size()>16)throw std::runtime_error("guest_run_diagnostic_inventory_over_bound");}
    }
    return files;
}
void settings_guest_diagnostic_result(State& s,const std::vector<fs::path>& previous,std::string_view entry){
    const auto directory=s.product.parent_path();fs::path exported;
    const bool created=wait([&]{const auto files=settings_guest_diagnostic_inventory(s);std::vector<fs::path> fresh;
        for(const auto& path:files)if(std::find(previous.begin(),previous.end(),path)==previous.end())fresh.push_back(path);
        if(fresh.size()!=1)return false;exported=fresh[0];return fs::is_regular_file(exported)&&fs::file_size(exported)>0&&fs::file_size(exported)<=300*1024;});
    const std::string prefix=entry=="tray_menu"?"tray":"settings_GUI";
    s.log.check(prefix+"_actual_creates_new_diagnostic",created,"\"actual_entry\":"+quote(entry));
    const auto attributes=GetFileAttributesW(exported.c_str());const auto name=exported.filename().wstring();
    if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||name.size()<16||!name.ends_with(L".txt")||
       !std::all_of(name.begin()+11,name.end()-4,[](wchar_t c){return c>=L'0'&&c<=L'9';}))throw std::runtime_error("guest_diagnostic_not_exact_new_plain_run_file");
    // serialize()'s fixed public comment belongs to the parsed settings,
    // rather than the diagnostic header whitelist. Both UI routes share it.
    constexpr std::string_view config_header="\n# PaneBind configuration v1; UTF-8; modifiers Alt=1 Ctrl=2 Shift=4 Super=8\n";
    const auto text=read(exported,300*1024);const auto config=text.find(config_header),tail=text.find("\n\nRecent failure: ");
    if(config==text.npos||tail==text.npos||tail<=config||s.product_source.empty()||!text.starts_with("PaneBind V1\nSOURCE_SHA="+s.product_source+"\nHUMAN_UAT=PENDING\n")||
       !text.ends_with("\nWindow titles, document contents and full rule paths are not collected.\n"))throw std::runtime_error("guest_diagnostic_structure_or_source_invalid");
    std::istringstream header(text.substr(0,config+1));std::string line;bool known_headers=true;
    while(std::getline(header,line)){
        const bool known=line=="PaneBind V1"||line=="SOURCE_SHA="+s.product_source||line=="HUMAN_UAT=PENDING"||
            line.starts_with("OS_BUILD=")||line.starts_with("OS_UBR=")||line.starts_with("LAST_GESTURE_END=")||
            line.starts_with("HOOK_OWNER_RUNNING=")||line.starts_with("DISPLAY_DPI=")||line.starts_with("LAST_WINDOW_CLASS=");
        known_headers=known_headers&&known;
    }
    const auto parsed=panebind::core::v1::parse(std::string_view(text).substr(config+1,tail-config));
    if(!known_headers||!parsed.value)throw std::runtime_error("guest_diagnostic_actual_headers_or_settings_not_parseable");
    bool private_values_absent=text.find(utf8(directory.wstring()))==text.npos&&text.find(utf8(s.output.wstring()))==text.npos&&
        text.find("INTENTIONALLY_INVALID_PANEBIND_GUEST_CONFIG")==text.npos;
    for(const auto& frame:s.frames){std::array<wchar_t,128> title{};const auto n=GetWindowTextW(frame.hwnd,title.data(),static_cast<int>(title.size()));
        if(!own(s,frame.hwnd)||n<=0||n>=static_cast<int>(title.size()))throw std::runtime_error("guest_privacy_exact_owned_title_unavailable");
        if(text.find(utf8(std::wstring(title.data(),n)))!=text.npos)private_values_absent=false;}
    std::size_t index{};for(const auto& rule:parsed.value->rules)if(rule.exe!="redacted-app-"+std::to_string(++index)+".exe")private_values_absent=false;
    const auto digest=hash(exported);
    s.log.check(prefix+"_diagnostic_actual_parse_and_privacy",private_values_absent&&!digest.empty(),"\"actual_entry\":"+quote(entry)+
        ",\"new_run_file\":"+quote(utf8(exported.filename().wstring()))+",\"sha256\":"+quote(digest)+
        ",\"actual_owned_titles_and_run_paths_absent\":"+(private_values_absent?"true":"false")+
        ",\"no_real_documents_opened\":true,\"not_universal_privacy_validation\":true,\"tray_or_full_M12_PASS_inferred\":false");
}
void settings_guest_fault_diagnostic(State& s,const SettingsGui& gui,std::string_view actual_failure){
    settings_guest_scope(s);const auto previous=settings_guest_diagnostic_inventory(s);HWND diagnostic{};unsigned matches{};
    for(HWND child=GetWindow(gui.root,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
        if(gui_class(child)==L"Button"&&gui_identity(s,gui,child)&&gui_text(s,gui,child)==L"诊断 (&D)"&&IsWindowVisible(child)&&IsWindowEnabled(child)){
            diagnostic=child;++matches;}
    s.log.check("settings_save_failure_diagnostic_actual_button_identified",matches==1,
        "\"actual_entry\":\"settings_footer_after_actual_readonly_save_failure\",\"fault_message_seeded\":false");
    gui_click(s,gui,diagnostic);
    // Reuse the real bounded file/header/settings parser and the existing
    // exact-owned-title, rule-path and Run-path privacy checks.
    settings_guest_diagnostic_result(s,previous,"settings_footer_after_actual_readonly_save_failure");
    const auto files=settings_guest_diagnostic_inventory(s);fs::path exported;unsigned fresh{};
    for(const auto& path:files)if(std::find(previous.begin(),previous.end(),path)==previous.end()){exported=path;++fresh;}
    if(fresh!=1)throw std::runtime_error("save_failure_diagnostic_new_file_identity_changed");
    const auto attributes=GetFileAttributesW(exported.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))
        throw std::runtime_error("save_failure_diagnostic_no_longer_plain");
    const auto text=read(exported,300*1024),digest=hash(exported);
    const std::string required_tail="\n\nRecent failure: "+std::string(actual_failure)+
        "\nWindow titles, document contents and full rule paths are not collected.\n";
    s.log.check("settings_fault_diagnostic_contains_actual_save_failure",!actual_failure.empty()&&text.ends_with(required_tail)&&!digest.empty(),
        "\"actual_entry\":\"settings_footer_after_actual_readonly_save_failure\",\"new_run_file\":"+quote(utf8(exported.filename().wstring()))+
        ",\"sha256\":"+quote(digest)+",\"actual_failure_origin\":\"preceding_product_READONLY_save_notice_and_acknowledged_modal\""+
        ",\"fault_message_seeded\":false,\"not_universal_privacy_validation\":true,\"complete_M12F02_PASS_inferred\":false");
}
[[maybe_unused]] void owned_settings_diagnostic_case(State& s){
    settings_guest_scope(s);const auto configuration=s.product.parent_path()/L"settings.ini";const auto original=read(configuration);
    auto& frame=s.frames[0];const HWND source=frame.hwnd;RECT client{};GUITHREADINFO context{sizeof(context)};
    if(!own(s,source)||!IsWindowVisible(source)||IsIconic(source)||s.left||s.control||!desktop()||
       !GetClientRect(source,&client)||client.left>=client.right||client.top>=client.bottom||
       !GetGUIThreadInfo(s.owner,&context)||context.hwndCapture||context.hwndMoveSize||context.hwndMenuOwner)
        throw std::runtime_error("settings_diagnostic_test_setup_exact_client_not_idle");
    POINT point{client.left+(client.right-client.left)/2,client.top+(client.bottom-client.top)/2};
    if(!ClientToScreen(source,&point)||point.x<SHRT_MIN||point.x>SHRT_MAX||point.y<SHRT_MIN||point.y>SHRT_MAX)
        throw std::runtime_error("settings_diagnostic_test_setup_client_screen_unknown");
    cursor(s,point);DWORD_PTR hit{};
    if(!own(s,source)||!desktop()||WindowFromPoint(point)!=source||
       !GetGUIThreadInfo(s.owner,&context)||context.hwndCapture||context.hwndMoveSize||context.hwndMenuOwner||
       !SendMessageTimeoutW(source,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y)),
            SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&hit)||hit!=HTCLIENT)
        throw std::runtime_error("settings_diagnostic_test_setup_client_hit_not_exact");
    const auto downs=frame.client_down[0].load(),ups=frame.client_up[0].load();
    button(s,true);button(s,false);
    const bool activated=wait([&]{return own(s,source)&&GetForegroundWindow()==source&&
        frame.client_down[0].load()==downs+1&&frame.client_up[0].load()==ups+1&&
        GetGUIThreadInfo(s.owner,&context)&&!context.hwndCapture&&!context.hwndMoveSize&&!context.hwndMenuOwner;});
    s.log.check("settings_diagnostic_test_setup_actual_client_activation",activated,
        "\"automated_test_setup\":true,\"actual_client_DOWN_UP_required\":true,\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+
        ",\"counts_before\":["+std::to_string(downs)+","+std::to_string(ups)+"],\"counts_after\":["+
        std::to_string(frame.client_down[0].load())+","+std::to_string(frame.client_up[0].load())+
        "],\"actual_foreground_matches\":"+(GetForegroundWindow()==source?"true":"false")+",\"SetForegroundWindow_called\":false,\"input_repeated\":false");
    const auto previous=settings_guest_diagnostic_inventory(s);const auto gui=gui_open(s);HWND diagnostic{};unsigned matches{};
    for(HWND child=GetWindow(gui.root,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
        if(gui_class(child)==L"Button"&&gui_identity(s,gui,child)&&gui_text(s,gui,child)==L"诊断 (&D)"&&IsWindowVisible(child)&&IsWindowEnabled(child)){
            diagnostic=child;++matches;}
    s.log.check("settings_GUI_diagnostic_actual_button_identified",matches==1,"\"actual_entry\":\"settings_footer\",\"tray_menu_used\":false");
    gui_click(s,gui,diagnostic);settings_guest_diagnostic_result(s,previous,"settings_footer");
    gui_press(s,gui,VK_ESCAPE);
    s.log.check("settings_GUI_diagnostic_return_and_configuration_unchanged",wait([&]{return !IsWindow(gui.root);})&&read(configuration)==original,
        "\"actual_Escape\":true,\"settings_saved_by_test\":false,\"tray_or_full_M12_PASS_inferred\":false");
}
// Real GUI fault on a fresh LOCAL guest copy. Never change a mapped Output
// DACL: that directory is backed by the host I: evidence directory.
void settings_guest_diagnostic_export_failure(State& s){
    settings_guest_scope(s);
    const auto original_product=s.product,original_log=s.product_log;
    const auto original_config=read(original_product.parent_path()/L"settings.ini"),binary_hash=hash(original_product);
    const auto original_settings=settings_guest_configuration(s);
    if(s.diagnostic_export_failure_local||s.left||s.control||s.explorer[0]||s.applications[0]||original_settings.startup||
       original_config.empty()||binary_hash.empty()||binary_hash!=hash(s.input_product))
        throw std::runtime_error("diagnostic_export_failure_requires_idle_exact_owned_portable_scope");
    const auto suffix=s.configuration=="Debug"?L"Debug":L"Release";
    const auto base=fs::path(L"C:\\PaneBindV1\\ExportFailure"),run_parent=base/s.run,local=run_parent/suffix;
    const auto evidence=s.output/(s.run+L"-diagnostic-export-failure-"+suffix);
    if(fs::exists(local)||fs::exists(evidence))throw std::runtime_error("diagnostic_export_failure_requires_fresh_local_and_evidence_paths");
    for(const auto& parent:std::array{fs::path(L"C:\\PaneBindV1"),base}){
        if(!fs::exists(parent)&&!CreateDirectoryW(parent.c_str(),nullptr))throw std::runtime_error("diagnostic_local_parent_create_failed");
        if(!plain_directory(parent))throw std::runtime_error("diagnostic_local_parent_not_plain");
    }
    const auto new_marker=[&](const fs::path& parent){const auto value=utf8(s.run)+"\n";
        Handle file(CreateFileW((parent/L"export-failure.marker").c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));DWORD actual{};
        if(file.value==INVALID_HANDLE_VALUE||!WriteFile(file.value,value.data(),static_cast<DWORD>(value.size()),&actual,nullptr)||actual!=value.size()||!FlushFileBuffers(file.value))
            throw std::runtime_error("diagnostic_local_fresh_marker_write_failed");};
    if(fs::exists(run_parent)){
        if(!plain_directory(run_parent)||!marker(run_parent/L"export-failure.marker",s.run))throw std::runtime_error("diagnostic_existing_Run_parent_ownership_unknown");
    }else{if(!CreateDirectoryW(run_parent.c_str(),nullptr)||!plain_directory(run_parent))throw std::runtime_error("diagnostic_Run_parent_create_failed");new_marker(run_parent);}
    if(!CreateDirectoryW(local.c_str(),nullptr)||!plain_directory(local))throw std::runtime_error("diagnostic_fresh_local_directory_create_failed");
    for(const auto* name:{L"panebind.exe",L"portable.flag",L"settings.ini"}){
        const auto from=original_product.parent_path()/name,to=local/name;const auto attributes=GetFileAttributesW(from.c_str());const auto expected=hash(from);
        if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||expected.empty()||
           !CopyFileW(from.c_str(),to.c_str(),TRUE)||hash(to)!=expected)throw std::runtime_error("diagnostic_local_copy_not_exact_new_plain_file");
        s.log.emit("diagnostic_export_failure_local_copy","\"filename\":"+quote(utf8(name))+",\"sha256\":"+quote(expected)+",\"no_product_rebuild\":true");
    }
    new_marker(local);
    Handle directory(CreateFileW(local.c_str(),READ_CONTROL|WRITE_DAC|FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    BY_HANDLE_FILE_INFORMATION identity{};PSECURITY_DESCRIPTOR raw_security{};PACL original_acl{};
    if(directory.value==INVALID_HANDLE_VALUE||!GetFileInformationByHandle(directory.value,&identity)||
       !(identity.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||(identity.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||
       GetSecurityInfo(directory.value,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,&original_acl,nullptr,&raw_security)!=ERROR_SUCCESS||
       !original_acl||!IsValidAcl(original_acl)){
        if(raw_security)LocalFree(raw_security);throw std::runtime_error("diagnostic_local_original_DACL_or_identity_unknown");}
    const auto security=std::unique_ptr<void,decltype(&LocalFree)>(raw_security,&LocalFree);
    SECURITY_DESCRIPTOR_CONTROL control{};DWORD revision{};
    if(!GetSecurityDescriptorControl(raw_security,&control,&revision))throw std::runtime_error("diagnostic_local_DACL_control_unknown");
    const SECURITY_INFORMATION acl_information=DACL_SECURITY_INFORMATION|((control&SE_DACL_PROTECTED)?PROTECTED_DACL_SECURITY_INFORMATION:UNPROTECTED_DACL_SECURITY_INFORMATION);
    std::vector<unsigned char> old_acl(original_acl->AclSize);std::copy_n(reinterpret_cast<unsigned char*>(original_acl),old_acl.size(),old_acl.begin());
    const auto same_directory=[&]{BY_HANDLE_FILE_INFORMATION held{},path{};
        Handle fresh(CreateFileW(local.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        return fresh.value!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(directory.value,&held)&&GetFileInformationByHandle(fresh.value,&path)&&
            (path.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&!(path.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&
            held.dwVolumeSerialNumber==identity.dwVolumeSerialNumber&&held.nFileIndexHigh==identity.nFileIndexHigh&&held.nFileIndexLow==identity.nFileIndexLow&&
            path.dwVolumeSerialNumber==identity.dwVolumeSerialNumber&&path.nFileIndexHigh==identity.nFileIndexHigh&&path.nFileIndexLow==identity.nFileIndexLow;
    };
    const auto current_acl=[&]{PSECURITY_DESCRIPTOR descriptor{};PACL acl{};std::vector<unsigned char> bytes;
        if(same_directory()&&GetSecurityInfo(directory.value,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,&acl,nullptr,&descriptor)==ERROR_SUCCESS&&acl&&IsValidAcl(acl)){
            SECURITY_DESCRIPTOR_CONTROL actual_control{};DWORD actual_revision{};
            if(GetSecurityDescriptorControl(descriptor,&actual_control,&actual_revision)&&
               (actual_control&SE_DACL_PROTECTED)==(control&SE_DACL_PROTECTED)){
                bytes.resize(acl->AclSize);std::copy_n(reinterpret_cast<unsigned char*>(acl),bytes.size(),bytes.begin());}}
        if(descriptor)LocalFree(descriptor);return bytes;};
    bool fault_may_exist=false,original_stopped=false,local_started=false,visible_failure=false,negative_checked=false;
    std::vector<unsigned char> fault_acl;std::string failure,observed_notice;
    const auto restore_acl=[&]{
        if(!fault_may_exist)return current_acl()==old_acl;
        const auto before=current_acl();const bool exact=same_directory()&&(before==fault_acl||before==old_acl);
        const bool attempted=exact&&before!=old_acl;const DWORD result=attempted?SetSecurityInfo(directory.value,SE_FILE_OBJECT,acl_information,nullptr,nullptr,
            reinterpret_cast<PACL>(old_acl.data()),nullptr):ERROR_SUCCESS;
        const bool restored=exact&&result==ERROR_SUCCESS&&current_acl()==old_acl;
        s.log.emit("diagnostic_export_failure_DACL_cleanup","\"exact_guest_local_directory\":"+std::string(exact?"true":"false")+
            ",\"SetSecurityInfo_attempted\":"+(attempted?"true":"false")+",\"native_error\":"+std::to_string(result)+
            ",\"original_DACL_bytes_restored\":"+(restored?"true":"false")+",\"mapped_Output_DACL_changed\":false");
        if(restored)fault_may_exist=false;return restored;
    };
    const auto normal_stop=[&](std::string_view stage){
        if(!s.process)return;const auto pid=s.product_pid;const auto mark=read_product(s).size();const auto driver_mark=s.log.mark();cleanup(s);
        const auto shutdown=record(s,mark,"shutdown"),rows=s.log.since(driver_mark);
        std::string stopped,client;std::istringstream stop_rows(rows);std::string row;
        while(std::getline(stop_rows,row))if(row.ends_with('}')){
            if(row.find("\"type\":\"product_stop\"")!=row.npos)stopped=row;
            if(row.find("\"type\":\"post_stop_actual_client_click\"")!=row.npos)client=row;}
        s.log.check("diagnostic_export_failure_normal_stop",!s.process&&number(shutdown,"\"process_id\":")==pid&&
            shutdown.find("\"own_hooks_stopped\":true")!=shutdown.npos&&shutdown.find("\"own_overlay_destroyed\":true")!=shutdown.npos&&
            shutdown.find("\"native_worker_exited\":true")!=shutdown.npos&&shutdown.find("\"visual_recovery_stopped\":true")!=shutdown.npos&&
            shutdown.find("\"visual_restore_confirmed\":true")!=shutdown.npos&&
            shutdown.find("\"issued_native_completion_unknown\":false")!=shutdown.npos&&rows.find("\"own_product_forced_termination\":true")==rows.npos&&
            stopped.find("\"quit_command_completed\":true")!=stopped.npos&&stopped.find("\"process_exit_observed\":true")!=stopped.npos&&
            stopped.find("\"product_exit_code\":0")!=stopped.npos&&client.find("\"check_requested\":true")!=client.npos&&
            client.find("\"executed\":true")!=client.npos&&client.find("\"actual_down_up_delivered\":true")!=client.npos&&
            client.find("\"DOWN_SendInput_called\":true")!=client.npos&&client.find("\"UP_SendInput_called\":true")!=client.npos,
            "\"stage\":"+quote(stage)+",\"shutdown\":"+quote(shutdown)+",\"product_stop\":"+quote(stopped)+",\"actual_client_recovery\":"+quote(client));
    };
    const auto diagnostic_button=[&](const SettingsGui& gui){HWND found{};unsigned count{};
        for(HWND h=GetWindow(gui.root,GW_CHILD);h;h=GetWindow(h,GW_HWNDNEXT))if(gui_class(h)==L"Button"&&gui_identity(s,gui,h)&&
            gui_text(s,gui,h)==L"诊断 (&D)"&&IsWindowVisible(h)&&IsWindowEnabled(h)){found=h;++count;}
        if(count!=1)throw std::runtime_error("diagnostic_failure_exact_actual_button_not_unique");return found;};
    try{
        original_stopped=true;normal_stop("original_run_before_local_copy");
        if(FindWindowW(L"PaneBind.V1.Application",L"PaneBind"))throw std::runtime_error("diagnostic_original_instance_still_present");
        s.product=local/L"panebind.exe";s.product_log=local/L"panebind.log.jsonl";s.diagnostic_export_failure_local=true;
        s.log.check("diagnostic_failure_local_ordinary_entry",launch(s,L"",false));local_started=true;
        s.log.check("diagnostic_failure_local_identity",wait([&]{return number(record(s,0,"startup"),"\"process_id\":")==s.product_pid;})&&
            hash(s.product)==binary_hash&&read(local/L"settings.ini")==original_config&&settings_guest_configuration(s)==original_settings,
            "\"actual_entry\":\"empty_arguments_same_EXE_hash_guest_local_copy\",\"no_diagnostic_flag\":true");
        const auto gui=gui_open(s);const auto button=diagnostic_button(gui);const auto previous=settings_guest_diagnostic_inventory(s);
        Handle caller_token,product_token;HANDLE raw{};
        if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&raw))throw std::runtime_error("diagnostic_failure_current_token_query_failed");caller_token.value=raw;
        if(!OpenProcessToken(s.process,TOKEN_QUERY,&raw))throw std::runtime_error("diagnostic_failure_product_token_query_failed");product_token.value=raw;
        const auto user=[](HANDLE token){DWORD size{};GetTokenInformation(token,TokenUser,nullptr,0,&size);
            if(size<sizeof(TOKEN_USER)||size>512)throw std::runtime_error("diagnostic_failure_TokenUser_size_not_bounded");std::vector<unsigned char> bytes(size);
            if(!GetTokenInformation(token,TokenUser,bytes.data(),size,&size))throw std::runtime_error("diagnostic_failure_TokenUser_unknown");return bytes;};
        auto caller_user=user(caller_token.value),product_user=user(product_token.value);auto sid=reinterpret_cast<TOKEN_USER*>(caller_user.data())->User.Sid;
        if(!IsValidSid(sid)||!IsValidSid(reinterpret_cast<TOKEN_USER*>(product_user.data())->User.Sid)||
           !EqualSid(sid,reinterpret_cast<TOKEN_USER*>(product_user.data())->User.Sid)||current_acl()!=old_acl)
            throw std::runtime_error("diagnostic_failure_same_actual_SID_or_original_DACL_not_confirmed");
        EXPLICIT_ACCESSW entry{};entry.grfAccessPermissions=FILE_ADD_FILE;entry.grfAccessMode=DENY_ACCESS;entry.grfInheritance=NO_INHERITANCE;
        BuildTrusteeWithSidW(&entry.Trustee,sid);PACL built{};const DWORD built_error=SetEntriesInAclW(1,&entry,original_acl,&built);
        const auto built_owner=std::unique_ptr<void,decltype(&LocalFree)>(built,&LocalFree);
        if(built_error!=ERROR_SUCCESS||!built||!IsValidAcl(built))throw std::runtime_error("diagnostic_failure_bounded_DACL_build_failed");
        fault_acl.resize(built->AclSize);std::copy_n(reinterpret_cast<unsigned char*>(built),fault_acl.size(),fault_acl.begin());
        fault_may_exist=true;const DWORD applied=SetSecurityInfo(directory.value,SE_FILE_OBJECT,acl_information,nullptr,nullptr,built,nullptr);
        s.log.check("diagnostic_failure_exact_local_DACL_applied",applied==ERROR_SUCCESS&&current_acl()==fault_acl,
            "\"native_error\":"+std::to_string(applied)+",\"denied_right\":\"FILE_ADD_FILE\",\"inheritance\":\"NO_INHERITANCE\",\"same_actual_product_and_driver_SID\":true,\"mapped_Output_DACL_changed\":false");
        const auto probe=local/L"denied-create-probe.tmp";SetLastError(0);
        HANDLE denied=CreateFileW(probe.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);const DWORD denied_error=denied==INVALID_HANDLE_VALUE?GetLastError():0;
        if(denied!=INVALID_HANDLE_VALUE){CloseHandle(denied);if(!DeleteFileW(probe.c_str()))throw std::runtime_error("diagnostic_failure_unexpected_probe_cleanup_failed");}
        s.log.check("diagnostic_failure_actual_new_file_denied",denied==INVALID_HANDLE_VALUE&&denied_error==ERROR_ACCESS_DENIED,
            "\"CreateFile_error\":"+std::to_string(denied_error)+",\"not_product_export_evidence\":true");
        const auto mark=read_product(s).size();settings_guest_activate(s,gui,button);
        const bool notice_observed=wait([&]{observed_notice=record(s,mark,"notice");return observed_notice.find("诊断写入失败；未声明导出成功。")!=observed_notice.npos;});
        SettingsGuestModal actual_modal;unsigned visible_checked{},text_queries{};
        visible_failure=wait([&]{
            struct Query {State* state;const SettingsGui* gui;unsigned* text_queries;SettingsGuestModal found;unsigned visited{},matches{};} query{&s,&gui,&text_queries};
            EnumThreadWindows(gui.tid,[](HWND h,LPARAM p)->BOOL{auto& q=*reinterpret_cast<Query*>(p);if(++q.visited>32)return FALSE;
                SettingsGuestModal modal{h,q.gui->tid};if(!settings_guest_modal_identity(*q.state,*q.gui,modal))return TRUE;
                struct Text {unsigned* queries;bool found{};unsigned visited{};} text{q.text_queries};
                EnumChildWindows(h,[](HWND child,LPARAM raw_text)->BOOL{auto& t=*reinterpret_cast<Text*>(raw_text);if(++t.visited>64)return FALSE;
                    if(gui_class(child)!=L"Static"||!IsWindowVisible(child))return TRUE;if(*t.queries>=8)return FALSE;++*t.queries;
                    std::array<wchar_t,1024> value{};DWORD_PTR copied{};
                    if(SendMessageTimeoutW(child,WM_GETTEXT,value.size(),reinterpret_cast<LPARAM>(value.data()),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&copied)&&
                       copied<value.size()&&std::wstring_view(value.data(),copied).find(L"诊断写入失败")!=std::wstring_view::npos)t.found=true;return TRUE;
                },reinterpret_cast<LPARAM>(&text));if(text.found){q.found=modal;++q.matches;}return TRUE;
            },reinterpret_cast<LPARAM>(&query));visible_checked=query.visited;actual_modal=query.found;return query.matches==1&&GetForegroundWindow()==actual_modal.root;
        },1000);
        unsigned artifacts{},children{};for(const auto& file:fs::directory_iterator(local)){
            if(++children>12)throw std::runtime_error("diagnostic_failure_local_inventory_over_bound");
            if(file.path().filename().wstring().starts_with(L"diagnostic-"))++artifacts;}
        negative_checked=notice_observed&&artifacts==previous.size()&&read(local/L"settings.ini")==original_config;
        s.log.emit("diagnostic_export_failure_observation","\"actual_notice\":"+quote(observed_notice)+",\"ordinary_non_diagnostic_entry\":true,\"visible_error_modal_observed\":"+
            std::string(visible_failure?"true":"false")+",\"visible_probe_budget_ms\":1000,\"last_owned_thread_windows_checked\":"+std::to_string(visible_checked)+
            ",\"actual_text_queries\":"+std::to_string(text_queries)+",\"text_query_total_limit\":8,\"per_text_timeout_ms\":100"+
            ",\"diagnostic_final_or_tmp_artifacts\":"+std::to_string(artifacts)+",\"configuration_bytes_unchanged\":"+
            (read(local/L"settings.ini")==original_config?"true":"false")+",\"log_notice_is_not_GUI_feedback\":true");
        if(visible_failure){settings_guest_modal_press(s,gui,actual_modal,VK_RETURN);s.log.check("diagnostic_failure_actual_error_acknowledged",wait([&]{return !IsWindow(actual_modal.root)&&GetForegroundWindow()==gui.root;}));}
        s.log.check("diagnostic_failure_true_negative_no_fake_package",negative_checked);
        s.log.check("diagnostic_failure_original_DACL_restored_before_positive",restore_acl());
        const auto before_success=settings_guest_diagnostic_inventory(s);settings_guest_activate(s,gui,diagnostic_button(gui));
        settings_guest_diagnostic_result(s,before_success,"settings_footer_after_guest_local_DACL_restoration");
        gui_press(s,gui,VK_ESCAPE);s.log.check("diagnostic_failure_recovered_GUI_unchanged",wait([&]{return !IsWindow(gui.root);})&&read(local/L"settings.ini")==original_config);
    }catch(const std::exception& error){failure=error.what();}
    // Missing visible feedback is judged only AFTER all real cleanup paths.
    bool acl_restored=false;try{acl_restored=restore_acl();}catch(const std::exception& error){failure+=";DACL_cleanup:";failure+=error.what();}
    if(local_started||s.product==local/L"panebind.exe")try{normal_stop("guest_local_copy_after_test");}catch(const std::exception& error){failure+=";local_stop:";failure+=error.what();}
    bool archived=false;
    try{
        if(!CreateDirectoryW(evidence.c_str(),nullptr)||!plain_directory(evidence))throw std::runtime_error("diagnostic_failure_new_evidence_directory_failed");
        unsigned copied{},diagnostics{},children{};for(const auto& file:fs::directory_iterator(local)){const auto name=file.path().filename().wstring();
            if(++children>12)throw std::runtime_error("diagnostic_failure_local_archive_inventory_over_bound");
            const bool diagnostic=name.size()>=16&&name.starts_with(L"diagnostic-")&&name.ends_with(L".txt")&&
                std::all_of(name.begin()+11,name.end()-4,[](wchar_t c){return c>=L'0'&&c<=L'9';});
            if(name!=L"panebind.log.jsonl"&&!diagnostic&&name!=L"export-failure.marker")continue;
            const auto attributes=GetFileAttributesW(file.path().c_str());const auto expected=hash(file.path());const auto target=name==L"panebind.log.jsonl"?L"product-log.jsonl":name;
            if(++copied>4||(diagnostic&&++diagnostics>2)||attributes==INVALID_FILE_ATTRIBUTES||(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
               file.file_size()>(name==L"export-failure.marker"?128u:diagnostic?300u*1024u:2u*1024u*1024u)||
               (name==L"export-failure.marker"&&!marker(file.path(),s.run))||expected.empty()||
               !CopyFileW(file.path().c_str(),(evidence/target).c_str(),TRUE)||hash(evidence/target)!=expected)
                throw std::runtime_error("diagnostic_failure_raw_evidence_copy_not_exact_bounded_plain_file");
            s.log.emit("diagnostic_export_failure_evidence_file","\"directory\":"+quote(utf8(evidence.filename().wstring()))+",\"filename\":"+quote(utf8(target))+",\"sha256\":"+quote(expected));
        }archived=copied>=2;
    }catch(const std::exception& error){failure+=";archive:";failure+=error.what();}
    s.product=original_product;s.product_log=original_log;s.diagnostic_export_failure_local=false;
    bool original_restored=!original_stopped;
    if(original_stopped&&!s.process)try{
        const auto mark=read_product(s).size();original_restored=launch(s,L"--diagnostic",false)&&wait([&]{return number(record(s,mark,"startup"),"\"process_id\":")==s.product_pid;})&&
            hash(s.product)==binary_hash&&read(original_product.parent_path()/L"settings.ini")==original_config&&settings_guest_configuration(s)==original_settings;
    }catch(const std::exception& error){failure+=";original_restart:";failure+=error.what();}
    s.log.emit("diagnostic_export_failure_cleanup","\"original_DACL_restored\":"+std::string(acl_restored?"true":"false")+
        ",\"raw_evidence_archived\":"+(archived?"true":"false")+",\"original_product_and_configuration_restored\":"+(original_restored?"true":"false")+
        ",\"guest_local_copy_retained\":true,\"mapped_Output_DACL_changed\":false,\"error\":"+quote(failure));
    if(!failure.empty()||!acl_restored||!archived||!original_restored)throw std::runtime_error("diagnostic_export_failure_case_or_cleanup_failed:"+failure);
    s.log.check("diagnostic_export_failure_visible_error_required",negative_checked&&visible_failure,
        "\"actual_log_notice_not_substituted_for_visible_error\":true,\"failure_does_not_skip_DACL_restore_or_product_restart\":true");
}
void settings_guest_corrupt_restart(State& s){
    namespace v=panebind::core::v1;const auto original=settings_guest_configuration(s);const auto path=s.product.parent_path()/L"settings.ini";
    std::vector<fs::path> old_backups;for(const auto& entry:fs::directory_iterator(path.parent_path()))if(entry.path().filename().wstring().starts_with(L"settings.ini.corrupt."))old_backups.push_back(entry.path());
    const auto stop_mark=s.log.mark();const auto product_stop_mark=read_product(s).size();const auto stopped_pid=s.product_pid;
    const bool exact_process_before=s.process&&stopped_pid&&GetProcessId(s.process)==stopped_pid&&process_birth(s.process)&&
        WaitForSingleObject(s.process,0)==WAIT_TIMEOUT;cleanup(s);std::string stopped,input_recovery;bool forced=false;
    const auto driver_text=s.log.since(stop_mark);
    if(!driver_text.empty()){
        std::istringstream rows(driver_text);std::string row;
        while(std::getline(rows,row))if(row.ends_with('}')){
            if(row.find("\"type\":\"product_stop\"")!=row.npos)stopped=row;
            if(row.find("\"type\":\"post_stop_actual_client_click\"")!=row.npos)input_recovery=row;
            forced=forced||row.find("\"own_product_forced_termination\":true")!=row.npos;
        }
    }
    const auto shutdown=record(s,product_stop_mark,"shutdown");
    const bool resources=number(shutdown,"\"process_id\":")==stopped_pid&&
        shutdown.find("\"own_hooks_stopped\":true")!=shutdown.npos&&shutdown.find("\"own_overlay_destroyed\":true")!=shutdown.npos&&
        shutdown.find("\"native_worker_exited\":true")!=shutdown.npos&&shutdown.find("\"issued_native_completion_unknown\":false")!=shutdown.npos&&
        shutdown.find("\"visual_recovery_stopped\":true")!=shutdown.npos&&shutdown.find("\"visual_restore_confirmed\":true")!=shutdown.npos;
    const bool delivered=input_recovery.find("\"check_requested\":true")!=input_recovery.npos&&
        input_recovery.find("\"executed\":true")!=input_recovery.npos&&input_recovery.find("\"actual_down_up_delivered\":true")!=input_recovery.npos&&
        input_recovery.find("\"DOWN_SendInput_called\":true")!=input_recovery.npos&&input_recovery.find("\"UP_SendInput_called\":true")!=input_recovery.npos;
    s.log.check("corrupt_config_normal_stop_before_test_write",exact_process_before&&!s.process&&!forced&&resources&&delivered&&
        stopped.find("\"quit_command_completed\":true")!=stopped.npos&&stopped.find("\"process_exit_observed\":true")!=stopped.npos&&
        stopped.find("\"product_exit_code\":0")!=stopped.npos,"\"stopped_PID\":"+std::to_string(stopped_pid)+
        ",\"this_stop_resource_receipt\":"+(resources?"true":"false")+",\"actual_client_input_recovery\":"+quote(delivered?"OBSERVED":"UNKNOWN_OR_FAILED")+
        ",\"forced_termination_observed\":"+(forced?"true":"false")+",\"historical_shutdown_not_reused\":true");
    const auto attrs=GetFileAttributesW(path.c_str());if(attrs==INVALID_FILE_ATTRIBUTES||(attrs&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))throw std::runtime_error("corrupt_config_not_exact_existing_plain_test_file");
    constexpr std::string_view corrupt="schema=1\nschema=1\nINTENTIONALLY_INVALID_PANEBIND_GUEST_CONFIG\n";
    {std::ofstream file(path,std::ios::binary|std::ios::trunc);file.write(corrupt.data(),static_cast<std::streamsize>(corrupt.size()));file.close();if(!file)throw std::runtime_error("corrupt_config_test_write_failed");}
    const auto start_mark=read_product(s).size();s.log.check("corrupt_config_same_product_normal_restart",launch(s,L"--diagnostic",false));
    s.log.check("corrupt_config_real_restart_and_notice",wait([&]{return !record(s,start_mark,"startup").empty()&&record(s,start_mark,"notice").find("配置损坏")!=std::string::npos;}));
    std::vector<fs::path> new_backups;for(const auto& entry:fs::directory_iterator(path.parent_path()))if(entry.path().filename().wstring().starts_with(L"settings.ini.corrupt.")&&
        std::find(old_backups.begin(),old_backups.end(),entry.path())==old_backups.end())new_backups.push_back(entry.path());
    s.log.check("corrupt_config_actual_backup_exact_bytes",new_backups.size()==1&&read(new_backups[0])==corrupt&&!fs::exists(path),
        "\"actual_backup\":"+(new_backups.size()==1?quote(utf8(new_backups[0].filename().wstring())):"null")+",\"no_historical_backup_relabelled\":true");
    auto gui=gui_open(s);gui_page_select(s,gui,1);s.log.check("corrupt_config_actual_GUI_default",gui_text(s,gui,gui_number(s,gui,L"吸引距离"))==std::to_wstring(v::default_settings().attraction_dip));
    gui_press(s,gui,VK_ESCAPE);s.log.check("corrupt_config_default_GUI_cancel",wait([&]{return !IsWindow(gui.root);}));settings_guest_move_effect(s,true,"corrupt_config_defaults_next_actual_Move");
    // The controlled baseline changed only generation in earlier cases. Save
    // through real GUI again; do not copy old settings over the recovered app.
    settings_guest_attraction(s,original.attraction_dip);const auto restored=settings_guest_configuration(s);auto wanted=original;wanted.generation=restored.generation;
    s.log.check("corrupt_config_real_GUI_restored_original_options",restored==wanted);
}
[[maybe_unused]] void owned_settings_lifecycle_cases(State& s){
    settings_guest_scope(s);namespace v=panebind::core::v1;const auto original=settings_guest_configuration(s);
    auto baseline=v::default_settings();baseline.generation=original.generation;
    if(original!=baseline||s.explorer[0]||s.explorer[1]||s.explorer[2]||s.applications[0]||s.applications[1]||s.applications[2])
        throw std::runtime_error("settings_lifecycle_requires_restored_default_options_and_exact_owned_only");
    std::array<Rect,3> before{};for(std::size_t n=0;n<3;++n){const auto r=visible(s.frames[n].hwnd);if(!own(s,s.frames[n].hwnd)||!r)throw std::runtime_error("settings_lifecycle_original_owned_frame_missing");before[n]=*r;}
    settings_guest_startup(s);settings_guest_exclusion(s);settings_guest_import_export(s);settings_guest_corrupt_restart(s);
    for(std::size_t n=0;n<3;++n)s.log.check("settings_lifecycle_original_owned_layout_restored",fixture_rect(s.frames[n].hwnd,before[n]));
    s.log.emit("settings_lifecycle_sequence","\"actual_startup_Run_add_remove\":true,\"actual_exclusion_native_Move_and_reenable\":true,\"actual_native_import_export\":true,"
        "\"actual_corrupt_backup_default_restart_and_Move\":true,\"real_GUI_input\":true,\"human_uat\":\"NOT_RUN\"");
}

// Actual tray/menu input, never a private command or a fabricated WM_COMMAND.
// Shell geometry identifies only this exact app HWND + UID 1. An inaccessible
// icon is a bounded BLOCKED result; no other notification icon is manipulated.
struct TrayGuestOwner {HWND app{};DWORD tid{};std::uint64_t birth{};};
bool tray_guest_owner(State& s,const TrayGuestOwner& owner){
    DWORD pid{};return desktop()&&s.process&&GetProcessId(s.process)==s.product_pid&&medium(s.process)&&
        WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&process_birth(s.process)==owner.birth&&
        FindWindowW(L"PaneBind.V1.Application",L"PaneBind")==owner.app&&
        GetWindowThreadProcessId(owner.app,&pid)==owner.tid&&pid==s.product_pid;
}
struct TrayGuestMenu {HWND window{};HMENU handle{};};
bool tray_guest_menu(State& s,const TrayGuestOwner& owner,TrayGuestMenu menu){
    DWORD pid{};GUITHREADINFO gui{sizeof(gui)};MENUBARINFO bar{sizeof(bar)};
    return tray_guest_owner(s,owner)&&menu.window&&gui_class(menu.window)==L"#32768"&&IsWindowVisible(menu.window)&&
        GetWindowThreadProcessId(menu.window,&pid)==owner.tid&&pid==s.product_pid&&
        GetGUIThreadInfo(owner.tid,&gui)&&gui.hwndMenuOwner==owner.app&&(gui.flags&GUI_INMENUMODE)&&
        GetForegroundWindow()==owner.app&&GetMenuBarInfo(menu.window,OBJID_CLIENT,0,&bar)&&bar.hMenu==menu.handle;
}
std::optional<TrayGuestMenu> tray_guest_open(State& s,const TrayGuestOwner& owner){
    if(!tray_guest_owner(s,owner)||s.left||s.control)throw std::runtime_error("tray_exact_idle_product_required");
    NOTIFYICONIDENTIFIER id{sizeof(id)};id.hWnd=owner.app;id.uID=1;RECT area{};
    const HRESULT queried=Shell_NotifyIconGetRect(&id,&area);DWORD shell_pid{},hit_pid{};
    GetWindowThreadProcessId(GetShellWindow(),&shell_pid);
    const bool rectangle=SUCCEEDED(queried)&&area.left<area.right&&area.top<area.bottom;
    POINT point{};HWND hit{};bool mapped=false;
    if(rectangle)point={area.left+(area.right-area.left)/2,area.top+(area.bottom-area.top)/2};
    if(rectangle&&MonitorFromPoint(point,MONITOR_DEFAULTTONULL)){
        cursor(s,point);RECT fresh{};hit=WindowFromPoint(point);GetWindowThreadProcessId(hit,&hit_pid);
        const auto root_class=gui_class(GetAncestor(hit,GA_ROOT));
        mapped=tray_guest_owner(s,owner)&&Shell_NotifyIconGetRect(&id,&fresh)==S_OK&&EqualRect(&area,&fresh)&&
            shell_pid&&hit_pid==shell_pid&&(root_class==L"Shell_TrayWnd"||root_class==L"NotifyIconOverflowWindow"||root_class==L"TopLevelWindowForOverflowXamlIsland");}
    s.log.emit("tray_exact_icon_rect","\"app_hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(owner.app))+
        ",\"product_pid\":"+std::to_string(s.product_pid)+",\"uid\":1,\"GetRect_hresult\":"+std::to_string(static_cast<long>(queried))+
        ",\"screen_rect\":"+(rectangle?rect({area.left,area.top,area.right,area.bottom}):"null")+
        ",\"screen_point\":["+std::to_string(point.x)+","+std::to_string(point.y)+"],\"foreground\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(GetForegroundWindow()))+",\"actual_hit\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(hit))+",\"hit_pid\":"+std::to_string(hit_pid)+
        ",\"exact_own_icon_shell_mapping\":"+(mapped?"true":"false")+",\"coordinate_conversion\":\"Shell_screen_pixels_PMv2_no_manual_scaling\"");
    if(!mapped)return {};
    bool right_down=false;try{for(const bool pressed:{true,false}){
        if(pressed&&(!tray_guest_owner(s,owner)||WindowFromPoint(point)!=hit))throw std::runtime_error("tray_icon_changed_before_actual_RIGHTDOWN");
        INPUT packet{};packet.type=INPUT_MOUSE;packet.mi.dwFlags=pressed?MOUSEEVENTF_RIGHTDOWN:MOUSEEVENTF_RIGHTUP;
        UINT sent=UINT_MAX;DWORD error{};inject(packet,&sent,&error);right_down=pressed;
        s.log.emit("tray_actual_right_input","\"DOWN\":"+std::string(pressed?"true":"false")+",\"SendInput_return\":"+std::to_string(sent)+",\"delivery_inferred\":false");
    }}catch(...){if(right_down){INPUT up{};up.type=INPUT_MOUSE;up.mi.dwFlags=MOUSEEVENTF_RIGHTUP;try{inject(up);}catch(...){}}throw;}
    TrayGuestMenu found;
    const bool opened=wait([&]{
        struct Search {State* s;const TrayGuestOwner* owner;TrayGuestMenu found;unsigned count{};} q{&s,&owner};
        EnumThreadWindows(owner.tid,[](HWND h,LPARAM data)->BOOL{auto& q=*reinterpret_cast<Search*>(data);MENUBARINFO bar{sizeof(bar)};
            if(gui_class(h)==L"#32768"&&IsWindowVisible(h)&&GetMenuBarInfo(h,OBJID_CLIENT,0,&bar)){
                TrayGuestMenu candidate{h,bar.hMenu};if(tray_guest_menu(*q.s,*q.owner,candidate)){q.found=candidate;++q.count;}}return TRUE;
        },reinterpret_cast<LPARAM>(&q));found=q.found;return q.count==1;
    });
    if(!opened)throw std::runtime_error("tray_actual_RIGHTUP_did_not_open_exact_product_menu");return found;
}
struct TrayGuestItem {UINT position{},id{};std::wstring text;RECT area{};};
std::vector<TrayGuestItem> tray_guest_items(State& s,const TrayGuestOwner& owner,TrayGuestMenu menu){
    if(!tray_guest_menu(s,owner,menu))throw std::runtime_error("tray_popup_identity_changed");
    const int count=GetMenuItemCount(menu.handle);if(count<7||count>12)throw std::runtime_error("tray_menu_count_outside_production_bound");
    std::vector<TrayGuestItem> result;std::string rows="[";
    for(int n=0;n<count;++n){MENUITEMINFOW info{sizeof(info)};std::array<wchar_t,256> label{};info.fMask=MIIM_STRING|MIIM_ID|MIIM_STATE|MIIM_FTYPE;
        info.dwTypeData=label.data();info.cch=static_cast<UINT>(label.size());RECT r{};
        if(!GetMenuItemInfoW(menu.handle,static_cast<UINT>(n),TRUE,&info)||info.cch>=label.size())throw std::runtime_error("tray_menu_read_unavailable");
        if(info.fType&MFT_SEPARATOR)continue;
        if((info.fState&(MFS_DISABLED|MFS_GRAYED))||!GetMenuItemRect(nullptr,menu.handle,static_cast<UINT>(n),&r)||r.left>=r.right||r.top>=r.bottom)
            throw std::runtime_error("tray_menu_item_not_enabled_or_visible");
        result.push_back({static_cast<UINT>(n),info.wID,std::wstring(label.data(),info.cch),r});
        if(rows.size()>1)rows+=',';rows+="{\"position\":"+std::to_string(n)+",\"id\":"+std::to_string(info.wID)+",\"text\":"+
            quote(utf8(result.back().text))+",\"screen_rect\":"+rect({r.left,r.top,r.right,r.bottom})+"}";
    }
    rows+=']';s.log.emit("tray_actual_menu_items","\"exact_product_PID_TID\":true,\"items\":"+rows);return result;
}
void tray_guest_choose(State& s,const TrayGuestOwner& owner,TrayGuestMenu menu,UINT id,std::wstring_view prefix){
    const auto items=tray_guest_items(s,owner,menu);const auto found=std::find_if(items.begin(),items.end(),[&](const auto& item){return item.id==id&&item.text.starts_with(prefix);});
    if(found==items.end())throw std::runtime_error("tray_expected_Chinese_menu_action_missing");
    const POINT point{found->area.left+(found->area.right-found->area.left)/2,found->area.top+(found->area.bottom-found->area.top)/2};cursor(s,point);
    RECT fresh{};const auto hit=WindowFromPoint(point);DWORD hit_pid{};
    if(!tray_guest_menu(s,owner,menu)||GetWindowThreadProcessId(hit,&hit_pid)!=owner.tid||hit_pid!=s.product_pid||gui_class(hit)!=L"#32768"||
       !GetMenuItemRect(nullptr,menu.handle,found->position,&fresh)||!EqualRect(&fresh,&found->area))throw std::runtime_error("tray_menu_actual_hit_or_mapping_changed");
    button(s,true);button(s,false);
    s.log.emit("tray_actual_menu_click","\"item_id\":"+std::to_string(id)+",\"Chinese_label\":"+quote(utf8(found->text))+",\"WM_COMMAND_injected\":false");
}
void tray_guest_dismiss(State& s,const TrayGuestOwner& owner,TrayGuestMenu menu){
    if(!tray_guest_menu(s,owner,menu))throw std::runtime_error("tray_Escape_not_exact_product_menu");key(s,VK_ESCAPE,true);key(s,VK_ESCAPE,false);
    s.log.check("tray_actual_Escape_closes_menu",wait([&]{GUITHREADINFO g{sizeof(g)};return tray_guest_owner(s,owner)&&GetGUIThreadInfo(owner.tid,&g)&&!g.hwndMenuOwner;}));
}
[[maybe_unused]] void owned_tray_entry_cases(State& s,bool simulate_taskbar_recreation=true){
    settings_guest_scope(s);const auto original=settings_guest_configuration(s);
    if(!original.enabled||s.left||s.control||s.explorer[0]||s.applications[0])throw std::runtime_error("tray_cases_require_enabled_exact_owned_baseline");
    const auto app=FindWindowW(L"PaneBind.V1.Application",L"PaneBind");DWORD pid{};const auto tid=GetWindowThreadProcessId(app,&pid);
    TrayGuestOwner owner{app,tid,process_birth(s.process)};if(!app||pid!=s.product_pid||!owner.birth||!tray_guest_owner(s,owner))throw std::runtime_error("tray_exact_product_app_missing");
    std::array<Rect,3> before{};for(std::size_t n=0;n<3;++n){const auto actual=visible(s.frames[n].hwnd);if(!own(s,s.frames[n].hwnd)||!actual)throw std::runtime_error("tray_exact_owned_geometry_missing");before[n]=*actual;}
    const auto open=[&]()->TrayGuestMenu{const auto menu=tray_guest_open(s,owner);if(!menu)throw std::runtime_error("tray_own_icon_not_exactly_reachable");return *menu;};
    const auto first=tray_guest_open(s,owner);
    if(!first){s.log.failed=true;s.log.emit("tray_entry_result","\"result\":\"BLOCKED\",\"reason\":\"exact_current_product_UID1_icon_not_reachable\",\"menu_input_sent\":false,\"other_flows_may_continue\":true");return;}
    const auto items=tray_guest_items(s,owner,*first);
    for(const auto& expected:std::array<std::pair<UINT,std::wstring_view>,6>{{{500,L"暂停增强"},{501,L"设置"},{502,L"排除上一个应用"},{503,L"帮助"},{504,L"导出诊断"},{505,L"退出"}}})
        s.log.check("tray_Chinese_action_discoverable",std::count_if(items.begin(),items.end(),[&](const auto& item){return item.id==expected.first&&item.text.starts_with(expected.second);})==1,
            "\"item_id\":"+std::to_string(expected.first)+",\"label\":"+quote(utf8(std::wstring(expected.second))));
    bool paused=false;
    try{
        tray_guest_choose(s,owner,*first,500,L"暂停增强");
        s.log.check("tray_actual_pause_saved",wait([&]{return !settings_guest_configuration(s).enabled;}));paused=true;
        settings_guest_effect_layout(s);const auto source=s.frames[0].hwnd;settings_guest_native_move(s,14,3,"tray_pause");
        tray_guest_choose(s,owner,open(),500,L"恢复增强");
        s.log.check("tray_actual_resume_saved",wait([&]{return settings_guest_configuration(s).enabled;}));paused=false;
        settings_guest_move_effect(s,true,"tray_resume_fresh_actual_Move");
        const auto current=visible(source);if(!current)throw std::runtime_error("tray_resume_resize_geometry_missing");auto resized=*current;resized.b+=28;
        const auto resize=down(s,0,false,HTBOTTOM);phase(s,0,resize,0,28,resized,"tray_resume_fresh_actual_Resize",true);up(s,0,resize,resized);
        tray_guest_choose(s,owner,open(),501,L"设置");SettingsGui settings;
        s.log.check("tray_actual_menu_opens_settings",wait([&]{struct Query{State* s;SettingsGui found;unsigned count{};} q{&s};
            EnumThreadWindows(owner.tid,[](HWND h,LPARAM data)->BOOL{auto& q=*reinterpret_cast<Query*>(data);SettingsGui candidate{h,GetDlgItem(h,100),GetWindowThreadProcessId(h,nullptr)};
                if(gui_class(h)==L"PaneBindV1SettingsWindow"&&candidate.tab&&gui_identity(*q.s,candidate,candidate.tab)){q.found=candidate;++q.count;}return TRUE;
            },reinterpret_cast<LPARAM>(&q));settings=q.found;return q.count==1&&GetForegroundWindow()==settings.root&&gui_identity(s,settings,gui_focus(settings));}));
        gui_press(s,settings,VK_ESCAPE);s.log.check("tray_actual_settings_Escape",wait([&]{return !IsWindow(settings.root);}));
        tray_guest_choose(s,owner,open(),503,L"帮助");HWND help{};
        s.log.check("tray_actual_help_dialog",wait([&]{struct Query{HWND app;DWORD pid;HWND found{};unsigned count{};} q{owner.app,s.product_pid};
            EnumThreadWindows(owner.tid,[](HWND h,LPARAM data)->BOOL{auto& q=*reinterpret_cast<Query*>(data);DWORD pid{};std::array<wchar_t,128> title{};GetWindowTextW(h,title.data(),static_cast<int>(title.size()));
                if(GetWindowThreadProcessId(h,&pid)&&pid==q.pid&&GetWindow(h,GW_OWNER)==q.app&&gui_class(h)==L"#32770"&&std::wstring_view(title.data())==L"PaneBind 帮助"&&IsWindowVisible(h)){q.found=h;++q.count;}return TRUE;
            },reinterpret_cast<LPARAM>(&q));help=q.found;return q.count==1&&GetForegroundWindow()==help;}));
        struct HelpText {HWND root;DWORD pid,tid;bool matched{};unsigned left{32};} help_text{help,s.product_pid,owner.tid};
        EnumChildWindows(help,[](HWND h,LPARAM data)->BOOL{auto& q=*reinterpret_cast<HelpText*>(data);if(!q.left--)return FALSE;DWORD pid{};
            if(gui_class(h)!=L"Static"||GetWindowThreadProcessId(h,&pid)!=q.tid||pid!=q.pid||!IsChild(q.root,h))return TRUE;
            std::array<wchar_t,1024> body{};DWORD_PTR copied{};
            if(SendMessageTimeoutW(h,WM_GETTEXT,body.size(),reinterpret_cast<LPARAM>(body.data()),SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&copied)&&copied<body.size()){
                const std::wstring_view actual(body.data(),copied);q.matched=q.matched||(actual.starts_with(L"普通权限运行。")&&actual.find(L"Ctrl+Shift+F11")!=actual.npos);}
            return TRUE;
        },reinterpret_cast<LPARAM>(&help_text));s.log.check("tray_help_actual_Chinese_usage",help_text.matched);
        GUITHREADINFO help_context{sizeof(help_context)};if(!tray_guest_owner(s,owner)||GetForegroundWindow()!=help||!GetGUIThreadInfo(owner.tid,&help_context)||
            !help_context.hwndFocus||(help_context.hwndFocus!=help&&!IsChild(help,help_context.hwndFocus)))throw std::runtime_error("tray_help_keyboard_identity_changed");
        key(s,VK_RETURN,true);key(s,VK_RETURN,false);s.log.check("tray_actual_Enter_closes_help",wait([&]{return !IsWindow(help);}));
        const auto previous=settings_guest_diagnostic_inventory(s);
        tray_guest_choose(s,owner,open(),504,L"导出诊断");settings_guest_diagnostic_result(s,previous,"tray_menu");
        if(simulate_taskbar_recreation){
            const UINT taskbar=RegisterWindowMessageW(L"TaskbarCreated");DWORD_PTR response{};const bool delivered=taskbar&&tray_guest_owner(s,owner)&&
                SendMessageTimeoutW(owner.app,taskbar,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,1000,&response)!=FALSE;
            s.log.check("tray_own_TaskbarCreated_simulation_delivered",delivered,"\"only_exact_product_app_not_broadcast\":true,\"actual_Shell_restart\":false");
            const auto recreated=open();tray_guest_items(s,owner,recreated);tray_guest_dismiss(s,owner,recreated);
        }
        const auto final=settings_guest_configuration(s);auto expected=original;expected.generation=final.generation;
        s.log.check("tray_options_restored_except_saved_generation",final==expected);
        for(std::size_t n=0;n<3;++n)s.log.check("tray_owned_original_layout_restored",own(s,s.frames[n].hwnd)&&fixture_rect(s.frames[n].hwnd,before[n]));
        s.log.emit("tray_entry_result","\"result\":\"PASS\",\"actual_UID1_mouse_menu\":true,\"native_pause_and_new_Move_Resize_after_resume\":true,\"actual_settings_help_diagnostic\":true,\"exit_item_present_not_clicked\":true,\"not_complete_M01_or_M12\":true,\"human_uat\":\"NOT_RUN\"");
    }catch(...){
        if(s.left){try{button(s,false);}catch(...){}}
        GUITHREADINFO active_menu{sizeof(active_menu)};
        if(tray_guest_owner(s,owner)&&GetForegroundWindow()==owner.app&&GetGUIThreadInfo(owner.tid,&active_menu)&&active_menu.hwndMenuOwner==owner.app){
            try{key(s,VK_ESCAPE,true);key(s,VK_ESCAPE,false);wait([&]{GUITHREADINFO g{sizeof(g)};return GetGUIThreadInfo(owner.tid,&g)&&!g.hwndMenuOwner;},1000);}catch(...){}
        }
        if(paused&&tray_guest_owner(s,owner)){try{const auto menu=open();tray_guest_choose(s,owner,menu,500,L"恢复增强");
            s.log.emit("tray_test_cleanup","\"resume_via_actual_menu_requested\":true,\"not_feature_PASS\":true");}catch(...){}}
        throw;
    }
}
