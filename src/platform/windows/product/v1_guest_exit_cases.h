#pragma once

// Exact disposable-guest fixtures only. Product commands, native readbacks
// and held process handles are the oracles; test termination is not a crash.
bool exit_recovery_same_user_session(HANDLE first,HANDLE second){
    HANDLE a{},b{};if(!OpenProcessToken(first,TOKEN_QUERY,&a))return false;Handle first_token(a);
    if(!OpenProcessToken(second,TOKEN_QUERY,&b))return false;Handle second_token(b);
    alignas(TOKEN_USER) std::array<unsigned char,512> left{},right{};DWORD bytes{},left_session{},right_session{};
    return GetTokenInformation(a,TokenUser,left.data(),static_cast<DWORD>(left.size()),&bytes)&&
        GetTokenInformation(b,TokenUser,right.data(),static_cast<DWORD>(right.size()),&bytes)&&
        IsValidSid(reinterpret_cast<TOKEN_USER*>(left.data())->User.Sid)&&IsValidSid(reinterpret_cast<TOKEN_USER*>(right.data())->User.Sid)&&
        EqualSid(reinterpret_cast<TOKEN_USER*>(left.data())->User.Sid,reinterpret_cast<TOKEN_USER*>(right.data())->User.Sid)&&
        GetTokenInformation(a,TokenSessionId,&left_session,sizeof(left_session),&bytes)&&
        GetTokenInformation(b,TokenSessionId,&right_session,sizeof(right_session),&bytes)&&left_session&&left_session==right_session;
}
struct ExitRecoveryHelper {
    DWORD pid{};std::uint64_t birth{};std::unique_ptr<Handle> handle;std::string mutation;
};
ExitRecoveryHelper exit_recovery_helper(State& s,const Gesture& g,std::uint64_t source_key){
    ExitRecoveryHelper out;out.mutation=record(s,g.offset,"visual_mutation",g.generation);
    const auto helper=number(out.mutation,"\"helper_pid\":");
    const auto parent_birth=process_birth(s.process);
    if(!source_key||!helper||helper>MAXDWORD||helper==s.product_pid||helper==GetCurrentProcessId()||
        number(out.mutation,"\"key\":")!=source_key||out.mutation.find("\"native_attempted\":true")==out.mutation.npos||
        out.mutation.find("\"native_completion_known\":true")==out.mutation.npos||
        out.mutation.find("\"visual_exact\":true")==out.mutation.npos||out.mutation.find("\"recovery_commit_ack\":true")==out.mutation.npos)
        throw std::runtime_error("exit_recovery_actual_visual_commit_and_helper_identity_required");
    out.pid=static_cast<DWORD>(helper);out.handle=std::make_unique<Handle>(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,out.pid));
    out.birth=out.handle->value?process_birth(out.handle->value):0;
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));PROCESSENTRY32W entry{sizeof(entry)};bool parent_known{};DWORD actual_parent{};
    if(snapshot.value!=INVALID_HANDLE_VALUE&&Process32FirstW(snapshot.value,&entry))do{
        if(entry.th32ProcessID==out.pid){parent_known=true;actual_parent=entry.th32ParentProcessID;break;}
    }while(Process32NextW(snapshot.value,&entry));
    std::array<wchar_t,32768> image{};DWORD length=static_cast<DWORD>(image.size());
    const bool image_known=out.handle->value&&QueryFullProcessImageNameW(out.handle->value,0,image.data(),&length);
    const auto actual_hash=image_known?hash(image.data()):std::string{},expected_hash=hash(s.product);
    const bool exact=out.handle->value&&out.birth&&parent_birth&&out.birth>=parent_birth&&parent_known&&actual_parent==s.product_pid&&
        WaitForSingleObject(out.handle->value,0)==WAIT_TIMEOUT&&image_known&&equal(image.data(),s.product.wstring())&&
        !actual_hash.empty()&&actual_hash==expected_hash&&expected_hash==hash(s.input_product)&&medium(out.handle->value)&&
        exit_recovery_same_user_session(s.process,out.handle->value);
    s.log.check("exit_recovery_actual_visual_helper_bound",exact,
        "\"actual_mutation\":"+quote(out.mutation)+",\"helper_PID\":"+std::to_string(out.pid)+",\"helper_birth\":"+std::to_string(out.birth)+
        ",\"actual_parent_PID\":"+(parent_known?std::to_string(actual_parent):"null")+",\"product_PID\":"+std::to_string(s.product_pid)+
        ",\"product_birth\":"+std::to_string(parent_birth)+",\"helper_image_sha256\":"+quote(actual_hash)+
        ",\"helper_control_rights\":\"QUERY_LIMITED_INFORMATION|SYNCHRONIZE\",\"helper_termination_permitted\":false");
    return out;
}
struct ExitRecoveryCapture {HWND overlay{};DWORD owner{};std::string ready;};
ExitRecoveryCapture exit_recovery_capture(LayoutFixtures& set,const Gesture& g,std::uint64_t source_key){
    auto& s=set.state;ExitRecoveryCapture out;std::istringstream rows(product_log_since(s,g.offset));std::string row;
    while(std::getline(rows,row))if(row.ends_with('}')&&row.find("\"type\":\"gesture_input_resource\"")!=row.npos&&
        row.find("\"fact\":\"capture_ready\"")!=row.npos&&number(row,"\"generation\":")==g.generation)out.ready=row;
    const auto overlay=number(out.ready,"\"overlay\":"),owner=number(out.ready,"\"capture_owner_tid\":");
    if(!overlay||!owner||owner>MAXDWORD)throw std::runtime_error("exit_recovery_actual_capture_receipt_missing");
    out.overlay=reinterpret_cast<HWND>(static_cast<std::uintptr_t>(overlay));out.owner=static_cast<DWORD>(owner);
    DWORD actual_pid{};const auto actual_tid=GetWindowThreadProcessId(out.overlay,&actual_pid);
    GUITHREADINFO source{sizeof(source)},resource{sizeof(resource)};
    const auto actual=visible(set.frames[0].hwnd);
    const auto placed=actual?placement(s,g.offset,g.generation,*actual,true):std::string{};
    const bool exact=actual&&set.owns(0)&&source_key&&s.left&&!s.left_up_attempted&&actual_up(s,g.offset,g.generation).empty()&&
        record(s,g.offset,"gesture_end",g.generation).empty()&&GetForegroundWindow()==set.frames[0].hwnd&&
        out.ready.find("\"capture_attempted\":true")!=out.ready.npos&&number(out.ready,"\"capture_actual_after\":")==overlay&&
        number(out.ready,"\"source\":")==reinterpret_cast<std::uintptr_t>(set.frames[0].hwnd)&&
        number(out.ready,"\"source_pid\":")==GetCurrentProcessId()&&number(out.ready,"\"source_tid\":")==set.thread&&
        actual_pid==s.product_pid&&actual_tid==out.owner&&IsWindow(out.overlay)&&
        GetGUIThreadInfo(set.thread,&source)&&GetGUIThreadInfo(out.owner,&resource)&&
        source.hwndCapture==out.overlay&&resource.hwndCapture==out.overlay&&!source.hwndMoveSize&&!source.hwndMenuOwner&&
        number(placed,"\"key\":")==source_key&&placed.find("\"authority_after\":true")!=placed.npos;
    s.log.check("exit_recovery_actual_active_Move_capture_before_terminator",exact,
        "\"generation\":"+std::to_string(g.generation)+",\"actual_capture_ready\":"+quote(out.ready)+",\"actual_last_placement\":"+quote(placed)+
        ",\"driver_left_outstanding\":"+(s.left?"true":"false")+",\"async_left_raw_SHORT\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+
        ",\"test_UP_not_yet_attempted\":"+(!s.left_up_attempted?"true":"false")+",\"NormalUp_or_physical_hold_not_inferred_from_ledger\":true");
    return out;
}
void exit_recovery_visual_baseline(LayoutFixtures& set){
    const HWND h=set.frames[0].hwnd;auto& s=set.state;
    if(!set.owns(0)||s.left||s.control)throw std::runtime_error("exit_recovery_visual_setup_exact_released_source_required");
    SetLastError(0);const auto style=GetWindowLongPtrW(h,GWL_EXSTYLE);if(!style&&GetLastError())throw std::runtime_error("exit_recovery_source_style_unknown");
    SetLastError(0);const auto old=SetWindowLongPtrW(h,GWL_EXSTYLE,style|WS_EX_LAYERED);
    if((!old&&GetLastError())||!SetLayeredWindowAttributes(h,0,173,LWA_ALPHA)||
        !SetWindowPos(h,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE))
        throw std::runtime_error("exit_recovery_exact_fixture_visual_setup_failed");
    s.log.check("exit_recovery_fixture_original_alpha_actual",set.owns(0)&&native_visual_is(h,173,false),
        "\"test_owned_fixture_setup_not_product_mutation\":true,\"expected_original_alpha\":173");
}
void exit_recovery_resources_after_exit(LayoutFixtures& set,const ExitRecoveryCapture& capture,const ExitRecoveryHelper& helper,Rect expected){
    auto& s=set.state;DWORD helper_exit=STILL_ACTIVE;GUITHREADINFO source{sizeof(source)},owner{sizeof(owner)};
    const bool helper_stopped=helper.handle&&helper.handle->value&&WaitForSingleObject(helper.handle->value,5000)==WAIT_OBJECT_0;
    const bool helper_code=helper_stopped&&GetExitCodeProcess(helper.handle->value,&helper_exit);
    const bool source_read=GetGUIThreadInfo(set.thread,&source)!=FALSE;
    const bool owner_read=GetGUIThreadInfo(capture.owner,&owner)!=FALSE;
    const bool product_stopped=s.process&&WaitForSingleObject(s.process,0)==WAIT_OBJECT_0;
    const bool restored=wait([&]{return set.owns(0)&&visible(set.frames[0].hwnd)==expected&&native_visual_is(set.frames[0].hwnd,173,false);},1500);
    const auto actual_visual=native_visual(set.frames[0].hwnd);
    s.log.check("exit_recovery_actual_resources_and_visual_restore",product_stopped&&helper_stopped&&helper_code&&helper_exit==0&&
        restored&&!IsWindow(capture.overlay)&&source_read&&!source.hwndCapture&&!source.hwndMoveSize&&!source.hwndMenuOwner&&
        !FindWindowW(L"PaneBind.V1.Application",L"PaneBind"),
        "\"product_process_exit_observed\":"+(product_stopped?"true":"false")+",\"helper_PID\":"+std::to_string(helper.pid)+
        ",\"helper_birth\":"+std::to_string(helper.birth)+",\"helper_exit_observed\":"+(helper_stopped?"true":"false")+
        ",\"helper_exit_code\":"+(helper_code?std::to_string(helper_exit):"null")+
        ",\"actual_overlay_gone\":"+(!IsWindow(capture.overlay)?"true":"false")+
        ",\"source_GUI_read\":"+(source_read?"true":"false")+",\"actual_source_capture\":"+(source_read?std::to_string(reinterpret_cast<std::uintptr_t>(source.hwndCapture)):"null")+
        ",\"exited_owner_GUI_read\":"+(owner_read?"true":"false")+",\"exited_owner_capture\":"+(owner_read?std::to_string(reinterpret_cast<std::uintptr_t>(owner.hwndCapture)):"null")+
        ",\"expected_restored_alpha\":173,\"source_actual_alpha\":"+(actual_visual&&actual_visual->layered&&actual_visual->known?std::to_string(actual_visual->alpha):"null")+
        ",\"source_geometry_expected\":"+rect(expected)+",\"guest_destruction_used\":false,\"helper_terminated_by_driver\":false");
}
void exit_recovery_cleanup_up_and_client(LayoutFixtures& set,const char* label){
    auto& s=set.state;const HWND h=set.frames[0].hwnd;GUITHREADINFO gui{sizeof(gui)};RECT client{};POINT point{};
    if(!s.left||s.left_up_attempted||s.process||!set.owns(0)||IsWindow(FindWindowW(L"PaneBind.V1.Application",L"PaneBind"))||
        !GetGUIThreadInfo(set.thread,&gui)||gui.hwndCapture||gui.hwndMoveSize||gui.hwndMenuOwner||!GetClientRect(h,&client))
        throw std::runtime_error("exit_recovery_cleanup_UP_requires_exited_product_exact_idle_source");
    point={(client.left+client.right)/2,(client.top+client.bottom)/2};
    if(!ClientToScreen(h,&point))throw std::runtime_error("exit_recovery_cleanup_client_point_unknown");
    GuestOwnedQueueUpObservation queue;s.log.check("exit_recovery_cleanup_exact_source_queue_observer",queue.start(h,set.thread));
    // Activation is explicitly TEST setup after actual parent/helper/resource
    // exit. It is not product focus recovery or a new gesture/NormalUp.
    const bool foreground=SetForegroundWindow(h)!=FALSE;cursor(s,point);DWORD_PTR hit{};
    const bool exact=foreground&&set.owns(0)&&GetForegroundWindow()==h&&WindowFromPoint(point)==h&&
        SendMessageTimeoutW(h,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&hit)&&hit==HTCLIENT&&
        GetGUIThreadInfo(set.thread,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner;
    s.log.check("exit_recovery_cleanup_UP_exact_TEST_client",exact,"\"activation_is_post_exit_test_setup\":true,\"foreground_reclaimed_by_product\":false");
    const auto before=set.frames[0].client_up[0].load();const DWORD reference=GetTickCount();button(s,false);std::uint64_t up{};
    const bool observed=layout_wait_sent([&]{up=queue.exact_source_click_up_qpc(h,set.thread,s.left_up_send_qpc,reference,point,0,1);
        return up&&set.frames[0].client_up[0].load()==before+1;},1500);
    const bool stopped=queue.stop();up=stopped?queue.exact_source_click_up_qpc(h,set.thread,s.left_up_send_qpc,reference,point,0,1):0;
    s.log.check(label,observed&&stopped&&up&&!s.left&&set.owns(0),
        "\"single_test_cleanup_UP_actual_source_qpc\":"+std::to_string(up)+",\"actual_source_queue\":"+queue.json()+
        ",\"source_client_UP_before\":"+std::to_string(before)+",\"source_client_UP_after\":"+std::to_string(set.frames[0].client_up[0].load())+
        ",\"product_NormalUp\":false,\"test_release_only_after_actual_product_exit\":true");
    pause_routing_client_pair(set,"exit_recovery_after_exit_actual_client_DOWN_UP");
}
void exit_recovery_no_instance_quit(State& s){
    if(s.process||FindWindowW(L"PaneBind.V1.Application",L"PaneBind"))throw std::runtime_error("exit_recovery_no_instance_precondition_failed");
    DWORD code=STILL_ACTIVE;const bool returned_zero=launch(s,L"--quit",true,&code);
    s.log.check("exit_recovery_repeated_quit_no_instance_returns_two",!returned_zero&&code==2&&!s.process&&
        !FindWindowW(L"PaneBind.V1.Application",L"PaneBind"),
        "\"actual_secondary_exit\":"+std::to_string(code)+",\"new_application_root_observed\":false,\"normal_start_not_requested\":true");
}
void exit_recovery_restart(State& s,DWORD old_pid,std::uint64_t old_birth,const std::string& binary){
    if(s.process||s.left||s.control||FindWindowW(L"PaneBind.V1.Application",L"PaneBind")||hash(s.product)!=binary||hash(s.input_product)!=binary)
        throw std::runtime_error("exit_recovery_same_EXE_released_restart_precondition_failed");
    const auto mark=read_product(s).size();s.log.check("exit_recovery_same_EXE_restart_created",launch(s,L"--diagnostic",false));
    const auto birth=process_birth(s.process);HWND root{};DWORD actual_pid{};std::string started;
    const bool observed=wait([&]{started=record(s,mark,"startup");root=FindWindowW(L"PaneBind.V1.Application",L"PaneBind");actual_pid=0;
        if(root)GetWindowThreadProcessId(root,&actual_pid);return number(started,"\"process_id\":")==s.product_pid&&root&&actual_pid==s.product_pid;},8000);
    s.log.check("exit_recovery_actual_new_instance_identity",observed&&s.process&&birth&&
        (s.product_pid!=old_pid||birth!=old_birth)&&GetProcessId(s.process)==s.product_pid&&medium(s.process)&&
        WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&hash(s.product)==binary,
        "\"previous_PID\":"+std::to_string(old_pid)+",\"previous_birth\":"+std::to_string(old_birth)+
        ",\"new_PID\":"+std::to_string(s.product_pid)+",\"new_birth\":"+std::to_string(birth)+
        ",\"unchanged_binary_sha256\":"+quote(binary)+",\"actual_startup\":"+quote(started));
}
void owned_exit_recovery_case(State& s){
    if(s.suite!="ExitRecoveryOnly"||s.left||s.control)throw std::runtime_error("exit_recovery_exact_guest_suite_idle_required");
    const auto original=chain_settings(s);const auto binary=hash(s.product);
    if(!original.enabled||binary.empty()||binary!=hash(s.input_product))throw std::runtime_error("exit_recovery_original_identity_or_enabled_unknown");
    LayoutFixtures set{s};set.create();const auto work=chain_work(set.frames[0].hwnd);
    if(work.width()<1100||work.height()<700)throw std::runtime_error("exit_recovery_actual_work_area_too_small");
    const Rect initial{work.l+240,work.t+180,work.l+640,work.t+480};const LayoutLimit limits{100,100,work.width(),work.height()};
    auto config=original;config.magnet_move=false;config.magnet_resize=false;config.docking=false;config.preview=false;
    config.feedback=false;config.drag_transparency=true;config.drag_alpha=190;config.shake=false;
    try{
        chain_gui_save(s,config);
        for(const bool forced:std::array{false,true}){
            const auto pid=s.product_pid;const auto birth=process_birth(s.process);chain_check_product(s,pid,birth);
            set.setup({initial},{limits});exit_recovery_visual_baseline(set);
            const auto g=layout_down(set,0,HTCAPTION,false);const Rect moved=translated(initial,-30,25);
            layout_phase(set,0,g,-30,25,{moved},forced?"exit_recovery_before_controlled_kill_actual_Move":"exit_recovery_before_active_quit_actual_Move");
            s.log.check("exit_recovery_active_temporary_alpha_actual",set.owns(0)&&native_visual_is(set.frames[0].hwnd,190,false),
                "\"actual_temporary_alpha\":190,\"original_alpha\":173");
            const auto key=number(record(s,g.offset,"gesture_start",g.generation),"\"key\":");
            const auto helper=exit_recovery_helper(s,g,key);const auto capture=exit_recovery_capture(set,g,key);
            chain_check_product(s,pid,birth);
            const auto stop_mark=read_product(s).size();constexpr DWORD test_exit=0x50425631u;DWORD command_exit=STILL_ACTIVE;
            bool command{};BOOL killed=FALSE;DWORD kill_error{};
            if(forced&&(GetProcessId(s.process)!=pid||process_birth(s.process)!=birth||hash(s.product)!=binary))
                throw std::runtime_error("exit_recovery_exact_product_identity_changed_before_test_kill");
            GUITHREADINFO active_source{sizeof(active_source)},active_owner{sizeof(active_owner)};
            if(!set.owns(0)||!s.left||s.left_up_attempted||!record(s,g.offset,"gesture_end",g.generation).empty()||
                !actual_up(s,g.offset,g.generation).empty()||GetForegroundWindow()!=set.frames[0].hwnd||
                !native_visual_is(set.frames[0].hwnd,190,false)||!GetGUIThreadInfo(set.thread,&active_source)||
                !GetGUIThreadInfo(capture.owner,&active_owner)||active_source.hwndCapture!=capture.overlay||active_owner.hwndCapture!=capture.overlay)
                throw std::runtime_error("exit_recovery_active_generation_or_capture_changed_before_terminator");
            LARGE_INTEGER trigger{};
            if(!QueryPerformanceCounter(&trigger)||trigger.QuadPart<=0)throw std::runtime_error("exit_recovery_terminator_QPC_unknown");
            if(forced){
                // Only this held process handle/PID/birth is eligible. Never
                // enumerate by filename or terminate its recovery helper.
                SetLastError(0);killed=TerminateProcess(s.process,test_exit);kill_error=killed?0:GetLastError();
            }else command=launch(s,L"--quit",true,&command_exit); // No cleanup(s), UP or stop hook before this command.
            const bool stopped=WaitForSingleObject(s.process,5000)==WAIT_OBJECT_0;DWORD code=STILL_ACTIVE;
            const bool exit_known=stopped&&GetExitCodeProcess(s.process,&code);
            s.log.check(forced?"exit_recovery_controlled_exact_product_termination":"exit_recovery_quit_while_Move_still_pressed",
                stopped&&exit_known&&s.left&&!s.left_up_attempted&&(forced?(killed&&code==test_exit):(command&&command_exit==0&&code==0)),
                "\"exact_product_PID\":"+std::to_string(pid)+",\"exact_product_birth\":"+std::to_string(birth)+
                ",\"trigger_before_qpc\":"+std::to_string(trigger.QuadPart)+",\"actual_process_exit\":"+(stopped?"true":"false")+
                ",\"actual_exit_code\":"+(exit_known?std::to_string(code):"null")+",\"quit_secondary_exit\":"+std::to_string(command_exit)+
                ",\"TerminateProcess_called\":"+(forced?"true":"false")+",\"TerminateProcess_error\":"+std::to_string(kill_error)+
                ",\"controlled_test_termination_not_natural_crash\":"+(forced?"true":"false")+",\"test_UP_attempted_before_exit\":false");
            const auto ended=record(s,g.offset,"gesture_end",g.generation),shutdown=record(s,stop_mark,"shutdown");
            if(!forced)s.log.check("exit_recovery_active_quit_actual_non_NormalUp_shutdown",
                number(ended,"\"end\":")==static_cast<unsigned>(v1::End::Exit)&&ended.find("\"normal_up\":false")!=ended.npos&&
                ended.find("\"pending_retired\":true")!=ended.npos&&number(shutdown,"\"process_id\":")==pid&&
                shutdown.find("\"own_hooks_stopped\":true")!=shutdown.npos&&shutdown.find("\"own_overlay_destroyed\":true")!=shutdown.npos&&
                shutdown.find("\"native_worker_exited\":true")!=shutdown.npos&&shutdown.find("\"issued_native_completion_unknown\":false")!=shutdown.npos&&
                shutdown.find("\"visual_recovery_stopped\":true")!=shutdown.npos&&shutdown.find("\"visual_restore_confirmed\":true")!=shutdown.npos&&
                shutdown.find("\"own_input_resources_stopped\":true")!=shutdown.npos&&shutdown.find("\"own_capture_released\":true")!=shutdown.npos&&
                shutdown.find("\"own_input_association_detached\":true")!=shutdown.npos&&
                input_resource_stop_observed(shutdown),"\"actual_gesture_end\":"+quote(ended)+",\"actual_shutdown\":"+quote(shutdown));
            else s.log.check("exit_recovery_controlled_kill_active_not_prior_abort",ended.empty()&&actual_up(s,g.offset,g.generation).empty(),
                "\"actual_gesture_end\":"+quote(ended)+",\"actual_shutdown\":"+quote(shutdown)+
                ",\"shutdown_or_NormalUp_not_inferred\":true,\"natural_crash_tested\":false");
            exit_recovery_resources_after_exit(set,capture,helper,moved);
            interruption_audit_calls(s,g,static_cast<std::uint64_t>(trigger.QuadPart),std::nullopt);
            CloseHandle(s.process);s.process=nullptr;
            exit_recovery_no_instance_quit(s);
            exit_recovery_cleanup_up_and_client(set,forced?"exit_recovery_after_controlled_kill_actual_cleanup_UP":"exit_recovery_after_active_quit_actual_cleanup_UP");
            exit_recovery_restart(s,pid,birth,binary);
            s.log.emit("exit_recovery_terminator_component","\"result\":\"PASS\",\"kind\":"+quote(forced?"controlled_exact_product_kill":"active_quit")+
                ",\"active_Move_and_temporary_alpha\":true,\"actual_helper_visual_and_input_resource_exit\":true,\"actual_post_exit_client_delivery\":true,\"human_uat\":\"NOT_RUN\"");
        }
        // New process identity means generation numbering may restart. Use
        // actual new receipts, never compare to a retired process's number.
        auto actual=visible(set.frames[0].hwnd);if(!actual||!set.owns(0))throw std::runtime_error("exit_recovery_restart_source_missing");
        auto g=layout_down(set,0,HTCAPTION,false);const auto moved=translated(*actual,35,25);
        layout_phase(set,0,g,35,25,{moved},"exit_recovery_restarted_same_EXE_actual_Move");layout_up(set,g,{moved});
        s.log.check("exit_recovery_restarted_Move_NormalUp_restores_original_alpha",set.owns(0)&&native_visual_is(set.frames[0].hwnd,173,false));
        g=layout_down(set,0,HTBOTTOM,false);Rect resized=moved;resized.b+=24;
        layout_phase(set,0,g,0,24,{resized},"exit_recovery_restarted_same_EXE_actual_Resize");layout_up(set,g,{resized});
        s.log.check("exit_recovery_restarted_Resize_NormalUp_original_visual",set.owns(0)&&native_visual_is(set.frames[0].hwnd,173,false));
        const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        s.log.check("exit_recovery_original_actual_GUI_configuration_restored",restored==expected);
        s.log.check("exit_recovery_exact_owned_windows_closed",set.close());
        s.log.emit("exit_recovery_component","\"result\":\"PASS\",\"active_quit\":true,\"controlled_product_termination\":true,"+
            std::string("\"same_EXE_restart_Move_Resize\":true,\"no_instance_quit_returns_two\":true,\"natural_crash\":\"NOT_RUN\",\"whole_version_PASS\":false,\"human_uat\":\"NOT_RUN\""));
    }catch(...){
        try{if(s.left&&!s.left_up_attempted)button(s,false);}catch(...){}
        try{if(s.process&&WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&!s.left&&!s.control)chain_gui_save(s,original);}catch(...){}
        throw;
    }
}
