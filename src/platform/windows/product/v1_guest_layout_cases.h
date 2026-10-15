#pragma once

// Guest-only cases: include inside v1_guest_fixture.cpp's unnamed namespace,
// AFTER its State/Frame, native input, evidence and geometry helpers. This is
// not a product header or an alternate solver. All expected rectangles below
// are independent contract values; only the real product computes placements.
// Deliberately not called until the parent selects this L2 phase.

struct LayoutLimit {LONG min_width{100},min_height{100},max_width{2000},max_height{1400};};
struct LayoutFixtures {
    State& state;std::array<Frame,8> frames;std::array<bool,3> old_visible{},visibility_saved{};
    DWORD thread{GetCurrentThreadId()};std::size_t active{};bool closed{};
    explicit LayoutFixtures(State& s):state(s){}
    bool owns(std::size_t index)const{
        if(index>=frames.size())return false;const auto& frame=frames[index];DWORD pid{};
        return frame.hwnd&&IsWindow(frame.hwnd)&&GetWindowThreadProcessId(frame.hwnd,&pid)==thread&&pid==GetCurrentProcessId()&&
            GetAncestor(frame.hwnd,GA_ROOT)==frame.hwnd&&gui_class(frame.hwnd)==state.class_name&&
            GetWindowLongPtrW(frame.hwnd,GWLP_USERDATA)==reinterpret_cast<LONG_PTR>(&frame);
    }
    void create(){
        if(!state.process||WaitForSingleObject(state.process,0)!=WAIT_TIMEOUT)throw std::runtime_error("layout_product_not_running");
        for(std::size_t n=0;n<3;++n){
            if(state.explorer[n]||state.applications[n]||!own(state,state.frames[n].hwnd))throw std::runtime_error("layout_other_targets_must_be_released_first");
            old_visible[n]=IsWindowVisible(state.frames[n].hwnd)!=FALSE;visibility_saved[n]=true;
            ShowWindow(state.frames[n].hwnd,SW_HIDE);
        }
        for(std::size_t n=0;n<frames.size();++n){
            auto& frame=frames[n];const auto title=L"PaneBind V1 layout — "+std::to_wstring(n+1);
            frame.hwnd=CreateWindowExW(0,state.class_name.c_str(),title.c_str(),WS_OVERLAPPEDWINDOW,
                100+static_cast<int>(n)*20,100,400,400,nullptr,nullptr,state.instance,&frame);
            if(!frame.hwnd)throw std::runtime_error("layout_test_window_creation_failed");
            ShowWindow(frame.hwnd,SW_SHOWNOACTIVATE);UpdateWindow(frame.hwnd);
            if(!wait([&]{return owns(n)&&visible(frame.hwnd).has_value();}))throw std::runtime_error("layout_actual_frame_unavailable");
        }
        state.log.emit("layout_owned_scope","\"windows\":8,\"pid\":"+std::to_string(GetCurrentProcessId())+
            ",\"tid\":"+std::to_string(thread)+",\"original_driver_frames_hidden\":true,\"app_grid\":1,\"automatic_guest_driver\":true");
    }
    void setup(const std::vector<Rect>& expected,const std::vector<LayoutLimit>& limits={}){
        if(expected.empty()||expected.size()>frames.size()||(!limits.empty()&&limits.size()!=expected.size()))throw std::runtime_error("layout_case_bound_invalid");
        if(state.left||state.control)throw std::runtime_error("layout_setup_requires_released_input");active=expected.size();
        for(std::size_t n=0;n<frames.size();++n){
            if(!owns(n))throw std::runtime_error("layout_owned_identity_changed");
            if(n>=active){ShowWindow(frames[n].hwnd,SW_HIDE);continue;}
            const auto observed=visible(frames[n].hwnd);RECT positioning{};
            if(!observed||!GetWindowRect(frames[n].hwnd,&positioning))throw std::runtime_error("layout_frame_insets_unknown");
            const LONG dx=positioning.right-positioning.left-observed->width(),dy=positioning.bottom-positioning.top-observed->height();
            const auto bound=limits.empty()?LayoutLimit{}:limits[n];
            if(bound.min_width<=0||bound.min_height<=0||bound.max_width<bound.min_width||bound.max_height<bound.min_height||
                bound.min_width+dx<=0||bound.min_height+dy<=0)throw std::runtime_error("layout_test_constraint_invalid");
            // These HWNDs belong to THIS driver thread, so their WndProc cannot
            // concurrently read a partially changed non-atomic Frame contract.
            frames[n].min_width=bound.min_width+dx;frames[n].min_height=bound.min_height+dy;
            frames[n].max_width=bound.max_width+dx;frames[n].max_height=bound.max_height+dy;frames[n].grid=1;
            ShowWindow(frames[n].hwnd,SW_SHOWNOACTIVATE);
            state.log.check("layout_actual_independent_setup",fixture_rect(frames[n].hwnd,expected[n]),
                "\"member\":"+std::to_string(n)+",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(frames[n].hwnd))+
                ",\"expected_visible\":"+rect(expected[n])+",\"declared_visible_min\":["+std::to_string(bound.min_width)+","+
                std::to_string(bound.min_height)+"],\"declared_visible_max\":["+std::to_string(bound.max_width)+","+
                std::to_string(bound.max_height)+"],\"native_grid\":1,\"not_claiming_unknown_application_grid\":true");
        }
    }
    bool close(){
        if(closed)return true;closed=true;bool exact=true;
        for(std::size_t n=0;n<frames.size();++n)if(frames[n].hwnd){
            if(!owns(n)){exact=false;continue;}
            const HWND h=frames[n].hwnd;exact=(DestroyWindow(h)!=FALSE&&!IsWindow(h))&&exact;frames[n].hwnd=nullptr;
        }
        for(std::size_t n=0;n<3;++n)if(visibility_saved[n]){if(own(state,state.frames[n].hwnd)){
            ShowWindow(state.frames[n].hwnd,old_visible[n]?SW_SHOWNOACTIVATE:SW_HIDE);
            exact=((IsWindowVisible(state.frames[n].hwnd)!=FALSE)==old_visible[n])&&exact;
        }else exact=false;}
        return exact;
    }
    ~LayoutFixtures(){if(!closed)close();} // test-owned windows only, not product cleanup PASS
};

