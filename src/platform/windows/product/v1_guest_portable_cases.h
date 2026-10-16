#pragma once

// Include after v1_guest_settings_cases.h inside the guarded fixture's unnamed
// namespace. This is the final Release case: a real delivered ZIP is extracted
// by the guest bootstrap, and its actual executable starts WITHOUT arguments.
// No diagnostic flag, private settings API or replacement raw EXE is used.

bool portable_hex(std::string_view text,std::size_t length,bool upper){
    return text.size()==length&&std::all_of(text.begin(),text.end(),[&](char c){
        return (c>='0'&&c<='9')||(upper?(c>='A'&&c<='F'):(c>='a'&&c<='f'));});
}
// Narrow reader for the immutable packager's flat string fields, not a general
// configuration parser. Escapes/duplicates/unknown encodings are rejected.
std::string portable_field(std::string_view text,std::string_view key){
    const auto token=std::string("\"")+std::string(key)+"\"";auto at=text.find(token);
    if(at==text.npos||text.find(token,at+token.size())!=text.npos)throw std::runtime_error("portable_duplicate_or_missing_field");
    at+=token.size();const auto skip=[&]{while(at<text.size()&&(text[at]==' '||text[at]=='\r'||text[at]=='\n'||text[at]=='\t'))++at;};
    skip();if(at==text.size()||text[at++]!=':')throw std::runtime_error("portable_field_colon_missing");skip();
    if(at==text.size()||text[at++]!='\"')throw std::runtime_error("portable_string_field_required");const auto begin=at;
    while(at<text.size()&&text[at]!='\"'){if(static_cast<unsigned char>(text[at])<32||text[at]=='\\')throw std::runtime_error("portable_field_encoding_rejected");++at;}
    if(at==text.size())throw std::runtime_error("portable_string_unterminated");return std::string(text.substr(begin,at-begin));
}
bool portable_plain_file(const fs::path& path){
    const auto attributes=GetFileAttributesW(path.c_str());return attributes!=INVALID_FILE_ATTRIBUTES&&
        !(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
}
void portable_stop_current(State& s,std::string_view stage){
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);const auto mark=read_product(s).size();const auto driver_mark=s.log.mark();
    if(!s.process||WaitForSingleObject(s.process,0)!=WAIT_TIMEOUT)throw std::runtime_error("portable_expected_live_product_before_normal_stop");
    cleanup(s);const auto receipt=s.log.since(driver_mark);const auto end=record(s,mark,"shutdown");
    const auto app=FindWindowW(L"PaneBind.V1.Application",L"PaneBind");DWORD actual_pid{};if(app)GetWindowThreadProcessId(app,&actual_pid);
    // The current ordinary product emits its own lifecycle records without
    // --diagnostic. Do not enable detailed logging to manufacture this fact.
    const bool known_shutdown=!end.empty()&&
        (number(end,"\"process_id\":")==pid&&end.find("\"own_hooks_stopped\":true")!=end.npos&&
         end.find("\"own_overlay_destroyed\":true")!=end.npos&&end.find("\"native_worker_exited\":true")!=end.npos&&
         end.find("\"issued_native_completion_unknown\":false")!=end.npos&&end.find("\"visual_recovery_stopped\":true")!=end.npos&&
         end.find("\"visual_restore_confirmed\":true")!=end.npos);
    s.log.check("portable_normal_exit_and_actual_input_recovery",birth&&!s.process&&actual_pid!=pid&&known_shutdown&&
        receipt.find("\"quit_command_completed\":true")!=receipt.npos&&receipt.find("\"process_exit_observed\":true")!=receipt.npos&&
        receipt.find("\"product_exit_code\":0")!=receipt.npos&&receipt.find("\"actual_down_up_delivered\":true")!=receipt.npos&&
        receipt.find("\"own_product_forced_termination\":true")==receipt.npos,
        "\"stage\":"+quote(stage)+",\"exact_product_pid\":"+std::to_string(pid)+",\"exact_product_birth\":"+std::to_string(birth)+",\"shutdown_log_observed\":"+(end.empty()?"false":"true")+
        ",\"ordinary_mode_no_diagnostic_argument\":true,\"guest_destruction_used\":false");
}
void portable_actual_move(State& s,bool magnetic,std::string_view stage){
    // Widen the exact owned source, not the hit-test gate. Keep the original
    // gap and cursor displacement so the independent attraction oracle is
    // unchanged; all targets move right by the same added source width.
    constexpr std::array<Rect,3> baseline{{{120,80,520,440},{540,80,800,260},{540,260,800,440}}};
    for(std::size_t n=0;n<3;++n){
        if(s.applications[n]||s.explorer[n]||!own(s,s.frames[n].hwnd))throw std::runtime_error("portable_Move_requires_only_exact_owned_frames");
        ShowWindow(s.frames[n].hwnd,SW_SHOWNOACTIVATE);
        s.log.check("portable_actual_owned_layout",fixture_rect(s.frames[n].hwnd,baseline[n]),"\"test_setup\":true,\"member\":"+std::to_string(n)+",\"expected_visible\":"+rect(baseline[n]));
    }
    const auto dpi=GetDpiForWindow(s.frames[0].hwnd);
    // This independent geometry oracle only applies where 10 DIP attracts a
    // 6 px / 3 px residual. Reject unsupported conditions, do not change it to
    // fit a product result. Windows may reject unsupported layouts separately.
    if(dpi<96||dpi>192)throw std::runtime_error("portable_geometry_oracle_requires_recorded_100_to_200_percent_DPI");
    const auto g=down(s,0);Sleep(60);cursor(s,{g.anchor.x+14,g.anchor.y+3});
    const auto expected=magnetic?Rect{140,80,540,440}:Rect{134,83,534,443};
    const bool geometry=wait([&]{return visible(s.frames[0].hwnd)==expected;});
    const auto actual=visible(s.frames[0].hwnd);
    s.log.check("portable_ordinary_entry_actual_Move_effect",geometry&&visible(s.frames[1].hwnd)==baseline[1]&&visible(s.frames[2].hwnd)==baseline[2],
        "\"stage\":"+quote(stage)+",\"source_dpi\":"+std::to_string(dpi)+",\"generation\":"+std::to_string(g.generation)+
        ",\"expected\":"+rect(expected)+",\"actual\":"+(actual?rect(*actual):"null")+
        ",\"evidence_kind\":\"independent_actual_geometry_and_real_START_UP\",\"native_API_receipt_claimed\":false");
    up(s,0,g,expected,product::guest::UpEvidenceMode::StatusSnapshot);
}
void owned_portable_zip_entry_case(State& s){
    settings_guest_scope(s);
    if(s.configuration!="Release")throw std::runtime_error("portable_ZIP_case_is_Release_only");
    const fs::path input=L"C:\\PaneBindV1\\Input";
    if(!plain_directory(input)||!marker(input/L"panebind-v1-input.marker",s.run)||!medium(GetCurrentProcess()))
        throw std::runtime_error("portable_exact_guest_input_or_Medium_identity_changed");
    const auto ready=read(s.output/(s.run+L"-portable-ready.marker"),512);std::istringstream lines(ready);std::array<std::string,6> identity;
    for(auto& line:identity)if(!std::getline(lines,line)||line.find('\r')!=line.npos)throw std::runtime_error("portable_ready_identity_incomplete");
    std::string extra;if(std::getline(lines,extra)||ready.empty()||ready.back()!='\n'||identity[0]!=utf8(s.run)||
        !portable_hex(identity[1],32,false)||identity[2]!=s.product_source||!portable_hex(identity[2],40,false)||
        !portable_hex(identity[3],64,true)||!portable_hex(identity[4],64,true)||!portable_hex(identity[5],64,true))
        throw std::runtime_error("portable_ready_identity_not_this_build");
    const auto manifest=read(input/L"manifest.json",256*1024);const auto at=manifest.find("\"portable_test\"");
    const auto files=at==manifest.npos?manifest.npos:manifest.find("\"files\"",at);
    if(at==manifest.npos||files==manifest.npos)throw std::runtime_error("portable_immutable_Input_contract_missing");
    const std::string_view spec(manifest.data()+at,files-at);
    if(portable_field(spec,"kind")!="PaneBindReleasePortable"||portable_field(spec,"relative_path")!="PaneBindV1-portable-test.zip"||
       portable_field(spec,"package_id")!=identity[1]||portable_field(spec,"source_commit")!=identity[2]||
       portable_field(spec,"zip_sha256")!=identity[3]||portable_field(spec,"manifest_sha256")!=identity[4]||portable_field(spec,"exe_sha256")!=identity[5])
        throw std::runtime_error("portable_mutable_marker_does_not_match_readonly_manifest");
    const auto run_root=s.output/(s.run+L"-便携 中文 空格"),payload=run_root/L"PaneBindV1",executable=payload/L"panebind.exe";
    if(!plain_directory(run_root)||!plain_directory(payload)||!portable_plain_file(s.output/(s.run+L"-portable-ready.marker"))||
       hash(input/L"PaneBindV1-portable-test.zip")!=identity[3]||hash(payload/L"manifest.json")!=identity[4]||
       hash(executable)!=identity[5]||hash(input/L"panebind.exe")!=identity[5]||hash(s.product)!=identity[5]||
       read(payload/L"portable.flag",128)!="PaneBind V1 portable candidate\n")throw std::runtime_error("portable_actual_ZIP_extracted_binary_or_marker_mismatch");
    const std::array<std::wstring,7> names{L"panebind.exe",L"portable.flag",L"LICENSE",L"中文快速入门.md",L"配置说明.md",L"已知问题.md",L"manifest.json"};
    unsigned count{};for(const auto& entry:fs::directory_iterator(payload)){
        if(!portable_plain_file(entry.path())||std::find(names.begin(),names.end(),entry.path().filename().wstring())==names.end())
            throw std::runtime_error("portable_unexpected_or_nonplain_extracted_file");++count;
    }
    if(count!=names.size())throw std::runtime_error("portable_exact_seven_files_required");
    const auto bracket=manifest.find('[',files),close=bracket==manifest.npos?manifest.npos:manifest.find(']',bracket);
    if(close==manifest.npos)throw std::runtime_error("portable_payload_inventory_missing");
    std::vector<std::string> checked;auto cursor_at=bracket+1;
    while(cursor_at<close){const auto open=manifest.find('{',cursor_at);if(open==manifest.npos||open>=close)break;
        const auto end=manifest.find('}',open);if(end==manifest.npos||end>=close)throw std::runtime_error("portable_payload_inventory_incomplete");
        const std::string_view row(manifest.data()+open,end-open+1);const auto name=portable_field(row,"relative_path"),digest=portable_field(row,"sha256");
        const auto found=std::find_if(names.begin(),names.end(),[&](const auto& candidate){return utf8(candidate)==name;});
        if(found==names.end()||std::find(checked.begin(),checked.end(),name)!=checked.end()||!portable_hex(digest,64,true)||hash(payload/(*found))!=digest)
            throw std::runtime_error("portable_payload_does_not_match_readonly_archive_inventory");
        checked.push_back(name);cursor_at=end+1;
    }
    if(checked.size()!=7)throw std::runtime_error("portable_full_ZIP_inventory_not_verified");
    PWSTR local{};if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)))throw std::runtime_error("portable_localappdata_identity_unknown");
    const auto profile=fs::path(local)/L"PaneBind";CoTaskMemFree(local);
    if(fs::exists(profile)||fs::exists(payload/L"settings.ini")||fs::exists(payload/L"panebind.log.jsonl"))
        throw std::runtime_error("portable_case_requires_new_extraction_and_absent_product_profile_no_unknown_cleanup");
    s.log.emit("portable_ZIP_verified","\"package_id\":"+quote(identity[1])+",\"zip_sha256\":"+quote(identity[3])+
        ",\"binary_sha256\":"+quote(identity[5])+",\"actual_directory\":"+quote(utf8(payload.wstring()))+
        ",\"seven_archived_files_verified\":true,\"configuration_preseeded\":false");
    for(auto& frame:s.frames)if(own(s,frame.hwnd))ShowWindow(frame.hwnd,SW_SHOWNOACTIVATE);
    portable_stop_current(s,"previous_guarded_instance_before_ZIP_launch");
    const auto old_product=s.product,old_log=s.product_log;const auto old_pid=s.product_pid;
    struct Restore {State& s;fs::path product,log;DWORD pid;~Restore(){s.product=product;s.product_log=log;s.product_pid=pid;}} restore{s,old_product,old_log,old_pid};
    s.product=executable;s.product_log=payload/L"panebind.log.jsonl";
    bool move_failed=false;
    const auto observe_move=[&](bool magnetic,std::string_view stage){
        const auto down_before=s.left_down_send_qpc;const bool control_before=s.control;
        try{portable_actual_move(s,magnetic,stage);}
        catch(const std::exception& error){
            if(s.suite!="IndependentEntry")throw;
            move_failed=true;s.log.failed=true;
            s.log.emit("portable_Move_failure_preserved","\"stage\":"+quote(stage)+",\"reason\":"+quote(error.what())+
                ",\"actual_Move\":\"FAIL\",\"I12_PASS_inferred\":false,\"no_UP_retransmission\":true");
            bool cleanup_up=false,cleanup_control=false;
            if(s.left){if(!s.left_down_send_qpc||s.left_down_send_qpc==down_before)throw std::runtime_error("portable_pending_DOWN_not_this_Move");
                button(s,false);cleanup_up=true;}
            if(s.control){if(control_before)throw std::runtime_error("portable_pending_Ctrl_not_this_Move");key(s,VK_CONTROL,false);cleanup_control=true;}
            s.log.emit("portable_test_pending_input_cleanup","\"stage\":"+quote(stage)+",\"single_cleanup_UP_inserted\":"+(cleanup_up?"true":"false")+
                ",\"owned_Ctrl_release_sent\":"+(cleanup_control?"true":"false")+",\"delivery_or_NormalUp_not_inferred\":true");
            // Only actual resource idleness permits the independent ordinary
            // entry's GUI/save/exit observations. Do not re-run this Move or
            // reset its state to manufacture a successful gesture.
            independent_entry_ready(s,std::string("portable.")+std::string(stage)+".after_failure",false);
        }
    };
    try{
        s.log.check("portable_actual_normal_no_argument_launch",launch(s,L"",false),
            "\"actual_executable\":"+quote(utf8(s.product.wstring()))+",\"arguments\":\"\",\"diagnostic_enabled\":false");
        s.log.check("portable_actual_normal_entry_startup",wait([&]{const auto row=record(s,0,"startup");return number(row,"\"process_id\":")==s.product_pid;}));
        SettingsGui ordinary_gui;unsigned visible_settings{};
        const bool ordinary_visible=wait([&]{
            struct Search {State* state;SettingsGui gui;unsigned count{};} search{&s};
            EnumWindows([](HWND h,LPARAM value)->BOOL{auto& q=*reinterpret_cast<Search*>(value);DWORD pid{},app_pid{};
                const auto tid=GetWindowThreadProcessId(h,&pid);
                if(pid!=q.state->product_pid||gui_class(h)!=L"PaneBindV1SettingsWindow"||!IsWindowVisible(h))return TRUE;
                const auto owner=GetWindow(h,GW_OWNER);const auto owner_tid=GetWindowThreadProcessId(owner,&app_pid);
                std::array<wchar_t,64> title{};const auto copied=GetWindowTextW(h,title.data(),static_cast<int>(title.size()));
                SettingsGui candidate{h,FindWindowExW(h,nullptr,WC_TABCONTROLW,nullptr),tid};
                if(owner_tid==tid&&app_pid==pid&&gui_class(owner)==L"PaneBind.V1.Application"&&
                    std::wstring_view(title.data(),copied)==L"PaneBind 设置"&&gui_identity(*q.state,candidate,h)){
                    q.gui=candidate;++q.count;}return TRUE;
            },reinterpret_cast<LPARAM>(&search));
            ordinary_gui=search.gui;visible_settings=search.count;return visible_settings==1;
        });
        s.log.check("portable_no_argument_launch_actual_visible_settings",ordinary_visible,
            "\"actual_product_PID\":"+std::to_string(s.product_pid)+",\"settings_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(ordinary_gui.root))+
            ",\"settings_TID\":"+std::to_string(ordinary_gui.tid)+",\"unique_exact_visible_Chinese_settings\":"+(ordinary_visible?"true":"false")+
            ",\"matching_windows\":"+std::to_string(visible_settings)+",\"settings_command_sent\":false,\"tray_or_full_I12_PASS_inferred\":false");
        const HANDLE held_main=s.process;const DWORD held_pid=s.product_pid;const auto held_birth=process_birth(held_main);
        s.log.check("portable_main_identity_before_empty_secondary",held_main&&held_birth&&GetProcessId(held_main)==held_pid&&
            WaitForSingleObject(held_main,0)==WAIT_TIMEOUT,
            "\"held_main_PID\":"+std::to_string(held_pid)+",\"held_main_birth\":"+std::to_string(held_birth));
        DWORD secondary_exit=STILL_ACTIVE;const bool secondary=launch(s,L"",true,&secondary_exit);
        DWORD settings_pid{};const DWORD settings_tid=GetWindowThreadProcessId(ordinary_gui.root,&settings_pid);
        s.log.check("portable_actual_empty_argument_secondary_launch",secondary&&secondary_exit==0&&s.process==held_main&&s.product_pid==held_pid&&
            GetProcessId(held_main)==held_pid&&process_birth(held_main)==held_birth&&WaitForSingleObject(held_main,0)==WAIT_TIMEOUT&&
            settings_pid==held_pid&&settings_tid==ordinary_gui.tid&&IsWindowVisible(ordinary_gui.root)&&gui_identity(s,ordinary_gui,ordinary_gui.root),
            "\"actual_executable\":"+quote(utf8(s.product.wstring()))+",\"arguments\":\"\",\"diagnostic_enabled\":false,\"secondary_exit_code\":"+
            std::to_string(secondary_exit)+",\"held_main_PID\":"+std::to_string(held_pid)+",\"held_main_birth\":"+std::to_string(held_birth)+
            ",\"same_settings_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(ordinary_gui.root))+",\"actual_settings_TID\":"+std::to_string(settings_tid));
        settings_guest_single_instance(s,held_pid,held_birth,"portable_empty_argument_secondary_launch");
        auto gui=gui_open(s);gui_page_select(s,gui,1);
        s.log.check("portable_fresh_default_actual_GUI",gui_text(s,gui,gui_number(s,gui,L"吸引距离"))==L"10"&&
            !fs::exists(payload/L"settings.ini")&&!fs::exists(profile));gui_press(s,gui,VK_ESCAPE);
        s.log.check("portable_actual_Escape_closes_settings",wait([&]{return !IsWindow(gui.root);}));
        observe_move(true,"fresh_default_10_DIP");
        gui=gui_open(s);gui_page_select(s,gui,1);gui_edit_number(s,gui,gui_number(s,gui,L"吸引距离"),0);gui_save_enter(s,gui);
        const auto saved=panebind::core::v1::parse(read(payload/L"settings.ini"));auto expected=panebind::core::v1::default_settings();
        if(saved.value)expected.generation=saved.value->generation;
        expected.attraction_dip=0;
        s.log.check("portable_actual_GUI_save_only_extracted_configuration",saved.value&&*saved.value==expected&&saved.value->generation>0&&!fs::exists(profile),
            "\"actual_configuration\":"+quote(utf8((payload/L"settings.ini").wstring()))+",\"user_profile_created\":false");
        observe_move(false,"GUI_saved_attraction_zero_actual_effect");
        portable_stop_current(s,"normal_no_argument_ZIP_instance");
        s.log.check("portable_delivered_bytes_retained_and_no_profile",hash(executable)==identity[5]&&hash(payload/L"manifest.json")==identity[4]&&
            hash(input/L"PaneBindV1-portable-test.zip")==identity[3]&&!fs::exists(profile));
        s.log.emit("portable_normal_entry_result","\"I12\":"+quote(move_failed?"FAIL":"PASS")+
            ",\"suite\":"+quote(s.suite)+",\"actual_Release_ZIP\":true,\"normal_arguments_empty\":true,\"actual_GUI_saved_configuration\":true,"+
            "\"actual_Move_effect\":"+quote(move_failed?"FAIL":"PASS")+",\"actual_normal_exit_and_bytes_checked\":true,\"human_UAT\":\"NOT_RUN\"");
    }catch(...){try{cleanup(s);}catch(...){}throw;}
}
