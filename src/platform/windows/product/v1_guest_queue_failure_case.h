#pragma once

// TEST ONLY: one explicit exact-guest injected failure at the production
// InputService::push full-queue failure branch, never a claim of natural queue
// exhaustion, OS PostMessage failure or a silently removed low-level hook.
[[maybe_unused]] void owned_queue_failure_case(State& s){
    namespace v=panebind::core::v1;namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;
    const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if(s.suite!="QueueFailureOnly"||!original.enabled||!original.rules.empty()||s.left||s.control||
       s.explorer[0]||s.applications[0]||binary.empty()||binary!=hash(s.input_product))
        throw std::runtime_error("queue_failure_exact_enabled_Medium_owned_guest_required");
    std::array<wchar_t,32768> image{};const auto image_size=GetModuleFileNameW(nullptr,image.data(),static_cast<DWORD>(image.size()));
    if(!image_size||image_size>=image.size())throw std::runtime_error("queue_failure_driver_image_unknown");
    const auto driver_hash=hash(image.data());if(driver_hash.empty())throw std::runtime_error("queue_failure_driver_hash_unknown");
    auto config=original;config.glue=true;config.magnet_move=false;config.magnet_resize=false;
    config.docking=false;config.stretch=false;config.preview=false;config.feedback=false;
    config.drag_transparency=false;config.shake=false;chain_gui_save(s,config);
    LayoutFixtures set{s};set.create();
    const auto gone_for=[&](const Gesture& g){std::string result,row;std::istringstream rows(product_log_since(s,g.offset));
        while(std::getline(rows,row))if(geometry_history_receipt(s,row,"gesture_input_resource")&&
            e::number(row,"\"generation\":")==g.generation&&e::quoted(row,"\"fact\":","isolation_gone"))result=row;return result;};
    const auto clean_gone=[&](const std::string& row){return !row.empty()&&
        e::unique_value(row,"\"overlay_destroyed\":")=="true"&&e::unique_value(row,"\"hotkey_unregistered\":")=="true"&&
        e::unique_value(row,"\"release_succeeded\":")=="true"&&e::unique_value(row,"\"detach_succeeded\":")=="true"&&
        e::number(row,"\"capture_after_release\":")==0;};
    try{
        const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<1000||work.height()<700)throw std::runtime_error("queue_failure_actual_work_area_too_small");
        const Rect before{work.l+220,work.t+180,work.l+620,work.t+500};set.setup({before});
        const HWND source=set.frames[0].hwnd;const auto g=layout_down(set,0,HTCAPTION,false);
        const Rect held=translated(before,21,14);layout_phase(set,0,g,21,14,{held},"queue_failure_actual_exact_Move_before_arm");
        const auto first=placement(s,g.offset,g.generation,held,true);const auto start=record(s,g.offset,"gesture_start",g.generation);
        s.log.check("queue_failure_actual_plain_active_exact_source_before_arm",set.owns(0)&&s.left&&!s.left_up_attempted&&
            actual_up(s,g.offset,g.generation).empty()&&visible(source)==held&&
            e::unique_value(start,"\"ctrl\":")=="false"&&e::unique_value(start,"\"resize\":")=="false"&&
            e::unique_value(first,"\"exact\":")=="true"&&e::unique_value(first,"\"native_attempted\":")=="true",
            "\"actual_START\":"+quote(start)+",\"actual_first_native_receipt\":"+quote(first)+",\"fault_not_yet_requested\":true");
        const auto mark=read_product(s).size();
        const auto command=L"--guest-queue-failure "+s.run+L" "+std::to_wstring(g.generation)+L" "+
            std::to_wstring(reinterpret_cast<std::uintptr_t>(source));
        const bool sent=launch(s,command,true);std::string armed;
        const bool accepted=wait([&]{armed=record(s,mark,"guest_queue_failure_arm",g.generation);return
            geometry_history_receipt(s,armed,"guest_queue_failure_arm")&&e::unique_value(armed,"\"armed\":")=="true"&&
            e::unique_value(armed,"\"permit_armed\":")=="true"&&e::unique_value(armed,"\"failure_fired\":")=="false"&&
            e::quoted(armed,"\"fault_run_id\":",utf8(s.run))&&e::number(armed,"\"fault_generation\":")==g.generation&&
            e::number(armed,"\"fault_source_HWND\":")==reinterpret_cast<std::uintptr_t>(source)&&
            e::number(armed,"\"fault_source_pid\":")==GetCurrentProcessId()&&
            e::number(armed,"\"fault_source_tid\":")==set.thread&&
            e::quoted(armed,"\"fault_driver_sha256\":",driver_hash);});
        s.log.check("queue_failure_actual_GUI_one_shot_arm_receipt",sent&&accepted&&set.owns(0)&&s.left&&
            GetForegroundWindow()==source&&visible(source)==held,
            "\"CLI_return_not_arm_proof\":true,\"actual_arm\":"+quote(armed));
        const auto initial_serial=e::number(armed,"\"actual_input_owner_start_serial\":");
        s.log.check("queue_failure_actual_input_owner_before_fault",initial_serial&&*initial_serial>0&&
            e::number(armed,"\"current_input_owner_tid\":").value_or(0)>0&&
            e::unique_value(armed,"\"current_healthy\":")=="true"&&e::unique_value(armed,"\"current_available\":")=="true"&&
            e::number(armed,"\"current_authorized_generation\":")==g.generation);
        // This next real cursor quantum is deliberately NOT layout_phase:
        // its correct outcome is a failed publication and zero new placement.
        cursor(s,{g.anchor.x+45,g.anchor.y+31});std::string fault;
        const bool fired=wait([&]{fault=record(s,mark,"input_queue_failure");return
            geometry_history_receipt(s,fault,"input_queue_failure")&&e::quoted(fault,"\"origin\":","guest_injected_failure")&&
            e::unique_value(fault,"\"failure_fired\":")=="true"&&e::unique_value(fault,"\"permit_consumed\":")=="true"&&
            e::unique_value(fault,"\"permit_armed\":")=="false"&&e::quoted(fault,"\"kind\":","Move")&&
            e::number(fault,"\"fault_generation\":")==g.generation&&
            e::number(fault,"\"fault_source_HWND\":")==reinterpret_cast<std::uintptr_t>(source)&&
            e::number(fault,"\"trigger_qpc\":").value_or(0)>0&&
            e::unique_value(fault,"\"same_production_full_failure_branch\":")=="true";});
        s.log.check("queue_failure_actual_next_Move_fired_same_production_failure_branch",fired&&
            e::unique_value(fault,"\"current_healthy\":")=="false"&&e::unique_value(fault,"\"current_available\":")=="false"&&
            e::number(fault,"\"current_authorized_generation\":")==0&&e::number(fault,"\"current_completion_generation\":")==0,
            "\"actual_failure\":"+quote(fault)+",\"natural_full_or_OS_post_failure_inferred\":false");
        const auto trigger=e::number(fault,"\"trigger_qpc\":").value_or(0);
        const auto ended=interruption_end(s,g,v::End::QueueFailure,false,source);std::string gone;
        s.log.check("queue_failure_actual_independent_input_resources_gone_before_UP",wait([&]{gone=gone_for(g);return clean_gone(gone);},1500)&&
            set.owns(0)&&GetForegroundWindow()==source&&visible(source)==held&&s.left&&!s.left_up_attempted&&
            actual_up(s,g.offset,g.generation).empty(),"\"actual_end\":"+quote(ended)+",\"actual_gone\":"+quote(gone));
        interruption_audit_calls(s,g,trigger,std::nullopt);
        const auto unhealthy=geometry_history_status(s);
        s.log.check("queue_failure_actual_reported_unhealthy_idle_without_product_restart",
            e::unique_value(unhealthy,"\"hook_owner_running\":")=="false"&&
            e::unique_value(unhealthy,"\"gesture_active\":")=="false"&&
            e::unique_value(unhealthy,"\"native_operation_pending\":")=="false"&&
            e::unique_value(unhealthy,"\"native_completion_unknown\":")=="false",
            "\"actual_status\":"+quote(unhealthy)+",\"hook_owner_running_is_service_healthy_not_actual_unhook_receipt\":true,\"OS_silent_hook_query_not_claimed\":true");

        // The one originally planned TEST UP goes to the exact still-foreground
        // owned client after real abort/Gone. No extra UP/fake legacy message,
        // foreground repair or foreign capture operation belongs here.
        GUITHREADINFO gui{sizeof(gui)};RECT client{};
        if(!set.owns(0)||GetForegroundWindow()!=source||!GetGUIThreadInfo(set.thread,&gui)||gui.hwndCapture||gui.hwndMoveSize||
           !GetClientRect(source,&client))throw std::runtime_error("queue_failure_cleanup_exact_idle_source_unknown");
        POINT point{(client.left+client.right)/2,(client.top+client.bottom)/2};
        if(!ClientToScreen(source,&point))throw std::runtime_error("queue_failure_cleanup_client_point_unknown");cursor(s,point);
        POINT actual{};DWORD hit_pid{};const bool cursor_known=GetCursorPos(&actual)!=FALSE;
        const HWND hit=cursor_known?WindowFromPoint(actual):nullptr;const DWORD hit_tid=hit?GetWindowThreadProcessId(hit,&hit_pid):0;
        DWORD_PTR region{};const bool region_known=hit==source&&SendMessageTimeoutW(source,WM_NCHITTEST,0,
            MAKELPARAM(static_cast<short>(actual.x),static_cast<short>(actual.y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&region)!=FALSE;
        s.log.check("queue_failure_original_UP_exact_TEST_client_destination",set.owns(0)&&GetForegroundWindow()==source&&
            cursor_known&&actual.x==point.x&&actual.y==point.y&&hit==source&&GetAncestor(hit,GA_ROOT)==source&&
            hit_pid==GetCurrentProcessId()&&hit_tid==set.thread&&region_known&&static_cast<int>(region)==HTCLIENT,
            "\"actual_hit\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+",\"actual_PID\":"+std::to_string(hit_pid)+
            ",\"actual_TID\":"+std::to_string(hit_tid)+",\"test_cleanup_only_not_product_NormalUp\":true");
        const auto client_ups=set.frames[0].client_up[0].load();GuestUpHookObservation head;
        s.log.check("queue_failure_original_UP_passive_guest_observer_ready",head.start());button(s,false);
        const auto cleanup=abort_cleanup_up_receipt(s,g.offset,g.generation);const bool head_stopped=head.stop();const auto up=head.json();
        const bool unique_up=head_stopped&&s.left_up_attempted&&s.left_up_send_qpc&&
            e::unique_value(up,"\"observation_final\":")=="true"&&e::unique_value(up,"\"CallNext_returned\":")=="true"&&
            e::number(up,"\"actual_left_UP_callbacks\":")==1&&e::number(up,"\"last_qpc\":").value_or(0)>=s.left_up_send_qpc&&
            e::number(up,"\"last_extra_information\":")==0x50425631&&
            (e::number(up,"\"last_flags\":").value_or(0)&LLMHF_INJECTED);
        s.log.check("queue_failure_unique_original_abort_UP_and_native_client_delivery",unique_up&&
            wait([&]{return set.frames[0].client_up[0].load()==client_ups+1;}),
            "\"actual_guest_UP\":"+up+","+cleanup.json()+
            ",\"client_UP_before\":"+std::to_string(client_ups)+",\"client_UP_after\":"+std::to_string(set.frames[0].client_up[0].load())+
            ",\"old_generation_NormalUp_inferred\":false,\"extra_UP_sent\":false");
        chain_check_product(s,pid,birth);s.log.check("queue_failure_public_pause_requested",launch(s,L"--pause",true));
        const auto paused=geometry_history_status(s);
        s.log.check("queue_failure_public_pause_actual",e::unique_value(paused,"\"enabled\":")=="false",
            "\"actual_status\":"+quote(paused));
        s.log.check("queue_failure_public_resume_rebuild_requested",launch(s,L"--command Pause",true));
        const auto recovered=geometry_history_status(s);
        s.log.check("queue_failure_public_resume_actual_input_owner_healthy",e::unique_value(recovered,"\"enabled\":")=="true"&&
            e::unique_value(recovered,"\"hook_owner_running\":")=="true","\"actual_status\":"+quote(recovered));
        chain_check_product(s,pid,birth);const auto next=layout_down(set,0,HTCAPTION,false);
        s.log.check("queue_failure_recovery_fresh_Move_generation",next.generation>g.generation);
        const Rect moved=translated(held,35,25);layout_phase(set,0,next,35,25,{moved},"queue_failure_recovery_actual_Move_exact");
        const auto repeated=read_product(s).size();
        const auto second_command=L"--guest-queue-failure "+s.run+L" "+std::to_wstring(next.generation)+L" "+
            std::to_wstring(reinterpret_cast<std::uintptr_t>(source));
        const bool second_request=launch(s,second_command,true);std::string denied;
        const bool rejected=wait([&]{denied=record(s,repeated,"guest_queue_failure_arm",next.generation);return
            geometry_history_receipt(s,denied,"guest_queue_failure_arm")&&e::unique_value(denied,"\"armed\":")=="false"&&
            e::quoted(denied,"\"reason\":","one_process_lifetime_permit_already_used_or_armed")&&
            e::unique_value(denied,"\"permit_consumed\":")=="true"&&e::number(denied,"\"fault_generation\":")==g.generation;});
        s.log.check("queue_failure_one_process_lifetime_permit_not_rearmed_after_recovery",second_request&&rejected,
            "\"actual_rejection\":"+quote(denied)+",\"repeated_fault_not_triggered\":true");
        s.log.check("queue_failure_actual_public_recovery_new_input_owner_start",initial_serial&&
            e::number(denied,"\"actual_input_owner_start_serial\":")==*initial_serial+1&&
            e::number(denied,"\"current_input_owner_tid\":").value_or(0)>0&&
            e::unique_value(denied,"\"current_healthy\":")=="true"&&e::unique_value(denied,"\"current_available\":")=="true"&&
            e::number(denied,"\"current_authorized_generation\":")==next.generation,
            "\"before_arm\":"+quote(armed)+",\"after_recovery_active_arm_rejection\":"+quote(denied)+
            ",\"OS_thread_ID_reuse_not_forced_or_rejected\":true,\"new_hook_initialization_not_alone_function_PASS\":true");
        layout_up(set,next,{moved});const auto resize=layout_down(set,0,HTBOTTOM,false);auto resized=moved;resized.b+=24;
        s.log.check("queue_failure_recovery_fresh_Resize_generation",resize.generation>next.generation);
        layout_phase(set,0,resize,0,24,{resized},"queue_failure_recovery_actual_Resize_exact");layout_up(set,resize,{resized});
        interruption_idle(s,set,true);chain_check_product(s,pid,birth);
        pause_routing_client_pair(set,"queue_failure_recovered_native_client_DOWN_UP");
        chain_gui_save(s,original);s.log.check("queue_failure_original_product_bytes_unchanged",hash(s.product)==binary);
        s.log.check("queue_failure_exact_owned_fixture_closed",set.close());chain_check_product(s,pid,birth);
        s.log.emit("queue_failure_component","\"result\":\"PASS_SCOPED_INJECTED_PRODUCTION_FAILURE_AND_PUBLIC_RECOVERY\","
            "\"same_product_instance\":true,\"fresh_Move_Resize_actual_UP\":true,\"actual_capacity_exhaustion\":\"NOT_RUN\","
            "\"OS_PostMessage_failure\":\"NOT_RUN\",\"OS_silent_hook_removal\":\"NOT_RUN\",\"human_UAT\":\"NOT_RUN\",\"whole_version_PASS\":false");
    }catch(...){pause_routing_failed_case_input(s);try{if(!s.left&&!s.control)chain_gui_save(s,original);}catch(...){}
        try{if(!set.closed)set.close();}catch(...){}throw;}
}