std::optional<POINT> layout_hit(LayoutFixtures& set,std::size_t source,int required){
    if(!set.owns(source))return {};const HWND h=set.frames[source].hwnd;RECT p{};if(!GetWindowRect(h,&p))return {};
    const bool left=required==HTLEFT||required==HTTOPLEFT||required==HTBOTTOMLEFT;
    const bool right=required==HTRIGHT||required==HTTOPRIGHT||required==HTBOTTOMRIGHT;
    const bool top=required==HTTOP||required==HTTOPLEFT||required==HTTOPRIGHT;
    const bool bottom=required==HTBOTTOM||required==HTBOTTOMLEFT||required==HTBOTTOMRIGHT;
    LONG x0=left?p.left:right?p.right-20:(p.left+p.right)/2-8;
    LONG x1=left?p.left+20:right?p.right:(p.left+p.right)/2+9;
    LONG y0=top?p.top:bottom?p.bottom-20:(p.top+p.bottom)/2-8;
    LONG y1=top?p.top+20:bottom?p.bottom:(p.top+p.bottom)/2+9;
    if(required==HTCAPTION){x0=p.left+16;x1=p.right-16;y0=p.top+3;y1=std::min<LONG>(p.bottom,p.top+64);}
    for(LONG y=y0;y<y1;++y)for(LONG x=x0;x<x1;++x){
        const POINT point{x,y};if(WindowFromPoint(point)!=h)continue;DWORD_PTR hit{};
        if(SendMessageTimeoutW(h,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(x),static_cast<short>(y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&hit)&&
            static_cast<int>(hit)==required)return point;
    }return {};
}
Gesture layout_down(LayoutFixtures& set,std::size_t source,int region,bool ctrl){
    auto& s=set.state;if(source>=set.active||!set.owns(source)||s.left||s.control)throw std::runtime_error("layout_source_not_exact_or_input_busy");
    const HWND h=set.frames[source].hwnd;
    s.log.check("layout_exact_source_foreground",SetForegroundWindow(h)&&wait([&]{return GetForegroundWindow()==h;}));
    const auto point=layout_hit(set,source,region);if(!point)throw std::runtime_error("layout_real_requested_HT_region_unavailable");
    cursor(s,*point);Sleep(100);if(ctrl)key(s,VK_CONTROL,true);
    Gesture g{*point,0,read(s.product_log).size()};button(s,true);
    const bool started=wait([&]{g.generation=number(record(s,g.offset,"gesture_start"),"\"generation\":");return g.generation!=0;});
    const auto start=record(s,g.offset,"gesture_start",g.generation);
    s.log.check("layout_real_DOWN_production_start",started&&start.find(ctrl?"\"ctrl\":true":"\"ctrl\":false")!=std::string::npos&&
        start.find(region==HTCAPTION?"\"resize\":false":"\"resize\":true")!=std::string::npos,
        "\"source\":"+std::to_string(source)+",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
        ",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"tid\":"+std::to_string(set.thread)+
        ",\"native_hit\":"+std::to_string(region)+",\"generation\":"+std::to_string(g.generation));return g;
}
void layout_phase(LayoutFixtures& set,std::size_t source,const Gesture& g,LONG dx,LONG dy,const std::vector<Rect>& expected,std::string name){
    auto& s=set.state;if(expected.size()!=set.active)throw std::runtime_error("layout_expected_member_count");
    std::vector<Rect> before;for(std::size_t n=0;n<set.active;++n){const auto actual=visible(set.frames[n].hwnd);
        if(!set.owns(n)||!actual)throw std::runtime_error("layout_member_identity_or_frame_lost");before.push_back(*actual);}
    Sleep(60);const auto mark=read(s.product_log).size();cursor(s,{g.anchor.x+dx,g.anchor.y+dy});
    const bool exact=wait([&]{
        for(std::size_t n=0;n<set.active;++n){
            if(!set.owns(n)||visible(set.frames[n].hwnd)!=expected[n])return false;
            if((before[n]!=expected[n]||n==source)&&placement(s,mark,g.generation,expected[n],before[n]!=expected[n]).empty())return false;
        }return true;
    });
    s.log.check(std::move(name),exact,"\"generation\":"+std::to_string(g.generation)+",\"cursor_total_delta\":["+
        std::to_string(dx)+","+std::to_string(dy)+"],\"participants\":"+std::to_string(set.active)+",\"actual_native_receipts_required\":true");
    for(std::size_t n=0;n<set.active;++n)s.log.emit("layout_actual_member","\"member\":"+std::to_string(n)+
        ",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(set.frames[n].hwnd))+",\"expected\":"+rect(expected[n])+
        ",\"actual\":"+rect(*visible(set.frames[n].hwnd)));
}
void layout_up(LayoutFixtures& set,const Gesture& g,const std::vector<Rect>& expected){
    auto& s=set.state;const auto mark=read(s.product_log).size();button(s,false);if(s.control)key(s,VK_CONTROL,false);
    s.log.check("layout_actual_matching_UP",wait([&]{return record(s,mark,"gesture_end",g.generation).find("\"normal_up\":true")!=std::string::npos;}));
    Sleep(100);GUITHREADINFO gui{sizeof(gui)};bool stable=GetGUIThreadInfo(set.thread,&gui)&&!gui.hwndCapture&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000);
    for(std::size_t n=0;n<expected.size();++n)stable=stable&&set.owns(n)&&visible(set.frames[n].hwnd)==expected[n];
    s.log.check("layout_UP_actual_geometry_and_no_capture",stable,"\"generation\":"+std::to_string(g.generation));
}

