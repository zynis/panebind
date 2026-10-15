#pragma once

// Include inside v1_guest_fixture.cpp's unnamed namespace, after gui_* and
// cleanup(). These cases use actual input in the exact guarded disposable
// guest. They are deliberately not a product settings API or host test.

void settings_guest_scope(State& s){
    std::array<wchar_t,128> user{};DWORD count=static_cast<DWORD>(user.size());
    const auto suffix=s.configuration=="Debug"?L"Debug":s.configuration=="Release"?L"Release":L"";
    if(!desktop()||!GetUserNameW(user.data(),&count)||!equal(user.data(),L"WDAGUtilityAccount")||
       s.run.size()!=32||!std::all_of(s.run.begin(),s.run.end(),[](wchar_t c){return(c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');})||
       !equal(s.output.wstring(),L"C:\\PaneBindV1\\Output")||!plain_directory(s.output)||
       !marker(s.output/L"panebind-v1-output.marker",s.run)||
       s.product!=s.output/(s.run+L"-product-"+suffix)/L"panebind.exe"||
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
    if(WindowFromPoint(point)!=control)throw std::runtime_error("settings_modal_button_actual_hit_mismatch");
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
void settings_guest_acknowledge(State& s,const SettingsGui& gui,std::wstring_view body_prefix){
    const auto modal=settings_guest_modal(s,gui,L"PaneBind");bool body=false;
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
void settings_guest_startup(State& s){
    const auto original=settings_guest_configuration(s);const auto expected=L"\""+s.product.wstring()+L"\" --startup";
    if(original.startup||settings_guest_run_value().exists)throw std::runtime_error("startup_test_requires_absent_Run_value_no_unknown_overwrite");
    try{for(const bool enabled:{true,false}){const auto gui=gui_open(s);gui_page_select(s,gui,0);
            const auto checkbox=gui_find(s,gui,L"Button",L"用户登录后启动");gui_click(s,gui,checkbox);
            s.log.check("startup_actual_checkbox",wait([&]{return (gui_read(s,gui,checkbox,BM_GETCHECK)==BST_CHECKED)==enabled;}));gui_save_enter(s,gui);
            const auto config=settings_guest_configuration(s);const auto entry=settings_guest_run_value();auto wanted=original;wanted.startup=enabled;wanted.generation=config.generation;
            s.log.check("startup_GUI_save_actual_Run_value",config==wanted&&(enabled?(entry.exists&&entry.type==REG_SZ&&entry.text==expected):!entry.exists),
                "\"enabled\":"+std::string(enabled?"true":"false")+",\"exact_current_product_exe\":true,\"actual_HKCU\":true");}
    }catch(...){try{const auto value=settings_guest_run_value();if(value.exists&&value.text==expected){HKEY key{};
                const auto opened=RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_SET_VALUE,&key);
                const auto result=opened==ERROR_SUCCESS?RegDeleteValueW(key,L"PaneBind"):opened;if(opened==ERROR_SUCCESS)RegCloseKey(key);
                s.log.emit("test_cleanup","\"resource\":\"exact_test_created_startup_value\",\"deleted\":"+std::string(result==ERROR_SUCCESS?"true":"false")+",\"product_GUI_PASS_inferred\":false");}}catch(...){}throw;}
}
void settings_guest_effect_layout(State& s){
    constexpr std::array<Rect,3> layout{{{120,80,360,440},{380,80,640,260},{380,260,640,440}}};
    for(std::size_t n=0;n<3;++n)s.log.check("settings_effect_exact_owned_setup",own(s,s.frames[n].hwnd)&&fixture_rect(s.frames[n].hwnd,layout[n]));
}
void settings_guest_move_effect(State& s,bool snap,std::string_view name){
    settings_guest_effect_layout(s);const auto g=down(s,0);const Rect expected=snap?Rect{140,80,380,440}:Rect{134,83,374,443};
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
    const auto mark=read(s.product_log).size();const auto starts=s.frames[0].starts.load(),ends=s.frames[0].ends.load();
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
void settings_guest_exclusion(State& s){
    namespace v=panebind::core::v1;const auto original=settings_guest_configuration(s);
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
    std::string rules;for(const auto row:v::settings_detail::split(v::serialize(original),'\n'))if(row.starts_with("rule_count=")||row.starts_with("rule.")){
        if(!rules.empty())rules+="\r\n";rules+=row;}
    gui=gui_open(s);gui_page_select(s,gui,4);std::wstring wide_rules(rules.begin(),rules.end());
    settings_guest_replace(s,gui,gui_number(s,gui,L"rule.编号"),wide_rules);gui_save_enter(s,gui);
    s.log.check("rule_actual_GUI_removed_exclusion",settings_guest_configuration(s).rules==original.rules);settings_guest_move_effect(s,true,"rule_unexcluded_next_actual_Move");
}
void settings_guest_attraction(State& s,int value){
    const auto gui=gui_open(s);gui_page_select(s,gui,1);gui_edit_number(s,gui,gui_number(s,gui,L"吸引距离"),static_cast<unsigned>(value));gui_save_enter(s,gui);
    s.log.check("settings_actual_attraction_saved",settings_guest_configuration(s).attraction_dip==value);
}
void settings_guest_import_export(State& s){
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
void settings_guest_corrupt_restart(State& s){
    namespace v=panebind::core::v1;const auto original=settings_guest_configuration(s);const auto path=s.product.parent_path()/L"settings.ini";
    std::vector<fs::path> old_backups;for(const auto& entry:fs::directory_iterator(path.parent_path()))if(entry.path().filename().wstring().starts_with(L"settings.ini.corrupt."))old_backups.push_back(entry.path());
    const auto stop_mark=read(s.log.path).size(),product_stop_mark=read(s.product_log).size();const auto stopped_pid=s.product_pid;
    const bool exact_process_before=s.process&&stopped_pid&&GetProcessId(s.process)==stopped_pid&&process_birth(s.process)&&
        WaitForSingleObject(s.process,0)==WAIT_TIMEOUT;cleanup(s);std::string stopped,input_recovery;bool forced=false;
    const auto driver_text=read(s.log.path);
    if(stop_mark<driver_text.size()){
        std::istringstream rows(driver_text.substr(stop_mark));std::string row;
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
    const auto start_mark=read(s.product_log).size();s.log.check("corrupt_config_same_product_normal_restart",launch(s,L"--diagnostic",false));
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
[[maybe_unused]] void owned_tray_entry_cases(State& s){
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
        const auto directory=s.product.parent_path();std::vector<fs::path> previous;
        for(const auto& entry:fs::directory_iterator(directory))if(entry.path().filename().wstring().starts_with(L"diagnostic-"))previous.push_back(entry.path());
        if(previous.size()>16)throw std::runtime_error("tray_run_diagnostic_inventory_over_bound");
        tray_guest_choose(s,owner,open(),504,L"导出诊断");fs::path exported;
        s.log.check("tray_actual_menu_creates_new_diagnostic",wait([&]{std::vector<fs::path> fresh;
            for(const auto& entry:fs::directory_iterator(directory))if(entry.path().filename().wstring().starts_with(L"diagnostic-")&&std::find(previous.begin(),previous.end(),entry.path())==previous.end())fresh.push_back(entry.path());
            if(fresh.size()!=1)return false;exported=fresh[0];return fs::is_regular_file(exported)&&fs::file_size(exported)>0&&fs::file_size(exported)<=300*1024;}));
        const auto attributes=GetFileAttributesW(exported.c_str());const auto name=exported.filename().wstring();
        if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||name.size()<16||!name.ends_with(L".txt")||
           !std::all_of(name.begin()+11,name.end()-4,[](wchar_t c){return c>=L'0'&&c<=L'9';}))throw std::runtime_error("tray_diagnostic_not_exact_new_plain_run_file");
        // serialize() starts with its fixed configuration comment. It belongs
        // to the parsed settings section, not the diagnostic header whitelist.
        constexpr std::string_view config_header="\n# PaneBind configuration v1; UTF-8; modifiers Alt=1 Ctrl=2 Shift=4 Super=8\n";
        const auto text=read(exported,300*1024);const auto config=text.find(config_header),tail=text.find("\n\nRecent failure: ");
        if(config==text.npos||tail==text.npos||tail<=config||!text.starts_with("PaneBind V1\nSOURCE_SHA=" PANEBIND_BUILD_SHA "\nHUMAN_UAT=PENDING\n")||
           !text.ends_with("\nWindow titles, document contents and full rule paths are not collected.\n"))throw std::runtime_error("tray_diagnostic_structure_or_source_invalid");
        std::istringstream header(text.substr(0,config+1));std::string line;bool known_headers=true;
        while(std::getline(header,line)){
            const bool known=line=="PaneBind V1"||line=="SOURCE_SHA=" PANEBIND_BUILD_SHA||line=="HUMAN_UAT=PENDING"||
                line.starts_with("OS_BUILD=")||line.starts_with("OS_UBR=")||line.starts_with("LAST_GESTURE_END=")||
                line.starts_with("HOOK_OWNER_RUNNING=")||line.starts_with("DISPLAY_DPI=")||line.starts_with("LAST_WINDOW_CLASS=");
            known_headers=known_headers&&known;
        }
        const auto parsed=panebind::core::v1::parse(std::string_view(text).substr(config+1,tail-config));
        if(!known_headers||!parsed.value)throw std::runtime_error("tray_diagnostic_actual_headers_or_settings_not_parseable");
        bool private_values_absent=text.find(utf8(directory.wstring()))==text.npos&&text.find(utf8(s.output.wstring()))==text.npos&&
            text.find("INTENTIONALLY_INVALID_PANEBIND_GUEST_CONFIG")==text.npos;
        for(const auto& frame:s.frames){std::array<wchar_t,128> title{};const auto n=GetWindowTextW(frame.hwnd,title.data(),static_cast<int>(title.size()));
            if(!own(s,frame.hwnd)||n<=0||n>=static_cast<int>(title.size()))throw std::runtime_error("tray_privacy_exact_owned_title_unavailable");
            if(text.find(utf8(std::wstring(title.data(),n)))!=text.npos)private_values_absent=false;}
        std::size_t index{};for(const auto& rule:parsed.value->rules)if(rule.exe!="redacted-app-"+std::to_string(++index)+".exe")private_values_absent=false;
        s.log.check("tray_diagnostic_actual_parse_and_privacy",private_values_absent,"\"new_run_file\":"+quote(utf8(exported.filename().wstring()))+
            ",\"sha256\":"+quote(hash(exported))+",\"actual_owned_titles_and_run_paths_absent\":"+(private_values_absent?"true":"false")+",\"no_real_documents_opened\":true,\"not_universal_privacy_validation\":true");
        const UINT taskbar=RegisterWindowMessageW(L"TaskbarCreated");DWORD_PTR response{};const bool delivered=taskbar&&tray_guest_owner(s,owner)&&
            SendMessageTimeoutW(owner.app,taskbar,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,1000,&response)!=FALSE;
        s.log.check("tray_own_TaskbarCreated_simulation_delivered",delivered,"\"only_exact_product_app_not_broadcast\":true,\"actual_Shell_restart\":false");
        const auto recreated=open();tray_guest_items(s,owner,recreated);tray_guest_dismiss(s,owner,recreated);
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
