#pragma once

// TEST-only observations of the existing production feedback overlay. Include
// after layout, product-chain, geometry-history and interruption helpers. This
// does not add a product UI/query API or infer feedback from docking evidence.
void feedback_actual_overlay(LayoutFixtures& set,const Gesture& gesture,std::size_t source,
    const std::vector<Rect>& actual,bool group,POINT ink,COLORREF expected_ink,const char* label){
    auto& s=set.state;const auto config=chain_settings(s);
    if(s.suite!="RemainingNativeContractsOnly"||!config.feedback||config.preview||config.docking||
       source>=set.active||actual.size()!=set.active||!s.left||!set.owns(source))
        throw std::runtime_error("feedback_exact_active_guest_and_distinct_feedback_configuration_required");
    const HWND h=set.frames[source].hwnd;const auto dpi=GetDpiForWindow(h);
    if(!dpi)throw std::runtime_error("feedback_actual_source_DPI_unknown");
    Rect expected=actual.front();
    for(std::size_t n=0;n<actual.size();++n){if(!set.owns(n)||visible(set.frames[n].hwnd)!=actual[n])
            throw std::runtime_error("feedback_actual_member_identity_or_geometry_changed");
        if(group)expected={std::min(expected.l,actual[n].l),std::min(expected.t,actual[n].t),
            std::max(expected.r,actual[n].r),std::max(expected.b,actual[n].b)};}
    if(!group)expected=actual[source];
    if(group)expected.t-=static_cast<LONG>((25LL*dpi+48)/96);
    const auto started=record(s,gesture.offset,"gesture_start",gesture.generation);
    const bool start_exact=geometry_history_receipt(s,started,"gesture_start")&&
        number(started,"\"generation\":")==gesture.generation&&
        started.find(group?"\"ctrl\":true":"\"ctrl\":false")!=started.npos&&
        number(started,"\"group_members\":")== (group?actual.size():1);
    HWND overlay{},hit{},hit_root{};LONG_PTR style{};RECT native{};COLORREF key{};BYTE alpha{};DWORD flags{},pid{};
    product::guest::CapturedReleaseFacts capture;bool direct=false,own_shield=false;DWORD_PTR transparent{};bool hit_read=false;
    const bool observed=wait([&]{overlay=chain_preview(s);if(!overlay||!IsWindowVisible(overlay)||
            !GetWindowRect(overlay,&native)||Rect{native.left,native.top,native.right,native.bottom}!=expected||
            !GetLayeredWindowAttributes(overlay,&key,&alpha,&flags))return false;
        const auto tid=GetWindowThreadProcessId(overlay,&pid);style=GetWindowLongPtrW(overlay,GWL_EXSTYLE);
        if(!tid||pid!=s.product_pid||alpha!=255||flags!=(LWA_COLORKEY|LWA_ALPHA)||
           (style&(WS_EX_NOACTIVATE|WS_EX_TRANSPARENT|WS_EX_LAYERED))!=(WS_EX_NOACTIVATE|WS_EX_TRANSPARENT|WS_EX_LAYERED)||
           GetForegroundWindow()!=h||GetUpdateRect(overlay,nullptr,FALSE))return false;
        POINT pointer{};if(!GetCursorPos(&pointer))return false;hit=WindowFromPoint(pointer);hit_root=hit?GetAncestor(hit,GA_ROOT):nullptr;
        direct=hit_root==h;capture=release_capture_facts(s,h,hit_root,gesture.generation);
        own_shield=product::guest::captured_release_hit_allowed(capture);
        SetLastError(0);hit_read=SendMessageTimeoutW(overlay,WM_NCHITTEST,0,
            MAKELPARAM(static_cast<short>(ink.x),static_cast<short>(ink.y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&transparent)!=FALSE;
        return start_exact&&(direct||own_shield)&&hit_root!=overlay&&hit_read&&
            static_cast<LRESULT>(transparent)==HTTRANSPARENT&&GetForegroundWindow()==h;
    });
    s.log.check("feedback_actual_nonactivating_colorkey_window",observed,
        "\"label\":"+quote(label)+",\"generation\":"+std::to_string(gesture.generation)+",\"expected_feedback_bounds\":"+rect(expected)+
        ",\"actual_bounds\":"+rect({native.left,native.top,native.right,native.bottom})+",\"actual_overlay\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(overlay))+",\"actual_overlay_PID\":"+std::to_string(pid)+
        ",\"actual_extended_style\":"+std::to_string(style)+",\"layered_flags\":"+std::to_string(flags)+
        ",\"layered_alpha\":"+std::to_string(alpha)+",\"color_key\":"+std::to_string(key)+
        ",\"actual_pointer_hit\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+",\"direct_source_hit\":"+(direct?"true":"false")+
        ",\"exact_current_shield_hit\":"+(own_shield?"true":"false")+",\"actual_NCHITTEST\":"+
        (hit_read?std::to_string(static_cast<LRESULT>(transparent)):"null")+",\"docking_or_preview_enabled\":false");
    // Two fixed points, only inside the exact source fixture: an independently
    // expected adopted edge/member border and an otherwise blank client area.
    // Read the real desktop composition, not PrintWindow or product internals.
    RECT client{};POINT blank{};
    if(!GetClientRect(h,&client)||client.right<=0||client.bottom<=0)
        throw std::runtime_error("feedback_exact_source_client_unknown");
    blank={client.right/4,client.bottom/4};
    if(!ClientToScreen(h,&blank)||ink.x<actual[source].l||ink.x>=actual[source].r||ink.y<actual[source].t||ink.y>=actual[source].b||
       GetForegroundWindow()!=h||!set.owns(source)||visible(h)!=actual[source])
        throw std::runtime_error("feedback_fixed_pixel_scope_changed");
    const HDC screen=GetDC(nullptr);const COLORREF edge=screen?GetPixel(screen,ink.x,ink.y):CLR_INVALID;
    const COLORREF center=screen?GetPixel(screen,blank.x,blank.y):CLR_INVALID;
    const bool released=screen&&ReleaseDC(nullptr,screen)==1;const COLORREF window=GetSysColor(COLOR_WINDOW);
    const auto close_channel=[](BYTE a,BYTE b){const int d=static_cast<int>(a)-static_cast<int>(b);return d>=-2&&d<=2;};
    // The native handoff shield is intentionally near-transparent (alpha1).
    // Its possible one-step compositing change is not a filled-feedback box.
    const bool clear_center=center!=CLR_INVALID&&center!=key&&center!=static_cast<COLORREF>(config.preview_color)&&
        close_channel(GetRValue(center),GetRValue(window))&&close_channel(GetGValue(center),GetGValue(window))&&
        close_channel(GetBValue(center),GetBValue(window));
    const bool exact_pixels=released&&edge==expected_ink&&clear_center&&GetForegroundWindow()==h&&
        set.owns(source)&&visible(h)==actual[source]&&chain_preview(s)==overlay&&IsWindowVisible(overlay);
    s.log.emit("feedback_fixed_pixel_observation","\"label\":"+quote(label)+",\"generation\":"+std::to_string(gesture.generation)+
        ",\"observation\":"+quote(exact_pixels?"OBSERVED":"UNKNOWN")+",\"edge_point\":["+std::to_string(ink.x)+","+std::to_string(ink.y)+
        "],\"actual_edge_COLORREF\":"+(edge==CLR_INVALID?"null":std::to_string(edge))+",\"expected_edge_COLORREF\":"+std::to_string(expected_ink)+
        ",\"blank_client_point\":["+std::to_string(blank.x)+","+std::to_string(blank.y)+"],\"actual_client_COLORREF\":"+
        (center==CLR_INVALID?"null":std::to_string(center))+",\"expected_COLOR_WINDOW\":"+std::to_string(window)+
        ",\"center_unfilled_with_at_most_two_channel_compositing_steps\":"+(clear_center?"true":"false")+
        ",\"screen_DC_released\":"+(released?"true":"false")+",\"pixels_read\":2,\"occlusion_absence_or_unique_failure_cause_inferred\":false");
    if(!exact_pixels)throw std::runtime_error("feedback_fixed_pixels_not_proven_no_visual_PASS");
}

void feedback_abort_cleanup(State& s,LayoutFixtures& set,const Gesture& g){
    namespace e=product::guest::up_evidence_detail;std::string gone;
    const bool stopped=wait([&]{std::istringstream rows(product_log_since(s,g.offset));std::string row;
        while(std::getline(rows,row))if(geometry_history_receipt(s,row,"gesture_input_resource")&&
            e::number(row,"\"generation\":")==g.generation&&e::quoted(row,"\"fact\":","isolation_gone"))gone=row;
        return !gone.empty()&&e::unique_value(gone,"\"overlay_destroyed\":")=="true"&&
            e::unique_value(gone,"\"release_succeeded\":")=="true"&&e::unique_value(gone,"\"detach_succeeded\":")=="true";
    });
    s.log.check("feedback_abort_actual_resource_exit",stopped,
        "\"generation\":"+std::to_string(g.generation)+",\"actual_resource_gone\":"+quote(gone));
    chain_preview_hidden(s,"feedback_abort_actual_feedback_hidden");
    if(s.left&&!s.left_up_attempted)button(s,false);
    if(s.control)key(s,VK_CONTROL,false);
    interruption_idle(s,set,true);
}

void owned_feedback_case(State& s){
    namespace v=panebind::core::v1;namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    if(s.suite!="RemainingNativeContractsOnly"||!original.enabled||!original.rules.empty()||s.left||s.control)
        throw std::runtime_error("feedback_exact_enabled_released_guest_required");
    auto config=original;config.feedback=true;config.preview=false;config.docking=false;config.stretch=false;
    config.magnet_move=true;config.magnet_resize=true;config.glue=true;config.drag_transparency=false;config.shake=false;
    config.attraction_dip=10;config.release_dip=16;config.speed_dip=20000;chain_gui_save(s,config);
    LayoutFixtures set{s};set.create();
    try{
        const auto work=chain_work(set.frames[0].hwnd);const auto dpi=GetDpiForWindow(set.frames[0].hwnd);
        if(work.width()<1300||work.height()<800||!dpi)throw std::runtime_error("feedback_declared_work_area_or_DPI_unavailable");
        const LayoutLimit limit{120,100,work.width(),work.height()};
        const Rect source{work.l+240,work.t+200,work.l+640,work.t+500},target{work.l+900,work.t+350,work.l+1220,work.t+650};
        const LONG gap=target.l-source.r,entry=static_cast<LONG>((10LL*dpi+48)/96),release=static_cast<LONG>((16LL*dpi+48)/96);
        const Rect snapped=translated(source,gap,0);set.setup({source,target},{limit,limit});
        auto g=layout_down(set,0,HTCAPTION,false);
        layout_phase(set,0,g,30,0,{translated(source,30,0),target},"feedback_free_real_Move_without_adopted_constraint");
        chain_preview_hidden(s,"feedback_no_success_hint_for_free_Move");
        layout_phase(set,0,g,gap-entry+1,0,{snapped,target},"feedback_real_exact_snap_receipt");
        feedback_actual_overlay(set,g,0,{snapped,target},false,{snapped.r-1,(target.t+snapped.b)/2},
            static_cast<COLORREF>(config.preview_color),"adopted_actual_right_edge");
        const Rect detached=translated(source,gap-release-1,0);
        layout_phase(set,0,g,gap-release-1,0,{detached,target},"feedback_detach_real_receipt");
        chain_preview_hidden(s,"feedback_actual_detach_removes_adopted_edge");layout_up(set,g,{detached,target});
        chain_preview_hidden(s,"feedback_actual_NormalUp_hides_feedback");

        set.setup({source,target},{limit,limit});g=layout_down(set,0,HTCAPTION,false);
        layout_phase(set,0,g,gap-entry+1,0,{snapped,target},"feedback_before_real_Escape");
        feedback_actual_overlay(set,g,0,{snapped,target},false,{snapped.r-1,(target.t+snapped.b)/2},
            static_cast<COLORREF>(config.preview_color),"edge_before_Escape");
        key(s,VK_ESCAPE,true);key(s,VK_ESCAPE,false);
        (void)interruption_end(s,g,v::End::Escape,false,set.frames[0].hwnd);
        s.log.check("feedback_Escape_restores_actual_source",wait([&]{return set.owns(0)&&visible(set.frames[0].hwnd)==source;}));
        feedback_abort_cleanup(s,set,g);

        // One actual app-constrained sizing call: a selected success edge is
        // visible first, then the test window's existing grid rejects exactness.
        set.setup({source,target},{limit,limit});g=layout_down(set,0,HTRIGHT,false);
        const Rect resize_snap{source.l,source.t,target.l,source.b};
        layout_phase(set,0,g,gap-entry+1,0,{resize_snap,target},"feedback_Resize_adopted_edge_before_nonexact_native_call");
        feedback_actual_overlay(set,g,0,{resize_snap,target},false,{resize_snap.r-1,(target.t+source.b)/2},
            static_cast<COLORREF>(config.preview_color),"Resize_actual_adopted_edge");
        RECT positioning{};if(!GetWindowRect(set.frames[0].hwnd,&positioning)||!set.set_grid(0,17))
            throw std::runtime_error("feedback_owned_grid_context_unavailable");
        LONG dx=gap+release+41;const LONG frame_width=positioning.right-positioning.left-resize_snap.width();
        if((source.width()+dx+frame_width)%17==0)++dx;
        const Rect requested{source.l,source.t,source.r+dx,source.b};
        cursor(s,{g.anchor.x+dx,g.anchor.y});std::string rejected;
        const bool actual_rejection=wait([&]{
            for(const auto& row:constraint_generation_placements(s,g.offset,g.generation))
                if(geometry_history_receipt(s,row,"placement")&&e::unique_value(row,"\"native_attempted\":")=="true"&&
                   e::unique_value(row,"\"native_succeeded\":")=="true"&&e::unique_value(row,"\"exact\":")=="false"&&
                   row.find("\"target\":"+rect(requested))!=row.npos)rejected=row;
            return !rejected.empty()&&!record(s,g.offset,"gesture_end",g.generation).empty();
        });
        s.log.check("feedback_actual_nonexact_writer_result",actual_rejection,
            "\"generation\":"+std::to_string(g.generation)+",\"actual_rejected_receipt\":"+quote(rejected)+
            ",\"fault_actor\":\"exact_blank_test_window_size_grid\",\"API_false_inferred\":false");
        feedback_abort_cleanup(s,set,g);

        const Rect a{work.l+240,work.t+200,work.l+640,work.t+600},
            b{a.r,a.t,a.r+400,a.t+200},c{a.r,a.t+200,a.r+400,a.b};
        std::vector<Rect> members{a,b,c};set.setup(members,{limit,limit,limit});
        g=layout_down(set,0,HTCAPTION,true);
        feedback_actual_overlay(set,g,0,members,true,{a.l,a.t+90},GetSysColor(COLOR_GRAYTEXT),"Ctrl_A_initial_actual_three_members");
        for(auto& member:members)member=translated(member,30,20);
        layout_phase(set,0,g,30,20,members,"feedback_Ctrl_actual_group_receipts");
        feedback_actual_overlay(set,g,0,members,true,{members[0].l,members[0].t+90},GetSysColor(COLOR_GRAYTEXT),"Ctrl_A_actual_moved_members");
        layout_up(set,g,members);chain_preview_hidden(s,"feedback_Ctrl_NormalUp_removes_group_hint");
        g=layout_down(set,1,HTRIGHT,false);members[1].r+=40;
        layout_phase(set,1,g,40,0,members,"feedback_real_B_Resize_before_relation_refresh");layout_up(set,g,members);
        g=layout_down(set,2,HTCAPTION,true);
        feedback_actual_overlay(set,g,2,members,true,{members[2].r-1,members[2].t+90},GetSysColor(COLOR_GRAYTEXT),"Ctrl_C_fresh_relation_after_real_B_Resize");
        for(auto& member:members)member=translated(member,18,10);
        layout_phase(set,2,g,18,10,members,"feedback_Ctrl_C_fresh_relation_real_motion_before_UP");
        layout_up(set,g,members);chain_preview_hidden(s,"feedback_source_switch_UP_removes_group_hint");
        config.feedback=false;chain_gui_save(s,config);g=layout_down(set,1,HTCAPTION,true);
        for(auto& member:members)member=translated(member,14,0);
        layout_phase(set,1,g,14,0,members,"feedback_GUI_disabled_actual_Ctrl_motion_still_works");
        chain_preview_hidden(s,"feedback_GUI_disable_suppresses_actual_group_hint");layout_up(set,g,members);
        const auto restored=chain_gui_save(s,original);auto expected_config=original;expected_config.generation=restored.generation;
        s.log.check("feedback_original_GUI_configuration_restored",restored==expected_config);
        chain_check_product(s,pid,birth);s.log.check("feedback_exact_owned_windows_closed",set.close());
        s.log.emit("owned_feedback_component","\"result\":\"PASS_SCOPED_OBSERVATIONS\",\"same_product_process\":true,"+
            std::string("\"actual_feedback_not_dock_preview\":true,\"actual_selected_edge_detach_Escape_writer_rejection_and_UP\":true,")+
            "\"actual_Ctrl_member_geometry_and_Resize_refresh\":true,\"GUI_disable\":true,\"Shift_help_visual_review\":\"NOT_RUN\","+
            "\"visual_clarity_and_human_UAT\":\"NOT_RUN\",\"whole_M10_or_version_PASS_inferred\":false");
    }catch(...){
        if(s.left&&!s.left_up_attempted)try{button(s,false);}catch(...){}
        if(s.control)try{key(s,VK_CONTROL,false);}catch(...){}
        (void)set.close();try{chain_gui_save(s,original);}catch(...){}throw;
    }
}
