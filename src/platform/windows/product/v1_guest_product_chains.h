#pragma once

// Guest fixture only, included after GUI/visual/layout helpers. Expected
// geometry is independently computed from actual platform rectangles and the
// explicitly selected fixture dimensions; never call the product's solver.
panebind::core::v1::Settings chain_settings(State& s){
    const auto parsed=panebind::core::v1::parse(read(s.product.parent_path()/L"settings.ini"));
    if(!parsed.value)throw std::runtime_error("chain_actual_configuration_invalid");return *parsed.value;
}
void chain_check_product(State& s,DWORD pid,std::uint64_t birth){
    if(!s.process||s.product_pid!=pid||GetProcessId(s.process)!=pid||process_birth(s.process)!=birth||
        WaitForSingleObject(s.process,0)!=WAIT_TIMEOUT||!desktop())throw std::runtime_error("chain_product_identity_or_interactive_desktop_lost");
}
void chain_checkbox(State& s,const SettingsGui& gui,const wchar_t* label,bool wanted){
    const auto control=gui_find(s,gui,L"Button",label);
    if((gui_read(s,gui,control,BM_GETCHECK)==BST_CHECKED)!=wanted)gui_click(s,gui,control);
    s.log.check("chain_GUI_actual_checkbox",wait([&]{return (gui_read(s,gui,control,BM_GETCHECK)==BST_CHECKED)==wanted;}),
        "\"label\":"+quote(utf8(label))+",\"wanted\":"+(wanted?"true":"false"));
}
std::wstring chain_edit_text(State& s,const SettingsGui& gui,HWND edit){
    std::array<wchar_t,16385> text{};const auto size=gui_read(s,gui,edit,WM_GETTEXT,text.size(),reinterpret_cast<LPARAM>(text.data()));
    if(size>=text.size()-1)throw std::runtime_error("chain_GUI_mapping_fragment_exceeds_test_bound");return {text.data(),size};
}
void chain_type_fragment(State& s,const SettingsGui& gui,HWND edit,const std::wstring& wanted){
    if(wanted.empty()||wanted.size()>16384)throw std::runtime_error("chain_GUI_fragment_bound_invalid");
    gui_click(s,gui,edit);gui_require_input(s,gui);key(s,VK_CONTROL,true);gui_press(s,gui,'A');key(s,VK_CONTROL,false);
    for(const wchar_t c:wanted){
        gui_require_input(s,gui);
        if(c==L'\r')continue;
        if(c==L'\n'){gui_press(s,gui,VK_RETURN);continue;} // The actual ES_WANTRETURN edit inserts CRLF.
        INPUT events[2]{};
        for(unsigned n=0;n<2;++n){events[n].type=INPUT_KEYBOARD;events[n].ki.wScan=c;
            events[n].ki.dwFlags=KEYEVENTF_UNICODE|(n?KEYEVENTF_KEYUP:0);events[n].ki.dwExtraInfo=0x50425631;}
        if(!desktop()||SendInput(2,events,sizeof(INPUT))!=2)throw std::runtime_error("chain_GUI_real_fragment_input_failed");
    }
    s.log.check("chain_GUI_actual_mapping_text",wait([&]{return chain_edit_text(s,gui,edit)==wanted;}),
        "\"characters\":"+std::to_string(wanted.size())+",\"clipboard_or_WM_SETTEXT\":false");
}
// Only the controls required by these chains are changed. All remaining
// configuration (including startup/rules/mouse mappings) must stay unchanged.
panebind::core::v1::Settings chain_gui_save(State& s,const panebind::core::v1::Settings& wanted){
    namespace v=panebind::core::v1;
    const auto before=chain_settings(s);
    if(wanted.startup!=before.startup||wanted.rules!=before.rules||wanted.mouse_bindings!=before.mouse_bindings||wanted.dock_actions!=before.dock_actions)
        throw std::runtime_error("chain_GUI_unsupported_or_unrequested_configuration_change");
    auto gui=gui_open(s);gui_page_select(s,gui,1);
    chain_checkbox(s,gui,L"移动时磁吸",wanted.magnet_move);chain_checkbox(s,gui,L"缩放时对",wanted.magnet_resize);
    chain_checkbox(s,gui,L"Ctrl 起手联动",wanted.glue);
    for(const auto& value:std::array<std::pair<const wchar_t*,int>,4>{{{L"吸引距离",wanted.attraction_dip},{L"释放距离",wanted.release_dip},
        {L"快移抑制",wanted.speed_dip},{L"邻接容差",wanted.adjacency_dip}}})
        if(gui_text(s,gui,gui_number(s,gui,value.first))!=std::to_wstring(value.second))
            gui_edit_number(s,gui,gui_number(s,gui,value.first),static_cast<unsigned>(value.second));
    gui_page_select(s,gui,2);
    chain_checkbox(s,gui,L"启用边角停靠",wanted.docking);chain_checkbox(s,gui,L"停靠时参考",wanted.smart_docking);
    chain_checkbox(s,gui,L"启用边／角双击",wanted.stretch);chain_checkbox(s,gui,L"智能拉伸",wanted.smart_stretch);
    chain_checkbox(s,gui,L"显示非激活",wanted.preview);
    gui_page_select(s,gui,3);
    if(gui_text(s,gui,gui_number(s,gui,L"键盘微移"))!=std::to_wstring(wanted.step_dip))
        gui_edit_number(s,gui,gui_number(s,gui,L"键盘微移"),static_cast<unsigned>(wanted.step_dip));
    if(wanted.hotkeys!=before.hotkeys){
        HWND edit{};
        for(HWND h=GetWindow(gui_page(s,gui),GW_CHILD);h;h=GetWindow(h,GW_HWNDNEXT))
            if(gui_class(h)==L"Edit"&&chain_edit_text(s,gui,h).starts_with(L"hotkey_count=")){edit=h;break;}
        if(!edit)throw std::runtime_error("chain_GUI_production_hotkey_editor_not_found");
        std::wstring fragment;const auto serialized=v::serialize(wanted);
        for(const auto line:v::settings_detail::split(serialized,'\n'))if(line.starts_with("hotkey_count=")||line.starts_with("hotkey.")){
            for(const char c:line){if(static_cast<unsigned char>(c)>=128)throw std::runtime_error("chain_hotkey_fragment_must_be_ASCII");fragment+=static_cast<wchar_t>(c);}fragment+=L"\r\n";
        }
        chain_type_fragment(s,gui,edit,fragment);
    }
    gui_page_select(s,gui,5);
    chain_checkbox(s,gui,L"显示实际吸附",wanted.feedback);chain_checkbox(s,gui,L"拖动期间临时透明",wanted.drag_transparency);
    chain_checkbox(s,gui,L"Move 中摇动",wanted.shake);
    for(const auto& value:std::array<std::pair<const wchar_t*,int>,5>{{{L"拖动不透明度",wanted.drag_alpha},{L"摇动不透明度",wanted.shake_alpha},
        {L"摇动幅度",wanted.shake_amplitude_dip},{L"摇动反向次数",wanted.shake_reversals},{L"摇动时间窗口",wanted.shake_window_ms}}})
        if(gui_text(s,gui,gui_number(s,gui,value.first))!=std::to_wstring(value.second))
            gui_edit_number(s,gui,gui_number(s,gui,value.first),static_cast<unsigned>(value.second));
    gui_save_enter(s,gui);const auto actual=chain_settings(s);auto expected=wanted;expected.generation=actual.generation;
    s.log.check("chain_GUI_saved_actual_configuration",actual==expected&&actual.generation>before.generation);
    return actual;
}
HWND chain_preview(State& s){
    struct Search {DWORD pid{};HWND hwnd{};unsigned count{};} search{s.product_pid};
    EnumWindows([](HWND h,LPARAM p)->BOOL{auto& find=*reinterpret_cast<Search*>(p);DWORD pid{};GetWindowThreadProcessId(h,&pid);
        if(pid==find.pid&&gui_class(h)==L"PaneBind.V1.Preview"){find.hwnd=h;++find.count;}return TRUE;},reinterpret_cast<LPARAM>(&search));
    return search.count==1?search.hwnd:nullptr;
}
void chain_preview_exact(State& s,HWND source,Rect expected){
    const auto config=chain_settings(s);RECT actual{};COLORREF color{};BYTE alpha{};DWORD flags{};POINT pointer{};HWND actual_hit{};
    const bool observed=wait([&]{const auto h=chain_preview(s);return h&&IsWindowVisible(h)&&GetWindowRect(h,&actual)&&
        Rect{actual.left,actual.top,actual.right,actual.bottom}==expected&&GetLayeredWindowAttributes(h,&color,&alpha,&flags)&&
        alpha==config.preview_alpha&&flags==LWA_ALPHA&&GetForegroundWindow()==source&&GetCursorPos(&pointer)&&
        (actual_hit=WindowFromPoint(pointer))&&GetAncestor(actual_hit,GA_ROOT)==source&&
        (GetWindowLongPtrW(h,GWL_EXSTYLE)&(WS_EX_NOACTIVATE|WS_EX_TRANSPARENT))==(WS_EX_NOACTIVATE|WS_EX_TRANSPARENT);});
    s.log.check("chain_actual_nonactivating_preview_rectangle",observed,"\"expected\":"+rect(expected)+
        ",\"actual\":"+rect({actual.left,actual.top,actual.right,actual.bottom})+",\"actual_alpha\":"+std::to_string(alpha)+
        ",\"actual_pointer_hit\":"+std::to_string(reinterpret_cast<std::uintptr_t>(actual_hit))+
        ",\"visual_clarity_review\":\"NOT_RUN\"");
}
void chain_preview_hidden(State& s,const char* label){
    s.log.check(label,wait([&]{const auto h=chain_preview(s);return h&&!IsWindowVisible(h);}));
}
void chain_front(LayoutFixtures& set,std::size_t index){
    if(index>=set.active||!set.owns(index)||set.state.left)throw std::runtime_error("chain_command_exact_released_target_required");
    const HWND h=set.frames[index].hwnd;
    set.state.log.check("chain_test_foreground_exact_owned",SetForegroundWindow(h)&&wait([&]{return GetForegroundWindow()==h;}));
}
std::string chain_any_placement(State& s,std::size_t offset,Rect expected){
    const auto text=read(s.product_log);if(offset>=text.size())return {};std::string row,result;std::istringstream rows(text.substr(offset));
    while(std::getline(rows,row))if(row.ends_with('}')&&row.find("\"type\":\"placement\"")!=std::string::npos&&
        row.find("\"native_attempted\":true")!=std::string::npos&&row.find("\"exact\":true")!=std::string::npos&&
        row.find("\"actual\":"+rect(expected))!=std::string::npos)result=row;return result;
}
void chain_command(LayoutFixtures& set,std::size_t source,const wchar_t* action,Rect expected){
    auto& s=set.state;chain_front(set,source);const auto mark=read(s.product_log).size();
    const bool sent=launch(s,L"--command "+std::wstring(action),true);std::string receipt;
    s.log.check("chain_real_command_"+utf8(action),sent&&wait([&]{receipt=chain_any_placement(s,mark,expected);
        return set.owns(source)&&visible(set.frames[source].hwnd)==expected&&!receipt.empty();}),
        "\"expected\":"+rect(expected)+",\"receipt\":"+quote(receipt));
}
void chain_hotkey(State& s,WORD code){
    try{key(s,VK_CONTROL,true);key(s,VK_MENU,true);key(s,code,true);key(s,code,false);key(s,VK_MENU,false);key(s,VK_CONTROL,false);}
    catch(...){try{key(s,code,false);key(s,VK_MENU,false);key(s,VK_CONTROL,false);}catch(...){}throw;}
}
void chain_hotkey_exact(LayoutFixtures& set,WORD code,Rect expected,const char* label){
    auto& s=set.state;chain_front(set,0);const auto mark=read(s.product_log).size();chain_hotkey(s,code);std::string receipt;
    s.log.check(label,wait([&]{receipt=chain_any_placement(s,mark,expected);return set.owns(0)&&visible(set.frames[0].hwnd)==expected&&!receipt.empty();}),
        "\"actual_registered_hotkey\":true,\"expected\":"+rect(expected)+",\"receipt\":"+quote(receipt));
}
Rect chain_work(HWND h){MONITORINFO info{sizeof(info)};
    if(!GetMonitorInfoW(MonitorFromWindow(h,MONITOR_DEFAULTTONULL),&info))throw std::runtime_error("chain_actual_work_area_unavailable");
    return {info.rcWork.left,info.rcWork.top,info.rcWork.right,info.rcWork.bottom};
}
void chain_double_click(LayoutFixtures& set,int native_region,bool shift,Rect expected,const char* label){
    auto& s=set.state;chain_front(set,0);const auto point=layout_hit(set,0,native_region);
    if(!point)throw std::runtime_error("chain_requested_native_double_click_region_not_observed");
    const auto before=visible(set.frames[0].hwnd);if(!before)throw std::runtime_error("chain_double_click_before_unknown");
    if(shift)key(s,VK_SHIFT,true);cursor(s,*point);Sleep(100);const auto mark=read(s.product_log).size();
    const auto started=GetTickCount64();button(s,true);button(s,false);
    if(!shift){s.log.check("chain_first_click_actually_terminated",wait([&]{return record(s,mark,"gesture_end").find("\"normal_up\":true")!=std::string::npos;},200));}
    else wait([&]{GUITHREADINFO info{sizeof(info)};return GetGUIThreadInfo(set.thread,&info)&&!info.hwndCapture&&!info.hwndMoveSize;},200);
    if(GetTickCount64()-started>=GetDoubleClickTime()||visible(set.frames[0].hwnd)!=before){
        if(shift)key(s,VK_SHIFT,false);throw std::runtime_error("chain_real_first_click_changed_geometry_or_double_click_timed_out");}
    button(s,true);std::string receipt;
    const bool changed=wait([&]{receipt=chain_any_placement(s,mark,expected);return set.owns(0)&&visible(set.frames[0].hwnd)==expected&&!receipt.empty();});
    button(s,false);if(shift)key(s,VK_SHIFT,false);
    s.log.check(label,changed&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000),"\"actual_native_hit\":"+std::to_string(native_region)+
        ",\"Shift\":"+(shift?"true":"false")+",\"real_DOWN_UP_DOWN_UP\":true,\"expected\":"+rect(expected)+",\"receipt\":"+quote(receipt));
}

