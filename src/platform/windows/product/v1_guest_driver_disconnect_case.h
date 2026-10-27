#pragma once

// Include after v1_guest_input_actor.h, within the existing fixture namespace.
// The child, not the supervisor, sends the interrupted DOWN/MOVE. This is one
// TEST transport-loss observation; PaneBind has no driver protocol.
bool input_actor_disconnect_driver_record(std::string_view row,std::string_view type){
    namespace e=product::guest::up_evidence_detail;
    return row.size()<=16384&&row.ends_with('}')&&row.find_first_of("\r\n")==row.npos&&
        e::quoted(row,"\"schema\":","panebind-v1-owned-driver/1")&&
        e::quoted(row,"\"source_sha\":",PANEBIND_BUILD_SHA)&&e::quoted(row,"\"type\":",type);
}
bool input_actor_disconnect_startup_preflight(State& s,LayoutFixtures& set,DWORD product_pid,
    std::uint64_t product_birth,POINT point,Rect initial,UINT dpi,HMONITOR monitor){
    namespace e=product::guest::up_evidence_detail;
    const HWND source=set.frames[0].hwnd;const auto source_birth=process_birth(GetCurrentProcess());
    const auto began=GetTickCount64();constexpr DWORD maximum_ms=60000;
    LARGE_INTEGER begin_qpc{};RECT native_before{};
    const auto keys_clear=[&]{if(s.left||s.control)return false;
        for(const int key:std::array{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2,VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN})
            if(GetAsyncKeyState(key)&0x8000)return false;return true;};
    const auto source_exact=[&](bool require_foreground){
        DWORD pid{};RECT native{};GUITHREADINFO gui{sizeof(gui)};POINT actual{};
        return s.suite=="DriverDisconnectOnly"&&source_birth&&process_birth(GetCurrentProcess())==source_birth&&
            s.process&&GetProcessId(s.process)==product_pid&&process_birth(s.process)==product_birth&&WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&
            set.owns(0)&&IsWindowVisible(source)&&GetAncestor(source,GA_ROOT)==source&&
            GetWindowThreadProcessId(source,&pid)==set.thread&&pid==GetCurrentProcessId()&&
            visible(source)==initial&&GetWindowRect(source,&native)&&EqualRect(&native_before,&native)&&
            GetDpiForWindow(source)==dpi&&MonitorFromWindow(source,MONITOR_DEFAULTTONULL)==monitor&&
            input_actor_desktop(set.thread,GetCurrentProcessId())&&medium(GetCurrentProcess())&&keys_clear()&&
            GetGUIThreadInfo(set.thread,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner&&
            !(gui.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE))&&
            (!require_foreground||(GetForegroundWindow()==source&&GetCursorPos(&actual)&&actual.x==point.x&&actual.y==point.y&&
                GetAncestor(WindowFromPoint(actual),GA_ROOT)==source));
    };
    bool observer_installed=false,observer_complete=true;std::string external,returned,reason;
    auto cursor_mark=s.log.mark();std::uint64_t external_end_qpc{};DWORD external_stamp{};
    if(!QueryPerformanceCounter(&begin_qpc)||begin_qpc.QuadPart<=0||!GetWindowRect(source,&native_before)||!source_exact(true)){
        reason="initial_exact_source_context_unconfirmed";
    }else{
        drain_foreground_observer();std::istringstream prior(s.log.since(0));std::string row;
        while(std::getline(prior,row)){
            if(input_actor_disconnect_driver_record(row,"guest_foreground_observer_lifecycle")&&
               e::quoted(row,"\"operation\":","install"))observer_installed=e::unique_value(row,"\"native_return\":")=="true";
            if(input_actor_disconnect_driver_record(row,"guest_foreground_observer_buffer")&&
               e::unique_value(row,"\"overflow\":")!="0")observer_complete=false;
        }
        if(!observer_installed||!observer_complete)reason="existing_foreground_observer_unavailable_or_incomplete";
    }
    bool expired=false,invalid=false;
    if(reason.empty()){
        // Existing bounded TEST wait pumps messages and drains this observer.
        // No fixed startup Sleep, extra activation or unknown-window control.
        (void)wait([&]{
            if(GetTickCount64()-began>=maximum_ms){expired=true;return true;}
            if(!set.owns(0)||!keys_clear()){invalid=true;reason="source_or_released_input_changed_before_actor";return true;}
            const auto suffix=s.log.since(cursor_mark);cursor_mark=s.log.mark();std::istringstream rows(suffix);std::string row;
            while(std::getline(rows,row)){
                if(input_actor_disconnect_driver_record(row,"guest_foreground_observer_buffer")&&
                   e::unique_value(row,"\"overflow\":")!="0"){
                    invalid=true;reason="foreground_observer_overflow_or_unknown";return true;
                }
                if(!input_actor_disconnect_driver_record(row,"guest_foreground_observed"))continue;
                const auto hwnd=e::number(row,"\"hwnd\":"),pid=e::number(row,"\"callback_window_pid_before\":");
                const auto tid=e::number(row,"\"callback_window_tid_before\":"),stamp_value=e::number(row,"\"native_event_time_u32\":");
                const auto callback_begin=e::number(row,"\"callback_begin_qpc\":"),callback_end=e::number(row,"\"callback_end_qpc\":");
                const auto event_birth=e::number(row,"\"callback_process_birth_filetime\":");
                if(!stamp_value||*stamp_value>MAXDWORD||!callback_begin||!*callback_begin||!callback_end||*callback_end<*callback_begin){
                    invalid=true;reason="foreground_transition_clock_unknown";return true;
                }
                const auto stamp=static_cast<DWORD>(*stamp_value);
                if(static_cast<DWORD>(stamp-static_cast<DWORD>(began))>=0x80000000u||
                   *callback_begin<static_cast<std::uint64_t>(begin_qpc.QuadPart))continue;
                const bool actual=hwnd&&*hwnd&&pid&&*pid&&*pid<=MAXDWORD&&tid&&*tid&&*tid<=MAXDWORD&&event_birth&&*event_birth&&
                    e::unique_value(row,"\"callback_window_identity_stable\":")=="true"&&
                    e::unique_value(row,"\"callback_process_birth_known\":")=="true"&&
                    e::unique_value(row,"\"class_utf8_known\":")=="true"&&e::unique_value(row,"\"class_may_be_truncated\":")=="false"&&
                    e::number(row,"\"callback_foreground_before\":")==hwnd&&e::number(row,"\"callback_foreground_after\":")==hwnd;
                if(!actual){invalid=true;reason="foreground_transition_identity_or_actual_context_unknown";return true;}
                if(external.empty()&&*pid!=GetCurrentProcessId()&&*pid!=product_pid&&
                   e::unique_value(row,"\"registered_task_PID_birth_match\":")=="false"){
                    external=row;external_end_qpc=*callback_end;external_stamp=stamp;
                }
                else if(!external.empty()&&hwnd==reinterpret_cast<std::uintptr_t>(source)&&pid==GetCurrentProcessId()&&
                    *tid==set.thread&&*event_birth==source_birth&&*callback_begin>external_end_qpc&&
                    static_cast<DWORD>(stamp-external_stamp)<0x80000000u)returned=row;
            }
            return !returned.empty()&&source_exact(true);
        },maximum_ms);
    }
    const bool accepted=reason.empty()&&!expired&&!invalid&&!external.empty()&&!returned.empty()&&source_exact(true)&&GetTickCount64()-began<maximum_ms;
    if(!accepted&&reason.empty())reason=expired?"natural_external_activity_and_exact_source_return_not_observed_within_bound":"fresh_source_context_not_confirmed_after_natural_return";
    s.log.emit("I08_disconnect_passive_startup_foreground_precondition","\"result\":"+quote(accepted?"PASS":"NOT_RUN")+
        ",\"maximum_observation_ms\":"+std::to_string(maximum_ms)+",\"began_tick\":"+std::to_string(began)+
        ",\"ended_tick\":"+std::to_string(GetTickCount64())+",\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+
        ",\"source_PID\":"+std::to_string(GetCurrentProcessId())+",\"source_TID\":"+std::to_string(set.thread)+",\"source_birth\":"+std::to_string(source_birth)+
        ",\"actual_external_foreground_receipt\":"+(external.empty()?"null":quote(external))+",\"actual_natural_source_return_receipt\":"+(returned.empty()?"null":quote(returned))+
        ",\"reason\":"+quote(reason)+",\"actor_created\":false,\"test_DOWN_sent\":false,\"SetForegroundWindow_calls_in_passive_phase\":0,"+
        "\"unknown_window_control_or_content_read\":false,\"startup_event_is_not_a_system_root_cause\":true,\"product_deadline_changed\":false");
    return accepted;
}
bool input_actor_disconnect_foreground_unchanged(State& s,LayoutFixtures& set,std::uint64_t mark,DWORD began,DWORD ended,std::string& reason){
    namespace e=product::guest::up_evidence_detail;
    const HWND source=set.frames[0].hwnd;const auto birth=process_birth(GetCurrentProcess());
    if(!mark||!birth||!set.owns(0)||GetForegroundWindow()!=source||static_cast<DWORD>(ended-began)>=0x80000000u){
        reason="pre_disconnect_boundary_or_actual_source_context_missing";return false;
    }
    drain_foreground_observer();std::istringstream rows(s.log.since(mark));std::string row;
    while(std::getline(rows,row)){
        if(input_actor_disconnect_driver_record(row,"guest_foreground_observer_buffer")&&e::unique_value(row,"\"overflow\":")!="0"){
            reason="foreground_observer_incomplete_after_actor_exit";return false;
        }
        if(!input_actor_disconnect_driver_record(row,"guest_foreground_observed"))continue;
        const auto stamp_value=e::number(row,"\"native_event_time_u32\":");
        if(!stamp_value||*stamp_value>MAXDWORD){reason="foreground_native_time_unknown_after_disconnect_boundary";return false;}
        const auto stamp=static_cast<DWORD>(*stamp_value);
        if(static_cast<DWORD>(stamp-began)>=0x80000000u||static_cast<DWORD>(ended-stamp)>=0x80000000u)continue;
        // The caller checks the exact source foreground independently. Any
        // observed external switch in this held interval is NOT this trial.
        if(e::number(row,"\"hwnd\":")!=reinterpret_cast<std::uintptr_t>(source)||
           e::number(row,"\"callback_window_pid_before\":")!=GetCurrentProcessId()||
           e::number(row,"\"callback_window_tid_before\":")!=set.thread||
           e::number(row,"\"callback_process_birth_filetime\":")!=birth||
           e::unique_value(row,"\"callback_process_birth_known\":")!="true"||
           e::unique_value(row,"\"callback_window_identity_stable\":")!="true"||
           e::unique_value(row,"\"class_utf8_known\":")!="true"||e::unique_value(row,"\"class_may_be_truncated\":")!="false"||
           e::number(row,"\"callback_foreground_before\":")!=reinterpret_cast<std::uintptr_t>(source)||
           e::number(row,"\"callback_foreground_after\":")!=reinterpret_cast<std::uintptr_t>(source)){
            reason="external_or_unknown_foreground_intervened_after_disconnect_boundary";return false;
        }
    }
    reason="no_external_foreground_observed_between_pre_exit_ACK_and_actual_terminal";return true;
}
bool input_actor_disconnect_cleanup_up(State& s,LayoutFixtures& set,std::size_t offset,
    std::uint64_t generation,bool& attempted){
    if(attempted||!s.left||s.left_up_attempted||!generation)return false;
    namespace e=product::guest::up_evidence_detail;
    // These are pre-UP abort/resource facts. The UP-specific receipt matcher
    // correctly requires an actual UP-send QPC, which does not exist yet.
    // Keep launch/PID/source/type/generation/shape checks without inventing
    // that QPC; the later actual UP uses the original strict matcher below.
    const auto resource_receipt=[&](std::string_view row,std::string_view type){
        return s.product_pid&&row.size()<=16384&&row.find_first_of("\r\n")==row.npos&&
            geometry_history_receipt(s,row,type)&&e::number(row,"\"generation\":")==generation;
    };
    const HWND source=set.frames[0].hwnd;std::string ended,gone;
    const bool resources_gone=wait([&]{
        ended=record(s,offset,"gesture_end",generation);gone.clear();
        std::istringstream rows(product_log_since(s,offset));std::string row;
        while(std::getline(rows,row))if(resource_receipt(row,"gesture_input_resource")&&
            e::quoted(row,"\"fact\":","isolation_gone"))gone=row;
        return resource_receipt(ended,"gesture_end")&&e::unique_value(ended,"\"normal_up\":")=="false"&&
            e::unique_value(ended,"\"pending_retired\":")=="true"&&
            e::unique_value(gone,"\"overlay_destroyed\":")=="true"&&
            e::unique_value(gone,"\"detach_completed\":")=="true"&&e::unique_value(gone,"\"detach_succeeded\":")=="true";
    },1000);
    const auto source_context=[&]{
        DWORD source_pid{};GUITHREADINFO gui{sizeof(gui)},foreground{sizeof(foreground)};POINT point{};
        return resources_gone&&desktop()&&s.process&&GetProcessId(s.process)==s.product_pid&&
            WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&s.product_evidence.bound()&&
            process_birth(s.process)==s.product_evidence.birth()&&set.owns(0)&&IsWindow(source)&&GetAncestor(source,GA_ROOT)==source&&
            GetWindowThreadProcessId(source,&source_pid)==set.thread&&source_pid==GetCurrentProcessId()&&
            gui_class(source)==L"PaneBind.V1.OwnedFixture."+s.run&&GetForegroundWindow()==source&&
            GetCursorPos(&point)&&GetAncestor(WindowFromPoint(point),GA_ROOT)==source&&
            GetGUIThreadInfo(set.thread,&gui)&&GetGUIThreadInfo(0,&foreground)&&
            !gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner&&!foreground.hwndCapture&&!foreground.hwndMoveSize&&!foreground.hwndMenuOwner&&
            !(gui.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE))&&
            !(foreground.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE))&&
            !(GetAsyncKeyState(VK_RBUTTON)&0x8000)&&!(GetAsyncKeyState(VK_MBUTTON)&0x8000)&&
            !(GetAsyncKeyState(VK_XBUTTON1)&0x8000)&&!(GetAsyncKeyState(VK_XBUTTON2)&0x8000)&&
            !(GetAsyncKeyState(VK_CONTROL)&0x8000)&&!(GetAsyncKeyState(VK_SHIFT)&0x8000)&&!(GetAsyncKeyState(VK_MENU)&0x8000)&&
            !(GetAsyncKeyState(VK_LWIN)&0x8000)&&!(GetAsyncKeyState(VK_RWIN)&0x8000);
    };
    if(!source_context()){
        s.log.emit("I08_disconnect_cleanup_UP_not_sent","\"generation\":"+std::to_string(generation)+
            ",\"actual_non_NormalUp_end\":"+quote(ended)+",\"actual_resource_Gone\":"+quote(gone)+
            ",\"reason\":\"exact_owned_idle_cleanup_destination_not_confirmed\",\"test_UP_attempted\":false,\"cleanup_known\":false");
        return false;
    }
    INPUT release{};release.type=INPUT_MOUSE;release.mi.dwFlags=MOUSEEVENTF_LEFTUP;release.mi.dwExtraInfo=0x50425631;
    LARGE_INTEGER before{},after{};
    if(!QueryPerformanceCounter(&before)||before.QuadPart<=0||!source_context())return false;
    // One cleanup UP, no final MOVE, replay, product stop or NormalUp claim.
    attempted=true;s.left_up_attempted=true;s.left_up_send_qpc=static_cast<std::uint64_t>(before.QuadPart);
    SetLastError(ERROR_SUCCESS);const UINT inserted=SendInput(1,&release,sizeof(release));const DWORD error=inserted==1?ERROR_SUCCESS:GetLastError();
    QueryPerformanceCounter(&after);if(inserted==1)s.left=false;
    s.log.emit("I08_disconnect_single_abort_cleanup_UP","\"generation\":"+std::to_string(generation)+
        ",\"actual_sender_PID\":"+std::to_string(GetCurrentProcessId())+",\"SendInput_attempted\":true,\"SendInput_return\":"+
        std::to_string(inserted)+",\"SendInput_error_on_failure\":"+std::to_string(error)+
        ",\"before_qpc\":"+std::to_string(before.QuadPart)+",\"after_qpc\":"+std::to_string(after.QuadPart)+
        ",\"packet_count\":1,\"mouse_flags\":"+std::to_string(release.mi.dwFlags)+
        ",\"new_MOVE_sent\":false,\"NormalUp_claimed\":false,\"delivery_not_inferred\":true");
    const auto observed=inserted==1?abort_cleanup_up_receipt(s,offset,generation):AbortCleanupUpReceipt{};
    s.log.emit("I08_disconnect_actual_abort_cleanup_UP_receipt",observed.json());return observed.matched;
}

