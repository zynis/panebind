#pragma once

// TEST ONLY: include inside the existing fixture namespace AFTER the exact
// guest settings, UI review and portable case helpers. Used only by the explicitly selected SettingsEditorsOnly suite. No product state is changed through native messages.

HWND review_runtime_status_control(State& s,const SettingsGui& gui){
    HWND found{};unsigned count{};
    for(HWND h=GetWindow(gui.root,GW_CHILD);h;h=GetWindow(h,GW_HWNDNEXT)){
        if(gui_class(h)!=L"Static"||!gui_identity(s,gui,h)||!IsWindowVisible(h))continue;
        const auto value=gui_text(s,gui,h);
        if(value==L"运行中"||value==L"已暂停"||value==L"输入故障"){found=h;++count;}
    }
    if(count!=1)throw std::runtime_error("settings_review_unique_actual_runtime_status_missing");
    return found;
}
std::string review_runtime_current_status(State& s){
    const auto mark=read_product(s).size();
    if(!launch(s,L"--status",true))throw std::runtime_error("settings_review_public_status_command_failed");
    std::string status;
    s.log.check("settings_review_runtime_fresh_status",wait([&]{status=record(s,mark,"status");
        return number(status,"\"process_id\":")==s.product_pid&&
            status.find("\"source_sha\":"+quote(s.product_source))!=status.npos;}),
        "\"actual_product_status\":"+quote(status));
    return status;
}
void review_runtime_actual_state(State& s,const SettingsGui& gui,bool enabled,int draft_attraction){
    const auto status=review_runtime_current_status(s);
    const auto caption=gui_text(s,gui,review_runtime_status_control(s,gui));
    const auto pause=GetDlgItem(gui.root,1240);
    s.log.check("settings_review_runtime_bar_actual_state_and_other_page_draft",gui_identity(s,gui,pause)&&
        caption==(enabled?L"运行中":L"已暂停")&&
        gui_text(s,gui,pause)==(enabled?L"立即暂停":L"立即恢复")&&
        status.find(enabled?"\"enabled\":true":"\"enabled\":false")!=status.npos&&
        status.find("\"hook_owner_running\":true")!=status.npos&&
        status.find("\"gesture_active\":false")!=status.npos&&
        status.find("\"native_operation_pending\":false")!=status.npos&&
        status.find("\"native_completion_unknown\":false")!=status.npos&&
        gui_text(s,gui,gui_number(s,gui,L"吸引距离"))==std::to_wstring(draft_attraction),
        "\"expected_enabled\":"+std::string(enabled?"true":"false")+
        ",\"uncommitted_attraction_DIP\":"+std::to_string(draft_attraction)+
        ",\"status_caption\":"+quote(utf8(caption))+
        ",\"actual_public_status_is_not_a_draft_checkbox\":true");
}
void review_runtime_button(State& s,const SettingsGui& gui){
    const auto button=GetDlgItem(gui.root,1240);review_tab_to(s,gui,button);
    gui_press(s,gui,VK_SPACE);
}
void owned_settings_runtime_bar_review(State& s){
    settings_guest_scope(s);
    const auto original=settings_guest_configuration(s);
    if(!original.enabled||!s.process||s.left||s.control)
        throw std::runtime_error("settings_review_runtime_requires_enabled_idle_exact_product");
    const auto changed=original.attraction_dip==0?1:original.attraction_dip-1;
    if(changed>=original.release_dip)throw std::runtime_error("settings_review_runtime_independent_draft_parameter_invalid");
    auto gui=gui_open(s);gui_page_select(s,gui,1);
    gui_edit_number(s,gui,gui_number(s,gui,L"吸引距离"),static_cast<unsigned>(changed));
    for(const bool enabled:{false,true}){
        review_runtime_button(s,gui);
        s.log.check("settings_review_runtime_immediate_state_persisted_without_other_draft",wait([&]{
            const auto actual=settings_guest_configuration(s);auto expected=original;
            expected.enabled=enabled;expected.generation=actual.generation;return actual==expected;
        }));
        review_runtime_actual_state(s,gui,enabled,changed);
    }
    // Exercise the pre-existing external emergency hotkey through real keys.
    // This checks sync_runtime rather than changing a checkbox or calling an
    // internal pause function. Paired releases are always test-side cleanup.
    const auto emergency_mark=read_product(s).size();const auto persisted_before_emergency=settings_guest_configuration(s);
    gui_require_input(s,gui);key(s,VK_CONTROL,true);key(s,VK_SHIFT,true);
    try{gui_press(s,gui,VK_F11);key(s,VK_SHIFT,false);key(s,VK_CONTROL,false);}
    catch(...){try{key(s,VK_SHIFT,false);key(s,VK_CONTROL,false);}catch(...){}throw;}
    s.log.check("settings_review_runtime_external_emergency_hotkey_observed",wait([&]{
        const auto row=record(s,emergency_mark,"emergency_hotkey");return number(row,"\"process_id\":")==s.product_pid&&
            row.find("\"native_message\":\"WM_HOTKEY\"")!=row.npos&&number(row,"\"registration_id\":")==910;
    }),"\"actual_Ctrl_Shift_F11_input\":true,\"internal_pause_not_called\":true,\"persistent_config_write_not_assumed\":true");
    review_runtime_actual_state(s,gui,false,changed);
    gui_press(s,gui,VK_ESCAPE);
    const bool closed=wait([&]{return !IsWindow(gui.root);});
    const auto after_cancel_status=review_runtime_current_status(s);
    s.log.check("settings_review_runtime_cancel_does_not_undo_immediate_pause",closed&&
        after_cancel_status.find("\"enabled\":false")!=after_cancel_status.npos&&
        settings_guest_configuration(s)==persisted_before_emergency,
        "\"emergency_pause_actual_runtime_checked\":true,\"persisted_enabled_not_used_as_runtime_oracle\":true");
    gui=gui_open(s);gui_page_select(s,gui,1);
    review_runtime_actual_state(s,gui,false,original.attraction_dip);
    review_runtime_button(s,gui);
    s.log.check("settings_review_runtime_restore_after_cancel",wait([&]{return settings_guest_configuration(s).enabled;}));
    review_runtime_actual_state(s,gui,true,original.attraction_dip);
    gui_press(s,gui,VK_ESCAPE);
    const auto final=settings_guest_configuration(s);auto expected=original;expected.generation=final.generation;
    s.log.check("settings_review_runtime_cancel_and_actual_running_settings_restored",wait([&]{return !IsWindow(gui.root);})&&final==expected);
}