[[maybe_unused]] void owned_dock_stretch_chains(State& s){
    namespace v=panebind::core::v1;
    const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    if(!original.enabled||original.dock_actions!=v::default_settings().dock_actions)
        throw std::runtime_error("I04_default_direction_actions_or_enabled_precondition_not_met");
    auto config=original;config.attraction_dip=10;config.release_dip=16;config.magnet_move=true;config.magnet_resize=true;
    config.docking=true;config.stretch=true;config.glue=true;config.smart_docking=false;config.smart_stretch=false;
    config.preview=true;config.feedback=false;config.drag_transparency=false;config.shake=false;
    chain_gui_save(s,config);
    LayoutFixtures set{s};set.create();const Rect work=chain_work(set.frames[0].hwnd);
    if(work.width()<1100||work.height()<850)throw std::runtime_error("I04_I05_current_guest_work_area_too_small_for_declared_fixture");
    const LayoutLimit limits{100,100,work.width(),work.height()};
    const Rect start{work.l+200,work.t+160,work.l+600,work.t+460};
    set.setup({start,{start.r,start.t,start.r+300,start.b}},{limits,limits});
    auto g=layout_down(set,0,HTCAPTION,false);
    layout_phase(set,0,g,-40,35,{translated(start,-40,35),{start.r,start.t,start.r+300,start.b}},"I04_real_free_Move");
    layout_phase(set,0,g,-6,3,{start,{start.r,start.t,start.r+300,start.b}},"I04_real_XY_resnap");
    layout_up(set,g,{start,{start.r,start.t,start.r+300,start.b}});
    ShowWindow(set.frames[1].hwnd,SW_HIDE);set.active=1;config.magnet_move=false;chain_gui_save(s,config);
    const LONG midx=work.l+work.width()/2,midy=work.t+work.height()/2;
    const Rect half{work.l,work.t,midx,work.b},quarter{work.l,work.t,midx,midy};
    g=layout_down(set,0,HTCAPTION,false);const POINT edge{work.l+1,midy};
    layout_phase(set,0,g,edge.x-g.anchor.x,edge.y-g.anchor.y,{translated(start,edge.x-g.anchor.x,edge.y-g.anchor.y)},"I04_actual_Move_to_screen_edge");
    chain_preview_exact(s,set.frames[0].hwnd,half);
    const POINT away{work.l+360,work.t+240};
    layout_phase(set,0,g,away.x-g.anchor.x,away.y-g.anchor.y,{translated(start,away.x-g.anchor.x,away.y-g.anchor.y)},"I04_leave_preview_without_docking");
    chain_preview_hidden(s,"I04_actual_leave_cancels_preview");
    const POINT corner{work.l+1,work.t+1};const Rect raw_corner=translated(start,corner.x-g.anchor.x,corner.y-g.anchor.y);
    layout_phase(set,0,g,corner.x-g.anchor.x,corner.y-g.anchor.y,{raw_corner},"I04_actual_Move_to_corner");
    chain_preview_exact(s,set.frames[0].hwnd,quarter);
    s.log.check("I04_preview_does_not_prematurely_dock",visible(set.frames[0].hwnd)==raw_corner&&
        placement(s,g.offset,g.generation,quarter,true).empty());
    layout_up(set,g,{quarter});
    const auto caption_after_UP=layout_hit(set,0,HTCAPTION);
    if(!caption_after_UP)throw std::runtime_error("I04_actual_caption_after_dock_unknown");
    cursor(s,*caption_after_UP); // Leave any legitimate edge-hover stretch preview.
    chain_preview_hidden(s,"I04_normal_UP_and_leave_edge_remove_preview");
    const auto dock_receipt=placement(s,g.offset,g.generation,quarter,true);
    s.log.check("I04_actual_UP_commits_corner_exact",dock_receipt.find("\"intent_reason\":\"normal_up_dock\"")!=std::string::npos,
        "\"production_receipt\":"+quote(dock_receipt));
    g=layout_down(set,0,HTCAPTION,false);
    const LONG ratio=static_cast<LONG>((static_cast<long long>(g.anchor.x-quarter.l)*start.width())/quarter.width());
    const LONG title_y=std::clamp<LONG>(g.anchor.y-quarter.t,0,start.height()-1);
    const Rect restored{g.anchor.x-ratio+40,g.anchor.y-title_y+45,g.anchor.x-ratio+40+start.width(),g.anchor.y-title_y+45+start.height()};
    layout_phase(set,0,g,40,45,{restored},"I04_actual_drag_out_restores_original_size_and_DOWN_anchor");layout_up(set,g,{restored});
    if(std::none_of(config.hotkeys.begin(),config.hotkeys.end(),[](const auto& h){return h.modifiers==3&&h.key==VK_LEFT&&h.action==v::Action::DockLeft;}))
        throw std::runtime_error("I04_actual_default_half_screen_hotkey_missing");
    chain_hotkey_exact(set,VK_LEFT,half,"I04_actual_registered_half_screen_hotkey");chain_command(set,0,L"Undo",restored);
    product::Catalog catalog;const auto displays=catalog.displays();
    if(displays.size()<2)s.log.emit("I04_cross_display","\"result\":\"ENVIRONMENT_BLOCKED\",\"reason\":\"one_actual_guest_monitor\",\"single_screen_chain_not_full_cross_screen_acceptance\":true");
    else{
        // Product Catalog deliberately rejects the CALLER's own PID. These
        // local fixture HWNDs therefore need independent native observation,
        // not a relaxed production eligibility policy or a fabricated token.
        const HWND h=set.frames[0].hwnd;const auto actual_visible=visible(h);RECT positioning{};MINMAXINFO track{};DWORD_PTR replied{};
        if(!set.owns(0)||actual_visible!=restored||!GetWindowRect(h,&positioning)||
            !SendMessageTimeoutW(h,WM_GETMINMAXINFO,0,reinterpret_cast<LPARAM>(&track),SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&replied))
            throw std::runtime_error("I04_cross_display_exact_owned_native_facts_unavailable");
        const auto current_monitor=MonitorFromWindow(h,MONITOR_DEFAULTTONULL);
        const auto from=std::find_if(displays.begin(),displays.end(),[&](const auto& d){return d.monitor==current_monitor;});
        if(from==displays.end()||!from->dpi)throw std::runtime_error("I04_cross_display_source_DPI_unknown");
        const auto& to=displays[(static_cast<std::size_t>(from-displays.begin())+1)%displays.size()];
        if(!to.dpi)throw std::runtime_error("I04_cross_display_destination_DPI_unknown");
        const auto scaled=[&](LONG px){return (static_cast<long long>(px)*to.dpi+from->dpi/2)/from->dpi;};
        const auto inset_x=static_cast<std::int64_t>(positioning.right)-positioning.left-restored.width();
        const auto inset_y=static_cast<std::int64_t>(positioning.bottom)-positioning.top-restored.height();
        const auto min_width=static_cast<std::int64_t>(track.ptMinTrackSize.x)-inset_x,min_height=static_cast<std::int64_t>(track.ptMinTrackSize.y)-inset_y;
        const auto max_width=std::min(static_cast<std::int64_t>(track.ptMaxTrackSize.x)-inset_x,to.work.width());
        const auto max_height=std::min(static_cast<std::int64_t>(track.ptMaxTrackSize.y)-inset_y,to.work.height());
        if(min_width<=0||min_height<=0||max_width<min_width||max_height<min_height)
            throw std::runtime_error("I04_cross_display_actual_size_constraints_have_no_intersection");
        const auto width=std::clamp<std::int64_t>(scaled(restored.width()),min_width,max_width);
        const auto height=std::clamp<std::int64_t>(scaled(restored.height()),min_height,max_height);
        const Rect next{static_cast<LONG>(to.work.left()),static_cast<LONG>(to.work.top()),static_cast<LONG>(to.work.left()+width),static_cast<LONG>(to.work.top()+height)};
        chain_command(set,0,L"NextDisplay",next);chain_command(set,0,L"Undo",restored);
        s.log.emit("I04_cross_display","\"result\":\"ACTUAL_COMMAND_GEOMETRY_CHECKED\",\"source_display_DPI\":"+
            std::to_string(from->dpi)+",\"destination_display_DPI\":"+std::to_string(to.dpi)+",\"mixed_DPI_full_matrix_pass_inferred\":false");
    }
    // Only the neighbour is arranged here. The source retains the actual
    // drag-out/Undo result; this cannot replace any preceding I04 operation.
    const Rect neighbour{restored.r,restored.t,restored.r+200,restored.b};
    if(neighbour.r>work.r)throw std::runtime_error("I04_post_dock_fixture_neighbour_not_inside_work_area");
    ShowWindow(set.frames[1].hwnd,SW_SHOWNOACTIVATE);set.active=2;
    s.log.check("I04_target_only_fixture_setup_for_resnap",fixture_rect(set.frames[1].hwnd,neighbour));
    config.magnet_move=true;chain_gui_save(s,config);g=layout_down(set,0,HTCAPTION,false);
    // The real drag-out/Undo source can be near the work-area left edge.
    // Move right for this FREE oracle: moving left40 from actual left48
    // yields8, legitimately inside the configured 15px attraction at144DPI.
    // Keep both original anchors and all preceding product operations intact.
    layout_phase(set,0,g,40,35,{translated(restored,40,35),neighbour},"I04_post_dock_real_free");
    layout_phase(set,0,g,-6,3,{restored,neighbour},"I04_post_dock_real_resnap");layout_up(set,g,{restored,neighbour});

    // I05: a real full-height 40%-width neighbour defines a non-half smart
    // boundary. Numerical expectations do not call smart_dock/stretch.
    const LONG line=work.l+work.width()*2/5;
    const Rect left{work.l,work.t,line,work.b},source{line+80,work.t+180,line+480,work.t+480};
    set.setup({source,left},{limits,limits});config.magnet_move=false;config.smart_docking=true;config.smart_stretch=true;
    chain_gui_save(s,config);const Rect remaining{line,work.t,work.r,work.b};
    chain_command(set,0,L"DockRight",remaining);chain_command(set,0,L"Undo",source);
    config.smart_docking=false;chain_gui_save(s,config);
    chain_command(set,0,L"DockRight",{midx,work.t,work.r,work.b});chain_command(set,0,L"Undo",source);
    chain_double_click(set,HTLEFT,false,{line,source.t,source.r,source.b},"I05_actual_edge_double_click_smart_stretch");chain_command(set,0,L"Undo",source);
    chain_double_click(set,HTRIGHT,true,{line,source.t,work.r,source.b},"I05_actual_Shift_edge_double_click_stretches_axis");chain_command(set,0,L"Undo",source);
    chain_double_click(set,HTBOTTOMRIGHT,false,{source.l,source.t,work.r,work.b},"I05_actual_corner_double_click_two_active_edges");chain_command(set,0,L"Undo",source);
    chain_double_click(set,HTBOTTOMRIGHT,true,remaining,"I05_actual_Shift_corner_both_axes");chain_command(set,0,L"Undo",source);
    const UINT dpi=GetDpiForWindow(set.frames[0].hwnd);if(!dpi)throw std::runtime_error("I05_microstep_actual_DPI_unknown");
    const LONG step=MulDiv(config.step_dip,static_cast<int>(dpi),96);const auto moved=translated(source,step,0);auto taller=moved;taller.b+=step;
    chain_command(set,0,L"MoveRight",moved);chain_command(set,0,L"HeightMore",taller);
    chain_command(set,0,L"Undo",moved);chain_command(set,0,L"Undo",source);
    WORD new_key{};for(const WORD candidate:std::array<WORD,3>{'H','J','K'})
        if(std::none_of(config.hotkeys.begin(),config.hotkeys.end(),[&](const auto& h){return h.modifiers==3&&h.key==candidate;})){new_key=candidate;break;}
    if(!new_key)throw std::runtime_error("I05_no_unused_test_hotkey_without_replacing_user_mapping");
    config.hotkeys.push_back({3,new_key,v::Action::MoveRight});chain_gui_save(s,config);
    chain_hotkey_exact(set,new_key,moved,"I05_actual_GUI_changed_hotkey_changes_geometry");chain_command(set,0,L"Undo",source);
    s.log.check("I05_neighbour_geometry_not_modified",set.owns(1)&&visible(set.frames[1].hwnd)==left);
    chain_gui_save(s,original);chain_check_product(s,pid,birth);s.log.check("I04_I05_exact_test_windows_closed",set.close());
    s.log.emit("I04_I05_actual_core_sequence","\"screen_edge_preview_leave_corner_UP_drag_restore\":true,\"real_half_screen_hotkey\":true,\"smart_on_off\":true,\"edge_corner_Shift_double_clicks\":true,\"micro_move_resize\":true,\"GUI_changed_hotkey\":true,\"same_product_process\":true,\"full_cross_screen_verdict_separate\":true,\"human_uat\":\"NOT_RUN\"");
}

