#pragma once

// Include inside the guarded fixture's unnamed namespace AFTER the existing
// layout/settings case headers. No product constraint override is introduced:
// Frame.grid is this test application's actual WINDOWPOSCHANGING policy.

std::vector<std::string> constraint_generation_placements(State& s,std::size_t offset,std::uint64_t generation){
    const auto bytes=read(s.product_log);std::vector<std::string> result;if(offset>=bytes.size())return result;
    std::istringstream rows(bytes.substr(offset));std::string row;
    while(std::getline(rows,row))if(row.ends_with('}')&&row.find("\"type\":\"placement\"")!=std::string::npos&&
        number(row,"\"generation\":")==generation)result.push_back(std::move(row));
    return result;
}
std::uint64_t constraint_last_product_sequence(State& s){
    std::istringstream rows(read(s.product_log));std::string row;std::uint64_t sequence{};
    while(std::getline(rows,row))if(row.ends_with('}')&&number(row,"\"process_id\":")==s.product_pid)
        sequence=std::max(sequence,number(row,"\"sequence\":"));return sequence;
}
bool constraint_current_status_after(State& s,std::size_t offset,std::uint64_t sequence){
    const auto status=record(s,offset,"status");
    return number(status,"\"process_id\":")==s.product_pid&&number(status,"\"sequence\":")>sequence;
}
bool constraint_existing_fixed_case(State& s){
    std::istringstream rows(read(s.log.path));std::string row;
    while(std::getline(rows,row))if(row.find("\"type\":\"case\"")!=std::string::npos&&
        row.find("\"case\":\"shared_fixed_size_neighbour_stops_resize_without_placement\"")!=std::string::npos&&
        row.find("\"result\":\"PASS\"")!=std::string::npos)return true;
    return false;
}
[[maybe_unused]] void owned_native_grid_constraint_case(State& s){
    settings_guest_scope(s);const auto config=settings_guest_configuration(s);
    if(!config.enabled||!config.glue||config.shake||config.drag_transparency||s.left||s.control)
        throw std::runtime_error("I10_grid_requires_enabled_idle_Glue_without_optional_visual_mutations");
    // Reuse the one already-executed fixed min=max case, not a second matrix.
    // Its actual driver record is required; a caller-supplied bool is not proof.
    const bool fixed_observed=constraint_existing_fixed_case(s);
    s.log.emit("I10_fixed_constraint_reuse","\"status\":"+quote(fixed_observed?"OBSERVED_PRIOR_CASE_PASS":"NOT_RUN")+
        ",\"actual_case\":\"shared_fixed_size_neighbour_stops_resize_without_placement\",\"new_fixed_case_executed\":false");

    LayoutFixtures set{s};set.create();
    const Rect initial{140,140,540,540};set.setup({initial},{{120,100,1000,1000}});
    auto& frame=set.frames[0];RECT native_before{};const auto visible_before=visible(frame.hwnd);
    if(!set.owns(0)||!visible_before||*visible_before!=initial||!GetWindowRect(frame.hwnd,&native_before))
        throw std::runtime_error("I10_grid_initial_native_frame_unavailable");
    const LONG frame_width=native_before.right-native_before.left-initial.width();
    const LONG frame_height=native_before.bottom-native_before.top-initial.height();
    constexpr LONG grid=17;LONG travel=37;
    const LONG initial_native_width=native_before.right-native_before.left;
    const LONG initial_native_height=native_before.bottom-native_before.top;
    if((initial_native_width+travel)%grid==0)++travel;
    // Independent expected set: the test WndProc quantizes POSITIONING cx/cy,
    // not visible dimensions. Values are fixed before sending the gesture;
    // neither the product's plan nor its returned actual builds the oracle.
    const LONG requested_native_width=initial_native_width+travel;
    const LONG expected_native_width=std::max(frame.min_width,(requested_native_width/grid)*grid);
    const LONG expected_native_height=std::max(frame.min_height,(initial_native_height/grid)*grid);
    const Rect requested{initial.l,initial.t,initial.r+travel,initial.b};
    const Rect constrained{initial.l,initial.t,initial.l+expected_native_width-frame_width,initial.t+expected_native_height-frame_height};
    if(constrained==requested||constrained.width()<=0||constrained.height()<=0)
        throw std::runtime_error("I10_grid_independent_nonexact_expectation_invalid");
    if(!set.set_grid(0,grid))throw std::runtime_error("I10_grid_owner_UI_update_failed");
    s.log.emit("I10_native_grid_policy","\"native_grid\":17,\"policy_actor\":\"exact_owned_test_application\",\"policy_units\":\"positioning_size\"," 
        "\"requested_visible\":"+rect(requested)+",\"independent_expected_visible\":"+rect(constrained)+
        ",\"requested_positioning_size\":["+std::to_string(requested_native_width)+","+std::to_string(initial_native_height)+
        "],\"expected_positioning_size\":["+std::to_string(expected_native_width)+","+std::to_string(expected_native_height)+
        "],\"product_grid_override\":false");
    const auto g=layout_down(set,0,HTRIGHT,false);Sleep(60);cursor(s,{g.anchor.x+travel,g.anchor.y});
    const auto attempted=[](const std::vector<std::string>& rows){return static_cast<unsigned>(std::count_if(rows.begin(),rows.end(),[](const auto& row){
        return row.find("\"native_attempted\":false")==std::string::npos&&row.find("\"native_callsite_claimed\":true")!=std::string::npos;}));};
    std::string receipt,first_end;bool pre_native_focus_lost=false,grid_observed=false;
    (void)wait([&]{
        const auto rows=constraint_generation_placements(s,g.offset,g.generation);
        for(const auto& row:rows){
            if(row.find("\"native_attempted\":true")!=std::string::npos&&row.find("\"native_completion_known\":true")!=std::string::npos&&
               row.find("\"native_succeeded\":true")!=std::string::npos&&row.find("\"identity_valid\":true")!=std::string::npos&&
               row.find("\"exact\":false")!=std::string::npos&&row.find("\"target\":"+rect(requested))!=std::string::npos&&
               row.find("\"actual\":"+rect(constrained))!=std::string::npos&&
               row.find("native_geometry_not_exact_constraints_unattributed")!=std::string::npos)receipt=row;
        }
        first_end=record(s,g.offset,"gesture_end",g.generation);
        grid_observed=!receipt.empty()&&visible(frame.hwnd)==constrained&&first_end.find("\"normal_up\":false")!=std::string::npos;
        pre_native_focus_lost=receipt.empty()&&attempted(rows)==0&&set.owns(0)&&visible(frame.hwnd)==initial&&
            number(first_end,"\"end\":")==static_cast<std::uint64_t>(panebind::core::v1::End::FocusLost)&&
            first_end.find("\"normal_up\":false")!=std::string::npos&&first_end.find("\"pending_retired\":true")!=std::string::npos;
        return grid_observed||pre_native_focus_lost;
    });
    const std::string grid_fields="\"generation\":"+std::to_string(g.generation)+",\"actual\":"+(visible(frame.hwnd)?rect(*visible(frame.hwnd)):"null")+
        ",\"observed_native_result\":"+(grid_observed?quote("SetWindowPos_returned_true_but_geometry_nonexact"):"null")+
        ",\"grid_native_status\":"+quote(pre_native_focus_lost?"NOT_RUN_PRE_NATIVE_FOCUS_LOST":grid_observed?"OBSERVED":"NOT_OBSERVED")+
        ",\"actual_end\":"+(first_end.empty()?"null":quote(first_end))+",\"not_arbitrary_app_grid_inference\":true";
    if(pre_native_focus_lost){s.log.failed=true;s.log.emit("case","\"case\":\"I10_native_grid_nonexact_rejected_not_false_API_failure\",\"result\":\"FAIL\","+grid_fields);}
    else s.log.check("I10_native_grid_nonexact_rejected_not_false_API_failure",grid_observed,grid_fields);
    const auto before_more=constraint_generation_placements(s,g.offset,g.generation);
    const unsigned expected_calls=pre_native_focus_lost?0u:1u;
    if(pre_native_focus_lost)s.log.emit("case","\"case\":\"I10_native_grid_single_attempt_before_test_UP\",\"result\":\"NOT_RUN\",\"actual_native_calls\":0,\"initial_FAIL_preserved\":true");
    else s.log.check("I10_native_grid_single_attempt_before_test_UP",attempted(before_more)==1&&s.left&&actual_up(s,g.offset,g.generation).empty()&&
        receipt.find("\"authority_after\":true")!=std::string::npos,
        "\"driver_left_outstanding\":"+std::string(s.left?"true":"false")+",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+
        ",\"last_native_authority_after\":"+(receipt.find("\"authority_after\":true")!=std::string::npos?"true":receipt.find("\"authority_after\":false")!=std::string::npos?"false":"null")+
        ",\"physical_button_held_not_inferred\":true");
    // A later real cursor input plus a fresh ordinary --status response checks
    // the bounded observation interval and independent product responsiveness.
    // The status sequence is NOT a common native clock/order with mouse input,
    // and does not claim that every native queue was drained.
    const auto barrier=constraint_last_product_sequence(s);cursor(s,{g.anchor.x+travel+53,g.anchor.y+11});
    const auto status_offset=read(s.product_log).size();
    DWORD status_exit{};s.log.check("I10_after_abort_status_command",launch(s,L"--status",true,&status_exit)&&status_exit==0);
    s.log.check("I10_after_abort_status_observed",wait([&]{return constraint_current_status_after(s,status_offset,barrier);}));
    // Read-only routing facts at the ORIGINAL UP point. Do not move, activate
    // a window, alter integrity, or insert another UP to improve this result.
    POINT up_cursor{};SetLastError(0);const bool cursor_known=GetCursorPos(&up_cursor)!=FALSE;const DWORD cursor_error=cursor_known?0:GetLastError();
    const HWND up_hit=cursor_known?WindowFromPoint(up_cursor):nullptr,up_root=up_hit?GetAncestor(up_hit,GA_ROOT):nullptr,up_foreground=GetForegroundWindow();
    DWORD up_hit_pid{},up_root_pid{};const DWORD up_hit_tid=up_hit?GetWindowThreadProcessId(up_hit,&up_hit_pid):0,up_root_tid=up_root?GetWindowThreadProcessId(up_root,&up_root_pid):0;
    const auto up_hit_class=up_hit?gui_class(up_hit):std::wstring{},up_root_class=up_root?gui_class(up_root):std::wstring{};
    int up_root_rid=-1;bool up_root_elevated{},up_root_medium{},up_root_identity{};DWORD up_root_token_error{};
    if(up_root_pid){SetLastError(0);Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,up_root_pid));
        if(!process.value)up_root_token_error=GetLastError();else{
            up_root_medium=medium(process.value,&up_root_rid,&up_root_elevated,&up_root_token_error);DWORD current_pid{};
            up_root_identity=WaitForSingleObject(process.value,0)==WAIT_TIMEOUT&&IsWindow(up_root)&&
                GetWindowThreadProcessId(up_root,&current_pid)==up_root_tid&&current_pid==up_root_pid;
            if(!up_root_identity)up_root_rid=-1;
        }
    }
    s.log.emit("I10_before_original_test_UP_routing","\"GetCursorPos_succeeded\":"+std::string(cursor_known?"true":"false")+
        ",\"GetCursorPos_error\":"+std::to_string(cursor_error)+",\"actual_cursor\":"+(cursor_known?"["+std::to_string(up_cursor.x)+","+std::to_string(up_cursor.y)+"]":"null")+
        ",\"hit_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(up_hit))+",\"hit_PID\":"+std::to_string(up_hit_pid)+",\"hit_TID\":"+std::to_string(up_hit_tid)+
        ",\"hit_class\":"+(up_hit_class.empty()?"null":quote(utf8(up_hit_class)))+",\"root_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(up_root))+
        ",\"root_PID\":"+std::to_string(up_root_pid)+",\"root_TID\":"+std::to_string(up_root_tid)+",\"root_class\":"+(up_root_class.empty()?"null":quote(utf8(up_root_class)))+
        ",\"hit_root_is_exact_source\":"+(set.owns(0)&&up_root==frame.hwnd?"true":"false")+",\"actual_foreground\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(up_foreground))+",\"foreground_is_exact_source\":"+(set.owns(0)&&up_foreground==frame.hwnd?"true":"false")+
        ",\"root_identity_still_current\":"+(up_root_identity?"true":"false")+",\"root_integrity_rid\":"+(up_root_rid<0?"null":std::to_string(up_root_rid))+
        ",\"root_elevated\":"+(up_root_rid<0?"null":up_root_elevated?"true":"false")+",\"root_medium_non_elevated\":"+(up_root_rid<0?"null":up_root_medium?"true":"false")+
        ",\"root_token_query_error\":"+std::to_string(up_root_token_error)+",\"original_trajectory_unchanged\":true,\"routing_cause_not_inferred\":true");
    button(s,false);
    std::string observed_up,observed_end;GUITHREADINFO source_gui{sizeof(source_gui)};bool source_gui_known{},driver_released{};DWORD source_gui_error{};
    const bool up_and_context=wait([&]{
        // Read EVERY independent fact even when actual UP is still missing.
        // Otherwise && short circuit would turn an absent receipt into an
        // unsupported statement about the native source thread's GUI state.
        driver_released=!s.left;observed_up=actual_up(s,g.offset,g.generation);observed_end=record(s,g.offset,"gesture_end",g.generation);
        source_gui={sizeof(source_gui)};SetLastError(0);source_gui_known=GetGUIThreadInfo(set.thread,&source_gui)!=FALSE;
        source_gui_error=source_gui_known?0:GetLastError();
        return driver_released&&!observed_up.empty()&&source_gui_known&&!source_gui.hwndCapture&&!source_gui.hwndMoveSize;
    });
    const bool ordinary_abort=observed_end.find("\"normal_up\":false")!=std::string::npos;std::string after_UP_status;bool snapshot_requested{},snapshot_observed{};DWORD snapshot_exit=STILL_ACTIVE;
    if(!up_and_context||!ordinary_abort||pre_native_focus_lost){const auto failed_up_mark=read(s.product_log).size();
        snapshot_requested=launch(s,L"--status",true,&snapshot_exit);
        snapshot_observed=wait([&]{after_UP_status=record(s,failed_up_mark,"status");return !after_UP_status.empty();},1000);
    }
    const std::string up_fields="\"actual_UP\":"+(observed_up.empty()?"null":quote(observed_up))+",\"driver_left_released_ledger\":"+(driver_released?"true":"false")+
        ",\"source_GUI_known\":"+(source_gui_known?"true":"false")+",\"source_GUI_query_error\":"+std::to_string(source_gui_error)+
        ",\"actual_capture\":"+(source_gui_known?std::to_string(reinterpret_cast<std::uintptr_t>(source_gui.hwndCapture)):"null")+
        ",\"actual_move_size\":"+(source_gui_known?std::to_string(reinterpret_cast<std::uintptr_t>(source_gui.hwndMoveSize)):"null")+
        ",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+",\"after_UP_status_requested\":"+(snapshot_requested?"true":"false")+
        ",\"after_UP_status_command_exit\":"+std::to_string(snapshot_exit)+",\"after_UP_status_observed\":"+(snapshot_observed?"true":"false")+
        ",\"after_UP_status_with_mouse_up_callbacks\":"+(after_UP_status.empty()?"null":quote(after_UP_status));
    if(up_and_context&&ordinary_abort)s.log.check("I10_abort_real_UP_not_NormalUp",true,up_fields);
    else{s.log.failed=true;s.log.emit("case","\"case\":\"I10_abort_real_UP_not_NormalUp\",\"result\":\"FAIL\","+up_fields);}
    const Rect required_actual=pre_native_focus_lost?initial:constrained;const auto recovery_actual=visible(frame.hwnd);
    const bool retired=ordinary_abort&&observed_end.find("\"pending_retired\":true")!=std::string::npos;
    const bool no_new_calls=attempted(constraint_generation_placements(s,g.offset,g.generation))==expected_calls;
    s.log.check("I10_retired_generation_no_new_placement_actual_state",retired&&no_new_calls&&set.owns(0)&&recovery_actual&&*recovery_actual==required_actual,
        "\"expected_native_calls\":"+std::to_string(expected_calls)+",\"grid_native_observed\":"+(grid_observed?"true":"false")+
        ",\"expected_actual_branch\":"+rect(required_actual)+",\"actual\":"+(recovery_actual?rect(*recovery_actual):"null"));
    if(pre_native_focus_lost||!up_and_context){
        s.log.check("I10_failed_subcase_safe_for_new_legal_gesture",driver_released&&source_gui_known&&!source_gui.hwndCapture&&!source_gui.hwndMoveSize&&!source_gui.hwndMenuOwner&&
            snapshot_requested&&snapshot_observed&&snapshot_exit==0&&after_UP_status.find("\"gesture_active\":false")!=std::string::npos&&
            after_UP_status.find("\"hook_owner_running\":true")!=std::string::npos&&retired&&no_new_calls,
            "\"missing_original_UP_not_relabelled_PASS\":true,\"initial_case_FAIL_preserved\":true,\"fresh_status\":"+quote(after_UP_status));
    }

    if(!set.set_grid(0,1))throw std::runtime_error("I10_grid_owner_UI_reset_failed");
    s.log.emit("I10_test_application_grid_reset","\"from\":17,\"to\":1,\"only_after_single_test_UP_sent\":true,\"actual_matching_UP_observed\":"+
        std::string(observed_up.empty()?"false":"true")+",\"actor\":\"test_owned_app\",\"geometry_reset\":false");
    // Recovery starts at the verified actual rectangle: original after a
    // pre-native FocusLost, constrained only after the real nonexact call.
    // No product restart, source replacement or geometry reset hides failure.
    const auto next=layout_down(set,0,HTCAPTION,false);const auto moved=translated(*recovery_actual,31,29);
    s.log.check("I10_post_grid_new_generation",next.generation>g.generation);
    layout_phase(set,0,next,31,29,{moved},"I10_post_grid_actual_ordinary_Move");layout_up(set,next,{moved});
    const Rect neighbour{moved.r,moved.t,moved.r+300,moved.b};
    set.setup({moved,neighbour}); // Source is placed at its SAME actual rect.
    const auto ctrl=layout_down(set,1,HTCAPTION,true);const std::vector<Rect> group{translated(moved,29,23),translated(neighbour,29,23)};
    s.log.check("I10_post_grid_Ctrl_new_generation",ctrl.generation>next.generation);
    layout_phase(set,1,ctrl,29,23,group,"I10_post_grid_actual_Ctrl_source_and_follower");layout_up(set,ctrl,group);
    s.log.check("I10_grid_owned_resources_destroyed",set.close(),"\"guest_destruction_is_not_cleanup\":true");
    s.log.emit("I10_native_constraint_scope","\"status\":\"PARTIAL\",\"one_native_grid_nonexact_and_recovery_observed\":"+std::string(grid_observed?"true":"false")+
        ",\"original_UP_observed\":"+(!observed_up.empty()?"true":"false")+",\"pre_native_FocusLost_FAIL_preserved\":"+(pre_native_focus_lost?"true":"false")+
        ",\"fixed_size_prior_actual_case_observed\":"+(fixed_observed?"true":"false")+
        ",\"second_member_SetWindowPos_FALSE\":\"SEPARATE_CASE\","
        "\"grid_nonexact_not_relabelled_as_API_FALSE\":true,\"whole_I10_PASS\":false,\"human_uat\":\"NOT_RUN\"");
}
