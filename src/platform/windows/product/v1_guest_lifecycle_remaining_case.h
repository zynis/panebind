#pragma once

// TEST ONLY: include after existing layout/product_chains/geometry_history.
// Wiring or compiling this scene is not a native execution result.
// c656 already observed close -> new root/new key -> rejected old
// Undo under its original212 identity; do NOT replay that sequence here.
[[maybe_unused]] void owned_lifecycle_remaining_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if(s.suite!="LifecycleRemainderOnly"||!original.enabled||!original.rules.empty()||s.left||s.control||
       (GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_CONTROL)&0x8000)||
       (GetAsyncKeyState(VK_SHIFT)&0x8000)||(GetAsyncKeyState(VK_MENU)&0x8000)||
       binary.empty()||binary!=hash(s.input_product))
        throw std::runtime_error("lifecycle_hide_exact_released_Medium_guest_required");
    auto config=original;config.glue=true;config.magnet_move=false;config.magnet_resize=false;config.stretch=true;
    config.docking=false;config.preview=false;config.feedback=false;config.drag_transparency=false;config.shake=false;
    (void)chain_gui_save(s,config);LayoutFixtures set{s};set.create();
    const auto placements=[&](std::size_t mark){
        std::vector<std::string> rows;std::istringstream input(product_log_since(s,mark));std::string row;
        while(std::getline(input,row))if(geometry_history_receipt(s,row,"placement"))rows.push_back(row);return rows;
    };
    try{
        const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<1050||work.height()<800)throw std::runtime_error("lifecycle_hide_actual_work_area_too_small");
        const Rect before{work.l+220,work.t+180,work.l+620,work.t+540};
        set.setup({before});const HWND source=set.frames[0].hwnd;
        const auto native=geometry_history_positioning(source);const auto old_key=geometry_history_source_key(set,before);
        DWORD actual_pid{};const auto actual_tid=GetWindowThreadProcessId(source,&actual_pid);
        const auto before_status=geometry_history_status(s);
        const auto eligible=e::number(before_status,"\"eligible_windows\":");
        const auto claims=e::number(before_status,"\"native_callsite_claims\":");
        const auto operations=e::number(before_status,"\"operations\":");
        s.log.check("lifecycle_hide_exact_admitted_source_before_any_DOWN",set.owns(0)&&native&&old_key&&
            actual_pid==GetCurrentProcessId()&&actual_tid==set.thread&&IsWindowVisible(source)&&eligible&&*eligible>0&&claims&&operations,
            "\"actual_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+
            ",\"actual_PID\":"+std::to_string(actual_pid)+",\"actual_TID\":"+std::to_string(actual_tid)+
            ",\"actual_key\":"+std::to_string(old_key)+",\"actual_visible\":"+rect(before)+
            ",\"actual_status\":"+quote(before_status)+",\"test_DOWN_sent\":false");
        const auto mark=read_product(s).size();chain_check_product(s,pid,birth);
        const bool hidden=set.run(LayoutUiOperation::Configure,{})&&set.owns(0)&&!IsWindowVisible(source);
        s.log.check("lifecycle_hide_actual_UI_owner_hidden",hidden,
            "\"same_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+
            ",\"previous_key\":"+std::to_string(old_key)+",\"owner_operation\":\"Configure_empty_expected\"");
        // --status runs the real product Catalog::all: it must observe the
        // still-live hidden root as ineligible before we show it again.
        const auto hidden_status=geometry_history_status(s);
        const auto hidden_eligible=e::number(hidden_status,"\"eligible_windows\":");
        s.log.check("lifecycle_hide_public_status_observed_ineligible",hidden_eligible&&
            *hidden_eligible+1==*eligible&&set.owns(0)&&!IsWindowVisible(source)&&
            e::number(hidden_status,"\"native_callsite_claims\":")==claims&&
            e::number(hidden_status,"\"operations\":")==operations&&placements(mark).empty(),
            "\"before_status\":"+quote(before_status)+",\"actual_hidden_status\":"+quote(hidden_status)+
            ",\"ShowWindow_return_not_used_as_product_observation\":true,\"test_DOWN_sent\":false");
        set.setup({before});const auto new_key=geometry_history_source_key(set,before);
        const auto shown_status=geometry_history_status(s);
        s.log.check("lifecycle_hide_show_fresh_key_not_old_admission",set.owns(0)&&set.frames[0].hwnd==source&&
            IsWindowVisible(source)&&visible(source)==before&&geometry_history_positioning(source)==native&&
            new_key&&new_key!=old_key&&e::number(shown_status,"\"eligible_windows\":")==eligible&&
            e::number(shown_status,"\"native_callsite_claims\":")==claims&&
            e::number(shown_status,"\"operations\":")==operations&&placements(mark).empty(),
            "\"same_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+
            ",\"pre_hide_key\":"+std::to_string(old_key)+",\"post_show_key\":"+std::to_string(new_key)+
            ",\"actual_status\":"+quote(shown_status)+",\"re_admission_not_write_authority\":true,\"test_DOWN_sent\":false");
        auto g=layout_down(set,0,HTCAPTION,false);const auto start=record(s,g.offset,"gesture_start",g.generation);
        s.log.check("lifecycle_hide_show_actual_new_key_START",g.generation&&
            e::number(start,"\"key\":")==new_key&&e::number(start,"\"group_members\":")==1&&
            e::unique_value(start,"\"resize\":")=="false"&&e::unique_value(start,"\"ctrl\":")=="false",
            "\"expected_new_key\":"+std::to_string(new_key)+",\"actual_START\":"+quote(start));
        const Rect expected=translated(before,21,14);
        layout_phase(set,0,g,21,14,{expected},"lifecycle_hide_show_new_key_actual_Move");
        const auto calls=placements(g.offset);bool exact=calls.size()==1;
        for(const auto& row:calls)exact=exact&&e::number(row,"\"generation\":")==g.generation&&
            e::number(row,"\"key\":")==new_key&&e::unique_value(row,"\"native_attempted\":")=="true"&&
            e::unique_value(row,"\"native_succeeded\":")=="true"&&e::unique_value(row,"\"native_completion_known\":")=="true"&&
            e::unique_value(row,"\"identity_valid\":")=="true"&&e::unique_value(row,"\"exact\":")=="true"&&
            row.find("\"target\":"+rect(expected)+",")!=row.npos&&row.find("\"actual\":"+rect(expected)+",")!=row.npos;
        s.log.check("lifecycle_hide_show_only_fresh_key_exact_native",exact,
            "\"actual_native_attempts\":"+std::to_string(calls.size())+",\"pre_hide_key\":"+std::to_string(old_key)+
            ",\"post_show_key\":"+std::to_string(new_key));
        layout_up(set,g,{expected});
        s.log.check("lifecycle_hide_show_real_UP_zero_late_write",placements(g.offset)==calls);
        chain_check_product(s,pid,birth);
        s.log.check("lifecycle_hide_same_product_binary_unchanged",hash(s.product)==binary);
        s.log.check("lifecycle_hide_exact_fixture_resources_closed",set.close());
        const auto restored=chain_gui_save(s,original);auto wanted=original;wanted.generation=restored.generation;
        s.log.check("lifecycle_hide_original_GUI_configuration_restored",restored==wanted);
        s.log.emit("lifecycle_hide_remaining_component","\"result\":\"PASS_SCOPED_OBSERVED_HIDE_SHOW_NEW_ADMISSION\","
            "\"actual_gestures\":1,\"close_recreate_scope\":\"REUSED_C656_ORIGINAL_IDENTITY_NOT_REPLAYED\","
            "\"stale_token_injection\":\"NOT_RUN_SEPARATE_L1\","
            "\"real_applications\":\"NOT_RUN\",\"human_UAT\":\"NOT_RUN\",\"whole_version_PASS\":false");
    }catch(...){
        pause_routing_failed_case_input(s);
        try{if(!set.closed)set.close();}catch(...){}
        try{if(s.process&&!s.left&&!s.control)chain_gui_save(s,original);}catch(...){}throw;
    }
}
