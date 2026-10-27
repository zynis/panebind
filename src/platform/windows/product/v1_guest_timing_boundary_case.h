#pragma once

// TEST ONLY: three real-window accounting boundaries, not a latency benchmark
// or completion of NF02. The frozen production EXE receives only ordinary
// mapped input/public commands. Native messages/UP/completion are not forged.
namespace timing_boundary_detail {
namespace e=product::guest::up_evidence_detail;
std::string native_population(const std::string& summary){
    const auto at=summary.find("\"hook_observed_to_postverify\":{");
    if(at==summary.npos)return {};const auto begin=summary.find('{',at),end=summary.find('}',begin);
    return end==summary.npos?std::string{}:summary.substr(begin,end-begin+1);
}
bool field(const std::string& row,std::string_view name,bool expected){
    return e::unique_value(row,"\""+std::string(name)+"\":")==std::optional<std::string_view>(expected?"true":"false");
}
std::string stop_instance(State& s,std::size_t mark){
    DWORD command_exit=STILL_ACTIVE,code=STILL_ACTIVE;const bool requested=launch(s,L"--quit",true,&command_exit);
    const DWORD result=s.process?WaitForSingleObject(s.process,2500):WAIT_FAILED;
    const bool exit_known=s.process&&result==WAIT_OBJECT_0&&GetExitCodeProcess(s.process,&code)!=FALSE;
    const auto summary=record(s,mark,"timing_summary");
    s.log.check("timing_boundary_actual_public_quit_and_final_summary",requested&&command_exit==0&&exit_known&&code==0&&
        geometry_history_receipt(s,summary,"timing_summary")&&field(summary,"after_native_writer_stop_and_final_drain",true),
        "\"quit_secondary_exit\":"+std::to_string(command_exit)+",\"held_process_wait\":"+std::to_string(result)+
        ",\"actual_process_exit\":"+(exit_known?std::to_string(code):"null")+",\"actual_final_summary\":"+quote(summary));
    return summary;
}
void close_instance_and_restart(State& s,DWORD pid,std::uint64_t birth,const std::string& binary){
    if(!s.process||WaitForSingleObject(s.process,0)!=WAIT_OBJECT_0)throw std::runtime_error("timing_boundary_old_process_not_signalled");
    CloseHandle(s.process);s.process=nullptr;exit_recovery_restart(s,pid,birth,binary);
}

// No guessed stop delay. This wait observes the exact product input-owner
// THREAD HANDLE becoming signalled, which Application::stop actually joins
// before writer.stop. This proves the stop path, NOT the exact interval inside
// writer.stop itself. It can release only this fixture's one bounded stall.
struct StopReleaseState {
    HANDLE owner{},cancel{CreateEventW(nullptr,TRUE,FALSE,nullptr)},done{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
    std::shared_ptr<TimingControlledStall> stall;std::atomic<DWORD> waited{WAIT_FAILED};
    std::atomic<bool> released{};DWORD tid{};std::atomic<std::uint64_t> owner_exit_observed_qpc{};
    ~StopReleaseState(){for(const auto h:std::array{owner,cancel,done})if(h)CloseHandle(h);}
};
struct StopRelease {
    std::shared_ptr<StopReleaseState> state;std::thread worker;
    StopRelease(State& s,std::shared_ptr<TimingControlledStall> stall):state(std::make_shared<StopReleaseState>()){
        HWND found{},previous{};unsigned count{},visited{};DWORD tid{};
        while((previous=FindWindowExW(HWND_MESSAGE,previous,L"PaneBind.V1.Input",nullptr))!=nullptr){
            if(++visited>64)throw std::runtime_error("timing_boundary_input_receiver_inventory_bound");
            DWORD pid{};const auto candidate=GetWindowThreadProcessId(previous,&pid);
            if(pid==s.product_pid){found=previous;tid=candidate;++count;}
        }
        if(count!=1||!found||!tid||!s.process||WaitForSingleObject(s.process,0)!=WAIT_TIMEOUT)
            throw std::runtime_error("timing_boundary_unique_product_input_owner_unknown");
        state->owner=OpenThread(SYNCHRONIZE|THREAD_QUERY_LIMITED_INFORMATION,FALSE,tid);state->tid=tid;state->stall=std::move(stall);
        if(!state->owner||!state->cancel||!state->done||GetProcessIdOfThread(state->owner)!=s.product_pid||
            GetThreadId(state->owner)!=tid||WaitForSingleObject(state->owner,0)!=WAIT_TIMEOUT)
            throw std::runtime_error("timing_boundary_held_input_owner_identity_unknown");
        worker=std::thread([shared=state]{const std::array handles{shared->cancel,shared->owner};
            const auto waited=WaitForMultipleObjects(static_cast<DWORD>(handles.size()),handles.data(),FALSE,3500);shared->waited=waited;
            if(waited==WAIT_OBJECT_0+1){LARGE_INTEGER q{};if(QueryPerformanceCounter(&q)&&q.QuadPart>0)
                shared->owner_exit_observed_qpc=static_cast<std::uint64_t>(q.QuadPart);
                shared->released=shared->stall->release_once();}
            SetEvent(shared->done);
        });
    }
    ~StopRelease(){if(worker.joinable()){SetEvent(state->cancel);
        if(WaitForSingleObject(state->done,500)==WAIT_OBJECT_0)worker.join();else worker.detach();}}
    bool completed()const{return WaitForSingleObject(state->done,0)==WAIT_OBJECT_0;}
};

void mapped_origin(State& s){
    namespace v=panebind::core::v1;const auto original=chain_settings(s);const auto binary=hash(s.product);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);LayoutFixtures set{s};set.create();
    const auto work=chain_work(set.frames[0].hwnd);if(work.width()<900||work.height()<650)throw std::runtime_error("timing_mapping_work_area_too_small");
    const Rect initial{work.l+220,work.t+180,work.l+620,work.t+480};set.setup({initial});
    auto config=original;config.magnet_move=false;config.magnet_resize=false;config.preview=false;config.feedback=false;
    config.drag_transparency=false;config.docking=false;config.shake=false;chain_gui_save(s,config);
    chain_gui_save_mouse(s,{{2,3,1,0,v::Action::MoveRight}});
    bool pending{},up_attempted{};std::uint64_t generation{},down_qpc{},up_qpc{};std::string observed_down,observed_up,command,mapped;
    const HWND mapped_source=set.frames[0].hwnd;POINT sent_point{};bool sent_point_known=false;
    const auto release_target=[&]{
        POINT actual{};DWORD source_pid{};GUITHREADINFO source_gui{sizeof(source_gui)},foreground_gui{sizeof(foreground_gui)};
        const auto idle_for_pair=[&](const GUITHREADINFO& gui){return
            (!gui.hwndCapture||gui.hwndCapture==mapped_source)&&!gui.hwndMoveSize&&!gui.hwndMenuOwner&&
            !(gui.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE));};
        // A successful MoveRight changes the source rectangle/hit region.
        // Recheck the REAL current destination, not the old HTRIGHT/rect.
        return sent_point_known&&desktop()&&set.owns(0)&&set.frames[0].hwnd==mapped_source&&
            GetWindowThreadProcessId(mapped_source,&source_pid)==set.thread&&source_pid==GetCurrentProcessId()&&
            GetForegroundWindow()==mapped_source&&GetCursorPos(&actual)&&actual.x==sent_point.x&&actual.y==sent_point.y&&
            GetAncestor(WindowFromPoint(actual),GA_ROOT)==mapped_source&&mapping_remaining_modifiers(0)&&
            GetGUIThreadInfo(set.thread,&source_gui)&&GetGUIThreadInfo(0,&foreground_gui)&&
            idle_for_pair(source_gui)&&idle_for_pair(foreground_gui);
    };
    try{
        chain_front(set,0);const auto h=set.frames[0].hwnd;const auto point=layout_hit(set,0,HTRIGHT);
        if(!point)throw std::runtime_error("timing_mapping_actual_HTRIGHT_missing");
        sent_point=*point;sent_point_known=true;
        const auto preparation_mark=read_product(s).size();cursor(s,*point);std::string preparation;
        s.log.check("timing_mapping_fresh_actual_preparation",chain_stretch_preparation(set,*point,HTRIGHT,initial,preparation_mark,3500,preparation)&&
            mapping_remaining_hit(set,*point,HTRIGHT,initial,0),"\"actual_preparation\":"+quote(preparation));
        const auto key=e::number(preparation,"\"key\":");const auto mark=read_product(s).size();
        const auto dpi=GetDpiForWindow(h);const auto step=dpi?MulDiv(config.step_dip,static_cast<int>(dpi),96):0;
        if(!key||!step||!set.owns(0))throw std::runtime_error("timing_mapping_key_or_DPI_unknown");
        const auto expected=translated(initial,step,0);
        const auto send=[&](bool pressed){
            if(!desktop()||!set.owns(0)||GetForegroundWindow()!=h||!mapping_remaining_modifiers(0)||
                (pressed?pending||!mapping_remaining_hit(set,*point,HTRIGHT,initial,0):!pending||up_attempted||!release_target()))
                throw std::runtime_error("timing_mapping_exact_context_or_middle_ledger_rejected");
            INPUT packet{};packet.type=INPUT_MOUSE;packet.mi.dwFlags=pressed?MOUSEEVENTF_MIDDLEDOWN:MOUSEEVENTF_MIDDLEUP;
            LARGE_INTEGER q{};if(!QueryPerformanceCounter(&q)||q.QuadPart<=0)throw std::runtime_error("timing_mapping_send_QPC_unknown");
            if(!pressed&&!release_target())throw std::runtime_error("timing_mapping_exact_UP_destination_changed");
            if(pressed)down_qpc=static_cast<std::uint64_t>(q.QuadPart);else{up_qpc=static_cast<std::uint64_t>(q.QuadPart);up_attempted=true;}
            UINT inserted=UINT_MAX;DWORD error{};try{inject(packet,&inserted,&error);}catch(...){
                s.log.emit("timing_mapping_input_error","\"DOWN\":"+std::string(pressed?"true":"false")+
                    ",\"actual_SendInput_return\":"+(inserted==UINT_MAX?"null":std::to_string(inserted))+",\"error\":"+std::to_string(error));throw;}
            pending=pressed;
        };
        send(true);const bool actual=wait([&]{observed_down=record(s,mark,"input_button_observed");generation=number(observed_down,"\"generation\":");
            command=record(s,mark,"mapped_command_completed",generation);mapped=placement(s,mark,0,expected,true);
            const auto native_before=e::number(mapped,"\"native_before_qpc\":"),native_after=e::number(mapped,"\"native_after_qpc\":"),
                geometry_qpc=e::number(mapped,"\"geometry_observed_qpc\":"),post_qpc=e::number(mapped,"\"post_observed_qpc\":");
            return geometry_history_receipt(s,observed_down,"input_button_observed")&&e::quoted(observed_down,"\"kind\":","DOWN")&&
                e::quoted(observed_down,"\"source\":","WH_MOUSE_LL")&&e::quoted(observed_down,"\"route\":","mapped")&&
                e::quoted(observed_down,"\"action\":","MoveRight")&&e::number(observed_down,"\"button\":")==3&&
                e::number(observed_down,"\"source_key\":")==key&&e::number(observed_down,"\"hook_observed_qpc\":").value_or(0)>=down_qpc&&
                generation&&geometry_history_receipt(s,command,"mapped_command_completed")&&field(command,"handler_invoked",true)&&
                field(command,"handler_returned",true)&&e::quoted(command,"\"action\":","MoveRight")&&
                e::number(command,"\"generation\":")==generation&&
                geometry_history_receipt(s,mapped,"placement")&&e::number(mapped,"\"key\":")==key&&
                // This same-display discrete command has no live capture
                // lease: unchecked/false is NOT a failed live post-context.
                // Keep its actual native/identity/geometry completion proof;
                // never accept a checked-but-invalid live context here.
                e::number(mapped,"\"generation\":")==0&&e::quoted(mapped,"\"intent_reason\":","command")&&
                field(mapped,"normal_up_completion",false)&&field(mapped,"explicit_destination_display_bound",false)&&
                field(mapped,"post_context_checked",false)&&field(mapped,"post_context_valid",false)&&
                e::quoted(mapped,"\"post_context_reason\":","")&&e::quoted(mapped,"\"reason\":","exact_native_completion")&&
                field(mapped,"native_callsite_claimed",true)&&field(mapped,"native_attempted",true)&&
                field(mapped,"native_succeeded",true)&&field(mapped,"native_completion_known",true)&&
                field(mapped,"identity_valid",true)&&field(mapped,"exact",true)&&e::number(mapped,"\"call_id\":").value_or(0)>0&&
                e::number(mapped,"\"hook_observed_qpc\":")==e::number(observed_down,"\"hook_observed_qpc\":")&&
                native_before&&native_after&&geometry_qpc&&post_qpc&&*native_before>0&&
                *native_before>=e::number(observed_down,"\"hook_observed_qpc\":").value_or(UINT64_MAX)&&
                *native_after>=*native_before&&*geometry_qpc>=*native_after&&*post_qpc>=*geometry_qpc&&
                set.owns(0)&&visible(h)==expected;
        },2000);
        // Always attempt this pair's unique UP before reporting a failed
        // command assertion. No extra DOWN or guessed product UP is added.
        send(false);const bool up=wait([&]{observed_up=actual_up(s,mark,generation,3);return
            geometry_history_receipt(s,observed_up,"input_button_observed")&&e::number(observed_up,"\"source_key\":")==key&&
            e::number(observed_up,"\"hook_observed_qpc\":").value_or(0)>=up_qpc;},1500);
        s.log.check("timing_mapping_actual_hook_origin_reaches_geometry",actual&&up,
            "\"actual_DOWN\":"+quote(observed_down)+",\"actual_matching_UP\":"+quote(observed_up)+
            ",\"actual_mapping_command\":"+quote(command)+",\"actual_placement\":"+quote(mapped)+",\"independent_expected\":"+rect(expected));
        const auto public_mark=read_product(s).size();s.log.check("timing_public_command_no_hook_control",launch(s,L"--command MoveLeft",true)&&
            wait([&]{const auto row=placement(s,public_mark,0,initial,true);return geometry_history_receipt(s,row,"placement")&&
                e::number(row,"\"hook_observed_qpc\":")==0&&e::number(row,"\"key\":")==key&&set.owns(0)&&visible(h)==initial;}),
            "\"actual_placement\":"+quote(placement(s,public_mark,0,initial,true))+",\"hook_origin_not_fabricated\":true");
        const auto summary=stop_instance(s,mark),population=native_population(summary);
        s.log.check("timing_mapping_summary_keeps_no_hook_missing",e::number(population,"\"samples\":")==1&&
            e::number(population,"\"missing_clock\":")==1&&e::number(population,"\"discarded\":")==1&&
            e::number(population,"\"unfinished_calls\":")==0&&field(summary,"native_writer_exited",true)&&
            field(summary,"issued_native_completion_unknown",false),"\"actual_summary\":"+quote(summary));
        s.log.check("timing_mapping_known_owned_windows_closed",set.close());close_instance_and_restart(s,pid,birth,binary);
        chain_gui_save_mouse(s,original.mouse_bindings);chain_gui_save(s,original);
    }catch(...){if(pending&&!up_attempted)try{if(release_target()){
            up_attempted=true;INPUT packet{};packet.type=INPUT_MOUSE;packet.mi.dwFlags=MOUSEEVENTF_MIDDLEUP;UINT inserted{};DWORD error{};
            inject(packet,&inserted,&error);pending=false;}}catch(...){}
        s.log.emit("timing_mapping_failure_cleanup","\"own_middle_DOWN_pending\":"+std::string(pending?"true":"false")+
            ",\"single_UP_attempted\":"+(up_attempted?"true":"false")+
            ",\"cleanup_status\":\"UNKNOWN\",\"NormalUp_or_delivery_PASS_inferred\":false");
        try{if(!set.closed)set.close();}catch(...){}throw;}
}

