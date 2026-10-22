#pragma once

// TEST ONLY. Public numeric controls not exercised by the older representative
// six-page checkbox test. No direct configuration or product window mutation.
void settings_controls_scope(State& s){
    settings_guest_scope(s);
    if(s.suite!="SettingsControlsRemainderOnly"||s.left||s.control||!chain_settings(s).enabled||
       !chain_settings(s).rules.empty()||s.explorer[0]||s.applications[0])
        throw std::runtime_error("settings_controls_exact_idle_owned_guest_required");
}
panebind::core::v1::Settings settings_controls_numbers(State& s,int adjacency,int step,int alpha,int color){
    settings_controls_scope(s);const auto before=chain_settings(s);auto wanted=before;
    wanted.adjacency_dip=adjacency;wanted.step_dip=step;wanted.preview_alpha=alpha;wanted.preview_color=color;
    const auto gui=gui_open(s);
    const auto edit=[&](unsigned page,const wchar_t* label,int value){
        gui_page_select(s,gui,page);const auto target=gui_number(s,gui,label);
        if(gui_text(s,gui,target)==std::to_wstring(value)){
            s.log.emit("settings_controls_unchanged_numeric_readback","\"label\":"+quote(utf8(label))+
                ",\"value\":"+std::to_string(value)+",\"actual_edit_requested\":false");return;}
        settings_invalid_number_input(s,gui,target,std::to_wstring(value));
        s.log.check("settings_controls_actual_public_numeric_draft",gui_text(s,gui,target)==std::to_wstring(value),
            "\"label\":"+quote(utf8(label))+",\"value\":"+std::to_string(value)+",\"actual_public_Tab_and_Unicode_input\":true");
    };
    edit(1,L"邻接容差",adjacency);edit(2,L"预览不透明度",alpha);edit(2,L"预览颜色",color);edit(3,L"键盘微移",step);
    gui_save_enter(s,gui);const auto saved=chain_settings(s);wanted.generation=saved.generation;
    s.log.check("settings_controls_four_numeric_GUI_save_exact",saved==wanted&&saved.generation>before.generation,
        "\"adjacency_DIP\":"+std::to_string(adjacency)+",\"step_DIP\":"+std::to_string(step)+
        ",\"preview_alpha\":"+std::to_string(alpha)+",\"preview_fill_COLORREF\":"+std::to_string(color)+
        ",\"generation\":"+std::to_string(saved.generation)+",\"configuration_file_directly_written\":false");
    return saved;
}
void settings_controls_restore(State& s,const panebind::core::v1::Settings& original){
    const auto numeric=settings_controls_numbers(s,original.adjacency_dip,original.step_dip,original.preview_alpha,original.preview_color);
    (void)numeric;const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
    s.log.check("settings_controls_original_configuration_restored",restored==expected);
}
void settings_controls_restart_and_readback(State& s,const panebind::core::v1::Settings& wanted){
    const auto old_pid=s.product_pid;const auto old_birth=process_birth(s.process);const auto binary=hash(s.product);
    cleanup(s);if(s.process)throw std::runtime_error("settings_controls_restart_requires_actual_old_product_exit");
    const auto mark=read_product(s).size();
    s.log.check("settings_controls_same_product_EXE_restart",launch(s,L"--diagnostic",false));
    s.log.check("settings_controls_new_actual_startup_after_save",wait([&]{
        const auto startup=record(s,mark,"startup");return number(startup,"\"process_id\":")==s.product_pid;})&&
        process_birth(s.process)&&process_birth(s.process)!=old_birth&&hash(s.product)==binary&&chain_settings(s)==wanted,
        "\"old_PID\":"+std::to_string(old_pid)+",\"old_birth\":"+std::to_string(old_birth)+
        ",\"new_PID\":"+std::to_string(s.product_pid)+",\"new_birth\":"+std::to_string(process_birth(s.process))+
        ",\"unchanged_product_binary_sha256\":"+quote(binary));
    const auto gui=gui_open(s);
    struct Field {unsigned page;const wchar_t* label;int value;};
    for(const auto& value:std::array<Field,4>{{
        {1,L"邻接容差",wanted.adjacency_dip},{2,L"预览不透明度",wanted.preview_alpha},
        {2,L"预览颜色",wanted.preview_color},{3,L"键盘微移",wanted.step_dip}}}){
        const auto [page,label,expected]=value;gui_page_select(s,gui,page);
        s.log.check("settings_controls_restart_actual_Edit_retains_value",gui_text(s,gui,gui_number(s,gui,label))==std::to_wstring(expected),
            "\"label\":"+quote(utf8(label))+",\"expected\":"+std::to_string(expected));
    }
    gui_press(s,gui,VK_ESCAPE);s.log.check("settings_controls_readback_cancel_preserves_saved_configuration",
        wait([&]{return !IsWindow(gui.root);})&&chain_settings(s)==wanted);
}
void owned_settings_preview_controls_case(State& s){
    settings_controls_scope(s);const auto original=chain_settings(s);const auto binary=hash(s.product);
    auto config=original;config.magnet_move=false;config.magnet_resize=false;config.glue=true;config.docking=true;
    config.smart_docking=false;config.stretch=false;config.smart_stretch=false;config.preview=true;
    config.feedback=false;config.drag_transparency=false;config.shake=false;chain_gui_save(s,config);
    // Alpha 255 is a changed, legal value and avoids pretending the ignored
    // LWA_ALPHA colorkey is a color readback or guessing composited RGB.
    constexpr COLORREF fill=RGB(32,96,160);constexpr int alpha=255;
    const auto saved=settings_controls_numbers(s,4,13,alpha,static_cast<int>(fill));
    settings_controls_restart_and_readback(s,saved);
    LayoutFixtures set{s};set.create();
    try{
        const auto work=chain_work(set.frames[0].hwnd);const LayoutLimit limits{120,100,work.width(),work.height()};
        if(work.width()<1100||work.height()<760||saved.dock_actions[0]!=panebind::core::v1::Action::DockLeft)
            throw std::runtime_error("settings_preview_controls_independent_left_work_or_action_unavailable");
        const Rect initial{work.l+260,work.t+160,work.l+660,work.t+460};set.setup({initial},{limits});
        const auto g=layout_down(set,0,HTCAPTION,false);const POINT edge{work.l+1,work.t+work.height()/2};
        const auto raw=translated(initial,edge.x-g.anchor.x,edge.y-g.anchor.y);
        layout_phase(set,0,g,edge.x-g.anchor.x,edge.y-g.anchor.y,{raw},"settings_controls_changed_color_alpha_real_preview");
        const Rect half{work.l,work.t,work.l+work.width()/2,work.b};chain_preview_exact(s,set.frames[0].hwnd,half);
        const HWND overlay=chain_preview(s),source=set.frames[0].hwnd;RECT client{};POINT sample{};
        DWORD pid{};const auto tid=GetWindowThreadProcessId(overlay,&pid);COLORREF ignored_key{};BYTE actual_alpha{};DWORD flags{};
        if(!set.owns(0)||!overlay||!tid||pid!=s.product_pid||GetForegroundWindow()!=source||
           !GetClientRect(source,&client)||client.right<=0||client.bottom<=0)
            throw std::runtime_error("settings_preview_controls_exact_source_and_product_overlay_unknown");
        POINT client_tl{client.left,client.top},client_br{client.right,client.bottom};
        if(!ClientToScreen(source,&client_tl)||!ClientToScreen(source,&client_br)||visible(source)!=raw)
            throw std::runtime_error("settings_preview_controls_actual_client_screen_geometry_unknown");
        // The original caption anchor can put part of the source off-screen
        // at the work edge. Sample the actual client/preview intersection,
        // not an assumed fraction of an unclipped source rectangle.
        const LONG left=std::max(client_tl.x,half.l+5),right=std::min(client_br.x,half.r-5);
        const LONG top=std::max(client_tl.y,half.t+5),bottom=std::min(client_br.y,half.b-5);
        if(right<=left||bottom<=top)
            throw std::runtime_error("settings_preview_controls_fixed_pixel_not_inside_exact_owned_client_and_preview");
        sample={left+(right-left)/2,top+(bottom-top)/2};
        const bool preview_observed=wait([&]{
            return set.owns(0)&&chain_preview(s)==overlay&&IsWindowVisible(overlay)&&GetForegroundWindow()==source&&
                GetLayeredWindowAttributes(overlay,&ignored_key,&actual_alpha,&flags)&&actual_alpha==alpha&&
                flags==LWA_ALPHA&&!GetUpdateRect(overlay,nullptr,FALSE);});
        s.log.check("settings_controls_changed_preview_native_alpha_and_paint_ready",preview_observed,
            "\"generation\":"+std::to_string(g.generation)+",\"actual_alpha\":"+std::to_string(actual_alpha)+
            ",\"layered_flags\":"+std::to_string(flags)+",\"colorkey_not_used_as_fill_color_proof\":true");
        // This TEST caller's DwmFlush is not a foreign product/session flush.
        // Actual appearance is established below by a bounded read-only wait
        // for this fixed opaque pixel, with no further input or changed oracle.
        const HRESULT presented=DwmFlush();
        s.log.check("settings_controls_changed_preview_actual_DwmFlush",SUCCEEDED(presented)&&
            set.owns(0)&&visible(source)==raw&&GetForegroundWindow()==source&&
            chain_preview(s)==overlay&&IsWindowVisible(overlay),
            "\"generation\":"+std::to_string(g.generation)+",\"DwmFlush_actual_HRESULT\":"+
            std::to_string(presented)+",\"calls\":1,\"screen_pixel_not_read_on_failure\":true,"+
            "\"does_not_flush_entire_session\":true,\"foreign_product_frame_completion_inferred\":false");
        const HDC screen=GetDC(nullptr);COLORREF pixel=CLR_INVALID;unsigned samples{};bool pixel_observed=false;
        try{pixel_observed=screen&&wait([&]{
            if(!s.left||!(GetAsyncKeyState(VK_LBUTTON)&0x8000)||!set.owns(0)||visible(source)!=raw||
               GetForegroundWindow()!=source||chain_preview(s)!=overlay||!IsWindowVisible(overlay))return false;
            ++samples;pixel=GetPixel(screen,sample.x,sample.y);return pixel==fill;
        },1000);}catch(...){if(screen)ReleaseDC(nullptr,screen);throw;}
        const bool released=screen&&ReleaseDC(nullptr,screen)==1;
        s.log.check("settings_controls_changed_preview_actual_opaque_fill_pixel",pixel_observed&&released&&pixel==fill&&
            set.owns(0)&&visible(source)==raw&&GetForegroundWindow()==source&&chain_preview(s)==overlay&&IsWindowVisible(overlay),
            "\"generation\":"+std::to_string(g.generation)+",\"screen_sample\":["+std::to_string(sample.x)+","+std::to_string(sample.y)+
            "],\"actual_screen_COLORREF\":"+(pixel==CLR_INVALID?"null":std::to_string(pixel))+
            ",\"independent_expected_RGB_32_96_160\":"+std::to_string(fill)+",\"actual_alpha\":255,\"screen_DC_released\":"+
            (released?"true":"false")+",\"sampling_semantics\":\"desktop_composition_at_exact_owned_blank_client_inside_opaque_preview\","+
            "\"fixed_pixel_read_samples\":"+std::to_string(samples)+",\"readiness_deadline_ms\":1000,\"extra_input_or_oracle_changes\":false,\"color_tolerance\":0");
        const Rect away=translated(initial,24,17);layout_phase(set,0,g,24,17,{away},"settings_controls_leave_changed_preview");
        chain_preview_hidden(s,"settings_controls_changed_preview_cancelled_without_commit");
        layout_up(set,g,{away});chain_preview_hidden(s,"settings_controls_changed_preview_hidden_after_actual_UP");
        s.log.check("settings_controls_preview_owned_fixture_closed",set.close());
        settings_controls_restore(s,original);s.log.check("settings_controls_preview_product_bytes_unchanged",hash(s.product)==binary);
        s.log.emit("settings_controls_preview_component","\"result\":\"PASS\",\"actual_GUI_color_and_alpha\":true,\"four_numeric_values_survived_same_EXE_restart\":true,\"animation_API_execution\":\"NOT_RUN\",\"tray_and_DPI_acceptance\":\"NOT_GRANTED\"");
    }catch(...){pause_routing_failed_case_input(s);
        try{if(!set.closed)set.close();}catch(...){}try{if(s.process&&!s.left&&!s.control)settings_controls_restore(s,original);}catch(...){}throw;}
}
void owned_settings_adjacency_control_case(State& s){
    namespace e=product::guest::up_evidence_detail;settings_controls_scope(s);const auto original=chain_settings(s);
    auto config=original;config.glue=true;config.magnet_move=false;config.magnet_resize=false;config.docking=false;
    config.preview=false;config.feedback=false;config.drag_transparency=false;config.shake=false;chain_gui_save(s,config);
    LayoutFixtures set{s};set.create();
    try{
        const auto work=chain_work(set.frames[0].hwnd);const UINT dpi=GetDpiForWindow(set.frames[0].hwnd);
        const LONG gap=MulDiv(2,static_cast<int>(dpi),96)+1,tolerance=MulDiv(4,static_cast<int>(dpi),96);
        if(!dpi||gap<=0||gap>tolerance||work.width()<1100||work.height()<760)
            throw std::runtime_error("settings_adjacency_independent_gap_or_current_DPI_unavailable");
        const Rect a{work.l+200,work.t+200,work.l+600,work.t+500};
        const Rect b{a.r+gap,a.t,a.r+gap+300,a.b};const LayoutLimit limits{120,100,work.width(),work.height()};
        for(const int requested:{4,0}){
            const auto live=chain_settings(s);settings_controls_numbers(s,requested,live.step_dip,live.preview_alpha,live.preview_color);
            set.setup({a,b},{limits,limits});const auto g=layout_down(set,0,HTCAPTION,true);
            const auto started=record(s,g.offset,"gesture_start",g.generation);const bool group=requested==4;
            s.log.check("settings_controls_adjacency_actual_locked_group",geometry_history_receipt(s,started,"gesture_start")&&
                e::unique_value(started,"\"ctrl\":")=="true"&&e::unique_value(started,"\"resize\":")=="false"&&
                e::number(started,"\"group_members\":")==(group?2U:1U),
                "\"generation\":"+std::to_string(g.generation)+",\"actual_requested_adjacency_DIP\":"+std::to_string(requested)+
                ",\"actual_source_DPI\":"+std::to_string(dpi)+",\"independent_gap_pixels\":"+std::to_string(gap)+
                ",\"positive_tolerance_pixels\":"+std::to_string(tolerance)+",\"expected_group_members\":"+(group?"2":"1")+
                ",\"actual_start_receipt\":"+quote(started));
            const std::vector<Rect> after{translated(a,24,17),group?translated(b,24,17):b};
            layout_phase(set,0,g,24,17,after,group?"settings_controls_adjacency_four_DIP_includes_neighbour":"settings_controls_adjacency_zero_DIP_keeps_neighbour_still");
            layout_up(set,g,after);
        }
        s.log.check("settings_controls_adjacency_owned_fixtures_closed",set.close());settings_controls_restore(s,original);
        s.log.emit("settings_controls_adjacency_component","\"result\":\"PASS\",\"same_predeclared_nonzero_gap\":true,\"actual_Ctrl_members_and_rectangles_changed_with_GUI_tolerance\":true,\"full_group_matrix\":\"NOT_RUN\"");
    }catch(...){pause_routing_failed_case_input(s);
        try{if(!set.closed)set.close();}catch(...){}try{if(s.process&&!s.left&&!s.control)settings_controls_restore(s,original);}catch(...){}throw;}
}
void owned_settings_step_control_case(State& s){
    settings_controls_scope(s);const auto original=chain_settings(s);auto config=original;
    config.magnet_move=false;config.magnet_resize=false;config.glue=false;config.docking=false;
    config.preview=false;config.feedback=false;config.drag_transparency=false;config.shake=false;chain_gui_save(s,config);
    const auto live=chain_settings(s);settings_controls_numbers(s,live.adjacency_dip,13,live.preview_alpha,live.preview_color);
    LayoutFixtures set{s};set.create();
    try{
        const auto work=chain_work(set.frames[0].hwnd);const UINT dpi=GetDpiForWindow(set.frames[0].hwnd);
        const LONG step=MulDiv(13,static_cast<int>(dpi),96);
        if(!dpi||step<=0||work.width()<1100||work.height()<760)throw std::runtime_error("settings_step_independent_DPI_or_work_unavailable");
        const Rect a{work.l+200,work.t+180,work.l+600,work.t+480};
        const Rect b{work.l+720,work.t+180,work.l+1020,work.t+480};const LayoutLimit limits{120,100,work.width(),work.height()};
        set.setup({a,b},{limits,limits});const Rect moved=translated(a,step,0);auto taller=moved;taller.b+=step;
        chain_command(set,0,L"MoveRight",moved);chain_command(set,0,L"HeightMore",taller);
        s.log.check("settings_controls_changed_step_native_Move_and_Resize_exact",set.owns(0)&&set.owns(1)&&
            visible(set.frames[0].hwnd)==taller&&visible(set.frames[1].hwnd)==b,
            "\"changed_step_DIP\":13,\"actual_source_DPI\":"+std::to_string(dpi)+",\"independent_step_pixels\":"+std::to_string(step)+
            ",\"initial\":"+rect(a)+",\"after_public_MoveRight\":"+rect(moved)+",\"after_public_HeightMore\":"+rect(taller)+
            ",\"other_member_unchanged\":true,\"public_product_command_not_TEST_writer\":true");
        s.log.check("settings_controls_step_owned_fixtures_closed",set.close());settings_controls_restore(s,original);
        s.log.emit("settings_controls_step_component","\"result\":\"PASS\",\"changed_GUI_step_changes_real_Move_and_Resize_command\":true,\"hotkey_mapping_or_full_version_PASS_inferred\":false");
    }catch(...){pause_routing_failed_case_input(s);
        try{if(!set.closed)set.close();}catch(...){}try{if(s.process&&!s.left&&!s.control)settings_controls_restore(s,original);}catch(...){}throw;}
}

