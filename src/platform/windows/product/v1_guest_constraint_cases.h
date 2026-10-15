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
    while(std::getline(rows,row))if(row.ends_with('}'))sequence=std::max(sequence,number(row,"\"sequence\":"));return sequence;
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
    frame.grid=grid; // Same owner thread as the exact newly-created WndProc.
    s.log.emit("I10_native_grid_policy","\"native_grid\":17,\"policy_actor\":\"exact_owned_test_application\",\"policy_units\":\"positioning_size\"," 
        "\"requested_visible\":"+rect(requested)+",\"independent_expected_visible\":"+rect(constrained)+
        ",\"requested_positioning_size\":["+std::to_string(requested_native_width)+","+std::to_string(initial_native_height)+
        "],\"expected_positioning_size\":["+std::to_string(expected_native_width)+","+std::to_string(expected_native_height)+
        "],\"product_grid_override\":false");
    const auto g=layout_down(set,0,HTRIGHT,false);Sleep(60);cursor(s,{g.anchor.x+travel,g.anchor.y});
    std::string receipt;
    const bool rejected=wait([&]{
        for(const auto& row:constraint_generation_placements(s,g.offset,g.generation)){
            if(row.find("\"native_attempted\":true")!=std::string::npos&&row.find("\"native_completion_known\":true")!=std::string::npos&&
               row.find("\"native_succeeded\":true")!=std::string::npos&&row.find("\"identity_valid\":true")!=std::string::npos&&
               row.find("\"exact\":false")!=std::string::npos&&row.find("\"target\":"+rect(requested))!=std::string::npos&&
               row.find("\"actual\":"+rect(constrained))!=std::string::npos&&
               row.find("native_geometry_not_exact_constraints_unattributed")!=std::string::npos)receipt=row;
        }
        return !receipt.empty()&&visible(frame.hwnd)==constrained&&
            record(s,g.offset,"gesture_end",g.generation).find("\"normal_up\":false")!=std::string::npos;
    });
    s.log.check("I10_native_grid_nonexact_rejected_not_false_API_failure",rejected,
        "\"generation\":"+std::to_string(g.generation)+",\"actual\":"+(visible(frame.hwnd)?rect(*visible(frame.hwnd)):"null")+
        ",\"observed_native_result\":\"SetWindowPos_returned_true_but_geometry_nonexact\",\"not_arbitrary_app_grid_inference\":true");
    const auto before_more=constraint_generation_placements(s,g.offset,g.generation);
    const auto attempted=[](const std::vector<std::string>& rows){return static_cast<unsigned>(std::count_if(rows.begin(),rows.end(),[](const auto& row){
        return row.find("\"native_attempted\":false")==std::string::npos&&row.find("\"native_callsite_claimed\":true")!=std::string::npos;}));};
    s.log.check("I10_native_grid_single_attempt_before_revoke",attempted(before_more)==1&&s.left&&(GetAsyncKeyState(VK_LBUTTON)&0x8000));
    // A later real cursor input plus a fresh ordinary --status response checks
    // the bounded observation interval and independent product responsiveness.
    // The status sequence is NOT a common native clock/order with mouse input,
    // and does not claim that every native queue was drained.
    const auto barrier=constraint_last_product_sequence(s);cursor(s,{g.anchor.x+travel+53,g.anchor.y+11});
    DWORD status_exit{};s.log.check("I10_after_abort_status_command",launch(s,L"--status",true,&status_exit)&&status_exit==0);
    s.log.check("I10_after_abort_status_observed",wait([&]{return number(record(s,0,"status"),"\"sequence\":")>barrier;}));
    button(s,false);
    s.log.check("I10_abort_real_UP_not_NormalUp",wait([&]{GUITHREADINFO info{sizeof(info)};return !s.left&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&
        GetGUIThreadInfo(set.thread,&info)&&!info.hwndCapture;})&&
        record(s,g.offset,"gesture_end",g.generation).find("\"normal_up\":false")!=std::string::npos);
    s.log.check("I10_no_grid_retry_or_new_placement_after_abort",attempted(constraint_generation_placements(s,g.offset,g.generation))==1&&visible(frame.hwnd)==constrained,
        "\"independent_actual_constraint_rejection\":true,\"pending_retired\":true");

    frame.grid=1;
    s.log.emit("I10_test_application_grid_reset","\"from\":17,\"to\":1,\"only_after_real_UP\":true,\"actor\":\"test_owned_app\",\"geometry_reset\":false");
    // Recovery starts at the actually constrained rectangle; no product
    // restart, source replacement or geometry reset hides the failed state.
    const auto next=layout_down(set,0,HTCAPTION,false);const auto moved=translated(constrained,31,29);
    s.log.check("I10_post_grid_new_generation",next.generation>g.generation);
    layout_phase(set,0,next,31,29,{moved},"I10_post_grid_actual_ordinary_Move");layout_up(set,next,{moved});
    const Rect neighbour{moved.r,moved.t,moved.r+300,moved.b};
    set.setup({moved,neighbour}); // Source is placed at its SAME actual rect.
    const auto ctrl=layout_down(set,1,HTCAPTION,true);const std::vector<Rect> group{translated(moved,29,23),translated(neighbour,29,23)};
    s.log.check("I10_post_grid_Ctrl_new_generation",ctrl.generation>next.generation);
    layout_phase(set,1,ctrl,29,23,group,"I10_post_grid_actual_Ctrl_source_and_follower");layout_up(set,ctrl,group);
    s.log.check("I10_grid_owned_resources_destroyed",set.close(),"\"guest_destruction_is_not_cleanup\":true");
    s.log.emit("I10_native_constraint_scope","\"status\":\"PARTIAL\",\"one_native_grid_nonexact_and_recovery_observed\":true," 
        "\"fixed_size_prior_actual_case_observed\":"+std::string(fixed_observed?"true":"false")+
        ",\"second_member_SetWindowPos_FALSE\":\"NOT_RUN\",\"missing_capability\":\"existing_fixture_has_no_safe_second_member_native_API_failure_injection\"," 
        "\"grid_nonexact_not_relabelled_as_API_FALSE\":true,\"whole_I10_PASS\":false,\"human_uat\":\"NOT_RUN\"");
}
