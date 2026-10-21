#pragma once

// TEST ONLY. Include after v1_guest_settings_cases.h and
// v1_guest_product_chains.h in the guarded fixture translation unit. All edits,
// Save clicks, error acknowledgements and the following Move use the real GUI
// and the existing guest driver; there is no direct settings mutation API.
void settings_invalid_number_input(State& s,const SettingsGui& gui,HWND edit,std::wstring_view wanted){
    if(wanted.empty()||wanted.size()>20||wanted.find_first_not_of(L"-0123456789")!=wanted.npos)
        throw std::runtime_error("settings_invalid_number_test_text_out_of_scope");
    gui_click(s,gui,edit);gui_require_input(s,gui);
    key(s,VK_CONTROL,true);gui_press(s,gui,'A');key(s,VK_CONTROL,false);
    const auto exact=[&]{
        return gui_identity(s,gui,edit)&&gui_class(edit)==L"Edit"&&
            IsWindowVisible(edit)&&IsWindowEnabled(edit)&&
            !(GetWindowLongPtrW(edit,GWL_STYLE)&(ES_READONLY|ES_MULTILINE))&&
            GetForegroundWindow()==gui.root&&gui_focus(gui)==edit&&!s.left&&!s.control;
    };
    if(!exact())throw std::runtime_error("settings_invalid_number_exact_single_line_edit_required");
    settings_guest_unicode(s,wanted,exact);
    s.log.check("settings_invalid_value_actual_single_line_keys",wait([&]{return exact()&&gui_text(s,gui,edit)==wanted;}),
        "\"actual_numeric_text\":"+quote(utf8(std::wstring(wanted)))+
        ",\"input_kind\":\"actual_paired_Unicode_keys\",\"WM_SETTEXT_or_clipboard\":false");
}

[[maybe_unused]] void owned_settings_invalid_values_case(State& s){
    settings_guest_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if((s.suite!="RemainingNativeContractsOnly"&&s.suite!="SettingsValidationOnly")||!original.enabled||!original.rules.empty()||
       s.left||s.control||binary.empty()||binary!=hash(s.input_product)||s.explorer[0]||s.applications[0])
        throw std::runtime_error("settings_invalid_values_exact_idle_owned_guest_required");
    std::array<Rect,3> original_rects{};
    for(std::size_t n=0;n<original_rects.size();++n){const auto actual=visible(s.frames[n].hwnd);
        if(!own(s,s.frames[n].hwnd)||!actual)throw std::runtime_error("settings_invalid_original_owned_geometry_unknown");
        original_rects[n]=*actual;
    }

    // Normalize only the three numerical fields and the Move switch used by
    // this independent oracle, through the same public GUI. Preserve all
    // original options and restore them at the end (generation may advance).
    auto baseline=original;baseline.magnet_move=true;baseline.attraction_dip=10;
    baseline.release_dip=16;baseline.speed_dip=20000;
    if(baseline!=original)baseline=chain_gui_save(s,baseline);
    const auto path=s.product.parent_path()/L"settings.ini";
    const auto baseline_bytes=read(path),baseline_hash=hash(path);
    if(baseline_bytes.empty()||baseline_hash.empty()||chain_settings(s)!=baseline)
        throw std::runtime_error("settings_invalid_current_saved_baseline_required");
    const auto gui=gui_open(s);gui_page_select(s,gui,1);
    struct InvalidValue {const wchar_t* label;const wchar_t* value;const wchar_t* message;unsigned repair;const char* name;};
    const std::array<InvalidValue,4> invalid{{
        {L"吸引距离",L"-1",L"参数超出范围：attraction_dip",10,"negative_attraction"},
        {L"吸引距离",L"2147483648",L"数字格式无效",10,"integer_overflow"},
        {L"释放距离",L"10",L"释放距离必须严格大于吸引距离",16,"release_equal_attraction"},
        {L"释放距离",L"9",L"释放距离必须严格大于吸引距离",16,"release_below_attraction"}
    }};
    for(const auto& value:invalid){
        chain_check_product(s,pid,birth);settings_guest_scope(s);
        const auto edit=gui_number(s,gui,value.label);
        settings_invalid_number_input(s,gui,edit,value.value);
        // A click reaches production IDOK/gather/validate. Never inject a
        // WM_COMMAND or call the parser as a substitute for this UI contract.
        settings_guest_activate(s,gui,GetDlgItem(gui.root,IDOK));
        settings_guest_acknowledge(s,gui,value.message,L"PaneBind：设置未更改");
        const auto after=chain_settings(s);
        s.log.check(std::string("settings_invalid_")+value.name+"_rejected_preserves_file_and_draft",
            gui_identity(s,gui,edit)&&IsWindow(gui.root)&&gui_text(s,gui,edit)==value.value&&
            read(path)==baseline_bytes&&hash(path)==baseline_hash&&after==baseline,
            "\"rejected_value\":"+quote(utf8(value.value))+",\"actual_Chinese_modal_checked\":true"+
            ",\"same_settings_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.root))+
            ",\"saved_configuration_generation\":"+std::to_string(after.generation)+
            ",\"saved_sha256\":"+quote(baseline_hash)+
            ",\"original_saved_bytes_unchanged\":true,\"invalid_draft_retained\":true");
        gui_edit_number(s,gui,edit,value.repair);
        s.log.check("settings_invalid_repair_remains_unsaved",read(path)==baseline_bytes&&chain_settings(s)==baseline&&
            gui_text(s,gui,gui_number(s,gui,L"吸引距离"))==L"10"&&
            gui_text(s,gui,gui_number(s,gui,L"释放距离"))==L"16");
    }

    // Save a valid change from the SAME rejected draft. The existing exact
    // Move oracle distinguishes attraction0 (free+14,+3) from the old live
    // attraction10 policy (XY snapped+20,0), and requires a real UP closure.
    gui_edit_number(s,gui,gui_number(s,gui,L"吸引距离"),0);gui_save_enter(s,gui);
    const auto saved=chain_settings(s);auto wanted=baseline;wanted.attraction_dip=0;wanted.generation=saved.generation;
    s.log.check("settings_invalid_same_draft_valid_save_applies",saved==wanted&&saved.generation>baseline.generation&&
        read(path)!=baseline_bytes&&!hash(path).empty(),"\"saved_configuration_generation\":"+std::to_string(saved.generation));
    settings_guest_move_effect(s,false,"settings_invalid_after_valid_save_actual_free_Move");
    chain_check_product(s,pid,birth);
    const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
    s.log.check("settings_invalid_original_options_and_product_bytes_restored",restored==expected&&
        hash(s.product)==binary&&binary==hash(s.input_product));
    for(std::size_t n=0;n<original_rects.size();++n)
        s.log.check("settings_invalid_original_owned_geometry_restored",own(s,s.frames[n].hwnd)&&fixture_rect(s.frames[n].hwnd,original_rects[n]));
    chain_check_product(s,pid,birth);
    s.log.emit("settings_invalid_values_component","\"result\":\"PASS\",\"actual_GUI_rejections\":4,"+
        std::string("\"same_draft_valid_save_and_following_actual_Move_UP\":true,\"same_product_instance\":true,")+
        "\"all_M11_parameters_or_whole_version_PASS_inferred\":false,\"human_UAT\":\"NOT_RUN\"");
}
