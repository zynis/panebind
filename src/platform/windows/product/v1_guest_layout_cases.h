#pragma once

// Guest-only cases: include inside v1_guest_fixture.cpp's unnamed namespace,
// AFTER its State/Frame, native input, evidence and geometry helpers. This is
// not a product header or an alternate solver. All expected rectangles below
// are independent contract values; only the real product computes placements.
// Deliberately not called until the parent selects this L2 phase.

struct LayoutLimit {LONG min_width{100},min_height{100},max_width{2000},max_height{1400};};
// Only the existing fixture UI STA creates, configures and destroys these
// windows. The driver is free to perform synchronous evidence IO without
// starving the product's bounded WM_GETMINMAXINFO query.
constexpr UINT layout_ui_command=WM_APP+0x62;
struct LayoutUiStorage {
    std::array<Frame,8> frames;std::array<HWND,3> originals{};
    std::array<LONG_PTR,3> original_data{};std::array<bool,3> old_visible{},visibility_saved{};
    HINSTANCE instance{};std::wstring class_name;DWORD owner{};HWND anchor{};
    std::atomic<bool> abandoned{};bool cleaned{};
    // Windows retain pointers to Frame. Keep that storage alive even when a
    // driver request times out; only the owner may release it after destruction.
    std::shared_ptr<LayoutUiStorage> window_lifetime;
};
enum class LayoutUiOperation {Create,Configure,Destroy,DestroyOne,Grid};
struct LayoutNativeSetup {
    bool attempted{},returned{};DWORD error{};Rect requested;UINT dpi{};
    POINT system_min{},system_min_track{},declared_min{},declared_max{};
    std::optional<Rect> after_visible,after_positioning;
};
struct LayoutUiRequest {
    std::shared_ptr<LayoutUiStorage> storage;LayoutUiOperation operation{};
    std::vector<Rect> expected;std::vector<LayoutLimit> limits;std::size_t member{};LONG grid{1};
    std::atomic<bool> cancelled{},done{},cleanup_done{};bool success{},cleanup_success{};std::string error;
    std::array<LayoutNativeSetup,8> native_setup;
};
std::atomic<std::shared_ptr<LayoutUiRequest>> layout_ui_pending;
bool layout_ui_owns(const LayoutUiStorage& storage,std::size_t index){
    if(index>=storage.frames.size())return false;const auto& frame=storage.frames[index];DWORD pid{};
    return frame.hwnd&&IsWindow(frame.hwnd)&&GetWindowThreadProcessId(frame.hwnd,&pid)==storage.owner&&pid==GetCurrentProcessId()&&
        GetAncestor(frame.hwnd,GA_ROOT)==frame.hwnd&&gui_class(frame.hwnd)==storage.class_name&&
        GetWindowLongPtrW(frame.hwnd,GWLP_USERDATA)==reinterpret_cast<LONG_PTR>(&frame);
}
bool layout_ui_original(const LayoutUiStorage& storage,std::size_t index){
    DWORD pid{};const HWND h=storage.originals[index];return h&&IsWindow(h)&&
        GetWindowThreadProcessId(h,&pid)==storage.owner&&pid==GetCurrentProcessId()&&GetAncestor(h,GA_ROOT)==h&&
        gui_class(h)==storage.class_name&&GetWindowLongPtrW(h,GWLP_USERDATA)==storage.original_data[index];
}
bool layout_ui_cleanup(const std::shared_ptr<LayoutUiStorage>& storage){
    if(GetCurrentThreadId()!=storage->owner)return false;bool exact=true;
    for(std::size_t n=0;n<storage->frames.size();++n){auto& frame=storage->frames[n];if(!frame.hwnd)continue;
        if(!IsWindow(frame.hwnd)){frame.hwnd=nullptr;continue;}
        if(!layout_ui_owns(*storage,n)){exact=false;continue;}const HWND h=frame.hwnd;
        if(DestroyWindow(h)&&!IsWindow(h))frame.hwnd=nullptr;else exact=false;
    }
    for(std::size_t n=0;n<storage->originals.size();++n)if(storage->visibility_saved[n]){
        if(!layout_ui_original(*storage,n)){exact=false;continue;}
        ShowWindow(storage->originals[n],storage->old_visible[n]?SW_SHOWNOACTIVATE:SW_HIDE);
        if((IsWindowVisible(storage->originals[n])!=FALSE)!=storage->old_visible[n])exact=false;
    }
    const bool gone=std::all_of(storage->frames.begin(),storage->frames.end(),[](const Frame& f){return !f.hwnd;});
    storage->cleaned=exact&&gone;if(gone)storage->window_lifetime.reset();return storage->cleaned;
}
bool layout_ui_message(HWND h,UINT message,WPARAM,LPARAM lp){
    if(message!=layout_ui_command)return false;auto request=layout_ui_pending.load(std::memory_order_acquire);
    if(!request||reinterpret_cast<LPARAM>(request.get())!=lp)return true;
    const auto storage=request->storage;
    if(h!=storage->anchor||GetCurrentThreadId()!=storage->owner||!layout_ui_original(*storage,0))return true;
    // A timed-out request remains owned by pending until this exact UI thread
    // can cancel it. No queued message ever refers to stack Frame storage.
    if(request->cancelled.load()||storage->abandoned.load()){
        request->cleanup_success=layout_ui_cleanup(storage);request->success=false;
        request->cleanup_done.store(true,std::memory_order_release);request->done.store(true,std::memory_order_release);
        auto same=request;layout_ui_pending.compare_exchange_strong(same,{});return true;
    }
    try{
        const auto check_cancel=[&]{if(request->cancelled.load()||storage->abandoned.load())throw std::runtime_error("layout_UI_request_cancelled");};
        if(request->operation==LayoutUiOperation::Create){
            storage->window_lifetime=storage;
            for(std::size_t n=0;n<storage->originals.size();++n){check_cancel();if(!layout_ui_original(*storage,n))throw std::runtime_error("layout_original_identity_changed");
                storage->old_visible[n]=IsWindowVisible(storage->originals[n])!=FALSE;storage->visibility_saved[n]=true;ShowWindow(storage->originals[n],SW_HIDE);}
            for(std::size_t n=0;n<storage->frames.size();++n){check_cancel();auto& frame=storage->frames[n];
                const auto title=L"PaneBind V1 layout — "+std::to_wstring(n+1);
                frame.hwnd=CreateWindowExW(0,storage->class_name.c_str(),title.c_str(),WS_OVERLAPPEDWINDOW,
                    100+static_cast<int>(n)*20,100,400,400,nullptr,nullptr,storage->instance,&frame);
                if(!frame.hwnd||!layout_ui_owns(*storage,n))throw std::runtime_error("layout_test_window_creation_failed");
                ShowWindow(frame.hwnd,SW_SHOWNOACTIVATE);UpdateWindow(frame.hwnd);
            }
        }else if(request->operation==LayoutUiOperation::Configure){
            for(std::size_t n=0;n<storage->frames.size();++n){check_cancel();if(!layout_ui_owns(*storage,n))throw std::runtime_error("layout_owned_identity_changed");
                auto& frame=storage->frames[n];if(n>=request->expected.size()){ShowWindow(frame.hwnd,SW_HIDE);continue;}
                const auto observed=visible(frame.hwnd);RECT positioning{};
                if(!observed||!GetWindowRect(frame.hwnd,&positioning))throw std::runtime_error("layout_frame_insets_unknown");
                const LONG dx=positioning.right-positioning.left-observed->width(),dy=positioning.bottom-positioning.top-observed->height();
                const auto bound=request->limits.empty()?LayoutLimit{}:request->limits[n];
                if(bound.min_width<=0||bound.min_height<=0||bound.max_width<bound.min_width||bound.max_height<bound.min_height||
                    bound.min_width+dx<=0||bound.min_height+dy<=0)throw std::runtime_error("layout_test_constraint_invalid");
                frame.min_width=bound.min_width+dx;frame.min_height=bound.min_height+dy;
                frame.max_width=bound.max_width+dx;frame.max_height=bound.max_height+dy;frame.grid=1;
                const auto target=request->expected[n];const LONG left=target.l-(observed->l-positioning.left),top=target.t-(observed->t-positioning.top);
                const LONG right=target.r+(positioning.right-observed->r),bottom=target.b+(positioning.bottom-observed->b);
                ShowWindow(frame.hwnd,SW_SHOWNOACTIVATE);
                auto& receipt=request->native_setup[n];receipt.dpi=GetDpiForWindow(frame.hwnd);receipt.requested={left,top,right,bottom};
                receipt.system_min={GetSystemMetricsForDpi(SM_CXMIN,receipt.dpi),GetSystemMetricsForDpi(SM_CYMIN,receipt.dpi)};
                receipt.system_min_track={GetSystemMetricsForDpi(SM_CXMINTRACK,receipt.dpi),GetSystemMetricsForDpi(SM_CYMINTRACK,receipt.dpi)};
                receipt.declared_min={frame.min_width,frame.min_height};receipt.declared_max={frame.max_width,frame.max_height};
                receipt.attempted=true;SetLastError(0);
                receipt.returned=SetWindowPos(frame.hwnd,nullptr,left,top,right-left,bottom-top,SWP_NOACTIVATE|SWP_NOZORDER)!=FALSE;
                receipt.error=receipt.returned?0:GetLastError();
                receipt.after_visible=visible(frame.hwnd);RECT actual{};
                if(GetWindowRect(frame.hwnd,&actual))receipt.after_positioning=Rect{actual.left,actual.top,actual.right,actual.bottom};
                if(!receipt.returned)
                    throw std::runtime_error("layout_owned_setup_native_placement_failed");
            }
        }else if(request->operation==LayoutUiOperation::Destroy){
            if(!layout_ui_cleanup(storage))throw std::runtime_error("layout_owned_UI_cleanup_failed");
        }else{
            check_cancel();if(!layout_ui_owns(*storage,request->member))throw std::runtime_error("layout_owned_member_identity_changed");
            auto& frame=storage->frames[request->member];
            if(request->operation==LayoutUiOperation::Grid){if(request->grid<1)throw std::runtime_error("layout_invalid_native_grid");frame.grid=request->grid;}
            else{const HWND original=frame.hwnd;if(!DestroyWindow(original)||IsWindow(original))throw std::runtime_error("layout_owned_member_destroy_failed");frame.hwnd=nullptr;}
        }
        check_cancel();request->success=true;
    }catch(const std::exception& error){request->error=error.what();storage->abandoned=true;request->success=false;}
    // Normal requests are acknowledged by the driver. Keeping pending until
    // that acknowledgement closes the timeout-vs-completion cleanup race.
    const bool abandoning=request->cancelled.load()||storage->abandoned.load();
    if(abandoning){request->success=false;request->cleanup_success=layout_ui_cleanup(storage);request->cleanup_done.store(true,std::memory_order_release);}
    request->done.store(true,std::memory_order_release);
    if(abandoning){auto same=request;layout_ui_pending.compare_exchange_strong(same,{});}
    return true;
}
struct LayoutFixtures {
    State& state;std::shared_ptr<LayoutUiStorage> storage;std::array<Frame,8>& frames;
    DWORD thread{};std::size_t active{};bool closed{},cleanup_exact{},configuration_observed{};std::array<LayoutNativeSetup,8> configured_native;
    explicit LayoutFixtures(State& s):state(s),storage(std::make_shared<LayoutUiStorage>()),frames(storage->frames),thread(s.owner){
        storage->instance=s.instance;storage->class_name=s.class_name;storage->owner=s.owner;storage->anchor=s.frames[0].hwnd;
        for(std::size_t n=0;n<s.frames.size();++n){storage->originals[n]=s.frames[n].hwnd;storage->original_data[n]=reinterpret_cast<LONG_PTR>(&s.frames[n]);}
    }
    bool owns(std::size_t index)const{return layout_ui_owns(*storage,index);}
    bool run(LayoutUiOperation operation,const std::vector<Rect>& expected={},const std::vector<LayoutLimit>& limits={},std::size_t member=0,LONG grid=1){
        if(operation==LayoutUiOperation::Configure)configuration_observed=false;
        auto request=std::make_shared<LayoutUiRequest>();request->storage=storage;request->operation=operation;request->expected=expected;
        request->limits=limits;request->member=member;request->grid=grid;std::shared_ptr<LayoutUiRequest> empty;
        if(!layout_ui_pending.compare_exchange_strong(empty,request))return false;
        if(!PostMessageW(storage->anchor,layout_ui_command,0,reinterpret_cast<LPARAM>(request.get()))){
            auto same=request;layout_ui_pending.compare_exchange_strong(same,{});return false;
        }
        const bool completed=wait([&]{return request->done.load(std::memory_order_acquire);},3500);
        if(!completed){request->cancelled=true;storage->abandoned=true;
            // One bounded cleanup request, not a retry loop. Its shared owner
            // persists if the UI is still unavailable; that is never PASS.
            (void)PostMessageW(storage->anchor,layout_ui_command,0,reinterpret_cast<LPARAM>(request.get()));
            const bool cleanup_observed=wait([&]{return request->cleanup_done.load(std::memory_order_acquire);},1000);
            state.log.emit("layout_UI_timeout","\"owner_tid\":"+std::to_string(thread)+",\"operation\":"+
                std::to_string(static_cast<unsigned>(operation))+",\"cleanup_completed\":"+(cleanup_observed?"true":"false")+
                ",\"cleanup_exact\":"+(cleanup_observed?(request->cleanup_success?"true":"false"):"null")+",\"request_PASS\":false");return false;
        }
        if(operation==LayoutUiOperation::Configure){configured_native=request->native_setup;configuration_observed=true;}
        auto same=request;layout_ui_pending.compare_exchange_strong(same,{});
        if(!request->success&&!request->error.empty())state.log.emit("layout_UI_failure","\"reason\":"+quote(request->error)+",\"owner_tid\":"+std::to_string(thread));
        return request->success;
    }
    void create(){
        if(!state.process||WaitForSingleObject(state.process,0)!=WAIT_TIMEOUT)throw std::runtime_error("layout_product_not_running");
        for(std::size_t n=0;n<3;++n){
            if(state.explorer[n]||state.applications[n]||!own(state,state.frames[n].hwnd))throw std::runtime_error("layout_other_targets_must_be_released_first");
        }
        if(!run(LayoutUiOperation::Create))throw std::runtime_error("layout_main_UI_creation_failed");
        for(std::size_t n=0;n<frames.size();++n)if(!wait([&]{return owns(n)&&visible(frames[n].hwnd).has_value();}))throw std::runtime_error("layout_actual_frame_unavailable");
        state.log.emit("layout_owned_scope","\"windows\":8,\"pid\":"+std::to_string(GetCurrentProcessId())+
            ",\"tid\":"+std::to_string(thread)+",\"driver_tid\":"+std::to_string(GetCurrentThreadId())+
            ",\"existing_main_UI_STA\":true,\"original_driver_frames_hidden\":true,\"app_grid\":1,\"automatic_guest_driver\":true");
    }
    void setup(const std::vector<Rect>& expected,const std::vector<LayoutLimit>& limits={}){
        if(expected.empty()||expected.size()>frames.size()||(!limits.empty()&&limits.size()!=expected.size()))throw std::runtime_error("layout_case_bound_invalid");
        if(state.left||state.control)throw std::runtime_error("layout_setup_requires_released_input");
        if(!run(LayoutUiOperation::Configure,expected,limits)){
            if(configuration_observed)for(std::size_t n=0;n<expected.size();++n)if(configured_native[n].attempted){const auto& native=configured_native[n];
                state.log.emit("layout_native_configuration_failure","\"member\":"+std::to_string(n)+",\"requested_positioning\":"+rect(native.requested)+
                    ",\"SetWindowPos_return\":"+(native.returned?"true":"false")+",\"SetWindowPos_error\":"+std::to_string(native.error)+
                    ",\"observed_visible_before_cleanup\":"+(native.after_visible?rect(*native.after_visible):"null")+
                    ",\"observed_positioning_before_cleanup\":"+(native.after_positioning?rect(*native.after_positioning):"null")+
                    ",\"actual_DPI\":"+std::to_string(native.dpi)+",\"SM_CXMIN_CYMIN\":["+std::to_string(native.system_min.x)+","+std::to_string(native.system_min.y)+
                    "],\"SM_CXMINTRACK_CYMINTRACK\":["+std::to_string(native.system_min_track.x)+","+std::to_string(native.system_min_track.y)+
                    "],\"declared_native_min\":["+std::to_string(native.declared_min.x)+","+std::to_string(native.declared_min.y)+"]"+
                    ",\"cleanup_may_already_have_destroyed_exact_window\":true");}
            throw std::runtime_error("layout_main_UI_configuration_failed");
        }active=expected.size();
        for(std::size_t n=0;n<active;++n){
            const auto bound=limits.empty()?LayoutLimit{}:limits[n];
            const bool exact=wait([&]{return owns(n)&&visible(frames[n].hwnd)==expected[n];},1000);
            const auto actual=visible(frames[n].hwnd);RECT actual_native{};const bool positioned=GetWindowRect(frames[n].hwnd,&actual_native)!=FALSE;
            const auto& native=configured_native[n];
            state.log.check("layout_actual_independent_setup",exact,
                "\"member\":"+std::to_string(n)+",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(frames[n].hwnd))+
                ",\"expected_visible\":"+rect(expected[n])+",\"declared_visible_min\":["+std::to_string(bound.min_width)+","+
                std::to_string(bound.min_height)+"],\"declared_visible_max\":["+std::to_string(bound.max_width)+","+
                std::to_string(bound.max_height)+"],\"native_grid\":1,\"not_claiming_unknown_application_grid\":true"+
                (exact?std::string{}:",\"actual_visible\":"+(actual?rect(*actual):"null")+",\"actual_positioning\":"+
                    (positioned?rect({actual_native.left,actual_native.top,actual_native.right,actual_native.bottom}):"null")+
                    ",\"requested_positioning\":"+rect(native.requested)+",\"fixture_SetWindowPos_attempted\":"+(native.attempted?"true":"false")+
                    ",\"fixture_SetWindowPos_return\":"+(native.returned?"true":"false")+",\"fixture_SetWindowPos_error\":"+std::to_string(native.error)+
                    ",\"actual_DPI\":"+std::to_string(native.dpi)+",\"SM_CXMIN_CYMIN\":["+std::to_string(native.system_min.x)+","+std::to_string(native.system_min.y)+
                    "],\"SM_CXMINTRACK_CYMINTRACK\":["+std::to_string(native.system_min_track.x)+","+std::to_string(native.system_min_track.y)+
                    "],\"declared_native_min\":["+std::to_string(native.declared_min.x)+","+std::to_string(native.declared_min.y)+
                    "],\"declared_native_max\":["+std::to_string(native.declared_max.x)+","+std::to_string(native.declared_max.y)+"]"));
        }
    }
    bool destroy(std::size_t member){return member<frames.size()&&run(LayoutUiOperation::DestroyOne,{}, {},member);}
    bool set_grid(std::size_t member,LONG grid){return member<frames.size()&&grid>0&&run(LayoutUiOperation::Grid,{}, {},member,grid);}
    bool close(){
        if(closed)return cleanup_exact;closed=true;cleanup_exact=run(LayoutUiOperation::Destroy);if(!cleanup_exact)state.log.failed=true;return cleanup_exact;
    }
    ~LayoutFixtures(){try{if(!closed)close();}catch(...){state.log.failed=true;}} // test-owned windows only, not product cleanup PASS
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
// The existing UI STA now owns these HWNDs and independently serves tracking
// queries. This driver wait also services nonqueued requests without dispatching
// unrelated queued input while it awaits a real product START receipt.
// This finite test observation is not a product input/polling mechanism.
bool layout_wait_sent(const std::function<bool()>& observed,DWORD duration){
    const auto deadline=GetTickCount64()+duration;
    for(;;){
        MSG message{};
        if(PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE|PM_QS_SENDMESSAGE)&&message.message==WM_QUIT)
            throw std::runtime_error("layout_driver_quit_during_sent_message_wait");
        if(observed())return true;
        const auto now=GetTickCount64();if(now>=deadline)return false;
        const auto pause=static_cast<DWORD>(std::min<ULONGLONG>(20,deadline-now));
        const auto result=MsgWaitForMultipleObjectsEx(0,nullptr,pause,QS_SENDMESSAGE,MWMO_INPUTAVAILABLE);
        if(result!=WAIT_OBJECT_0&&result!=WAIT_TIMEOUT)throw std::runtime_error("layout_sent_message_wait_failed");
    }
}
void layout_failed_start_release(State& s,std::uint64_t generation,bool down_sent){
    bool up_sent=false,control_sent=false,released=false;std::string error;
    try{
        // Send the real test UP BEFORE ordinary queue dispatch/stack cleanup.
        // A fallback native modal loop must not get entered while DOWN remains
        // held, and an injection return is not a product NormalUp receipt.
        if(s.left){button(s,false);up_sent=true;}
        if(s.control){INPUT packet{};packet.type=INPUT_KEYBOARD;packet.ki.wVk=VK_CONTROL;
            packet.ki.dwFlags=KEYEVENTF_KEYUP;packet.ki.dwExtraInfo=0x50425631;
            if(!desktop()||SendInput(1,&packet,sizeof(packet))!=1)throw std::runtime_error("layout_failed_start_control_release_failed");
            s.control=false;control_sent=true;}
        released=layout_wait_sent([&]{return !(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&!(GetAsyncKeyState(VK_CONTROL)&0x8000);},250);
    }catch(const std::exception& failure){error=failure.what();}
    s.log.emit("layout_failed_START_test_cleanup","\"generation\":"+std::to_string(generation)+
        ",\"test_DOWN_sent\":"+(down_sent?"true":"false")+",\"test_UP_sent\":"+(up_sent?"true":"false")+
        ",\"test_Control_UP_sent\":"+(control_sent?"true":"false")+",\"released_state_observed\":"+(released?"true":"false")+
        ",\"error\":"+quote(error)+",\"queued_NC_input_dispatched_before_release\":false,\"product_NormalUp_or_fallback_PASS_inferred\":false");
}
Gesture layout_down(LayoutFixtures& set,std::size_t source,int region,bool ctrl){
    auto& s=set.state;if(source>=set.active||!set.owns(source)||s.left||s.control)throw std::runtime_error("layout_source_not_exact_or_input_busy");
    const HWND h=set.frames[source].hwnd;
    s.log.check("layout_exact_source_foreground",SetForegroundWindow(h)&&wait([&]{return GetForegroundWindow()==h;}));
    const auto point=layout_hit(set,source,region);if(!point)throw std::runtime_error("layout_real_requested_HT_region_unavailable");
    const auto original_visible=visible(h);RECT original_positioning{};
    if(!original_visible||!GetWindowRect(h,&original_positioning))throw std::runtime_error("layout_pre_DOWN_geometry_unknown");
    Gesture g{*point,0,0};bool down_sent=false,started=false;std::string start;
    try{
        // Keep the original 100 ms preparation interval, but actually serve
        // the product's synchronous queries. No Sleep/key/launch gap after it.
        if(ctrl)key(s,VK_CONTROL,true);g.offset=read(s.product_log).size();
        for(unsigned preparation=0;preparation<2;++preparation){
            cursor(s,*point);const auto hover=GetTickCount64();
            (void)layout_wait_sent([&]{return GetTickCount64()-hover>=100;},100);
            const bool exact=set.owns(source);const auto actual_visible=visible(h);RECT actual_positioning{};
            const bool positioning_known=GetWindowRect(h,&actual_positioning)!=FALSE;
            const bool geometry_unchanged=actual_visible==original_visible&&positioning_known&&EqualRect(&actual_positioning,&original_positioning);
            POINT actual_cursor{};const bool cursor_known=GetCursorPos(&actual_cursor)!=FALSE;
            const HWND actual_hit=cursor_known?WindowFromPoint(actual_cursor):nullptr;
            const HWND actual_root=actual_hit?GetAncestor(actual_hit,GA_ROOT):nullptr;
            DWORD hit_pid{},foreground_pid{};const DWORD hit_tid=actual_hit?GetWindowThreadProcessId(actual_hit,&hit_pid):0;
            const HWND foreground_now=GetForegroundWindow();const DWORD foreground_tid=foreground_now?GetWindowThreadProcessId(foreground_now,&foreground_pid):0;
            DWORD_PTR actual_region{};const bool region_known=actual_hit==h&&SendMessageTimeoutW(h,WM_NCHITTEST,0,
                MAKELPARAM(static_cast<short>(point->x),static_cast<short>(point->y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&actual_region)!=FALSE;
            GUITHREADINFO native{sizeof(native)};SetLastError(0);const bool gui_known=GetGUIThreadInfo(set.thread,&native)!=FALSE;
            const DWORD gui_error=gui_known?0:GetLastError();
            const bool native_idle=gui_known&&!native.hwndCapture&&!native.hwndMoveSize&&!native.hwndMenuOwner;
            const bool ready=exact&&geometry_unchanged&&foreground_now==h&&cursor_known&&actual_cursor.x==point->x&&actual_cursor.y==point->y&&
                actual_root==h&&actual_hit==h&&region_known&&static_cast<int>(actual_region)==region&&native_idle;
            // Do not write evidence between a successful fresh check and DOWN.
            // On rejection preserve each actual fact instead of an opaque OR.
            if(ready)break;
            s.log.emit("layout_pre_DOWN_preparation_rejected","\"preparation\":"+std::to_string(preparation+1)+
                ",\"test_DOWN_sent\":false,\"exact_source\":"+(exact?"true":"false")+",\"geometry_unchanged\":"+(geometry_unchanged?"true":"false")+
                ",\"expected_visible\":"+rect(*original_visible)+",\"actual_visible\":"+(actual_visible?rect(*actual_visible):"null")+
                ",\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+",\"actual_foreground\":"+
                std::to_string(reinterpret_cast<std::uintptr_t>(foreground_now))+",\"foreground_PID\":"+std::to_string(foreground_pid)+
                ",\"foreground_TID\":"+std::to_string(foreground_tid)+",\"foreground_class\":"+(foreground_now?quote(utf8(gui_class(foreground_now))):"null")+
                ",\"actual_cursor\":"+(cursor_known?"["+std::to_string(actual_cursor.x)+","+std::to_string(actual_cursor.y)+"]":"null")+
                ",\"actual_hit\":"+std::to_string(reinterpret_cast<std::uintptr_t>(actual_hit))+",\"hit_root\":"+
                std::to_string(reinterpret_cast<std::uintptr_t>(actual_root))+",\"hit_PID\":"+std::to_string(hit_pid)+",\"hit_TID\":"+std::to_string(hit_tid)+
                ",\"actual_native_region\":"+(region_known?std::to_string(actual_region):"null")+",\"expected_native_region\":"+std::to_string(region)+
                ",\"source_GUI_known\":"+(gui_known?"true":"false")+",\"source_GUI_error\":"+std::to_string(gui_error)+
                ",\"actual_capture\":"+(gui_known?std::to_string(reinterpret_cast<std::uintptr_t>(native.hwndCapture)):"null")+
                ",\"actual_move_size\":"+(gui_known?std::to_string(reinterpret_cast<std::uintptr_t>(native.hwndMoveSize)):"null")+
                ",\"actual_menu_owner\":"+(gui_known?std::to_string(reinterpret_cast<std::uintptr_t>(native.hwndMenuOwner)):"null")+
                ",\"unknown_window_not_manipulated\":true");
            // One bounded repair of exact TEST-owned foreground/hover setup,
            // never a retry after input. Identity/geometry/native-context
            // failures cannot be accepted as a new preparation baseline.
            if(preparation||!exact||!geometry_unchanged||!native_idle||!set.owns(source))
                throw std::runtime_error("layout_source_or_hover_hit_changed_before_DOWN");
            const bool foreground_return=SetForegroundWindow(h)!=FALSE;
            const bool foreground_observed=layout_wait_sent([&]{return GetForegroundWindow()==h;},1000);
            s.log.check("layout_exact_owned_pre_DOWN_reprepare",foreground_return&&foreground_observed,
                "\"SetForegroundWindow_return\":"+std::string(foreground_return?"true":"false")+
                ",\"actual_foreground_matches\":"+(foreground_observed?"true":"false")+",\"test_DOWN_sent\":false,\"maximum_repreparations\":1");
        }
        button(s,true);down_sent=true;
        started=layout_wait_sent([&]{g.generation=number(record(s,g.offset,"gesture_start"),"\"generation\":");return g.generation!=0;},3500);
        start=record(s,g.offset,"gesture_start",g.generation);
    }catch(...){layout_failed_start_release(s,g.generation,down_sent);throw;}
    GUITHREADINFO native{sizeof(native)};const bool native_read=GetGUIThreadInfo(set.thread,&native)!=FALSE;
    POINT actual_cursor{};const bool cursor_known=GetCursorPos(&actual_cursor)!=FALSE;
    const HWND hit=cursor_known?WindowFromPoint(actual_cursor):nullptr,hit_root=hit?GetAncestor(hit,GA_ROOT):nullptr;
    const auto capture=release_capture_facts(s,h,hit_root,g.generation);
    const bool exact_shield=cursor_known&&product::guest::captured_release_hit_allowed(capture);
    const bool native_idle=native_read&&!native.hwndCapture&&!native.hwndMoveSize&&!native.hwndMenuOwner&&
        !(native.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE));
    // A real READY lease has no idle fallback: an already lost/released
    // capture cannot pass as "no capture". Old product routes without ANY
    // matching-process READY receipt keep their former idle-only contract.
    const bool capture_context=exact_shield||(!capture.generation&&native_idle);
    const bool start_identity=g.generation&&number(start,"\"process_id\":")==s.product_pid&&
        start.find("\"source_sha\":"+quote(s.product_source))!=start.npos;
    const bool accepted=started&&start_identity&&set.owns(source)&&GetForegroundWindow()==h&&native_read&&capture_context&&
        !native.hwndMoveSize&&!native.hwndMenuOwner&&
        start.find(ctrl?"\"ctrl\":true":"\"ctrl\":false")!=std::string::npos&&
        start.find(region==HTCAPTION?"\"resize\":false":"\"resize\":true")!=std::string::npos;
    if(!accepted)layout_failed_start_release(s,g.generation,down_sent);
    s.log.check("layout_real_DOWN_production_start",accepted,
        "\"source\":"+std::to_string(source)+",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
        ",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"tid\":"+std::to_string(set.thread)+
        ",\"native_hit\":"+std::to_string(region)+",\"generation\":"+std::to_string(g.generation)+
        ",\"START_wait_services_sent_only\":true,\"actual_GUI_facts_available\":"+(native_read?"true":"false")+
        ",\"actual_capture\":"+(native_read?std::to_string(reinterpret_cast<std::uintptr_t>(native.hwndCapture)):"null")+
        ",\"actual_move_size\":"+(native_read?std::to_string(reinterpret_cast<std::uintptr_t>(native.hwndMoveSize)):"null")+
        ",\"capture_context\":"+quote(exact_shield?"exact_current_product_shield":capture_context?"legacy_native_idle":"rejected")+
        ",\"START_exact_product_identity\":"+(start_identity?"true":"false")+
        ",\"ready_generation\":"+std::to_string(capture.generation)+",\"ready_overlay\":"+std::to_string(capture.overlay)+
        ",\"overlay_actual_PID\":"+std::to_string(capture.overlay_pid)+",\"overlay_actual_TID\":"+std::to_string(capture.overlay_tid)+
        ",\"overlay_owner_actual_capture\":"+std::to_string(capture.owner_capture)+
        ",\"actual_hit_root\":"+std::to_string(capture.hit_root)+",\"ready_not_retired\":"+(capture.not_retired?"true":"false")+
        ",\"actual_native_modal_starts\":"+std::to_string(set.frames[source].starts.load())+",\"production_start\":"+quote(start));return g;
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
    s.log.check("layout_actual_matching_UP",wait([&]{return !actual_up(s,mark,g.generation).empty()&&record(s,mark,"gesture_end",g.generation).find("\"normal_up\":true")!=std::string::npos;}));
    Sleep(100);GUITHREADINFO gui{sizeof(gui)};bool stable=!s.left&&GetGUIThreadInfo(set.thread,&gui)&&!gui.hwndCapture;
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

    std::vector<Rect> eight,moved;std::vector<LayoutLimit> eight_limits;
    try{
        MONITORINFO display{sizeof(display)};const auto monitor=MonitorFromWindow(set.frames[0].hwnd,MONITOR_DEFAULTTONULL);
        if(!monitor||!GetMonitorInfoW(monitor,&display))throw std::runtime_error("eight_owned_actual_monitor_unknown");
        const LONG work_width=display.rcWork.right-display.rcWork.left,work_height=display.rcWork.bottom-display.rcWork.top;
        LONG width=250,height=180,margin=0;
        for(std::size_t n=0;n<8;++n){const auto frame=visible(set.frames[n].hwnd);RECT native{};const auto dpi=GetDpiForWindow(set.frames[n].hwnd);
            if(!set.owns(n)||!frame||!GetWindowRect(set.frames[n].hwnd,&native)||!dpi||MonitorFromWindow(set.frames[n].hwnd,MONITOR_DEFAULTTONULL)!=monitor)
                throw std::runtime_error("eight_owned_actual_frame_or_DPI_unknown");
            const LONG dx=native.right-native.left-frame->width(),dy=native.bottom-native.top-frame->height();
            const LONG padding=MulDiv(32,static_cast<int>(dpi),96);
            const LONG min_w=std::max(GetSystemMetricsForDpi(SM_CXMIN,dpi),GetSystemMetricsForDpi(SM_CXMINTRACK,dpi));
            const LONG min_h=std::max(GetSystemMetricsForDpi(SM_CYMIN,dpi),GetSystemMetricsForDpi(SM_CYMINTRACK,dpi));
            if(padding<=0||min_w<=0||min_h<=0||dx<0||dy<0)throw std::runtime_error("eight_owned_native_minimum_unknown");
            width=std::max(width,min_w-dx+padding);height=std::max(height,min_h-dy+padding);margin=std::max(margin,padding);
            s.log.emit("eight_owned_actual_native_minimum","\"member\":"+std::to_string(n)+",\"dpi\":"+std::to_string(dpi)+
                ",\"native_minimum\":["+std::to_string(min_w)+","+std::to_string(min_h)+"],\"actual_frame_delta\":["+
                std::to_string(dx)+","+std::to_string(dy)+"],\"safety_margin_pixels\":"+std::to_string(padding));
        }
        LONG columns=4,rows=2;
        const auto fits=[&](LONG cols,LONG count){return static_cast<long long>(cols)*width+margin*2+40<=work_width&&static_cast<long long>(count)*height+margin*2+30<=work_height;};
        if(!fits(columns,rows)){columns=2;rows=4;if(!fits(columns,rows))throw std::runtime_error("eight_owned_actual_work_area_too_small");}
        for(LONG n=0;n<8;++n){const LONG x=display.rcWork.left+margin+(n%columns)*width,y=display.rcWork.top+margin+(n/columns)*height;
            const Rect initial{x,y,x+width,y+height};eight.push_back(initial);moved.push_back(translated(initial,40,30));eight_limits.push_back({100,100,work_width,work_height});}
        s.log.emit("eight_owned_independent_geometry","\"columns\":"+std::to_string(columns)+",\"rows\":"+std::to_string(rows)+
            ",\"visible_cell\":["+std::to_string(width)+","+std::to_string(height)+"],\"actual_work_area\":"+
            rect({display.rcWork.left,display.rcWork.top,display.rcWork.right,display.rcWork.bottom})+",\"product_target_not_used_as_oracle\":true");
        set.setup(eight,eight_limits);
    }catch(const std::exception& failure){
        const std::string why=failure.what();
        // This is ONLY the known eight-window pre-DOWN preparation branch.
        // A failed actual gesture never enters this continuation path.
        if(why!="layout_actual_independent_setup"&&why!="layout_main_UI_configuration_failed"&&!why.starts_with("eight_owned_"))throw;
        s.log.failed=true;const auto previous_end=record(s,g.offset,"gesture_end",g.generation);const auto mark=read(s.product_log).size();
        const bool status=launch(s,L"--status",true)&&wait([&]{return record(s,mark,"status").find("\"gesture_active\":false")!=std::string::npos;});
        const bool closed=set.close();GUITHREADINFO gui{sizeof(gui)};
        s.log.check("eight_owned_pre_DOWN_failure_exact_cleanup",closed&&status&&!s.left&&!s.control&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&
            !(GetAsyncKeyState(VK_CONTROL)&0x8000)&&GetGUIThreadInfo(set.thread,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&
            previous_end.find("\"normal_up\":true")!=std::string::npos&&previous_end.find("\"pending_retired\":true")!=std::string::npos,
            "\"original_failure\":"+quote(why)+",\"last_four_window_generation\":"+std::to_string(g.generation)+
            ",\"eight_window_DOWN_sent\":false,\"eight_window_functional_PASS\":false,\"original_FAIL_preserved\":true");return;
    }
    g=layout_down(set,7,HTCAPTION,true);
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
    s.log.check("temporary_inflight_actual_generation_source_key",source_key!=0&&s.left&&actual_up(s,g.offset,g.generation).empty());
    stall.armed=true;Sleep(60);cursor(s,{g.anchor.x-42,g.anchor.y+35});
    const bool native_entered=wait([&]{return stall.entered.load();},800);
    const bool native_entry_valid=native_entered&&s.left&&!stall.finished.load()&&actual_up(s,g.offset,g.generation).empty();
    // Function arguments have no left-to-right evaluation guarantee. Capture
    // actual WndProc evidence only AFTER the wait published entered=true.
    const auto entry_qpc=stall.before_qpc.load();const auto entry_flags=stall.flags.load();
    const auto entry_x=stall.x.load(),entry_y=stall.y.load(),entry_width=stall.width.load(),entry_height=stall.height.load();
    const auto entry_async=stall.async_left_state.load();
    s.log.check("temporary_inflight_real_native_WndProc_entered",native_entry_valid,
        "\"generation\":"+std::to_string(g.generation)+",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
        ",\"message\":\"WM_WINDOWPOSCHANGING\",\"test_owned_pause_ms\":1000,\"native_entry_qpc\":"+std::to_string(entry_qpc)+
        ",\"actual_flags\":"+std::to_string(entry_flags)+",\"actual_positioning\":["+std::to_string(entry_x)+","+
        std::to_string(entry_y)+","+std::to_string(entry_width)+","+std::to_string(entry_height)+"],\"driver_left_outstanding\":"+(s.left?"true":"false")+
        ",\"async_left_at_native_entry\":"+std::to_string(entry_async)+",\"physical_button_held_not_inferred\":true");
    LARGE_INTEGER up_before{},up_after{};QueryPerformanceCounter(&up_before);
    button(s,false);QueryPerformanceCounter(&up_after); // Exactly one real test UP, never a product cleanup substitute.
    s.log.emit("temporary_inflight_real_UP_sent","\"generation\":"+std::to_string(g.generation)+
        ",\"send_before_qpc\":"+std::to_string(up_before.QuadPart)+",\"send_after_qpc\":"+std::to_string(up_after.QuadPart)+
        ",\"source_still_blocked\":"+(stall.finished?"false":"true")+",\"single_test_UP\":true");
    s.log.check("temporary_inflight_UP_before_source_returns",!stall.finished&&up_before.QuadPart>0&&
        static_cast<std::uint64_t>(up_before.QuadPart)>stall.before_qpc.load());
    const auto abort_up=abort_cleanup_up_receipt(s,g.offset,g.generation);
    std::string ended=abort_up.ended;
    const bool retired_with_UP=abort_up.matched;
    const auto initial_UP=actual_up(s,g.offset,g.generation);const bool missing_UP=!retired_with_UP&&initial_UP.empty();
    const std::string retirement_fields="\"generation\":"+std::to_string(g.generation)+",\"NormalUp_not_inferred_from_test_UP\":true,\"actual_UP\":"+
        (initial_UP.empty()?"null":quote(initial_UP))+",\"actual_end\":"+(ended.empty()?"null":quote(ended))+","+abort_up.json();
    if(missing_UP){s.log.failed=true;s.log.emit("case","\"case\":\"temporary_inflight_generation_actually_retired\",\"result\":\"FAIL\","+retirement_fields);}
    else s.log.check("temporary_inflight_generation_actually_retired",retired_with_UP,retirement_fields);
    s.log.emit("temporary_inflight_actual_end","\"generation\":"+std::to_string(g.generation)+",\"production_end\":"+quote(ended)+
        ",\"actual_UP\":"+(initial_UP.empty()?"null":quote(initial_UP)));
    // One UP was inserted by the test driver. Missing receipt remains unknown;
    // this subsequent cursor input must not renew the retired generation.
    cursor(s,{g.anchor.x-58,g.anchor.y+45});
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
    const bool late_observed=wait(find_late,2000);
    s.log.check("temporary_inflight_late_native_receipt_exact",late_observed&&
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
    s.log.check("temporary_inflight_after_test_UP_send_no_new_geometry_call",calls==1&&after_up==0&&!s.left&&
        s.process&&WaitForSingleObject(s.process,0)==WAIT_TIMEOUT,
        "\"issued_geometry_calls\":"+std::to_string(calls)+",\"issued_after_test_UP_send_start\":"+std::to_string(after_up)+
        ",\"actual_UP_timing_not_inferred\":true");
    if(missing_UP){
        const auto status_mark=read(s.product_log).size();std::string status;DWORD status_exit=STILL_ACTIVE;
        const bool requested=launch(s,L"--status",true,&status_exit);
        const bool observed=wait([&]{status=record(s,status_mark,"status");return !status.empty();},1000);
        ended=record(s,g.offset,"gesture_end",g.generation);GUITHREADINFO gui{sizeof(gui)};
        const bool native_idle=GetGUIThreadInfo(s.owner,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner;
        s.log.check("temporary_inflight_missing_UP_actual_cleanup_safe",requested&&status_exit==0&&observed&&
            status.find("\"gesture_active\":false")!=std::string::npos&&status.find("\"hook_owner_running\":true")!=std::string::npos&&
            ended.find("\"normal_up\":false")!=std::string::npos&&ended.find("\"pending_retired\":true")!=std::string::npos&&
            native_idle&&own(s,h)&&visible(h)==late_target&&native_visual_is(h,173,false)&&stall.finished.load()&&calls==1&&after_up==0,
            "\"original_UP_case_FAIL_preserved\":true,\"actual_status\":"+quote(status)+",\"actual_end\":"+quote(ended)+
            ",\"source_native_idle\":"+(native_idle?"true":"false")+",\"late_completion_and_helper_cleanup_required_above\":true");
    }
    // No product restart or reconfiguration between the old completion and
    // this new actual DOWN. The next generation still uses its own anchors.
    if(!missing_UP){
        const auto next=down(s,0);visual_exact(s,h,190,false,"temporary_inflight_next_gesture_alpha190");
        s.log.check("temporary_inflight_next_generation_is_fresh",next.generation>g.generation);
        const auto next_target=translated(late_target,40,30);
        phase(s,0,next,40,30,next_target,"temporary_inflight_next_real_Move_exact",true);up(s,0,next,next_target);
        visual_exact(s,h,173,false,"temporary_inflight_next_real_UP_restores_alpha173");
    }else s.log.emit("case","\"case\":\"temporary_inflight_next_real_Move_UP\",\"result\":\"NOT_RUN\",\"reason\":\"original_matching_UP_not_observed\",\"original_FAIL_preserved\":true");
    visual_gui_save(s,original_config);
    visual_command(s,h,L"OpacityUp",205,false); // retire the permanent ownership before app-side fixture restoration
    SetLastError(0);const auto ex=GetWindowLongPtrW(h,GWL_EXSTYLE);if(!ex&&GetLastError())throw std::runtime_error("temporary_inflight_original_style_read_failed");
    SetLastError(0);const auto prior=SetWindowLongPtrW(h,GWL_EXSTYLE,original_visual->layered?ex|WS_EX_LAYERED:ex&~WS_EX_LAYERED);
    if((!prior&&GetLastError())||(original_visual->layered&&!SetLayeredWindowAttributes(h,original_visual->color,original_visual->alpha,original_visual->flags))||
        !SetWindowPos(h,original_visual->topmost?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE))
        throw std::runtime_error("temporary_inflight_original_visual_restore_failed");
    s.log.check("temporary_inflight_fixture_original_tuple_restored",wait([&]{const auto now=native_visual(h);return now&&*now==*original_visual;}));
    for(std::size_t n=0;n<3;++n){s.log.check("temporary_inflight_original_layout_restored",own(s,s.frames[n].hwnd)&&fixture_rect(s.frames[n].hwnd,original_rects[n]));
        ShowWindow(s.frames[n].hwnd,original_shown[n]?SW_SHOWNOACTIVATE:SW_HIDE);
        s.log.check("temporary_inflight_original_visibility_restored",own(s,s.frames[n].hwnd)&&(IsWindowVisible(s.frames[n].hwnd)!=FALSE)==original_shown[n]);}
    if(missing_UP){
        const auto mark=read(s.product_log).size();std::string status;DWORD exit=STILL_ACTIVE;
        const bool requested=launch(s,L"--status",true,&exit);const bool observed=wait([&]{status=record(s,mark,"status");return !status.empty();},1000);
        GUITHREADINFO gui{sizeof(gui)};const auto restored_config=settings::parse(read(s.product.parent_path()/L"settings.ini"));
        auto expected_config=original_config;if(restored_config.value)expected_config.generation=restored_config.value->generation;
        bool layout_restored=true;for(std::size_t n=0;n<3;++n)layout_restored=layout_restored&&own(s,s.frames[n].hwnd)&&
            visible(s.frames[n].hwnd)==original_rects[n]&&(IsWindowVisible(s.frames[n].hwnd)!=FALSE)==original_shown[n];
        unsigned final_calls{};std::istringstream rows(read(s.product_log).substr(g.offset));std::string observed_row;
        while(std::getline(rows,observed_row))if(observed_row.find("\"type\":\"placement\"")!=std::string::npos&&
            number(observed_row,"\"generation\":")==g.generation&&observed_row.find("\"native_callsite_claimed\":true")!=std::string::npos)++final_calls;
        s.log.check("temporary_inflight_failed_case_restored_safe_for_independent_flow",requested&&exit==0&&observed&&
            status.find("\"gesture_active\":false")!=std::string::npos&&status.find("\"hook_owner_running\":true")!=std::string::npos&&
            restored_config.value&&*restored_config.value==expected_config&&layout_restored&&native_visual(h)==original_visual&&final_calls==1&&
            !s.left&&!s.control&&GetGUIThreadInfo(s.owner,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner,
            "\"actual_status\":"+quote(status)+",\"old_generation_native_calls\":"+std::to_string(final_calls)+
            ",\"original_UP_FAIL_preserved\":true,\"configuration_visual_geometry_visibility_verified\":true");
    }
    // Reset only this completed, logged test injection so later independent
    // writer-exit cases may use the same exact fixture. No product facts or
    // observed raw/legacy event counters are rewritten.
    stall.armed=false;stall.entered=false;stall.finished=false;stall.async_left_state=0;
    s.log.emit("temporary_visual_inflight_sequence","\"result\":"+quote(missing_UP?"FAIL_SAFE_TO_CONTINUE":"PASS")+
        ",\"single_test_UP_sent\":true,\"matching_UP_observed_in_required_interval\":"+(missing_UP?"false":"true")+
        ",\"late_exact_geometry_observed\":true,\"actual_temporary_alpha_restored\":173,\"next_actual_Move_UP\":"+(missing_UP?"false":"true")+
        ",\"next_actual_Move_UP_status\":"+quote(missing_UP?"NOT_RUN":"PASS")+",\"same_product_process\":true,\"human_uat\":\"NOT_RUN\"");
}
