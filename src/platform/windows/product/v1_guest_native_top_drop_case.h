#pragma once

// TEST ONLY. The default top-edge release route is distinct from the public
// Maximize command and from the already-tested custom DockTop direction.
// Include after the existing dock/geometry/resource helpers in the guarded
// fixture TU. One real ordinary gesture, no private placement/state shortcut.
[[maybe_unused]] void owned_native_top_drop_case(State& s){
    namespace v=panebind::core::v1;
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if(s.suite!="NativeTopDropOnly"||!original.enabled||!original.rules.empty()||s.left||s.control||
       s.explorer[0]||s.applications[0]||binary.empty()||binary!=hash(s.input_product)||
       original.dock_actions[2]!=v::Action::Maximize||
       v::default_settings().dock_actions[2]!=v::Action::Maximize)
        throw std::runtime_error("native_top_drop_exact_default_top_owned_guest_required");
    for(const int code:std::array{VK_LBUTTON,VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN})
        if(GetAsyncKeyState(code)&0x8000)throw std::runtime_error("native_top_drop_actual_released_input_required");
    auto config=original;config.magnet_move=false;config.magnet_resize=false;config.glue=false;
    config.docking=true;config.smart_docking=false;config.stretch=false;config.smart_stretch=false;
    config.preview=true;config.feedback=false;config.drag_transparency=false;config.shake=false;
    chain_gui_save(s,config);LayoutFixtures set{s};set.create();
    const auto rows=[&](const Gesture& g,std::string_view type){
        std::vector<std::string> found;std::istringstream input(product_log_since(s,g.offset));std::string row;
        while(std::getline(input,row))if(remaining_group_receipt(s,row,type)&&
            e::number(row,"\"generation\":")==g.generation)found.push_back(std::move(row));
        return found;
    };
    try{
        product::Catalog catalog;const auto displays=catalog.displays();const auto work=chain_work(set.frames[0].hwnd);
        if(displays.size()!=1||work.width()<1200||work.height()<850)
            throw std::runtime_error("native_top_drop_actual_single_display_oracle_required");
        const Rect initial{work.l+200,work.t+160,work.l+600,work.t+460};
        set.setup({initial},{{100,100,work.width(),work.height()}});
        const HWND source=set.frames[0].hwnd;const UINT dpi=GetDpiForWindow(source);
        WINDOWPLACEMENT before{sizeof(before)};
        if(!dpi||!set.owns(0)||!GetWindowPlacement(source,&before)||before.showCmd!=SW_SHOWNORMAL||IsZoomed(source))
            throw std::runtime_error("native_top_drop_actual_normal_baseline_required");
        const auto g=layout_down(set,0,HTCAPTION,false);
        const auto start=record(s,g.offset,"gesture_start",g.generation);const auto key=e::number(start,"\"key\":");
        s.log.check("native_top_drop_actual_plain_source_generation",remaining_group_receipt(s,start,"gesture_start")&&
            key&&*key&&e::unique_value(start,"\"ctrl\":")=="false"&&e::unique_value(start,"\"resize\":")=="false",
            "\"actual_START\":"+quote(start)+",\"default_top_action\":\"Maximize\"");
        const POINT top{work.l+work.width()/2,work.t+1};
        const LONG dx=top.x-g.anchor.x,dy=top.y-g.anchor.y;const Rect raw=translated(initial,dx,dy);
        layout_phase(set,0,g,dx,dy,{raw},"native_top_drop_actual_free_Move_to_top_center");
        chain_preview_exact(s,source,work);
        WINDOWPLACEMENT held{sizeof(held)};const auto placements_before=rows(g,"placement");
        s.log.check("native_top_drop_preview_and_no_early_maximize",set.owns(0)&&s.left&&!s.left_up_attempted&&
            visible(source)==raw&&!IsZoomed(source)&&GetWindowPlacement(source,&held)&&held.showCmd==SW_SHOWNORMAL&&
            rows(g,"normal_up_maximize").empty()&&dock_remaining_commit_receipts(s,g).empty()&&
            actual_up(s,g.offset,g.generation).empty()&&raw!=work,
            "\"generation\":"+std::to_string(g.generation)+",\"physical_work_area\":"+rect(work)+
            ",\"independent_preview\":"+rect(work)+",\"actual_before_UP_visible\":"+rect(raw)+
            ",\"default_top_action\":\"Maximize\",\"command_Maximize_used\":false");
        // Reuse the actual single-UP, passive hook, product matching-UP and
        // cleanup observer. Native Maximize is asynchronous, so its geometry
        // oracle is the separate bounded actual-state check below rather than
        // a fixed-delay rectangle assumption inside the generic UP helper.
        layout_up(set,g,{});
        WINDOWPLACEMENT maximized{sizeof(maximized)};std::string maximize,end;
        const bool observed=wait([&]{
            maximize=record(s,g.offset,"normal_up_maximize",g.generation);
            end=record(s,g.offset,"gesture_end",g.generation);
            return set.owns(0)&&GetForegroundWindow()==source&&GetDpiForWindow(source)==dpi&&
                IsZoomed(source)&&!IsIconic(source)&&GetWindowPlacement(source,&maximized)&&
                maximized.showCmd==SW_SHOWMAXIMIZED&&visible(source)==work&&
                remaining_group_receipt(s,maximize,"normal_up_maximize")&&
                e::unique_value(maximize,"\"native_attempted\":")=="true"&&
                e::unique_value(maximize,"\"request_started\":")=="true"&&
                remaining_group_receipt(s,end,"gesture_end")&&e::unique_value(end,"\"normal_up\":")=="true";
        },1500);
        s.log.check("native_top_drop_unique_UP_actual_Maximize_state",observed&&rows(g,"normal_up_maximize").size()==1&&
            dock_remaining_commit_receipts(s,g).empty(),
            "\"generation\":"+std::to_string(g.generation)+",\"source_HWND\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(source))+",\"source_key\":"+std::to_string(key.value_or(0))+
            ",\"actual_IsZoomed\":"+(IsZoomed(source)?"true":"false")+
            ",\"actual_showCmd\":"+std::to_string(maximized.showCmd)+",\"actual_visible\":"+
            (visible(source)?rect(*visible(source)):"null")+",\"expected_work\":"+rect(work)+
            ",\"actual_positioning\":"+(geometry_history_positioning(source)?rect(*geometry_history_positioning(source)):"null")+
            ",\"actual_maximize_receipt\":"+quote(maximize)+",\"actual_end\":"+quote(end)+
            ",\"API_request_return_not_state_completion\":true");
        const auto raw_up=transparency_resource(s,g,"raw_mouse"),legacy=transparency_resource(s,g,"legacy_left_up");
        const auto gone=transparency_resource(s,g,"isolation_gone");GUITHREADINFO gui{sizeof(gui)};
        s.log.check("native_top_drop_actual_Raw_legacy_NormalUp_resources_and_no_late_geometry",
            remaining_group_receipt(s,raw_up,"gesture_input_resource")&&
            (e::number(raw_up,"\"raw_button_flags\":").value_or(0)&RI_MOUSE_LEFT_BUTTON_UP)&&
            remaining_group_receipt(s,legacy,"gesture_input_resource")&&e::unique_value(legacy,"\"actual_legacy_up_accepted\":")=="true"&&
            remaining_group_receipt(s,gone,"gesture_input_resource")&&e::unique_value(gone,"\"overlay_destroyed\":")=="true"&&
            e::unique_value(gone,"\"hotkey_unregistered\":")=="true"&&e::number(gone,"\"removal_reason\":")==0&&
            e::unique_value(gone,"\"release_succeeded\":")=="true"&&e::unique_value(gone,"\"detach_succeeded\":")=="true"&&
            GetGUIThreadInfo(set.thread,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner&&
            placements_before==rows(g,"placement")&&!s.left,
            "\"generation\":"+std::to_string(g.generation)+",\"actual_Raw_UP\":"+quote(raw_up)+
            ",\"actual_legacy_UP\":"+quote(legacy)+",\"actual_Gone\":"+quote(gone)+
            ",\"no_new_continuous_placement_after_UP\":true,\"normal_up_maximize_is_distinct_discrete_state_request\":true");
        chain_preview_hidden(s,"native_top_drop_actual_UP_hides_preview");chain_check_product(s,pid,birth);
        const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        s.log.check("native_top_drop_original_GUI_and_frozen_binary_restored",restored==expected&&hash(s.product)==binary);
        const bool closed=set.close();s.log.check("native_top_drop_exact_test_windows_actually_closed",closed);
        s.log.emit("native_top_drop_component","\"result\":\"PASS_SCOPED_DEFAULT_TOP_PREVIEW_REAL_UP_AND_ACTUAL_MAXIMIZE\","+
            std::string("\"default_top_action\":\"Maximize\",\"public_Maximize_command_substituted\":false,\"single_product_gesture\":true,")+
            "\"restore_dragout_3141_not_repeated\":true,\"magnet_attraction_retested\":false,\"Full_suite\":\"NOT_RUN\",\"human_UAT\":\"NOT_RUN\",\"whole_version_PASS\":false");
    }catch(...){
        pause_routing_failed_case_input(s);
        try{if(s.process&&WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&!s.left&&!s.control)(void)chain_gui_save(s,original);}catch(...){}
        try{if(!set.closed)(void)set.close();}catch(...){}throw;
    }
}
