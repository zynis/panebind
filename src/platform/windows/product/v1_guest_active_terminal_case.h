#pragma once

// TEST ONLY. Reach the real native-first enhanced session before inserting
// one MOVE/UP array. UP may legitimately retire a MOVE which was not issued.
void owned_active_terminal_burst_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if(s.suite!="FixedAndTerminalOnly"||!original.enabled||s.left||s.control||binary.empty()||binary!=hash(s.input_product))
        throw std::runtime_error("active_terminal_exact_idle_guest_suite_required");
    auto config=original;config.glue=false;config.magnet_move=false;config.magnet_resize=false;config.docking=false;
    config.preview=false;config.feedback=false;config.drag_transparency=false;config.shake=false;
    const auto applied=chain_gui_save(s,config);LayoutFixtures set{s};
    try{
        set.create();const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<1000||work.height()<700)throw std::runtime_error("active_terminal_owned_work_area_too_small");
        const Rect initial{work.l+240,work.t+180,work.l+640,work.t+480};
        set.setup({initial},{{120,100,work.width(),work.height()}});
        const HWND source=set.frames[0].hwnd;const auto starts=set.frames[0].starts.load(),ends=set.frames[0].ends.load();
        const auto g=layout_down(set,0,HTCAPTION,false);chain_check_product(s,pid,birth);
        const auto row_for=[&](std::string_view row,std::string_view type){
            return geometry_history_receipt(s,row,type)&&e::number(row,"\"generation\":")==g.generation;
        };
        const auto fact=[&](std::string_view name){
            std::string found,row;std::istringstream rows(product_log_since(s,g.offset));
            while(std::getline(rows,row))if(row_for(row,"gesture_input_resource")&&e::quoted(row,"\"fact\":",name)&&
                (name!="raw_mouse"||(e::number(row,"\"raw_button_flags\":").value_or(0)&RI_MOUSE_LEFT_BUTTON_UP)))found=row;
            return found;
        };
        const auto active_mark=read_product(s).size();std::string active_status;
        const bool active_requested=launch(s,L"--status",true);
        const bool active_observed=active_requested&&wait([&]{active_status=record(s,active_mark,"status");return
            geometry_history_receipt(s,active_status,"status")&&e::unique_value(active_status,"\"enabled\":")=="true"&&
            e::unique_value(active_status,"\"hook_owner_running\":")=="true"&&e::unique_value(active_status,"\"gesture_active\":")=="true"&&
            e::unique_value(active_status,"\"native_operation_pending\":")=="false"&&e::unique_value(active_status,"\"native_completion_unknown\":")=="false";});
        const auto start=record(s,g.offset,"gesture_start",g.generation),pending=record(s,g.offset,"native_move_pending",g.generation);
        const auto native_start=record(s,g.offset,"native_move_start",g.generation),native_end=record(s,g.offset,"native_move_end",g.generation);
        const auto cancel=record(s,g.offset,"native_move_cancel",g.generation),handoff=record(s,g.offset,"native_move_handoff",g.generation);
        const auto ready=fact("capture_ready");const auto handoff_actual=visible(source);
        const auto key=e::number(start,"\"key\":"),calls_before=e::number(active_status,"\"native_callsite_claims\":");
        const auto old_up_count=e::number(active_status,"\"callback_count\":");
        unsigned prefix_placements{},cancel_attempts{};std::string row;std::istringstream prefix(product_log_since(s,g.offset));
        while(std::getline(prefix,row)){
            if(row_for(row,"placement"))++prefix_placements;
            if(row_for(row,"native_move_cancel")&&e::unique_value(row,"\"native_attempted\":")=="true")++cancel_attempts;
        }
        const auto source_id=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(source));
        const auto native_start_qpc=set.frames[0].native_start_qpc.load(),native_end_qpc=set.frames[0].native_end_qpc.load();
        const bool ready_before_burst=active_observed&&key&&*key&&calls_before&&old_up_count&&handoff_actual&&set.owns(0)&&
            row_for(start,"gesture_start")&&e::unique_value(start,"\"native_first_handoff\":")=="true"&&
            e::unique_value(start,"\"resize\":")=="false"&&e::unique_value(start,"\"ctrl\":")=="false"&&e::number(start,"\"group_members\":")==1&&
            start.find("\"original\":"+rect(initial)+",")!=start.npos&&
            row_for(pending,"native_move_pending")&&e::number(pending,"\"source\":")==source_id&&e::number(pending,"\"source_key\":")==key&&
            e::number(pending,"\"source_pid\":")==GetCurrentProcessId()&&e::number(pending,"\"source_tid\":")==set.thread&&
            pending.find("\"down_point\":["+std::to_string(g.anchor.x)+","+std::to_string(g.anchor.y)+"],")!=pending.npos&&
            row_for(native_start,"native_move_start")&&row_for(native_end,"native_move_end")&&
            e::number(native_start,"\"source\":")==source_id&&e::number(native_end,"\"source\":")==source_id&&
            e::number(native_start,"\"event\":")==EVENT_SYSTEM_MOVESIZESTART&&e::number(native_end,"\"event\":")==EVENT_SYSTEM_MOVESIZEEND&&
            e::number(native_end,"\"owner_sequence\":").value_or(0)>e::number(native_start,"\"owner_sequence\":").value_or(UINT64_MAX)&&
            set.frames[0].starts.load()==starts+1&&set.frames[0].ends.load()==ends+1&&native_start_qpc&&native_end_qpc>native_start_qpc&&
            row_for(cancel,"native_move_cancel")&&cancel_attempts==1&&e::unique_value(cancel,"\"api_returned_success\":")=="true"&&
            e::number(cancel,"\"before_qpc\":").value_or(0)>=native_start_qpc&&e::number(cancel,"\"before_qpc\":").value_or(UINT64_MAX)<=native_end_qpc&&
            row_for(handoff,"native_move_handoff")&&e::unique_value(handoff,"\"accepted\":")=="true"&&
            handoff.find("\"intent_anchor\":"+rect(initial)+",")!=handoff.npos&&
            handoff.find("\"continuity_baseline\":"+rect(*handoff_actual)+",")!=handoff.npos&&
            row_for(ready,"gesture_input_resource")&&e::quoted(ready,"\"entry_mode\":","native_move_handoff")&&
            e::number(ready,"\"source\":")==source_id&&e::number(ready,"\"source_pid\":")==GetCurrentProcessId()&&
            e::number(ready,"\"source_tid\":")==set.thread&&e::unique_value(ready,"\"attach_succeeded\":")=="true"&&
            e::unique_value(ready,"\"capture_attempted\":")=="true"&&e::number(ready,"\"capture_failure\":")==0&&
            e::number(ready,"\"capture_actual_after\":")==e::number(ready,"\"overlay\":")&&
            e::number(ready,"\"resource_observed_qpc\":").value_or(0)>=native_end_qpc&&
            !prefix_placements&&actual_up(s,g.offset,g.generation).empty()&&s.left&&!s.left_up_attempted;
        s.log.check("active_terminal_actual_enhanced_handoff_before_burst",ready_before_burst,
            "\"generation\":"+std::to_string(g.generation)+",\"original_intent_rect\":"+rect(initial)+
            ",\"actual_handoff_rect\":"+(handoff_actual?rect(*handoff_actual):"null")+",\"prefix_placements\":"+std::to_string(prefix_placements)+
            ",\"actual_START\":"+quote(native_start)+",\"actual_cancel\":"+quote(cancel)+",\"actual_END\":"+quote(native_end)+
            ",\"actual_handoff\":"+quote(handoff)+",\"actual_capture_ready\":"+quote(ready));
        const Rect held=*handoff_actual;const POINT destination{g.anchor.x+67,g.anchor.y+29};const Rect target=translated(initial,67,29);
        if(target.l<work.l||target.t<work.t||target.r>work.r||target.b>work.b||target==held)
            throw std::runtime_error("active_terminal_independent_destination_not_distinct_and_bounded");
        const int x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if(width<2||height<2||destination.x<x||destination.y<y||destination.x>=x+width||destination.y>=y+height)
            throw std::runtime_error("active_terminal_destination_outside_actual_virtual_screen");
        GuestUpHookObservation head;s.log.check("active_terminal_passive_burst_observer_ready",head.start());
        POINT before_point{};
        if(!desktop()||!GetCursorPos(&before_point)||!set.owns(0)||GetForegroundWindow()!=source||visible(source)!=held||
            !(GetAsyncKeyState(VK_LBUTTON)&0x8000)||s.control||!s.left||s.left_up_attempted)
            throw std::runtime_error("active_terminal_actual_context_changed_before_array");
        const HWND hit_root=GetAncestor(WindowFromPoint(before_point),GA_ROOT);
        const auto capture=release_capture_facts(s,source,hit_root,g.generation);
        if(!product::guest::captured_release_hit_allowed(capture))throw std::runtime_error("active_terminal_fresh_exact_capture_required");
        std::array<INPUT,2> packets{};for(auto& packet:packets){packet.type=INPUT_MOUSE;packet.mi.dwExtraInfo=0x50425631;}
        packets[0].mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
        packets[0].mi.dx=static_cast<LONG>(((static_cast<long long>(destination.x-x)*2+1)*65536)/(static_cast<long long>(width)*2));
        packets[0].mi.dy=static_cast<LONG>(((static_cast<long long>(destination.y-y)*2+1)*65536)/(static_cast<long long>(height)*2));
        packets[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
        LARGE_INTEGER before{},after{};if(!QueryPerformanceCounter(&before)||before.QuadPart<=0)throw std::runtime_error("active_terminal_burst_clock_unknown");
        SetLastError(0);const auto inserted=SendInput(static_cast<UINT>(packets.size()),packets.data(),sizeof(INPUT));
        const DWORD insertion_error=inserted==packets.size()?0:GetLastError();const bool after_known=QueryPerformanceCounter(&after)!=FALSE&&after.QuadPart>=before.QuadPart;
        // One UP was attempted even on partial insertion. The outer cleanup's
        // existing same-DOWN guard must not retransmit it or claim delivery.
        s.left=inserted!=packets.size();s.left_up_attempted=true;s.left_up_send_qpc=static_cast<std::uint64_t>(before.QuadPart);
        s.log.check("active_terminal_single_MOVE_UP_array_inserted",inserted==packets.size()&&after_known,
            "\"generation\":"+std::to_string(g.generation)+",\"packet_count\":2,\"SendInput_return\":"+std::to_string(inserted)+
            ",\"win32_error\":"+std::to_string(insertion_error)+",\"before_qpc\":"+std::to_string(before.QuadPart)+
            ",\"after_qpc\":"+(after_known?std::to_string(after.QuadPart):"null")+",\"interpacket_wait_or_log\":false,\"delivery_inferred\":false");
        const product::guest::UpEvidenceIdentity up_identity{pid,s.product_source,g.generation,s.left_up_send_qpc};
        std::string ended,legacy,gone,raw,observed_up;
        const bool normal=wait([&]{ended=record(s,g.offset,"gesture_end",g.generation);legacy=fact("legacy_left_up");gone=fact("isolation_gone");
            raw=fact("raw_mouse");observed_up=actual_up(s,g.offset,g.generation);
            return row_for(ended,"gesture_end")&&e::unique_value(ended,"\"normal_up\":")=="true"&&e::unique_value(ended,"\"pending_retired\":")=="true"&&
                product::guest::matching_clean_captured_up(legacy,gone,up_identity)&&e::unique_value(gone,"\"hotkey_unregistered\":")=="true"&&
                row_for(raw,"gesture_input_resource")&&(e::number(raw,"\"raw_button_flags\":").value_or(0)&RI_MOUSE_LEFT_BUTTON_UP)&&
                e::number(raw,"\"resource_observed_qpc\":").value_or(0)>=s.left_up_send_qpc&&
                row_for(observed_up,"input_button_observed")&&e::quoted(observed_up,"\"source\":","shield_queued_legacy")&&e::number(observed_up,"\"source_key\":")==key;
        },5000);
        const bool head_stopped=head.stop();const auto observation=head.json();
        const auto fixed=[&](UINT message){
            const auto token="\"native_message\":"+std::to_string(message)+",";const auto at=observation.find(token);
            if(at==observation.npos||observation.find(token,at+token.size())!=observation.npos)return std::string{};
            const auto end=observation.find('}',at);return end==observation.npos?std::string{}:observation.substr(at,end-at+1);
        };
        const auto move_seen=fixed(WM_MOUSEMOVE),up_seen=fixed(WM_LBUTTONUP);
        const auto move_qpc=e::number(move_seen,"\"last_entry_qpc\":"),move_return=e::number(move_seen,"\"last_CallNext_return_qpc\":");
        const auto head_up=e::number(up_seen,"\"last_entry_qpc\":"),head_up_return=e::number(up_seen,"\"last_CallNext_return_qpc\":");
        s.log.check("active_terminal_actual_single_burst_and_NormalUp_resources",normal&&head_stopped&&
            e::number(move_seen,"\"callback_count\":")==1&&e::number(up_seen,"\"callback_count\":")==1&&
            move_qpc&&move_return&&head_up&&head_up_return&&*move_qpc>=s.left_up_send_qpc&&*move_return>=*move_qpc&&
            *head_up>*move_qpc&&*head_up_return>=*head_up&&e::number(observation,"\"last_extra_information\":")==0x50425631&&
            e::unique_value(observation,"\"CallNext_returned\":")=="true",
            "\"generation\":"+std::to_string(g.generation)+",\"actual_UP\":"+quote(observed_up)+",\"actual_Raw_UP\":"+quote(raw)+
            ",\"actual_legacy_UP\":"+quote(legacy)+",\"actual_resource_gone\":"+quote(gone)+",\"actual_end\":"+quote(ended)+
            ",\"passive_head\":"+observation+",\"passive_observer_is_product_permission\":false");
        // NormalUp was consumed by the GUI, but an UP-before-completion call
        // may still be in flight. First obtain a fresh command-consumption
        // boundary without requiring idle. Its sequence after gesture_end
        // makes the complete preceding placement prefix observable; zero
        // rows at an arbitrary file-read instant would not prove zero calls.
        const auto terminal_mark=read_product(s).size(),end_sequence=e::number(ended,"\"sequence\":").value_or(0);
        std::string terminal_status;const bool terminal_requested=launch(s,L"--status",true);
        const bool terminal_consumed=terminal_requested&&end_sequence&&wait([&]{terminal_status=record(s,terminal_mark,"status");
            return geometry_history_receipt(s,terminal_status,"status")&&e::number(terminal_status,"\"sequence\":").value_or(0)>end_sequence&&
                e::unique_value(terminal_status,"\"enabled\":")=="true"&&e::unique_value(terminal_status,"\"hook_owner_running\":")=="true"&&
                e::unique_value(terminal_status,"\"gesture_active\":")=="false"&&e::unique_value(terminal_status,"\"native_move_handoff_pending\":")=="false";
        });
        s.log.check("active_terminal_GUI_terminal_receipt_boundary",terminal_consumed,
            "\"end_sequence\":"+std::to_string(end_sequence)+",\"actual_status\":"+quote(terminal_status)+",\"idle_not_yet_required\":true");
        std::uint64_t callback_count{},up_qpc{},up_generation{};
        const std::string_view callback_key="\"mouse_up_callbacks\":";const auto at=terminal_status.find(callback_key);
        bool callback_exact=false;
        if(at!=terminal_status.npos&&terminal_status.find(callback_key,at+callback_key.size())==terminal_status.npos){
            e::Cursor callback{std::string_view(terminal_status).substr(at+callback_key.size())};
            callback_exact=callback.take("{\"snapshot_known\":true,\"callback_count\":")&&callback.number(callback_count)&&callback_count==*old_up_count+1&&
                callback.take(",\"last\":{\"hook_observed_qpc\":")&&callback.number(up_qpc)&&up_qpc>=*head_up&&up_qpc<=*head_up_return&&
                callback.take(",\"button\":1,\"generation_at_entry\":")&&callback.number(up_generation)&&up_generation==g.generation&&
                callback.take(",\"consumed_before\":false,\"aborted_before\":false,\"matching_branch_taken\":true,\"push_attempted\":false,\"push_succeeded\":false}}}")&&callback.text.empty();
        }
        s.log.check("active_terminal_exact_product_UP_callback_not_later_GUI_UP",callback_exact,
            "\"generation\":"+std::to_string(g.generation)+",\"product_callback_UP_qpc\":"+std::to_string(up_qpc)+
            ",\"actual_status\":"+quote(terminal_status)+",\"callback_snapshot_not_NormalUp_by_itself\":true");
        const auto raw_qpc=e::number(raw,"\"resource_observed_qpc\":");const auto barrier=std::min(up_qpc,*raw_qpc);
        unsigned claims{},placements{};bool valid=true;std::string issued,completion,old_placements;std::uint64_t issued_id{};
        std::istringstream rows(product_log_since(s,g.offset));
        while(std::getline(rows,row))if(row_for(row,"placement")){
            ++placements;old_placements+=row+'\n';valid=valid&&e::number(row,"\"key\":")==key&&
                e::unique_value(row,"\"normal_up_completion\":")=="false"&&row.find("\"target\":"+rect(target)+",")!=row.npos;
            const auto sampled=e::number(row,"\"hook_observed_qpc\":");
            valid=valid&&sampled&&*sampled>=*move_qpc&&*sampled<=*move_return;
            if(e::unique_value(row,"\"native_callsite_claimed\":")=="true"){
                ++claims;issued=row;issued_id=e::number(row,"\"call_id\":").value_or(0);valid=valid&&issued_id!=0;
            }else valid=valid&&e::unique_value(row,"\"native_callsite_claimed\":")=="false"&&
                e::unique_value(row,"\"native_attempted\":")=="false"&&e::quoted(row,"\"reason\":","operation_revoked_before_native_attempt");
        }
        if(claims==1){
            completion=issued;
            if(e::unique_value(issued,"\"native_completion_known\":")!="true"){
                const bool completed=issued_id&&wait([&]{
                    std::istringstream later(product_log_since(s,g.offset));while(std::getline(later,row))
                        if(geometry_history_receipt(s,row,"native_completion")&&e::number(row,"\"call_id\":")==issued_id&&
                           e::number(row,"\"key\":")==key&&row.find("\"target\":"+rect(target)+",")!=row.npos&&
                           e::unique_value(row,"\"native_completion_known\":")=="true")completion=row;
                    return e::quoted(completion,"\"type\":","native_completion");
                },5000);
                s.log.check("active_terminal_claimed_native_completion_observed_before_idle",completed,
                    "\"generation\":"+std::to_string(g.generation)+",\"call_id\":"+std::to_string(issued_id)+
                    ",\"actual_completion\":"+quote(completion)+",\"new_authority_granted\":false");
            }
            const bool delayed=e::quoted(completion,"\"type\":","native_completion");
            const auto native_before=e::number(completion,"\"native_before_qpc\":"),native_after=e::number(completion,"\"native_after_qpc\":");
            // The issued call's exact completion is distinct from permission
            // to issue another call. Only the product's narrow documented
            // capture-cleanup transition may accompany false post-context;
            // actual NormalUp/release/detach/destruction were checked above.
            const bool post_context=e::unique_value(completion,"\"post_context_valid\":")=="true"||
                (e::unique_value(completion,"\"post_context_valid\":")=="false"&&
                 e::quoted(completion,"\"post_context_reason\":","post_native_capture_cleanup_not_yet_confirmed"));
            valid=valid&&e::number(completion,"\"key\":")==key&&e::unique_value(completion,"\"native_attempted\":")=="true"&&
                e::unique_value(completion,"\"native_completion_known\":")=="true"&&e::unique_value(completion,"\"native_succeeded\":")=="true"&&
                e::unique_value(completion,"\"identity_valid\":")=="true"&&e::unique_value(completion,delayed?"\"geometry_exact_at_delayed_observation\":":"\"exact\":")=="true"&&
                completion.find("\"target\":"+rect(target)+",")!=completion.npos&&
                completion.find(std::string(delayed?"\"delayed_actual\":":"\"actual\":")+rect(target)+",")!=completion.npos&&
                native_before&&native_after&&*native_before>=s.left_up_send_qpc&&*native_before<barrier&&*native_after>=*native_before&&
                post_context&&
                (!delayed||e::unique_value(completion,"\"new_write_authority_granted\":")=="false");
        }
        // Only after an actual claimed call has completed is a one-shot idle
        // status meaningful. The zero-claim branch never waits for a receipt
        // for a native call which was not issued.
        const auto status=geometry_history_status(s,e::number(terminal_status,"\"sequence\":").value_or(0));
        const Rect expected=claims?target:held;const auto calls_after=e::number(status,"\"native_callsite_claims\":");
        POINT actual_cursor{};GUITHREADINFO gui{sizeof(gui)};
        s.log.check("active_terminal_retired_unissued_or_pre_UP_issued_exact_completion",valid&&placements<=1&&claims<=1&&calls_after&&
            *calls_after==*calls_before+claims&&set.owns(0)&&visible(source)==expected&&GetCursorPos(&actual_cursor)&&
            actual_cursor.x==destination.x&&actual_cursor.y==destination.y&&!s.left&&GetGUIThreadInfo(set.thread,&gui)&&
            !gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner,
            "\"generation\":"+std::to_string(g.generation)+",\"branch\":"+quote(claims?"issued_before_UP_actual_completion":"unissued_motion_retired_by_UP")+
            ",\"original_intent_rect\":"+rect(initial)+",\"handoff_validation_rect\":"+rect(held)+",\"independent_target\":"+rect(target)+
            ",\"expected_actual\":"+rect(expected)+",\"native_claims\":"+std::to_string(claims)+",\"placement_receipts\":"+std::to_string(placements)+
            ",\"product_LL_UP_qpc\":"+std::to_string(up_qpc)+",\"Raw_UP_receipt_qpc\":"+std::to_string(*raw_qpc)+
            ",\"observed_UP_barrier_qpc\":"+std::to_string(barrier)+",\"actual_native_receipt\":"+quote(completion)+
            ",\"GUI_lag_forced_or_proven\":false,\"completion_after_UP_is_not_new_issue\":true");
        chain_check_product(s,pid,birth);
        const auto next=layout_down(set,0,HTCAPTION,false);const Rect next_expected=translated(expected,31,17);
        s.log.check("active_terminal_following_generation_is_fresh",next.generation>g.generation,
            "\"retired_generation\":"+std::to_string(g.generation)+",\"new_generation\":"+std::to_string(next.generation));
        layout_phase(set,0,next,31,17,{next_expected},"active_terminal_following_actual_Move_exact");layout_up(set,next,{next_expected});
        (void)geometry_history_status(s);std::string after_old;std::istringstream after_rows(product_log_since(s,g.offset));
        while(std::getline(after_rows,row))if(row_for(row,"placement"))after_old+=row+'\n';
        s.log.check("active_terminal_retired_generation_has_no_late_new_placement",after_old==old_placements&&set.owns(0)&&visible(source)==next_expected,
            "\"retired_generation\":"+std::to_string(g.generation)+",\"following_generation\":"+std::to_string(next.generation));
        const auto restored=chain_gui_save(s,original);auto restored_expected=original;restored_expected.generation=restored.generation;
        s.log.check("active_terminal_original_configuration_restored",restored==restored_expected);chain_check_product(s,pid,birth);
        s.log.check("active_terminal_same_product_binary_and_exact_roots_closed",hash(s.product)==binary&&set.close());
        s.log.emit("active_terminal_burst_component","\"result\":\"PASS\",\"already_enhanced_before_burst\":true,"+
            std::string("\"one_MOVE_then_single_UP\":true,\"unissued_retirement_or_pre_UP_issue_classified\":true,\"actual_following_Move_NormalUp\":true,")+
            "\"unobserved_race_branches_claimed\":false,\"whole_version_PASS\":false,\"human_uat\":\"NOT_RUN\"");
    }catch(...){
        pause_routing_failed_case_input(s);
        try{if(s.process&&!s.left&&!s.control){(void)geometry_history_status(s);if(chain_settings(s)==applied)chain_gui_save(s,original);}}catch(...){}
        try{if(!set.closed)set.close();}catch(...){}throw;
    }
}
