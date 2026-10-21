#pragma once

// Guest TEST ONLY. Include after the existing layout, geometry-history,
// interruption, resize-magnet and remaining-group helpers. The height-only
// branch does not replay the two already-observed group/other-source gestures.
void owned_group_remaining_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    const bool height_only=s.suite=="GroupHeightOnly";
    if((!height_only&&s.suite!="GroupRemainderOnly")||!original.enabled||!original.rules.empty()||s.left||s.control||
       (GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_CONTROL)&0x8000))
        throw std::runtime_error("group_remaining_exact_enabled_released_guest_required");
    auto config=original;config.glue=true;config.magnet_move=!height_only;config.magnet_resize=false;config.docking=false;
    config.preview=false;config.feedback=false;config.drag_transparency=false;config.shake=false;
    config.attraction_dip=10;config.release_dip=16;config.speed_dip=20000;
    auto applied=chain_gui_save(s,config);LayoutFixtures set{s};set.create();
    const auto placements=[&](const Gesture& g){
        std::string result,row;std::istringstream rows(product_log_since(s,g.offset));
        while(std::getline(rows,row))if(remaining_group_receipt(s,row,"placement")&&
            e::number(row,"\"generation\":")==g.generation)result+=row+'\n';return result;
    };
    const auto finish_up=[&](const Gesture& g,const std::vector<Rect>& expected){
        const auto before=placements(g);layout_up(set,g,expected);
        s.log.check("group_remaining_actual_UP_no_new_placement",placements(g)==before,
            "\"generation\":"+std::to_string(g.generation)+",\"same_generation_placement_rows_unchanged\":true");
        chain_check_product(s,pid,birth);
    };
    const auto resize_start=[&](const Gesture& g,std::size_t source,const Rect& before){
        const auto row=record(s,g.offset,"gesture_start",g.generation);
        s.log.check("group_remaining_actual_shared_Resize_start",remaining_group_receipt(s,row,"gesture_start")&&
            e::number(row,"\"generation\":")==g.generation&&e::number(row,"\"group_members\":")==2&&
            e::unique_value(row,"\"resize\":")=="true"&&e::unique_value(row,"\"ctrl\":")=="true"&&
            row.find("\"original\":"+rect(before)+",")!=row.npos&&set.owns(source),
            "\"source_member\":"+std::to_string(source)+",\"actual_start\":"+quote(row));
    };
    try{
        const auto work=chain_work(set.frames[0].hwnd);const auto dpi=GetDpiForWindow(set.frames[0].hwnd);
        // This finite single-monitor test does not manufacture a negative
        // desktop or broaden the existing display scope. Distances come from
        // the actual source DPI and GUI-saved configuration, not a 15px magic.
        if(work.width()<1900||work.height()<1000||!dpi||dpi>384)
            throw std::runtime_error("group_remaining_declared_actual_display_unavailable");
        const LONG entry=static_cast<LONG>((static_cast<std::uint64_t>(applied.attraction_dip)*dpi+48)/96);
        const LONG release=static_cast<LONG>((static_cast<std::uint64_t>(applied.release_dip)*dpi+48)/96);
        if(entry<=0||release<=entry||entry>=100)throw std::runtime_error("group_remaining_actual_distance_oracle_invalid");
        Gesture g{};std::uint64_t previous{};
        const Rect upper{100,100,500,500},lower{100,500,500,900};
        const std::vector<Rect> horizontal{upper,lower};
        if(!height_only){
        const LayoutLimit limits{120,100,work.width(),work.height()};
        const Rect a{work.l+240,work.t+200,work.l+640,work.t+600},
            b{work.l+640,work.t+200,work.l+1040,work.t+400},
            c{work.l+640,work.t+400,work.l+1040,work.t+600},
            target{work.l+1300,work.t+300,work.l+1620,work.t+700};
        const std::vector<Rect> initial{a,b,c,target};set.setup(initial,{limits,limits,limits,limits});
        const auto native=resize_magnet_native_baseline(set,initial);
        const LONG snapped_dx=target.l-c.r,raw_dx=snapped_dx-entry;
        const Rect raw_source=translated(a,raw_dx,0),raw_hull=translated({a.l,a.t,c.r,a.b},raw_dx,0);
        s.log.check("group_remaining_independent_hull_not_leader_oracle",raw_dx>0&&target.l-raw_hull.r==entry&&
            target.l-raw_source.r>release&&chain_settings(s)==applied,
            "\"actual_DPI\":"+std::to_string(dpi)+",\"attraction_DIP\":"+std::to_string(applied.attraction_dip)+
            ",\"entry_pixels\":"+std::to_string(entry)+",\"release_pixels\":"+std::to_string(release)+
            ",\"raw_source\":"+rect(raw_source)+",\"raw_group_hull\":"+rect(raw_hull)+
            ",\"stationary_nonmember\":"+rect(target)+",\"snapped_absolute_delta\":["+std::to_string(snapped_dx)+",0]");
        g=layout_down(set,0,HTCAPTION,true);const auto source_key=remaining_group_start(s,g,3,"group_remaining_hull_actual_three_members");
        std::vector<Rect> snapped{translated(a,snapped_dx,0),translated(b,snapped_dx,0),translated(c,snapped_dx,0),target};
        layout_phase(set,0,g,raw_dx,0,snapped,"group_remaining_actual_hull_snap_not_leader_snap");
        std::vector<std::uint64_t> keys{source_key};for(std::size_t n=1;n<3;++n)keys.push_back(remaining_group_actual_key(s,g,snapped[n]));
        unsigned calls{};bool exact=true;std::string row;std::istringstream rows(placements(g));
        while(std::getline(rows,row)){++calls;exact=exact&&e::unique_value(row,"\"native_attempted\":")=="true"&&
            e::unique_value(row,"\"native_succeeded\":")=="true"&&e::unique_value(row,"\"exact\":")=="true";}
        for(std::size_t n=0;n<initial.size();++n)exact=exact&&set.owns(n)&&visible(set.frames[n].hwnd)==snapped[n]&&
            geometry_history_positioning(set.frames[n].hwnd)==(n<3?translated(native[n],snapped_dx,0):native[n])&&
            GetDpiForWindow(set.frames[n].hwnd)==dpi;
        s.log.check("group_remaining_hull_three_exact_calls_rigid_offsets_nonmember_unchanged",exact&&calls==3,
            "\"generation\":"+std::to_string(g.generation)+",\"placement_records\":"+std::to_string(calls)+
            ",\"original_group_members\":3,\"each_member_absolute_delta\":["+std::to_string(snapped_dx)+
            ",0],\"target_visible_and_positioning_unchanged\":true,\"raw_leader_gap_outside_release\":true");
        remaining_group_native_scope(s,g,keys,"group_remaining_no_nonmember_native_call");finish_up(g,snapped);
        previous=g.generation;

        // The already-observed upper-source horizontal case is NOT repeated.
        // Start at the lower window's actual HTTOP and keep its DOWN anchor.
        config.magnet_move=false;applied=chain_gui_save(s,config);
        set.setup(horizontal,{{100,250,1000,1000},{100,300,1000,1000}});
        (void)resize_magnet_native_baseline(set,horizontal);
        g=layout_down(set,1,HTTOP,true);resize_start(g,1,lower);
        s.log.check("group_remaining_lower_source_new_generation",g.generation>previous);
        layout_phase(set,1,g,0,80,{{100,100,500,580},{100,580,500,900}},"group_remaining_lower_HTTOP_shared_line580");
        const std::vector<Rect> clamped{{100,100,500,600},{100,600,500,900}};
        layout_phase(set,1,g,0,180,clamped,"group_remaining_lower_HTTOP_min_intersection600");
        finish_up(g,clamped);previous=g.generation;
        }

        // Height alone is fixed; width is NOT fixed. No native placement is
        // allowed merely to discover the neighbor's declared height clamp.
        set.setup(horizontal,{{100,250,1000,1000},{100,400,1000,400}});
        const auto fixed_native=resize_magnet_native_baseline(set,horizontal);
        const auto prior=geometry_history_status(s);
        g=layout_down(set,0,HTBOTTOM,true);resize_start(g,0,upper);
        s.log.check("group_remaining_fixed_height_new_generation",g.generation>previous);
        Sleep(60);refuse_observed_terminal_motion(s,g);LARGE_INTEGER move_clock{};
        const DWORD move_native_tick=GetTickCount();
        if(!QueryPerformanceCounter(&move_clock)||move_clock.QuadPart<=0)
            throw std::runtime_error("group_remaining_fixed_height_move_clock_unknown");
        const POINT requested_cursor{g.anchor.x,g.anchor.y+80};cursor(s,requested_cursor);std::string captured_move;
        product::guest::CapturedReleaseFacts held_capture;
        const bool held_move=wait([&]{
            POINT actual_cursor{};if(!GetCursorPos(&actual_cursor)||actual_cursor.x!=requested_cursor.x||
                actual_cursor.y!=requested_cursor.y||GetForegroundWindow()!=set.frames[0].hwnd||
                !set.owns(0)||!set.owns(1)||!s.left||s.left_up_attempted||!s.control||
                !(GetAsyncKeyState(VK_LBUTTON)&0x8000)||!(GetAsyncKeyState(VK_CONTROL)&0x8000))return false;
            const auto hit=WindowFromPoint(actual_cursor);const auto root=hit?GetAncestor(hit,GA_ROOT):nullptr;
            held_capture=release_capture_facts(s,set.frames[0].hwnd,root,g.generation);
            if(!product::guest::captured_release_hit_allowed(held_capture))return false;
            captured_move.clear();std::istringstream observed(product_log_since(s,g.offset));std::string item;
            while(std::getline(observed,item))if(remaining_group_receipt(s,item,"gesture_input_resource")&&
                e::number(item,"\"generation\":")==g.generation&&e::quoted(item,"\"fact\":","overlay_mouse_route")&&
                e::number(item,"\"native_message\":")==WM_MOUSEMOVE&&
                e::number(item,"\"source\":")==reinterpret_cast<std::uintptr_t>(set.frames[0].hwnd)&&
                e::number(item,"\"source_pid\":")==GetCurrentProcessId()&&e::number(item,"\"source_tid\":")==set.thread&&
                e::number(item,"\"overlay\":")==held_capture.overlay&&
                e::number(item,"\"resource_observed_qpc\":").value_or(0)>=static_cast<std::uint64_t>(move_clock.QuadPart)){
                    const auto stamp=e::number(item,"\"native_message_time\":");
                    // Same native DWORD message-tick basis, wrap-aware and
                    // bounded by this one-second wait; not a QPC conversion or
                    // a reconstructed common Raw/WinEvent total ordering.
                    if(stamp&&*stamp<=MAXDWORD&&static_cast<DWORD>(static_cast<DWORD>(*stamp)-move_native_tick)<0x80000000u)
                        captured_move=item;
                }
            return !captured_move.empty()&&actual_up(s,g.offset,g.generation).empty()&&
                GetForegroundWindow()==set.frames[0].hwnd&&set.owns(0)&&set.owns(1)&&s.left&&!s.left_up_attempted;
        },1000);
        s.log.check("group_remaining_fixed_height_actual_held_motion_observation",held_move,
            "\"generation\":"+std::to_string(g.generation)+",\"actual_captured_WM_MOUSEMOVE\":"+quote(captured_move)+
            ",\"cursor_call_before_QPC\":"+std::to_string(move_clock.QuadPart)+
            ",\"cursor_call_before_native_tick_u32\":"+std::to_string(move_native_tick)+
            ",\"actual_capture_overlay\":"+std::to_string(held_capture.overlay)+
            ",\"actual_source_capture\":"+std::to_string(held_capture.source_capture)+
            ",\"actual_owner_capture\":"+std::to_string(held_capture.owner_capture)+
            ",\"unique_TEST_cursor_delta\":[0,80],\"solver_consumption_or_UP_not_inferred_from_captured_message\":true");
        finish_up(g,horizontal);const auto after=geometry_history_status(s);
        bool unchanged=placements(g).empty();for(std::size_t n=0;n<2;++n)unchanged=unchanged&&set.owns(n)&&
            visible(set.frames[n].hwnd)==horizontal[n]&&geometry_history_positioning(set.frames[n].hwnd)==fixed_native[n];
        const auto before_claims=e::number(prior,"\"native_callsite_claims\":"),after_claims=e::number(after,"\"native_callsite_claims\":");
        s.log.check("group_remaining_fixed_height_neighbour_clamps_whole_line_zero_native",unchanged&&before_claims&&after_claims&&*before_claims==*after_claims,
            "\"generation\":"+std::to_string(g.generation)+",\"actual_TEST_cursor_delta\":[0,80]"+
            ",\"neighbor_declared_min_max_visible_height\":[400,400],\"neighbor_width_not_fixed\":true"+
            ",\"shared_line_must_remain_y\":500,\"actual_source_and_neighbor_unchanged\":true"+
            ",\"placement_records\":0,\"new_native_claims\":0,\"prior_status\":"+quote(prior)+",\"after_status\":"+quote(after));
        const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        s.log.check("group_remaining_original_GUI_configuration_restored",restored==expected);
        chain_check_product(s,pid,birth);s.log.check("group_remaining_exact_test_windows_closed",set.close());
        s.log.emit("group_remaining_component","\"result\":"+quote(height_only?"PASS_SCOPED_FIXED_HEIGHT":"PASS_SCOPED_THREE_GESTURES")+
            ",\"same_product_process\":true,\"group_hull_not_leader_snap\":"+(height_only?"null":"true")+
            ",\"horizontal_lower_source\":"+(height_only?"null":"true")+",\"fixed_height_clamp\":true,"+
            "\"prior_group_and_lower_source_skipped\":"+(height_only?"true":"false")+","+
            "\"Full_suite\":\"NOT_RUN\",\"real_applications\":\"NOT_RUN\",\"human_UAT\":\"NOT_RUN\",\"whole_version_PASS\":false");
    }catch(...){
        pause_routing_failed_case_input(s);
        try{if(s.process&&!s.left&&!s.control)chain_gui_save(s,original);}catch(...){}
        try{if(!set.closed)set.close();}catch(...){}throw;
    }
}

