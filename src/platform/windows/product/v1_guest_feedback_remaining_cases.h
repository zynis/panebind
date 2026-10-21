#pragma once

// TEST ONLY. Include after v1_guest_feedback_cases.h. Observe the one shared
// Resize line and the existing public Shift explanation; do not repeat the
// previously tested Move/member/source-switch feedback sequence.
void feedback_remaining_shared_stroke(LayoutFixtures& set,const Gesture& g,const std::vector<Rect>& actual,
    LONG line,LONG y,const char* label){
    auto& s=set.state;const auto config=chain_settings(s);const auto dpi=GetDpiForWindow(set.frames[0].hwnd);
    const auto ink=static_cast<COLORREF>(config.preview_color);
    const LONG width=static_cast<LONG>((3LL*dpi+48)/96),first=line-width/2;
    if(s.suite!="FeedbackRemainderOnly"||actual.size()!=2||set.active!=2||!s.left||!dpi||width<1||width>12||
       ink==GetSysColor(COLOR_GRAYTEXT)||ink==GetSysColor(COLOR_WINDOW)||
       actual[0].r!=line||actual[1].l!=line||y<=std::max(actual[0].t,actual[1].t)||y>=std::min(actual[0].b,actual[1].b))
        throw std::runtime_error("feedback_remaining_exact_shared_segment_and_distinct_color_required");
    for(std::size_t n=0;n<actual.size();++n)if(!set.owns(n)||visible(set.frames[n].hwnd)!=actual[n])
        throw std::runtime_error("feedback_remaining_shared_actual_geometry_changed");
    const HWND overlay=chain_preview(s);const HDC screen=GetDC(nullptr);bool exact=screen&&overlay&&IsWindowVisible(overlay)&&
        GetForegroundWindow()==set.frames[0].hwnd;std::string pixels="[";
    // At most14 pixels, all inside the two exact blank fixtures. The colored
    // width is checked separately from the existing neutral member-outline
    // probe; merely seeing one colored point cannot prove a thick shared line.
    for(LONG offset=-1;offset<=width;++offset){const LONG x=first+offset;
        const bool in_owned=(x>=actual[0].l&&x<actual[0].r)||(x>=actual[1].l&&x<actual[1].r);
        const COLORREF color=screen&&in_owned?GetPixel(screen,x,y):CLR_INVALID;
        exact=exact&&in_owned&&color!=CLR_INVALID&&((offset>=0&&offset<width)?color==ink:color!=ink);
        if(offset!=-1)pixels+=',';pixels+=color==CLR_INVALID?"null":std::to_string(color);
    }
    pixels+=']';const bool released=screen&&ReleaseDC(nullptr,screen)==1;
    exact=exact&&released&&set.owns(0)&&set.owns(1)&&visible(set.frames[0].hwnd)==actual[0]&&
        visible(set.frames[1].hwnd)==actual[1]&&chain_preview(s)==overlay&&IsWindowVisible(overlay)&&
        GetForegroundWindow()==set.frames[0].hwnd;
    s.log.emit("feedback_remaining_actual_shared_stroke","\"label\":"+quote(label)+",\"generation\":"+std::to_string(g.generation)+
        ",\"observation\":"+quote(exact?"OBSERVED":"UNKNOWN")+",\"actual_DPI\":"+std::to_string(dpi)+
        ",\"shared_line_x\":"+std::to_string(line)+",\"sample_y\":"+std::to_string(y)+
        ",\"expected_colored_width_pixels\":"+std::to_string(width)+",\"first_colored_x\":"+std::to_string(first)+
        ",\"sample_COLORREFs_outside_then_strip_then_outside\":"+pixels+",\"expected_COLORREF\":"+std::to_string(ink)+
        ",\"screen_DC_released\":"+(released?"true":"false")+",\"occlusion_or_unique_failure_cause_inferred\":false");
    if(!exact)throw std::runtime_error("feedback_remaining_shared_stroke_pixels_not_proven");
}

