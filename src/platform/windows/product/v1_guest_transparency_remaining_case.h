#pragma once

// TEST ONLY: the two remaining M10-F04 scopes, not another visual/exit suite.
// Real product input/resources/native receipts; no alpha return is mocked.
std::string transparency_resource(State& s,const Gesture& g,std::string_view fact){
    namespace e=product::guest::up_evidence_detail;
    std::string found;std::istringstream rows(product_log_since(s,g.offset));std::string row;
    while(std::getline(rows,row))if(e::quoted(row,"\"type\":","gesture_input_resource")&&
        e::quoted(row,"\"source_sha\":",s.product_source)&&e::number(row,"\"process_id\":")==s.product_pid&&
        e::number(row,"\"generation\":")==g.generation&&e::quoted(row,"\"fact\":",fact))found=row;
    return found;
}
void transparency_scope(State& s){
    settings_guest_scope(s);
    if(s.suite!="TransparencyRemainderOnly"||!desktop()||s.left||s.control||!chain_settings(s).enabled||
       (GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_CONTROL)&0x8000)||
       (GetAsyncKeyState(VK_SHIFT)&0x8000)||(GetAsyncKeyState(VK_MENU)&0x8000))
        throw std::runtime_error("transparency_exact_released_enabled_guest_required");
}
void transparency_owned_visual_setup(LayoutFixtures& set,std::size_t index,BYTE alpha,bool top){
    auto& s=set.state;if(!set.owns(index))throw std::runtime_error("transparency_setup_not_exact_layout_owned_source");
    const HWND h=set.frames[index].hwnd;SetLastError(ERROR_SUCCESS);const auto style=GetWindowLongPtrW(h,GWL_EXSTYLE);
    const DWORD read_error=style?ERROR_SUCCESS:GetLastError();
    if(read_error)throw std::runtime_error("transparency_setup_original_style_unknown");
    bool style_attempted{},alpha_attempted{},position_attempted{};LONG_PTR previous{};
    BOOL alpha_return{},position_return{};DWORD style_error{},alpha_error{},position_error{};
    if(set.owns(index)){style_attempted=true;SetLastError(ERROR_SUCCESS);previous=SetWindowLongPtrW(h,GWL_EXSTYLE,style|WS_EX_LAYERED);
        style_error=previous?ERROR_SUCCESS:GetLastError();}
    if(style_attempted&&!style_error&&set.owns(index)){alpha_attempted=true;SetLastError(ERROR_SUCCESS);
        alpha_return=SetLayeredWindowAttributes(h,0,alpha,LWA_ALPHA);alpha_error=alpha_return?ERROR_SUCCESS:GetLastError();}
    if(alpha_return&&set.owns(index)){position_attempted=true;SetLastError(ERROR_SUCCESS);
        position_return=SetWindowPos(h,top?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        position_error=position_return?ERROR_SUCCESS:GetLastError();}
    const bool observed=style_attempted&&!style_error&&alpha_return&&position_return&&
        wait([&]{return set.owns(index)&&native_visual_is(h,alpha,top);},1000);
    s.log.check("transparency_exact_layout_owned_actual_visual_setup",observed,
        "\"actor\":\"fixture_app_not_product\",\"member\":"+std::to_string(index)+
        ",\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
        ",\"style_attempted\":"+(style_attempted?"true":"false")+",\"style_previous\":"+std::to_string(previous)+
        ",\"style_error\":"+std::to_string(style_error)+",\"alpha_attempted\":"+(alpha_attempted?"true":"false")+
        ",\"alpha_return\":"+(alpha_return?"true":"false")+",\"alpha_error\":"+std::to_string(alpha_error)+
        ",\"position_attempted\":"+(position_attempted?"true":"false")+",\"position_return\":"+(position_return?"true":"false")+
        ",\"position_error\":"+std::to_string(position_error)+",\"expected_alpha\":"+std::to_string(alpha)+
        ",\"expected_topmost\":"+(top?"true":"false")+",\"old_main_fixture_own_guard_unchanged\":true");
}
void transparency_owned_visual_exact(LayoutFixtures& set,std::size_t index,BYTE alpha,bool top,std::string name){
    const HWND h=set.frames.at(index).hwnd;
    const bool observed=wait([&]{return set.owns(index)&&native_visual_is(h,alpha,top);},1000);
    const auto actual=set.owns(index)?native_visual(h):std::optional<NativeVisual>{};
    const bool exact=actual&&actual->layered&&actual->known&&actual->alpha==alpha&&actual->color==0&&actual->flags==LWA_ALPHA&&actual->topmost==top;
    set.state.log.check(std::move(name),observed&&exact,
        "\"member\":"+std::to_string(index)+",\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
        ",\"expected_alpha\":"+std::to_string(alpha)+",\"expected_topmost\":"+(top?"true":"false")+
        ",\"actual_alpha\":"+(actual&&actual->layered&&actual->known?std::to_string(actual->alpha):"null")+
        ",\"actual_topmost\":"+(actual?(actual->topmost?"true":"false"):"null")+
        ",\"actual_layered\":"+(actual?(actual->layered?"true":"false"):"null")+
        ",\"actual_tuple_known\":"+(actual?(actual->known?"true":"false"):"null")+
        ",\"readback\":\"GetLayeredWindowAttributes_and_GWL_EXSTYLE_exact_LayoutFixtures_identity\"");
}
void transparency_owned_per_pixel_setup(LayoutFixtures& set,std::size_t index){
    auto& s=set.state;if(!set.owns(index))throw std::runtime_error("transparency_per_pixel_not_exact_owned_source");
    const HWND h=set.frames[index].hwnd;RECT before{};const auto visible_before=visible(h);
    SetLastError(ERROR_SUCCESS);const auto class_style=GetClassLongPtrW(h,GCL_STYLE);
    const DWORD class_error=class_style?0:GetLastError();
    if(class_error||(class_style&(CS_OWNDC|CS_CLASSDC))||!visible_before||!GetWindowRect(h,&before))
        throw std::runtime_error("transparency_per_pixel_requires_actual_ordinary_class_geometry");
    const LONG width=before.right-before.left,height=before.bottom-before.top;
    if(width<=0||height<=0||width>2048||height>2048)
        throw std::runtime_error("transparency_per_pixel_DIB_size_out_of_bounded_scope");
    SetLastError(ERROR_SUCCESS);const auto ex=GetWindowLongPtrW(h,GWL_EXSTYLE);const DWORD read_error=ex?0:GetLastError();
    if(read_error||(ex&WS_EX_LAYERED))throw std::runtime_error("transparency_per_pixel_requires_original_nonlayered_source");
    struct OwnGdi {
        HDC screen{},memory{};HBITMAP bitmap{};HGDIOBJ previous{};bool selected{},closed{};
        bool selection_restored{},memory_deleted{},bitmap_deleted{},screen_released{};
        void close()noexcept{if(closed)return;closed=true;
            selection_restored=!selected||SelectObject(memory,previous)==bitmap;
            memory_deleted=!memory||DeleteDC(memory)!=FALSE;
            bitmap_deleted=!bitmap||DeleteObject(bitmap)!=FALSE;
            screen_released=!screen||ReleaseDC(nullptr,screen)==1;
        }
        ~OwnGdi(){close();}
    } gdi;
    HDC& screen=gdi.screen;HDC& memory=gdi.memory;HBITMAP& bitmap=gdi.bitmap;
    screen=GetDC(nullptr);memory=screen?CreateCompatibleDC(screen):nullptr;void* pixels{};
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;
    info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    bitmap=memory?CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0):nullptr;
    gdi.previous=bitmap?SelectObject(memory,bitmap):nullptr;
    const bool selected=gdi.previous&&gdi.previous!=HGDI_ERROR;gdi.selected=selected;bool style_attempted{},update_attempted{};
    LONG_PTR style_previous{};DWORD style_error{},update_error{};BOOL updated{};
    if(selected&&pixels){
        // One opaque premultiplied BGRA image owned by this TEST window.
        std::fill_n(static_cast<DWORD*>(pixels),static_cast<std::size_t>(width)*height,0xff80a0c0u);
        if(set.owns(index)){style_attempted=true;SetLastError(ERROR_SUCCESS);
            style_previous=SetWindowLongPtrW(h,GWL_EXSTYLE,ex|WS_EX_LAYERED);style_error=style_previous?0:GetLastError();}
        if(style_attempted&&!style_error&&set.owns(index)){
            POINT destination{before.left,before.top},origin{};SIZE size{width,height};
            BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};update_attempted=true;SetLastError(ERROR_SUCCESS);
            updated=UpdateLayeredWindow(h,screen,&destination,&size,memory,&origin,0,&blend,ULW_ALPHA);
            update_error=updated?0:GetLastError();
        }
    }
    const POINT center{before.left+width/2,before.top+height/2};COLORREF pixel=CLR_INVALID;
    const bool opaque_hit=updated&&wait([&]{if(!set.owns(index)||WindowFromPoint(center)!=h)return false;
        pixel=GetPixel(screen,center.x,center.y);return pixel==RGB(0x80,0xa0,0xc0);},1000);
    COLORREF color{};BYTE alpha{};DWORD flags{};SetLastError(ERROR_SUCCESS);
    const BOOL read_alpha=set.owns(index)?GetLayeredWindowAttributes(h,&color,&alpha,&flags):FALSE;
    const DWORD alpha_error=read_alpha?0:GetLastError();
    RECT after{};const bool geometry_same=set.owns(index)&&GetWindowRect(h,&after)&&EqualRect(&before,&after)&&visible(h)==visible_before;
    const bool layered=set.owns(index)&&(GetWindowLongPtrW(h,GWL_EXSTYLE)&WS_EX_LAYERED)!=0;
    gdi.close();
    s.log.check("transparency_actual_per_pixel_unreadable_alpha_precondition",selected&&pixels&&style_attempted&&!style_error&&
        update_attempted&&updated&&opaque_hit&&geometry_same&&layered&&!read_alpha&&GetForegroundWindow()==h&&
        gdi.selection_restored&&gdi.memory_deleted&&gdi.bitmap_deleted&&gdi.screen_released,
        "\"actor\":\"exact_fixture_app_not_product\",\"member\":"+std::to_string(index)+
        ",\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+",\"actual_class_style\":"+std::to_string(class_style)+
        ",\"class_style_query_error\":"+std::to_string(class_error)+
        ",\"SetWindowLong_attempted\":"+(style_attempted?"true":"false")+",\"style_previous\":"+std::to_string(style_previous)+
        ",\"style_error\":"+std::to_string(style_error)+",\"UpdateLayeredWindow_attempted\":"+(update_attempted?"true":"false")+
        ",\"UpdateLayeredWindow_return\":"+(updated?"true":"false")+",\"UpdateLayeredWindow_error\":"+std::to_string(update_error)+
        ",\"GetLayeredWindowAttributes_return\":"+(read_alpha?"true":"false")+",\"GetLayeredWindowAttributes_error\":"+std::to_string(alpha_error)+
        ",\"original_window_alpha_tuple\":\"UNKNOWN_NOT_FABRICATED\",\"all_TEST_DIB_pixel_alpha\":255"+
        ",\"opaque_center_exact_hit\":"+(opaque_hit?"true":"false")+",\"actual_center_RGB\":"+std::to_string(pixel)+
        ",\"actual_positioning_and_visible_unchanged\":"+(geometry_same?"true":"false")+
        ",\"original_GDI_selection_restored\":"+(gdi.selection_restored?"true":"false")+",\"memory_DC_deleted\":"+(gdi.memory_deleted?"true":"false")+
        ",\"DIB_deleted\":"+(gdi.bitmap_deleted?"true":"false")+",\"screen_DC_released\":"+(gdi.screen_released?"true":"false")+
        ",\"mock_API_failure_used\":false,\"SetLayeredWindowAttributes_called_by_TEST\":false");
}
void transparency_restore(State& s,LayoutFixtures& set,const panebind::core::v1::Settings& original){
    const auto actual=chain_gui_save(s,original);auto expected=original;expected.generation=actual.generation;
    s.log.check("transparency_original_GUI_configuration_restored",actual==expected);
    const bool closed=set.close();
    s.log.check("transparency_exact_owned_roots_and_alias_closed",closed&&
        (!set.storage->alias_ever_registered||(set.storage->alias_unregistered&&!set.storage->alias_registered)),
        "\"alias_ever_registered\":"+std::string(set.storage->alias_ever_registered?"true":"false")+
        ",\"alias_actually_unregistered\":"+(set.storage->alias_unregistered?"true":"false")+
        ",\"unknown_or_user_resources_closed\":false");
}
void transparency_failure_cleanup(State& s,LayoutFixtures& set,const panebind::core::v1::Settings& original){
    if(s.left&&!s.left_up_attempted)try{button(s,false);}catch(...){s.log.failed=true;}
    if(!s.left&&!s.control)try{transparency_restore(s,set,original);}catch(const std::exception& error){
        s.log.failed=true;s.log.emit("transparency_failure_cleanup","\"reason\":"+quote(error.what())+
            ",\"original_failure_preserved\":true,\"cleanup_PASS_inferred\":false");}
}
void transparency_gone(State& s,LayoutFixtures& set,const Gesture& g){
    namespace e=product::guest::up_evidence_detail;
    std::string gone;const bool observed=wait([&]{gone=transparency_resource(s,g,"isolation_gone");return
        e::unique_value(gone,"\"overlay_destroyed\":")=="true"&&
        e::unique_value(gone,"\"hotkey_unregistered\":")=="true"&&
        e::unique_value(gone,"\"release_succeeded\":")=="true"&&
        e::unique_value(gone,"\"detach_succeeded\":")=="true";},2000);
    GUITHREADINFO actual{sizeof(actual)};
    s.log.check("transparency_real_resource_exit",observed&&GetGUIThreadInfo(set.thread,&actual)&&
        !actual.hwndCapture&&!actual.hwndMoveSize&&!actual.hwndMenuOwner,
        "\"generation\":"+std::to_string(g.generation)+",\"actual_Gone\":"+quote(gone)+
        ",\"ReleaseCapture_call_not_inferred_when_no_own_capture\":true,\"guest_destruction_used\":false");
}
[[maybe_unused]] void owned_transparency_focus_remaining_case(State& s){
    namespace v=panebind::core::v1;
    transparency_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;
    const auto birth=process_birth(s.process);auto config=original;
    config.magnet_move=false;config.magnet_resize=false;config.docking=false;config.shake=false;
    config.drag_transparency=true;config.drag_alpha=190;chain_gui_save(s,config);
    LayoutFixtures set{s};set.create();
    try{
        const auto work=chain_work(set.frames[0].hwnd);if(work.width()<1200||work.height()<650)
            throw std::runtime_error("transparency_two_owned_sources_need_actual_work_area");
        const std::vector<Rect> initial{{work.l+180,work.t+150,work.l+580,work.t+450},
            {work.l+730,work.t+150,work.l+1130,work.t+450}};
        set.setup(initial);const HWND source=set.frames[0].hwnd,other=set.frames[1].hwnd;
        transparency_owned_visual_setup(set,0,173,true);const auto before=native_visual(source);
        if(!before||!before->known||before->alpha!=173||!before->topmost)
            throw std::runtime_error("transparency_actual_original_173_topmost_required");
        const auto g=layout_down(set,0,HTCAPTION,false);transparency_owned_visual_exact(set,0,190,true,"transparency_focus_actual_held_alpha190");
        auto moved=initial;moved[0]=translated(initial[0],35,25);
        layout_phase(set,0,g,35,25,moved,"transparency_focus_actual_Move_before_loss");
        const auto ready=transparency_resource(s,g,"capture_ready");
        if(ready.empty()||!s.left||s.left_up_attempted||!actual_up(s,g.offset,g.generation).empty())
            throw std::runtime_error("transparency_focus_actual_held_capture_required");
        LARGE_INTEGER trigger{};if(!QueryPerformanceCounter(&trigger)||trigger.QuadPart<=0)
            throw std::runtime_error("transparency_focus_trigger_clock_unknown");
        const bool requested=set.owns(1)&&SetForegroundWindow(other)!=FALSE;
        const bool changed=requested&&wait([&]{return set.owns(1)&&GetForegroundWindow()==other;},1000);
        s.log.check("transparency_actual_owned_FocusLost_trigger",changed,
            "\"generation\":"+std::to_string(g.generation)+",\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+
            ",\"other_exact_owned\":"+std::to_string(reinterpret_cast<std::uintptr_t>(other))+
            ",\"trigger_before_qpc\":"+std::to_string(trigger.QuadPart)+",\"actual_capture_ready\":"+quote(ready)+
            ",\"product_SetForegroundWindow_or_abort_handler_called\":false,\"test_UP_not_yet_sent\":true");
        (void)interruption_end(s,g,v::End::FocusLost,false,source);
        transparency_gone(s,set,g);transparency_owned_visual_exact(set,0,173,true,"transparency_FocusLost_restores_original_173_not_255");
        s.log.check("transparency_focus_geometry_and_foreground_preserved",set.owns(0)&&visible(source)==moved[0]&&GetForegroundWindow()==other);
        button(s,false);const auto cleanup=abort_cleanup_up_receipt(s,g.offset,g.generation);
        s.log.check("transparency_single_actual_abort_cleanup_UP",cleanup.matched&&!s.left,
            "\"NormalUp_inferred\":false,"+cleanup.json());
        interruption_idle(s,set,true);interruption_audit_calls(s,g,static_cast<std::uint64_t>(trigger.QuadPart),std::nullopt);
        chain_check_product(s,pid,birth);
        const auto next=layout_down(set,0,HTCAPTION,false);s.log.check("transparency_same_instance_fresh_generation",next.generation>g.generation);
        moved[0]=translated(moved[0],25,20);layout_phase(set,0,next,25,20,moved,"transparency_after_focus_same_instance_Move");
        layout_up(set,next,moved);transparency_owned_visual_exact(set,0,173,true,"transparency_after_focus_NormalUp_restores_original");
        unsigned late{},unknown{};std::istringstream rows(product_log_since(s,g.offset));std::string row;
        while(std::getline(rows,row))if(row.find("\"type\":\"placement\"")!=row.npos&&number(row,"\"generation\":")==g.generation&&
            row.find("\"native_callsite_claimed\":true")!=row.npos){const auto qpc=number(row,"\"native_before_qpc\":");
            if(!qpc)++unknown;else if(qpc>=static_cast<std::uint64_t>(trigger.QuadPart))++late;}
        s.log.check("transparency_retired_generation_no_late_motion_after_successor",!late&&!unknown,
            "\"retired_generation\":"+std::to_string(g.generation)+",\"late_native_calls\":"+std::to_string(late)+
            ",\"unknown_native_clocks\":"+std::to_string(unknown)+",\"owned_alpha_cleanup_not_new_motion\":true");
        chain_check_product(s,pid,birth);transparency_restore(s,set,original);
        s.log.emit("transparency_focus_remaining_component","\"result\":\"PASS\",\"same_product_instance\":true,"+
            std::string("\"real_FocusLost_and_original_alpha_restore\":true,\"following_Move_NormalUp\":true,\"exit_suite_repeated\":false,\"human_uat\":\"NOT_RUN\""));
    }catch(...){transparency_failure_cleanup(s,set,original);throw;}
}
[[maybe_unused]] void owned_transparency_unsupported_remaining_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    transparency_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;
    const auto birth=process_birth(s.process);auto config=original;
    config.magnet_move=false;config.magnet_resize=false;config.docking=false;config.shake=false;
    config.drag_transparency=true;config.drag_alpha=190;chain_gui_save(s,config);
    LayoutFixtures set{s};set.create();
    try{
        const auto work=chain_work(set.frames[0].hwnd);if(work.width()<800||work.height()<650)
            throw std::runtime_error("transparency_per_pixel_actual_work_area_too_small");
        const std::vector<Rect> initial{{work.l+220,work.t+180,work.l+620,work.t+480}};
        set.setup(initial);const HWND source=set.frames[0].hwnd;
        transparency_owned_per_pixel_setup(set,0);
        const auto before=native_visual(source);
        if(!before||!before->layered||before->known)
            throw std::runtime_error("transparency_actual_per_pixel_alpha_must_really_be_unreadable");
        const auto g=layout_down(set,0,HTCAPTION,false);
        const auto start=record(s,g.offset,"gesture_start",g.generation);const auto source_key=number(start,"\"key\":");
        if(!source_key)throw std::runtime_error("transparency_per_pixel_exact_source_key_unknown");
        auto moved=initial;moved[0]=translated(initial[0],35,25);
        layout_phase(set,0,g,35,25,moved,"transparency_unreadable_alpha_free_Move_still_exact");layout_up(set,g,moved);
        transparency_gone(s,set,g);
        const auto after=native_visual(source);COLORREF color{};BYTE alpha{};DWORD flags{};SetLastError(ERROR_SUCCESS);
        const BOOL read_alpha=set.owns(0)?GetLayeredWindowAttributes(source,&color,&alpha,&flags):FALSE;
        const DWORD alpha_error=read_alpha?0:GetLastError();std::string notice;unsigned visual_requests{};
        {std::istringstream rows(product_log_since(s,g.offset));std::string row;
            while(std::getline(rows,row))if(e::number(row,"\"process_id\":")==pid&&e::quoted(row,"\"source_sha\":",s.product_source)){
                if(e::quoted(row,"\"type\":","visual_mutation")&&e::number(row,"\"key\":")==source_key)++visual_requests;
                if(e::quoted(row,"\"type\":","notice"))notice=row;
            }}
        const bool limitation=notice.find("透明")!=notice.npos&&
            (notice.find("不支持")!=notice.npos||notice.find("未应用")!=notice.npos||notice.find("无法")!=notice.npos);
        const bool protected_unknown=set.owns(0)&&after&&after->layered&&!after->known&&!read_alpha&&
            after->topmost==before->topmost&&!visual_requests&&visible(source)==moved[0];
        s.log.emit("transparency_per_pixel_after_real_UP_facts","\"generation\":"+std::to_string(g.generation)+
            ",\"source_key\":"+std::to_string(source_key)+",\"GetLayeredWindowAttributes_return\":"+(read_alpha?"true":"false")+
            ",\"GetLayeredWindowAttributes_error\":"+std::to_string(alpha_error)+",\"product_visual_requests_for_source\":"+std::to_string(visual_requests)+
            ",\"unknown_alpha_not_overwritten\":"+(protected_unknown?"true":"false")+
            ",\"original_unknown_alpha_not_reconstructed\":true,\"native_visual_API_rejection_not_invented\":true"+
            ",\"actual_notice\":"+(notice.empty()?"null":quote(notice))+",\"limitation_diagnostic_observed\":"+(limitation?"true":"false")+
            ",\"visible_limitation_feedback\":\"NOT_PROVEN_BY_NOTICE_LOG\",\"ordinary_Move_and_actual_UP_checked_separately\":true");
        chain_check_product(s,pid,birth);transparency_restore(s,set,original);
        s.log.check("transparency_actual_unreadable_alpha_not_overwritten",protected_unknown,
            "\"product_visual_requests_for_source\":"+std::to_string(visual_requests)+
            ",\"unsupported_route_is_actual_unreadable_per_window_alpha_not_mock_API_failure\":true");
        // A silent safe decline is not the required visible limitation feedback.
        s.log.check("transparency_unsupported_drag_has_explicit_limitation_diagnostic",limitation,
            "\"actual_notice\":"+(notice.empty()?"null":quote(notice))+",\"visible_UI_still_requires_actual_observation\":true");
        s.log.emit("transparency_unsupported_remaining_component","\"result\":\"PARTIAL\",\"safety_and_Move_checked\":true,"+
            std::string("\"visible_limitation_feedback\":\"NOT_RUN\",\"whole_M10_F04_PASS\":false,\"human_uat\":\"NOT_RUN\""));
    }catch(...){if(!set.closed)transparency_failure_cleanup(s,set,original);throw;}
}
