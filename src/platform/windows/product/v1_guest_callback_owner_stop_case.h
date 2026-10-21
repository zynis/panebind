#pragma once

// TEST ONLY. Explicitly stop the verified product input-owner thread; this
// does not simulate or prove Windows silently removing a low-level hook.
void owned_callback_owner_stop_case(State& s){
    namespace v=panebind::core::v1;namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;
    const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if(s.suite!="CallbackOwnerStopOnly"||!original.enabled||!original.rules.empty()||s.left||s.control||
       s.explorer[0]||s.applications[0]||binary.empty()||binary!=hash(s.input_product))
        throw std::runtime_error("callback_owner_stop_exact_enabled_Medium_owned_guest_required");
    std::array<wchar_t,32768> image{};const auto image_size=GetModuleFileNameW(nullptr,image.data(),static_cast<DWORD>(image.size()));
    if(!image_size||image_size>=image.size())throw std::runtime_error("callback_owner_stop_driver_image_unknown");
    const auto driver_hash=hash(image.data());if(driver_hash.empty())throw std::runtime_error("callback_owner_stop_driver_hash_unknown");
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
    const auto event_unhooks=[](const std::string& row){
        const auto start=row.find("\"event_unhook\":[");if(start==row.npos)return false;
        const auto end=row.find(']',start);if(end==row.npos)return false;
        const auto events=row.substr(start,end-start);std::size_t at{};
        for(unsigned index=0;index<6;++index){const auto first=events.find('{',at);if(first==events.npos)return false;
            const auto last=events.find('}',first);if(last==events.npos)return false;const auto actual=events.substr(first,last-first+1);
            if(e::number(actual,"\"index\":")!=index||e::unique_value(actual,"\"attempted\":")!="true"||
               e::unique_value(actual,"\"native_return\":")!="true"||e::number(actual,"\"error\":")!=0)return false;
            at=last+1;
        }return events.find('{',at)==events.npos;
    };
    try{
        const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<1000||work.height()<700)throw std::runtime_error("callback_owner_stop_actual_work_area_too_small");
        const Rect before{work.l+220,work.t+180,work.l+620,work.t+500};set.setup({before});
        const HWND source=set.frames[0].hwnd;const auto g=layout_down(set,0,HTCAPTION,false);
        const Rect held=translated(before,21,14);layout_phase(set,0,g,21,14,{held},"callback_owner_stop_actual_exact_Move_before_request");
        const auto first=placement(s,g.offset,g.generation,held,true);const auto start=record(s,g.offset,"gesture_start",g.generation);
        s.log.check("callback_owner_stop_actual_plain_active_exact_source_before_request",set.owns(0)&&s.left&&!s.left_up_attempted&&
            actual_up(s,g.offset,g.generation).empty()&&visible(source)==held&&
            e::unique_value(start,"\"ctrl\":")=="false"&&e::unique_value(start,"\"resize\":")=="false"&&
            e::unique_value(first,"\"exact\":")=="true"&&e::unique_value(first,"\"native_attempted\":")=="true",
            "\"actual_START\":"+quote(start)+",\"actual_first_native_receipt\":"+quote(first)+",\"stop_not_yet_requested\":true");
        const auto mark=read_product(s).size();
        const auto command=L"--guest-callback-owner-stop "+s.run+L" "+std::to_wstring(g.generation)+L" "+
            std::to_wstring(reinterpret_cast<std::uintptr_t>(source));
        const bool sent=launch(s,command,true);std::string armed;
        const bool accepted=wait([&]{armed=record(s,mark,"guest_callback_owner_stop_arm",g.generation);return
            geometry_history_receipt(s,armed,"guest_callback_owner_stop_arm")&&e::unique_value(armed,"\"requested\":")=="true"&&
            e::unique_value(armed,"\"stop_request_posted\":")=="true"&&
            e::quoted(armed,"\"reason\":","exact_guest_callback_owner_stop_posted")&&
            e::quoted(armed,"\"fault_run_id\":",utf8(s.run))&&e::number(armed,"\"fault_generation\":")==g.generation&&
            e::number(armed,"\"fault_source_HWND\":")==reinterpret_cast<std::uintptr_t>(source)&&
            e::number(armed,"\"fault_source_key\":")==e::number(first,"\"key\":")&&
            e::number(armed,"\"fault_source_pid\":")==GetCurrentProcessId()&&
            e::number(armed,"\"fault_source_tid\":")==set.thread&&
            e::number(armed,"\"fault_source_birth\":")==process_birth(GetCurrentProcess())&&
            e::quoted(armed,"\"fault_driver_sha256\":",driver_hash);});
        const auto initial_serial=e::number(armed,"\"actual_input_owner_start_serial\":");
        s.log.check("callback_owner_stop_actual_request_posted_not_thread_exit_proof",sent&&accepted&&initial_serial&&*initial_serial>0,
            "\"actual_arm\":"+quote(armed)+",\"owner_may_already_have_exited_at_GUI_log_time\":true");
        std::string trigger;
        const bool triggered=wait([&]{trigger=record(s,mark,"input_callback_owner_stop",g.generation);return
            geometry_history_receipt(s,trigger,"input_callback_owner_stop")&&e::quoted(trigger,"\"origin\":","guest_injected_callback_owner_stop")&&
            e::unique_value(trigger,"\"owner_stop_triggered\":")=="true"&&
            e::unique_value(trigger,"\"callback_stop_permit_consumed\":")=="true"&&
            e::number(trigger,"\"callback_stop_trigger_qpc\":").value_or(0)>0;});
        const auto trigger_qpc=e::number(trigger,"\"callback_stop_trigger_qpc\":").value_or(0);
        s.log.check("callback_owner_stop_actual_owner_trigger_revoked_authority",triggered&&
            e::number(trigger,"\"fault_generation\":")==g.generation&&
            e::number(trigger,"\"fault_source_HWND\":")==reinterpret_cast<std::uintptr_t>(source)&&
            e::unique_value(trigger,"\"current_healthy\":")=="false"&&e::unique_value(trigger,"\"current_available\":")=="false"&&
            e::number(trigger,"\"current_authorized_generation\":")==0&&e::number(trigger,"\"current_completion_generation\":")==0,
            "\"actual_trigger\":"+quote(trigger)+",\"OS_silent_hook_removal_inferred\":false");
        std::string exited;
        const bool owner_exited=wait([&]{exited=record(s,mark,"input_callback_owner_exit",g.generation);return
            geometry_history_receipt(s,exited,"input_callback_owner_exit")&&
            e::unique_value(exited,"\"thread_object_signaled\":")=="true"&&
            e::unique_value(exited,"\"thread_exit_observed\":")=="true";},2000);
        const auto exit_code=e::number(exited,"\"thread_exit_code\":");
        const auto unhook_before=e::number(exited,"\"mouse_unhook_before_qpc\":");
        const auto unhook_after=e::number(exited,"\"mouse_unhook_after_qpc\":");
        s.log.check("callback_owner_stop_actual_signaled_HANDLE_and_native_API_teardown",owner_exited&&
            e::unique_value(exited,"\"actual_owner_exit_recorded\":")=="true"&&
            e::unique_value(exited,"\"thread_wait_registered\":")=="true"&&e::number(exited,"\"thread_wait_result\":")==WAIT_OBJECT_0&&
            exit_code&&*exit_code!=STILL_ACTIVE&&e::number(exited,"\"actual_stopped_owner_tid\":").value_or(0)>0&&
            e::number(exited,"\"mouse_hook_handle\":").value_or(0)>0&&
            e::unique_value(exited,"\"mouse_unhook_attempted\":")=="true"&&e::unique_value(exited,"\"mouse_unhook_return\":")=="true"&&
            e::number(exited,"\"mouse_unhook_error\":")==0&&unhook_before&&*unhook_before>=trigger_qpc&&
            unhook_after&&*unhook_after>=*unhook_before&&e::number(exited,"\"owner_exit_record_qpc\":").value_or(0)>=*unhook_after&&
            e::unique_value(exited,"\"hotkey_unregister_return\":")=="true"&&
            e::unique_value(exited,"\"raw_unregister_return\":")=="true"&&
            e::unique_value(exited,"\"receiver_destroy_return\":")=="true"&&
            e::unique_value(exited,"\"shield_stop_clean\":")=="true"&&event_unhooks(exited)&&
            e::unique_value(exited,"\"callback_stop_cleanup_confirmed\":")=="true",
            "\"actual_owner_exit\":"+quote(exited)+",\"healthy_false_not_used_as_native_unhook_or_exit_proof\":true");
        const auto ended=interruption_end(s,g,v::End::InputUnavailable,false,source);std::string gone;
        const bool resources_gone=wait([&]{gone=gone_for(g);return clean_gone(gone);},1500);
        s.log.check("callback_owner_stop_actual_independent_shield_gone_before_original_UP",resources_gone&&
            set.owns(0)&&GetForegroundWindow()==source&&visible(source)==held&&s.left&&!s.left_up_attempted&&
            actual_up(s,g.offset,g.generation).empty(),"\"actual_end\":"+quote(ended)+",\"actual_gone\":"+quote(gone));
        interruption_audit_calls(s,g,trigger_qpc,std::nullopt);
        const auto unhealthy=queue_failure_status(s,true,false,e::number(exited,"\"sequence\":").value_or(0));
        chain_check_product(s,pid,birth);
        // The stopped owner cannot supply a later product LL callback. Observe
        // the original single TEST UP separately, including actual client delivery.
        GUITHREADINFO gui{sizeof(gui)};RECT client{};
        if(!set.owns(0)||GetForegroundWindow()!=source||!GetGUIThreadInfo(set.thread,&gui)||gui.hwndCapture||gui.hwndMoveSize||
           !GetClientRect(source,&client))throw std::runtime_error("callback_owner_stop_cleanup_exact_idle_source_unknown");
        POINT point{(client.left+client.right)/2,(client.top+client.bottom)/2};
        if(!ClientToScreen(source,&point))throw std::runtime_error("callback_owner_stop_cleanup_client_point_unknown");cursor(s,point);
        POINT actual{};DWORD hit_pid{};const bool cursor_known=GetCursorPos(&actual)!=FALSE;
        const HWND hit=cursor_known?WindowFromPoint(actual):nullptr;const DWORD hit_tid=hit?GetWindowThreadProcessId(hit,&hit_pid):0;
        DWORD_PTR region{};const bool region_known=hit==source&&SendMessageTimeoutW(source,WM_NCHITTEST,0,
            MAKELPARAM(static_cast<short>(actual.x),static_cast<short>(actual.y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&region)!=FALSE;
        s.log.check("callback_owner_stop_original_UP_exact_TEST_client_destination",set.owns(0)&&GetForegroundWindow()==source&&
            cursor_known&&actual.x==point.x&&actual.y==point.y&&hit==source&&GetAncestor(hit,GA_ROOT)==source&&
            hit_pid==GetCurrentProcessId()&&hit_tid==set.thread&&region_known&&static_cast<int>(region)==HTCLIENT,
            "\"actual_hit\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+",\"actual_PID\":"+std::to_string(hit_pid)+
            ",\"actual_TID\":"+std::to_string(hit_tid)+",\"test_cleanup_only_not_product_NormalUp\":true");
        const auto client_ups=set.frames[0].client_up[0].load();GuestUpHookObservation head;
        s.log.check("callback_owner_stop_original_UP_passive_guest_observer_ready",head.start());button(s,false);
        const auto cleanup=abort_cleanup_up_receipt(s,g.offset,g.generation);const bool head_stopped=head.stop();const auto up=head.json();
        const bool unique_up=head_stopped&&s.left_up_attempted&&s.left_up_send_qpc&&
            e::unique_value(up,"\"observation_final\":")=="true"&&e::unique_value(up,"\"CallNext_returned\":")=="true"&&
            e::number(up,"\"actual_left_UP_callbacks\":")==1&&e::number(up,"\"last_qpc\":").value_or(0)>=s.left_up_send_qpc&&
            e::number(up,"\"last_extra_information\":")==0x50425631&&
            (e::number(up,"\"last_flags\":").value_or(0)&LLMHF_INJECTED);
        const bool client_up=wait([&]{return set.frames[0].client_up[0].load()==client_ups+1;});
        s.log.check("callback_owner_stop_unique_original_abort_UP_and_native_client_delivery",unique_up&&client_up,
            "\"actual_guest_UP\":"+up+","+cleanup.json()+
            ",\"client_UP_before\":"+std::to_string(client_ups)+",\"client_UP_after\":"+std::to_string(set.frames[0].client_up[0].load())+
            ",\"product_LL_UP_required_after_actual_owner_exit\":false,\"old_generation_NormalUp_inferred\":false,\"extra_UP_sent\":false");
        interruption_audit_calls(s,g,trigger_qpc,std::nullopt);
        s.log.check("callback_owner_stop_public_pause_requested",launch(s,L"--pause",true));
        const auto paused=queue_failure_status(s,false,false,e::number(unhealthy,"\"sequence\":").value_or(0));
        s.log.check("callback_owner_stop_public_resume_rebuild_requested",launch(s,L"--command Pause",true));
        const auto recovered=queue_failure_status(s,true,true,e::number(paused,"\"sequence\":").value_or(0));
        s.log.check("callback_owner_stop_public_resume_actual_input_owner_healthy",e::unique_value(recovered,"\"hook_owner_running\":")=="true",
            "\"actual_recovered_status\":"+quote(recovered));
        chain_check_product(s,pid,birth);const auto next=layout_down(set,0,HTCAPTION,false);
        s.log.check("callback_owner_stop_recovery_fresh_Move_generation",next.generation>g.generation);
        const Rect moved=translated(held,35,25);layout_phase(set,0,next,35,25,{moved},"callback_owner_stop_recovery_actual_Move_exact");
        const auto repeated=read_product(s).size();
        const auto second_command=L"--guest-callback-owner-stop "+s.run+L" "+std::to_wstring(next.generation)+L" "+
            std::to_wstring(reinterpret_cast<std::uintptr_t>(source));
        const bool second_request=launch(s,second_command,true);std::string denied;
        const bool rejected=wait([&]{denied=record(s,repeated,"guest_callback_owner_stop_arm",next.generation);return
            geometry_history_receipt(s,denied,"guest_callback_owner_stop_arm")&&e::unique_value(denied,"\"requested\":")=="false"&&
            e::quoted(denied,"\"reason\":","one_process_lifetime_permit_already_used_or_armed")&&
            e::unique_value(denied,"\"callback_stop_permit_consumed\":")=="true"&&e::number(denied,"\"fault_generation\":")==g.generation;});
        s.log.check("callback_owner_stop_one_process_lifetime_permit_not_rearmed",second_request&&rejected,
            "\"actual_second_request_rejection\":"+quote(denied));
        s.log.check("callback_owner_stop_actual_public_recovery_new_owner_start",initial_serial&&
            e::number(denied,"\"actual_input_owner_start_serial\":")==*initial_serial+1&&
            e::number(denied,"\"current_input_owner_tid\":").value_or(0)>0&&
            e::unique_value(denied,"\"current_healthy\":")=="true"&&e::unique_value(denied,"\"current_available\":")=="true"&&
            e::number(denied,"\"current_authorized_generation\":")==next.generation,
            "\"original_arm\":"+quote(armed)+",\"actual_active_recovery\":"+quote(denied)+",\"OS_TID_reuse_not_forced_or_rejected\":true");
        layout_up(set,next,{moved});const auto resize=layout_down(set,0,HTBOTTOM,false);auto resized=moved;resized.b+=24;
        s.log.check("callback_owner_stop_recovery_fresh_Resize_generation",resize.generation>next.generation);
        layout_phase(set,0,resize,0,24,{resized},"callback_owner_stop_recovery_actual_Resize_exact");layout_up(set,resize,{resized});
        interruption_idle(s,set,true);chain_check_product(s,pid,birth);
        pause_routing_client_pair(set,"callback_owner_stop_recovered_native_client_DOWN_UP");
        unsigned old_late{},old_unknown{};std::string row;std::istringstream rows(product_log_since(s,g.offset));
        while(std::getline(rows,row))if(geometry_history_receipt(s,row,"placement")&&
            e::number(row,"\"generation\":")==g.generation&&e::unique_value(row,"\"native_callsite_claimed\":")=="true"){
            const auto qpc=e::number(row,"\"native_before_qpc\":");if(!qpc||!*qpc)++old_unknown;
            else if(*qpc>=trigger_qpc)++old_late;
        }
        s.log.check("callback_owner_stop_retired_generation_still_zero_late_after_recovery",!old_late&&!old_unknown,
            "\"retired_generation\":"+std::to_string(g.generation)+",\"trigger_qpc\":"+std::to_string(trigger_qpc)+
            ",\"old_generation_late_native_calls\":"+std::to_string(old_late)+",\"unknown_call_clocks\":"+std::to_string(old_unknown)+
            ",\"valid_successor_calls_not_counted_as_old_writes\":true");
        chain_gui_save(s,original);s.log.check("callback_owner_stop_original_product_bytes_unchanged",hash(s.product)==binary);
        s.log.check("callback_owner_stop_exact_owned_fixture_closed",set.close());chain_check_product(s,pid,birth);
        s.log.emit("callback_owner_stop_component","\"result\":\"PASS_SCOPED_EXPLICIT_INPUT_OWNER_EXIT_AND_PUBLIC_RECOVERY\","
            "\"same_product_instance\":true,\"actual_thread_HANDLE_and_native_teardown_required\":true,"
            "\"fresh_Move_Resize_actual_UP\":true,\"OS_silent_hook_removal\":\"NOT_RUN\",\"human_UAT\":\"NOT_RUN\",\"whole_version_PASS\":false");
    }catch(...){pause_routing_failed_case_input(s);try{if(!s.left&&!s.control)chain_gui_save(s,original);}catch(...){}
        try{if(!set.closed)set.close();}catch(...){}throw;}
}
