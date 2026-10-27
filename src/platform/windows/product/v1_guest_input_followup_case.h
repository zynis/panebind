#pragma once

// TEST ONLY: two actual UP dock commits, not an eight-zone/stability replay.
// The source clock is the shield WndProc's legacy LEFTUP dispatch, never a
// replacement for WH_MOUSE_LL. Both origins remain visible in native receipts.
void owned_legacy_dock_timing_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    if(s.suite!="InputFollowupOnly"||!original.enabled||!original.rules.empty()||s.left||s.control)
        throw std::runtime_error("legacy_dock_timing_exact_idle_guest_required");
    auto config=original;config.docking=true;config.smart_docking=false;
    config.magnet_move=false;config.magnet_resize=false;config.glue=false;
    config.preview=true;config.feedback=false;config.shake=false;config.drag_transparency=false;
    config.dock_actions=panebind::core::v1::default_settings().dock_actions;
    const auto applied=chain_gui_save(s,config);LayoutFixtures set{s};set.create();
    try{
        const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<1200||work.height()<850)throw std::runtime_error("legacy_dock_timing_work_area_unavailable");
        const LONG mx=work.l+work.width()/2,my=work.t+work.height()/2;
        const std::array<Rect,2> initial{{{work.l+180,work.t+160,work.l+580,work.t+460},
            {work.l+700,work.t+200,work.l+1100,work.t+500}}};
        const std::array<POINT,2> cursor_targets{{{work.l+1,work.t+1},{work.r-1,work.b-1}}};
        const std::array<Rect,2> expected{{{work.l,work.t,mx,my},{mx,my,work.r,work.b}}};
        const LayoutLimit limits{100,100,work.width(),work.height()};
        set.setup({initial[0],initial[1]},{limits,limits});
        std::uint64_t previous_generation{};
        for(std::size_t n=0;n<initial.size();++n){
            chain_check_product(s,pid,birth);
            const auto g=layout_down(set,n,HTCAPTION,false);
            const LONG dx=cursor_targets[n].x-g.anchor.x,dy=cursor_targets[n].y-g.anchor.y;
            const auto first=visible(set.frames[0].hwnd),second=visible(set.frames[1].hwnd);
            if(!set.owns(0)||!set.owns(1)||!first||!second)
                throw std::runtime_error("legacy_dock_exact_owned_geometry_missing");
            std::vector<Rect> actual_expected{*first,*second};
            actual_expected[n]=translated(initial[n],dx,dy);
            layout_phase(set,n,g,dx,dy,actual_expected,"legacy_dock_actual_Move_to_zone");
            chain_preview_exact(s,set.frames[n].hwnd,expected[n]);
            s.log.check("legacy_dock_no_discrete_write_before_actual_UP",
                g.generation>previous_generation&&s.left&&dock_remaining_commit_receipts(s,g).empty());
            actual_expected[n]=expected[n];layout_up(set,g,actual_expected);
            const auto native=placement(s,g.offset,g.generation,expected[n],true);
            const auto up=actual_up(s,g.offset,g.generation);
            const auto commits=dock_remaining_commit_receipts(s,g);
            const auto dispatch=e::number(native,"\"legacy_up_dispatch_qpc\":");
            const auto observed=e::number(up,"\"resource_observed_qpc\":");
            const auto post=e::number(native,"\"post_observed_qpc\":");
            const bool clocks=dispatch&&*dispatch&&observed&&*dispatch==*observed&&post&&*post>=*dispatch;
            s.log.check("legacy_dock_actual_dispatch_to_exact_postverify_separate_from_hook",
                commits.size()==1&&commits.front()==native&&geometry_history_receipt(s,native,"placement")&&
                e::quoted(up,"\"source\":","shield_queued_legacy")&&e::quoted(up,"\"kind\":","UP")&&
                e::number(up,"\"generation\":")==g.generation&&e::number(native,"\"hook_observed_qpc\":")==0&&
                e::unique_value(native,"\"native_callsite_claimed\":")=="true"&&
                e::unique_value(native,"\"native_completion_known\":")=="true"&&
                e::unique_value(native,"\"post_context_valid\":")=="true"&&clocks,
                "\"member\":"+std::to_string(n)+",\"generation\":"+std::to_string(g.generation)+
                ",\"actual_UP\":"+quote(up)+",\"actual_native_receipt\":"+quote(native)+
                ",\"clock_origin\":\"shield_WndProc_legacy_LEFTUP_dispatch_not_hook_or_hardware\","+
                "\"old_122_hook_samples_rebound\":false,\"not_physical_display_latency\":true");
            previous_generation=g.generation;
        }
        chain_check_product(s,pid,birth);s.log.check("legacy_dock_test_windows_closed",set.close());
        const auto restored=chain_gui_save(s,original);auto wanted=original;wanted.generation=restored.generation;
        s.log.check("legacy_dock_original_configuration_restored",restored==wanted&&!s.left&&!s.control);
        s.log.emit("legacy_dock_timing_component","\"result\":\"TWO_ACTUAL_LEGACY_UP_DOCK_COMMITS_CHECKED\","
            "\"old_Run_dispatch_clocks\":\"NOT_RECOVERABLE_FROM_RETAINED_RECORDS\","
            "\"hook_population_merged\":false,\"eight_zone_replay\":false,\"human_uat\":\"NOT_RUN\"");
    }catch(...){pause_routing_failed_case_input(s);
        try{if(s.process&&!s.left&&!s.control&&GetProcessId(s.process)==pid&&process_birth(s.process)==birth&&
            WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&chain_settings(s)==applied)chain_gui_save(s,original);}catch(...){}
        throw;
    }
}