[[maybe_unused]] void owned_feedback_remainder_case(State& s){
    settings_guest_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;
    const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if(s.suite!="FeedbackRemainderOnly"||!original.enabled||!original.rules.empty()||s.left||s.control||
       binary.empty()||binary!=hash(s.input_product))throw std::runtime_error("feedback_remainder_exact_idle_owned_guest_required");
    const auto saved_bytes=read(s.product.parent_path()/L"settings.ini");const auto gui=gui_open(s);gui_page_select(s,gui,1);
    const auto note=gui_find(s,gui,L"Static",L"释放距离必须严格大于吸引距离。");
    const auto text=gui_text(s,gui,note);RECT note_rect{},page_rect{},root_client{};
    const auto page=gui_page(s,gui);const bool geometry=GetWindowRect(note,&note_rect)&&GetWindowRect(page,&page_rect)&&GetClientRect(gui.root,&root_client);
    if(geometry)MapWindowPoints(gui.root,nullptr,reinterpret_cast<POINT*>(&root_client),2);
    const bool visible_note=geometry&&gui_identity(s,gui,note)&&IsWindowVisible(note)&&
        note_rect.left>=page_rect.left&&note_rect.right<=page_rect.right&&note_rect.top>=page_rect.top&&note_rect.bottom<=page_rect.bottom&&
        note_rect.left>=root_client.left&&note_rect.right<=root_client.right&&note_rect.top>=root_client.top&&note_rect.bottom<=root_client.bottom;
    s.log.check("feedback_public_Shift_bypass_explanation_actual_visible_text",visible_note&&
        text.find(L"Shift 起手临时绕过磁吸、停靠与联动。")!=text.npos&&
        text.find(L"当前手势固定旧配置，下次起手使用新设置。")!=text.npos,
        "\"actual_public_note\":"+quote(utf8(text))+",\"actual_note_rect\":"+rect({note_rect.left,note_rect.top,note_rect.right,note_rect.bottom})+
        ",\"settings_page\":1,\"same_product_PID\":"+std::to_string(pid)+",\"tray_help_used\":false,\"readability_review\":\"NOT_RUN\"");
    guest_capture_settings_page(s,gui,1);
    gui_press(s,gui,VK_ESCAPE);
    s.log.check("feedback_public_explanation_cancel_preserves_configuration",wait([&]{return !IsWindow(gui.root);})&&
        read(s.product.parent_path()/L"settings.ini")==saved_bytes&&chain_settings(s)==original);

    auto config=original;config.glue=true;config.feedback=true;config.preview=false;config.docking=false;config.stretch=false;
    config.magnet_move=false;config.magnet_resize=false;config.drag_transparency=false;config.shake=false;
    const auto applied=chain_gui_save(s,config);LayoutFixtures set{s};
    try{
        set.create();const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<1300||work.height()<800)throw std::runtime_error("feedback_remainder_actual_work_area_too_small");
        const Rect a{work.l+240,work.t+200,work.l+700,work.t+600},b{a.r,a.t,a.r+400,a.b};
        const LayoutLimit limits{120,100,work.width(),work.height()};set.setup({a,b},{limits,limits});
        const auto g=layout_down(set,0,HTRIGHT,true);const auto start=record(s,g.offset,"gesture_start",g.generation);
        s.log.check("feedback_remaining_actual_Ctrl_shared_Resize_start",geometry_history_receipt(s,start,"gesture_start")&&
            start.find("\"ctrl\":true")!=start.npos&&start.find("\"resize\":true")!=start.npos&&number(start,"\"group_members\":")==2,
            "\"generation\":"+std::to_string(g.generation)+",\"actual_start\":"+quote(start));
        feedback_actual_overlay(set,g,0,{a,b},true,{a.r-1,a.t+90},static_cast<COLORREF>(config.preview_color),"Ctrl_Resize_original_actual_shared_line");
        feedback_remaining_shared_stroke(set,g,{a,b},a.r,a.t+90,"before_real_Resize");
        const Rect resized_a{a.l,a.t,a.r+40,a.b},resized_b{b.l+40,b.t,b.r,b.b};
        layout_phase(set,0,g,40,0,{resized_a,resized_b},"feedback_remaining_real_shared_Resize_actual_geometry");
        feedback_actual_overlay(set,g,0,{resized_a,resized_b},true,{resized_a.r-1,resized_a.t+90},
            static_cast<COLORREF>(config.preview_color),"Ctrl_Resize_moved_actual_shared_line");
        feedback_remaining_shared_stroke(set,g,{resized_a,resized_b},resized_a.r,resized_a.t+90,"after_exact_Resize");
        layout_up(set,g,{resized_a,resized_b});chain_preview_hidden(s,"feedback_remaining_real_UP_hides_shared_line_and_member_hint");
        const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        s.log.check("feedback_remaining_original_configuration_and_binary_restored",restored==expected&&hash(s.product)==binary);
        chain_check_product(s,pid,birth);s.log.check("feedback_remaining_exact_owned_windows_closed",set.close());
        s.log.emit("feedback_remaining_component","\"result\":\"PASS_SCOPED_OBSERVATIONS\",\"same_product_instance\":true,"
            "\"actual_shared_Resize_colored_stroke_before_and_after_exact_motion\":true,\"actual_NormalUp_hidden\":true,"
            "\"public_Shift_explanation_read_and_captured\":true,\"visual_readability_review\":\"NOT_RUN\","
            "\"prior_member_outline_sequence_repeated\":false,\"whole_M10_or_version_PASS_inferred\":false,\"human_UAT\":\"NOT_RUN\"");
    }catch(...){
        pause_routing_failed_case_input(s);
        try{if(s.process&&!s.left&&!s.control&&chain_settings(s)==applied)(void)chain_gui_save(s,original);}catch(...){}
        try{if(!set.closed)set.close();}catch(...){}throw;
    }
}