[[maybe_unused]] void owned_layout_cases(State& s){
    LayoutFixtures set{s};set.create();
    // G06 vertical, independent visible min widths 250 / 300. Both sides
    // initiate a real Ctrl resize, and the original DOWN remains the anchor.
    const std::vector<Rect> pair{{100,100,500,500},{500,100,900,500}};
    const std::vector<LayoutLimit> pair_limits{{250,100,1000,1000},{300,100,1000,1000}};
    set.setup(pair,pair_limits);auto g=layout_down(set,0,HTRIGHT,true);
    layout_phase(set,0,g,80,0,{{100,100,580,500},{580,100,900,500}},"G06_vertical_plus80");
    const std::vector<Rect> clamped{{100,100,600,500},{600,100,900,500}};
    layout_phase(set,0,g,180,0,clamped,"G06_vertical_min_intersection_clamp600");layout_up(set,g,clamped);
    set.setup(pair,pair_limits);g=layout_down(set,1,HTLEFT,true);
    layout_phase(set,1,g,80,0,{{100,100,580,500},{580,100,900,500}},"G06_opposite_source_same_shared_line");
    layout_up(set,g,{{100,100,580,500},{580,100,900,500}});
    set.setup({{100,100,500,500},{100,500,500,900}},{{100,250,1000,1000},{100,300,1000,1000}});
    g=layout_down(set,0,HTBOTTOM,true);
    layout_phase(set,0,g,0,80,{{100,100,500,580},{100,580,500,900}},"G06_horizontal_plus80");
    layout_phase(set,0,g,0,180,{{100,100,500,600},{100,600,500,900}},"G06_horizontal_min_intersection_clamp600");
    layout_up(set,g,{{100,100,500,600},{100,600,500,900}});
    set.setup(pair,{{250,100,1000,1000},{400,400,400,400}});
    g=layout_down(set,0,HTRIGHT,true);Sleep(60);cursor(s,{g.anchor.x+80,g.anchor.y});
    layout_up(set,g,pair);
    s.log.check("shared_fixed_size_neighbour_stops_resize_without_placement",record(s,g.offset,"placement",g.generation).empty(),
        "\"fixed_member_visible_size\":[400,400],\"actual_DOWN_Move_UP\":true");

    // I03: one continuous running product, T vertical -> horizontal -> min
    // -> pull back -> actual ordinary Move -> form L -> changed leader.
    set.setup({{100,100,500,500},{500,100,900,300},{500,300,900,500}},
        {{250,100,600,800},{300,100,600,800},{250,100,600,800}});
    g=layout_down(set,0,HTRIGHT,true);
    layout_phase(set,0,g,60,0,{{100,100,560,500},{560,100,900,300},{560,300,900,500}},"I03_T_shared_vertical560");
    layout_up(set,g,{{100,100,560,500},{560,100,900,300},{560,300,900,500}});
    g=layout_down(set,1,HTBOTTOM,true);
    layout_phase(set,1,g,0,30,{{100,100,560,500},{560,100,900,330},{560,330,900,500}},"I03_T_shared_horizontal330");
    layout_up(set,g,{{100,100,560,500},{560,100,900,330},{560,330,900,500}});
    g=layout_down(set,1,HTLEFT,true);
    layout_phase(set,1,g,180,0,{{100,100,600,500},{600,100,900,330},{600,330,900,500}},"I03_T_reaches_member_min");
    const std::vector<Rect> pulled{{100,100,540,500},{540,100,900,330},{540,330,900,500}};
    layout_phase(set,1,g,-20,0,pulled,"I03_T_pullback_uses_original_DOWN");layout_up(set,g,pulled);
    g=layout_down(set,0,HTCAPTION,false);
    layout_phase(set,0,g,-40,35,{{60,135,500,535},pulled[1],pulled[2]},"I03_post_Resize_ordinary_Move");
    layout_up(set,g,{{60,135,500,535},pulled[1],pulled[2]});
    g=layout_down(set,0,HTCAPTION,false);layout_phase(set,0,g,40,-35,pulled,"I03_actual_Move_reforms_contact");layout_up(set,g,pulled);
    ShowWindow(set.frames[2].hwnd,SW_HIDE);set.active=2;g=layout_down(set,1,HTLEFT,true);
    const std::vector<Rect> ell{{100,100,560,500},{560,100,900,330}};
    layout_phase(set,1,g,20,0,ell,"I03_L_shared_line_no_missing_window");layout_up(set,g,ell);
    g=layout_down(set,1,HTCAPTION,true);layout_phase(set,1,g,40,30,{{140,130,600,530},{600,130,940,360}},"I03_L_changed_leader_Ctrl_Move");
    layout_up(set,g,{{140,130,600,530},{600,130,940,360}});

    set.setup({{100,100,500,300},{500,100,900,300},{100,300,500,500},{500,300,900,500}},
        {{250,150,500,350},{300,100,500,350},{220,140,500,350},{280,130,500,350}});
    g=layout_down(set,0,HTBOTTOMRIGHT,true);
    const std::vector<Rect> four{{100,100,540,330},{540,100,900,330},{100,330,540,500},{540,330,900,500}};
    layout_phase(set,0,g,40,30,four,"G07_four_grid_two_shared_axes");layout_up(set,g,four);
    g=layout_down(set,3,HTCAPTION,false);
    layout_phase(set,3,g,40,35,{four[0],four[1],four[2],{580,365,940,535}},"G07_four_grid_following_ordinary_Move");
    layout_up(set,g,{four[0],four[1],four[2],{580,365,940,535}});

    std::vector<Rect> eight,moved;
    for(LONG n=0;n<8;++n){const LONG x=100+(n%4)*180,y=100+(n/4)*180;eight.push_back({x,y,x+180,y+180});moved.push_back({x+40,y+30,x+220,y+210});}
    set.setup(eight);g=layout_down(set,7,HTCAPTION,true);
    layout_phase(set,7,g,40,30,moved,"eight_owned_last_member_real_Ctrl_source");layout_up(set,g,moved);
    s.log.check("layout_eight_owned_resources_destroyed",set.close(),"\"guest_destruction_is_not_product_cleanup\":true");
    s.log.emit("owned_layout_sequence","\"G06_vertical_horizontal\":true,\"I03_T_to_L_continuous\":true,\"G07_four_grid\":true,\"eight_actual_owned\":true,\"app_grid\":1,\"human_uat\":\"NOT_RUN\"");
}