// The single remaining M07-F04 native L-corner contract. It neither replays
// the already-observed T/L shared-edge cases nor the four-grid scene.
void owned_L_corner_remaining_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    if(s.suite!="LCornerOnly"||!original.enabled||!original.rules.empty()||s.left||s.control||
       (GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_CONTROL)&0x8000))
        throw std::runtime_error("L_corner_exact_enabled_released_guest_required");
    auto config=original;config.glue=true;config.magnet_move=false;config.magnet_resize=false;config.docking=false;
    config.preview=false;config.feedback=false;config.drag_transparency=false;config.shake=false;
    const auto applied=chain_gui_save(s,config);LayoutFixtures set{s};set.create();
    try{
        const std::vector<Rect> initial{{100,100,500,500},{500,100,900,300}};
        const std::vector<Rect> expected{{100,100,520,500},{520,100,900,330}};
        set.setup(initial,{{250,100,420,800},{370,100,600,800}});
        const auto native=resize_magnet_native_baseline(set,initial);
        const auto dpi=GetDpiForWindow(set.frames[1].hwnd);
        s.log.check("L_corner_independent_two_axis_oracle",chain_settings(s)==applied&&set.active==2&&dpi,
            "\"source_member\":1,\"native_hit_required\":"+std::to_string(HTBOTTOMLEFT)+
            ",\"cursor_total_delta\":[80,30],\"independent_shared_X_interval\":[350,520],"+
            "\"requested_shared_X\":580,\"clamped_shared_X\":520,\"independent_B_bottom_Y\":330,"+
            "\"A_visible_min_max_width\":[250,420],\"B_visible_min_max_width\":[370,600],"+
            "\"expected_A\":"+rect(expected[0])+",\"expected_B\":"+rect(expected[1])+",\"missing_C_not_created\":true");
        const auto g=layout_down(set,1,HTBOTTOMLEFT,true);
        const auto started=record(s,g.offset,"gesture_start",g.generation);const auto source_key=e::number(started,"\"key\":");
        s.log.check("L_corner_actual_sourceB_shared_corner_START",remaining_group_receipt(s,started,"gesture_start")&&
            source_key&&*source_key&&e::number(started,"\"group_members\":")==2&&
            e::unique_value(started,"\"resize\":")=="true"&&e::unique_value(started,"\"ctrl\":")=="true"&&
            started.find("\"original\":"+rect(initial[1])+",")!=started.npos&&set.owns(0)&&set.owns(1),
            "\"generation\":"+std::to_string(g.generation)+",\"actual_START\":"+quote(started));
        layout_phase(set,1,g,80,30,expected,"L_corner_actual_X_clamp520_Y_continues330");
        const std::vector<std::uint64_t> keys{remaining_group_actual_key(s,g,expected[0]),remaining_group_actual_key(s,g,expected[1])};
        remaining_group_native_scope(s,g,keys,"L_corner_only_exact_two_member_native_scope");
        const auto placements=[&]{std::string result,row;std::istringstream rows(product_log_since(s,g.offset));
            while(std::getline(rows,row))if(remaining_group_receipt(s,row,"placement")&&
                e::number(row,"\"generation\":")==g.generation)result+=row+'\n';return result;};
        unsigned calls{};bool exact=keys[0]!=keys[1]&&keys[1]==source_key;std::string row;std::istringstream rows(placements());
        while(std::getline(rows,row))if(e::unique_value(row,"\"native_attempted\":")=="true"){
            ++calls;exact=exact&&e::quoted(row,"\"intent_reason\":","shared_line_resize")&&
                e::unique_value(row,"\"native_succeeded\":")=="true"&&e::unique_value(row,"\"native_completion_known\":")=="true"&&
                e::unique_value(row,"\"post_context_valid\":")=="true";
        }
        for(std::size_t n=0;n<2;++n){
            const Rect expected_native{native[n].l+expected[n].l-initial[n].l,native[n].t+expected[n].t-initial[n].t,
                native[n].r+expected[n].r-initial[n].r,native[n].b+expected[n].b-initial[n].b};
            exact=exact&&set.owns(n)&&visible(set.frames[n].hwnd)==expected[n]&&
                geometry_history_positioning(set.frames[n].hwnd)==expected_native&&GetDpiForWindow(set.frames[n].hwnd)==dpi;
        }
        s.log.check("L_corner_actual_native_two_calls_frame_exact",exact&&calls==2,
            "\"generation\":"+std::to_string(g.generation)+",\"actual_native_attempts\":"+std::to_string(calls)+
            ",\"A_outer_top_bottom_unchanged\":true,\"B_outer_top_right_unchanged\":true,\"X_clamp_does_not_clamp_Y\":true");
        const auto before_up=placements();layout_up(set,g,expected);std::string gone;
        std::istringstream resources(product_log_since(s,g.offset));
        while(std::getline(resources,row))if(remaining_group_receipt(s,row,"gesture_input_resource")&&
            e::number(row,"\"generation\":")==g.generation&&e::quoted(row,"\"fact\":","isolation_gone"))gone=row;
        const auto ended=record(s,g.offset,"gesture_end",g.generation);
        s.log.check("L_corner_actual_NormalUp_no_new_placement_resources_gone",placements()==before_up&&
            remaining_group_receipt(s,ended,"gesture_end")&&e::unique_value(ended,"\"normal_up\":")=="true"&&
            remaining_group_receipt(s,gone,"gesture_input_resource")&&e::quoted(gone,"\"fact\":","isolation_gone")&&
            e::unique_value(gone,"\"release_succeeded\":")=="true"&&e::unique_value(gone,"\"detach_succeeded\":")=="true"&&
            e::unique_value(gone,"\"overlay_destroyed\":")=="true"&&e::unique_value(gone,"\"hotkey_unregistered\":")=="true",
            "\"generation\":"+std::to_string(g.generation)+",\"actual_end\":"+quote(ended)+",\"actual_resources_gone\":"+quote(gone));
        chain_check_product(s,pid,birth);const auto restored=chain_gui_save(s,original);auto restored_expected=original;
        restored_expected.generation=restored.generation;s.log.check("L_corner_original_GUI_configuration_restored",restored==restored_expected);
        s.log.check("L_corner_exact_test_windows_closed",set.close());chain_check_product(s,pid,birth);
        s.log.emit("L_corner_component",std::string("\"result\":\"PASS_SCOPED_ONE_L_CORNER\",\"actual_gestures\":1,")+
            "\"existing_T_L_edges_four_grid_not_replayed\":true,\"real_applications\":\"NOT_RUN\",\"human_UAT\":\"NOT_RUN\",\"whole_version_PASS\":false");
    }catch(...){
        pause_routing_failed_case_input(s);
        try{if(s.process&&!s.left&&!s.control)chain_gui_save(s,original);}catch(...){}
        try{if(!set.closed)set.close();}catch(...){}throw;
    }
}
