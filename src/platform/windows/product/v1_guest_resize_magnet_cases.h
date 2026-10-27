#pragma once

// Eight distinct active-edge contracts, not repeated stability attempts.
// All geometry oracles below use independent actual Win32/DWM rectangles.
std::vector<Rect> resize_magnet_native_baseline(LayoutFixtures& set,const std::vector<Rect>& expected){
    std::vector<Rect> native;auto& s=set.state;
    if(expected.size()!=set.active)throw std::runtime_error("resize_magnet_baseline_member_count");
    const auto monitor=MonitorFromWindow(set.frames[0].hwnd,MONITOR_DEFAULTTONULL);const auto dpi=GetDpiForWindow(set.frames[0].hwnd);
    for(std::size_t index=0;index<set.active;++index){
        const auto h=set.frames[index].hwnd;DWORD pid{};const auto tid=GetWindowThreadProcessId(h,&pid);const auto actual=visible(h),positioning=geometry_history_positioning(h);
        const bool exact=set.owns(index)&&actual==expected[index]&&positioning&&pid==GetCurrentProcessId()&&tid==set.thread&&
            IsWindowVisible(h)&&!IsIconic(h)&&!IsZoomed(h)&&gui_class(h)==s.class_name&&monitor&&
            MonitorFromWindow(h,MONITOR_DEFAULTTONULL)==monitor&&dpi&&GetDpiForWindow(h)==dpi;
        s.log.check("resize_magnet_actual_source_and_candidate_frame",exact,
            "\"member\":"+std::to_string(index)+",\"HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
            ",\"PID\":"+std::to_string(pid)+",\"TID\":"+std::to_string(tid)+",\"DPI\":"+std::to_string(GetDpiForWindow(h))+
            ",\"actual_visible\":"+(actual?rect(*actual):"null")+",\"actual_positioning\":"+(positioning?rect(*positioning):"null")+
            ",\"actual_rules_empty_same_fixture_class\":"+(chain_settings(s).rules.empty()?"true":"false")+",\"candidate_is_write_authority\":false");
        native.push_back(*positioning);
    }return native;
}
void resize_magnet_quantum(LayoutFixtures& set,const Gesture& g,int region,LONG dx,LONG dy,
    const std::vector<Rect>& initial,const std::vector<Rect>& native,const std::vector<Rect>& expected,const char* label){
    namespace e=product::guest::up_evidence_detail;auto& s=set.state;
    const bool left=region==HTLEFT||region==HTTOPLEFT||region==HTBOTTOMLEFT;
    const bool right=region==HTRIGHT||region==HTTOPRIGHT||region==HTBOTTOMRIGHT;
    const bool top=region==HTTOP||region==HTTOPLEFT||region==HTTOPRIGHT;
    const bool bottom=region==HTBOTTOM||region==HTBOTTOMLEFT||region==HTBOTTOMRIGHT;
    if(initial.size()!=set.active||native.size()!=set.active||expected.size()!=set.active||!(left||right||top||bottom))
        throw std::runtime_error("resize_magnet_exact_quantum_oracle_missing");
    const auto started=record(s,g.offset,"gesture_start",g.generation);const auto key=e::number(started,"\"key\":");
    s.log.check("resize_magnet_actual_plain_Resize_route",key&&*key&&geometry_history_receipt(s,started,"gesture_start")&&
        e::unique_value(started,"\"resize\":")=="true"&&e::unique_value(started,"\"ctrl\":")=="false");
    // InputService samples GetTickCount64 at the real hook. This finite test
    // wait follows actual START; it is not a product timer or authority lease.
    const auto after_start=GetTickCount64();
    const bool elapsed=wait([&]{return GetTickCount64()-after_start>=120;},160);
    const auto elapsed_ms=GetTickCount64()-after_start;const auto dpi=GetDpiForWindow(set.frames[0].hwnd);
    const auto settings=chain_settings(s);const auto speed=MulDiv(settings.speed_dip,static_cast<int>(dpi),96);
    const auto distance=std::max<std::uint64_t>(static_cast<std::uint64_t>(dx<0?-static_cast<std::int64_t>(dx):dx),static_cast<std::uint64_t>(dy<0?-static_cast<std::int64_t>(dy):dy));
    s.log.check("resize_magnet_actual_bounded_below_speed_setup",elapsed&&dpi&&speed>0&&distance*1000<static_cast<std::uint64_t>(speed)*elapsed_ms,
        "\"actual_elapsed_after_START_ms\":"+std::to_string(elapsed_ms)+",\"max_cursor_delta_pixels\":"+std::to_string(distance)+
        ",\"configured_speed_DIP_per_second\":"+std::to_string(settings.speed_dip)+",\"actual_DPI\":"+std::to_string(dpi)+
        ",\"internal_motion_classification_not_observed\":true");
    const auto mark=read_product(s).size();LARGE_INTEGER before{};
    if(!QueryPerformanceCounter(&before)||before.QuadPart<=0)throw std::runtime_error("resize_magnet_actual_input_clock_unknown");
    layout_phase(set,0,g,dx,dy,expected,label);
    unsigned native_calls{};bool receipts=true;std::string placement;std::istringstream rows(product_log_since(s,mark));std::string row;
    while(std::getline(rows,row))if(row.find("\"type\":\"placement\"")!=row.npos){
        receipts=receipts&&geometry_history_receipt(s,row,"placement")&&e::number(row,"\"generation\":")==g.generation&&
            e::number(row,"\"key\":")==key&&e::unique_value(row,"\"identity_valid\":")=="true"&&e::unique_value(row,"\"exact\":")=="true";
        if(e::unique_value(row,"\"native_callsite_claimed\":")=="true"){
            ++native_calls;placement=row;receipts=receipts&&geometry_history_receipt(s,row,"placement")&&e::number(row,"\"key\":")==key&&
                e::quoted(row,"\"intent_reason\":","single_resize")&&e::unique_value(row,"\"native_attempted\":")=="true"&&
                e::unique_value(row,"\"native_completion_known\":")=="true"&&e::unique_value(row,"\"identity_valid\":")=="true"&&
                e::unique_value(row,"\"exact\":")=="true"&&e::unique_value(row,"\"declared_track_limits_observed\":")=="true"&&
                e::number(row,"\"hook_observed_qpc\":").value_or(0)>=static_cast<std::uint64_t>(before.QuadPart)&&
                row.find("\"target\":"+rect(expected[0]))!=row.npos&&row.find("\"actual\":"+rect(expected[0]))!=row.npos;
        }else if(e::unique_value(row,"\"native_callsite_claimed\":")!="false"||e::unique_value(row,"\"native_attempted\":")!="false"||
            !e::quoted(row,"\"reason\":","already_exact_no_native_call"))receipts=false;
    }
    const auto actual=visible(set.frames[0].hwnd),positioning=geometry_history_positioning(set.frames[0].hwnd);
    const Rect expected_native{native[0].l+expected[0].l-initial[0].l,native[0].t+expected[0].t-initial[0].t,
        native[0].r+expected[0].r-initial[0].r,native[0].b+expected[0].b-initial[0].b};
    const bool fixed=actual&&positioning&&(!left||(actual->r==initial[0].r&&positioning->r==native[0].r))&&
        (!right||(actual->l==initial[0].l&&positioning->l==native[0].l))&&(!top||(actual->b==initial[0].b&&positioning->b==native[0].b))&&
        (!bottom||(actual->t==initial[0].t&&positioning->t==native[0].t))&&((left||right)||(actual->l==initial[0].l&&actual->r==initial[0].r&&positioning->l==native[0].l&&positioning->r==native[0].r))&&
        ((top||bottom)||(actual->t==initial[0].t&&actual->b==initial[0].b&&positioning->t==native[0].t&&positioning->b==native[0].b));
    bool candidates_unchanged=true;for(std::size_t index=1;index<set.active;++index)candidates_unchanged=candidates_unchanged&&set.owns(index)&&
        visible(set.frames[index].hwnd)==initial[index]&&geometry_history_positioning(set.frames[index].hwnd)==native[index];
    const Rect raw{initial[0].l+(left?dx:0),initial[0].t+(top?dy:0),initial[0].r+(right?dx:0),initial[0].b+(bottom?dy:0)};
    s.log.check("resize_magnet_single_writer_fixed_edges_and_targets",receipts&&native_calls==1&&set.owns(0)&&actual==expected[0]&&positioning==expected_native&&fixed&&candidates_unchanged,
        "\"generation\":"+std::to_string(g.generation)+",\"native_region\":"+std::to_string(region)+",\"original_DOWN_visible\":"+rect(initial[0])+
        ",\"original_DOWN_positioning\":"+rect(native[0])+",\"unconstrained_original_DOWN_intent\":"+rect(raw)+
        ",\"expected_visible\":"+rect(expected[0])+",\"actual_visible\":"+(actual?rect(*actual):"null")+
        ",\"independent_expected_positioning\":"+rect(expected_native)+",\"actual_positioning\":"+(positioning?rect(*positioning):"null")+
        ",\"actual_source_native_calls_this_quantum\":"+std::to_string(native_calls)+",\"actual_placement\":"+quote(placement)+
        ",\"fixed_edges_and_candidate_rectangles_exact\":"+(fixed&&candidates_unchanged?"true":"false")+",\"internal_proposal_is_native_attempt\":false");
}
void resize_magnet_fixed_size_blackbox(LayoutFixtures& set,Rect source,Rect neighbour,LayoutLimit target_limit,std::uint64_t previous){
    namespace e=product::guest::up_evidence_detail;auto& s=set.state;
    const std::vector<Rect> initial{source,neighbour};const LayoutLimit fixed{400,300,400,300};
    set.setup(initial,{fixed,target_limit});const auto native=resize_magnet_native_baseline(set,initial);
    const auto h=set.frames[0].hwnd;MINMAXINFO limits{};DWORD_PTR answered{};SetLastError(0);
    const bool limits_read=SendMessageTimeoutW(h,WM_GETMINMAXINFO,0,reinterpret_cast<LPARAM>(&limits),
        SMTO_ABORTIFHUNG|SMTO_BLOCK|SMTO_ERRORONEXIT,100,&answered)!=0;const auto limits_error=limits_read?0:GetLastError();
    SetLastError(0);const auto style=GetWindowLongPtrW(h,GWL_STYLE);const bool style_known=style!=0||GetLastError()==0;
    const auto border_width=native[0].width()-source.width(),border_height=native[0].height()-source.height();
    s.log.check("resize_magnet_fixed_size_actual_thickframe_and_declared_limits",set.owns(0)&&source.width()==400&&source.height()==300&&
        limits_read&&style_known&&(style&WS_THICKFRAME)&&limits.ptMinTrackSize.x-border_width==400&&
        limits.ptMaxTrackSize.x-border_width==400&&limits.ptMinTrackSize.y-border_height==300&&limits.ptMaxTrackSize.y-border_height==300,
        "\"actual_style\":"+std::to_string(style)+",\"WM_GETMINMAXINFO_returned\":"+(limits_read?"true":"false")+
        ",\"WM_GETMINMAXINFO_error\":"+std::to_string(limits_error)+",\"actual_native_min\":["+std::to_string(limits.ptMinTrackSize.x)+","+std::to_string(limits.ptMinTrackSize.y)+
        "],\"actual_native_max\":["+std::to_string(limits.ptMaxTrackSize.x)+","+std::to_string(limits.ptMaxTrackSize.y)+"],\"visible_limits\":[400,300,400,300]");
    const auto prior=geometry_history_status(s);const auto mark=read_product(s).size();const auto g=layout_down(set,0,HTBOTTOMRIGHT,false);
    const auto started=record(s,g.offset,"gesture_start",g.generation);const auto key=e::number(started,"\"key\":");
    s.log.check("resize_magnet_fixed_size_actual_new_plain_Resize",g.generation>previous&&key&&*key&&
        geometry_history_receipt(s,started,"gesture_start")&&e::unique_value(started,"\"resize\":")=="true"&&e::unique_value(started,"\"ctrl\":")=="false",
        "\"generation\":"+std::to_string(g.generation)+",\"actual_start\":"+quote(started));
    const POINT attempted{g.anchor.x+30,g.anchor.y+25};POINT actual_cursor{};cursor(s,attempted);
    const bool cursor_seen=GetCursorPos(&actual_cursor)&&actual_cursor.x==attempted.x&&actual_cursor.y==attempted.y;
    const auto unchanged=[&]{for(std::size_t n=0;n<initial.size();++n)if(!set.owns(n)||visible(set.frames[n].hwnd)!=initial[n]||
        geometry_history_positioning(set.frames[n].hwnd)!=native[n])return false;return true;};
    s.log.check("resize_magnet_fixed_size_actual_cursor_attempt_without_geometry_change",cursor_seen&&s.left&&unchanged(),
        "\"generation\":"+std::to_string(g.generation)+",\"original_DOWN_visible\":"+rect(source)+",\"original_DOWN_positioning\":"+rect(native[0])+
        ",\"requested_cursor\":["+std::to_string(attempted.x)+","+std::to_string(attempted.y)+"],\"actual_cursor\":["+std::to_string(actual_cursor.x)+","+std::to_string(actual_cursor.y)+
        "],\"single_cursor_packet_requested\":true,\"scope\":\"fixed_size_blackbox_Resize_attempt\",\"solver_motion_consumption_observed\":false");
    // Empty legal Resize plans have no placement receipt. Do not call
    // layout_phase(), which correctly requires a placement for its own scope.
    // The existing UP path independently requires this generation's actual UP,
    // NormalUp and cleanup; zero calls never substitutes for those facts.
    layout_up(set,g,initial);const auto ended=record(s,g.offset,"gesture_end",g.generation);const auto end_sequence=e::number(ended,"\"sequence\":");
    s.log.check("resize_magnet_fixed_size_actual_NormalUp_before_status",geometry_history_receipt(s,ended,"gesture_end")&&
        end_sequence&&*end_sequence&&e::unique_value(ended,"\"normal_up\":")=="true", "\"actual_end\":"+quote(ended));
    const auto after=geometry_history_status(s,*end_sequence);unsigned placements{};std::istringstream rows(product_log_since(s,mark));std::string row;
    while(std::getline(rows,row))if(e::quoted(row,"\"type\":","placement"))++placements;
    const auto prior_operations=e::number(prior,"\"operations\":"),after_operations=e::number(after,"\"operations\":");
    const auto prior_calls=e::number(prior,"\"native_callsite_claims\":"),after_calls=e::number(after,"\"native_callsite_claims\":");
    const auto actual=visible(h),positioning=geometry_history_positioning(h);
    s.log.check("resize_magnet_fixed_size_blackbox_zero_placement_and_native",unchanged()&&!placements&&prior_operations&&after_operations&&
        prior_calls&&after_calls&&*prior_operations==*after_operations&&*prior_calls==*after_calls,
        "\"generation\":"+std::to_string(g.generation)+",\"source_key\":"+std::to_string(*key)+",\"actual_visible\":"+(actual?rect(*actual):"null")+
        ",\"actual_positioning\":"+(positioning?rect(*positioning):"null")+",\"new_placement_records\":"+std::to_string(placements)+
        ",\"prior_status\":"+quote(prior)+",\"after_NormalUp_status\":"+quote(after)+
        ",\"scope\":\"actual_Resize_attempt_NormalUp_and_unchanged_geometry\",\"zero_calls_prove_solver_consumption\":false");
    const auto move=layout_down(set,0,HTCAPTION,false);const auto move_start=record(s,move.offset,"gesture_start",move.generation);
    s.log.check("resize_magnet_fixed_size_following_actual_plain_Move_generation",move.generation>g.generation&&
        geometry_history_receipt(s,move_start,"gesture_start")&&e::unique_value(move_start,"\"resize\":")=="false"&&e::unique_value(move_start,"\"ctrl\":")=="false");
    const auto moved=translated(source,25,15);layout_phase(set,0,move,25,15,{moved,neighbour},"resize_magnet_fixed_size_following_actual_Move");
    layout_up(set,move,{moved,neighbour});const auto moved_actual=visible(h),moved_positioning=geometry_history_positioning(h);
    s.log.check("resize_magnet_fixed_size_Move_keeps_size_and_other_positioning",set.owns(0)&&set.owns(1)&&moved_actual==moved&&
        moved_positioning==translated(native[0],25,15)&&visible(set.frames[1].hwnd)==neighbour&&geometry_history_positioning(set.frames[1].hwnd)==native[1],
        "\"generation\":"+std::to_string(move.generation)+",\"configured_visible_limits_unchanged\":[400,300,400,300],\"actual_visible\":"+(moved_actual?rect(*moved_actual):"null")+
        ",\"actual_positioning\":"+(moved_positioning?rect(*moved_positioning):"null"));
}
void owned_fixed_size_resize_case(State& s){
    settings_guest_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    if((s.suite!="FixedAndTerminalOnly"&&s.suite!="FixedSizeResizeOnly")||!original.enabled||!original.rules.empty()||s.left||s.control)
        throw std::runtime_error("fixed_size_Resize_exact_released_guest_with_unexcluded_fixtures_required");
    auto config=original;config.magnet_resize=true;config.magnet_move=false;config.glue=false;config.docking=false;config.preview=false;config.feedback=false;
    config.drag_transparency=false;config.shake=false;const auto applied=chain_gui_save(s,config);LayoutFixtures set{s};
    try{
        set.create();const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<1200||work.height()<740)throw std::runtime_error("fixed_size_Resize_work_area_below_exact_fixture_requirement");
        const Rect source{work.l+420,work.t+220,work.l+820,work.t+520};
        const Rect neighbour{source.l+456,source.t+40,source.l+676,source.b-40};
        resize_magnet_fixed_size_blackbox(set,source,neighbour,{120,100,work.width(),work.height()},0);
        chain_check_product(s,pid,birth);const auto restored=chain_gui_save(s,original);auto expected_config=original;expected_config.generation=restored.generation;
        s.log.check("fixed_size_Resize_original_configuration_restored",restored==expected_config);
        s.log.check("fixed_size_Resize_exact_test_roots_closed",set.close());
        s.log.emit("fixed_size_Resize_component","\"result\":\"PASS\",\"scope\":\"one_fixed_size_blackbox_Resize_attempt_NormalUp_then_actual_Move\",\"Resize_placement_records\":0,"+
            std::string("\"source_visible_limits\":[400,300,400,300],\"solver_motion_consumption_observed\":false,\"prior_eight_directions_repeated\":false,\"application_matrix\":\"NOT_RUN\",\"whole_M05_F04_or_version_PASS\":false,\"human_uat\":\"NOT_RUN\""));
    }catch(...){pause_routing_failed_case_input(s);try{if(!set.closed)set.close();}catch(...){}
        try{if(s.process&&chain_settings(s)==applied&&!s.left&&!s.control)chain_gui_save(s,original);}catch(...){}throw;}
}
void resize_magnet_maximum_pullback_and_move(LayoutFixtures& set,Rect source,Rect work,const LayoutLimit& limit,std::uint64_t previous){
    auto& s=set.state;
    const Rect neighbour{source.l+456,source.t+40,source.l+676,source.b-40};const LayoutLimit bounded{120,100,450,work.height()};
    const std::vector<Rect> initial{source,neighbour};set.setup(initial,{bounded,limit});const auto native=resize_magnet_native_baseline(set,initial);
    const auto g=layout_down(set,0,HTRIGHT,false);s.log.check("resize_magnet_limit_case_new_generation",g.generation>previous);
    const Rect clamped{source.l,source.t,source.l+450,source.b},pulled{source.l,source.t,source.l+430,source.b};
    s.log.emit("resize_magnet_independent_conflict_oracle","\"source_max_visible_width\":450,\"actual_neighbour_left\":"+std::to_string(neighbour.l)+
        ",\"candidate_width_if_adopted\":456,\"expected_legal_width\":450,\"source_original_width\":400,\"internal_rejected_proposal_not_directly_observed\":true");
    resize_magnet_quantum(set,g,HTRIGHT,70,0,initial,native,{clamped,neighbour},"resize_magnet_max_constraint_precedes_candidate");
    resize_magnet_quantum(set,g,HTRIGHT,30,0,initial,native,{pulled,neighbour},"resize_magnet_pullback_uses_original_DOWN_not_clamped_anchor");
    layout_up(set,g,{pulled,neighbour});unsigned constraint_calls{};std::istringstream audit(product_log_since(s,g.offset));std::string row;
    while(std::getline(audit,row))if(row.find("\"type\":\"placement\"")!=row.npos&&number(row,"\"generation\":")==g.generation&&row.find("\"native_callsite_claimed\":true")!=row.npos)++constraint_calls;
    s.log.check("resize_magnet_constraint_two_quanta_no_recursive_or_late_native_call",constraint_calls==2&&set.owns(0)&&set.owns(1)&&
        geometry_history_positioning(set.frames[0].hwnd)==Rect{native[0].l,native[0].t,native[0].r+30,native[0].b}&&
        geometry_history_positioning(set.frames[1].hwnd)==native[1],"\"actual_generation_native_calls\":"+std::to_string(constraint_calls)+",\"expected_two_requested_quanta\":2");
    const auto move=layout_down(set,0,HTCAPTION,false);
    s.log.check("resize_magnet_after_limit_normal_UP_new_Move_generation",move.generation>g.generation);
    const auto moved=translated(pulled,25,15);layout_phase(set,0,move,25,15,{moved,neighbour},"resize_magnet_after_limit_actual_plain_Move");layout_up(set,move,{moved,neighbour});
}
void owned_resize_magnet_case(State& s){
    settings_guest_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    if(s.suite!="ResizeMagnetOnly"||!original.enabled||!original.rules.empty()||s.left||s.control)throw std::runtime_error("resize_magnet_exact_released_guest_with_unexcluded_fixtures_required");
    auto config=original;config.magnet_resize=true;config.magnet_move=false;config.glue=false;config.docking=false;config.preview=false;config.feedback=false;
    config.drag_transparency=false;config.shake=false;config.attraction_dip=10;config.release_dip=16;config.speed_dip=2000;
    const auto applied=chain_gui_save(s,config);LayoutFixtures set{s};
    try{
        set.create();const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<1200||work.height()<740)throw std::runtime_error("resize_magnet_work_area_below_exact_fixture_requirement");
        const Rect source{work.l+420,work.t+220,work.l+820,work.t+520};const auto dpi=GetDpiForWindow(set.frames[0].hwnd);
        const auto attraction=MulDiv(10,static_cast<int>(dpi),96);
        s.log.check("resize_magnet_actual_4px_candidate_distance_inside_configured_field",dpi&&attraction>=6&&attraction<30,
            "\"actual_DPI\":"+std::to_string(dpi)+",\"actual_attraction_pixels\":"+std::to_string(attraction)+",\"normal_candidate_gap_pixels\":4,\"limit_conflict_candidate_gap_pixels\":6");
        const LayoutLimit limit{120,100,work.width(),work.height()};std::uint64_t previous{};
        for(const int region:std::array{HTLEFT,HTRIGHT,HTTOP,HTBOTTOM,HTTOPLEFT,HTTOPRIGHT,HTBOTTOMLEFT,HTBOTTOMRIGHT}){
            const bool left=region==HTLEFT||region==HTTOPLEFT||region==HTBOTTOMLEFT,right=region==HTRIGHT||region==HTTOPRIGHT||region==HTBOTTOMRIGHT;
            const bool top=region==HTTOP||region==HTTOPLEFT||region==HTTOPRIGHT,bottom=region==HTBOTTOM||region==HTBOTTOMLEFT||region==HTBOTTOMRIGHT;
            std::vector<Rect> initial{source};
            if(left)initial.push_back({source.l-280,source.t+40,source.l-60,source.b-40});
            if(right)initial.push_back({source.r+60,source.t+40,source.r+280,source.b-40});
            if(top)initial.push_back({source.l+40,source.t-180,source.r-40,source.t-60});
            if(bottom)initial.push_back({source.l+40,source.b+60,source.r-40,source.b+180});
            set.setup(initial,std::vector<LayoutLimit>(initial.size(),limit));const auto native=resize_magnet_native_baseline(set,initial);
            auto expected=initial;expected[0]={source.l-(left?60:0),source.t-(top?60:0),source.r+(right?60:0),source.b+(bottom?60:0)};
            const auto g=layout_down(set,0,region,false);s.log.check("resize_magnet_new_direction_new_generation",g.generation>previous);previous=g.generation;
            resize_magnet_quantum(set,g,region,left?-56:right?56:0,top?-56:bottom?56:0,initial,native,expected,"resize_magnet_actual_unique_direction_snap");
            layout_up(set,g,expected);chain_check_product(s,pid,birth);
        }
        resize_magnet_maximum_pullback_and_move(set,source,work,limit,previous);
        chain_check_product(s,pid,birth);const auto restored=chain_gui_save(s,original);auto expected_config=original;expected_config.generation=restored.generation;
        s.log.check("resize_magnet_original_configuration_restored",restored==expected_config);s.log.check("resize_magnet_exact_test_roots_closed",set.close());
        s.log.emit("resize_magnet_component","\"result\":\"PASS\",\"unique_plain_resize_directions\":8,\"fixed_edges_and_real_neighbours\":true,\"size_priority_and_original_DOWN_pullback\":true,"+
            std::string("\"new_actual_Move_after_Resize\":true,\"fixed_size_fixture\":\"NOT_RUN\",\"application_matrix\":\"NOT_RUN\",\"whole_M05_F04_or_version_PASS\":false,\"human_uat\":\"NOT_RUN\""));
    }catch(...){pause_routing_failed_case_input(s);try{if(!set.closed)set.close();}catch(...){}
        try{if(s.process&&chain_settings(s)==applied&&!s.left&&!s.control)chain_gui_save(s,original);}catch(...){}throw;}
}
