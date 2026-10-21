#pragma once
#include <exception>

// Guest-only single I05 corner action. Keep chain_double_click's original
// timing and non-strict scope: current product LL records, not test insertion
// counts or this helper's descriptive fields, must establish actual delivery.
bool corner_input_actual_idle(LayoutFixtures& set,std::size_t scope_mark,std::string& status,
    std::string& resources){
    namespace e=product::guest::up_evidence_detail;
    auto& s=set.state;status.clear();resources="[]";const auto mark=read_product(s).size();
    const bool requested=launch(s,L"--status",true);
    return requested&&wait([&]{
        status=record(s,mark,"status");resources="[";
        struct Resource {std::uint64_t generation{},overlay{};bool gone{};};
        std::array<Resource,2> observed{};std::size_t used{};bool known=true;
        std::istringstream rows(product_log_since(s,scope_mark));std::string row;
        while(std::getline(rows,row)){
            if(!e::quoted(row,"\"type\":","gesture_input_resource"))continue;
            const auto generation=e::number(row,"\"generation\":"),overlay=e::number(row,"\"overlay\":");
            if(!generation){known=false;continue;}
            if(!*generation)continue; // Process-lifetime idle Raw observation is not a gesture shield.
            if(!overlay){known=false;continue;}
            if(!*overlay)continue; // A pre-creation failure did not create an overlay.
            if(resources.size()>1)resources+=',';resources+=row;
            if(!e::quoted(row,"\"schema\":","panebind-v1/1")||!e::quoted(row,"\"source_sha\":",s.product_source)||
               e::number(row,"\"process_id\":")!=s.product_pid||
               e::number(row,"\"source\":")!=reinterpret_cast<std::uintptr_t>(set.frames[0].hwnd)||
               e::number(row,"\"source_pid\":")!=GetCurrentProcessId()||e::number(row,"\"source_tid\":")!=set.thread){known=false;continue;}
            std::size_t index{};while(index<used&&observed[index].generation!=*generation)++index;
            if(index==used){if(used==observed.size()){known=false;continue;}observed[used++]={*generation,*overlay,false};}
            if(observed[index].overlay!=*overlay){known=false;continue;}
            if(e::quoted(row,"\"fact\":","isolation_gone")){
                const auto attached=e::unique_value(row,"\"attach_succeeded\":");
                observed[index].gone=e::unique_value(row,"\"overlay_destroyed\":")=="true"&&
                    e::unique_value(row,"\"hotkey_unregistered\":")=="true"&&
                    (attached=="false"||(attached=="true"&&e::unique_value(row,"\"detach_completed\":")=="true"&&
                        e::unique_value(row,"\"detach_succeeded\":")=="true"));
            }
        }
        resources+=']';
        for(std::size_t n=0;n<used;++n)known=known&&observed[n].gone&&
            !IsWindow(reinterpret_cast<HWND>(static_cast<std::uintptr_t>(observed[n].overlay)));
        GUITHREADINFO gui{sizeof(gui)};
        return known&&!s.left&&!s.control&&set.owns(0)&&set.owns(1)&&
            product::guest::matching_idle_status(status,{s.product_pid,s.product_source,0,0})&&
            e::unique_value(status,"\"enabled\":")=="true"&&e::unique_value(status,"\"native_move_handoff_pending\":")=="false"&&
            GetGUIThreadInfo(set.thread,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner;
    },1500);
}

bool corner_input_driver_send_facts(State& s,std::uint64_t mark,bool helper_returned){
    namespace e=product::guest::up_evidence_detail;
    std::istringstream rows(s.log.since(mark));std::string row,buttons="[",cursor_receipts="[";
    unsigned count{};bool exact=true;std::uint64_t previous{};
    while(std::getline(rows,row)){
        if(e::quoted(row,"\"type\":","test_cursor_observation")){
            if(cursor_receipts.size()>1)cursor_receipts+=',';cursor_receipts+=row;
        }
        if(!e::quoted(row,"\"type\":","test_button_send"))continue;
        if(count)buttons+=',';buttons+=row;
        const auto before=e::number(row,"\"send_before_qpc\":"),after=e::number(row,"\"send_after_qpc\":");
        exact=exact&&e::quoted(row,"\"schema\":","panebind-v1-owned-driver/1")&&
            e::quoted(row,"\"source_sha\":",PANEBIND_BUILD_SHA)&&
            e::quoted(row,"\"requested\":",count%2?"UP":"DOWN")&&
            e::unique_value(row,"\"SendInput_called\":")=="true"&&e::number(row,"\"SendInput_return\":")==1&&
            before&&after&&*before>previous&&*after>=*before;
        if(after)previous=*after;++count;
    }
    buttons+=']';cursor_receipts+=']';POINT point{};const bool pointer_known=GetCursorPos(&point)!=FALSE;
    s.log.emit("corner_input_driver_send_facts","\"helper_returned\":"+std::string(helper_returned?"true":"false")+
        ",\"driver_button_rows\":"+std::to_string(count)+",\"ordered_single_packet_insertions\":"+(exact?"true":"false")+
        ",\"actual_driver_button_records\":"+buttons+",\"actual_cursor_observation_records\":"+cursor_receipts+
        ",\"pointer_sample_phase\":\"after_helper_not_per_button\",\"actual_pointer\":"+
        (pointer_known?"["+std::to_string(point.x)+","+std::to_string(point.y)+"]":"null")+
        ",\"per_button_point_not_sampled_by_existing_helper\":true,\"native_DOWN_UP_delivery_inferred\":false,"+
        "\"NormalUp_inferred\":false,\"actual_two_pairs_require_product_LL_trace_review\":true");
    return exact&&count==4;
}

void owned_corner_input_case(State& s){
    namespace v=panebind::core::v1;
    if(s.suite!="CornerInputOnly"||!desktop()||s.left||s.control||(GetAsyncKeyState(VK_SHIFT)&0x8000))
        throw std::runtime_error("corner_input_exact_released_guest_scope_required");
    const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    const auto binary=hash(s.product);
    if(!original.enabled||original.dock_actions!=v::default_settings().dock_actions)
        throw std::runtime_error("corner_input_original_I05_configuration_precondition_not_met");
    chain_check_product(s,pid,birth);LayoutFixtures set{s};set.create();
    std::exception_ptr failure;std::string failure_reason,status,resources="[]";bool helper_returned{},ledger_exact{},idle{};
    bool settings_changed{};std::size_t product_mark{};
    try{
        const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<1100||work.height()<850)throw std::runtime_error("corner_input_original_I05_work_area_precondition_not_met");
        const LONG line=work.l+work.width()*2/5;
        const Rect target{work.l,work.t,line,work.b},source{line+80,work.t+180,line+480,work.t+480};
        const Rect expected{source.l,source.t,work.r,work.b};
        const LayoutLimit limits{100,100,work.width(),work.height()};set.setup({source,target},{limits,limits});
        auto config=original;config.attraction_dip=10;config.release_dip=16;config.magnet_move=false;config.magnet_resize=true;
        config.docking=true;config.stretch=true;config.glue=true;config.smart_docking=false;config.smart_stretch=true;
        config.preview=true;config.feedback=false;config.drag_transparency=false;config.shake=false;
        settings_changed=true;chain_gui_save(s,config);
        product_mark=read_product(s).size();const auto driver_mark=s.log.mark();
        try{
            chain_double_click(set,HTBOTTOMRIGHT,false,expected,"corner_input_original_I05_single_BR_geometry",false);
            helper_returned=true;
        }catch(const std::exception& error){failure=std::current_exception();failure_reason=error.what();}
        ledger_exact=corner_input_driver_send_facts(s,driver_mark,helper_returned);
        if(failure)std::rethrow_exception(failure);
        s.log.check("corner_input_single_BR_geometry_and_driver_insertions_only",helper_returned&&ledger_exact&&set.owns(0)&&set.owns(1)&&
            visible(set.frames[0].hwnd)==expected&&visible(set.frames[1].hwnd)==target,
            "\"expected\":"+rect(expected)+",\"actual_source\":"+(visible(set.frames[0].hwnd)?rect(*visible(set.frames[0].hwnd)):"null")+
            ",\"actual_target\":"+(visible(set.frames[1].hwnd)?rect(*visible(set.frames[1].hwnd)):"null")+
            ",\"original_I05_helper_timing_unchanged\":true,\"extra_click_or_second_preparation_wait_added\":false,"+
            "\"two_actual_UP_or_full_I05_PASS_inferred\":false");
        idle=corner_input_actual_idle(set,product_mark,status,resources);
        s.log.check("corner_input_actual_idle_and_observed_resources_gone",idle,
            "\"actual_status\":"+quote(status)+",\"actual_scoped_resource_records\":"+resources+",\"guest_destruction_used_as_cleanup\":false");
        chain_command(set,0,L"Undo",source);
        s.log.check("corner_input_actual_Undo_and_original_target_retained",set.owns(0)&&set.owns(1)&&
            visible(set.frames[0].hwnd)==source&&visible(set.frames[1].hwnd)==target);
    }catch(const std::exception& error){if(!failure){failure=std::current_exception();failure_reason=error.what();}}
    // Do not add a click or repeat an attempted UP. These are cleanup facts,
    // never replacements for missing LL delivery or the original failed case.
    bool cleanup_up_attempted{};std::string cleanup_error;
    if(s.left&&!s.left_up_attempted){cleanup_up_attempted=true;try{button(s,false);}catch(const std::exception& error){cleanup_error=error.what();}}
    if(failure){
        try{idle=corner_input_actual_idle(set,product_mark,status,resources);}catch(const std::exception& error){cleanup_error+=';';cleanup_error+=error.what();}
        s.log.emit("corner_input_original_failure_and_cleanup","\"original_failure\":"+quote(failure_reason)+
            ",\"single_pending_UP_attempted\":"+(cleanup_up_attempted?"true":"false")+
            ",\"UP_already_attempted_not_repeated\":"+(s.left_up_attempted?"true":"false")+
            ",\"actual_idle_and_resources_gone\":"+(idle?"true":"false")+",\"cleanup_error\":"+quote(cleanup_error)+
            ",\"actual_status\":"+quote(status)+",\"actual_scoped_resource_records\":"+resources+
            ",\"original_failed_function_result_preserved\":true,\"cleanup_or_NormalUp_PASS_inferred\":false");
    }
    bool restored{};if(settings_changed&&idle&&!s.left&&!s.control){
        try{const auto actual=chain_gui_save(s,original);auto expected=original;expected.generation=actual.generation;restored=actual==expected;}
        catch(const std::exception& error){if(!failure){failure=std::current_exception();failure_reason=error.what();}}
    }
    const bool closed=set.close();
    s.log.emit("corner_input_final_exact_cleanup","\"original_configuration_restored\":"+std::string(restored?"true":"false")+
        ",\"exact_test_windows_closed\":"+(closed?"true":"false")+",\"original_failure_preserved\":"+(failure?"true":"false")+
        ",\"guest_destruction_used_as_product_cleanup\":false");
    if(failure)std::rethrow_exception(failure);
    chain_check_product(s,pid,birth);
    s.log.check("corner_input_original_settings_exact_windows_and_product_retained",restored&&closed&&hash(s.product)==binary);
    s.log.emit("corner_input_component","\"result\":\"PASS\",\"scope\":\"single_original_I05_BR_geometry_and_driver_insertion_only\","
        "\"actual_product_two_DOWN_UP_pairs\":\"REQUIRES_LL_TRACE_REVIEW\",\"product_restarted\":false,"
        "\"historical_failures_preserved\":true,\"whole_F04_I05_or_version_PASS\":false,\"human_uat\":\"NOT_RUN\"");
}
