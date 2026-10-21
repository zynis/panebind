#pragma once

// TEST ONLY. Include after the existing layout, settings, product-chain and
// geometry-history helpers. This fills the one normal in-flight modifier-change
// gap; it is not another group/stability matrix or a product input entry.
[[maybe_unused]] void owned_modifier_lock_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if(s.suite!="ModifierLockOnly"||!original.enabled||!original.rules.empty()||s.left||s.control||
       (GetAsyncKeyState(VK_SHIFT)&0x8000)||binary.empty()||binary!=hash(s.input_product))
        throw std::runtime_error("modifier_lock_exact_idle_owned_guest_required");
    auto config=original;config.glue=true;config.magnet_move=false;config.magnet_resize=false;
    config.docking=false;config.preview=false;config.feedback=false;config.drag_transparency=false;config.shake=false;
    const auto applied=chain_gui_save(s,config);LayoutFixtures set{s};
    bool shift_down_attempted=false,shift_up_attempted=false;
    try{
        set.create();const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<1300||work.height()<800)throw std::runtime_error("modifier_lock_actual_work_area_too_small");
        const std::vector<Rect> initial{{work.l+220,work.t+180,work.l+700,work.t+500},
            {work.l+700,work.t+180,work.l+1100,work.t+500}};
        const LayoutLimit limit{120,100,work.width(),work.height()};set.setup(initial,{limit,limit});
        const auto grouped=layout_down(set,0,HTCAPTION,true);
        const auto start=record(s,grouped.offset,"gesture_start",grouped.generation);
        const auto pending=record(s,grouped.offset,"native_move_pending",grouped.generation);
        const auto source_key=e::number(start,"\"key\":");
        s.log.check("modifier_lock_actual_Ctrl_DOWN_freezes_two_members",
            geometry_history_receipt(s,start,"gesture_start")&&source_key&&*source_key&&
            e::number(start,"\"generation\":")==grouped.generation&&e::unique_value(start,"\"ctrl\":")=="true"&&
            e::unique_value(start,"\"resize\":")=="false"&&e::number(start,"\"group_members\":")==2&&
            e::unique_value(start,"\"original_DOWN_group_frozen\":")=="true"&&
            geometry_history_receipt(s,pending,"native_move_pending")&&e::number(pending,"\"locked_modifiers\":")==2&&
            e::number(pending,"\"source\":")==reinterpret_cast<std::uintptr_t>(set.frames[0].hwnd),
            "\"generation\":"+std::to_string(grouped.generation)+",\"actual_start\":"+quote(start)+
            ",\"actual_DOWN_pending\":"+quote(pending));
        const std::vector<Rect> first{translated(initial[0],25,15),translated(initial[1],25,15)};
        layout_phase(set,0,grouped,25,15,first,"modifier_lock_before_change_actual_group_Move");
        if(!s.left||!s.control||actual_up(s,grouped.offset,grouped.generation).size()||
           GetForegroundWindow()!=set.frames[0].hwnd||!(GetAsyncKeyState(VK_LBUTTON)&0x8000)||
           !(GetAsyncKeyState(VK_CONTROL)&0x8000))
            throw std::runtime_error("modifier_lock_actual_held_Ctrl_source_required_before_change");
        const auto changed_mark=read_product(s).size();
        key(s,VK_CONTROL,false);shift_down_attempted=true;key(s,VK_SHIFT,true);
        const SHORT left=GetAsyncKeyState(VK_LBUTTON),control=GetAsyncKeyState(VK_CONTROL),shift=GetAsyncKeyState(VK_SHIFT);
        s.log.check("modifier_lock_actual_Ctrl_UP_Shift_DOWN_while_mouse_held",
            s.left&&!s.control&&(left&0x8000)&&!(control&0x8000)&&(shift&0x8000)&&
            set.owns(0)&&set.owns(1)&&GetForegroundWindow()==set.frames[0].hwnd&&
            actual_up(s,grouped.offset,grouped.generation).empty(),
            "\"generation\":"+std::to_string(grouped.generation)+",\"actual_left_SHORT\":"+std::to_string(left)+
            ",\"actual_Ctrl_SHORT\":"+std::to_string(control)+",\"actual_Shift_SHORT\":"+std::to_string(shift)+
            ",\"test_keys_not_product_state_mutation\":true,\"mouse_UP_not_sent\":true");
        const std::vector<Rect> second{translated(initial[0],55,35),translated(initial[1],55,35)};
        layout_phase(set,0,grouped,55,35,second,"modifier_lock_changed_live_keys_keep_original_group_route");
        unsigned new_starts{},placements{},wrong_generation{},exact_calls{};
        {std::istringstream rows(product_log_since(s,changed_mark));std::string row;
        while(std::getline(rows,row)){
            if(geometry_history_receipt(s,row,"gesture_start"))++new_starts;
            if(!geometry_history_receipt(s,row,"placement"))continue;++placements;
            if(e::number(row,"\"generation\":")!=grouped.generation)++wrong_generation;
            if(e::unique_value(row,"\"native_attempted\":")=="true"&&e::unique_value(row,"\"exact\":")=="true")++exact_calls;
        }}
        s.log.check("modifier_lock_original_generation_continues_without_reroute",
            !new_starts&&!wrong_generation&&placements==2&&exact_calls==2&&
            record(s,grouped.offset,"gesture_end",grouped.generation).empty(),
            "\"generation\":"+std::to_string(grouped.generation)+",\"new_STARTs\":"+std::to_string(new_starts)+
            ",\"placements_after_live_key_change\":"+std::to_string(placements)+
            ",\"exact_native_calls\":"+std::to_string(exact_calls)+",\"wrong_generation\":"+std::to_string(wrong_generation));
        layout_up(set,grouped,second); // Actual NormalUp with Shift still down.
        shift_up_attempted=true;key(s,VK_SHIFT,false);
        s.log.check("modifier_lock_first_pair_and_owned_keys_released",!s.left&&!s.control&&
            !(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&!(GetAsyncKeyState(VK_CONTROL)&0x8000)&&!(GetAsyncKeyState(VK_SHIFT)&0x8000));

        // The next plain DOWN is on the former FOLLOWER. Its new source and
        // single-member route must not inherit g1's Ctrl or frozen membership.
        const auto plain=layout_down(set,1,HTCAPTION,false);
        const auto plain_start=record(s,plain.offset,"gesture_start",plain.generation);
        const auto plain_key=e::number(plain_start,"\"key\":");
        s.log.check("modifier_lock_next_plain_DOWN_reselects_former_follower",
            plain.generation>grouped.generation&&geometry_history_receipt(s,plain_start,"gesture_start")&&
            plain_key&&*plain_key&&plain_key!=source_key&&e::unique_value(plain_start,"\"ctrl\":")=="false"&&
            e::number(plain_start,"\"group_members\":")==1&&e::unique_value(plain_start,"\"resize\":")=="false",
            "\"previous_generation\":"+std::to_string(grouped.generation)+",\"actual_start\":"+quote(plain_start));
        const std::vector<Rect> single{second[0],translated(second[1],35,30)};
        layout_phase(set,1,plain,35,30,single,"modifier_lock_next_plain_Move_does_not_move_old_group");
        unsigned plain_calls{},foreign_calls{};
        {std::istringstream rows(product_log_since(s,plain.offset));std::string row;
        while(std::getline(rows,row))if(geometry_history_receipt(s,row,"placement")&&
            e::unique_value(row,"\"native_attempted\":")=="true"){
            ++plain_calls;if(e::number(row,"\"generation\":")!=plain.generation||e::number(row,"\"key\":")!=plain_key)++foreign_calls;
        }}
        s.log.check("modifier_lock_next_plain_has_only_fresh_source_native_call",plain_calls==1&&!foreign_calls&&
            visible(set.frames[0].hwnd)==single[0],"\"native_calls\":"+std::to_string(plain_calls)+
            ",\"foreign_or_old_source_calls\":"+std::to_string(foreign_calls));
        layout_up(set,plain,single);

        // A newly held Ctrl+Shift gets the public Shift-bypass priority, not
        // the old Ctrl route. The existing helper independently observes the
        // native START/END/source-queue UP and requires zero product writers.
        (void)pause_routing_native_drag(set,0,20,25,true,"modifier_lock_next_Ctrl_Shift_DOWN_is_native_bypass");
        const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        s.log.check("modifier_lock_original_configuration_and_binary_restored",restored==expected&&hash(s.product)==binary&&binary==hash(s.input_product));
        chain_check_product(s,pid,birth);s.log.check("modifier_lock_exact_owned_roots_closed",set.close());
        s.log.emit("modifier_lock_component","\"result\":\"PASS\",\"same_product_instance\":true,"
            "\"held_Ctrl_group_survives_Ctrl_release_and_Shift_press\":true,\"actual_NormalUp\":true,"
            "\"following_plain_source_reselected\":true,\"following_Ctrl_Shift_native_bypass\":true,"
            "\"whole_M04_or_version_PASS_inferred\":false,\"human_UAT\":\"NOT_RUN\"");
    }catch(...){
        pause_routing_failed_case_input(s);
        if(shift_down_attempted&&!shift_up_attempted){shift_up_attempted=true;try{key(s,VK_SHIFT,false);}catch(...) {}}
        try{if(s.process&&!s.left&&!s.control&&!(GetAsyncKeyState(VK_SHIFT)&0x8000)&&chain_settings(s)==applied)
            (void)chain_gui_save(s,original);}catch(...){}
        try{if(!set.closed)set.close();}catch(...){}throw;
    }
}
