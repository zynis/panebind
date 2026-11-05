#pragma once

// TEST ONLY: five finite demonstrations reuse the ordinary guarded production
// path and independent geometry oracles. PNGs are original desktop pixels.
// A frame is illustration; the adjacent actual receipts decide the test.
void review_capture_scene(State& s,std::string_view scene,std::string_view phase_name,const std::vector<HWND>& roots,
                          const std::function<bool()>& exact){
    if(s.suite!="SettingsEditorsOnly"||!desktop()||!s.process||!medium(s.process)||roots.empty()||roots.size()>8||!exact()||
       scene.empty()||scene.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-")!=scene.npos||
       phase_name.empty()||phase_name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-")!=phase_name.npos)
        throw std::runtime_error("review_scene_exact_guest_objects_required");
    const auto foreground=GetForegroundWindow();
    if(std::find(roots.begin(),roots.end(),foreground)==roots.end())throw std::runtime_error("review_scene_foreground_not_selected_object");
    Rect region{};bool first=true;std::string identities="[";
    for(const auto h:roots){DWORD pid{};const auto tid=GetWindowThreadProcessId(h,&pid);RECT r{};const auto dpi=GetDpiForWindow(h);
        if(!h||!tid||!dpi||!IsWindowVisible(h)||!GetWindowRect(h,&r)||!exact())throw std::runtime_error("review_scene_current_object_unknown");
        if(first){region={r.left,r.top,r.right,r.bottom};first=false;}else{region.l=std::min(region.l,r.left);region.t=std::min(region.t,r.top);region.r=std::max(region.r,r.right);region.b=std::max(region.b,r.bottom);}
        if(identities.size()>1)identities+=',';identities+="{\"HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
            ",\"PID\":"+std::to_string(pid)+",\"TID\":"+std::to_string(tid)+",\"dpi\":"+std::to_string(dpi)+
            ",\"positioning_rect\":"+rect({r.left,r.top,r.right,r.bottom})+",\"visible_rect\":"+(visible(h)?rect(*visible(h)):"null")+"}";
    }identities+=']';
    const auto preview=chain_preview(s);if(preview&&IsWindowVisible(preview)){DWORD pid{};GetWindowThreadProcessId(preview,&pid);RECT r{};
        if(pid!=s.product_pid||!GetWindowRect(preview,&r))throw std::runtime_error("review_scene_preview_identity_unknown");
        region.l=std::min(region.l,r.left);region.t=std::min(region.t,r.top);region.r=std::max(region.r,r.right);region.b=std::max(region.b,r.bottom);
    }
    const LONG x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN);
    const LONG w=GetSystemMetrics(SM_CXVIRTUALSCREEN),h=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    region={std::max(x,region.l),std::max(y,region.t),std::min(x+w,region.r),std::min(y+h,region.b)};
    if(region.width()<=0||region.height()<=0||region.width()>4096||region.height()>4096||std::uint64_t(region.width())*region.height()>8ULL*1024*1024)
        throw std::runtime_error("review_scene_actual_union_out_of_bound");
    const auto file=s.run+L"-"+std::wstring(s.configuration.begin(),s.configuration.end())+L"-demo-"+
        std::wstring(scene.begin(),scene.end())+L"-"+std::wstring(phase_name.begin(),phase_name.end())+L".png";
    const auto path=s.output/file;if(!plain_directory(s.output)||fs::exists(path))throw std::runtime_error("review_scene_new_PNG_required");
    struct Pixels {HDC screen{},memory{};HBITMAP bitmap{};HGDIOBJ previous{};
        ~Pixels(){if(memory&&previous&&previous!=HGDI_ERROR)SelectObject(memory,previous);if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);if(screen)ReleaseDC(nullptr,screen);}} pixels;
    const auto presented=DwmFlush();pixels.screen=GetDC(nullptr);if(!pixels.screen)throw std::runtime_error("review_scene_GetDC_failed");
    pixels.memory=CreateCompatibleDC(pixels.screen);pixels.bitmap=CreateCompatibleBitmap(pixels.screen,region.width(),region.height());
    if(!pixels.memory||!pixels.bitmap)throw std::runtime_error("review_scene_GDI_allocation_failed");
    pixels.previous=SelectObject(pixels.memory,pixels.bitmap);
    if(!pixels.previous||pixels.previous==HGDI_ERROR||FAILED(presented)||!exact()||GetForegroundWindow()!=foreground||
       !BitBlt(pixels.memory,0,0,region.width(),region.height(),pixels.screen,region.l,region.t,SRCCOPY|CAPTUREBLT)||!GdiFlush())
        throw std::runtime_error("review_scene_fresh_guard_or_actual_BitBlt_failed");
    review_png(path,pixels.bitmap,static_cast<UINT>(region.width()),static_cast<UINT>(region.height()));
    s.log.emit("settings_demo_actual_PNG","\"scene\":"+quote(scene)+",\"phase\":"+quote(phase_name)+
        ",\"product_source_sha\":"+quote(s.product_source)+",\"product_EXE_sha256\":"+quote(hash(s.product))+
        ",\"driver_source_sha\":"+quote(PANEBIND_BUILD_SHA)+",\"objects\":"+identities+
        ",\"screen_region\":"+rect(region)+",\"file\":"+quote(utf8(file))+",\"sha256\":"+quote(hash(path))+
        ",\"raw_original_pixels\":true,\"retouched\":false,\"action_receipt_required_separately\":true,\"human_UAT\":\"NOT_RUN\"");
}
void owned_settings_ui_demo_case(State& s){
    settings_guest_scope(s);const auto baseline=chain_settings(s);
    if(!baseline.enabled||!baseline.magnet_move||!baseline.glue||!baseline.docking||!baseline.preview||
       baseline.attraction_dip<6||baseline.release_dip<12)throw std::runtime_error("settings_demo_required_actual_options_not_enabled");
    const auto source_pid=s.product_pid;const auto source_birth=process_birth(s.process);
    LayoutFixtures set{s};set.create();
    const auto exact=[&]{if(!s.process||s.product_pid!=source_pid||process_birth(s.process)!=source_birth||WaitForSingleObject(s.process,0)!=WAIT_TIMEOUT)return false;
        for(std::size_t i=0;i<set.active;++i)if(!set.owns(i))return false;return true;};
    const auto roots=[&]{std::vector<HWND> result;for(std::size_t i=0;i<set.active;++i)result.push_back(set.frames[i].hwnd);return result;};
    try{
        const auto work=chain_work(set.frames[0].hwnd);const LayoutLimit limit{120,100,work.width(),work.height()};
        if(work.width()<1100||work.height()<760)throw std::runtime_error("settings_demo_actual_work_area_too_small");
        const std::vector<Rect> separated{{120,80,520,440},{540,80,800,260},{540,260,800,440}};
        set.setup(separated,{limit,limit,limit});chain_front(set,0);review_capture_scene(s,"01-magnet","before",roots(),exact);
        auto g=layout_down(set,0,HTCAPTION,false);auto expected=separated;expected[0]=translated(separated[0],-40,40);
        layout_phase(set,0,g,-40,40,expected,"T10_plain_free");expected[0]=translated(separated[0],20,0);
        layout_phase(set,0,g,14,3,expected,"T10_plain_XY_snap");review_capture_scene(s,"01-magnet","during",roots(),exact);
        layout_phase(set,0,g,12,5,expected,"T10_plain_hold");expected[0]=translated(separated[0],-40,40);
        layout_phase(set,0,g,-40,40,expected,"T10_plain_detach");expected[0]=translated(separated[0],20,0);
        layout_phase(set,0,g,15,4,expected,"T10_plain_reacquire");layout_up(set,g,expected);
        review_capture_scene(s,"01-magnet","after",roots(),exact);

        const std::vector<Rect> adjacent{{120,80,520,440},{520,80,780,260},{520,260,780,440}};
        set.setup(adjacent,{limit,limit,limit});chain_front(set,0);review_capture_scene(s,"02-ctrl-group","before",roots(),exact);
        g=layout_down(set,0,HTCAPTION,true);expected=adjacent;for(auto& r:expected)r=translated(r,30,20);
        layout_phase(set,0,g,30,20,expected,"T10_actual_three_member_Ctrl_Move");
        review_capture_scene(s,"02-ctrl-group","during",roots(),exact);layout_up(set,g,expected);
        review_capture_scene(s,"02-ctrl-group","after",roots(),exact);

        const std::vector<Rect> pair{{120,80,520,440},{520,80,920,440}};
        set.setup(pair,{limit,limit});chain_front(set,0);review_capture_scene(s,"03-shared-resize","before",roots(),exact);
        g=layout_down(set,0,HTRIGHT,true);const std::vector<Rect> resized{{120,80,560,440},{560,80,920,440}};
        layout_phase(set,0,g,40,0,resized,"T10_actual_two_member_Ctrl_shared_resize");
        review_capture_scene(s,"03-shared-resize","during",roots(),exact);layout_up(set,g,resized);
        review_capture_scene(s,"03-shared-resize","after",roots(),exact);

        // Isolate the preview from smart target splits through the actual
        // public checkboxes; restore these options immediately after this clip.
        auto gui=gui_open(s);gui_page_select(s,gui,1);chain_checkbox(s,gui,L"移动时磁吸",false);
        gui_page_select(s,gui,2);chain_checkbox(s,gui,L"停靠时参考",false);gui_save_enter(s,gui);
        const Rect initial{work.l+260,work.t+160,work.l+660,work.t+460};set.setup({initial},{limit});chain_front(set,0);
        review_capture_scene(s,"04-dock-preview","before",roots(),exact);g=layout_down(set,0,HTCAPTION,false);
        const POINT edge{work.l+1,work.t+work.height()/2};const auto raw=translated(initial,edge.x-g.anchor.x,edge.y-g.anchor.y);
        layout_phase(set,0,g,edge.x-g.anchor.x,edge.y-g.anchor.y,{raw},"T10_actual_left_preview_before_UP");
        const Rect half{work.l,work.t,work.l+work.width()/2,work.b};chain_preview_exact(s,set.frames[0].hwnd,half);
        review_capture_scene(s,"04-dock-preview","during",roots(),exact);layout_up(set,g,{half});chain_preview_hidden(s,"T10_actual_UP_preview_destroyed");
        const auto committed=placement(s,g.offset,g.generation,half,true);
        s.log.check("T10_actual_NormalUp_dock_commit",!committed.empty()&&committed.find("\"intent_reason\":\"normal_up_dock\"")!=committed.npos,
            "\"receipt\":"+quote(committed));
        chain_hotkey_exact(set,'U',raw,"T10_actual_Undo_of_dock_commit");
        review_capture_scene(s,"04-dock-preview","after",roots(),exact);
        gui=gui_open(s);gui_page_select(s,gui,1);chain_checkbox(s,gui,L"移动时磁吸",baseline.magnet_move);
        gui_page_select(s,gui,2);chain_checkbox(s,gui,L"停靠时参考",baseline.smart_docking);gui_save_enter(s,gui);
        s.log.check("T10_temporary_preview_options_restored",chain_settings(s).magnet_move==baseline.magnet_move&&chain_settings(s).smart_docking==baseline.smart_docking);

        const auto live=chain_settings(s);const auto existing=std::find_if(live.hotkeys.begin(),live.hotkeys.end(),[](const auto& h){return h.modifiers==3&&h.key=='J'&&h.action==v1::Action::Center;});
        if(existing==live.hotkeys.end())throw std::runtime_error("settings_demo_requires_T02_saved_CtrlAltJ");
        const auto index=static_cast<std::size_t>(existing-live.hotkeys.begin());gui=gui_open(s);gui_page_select(s,gui,3);
        review_capture_scene(s,"05-hotkey-editor","before",{gui.root},[&]{return exact()&&gui_identity(s,gui,gui.root)&&GetForegroundWindow()==gui.root;});review_list_select(s,gui,1200,index);const auto editor=review_open_editor(s,gui,1202);
        review_editor_combo(s,gui,editor,1300,'K');
        review_capture_scene(s,"05-hotkey-editor","during",{editor.root},[&]{return exact()&&review_editor_identity(s,gui,editor,editor.root)&&GetForegroundWindow()==editor.root;});
        review_editor_finish(s,gui,editor,true);gui_save_enter(s,gui);
        const Rect command_initial{work.l+200,work.t+150,work.l+600,work.t+450};set.setup({command_initial},{limit});
        const Rect center{work.l+(work.width()-command_initial.width())/2,work.t+(work.height()-command_initial.height())/2,0,0};
        const Rect centered{center.l,center.t,center.l+command_initial.width(),center.t+command_initial.height()};
        chain_hotkey_exact(set,'K',centered,"T10_new_structured_editor_key_K_actual_Center");
        review_capture_scene(s,"05-hotkey-editor","after",roots(),exact);
        s.log.check("T10_all_owned_demo_windows_closed",set.close());
        s.log.emit("settings_ui_T10_scope","\"five_demonstration_segments_returned\":true,\"original_size_frames_and_actual_geometry_receipts\":true,\"I02_I03_or_app_matrix_PASS_inferred\":false,\"human_UAT\":\"NOT_RUN\"");
    }catch(...){pause_routing_failed_case_input(s);try{if(!set.closed)set.close();}catch(...){}throw;}
}

void owned_settings_ui_demo_after_portable(State& s){
    if(s.process)throw std::runtime_error("settings_demo_requires_T09_completed_normal_exit");
    const auto suffix=s.configuration=="Debug"?L"Debug":L"Release";
    const auto expected=s.output/(s.run+L"-product-"+suffix)/L"panebind.exe";
    if(s.product!=expected||!plain_directory(s.product.parent_path())||hash(s.product)!=hash(s.input_product))
        throw std::runtime_error("settings_demo_original_Run_copy_identity_not_exact");
    const auto mark=read_product(s).size();s.log.check("T10_same_candidate_restart_after_T09_normal_exit",launch(s,L"--diagnostic",false)&&wait([&]{
        const auto row=record(s,mark,"startup");return number(row,"\"process_id\":")==s.product_pid&&row.find("\"source_sha\":"+quote(s.product_source))!=row.npos;},8000));
    owned_settings_ui_demo_case(s);
}