void chain_i02_owned_core(State& s,unsigned round){
    const Rect work=chain_work(s.frames[0].hwnd);const LONG x=work.l+120,y=work.t+80;
    const std::array<Rect,3> initial{{{x,y,x+260,y+360},{x+260,y,x+520,y+180},{x+260,y+180,x+520,y+360}}};
    for(std::size_t n=0;n<3;++n)s.log.check("I11_round_initial_owned_layout",own(s,s.frames[n].hwnd)&&fixture_rect(s.frames[n].hwnd,initial[n]));
    const auto label="I11_round"+std::to_string(round);
    five(s,0,label+".A_initial");ctrl_move(s,0,34,0,label+".Ctrl_A");five(s,1,label+".B");five(s,2,label+".C");
    const auto before=visible(s.frames[1].hwnd);if(!before)throw std::runtime_error("I11_round_B_Resize_before_unknown");
    const auto g=down(s,1,false,HTBOTTOM);auto after=*before;after.b+=28;phase(s,1,g,0,28,after,label+".actual_Resize",true);up(s,1,g,after);
    five(s,0,label+".A_post_Resize");ctrl_move(s,2,22,18,label+".Ctrl_C");
    s.log.emit("I11_round_I02_core","\"round\":"+std::to_string(round)+",\"actual_owned_inputs\":true,\"Explorer_I02_pass_inferred\":false");
}
void chain_i03_owned_core(State& s){
    LayoutFixtures set{s};set.create();
    set.setup({{100,100,500,500},{500,100,900,300},{500,300,900,500}},{{250,100,600,800},{300,100,600,800},{250,100,600,800}});
    auto g=layout_down(set,0,HTRIGHT,true);layout_phase(set,0,g,60,0,{{100,100,560,500},{560,100,900,300},{560,300,900,500}},"I11_I03_vertical");layout_up(set,g,{{100,100,560,500},{560,100,900,300},{560,300,900,500}});
    g=layout_down(set,1,HTBOTTOM,true);layout_phase(set,1,g,0,30,{{100,100,560,500},{560,100,900,330},{560,330,900,500}},"I11_I03_horizontal");layout_up(set,g,{{100,100,560,500},{560,100,900,330},{560,330,900,500}});
    g=layout_down(set,1,HTLEFT,true);layout_phase(set,1,g,180,0,{{100,100,600,500},{600,100,900,330},{600,330,900,500}},"I11_I03_minimum");
    const std::vector<Rect> pull{{100,100,540,500},{540,100,900,330},{540,330,900,500}};
    layout_phase(set,1,g,-20,0,pull,"I11_I03_original_DOWN_pullback");layout_up(set,g,pull);
    g=layout_down(set,0,HTCAPTION,false);layout_phase(set,0,g,-40,35,{{60,135,500,535},pull[1],pull[2]},"I11_I03_following_Move");layout_up(set,g,{{60,135,500,535},pull[1],pull[2]});
    ShowWindow(set.frames[2].hwnd,SW_HIDE);set.active=2;
    // The L is formed by an actual source Move, not by resetting its position.
    g=layout_down(set,0,HTCAPTION,false);layout_phase(set,0,g,40,-35,{pull[0],pull[1]},"I11_I03_actual_Move_forms_L");layout_up(set,g,{pull[0],pull[1]});
    g=layout_down(set,1,HTCAPTION,true);layout_phase(set,1,g,40,30,{{140,130,580,530},{580,130,940,360}},"I11_I03_changed_leader");layout_up(set,g,{{140,130,580,530},{580,130,940,360}});
    s.log.check("I11_I03_local_resources_closed",set.close());
}
void chain_restore_app_visual(State& s,HWND h,const NativeVisual& original){
    if(!own(s,h))throw std::runtime_error("chain_original_visual_identity_changed");
    SetLastError(0);const auto ex=GetWindowLongPtrW(h,GWL_EXSTYLE);if(!ex&&GetLastError())throw std::runtime_error("chain_original_visual_style_unavailable");
    SetLastError(0);const auto old=SetWindowLongPtrW(h,GWL_EXSTYLE,original.layered?ex|WS_EX_LAYERED:ex&~WS_EX_LAYERED);
    if((!old&&GetLastError())||(original.layered&&!SetLayeredWindowAttributes(h,original.color,original.alpha,original.flags))||
        !SetWindowPos(h,original.topmost?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE))
        throw std::runtime_error("chain_original_app_visual_restore_failed");
    s.log.check("chain_original_app_visual_exact",wait([&]{const auto now=native_visual(h);return now&&*now==original;}));
}
// M09-F02: one real client pair and one real mapped caption pair per button.
// This is a guest-only input driver, not another product routing path.
[[maybe_unused]] void owned_mouse_mapping_cases(State& s){
    namespace v=panebind::core::v1;
    const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    if(!original.enabled||s.left||s.control)throw std::runtime_error("M09_mouse_mapping_requires_enabled_released_product");
    chain_check_product(s,pid,birth);LayoutFixtures set{s};set.create();const auto work=chain_work(set.frames[0].hwnd);
    if(work.width()<900||work.height()<650)throw std::runtime_error("M09_mouse_mapping_actual_work_area_too_small");
    const Rect initial{work.l+220,work.t+180,work.l+700,work.t+500};
    set.setup({initial},{{120,100,work.width(),work.height()}});auto& frame=set.frames[0];const HWND h=frame.hwnd;
    const auto original_visual=native_visual(h);if(!set.owns(0)||!original_visual||!original_visual->known)
        throw std::runtime_error("M09_mouse_mapping_exact_native_visual_unknown");
    unsigned pending_button{};bool configuration_changed{};
    const auto counts=[&]{std::array<std::array<unsigned,2>,5> result{};
        for(std::size_t n=0;n<5;++n)result[n]={frame.client_down[n].load(),frame.client_up[n].load()};return result;};
    const auto count_json=[](const std::array<std::array<unsigned,2>,5>& value){std::string out="[";
        for(std::size_t n=0;n<value.size();++n){if(n)out+=',';out+='['+std::to_string(value[n][0])+','+std::to_string(value[n][1])+']';}return out+']';};
    const auto released_modifiers=[&]{for(const int code:std::array{VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN})
        if(GetAsyncKeyState(code)&0x8000)return false;return !s.control;};
    const auto send_button=[&](unsigned which,bool pressed){
        if(which<1||which>5||!desktop()||(pressed&&(pending_button||!set.owns(0)||GetForegroundWindow()!=h||!released_modifiers())))
            throw std::runtime_error("M09_mouse_mapping_real_input_context_invalid");
        if(which==1){
            try{button(s,pressed);}catch(...){pending_button=s.left?1u:0u;throw;}
            pending_button=pressed?which:0;return;
        }
        INPUT input{};input.type=INPUT_MOUSE;
        constexpr std::array<DWORD,5> down_flags{MOUSEEVENTF_LEFTDOWN,MOUSEEVENTF_RIGHTDOWN,MOUSEEVENTF_MIDDLEDOWN,MOUSEEVENTF_XDOWN,MOUSEEVENTF_XDOWN};
        constexpr std::array<DWORD,5> up_flags{MOUSEEVENTF_LEFTUP,MOUSEEVENTF_RIGHTUP,MOUSEEVENTF_MIDDLEUP,MOUSEEVENTF_XUP,MOUSEEVENTF_XUP};
        input.mi.dwFlags=(pressed?down_flags:up_flags)[which-1];input.mi.mouseData=which==4?XBUTTON1:which==5?XBUTTON2:0;
        LARGE_INTEGER before{},after{};QueryPerformanceCounter(&before);UINT inserted=UINT_MAX;DWORD error{};
        const auto receipt=[&]{QueryPerformanceCounter(&after);s.log.emit("M09_test_button_send","\"button\":"+std::to_string(which)+
            ",\"requested\":"+quote(pressed?"DOWN":"UP")+",\"send_before_qpc\":"+std::to_string(before.QuadPart>0?before.QuadPart:0)+
            ",\"send_after_qpc\":"+std::to_string(after.QuadPart>0?after.QuadPart:0)+",\"mouse_flags\":"+std::to_string(input.mi.dwFlags)+
            ",\"mouse_data\":"+std::to_string(input.mi.mouseData)+",\"SendInput_called\":"+(inserted==UINT_MAX?"false":"true")+
            ",\"SendInput_return\":"+(inserted==UINT_MAX?"null":std::to_string(inserted))+",\"SendInput_error\":"+std::to_string(error)+
            ",\"driver_button_outstanding\":"+std::to_string(pending_button)+",\"delivery_and_physical_held_not_inferred\":true");};
        try{inject(input,&inserted,&error);}catch(...){receipt();throw;}
        pending_button=pressed?which:0;receipt();
    };
    const auto real_point=[&](bool caption){
        chain_front(set,0);POINT point{};
        if(caption){const auto hit=layout_hit(set,0,HTCAPTION);if(!hit)throw std::runtime_error("M09_actual_caption_unavailable");point=*hit;}
        else{RECT client{};if(!GetClientRect(h,&client))throw std::runtime_error("M09_actual_client_unavailable");
            point={(client.left+client.right)/2,(client.top+client.bottom)/2};
            if(!ClientToScreen(h,&point))throw std::runtime_error("M09_actual_client_screen_unknown");}
        cursor(s,point);const HWND actual_hit=WindowFromPoint(point);DWORD_PTR region{};GUITHREADINFO info{sizeof(info)};
        const bool native_hit=actual_hit==h&&SendMessageTimeoutW(h,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y)),
            SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&region)&&static_cast<int>(region)==(caption?HTCAPTION:HTCLIENT);
        s.log.check("M09_real_exact_source_and_native_region",set.owns(0)&&GetForegroundWindow()==h&&native_hit&&released_modifiers()&&
            GetGUIThreadInfo(set.thread,&info)&&!info.hwndCapture&&!info.hwndMoveSize&&!info.hwndMenuOwner,
            "\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+",\"source_TID\":"+std::to_string(set.thread)+
            ",\"actual_native_region\":"+std::to_string(region)+",\"expected_region\":"+std::to_string(caption?HTCAPTION:HTCLIENT)+
            ",\"actual_point\":["+std::to_string(point.x)+","+std::to_string(point.y)+"],\"actual_modifiers_clear\":"+(released_modifiers()?"true":"false"));
        // Existing source UI STA independently serves preparation's sent query.
        (void)wait([]{return false;},100);
    };
    const auto client_pair=[&](unsigned which,const char* stage){
        real_point(false);const auto before=counts();const auto raw_before=raw_mouse_observer_json();const auto mark=read(s.product_log).size();const auto starts=frame.starts.load();
        send_button(which,true);const bool down_observed=wait([&]{return frame.client_down[which-1].load()==before[which-1][0]+1;},1000);
        send_button(which,false);const bool up_observed=wait([&]{return frame.client_up[which-1].load()==before[which-1][1]+1;},1000);
        auto expected=before;++expected[which-1][0];++expected[which-1][1];const auto after=counts();const auto raw_after=raw_mouse_observer_json();GUITHREADINFO info{sizeof(info)};
        s.log.check("M09_actual_unmapped_client_button_pair",down_observed&&up_observed&&after==expected&&set.owns(0)&&visible(h)==initial&&
            record(s,mark,"gesture_start").empty()&&frame.starts.load()==starts&&GetGUIThreadInfo(set.thread,&info)&&!info.hwndCapture&&!info.hwndMoveSize&&!info.hwndMenuOwner,
            "\"stage\":"+quote(stage)+",\"button\":"+std::to_string(which)+",\"actual_client_DOWN_UP_counts_before\":"+count_json(before)+
            ",\"actual_client_DOWN_UP_counts_after\":"+count_json(after)+",\"native_client_message_delivery_observed\":"+(down_observed&&up_observed?"true":"false")+",\"fixture_Raw_before\":"+
            raw_before+",\"fixture_Raw_after\":"+raw_after+",\"Raw_zero_does_not_fail_product_case\":true");
    };
    const auto mouse_fragment=[](const v::Settings& value){std::wstring fragment;const auto serialized=v::serialize(value);
        for(const auto row:v::settings_detail::split(serialized,'\n'))if(row.starts_with("mouse_count=")||row.starts_with("mouse.")){
            for(const char c:row){if(static_cast<unsigned char>(c)>=128)throw std::runtime_error("M09_mouse_fragment_non_ASCII");fragment+=static_cast<wchar_t>(c);}fragment+=L"\r\n";
        }if(fragment.empty()||fragment.size()>16384)throw std::runtime_error("M09_original_mouse_fragment_exceeds_existing_GUI_test_bound");return fragment;};
    const auto original_fragment=mouse_fragment(original);
    const auto save_mouse=[&](const std::vector<v::MouseBinding>& bindings){
        const auto before=chain_settings(s);auto wanted=before;wanted.mouse_bindings=bindings;const auto fragment=mouse_fragment(wanted);
        auto gui=gui_open(s);gui_page_select(s,gui,3);HWND edit{};
        for(HWND child=GetWindow(gui_page(s,gui),GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
            if(gui_class(child)==L"Edit"&&chain_edit_text(s,gui,child).starts_with(L"mouse_count=")){edit=child;break;}
        if(!edit)throw std::runtime_error("M09_actual_mouse_not_hotkey_editor_missing");
        s.log.emit("M09_actual_mouse_editor","\"production_page\":3,\"control_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(edit))+
            ",\"actual_control_ID\":"+std::to_string(GetDlgCtrlID(edit))+",\"class\":\"Edit\",\"actual_prefix\":\"mouse_count=\",\"hotkey_editor_used\":false");
        chain_type_fragment(s,gui,edit,fragment);gui_save_enter(s,gui);const auto actual=chain_settings(s);wanted.generation=actual.generation;
        s.log.check("M09_actual_GUI_mouse_configuration_saved",actual==wanted&&actual.generation>before.generation);
    };
    const auto restore=[&]{
        const auto mark=read(s.product_log).size();
        s.log.check("M09_actual_pause_restores_owned_visual",launch(s,L"--pause",true)&&wait([&]{const auto row=record(s,mark,"status");
            const auto actual=native_visual(h);return row.find("\"enabled\":false")!=std::string::npos&&actual&&*actual==*original_visual;}));
        // Restore the actual topmost tuple BEFORE opening settings, so our
        // own test topmost source cannot obscure the real editor's hit target.
        if(configuration_changed){save_mouse(original.mouse_bindings);configuration_changed=false;}
        const auto resume=read(s.product_log).size();s.log.check("M09_actual_resume_preserves_original_configuration",launch(s,L"--command Pause",true)&&wait([&]{
            return record(s,resume,"status").find("\"enabled\":true")!=std::string::npos;}));
        const auto actual=chain_settings(s);auto expected=original;expected.generation=actual.generation;
        s.log.check("M09_original_full_configuration_and_visual_restored",actual==expected&&set.owns(0)&&visible(h)==initial&&
            native_visual(h)==original_visual,"\"original_mouse_fragment_characters\":"+std::to_string(original_fragment.size()));
    };
    try{
        for(unsigned which=1;which<=5;++which)client_pair(which,"before_caption_mapping");
        std::vector<v::MouseBinding> bindings;for(int which=1;which<=5;++which)bindings.push_back({0,which,1,0,v::Action::ToggleTopmost});
        configuration_changed=true;save_mouse(bindings);
        for(unsigned which=1;which<=5;++which){
            real_point(true);const auto mark=read(s.product_log).size();const auto before=counts();const auto raw_before=raw_mouse_observer_json();const auto before_visual=native_visual(h);
            if(!before_visual||!before_visual->known)throw std::runtime_error("M09_before_mapped_native_tuple_unknown");
            auto expected_visual=*before_visual;expected_visual.topmost=!expected_visual.topmost;
            send_button(which,true);std::string mapped,completed;std::uint64_t generation{};
            const bool changed=wait([&]{
                mapped.clear();std::istringstream rows(read(s.product_log).substr(mark));std::string row;
                while(std::getline(rows,row))if(row.ends_with('}')&&row.find("\"type\":\"input_button_observed\"")!=std::string::npos&&
                    row.find("\"source\":\"WH_MOUSE_LL\"")!=std::string::npos&&row.find("\"kind\":\"DOWN\"")!=std::string::npos&&
                    row.find("\"route\":\"mapped\"")!=std::string::npos&&row.find("\"action\":\"ToggleTopmost\"")!=std::string::npos&&
                    number(row,"\"button\":")==which&&number(row,"\"hook_observed_qpc\":"))mapped=row;
                generation=number(mapped,"\"generation\":");completed=record(s,mark,"mapped_command_completed",generation);
                const auto actual=native_visual(h);return generation&&completed.find("\"action\":\"ToggleTopmost\"")!=std::string::npos&&
                    completed.find("\"handler_invoked\":true")!=std::string::npos&&completed.find("\"handler_returned\":true")!=std::string::npos&&
                    actual&&*actual==expected_visual&&set.owns(0)&&visible(h)==initial;
            },1500);
            send_button(which,false);std::string observed_up;
            const bool up_observed=wait([&]{observed_up=actual_up(s,mark,generation,static_cast<int>(which));return !observed_up.empty();},1500);
            GUITHREADINFO info{sizeof(info)};const auto after=counts();const auto raw_after=raw_mouse_observer_json();
            s.log.check("M09_real_five_button_caption_mapping",changed&&up_observed&&after==before&&record(s,mark,"gesture_start",generation).empty()&&
                GetGUIThreadInfo(set.thread,&info)&&!info.hwndCapture&&!info.hwndMoveSize&&!info.hwndMenuOwner,
                "\"button\":"+std::to_string(which)+",\"generation\":"+std::to_string(generation)+",\"actual_mapped_DOWN\":"+(mapped.empty()?"null":quote(mapped))+
                ",\"actual_command_receipt\":"+(completed.empty()?"null":quote(completed))+",\"actual_matching_UP\":"+(observed_up.empty()?"null":quote(observed_up))+
                ",\"expected_native_topmost\":"+(expected_visual.topmost?"true":"false")+",\"complete_native_visual_tuple_exact\":"+(native_visual(h)==std::optional<NativeVisual>{expected_visual}?"true":"false")+
                ",\"actual_client_DOWN_UP_counts_before\":"+count_json(before)+",\"actual_client_DOWN_UP_counts_after\":"+count_json(after)+
                ",\"fixture_Raw_before\":"+raw_before+",\"fixture_Raw_after\":"+raw_after+",\"Raw_zero_does_not_fail_product_case\":true"+
                ",\"no_client_button_misdelivery\":"+(after==before?"true":"false")+",\"drag_NormalUp_or_native_success_from_handler_return_not_inferred\":true");
        }
        restore();client_pair(1,"after_original_mouse_and_visual_restored");chain_check_product(s,pid,birth);
        s.log.check("M09_five_button_owned_resources_closed",set.close());
        s.log.emit("M09_actual_five_button_mapping_sequence","\"title_single_click_buttons\":[1,2,3,4,5],\"actual_client_passthrough_buttons\":[1,2,3,4,5],\"actual_GUI_mouse_editor\":true,\"original_mouse_configuration_and_visual_restored\":true,\"human_uat\":\"NOT_RUN\"");
    }catch(...){
        if(pending_button)try{send_button(pending_button,false);}catch(...){s.log.failed=true;}
        try{restore();}catch(const std::exception& error){s.log.failed=true;s.log.emit("M09_mapping_recovery_failed","\"reason\":"+quote(error.what())+",\"cleanup_PASS\":false");}
        throw;
    }
}
void chain_i06_owned_core(State& s){
    const auto original=chain_settings(s);const HWND h=s.frames[0].hwnd;const auto old=native_visual(h);
    if(!own(s,h)||!old||!old->known)throw std::runtime_error("I11_I06_exact_original_visual_unknown");
    auto config=original;config.attraction_dip=0;config.drag_transparency=true;config.drag_alpha=190;config.shake=false;
    chain_gui_save(s,config);own_visual_setup(s,h,173,true);
    const auto before=visible(h);if(!before)throw std::runtime_error("I11_I06_geometry_unknown");
    auto g=down(s,0);visual_exact(s,h,190,true,"I11_I06_real_drag_alpha190");const auto target=translated(*before,-40,35);
    phase(s,0,g,-40,35,target,"I11_I06_real_transparent_Move",true);up(s,0,g,target);visual_exact(s,h,173,true,"I11_I06_real_UP_alpha173");
    config.drag_transparency=false;config.shake=true;config.shake_alpha=190;config.shake_amplitude_dip=24;config.shake_reversals=3;config.shake_window_ms=2000;
    chain_gui_save(s,config);own_visual_setup(s,h,173,false);const UINT dpi=GetDpiForWindow(h);if(!dpi)throw std::runtime_error("I11_I06_DPI_unknown");
    const LONG step=std::max<LONG>(48,MulDiv(48,static_cast<int>(dpi),96));
    for(unsigned n=0;n<2;++n){const auto start=visible(h);if(!start)throw std::runtime_error("I11_I06_shake_geometry_unknown");g=down(s,0);LONG dx{};
        for(unsigned k=0;k<5;++k){dx=k%2?-step:step;phase(s,0,g,dx,0,translated(*start,dx,0),"I11_I06_actual_shake_sample",true);}
        visual_exact(s,h,n?173:190,n==0,"I11_I06_actual_shake_tuple");up(s,0,g,translated(*start,dx,0));
    }
    s.log.check("I11_I06_actual_pause",launch(s,L"--pause",true));visual_exact(s,h,173,false,"I11_I06_pause_restores_actual_tuple");
    const auto mark=read(s.product_log).size();s.log.check("I11_I06_actual_resume",launch(s,L"--command Pause",true)&&
        wait([&]{return record(s,mark,"status").find("\"enabled\":true")!=std::string::npos;}));
    chain_gui_save(s,original);chain_restore_app_visual(s,h,*old);
    s.log.emit("I11_I06_core","\"actual_drag_UP_shake_on_off_pause_resume\":true,\"product_exit_tested_only_by_final_cleanup\":true");
}

// One-shot test scheduling, not product polling. It waits on this exact
// process plus a finite timer and pumps the existing driver STA for COM.
void chain_wait_until(State& s,std::uint64_t target,std::uint64_t frequency){
    LARGE_INTEGER current{};if(!frequency||!QueryPerformanceCounter(&current)||current.QuadPart<=0)throw std::runtime_error("I11_wait_clock_unavailable");
    if(static_cast<std::uint64_t>(current.QuadPart)>=target)return;
    const long double ticks=static_cast<long double>(target-static_cast<std::uint64_t>(current.QuadPart))*10000000.0L/frequency;
    if(ticks<=0||ticks>450000001.0L)throw std::runtime_error("I11_one_shot_wait_not_bounded_to_45_seconds");
    LARGE_INTEGER due{};due.QuadPart=-static_cast<LONGLONG>(std::ceil(ticks));
    Handle timer(CreateWaitableTimerW(nullptr,TRUE,nullptr));
    if(!timer.value||!SetWaitableTimer(timer.value,&due,0,nullptr,nullptr,FALSE))throw std::runtime_error("I11_one_shot_timer_failed");
    const HANDLE events[]{s.process,timer.value};
    for(;;){const auto result=MsgWaitForMultipleObjectsEx(2,events,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        if(result==WAIT_OBJECT_0)throw std::runtime_error("I11_product_exited_during_idle");
        if(result==WAIT_OBJECT_0+1)break;
        if(result!=WAIT_OBJECT_0+2)throw std::runtime_error("I11_event_wait_failed");
        MSG message{};unsigned budget=256;
        while(budget--&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
            if(message.message==WM_QUIT)throw std::runtime_error("I11_driver_quit");TranslateMessage(&message);DispatchMessageW(&message);}
    }
}
panebind::platform::windows::product::guest_test::IdleCounters chain_resource_sample(State& s,DWORD pid,std::uint64_t birth,const char* stage){
    namespace measure=panebind::platform::windows::product::guest_test;
    chain_check_product(s,pid,birth);measure::IdleMeasurement report;report.pid=pid;report.expected_birth=birth;measure::IdleCounters counters;
    if(!measure::idle_detail::capture(s.process,counters,report))throw std::runtime_error("I11_actual_resource_sample_"+report.reason);
    s.log.emit("I11_actual_resources","\"stage\":"+quote(stage)+",\"pid\":"+std::to_string(pid)+",\"birth\":"+std::to_string(birth)+
        ",\"qpc\":"+std::to_string(counters.qpc)+",\"handles\":"+std::to_string(counters.handles)+",\"GDI\":"+
        std::to_string(counters.gdi)+",\"USER\":"+std::to_string(counters.user)+",\"working_bytes\":"+std::to_string(counters.working_bytes)+
        ",\"private_commit_bytes\":"+std::to_string(counters.private_commit_bytes));return counters;
}
void chain_idle_facts(State& s){
    if(s.left||s.control||(GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_CONTROL)&0x8000))throw std::runtime_error("I11_actual_input_not_released");
    // Resize UP may correctly leave a read-only double-click hover preview.
    // Move the released pointer into the exact fixture's client before the
    // idle/overlay measurement; do not mistake that supported preview for a
    // leaked active-gesture overlay.
    const HWND h=s.frames[0].hwnd;foreground(s,h);RECT client{};POINT point{};
    if(!GetClientRect(h,&client))throw std::runtime_error("I11_idle_exact_client_geometry_unknown");
    point={(client.left+client.right)/2,(client.top+client.bottom)/2};
    if(!ClientToScreen(h,&point))throw std::runtime_error("I11_idle_client_screen_geometry_unknown");
    cursor(s,point);const HWND hit=WindowFromPoint(point);
    if(!hit||GetAncestor(hit,GA_ROOT)!=h)throw std::runtime_error("I11_idle_exact_client_hit_not_observed");
    const auto mark=read(s.product_log).size();
    s.log.check("I11_actual_product_idle_status",launch(s,L"--status",true)&&wait([&]{const auto row=record(s,mark,"status");
        return row.find("\"gesture_active\":false")!=std::string::npos&&row.find("\"enabled\":true")!=std::string::npos;}));
    chain_preview_hidden(s,"I11_actual_idle_overlay_hidden");GUITHREADINFO info{sizeof(info)};
    s.log.check("I11_actual_owned_thread_no_capture_or_modal",GetGUIThreadInfo(s.owner,&info)&&!info.hwndCapture&&!info.hwndMoveSize);
}
[[maybe_unused]] void owned_finite_I11_chain(State& s){
    namespace v=panebind::core::v1;
    if(s.configuration!="Release")throw std::runtime_error("I11_requires_actual_Release_configuration");
    const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    for(std::size_t n=0;n<3;++n)if(s.explorer[n]||s.applications[n]||!own(s,s.frames[n].hwnd))throw std::runtime_error("I11_exact_owned_scope_requires_other_bindings_released");
    auto config=original;config.enabled=true;config.attraction_dip=10;config.release_dip=16;config.magnet_move=true;config.magnet_resize=true;config.glue=true;
    config.drag_transparency=false;config.shake=false;chain_gui_save(s,config);
    (void)chain_resource_sample(s,pid,birth,"pre_core_rounds_after_existing_warmup");
    for(unsigned round=1;round<=2;++round){
        chain_check_product(s,pid,birth);chain_i02_owned_core(s,round);chain_i03_owned_core(s);owned_dock_stretch_chains(s);
        chain_i06_owned_core(s);chain_idle_facts(s);chain_check_product(s,pid,birth);
        s.log.emit("I11_actual_core_round_complete","\"round\":"+std::to_string(round)+",\"pid\":"+std::to_string(pid)+
            ",\"product_restarted\":false,\"scope\":\"owned_core_I02_I03_I04_I05_I06\",\"Explorer_and_cross_screen_verdicts_separate\":true");
    }
    const Rect work=chain_work(s.frames[0].hwnd);const LONG x=work.l+140,y=work.t+100;
    const std::array<Rect,3> layout{{{x,y,x+320,y+400},{x+320,y,x+640,y+200},{x+320,y+200,x+640,y+400}}};
    for(std::size_t n=0;n<3;++n)s.log.check("I11_mixed_initial_fixture_setup",fixture_rect(s.frames[n].hwnd,layout[n]));
    const auto original_visual=native_visual(s.frames[0].hwnd);if(!original_visual||!original_visual->known)throw std::runtime_error("I11_mixed_original_visual_unknown");
    config=chain_settings(s);config.attraction_dip=0;config.drag_transparency=true;config.drag_alpha=190;config.shake=false;
    chain_gui_save(s,config);own_visual_setup(s,s.frames[0].hwnd,173,false);chain_idle_facts(s);
    using Counters=product::guest_test::IdleCounters;std::vector<Counters> samples;
    samples.push_back(chain_resource_sample(s,pid,birth,"mixed_15min_start"));
    LARGE_INTEGER frequency{},started{};
    if(!QueryPerformanceFrequency(&frequency)||frequency.QuadPart<=0||frequency.QuadPart>std::numeric_limits<LONGLONG>::max()/901||
        !QueryPerformanceCounter(&started)||started.QuadPart<=0||static_cast<std::uint64_t>(started.QuadPart)>
        std::numeric_limits<std::uint64_t>::max()-static_cast<std::uint64_t>(frequency.QuadPart)*900)
        throw std::runtime_error("I11_actual_15min_clock_invalid");
    for(unsigned slot=0;slot<20;++slot){
        chain_check_product(s,pid,birth);const auto source=static_cast<std::size_t>(slot%3);const auto before=visible(s.frames[source].hwnd);
        if(!before)throw std::runtime_error("I11_mixed_source_geometry_unknown");
        if(slot%5==0||slot%5==4){
            auto g=down(s,source);const auto target=translated(*before,40,30);
            phase(s,source,g,40,30,target,"I11_mixed_real_Move",true);up(s,source,g,target);
            g=down(s,source);phase(s,source,g,-40,-30,*before,"I11_mixed_real_Move_back",true);up(s,source,g,*before);
            if(source==0)visual_exact(s,s.frames[0].hwnd,173,false,"I11_mixed_actual_drag_tuple_restored");
        }else if(slot%5==1){
            auto target=*before;target.b+=20;auto g=down(s,source,false,HTBOTTOM);
            phase(s,source,g,0,20,target,"I11_mixed_actual_Resize",true);up(s,source,g,target);
            g=down(s,source,false,HTBOTTOM);phase(s,source,g,0,-20,*before,"I11_mixed_actual_Resize_back",true);up(s,source,g,*before);
        }else if(slot%5==2){ctrl_move(s,source,20,10,"I11_mixed_real_Ctrl_Move");ctrl_move(s,source,-20,-10,"I11_mixed_real_Ctrl_Move_back");}
        else{
            foreground(s,s.frames[source].hwnd);const auto mark=read(s.product_log).size();const Rect center{
                work.l+(work.width()-before->width())/2,work.t+(work.height()-before->height())/2,
                work.l+(work.width()-before->width())/2+before->width(),work.t+(work.height()-before->height())/2+before->height()};
            s.log.check("I11_mixed_real_Center",launch(s,L"--command Center",true)&&wait([&]{return visible(s.frames[source].hwnd)==center&&!chain_any_placement(s,mark,center).empty();}));
            const auto undo=read(s.product_log).size();s.log.check("I11_mixed_real_Undo",launch(s,L"--command Undo",true)&&wait([&]{return visible(s.frames[source].hwnd)==before&&!chain_any_placement(s,undo,*before).empty();}));
        }
        chain_idle_facts(s);
        const auto deadline=static_cast<std::uint64_t>(started.QuadPart)+static_cast<std::uint64_t>(frequency.QuadPart)*45*(slot+1);
        LARGE_INTEGER before_idle{};QueryPerformanceCounter(&before_idle);
        if(before_idle.QuadPart<=0||static_cast<std::uint64_t>(before_idle.QuadPart)>=deadline)throw std::runtime_error("I11_action_exceeded_its_finite_45_second_slot");
        s.log.emit("I11_actual_mixed_action_complete","\"slot\":"+std::to_string(slot)+",\"source\":"+std::to_string(source)+
            ",\"qpc\":"+std::to_string(before_idle.QuadPart)+",\"next_one_shot_deadline_qpc\":"+std::to_string(deadline));
        chain_wait_until(s,deadline,static_cast<std::uint64_t>(frequency.QuadPart));chain_check_product(s,pid,birth);
        if(slot==6||slot==13||slot==19)samples.push_back(chain_resource_sample(s,pid,birth,slot==6?"mixed_5min15s":slot==13?"mixed_10min30s":"mixed_15min_end"));
    }
    LARGE_INTEGER ended{};QueryPerformanceCounter(&ended);const double seconds=static_cast<double>(ended.QuadPart-started.QuadPart)/frequency.QuadPart;
    chain_idle_facts(s);s.log.check("I11_actual_same_process_15_minutes_elapsed",seconds>=900&&samples.size()==4);
    const auto increasing=[&](auto member){for(std::size_t n=1;n<samples.size();++n)if(samples[n].*member<=samples[n-1].*member)return false;return true;};
    const bool handles_increasing=increasing(&Counters::handles),gdi_increasing=increasing(&Counters::gdi),user_increasing=increasing(&Counters::user);
    chain_gui_save(s,original);chain_restore_app_visual(s,s.frames[0].hwnd,*original_visual);
    s.log.emit("I11_finite_actual_sequence","\"result\":\"CORE_AND_15MIN_COMPLETE_FINAL_EXIT_AND_RESOURCE_REVIEW_PENDING\",\"pid\":"+
        std::to_string(pid)+",\"birth\":"+std::to_string(birth)+",\"elapsed_seconds\":"+std::to_string(seconds)+
        ",\"core_rounds\":2,\"actual_mixed_action_slots\":20,\"strictly_increasing_handles\":"+(handles_increasing?"true":"false")+
        ",\"strictly_increasing_GDI\":"+(gdi_increasing?"true":"false")+",\"strictly_increasing_USER\":"+(user_increasing?"true":"false")+
        ",\"finite_caches_need_actual_explanation\":true,\"full_stability_or_Explorer_pass_inferred\":false,\"final_exit_checked_by_normal_fixture_cleanup\":true");
}
