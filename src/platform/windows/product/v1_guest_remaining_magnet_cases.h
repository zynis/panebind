#pragma once

// Guest-only black-box Move contracts. Expected rectangles are independent
// arithmetic from DOWN, not a call to the production magnet/motion solver.
void owned_remaining_magnet_policy_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if(s.suite!="RemainingMoveContractsOnly"||!original.enabled||!original.rules.empty()||s.left||s.control||
       binary.empty()||binary!=hash(s.input_product))throw std::runtime_error("remaining_magnet_exact_idle_guest_required");
    auto config=original;config.magnet_move=true;config.magnet_resize=false;config.glue=false;config.docking=false;
    config.preview=false;config.feedback=false;config.drag_transparency=false;config.shake=false;
    config.attraction_dip=10;config.release_dip=16;config.speed_dip=20000;
    auto applied=chain_gui_save(s,config);LayoutFixtures set{s};
    try{
        set.create();const auto work=chain_work(set.frames[0].hwnd);const auto dpi=GetDpiForWindow(set.frames[0].hwnd);
        if(work.width()<1300||work.height()<800||!dpi||dpi>384)
            throw std::runtime_error("remaining_magnet_actual_work_area_or_DPI_outside_declared_fixture");
        // Match the documented product rounding, rather than MulDiv's signed
        // tie behaviour; these are independent numerical distance oracles.
        const LONG entry=static_cast<LONG>((10LL*dpi+48)/96),release=static_cast<LONG>((16LL*dpi+48)/96);
        const Rect source{work.l+240,work.t+200,work.l+640,work.t+500};
        const Rect target{work.l+900,work.t+350,work.l+1220,work.t+650};
        const LONG gap=target.l-source.r;const Rect snapped=translated(source,gap,0);
        const LayoutLimit limit{120,100,work.width(),work.height()};
        std::vector<Rect> initial{source,target},native;
        const auto setup=[&](Rect actual_source){
            initial={actual_source,target};set.setup(initial,{limit,limit});
            native=resize_magnet_native_baseline(set,initial);
        };
        const auto start_key=[&](const Gesture& g){
            const auto row=record(s,g.offset,"gesture_start",g.generation);const auto key=e::number(row,"\"key\":");
            const bool exact=geometry_history_receipt(s,row,"gesture_start")&&key&&*key&&
                e::unique_value(row,"\"resize\":")=="false"&&e::unique_value(row,"\"ctrl\":")=="false"&&
                e::number(row,"\"group_members\":")==1&&row.find("\"original\":"+rect(initial[0])+",")!=row.npos&&
                chain_settings(s)==applied;
            s.log.check("remaining_magnet_actual_plain_Move_original_anchor",exact,
                "\"generation\":"+std::to_string(g.generation)+",\"actual_start\":"+quote(row)+
                ",\"GUI_saved_configuration_generation\":"+std::to_string(applied.generation));return *key;
        };
        const auto placements=[&](const Gesture& g){
            std::string result,row;std::istringstream rows(product_log_since(s,g.offset));
            while(std::getline(rows,row))if(geometry_history_receipt(s,row,"placement")&&
                e::number(row,"\"generation\":")==g.generation)result+=row+'\n';return result;
        };
        const auto quantum=[&](const Gesture& g,std::uint64_t key,LONG dx,Rect expected,const char* name){
            const auto before=visible(set.frames[0].hwnd);const auto mark=read_product(s).size();LARGE_INTEGER clock{};
            if(!before||!QueryPerformanceCounter(&clock)||clock.QuadPart<=0)
                throw std::runtime_error("remaining_magnet_quantum_actual_before_or_clock_unknown");
            layout_phase(set,0,g,dx,0,{expected,target},name);
            unsigned calls{},rows_seen{};bool valid=true;std::string row,receipt;std::istringstream rows(product_log_since(s,mark));
            while(std::getline(rows,row))if(e::quoted(row,"\"type\":","placement")){
                ++rows_seen;receipt=row;
                valid=valid&&geometry_history_receipt(s,row,"placement")&&e::number(row,"\"generation\":")==g.generation&&
                    e::number(row,"\"key\":")==key&&e::unique_value(row,"\"identity_valid\":")=="true"&&
                    e::unique_value(row,"\"exact\":")=="true"&&
                    row.find("\"target\":"+rect(expected)+",")!=row.npos&&row.find("\"actual\":"+rect(expected)+",")!=row.npos&&
                    e::number(row,"\"hook_observed_qpc\":").value_or(0)>=static_cast<std::uint64_t>(clock.QuadPart);
                if(e::unique_value(row,"\"native_callsite_claimed\":")=="true"){
                    ++calls;const auto issued=e::number(row,"\"native_before_qpc\":"),completed=e::number(row,"\"native_after_qpc\":");
                    valid=valid&&e::unique_value(row,"\"native_attempted\":")=="true"&&
                        e::unique_value(row,"\"native_succeeded\":")=="true"&&e::unique_value(row,"\"native_completion_known\":")=="true"&&
                        issued&&completed&&*issued>=e::number(row,"\"hook_observed_qpc\":").value_or(UINT64_MAX)&&*completed>=*issued;
                }else valid=valid&&e::unique_value(row,"\"native_callsite_claimed\":")=="false"&&
                    e::unique_value(row,"\"native_attempted\":")=="false"&&e::quoted(row,"\"reason\":","already_exact_no_native_call");
            }
            const Rect expected_native=translated(native[0],expected.l-initial[0].l,expected.t-initial[0].t);
            const bool actual=set.owns(0)&&set.owns(1)&&visible(set.frames[0].hwnd)==expected&&
                geometry_history_positioning(set.frames[0].hwnd)==expected_native&&visible(set.frames[1].hwnd)==target&&
                geometry_history_positioning(set.frames[1].hwnd)==native[1]&&expected.width()==initial[0].width()&&
                expected.height()==initial[0].height()&&GetDpiForWindow(set.frames[0].hwnd)==dpi;
            s.log.check("remaining_magnet_original_DOWN_single_writer_quantum",valid&&actual&&rows_seen==1&&calls<=1&&
                calls==(*before==expected?0u:1u),
                "\"quantum\":"+quote(name)+",\"generation\":"+std::to_string(g.generation)+",\"source_key\":"+std::to_string(key)+
                ",\"original_DOWN_visible\":"+rect(initial[0])+",\"raw_original_DOWN_intent\":"+rect(translated(initial[0],dx,0))+
                ",\"expected_visible\":"+rect(expected)+",\"expected_positioning\":"+rect(expected_native)+
                ",\"target_unchanged\":"+(actual?"true":"false")+",\"placement_records\":"+std::to_string(rows_seen)+
                ",\"native_calls_this_quantum\":"+std::to_string(calls)+",\"actual_placement\":"+quote(receipt));return receipt;
        };
        const auto normal_up=[&](const Gesture& g,Rect expected){
            const auto before=placements(g);POINT point{};if(!GetCursorPos(&point))throw std::runtime_error("remaining_magnet_pre_UP_cursor_unknown");
            layout_up(set,g,{expected,target});std::string legacy,gone,ll,row;std::istringstream rows(product_log_since(s,g.offset));
            while(std::getline(rows,row))if(e::number(row,"\"generation\":")==g.generation){
                if(geometry_history_receipt(s,row,"gesture_input_resource")){
                    if(e::quoted(row,"\"fact\":","legacy_left_up"))legacy=row;
                    if(e::quoted(row,"\"fact\":","isolation_gone"))gone=row;
                }
                if(geometry_history_receipt(s,row,"input_button_observed")&&e::quoted(row,"\"kind\":","UP")&&
                   e::quoted(row,"\"source\":","WH_MOUSE_LL")&&e::number(row,"\"button\":")==1)ll=row;
            }
            const auto ended=record(s,g.offset,"gesture_end",g.generation);
            const auto status=geometry_history_status(s,e::number(ended,"\"sequence\":").value_or(0));POINT after{};
            const bool exact=geometry_history_receipt(s,ended,"gesture_end")&&e::unique_value(ended,"\"normal_up\":")=="true"&&
                !ll.empty()&&e::number(ll,"\"hook_observed_qpc\":").value_or(0)>=s.left_up_send_qpc&&
                e::unique_value(legacy,"\"actual_legacy_up_accepted\":")=="true"&&
                e::unique_value(gone,"\"release_succeeded\":")=="true"&&e::unique_value(gone,"\"detach_succeeded\":")=="true"&&
                e::unique_value(gone,"\"overlay_destroyed\":")=="true"&&e::unique_value(gone,"\"hotkey_unregistered\":")=="true"&&
                GetCursorPos(&after)&&point.x==after.x&&point.y==after.y&&placements(g)==before&&
                set.owns(0)&&set.owns(1)&&visible(set.frames[0].hwnd)==expected&&visible(set.frames[1].hwnd)==target;
            s.log.check("remaining_magnet_actual_NormalUp_no_late_snap_or_new_placement",exact,
                "\"generation\":"+std::to_string(g.generation)+",\"actual_LL_UP\":"+quote(ll)+",\"actual_legacy_UP\":"+quote(legacy)+
                ",\"actual_resource_gone\":"+quote(gone)+",\"actual_end\":"+quote(ended)+",\"idle_status\":"+quote(status)+
                ",\"same_cursor_UP\":true,\"terminal_resolve_or_additional_UP_used\":false");chain_check_product(s,pid,birth);
        };

        setup(source);s.log.check("remaining_magnet_real_DPI_10_16_distance_oracles",entry>0&&release>entry&&gap>release+1,
            "\"actual_DPI\":"+std::to_string(dpi)+",\"attraction_DIP\":10,\"release_DIP\":16,\"entry_pixels\":"+std::to_string(entry)+
            ",\"release_pixels\":"+std::to_string(release)+",\"GUI_saved_speed_DIP_per_second\":20000");
        const auto boundary=layout_down(set,0,HTCAPTION,false);const auto key=start_key(boundary);
        (void)quantum(boundary,key,gap-entry-1,translated(source,gap-entry-1,0),"remaining_magnet_entry_plus_one_pixel_free");
        (void)quantum(boundary,key,gap-entry,snapped,"remaining_magnet_entry_exact_boundary_snaps");
        (void)quantum(boundary,key,gap-entry-1,snapped,"remaining_magnet_held_beyond_entry_without_reanchoring");
        (void)quantum(boundary,key,gap-release,snapped,"remaining_magnet_release_exact_boundary_still_held");
        (void)quantum(boundary,key,gap-release-1,translated(source,gap-release-1,0),"remaining_magnet_release_plus_one_pixel_detaches");
        (void)quantum(boundary,key,gap-entry,snapped,"remaining_magnet_original_anchor_reentry_snaps_again");normal_up(boundary,snapped);

        // One deliberately low, valid setting makes a single ordinary large
        // input a useful black-box contrast. QPC is an observed input bound,
        // NOT the unlogged production millisecond MotionSample/enum.
        config.speed_dip=1;applied=chain_gui_save(s,config);setup(source);
        const auto fast=layout_down(set,0,HTCAPTION,false);const auto fast_key=start_key(fast);
        s.log.check("remaining_magnet_velocity_generation_is_fresh",fast.generation>boundary.generation);
        const LONG fast_dx=gap-entry+1;const auto free=translated(source,fast_dx,0);
        const auto pending=record(s,fast.offset,"native_move_pending",fast.generation);
        const auto down_qpc=e::number(pending,"\"down_hook_qpc\":");LARGE_INTEGER frequency{};
        const bool frequency_known=QueryPerformanceFrequency(&frequency)&&frequency.QuadPart>0;
        const auto free_receipt=quantum(fast,fast_key,fast_dx,free,"remaining_magnet_low_speed_policy_free_inside_attraction");
        const auto move_qpc=e::number(free_receipt,"\"hook_observed_qpc\":");
        const auto limit_pixels=static_cast<std::uint64_t>((static_cast<std::uint64_t>(dpi)+48)/96);
        const bool measured=geometry_history_receipt(s,pending,"native_move_pending")&&down_qpc&&move_qpc&&*move_qpc>*down_qpc&&
            frequency_known&&limit_pixels&&static_cast<std::uint64_t>(fast_dx)*static_cast<std::uint64_t>(frequency.QuadPart)>
                limit_pixels*(*move_qpc-*down_qpc);
        const auto hold=GetTickCount64();const bool held=wait([&]{return GetTickCount64()-hold>=120;},180);
        s.log.check("remaining_magnet_free_before_same_point_UP",held&&set.owns(0)&&visible(set.frames[0].hwnd)==free&&
            set.owns(1)&&visible(set.frames[1].hwnd)==target,"\"actual_held_ms\":"+std::to_string(GetTickCount64()-hold));
        normal_up(fast,free);
        s.log.emit("remaining_magnet_velocity_blackbox","\"result\":"+quote(measured?"PASS":"NOT_RUN")+
            ",\"GUI_saved_speed_DIP_per_second\":1,\"actual_DPI\":"+std::to_string(dpi)+
            ",\"original_DOWN_qpc\":"+(down_qpc?std::to_string(*down_qpc):"null")+
            ",\"actual_motion_hook_qpc\":"+(move_qpc?std::to_string(*move_qpc):"null")+
            ",\"QPC_frequency\":"+(frequency_known?std::to_string(frequency.QuadPart):"null")+
            ",\"raw_candidate_gap_pixels\":"+std::to_string(entry-1)+",\"actual_free_placement\":"+quote(free_receipt)+
            ",\"observed_DOWN_to_motion_average_exceeds_setting\":"+(measured?"true":"false")+
            ",\"production_MotionState_or_ticks_observed\":false,\"Suppressed_enum_PASS_inferred\":false,\"invalid_timestamp_native_scenario\":\"NOT_RUN\"");
        if(!measured)s.log.failed=true;
        config.speed_dip=2000;applied=chain_gui_save(s,config);
        initial={free,target};native=resize_magnet_native_baseline(set,initial);
        const auto next=layout_down(set,0,HTCAPTION,false);const auto next_key=start_key(next);
        s.log.check("remaining_magnet_following_default_speed_generation_fresh",next.generation>fast.generation);
        (void)quantum(next,next_key,1,snapped,"remaining_magnet_next_slow_Move_default_speed_recovers_attraction");normal_up(next,snapped);
        const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        s.log.check("remaining_magnet_original_settings_and_binary_restored",restored==expected&&hash(s.product)==binary);
        chain_check_product(s,pid,birth);s.log.check("remaining_magnet_exact_fixture_roots_closed",set.close());
        s.log.emit("remaining_magnet_policy_component","\"result\":"+quote(measured?"PASS":"PARTIAL")+
            ",\"scope\":\"actual_10_16_DIP_boundaries_original_DOWN_hysteresis_single_writer_and_speed_setting_blackbox\","
            "\"real_application_matrix\":\"NOT_RUN\",\"production_motion_enum_observed\":false,\"whole_M05_F05_or_version_PASS\":false,\"human_uat\":\"NOT_RUN\"");
    }catch(...){
        pause_routing_failed_case_input(s);
        try{if(s.process&&!s.left&&!s.control){(void)geometry_history_status(s);if(chain_settings(s)==applied)chain_gui_save(s,original);}}catch(...){}
        try{if(!set.closed)set.close();}catch(...){}throw;
    }
}
