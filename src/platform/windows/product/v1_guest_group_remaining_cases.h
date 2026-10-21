#pragma once

// Guest TEST ONLY. Include after the existing layout, geometry-history,
// interruption, resize-magnet and remaining-group helpers. Three gestures
// fill concrete independent contracts; this is not another layout/Full suite.
void owned_group_remaining_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    if(s.suite!="GroupRemainderOnly"||!original.enabled||!original.rules.empty()||s.left||s.control||
       (GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_CONTROL)&0x8000))
        throw std::runtime_error("group_remaining_exact_enabled_released_guest_required");
    auto config=original;config.glue=true;config.magnet_move=true;config.magnet_resize=false;config.docking=false;
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
        auto g=layout_down(set,0,HTCAPTION,true);const auto source_key=remaining_group_start(s,g,3,"group_remaining_hull_actual_three_members");
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
        auto previous=g.generation;

        // The already-observed upper-source horizontal case is NOT repeated.
        // Start at the lower window's actual HTTOP and keep its DOWN anchor.
        config.magnet_move=false;applied=chain_gui_save(s,config);
        const Rect upper{100,100,500,500},lower{100,500,500,900};
        const std::vector<Rect> horizontal{upper,lower};
        set.setup(horizontal,{{100,250,1000,1000},{100,300,1000,1000}});
        (void)resize_magnet_native_baseline(set,horizontal);
        g=layout_down(set,1,HTTOP,true);resize_start(g,1,lower);
        s.log.check("group_remaining_lower_source_new_generation",g.generation>previous);
        layout_phase(set,1,g,0,80,{{100,100,500,580},{100,580,500,900}},"group_remaining_lower_HTTOP_shared_line580");
        const std::vector<Rect> clamped{{100,100,500,600},{100,600,500,900}};
        layout_phase(set,1,g,0,180,clamped,"group_remaining_lower_HTTOP_min_intersection600");
        finish_up(g,clamped);previous=g.generation;

        // Height alone is fixed; width is NOT fixed. No native placement is
        // allowed merely to discover the neighbor's declared height clamp.
        set.setup(horizontal,{{100,250,1000,1000},{100,400,1000,400}});
        const auto fixed_native=resize_magnet_native_baseline(set,horizontal);
        const auto prior=geometry_history_status(s);
        g=layout_down(set,0,HTBOTTOM,true);resize_start(g,0,upper);
        s.log.check("group_remaining_fixed_height_new_generation",g.generation>previous);
        Sleep(60);refuse_observed_terminal_motion(s,g);LARGE_INTEGER move_clock{};
        if(!QueryPerformanceCounter(&move_clock)||move_clock.QuadPart<=0)
            throw std::runtime_error("group_remaining_fixed_height_move_clock_unknown");
        cursor(s,{g.anchor.x,g.anchor.y+80});std::string raw_move;
        const bool held_move=wait([&]{std::istringstream observed(product_log_since(s,g.offset));std::string item;
            while(std::getline(observed,item))if(remaining_group_receipt(s,item,"gesture_input_resource")&&
                e::number(item,"\"generation\":")==g.generation&&e::quoted(item,"\"fact\":","raw_mouse")&&
                e::number(item,"\"raw_button_flags\":")==0&&
                e::number(item,"\"resource_observed_qpc\":").value_or(0)>=static_cast<std::uint64_t>(move_clock.QuadPart))raw_move=item;
            return !raw_move.empty()&&s.left&&!s.left_up_attempted&&
                (GetAsyncKeyState(VK_LBUTTON)&0x8000)&&actual_up(s,g.offset,g.generation).empty();},1000);
        s.log.check("group_remaining_fixed_height_actual_held_motion_observation",held_move,
            "\"generation\":"+std::to_string(g.generation)+",\"actual_resource_motion\":"+quote(raw_move)+
            ",\"unique_TEST_cursor_delta\":[0,80],\"solver_consumption_or_UP_not_inferred_from_Raw\":true");
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
        s.log.emit("group_remaining_component","\"result\":\"PASS_SCOPED_THREE_GESTURES\",\"same_product_process\":true,"+
            std::string("\"group_hull_not_leader_snap\":true,\"horizontal_lower_source\":true,\"fixed_height_clamp\":true,")+
            "\"Full_suite\":\"NOT_RUN\",\"real_applications\":\"NOT_RUN\",\"human_UAT\":\"NOT_RUN\",\"whole_version_PASS\":false");
    }catch(...){
        pause_routing_failed_case_input(s);
        try{if(s.process&&!s.left&&!s.control)chain_gui_save(s,original);}catch(...){}
        try{if(!set.closed)set.close();}catch(...){}throw;
    }
}
