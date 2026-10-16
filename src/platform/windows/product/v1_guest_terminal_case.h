// Guest driver only: one real, fast DOWN/MOVE/UP insertion per distinct route.
// No product test hook,
// artificial product stall, solver-derived expectation or synthetic receipt.
void owned_rapid_terminal_case(State& s,bool ctrl,bool resize){
    const auto route=ctrl?(resize?"ctrl_right_resize":"ctrl_caption_move"):
        (resize?"plain_right_resize":"plain_caption_move");
    const auto route_fields=",\"route\":"+quote(route)+",\"locked_Ctrl\":"+(ctrl?"true":"false")+
        ",\"requested_actual_hit\":"+std::to_string(resize?HTRIGHT:HTCAPTION);
    if(s.left||s.control)throw std::runtime_error("rapid_terminal_input_ledger_not_idle");
    // Independent routes must not accidentally form a native double-click.
    // This TEST-only wait is outside the unchanged DOWN/MOVE/UP triplet.
    const UINT double_click_ms=GetDoubleClickTime();
    if(!double_click_ms||double_click_ms>5000)throw std::runtime_error("rapid_terminal_system_double_click_interval_unavailable");
    const auto interval_before=GetTickCount64();Sleep(double_click_ms+1);const auto interval_waited=GetTickCount64()-interval_before;
    s.log.check("rapid_terminal_distinct_route_double_click_interval",interval_waited>double_click_ms&&GetDoubleClickTime()==double_click_ms,
        "\"system_GetDoubleClickTime_ms\":"+std::to_string(double_click_ms)+",\"requested_wait_ms\":"+std::to_string(double_click_ms+1)+
        ",\"actual_wait_ms\":"+std::to_string(interval_waited)+",\"outside_three_packet_gesture\":true,\"prior_failure_cause_not_inferred\":true"+route_fields);
    // Error cleanup may release only this case's real Ctrl press, never add a
    // guessed mouse UP after a partially inserted three-packet array.
    struct ControlCleanup{State& state;bool enabled;~ControlCleanup(){if(enabled&&state.control)try{key(state,VK_CONTROL,false);}catch(...){}}} control_cleanup{s,ctrl};
    std::array<Rect,3> previous{};
    for(std::size_t n=0;n<3;++n){const auto actual=visible(s.frames[n].hwnd);
        if(!actual||!own(s,s.frames[n].hwnd))throw std::runtime_error("rapid_terminal_requires_exact_owned_roots");
        previous[n]=*actual;
    }
    for(std::size_t n=1;n<3;++n)ShowWindow(s.frames[n].hwnd,SW_HIDE);
    const Rect start{160,240,560,580};
    const Rect expected=resize?Rect{160,240,595,580}:Rect{195,271,595,611};
    s.log.check("rapid_terminal_actual_fixture_setup",fixture_rect(s.frames[0].hwnd,start),
        "\"test_setup_only\":true,\"expected\":"+rect(start)+route_fields);
    foreground(s,s.frames[0].hwnd);
    if(ctrl)key(s,VK_CONTROL,true); // Locked before actual hover preparation.
    const auto anchor=hit_point(s.frames[0].hwnd,resize?HTRIGHT:HTCAPTION);
    s.log.check("rapid_terminal_actual_nonclient_hit",anchor.has_value(),"\"actual_hit_required\":true"+route_fields);cursor(s,*anchor);Sleep(100);
    Gesture g{*anchor,0,read_product(s).size()};
    const POINT destination{g.anchor.x+35,g.anchor.y+(resize?0:31)};
    const int x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if(!desktop()||!own(s,s.frames[0].hwnd)||GetForegroundWindow()!=s.frames[0].hwnd||
       width<2||height<2||destination.x<x||destination.y<y||destination.x>=x+width||destination.y>=y+height)
        throw std::runtime_error("rapid_terminal_guest_context_changed");
    const auto starts_before=s.frames[0].starts.load(),ends_before=s.frames[0].ends.load();
    const auto native_before=native_move_boundary_json(s.frames[0]);
    GuestUpHookObservation observer;s.log.check("rapid_terminal_passive_observer_ready",observer.start(),"\"test_only\":true"+route_fields);
    GuestOwnedQueueUpObservation queue_observer;
    s.log.check("rapid_terminal_exact_owned_queue_observer_ready",own(s,s.frames[0].hwnd)&&s.owner&&queue_observer.start(s.frames[0].hwnd,s.owner),
        "\"test_only\":true,\"WH_GETMESSAGE_thread_specific\":true,\"hMod_null\":true,\"queue_UP_not_yet_a_PASS_alternative\":true"+route_fields);
    std::array<INPUT,3> packets{};for(auto& packet:packets){packet.type=INPUT_MOUSE;packet.mi.dwExtraInfo=0x50425631;}
    packets[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
    packets[1].mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
    packets[1].mi.dx=static_cast<LONG>(((static_cast<long long>(destination.x-x)*2+1)*65536)/(static_cast<long long>(width)*2));
    packets[1].mi.dy=static_cast<LONG>(((static_cast<long long>(destination.y-y)*2+1)*65536)/(static_cast<long long>(height)*2));
    packets[2].mi.dwFlags=MOUSEEVENTF_LEFTUP;
    LARGE_INTEGER before{},after{};QueryPerformanceCounter(&before);SetLastError(0);
    const auto inserted=SendInput(static_cast<UINT>(packets.size()),packets.data(),sizeof(INPUT));
    const auto error=inserted==packets.size()?0:GetLastError();QueryPerformanceCounter(&after);
    // The array already attempted its one UP. A partial insertion is UNKNOWN:
    // preserve that failure and never guess which element permits another UP.
    s.left=inserted!=packets.size();s.left_up_attempted=true;
    s.left_down_send_qpc=s.left_up_send_qpc=before.QuadPart>0?static_cast<std::uint64_t>(before.QuadPart):0;
    if(ctrl)key(s,VK_CONTROL,false); // Not mouse-UP delivery evidence.
    s.log.check("rapid_terminal_single_DOWN_MOVE_UP_array_inserted",inserted==packets.size(),
        "\"generation\":"+std::to_string(g.generation)+",\"SendInput_return\":"+std::to_string(inserted)+
        ",\"win32_error\":"+std::to_string(error)+",\"before_qpc\":"+std::to_string(before.QuadPart)+
        ",\"after_qpc\":"+std::to_string(after.QuadPart)+",\"wait_for_product_START_before_UP\":false,\"delivery_not_inferred\":true"+
        ",\"partial_packet_delivery\":"+quote(inserted==packets.size()?"not_partial":"UNKNOWN")+route_fields);
    std::string start_receipt,pending,release,end;bool native_only=false,native_LL_up=false;std::uint64_t owned_queue_up{};
    const bool ended=wait([&]{start_receipt=record(s,g.offset,"gesture_start");pending=record(s,g.offset,"native_move_pending");
        const auto enhanced_generation=number(start_receipt,"\"generation\":");
        g.generation=enhanced_generation?enhanced_generation:number(pending,"\"generation\":");
        if(!g.generation)return false;
        if(enhanced_generation){end=record(s,g.offset,"gesture_end",g.generation);native_only=false;
            return end.find("\"normal_up\":true")!=end.npos;}
        release=record(s,g.offset,"native_move_passthrough_release",g.generation);
        const auto pending_point="\"down_point\":["+std::to_string(g.anchor.x)+","+std::to_string(g.anchor.y)+"]";
        const bool pending_source=number(pending,"\"process_id\":")==s.product_pid&&
            pending.find("\"source_sha\":"+quote(s.product_source))!=pending.npos&&
            number(pending,"\"source\":")==reinterpret_cast<std::uintptr_t>(s.frames[0].hwnd)&&
            number(pending,"\"source_pid\":")==GetCurrentProcessId()&&number(pending,"\"source_tid\":")==s.owner&&
            number(pending,"\"actual_DOWN_region\":")==static_cast<std::uint64_t>(resize?2:0)&&
            number(pending,"\"locked_modifiers\":")==static_cast<std::uint64_t>(ctrl?2:0)&&
            number(pending,"\"down_hook_qpc\":")>=s.left_down_send_qpc&&pending.find(pending_point)!=pending.npos&&
            pending.find("\"down_consumed\":false")!=pending.npos&&pending.find("\"write_authority\":false")!=pending.npos;
        native_LL_up=number(release,"\"process_id\":")==s.product_pid&&
            release.find("\"source_sha\":"+quote(s.product_source))!=release.npos&&
            number(release,"\"source\":")==reinterpret_cast<std::uintptr_t>(s.frames[0].hwnd)&&
            number(release,"\"actual_LL_UP_qpc\":")>=s.left_up_send_qpc&&
            release.find("\"normalized_NormalUp\":false")!=release.npos&&release.find("\"new_native_placement\":false")!=release.npos;
        owned_queue_up=pending_source?queue_observer.exact_source_up_qpc(s.frames[0].hwnd,s.owner,s.left_up_send_qpc,
            static_cast<DWORD>(number(pending,"\"down_native_time\":")),destination,s.frames[0].native_end_qpc.load()):0;
        native_only=pending_source&&(native_LL_up||owned_queue_up);
        return native_only&&s.frames[0].starts.load()==starts_before+1&&s.frames[0].ends.load()==ends_before+1&&visible(s.frames[0].hwnd)==expected;
    });
    const bool stopped=observer.stop(),queue_stopped=queue_observer.stop();const auto observed_up=actual_up(s,g.offset,g.generation);
    // Recheck the retained immutable per-probe record after actual Unhook;
    // active callbacks or any extra removed UP still refuse the alternative.
    owned_queue_up=queue_stopped?queue_observer.exact_source_up_qpc(s.frames[0].hwnd,s.owner,s.left_up_send_qpc,
        static_cast<DWORD>(number(pending,"\"down_native_time\":")),destination,s.frames[0].native_end_qpc.load()):0;
    const auto actual_up_qpc=native_only?(native_LL_up?number(release,"\"actual_LL_UP_qpc\":"):owned_queue_up):
        std::max(number(observed_up,"\"hook_observed_qpc\":"),number(observed_up,"\"resource_observed_qpc\":"));
    const auto last=record(s,g.offset,"placement",g.generation);
    Sleep(150);POINT cursor_after{};const bool cursor_known=GetCursorPos(&cursor_after)!=FALSE;
    unsigned terminal_native{},ordinary_native{},new_issue_at_or_after_UP{},cancel_native{},capture_ready{};
    std::istringstream rows(product_log_since(s,g.offset));std::string row;
    while(std::getline(rows,row))if(row.ends_with('}')&&number(row,"\"generation\":")==g.generation){
        if(row.find("\"type\":\"placement\"")!=row.npos&&row.find("\"native_attempted\":true")!=row.npos){
            if(row.find("\"normal_up_completion\":true")!=row.npos)++terminal_native;else ++ordinary_native;
            const auto issued=number(row,"\"native_before_qpc\":");
            if(!actual_up_qpc||!issued||issued>=actual_up_qpc)++new_issue_at_or_after_UP;
        }
        if(row.find("\"type\":\"native_move_cancel\"")!=row.npos&&row.find("\"native_attempted\":true")!=row.npos)++cancel_native;
        if(row.find("\"type\":\"gesture_input_resource\"")!=row.npos&&row.find("\"fact\":\"capture_ready\"")!=row.npos)++capture_ready;
    }
    GUITHREADINFO gui{sizeof(gui)};const bool gui_known=GetGUIThreadInfo(s.owner,&gui)!=FALSE;
    const bool gui_idle=gui_known&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner&&
        !(gui.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE));
    const auto actual_start=record(s,g.offset,"native_move_start",g.generation);
    const auto actual_end=record(s,g.offset,"native_move_end",g.generation);
    const bool actual_native_boundaries=s.frames[0].starts.load()==starts_before+1&&s.frames[0].ends.load()==ends_before+1;
    const bool enhanced_result=!native_only&&end.find("\"normal_up\":true")!=end.npos&&!observed_up.empty()&&
        start_receipt.find(resize?"\"resize\":true":"\"resize\":false")!=start_receipt.npos&&
        start_receipt.find(ctrl?"\"ctrl\":true":"\"ctrl\":false")!=start_receipt.npos&&
        number(actual_start,"\"source\":")==reinterpret_cast<std::uintptr_t>(s.frames[0].hwnd)&&
        number(actual_end,"\"source\":")==reinterpret_cast<std::uintptr_t>(s.frames[0].hwnd)&&
        number(actual_start,"\"event\":")==EVENT_SYSTEM_MOVESIZESTART&&number(actual_end,"\"event\":")==EVENT_SYSTEM_MOVESIZEEND&&
        actual_native_boundaries&&ordinary_native>=1&&terminal_native==0&&new_issue_at_or_after_UP==0;
    const bool native_result=native_only&&(native_LL_up||owned_queue_up)&&start_receipt.empty()&&record(s,g.offset,"gesture_start").empty()&&
        record(s,g.offset,"gesture_end",g.generation).empty()&&actual_native_boundaries&&
        s.frames[0].native_start_qpc.load()>=s.left_down_send_qpc&&
        ordinary_native==0&&terminal_native==0&&cancel_native==0&&capture_ready==0;
    s.log.check("rapid_terminal_real_UP_final_geometry_and_no_late_write",ended&&stopped&&queue_stopped&&(enhanced_result||native_result)&&
        own(s,s.frames[0].hwnd)&&GetForegroundWindow()==s.frames[0].hwnd&&gui_idle&&
        visible(s.frames[0].hwnd)==expected&&cursor_known&&cursor_after.x==destination.x&&cursor_after.y==destination.y&&
        last==record(s,g.offset,"placement",g.generation),
        "\"generation\":"+std::to_string(g.generation)+route_fields+",\"expected\":"+rect(expected)+
        ",\"actual\":"+(visible(s.frames[0].hwnd)?rect(*visible(s.frames[0].hwnd)):"null")+
        ",\"ordinary_native_attempts\":"+std::to_string(ordinary_native)+",\"terminal_native_attempts\":"+std::to_string(terminal_native)+
        ",\"product_issues_at_or_after_actual_UP\":"+std::to_string(new_issue_at_or_after_UP)+",\"product_native_cancel_attempts\":"+std::to_string(cancel_native)+
        ",\"product_capture_ready_count\":"+std::to_string(capture_ready)+",\"actual_native_START_delta\":"+std::to_string(s.frames[0].starts.load()-starts_before)+
        ",\"actual_native_END_delta\":"+std::to_string(s.frames[0].ends.load()-ends_before)+",\"source_GUI_known\":"+(gui_known?"true":"false")+
        ",\"source_GUI_idle\":"+(gui_idle?"true":"false")+",\"actual_cursor\":"+(cursor_known?"["+std::to_string(cursor_after.x)+","+std::to_string(cursor_after.y)+"]":"null")+
        ",\"path\":"+quote(native_result?"native_only_passthrough_not_enhancement_or_NormalUp":enhanced_result?"enhanced_actual_UP_before_retirement":"UNKNOWN_missing_required_actual_release_evidence")+
        ",\"actual_pending_receipt\":"+quote(pending)+",\"actual_native_passthrough_LL_UP\":"+quote(release)+
        ",\"actual_product_native_START\":"+quote(actual_start)+",\"actual_product_native_END\":"+quote(actual_end)+
        ",\"Glue_follower_or_enhanced_Resize_inferred_from_native_only\":false"+
        ",\"actual_UP_qpc\":"+std::to_string(actual_up_qpc)+",\"source_native_before\":"+native_before+
        ",\"source_native_after\":"+native_move_boundary_json(s.frames[0])+
        ",\"actual_source_WM_ENTERSIZEMOVE_qpc\":"+std::to_string(s.frames[0].native_start_qpc.load())+
        ",\"actual_source_WM_EXITSIZEMOVE_qpc\":"+std::to_string(s.frames[0].native_end_qpc.load())+
        ",\"source_queue_UP_observation\":"+queue_observer.json()+",\"queue_UP_used_as_native_only_release_alternative\":"+
        (native_result&&!native_LL_up&&owned_queue_up?"true":"false")+",\"queue_UP_is_product_NormalUp\":false"+
        ",\"product_actual_UP\":"+quote(observed_up)+",\"production_end\":"+quote(end)+
        ",\"passive_hook\":"+observer.json()+",\"GUI_lag_not_inferred_from_test_insertion\":true");
    for(std::size_t n=0;n<3;++n){s.log.check("rapid_terminal_restore_fixture_setup",fixture_rect(s.frames[n].hwnd,previous[n]),
        "\"member\":"+std::to_string(n)+",\"test_setup_only\":true"+route_fields);ShowWindow(s.frames[n].hwnd,SW_SHOWNOACTIVATE);}
}
void owned_rapid_terminal_move(State& s){
    owned_rapid_terminal_case(s,false,false);
    owned_rapid_terminal_case(s,true,false);
    owned_rapid_terminal_case(s,false,true);
    owned_rapid_terminal_case(s,true,true);
}
