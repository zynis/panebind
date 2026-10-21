#pragma once

// TEST ONLY. Include after v1_guest_interruptions.h. Two real identity domains
// exercise the existing two callers; a Layout Frame is not a State source slot.
void motion_guard_observe_refusal(State& s,const Gesture& g,std::string_view caller,
    const std::string& ended,const std::function<void()>& invoke){
    namespace e=product::guest::up_evidence_detail;
    POINT before{},after{};
    if(!GetCursorPos(&before)||!g.generation||ended.empty()||!s.left||s.left_up_attempted)
        throw std::runtime_error("motion_guard_missing_actual_terminal_pressed_context");
    const auto mark=s.log.mark(),sequence=s.log.sequence;
    const auto down_qpc=s.left_down_send_qpc,up_qpc=s.left_up_send_qpc;
    const bool control=s.control;bool refused{};std::string exception;
    try{invoke();}catch(const std::exception& error){exception=error.what();
        refused=exception=="test_motion_after_observed_termination_refused";}
    const auto text=s.log.since(mark);std::istringstream rows(text);std::string row,receipt;unsigned count{};
    while(std::getline(rows,row)){if(row.empty())continue;++count;receipt=row;}
    const bool actual=refused&&count==1&&s.log.sequence==sequence+1&&
        e::quoted(receipt,"\"schema\":","panebind-v1-owned-driver/1")&&
        e::quoted(receipt,"\"source_sha\":",PANEBIND_BUILD_SHA)&&
        e::quoted(receipt,"\"type\":","test_motion_not_sent")&&
        e::number(receipt,"\"generation\":")==g.generation&&
        e::quoted(receipt,"\"reason\":","actual_product_generation_already_terminated")&&
        receipt.find("\"production_end\":"+quote(ended)+",")!=receipt.npos&&
        e::unique_value(receipt,"\"SendInput_called\":")=="false"&&
        e::unique_value(receipt,"\"interrupted_chain_PASS_inferred\":")=="false"&&
        GetCursorPos(&after)&&before.x==after.x&&before.y==after.y&&
        s.left&&!s.left_up_attempted&&s.control==control&&
        s.left_down_send_qpc==down_qpc&&s.left_up_send_qpc==up_qpc;
    s.log.check("motion_guard_actual_caller_refused_without_test_input",actual,
        "\"caller\":"+quote(caller)+",\"generation\":"+std::to_string(g.generation)+
        ",\"actual_exception\":"+quote(exception)+",\"actual_guard_receipt\":"+quote(receipt)+
        ",\"driver_rows_in_exact_call_interval\":"+std::to_string(count)+
        ",\"cursor_before\":["+std::to_string(before.x)+","+std::to_string(before.y)+
        "],\"cursor_after\":"+(actual?"["+std::to_string(after.x)+","+std::to_string(after.y)+"]":"null")+
        ",\"input_send_ledger_unchanged_required\":true,\"tool_refusal_only\":true,"+
        "\"original_interrupted_chain_result\":\"FAIL\",\"historical_c899_FAIL_preserved\":true,"+
        "\"atomic_future_permission_inferred\":false");
}