SettingsGui review_runtime_visible_no_argument_gui(State& s){
    SettingsGui found;unsigned matches{};
    const bool visible=wait([&]{
        struct Search {State* s;SettingsGui gui;unsigned matches{};} q{&s};
        EnumWindows([](HWND h,LPARAM data)->BOOL{auto& q=*reinterpret_cast<Search*>(data);DWORD pid{};
            const auto tid=GetWindowThreadProcessId(h,&pid);
            if(pid!=q.s->product_pid||gui_class(h)!=L"PaneBindV1SettingsWindow"||!IsWindowVisible(h))return TRUE;
            SettingsGui candidate{h,GetDlgItem(h,100),tid};
            if(candidate.tab&&gui_identity(*q.s,candidate,candidate.tab)){
                std::array<wchar_t,64> title{};const auto n=GetWindowTextW(h,title.data(),static_cast<int>(title.size()));
                if(std::wstring_view(title.data(),n)==L"PaneBind 设置"){q.gui=candidate;++q.matches;}}
            return TRUE;
        },reinterpret_cast<LPARAM>(&q));found=q.gui;matches=q.matches;
        return matches==1&&GetForegroundWindow()==found.root&&gui_identity(s,found,gui_focus(found));
    });
    s.log.check("settings_review_portable_empty_arguments_actual_GUI",visible,
        "\"PID\":"+std::to_string(s.product_pid)+",\"root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(found.root))+
        ",\"matching_product_settings\":"+std::to_string(matches)+",\"launch_arguments\":\"\",\"settings_command_sent\":false");
    return found;
}
HWND review_runtime_task_button(State& s,const SettingsGui& gui,const SettingsGuestModal& modal,int id,std::wstring_view label){
    const auto target=GetDlgItem(modal.root,id);DWORD pid{};
    if(!settings_guest_modal_identity(s,gui,modal)||GetWindowThreadProcessId(target,&pid)!=modal.tid||
       pid!=s.product_pid||!IsChild(modal.root,target)||gui_class(target)!=L"Button"||
       !IsWindowVisible(target)||!IsWindowEnabled(target))throw std::runtime_error("settings_review_exact_TaskDialog_button_missing");
    std::array<wchar_t,128> actual{};DWORD_PTR n{};
    if(!SendMessageTimeoutW(target,WM_GETTEXT,actual.size(),reinterpret_cast<LPARAM>(actual.data()),SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&n)||
       n>=actual.size()||std::wstring_view(actual.data(),n)!=label)throw std::runtime_error("settings_review_TaskDialog_button_label_mismatch");
    return target;
}
void review_runtime_exit_button(State& s,const SettingsGui& gui){
    const auto h=GetDlgItem(gui.root,1242);review_tab_to(s,gui,h);
    // Unlike gui_click, this does not require the clicked root to retain focus
    // after a real activation that legitimately opens its own native dialog.
    settings_guest_activate(s,gui,h);
}
void review_runtime_ui_exit_finished(State& s,std::size_t mark,std::string_view stage){
    if(!s.process||GetProcessId(s.process)!=s.product_pid||!process_birth(s.process))
        throw std::runtime_error("settings_review_UI_exit_original_process_identity_missing");
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    const bool gone=wait([&]{return WaitForSingleObject(s.process,0)==WAIT_OBJECT_0;},5000);
    DWORD code=STILL_ACTIVE;if(gone)GetExitCodeProcess(s.process,&code);
    const auto end=record(s,mark,"shutdown");
    const auto app=FindWindowW(L"PaneBind.V1.Application",L"PaneBind");DWORD app_pid{};if(app)GetWindowThreadProcessId(app,&app_pid);
    const bool stopped=gone&&code==0&&app_pid!=pid&&number(end,"\"process_id\":")==pid&&
        end.find("\"source_sha\":"+quote(s.product_source))!=end.npos&&
        end.find("\"own_hooks_stopped\":true")!=end.npos&&
        end.find("\"own_overlay_destroyed\":true")!=end.npos&&
        end.find("\"native_worker_exited\":true")!=end.npos&&
        end.find("\"issued_native_completion_unknown\":false")!=end.npos&&
        end.find("\"visual_recovery_stopped\":true")!=end.npos&&
        end.find("\"visual_restore_confirmed\":true")!=end.npos&&input_resource_stop_observed(end);
    s.log.check("settings_review_actual_runtime_bar_normal_exit_and_resource_facts",stopped,
        "\"stage\":"+quote(stage)+",\"PID\":"+std::to_string(pid)+",\"birth\":"+std::to_string(birth)+
        ",\"exit_code\":"+std::to_string(code)+",\"actual_shutdown_receipt\":"+quote(end)+
        ",\"private_exit_or_secondary_quit_used\":false,\"guest_destruction_used\":false");
    if(!stopped)throw std::runtime_error("settings_review_runtime_bar_exit_or_resources_not_confirmed");
    CloseHandle(s.process);s.process=nullptr;
}
void owned_settings_portable_runtime_review(State& s){
    settings_guest_scope(s);
    if(s.configuration!="Release"||s.left||s.control)throw std::runtime_error("settings_review_portable_Release_idle_guest_required");
    const fs::path input=L"C:\\PaneBindV1\\Input";
    if(!plain_directory(input)||!marker(input/L"panebind-v1-input.marker",s.run)||!medium(GetCurrentProcess()))
        throw std::runtime_error("settings_review_portable_guarded_input_or_Medium_unknown");
    const auto ready=read(s.output/(s.run+L"-portable-ready.marker"),512);std::istringstream rows(ready);std::array<std::string,6> identity;
    for(auto& row:identity)if(!std::getline(rows,row)||row.find('\r')!=row.npos)throw std::runtime_error("settings_review_portable_ready_marker_incomplete");
    std::string extra;
    if(std::getline(rows,extra)||ready.empty()||ready.back()!='\n'||identity[0]!=utf8(s.run)||
       !portable_hex(identity[1],32,false)||identity[2]!=s.product_source||!portable_hex(identity[2],40,false)||
       !portable_hex(identity[3],64,true)||!portable_hex(identity[4],64,true)||!portable_hex(identity[5],64,true))
        throw std::runtime_error("settings_review_portable_marker_not_this_product_and_Run");
    const auto manifest=read(input/L"manifest.json",256*1024);const auto at=manifest.find("\"portable_test\"");
    const auto files=at==manifest.npos?manifest.npos:manifest.find("\"files\"",at);
    if(at==manifest.npos||files==manifest.npos)throw std::runtime_error("settings_review_portable_immutable_input_contract_missing");
    const std::string_view spec(manifest.data()+at,files-at);
    if(portable_field(spec,"kind")!="PaneBindReleasePortable"||portable_field(spec,"relative_path")!="PaneBindV1-portable-test.zip"||
       portable_field(spec,"package_id")!=identity[1]||portable_field(spec,"source_commit")!=identity[2]||
       portable_field(spec,"zip_sha256")!=identity[3]||portable_field(spec,"manifest_sha256")!=identity[4]||portable_field(spec,"exe_sha256")!=identity[5])
        throw std::runtime_error("settings_review_portable_readonly_manifest_identity_mismatch");
    const auto root=s.output/(s.run+L"-便携 中文 空格"),payload=root/L"PaneBindV1",exe=payload/L"panebind.exe";
    if(!plain_directory(root)||!plain_directory(payload)||hash(input/L"PaneBindV1-portable-test.zip")!=identity[3]||
       hash(payload/L"manifest.json")!=identity[4]||hash(exe)!=identity[5]||hash(s.product)!=identity[5]||
       hash(input/L"panebind.exe")!=identity[5]||read(payload/L"portable.flag",128)!="PaneBind V1 portable candidate\n")
        throw std::runtime_error("settings_review_portable_exact_extraction_or_product_bytes_mismatch");
    const std::array<std::wstring,7> names{L"panebind.exe",L"portable.flag",L"LICENSE",L"中文快速入门.md",L"配置说明.md",L"已知问题.md",L"manifest.json"};
    unsigned count{};for(const auto& f:fs::directory_iterator(payload)){
        if(!portable_plain_file(f.path())||std::find(names.begin(),names.end(),f.path().filename().wstring())==names.end())
            throw std::runtime_error("settings_review_portable_unknown_or_nonplain_file_in_extraction");++count;}
    if(count!=names.size())throw std::runtime_error("settings_review_portable_requires_exact_seven_archived_files");
    const auto begin=manifest.find('[',files),last=begin==manifest.npos?manifest.npos:manifest.find(']',begin);
    if(last==manifest.npos)throw std::runtime_error("settings_review_portable_file_inventory_missing");
    std::vector<std::string> checked;auto offset=begin+1;
    while(offset<last){const auto open=manifest.find('{',offset);if(open==manifest.npos||open>=last)break;
        const auto close=manifest.find('}',open);if(close==manifest.npos||close>=last)throw std::runtime_error("settings_review_portable_file_inventory_incomplete");
        const std::string_view entry(manifest.data()+open,close-open+1);const auto name=portable_field(entry,"relative_path"),digest=portable_field(entry,"sha256");
        const auto found=std::find_if(names.begin(),names.end(),[&](const auto& f){return utf8(f)==name;});
        if(found==names.end()||std::find(checked.begin(),checked.end(),name)!=checked.end()||!portable_hex(digest,64,true)||hash(payload/(*found))!=digest)
            throw std::runtime_error("settings_review_portable_archived_payload_inventory_mismatch");
        checked.push_back(name);offset=close+1;
    }
    if(checked.size()!=names.size()||fs::exists(payload/L"settings.ini")||fs::exists(payload/L"panebind.log.jsonl"))
        throw std::runtime_error("settings_review_portable_needs_fresh_complete_extraction_no_unknown_cleanup");
    PWSTR local{};if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)))throw std::runtime_error("settings_review_portable_profile_path_unknown");
    const auto profile=fs::path(local)/L"PaneBind";CoTaskMemFree(local);
    if(fs::exists(profile))throw std::runtime_error("settings_review_portable_unknown_existing_profile_not_removed");
    portable_stop_current(s,"settings_review_before_new_ZIP_entry");
    const auto old_product=s.product,old_log=s.product_log;const auto old_pid=s.product_pid;
    struct Restore {State& s;fs::path product,log;DWORD pid;~Restore(){s.product=product;s.product_log=log;s.product_pid=pid;}} restore{s,old_product,old_log,old_pid};
    s.product=exe;s.product_log=payload/L"panebind.log.jsonl";
    try{
        s.log.check("settings_review_portable_actual_no_argument_launch",launch(s,L"",false),
            "\"actual_executable\":"+quote(utf8(exe.wstring()))+",\"arguments\":\"\",\"source_sha\":"+quote(s.product_source)+
            ",\"binary_sha256\":"+quote(identity[5])+",\"ZIP_sha256\":"+quote(identity[3]));
        auto gui=review_runtime_visible_no_argument_gui(s);
        const auto held=s.process;const auto pid=s.product_pid;const auto birth=process_birth(s.process);
        DWORD second_code=STILL_ACTIVE;
        s.log.check("settings_review_portable_actual_no_argument_second_launch",launch(s,L"",true,&second_code)&&
            second_code==0&&s.process==held&&s.product_pid==pid&&process_birth(s.process)==birth&&
            gui_identity(s,gui,gui.root)&&IsWindowVisible(gui.root),
            "\"ordinary_arguments_empty\":true,\"held_PID\":"+std::to_string(pid)+",\"held_birth\":"+std::to_string(birth));
        settings_guest_single_instance(s,pid,birth,"settings_review_new_ZIP_empty_secondary");
        gui_page_select(s,gui,1);gui_edit_number(s,gui,gui_number(s,gui,L"吸引距离"),9);gui_save_enter(s,gui);
        const auto saved=settings_guest_configuration(s);auto expected=panebind::core::v1::default_settings();
        expected.attraction_dip=9;expected.generation=saved.generation;
        s.log.check("settings_review_portable_UI_saved_new_extraction_configuration",saved==expected&&!fs::exists(profile));
        gui=gui_open(s);const auto exit_mark=read_product(s).size();review_runtime_exit_button(s,gui);
        review_runtime_ui_exit_finished(s,exit_mark,"no_draft_runtime_bar_exit");
        s.log.check("settings_review_portable_no_argument_restart_saved_config",launch(s,L"",false));
        gui=review_runtime_visible_no_argument_gui(s);gui_page_select(s,gui,1);
        s.log.check("settings_review_portable_restart_reads_saved_configuration",settings_guest_configuration(s)==saved&&
            gui_text(s,gui,gui_number(s,gui,L"吸引距离"))==L"9");
        gui_edit_number(s,gui,gui_number(s,gui,L"吸引距离"),8);
        review_runtime_exit_button(s,gui);auto modal=settings_guest_modal(s,gui,L"PaneBind");
        const auto return_button=review_runtime_task_button(s,gui,modal,IDCANCEL,L"返回设置");
        GUITHREADINFO info{sizeof(info)};
        s.log.check("settings_review_unsaved_exit_default_returns_to_settings",GetGUIThreadInfo(modal.tid,&info)&&info.hwndFocus==return_button,
            "\"default_return_button_actual_focus\":"+std::to_string(reinterpret_cast<std::uintptr_t>(info.hwndFocus)));
        settings_guest_modal_press(s,gui,modal,VK_RETURN);
        s.log.check("settings_review_actual_default_return_retains_draft_and_running_instance",wait([&]{return !IsWindow(modal.root)&&
            GetForegroundWindow()==gui.root&&gui_identity(s,gui,gui_focus(gui));})&&WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&
            gui_text(s,gui,gui_number(s,gui,L"吸引距离"))==L"8"&&settings_guest_configuration(s)==saved);
        review_runtime_exit_button(s,gui);modal=settings_guest_modal(s,gui,L"PaneBind");
        const auto discard=review_runtime_task_button(s,gui,modal,IDYES,L"放弃修改并退出");
        for(unsigned n=0;n<16;++n){GUITHREADINFO current{sizeof(current)};if(!GetGUIThreadInfo(modal.tid,&current))throw std::runtime_error("settings_review_TaskDialog_focus_unknown");
            if(current.hwndFocus==discard)break;settings_guest_modal_press(s,gui,modal,VK_TAB);}
        GUITHREADINFO selected{sizeof(selected)};
        if(!GetGUIThreadInfo(modal.tid,&selected)||selected.hwndFocus!=discard)throw std::runtime_error("settings_review_TaskDialog_discard_not_reached_by_Tab");
        const auto discard_mark=read_product(s).size();settings_guest_modal_press(s,gui,modal,VK_RETURN);
        review_runtime_ui_exit_finished(s,discard_mark,"actual_discard_draft_runtime_bar_exit");
        s.log.check("settings_review_portable_exit_does_not_save_uncommitted_draft",settings_guest_configuration(s)==saved&&
            hash(exe)==identity[5]&&hash(payload/L"manifest.json")==identity[4]&&hash(input/L"PaneBindV1-portable-test.zip")==identity[3]&&!fs::exists(profile));
        s.log.emit("settings_review_T09_scope","\"ordinary_fresh_ZIP_GUI_and_runtime_bar_exit\":true,\"no_draft_and_dirty_exit\":true,"
            "\"actual_TaskDialog_default_return_and_discard\":true,\"saved_config_restart\":true,\"tray_PASS_inferred\":false,\"human_UAT\":\"NOT_RUN\"");
    }catch(...){try{cleanup(s);}catch(...){}throw;}
}