panebind::core::v1::Settings settings_controls_animation_save(State& s,bool enabled){
    settings_controls_scope(s);const auto before=chain_settings(s);auto wanted=before;wanted.animations=enabled;
    const auto gui=gui_open(s);gui_page_select(s,gui,0);
    chain_checkbox(s,gui,L"设置窗口打开时淡入",enabled);
    gui_save_enter(s,gui);const auto saved=chain_settings(s);wanted.generation=saved.generation;
    s.log.check("settings_controls_animation_actual_public_Save",saved==wanted&&saved.generation>before.generation,
        "\"requested_animation_option\":"+std::string(enabled?"true":"false")+",\"actual_saved_generation\":"+
        std::to_string(saved.generation)+",\"all_other_options_preserved\":true,\"animation_native_API_trace\":\"NOT_RUN\"");
    return saved;
}
void owned_settings_animation_control_case(State& s){
    settings_controls_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if(original.animations)throw std::runtime_error("settings_animation_test_requires_actual_default_OFF_before_ON_OFF_pair");
    try{
        const auto enabled=settings_controls_animation_save(s,true);
        auto gui=gui_open(s);gui_page_select(s,gui,0);
        const auto checkbox=gui_find(s,gui,L"Button",L"设置窗口打开时淡入");
        const HWND focus=gui_focus(gui);
        s.log.check("settings_controls_animation_ON_reopens_actual_visible_focused_GUI",
            gui_identity(s,gui,checkbox)&&IsWindowVisible(gui.root)&&IsWindowEnabled(gui.root)&&
            GetForegroundWindow()==gui.root&&gui_identity(s,gui,focus)&&
            gui_read(s,gui,checkbox,BM_GETCHECK)==BST_CHECKED&&chain_settings(s)==enabled,
            "\"settings_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.root))+
            ",\"actual_focus_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(focus))+
            ",\"saved_animation_option\":true,\"fade_or_AnimateWindow_call_inferred_from_visible_GUI\":false");
        // Save OFF in this actually reopened GUI, rather than changing a file
        // or asserting that a checkbox readback proves an animation API call.
        chain_checkbox(s,gui,L"设置窗口打开时淡入",false);gui_save_enter(s,gui);
        const auto disabled=chain_settings(s);auto expected=enabled;expected.animations=false;expected.generation=disabled.generation;
        s.log.check("settings_controls_animation_OFF_actual_same_GUI_Save",disabled==expected&&disabled.generation>enabled.generation);
        gui=gui_open(s);gui_page_select(s,gui,0);
        const auto off_checkbox=gui_find(s,gui,L"Button",L"设置窗口打开时淡入");const HWND off_focus=gui_focus(gui);
        s.log.check("settings_controls_animation_OFF_reopens_actual_visible_focused_GUI",
            gui_identity(s,gui,off_checkbox)&&IsWindowVisible(gui.root)&&IsWindowEnabled(gui.root)&&
            GetForegroundWindow()==gui.root&&gui_identity(s,gui,off_focus)&&
            gui_read(s,gui,off_checkbox,BM_GETCHECK)==BST_UNCHECKED&&chain_settings(s)==disabled,
            "\"settings_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.root))+
            ",\"actual_focus_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(off_focus))+
            ",\"actual_saved_animation_option\":false,\"animation_native_API_trace\":\"NOT_RUN\","+
            "\"source_fact\":\"settings_window.cpp animate() short-circuits on draft.animations=false; show() skips AnimateWindow when !animate()\""+
            ",\"timing_threshold_used_as_API_evidence\":false");
        const auto bytes=read(s.product.parent_path()/L"settings.ini");gui_press(s,gui,VK_ESCAPE);
        auto restored=original;restored.generation=disabled.generation;
        s.log.check("settings_controls_animation_original_options_and_cancel_restored",
            wait([&]{return !IsWindow(gui.root);})&&chain_settings(s)==restored&&
            read(s.product.parent_path()/L"settings.ini")==bytes&&hash(s.product)==binary);
        chain_check_product(s,pid,birth);
        s.log.emit("settings_controls_animation_component","\"result\":\"PASS_PUBLIC_CONTROL_SOURCE_AND_RUNTIME_SCOPE\","+
            std::string("\"actual_ON_Save_visible_reopen_then_OFF_Save_visible_reopen\":true,\"original_options_restored\":true,")+
            "\"OFF_short_circuit_basis\":\"actual false saved value plus unchanged production source\",\"AnimateWindow_native_call_trace\":\"NOT_RUN\",\"100ms_fade_trajectory\":\"NOT_RUN\",\"system_animation_policy_changed\":false,\"whole_version_PASS\":false");
    }catch(...){pause_routing_failed_case_input(s);
        try{if(s.process&&!s.left&&!s.control)(void)settings_controls_animation_save(s,original.animations);}catch(...){}throw;}
}
