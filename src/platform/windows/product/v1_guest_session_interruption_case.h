#pragma once

// TEST ONLY: guide M04-F04/M02-F05 explicitly permits session-change
// injection. This never locks/unlocks Windows or manufactures button input
// receipts. Include after the existing owner/resource/interruption helpers.
std::wstring session_interruption_desktop(DWORD tid){
    const HDESK handle=tid?GetThreadDesktop(tid):nullptr;std::array<wchar_t,256> name{};DWORD needed{};
    if(!handle||!GetUserObjectInformationW(handle,UOI_NAME,name.data(),static_cast<DWORD>(sizeof(name)),&needed)||
       !needed||needed>sizeof(name)||name.back()!=L'\0')return {};
    return name.data();
}
bool session_interruption_owner(State& s,const TrayGuestOwner& owner,DWORD session,std::wstring_view desktop_name){
    DWORD current_session{},product_session{};
    return tray_guest_owner(s,owner)&&owner.app&&owner.tid&&owner.birth&&session&&
        GetAncestor(owner.app,GA_ROOT)==owner.app&&gui_class(owner.app)==L"PaneBind.V1.Application"&&
        ProcessIdToSessionId(GetCurrentProcessId(),&current_session)&&current_session==session&&
        ProcessIdToSessionId(s.product_pid,&product_session)&&product_session==session&&
        !desktop_name.empty()&&session_interruption_desktop(GetCurrentThreadId())==desktop_name&&
        session_interruption_desktop(owner.tid)==desktop_name;
}
[[maybe_unused]] void owned_session_interruption_case(State& s){
    namespace v=panebind::core::v1;namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;
    const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if(s.suite!="SessionInterruptionOnly"||!original.enabled||!original.rules.empty()||s.left||s.control||
       s.explorer[0]||s.applications[0]||binary.empty()||binary!=hash(s.input_product))
        throw std::runtime_error("session_interruption_exact_enabled_owned_guest_required");
    for(const int code:std::array{VK_LBUTTON,VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN})
        if(GetAsyncKeyState(code)&0x8000)throw std::runtime_error("session_interruption_actual_released_input_required");
    const HWND app=FindWindowW(L"PaneBind.V1.Application",L"PaneBind");DWORD owner_pid{};
    const DWORD owner_tid=app?GetWindowThreadProcessId(app,&owner_pid):0;
    const TrayGuestOwner owner{app,owner_tid,birth};DWORD session{};
    const auto desktop_name=session_interruption_desktop(GetCurrentThreadId());
    if(owner_pid!=pid||!ProcessIdToSessionId(pid,&session)||!session_interruption_owner(s,owner,session,desktop_name))
        throw std::runtime_error("session_interruption_exact_current_product_owner_session_unknown");
    auto wrong=owner;wrong.birth^=1;
    s.log.check("session_interruption_owner_rejects_other_birth_or_session",
        !session_interruption_owner(s,wrong,session,desktop_name)&&
        !session_interruption_owner(s,owner,session^1u,desktop_name),
        "\"TEST_only_read_only_negative\":true,\"message_sent\":false");
    auto config=original;config.magnet_move=false;config.magnet_resize=false;config.glue=false;
    config.docking=false;config.stretch=false;config.smart_stretch=false;config.feedback=false;
    config.drag_transparency=false;config.shake=false;chain_gui_save(s,config);
    LayoutFixtures set{s};set.create();bool lock_attempted{},unlock_attempted{};
    const auto inject=[&](bool locked,std::string_view stage){
        bool& attempted=locked?lock_attempted:unlock_attempted;
        if(attempted||!session_interruption_owner(s,owner,session,desktop_name))
            throw std::runtime_error("session_interruption_exact_owner_or_single_injection_lost");
        attempted=true;LARGE_INTEGER before{},after{};
        if(!QueryPerformanceCounter(&before)||before.QuadPart<=0)throw std::runtime_error("session_interruption_QPC_unknown");
        DWORD_PTR result{};SetLastError(ERROR_SUCCESS);
        const bool sent=SendMessageTimeoutW(owner.app,WM_WTSSESSION_CHANGE,
            locked?WTS_SESSION_LOCK:WTS_SESSION_UNLOCK,static_cast<LPARAM>(session),
            SMTO_ABORTIFHUNG|SMTO_ERRORONEXIT,1000,&result)!=FALSE;
        const DWORD error=sent?ERROR_SUCCESS:GetLastError();const bool after_known=QueryPerformanceCounter(&after)!=FALSE;
        s.log.emit("session_interruption_TEST_notification",
            "\"stage\":"+quote(stage)+",\"owner_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(owner.app))+
            ",\"owner_PID\":"+std::to_string(pid)+",\"owner_TID\":"+std::to_string(owner.tid)+
            ",\"owner_birth\":"+std::to_string(birth)+",\"actual_session_ID\":"+std::to_string(session)+
            ",\"actual_desktop_name\":"+quote(utf8(desktop_name))+",\"message\":\"WM_WTSSESSION_CHANGE\",\"wParam\":"+
            std::to_string(locked?WTS_SESSION_LOCK:WTS_SESSION_UNLOCK)+",\"lParam_session_ID\":"+std::to_string(session)+
            ",\"TEST_injected_notification\":true,\"actual_OS_lock_unlock\":false,\"message_attempt_limit\":1,\"timeout_ms\":1000"+
            ",\"send_before_qpc\":"+std::to_string(before.QuadPart)+",\"send_after_qpc\":"+
            (after_known?std::to_string(after.QuadPart):"null")+",\"SendMessageTimeout_returned_nonzero\":"+(sent?"true":"false")+
            ",\"message_LRESULT_raw\":"+std::to_string(result)+",\"send_error\":"+std::to_string(error)+
            ",\"message_return_is_not_product_semantic_or_cleanup_PASS\":true");
        if(!sent)throw std::runtime_error("session_interruption_TEST_notification_not_completed");
        return static_cast<std::uint64_t>(before.QuadPart);
    };
    try{
        chain_check_product(s,pid,birth);const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<1000||work.height()<650)throw std::runtime_error("session_interruption_actual_work_area_too_small");
        const Rect initial{work.l+220,work.t+180,work.l+620,work.t+480};set.setup({initial});
        const HWND source=set.frames[0].hwnd;const auto g=layout_down(set,0,HTCAPTION,false);
        const auto start=record(s,g.offset,"gesture_start",g.generation);
        s.log.check("session_interruption_actual_plain_source_route",remaining_group_receipt(s,start,"gesture_start")&&
            e::unique_value(start,"\"ctrl\":")=="false"&&e::unique_value(start,"\"resize\":")=="false"&&
            e::number(start,"\"group_members\":")==1&&start.find("\"original\":"+rect(initial)+",")!=start.npos,
            "\"actual_START\":"+quote(start));
        const std::vector<Rect> moved{translated(initial,35,25)};
        layout_phase(set,0,g,35,25,moved,"session_interruption_real_held_Move_exact_before_TEST_lock");
        const auto ready=transparency_resource(s,g,"capture_ready");
        const HWND overlay=reinterpret_cast<HWND>(static_cast<std::uintptr_t>(number(ready,"\"overlay\":")));
        DWORD overlay_pid{};const DWORD overlay_tid=overlay?GetWindowThreadProcessId(overlay,&overlay_pid):0;
        GUITHREADINFO source_gui{sizeof(source_gui)},resource_gui{sizeof(resource_gui)};
        const bool held=session_interruption_owner(s,owner,session,desktop_name)&&set.owns(0)&&
            GetForegroundWindow()==source&&s.left&&!s.left_up_attempted&&(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&
            actual_up(s,g.offset,g.generation).empty()&&overlay&&IsWindow(overlay)&&overlay_pid==pid&&overlay_tid&&
            number(ready,"\"source\":")==reinterpret_cast<std::uintptr_t>(source)&&
            number(ready,"\"source_pid\":")==GetCurrentProcessId()&&number(ready,"\"source_tid\":")==set.thread&&
            number(ready,"\"capture_owner_tid\":")==overlay_tid&&!record(s,g.offset,"native_move_end",g.generation).empty()&&
            GetGUIThreadInfo(set.thread,&source_gui)&&source_gui.hwndCapture==overlay&&!source_gui.hwndMoveSize&&!source_gui.hwndMenuOwner&&
            GetGUIThreadInfo(overlay_tid,&resource_gui)&&resource_gui.hwndCapture==overlay&&!resource_gui.hwndMoveSize&&!resource_gui.hwndMenuOwner;
        s.log.check("session_interruption_exact_active_source_capture_and_product_owner",held,
            "\"generation\":"+std::to_string(g.generation)+",\"actual_capture_ready\":"+quote(ready)+
            ",\"test_UP_not_yet_sent\":true,\"actual_OS_lock_inferred\":false");
        const auto trigger=inject(true,"held_Move_TEST_LOCK");
        const auto ended=interruption_end(s,g,v::End::SessionLost,false,source);
        std::string released,detached,gone;
        const bool stopped=wait([&]{
            released=transparency_resource(s,g,"capture_released");detached=transparency_resource(s,g,"thread_detached");
            gone=transparency_resource(s,g,"isolation_gone");
            return e::unique_value(released,"\"capture_owner_observation_known\":")=="true"&&
                e::number(released,"\"capture_release_before_HWND\":")==reinterpret_cast<std::uintptr_t>(overlay)&&
                e::unique_value(released,"\"release_attempted\":")=="true"&&e::unique_value(released,"\"release_succeeded\":")=="true"&&
                e::number(released,"\"capture_after_release\":")==0&&
                e::unique_value(detached,"\"detach_attempted\":")=="true"&&
                e::unique_value(detached,"\"detach_completed\":")=="true"&&e::unique_value(detached,"\"detach_succeeded\":")=="true"&&
                e::unique_value(gone,"\"overlay_destroyed\":")=="true"&&e::unique_value(gone,"\"hotkey_unregistered\":")=="true"&&
                e::unique_value(gone,"\"release_succeeded\":")=="true"&&e::unique_value(gone,"\"detach_succeeded\":")=="true"&&
                e::number(gone,"\"removal_reason\":").value_or(0)!=0;
        },2000);
        GUITHREADINFO after{sizeof(after)};
        s.log.check("session_interruption_real_End5_nonNormal_Gone_before_cleanup_UP",stopped&&
            remaining_group_receipt(s,ended,"gesture_end")&&
            e::number(ended,"\"end\":")==static_cast<unsigned>(v::End::SessionLost)&&
            e::unique_value(ended,"\"normal_up\":")=="false"&&e::unique_value(ended,"\"pending_retired\":")=="true"&&
            !IsWindow(overlay)&&set.owns(0)&&visible(source)==moved[0]&&GetForegroundWindow()==source&&
            s.left&&!s.left_up_attempted&&actual_up(s,g.offset,g.generation).empty()&&
            GetGUIThreadInfo(set.thread,&after)&&!after.hwndCapture&&!after.hwndMoveSize&&!after.hwndMenuOwner,
            "\"generation\":"+std::to_string(g.generation)+",\"actual_end\":"+quote(ended)+
            ",\"actual_capture_release\":"+quote(released)+",\"actual_detach\":"+quote(detached)+",\"actual_Gone\":"+quote(gone)+
            ",\"resource_wait_limit_ms\":2000,\"TEST_cleanup_UP_not_yet_sent\":true,\"guest_destruction_used\":false");
        interruption_audit_calls(s,g,trigger,std::nullopt);
        // Route the one original TEST cleanup UP to the already-foreground
        // exact owned client after real abort/Gone. Do not reclaim focus,
        // capture or old-generation authority; this is not old NormalUp.
        RECT client{};if(!GetClientRect(source,&client))throw std::runtime_error("session_interruption_cleanup_client_unknown");
        POINT destination{(client.left+client.right)/2,(client.top+client.bottom)/2};
        if(!ClientToScreen(source,&destination))throw std::runtime_error("session_interruption_cleanup_point_unknown");
        cursor(s,destination);const HWND hit=WindowFromPoint(destination);DWORD_PTR region{};
        const bool hit_known=hit==source&&SendMessageTimeoutW(source,WM_NCHITTEST,0,
            MAKELPARAM(static_cast<short>(destination.x),static_cast<short>(destination.y)),
            SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&region)!=FALSE;
        s.log.check("session_interruption_unique_cleanup_UP_exact_owned_client",session_interruption_owner(s,owner,session,desktop_name)&&
            set.owns(0)&&GetForegroundWindow()==source&&hit==source&&GetAncestor(hit,GA_ROOT)==source&&hit_known&&
            static_cast<int>(region)==HTCLIENT&&visible(source)==moved[0]&&s.left&&!s.left_up_attempted,
            "\"actual_hit\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+
            ",\"actual_hit_test\":"+std::to_string(region)+",\"TEST_foreground_or_capture_change\":false");
        const auto client_ups=set.frames[0].up.load();GuestUpHookObservation observer;
        s.log.check("session_interruption_unique_cleanup_UP_observer_ready",observer.start());
        button(s,false);const auto cleanup=abort_cleanup_up_receipt(s,g.offset,g.generation);
        const bool client_up=wait([&]{return set.owns(0)&&set.frames[0].up.load()==client_ups+1;},1000);
        const bool observer_stopped=observer.stop();const auto head=observer.json();
        const bool unique_up=observer_stopped&&s.left_up_attempted&&s.left_up_send_qpc&&
            e::unique_value(head,"\"snapshot_coherent\":")=="true"&&e::unique_value(head,"\"observation_final\":")=="true"&&
            e::unique_value(head,"\"installed\":")=="true"&&e::unique_value(head,"\"CallNext_returned\":")=="true"&&
            e::number(head,"\"actual_left_UP_callbacks\":")==1&&e::number(head,"\"last_qpc\":").value_or(0)>=s.left_up_send_qpc&&
            e::number(head,"\"last_extra_information\":")==0x50425631&&
            (e::number(head,"\"last_flags\":").value_or(0)&LLMHF_INJECTED);
        s.log.check("session_interruption_single_real_TEST_abort_UP_observed",unique_up&&client_up&&!s.left&&!s.control,
            "\"old_generation\":"+std::to_string(g.generation)+",\"single_UP_send_before_qpc\":"+std::to_string(s.left_up_send_qpc)+
            ",\"actual_guest_hook\":"+head+",\"actual_owned_client_UP_delta\":"+std::to_string(set.frames[0].up.load()-client_ups)+
            ","+cleanup.json()+",\"guest_hook_and_client_UP_are_not_old_generation_product_NormalUp\":true,\"extra_UP_sent\":false");
        interruption_audit_calls(s,g,trigger,std::nullopt);interruption_idle(s,set,true);
        inject(false,"released_TEST_UNLOCK_before_fresh_gestures");
        chain_check_product(s,pid,birth);interruption_recovery(s,set,g.generation);chain_check_product(s,pid,birth);
        unsigned late{},unknown{};std::istringstream rows(product_log_since(s,g.offset));std::string row;
        while(std::getline(rows,row))if(remaining_group_receipt(s,row,"placement")&&e::number(row,"\"generation\":")==g.generation&&
            e::unique_value(row,"\"native_callsite_claimed\":")=="true"){
            const auto qpc=e::number(row,"\"native_before_qpc\":");if(!qpc||!*qpc)++unknown;else if(*qpc>=trigger)++late;
        }
        s.log.check("session_interruption_old_generation_zero_new_native_calls_after_recovery",!late&&!unknown&&
            session_interruption_owner(s,owner,session,desktop_name)&&hash(s.product)==binary,
            "\"retired_generation\":"+std::to_string(g.generation)+",\"late_old_native_calls\":"+std::to_string(late)+
            ",\"unknown_native_clocks\":"+std::to_string(unknown)+",\"inflight_call_cancelled_claimed\":false");
        const auto saved=chain_gui_save(s,original);auto expected=original;expected.generation=saved.generation;
        s.log.check("session_interruption_original_configuration_restored",saved==expected);
        const bool closed=set.close();s.log.check("session_interruption_exact_owned_roots_closed",closed);
        s.log.emit("session_interruption_component","\"result\":\"PASS\",\"TEST_session_message_injection\":true,"+
            std::string("\"actual_OS_lock_unlock\":\"NOT_RUN\",\"same_product_PID_Move_Resize_recovery\":true,\"old_generation_NormalUp\":false,\"human_UAT\":\"NOT_RUN\""));
    }catch(...){
        if(s.left&&!s.left_up_attempted)try{button(s,false);}catch(...){s.log.failed=true;}
        if(lock_attempted&&!unlock_attempted&&session_interruption_owner(s,owner,session,desktop_name))
            try{inject(false,"failure_cleanup_TEST_UNLOCK_not_PASS");}catch(...){s.log.failed=true;}
        if(!s.left&&!s.control)try{chain_gui_save(s,original);}catch(...){s.log.failed=true;}
        throw;
    }
}
