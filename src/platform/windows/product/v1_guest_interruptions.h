#pragma once

// Guest-only I08. Include after the existing layout/product-chain helpers.
// The test uses real keyboard/mouse input, native foreground and destruction;
// it never calls a product abort handler or manufactures a terminal event.
enum class GuestInterruption { Escape, Foreground, EmergencyHotkey, SourceDestroyed };
void owned_input_actor_exit(State&,LayoutFixtures&,DWORD,std::uint64_t);
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
    const auto text=product_log_since(s,g.offset);if(text.empty()||!trigger_qpc)throw std::runtime_error("I08_native_audit_evidence_missing");
    unsigned forbidden{},restorations{},unknown_clock{};std::istringstream rows(text);std::string row;
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
    const auto mark=read_product(s).size();std::string status;
    const bool observed=launch(s,L"--status",true)&&wait([&]{status=record(s,mark,"status");return
        status.find("\"gesture_active\":false")!=std::string::npos&&status.find(enabled?"\"enabled\":true":"\"enabled\":false")!=std::string::npos&&
        status.find("\"hook_owner_running\":true")!=std::string::npos;});
    GUITHREADINFO info{sizeof(info)};
    s.log.check("I08_actual_idle_input_and_resource_facts",observed&&!s.left&&!s.control&&
        !(GetAsyncKeyState(VK_CONTROL)&0x8000)&&
        GetGUIThreadInfo(set.thread,&info)&&!info.hwndCapture&&!info.hwndMoveSize&&!info.hwndMenuOwner,
        "\"production_status\":"+quote(status)+",\"actual_source_thread_capture\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(info.hwndCapture))+
        ",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+
        ",\"resident_hook_expected_until_final_product_exit\":true,\"full_shutdown_not_inferred\":true");
    chain_preview_hidden(s,"I08_actual_gesture_overlay_hidden");
}
void interruption_recovery(State& s,LayoutFixtures& set,std::uint64_t retired,bool require_existing_foreground=false){
    std::vector<Rect> actual;
    for(std::size_t n=0;n<set.active;++n){const auto r=visible(set.frames[n].hwnd);
        if(!set.owns(n)||!r)throw std::runtime_error("I08_recovery_actual_member_identity_unknown");actual.push_back(*r);}
    auto next=layout_down(set,0,HTCAPTION,false,require_existing_foreground);
    s.log.check("I08_recovery_new_generation",next.generation>retired);
    actual[0]=translated(actual[0],35,25);layout_phase(set,0,next,35,25,actual,"I08_recovery_actual_Move_exact",require_existing_foreground);layout_up(set,next,actual,require_existing_foreground);
    next=layout_down(set,0,HTBOTTOM,false,require_existing_foreground);actual[0].b+=24;
    layout_phase(set,0,next,0,24,actual,"I08_recovery_actual_Resize_exact",require_existing_foreground);layout_up(set,next,actual,require_existing_foreground);
    // Remove the legitimate edge-hover stretch preview with a real released
    // cursor movement, rather than calling a product UI cleanup handler.
    const HWND h=set.frames[0].hwnd;RECT client{};POINT point{};
    if(require_existing_foreground&&(!desktop()||!set.owns(0)||GetForegroundWindow()!=h))
        throw std::runtime_error("I08_nonactivating_recovery_client_foreground_not_current");
    if(!GetClientRect(h,&client))throw std::runtime_error("I08_recovery_client_unknown");
    point={(client.left+client.right)/2,(client.top+client.bottom)/2};
    if(!ClientToScreen(h,&point))throw std::runtime_error("I08_recovery_client_screen_unknown");cursor(s,point);
    const auto hit=WindowFromPoint(point);if(!hit||GetAncestor(hit,GA_ROOT)!=h)throw std::runtime_error("I08_recovery_exact_client_hit_not_observed");
    interruption_idle(s,set,true);
}
// Two finite, guest-only checks for the remaining concrete recovery branches.
// They reuse this product instance, its public commands and real input. Neither
// a successful message send nor an async button bit is a delivery receipt.
void recovery_contract_scope(State& s){
    if(s.suite!="RecoveryContractsOnly"||!desktop()||s.left||s.control||
        (GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_MBUTTON)&0x8000)||
        !chain_settings(s).enabled)throw std::runtime_error("recovery_exact_released_enabled_guest_required");
}
std::string recovery_button_admission(State& s,std::size_t mark,std::uint64_t qpc,
    unsigned button_value,std::string_view kind){
    namespace e=product::guest::up_evidence_detail;
    std::istringstream rows(product_log_since(s,mark));std::string row;
    while(std::getline(rows,row))if(e::quoted(row,"\"type\":","button_admission")&&
        e::quoted(row,"\"source_sha\":",s.product_source)&&e::number(row,"\"process_id\":")==s.product_pid&&
        e::number(row,"\"button\":")==button_value&&e::quoted(row,"\"kind\":",kind)&&
        e::number(row,"\"hook_observed_qpc\":").value_or(0)>=qpc)return row;
    return {};
}
std::string_view recovery_admission_state(const std::string& row,std::string_view name){
    const auto at=row.find(name);if(at==row.npos||row.find(name,at+name.size())!=row.npos)return {};
    const auto begin=at+name.size(),end=row.find('}',begin);
    if(begin>=row.size()||row[begin]!='{'||end==row.npos)return {};
    return std::string_view(row).substr(begin,end-begin+1);
}
void owned_revoked_pair_recovery_case(State& s){
    namespace v=panebind::core::v1;namespace e=product::guest::up_evidence_detail;
    recovery_contract_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    LayoutFixtures set{s};set.create();const auto work=chain_work(set.frames[0].hwnd);
    if(work.width()<900||work.height()<650)throw std::runtime_error("revoked_pair_work_area_too_small");
    set.setup({{work.l+220,work.t+180,work.l+700,work.t+500}});const HWND h=set.frames[0].hwnd;
    bool middle_pending=false,up_attempted=false;
    const auto send_middle=[&](bool pressed){
        if(!desktop()||!set.owns(0)||GetForegroundWindow()!=h||(!pressed&&(!middle_pending||up_attempted)))
            throw std::runtime_error("revoked_pair_single_TEST_input_context_invalid");
        INPUT packet{};packet.type=INPUT_MOUSE;packet.mi.dwFlags=pressed?MOUSEEVENTF_MIDDLEDOWN:MOUSEEVENTF_MIDDLEUP;
        LARGE_INTEGER before{},after{};QueryPerformanceCounter(&before);if(before.QuadPart<=0)throw std::runtime_error("revoked_pair_QPC_missing");
        if(!pressed)up_attempted=true;UINT inserted=UINT_MAX;DWORD error{};
        try{inject(packet,&inserted,&error);}catch(...){s.log.emit("revoked_pair_TEST_send_failure","\"requested_down\":"+
            std::string(pressed?"true":"false")+",\"SendInput_return\":"+(inserted==UINT_MAX?"null":std::to_string(inserted))+
            ",\"error\":"+std::to_string(error)+",\"delivery_inferred\":false");throw;}
        QueryPerformanceCounter(&after);middle_pending=pressed;
        s.log.emit("revoked_pair_TEST_send","\"button\":3,\"requested_down\":"+std::string(pressed?"true":"false")+
            ",\"send_before_qpc\":"+std::to_string(before.QuadPart)+",\"send_after_qpc\":"+std::to_string(after.QuadPart)+
            ",\"SendInput_return\":"+std::to_string(inserted)+",\"delivery_inferred\":false,\"intentional_second_DOWN_without_old_UP\":true");
        return static_cast<std::uint64_t>(before.QuadPart);
    };
    try{
        // A real mapped non-left press exercises the production non-capture
        // pair. Its public Pause revokes authority while that old UP is absent.
        chain_gui_save_mouse(s,{{0,3,1,0,v::Action::MoveRight}});chain_front(set,0);
        const auto point=layout_hit(set,0,HTCAPTION);if(!point)throw std::runtime_error("revoked_pair_caption_unknown");
        const auto geometry=visible(h);if(!geometry)throw std::runtime_error("revoked_pair_initial_geometry_unknown");
        const auto mark=read_product(s).size();cursor(s,*point);std::string prepared;
        // The existing stretch readiness helper deliberately accepts only
        // edges/corners. Read this real caption preparation directly instead.
        const bool caption_prepared=wait([&]{prepared=record(s,mark,"input_preparation");return
            e::quoted(prepared,"\"schema\":","panebind-v1/1")&&e::quoted(prepared,"\"source_sha\":",s.product_source)&&
            e::number(prepared,"\"process_id\":")==pid&&e::quoted(prepared,"\"reason\":","prepared_actual_source_and_hit")&&
            e::number(prepared,"\"root\":")==reinterpret_cast<std::uintptr_t>(h)&&
            e::number(prepared,"\"hit\":")==reinterpret_cast<std::uintptr_t>(h)&&e::number(prepared,"\"hit_pid\":")==GetCurrentProcessId()&&
            e::number(prepared,"\"region\":")==0&&e::number(prepared,"\"actual_hit_test\":")==HTCAPTION&&
            prepared.find("\"point\":["+std::to_string(point->x)+","+std::to_string(point->y)+"]")!=prepared.npos&&
            set.owns(0)&&visible(h)==geometry;},3500);
        if(!caption_prepared)
            throw std::runtime_error("revoked_pair_actual_preparation_missing");
        const auto old_down_qpc=send_middle(true);std::string old_down,command;
        const bool old_observed=wait([&]{old_down=recovery_button_admission(s,mark,old_down_qpc,3,"DOWN");
            command=record(s,mark,"mapped_command_completed");return !old_down.empty()&&!command.empty()&&
                e::unique_value(old_down,"\"own_consume_decision\":")=="true"&&e::quoted(old_down,"\"route\":","mapped")&&
                e::unique_value(recovery_admission_state(old_down,"\"exit_state\":"),"\"consumed\":")=="true"&&
                e::unique_value(recovery_admission_state(old_down,"\"exit_state\":"),"\"capture_pair\":")=="false"&&
                e::number(recovery_admission_state(old_down,"\"exit_state\":"),"\"generation\":")==number(command,"\"generation\":")&&
                e::unique_value(command,"\"active_generation_revoke_CAS_succeeded\":")=="true";},1500);
        s.log.check("D09_actual_old_mapped_consumed_DOWN",old_observed,
            "\"actual_DOWN\":"+quote(old_down)+",\"actual_command\":"+quote(command)+",\"old_UP_not_sent\":true");
        const auto old_generation=number(command,"\"generation\":");const auto pause_mark=read_product(s).size();std::string paused;
        const bool pause_observed=launch(s,L"--pause",true)&&wait([&]{paused=record(s,pause_mark,"status");
            return paused.find("\"enabled\":false")!=paused.npos&&paused.find("\"hook_owner_running\":true")!=paused.npos;});
        s.log.check("D09_public_Pause_before_new_pair",pause_observed,
            "\"actual_status\":"+quote(paused)+",\"old_UP_not_sent\":true");
        RECT client{};if(!GetClientRect(h,&client))throw std::runtime_error("revoked_pair_client_unknown");
        POINT center{(client.left+client.right)/2,(client.top+client.bottom)/2};
        if(!ClientToScreen(h,&center))throw std::runtime_error("revoked_pair_client_screen_unknown");cursor(s,center);
        if(!set.owns(0)||WindowFromPoint(center)!=h||GetForegroundWindow()!=h)throw std::runtime_error("revoked_pair_exact_client_hit_missing");
        const auto down_count=set.frames[0].client_down[2].load(),up_count=set.frames[0].client_up[2].load();
        const auto second_qpc=send_middle(true);std::string new_down;
        const bool new_down_observed=wait([&]{new_down=recovery_button_admission(s,pause_mark,second_qpc,3,"DOWN");
            const auto before=recovery_admission_state(new_down,"\"entry_state\":"),after=recovery_admission_state(new_down,"\"exit_state\":");
            return !new_down.empty()&&e::number(before,"\"generation\":")==old_generation&&e::number(before,"\"authority\":")==0&&
                e::unique_value(before,"\"consumed\":")=="true"&&e::unique_value(before,"\"capture_pair\":")=="false"&&
                e::unique_value(before,"\"paused\":")=="true"&&e::unique_value(after,"\"consumed\":")=="false"&&
                e::unique_value(new_down,"\"own_consume_decision\":")=="false"&&set.frames[0].client_down[2].load()==down_count+1;},1500);
        s.log.check("D09_actual_new_DOWN_retires_revoked_old_pair",new_down_observed,
            "\"actual_new_DOWN\":"+quote(new_down)+",\"client_DOWN_before\":"+std::to_string(down_count)+
            ",\"client_DOWN_after\":"+std::to_string(set.frames[0].client_down[2].load())+",\"old_UP_still_absent\":true");
        const auto up_qpc=send_middle(false);std::string new_up;
        const bool new_up_observed=wait([&]{new_up=recovery_button_admission(s,pause_mark,up_qpc,3,"UP");return
            !new_up.empty()&&e::unique_value(new_up,"\"matching_UP_branch\":")=="false"&&
            e::unique_value(new_up,"\"own_consume_decision\":")=="false"&&
            e::unique_value(new_up,"\"normalized_UP_push_attempted\":")=="false"&&set.frames[0].client_up[2].load()==up_count+1;},1500);
        s.log.check("D09_actual_new_UP_not_stolen_by_retired_pair",new_up_observed,
            "\"actual_new_UP\":"+quote(new_up)+",\"client_UP_before\":"+std::to_string(up_count)+
            ",\"client_UP_after\":"+std::to_string(set.frames[0].client_up[2].load())+",\"one_actual_UP_after_two_test_DOWNs\":true,\"NormalUp_not_inferred\":true");
        chain_gui_save_mouse(s,original.mouse_bindings);const auto resume_mark=read_product(s).size();
        s.log.check("D09_resume_same_product",launch(s,L"--command Pause",true)&&wait([&]{const auto status=record(s,resume_mark,"status");
            return status.find("\"enabled\":true")!=status.npos&&status.find("\"hook_owner_running\":true")!=status.npos;}));
        chain_check_product(s,pid,birth);interruption_recovery(s,set,old_generation);chain_check_product(s,pid,birth);
        chain_gui_save(s,original);s.log.check("D09_exact_fixture_cleanup",set.close());
        s.log.emit("revoked_pair_recovery_component","\"result\":\"PASS\",\"same_product_instance\":true,\"test_button\":3,\"new_client_DOWN_UP_delivered\":true,\"following_Move_Resize_NormalUp\":true");
    }catch(...){if(middle_pending&&!up_attempted)try{send_middle(false);}catch(...){}
        s.log.emit("revoked_pair_failure_cleanup","\"middle_test_input_outstanding\":"+std::string(middle_pending?"true":"false")+
            ",\"UP_already_attempted\":"+(up_attempted?"true":"false")+",\"cleanup_or_PASS_inferred\":false");throw;}
}
void owned_capture_loss_recovery_case(State& s){
    namespace v=panebind::core::v1;namespace e=product::guest::up_evidence_detail;
    recovery_contract_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    auto config=original;config.magnet_move=false;config.magnet_resize=false;config.docking=false;config.drag_transparency=false;config.shake=false;
    chain_gui_save(s,config);LayoutFixtures set{s};set.create();const auto work=chain_work(set.frames[0].hwnd);
    set.setup({{work.l+220,work.t+180,work.l+700,work.t+500}});const auto h=set.frames[0].hwnd;
    const auto initial=visible(h);if(!initial)throw std::runtime_error("capture_loss_initial_geometry_unknown");
    const auto g=layout_down(set,0,HTCAPTION,false);std::vector<Rect> actual{translated(*initial,35,25)};
    layout_phase(set,0,g,35,25,actual,"D24_real_active_Move_before_capture_loss");
    // Scan for this exact READY, not merely the latest resource fact.
    std::string capture_ready;{std::istringstream rows(product_log_since(s,g.offset));std::string row;while(std::getline(rows,row))
        if(e::quoted(row,"\"type\":","gesture_input_resource")&&e::quoted(row,"\"fact\":","capture_ready")&&
           e::number(row,"\"generation\":")==g.generation&&e::number(row,"\"process_id\":")==pid&&
           e::quoted(row,"\"source_sha\":",s.product_source))capture_ready=row;}
    const auto overlay_value=number(capture_ready,"\"overlay\":");const HWND overlay=reinterpret_cast<HWND>(static_cast<std::uintptr_t>(overlay_value));
    DWORD overlay_pid{};const auto overlay_tid=overlay?GetWindowThreadProcessId(overlay,&overlay_pid):0;
    GUITHREADINFO owner{sizeof(owner)},source{sizeof(source)};
    const bool exact=desktop()&&set.owns(0)&&GetForegroundWindow()==h&&s.left&&!s.left_up_attempted&&
        actual_up(s,g.offset,g.generation).empty()&&IsWindow(overlay)&&overlay_pid==pid&&overlay_tid&&
        number(capture_ready,"\"capture_owner_tid\":")==overlay_tid&&number(capture_ready,"\"source\":")==reinterpret_cast<std::uintptr_t>(h)&&
        number(capture_ready,"\"source_pid\":")==GetCurrentProcessId()&&number(capture_ready,"\"source_tid\":")==set.thread&&
        record(s,g.offset,"native_move_end",g.generation).empty()==false&&
        GetGUIThreadInfo(overlay_tid,&owner)&&owner.hwndCapture==overlay&&!owner.hwndMoveSize&&!owner.hwndMenuOwner&&
        GetGUIThreadInfo(set.thread,&source)&&source.hwndCapture==overlay&&!source.hwndMoveSize&&!source.hwndMenuOwner;
    s.log.check("D24_exact_current_product_overlay_capture_loss_fault_scope",exact,"\"generation\":"+std::to_string(g.generation)+
        ",\"actual_capture_ready\":"+quote(capture_ready)+",\"overlay\":"+std::to_string(overlay_value)+",\"overlay_pid\":"+
        std::to_string(overlay_pid)+",\"overlay_tid\":"+std::to_string(overlay_tid)+",\"source_capture\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(source.hwndCapture))+",\"owner_capture\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(owner.hwndCapture))+",\"message_sent\":false");
    LARGE_INTEGER trigger{};if(!QueryPerformanceCounter(&trigger)||trigger.QuadPart<=0)throw std::runtime_error("capture_loss_trigger_clock_missing");
    DWORD_PTR result{};SetLastError(0);const bool sent=SendMessageTimeoutW(overlay,WM_CANCELMODE,0,0,
        SMTO_ABORTIFHUNG|SMTO_ERRORONEXIT,1000,&result)!=0;const auto error=sent?0:GetLastError();
    s.log.emit("D24_exact_overlay_TEST_WM_CANCELMODE","\"generation\":"+std::to_string(g.generation)+",\"overlay\":"+
        std::to_string(overlay_value)+",\"trigger_before_qpc\":"+std::to_string(trigger.QuadPart)+",\"send_returned\":"+(sent?"true":"false")+
        ",\"message_result\":"+std::to_string(result)+",\"error\":"+std::to_string(error)+
        ",\"test_called_ReleaseCapture\":false,\"capture_loss_or_cleanup_from_send_inferred\":false");
    if(!sent)throw std::runtime_error("capture_loss_exact_overlay_fault_send_failed");
    std::string lost,released,gone;
    const bool observed=wait([&]{std::istringstream rows(product_log_since(s,g.offset));std::string row;
        while(std::getline(rows,row))if(e::quoted(row,"\"type\":","gesture_input_resource")&&e::number(row,"\"generation\":")==g.generation&&
            e::number(row,"\"process_id\":")==pid&&e::quoted(row,"\"source_sha\":",s.product_source)&&e::number(row,"\"overlay\":")==overlay_value){
            if(e::quoted(row,"\"fact\":","capture_lost"))lost=row;
            if(e::quoted(row,"\"fact\":","capture_released"))released=row;
            if(e::quoted(row,"\"fact\":","isolation_gone"))gone=row;}
        return !lost.empty()&&!released.empty()&&!gone.empty()&&e::number(lost,"\"resource_observed_qpc\":").value_or(0)>=static_cast<std::uint64_t>(trigger.QuadPart)&&
            e::unique_value(released,"\"capture_owner_observation_known\":")=="true"&&
            e::number(released,"\"capture_release_before_HWND\":")==0&&e::unique_value(released,"\"release_attempted\":")=="false"&&
            e::unique_value(released,"\"capture_release_after_is_copied_no_call\":")=="true"&&e::number(released,"\"capture_after_release\":")==0&&
            e::unique_value(gone,"\"overlay_destroyed\":")=="true"&&e::unique_value(gone,"\"hotkey_unregistered\":")=="true"&&
            e::unique_value(gone,"\"detach_succeeded\":")=="true"&&e::number(gone,"\"removal_reason\":").value_or(0)!=0;},2000);
    s.log.check("D24_actual_capture_loss_no_own_release_detach_Gone",observed,"\"generation\":"+std::to_string(g.generation)+
        ",\"actual_capture_lost\":"+quote(lost)+",\"actual_no_own_capture_observation\":"+quote(released)+",\"actual_Gone\":"+quote(gone)+
        ",\"Raw_or_legacy_UP_not_inferred\":true");
    interruption_end(s,g,v::End::SourceLost,false,h);cursor(s,{g.anchor.x+70,g.anchor.y+80});
    button(s,false);const auto abort_up=abort_cleanup_up_receipt(s,g.offset,g.generation);
    s.log.check("D24_matching_actual_single_abort_UP",abort_up.matched,abort_up.json()+",\"NormalUp_inferred\":false");
    interruption_audit_calls(s,g,static_cast<std::uint64_t>(trigger.QuadPart),std::nullopt);
    s.log.check("D24_no_post_loss_geometry_overwrite",set.owns(0)&&visible(h)==actual[0]);interruption_idle(s,set,true);
    chain_check_product(s,pid,birth);interruption_recovery(s,set,g.generation);chain_check_product(s,pid,birth);
    chain_gui_save(s,original);s.log.check("D24_exact_fixture_cleanup",set.close());
    s.log.emit("capture_loss_recovery_component","\"result\":\"PASS\",\"actual_no_own_abort_cleanup\":true,\"same_product_instance\":true,\"following_Move_Resize_NormalUp\":true,\"capture_reacquired_by_test\":false");
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
        const auto last_native=placement(s,g.offset,g.generation,moved[source],true);
        s.log.check("I08_actual_active_source_before_interruption",source_key&&set.owns(source)&&s.left&&actual_up(s,g.offset,g.generation).empty()&&
            last_native.find("\"authority_after\":true")!=std::string::npos,
            "\"driver_left_outstanding\":"+std::string(s.left?"true":"false")+",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+
            ",\"actual_authorized_native_receipt\":"+quote(last_native)+",\"physical_button_held_not_inferred\":true");
        const auto mark=read_product(s).size();LARGE_INTEGER trigger{};
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
            std::to_string(trigger.QuadPart)+",\"driver_left_outstanding\":"+(s.left?"true":"false")+
            ",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+
            ",\"product_handler_directly_called\":false");
        const auto ended=interruption_end(s,g,expected,destroyed,h);
        if(cause==GuestInterruption::EmergencyHotkey){
            std::string hotkey;
            const bool observed=wait([&]{hotkey=record(s,mark,"emergency_hotkey");return
                product::guest::matching_emergency_hotkey_receipt(hotkey,s.product_pid,s.product_source)&&
                s.left&&actual_up(s,g.offset,g.generation).empty();},1500);
            s.log.check("I08_actual_WM_HOTKEY_while_pressed",observed&&s.left&&actual_up(s,g.offset,g.generation).empty(),
                "\"production_hotkey\":"+quote(hotkey)+
                ",\"driver_left_outstanding\":"+(s.left?"true":"false")+",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON)));
        }
        const Rect expected_source=cause==GuestInterruption::Escape?initial[source]:moved[source];
        if(!destroyed)s.log.check("I08_actual_geometry_after_interruption",wait([&]{return set.owns(source)&&visible(h)==expected_source;}),
            "\"expected\":"+rect(expected_source)+",\"end\":"+quote(ended));
        // The WinEvent/keyboard must terminate BEFORE this additional mouse
        // movement; otherwise this test would conceal motion-only recovery.
        cursor(s,{g.anchor.x+70,g.anchor.y+80});
        if(focus_target)s.log.check("I08_product_does_not_retake_foreground",GetForegroundWindow()==focus_target);
        // TEST cleanup only, AFTER actual source destruction and abort/Gone.
        // Do not send the one planned UP toward the now-absent source or an
        // unowned/high desktop. Prepare the surviving blank fixture instead.
        // This is not product focus recovery, NormalUp, or proof of delivery.
        bool cleanup_target_attempted=false,cleanup_target_ready=false;std::string cleanup_target_error;
        HWND cleanup_target{};POINT cleanup_point{};std::string cleanup_gone;
        if(destroyed)try{
            const bool gone=wait([&]{cleanup_gone=record(s,g.offset,"gesture_input_resource",g.generation);return
                cleanup_gone.find("\"fact\":\"isolation_gone\"")!=cleanup_gone.npos&&
                cleanup_gone.find("\"overlay_destroyed\":true")!=cleanup_gone.npos&&
                cleanup_gone.find("\"hotkey_unregistered\":true")!=cleanup_gone.npos&&
                cleanup_gone.find("\"release_succeeded\":true")!=cleanup_gone.npos&&
                cleanup_gone.find("\"detach_succeeded\":true")!=cleanup_gone.npos;},500);
            if(!gone)throw std::runtime_error("destroyed_source_cleanup_actual_abort_Gone_not_observed");
            cleanup_target=set.frames[0].hwnd;GUITHREADINFO gui{sizeof(gui)};RECT client{};
            if(source==0||!set.owns(0)||!GetGUIThreadInfo(set.thread,&gui)||gui.hwndCapture||gui.hwndMoveSize||gui.hwndMenuOwner||
                !GetClientRect(cleanup_target,&client)||client.right-client.left<8||client.bottom-client.top<8)
                throw std::runtime_error("destroyed_source_cleanup_survivor_not_exact_idle_blank");
            cleanup_point={(client.left+client.right)/2,(client.top+client.bottom)/2};
            if(!ClientToScreen(cleanup_target,&cleanup_point))throw std::runtime_error("destroyed_source_cleanup_client_point_unknown");
            cleanup_target_attempted=true;
            const bool activated=SetForegroundWindow(cleanup_target)!=FALSE;
            if(!activated||!set.owns(0)||GetForegroundWindow()!=cleanup_target)
                throw std::runtime_error("destroyed_source_cleanup_exact_TEST_foreground_not_observed");
            cursor(s,cleanup_point);
            const auto hit=WindowFromPoint(cleanup_point);DWORD hit_pid{};
            const auto hit_tid=hit?GetWindowThreadProcessId(hit,&hit_pid):0;
            cleanup_target_ready=set.owns(0)&&GetForegroundWindow()==cleanup_target&&hit==cleanup_target&&
                GetAncestor(hit,GA_ROOT)==cleanup_target&&hit_pid==GetCurrentProcessId()&&hit_tid==set.thread;
            if(!cleanup_target_ready)cleanup_target_error="destroyed_source_cleanup_exact_TEST_client_hit_not_observed";
        }catch(const std::exception& error){cleanup_target_error=error.what();}
        // One short-lived EXISTING passive observer around this already
        // planned cleanup UP, only after the exact source was destroyed. It
        // diagnoses guest-chain delivery, never replaces the product receipt.
        std::unique_ptr<GuestUpHookObservation> cleanup_up_head;
        bool head_ready=false,head_finished=false;
        if(destroyed){cleanup_up_head=std::make_unique<GuestUpHookObservation>();head_ready=cleanup_up_head->start();}
        const auto finish_cleanup_up_head=[&](std::string_view stage){
            if(!cleanup_up_head||head_finished)return;head_finished=true;
            const bool stopped=cleanup_up_head->stop();const auto actual_head=cleanup_up_head->json();
            const bool one_tagged=head_ready&&stopped&&s.left_up_attempted&&s.left_up_send_qpc&&
                number(actual_head,"\"actual_left_UP_callbacks\":")==1&&number(actual_head,"\"last_qpc\":")>=s.left_up_send_qpc&&
                number(actual_head,"\"last_extra_information\":")==0x50425631&&
                (number(actual_head,"\"last_flags\":")&LLMHF_INJECTED);
            if(!head_ready||!stopped)s.log.failed=true;
            s.log.emit("I08_destroyed_source_cleanup_UP_guest_head","\"expected_retired_generation\":"+std::to_string(g.generation)+
                ",\"stage\":"+quote(stage)+",\"single_planned_test_UP_attempted\":"+(s.left_up_attempted?"true":"false")+
                ",\"single_planned_test_UP_send_before_qpc\":"+(s.left_up_attempted?std::to_string(s.left_up_send_qpc):"null")+
                ",\"observer_start_confirmed\":"+(head_ready?"true":"false")+",\"observer_stop_confirmed\":"+(stopped?"true":"false")+
                ",\"single_tagged_guest_head_UP_observed\":"+(one_tagged?"true":"false")+",\"actual_head\":"+actual_head+
                ",\"product_UP_or_NormalUp_inferred\":false,\"Raw_fallback_used\":false,\"additional_UP_sent\":false");
        };
        LARGE_INTEGER up_before{};if(!QueryPerformanceCounter(&up_before)||up_before.QuadPart<=0){
            finish_cleanup_up_head("unique_cleanup_UP_clock_unavailable");throw std::runtime_error("I08_actual_UP_clock_unavailable");}
        AbortCleanupUpReceipt abort_up;
        try{button(s,false);abort_up=abort_cleanup_up_receipt(s,g.offset,g.generation);}
        catch(...){finish_cleanup_up_head("exception_around_unique_cleanup_UP");throw;}
        finish_cleanup_up_head("after_actual_product_cleanup_UP_observation");
        if(destroyed){
            if(!cleanup_target_ready)s.log.failed=true;
            s.log.emit("I08_destroyed_source_single_UP_TEST_destination","\"exact_survivor\":"+
                std::to_string(reinterpret_cast<std::uintptr_t>(cleanup_target))+",\"point\":["+
                std::to_string(cleanup_point.x)+","+std::to_string(cleanup_point.y)+"],\"single_setup_attempted\":"+
                (cleanup_target_attempted?"true":"false")+",\"actual_exact_client_ready\":"+(cleanup_target_ready?"true":"false")+
                ",\"reason\":"+quote(cleanup_target_error)+",\"actual_abort_Gone_before_TEST_setup\":"+quote(cleanup_gone)+
                ",\"product_retake_foreground_or_NormalUp_inferred\":false,\"UP_retried\":false");
        }
        const bool matching_up=!s.left&&abort_up.matched;
        const std::string up_fields="\"single_test_UP_sent\":true,\"send_before_qpc\":"+std::to_string(up_before.QuadPart)+
            ",\"actual_UP\":"+(actual_up(s,g.offset,g.generation).empty()?"null":quote(actual_up(s,g.offset,g.generation)))+
            ",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+",\"not_NormalUp_evidence\":true,"+abort_up.json();
        if(matching_up)s.log.check("I08_matching_test_UP_actually_observed",true,up_fields);
        else{s.log.failed=true;s.log.emit("case","\"case\":\"I08_matching_test_UP_actually_observed\",\"result\":\"FAIL\","+up_fields);}
        // Bounded observation following one actual post-revocation cursor
        // event. Native entry QPC, not JSONL order, decides call timing.
        Sleep(120);interruption_audit_calls(s,g,static_cast<std::uint64_t>(trigger.QuadPart),
            cause==GuestInterruption::Escape?std::optional<Rect>{initial[source]}:std::nullopt);
        if(!destroyed)s.log.check("I08_no_post_revoke_geometry_overwrite",set.owns(source)&&visible(h)==expected_source);
        interruption_idle(s,set,cause!=GuestInterruption::EmergencyHotkey);
        if(focus_target)s.log.check("I08_status_did_not_retake_foreground",GetForegroundWindow()==focus_target);
        if(cause==GuestInterruption::EmergencyHotkey){
            const auto resume=read_product(s).size();
            s.log.check("I08_resume_via_existing_product_command",launch(s,L"--command Pause",true)&&wait([&]{const auto status=record(s,resume,"status");return
                status.find("\"enabled\":true")!=std::string::npos&&status.find("\"hook_owner_running\":true")!=std::string::npos;}));
        }
        chain_check_product(s,pid,birth);interruption_recovery(s,set,g.generation);chain_check_product(s,pid,birth);
        s.log.emit("I08_cause_and_following_recovery","\"cause\":"+std::to_string(static_cast<unsigned>(cause))+
            ",\"actual_non_NormalUp_termination\":true,\"original_abort_UP_observed\":"+(matching_up?"true":"false")+
            ",\"original_UP_subcase_FAIL_preserved\":"+(matching_up?"false":"true")+",\"new_real_Move_Resize_UP\":true,\"product_restarted\":false");
    }
    owned_input_actor_exit(s,set,pid,birth);
    chain_gui_save(s,original);s.log.check("I08_exact_fixture_resources_closed",set.close());chain_check_product(s,pid,birth);
    s.log.emit("I08_actual_interruption_sequence","\"result\":\"FOUR_CAUSES_AND_INPUT_ACTOR_EXIT_DEADLINE_CHECKED\",\"Escape\":true,\"real_foreground\":true,\"exact_blank_source_close\":true,\"actual_WM_HOTKEY\":true,\"input_actor_exit_then_real_30s_Deadline\":true,\"product_driver_channel\":false,\"same_product_process\":true,\"full_I08_PASS_inferred\":false,\"human_uat\":\"NOT_RUN\"");
}
