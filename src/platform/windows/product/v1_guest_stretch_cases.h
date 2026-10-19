#pragma once

// Guest-only F04 coverage. Real input, native receipts and independently
// declared rectangles; no product algorithm, simulated UP or extra LL hook.
bool stretch_input_idle(LayoutFixtures& set){
    GUITHREADINFO gui{sizeof(gui)};
    return set.active==1&&set.owns(0)&&GetGUIThreadInfo(set.thread,&gui)&&
        !gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner&&
        !(gui.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE));
}
void stretch_input_fresh(LayoutFixtures& set,POINT point,int region,Rect expected){
    POINT actual{};DWORD_PTR hit{};auto& s=set.state;const HWND h=set.frames[0].hwnd;
    if(!desktop()||s.left||s.control||!stretch_input_idle(set)||visible(h)!=expected||GetForegroundWindow()!=h||
        !GetCursorPos(&actual)||actual.x!=point.x||actual.y!=point.y||WindowFromPoint(actual)!=h||
        !SendMessageTimeoutW(h,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y)),
            SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&hit)||static_cast<int>(hit)!=region)
        throw std::runtime_error("F04_actual_exact_fresh_DOWN_context_changed");
}
void stretch_input_no_enhanced(LayoutFixtures& set,std::size_t mark,const char* label){
    auto& s=set.state;unsigned starts{},placements{},cancels{},resources{},idle_raw{},mapped_stretch{};
    std::istringstream rows(product_log_since(s,mark));std::string row;
    while(std::getline(rows,row))if(row.ends_with('}')){
        if(row.find("\"type\":\"gesture_start\"")!=row.npos)++starts;
        if(row.find("\"type\":\"placement\"")!=row.npos)++placements;
        if(row.find("\"type\":\"native_move_cancel\"")!=row.npos)++cancels;
        if(row.find("\"type\":\"mapped_command_completed\"")!=row.npos&&row.find("\"action\":\"Stretch")!=row.npos)++mapped_stretch;
        if(row.find("\"type\":\"gesture_input_resource\"")!=row.npos){
            if(pause_routing_idle_raw_observation(row,s.product_pid,s.product_source))++idle_raw;else ++resources;
        }
    }
    s.log.check(label,!starts&&!placements&&!cancels&&!resources&&!mapped_stretch&&stretch_input_idle(set),
        "\"product_gesture_starts\":"+std::to_string(starts)+",\"product_placements\":"+std::to_string(placements)+
        ",\"product_cancels\":"+std::to_string(cancels)+",\"nonidle_or_unknown_resources\":"+std::to_string(resources)+
        ",\"strict_readonly_idle_raw_observations\":"+std::to_string(idle_raw)+",\"mapped_stretch_handlers\":"+std::to_string(mapped_stretch));
}
// The ordinary edge's first click already has an exact product LL-UP native
// passthrough receipt. Reuse that fact without allocating another queue slot.
DWORD stretch_input_plain_native_click(LayoutFixtures& set,POINT point,Rect expected,const char* label){
    auto& s=set.state;stretch_input_fresh(set,point,HTRIGHT,expected);RECT positioning{};
    if(!GetWindowRect(set.frames[0].hwnd,&positioning))throw std::runtime_error("F04_native_click_positioning_unknown");
    const auto mark=read_product(s).size();button(s,true);button(s,false);
    std::string pending,released;std::uint64_t generation{};
    const bool matched=wait([&]{
        pending.clear();released.clear();unsigned pending_count{},released_count{};
        std::istringstream rows(product_log_since(s,mark));std::string row;
        while(std::getline(rows,row))if(row.ends_with('}')&&row.find("\"type\":\"native_move_pending\"")!=row.npos&&
            number(row,"\"source\":")==reinterpret_cast<std::uintptr_t>(set.frames[0].hwnd)&&
            number(row,"\"source_pid\":")==GetCurrentProcessId()&&number(row,"\"source_tid\":")==set.thread&&
            number(row,"\"down_hook_qpc\":")>=s.left_down_send_qpc){pending=row;++pending_count;}
        generation=number(pending,"\"generation\":");
        rows.clear();rows.str(product_log_since(s,mark));
        while(std::getline(rows,row))if(row.ends_with('}')&&row.find("\"type\":\"native_move_passthrough_release\"")!=row.npos&&
            number(row,"\"generation\":")==generation){released=row;++released_count;}
        RECT after{};const bool unchanged=set.owns(0)&&visible(set.frames[0].hwnd)==expected&&
            GetWindowRect(set.frames[0].hwnd,&after)&&EqualRect(&positioning,&after);
        const product::guest::NativeFirstClickFacts facts{reinterpret_cast<std::uintptr_t>(set.frames[0].hwnd),
            GetCurrentProcessId(),set.thread,s.left_down_send_qpc,GetDoubleClickTime(),2,unchanged,
            GetForegroundWindow()==set.frames[0].hwnd,stretch_input_idle(set),record(s,mark,"placement").empty()&&
            record(s,mark,"gesture_start").empty()&&record(s,mark,"native_move_cancel").empty()};
        return pending_count==1&&released_count==1&&product::guest::matching_native_first_click_release(
            pending,released,{s.product_pid,s.product_source,generation,s.left_up_send_qpc},facts);
    },GetDoubleClickTime());
    s.log.check(label,matched,"\"actual_native_pending\":"+quote(pending)+",\"actual_LL_UP_native_release\":"+quote(released)+
        ",\"single_test_UP_send_qpc\":"+std::to_string(s.left_up_send_qpc)+",\"NormalUp_inferred\":false");
    return static_cast<DWORD>(number(pending,"\"down_native_time\":"));
}
void stretch_input_timeout_case(LayoutFixtures& set,Rect source){
    auto& s=set.state;chain_front(set,0);const auto point=layout_hit(set,0,HTRIGHT);
    if(!point)throw std::runtime_error("F04_timeout_native_edge_unknown");cursor(s,*point);
    // Do not inherit a double click from the preceding positive/Undo chain.
    (void)layout_wait_sent([]{return false;},GetDoubleClickTime()+32);
    const auto mark=read_product(s).size();
    const auto first=stretch_input_plain_native_click(set,*point,source,"F04_timeout_first_actual_UP");
    // Wait after proven first UP, then compare actual native DOWN times.
    (void)layout_wait_sent([]{return false;},GetDoubleClickTime()+32);
    const auto second=stretch_input_plain_native_click(set,*point,source,"F04_timeout_second_actual_UP");
    const auto interval=static_cast<DWORD>(second-first);
    s.log.check("F04_timeout_actual_interval_exceeds_system_threshold",first&&second&&interval>GetDoubleClickTime(),
        "\"actual_native_DOWN_interval_ms\":"+std::to_string(interval)+",\"system_double_click_ms\":"+std::to_string(GetDoubleClickTime()));
    stretch_input_no_enhanced(set,mark,"F04_timeout_two_actual_clicks_not_stretch");
}
struct StretchNativeDown {
    bool known{};std::uint64_t count{},qpc{};std::uintptr_t hwnd{};DWORD pid{},tid{},time{};
    UINT message{};WPARAM hit{};POINT point{};LPARAM extra{};
    bool matches(LayoutFixtures& set,POINT expected,std::uint64_t expected_count,std::uint64_t sent,DWORD stamp)const{
        return known&&count==expected_count&&hwnd==reinterpret_cast<std::uintptr_t>(set.frames[0].hwnd)&&
            pid==GetCurrentProcessId()&&tid==set.thread&&qpc>=sent&&sent&&
            (message==WM_NCLBUTTONDOWN||message==WM_NCLBUTTONDBLCLK)&&hit==HTRIGHT&&
            point.x==expected.x&&point.y==expected.y&&extra==0x50425631&&static_cast<LONG>(time-stamp)>=0;
    }
    std::string json()const{return "{\"snapshot_known\":"+std::string(known?"true":"false")+
        ",\"count\":"+std::to_string(count)+",\"source\":"+std::to_string(hwnd)+",\"source_pid\":"+std::to_string(pid)+
        ",\"source_tid\":"+std::to_string(tid)+",\"actual_message\":"+std::to_string(message)+
        ",\"actual_wParam_hit\":"+std::to_string(hit)+",\"actual_message_time\":"+std::to_string(time)+
        ",\"actual_dispatch_qpc\":"+std::to_string(qpc)+",\"actual_screen_point\":["+std::to_string(point.x)+","+std::to_string(point.y)+
        "],\"actual_GetMessageExtraInfo\":"+std::to_string(extra)+",\"DefWindowProc_unchanged\":true}";}
};
StretchNativeDown stretch_native_down(const Frame& frame){
    const auto& f=frame.native_click_down;
    for(unsigned attempt=0;attempt<3;++attempt){
        const auto version=f.version.load();if(version&1u)continue;StretchNativeDown out;
        out.count=f.count.load();out.qpc=f.qpc.load();out.hwnd=f.hwnd.load();out.pid=f.pid.load();out.tid=f.tid.load();
        out.time=static_cast<DWORD>(f.time.load());out.message=f.message.load();out.hit=f.hit.load();out.extra=f.extra.load();
        const auto point=f.point.load();out.point={static_cast<short>(LOWORD(point)),static_cast<short>(HIWORD(point))};
        if(version==f.version.load()){out.known=true;return out;}
    }
    return {};
}
bool stretch_input_movement_case(LayoutFixtures& set,Rect source){
    auto& s=set.state;chain_front(set,0);const auto point=layout_hit(set,0,HTRIGHT);
    const int threshold=GetSystemMetrics(SM_CXDRAG);if(!point||threshold<1||threshold>128)
        throw std::runtime_error("F04_movement_actual_edge_or_system_drag_threshold_unavailable");
    cursor(s,*point);(void)layout_wait_sent([]{return false;},GetDoubleClickTime()+32);
    GuestOwnedQueueUpObservation queue;s.log.check("F04_movement_exact_source_queue_observer",queue.start(set.frames[0].hwnd,set.thread));
    bool shift_attempted{},shift_released{};const auto mark=read_product(s).size();auto& frame=set.frames[0];
    const auto starts=frame.starts.load(),ends=frame.ends.load();
    const auto ncdowns=frame.native_move_boundary.count[0].load();
    const auto click_downs=frame.native_click_down.count.load();
    try{
        shift_attempted=true;key(s,VK_SHIFT,true);stretch_input_fresh(set,*point,HTRIGHT,source);
        std::string first_preparation;
        const bool initial_prepared=chain_stretch_preparation(set,*point,HTRIGHT,source,0,1000,first_preparation);
        s.log.check("F04_movement_initial_exact_product_preparation",initial_prepared,
            "\"actual_preparation\":"+quote(first_preparation)+",\"receipt_is_write_authority\":false");
        stretch_input_fresh(set,*point,HTRIGHT,source);
        const auto preparation_mark=read_product(s).size();
        const DWORD stamp1=GetTickCount();button(s,true);
        const auto down1_sent=s.left_down_send_qpc;
        const LONG delta=threshold+8;cursor(s,{point->x+delta,point->y});
        const Rect widened{source.l,source.t,source.r+delta,source.b};
        s.log.check("F04_movement_real_native_resize_exceeds_drag_threshold",layout_wait_sent([&]{return set.owns(0)&&
            frame.starts.load()==starts+1&&visible(frame.hwnd)==widened;},1200),
            "\"system_CXDRAG\":"+std::to_string(threshold)+",\"actual_cursor_delta\":"+std::to_string(delta)+
            ",\"independent_expected_native_rect\":"+rect(widened));
        cursor(s,*point);const bool returned=layout_wait_sent([&]{return visible(frame.hwnd)==source;},1200);
        button(s,false);const auto up1=s.left_up_send_qpc;std::uint64_t q1{};
        const bool first=layout_wait_sent([&]{q1=queue.exact_source_click_up_qpc(frame.hwnd,set.thread,up1,stamp1,*point,0,1,frame.native_end_qpc.load());
            return q1&&frame.ends.load()==ends+1&&stretch_input_idle(set);},1200);
        const auto down1=stretch_native_down(frame);const auto time1=down1.time;
        const auto end1=frame.native_end_qpc.load();
        s.log.check("F04_movement_first_actual_resize_UP_END",returned&&first&&q1&&frame.native_move_boundary.count[0].load()==ncdowns+1&&
            down1.matches(set,*point,click_downs+1,down1_sent,stamp1)&&down1.message==WM_NCLBUTTONDOWN,
            "\"source_queue\":"+queue.json()+",\"actual_native_END_qpc\":"+std::to_string(frame.native_end_qpc.load())+
            ",\"actual_first_DOWN\":"+down1.json()+
            ",\"source_geometry_returned_by_native_resize_not_setup\":true");
        // One attempt only. Slow test IO may consume the actual double-click
        // interval; that cannot prove the movement-threshold branch.
        std::string second_preparation;
        const auto incomplete=[&](const char* reason){
            shift_released=true;key(s,VK_SHIFT,false);const bool stopped=queue.stop();
            s.log.check("F04_movement_incomplete_attempt_cleaned",stopped&&stretch_input_idle(set)&&visible(frame.hwnd)==source);
            stretch_input_no_enhanced(set,mark,"F04_movement_first_click_only_no_enhancement");
            s.log.emit("case","\"case\":\"F04_movement_threshold\",\"result\":\"NOT_RUN\",\"reason\":"+quote(reason)+
                ",\"actual_second_preparation\":"+quote(second_preparation)+",\"no_automatic_retry\":true");
            return false;
        };
        const auto elapsed=static_cast<DWORD>(GetTickCount()-time1);
        if(elapsed>=GetDoubleClickTime())return incomplete("actual_double_click_time_window_not_established");
        const bool prepared=chain_stretch_preparation(set,*point,HTRIGHT,source,preparation_mark,
            GetDoubleClickTime()-elapsed,second_preparation,number(first_preparation,"\"key\":"));
        if(!prepared||static_cast<DWORD>(GetTickCount()-time1)>=GetDoubleClickTime())
            return incomplete("second_actual_exact_product_preparation_not_observed_within_time_window");
        stretch_input_fresh(set,*point,HTRIGHT,source);
        if(static_cast<DWORD>(GetTickCount()-time1)>=GetDoubleClickTime())return incomplete("fresh_second_DOWN_time_window_expired");
        const DWORD stamp2=GetTickCount();button(s,true);const auto down2_sent=s.left_down_send_qpc;button(s,false);
        const auto up2=s.left_up_send_qpc;std::uint64_t q2{};
        const bool second=layout_wait_sent([&]{
            const bool second_loop=frame.starts.load()>starts+1;
            q2=queue.exact_source_click_up_qpc(frame.hwnd,set.thread,up2,stamp2,*point,1,2,second_loop?frame.native_end_qpc.load():0);
            return q2&&stretch_input_idle(set)&&(!second_loop||frame.ends.load()==ends+2);
        },1200);
        const auto down2=stretch_native_down(frame);
        const bool second_loop=frame.starts.load()>starts+1;
        const bool loops_exact=second_loop?frame.starts.load()==starts+2&&frame.ends.load()==ends+2&&frame.native_end_qpc.load()>end1:
            frame.starts.load()==starts+1&&frame.ends.load()==ends+1&&frame.native_end_qpc.load()==end1;
        shift_released=true;key(s,VK_SHIFT,false);const bool stopped=queue.stop();
        q1=stopped?queue.exact_source_click_up_qpc(frame.hwnd,set.thread,up1,stamp1,*point,0,2,end1):0;
        q2=stopped?queue.exact_source_click_up_qpc(frame.hwnd,set.thread,up2,stamp2,*point,1,2,second_loop?frame.native_end_qpc.load():0):0;
        const auto interval=static_cast<DWORD>(down2.time-time1);
        s.log.check("F04_movement_two_actual_native_clicks_cleanup",second&&stopped&&q1&&q2&&loops_exact&&
            visible(frame.hwnd)==source&&stretch_input_idle(set),
            "\"actual_source_queue\":"+queue.json()+",\"actual_NC_DOWN_interval_ms\":"+std::to_string(interval)+
            ",\"actual_first_DOWN\":"+down1.json()+",\"actual_second_DOWN\":"+down2.json()+
            ",\"actual_second_preparation\":"+quote(second_preparation)+",\"second_preparation_checked_before_DOWN\":true"+
            ",\"second_native_modal_loop_observed\":"+(second_loop?"true":"false")+
            ",\"actual_START_delta\":"+std::to_string(frame.starts.load()-starts)+",\"actual_END_delta\":"+std::to_string(frame.ends.load()-ends)+
            ",\"system_double_click_ms\":"+std::to_string(GetDoubleClickTime()));
        stretch_input_no_enhanced(set,mark,"F04_movement_threshold_no_stretch_or_product_writer");
        const bool established=down2.matches(set,*point,click_downs+2,down2_sent,stamp2)&&down2.qpc>down1.qpc&&interval<=GetDoubleClickTime();
        s.log.emit("case","\"case\":\"F04_movement_threshold\",\"result\":"+quote(established?"PASS":"NOT_RUN")+
            ",\"actual_time_and_second_DOWN_condition_established\":"+(established?"true":"false")+
            ",\"reason\":"+quote(established?"actual_prepared_second_DOWN_and_source_UP_verified":"second_native_DOWN_or_actual_time_window_not_established")+
            ",\"no_layout_reset_between_clicks\":true,\"no_automatic_retry\":true");
        return established;
    }catch(...){
        try{if(s.left&&!s.left_up_attempted)button(s,false);}catch(...){}
        try{if(shift_attempted&&!shift_released){shift_released=true;key(s,VK_SHIFT,false);}}catch(...){}
        (void)queue.stop();throw;
    }
}
void stretch_input_caption_button_case(LayoutFixtures& set,Rect source){
    auto& s=set.state;chain_front(set,0);const HWND h=set.frames[0].hwnd;
    const auto caption=native_caption_observation(h,{});std::optional<POINT> point;
    if(caption.window_known&&SUCCEEDED(caption.hresult)){
        // DWM bounds are window-relative and describe all caption buttons.
        // A fixed, finite 24 point scan still requires actual HTMAXBUTTON.
        const LONG left=caption.window.left+caption.buttons.left,right=caption.window.left+caption.buttons.right;
        const LONG top=caption.window.top+caption.buttons.top,bottom=caption.window.top+caption.buttons.bottom;
        if(left<right&&top<bottom)for(unsigned y=0;y<2&&!point;++y)for(unsigned x=0;x<12&&!point;++x){
            const POINT p{left+(right-left)*(2*static_cast<LONG>(x)+1)/24,
                top+(bottom-top)*(2*static_cast<LONG>(y)+1)/4};DWORD_PTR hit{};
            if(WindowFromPoint(p)==h&&SendMessageTimeoutW(h,WM_NCHITTEST,0,
                MAKELPARAM(static_cast<short>(p.x),static_cast<short>(p.y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&hit)&&hit==HTMAXBUTTON)point=p;
        }
    }
    s.log.check("F04_actual_owned_HTMAXBUTTON_identified",point.has_value(),"\"actual_DWM_caption\":"+native_caption_json(caption));
    cursor(s,*point);stretch_input_fresh(set,*point,HTMAXBUTTON,source);
    GuestOwnedQueueUpObservation queue;s.log.check("F04_caption_button_source_queue_observer",queue.start(h,set.thread));
    const auto mark=read_product(s).size();const DWORD stamp=GetTickCount();button(s,true);button(s,false);
    const auto up=s.left_up_send_qpc;std::uint64_t actual_up{};
    const bool maximized=layout_wait_sent([&]{actual_up=queue.exact_source_click_up_qpc(h,set.thread,up,stamp,*point,0,1);
        return actual_up&&set.owns(0)&&IsZoomed(h)&&stretch_input_idle(set);},1500);
    const bool stopped=queue.stop();actual_up=stopped?queue.exact_source_click_up_qpc(h,set.thread,up,stamp,*point,0,1):0;
    s.log.check("F04_caption_button_actual_native_maximize_and_UP",maximized&&stopped&&actual_up,
        "\"actual_source_queue\":"+queue.json()+",\"actual_IsZoomed\":"+std::string(IsZoomed(h)?"true":"false")+
        ",\"native_END_or_product_NormalUp_inferred\":false");
    stretch_input_no_enhanced(set,mark,"F04_caption_button_not_misclassified_as_stretch");
    // The real Restore command selects native SW_RESTORE for a maximized
    // source. It need not emit a geometry writer placement.
    const auto restore_mark=read_product(s).size();const bool sent=launch(s,L"--command Restore",true);
    s.log.check("F04_same_exact_source_native_Restore",sent&&wait([&]{return set.owns(0)&&!IsZoomed(h)&&visible(h)==source&&
        stretch_input_idle(set)&&!record(s,restore_mark,"command").empty();}),"\"actual_command\":"+quote(record(s,restore_mark,"command")));
}
void owned_stretch_input_case(State& s){
    const bool boundary_only=s.suite=="StretchBoundaryOnly";
    if((s.suite!="StretchInputOnly"&&!boundary_only)||s.left||s.control||(GetAsyncKeyState(VK_SHIFT)&0x8000))
        throw std::runtime_error("F04_exact_guest_suite_released_input_required");
    const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    if(!original.enabled)throw std::runtime_error("F04_enabled_product_required");
    LayoutFixtures set{s};set.create();const auto work=chain_work(set.frames[0].hwnd);
    if(work.width()<1100||work.height()<850)throw std::runtime_error("F04_actual_work_area_too_small_for_independent_fixture");
    const Rect source{work.l+300,work.t+220,work.l+700,work.t+570};
    const LayoutLimit limit{100,100,work.width(),work.height()};
    auto config=original;config.stretch=true;config.smart_stretch=false;config.docking=false;
    config.preview=false;config.feedback=false;config.drag_transparency=false;config.shake=false;
    bool timing_established{};
    try{
        const auto applied=chain_gui_save(s,config);s.log.check("F04_actual_GUI_stretch_enabled_smart_disabled",applied.stretch&&!applied.smart_stretch);
        if(!boundary_only){
        struct Direction{int region;bool shift;Rect expected;const char* label;};
        const std::array<Direction,9> cases{{
            {HTRIGHT,false,{source.l,source.t,work.r,source.b},"F04_Right_actual_UP_fixed_left_top_bottom"},
            {HTTOP,false,{source.l,work.t,source.r,source.b},"F04_Top_actual_UP_fixed_left_right_bottom"},
            {HTBOTTOM,false,{source.l,source.t,source.r,work.b},"F04_Bottom_actual_UP_fixed_left_right_top"},
            {HTTOPLEFT,false,{work.l,work.t,source.r,source.b},"F04_TopLeft_actual_cleanup_UP_fixed_right_bottom"},
            {HTTOPRIGHT,false,{source.l,work.t,work.r,source.b},"F04_TopRight_actual_cleanup_UP_fixed_left_bottom"},
            {HTBOTTOMLEFT,false,{work.l,source.t,source.r,work.b},"F04_BottomLeft_actual_cleanup_UP_fixed_right_top"},
            {HTBOTTOMRIGHT,false,{source.l,source.t,work.r,work.b},"F04_BottomRight_actual_cleanup_UP_fixed_left_top"},
            {HTRIGHT,true,{work.l,source.t,work.r,source.b},"F04_Shift_Right_horizontal_actual_UP_fixed_top_bottom"},
            {HTTOP,true,{source.l,work.t,source.r,work.b},"F04_Shift_Top_vertical_actual_UP_fixed_left_right"}
        }};
        for(const auto& item:cases){
            chain_check_product(s,pid,birth);set.setup({source},{limit});
            // Isolate named actions, not a repeated stability batch.
            (void)layout_wait_sent([]{return false;},GetDoubleClickTime()+32);
            chain_double_click(set,item.region,item.shift,item.expected,item.label,true);
            chain_command(set,0,L"Undo",source);
            s.log.check("F04_same_exact_source_Undo_and_idle",set.owns(0)&&visible(set.frames[0].hwnd)==source&&stretch_input_idle(set),
                "\"action\":"+quote(item.label)+",\"independent_original\":"+rect(source));
        }
        set.setup({source},{limit});stretch_input_timeout_case(set,source);
        }else s.log.emit("stretch_boundary_omitted_historical_scope",
            "\"named_directions\":\"NOT_RUN\",\"timeout_negative\":\"NOT_RUN\",\"prior_product212_run_id\":\"d7e71d93c1ea449aa90e49dbd0ac6868\",\"historical_results_not_rebound\":true");
        set.setup({source},{limit});timing_established=stretch_input_movement_case(set,source);
        set.setup({source},{limit});stretch_input_caption_button_case(set,source);
        chain_check_product(s,pid,birth);const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        s.log.check("F04_original_actual_GUI_configuration_restored",restored==expected);
        s.log.check("F04_exact_owned_resources_closed",set.close());
        s.log.emit("stretch_input_component","\"result\":"+quote(timing_established?"PASS":"PARTIAL")+
            ",\"suite\":"+quote(s.suite)+",\"named_direction_Shift_actions\":"+std::to_string(boundary_only?0:9)+
            ",\"named_directions\":"+quote(boundary_only?"NOT_RUN":"PASS_SCOPED")+
            ",\"timeout_negative\":"+quote(boundary_only?"NOT_RUN":"PASS_SCOPED")+
            ",\"movement_time_condition\":"+quote(timing_established?"ESTABLISHED":"NOT_RUN")+
            ",\"old_left_6dba_evidence_not_rebound\":true,\"Full_I05_or_version_PASS\":false,\"human_uat\":\"NOT_RUN\"");
    }catch(...){
        // Cleanup only this driver's unique outstanding input while the exact
        // source Frame/STA are still alive; never replay an attempted UP.
        try{if(s.left&&!s.left_up_attempted)button(s,false);}catch(...){}
        try{chain_gui_save(s,original);}catch(...){s.log.emit("F04_failed_case_configuration_restore","\"result\":\"UNKNOWN\"");}
        throw;
    }
}