void owned_driver_disconnect_case(State& s){
    if(s.suite!="DriverDisconnectOnly"||!desktop()||s.left||s.control||!medium(GetCurrentProcess()))
        throw std::runtime_error("I08_disconnect_exact_guest_suite_required");
    const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    if(!original.enabled)throw std::runtime_error("I08_disconnect_product_enabled_required");
    chain_check_product(s,pid,birth);
    auto configured=original;configured.magnet_move=false;configured.magnet_resize=false;configured.docking=false;
    configured.shake=false;configured.drag_transparency=false;chain_gui_save(s,configured);
    LayoutFixtures set{s};set.create();const auto work=chain_work(set.frames[0].hwnd);
    if(work.width()<900||work.height()<650)throw std::runtime_error("I08_disconnect_work_area_too_small");
    set.setup({{work.l+220,work.t+180,work.l+700,work.t+500}},{{100,100,work.width(),work.height()}});
    s.log.emit("I08_disconnect_test_contract","\"actual_input_sender\":\"exact_child_fixture_actor\",\"supervisor_owns_source_UI\":true,"
        "\"product_driver_channel\":false,\"product_driver_notification_inferred\":false,\"hard_process_death_scope\":\"NOT_RUN\","
        "\"normal_UP_disconnect_scope\":\"NOT_RUN\",\"passive_startup_external_then_natural_source_return_required_before_child_DOWN\":true,"
        "\"product_deadline_unchanged\":true,\"exact_30000ms_not_acceptance_gate\":true,\"post_exit_FocusLost_or_Pause_cannot_PASS\":true,"
        "\"old_independent_deadline_suite_repeated\":false,\"whole_I08_PASS\":false");
    owned_input_actor_exit_transport(s,set,pid,birth,true);
    chain_check_product(s,pid,birth);chain_gui_save(s,original);
    s.log.check("I08_disconnect_exact_fixture_resources_closed",set.close());chain_check_product(s,pid,birth);
    s.log.emit("I08_disconnect_scope_complete","\"result\":\"REAL_INPUT_CHILD_EXIT_PACKET_IMMUTABLE_AND_SUPERVISOR_RECOVERY_OBSERVED\","
        "\"same_product_PID\":"+std::to_string(pid)+",\"same_product_birth\":"+std::to_string(birth)+
        ",\"source_and_product_kept_alive_during_disconnect\":true,\"supervisor_new_Move_Resize\":true,"
        "\"new_input_actor_Move_Resize\":\"NOT_RUN\",\"product_DriverDisconnected_event\":\"NOT_IMPLEMENTED_NOT_INFERRED\","
        "\"whole_I08_PASS\":false,\"human_uat\":\"NOT_RUN\"");
}
