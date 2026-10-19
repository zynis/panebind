#pragma once

// One owned-target rule contrast, included after product_chains. Production
// GUI, Catalog filtering, StretchLeft, writer and Undo remain unchanged.
std::wstring owned_rule_utf8(std::string_view value){
    if(value.size()>panebind::core::v1::settings_detail::max_file_bytes)throw std::runtime_error("owned_rule_fragment_over_bound");
    if(value.empty())return {};
    const auto size=static_cast<int>(value.size());const auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),size,nullptr,0);
    if(count<=0)throw std::runtime_error("owned_rule_original_fragment_invalid_UTF8");
    std::wstring result(static_cast<std::size_t>(count),L'\0');
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),size,result.data(),count)!=count)
        throw std::runtime_error("owned_rule_original_fragment_UTF8_conversion_failed");return result;
}
panebind::core::v1::Settings owned_rule_restore_rules_GUI(State& s,const panebind::core::v1::Settings& original){
    namespace v=panebind::core::v1;const auto before=settings_guest_configuration(s);
    std::string fragment;const auto serialized=v::serialize(original);
    for(const auto line:v::settings_detail::split(serialized,'\n'))if(line.starts_with("rule_count=")||line.starts_with("rule.")){
        if(!fragment.empty())fragment+="\r\n";fragment+=line;
    }
    auto gui=gui_open(s);gui_page_select(s,gui,4);
    settings_guest_replace(s,gui,gui_number(s,gui,L"rule.编号"),owned_rule_utf8(fragment));gui_save_enter(s,gui);
    const auto actual=settings_guest_configuration(s);auto expected=before;expected.rules=original.rules;expected.generation=actual.generation;
    s.log.check("owned_rule_actual_GUI_restored_exact_original_rules",actual==expected&&actual.generation>before.generation,
        "\"actual_rule_count\":"+std::to_string(actual.rules.size())+",\"other_configuration_unchanged\":true,\"UTF8_original_rules_preserved\":true");
    return actual;
}
std::uint64_t owned_rule_stretch_and_Undo(LayoutFixtures& set,Rect source,Rect target,Rect expected,const char* label){
    auto& s=set.state;const auto mark=read_product(s).size();
    if(set.active!=2||!set.owns(0)||!set.owns(1)||!IsWindowVisible(set.frames[0].hwnd)||!IsWindowVisible(set.frames[1].hwnd)||
       visible(set.frames[0].hwnd)!=source||visible(set.frames[1].hwnd)!=target)
        throw std::runtime_error("owned_rule_same_visible_source_and_target_required");
    chain_command(set,0,L"StretchLeft",expected);const auto receipt=chain_any_placement(s,mark,expected);const auto source_key=number(receipt,"\"key\":");
    const auto actual_source=visible(set.frames[0].hwnd),actual_target=visible(set.frames[1].hwnd);
    s.log.check(label,source_key&&set.owns(0)&&set.owns(1)&&IsWindowVisible(set.frames[1].hwnd)&&
        actual_source==expected&&actual_target==target,
        "\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(set.frames[0].hwnd))+
        ",\"target_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(set.frames[1].hwnd))+
        ",\"expected_source\":"+rect(expected)+",\"actual_source\":"+(actual_source?rect(*actual_source):"null")+
        ",\"expected_unchanged_target\":"+rect(target)+",\"actual_target\":"+(actual_target?rect(*actual_target):"null")+
        ",\"actual_source_receipt\":"+quote(receipt)+
        ",\"hidden_target_is_rule_exclusion_evidence\":false");
    chain_command(set,0,L"Undo",source);unsigned source_calls{},other_calls{};std::istringstream rows(product_log_since(s,mark));std::string row;
    while(std::getline(rows,row))if(row.ends_with('}')&&row.find("\"type\":\"placement\"")!=row.npos&&
        row.find("\"native_callsite_claimed\":true")!=row.npos){
        if(number(row,"\"key\":")==source_key)++source_calls;else ++other_calls;
    }
    const auto undone_source=visible(set.frames[0].hwnd),undone_target=visible(set.frames[1].hwnd);
    s.log.check("owned_rule_actual_Undo_source_only_target_visible_unchanged",source_calls>=2&&!other_calls&&set.owns(0)&&set.owns(1)&&
        undone_source==source&&undone_target==target&&IsWindowVisible(set.frames[1].hwnd),
        "\"phase\":"+quote(label)+",\"source_native_calls\":"+std::to_string(source_calls)+",\"other_native_calls\":"+
        std::to_string(other_calls)+",\"actual_source\":"+(undone_source?rect(*undone_source):"null")+
        ",\"actual_target\":"+(undone_target?rect(*undone_target):"null"));return source_key;
}
void owned_rule_target_stretch_case(State& s){
    namespace v=panebind::core::v1;settings_guest_scope(s);const auto original=settings_guest_configuration(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    if(s.suite!="OwnedRulesOnly"||!original.enabled)throw std::runtime_error("owned_rule_exact_enabled_scope_required");
    auto config=original;config.stretch=true;config.smart_stretch=true;config.drag_transparency=false;config.shake=false;
    const auto baseline_config=chain_gui_save(s,config);LayoutFixtures set{s};set.create(true);
    std::vector<v::Rule> added_rules;bool configuration_restored=false;
    try{
        const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<1250||work.height()<700)throw std::runtime_error("owned_rule_actual_work_area_too_small");
        const Rect source{work.l+700,work.t+180,work.l+1100,work.t+500};
        const Rect target{work.l+300,work.t+120,work.l+600,work.t+560};
        const Rect near_edge{target.r,source.t,source.r,source.b},work_edge{work.l,source.t,source.r,source.b};
        const LayoutLimit limit{120,100,work.width(),work.height()};set.setup({source,target},{limit,limit});chain_check_product(s,pid,birth);
        const HWND source_hwnd=set.frames[0].hwnd,target_hwnd=set.frames[1].hwnd;const auto alias=set.storage->target1_alias;
        s.log.check("owned_rule_distinct_exact_classes_and_positive_overlap",set.owns(0)&&set.owns(1)&&
            !alias.empty()&&gui_class(source_hwnd)==s.class_name&&gui_class(target_hwnd)==alias&&
            source.l>target.r&&source.t<target.b&&source.b>target.t,
            "\"actual_created_windows\":8,\"active_visible_windows\":2,\"actual_source_class\":"+quote(utf8(s.class_name))+
            ",\"actual_target_class\":"+quote(utf8(alias))+",\"qualification_observed_through_production_commands_not_driver_Catalog\":true");
        const auto baseline_key=owned_rule_stretch_and_Undo(set,source,target,near_edge,"owned_rule_baseline_near_target_edge");
        // Capture the exact target as the product's last real foreground app,
        // then use its existing Add Revalidated Foreground/Class GUI contract.
        chain_front(set,1);const auto caption=layout_hit(set,1,HTCAPTION);
        if(!caption)throw std::runtime_error("owned_rule_actual_target_caption_unknown");cursor(s,*caption);(void)layout_wait_sent([]{return false;},100);
        s.log.check("owned_rule_exact_target_before_GUI_add",set.owns(1)&&GetForegroundWindow()==target_hwnd&&
            WindowFromPoint(*caption)==target_hwnd&&visible(target_hwnd)==target&&IsWindowVisible(target_hwnd));
        auto gui=gui_open(s);gui_page_select(s,gui,4);const auto class_filter=gui_find(s,gui,L"Button",L"仅限制为该窗口类名");
        if(gui_read(s,gui,class_filter,BM_GETCHECK)!=BST_CHECKED)gui_click(s,gui,class_filter);
        s.log.check("owned_rule_actual_class_filter_checked",wait([&]{return gui_read(s,gui,class_filter,BM_GETCHECK)==BST_CHECKED;}));
        settings_guest_activate(s,gui,GetDlgItem(gui_page(s,gui),115));
        settings_guest_acknowledge(s,gui,L"已将重新核验的应用加入排除草稿");gui_save_enter(s,gui);
        const auto excluded=settings_guest_configuration(s);std::array<wchar_t,32768> image{};
        const auto image_length=GetModuleFileNameW(nullptr,image.data(),static_cast<DWORD>(image.size()));
        if(!image_length||image_length>=image.size())throw std::runtime_error("owned_rule_actual_driver_image_unknown");
        const auto actual_image=utf8(std::wstring(image.data(),image_length));
        const bool prefix_preserved=excluded.rules.size()==baseline_config.rules.size()+1&&
            std::equal(baseline_config.rules.begin(),baseline_config.rules.end(),excluded.rules.begin());
        const bool exact_added=prefix_preserved&&excluded.rules.back().excluded&&excluded.rules.back().window_class==utf8(alias)&&
            v::settings_detail::ascii_fold(excluded.rules.back().exe)==v::settings_detail::ascii_fold(actual_image)&&
            excluded.rules.back().feature_mask==0xffffffffU;
        auto expected_excluded=baseline_config;expected_excluded.rules=excluded.rules;expected_excluded.generation=excluded.generation;
        s.log.check("owned_rule_GUI_added_exact_target_path_alias_exclusion",exact_added&&excluded==expected_excluded,
            "\"actual_path\":"+quote(actual_image)+",\"actual_class\":"+quote(utf8(alias))+
            ",\"excluded\":"+(exact_added?"true":"false")+",\"source_class_is_different\":true,\"rule_count\":"+std::to_string(excluded.rules.size()));
        added_rules=excluded.rules;
        const auto excluded_key=owned_rule_stretch_and_Undo(set,source,target,work_edge,"owned_rule_excluded_visible_target_uses_work_edge");
        const auto removed=owned_rule_restore_rules_GUI(s,original);
        s.log.check("owned_rule_target_rule_removed_not_hidden_or_recreated",removed.rules==original.rules&&
            set.frames[0].hwnd==source_hwnd&&set.frames[1].hwnd==target_hwnd&&set.owns(0)&&set.owns(1)&&IsWindowVisible(target_hwnd));
        const auto restored_key=owned_rule_stretch_and_Undo(set,source,target,near_edge,"owned_rule_removed_target_returns_same_near_edge");
        DWORD actual_source_pid{};const auto actual_source_tid=GetWindowThreadProcessId(source_hwnd,&actual_source_pid);
        s.log.check("owned_rule_source_identity_retained_across_target_policy",baseline_key&&excluded_key&&restored_key&&
            set.frames[0].hwnd==source_hwnd&&set.owns(0)&&actual_source_pid==GetCurrentProcessId()&&actual_source_tid==set.thread,
            "\"baseline_key\":"+std::to_string(baseline_key)+",\"excluded_key\":"+std::to_string(excluded_key)+
            ",\"restored_key\":"+std::to_string(restored_key)+",\"actual_source_HWND\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(source_hwnd))+",\"actual_source_PID\":"+std::to_string(actual_source_pid)+
            ",\"actual_source_TID\":"+std::to_string(actual_source_tid)+",\"intrinsic_source_Frame_identity_verified\":true,"+
            "\"Catalog_key_persistence_required\":false,\"target_geometry_never_reset_between_policy_steps\":true");
        const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        configuration_restored=restored==expected;s.log.check("owned_rule_full_original_configuration_restored",configuration_restored);
        chain_check_product(s,pid,birth);s.log.check("owned_rule_exact_windows_destroyed_and_alias_unregistered",set.close()&&
            set.storage->alias_ever_registered&&set.storage->alias_unregister_attempted&&set.storage->alias_unregistered&&
            set.storage->alias_unregistration_tid==set.thread);
        s.log.emit("owned_rule_target_stretch_component","\"result\":\"PASS\",\"production_GUI_rule_target_filter_and_Undo\":true,\"same_source_and_target\":true,\"same_product_process\":true,\"hidden_target_substitute\":false,\"real_application_matrix\":\"NOT_RUN\",\"whole_version_PASS\":false,\"human_uat\":\"NOT_RUN\"");
    }catch(...){
        pause_routing_failed_case_input(s);bool restored=false;std::string reason;
        try{
            const auto current=settings_guest_configuration(s);
            if(current.rules!=original.rules&&(added_rules.empty()||current.rules!=added_rules))
                throw std::runtime_error("owned_rule_unrecognized_current_rules_not_overwritten");
            if(current.rules!=original.rules)owned_rule_restore_rules_GUI(s,original);
            const auto actual=chain_gui_save(s,original);auto expected=original;expected.generation=actual.generation;restored=actual==expected;
        }catch(const std::exception& error){reason=error.what();}
        s.log.emit("owned_rule_failure_configuration_cleanup","\"original_configuration_restored\":"+std::string(restored?"true":"false")+
            ",\"reason\":"+quote(reason)+",\"original_failure_preserved\":true,\"function_PASS_inferred\":false");throw;
    }
}
