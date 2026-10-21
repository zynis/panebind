#pragma once

// TEST ONLY: the two remaining M10-F04 scopes, not another visual/exit suite.
// Real product input/resources/native receipts; no alpha return is mocked.
std::string transparency_resource(State& s,const Gesture& g,std::string_view fact){
    namespace e=product::guest::up_evidence_detail;
    std::string found;std::istringstream rows(product_log_since(s,g.offset));std::string row;
    while(std::getline(rows,row))if(e::quoted(row,"\"type\":","gesture_input_resource")&&
        e::quoted(row,"\"source_sha\":",s.product_source)&&e::number(row,"\"process_id\":")==s.product_pid&&
        e::number(row,"\"generation\":")==g.generation&&e::quoted(row,"\"fact\":",fact))found=row;
    return found;
}
void transparency_scope(State& s){
    settings_guest_scope(s);
    if(s.suite!="TransparencyRemainderOnly"||!desktop()||s.left||s.control||!chain_settings(s).enabled||
       (GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_CONTROL)&0x8000)||
       (GetAsyncKeyState(VK_SHIFT)&0x8000)||(GetAsyncKeyState(VK_MENU)&0x8000))
        throw std::runtime_error("transparency_exact_released_enabled_guest_required");
}
void transparency_restore(State& s,LayoutFixtures& set,const panebind::core::v1::Settings& original){
    const auto actual=chain_gui_save(s,original);auto expected=original;expected.generation=actual.generation;
    s.log.check("transparency_original_GUI_configuration_restored",actual==expected);
    s.log.check("transparency_exact_owned_roots_and_alias_closed",set.close()&&
        (!set.storage->alias_ever_registered||(set.storage->alias_unregistered&&!set.storage->alias_registered)),
        "\"alias_ever_registered\":"+std::string(set.storage->alias_ever_registered?"true":"false")+
        ",\"alias_actually_unregistered\":"+(set.storage->alias_unregistered?"true":"false")+
        ",\"unknown_or_user_resources_closed\":false");
}
void transparency_failure_cleanup(State& s,LayoutFixtures& set,const panebind::core::v1::Settings& original){
    if(s.left&&!s.left_up_attempted)try{button(s,false);}catch(...){s.log.failed=true;}
    if(!s.left&&!s.control)try{transparency_restore(s,set,original);}catch(const std::exception& error){
        s.log.failed=true;s.log.emit("transparency_failure_cleanup","\"reason\":"+quote(error.what())+
            ",\"original_failure_preserved\":true,\"cleanup_PASS_inferred\":false");}
}
void transparency_gone(State& s,LayoutFixtures& set,const Gesture& g){
    namespace e=product::guest::up_evidence_detail;
    std::string gone;const bool observed=wait([&]{gone=transparency_resource(s,g,"isolation_gone");return
        e::unique_value(gone,"\"overlay_destroyed\":")=="true"&&
        e::unique_value(gone,"\"hotkey_unregistered\":")=="true"&&
        e::unique_value(gone,"\"release_succeeded\":")=="true"&&
        e::unique_value(gone,"\"detach_succeeded\":")=="true";},2000);
    GUITHREADINFO actual{sizeof(actual)};
    s.log.check("transparency_real_resource_exit",observed&&GetGUIThreadInfo(set.thread,&actual)&&
        !actual.hwndCapture&&!actual.hwndMoveSize&&!actual.hwndMenuOwner,
        "\"generation\":"+std::to_string(g.generation)+",\"actual_Gone\":"+quote(gone)+
        ",\"ReleaseCapture_call_not_inferred_when_no_own_capture\":true,\"guest_destruction_used\":false");
}
[[maybe_unused]] void owned_transparency_focus_remaining_case(State& s){
    namespace v=panebind::core::v1;
    transparency_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;
    const auto birth=process_birth(s.process);auto config=original;
    config.magnet_move=false;config.magnet_resize=false;config.docking=false;config.shake=false;
    config.drag_transparency=true;config.drag_alpha=190;chain_gui_save(s,config);
    LayoutFixtures set{s};set.create();
    try{
        const auto work=chain_work(set.frames[0].hwnd);if(work.width()<1200||work.height()<650)
            throw std::runtime_error("transparency_two_owned_sources_need_actual_work_area");
        const std::vector<Rect> initial{{work.l+180,work.t+150,work.l+580,work.t+450},
            {work.l+730,work.t+150,work.l+1130,work.t+450}};
        set.setup(initial);const HWND source=set.frames[0].hwnd,other=set.frames[1].hwnd;
        own_visual_setup(s,source,173,true);const auto before=native_visual(source);
        if(!before||!before->known||before->alpha!=173||!before->topmost)
            throw std::runtime_error("transparency_actual_original_173_topmost_required");
        const auto g=layout_down(set,0,HTCAPTION,false);visual_exact(s,source,190,true,"transparency_focus_actual_held_alpha190");
        auto moved=initial;moved[0]=translated(initial[0],35,25);
        layout_phase(set,0,g,35,25,moved,"transparency_focus_actual_Move_before_loss");
        const auto ready=transparency_resource(s,g,"capture_ready");
        if(ready.empty()||!s.left||s.left_up_attempted||!actual_up(s,g.offset,g.generation).empty())
            throw std::runtime_error("transparency_focus_actual_held_capture_required");
        LARGE_INTEGER trigger{};if(!QueryPerformanceCounter(&trigger)||trigger.QuadPart<=0)
            throw std::runtime_error("transparency_focus_trigger_clock_unknown");
        const bool requested=set.owns(1)&&SetForegroundWindow(other)!=FALSE;
        const bool changed=requested&&wait([&]{return set.owns(1)&&GetForegroundWindow()==other;},1000);
        s.log.check("transparency_actual_owned_FocusLost_trigger",changed,
            "\"generation\":"+std::to_string(g.generation)+",\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+
            ",\"other_exact_owned\":"+std::to_string(reinterpret_cast<std::uintptr_t>(other))+
            ",\"trigger_before_qpc\":"+std::to_string(trigger.QuadPart)+",\"actual_capture_ready\":"+quote(ready)+
            ",\"product_SetForegroundWindow_or_abort_handler_called\":false,\"test_UP_not_yet_sent\":true");
        (void)interruption_end(s,g,v::End::FocusLost,false,source);
        transparency_gone(s,set,g);visual_exact(s,source,173,true,"transparency_FocusLost_restores_original_173_not_255");
        s.log.check("transparency_focus_geometry_and_foreground_preserved",set.owns(0)&&visible(source)==moved[0]&&GetForegroundWindow()==other);
        button(s,false);const auto cleanup=abort_cleanup_up_receipt(s,g.offset,g.generation);
        s.log.check("transparency_single_actual_abort_cleanup_UP",cleanup.matched&&!s.left,
            "\"NormalUp_inferred\":false,"+cleanup.json());
        interruption_idle(s,set,true);interruption_audit_calls(s,g,static_cast<std::uint64_t>(trigger.QuadPart),std::nullopt);
        chain_check_product(s,pid,birth);
        const auto next=layout_down(set,0,HTCAPTION,false);s.log.check("transparency_same_instance_fresh_generation",next.generation>g.generation);
        moved[0]=translated(moved[0],25,20);layout_phase(set,0,next,25,20,moved,"transparency_after_focus_same_instance_Move");
        layout_up(set,next,moved);visual_exact(s,source,173,true,"transparency_after_focus_NormalUp_restores_original");
        unsigned late{},unknown{};std::istringstream rows(product_log_since(s,g.offset));std::string row;
        while(std::getline(rows,row))if(row.find("\"type\":\"placement\"")!=row.npos&&number(row,"\"generation\":")==g.generation&&
            row.find("\"native_callsite_claimed\":true")!=row.npos){const auto qpc=number(row,"\"native_before_qpc\":");
            if(!qpc)++unknown;else if(qpc>=static_cast<std::uint64_t>(trigger.QuadPart))++late;}
        s.log.check("transparency_retired_generation_no_late_motion_after_successor",!late&&!unknown,
            "\"retired_generation\":"+std::to_string(g.generation)+",\"late_native_calls\":"+std::to_string(late)+
            ",\"unknown_native_clocks\":"+std::to_string(unknown)+",\"owned_alpha_cleanup_not_new_motion\":true");
        chain_check_product(s,pid,birth);transparency_restore(s,set,original);
        s.log.emit("transparency_focus_remaining_component","\"result\":\"PASS\",\"same_product_instance\":true,"+
            std::string("\"real_FocusLost_and_original_alpha_restore\":true,\"following_Move_NormalUp\":true,\"exit_suite_repeated\":false,\"human_uat\":\"NOT_RUN\""));
    }catch(...){transparency_failure_cleanup(s,set,original);throw;}
}
[[maybe_unused]] void owned_transparency_unsupported_remaining_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    transparency_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;
    const auto birth=process_birth(s.process);auto config=original;
    config.magnet_move=false;config.magnet_resize=false;config.docking=false;config.shake=false;
    config.drag_transparency=true;config.drag_alpha=190;chain_gui_save(s,config);
    LayoutFixtures set{s};set.create(false,true);
    try{
        const auto work=chain_work(set.frames[0].hwnd);if(work.width()<1200||work.height()<650)
            throw std::runtime_error("transparency_own_dc_actual_work_area_too_small");
        const std::vector<Rect> initial{{work.l+180,work.t+150,work.l+580,work.t+450},
            {work.l+730,work.t+150,work.l+1130,work.t+450}};
        set.setup(initial);const HWND source=set.frames[1].hwnd;const auto before=native_visual(source);
        RECT before_native{};const bool before_position_known=GetWindowRect(source,&before_native)!=FALSE;
        const auto style=GetClassLongPtrW(source,GCL_STYLE);
        s.log.check("transparency_actual_OWNDC_unsupported_layered_precondition",set.owns(1)&&before&&before->known&&
            !before->layered&&!before->topmost&&before_position_known&&(style&CS_OWNDC)!=0,
            "\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+
            ",\"actual_class_style\":"+std::to_string(style)+",\"original_layered\":false,\"mock_API_failure_used\":false");
        const auto g=layout_down(set,1,HTCAPTION,false);std::string mutation;
        const bool visual_observed=wait([&]{mutation=record(s,g.offset,"visual_mutation",g.generation);return !mutation.empty();},1000);
        const bool native_sequence_rejected=visual_observed&&e::unique_value(mutation,"\"native_attempted\":")=="true"&&
            e::unique_value(mutation,"\"native_completion_known\":")=="true"&&
            e::unique_value(mutation,"\"native_sequence_returned_success\":")=="false"&&
            (e::unique_value(mutation,"\"style_native_attempted\":")=="true"||e::unique_value(mutation,"\"alpha_native_attempted\":")=="true");
        s.log.emit("transparency_actual_unsupported_visual_observation","\"generation\":"+std::to_string(g.generation)+
            ",\"actual_product_mutation\":"+quote(mutation)+",\"native_sequence_rejected\":"+(native_sequence_rejected?"true":"false")+
            ",\"individual_API_raw_return_or_error\":\"UNKNOWN_NOT_LOGGED_BY_THIS_CANDIDATE\",\"expected_Win32_error_not_invented\":true");
        auto moved=initial;moved[1]=translated(initial[1],35,25);
        layout_phase(set,1,g,35,25,moved,"transparency_unsupported_alpha_free_Move_still_exact");layout_up(set,g,moved);
        transparency_gone(s,set,g);
        const auto after=native_visual(source);std::string notice;
        {std::istringstream rows(product_log_since(s,g.offset));std::string row;
            while(std::getline(rows,row))if(e::quoted(row,"\"type\":","notice")&&e::number(row,"\"process_id\":")==pid&&
                e::quoted(row,"\"source_sha\":",s.product_source))notice=row;}
        const bool limitation=notice.find("透明")!=notice.npos&&
            (notice.find("不支持")!=notice.npos||notice.find("未应用")!=notice.npos||notice.find("无法")!=notice.npos);
        const bool unchanged=before&&after&&*after==*before;
        s.log.emit("transparency_unsupported_after_real_UP_facts","\"native_sequence_rejected\":"+std::string(native_sequence_rejected?"true":"false")+
            ",\"original_native_tuple_preserved\":"+(unchanged?"true":"false")+
            ",\"actual_notice\":"+(notice.empty()?"null":quote(notice))+",\"limitation_diagnostic_observed\":"+(limitation?"true":"false")+
            ",\"visible_limitation_feedback\":\"NOT_PROVEN_BY_NOTICE_LOG\",\"ordinary_Move_and_actual_UP_checked_separately\":true");
        chain_check_product(s,pid,birth);transparency_restore(s,set,original);
        s.log.check("transparency_real_native_rejection_not_mock",native_sequence_rejected,
            "\"actual_product_mutation\":"+quote(mutation)+",\"class_documentation_alone_not_runtime_proof\":true");
        s.log.check("transparency_unsupported_source_original_tuple_safe",unchanged);
        // This must honestly FAIL on a candidate which silently ignores the
        // unsupported request. A notice log alone is not visual acceptance.
        s.log.check("transparency_unsupported_drag_has_explicit_limitation_diagnostic",limitation,
            "\"actual_notice\":"+(notice.empty()?"null":quote(notice))+",\"visible_UI_still_requires_actual_observation\":true");
        s.log.emit("transparency_unsupported_remaining_component","\"result\":\"PARTIAL\",\"safety_and_Move_checked\":true,"+
            std::string("\"visible_limitation_feedback\":\"NOT_RUN\",\"whole_M10_F04_PASS\":false,\"human_uat\":\"NOT_RUN\""));
    }catch(...){if(!set.closed)transparency_failure_cleanup(s,set,original);throw;}
}
