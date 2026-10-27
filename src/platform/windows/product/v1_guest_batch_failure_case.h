#pragma once

// Guest-only I10 continuation. Include after v1_guest_constraint_cases.h.
// No production API outcome is overridden. The exact owned test application's
// second actually received sizing operation destroys its own HWND, on its UI
// thread; the real outer SetWindowPos and postverification classify the result.
void batch_loss_abort_cleanup_up(LayoutFixtures& set,const Gesture& g){
    auto& s=set.state;namespace e=product::guest::up_evidence_detail;
    const HWND source=set.frames[0].hwnd;std::string ended,gone;
    const auto receipt=[&](std::string_view row,std::string_view type){
        return !row.empty()&&row.size()<=16384&&row.front()=='{'&&row.back()=='}'&&row.find_first_of("\r\n")==row.npos&&
            e::quoted(row,"\"schema\":","panebind-v1/1")&&e::quoted(row,"\"type\":",type)&&
            e::quoted(row,"\"source_sha\":",s.product_source)&&e::number(row,"\"process_id\":")==s.product_pid&&
            e::number(row,"\"generation\":")==g.generation;
    };
    const bool resources_gone=wait([&]{
        ended=record(s,g.offset,"gesture_end",g.generation);gone.clear();
        std::istringstream rows(product_log_since(s,g.offset));std::string row;
        while(std::getline(rows,row))if(receipt(row,"gesture_input_resource")&&e::quoted(row,"\"fact\":","isolation_gone"))gone=row;
        return g.generation&&receipt(ended,"gesture_end")&&e::unique_value(ended,"\"normal_up\":")=="false"&&
            e::unique_value(ended,"\"pending_retired\":")=="true"&&e::number(ended,"\"end\":")&&e::number(ended,"\"end\":")!=0&&
            receipt(gone,"gesture_input_resource")&&e::number(gone,"\"source\":")==reinterpret_cast<std::uintptr_t>(source)&&
            e::number(gone,"\"source_pid\":")==GetCurrentProcessId()&&e::number(gone,"\"source_tid\":")==set.thread&&
            e::number(gone,"\"overlay\":")&&e::number(gone,"\"overlay\":")!=0&&
            e::unique_value(gone,"\"release_attempted\":")=="true"&&e::unique_value(gone,"\"release_completed\":")=="true"&&
            e::unique_value(gone,"\"release_succeeded\":")=="true"&&e::number(gone,"\"capture_after_release\":")==0&&
            e::unique_value(gone,"\"detach_attempted\":")=="true"&&e::unique_value(gone,"\"detach_completed\":")=="true"&&
            e::unique_value(gone,"\"detach_succeeded\":")=="true"&&e::unique_value(gone,"\"overlay_destroyed\":")=="true"&&
            e::unique_value(gone,"\"hotkey_unregistered\":")=="true";
    },1500);
    RECT positioning{},client{};const auto original=visible(source);POINT point{};bool move_called=false,ready=false;
    HWND hit{},root{};DWORD hit_pid{},hit_tid{};std::string why;
    const auto exact_client=[&]{
        DWORD pid{};GUITHREADINFO gui{sizeof(gui)};RECT now{},current_client{};
        hit=WindowFromPoint(point);root=hit?GetAncestor(hit,GA_ROOT):nullptr;hit_pid=0;hit_tid=hit?GetWindowThreadProcessId(hit,&hit_pid):0;
        POINT mapped{static_cast<LONG>((static_cast<long long>(client.left)+client.right)/2),
            static_cast<LONG>((static_cast<long long>(client.top)+client.bottom)/2)};
        return desktop()&&s.process&&GetProcessId(s.process)==s.product_pid&&WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&
            process_birth(s.process)&&set.owns(0)&&set.frames[0].hwnd==source&&GetWindowThreadProcessId(source,&pid)==set.thread&&pid==GetCurrentProcessId()&&
            s.left&&!s.left_up_attempted&&s.control&&GetForegroundWindow()==source&&IsWindowVisible(source)&&!IsIconic(source)&&
            original&&visible(source)==original&&GetWindowRect(source,&now)&&EqualRect(&now,&positioning)&&
            GetClientRect(source,&current_client)&&EqualRect(&current_client,&client)&&ClientToScreen(source,&mapped)&&mapped.x==point.x&&mapped.y==point.y&&
            hit==source&&root==source&&hit_pid==pid&&hit_tid==set.thread&&GetGUIThreadInfo(set.thread,&gui)&&
            !gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner&&
            !(gui.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE));
    };
    try{
        if(!resources_gone||!original||!GetWindowRect(source,&positioning)||!GetClientRect(source,&client)||
           static_cast<long long>(client.right)-client.left<3||static_cast<long long>(client.bottom)-client.top<3)
            throw std::runtime_error("actual_abort_resources_or_exact_client_unavailable");
        point={static_cast<LONG>((static_cast<long long>(client.left)+client.right)/2),static_cast<LONG>((static_cast<long long>(client.top)+client.bottom)/2)};
        if(!ClientToScreen(source,&point)||point.x<=original->l||point.y<=original->t||point.x>=original->r-1||point.y>=original->b-1||!exact_client())
            throw std::runtime_error("exact_owned_idle_foreground_client_required");
        // Reuse the existing owned-client preparation and input helper. This
        // is one TEST MOVE, never placement, activation, capture or a new UP.
        move_called=true;cursor(s,point);POINT actual{};
        if(!GetCursorPos(&actual)||actual.x!=point.x||actual.y!=point.y||!exact_client())
            throw std::runtime_error("actual_cursor_or_client_changed_after_cleanup_MOVE");
        ready=true;why="exact_owned_idle_client_observed_before_unique_abort_UP";
    }catch(const std::exception& error){why=error.what();}catch(...){why="abort_UP_preparation_unknown";}
    s.log.emit("I10_batch_single_UP_client_preparation","\"generation\":"+std::to_string(g.generation)+
        ",\"resources_gone_observed\":"+(resources_gone?"true":"false")+",\"preparation_ready\":"+(ready?"true":"false")+
        ",\"reason\":"+quote(why)+",\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+
        ",\"client_screen\":["+std::to_string(point.x)+","+std::to_string(point.y)+"],\"actual_hit\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+
        ",\"actual_root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(root))+",\"hit_PID\":"+std::to_string(hit_pid)+",\"hit_TID\":"+std::to_string(hit_tid)+
        ",\"cleanup_MOVE_called_once\":"+(move_called?"true":"false")+",\"UP_not_yet_attempted\":true,\"delivery_or_NormalUp_inferred\":false");
    POINT fresh{};
    // Logging is not a lease: recheck the actual destination immediately
    // before the original single UP. Rejection leaves its ledger pending.
    if(!ready||!GetCursorPos(&fresh)||fresh.x!=point.x||fresh.y!=point.y||!exact_client())
        throw std::runtime_error("I10_abort_unique_UP_destination_not_confirmed");
    button(s,false);
}
void batch_loss_release(LayoutFixtures& set,const Gesture& g,const std::vector<std::size_t>& live,const std::vector<Rect>& expected){
    auto& s=set.state;const auto mark=read_product(s).size();button(s,false);if(s.control)key(s,VK_CONTROL,false);
    s.log.check("I10_recovery_actual_matching_UP",wait([&]{return !actual_up(s,mark,g.generation).empty()&&record(s,mark,"gesture_end",g.generation).find("\"normal_up\":true")!=std::string::npos;}));
    GUITHREADINFO gui{sizeof(gui)};bool exact=!s.left&&GetGUIThreadInfo(set.thread,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize;
    for(std::size_t n=0;n<live.size();++n)exact=exact&&set.owns(live[n])&&visible(set.frames[live[n]].hwnd)==expected[n];
    s.log.check("I10_recovery_UP_actual_survivors",exact,"\"generation\":"+std::to_string(g.generation));
}
void batch_loss_move(LayoutFixtures& set,const std::vector<std::size_t>& live,std::size_t source,const std::vector<Rect>& expected,bool ctrl,std::uint64_t retired,const char* name){
    auto& s=set.state;const auto before=visible(set.frames[source].hwnd);if(!before||expected.size()!=live.size())throw std::runtime_error("I10_recovery_actual_scope_unknown");
    const auto selected=std::find(live.begin(),live.end(),source);if(selected==live.end())throw std::runtime_error("I10_recovery_source_not_survivor");
    const auto target=expected[static_cast<std::size_t>(selected-live.begin())];const LONG dx=target.l-before->l,dy=target.t-before->t;
    const auto g=layout_down(set,source,HTCAPTION,ctrl);s.log.check("I10_recovery_fresh_generation",g.generation>retired);
    Sleep(60);const auto mark=read_product(s).size();cursor(s,{g.anchor.x+dx,g.anchor.y+dy});
    const bool exact=wait([&]{for(std::size_t n=0;n<live.size();++n){if(!set.owns(live[n])||visible(set.frames[live[n]].hwnd)!=expected[n])return false;
        if((ctrl||live[n]==source)&&placement(s,mark,g.generation,expected[n],true).empty())return false;}return true;});
    s.log.check(name,exact,"\"generation\":"+std::to_string(g.generation)+",\"same_surviving_HWNDs\":true,\"fresh_actual_snapshots\":true");
    batch_loss_release(set,g,live,expected);
}
[[maybe_unused]] void owned_second_member_native_failure_case(State& s){
    const auto config=settings_guest_configuration(s);
    if(!config.enabled||!config.glue||config.shake||config.drag_transparency||s.left||s.control)
        throw std::runtime_error("I10_batch_failure_requires_idle_Glue_without_optional_visual_mutations");
    LayoutFixtures set{s};set.create();
    const std::vector<Rect> initial{{100,100,500,500},{500,100,900,300},{500,300,900,500}};
    const std::vector<Rect> resized{{100,100,560,500},{560,100,900,300},{560,300,900,500}};
    set.setup(initial,{{250,100,1000,1000},{250,100,1000,1000},{250,100,1000,1000}});
    const auto loss=std::make_shared<NativeBatchLoss>();loss->owner=set.thread;
    for(std::size_t n=0;n<3;++n){if(!set.owns(n))throw std::runtime_error("I10_batch_exact_identity_missing");
        loss->exact[n]=set.frames[n].hwnd;loss->userdata[n]=reinterpret_cast<LONG_PTR>(&set.frames[n]);}
    const auto disarm=[&]{for(std::size_t n=0;n<3;++n)set.frames[n].native_batch_loss.store({});};
    bool guarded_cleanup_started=false;
    try{
        const auto g=layout_down(set,0,HTRIGHT,true);
        // Arming AFTER actual START excludes setup, hover and min/max queries.
        for(std::size_t n=0;n<3;++n)set.frames[n].native_batch_loss.store(loss);
        Sleep(60);cursor(s,{g.anchor.x+60,g.anchor.y});
        const bool destruction=wait([&]{return loss->completed.load(std::memory_order_acquire);},1500);
        std::size_t first=3,second=3;for(std::size_t n=0;n<3;++n){if(loss->member[n].order==1)first=n;if(loss->member[n].order==2)second=n;}
        s.log.check("I10_second_actual_sizing_member_destroyed",destruction&&loss->destroy_called&&loss->destroy_return&&loss->destroy_gone&&s.left&&actual_up(s,g.offset,g.generation).empty()&&
            loss->arrivals==2&&first<3&&second<3&&first!=second&&!IsWindow(loss->exact[second]),
            "\"first_member\":"+std::to_string(first)+",\"second_member\":"+std::to_string(second)+
            ",\"DestroyWindow_return\":"+(loss->destroy_return?"true":"false")+",\"DestroyWindow_error\":"+std::to_string(loss->destroy_error)+
            ",\"destroy_qpc\":"+std::to_string(loss->destroy_qpc)+",\"destroy_on_actual_owner_thread\":true,\"driver_left_outstanding\":"+(s.left?"true":"false")+
            ",\"async_left_at_destroy\":"+std::to_string(loss->async_left_state.load())+",\"physical_button_held_not_inferred\":true,\"outer_SetWindowPos_result_not_inferred\":true");
        for(std::size_t n=0;n<3;++n)if(loss->member[n].order){const auto& actual=loss->member[n];
            s.log.emit("I10_owned_actual_sizing_callback","\"member\":"+std::to_string(n)+",\"hwnd\":"+
                std::to_string(reinterpret_cast<std::uintptr_t>(loss->exact[n]))+",\"actual_order\":"+std::to_string(actual.order)+
                ",\"WINDOWPOS_flags\":"+std::to_string(actual.flags)+",\"positioning_request\":["+std::to_string(actual.x)+","+
                std::to_string(actual.y)+","+std::to_string(actual.width)+","+std::to_string(actual.height)+"],\"outer_API_result_not_inferred\":true");}
        std::string first_receipt,second_receipt,ended;
        const auto calls=[&]{auto rows=constraint_generation_placements(s,g.offset,g.generation);rows.erase(std::remove_if(rows.begin(),rows.end(),[](const auto& row){
            return row.find("\"native_attempted\":true")==std::string::npos;}),rows.end());return rows;};
        const bool observed=wait([&]{const auto rows=calls();if(rows.size()!=2)return false;first_receipt=rows[0];second_receipt=rows[1];ended=record(s,g.offset,"gesture_end",g.generation);
            return first_receipt.find("\"exact\":true")!=std::string::npos&&first_receipt.find("\"identity_valid\":true")!=std::string::npos&&
                first_receipt.find("\"target\":"+rect(resized[first]))!=std::string::npos&&first_receipt.find("\"actual\":"+rect(resized[first]))!=std::string::npos&&
                second_receipt.find("\"native_completion_known\":true")!=std::string::npos&&second_receipt.find("\"exact\":false")!=std::string::npos&&
                second_receipt.find("\"identity_valid\":false")!=std::string::npos&&second_receipt.find("\"target\":"+rect(resized[second]))!=std::string::npos&&
                second_receipt.find("\"actual\":null")!=std::string::npos&&number(first_receipt,"\"operation\":")==number(second_receipt,"\"operation\":")&&
                number(first_receipt,"\"native_after_qpc\":")&&number(first_receipt,"\"native_after_qpc\":")<=number(second_receipt,"\"native_before_qpc\":")&&
                ended.find("\"normal_up\":false")!=std::string::npos&&ended.find("\"pending_retired\":true")!=std::string::npos;});
        s.log.check("I10_first_exact_second_real_native_loss_no_third_attempt",observed&&visible(loss->exact[first])==resized[first]&&loss->arrivals==2,
            "\"first_receipt\":"+quote(first_receipt)+",\"second_receipt\":"+quote(second_receipt)+",\"actual_end\":"+quote(ended));
        const bool api_false=second_receipt.find("\"native_succeeded\":false")!=std::string::npos;
        s.log.emit("I10_second_native_outcome","\"status\":"+quote(api_false?"ACTUAL_API_FALSE_OBSERVED":"PARTIAL_API_TRUE_IDENTITY_LOST")+
            ",\"SetWindowPos_FALSE_observed\":"+(api_false?"true":"false")+",\"whole_I10_PASS\":false,\"actual_receipt\":"+quote(second_receipt));
        disarm();const auto before=calls().size();const auto barrier=constraint_last_product_sequence(s);
        const auto status_offset=read_product(s).size();
        s.log.check("I10_failure_status_responsive",launch(s,L"--status",true)&&wait([&]{return constraint_current_status_after(s,status_offset,barrier);}));
        guarded_cleanup_started=true;batch_loss_abort_cleanup_up(set,g);key(s,VK_CONTROL,false);
        // The already-aborted shield pair may be fully retired before this
        // one cleanup UP. Its actual LL callback then has no normalized UP to
        // enqueue. Require the existing exact after-send public receipt and
        // non-Normal resource cleanup, never manufacture a new normal event.
        const auto cleanup_up=abort_cleanup_up_receipt(s,g.offset,g.generation);
        GUITHREADINFO gui{sizeof(gui)};
        s.log.check("I10_failure_real_UP_without_new_batch_attempt",cleanup_up.matched&&
            wait([&]{return !s.left&&GetGUIThreadInfo(set.thread,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize;})&&
            calls().size()==before&&before==2&&loss->arrivals==2&&record(s,g.offset,"gesture_end",g.generation).find("\"normal_up\":false")!=std::string::npos,
            "\"third_member_native_attempts\":0,\"no_compensating_window_movement\":true,\"actual_UP\":"+quote(actual_up(s,g.offset,g.generation))+
            ",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+","+cleanup_up.json());
        guarded_cleanup_started=false;
        std::vector<std::size_t> live;std::vector<Rect> actual;
        for(std::size_t n=0;n<3;++n)if(n!=second){const auto frame=visible(set.frames[n].hwnd);if(!set.owns(n)||!frame)throw std::runtime_error("I10_survivor_actual_snapshot_missing");live.push_back(n);actual.push_back(*frame);}
        const std::size_t source=live[0];const Rect neighbour=actual[1];const Rect from=actual[0];
        // Preserve the actually partial layout first. Form a legal relation
        // with real subsequent Move gestures, never fixture repositioning.
        actual[0]={neighbour.r+80,neighbour.t+40,neighbour.r+80+from.width(),neighbour.t+40+from.height()};
        batch_loss_move(set,live,source,actual,false,g.generation,"I10_partial_state_fresh_actual_ordinary_Move");
        actual[0]={neighbour.r,neighbour.t,neighbour.r+from.width(),neighbour.t+from.height()};
        batch_loss_move(set,live,source,actual,false,g.generation,"I10_real_Move_forms_new_survivor_contact");
        for(auto& frame:actual)frame=translated(frame,33,27);
        batch_loss_move(set,live,source,actual,true,g.generation,"I10_partial_failure_fresh_Ctrl_two_survivors");
        s.log.check("I10_batch_failure_owned_resources_destroyed",set.close(),"\"guest_destruction_is_not_cleanup\":true");
    }catch(...){disarm();try{if(s.left&&!guarded_cleanup_started)button(s,false);if(s.control)key(s,VK_CONTROL,false);}catch(...){}throw;}
}
