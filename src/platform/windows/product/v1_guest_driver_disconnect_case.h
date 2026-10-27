#pragma once

// Include after v1_guest_input_actor.h, within the existing fixture namespace.
// The child, not the supervisor, sends the interrupted DOWN/MOVE. This is one
// TEST transport-loss observation; PaneBind has no driver protocol.
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
        "\"normal_UP_disconnect_scope\":\"NOT_RUN\",\"old_independent_deadline_suite_repeated\":false,\"whole_I08_PASS\":false");
    owned_input_actor_exit_transport(s,set,pid,birth,true);
    chain_check_product(s,pid,birth);chain_gui_save(s,original);
    s.log.check("I08_disconnect_exact_fixture_resources_closed",set.close());chain_check_product(s,pid,birth);
    s.log.emit("I08_disconnect_scope_complete","\"result\":\"REAL_INPUT_CHILD_EXIT_PACKET_IMMUTABLE_AND_SUPERVISOR_RECOVERY_OBSERVED\","
        "\"same_product_PID\":"+std::to_string(pid)+",\"same_product_birth\":"+std::to_string(birth)+
        ",\"source_and_product_kept_alive_during_disconnect\":true,\"supervisor_new_Move_Resize\":true,"
        "\"new_input_actor_Move_Resize\":\"NOT_RUN\",\"product_DriverDisconnected_event\":\"NOT_IMPLEMENTED_NOT_INFERRED\","
        "\"whole_I08_PASS\":false,\"human_uat\":\"NOT_RUN\"");
}
