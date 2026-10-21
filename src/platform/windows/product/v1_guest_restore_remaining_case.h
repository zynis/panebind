#pragma once

// TEST ONLY. Include after the existing layout, geometry-history, product-chain
// and remaining-group helpers. This covers only maximized drag-out and native
// Snap coexistence, not the already-observed normal/product-docked scenes.
// SPI_GETWINARRANGING / SPI_GETDOCKMOVING are read-only SystemParametersInfoW
// queries. Win+Left is the documented native Snap shortcut; no restore-key
// shortcut, system setting write or unknown-foreground rescue is used.
void owned_restore_remaining_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;
    const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if(s.suite!="RestoreRemainderOnly"||!original.enabled||!original.rules.empty()||s.left||s.control||
       binary.empty()||binary!=hash(s.input_product))throw std::runtime_error("restore_remaining_exact_owned_guest_required");
    for(const int code:std::array{VK_LBUTTON,VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN,VK_LEFT})
        if(GetAsyncKeyState(code)&0x8000)throw std::runtime_error("restore_remaining_released_input_required");
    for(const auto& binding:original.hotkeys)if((binding.modifiers&8u)&&binding.key==VK_LEFT)
        throw std::runtime_error("restore_remaining_native_WinLeft_has_product_binding");
    auto config=original;config.magnet_move=false;config.magnet_resize=false;config.glue=false;config.docking=true;
    config.smart_docking=false;config.preview=false;config.feedback=false;config.stretch=false;config.smart_stretch=false;
    config.drag_transparency=false;config.shake=false;
    const auto applied=chain_gui_save(s,config);LayoutFixtures set{s};set.create();
    const auto placements=[&](const Gesture& g){
        std::string out,row;std::istringstream rows(product_log_since(s,g.offset));
        while(std::getline(rows,row))if(remaining_group_receipt(s,row,"placement")&&
            e::number(row,"\"generation\":")==g.generation)out+=row+'\n';return out;
    };
    const auto native_quiet=[&](std::size_t offset){
        std::string row;std::istringstream rows(product_log_since(s,offset));
        while(std::getline(rows,row))if(remaining_group_receipt(s,row,"command")||remaining_group_receipt(s,row,"gesture_start")||
            (remaining_group_receipt(s,row,"placement")&&e::unique_value(row,"\"native_callsite_claimed\":")=="true"))return false;
        return true;
    };
    const auto read_setting=[](UINT action,BOOL& value,DWORD& error){
        SetLastError(0);const bool ok=SystemParametersInfoW(action,0,&value,0)!=FALSE;error=ok?0:GetLastError();return ok;
    };
    const auto drag_out=[&](Rect down_visible,Rect prior_normal,Rect prior_normal_native,bool maximized){
        const HWND h=set.frames[0].hwnd;const auto dpi=GetDpiForWindow(h);
        if(!set.owns(0)||GetForegroundWindow()!=h||visible(h)!=down_visible||!dpi||
           (IsZoomed(h)!=FALSE)!=maximized)throw std::runtime_error("restore_remaining_start_state_changed");
        const auto g=layout_down(set,0,HTCAPTION,false);
        const LONG offset_x=std::clamp<LONG>(g.anchor.x-down_visible.l,0,down_visible.width());
        const LONG proportional_x=static_cast<LONG>(static_cast<long long>(offset_x)*prior_normal.width()/down_visible.width());
        const LONG title_y=std::clamp<LONG>(g.anchor.y-down_visible.t,0,prior_normal.height()-1);
        // Independent original-DOWN oracle, not restored_move_anchor() or a
        // returned production plan. The size was read BEFORE maximize/Snap.
        const Rect anchor{g.anchor.x-proportional_x,g.anchor.y-title_y,
            g.anchor.x-proportional_x+prior_normal.width(),g.anchor.y-title_y+prior_normal.height()};
        const Rect expected=translated(anchor,40,45);
        s.log.emit("restore_remaining_independent_original_DOWN_oracle",
            std::string("\"scope\":")+quote(maximized?"maximized":"native_Snap")+",\"generation\":"+std::to_string(g.generation)+
            ",\"actual_DOWN_source_visible\":"+rect(down_visible)+",\"prior_normal_visible\":"+rect(prior_normal)+
            ",\"original_DOWN\":["+std::to_string(g.anchor.x)+","+std::to_string(g.anchor.y)+"],\"independent_anchor\":"+rect(anchor)+
            ",\"single_cursor_total_delta\":[40,45],\"independent_expected\":"+rect(expected)+",\"oracle_uses_product_solver\":false");
        layout_phase(set,0,g,40,45,{expected},maximized?
            "restore_remaining_maximized_dragout_original_DOWN_exact":"restore_remaining_native_Snap_dragout_original_DOWN_exact");
        const auto request=record(s,g.offset,"native_restore_request",g.generation);
        const auto observed=record(s,g.offset,"native_restore_observed",g.generation);
        if(maximized)s.log.check("restore_remaining_maximized_actual_native_restore_observed",
            remaining_group_receipt(s,request,"native_restore_request")&&
            e::unique_value(request,"\"native_attempted\":")=="true"&&e::unique_value(request,"\"request_started\":")=="true"&&
            remaining_group_receipt(s,observed,"native_restore_observed")&&
            e::unique_value(observed,"\"actual_normal_observed\":")=="true"&&
            observed.find("\"actual\":"+rect(prior_normal)+",")!=observed.npos&&
            observed.find("\"intent_anchor\":"+rect(anchor)+",")!=observed.npos&&
            e::unique_value(observed,"\"original_down_preserved\":")=="true",
            std::string("\"actual_request\":")+quote(request)+",\"actual_normal_observed\":"+quote(observed));
        else s.log.emit("restore_remaining_native_Snap_actual_restore_receipts",
            std::string("\"actual_product_restore_request\":")+(request.empty()?"null":quote(request))+
            ",\"actual_product_restore_observed\":"+(observed.empty()?"null":quote(observed))+
            ",\"native_OS_restore_not_inferred_from_missing_product_API_receipt\":true");
        const Rect expected_native{prior_normal_native.l+expected.l-prior_normal.l,prior_normal_native.t+expected.t-prior_normal.t,
            prior_normal_native.r+expected.r-prior_normal.r,prior_normal_native.b+expected.b-prior_normal.b};
        s.log.check("restore_remaining_actual_normal_size_and_frame_restored",set.owns(0)&&!IsZoomed(h)&&!IsIconic(h)&&
            visible(h)==expected&&geometry_history_positioning(h)==expected_native&&GetDpiForWindow(h)==dpi,
            std::string("\"scope\":")+quote(maximized?"maximized":"native_Snap")+",\"expected_positioning\":"+rect(expected_native));
        const auto before=placements(g);layout_up(set,g,{expected});
        const auto ended=record(s,g.offset,"gesture_end",g.generation);
        s.log.check("restore_remaining_real_NormalUp_no_late_placement",placements(g)==before&&
            remaining_group_receipt(s,ended,"gesture_end")&&e::unique_value(ended,"\"normal_up\":")=="true",
            std::string("\"scope\":")+quote(maximized?"maximized":"native_Snap")+",\"generation\":"+std::to_string(g.generation)+
            ",\"actual_end\":"+quote(ended));
        chain_check_product(s,pid,birth);
        s.log.emit("restore_remaining_scope",std::string("\"scope\":")+quote(maximized?"maximized":"native_Snap")+
            ",\"result\":\"PASS_SCOPED_ACTUAL_DRAG_AND_NORMAL_UP\",\"generation\":"+std::to_string(g.generation));
        return expected;
    };
    try{
        product::Catalog catalog;const auto displays=catalog.displays();const auto work=chain_work(set.frames[0].hwnd);
        if(displays.size()!=1||work.width()<1200||work.height()<850)
            throw std::runtime_error("restore_remaining_actual_single_display_oracle_unavailable");
        const Rect normal{work.l+200,work.t+160,work.l+600,work.t+460};const LayoutLimit limit{100,100,work.width(),work.height()};
        set.setup({normal},{limit});const auto native=resize_magnet_native_baseline(set,{normal});const HWND source=set.frames[0].hwnd;
        BOOL arranging{},dockmoving{},dragmax{};DWORD arranging_error{},dock_error{},drag_error{};
        const bool arranging_known=read_setting(SPI_GETWINARRANGING,arranging,arranging_error);
        const bool dock_known=read_setting(SPI_GETDOCKMOVING,dockmoving,dock_error);
        const bool drag_known=read_setting(SPI_GETDRAGFROMMAXIMIZE,dragmax,drag_error);
        s.log.emit("restore_remaining_readonly_native_settings",
            std::string("\"SPI_GETWINARRANGING_known\":")+(arranging_known?"true":"false")+
            ",\"window_arranging\":"+(arranging_known?(arranging?"true":"false"):"null")+",\"window_arranging_error\":"+std::to_string(arranging_error)+
            ",\"SPI_GETDOCKMOVING_known\":"+(dock_known?"true":"false")+",\"dock_moving\":"+(dock_known?(dockmoving?"true":"false"):"null")+
            ",\"dock_moving_error\":"+std::to_string(dock_error)+",\"SPI_GETDRAGFROMMAXIMIZE_known\":"+(drag_known?"true":"false")+
            ",\"drag_from_maximize\":"+(drag_known?(dragmax?"true":"false"):"null")+",\"drag_from_maximize_error\":"+std::to_string(drag_error)+
            ",\"SPI_SET_or_registry_write\":false,\"actual_shell_window\":"+std::to_string(reinterpret_cast<std::uintptr_t>(GetShellWindow()))+
            ",\"actual_work_area\":"+rect(work)+",\"native_Snap_behavior_not_inferred\":true");
        chain_front(set,0);WINDOWPLACEMENT normal_wp{sizeof(normal_wp)},max_wp{sizeof(max_wp)};
        if(!GetWindowPlacement(source,&normal_wp)||IsZoomed(source))throw std::runtime_error("restore_remaining_pre_maximize_WINDOWPLACEMENT_unknown");
        const auto max_mark=read_product(s).size();const bool requested=launch(s,L"--command Maximize",true);std::string command;
        const bool maximized=requested&&wait([&]{command=record(s,max_mark,"command");return set.owns(0)&&
            GetForegroundWindow()==source&&IsZoomed(source)&&GetWindowPlacement(source,&max_wp)&&max_wp.showCmd==SW_SHOWMAXIMIZED&&
            remaining_group_receipt(s,command,"command")&&e::quoted(command,"\"action\":","Maximize");});
        const auto max_visible=visible(source),max_native=geometry_history_positioning(source);
        s.log.check("restore_remaining_public_Maximize_actual_state",maximized&&max_visible&&max_native&&
            EqualRect(&normal_wp.rcNormalPosition,&max_wp.rcNormalPosition),
            std::string("\"actual_command\":")+quote(command)+",\"actual_maximized_visible\":"+(max_visible?rect(*max_visible):"null")+
            ",\"actual_maximized_positioning\":"+(max_native?rect(*max_native):"null")+",\"actual_showCmd\":"+std::to_string(max_wp.showCmd)+
            ",\"ShowWindow_request_return_not_state_completion\":true");
        const Rect restored=drag_out(*max_visible,normal,native[0],true);
        const auto restored_native=geometry_history_positioning(source);
        if(!restored_native||chain_settings(s)!=applied)throw std::runtime_error("restore_remaining_pre_Snap_baseline_changed");
        bool snap_pass=false;std::string blocked;
        if(!arranging_known||!dock_known)blocked="native_arrangement_settings_UNKNOWN";
        else if(!arranging||!dockmoving)blocked="native_Snap_disabled_not_changed";
        if(blocked.empty()){
            WINDOWPLACEMENT before_snap{sizeof(before_snap)},after_snap{sizeof(after_snap)};
            if(!set.owns(0)||GetForegroundWindow()!=source||visible(source)!=restored||!GetWindowPlacement(source,&before_snap))
                throw std::runtime_error("restore_remaining_native_Snap_exact_foreground_changed");
            const auto status_before=geometry_history_status(s);const auto mark=read_product(s).size();
            LARGE_INTEGER before{},after{};QueryPerformanceCounter(&before);
            bool win_attempted=false,win_up=false,left_attempted=false,left_up=false;
            try{
                win_attempted=true;key(s,VK_LWIN,true);left_attempted=true;key(s,VK_LEFT,true);
                left_up=true;key(s,VK_LEFT,false);win_up=true;key(s,VK_LWIN,false);
            }catch(...){
                try{if(left_attempted&&!left_up){left_up=true;key(s,VK_LEFT,false);}}catch(...){}
                try{if(win_attempted&&!win_up){win_up=true;key(s,VK_LWIN,false);}}catch(...){}throw;
            }
            QueryPerformanceCounter(&after);const Rect half{work.l,work.t,work.l+work.width()/2,work.b};
            const bool snapped=wait([&]{return set.owns(0)&&!IsZoomed(source)&&!IsIconic(source)&&visible(source)==half;},1500);
            const auto actual=visible(source);const bool source_foreground=GetForegroundWindow()==source;
            const bool placement_known=set.owns(0)&&GetWindowPlacement(source,&after_snap)!=FALSE;
            const auto status_after=geometry_history_status(s);
            const bool no_product_native=native_quiet(mark)&&
                e::number(status_before,"\"native_callsite_claims\":")==e::number(status_after,"\"native_callsite_claims\":");
            s.log.emit("restore_remaining_actual_native_WinLeft",
                std::string("\"TEST_Win_DOWN_Left_DOWN_UP_Win_UP_helper_returned\":true,\"single_chord_no_retransmission\":true,\"before_QPC\":")+
                std::to_string(before.QuadPart)+",\"after_QPC\":"+std::to_string(after.QuadPart)+
                ",\"expected_work_half\":"+rect(half)+",\"actual_visible\":"+(actual?rect(*actual):"null")+
                ",\"native_half_observed\":"+(snapped?"true":"false")+",\"actual_foreground_is_source\":"+(source_foreground?"true":"false")+
                ",\"WINDOWPLACEMENT_known\":"+(placement_known?"true":"false")+",\"actual_showCmd\":"+std::to_string(after_snap.showCmd)+
                ",\"before_rcNormalPosition\":"+rect({before_snap.rcNormalPosition.left,before_snap.rcNormalPosition.top,before_snap.rcNormalPosition.right,before_snap.rcNormalPosition.bottom})+
                ",\"after_rcNormalPosition\":"+rect({after_snap.rcNormalPosition.left,after_snap.rcNormalPosition.top,after_snap.rcNormalPosition.right,after_snap.rcNormalPosition.bottom})+
                ",\"no_product_native_or_command_observed\":"+(no_product_native?"true":"false")+
                ",\"status_before\":"+quote(status_before)+",\"status_after\":"+quote(status_after)+",\"native_effect_not_inferred_from_SendInput\":true");
            if(!no_product_native)throw std::runtime_error("restore_remaining_product_intercepted_native_WinLeft");
            if(!source_foreground)blocked="native_Snap_changed_foreground_not_reacquired";
            else if(!snapped)blocked="native_WinLeft_half_not_observed_in_existing_guest";
            else if(!placement_known)blocked="native_Snap_WINDOWPLACEMENT_UNKNOWN";
            if(blocked.empty()){
                // Exactly one real caption drag from native Snap. Its restore
                // size/original anchor must match the pre-Snap normal snapshot.
                // Missing/contradictory platform receipts fail, never become true.
                const Rect after_drag=drag_out(half,restored,*restored_native,false);
                BOOL now_arranging{},now_docking{};DWORD error1{},error2{};
                snap_pass=read_setting(SPI_GETWINARRANGING,now_arranging,error1)&&
                    read_setting(SPI_GETDOCKMOVING,now_docking,error2)&&now_arranging&&now_docking&&
                    set.owns(0)&&visible(source)==after_drag;
                s.log.check("restore_remaining_native_Snap_kept_enabled_after_real_drag",snap_pass,
                    "\"no_system_setting_changes\":true,\"only_one_post_native_Snap_drag\":true");
            }
        }
        if(!blocked.empty())s.log.emit("restore_remaining_native_Snap_scope",
            std::string("\"result\":\"ENVIRONMENT_BLOCKED\",\"reason\":")+quote(blocked)+
            ",\"maximized_or_ordinary_Move_not_substituted_for_native_Snap_PASS\":true,\"unknown_foreground_not_manipulated\":true");
        const auto saved=chain_gui_save(s,original);auto expected_config=original;expected_config.generation=saved.generation;
        s.log.check("restore_remaining_original_GUI_and_frozen_binary_restored",saved==expected_config&&hash(s.product)==binary);
        chain_check_product(s,pid,birth);s.log.check("restore_remaining_exact_test_windows_closed",set.close());
        s.log.emit("restore_remaining_component",
            std::string("\"result\":")+quote(snap_pass?"PASS_SCOPED_MAXIMIZED_AND_NATIVE_SNAP":"PARTIAL_MAXIMIZED_PASS_NATIVE_SNAP_BLOCKED")+
            ",\"maximized_drag\":\"PASS\",\"native_Snap_coexistence\":"+quote(snap_pass?"PASS":"ENVIRONMENT_BLOCKED")+
            ",\"ordinary_and_product_docked_history_not_replayed\":true,\"human_UAT\":\"NOT_RUN\",\"whole_version_PASS\":false");
    }catch(...){
        pause_routing_failed_case_input(s);
        try{if(s.process&&!s.left&&!s.control)chain_gui_save(s,original);}catch(...){}
        try{if(!set.closed)set.close();}catch(...){}throw;
    }
}
