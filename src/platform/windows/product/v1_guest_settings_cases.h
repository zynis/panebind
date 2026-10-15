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
    settings_guest_unicode(s,text,[&]{return gui_identity(s,gui,edit)&&GetForegroundWindow()==gui.root&&gui_focus(gui)==edit;});
    s.log.check("settings_actual_editor_input",wait([&]{return settings_guest_text(s,gui,edit)==text;}),
        "\"input_kind\":\"actual_Unicode_keys\",\"content_logged\":false");
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
    std::vector<HWND> candidates;
    struct Search {std::vector<HWND>* candidates;} query{&candidates};
    EnumChildWindows(modal.root,[](HWND h,LPARAM data)->BOOL{auto& q=*reinterpret_cast<Search*>(data);
        if(gui_class(h)!=L"Edit"||!IsWindowVisible(h)||!IsWindowEnabled(h)||(GetWindowLongPtrW(h,GWL_STYLE)&ES_READONLY))return TRUE;
        bool filename=GetDlgCtrlID(h)==1152;HWND parent=GetParent(h);unsigned limit=8;
        while(parent&&limit--){if(GetDlgCtrlID(parent)==1148){filename=true;break;}parent=GetParent(parent);}
        if(filename)q.candidates->push_back(h);return TRUE;
    },reinterpret_cast<LPARAM>(&query));
    if(!settings_guest_modal_identity(s,gui,modal)||candidates.size()!=1)throw std::runtime_error("native_filename_edit_not_uniquely_identified");
    DWORD pid{};if(GetWindowThreadProcessId(candidates[0],&pid)!=modal.tid||pid!=s.product_pid)throw std::runtime_error("native_filename_edit_identity_changed");
    return candidates[0];
}
void settings_guest_file_path(State& s,const SettingsGui& gui,bool save,const fs::path& path){
    if(path.parent_path()!=s.output||path.extension()!=L".conf")throw std::runtime_error("settings_native_dialog_path_not_run_output");
    settings_guest_activate(s,gui,GetDlgItem(gui.root,save?113:112));
    const auto modal=settings_guest_modal(s,gui,save?L"导出当前设置草稿":L"导入 PaneBind 配置");
    const auto edit=settings_guest_filename(s,gui,modal);RECT r{};if(!GetWindowRect(edit,&r))throw std::runtime_error("native_filename_edit_bounds_missing");
    const POINT point{r.left+(r.right-r.left)/2,r.top+(r.bottom-r.top)/2};cursor(s,point);
    if(WindowFromPoint(point)!=edit)throw std::runtime_error("native_filename_edit_actual_hit_mismatch");button(s,true);button(s,false);
    key(s,VK_CONTROL,true);settings_guest_modal_press(s,gui,modal,'A');key(s,VK_CONTROL,false);
    settings_guest_unicode(s,path.wstring(),[&]{GUITHREADINFO info{sizeof(info)};return settings_guest_modal_identity(s,gui,modal)&&
        GetForegroundWindow()==modal.root&&GetGUIThreadInfo(modal.tid,&info)&&info.hwndFocus==edit;});
    std::wstring actual(path.wstring().size()+2,L'\0');DWORD_PTR copied{};
    const bool text_read=SendMessageTimeoutW(edit,WM_GETTEXT,actual.size(),reinterpret_cast<LPARAM>(actual.data()),SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&copied)!=FALSE;
    if(!text_read||copied>=actual.size())throw std::runtime_error("native_filename_actual_text_unavailable");actual.resize(copied);
    s.log.check("settings_native_filename_actual_keys",actual==path.wstring(),"\"new_run_output_path\":"+quote(utf8(path.wstring())));
    settings_guest_modal_press(s,gui,modal,VK_RETURN);
    settings_guest_acknowledge(s,gui,save?L"已导出经过校验的设置草稿":L"已导入到草稿");
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
            s.log.check("startup_actual_checkbox",(gui_read(s,gui,checkbox,BM_GETCHECK)==BST_CHECKED)==enabled);gui_save_enter(s,gui);
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
void settings_guest_exclusion(State& s){
    namespace v=panebind::core::v1;const auto original=settings_guest_configuration(s);
    foreground(s,s.frames[0].hwnd);const auto point=hit_point(s.frames[0].hwnd,HTCAPTION);if(!point)throw std::runtime_error("rule_exact_source_caption_missing");cursor(s,*point);Sleep(100);
    auto gui=gui_open(s);gui_page_select(s,gui,4);const auto restricted=gui_find(s,gui,L"Button",L"仅限制为该窗口类名");
    if(gui_read(s,gui,restricted,BM_GETCHECK)!=BST_CHECKED)gui_click(s,gui,restricted);
    settings_guest_activate(s,gui,GetDlgItem(gui_page(s,gui),115));settings_guest_acknowledge(s,gui,L"已将重新核验的应用加入排除草稿");gui_save_enter(s,gui);
    const auto excluded=settings_guest_configuration(s);std::wstring fixture_exe(32768,L'\0');
    const auto exe_chars=GetModuleFileNameW(nullptr,fixture_exe.data(),static_cast<DWORD>(fixture_exe.size()));
    if(!exe_chars||exe_chars>=fixture_exe.size())throw std::runtime_error("rule_actual_fixture_executable_unknown");fixture_exe.resize(exe_chars);
    const bool exact_rule=excluded.rules.size()==original.rules.size()+1&&std::any_of(excluded.rules.begin(),excluded.rules.end(),[&](const auto& rule){
        return v::settings_detail::ascii_fold(rule.exe)==v::settings_detail::ascii_fold(utf8(fixture_exe))&&
            rule.excluded&&rule.window_class==utf8(s.class_name)&&rule.feature_mask==0xffffffffU;});
    s.log.check("rule_actual_GUI_added_exact_source_class",exact_rule);
    settings_guest_effect_layout(s);const auto before=visible(s.frames[0].hwnd);const auto anchor=hit_point(s.frames[0].hwnd,HTCAPTION);
    if(!before||!anchor)throw std::runtime_error("excluded_source_geometry_missing");foreground(s,s.frames[0].hwnd);cursor(s,*anchor);Sleep(100);
    const auto mark=read(s.product_log).size();const auto starts=s.frames[0].starts.load(),ends=s.frames[0].ends.load();button(s,true);
    s.log.check("excluded_Move_actual_native_START",wait([&]{return s.frames[0].starts.load()==starts+1;}));cursor(s,{anchor->x+36,anchor->y+21});
    const auto expected=translated(*before,36,21);s.log.check("excluded_Move_actual_native_geometry",wait([&]{return visible(s.frames[0].hwnd)==expected;}));button(s,false);
    s.log.check("excluded_Move_native_END_no_product_writer",wait([&]{return s.frames[0].ends.load()==ends+1;})&&record(s,mark,"gesture_start").empty()&&record(s,mark,"placement").empty(),
        "\"ordinary_native_Move_preserved\":true,\"actual_END_observed\":true");
    std::string rules;for(const auto row:v::settings_detail::split(v::serialize(original),'\n'))if(row.starts_with("rule_count=")||row.starts_with("rule.")){rules+=row;rules+="\r\n";}
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
    const auto stop_mark=read(s.product_log).size();cleanup(s);const auto stopped=record(s,stop_mark,"product_stop");
    s.log.check("corrupt_config_normal_stop_before_test_write",!s.process&&stopped.find("\"process_exit_observed\":true")!=std::string::npos&&stopped.find("\"product_exit_code\":0")!=std::string::npos);
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
