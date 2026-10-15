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
        const auto actual=catalog.get(set.frames[0].hwnd,config);if(!actual)throw std::runtime_error("I04_cross_display_source_identity_unknown");
        const auto from=std::find_if(displays.begin(),displays.end(),[&](const auto& d){return d.monitor==actual->monitor;});
        if(from==displays.end()||!from->dpi)throw std::runtime_error("I04_cross_display_source_DPI_unknown");
        const auto& to=displays[(static_cast<std::size_t>(from-displays.begin())+1)%displays.size()];
        if(!to.dpi)throw std::runtime_error("I04_cross_display_destination_DPI_unknown");
        const auto scaled=[&](LONG px){return (static_cast<long long>(px)*to.dpi+from->dpi/2)/from->dpi;};
        const auto max_width=std::min(actual->limits.max_width,to.work.width()),max_height=std::min(actual->limits.max_height,to.work.height());
        if(max_width<actual->limits.min_width||max_height<actual->limits.min_height)
            throw std::runtime_error("I04_cross_display_actual_size_constraints_have_no_intersection");
        const auto width=std::clamp<std::int64_t>(scaled(restored.width()),actual->limits.min_width,max_width);
        const auto height=std::clamp<std::int64_t>(scaled(restored.height()),actual->limits.min_height,max_height);
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
    layout_phase(set,0,g,-40,35,{translated(restored,-40,35),neighbour},"I04_post_dock_real_free");
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