// One bounded, real in-flight geometry call while a TEMPORARY drag alpha is
// owned by the product. Unlike blocked_writer_case this does not quit/restart
// the product: the late call must complete, the original temporary tuple must
// be restored, and an independently started next gesture must still work.
[[maybe_unused]] void owned_temporary_visual_inflight_case(State& s){
    namespace settings=panebind::core::v1;
    const HWND h=s.frames[0].hwnd;
    if(!s.process||WaitForSingleObject(s.process,0)!=WAIT_TIMEOUT||!own(s,h)||source_window(s,0)!=h||s.left||s.control)
        throw std::runtime_error("temporary_inflight_exact_live_owned_mode_required");
    for(std::size_t n=0;n<3;++n)if(s.explorer[n]||s.applications[n]||!own(s,s.frames[n].hwnd))
        throw std::runtime_error("temporary_inflight_other_target_bindings_must_be_released");
    const auto original_visual=native_visual(h);
    const auto parsed=settings::parse(read(s.product.parent_path()/L"settings.ini"));
    if(!original_visual||!original_visual->known||!parsed.value||!parsed.value->enabled)
        throw std::runtime_error("temporary_inflight_original_visual_or_configuration_unknown");
    const auto original_config=*parsed.value;
    std::array<Rect,3> original_rects{};std::array<bool,3> original_shown{};
    for(std::size_t n=0;n<3;++n){const auto actual=visible(s.frames[n].hwnd);
        if(!actual)throw std::runtime_error("temporary_inflight_original_geometry_unknown");
        original_rects[n]=*actual;original_shown[n]=IsWindowVisible(s.frames[n].hwnd)!=FALSE;
        if(n)ShowWindow(s.frames[n].hwnd,SW_HIDE);
    }
    MONITORINFO monitor{sizeof(monitor)};
    if(!GetMonitorInfoW(MonitorFromWindow(h,MONITOR_DEFAULTTONULL),&monitor))throw std::runtime_error("temporary_inflight_work_area_unknown");
    const Rect initial{monitor.rcWork.left+200,monitor.rcWork.top+160,monitor.rcWork.left+600,monitor.rcWork.top+460};
    s.log.check("temporary_inflight_independent_fixture_layout",fixture_rect(h,initial));
    auto desired=original_config;desired.attraction_dip=0;desired.drag_transparency=true;desired.drag_alpha=190;desired.shake=false;
    visual_gui_save(s,desired);own_visual_setup(s,h,205,false);
    // The permanent command's original is205, while the pre-DOWN temporary
    // original is173. Falling back to the helper's oldest original would be
    // observably WRONG while the product is still running.
    visual_command(s,h,L"OpacityDown",173,false);
    auto& stall=s.frames[0].stall;
    s.log.check("temporary_inflight_no_previous_native_stall",!stall.armed&&!stall.entered&&!stall.finished);
    const auto g=down(s,0);visual_exact(s,h,190,false,"temporary_inflight_actual_pressed_alpha190");
    const auto source_key=number(record(s,g.offset,"gesture_start",g.generation),"\"key\":");
    s.log.check("temporary_inflight_actual_generation_source_key",source_key!=0);
    stall.armed=true;Sleep(60);cursor(s,{g.anchor.x-42,g.anchor.y+35});
    s.log.check("temporary_inflight_real_native_WndProc_entered",wait([&]{return stall.entered.load();},800)&&
        stall.left_down.load()&&!stall.finished.load()&&(GetAsyncKeyState(VK_LBUTTON)&0x8000),
        "\"generation\":"+std::to_string(g.generation)+",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
        ",\"message\":\"WM_WINDOWPOSCHANGING\",\"test_owned_pause_ms\":1000,\"native_entry_qpc\":"+std::to_string(stall.before_qpc)+
        ",\"actual_flags\":"+std::to_string(stall.flags)+",\"actual_positioning\":["+std::to_string(stall.x)+","+
        std::to_string(stall.y)+","+std::to_string(stall.width)+","+std::to_string(stall.height)+"]");
    LARGE_INTEGER up_before{},up_after{};QueryPerformanceCounter(&up_before);
    button(s,false);QueryPerformanceCounter(&up_after); // Exactly one real test UP, never a product cleanup substitute.
    s.log.emit("temporary_inflight_real_UP_sent","\"generation\":"+std::to_string(g.generation)+
        ",\"send_before_qpc\":"+std::to_string(up_before.QuadPart)+",\"send_after_qpc\":"+std::to_string(up_after.QuadPart)+
        ",\"source_still_blocked\":"+(stall.finished?"false":"true")+",\"single_test_UP\":true");
    s.log.check("temporary_inflight_UP_before_source_returns",!stall.finished&&up_before.QuadPart>0&&
        static_cast<std::uint64_t>(up_before.QuadPart)>stall.before_qpc.load());
    std::string ended;
    s.log.check("temporary_inflight_generation_actually_retired",wait([&]{ended=record(s,g.offset,"gesture_end",g.generation);
        return ended.find("\"pending_retired\":true")!=std::string::npos;},1000),
        "\"generation\":"+std::to_string(g.generation)+",\"NormalUp_not_inferred_from_test_UP\":true");
    s.log.emit("temporary_inflight_actual_end","\"generation\":"+std::to_string(g.generation)+",\"production_end\":"+quote(ended));
    cursor(s,{g.anchor.x-58,g.anchor.y+45}); // No button held: this must not grant another placement.
    s.log.check("temporary_inflight_source_boundedly_resumed",wait([&]{return stall.finished.load();},1500));
    const Rect late_target=translated(initial,-42,35);
    std::uint64_t call_id{};std::string issued,late;
    const auto find_late=[&]{
        const auto text=read(s.product_log);if(g.offset>=text.size())return false;
        std::istringstream rows(text.substr(g.offset));std::string row;
        while(std::getline(rows,row))if(row.ends_with('}')&&row.find("\"type\":\"placement\"")!=std::string::npos&&
            number(row,"\"generation\":")==g.generation&&row.find("\"native_callsite_claimed\":true")!=std::string::npos){
            issued=row;call_id=number(row,"\"call_id\":");
        }
        if(!call_id)return false;
        std::istringstream completions(text.substr(g.offset));
        while(std::getline(completions,row))if(row.ends_with('}')&&row.find("\"type\":\"native_completion\"")!=std::string::npos&&
            number(row,"\"call_id\":")==call_id)late=row;
        return !late.empty();
    };
    s.log.check("temporary_inflight_late_native_receipt_exact",wait(find_late,2000)&&
        issued.find("\"native_completion_known\":false")!=std::string::npos&&
        number(issued,"\"native_before_qpc\":")<static_cast<std::uint64_t>(up_before.QuadPart)&&
        number(late,"\"key\":")==source_key&&late.find("\"native_completion_known\":true")!=std::string::npos&&
        late.find("\"native_attempted\":true")!=std::string::npos&&late.find("\"native_succeeded\":true")!=std::string::npos&&
        late.find("\"identity_valid\":true")!=std::string::npos&&late.find("\"geometry_exact_at_delayed_observation\":true")!=std::string::npos&&
        late.find("\"delayed_actual\":"+rect(late_target))!=std::string::npos&&
        late.find("\"new_write_authority_granted\":false")!=std::string::npos&&
        number(late,"\"native_after_qpc\":")>static_cast<std::uint64_t>(up_after.QuadPart)&&
        stall.after_qpc.load()>static_cast<std::uint64_t>(up_after.QuadPart)&&visible(h)==late_target,
        "\"generation\":"+std::to_string(g.generation)+",\"issued\":"+quote(issued)+",\"late\":"+quote(late)+
        ",\"source_return_qpc\":"+std::to_string(stall.after_qpc));
    // This is the app's pre-DOWN alpha173, NOT a permanent opacity-map
    // original. Require actual restored properties AND the product's exact,
    // helper-acknowledged cleanup receipt after the old geometry call finishes.
    visual_exact(s,h,173,false,"temporary_inflight_deferred_cleanup_restores_actual_alpha173");
    std::string restored;
    const bool cleanup_receipt=wait([&]{const auto text=read(s.product_log);if(g.offset>=text.size())return false;
        std::istringstream rows(text.substr(g.offset));std::string row;
        while(std::getline(rows,row))if(row.ends_with('}')&&row.find("\"type\":\"visual_mutation\"")!=std::string::npos&&
            number(row,"\"key\":")==source_key&&row.find("\"cleanup_owned_property\":true")!=std::string::npos&&
            row.find("\"native_completion_known\":true")!=std::string::npos&&row.find("\"visual_exact\":true")!=std::string::npos&&
            row.find("\"recovery_commit_ack\":true")!=std::string::npos&&
            row.find("\"before\":{\"layered\":true,\"attributes_known\":true,\"alpha\":190,")!=std::string::npos&&
            row.find("\"actual\":{\"layered\":true,\"attributes_known\":true,\"alpha\":173,")!=std::string::npos)restored=row;
        return !restored.empty();},1500);
    s.log.check("temporary_inflight_actual_temporary_cleanup_receipt",cleanup_receipt,"\"production_cleanup\":"+quote(restored));
    unsigned calls{},after_up{};std::istringstream placements(read(s.product_log).substr(g.offset));std::string row;
    while(std::getline(placements,row))if(row.find("\"type\":\"placement\"")!=std::string::npos&&
        number(row,"\"generation\":")==g.generation&&row.find("\"native_callsite_claimed\":true")!=std::string::npos){
        ++calls;if(number(row,"\"native_before_qpc\":")>=static_cast<std::uint64_t>(up_before.QuadPart))++after_up;
    }
    s.log.check("temporary_inflight_UP_no_new_geometry_call",calls==1&&after_up==0&&!s.left&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&
        s.process&&WaitForSingleObject(s.process,0)==WAIT_TIMEOUT,
        "\"issued_geometry_calls\":"+std::to_string(calls)+",\"issued_after_test_UP_send_start\":"+std::to_string(after_up));
    // No product restart or reconfiguration between the old completion and
    // this new actual DOWN. The next generation still uses its own anchors.
    const auto next=down(s,0);visual_exact(s,h,190,false,"temporary_inflight_next_gesture_alpha190");
    s.log.check("temporary_inflight_next_generation_is_fresh",next.generation>g.generation);
    const auto next_target=translated(late_target,40,30);
    phase(s,0,next,40,30,next_target,"temporary_inflight_next_real_Move_exact",true);up(s,0,next,next_target);
    visual_exact(s,h,173,false,"temporary_inflight_next_real_UP_restores_alpha173");
    visual_gui_save(s,original_config);
    visual_command(s,h,L"OpacityUp",205,false); // retire the permanent ownership before app-side fixture restoration
    SetLastError(0);const auto ex=GetWindowLongPtrW(h,GWL_EXSTYLE);if(!ex&&GetLastError())throw std::runtime_error("temporary_inflight_original_style_read_failed");
    SetLastError(0);const auto prior=SetWindowLongPtrW(h,GWL_EXSTYLE,original_visual->layered?ex|WS_EX_LAYERED:ex&~WS_EX_LAYERED);
    if((!prior&&GetLastError())||(original_visual->layered&&!SetLayeredWindowAttributes(h,original_visual->color,original_visual->alpha,original_visual->flags))||
        !SetWindowPos(h,original_visual->topmost?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE))
        throw std::runtime_error("temporary_inflight_original_visual_restore_failed");
    s.log.check("temporary_inflight_fixture_original_tuple_restored",wait([&]{const auto now=native_visual(h);return now&&*now==*original_visual;}));
    for(std::size_t n=0;n<3;++n){s.log.check("temporary_inflight_original_layout_restored",own(s,s.frames[n].hwnd)&&fixture_rect(s.frames[n].hwnd,original_rects[n]));
        ShowWindow(s.frames[n].hwnd,original_shown[n]?SW_SHOWNOACTIVATE:SW_HIDE);}
    // Reset only this completed, logged test injection so later independent
    // writer-exit cases may use the same exact fixture. No product facts or
    // observed raw/legacy event counters are rewritten.
    stall.armed=false;stall.entered=false;stall.finished=false;stall.left_down=false;
    s.log.emit("temporary_visual_inflight_sequence","\"single_test_UP\":true,\"late_exact_geometry_observed\":true,\"actual_temporary_alpha_restored\":173,\"next_actual_Move_UP\":true,\"same_product_process\":true,\"human_uat\":\"NOT_RUN\"");
}
