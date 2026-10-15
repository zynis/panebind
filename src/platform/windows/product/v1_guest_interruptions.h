#pragma once

// Guest-only I08. Include after the existing layout/product-chain helpers.
// The test uses real keyboard/mouse input, native foreground and destruction;
// it never calls a product abort handler or manufactures a terminal event.
enum class GuestInterruption { Escape, Foreground, EmergencyHotkey, SourceDestroyed };
std::string interruption_end(State& s,const Gesture& g,panebind::core::v1::End expected,bool destroyed,HWND old_source){
    namespace v=panebind::core::v1;std::string ended;
    const bool observed=wait([&]{
        ended=record(s,g.offset,"gesture_end",g.generation);
        if(ended.empty()||ended.find("\"normal_up\":false")==std::string::npos||ended.find("\"pending_retired\":true")==std::string::npos)return false;
        const auto reason=number(ended,"\"end\":");
        // Destroying a foreground window can deliver either destroy or the
        // consequent foreground event first. Both are real invalidations; no
        // unrelated Paused/Deadline/NormalUp result is accepted as a close.
        return reason==static_cast<std::uint64_t>(expected)||(destroyed&&!IsWindow(old_source)&&
            GetForegroundWindow()!=old_source&&reason==static_cast<std::uint64_t>(v::End::FocusLost));
    },1500);
    s.log.check("I08_actual_non_NormalUp_termination",observed,"\"generation\":"+std::to_string(g.generation)+
        ",\"expected_primary_end\":"+std::to_string(static_cast<unsigned>(expected))+
        ",\"actual_source_destroyed\":"+(destroyed?"true":"false")+",\"production_end\":"+quote(ended));return ended;
}
void interruption_audit_calls(State& s,const Gesture& g,std::uint64_t trigger_qpc,std::optional<Rect> escape_restore){
    const auto text=read(s.product_log);if(g.offset>=text.size()||!trigger_qpc)throw std::runtime_error("I08_native_audit_evidence_missing");
    unsigned forbidden{},restorations{},unknown_clock{};std::istringstream rows(text.substr(g.offset));std::string row;
    while(std::getline(rows,row))if(row.ends_with('}')&&row.find("\"type\":\"placement\"")!=std::string::npos&&
        row.find("\"native_callsite_claimed\":true")!=std::string::npos){
        const auto before=number(row,"\"native_before_qpc\":");if(!before){++unknown_clock;continue;}
        if(before<trigger_qpc)continue;
        const bool allowed_restore=escape_restore&&number(row,"\"generation\":")==0&&
            row.find("\"intent_reason\":\"escape_restore\"")!=std::string::npos&&
            row.find("\"native_attempted\":true")!=std::string::npos&&row.find("\"exact\":true")!=std::string::npos&&
            row.find("\"actual\":"+rect(*escape_restore))!=std::string::npos;
        if(allowed_restore)++restorations;else ++forbidden;
    }
    s.log.check("I08_no_new_drag_native_call_after_termination_trigger",!forbidden&&!unknown_clock&&restorations==(escape_restore?1u:0u),
        "\"generation\":"+std::to_string(g.generation)+",\"trigger_before_qpc\":"+std::to_string(trigger_qpc)+
        ",\"forbidden_new_calls\":"+std::to_string(forbidden)+",\"unknown_call_clocks\":"+std::to_string(unknown_clock)+
        ",\"explicit_exact_Escape_restoration_calls\":"+std::to_string(restorations)+
        ",\"Escape_cleanup_is_not_renewed_drag_authority\":true");
}
void interruption_idle(State& s,LayoutFixtures& set,bool enabled){
    const auto mark=read(s.product_log).size();std::string status;
    const bool observed=launch(s,L"--status",true)&&wait([&]{status=record(s,mark,"status");return
        status.find("\"gesture_active\":false")!=std::string::npos&&status.find(enabled?"\"enabled\":true":"\"enabled\":false")!=std::string::npos&&
        status.find("\"hook_owner_running\":true")!=std::string::npos;});
    GUITHREADINFO info{sizeof(info)};
    s.log.check("I08_actual_idle_input_and_resource_facts",observed&&!s.left&&!s.control&&
        !(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&!(GetAsyncKeyState(VK_CONTROL)&0x8000)&&
        GetGUIThreadInfo(set.thread,&info)&&!info.hwndCapture&&!info.hwndMoveSize&&!info.hwndMenuOwner,
        "\"production_status\":"+quote(status)+",\"actual_source_thread_capture\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(info.hwndCapture))+
        ",\"resident_hook_expected_until_final_product_exit\":true,\"full_shutdown_not_inferred\":true");
    chain_preview_hidden(s,"I08_actual_gesture_overlay_hidden");
}
void interruption_recovery(State& s,LayoutFixtures& set,std::uint64_t retired){
    std::vector<Rect> actual;
    for(std::size_t n=0;n<set.active;++n){const auto r=visible(set.frames[n].hwnd);
        if(!set.owns(n)||!r)throw std::runtime_error("I08_recovery_actual_member_identity_unknown");actual.push_back(*r);}
    auto next=layout_down(set,0,HTCAPTION,false);
    s.log.check("I08_recovery_new_generation",next.generation>retired);
    actual[0]=translated(actual[0],35,25);layout_phase(set,0,next,35,25,actual,"I08_recovery_actual_Move_exact");layout_up(set,next,actual);
    next=layout_down(set,0,HTBOTTOM,false);actual[0].b+=24;
    layout_phase(set,0,next,0,24,actual,"I08_recovery_actual_Resize_exact");layout_up(set,next,actual);
    // Remove the legitimate edge-hover stretch preview with a real released
    // cursor movement, rather than calling a product UI cleanup handler.
    const HWND h=set.frames[0].hwnd;RECT client{};POINT point{};
    if(!GetClientRect(h,&client))throw std::runtime_error("I08_recovery_client_unknown");
    point={(client.left+client.right)/2,(client.top+client.bottom)/2};
    if(!ClientToScreen(h,&point))throw std::runtime_error("I08_recovery_client_screen_unknown");cursor(s,point);
    const auto hit=WindowFromPoint(point);if(!hit||GetAncestor(hit,GA_ROOT)!=h)throw std::runtime_error("I08_recovery_exact_client_hit_not_observed");
    interruption_idle(s,set,true);
}
[[maybe_unused]] void owned_interruption_cases(State& s){
    namespace v=panebind::core::v1;
    const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    if(!original.enabled)throw std::runtime_error("I08_running_enabled_product_required");
    chain_check_product(s,pid,birth);
    auto config=original;config.magnet_move=false;config.magnet_resize=false;config.docking=false;
    config.drag_transparency=false;config.shake=false;chain_gui_save(s,config);
    LayoutFixtures set{s};set.create();const auto work=chain_work(set.frames[0].hwnd);
    if(work.width()<1250||work.height()<700)throw std::runtime_error("I08_actual_guest_work_area_too_small_for_two_declared_sources");
    const std::vector<Rect> initial{{work.l+220,work.t+180,work.l+620,work.t+480},
        {work.l+760,work.t+180,work.l+1160,work.t+480}};
    const LayoutLimit limits{100,100,work.width(),work.height()};
    // One instance of each required cause; this is not a repetition matrix.
    // Close runs last so recovery uses the surviving exact source, not a new
    // HWND whose possible numeric reuse could obscure the invalidation.
    for(const auto cause:std::array{GuestInterruption::Escape,GuestInterruption::Foreground,GuestInterruption::EmergencyHotkey,GuestInterruption::SourceDestroyed}){
        chain_check_product(s,pid,birth);set.setup(initial,{limits,limits});
        const std::size_t source=cause==GuestInterruption::SourceDestroyed?1u:0u;
        const HWND h=set.frames[source].hwnd;const auto g=layout_down(set,source,HTCAPTION,false);
        auto moved=initial;moved[source]=translated(initial[source],-40,35);
        layout_phase(set,source,g,-40,35,moved,"I08_real_active_Move_before_interruption");
        const auto source_key=number(record(s,g.offset,"gesture_start",g.generation),"\"key\":");
        s.log.check("I08_actual_pressed_source_before_interruption",source_key&&set.owns(source)&&s.left&&(GetAsyncKeyState(VK_LBUTTON)&0x8000));
        const auto mark=read(s.product_log).size();LARGE_INTEGER trigger{};
        if(!QueryPerformanceCounter(&trigger)||trigger.QuadPart<=0)throw std::runtime_error("I08_trigger_QPC_unavailable");
        v::End expected=v::End::Escape;bool destroyed=false;HWND focus_target{};
        if(cause==GuestInterruption::Escape){key(s,VK_ESCAPE,true);key(s,VK_ESCAPE,false);}
        else if(cause==GuestInterruption::Foreground){
            expected=v::End::FocusLost;focus_target=set.frames[1].hwnd;
            s.log.check("I08_real_test_owned_foreground_switch",set.owns(1)&&SetForegroundWindow(focus_target)&&wait([&]{return GetForegroundWindow()==focus_target;}));
        }else if(cause==GuestInterruption::EmergencyHotkey){
            expected=v::End::Paused;
            try{key(s,VK_CONTROL,true);key(s,VK_SHIFT,true);key(s,VK_F11,true);key(s,VK_F11,false);key(s,VK_SHIFT,false);key(s,VK_CONTROL,false);}
            catch(...){try{key(s,VK_F11,false);key(s,VK_SHIFT,false);key(s,VK_CONTROL,false);}catch(...){}throw;}
        }else{
            expected=v::End::SourceLost;
            s.log.check("I08_close_only_exact_new_blank_source",set.owns(source)&&set.destroy(source)&&!IsWindow(h),
                "\"native_API\":\"DestroyWindow\",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
                ",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"tid\":"+std::to_string(set.thread)+",\"no_user_document\":true");
            destroyed=true;set.active=1;
        }
        s.log.emit("I08_actual_terminator","\"cause\":"+std::to_string(static_cast<unsigned>(cause))+
            ",\"generation\":"+std::to_string(g.generation)+",\"source_key\":"+std::to_string(source_key)+
            ",\"source_hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+",\"trigger_before_qpc\":"+
            std::to_string(trigger.QuadPart)+",\"physical_left_still_down\":"+((GetAsyncKeyState(VK_LBUTTON)&0x8000)?"true":"false")+
            ",\"product_handler_directly_called\":false");
        const auto ended=interruption_end(s,g,expected,destroyed,h);
        if(cause==GuestInterruption::EmergencyHotkey){
            const auto hotkey=record(s,mark,"emergency_hotkey");
            s.log.check("I08_actual_WM_HOTKEY_while_pressed",hotkey.find("\"native_message\":\"WM_HOTKEY\"")!=std::string::npos&&
                number(hotkey,"\"registration_id\":")==910&&(GetAsyncKeyState(VK_LBUTTON)&0x8000),"\"production_hotkey\":"+quote(hotkey));
        }
        const Rect expected_source=cause==GuestInterruption::Escape?initial[source]:moved[source];
        if(!destroyed)s.log.check("I08_actual_geometry_after_interruption",wait([&]{return set.owns(source)&&visible(h)==expected_source;}),
            "\"expected\":"+rect(expected_source)+",\"end\":"+quote(ended));
        // The WinEvent/keyboard must terminate BEFORE this additional mouse
        // movement; otherwise this test would conceal motion-only recovery.
        cursor(s,{g.anchor.x+70,g.anchor.y+80});
        if(focus_target)s.log.check("I08_product_does_not_retake_foreground",GetForegroundWindow()==focus_target);
        LARGE_INTEGER up_before{};if(!QueryPerformanceCounter(&up_before)||up_before.QuadPart<=0)throw std::runtime_error("I08_actual_UP_clock_unavailable");
        button(s,false);
        s.log.check("I08_matching_test_UP_actually_released",wait([&]{return !(GetAsyncKeyState(VK_LBUTTON)&0x8000);}),
            "\"single_actual_UP\":true,\"send_before_qpc\":"+std::to_string(up_before.QuadPart)+",\"not_NormalUp_evidence\":true");
        // Bounded observation following one actual post-revocation cursor
        // event. Native entry QPC, not JSONL order, decides call timing.
        Sleep(120);interruption_audit_calls(s,g,static_cast<std::uint64_t>(trigger.QuadPart),
            cause==GuestInterruption::Escape?std::optional<Rect>{initial[source]}:std::nullopt);
        if(!destroyed)s.log.check("I08_no_post_revoke_geometry_overwrite",set.owns(source)&&visible(h)==expected_source);
        interruption_idle(s,set,cause!=GuestInterruption::EmergencyHotkey);
        if(focus_target)s.log.check("I08_status_did_not_retake_foreground",GetForegroundWindow()==focus_target);
        if(cause==GuestInterruption::EmergencyHotkey){
            const auto resume=read(s.product_log).size();
            s.log.check("I08_resume_via_existing_product_command",launch(s,L"--command Pause",true)&&wait([&]{const auto status=record(s,resume,"status");return
                status.find("\"enabled\":true")!=std::string::npos&&status.find("\"hook_owner_running\":true")!=std::string::npos;}));
        }
        chain_check_product(s,pid,birth);interruption_recovery(s,set,g.generation);chain_check_product(s,pid,birth);
        s.log.emit("I08_cause_and_following_recovery","\"cause\":"+std::to_string(static_cast<unsigned>(cause))+
            ",\"actual_non_NormalUp_termination\":true,\"new_real_Move_Resize_UP\":true,\"product_restarted\":false");
    }
    chain_gui_save(s,original);s.log.check("I08_exact_fixture_resources_closed",set.close());chain_check_product(s,pid,birth);
    // V1 is a standalone product, not an attached driver-controlled runtime.
    // Killing this fixture also destroys its sources, so that would repeat
    // SourceDestroyed rather than prove a distinct driver-disconnect route.
    s.log.emit("I08_driver_disconnect","\"result\":\"NOT_RUN_CAPABILITY_GAP\",\"reason\":\"no_existing_independent_driver_disconnect_observation_path\",\"source_destruction_not_relabelled\":true");
    s.log.emit("I08_actual_interruption_sequence","\"result\":\"FOUR_CAUSES_AND_RECOVERY_CHECKED_DRIVER_DISCONNECT_NOT_RUN\",\"Escape\":true,\"real_foreground\":true,\"exact_blank_source_close\":true,\"actual_WM_HOTKEY\":true,\"same_product_process\":true,\"full_I08_PASS_inferred\":false,\"human_uat\":\"NOT_RUN\"");
}
