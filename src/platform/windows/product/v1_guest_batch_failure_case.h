#pragma once

// Guest-only I10 continuation. Include after v1_guest_constraint_cases.h.
// No production API outcome is overridden. The exact owned test application's
// second actually received sizing operation destroys its own HWND, on its UI
// thread; the real outer SetWindowPos and postverification classify the result.
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
        disarm();const auto before=calls().size();const auto barrier=constraint_last_product_sequence(s);cursor(s,{g.anchor.x+91,g.anchor.y+23});
        const auto status_offset=read_product(s).size();
        s.log.check("I10_failure_status_responsive",launch(s,L"--status",true)&&wait([&]{return constraint_current_status_after(s,status_offset,barrier);}));
        button(s,false);key(s,VK_CONTROL,false);
        GUITHREADINFO gui{sizeof(gui)};
        s.log.check("I10_failure_real_UP_without_new_batch_attempt",wait([&]{return !s.left&&!actual_up(s,g.offset,g.generation).empty()&&GetGUIThreadInfo(set.thread,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize;})&&
            calls().size()==before&&before==2&&loss->arrivals==2&&record(s,g.offset,"gesture_end",g.generation).find("\"normal_up\":false")!=std::string::npos,
            "\"third_member_native_attempts\":0,\"no_compensating_window_movement\":true,\"actual_UP\":"+quote(actual_up(s,g.offset,g.generation))+
            ",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON)));
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
    }catch(...){disarm();try{if(s.left)button(s,false);if(s.control)key(s,VK_CONTROL,false);}catch(...){}throw;}
}
