#pragma once

// Finite actual-command checks for M03-F02/F04. These fixtures never call the
// product's layout solver or replace a queued command with a synthetic receipt.
bool geometry_history_receipt(State& s,std::string_view row,std::string_view type){
    namespace e=product::guest::up_evidence_detail;
    return !row.empty()&&row.front()=='{'&&row.back()=='}'&&s.product_source.size()==40&&
        e::quoted(row,"\"schema\":","panebind-v1/1")&&e::quoted(row,"\"source_sha\":",s.product_source)&&
        e::quoted(row,"\"type\":",type)&&e::number(row,"\"process_id\":")==s.product_pid;
}
std::optional<Rect> geometry_history_positioning(HWND h){
    RECT r{};if(!h||!IsWindow(h)||!GetWindowRect(h,&r))return {};
    return Rect{r.left,r.top,r.right,r.bottom};
}
std::string geometry_history_status(State& s,std::uint64_t after_sequence=0){
    namespace e=product::guest::up_evidence_detail;
    const auto mark=read_product(s).size();std::string status;const bool requested=launch(s,L"--status",true);
    const bool observed=requested&&wait([&]{status=record(s,mark,"status");const auto sequence=e::number(status,"\"sequence\":");
        return geometry_history_receipt(s,status,"status")&&sequence&&*sequence>after_sequence&&
            product::guest::matching_idle_status(status,{s.product_pid,s.product_source,0,0})&&
            e::unique_value(status,"\"enabled\":")=="true"&&e::unique_value(status,"\"native_move_handoff_pending\":")=="false"&&
            e::number(status,"\"operations\":")&&e::number(status,"\"native_callsite_claims\":");
    });
    s.log.check("geometry_history_actual_status_barrier",observed,
        "\"after_sequence\":"+std::to_string(after_sequence)+",\"actual_status\":"+quote(status));return status;
}
std::uint64_t geometry_history_source_key(LayoutFixtures& set,Rect expected){
    auto& s=set.state;chain_front(set,0);const auto point=layout_hit(set,0,HTRIGHT);
    if(!point)throw std::runtime_error("geometry_history_exact_source_native_edge_missing");
    const auto mark=read_product(s).size();cursor(s,*point);std::string preparation;
    const bool prepared=chain_stretch_preparation(set,*point,HTRIGHT,expected,mark,3500,preparation);
    const auto key=product::guest::up_evidence_detail::number(preparation,"\"key\":");
    s.log.check("geometry_history_actual_source_identity_admitted",prepared&&key&&*key,
        "\"actual_preparation\":"+quote(preparation)+",\"receipt_is_write_authority\":false");return *key;
}
void geometry_history_command(LayoutFixtures& set,std::uint64_t key,const wchar_t* action,Rect expected,bool no_native=false){
    namespace e=product::guest::up_evidence_detail;
    auto& s=set.state;chain_front(set,0);const HWND source=set.frames[0].hwnd;
    const auto before=visible(source),positioning=geometry_history_positioning(source);
    if(!key||!before||!positioning||!set.owns(0)||IsZoomed(source)||IsIconic(source))
        throw std::runtime_error("geometry_history_command_exact_normal_source_required");
    const auto prior=geometry_history_status(s);const auto mark=read_product(s).size();std::string command,placement;
    const auto name=utf8(action);const bool sent=launch(s,L"--command "+std::wstring(action),true);
    const bool observed=sent&&wait([&]{
        command=record(s,mark,"command");placement=record(s,mark,"placement");
        const auto sequence=e::number(command,"\"sequence\":"),placed=e::number(placement,"\"sequence\":");
        return geometry_history_receipt(s,command,"command")&&e::quoted(command,"\"action\":",name)&&
            e::number(command,"\"key\":")==key&&geometry_history_receipt(s,placement,"placement")&&
            e::number(placement,"\"key\":")==key&&e::number(placement,"\"generation\":")==0&&
            e::quoted(placement,"\"intent_reason\":","command")&&sequence&&placed&&*sequence>*placed;
    });
    s.log.check("geometry_history_actual_command_consumed",observed,
        "\"action\":"+quote(name)+",\"command\":"+quote(command)+",\"placement\":"+quote(placement));
    // WM_COPYDATA only enqueues the command. Request status after its actual
    // command receipt, never interpret the secondary process exit as execution.
    const auto status=geometry_history_status(s,*e::number(command,"\"sequence\":"));
    unsigned placements{},commands{},claims{};bool scope_exact=true;
    std::istringstream rows(product_log_since(s,mark));std::string row;
    while(std::getline(rows,row))if(row.ends_with('}')){
        if(e::quoted(row,"\"type\":","placement")){
            ++placements;scope_exact=scope_exact&&geometry_history_receipt(s,row,"placement")&&e::number(row,"\"key\":")==key;
            if(e::unique_value(row,"\"native_callsite_claimed\":")=="true")++claims;
            else if(e::unique_value(row,"\"native_callsite_claimed\":")!="false")scope_exact=false;
        }
        if(e::quoted(row,"\"type\":","command"))++commands;
    }
    const auto actual=visible(source),actual_positioning=geometry_history_positioning(source);
    const bool native_facts=no_native?
        e::quoted(placement,"\"reason\":","already_exact_no_native_call")&&e::number(placement,"\"call_id\":")==0&&
            e::unique_value(placement,"\"native_attempted\":")=="false"&&claims==0:
        e::quoted(placement,"\"reason\":","exact_native_completion")&&e::number(placement,"\"call_id\":").value_or(0)>0&&
            e::unique_value(placement,"\"native_attempted\":")=="true"&&e::unique_value(placement,"\"native_succeeded\":")=="true"&&claims==1;
    const auto before_calls=e::number(prior,"\"native_callsite_claims\":"),after_calls=e::number(status,"\"native_callsite_claims\":");
    const auto before_ops=e::number(prior,"\"operations\":"),after_ops=e::number(status,"\"operations\":");
    s.log.check("geometry_history_"+name+(no_native?"_actual_noop":"_actual_exact"),
        set.owns(0)&&actual==expected&&actual_positioning&&native_facts&&scope_exact&&placements==1&&commands==1&&
        before_calls&&after_calls&&*after_calls==*before_calls+(no_native?0:1)&&before_ops&&after_ops&&*after_ops==*before_ops+1&&
        e::unique_value(placement,"\"native_completion_known\":")=="true"&&e::unique_value(placement,"\"identity_valid\":")=="true"&&
        e::unique_value(placement,"\"exact\":")=="true"&&placement.find("\"target\":"+rect(expected))!=placement.npos&&
        placement.find("\"actual\":"+rect(expected))!=placement.npos&&(!no_native||(*before==expected&&actual_positioning==positioning)),
        "\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+",\"source_key\":"+std::to_string(key)+
        ",\"expected_visible\":"+rect(expected)+",\"actual_visible\":"+(actual?rect(*actual):"null")+
        ",\"before_positioning\":"+rect(*positioning)+",\"actual_positioning\":"+(actual_positioning?rect(*actual_positioning):"null")+
        ",\"actual_placement\":"+quote(placement)+",\"prior_status\":"+quote(prior)+",\"after_status\":"+quote(status)+
        ",\"new_placement_records\":"+std::to_string(placements)+",\"new_native_claims\":"+std::to_string(claims)+
        ",\"no_op_native_succeeded_not_required\":true");
}
void geometry_history_rejected_undo(LayoutFixtures& set,std::uint64_t key,const char* label){
    namespace e=product::guest::up_evidence_detail;
    auto& s=set.state;chain_front(set,0);const auto source=set.frames[0].hwnd;
    const auto before=visible(source),positioning=geometry_history_positioning(source);const auto prior=geometry_history_status(s);
    const auto mark=read_product(s).size();std::string notice;const bool sent=launch(s,L"--command Undo",true);
    const bool consumed=sent&&wait([&]{notice=record(s,mark,"notice");const auto sequence=e::number(notice,"\"sequence\":");
        return geometry_history_receipt(s,notice,"notice")&&sequence&&*sequence&&
            e::quoted(notice,"\"reason\":",utf8(L"当前窗口已改变，或没有仍归属本窗口的布局可撤销。"));});
    s.log.check("geometry_history_Undo_actual_no_history_notice",consumed,"\"actual_notice\":"+quote(notice));
    const auto status=geometry_history_status(s,*e::number(notice,"\"sequence\":"));
    unsigned placements{},commands{};std::istringstream rows(product_log_since(s,mark));std::string row;
    while(std::getline(rows,row))if(row.ends_with('}')){
        if(e::quoted(row,"\"type\":","placement"))++placements;
        if(e::quoted(row,"\"type\":","command"))++commands;
    }
    const auto actual=visible(source),actual_positioning=geometry_history_positioning(source);
    s.log.check(label,key&&set.owns(0)&&before&&positioning&&actual==before&&actual_positioning==positioning&&!placements&&!commands&&
        e::number(prior,"\"native_callsite_claims\":")==e::number(status,"\"native_callsite_claims\":")&&
        e::number(prior,"\"operations\":")==e::number(status,"\"operations\":"),
        "\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+",\"source_key\":"+std::to_string(key)+
        ",\"actual_notice\":"+quote(notice)+",\"after_status\":"+quote(status)+
        ",\"before_visible\":"+(before?rect(*before):"null")+",\"actual_visible\":"+(actual?rect(*actual):"null")+
        ",\"before_positioning\":"+(positioning?rect(*positioning):"null")+",\"actual_positioning\":"+(actual_positioning?rect(*actual_positioning):"null")+
        ",\"new_placements\":"+std::to_string(placements)+",\"new_commands\":"+std::to_string(commands)+
        ",\"early_Undo_has_no_command_receipt\":true,\"exit_zero_is_not_execution_proof\":true");
}
void geometry_history_native_max_restore(LayoutFixtures& set,Rect expected){
    namespace e=product::guest::up_evidence_detail;
    auto& s=set.state;chain_front(set,0);const auto source=set.frames[0].hwnd;
    const auto positioning=geometry_history_positioning(source);const auto prior=geometry_history_status(s);const auto mark=read_product(s).size();
    WINDOWPLACEMENT original{sizeof(original)},maximized{sizeof(maximized)},restored{sizeof(restored)};
    if(!set.owns(0)||visible(source)!=expected||!positioning||!GetWindowPlacement(source,&original))throw std::runtime_error("geometry_history_native_normal_state_unknown");
    const bool max_queued=PostMessageW(source,WM_SYSCOMMAND,SC_MAXIMIZE,0)!=FALSE;
    const bool max_observed=max_queued&&wait([&]{return set.owns(0)&&IsZoomed(source)&&GetWindowPlacement(source,&maximized)&&maximized.showCmd==SW_SHOWMAXIMIZED;});
    s.log.check("geometry_history_actual_native_Maximize",max_observed,
        "\"exact_owned_source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+",\"actual_showCmd\":"+std::to_string(maximized.showCmd)+
        ",\"queued_system_command_not_observation\":true,\"product_Maximize_command_used\":false");
    const bool restore_queued=set.owns(0)&&PostMessageW(source,WM_SYSCOMMAND,SC_RESTORE,0)!=FALSE;
    const bool restore_observed=restore_queued&&wait([&]{return set.owns(0)&&!IsZoomed(source)&&!IsIconic(source)&&
        visible(source)==expected&&geometry_history_positioning(source)==positioning&&GetWindowPlacement(source,&restored);});
    const auto status=geometry_history_status(s);unsigned placements{};std::istringstream rows(product_log_since(s,mark));std::string row;
    while(std::getline(rows,row))if(e::quoted(row,"\"type\":","placement"))++placements;
    const auto actual=visible(source),actual_positioning=geometry_history_positioning(source);
    s.log.check("geometry_history_actual_native_Restore_retains_layout_geometry",restore_observed&&set.owns(0)&&actual==expected&&actual_positioning==positioning&&!placements&&
        EqualRect(&original.rcNormalPosition,&maximized.rcNormalPosition)&&EqualRect(&original.rcNormalPosition,&restored.rcNormalPosition)&&
        e::number(prior,"\"native_callsite_claims\":")==e::number(status,"\"native_callsite_claims\":"),
        "\"actual_restored_visible\":"+(actual?rect(*actual):"null")+
        ",\"actual_normal_positioning\":"+(actual_positioning?rect(*actual_positioning):"null")+
        ",\"new_product_placements\":"+std::to_string(placements)+",\"history_preservation_requires_subsequent_actual_Undo\":true");
}
void owned_geometry_history_case(State& s){
    settings_guest_scope(s);
    if(s.suite!="GeometryHistoryOnly"||s.left||s.control)throw std::runtime_error("geometry_history_exact_released_guest_suite_required");
    const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    if(!original.enabled)throw std::runtime_error("geometry_history_enabled_product_required");
    auto wanted=original;wanted.docking=true;wanted.smart_docking=false;wanted.stretch=true;wanted.smart_stretch=false;
    wanted.magnet_move=false;wanted.magnet_resize=false;wanted.glue=false;wanted.preview=false;wanted.drag_transparency=false;wanted.shake=false;
    const auto applied=chain_gui_save(s,wanted);LayoutFixtures set{s};
    try{
        set.create();const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<1100||work.height()<700)throw std::runtime_error("geometry_history_work_area_below_finite_fixture_requirement");
        const LayoutLimit limit{120,100,work.width(),work.height()};
        const Rect source{work.l+160,work.t+140,work.l+560,work.t+400};set.setup({source},{limit});
        const auto key=geometry_history_source_key(set,source);
        const LONG l=work.l+(work.width()-source.width())/2,t=work.t+(work.height()-source.height())/2;
        const Rect center{l,t,l+source.width(),t+source.height()},bottom{source.l,source.t,source.r,work.b};
        geometry_history_command(set,key,L"Center",center);geometry_history_command(set,key,L"Center",center,true);
        geometry_history_command(set,key,L"Undo",source);
        geometry_history_command(set,key,L"StretchBottom",bottom);geometry_history_command(set,key,L"StretchBottom",bottom,true);
        geometry_history_command(set,key,L"Undo",source);
        const Rect dock{work.l,work.t,work.l+work.width()/2,work.t+work.height()/2},stretched{dock.l,dock.t,dock.r,work.b};
        geometry_history_command(set,key,L"DockTopLeft",dock);geometry_history_command(set,key,L"StretchBottom",stretched);
        geometry_history_command(set,key,L"Undo",dock);geometry_history_command(set,key,L"Undo",source);
        geometry_history_command(set,key,L"DockTopLeft",dock);geometry_history_native_max_restore(set,dock);
        geometry_history_command(set,key,L"Undo",source);
        geometry_history_command(set,key,L"DockTopLeft",dock);
        const auto moved=pause_routing_native_drag(set,0,60,40,true,"geometry_history_actual_Shift_native_manual_Move_UP_END");
        s.log.check("geometry_history_manual_movement_not_new_layout_baseline",moved.size()==1&&moved[0]==translated(dock,60,40));
        geometry_history_rejected_undo(set,key,"geometry_history_manual_Move_Undo_does_not_force_old_layout");
        const Rect moved_center{work.l+(work.width()-dock.width())/2,work.t+(work.height()-dock.height())/2,
            work.l+(work.width()-dock.width())/2+dock.width(),work.t+(work.height()-dock.height())/2+dock.height()};
        geometry_history_command(set,key,L"Center",moved_center); // Leave real history on the identity which is about to close.
        const auto old_hwnd=set.frames[0].hwnd;const auto old_class=gui_class(old_hwnd);const auto old_frame=reinterpret_cast<std::uintptr_t>(&set.frames[0]);
        s.log.check("geometry_history_old_identity_actually_destroyed",set.close()&&!IsWindow(old_hwnd));
        LayoutFixtures fresh{s};fresh.create();const Rect fresh_source{work.l+230,work.t+180,work.l+610,work.t+490};
        fresh.setup({fresh_source},{limit});const auto fresh_key=geometry_history_source_key(fresh,fresh_source);
        DWORD actual_pid{};const auto actual_tid=GetWindowThreadProcessId(fresh.frames[0].hwnd,&actual_pid);
        s.log.check("geometry_history_same_process_class_new_exact_identity",fresh.owns(0)&&actual_pid==GetCurrentProcessId()&&actual_tid==set.thread&&
            gui_class(fresh.frames[0].hwnd)==old_class&&reinterpret_cast<std::uintptr_t>(&fresh.frames[0])!=old_frame&&fresh_key!=key,
            "\"old_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(old_hwnd))+",\"new_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(fresh.frames[0].hwnd))+
            ",\"old_product_key\":"+std::to_string(key)+",\"new_product_key\":"+std::to_string(fresh_key)+",\"actual_PID\":"+std::to_string(actual_pid)+
            ",\"actual_TID\":"+std::to_string(actual_tid)+",\"OS_HWND_value_reused\":"+(old_hwnd==fresh.frames[0].hwnd?"true":"false")+
            ",\"same_PID_TID_class_not_identity_authority\":true");
        geometry_history_rejected_undo(fresh,fresh_key,"geometry_history_recreated_source_does_not_inherit_old_history");
        s.log.check("geometry_history_new_exact_windows_closed",fresh.close());chain_check_product(s,pid,birth);
        const auto restored=chain_gui_save(s,original);auto expected_config=original;expected_config.generation=restored.generation;
        s.log.check("geometry_history_original_configuration_restored",restored==expected_config);
        s.log.emit("geometry_history_component","\"result\":\"PASS\",\"same_product_process\":true,\"actual_noop_Center_StretchBottom\":true,"+
            std::string("\"actual_discrete_history_and_native_Max_Restore\":true,\"manual_and_new_identity_history_boundaries\":true,\"full_M03_or_version_PASS\":false,\"human_uat\":\"NOT_RUN\""));
    }catch(...){
        pause_routing_failed_case_input(s);try{if(!set.closed)set.close();}catch(...){}
        try{if(chain_settings(s)==applied&&!s.left&&!s.control)chain_gui_save(s,original);}catch(...){}
        throw;
    }
}
