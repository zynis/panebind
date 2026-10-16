// Guest driver only: one real, fast MOVE+UP insertion. No product test hook,
// artificial product stall, solver-derived expectation or synthetic receipt.
void owned_rapid_terminal_move(State& s){
    std::array<Rect,3> previous{};
    for(std::size_t n=0;n<3;++n){const auto actual=visible(s.frames[n].hwnd);
        if(!actual||!own(s,s.frames[n].hwnd))throw std::runtime_error("rapid_terminal_requires_exact_owned_roots");
        previous[n]=*actual;
    }
    for(std::size_t n=1;n<3;++n)ShowWindow(s.frames[n].hwnd,SW_HIDE);
    const Rect start{160,240,560,580},expected{195,271,595,611};
    s.log.check("rapid_terminal_actual_fixture_setup",fixture_rect(s.frames[0].hwnd,start),
        "\"test_setup_only\":true,\"expected\":"+rect(start));
    foreground(s,s.frames[0].hwnd);const auto anchor=hit_point(s.frames[0].hwnd,HTCAPTION);
    s.log.check("rapid_terminal_actual_caption",anchor.has_value());cursor(s,*anchor);Sleep(100);
    if(s.left||s.control)throw std::runtime_error("rapid_terminal_input_ledger_not_idle");
    Gesture g{*anchor,0,read(s.product_log).size()};
    const POINT destination{g.anchor.x+35,g.anchor.y+31};
    const int x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if(!desktop()||!own(s,s.frames[0].hwnd)||GetForegroundWindow()!=s.frames[0].hwnd||
       width<2||height<2||destination.x<x||destination.y<y||destination.x>=x+width||destination.y>=y+height)
        throw std::runtime_error("rapid_terminal_guest_context_changed");
    GuestUpHookObservation observer;s.log.check("rapid_terminal_passive_observer_ready",observer.start());
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
    s.log.check("rapid_terminal_single_DOWN_MOVE_UP_array_inserted",inserted==packets.size(),
        "\"generation\":"+std::to_string(g.generation)+",\"SendInput_return\":"+std::to_string(inserted)+
        ",\"win32_error\":"+std::to_string(error)+",\"before_qpc\":"+std::to_string(before.QuadPart)+
        ",\"after_qpc\":"+std::to_string(after.QuadPart)+",\"wait_for_product_START_before_UP\":false,\"delivery_not_inferred\":true");
    std::string end;
    const bool ended=wait([&]{g.generation=number(record(s,g.offset,"gesture_start"),"\"generation\":");
        if(!g.generation)return false;end=record(s,g.offset,"gesture_end",g.generation);
        return end.find("\"normal_up\":true")!=end.npos;});
    const bool stopped=observer.stop();const auto observed_up=actual_up(s,g.offset,g.generation);
    unsigned terminal_native{},ordinary_native{};
    std::istringstream rows(read(s.product_log).substr(g.offset));std::string row;
    while(std::getline(rows,row))if(row.ends_with('}')&&row.find("\"type\":\"placement\"")!=row.npos&&
        number(row,"\"generation\":")==g.generation&&row.find("\"native_attempted\":true")!=row.npos){
        if(row.find("\"normal_up_completion\":true")!=row.npos)++terminal_native;else ++ordinary_native;
    }
    const auto last=record(s,g.offset,"placement",g.generation);
    Sleep(150);POINT cursor_after{};const bool cursor_known=GetCursorPos(&cursor_after)!=FALSE;
    s.log.check("rapid_terminal_real_UP_final_geometry_and_no_late_write",ended&&!observed_up.empty()&&stopped&&
        visible(s.frames[0].hwnd)==expected&&cursor_known&&cursor_after.x==destination.x&&cursor_after.y==destination.y&&
        terminal_native<=1&&ordinary_native+terminal_native>=1&&last==record(s,g.offset,"placement",g.generation),
        "\"generation\":"+std::to_string(g.generation)+",\"expected\":"+rect(expected)+
        ",\"actual\":"+(visible(s.frames[0].hwnd)?rect(*visible(s.frames[0].hwnd)):"null")+
        ",\"ordinary_native_attempts\":"+std::to_string(ordinary_native)+",\"terminal_native_attempts\":"+std::to_string(terminal_native)+
        ",\"product_actual_UP\":"+quote(observed_up)+",\"production_end\":"+quote(end)+
        ",\"passive_hook\":"+observer.json()+",\"GUI_lag_not_inferred_from_test_insertion\":true");
    for(std::size_t n=0;n<3;++n){s.log.check("rapid_terminal_restore_fixture_setup",fixture_rect(s.frames[n].hwnd,previous[n]),
        "\"member\":"+std::to_string(n)+",\"test_setup_only\":true");ShowWindow(s.frames[n].hwnd,SW_SHOWNOACTIVATE);}
}
