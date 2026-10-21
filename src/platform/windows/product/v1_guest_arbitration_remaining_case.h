#pragma once

// TEST ONLY. Include after the existing layout, product-chain, interruption,
// geometry-history and remaining-group helpers. Exactly three previously
// unobserved competing-mode cases, not another modifier/pause/settings matrix.
void owned_arbitration_remaining_case(State& s){
    namespace v=panebind::core::v1;namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;
    const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if(s.suite!="ArbitrationRemainderOnly"||!original.enabled||!original.rules.empty()||s.left||s.control||
       binary.empty()||binary!=hash(s.input_product))throw std::runtime_error("arbitration_remaining_exact_guest_required");
    for(const int code:std::array<int,7>{VK_LBUTTON,VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN,'C'})
        if(GetAsyncKeyState(code)&0x8000)throw std::runtime_error("arbitration_remaining_initial_keys_not_released");
    auto config=original;config.glue=true;config.magnet_move=true;config.magnet_resize=false;config.docking=true;
    config.smart_docking=false;config.preview=true;config.feedback=false;config.stretch=false;config.smart_stretch=false;
    config.drag_transparency=false;config.shake=false;config.attraction_dip=10;config.release_dip=16;config.speed_dip=20000;
    auto applied=chain_gui_save(s,config);LayoutFixtures set{s};set.create();
    const auto rows_for=[&](std::size_t offset,std::string_view type,std::uint64_t generation){
        std::vector<std::string> found;std::string row;std::istringstream rows(product_log_since(s,offset));
        while(std::getline(rows,row))if(remaining_group_receipt(s,row,type)&&e::number(row,"\"generation\":")==generation)found.push_back(row);
        return found;
    };
    const auto no_discrete=[&](const Gesture& g){
        if(!record(s,g.offset,"normal_up_maximize",g.generation).empty())return false;
        for(const auto& row:rows_for(g.offset,"placement",g.generation))
            if(e::quoted(row,"\"intent_reason\":","normal_up_dock"))return false;
        return true;
    };
    const auto exact_native_count=[&](const Gesture& g,std::size_t count){
        unsigned native{};bool valid=true;std::uint64_t operation{};
        for(const auto& row:rows_for(g.offset,"placement",g.generation))if(e::unique_value(row,"\"native_callsite_claimed\":")=="true"){
            ++native;const auto actual_operation=e::number(row,"\"operation\":");
            valid=valid&&actual_operation&&*actual_operation&&(!operation||operation==*actual_operation)&&
                e::unique_value(row,"\"native_attempted\":")=="true"&&e::unique_value(row,"\"native_succeeded\":")=="true"&&
                e::unique_value(row,"\"native_completion_known\":")=="true"&&e::unique_value(row,"\"identity_valid\":")=="true"&&
                e::unique_value(row,"\"exact\":")=="true";operation=actual_operation.value_or(0);
        }
        s.log.check("arbitration_remaining_one_native_per_participant_same_quantum",valid&&native==count,
            std::string("\"generation\":")+std::to_string(g.generation)+",\"actual_native_attempts\":"+std::to_string(native)+
            ",\"expected_participants\":"+std::to_string(count)+",\"actual_operation\":"+std::to_string(operation));
    };
    const auto actual_gone=[&](const Gesture& g){
        std::string gone;for(const auto& row:rows_for(g.offset,"gesture_input_resource",g.generation))
            if(e::quoted(row,"\"fact\":","isolation_gone"))gone=row;
        return gone;
    };
    const auto gone_valid=[&](const std::string& row){
        return remaining_group_receipt(s,row,"gesture_input_resource")&&e::quoted(row,"\"fact\":","isolation_gone")&&
            e::unique_value(row,"\"release_succeeded\":")=="true"&&e::unique_value(row,"\"detach_succeeded\":")=="true"&&
            e::unique_value(row,"\"overlay_destroyed\":")=="true"&&e::unique_value(row,"\"hotkey_unregistered\":")=="true";
    };
    try{
        const auto work=chain_work(set.frames[0].hwnd);const auto dpi=GetDpiForWindow(set.frames[0].hwnd);
        if(work.width()<1200||work.height()<900||!dpi||dpi>384)throw std::runtime_error("arbitration_remaining_actual_work_DPI_unavailable");
        const Rect a{work.l+240,work.t+200,work.l+640,work.t+500},b{work.l+640,work.t+200,work.l+940,work.t+500};
        const LayoutLimit limit{120,100,work.width(),work.height()};const std::vector<Rect> pair{a,b};
        set.setup(pair,{limit,limit});(void)resize_magnet_native_baseline(set,pair);chain_front(set,0);
        const auto caption=layout_hit(set,0,HTCAPTION);if(!caption)throw std::runtime_error("arbitration_remaining_actual_caption_missing");
        const LONG caption_gap=caption->x-a.l-1;
        // Raise a LEGAL public setting only before DOWN if this actual native
        // caption offset exceeds the current DPI-converted attraction range.
        const auto pixels=[&](unsigned dip){return static_cast<LONG>((static_cast<std::uint64_t>(dip)*dpi+48)/96);};
        if(caption_gap<=0)throw std::runtime_error("arbitration_remaining_caption_gap_not_distinguishable");
        if(caption_gap>pixels(applied.attraction_dip)){
            const auto needed=static_cast<int>((static_cast<std::uint64_t>(caption_gap+1)*96+dpi-1)/dpi);
            config.attraction_dip=std::max(config.attraction_dip,needed);config.release_dip=std::max(config.release_dip,config.attraction_dip+6);
            if(!v::validate(config).empty())throw std::runtime_error("arbitration_remaining_caption_requires_illegal_attraction");
            applied=chain_gui_save(s,config);
        }
        const LONG entry=pixels(applied.attraction_dip),release=pixels(applied.release_dip);
        if(entry<=0||release<=entry||chain_settings(s)!=applied)throw std::runtime_error("arbitration_remaining_actual_configuration_unknown");
        const POINT edge{work.l+1,work.t+work.height()/2};
        const auto edge_expected=[&](const Gesture& g){
            const LONG gap=g.anchor.x-a.l-1,top=edge.y-(g.anchor.y-a.t);
            const Rect raw=translated(a,edge.x-g.anchor.x,edge.y-g.anchor.y);
            const Rect expected{work.l,top,work.l+a.width(),top+a.height()};
            s.log.check("arbitration_remaining_independent_work_edge_magnet_oracle",gap>0&&gap<=entry&&raw.l==work.l-gap&&
                raw!=expected&&top-work.t>release&&work.b-expected.b>release&&GetDpiForWindow(set.frames[0].hwnd)==dpi,
                std::string("\"generation\":")+std::to_string(g.generation)+",\"actual_DPI\":"+std::to_string(dpi)+
                ",\"actual_attraction_DIP\":"+std::to_string(applied.attraction_dip)+",\"entry_pixels\":"+std::to_string(entry)+
                ",\"actual_original_DOWN\":["+std::to_string(g.anchor.x)+","+std::to_string(g.anchor.y)+"],\"cursor_target\":["+
                std::to_string(edge.x)+","+std::to_string(edge.y)+"],\"raw_source\":"+rect(raw)+",\"independent_magnet_source\":"+rect(expected)+
                ",\"raw_gap_pixels\":"+std::to_string(gap)+",\"oracle_uses_product_solver\":false");
            return expected;
        };
        // 1. Ctrl group magnet is active at a REAL docking cursor zone. Ctrl
        // wins through the real UP; no source-only or release-dock write.
        auto g=layout_down(set,0,HTCAPTION,true);const auto first_generation=g.generation;
        const auto source_key=remaining_group_start(s,g,2,"arbitration_remaining_actual_Ctrl_two_member_route");
        const Rect left=edge_expected(g);
        const std::vector<Rect> expected_group{left,{left.r,left.t,left.r+b.width(),left.b}};
        layout_phase(set,0,g,edge.x-g.anchor.x,edge.y-g.anchor.y,expected_group,"arbitration_remaining_Ctrl_magnet_in_dock_zone");
        const auto follower_key=remaining_group_actual_key(s,g,expected_group[1]);
        remaining_group_native_scope(s,g,{source_key,follower_key},"arbitration_remaining_Ctrl_only_group_native_scope");
        exact_native_count(g,2);chain_preview_hidden(s,"arbitration_remaining_Ctrl_in_zone_has_no_dock_preview");
        s.log.check("arbitration_remaining_Ctrl_held_no_discrete_dock",no_discrete(g)&&s.left&&!s.left_up_attempted&&
            actual_up(s,g.offset,g.generation).empty());
        const auto before_group_up=rows_for(g.offset,"placement",g.generation);layout_up(set,g,expected_group);
        chain_preview_hidden(s,"arbitration_remaining_Ctrl_UP_has_no_dock_preview");
        s.log.check("arbitration_remaining_Ctrl_NormalUp_no_dock_or_new_writer",no_discrete(g)&&
            rows_for(g.offset,"placement",g.generation)==before_group_up&&gone_valid(actual_gone(g)),
            std::string("\"generation\":")+std::to_string(g.generation)+",\"actual_gone\":"+quote(actual_gone(g)));
        chain_check_product(s,pid,birth);

        // 2. SAME enabled modes, plain source. A real screen-edge magnet
        // correction occurs while the dock preview is active. Continuous
        // source placement and the unique release-dock operation are separate.
        set.setup({a},{limit});(void)resize_magnet_native_baseline(set,{a});g=layout_down(set,0,HTCAPTION,false);
        s.log.check("arbitration_remaining_plain_after_Ctrl_fresh_generation",g.generation>first_generation);
        const Rect snapped=edge_expected(g),half{work.l,work.t,work.l+work.width()/2,work.b};
        layout_phase(set,0,g,edge.x-g.anchor.x,edge.y-g.anchor.y,{snapped},"arbitration_remaining_plain_magnet_and_dock_preview");
        exact_native_count(g,1);chain_preview_exact(s,set.frames[0].hwnd,half);
        s.log.check("arbitration_remaining_plain_preview_not_early_dock",no_discrete(g)&&s.left&&!s.left_up_attempted&&
            visible(set.frames[0].hwnd)==snapped&&actual_up(s,g.offset,g.generation).empty());
        layout_up(set,g,{half});chain_preview_hidden(s,"arbitration_remaining_plain_UP_hides_preview");
        unsigned commits{},calls{},continuous{};std::string commit;
        for(const auto& row:rows_for(g.offset,"placement",g.generation)){
            if(e::unique_value(row,"\"native_attempted\":")=="true")++calls;
            if(e::quoted(row,"\"intent_reason\":","normal_up_dock")){++commits;commit=row;}
            else if(e::unique_value(row,"\"native_attempted\":")=="true")++continuous;
        }
        s.log.check("arbitration_remaining_plain_unique_release_commit",commits==1&&continuous==1&&calls==2&&
            e::unique_value(commit,"\"native_attempted\":")=="true"&&e::unique_value(commit,"\"native_succeeded\":")=="true"&&
            e::unique_value(commit,"\"identity_valid\":")=="true"&&e::unique_value(commit,"\"exact\":")=="true"&&
            commit.find("\"target\":"+rect(half)+",")!=commit.npos&&commit.find("\"actual\":"+rect(half)+",")!=commit.npos&&
            gone_valid(actual_gone(g))&&!IsZoomed(set.frames[0].hwnd),
            std::string("\"generation\":")+std::to_string(g.generation)+",\"actual_continuous_native_calls\":"+std::to_string(continuous)+
            ",\"actual_release_dock_native_calls\":"+std::to_string(commits)+",\"actual_release_receipt\":"+quote(commit));
        chain_check_product(s,pid,birth);const auto plain_generation=g.generation;

        // 3. Existing public Ctrl+Alt+C mapping, delivered as real keys while
        // the original Ctrl gesture still holds LEFT. No IPC command or fake
        // WM_HOTKEY is sent and chain_hotkey_exact cannot be used while held.
        if(std::count_if(applied.hotkeys.begin(),applied.hotkeys.end(),[](const auto& h){return h.modifiers==3&&h.key=='C'&&h.action==v::Action::Center;})!=1)
            throw std::runtime_error("arbitration_remaining_public_CtrlAltC_Center_mapping_required");
        set.setup(pair,{limit,limit});(void)resize_magnet_native_baseline(set,pair);g=layout_down(set,0,HTCAPTION,true);
        const auto key_id=remaining_group_start(s,g,2,"arbitration_remaining_hotkey_actual_active_Ctrl_route");
        s.log.check("arbitration_remaining_hotkey_fresh_generation",g.generation>plain_generation);
        const std::vector<Rect> moved{translated(a,30,25),translated(b,30,25)};
        layout_phase(set,0,g,30,25,moved,"arbitration_remaining_actual_Ctrl_before_nonstop_hotkey");
        if(!set.owns(0)||!set.owns(1)||GetForegroundWindow()!=set.frames[0].hwnd||!s.left||s.left_up_attempted||!s.control||
           !(GetAsyncKeyState(VK_LBUTTON)&0x8000)||!(GetAsyncKeyState(VK_CONTROL)&0x8000)||
           (GetAsyncKeyState(VK_MENU)&0x8000)||(GetAsyncKeyState('C')&0x8000)||!actual_up(s,g.offset,g.generation).empty())
            throw std::runtime_error("arbitration_remaining_hotkey_real_held_context_changed");
        const LONG cx=work.l+(work.width()-a.width())/2,cy=work.t+(work.height()-a.height())/2;
        const Rect centered{cx,cy,cx+a.width(),cy+a.height()};const auto command_mark=read_product(s).size();
        LARGE_INTEGER trigger{};if(!QueryPerformanceCounter(&trigger)||trigger.QuadPart<=0)throw std::runtime_error("arbitration_remaining_hotkey_clock_unknown");
        bool alt_attempted=false,alt_up=false,c_attempted=false,c_up=false;
        try{alt_attempted=true;key(s,VK_MENU,true);c_attempted=true;key(s,'C',true);c_up=true;key(s,'C',false);alt_up=true;key(s,VK_MENU,false);}
        catch(...){try{if(c_attempted&&!c_up){c_up=true;key(s,'C',false);}}catch(...){}
            try{if(alt_attempted&&!alt_up){alt_up=true;key(s,VK_MENU,false);}}catch(...){}throw;}
        std::string ended,command,receipt;
        const auto actual_command=[&]{
            ended=record(s,g.offset,"gesture_end",g.generation);command=record(s,command_mark,"command");receipt.clear();
            for(const auto& row:rows_for(command_mark,"placement",0))if(e::quoted(row,"\"intent_reason\":","command")&&
                e::number(row,"\"key\":")==key_id)receipt=row;
            return set.owns(0)&&set.owns(1)&&visible(set.frames[0].hwnd)==centered&&visible(set.frames[1].hwnd)==moved[1]&&
                remaining_group_receipt(s,ended,"gesture_end")&&e::number(ended,"\"end\":")==static_cast<unsigned>(v::End::Paused)&&
                e::unique_value(ended,"\"normal_up\":")=="false"&&e::unique_value(ended,"\"pending_retired\":")=="true"&&
                remaining_group_receipt(s,command,"command")&&e::quoted(command,"\"action\":","Center")&&e::number(command,"\"key\":")==key_id&&
                !receipt.empty()&&e::unique_value(receipt,"\"native_attempted\":")=="true"&&e::unique_value(receipt,"\"native_succeeded\":")=="true"&&
                e::unique_value(receipt,"\"identity_valid\":")=="true"&&e::unique_value(receipt,"\"exact\":")=="true"&&
                receipt.find("\"target\":"+rect(centered)+",")!=receipt.npos&&receipt.find("\"actual\":"+rect(centered)+",")!=receipt.npos;
        };
        const bool command_observed=wait(actual_command);
        s.log.check("arbitration_remaining_actual_nonstop_hotkey_beats_active_Ctrl",command_observed,
            std::string("\"old_generation\":")+std::to_string(g.generation)+",\"actual_registered_mapping\":\"Ctrl+Alt+C -> Center\","+
            "\"actual_TEST_key_chord_helper_returned\":true,\"IPC_or_WM_HOTKEY_injection\":false,\"direct_WM_HOTKEY_receipt_not_claimed\":true,"+
            "\"chord_before_QPC\":"+std::to_string(trigger.QuadPart)+",\"expected_center\":"+rect(centered)+
            ",\"actual_old_end\":"+quote(ended)+",\"actual_command\":"+quote(command)+",\"actual_native_receipt\":"+quote(receipt));
        unsigned command_calls{},command_rows{},late{},unknown{},recovery_calls{};std::uint64_t recovery_generation{};
        const auto audit=[&]{
            command_calls=command_rows=late=unknown=recovery_calls=0;std::string row;std::istringstream rows(product_log_since(s,command_mark));
            while(std::getline(rows,row)){
                if(remaining_group_receipt(s,row,"command"))++command_rows;
                if(!remaining_group_receipt(s,row,"placement")||e::unique_value(row,"\"native_callsite_claimed\":")!="true")continue;
                const auto generation=e::number(row,"\"generation\":");
                if(generation==0){++command_calls;if(e::number(row,"\"key\":")!=key_id||!e::quoted(row,"\"intent_reason\":","command"))++unknown;}
                else if(generation==g.generation){const auto clock=e::number(row,"\"native_before_qpc\":");
                    if(!clock||!*clock)++unknown;else if(*clock>=static_cast<std::uint64_t>(trigger.QuadPart))++late;}
                else if(recovery_generation&&generation==recovery_generation){++recovery_calls;
                    if(e::number(row,"\"key\":")!=key_id||!e::quoted(row,"\"intent_reason\":","group_translation")||
                       e::unique_value(row,"\"identity_valid\":")!="true"||e::unique_value(row,"\"exact\":")!="true")++unknown;}
                else ++unknown;
            }
        };
        // A real terminal key command does not imply mouse UP. Observe the
        // one already-planned TEST UP at a passive guest hook as well. Keep
        // the product's stricter LL/status result literally separate: after
        // abort/Gone, idle Raw or a guest-hook UP never becomes old NormalUp.
        GuestUpHookObservation cleanup_head;
        s.log.check("arbitration_remaining_single_cleanup_UP_guest_observer_ready",cleanup_head.start(),
            "\"old_generation\":"+std::to_string(g.generation)+",\"product_authority_or_UP_receipt\":false");
        button(s,false);key(s,VK_CONTROL,false);const auto cleanup=abort_cleanup_up_receipt(s,g.offset,g.generation);
        const bool head_stopped=cleanup_head.stop();const auto head=cleanup_head.json();
        const bool guest_up=head_stopped&&s.left_up_attempted&&s.left_up_send_qpc&&
            e::unique_value(head,"\"snapshot_coherent\":")=="true"&&e::unique_value(head,"\"observation_final\":")=="true"&&
            e::unique_value(head,"\"installed\":")=="true"&&e::unique_value(head,"\"CallNext_returned\":")=="true"&&
            e::number(head,"\"actual_left_UP_callbacks\":")==1&&e::number(head,"\"last_qpc\":").value_or(0)>=s.left_up_send_qpc&&
            e::number(head,"\"last_extra_information\":")==0x50425631&&
            (e::number(head,"\"last_flags\":").value_or(0)&LLMHF_INJECTED);
        s.log.emit("arbitration_remaining_unique_abort_UP_observations",
            std::string("\"old_generation\":")+std::to_string(g.generation)+",\"single_planned_UP_send_before_qpc\":"+
            std::to_string(s.left_up_send_qpc)+",\"actual_guest_head\":"+head+
            ",\"single_tagged_guest_hook_UP_observed\":"+(guest_up?"true":"false")+
            ",\"observer_stop_confirmed\":"+(head_stopped?"true":"false")+
            ",\"product_LL_cleanup_UP_observed\":"+(cleanup.matched?"true":"false")+","+cleanup.json()+
            ",\"guest_hook_is_not_product_or_window_UP_receipt\":true,\"old_generation_NormalUp_inferred\":false,\"extra_cleanup_UP_sent\":false");
        const auto status=geometry_history_status(s);audit();GUITHREADINFO gui{sizeof(gui)};
        s.log.check("arbitration_remaining_single_command_retired_actual_guest_abort_UP",guest_up&&
            command_calls==1&&command_rows==1&&!late&&!unknown&&gone_valid(actual_gone(g))&&
            GetGUIThreadInfo(set.thread,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&
            set.owns(0)&&set.owns(1)&&visible(set.frames[0].hwnd)==centered&&visible(set.frames[1].hwnd)==moved[1]&&!s.left&&!s.control,
            std::string("\"old_generation\":")+std::to_string(g.generation)+",\"command_native_calls\":"+std::to_string(command_calls)+
            ",\"command_rows\":"+std::to_string(command_rows)+",\"new_old_generation_native_calls\":"+std::to_string(late)+
            ",\"unknown_calls\":"+std::to_string(unknown)+",\"cleanup_UP_is_NormalUp\":false,\"actual_status\":"+quote(status)+","+
            cleanup.json()+",\"actual_gone\":"+quote(actual_gone(g))+
            ",\"actual_guest_hook_UP_observed\":"+(guest_up?"true":"false")+",\"product_LL_observation_gap_not_relabelled\":true");
        // No reset, product restart, extra old UP or preparation-delay retry.
        // The normal subsequent operation must start from actual Center output
        // and prove new authority, actual NormalUp and ordinary client pairing.
        chain_check_product(s,pid,birth);const auto following=layout_down(set,0,HTCAPTION,false);
        recovery_generation=following.generation;const auto following_start=record(s,following.offset,"gesture_start",following.generation);
        s.log.check("arbitration_remaining_after_command_actual_fresh_plain_route",following.generation>g.generation&&
            remaining_group_receipt(s,following_start,"gesture_start")&&e::number(following_start,"\"key\":")==key_id&&
            e::unique_value(following_start,"\"ctrl\":")=="false"&&e::unique_value(following_start,"\"resize\":")=="false"&&
            e::number(following_start,"\"group_members\":")==1&&
            following_start.find("\"original\":"+rect(centered)+",")!=following_start.npos,
            "\"old_generation\":"+std::to_string(g.generation)+",\"actual_new_START\":"+quote(following_start)+
            ",\"product_restarted_or_layout_reset\":false");
        const std::vector<Rect> recovered{translated(centered,24,17),moved[1]};
        layout_phase(set,0,following,24,17,recovered,"arbitration_remaining_after_command_fresh_Move_exact");
        exact_native_count(following,1);layout_up(set,following,recovered);
        const auto following_end=record(s,following.offset,"gesture_end",following.generation);
        s.log.check("arbitration_remaining_following_actual_NormalUp_and_resources",remaining_group_receipt(s,following_end,"gesture_end")&&
            e::number(following_end,"\"end\":")==static_cast<unsigned>(v::End::NormalUp)&&
            e::unique_value(following_end,"\"normal_up\":")=="true"&&gone_valid(actual_gone(following))&&no_discrete(following),
            "\"actual_new_end\":"+quote(following_end)+",\"actual_new_gone\":"+quote(actual_gone(following))+
            ",\"old_aborted_generation_NormalUp_inferred\":false");
        pause_routing_client_pair(set,"arbitration_remaining_after_command_fresh_native_client_DOWN_UP");
        audit();s.log.check("arbitration_remaining_after_recovery_no_old_generation_or_extra_command",command_calls==1&&
            command_rows==1&&recovery_calls==1&&!late&&!unknown&&set.owns(0)&&set.owns(1)&&
            visible(set.frames[0].hwnd)==recovered[0]&&visible(set.frames[1].hwnd)==recovered[1],
            "\"old_generation\":"+std::to_string(g.generation)+",\"recovery_generation\":"+std::to_string(recovery_generation)+
            ",\"command_native_calls\":"+std::to_string(command_calls)+",\"command_rows\":"+std::to_string(command_rows)+
            ",\"recovery_native_calls\":"+std::to_string(recovery_calls)+",\"new_old_generation_calls\":"+std::to_string(late)+
            ",\"unknown_calls\":"+std::to_string(unknown));
        chain_check_product(s,pid,birth);const auto saved=chain_gui_save(s,original);auto expected_config=original;
        expected_config.generation=saved.generation;s.log.check("arbitration_remaining_original_GUI_and_product_bytes_restored",saved==expected_config&&hash(s.product)==binary);
        s.log.check("arbitration_remaining_exact_test_windows_closed",set.close());chain_check_product(s,pid,birth);
        s.log.emit("arbitration_remaining_component",std::string("\"result\":\"PASS_SCOPED_THREE_COMPETING_MODE_CASES\",")+
            "\"Ctrl_magnet_docking_edge\":\"PASS\",\"plain_magnet_dock_unique_UP\":\"PASS\",\"active_Ctrl_nonstop_hotkey\":\"PASS\","+
            "\"aborted_generation_UP_scope\":\"actual_test_guest_hook_not_product_or_NormalUp\",\"following_plain_Move_and_native_client_pair\":true,"+
            "\"prior_modifier_pause_settings_cases_replayed\":false,\"real_applications\":\"NOT_RUN\",\"human_UAT\":\"NOT_RUN\",\"whole_version_PASS\":false");
    }catch(...){
        pause_routing_failed_case_input(s);
        try{if(s.process&&!s.left&&!s.control)chain_gui_save(s,original);}catch(...){}
        try{if(!set.closed)set.close();}catch(...){}throw;
    }
}