void stop_boundary(State& s,bool release_during_stop){
    const auto original=chain_settings(s);const auto binary=hash(s.product);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    LayoutFixtures set{s};set.create();const auto work=chain_work(set.frames[0].hwnd);
    if(work.width()<900||work.height()<650)throw std::runtime_error("timing_stop_work_area_too_small");
    const Rect initial{work.l+220,work.t+180,work.l+620,work.t+480};set.setup({initial});
    auto config=original;config.magnet_move=false;config.magnet_resize=false;config.preview=false;config.feedback=false;
    config.drag_transparency=false;config.docking=false;config.shake=false;chain_gui_save(s,config);
    const auto stall=std::make_shared<TimingControlledStall>();if(!stall->valid())throw std::runtime_error("timing_controlled_stall_events_failed");
    std::unique_ptr<StopRelease> stop_release;
    try{
        const auto g=layout_down(set,0,HTRIGHT,false);const auto mark=read_product(s).size();
        set.frames[0].timing_stall.store(stall);stall->armed=true;
        refuse_observed_terminal_motion(s,g);cursor(s,{g.anchor.x+32,g.anchor.y});
        const bool entered=wait([&]{return WaitForSingleObject(stall->entered,0)==WAIT_OBJECT_0;},1000);
        s.log.check("timing_stop_real_issued_Resize_entered",entered&&stall->entered_observed&&stall->receiver_tid==set.thread&&
            stall->before_qpc&&!(stall->flags&SWP_NOSIZE)&&s.left&&!s.left_up_attempted,
            "\"generation\":"+std::to_string(g.generation)+",\"receiver_TID\":"+std::to_string(stall->receiver_tid.load())+
            ",\"actual_WM_WINDOWPOSCHANGING_qpc\":"+std::to_string(stall->before_qpc.load())+
            ",\"unmodified_WINDOWPOS_flags\":"+std::to_string(stall->flags.load())+",\"source_wait_max_ms\":4000");
        if(release_during_stop)stop_release=std::make_unique<StopRelease>(s,stall);
        const auto summary=stop_instance(s,mark),population=native_population(summary);const auto shutdown=record(s,mark,"shutdown");
        const auto completion=record(s,mark,"native_completion");const auto issued=record(s,mark,"placement",g.generation);
        const bool known=field(summary,"native_writer_exited",true)&&field(summary,"issued_native_completion_unknown",false);
        s.log.emit("timing_stop_actual_boundaries","\"release_mode\":"+quote(release_during_stop?"actual_input_owner_thread_exit":"after_actual_product_exit")+
            ",\"actual_issued_receipt\":"+quote(issued)+",\"actual_completion\":"+quote(completion)+",\"actual_shutdown\":"+quote(shutdown)+
            ",\"source_WndProc_wait_finished_at_product_exit\":"+(stall->finished_observed?"true":"false")+
            ",\"owner_thread_wait\":"+(stop_release?std::to_string(stop_release->state->waited.load()):"null")+
            ",\"owner_thread_exit_observed_qpc\":"+(stop_release?std::to_string(stop_release->state->owner_exit_observed_qpc.load()):"null")+
            ",\"source_release_qpc\":"+std::to_string(stall->release_qpc.load())+",\"source_WndProc_wait_finished_qpc\":"+
            std::to_string(stall->after_qpc.load())+",\"stop_not_NormalUp\":true,\"unknown_not_counted_as_completion\":true");
        if(release_during_stop){
            s.log.check("timing_stop_final_drain_includes_actual_last_completion",stop_release&&stop_release->completed()&&
                stop_release->state->waited==WAIT_OBJECT_0+1&&stop_release->state->released&&stall->finished_observed&&
                stall->wait_result==WAIT_OBJECT_0&&known&&geometry_history_receipt(s,completion,"native_completion")&&
                e::number(completion,"\"call_id\":")==e::number(issued,"\"call_id\":")&&
                field(completion,"native_completion_known",true)&&field(completion,"native_attempted",true)&&
                e::number(completion,"\"native_after_qpc\":").value_or(0)>=stall->after_qpc.load()&&
                e::number(completion,"\"post_observed_qpc\":").value_or(0)>0&&
                e::number(completion,"\"sequence\":").value_or(UINT64_MAX)<e::number(summary,"\"sequence\":").value_or(0)&&
                e::number(population,"\"samples\":")==1&&e::number(population,"\"unfinished_calls\":")==0,
                "\"actual_final_summary\":"+quote(summary)+",\"actual_last_completion\":"+quote(completion));
        }else{
            s.log.check("timing_stop_unknown_and_unfinished_not_zeroed",!stall->finished_observed&&!known&&
                field(summary,"native_writer_exited",false)&&field(summary,"issued_native_completion_unknown",true)&&
                field(issued,"native_callsite_claimed",true)&&field(issued,"native_completion_known",false)&&completion.empty()&&
                e::number(population,"\"samples\":")==0&&e::number(population,"\"unfinished_calls\":")==1,
                "\"actual_final_summary\":"+quote(summary)+",\"unknown_native_not_cancelled_or_PASS\":true");
            // The old product really exited; only now release this test-owned
            // native receiver. This cannot amend its already-final summary.
            s.log.check("timing_stop_post_exit_source_release_event",stall->release_once());
        }
        stop_release.reset();s.log.check("timing_stop_test_receiver_bounded_resume",wait([&]{return
            WaitForSingleObject(stall->finished,0)==WAIT_OBJECT_0;},1500)&&stall->wait_result==WAIT_OBJECT_0);
        set.frames[0].timing_stall.store({});
        CloseHandle(s.process);s.process=nullptr;
        exit_recovery_cleanup_up_and_client(set,"timing_stop_actual_TEST_cleanup_UP_after_product_exit_not_NormalUp");
        s.log.check("timing_stop_known_owned_windows_closed",set.close());exit_recovery_restart(s,pid,birth,binary);chain_gui_save(s,original);
    }catch(...){stall->release_once();stop_release.reset();
        (void)wait([&]{return WaitForSingleObject(stall->finished,0)==WAIT_OBJECT_0||!stall->entered_observed;},1500);
        set.frames[0].timing_stall.store({});try{if(s.left&&!s.left_up_attempted)button(s,false);}catch(...){}
        try{if(!set.closed)set.close();}catch(...){}throw;}
}
}
void owned_timing_boundary_case(State& s){
    settings_guest_scope(s);if(s.suite!="TimingBoundaryOnly"||s.left||s.control||!chain_settings(s).enabled)
        throw std::runtime_error("timing_boundary_exact_enabled_released_guest_required");
    for(const int key:std::array{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2,VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN})
        if(GetAsyncKeyState(key)&0x8000)throw std::runtime_error("timing_boundary_initial_keys_not_released");
    timing_boundary_detail::mapped_origin(s);
    timing_boundary_detail::stop_boundary(s,true);
    timing_boundary_detail::stop_boundary(s,false);
    s.log.emit("timing_boundary_component","\"result\":\"PASS\",\"real_mapped_hook_and_public_missing_origin\":true,"
        "\"last_issued_stop_completion_in_final_summary\":true,\"held_unknown_and_unfinished_preserved\":true,"
        "\"NF02_whole_gate\":false,\"latency_budget_PASS\":false,\"I11\":\"NOT_RUN\",\"human_uat\":\"NOT_RUN\"");
}