void motion_guard_switch_and_cleanup(State& s,const Gesture& g,HWND source,HWND control,
    DWORD source_tid,const std::function<bool()>& exact,const std::function<void(const std::string&)>& guarded_call){
    namespace e=product::guest::up_evidence_detail;
    if(!exact()||!source||source==control||!s.left||s.left_up_attempted||
       !actual_up(s,g.offset,g.generation).empty())throw std::runtime_error("motion_guard_live_exact_source_required");
    const auto source_rect=visible(source),control_rect=visible(control);
    if(!source_rect||!control_rect)throw std::runtime_error("motion_guard_actual_geometry_unknown");
    LARGE_INTEGER trigger{};if(!QueryPerformanceCounter(&trigger)||trigger.QuadPart<=0)
        throw std::runtime_error("motion_guard_switch_clock_unknown");
    const bool requested=SetForegroundWindow(control)!=FALSE;
    s.log.check("motion_guard_real_exact_owned_foreground_switch",requested&&wait([&]{return exact()&&GetForegroundWindow()==control;}),
        "\"generation\":"+std::to_string(g.generation)+",\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+
        ",\"control\":"+std::to_string(reinterpret_cast<std::uintptr_t>(control))+
        ",\"test_owned_foreground_setup_not_product_retake\":true");
    const auto ended=interruption_end(s,g,panebind::core::v1::End::FocusLost,false,source);
    s.log.check("motion_guard_real_bound_terminal_identity",e::quoted(ended,"\"schema\":","panebind-v1/1")&&
        e::quoted(ended,"\"source_sha\":",s.product_source)&&e::number(ended,"\"process_id\":")==s.product_pid&&
        e::number(ended,"\"generation\":")==g.generation&&exact()&&GetForegroundWindow()==control&&
        visible(source)==source_rect&&visible(control)==control_rect&&!s.left_up_attempted);
    guarded_call(ended);
    // Observe teardown before the unique TEST cleanup UP. Capture can already
    // have been released by Windows; the existing abort receipt checks that
    // distinction instead of requiring an invented successful ReleaseCapture.
    std::string gone;
    const bool resources_gone=wait([&]{
        gone.clear();std::istringstream rows(product_log_since(s,g.offset));std::string row;
        while(std::getline(rows,row))if(e::quoted(row,"\"type\":","gesture_input_resource")&&
            e::number(row,"\"generation\":")==g.generation&&e::quoted(row,"\"fact\":","isolation_gone"))gone=row;
        GUITHREADINFO gui{sizeof(gui)};const auto owner=e::number(gone,"\"capture_owner_tid\":");
        GUITHREADINFO resource_gui{sizeof(resource_gui)};
        return exact()&&e::quoted(gone,"\"schema\":","panebind-v1/1")&&
            e::quoted(gone,"\"source_sha\":",s.product_source)&&e::number(gone,"\"process_id\":")==s.product_pid&&
            e::number(gone,"\"source\":")==reinterpret_cast<std::uintptr_t>(source)&&
            e::number(gone,"\"source_pid\":")==GetCurrentProcessId()&&e::number(gone,"\"source_tid\":")==source_tid&&
            e::unique_value(gone,"\"overlay_destroyed\":")=="true"&&
            e::unique_value(gone,"\"hotkey_unregistered\":")=="true"&&
            e::unique_value(gone,"\"detach_succeeded\":")=="true"&&
            GetGUIThreadInfo(source_tid,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner&&
            owner&&*owner&&GetGUIThreadInfo(static_cast<DWORD>(*owner),&resource_gui)&&!resource_gui.hwndCapture;
    },1500);
    POINT point{};const bool cursor_known=GetCursorPos(&point)!=FALSE;
    const HWND hit=cursor_known?GetAncestor(WindowFromPoint(point),GA_ROOT):nullptr;
    // The two known blank windows are separate. Keep the cursor at the source,
    // with the other known window foreground, so button(false) uses its existing
    // standalone UP branch, not the normal final-MOVE/UP pair.
    s.log.check("motion_guard_unique_cleanup_UP_destination",resources_gone&&exact()&&s.left&&!s.left_up_attempted&&
        cursor_known&&hit==source&&GetForegroundWindow()==control,
        "\"actual_resource_gone\":"+quote(gone)+",\"cursor_root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+
        ",\"no_cleanup_cursor_movement\":true,\"normal_UP_not_inferred\":true");
    button(s,false);const auto receipt=abort_cleanup_up_receipt(s,g.offset,g.generation);
    s.log.check("motion_guard_actual_single_abort_cleanup_UP",receipt.matched&&!s.left&&s.left_up_attempted,receipt.json());
    interruption_audit_calls(s,g,static_cast<std::uint64_t>(trigger.QuadPart),std::nullopt);
    s.log.check("motion_guard_no_geometry_overwrite_or_foreground_retake",exact()&&
        GetForegroundWindow()==control&&visible(source)==source_rect&&visible(control)==control_rect);
}

void owned_observed_motion_guard_case(State& s){
    if(s.suite!="MotionGuardOnly"||!desktop()||s.left||s.control)
        throw std::runtime_error("motion_guard_exact_released_guest_suite_required");
    const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    const auto binary=hash(s.product);
    if(!original.enabled)throw std::runtime_error("motion_guard_enabled_product_required");
    chain_check_product(s,pid,birth);
    auto config=original;config.magnet_move=false;config.magnet_resize=false;config.docking=false;
    config.drag_transparency=false;config.shake=false;chain_gui_save(s,config);
    try{
        {
            LayoutFixtures set{s};set.create();const auto work=chain_work(set.frames[0].hwnd);
            if(work.width()<1250||work.height()<700)throw std::runtime_error("motion_guard_actual_work_area_too_small");
            const std::vector<Rect> initial{{work.l+220,work.t+180,work.l+620,work.t+480},
                {work.l+760,work.t+180,work.l+1160,work.t+480}};
            const LayoutLimit limit{100,100,work.width(),work.height()};set.setup(initial,{limit,limit});
            try{
                const auto g=layout_down(set,0,HTCAPTION,false);auto moved=initial;moved[0]=translated(initial[0],-40,35);
                layout_phase(set,0,g,-40,35,moved,"motion_guard_layout_real_active_Move");
                motion_guard_switch_and_cleanup(s,g,set.frames[0].hwnd,set.frames[1].hwnd,set.thread,
                    [&]{return set.owns(0)&&set.owns(1);},[&](const std::string& ended){
                        motion_guard_observe_refusal(s,g,"layout_phase",ended,[&]{
                            auto rejected=moved;rejected[0]=translated(initial[0],65,45);
                            layout_phase(set,0,g,65,45,rejected,"motion_guard_layout_unexpected_post_end_Move");});
                    });
                interruption_idle(s,set,true);interruption_recovery(s,set,g.generation);chain_check_product(s,pid,birth);
                s.log.check("motion_guard_layout_identity_resources_closed",set.close());
            }catch(...){
                if(s.left&&!s.left_up_attempted)try{button(s,false);}catch(...){}
                if(s.control)try{key(s,VK_CONTROL,false);}catch(...){}
                try{if(!set.closed)set.close();}catch(...){}throw;
            }
        }
        // The original State windows have their own Frame/authority contract.
        // Reuse that actual source path rather than adapting a Layout identity.
        if(!own(s,s.frames[0].hwnd)||!own(s,s.frames[1].hwnd)||s.explorer[0]||s.applications[0])
            throw std::runtime_error("motion_guard_original_State_sources_required");
        const auto before=visible(s.frames[0].hwnd);if(!before)throw std::runtime_error("motion_guard_State_geometry_unknown");
        const auto g=down(s,0,false,HTCAPTION);const auto moved=translated(*before,-25,15);
        phase(s,0,g,-25,15,moved,"motion_guard_State_real_active_Move",true);
        motion_guard_switch_and_cleanup(s,g,s.frames[0].hwnd,s.frames[1].hwnd,s.owner,
            [&]{return own(s,s.frames[0].hwnd)&&own(s,s.frames[1].hwnd);},[&](const std::string& ended){
                motion_guard_observe_refusal(s,g,"phase",ended,[&]{
                    phase(s,0,g,65,45,translated(*before,65,45),"motion_guard_State_unexpected_post_end_Move",true);});
            });
        const auto idle_mark=read_product(s).size();
        s.log.check("motion_guard_State_actual_idle_before_new_gesture",launch(s,L"--status",true)&&wait([&]{
            const auto row=record(s,idle_mark,"status");GUITHREADINFO gui{sizeof(gui)};
            return product::guest::matching_idle_status(row,{pid,s.product_source,0,0})&&
                GetGUIThreadInfo(s.owner,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner;
        }));
        const auto actual=visible(s.frames[0].hwnd);if(!actual)throw std::runtime_error("motion_guard_State_recovery_geometry_unknown");
        auto next=down(s,0,false,HTCAPTION);
        s.log.check("motion_guard_State_recovery_new_generation",next.generation>g.generation);
        const auto recovered=translated(*actual,35,25);
        phase(s,0,next,35,25,recovered,"motion_guard_State_following_Move_exact",true);up(s,0,next,recovered);
        next=down(s,0,false,HTBOTTOM);auto resized=recovered;resized.b+=24;
        phase(s,0,next,0,24,resized,"motion_guard_State_following_Resize_exact",true);up(s,0,next,resized);
        chain_check_product(s,pid,birth);
        const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        s.log.check("motion_guard_original_configuration_and_product_identity_retained",restored==expected&&hash(s.product)==binary);
        s.log.emit("motion_guard_actual_two_callers_and_following_recovery",
            "\"result\":\"PASS\",\"scope\":\"test_driver_refusal_and_current_product_owned_recovery_only\","
            "\"actual_guarded_callers\":[\"phase\",\"layout_phase\"],\"product_restarted\":false,"
            "\"historical_c899_FAIL_preserved\":true,\"whole_I08_or_M04_or_Full_PASS\":false,\"human_uat\":\"NOT_RUN\"");
    }catch(...){
        if(s.left&&!s.left_up_attempted)try{button(s,false);}catch(...){}
        if(s.control)try{key(s,VK_CONTROL,false);}catch(...){}
        if(!s.left&&!s.control)try{chain_gui_save(s,original);}catch(...){}throw;
    }
}
