#pragma once

// Only the disposable test application's exact owned roots are changed here.
// App-side native changes are not product-writer receipts or UI-thread claims.
void visual_ownership_app_change(LayoutFixtures& set,std::optional<BYTE> alpha,std::optional<bool> top,const char* label){
    auto& s=set.state;const HWND h=set.frames[0].hwnd;DWORD pid{};const auto tid=GetWindowThreadProcessId(h,&pid);
    const auto before=native_visual(h);const auto geometry=visible(h),positioning=geometry_history_positioning(h);
    if(!set.owns(0)||pid!=GetCurrentProcessId()||tid!=set.thread||!before||!before->known||!geometry||!positioning||(!alpha&&!top))
        throw std::runtime_error("visual_ownership_app_change_exact_known_own_root_required");
    auto expected=*before;bool style_attempted=false,alpha_attempted=false,top_attempted=false,style_ok=true,alpha_ok=true,top_ok=true;DWORD style_error{},alpha_error{},top_error{};
    if(alpha){
        if(!before->layered){
            SetLastError(0);const auto style=GetWindowLongPtrW(h,GWL_EXSTYLE);const auto read_error=GetLastError();
            if((!style&&read_error)||!set.owns(0))throw std::runtime_error("visual_ownership_app_style_read_or_identity_lost");
            style_attempted=true;SetLastError(0);const auto previous=SetWindowLongPtrW(h,GWL_EXSTYLE,style|WS_EX_LAYERED);
            style_error=GetLastError();style_ok=previous!=0||style_error==0;
        }
        if(!set.owns(0))throw std::runtime_error("visual_ownership_app_identity_changed_before_alpha");
        alpha_ok=false;if(style_ok){alpha_attempted=true;SetLastError(0);alpha_ok=SetLayeredWindowAttributes(h,0,*alpha,LWA_ALPHA)!=FALSE;alpha_error=GetLastError();}
        expected.layered=true;expected.known=true;expected.alpha=*alpha;expected.color=0;expected.flags=LWA_ALPHA;
    }
    if(top){
        if(!set.owns(0))throw std::runtime_error("visual_ownership_app_identity_changed_before_topmost");
        top_attempted=true;SetLastError(0);top_ok=SetWindowPos(h,*top?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)!=FALSE;
        top_error=GetLastError();expected.topmost=*top;
    }
    const bool observed=wait([&]{return set.owns(0)&&native_visual(h)==expected&&visible(h)==geometry&&geometry_history_positioning(h)==positioning;},1500);
    const auto actual=native_visual(h);
    s.log.check(label,style_ok&&alpha_ok&&top_ok&&observed,
        "\"actor\":\"test_application_not_product_writer\",\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
        ",\"actual_PID\":"+std::to_string(pid)+",\"actual_owner_TID\":"+std::to_string(tid)+",\"actual_caller_TID\":"+std::to_string(GetCurrentThreadId())+
        ",\"UI_thread_handling_claimed\":false,\"style_attempted\":"+(style_attempted?"true":"false")+",\"style_return_known_success\":"+(style_attempted?(style_ok?"true":"false"):"null")+
        ",\"style_error_raw\":"+(style_attempted?std::to_string(style_error):"null")+",\"alpha_attempted\":"+(alpha_attempted?"true":"false")+",\"alpha_returned\":"+(alpha_attempted?(alpha_ok?"true":"false"):"null")+
        ",\"alpha_error_raw\":"+(alpha_attempted?std::to_string(alpha_error):"null")+",\"topmost_attempted\":"+(top_attempted?"true":"false")+",\"topmost_returned\":"+(top_attempted?(top_ok?"true":"false"):"null")+
        ",\"topmost_error_raw\":"+(top_attempted?std::to_string(top_error):"null")+",\"before_alpha\":"+std::to_string(before->alpha)+",\"before_topmost\":"+(before->topmost?"true":"false")+
        ",\"actual_alpha\":"+(actual&&actual->known?std::to_string(actual->alpha):"null")+",\"actual_topmost\":"+(actual?(actual->topmost?"true":"false"):"null")+
        ",\"actual_layered\":"+(actual?(actual->layered?"true":"false"):"null")+",\"actual_color\":"+(actual&&actual->known?std::to_string(actual->color):"null")+
        ",\"actual_flags\":"+(actual&&actual->known?std::to_string(actual->flags):"null")+",\"independent_geometry_unchanged\":true");
}
std::string visual_ownership_command(LayoutFixtures& set,std::uint64_t key,const wchar_t* action,BYTE alpha,bool top){
    namespace e=product::guest::up_evidence_detail;
    auto& s=set.state;chain_front(set,0);const auto h=set.frames[0].hwnd;
    const auto before=visible(h),positioning=geometry_history_positioning(h);const auto mark=read_product(s).size();std::string command,mutation;
    const bool requested=launch(s,L"--command "+std::wstring(action),true);
    const bool observed=requested&&wait([&]{
        command=record(s,mark,"command");mutation=record(s,mark,"visual_mutation");
        const auto sequence=e::number(command,"\"sequence\":"),changed=e::number(mutation,"\"sequence\":");
        return geometry_history_receipt(s,command,"command")&&e::quoted(command,"\"action\":",utf8(action))&&e::number(command,"\"key\":")==key&&
            geometry_history_receipt(s,mutation,"visual_mutation")&&e::number(mutation,"\"key\":")==key&&e::number(mutation,"\"generation\":")==0&&
            e::unique_value(mutation,"\"native_attempted\":")=="true"&&e::unique_value(mutation,"\"native_completion_known\":")=="true"&&
            e::unique_value(mutation,"\"native_sequence_returned_success\":")=="true"&&e::unique_value(mutation,"\"cleanup_owned_property\":")=="false"&&
            e::unique_value(mutation,"\"visual_exact\":")=="true"&&e::unique_value(mutation,"\"recovery_commit_ack\":")=="true"&&
            sequence&&changed&&*sequence>*changed&&set.owns(0)&&native_visual_is(h,alpha,top);
    });
    s.log.check("visual_ownership_actual_command_consumed",observed&&before&&positioning&&visible(h)==before&&geometry_history_positioning(h)==positioning,
        "\"action\":"+quote(utf8(action))+",\"actual_command\":"+quote(command)+",\"actual_visual_receipt\":"+quote(mutation)+
        ",\"expected_alpha\":"+std::to_string(alpha)+",\"expected_topmost\":"+(top?"true":"false")+",\"geometry_unchanged\":true");
    (void)geometry_history_status(s,*e::number(command,"\"sequence\":"));return mutation;
}
std::string visual_ownership_pause_state(State& s,bool paused){
    namespace e=product::guest::up_evidence_detail;
    const auto mark=read_product(s).size();std::string status;
    const bool requested=launch(s,paused?L"--pause":L"--command Pause",true);
    const bool observed=requested&&wait([&]{status=record(s,mark,"status");return
        geometry_history_receipt(s,status,"status")&&product::guest::matching_idle_status(status,{s.product_pid,s.product_source,0,0})&&
        e::unique_value(status,"\"enabled\":")==(paused?"false":"true")&&e::number(status,"\"sequence\":").value_or(0)>0;});
    s.log.check("visual_ownership_actual_Pause_Resume_status",observed,"\"requested_paused\":"+(paused?std::string("true"):std::string("false"))+
        ",\"actual_status\":"+quote(status)+",\"secondary_exit_is_not_consumption\":true");return status;
}
void visual_ownership_pause_check(LayoutFixtures& set,std::uint64_t key,BYTE alpha,bool top,bool restore_alpha,bool restore_top,const char* label){
    namespace e=product::guest::up_evidence_detail;
    auto& s=set.state;const auto h=set.frames[0].hwnd;const auto geometry=visible(h),positioning=geometry_history_positioning(h);
    const auto mark=read_product(s).size();const auto status=visual_ownership_pause_state(s,true);
    unsigned mutations{};bool exact=true;std::string cleanup;
    std::istringstream rows(product_log_since(s,mark));std::string row;
    while(std::getline(rows,row))if(e::quoted(row,"\"type\":","visual_mutation")){
        ++mutations;cleanup=row;exact=exact&&geometry_history_receipt(s,row,"visual_mutation")&&e::number(row,"\"key\":")==key&&
            e::unique_value(row,"\"cleanup_owned_property\":")=="true"&&e::unique_value(row,"\"native_attempted\":")=="true"&&
            e::unique_value(row,"\"native_completion_known\":")=="true"&&e::unique_value(row,"\"native_sequence_returned_success\":")=="true"&&e::unique_value(row,"\"visual_exact\":")=="true"&&
            e::unique_value(row,"\"recovery_commit_ack\":")=="true"&&e::unique_value(row,"\"style_native_attempted\":")=="false"&&
            e::unique_value(row,"\"alpha_native_attempted\":")== (restore_alpha?"true":"false")&&
            e::unique_value(row,"\"zorder_native_attempted\":")== (restore_top?"true":"false")&&
            e::number(row,"\"sequence\":").value_or(UINT64_MAX)<e::number(status,"\"sequence\":").value_or(0);
    }
    const auto actual=native_visual(h);const unsigned expected_mutations=restore_alpha||restore_top?1u:0u;
    s.log.check(label,mutations==expected_mutations&&exact&&set.owns(0)&&geometry&&positioning&&visible(h)==geometry&&
        geometry_history_positioning(h)==positioning&&native_visual_is(h,alpha,top),
        "\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+",\"source_key\":"+std::to_string(key)+
        ",\"actual_cleanup_mutations\":"+std::to_string(mutations)+",\"actual_cleanup_receipt\":"+quote(cleanup)+
        ",\"actual_alpha\":"+(actual&&actual->known?std::to_string(actual->alpha):"null")+",\"actual_topmost\":"+(actual?(actual->topmost?"true":"false"):"null")+
        ",\"actual_status\":"+quote(status)+",\"cleanup_not_fixed_255\":true");
}
void visual_ownership_shake_negatives(LayoutFixtures& set){
    namespace e=product::guest::up_evidence_detail;
    auto& s=set.state;const auto h=set.frames[0].hwnd;const auto dpi=GetDpiForWindow(h);
    if(!dpi)throw std::runtime_error("visual_ownership_shake_negative_DPI_unknown");
    const LONG step=std::max<LONG>(48,MulDiv(48,static_cast<int>(dpi),96));LARGE_INTEGER frequency{};
    if(!QueryPerformanceFrequency(&frequency)||frequency.QuadPart<=0)throw std::runtime_error("visual_ownership_shake_negative_clock_unknown");
    for(const bool slow:std::array{true,false}){
        const auto before=visible(h);if(!before||!set.owns(0))throw std::runtime_error("visual_ownership_shake_negative_source_unknown");
        const auto g=layout_down(set,0,HTCAPTION,false);LONG dx{};std::uint64_t prior_qpc{};
        const unsigned samples=slow?5u:3u;
        for(unsigned index=0;index<samples;++index){
            if(slow){const auto deadline=GetTickCount64()+1000;
                if(!wait([&]{return GetTickCount64()>=deadline;},1300))throw std::runtime_error("visual_ownership_slow_sample_interval_not_established");}
            dx=slow?(index%2?-step:step):(index==1?2*step:step);
            layout_phase(set,0,g,dx,0,{translated(*before,dx,0)},slow?"visual_ownership_slow_alternating_actual_Move":"visual_ownership_single_reversal_actual_Move");
            if(slow){const auto placement=record(s,g.offset,"placement",g.generation);const auto qpc=e::number(placement,"\"hook_observed_qpc\":");
                const bool actual_time=geometry_history_receipt(s,placement,"placement")&&qpc&&*qpc&&
                    (!prior_qpc||(*qpc>prior_qpc&&*qpc-prior_qpc>static_cast<std::uint64_t>(frequency.QuadPart)*701/1000));
                s.log.check("visual_ownership_slow_actual_hook_interval_exceeds_window",actual_time,
                    "\"actual_placement\":"+quote(placement)+",\"previous_hook_QPC\":"+std::to_string(prior_qpc)+",\"window_ms\":700");prior_qpc=*qpc;}
        }
        layout_up(set,g,{translated(*before,dx,0)});unsigned mutations{};std::istringstream rows(product_log_since(s,g.offset));std::string row;
        while(std::getline(rows,row))if(e::quoted(row,"\"type\":","visual_mutation"))++mutations;
        s.log.check(slow?"visual_ownership_slow_alternation_no_shake":"visual_ownership_single_reversal_no_shake",
            !mutations&&set.owns(0)&&native_visual_is(h,173,false),"\"generation\":"+std::to_string(g.generation)+
            ",\"actual_visual_mutations\":"+std::to_string(mutations)+",\"actual_NormalUp_and_resources_required_by_shared_layout_up\":true");
    }
}
void owned_visual_ownership_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if(s.suite!="VisualOwnershipOnly"||!original.enabled||s.left||s.control)throw std::runtime_error("visual_ownership_exact_released_guest_suite_required");
    auto config=original;config.drag_transparency=false;config.shake=false;config.magnet_move=false;config.magnet_resize=false;config.glue=false;config.docking=false;config.preview=false;
    auto applied=chain_gui_save(s,config);LayoutFixtures set{s};
    try{
        set.create();const auto work=chain_work(set.frames[0].hwnd);if(work.width()<1100||work.height()<700)throw std::runtime_error("visual_ownership_owned_work_area_too_small");
        const Rect source{work.l+280,work.t+180,work.l+680,work.t+480};const LayoutLimit limit{120,100,work.width(),work.height()};set.setup({source},{limit});
        visual_ownership_app_change(set,BYTE{173},false,"visual_ownership_app_non255_original_tuple");const auto key=geometry_history_source_key(set,source);
        (void)visual_ownership_command(set,key,L"OpacityDown",141,false);(void)visual_ownership_command(set,key,L"ToggleTopmost",141,true);
        visual_ownership_app_change(set,BYTE{201},std::nullopt,"visual_ownership_app_changes_only_alpha_201");
        visual_ownership_pause_check(set,key,201,false,false,true,"visual_ownership_app_alpha_retained_product_top_restored");
        (void)visual_ownership_pause_state(s,false);
        visual_ownership_app_change(set,BYTE{173},false,"visual_ownership_second_app_non255_original_tuple");
        (void)visual_ownership_command(set,key,L"OpacityDown",141,false);(void)visual_ownership_command(set,key,L"ToggleTopmost",141,true);
        visual_ownership_app_change(set,std::nullopt,false,"visual_ownership_app_changes_only_topmost");
        visual_ownership_pause_check(set,key,173,false,true,false,"visual_ownership_app_top_retained_product_alpha_restored");
        (void)visual_ownership_pause_state(s,false);
        config.shake=true;config.shake_amplitude_dip=24;config.shake_reversals=3;config.shake_window_ms=700;applied=chain_gui_save(s,config);
        visual_ownership_shake_negatives(set);config.shake=false;applied=chain_gui_save(s,config);
        visual_ownership_app_change(set,BYTE{173},false,"visual_ownership_closing_source_original_tuple");
        (void)visual_ownership_command(set,key,L"OpacityDown",141,false);const auto mutation=visual_ownership_command(set,key,L"ToggleTopmost",141,true);
        const auto helper_pid=e::number(mutation,"\"helper_pid\":");
        Handle helper(helper_pid&&*helper_pid&&*helper_pid<=MAXDWORD?OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,static_cast<DWORD>(*helper_pid)):nullptr);
        const auto helper_birth=helper.value?process_birth(helper.value):0;std::array<wchar_t,32768> image{};DWORD length=static_cast<DWORD>(image.size());
        const bool image_read=helper.value&&QueryFullProcessImageNameW(helper.value,0,image.data(),&length);
        Handle processes(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));PROCESSENTRY32W entry{sizeof(entry)};bool parent_known{};DWORD parent{};
        if(processes.value!=INVALID_HANDLE_VALUE&&Process32FirstW(processes.value,&entry))do{if(helper_pid&&entry.th32ProcessID==*helper_pid){parent=entry.th32ParentProcessID;parent_known=true;break;}}while(Process32NextW(processes.value,&entry));
        s.log.check("visual_ownership_actual_helper_identity_before_source_close",helper.value&&helper_birth>=birth&&helper_birth&&image_read&&parent_known&&parent==pid&&
            *helper_pid!=pid&&*helper_pid!=GetCurrentProcessId()&&WaitForSingleObject(helper.value,0)==WAIT_TIMEOUT&&medium(helper.value)&&
            exit_recovery_same_user_session(s.process,helper.value)&&equal(image.data(),s.product.wstring())&&hash(image.data())==binary,
            "\"actual_helper_PID\":"+(helper_pid?std::to_string(*helper_pid):"null")+",\"actual_helper_birth\":"+std::to_string(helper_birth)+
            ",\"actual_parent_PID\":"+(parent_known?std::to_string(parent):"null")+",\"held_handle_rights\":\"QUERY_LIMITED_INFORMATION|SYNCHRONIZE\"");
        const auto old_hwnd=set.frames[0].hwnd;const auto old_frame=reinterpret_cast<std::uintptr_t>(&set.frames[0]);
        s.log.check("visual_ownership_old_source_actually_closed",set.close()&&!IsWindow(old_hwnd));
        LayoutFixtures fresh{s};fresh.create();const Rect next{work.r-440,work.t+160,work.r-40,work.t+460};fresh.setup({next},{limit});
        visual_ownership_app_change(fresh,BYTE{141},true,"visual_ownership_new_root_matches_old_applied_tuple_not_old_identity");
        const auto new_key=geometry_history_source_key(fresh,next);DWORD actual_pid{};const auto tid=GetWindowThreadProcessId(fresh.frames[0].hwnd,&actual_pid);
        s.log.check("visual_ownership_fresh_Frame_root_and_key_not_PID_class_authority",new_key!=key&&fresh.owns(0)&&actual_pid==GetCurrentProcessId()&&tid==set.thread&&
            gui_class(fresh.frames[0].hwnd)==s.class_name&&reinterpret_cast<std::uintptr_t>(&fresh.frames[0])!=old_frame,
            "\"old_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(old_hwnd))+",\"new_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(fresh.frames[0].hwnd))+
            ",\"old_product_key\":"+std::to_string(key)+",\"new_product_key\":"+std::to_string(new_key)+",\"OS_HWND_value_reused\":"+(old_hwnd==fresh.frames[0].hwnd?"true":"false"));
        visual_ownership_pause_check(fresh,new_key,141,true,false,false,"visual_ownership_Pause_does_not_restore_old_tuple_to_new_root");
        (void)visual_ownership_pause_state(s,false);const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        s.log.check("visual_ownership_actual_original_configuration_restored",restored==expected);chain_check_product(s,pid,birth);
        s.log.check("visual_ownership_same_product_binary_before_normal_Exit",hash(s.product)==binary&&binary==hash(s.input_product));
        const auto stop_mark=read_product(s).size();cleanup(s);const auto shutdown=record(s,stop_mark,"shutdown");DWORD helper_exit=STILL_ACTIVE;
        const bool helper_stopped=WaitForSingleObject(helper.value,5000)==WAIT_OBJECT_0&&GetExitCodeProcess(helper.value,&helper_exit);
        s.log.check("visual_ownership_normal_Exit_helper_stopped_new_root_untouched",!s.process&&geometry_history_receipt(s,shutdown,"shutdown")&&
            e::unique_value(shutdown,"\"visual_recovery_stopped\":")=="true"&&e::unique_value(shutdown,"\"visual_restore_confirmed\":")=="true"&&
            e::unique_value(shutdown,"\"native_worker_exited\":")=="true"&&input_resource_stop_observed(shutdown)&&helper_stopped&&helper_exit==0&&
            fresh.owns(0)&&native_visual_is(fresh.frames[0].hwnd,141,true)&&visible(fresh.frames[0].hwnd)==next,
            "\"actual_shutdown\":"+quote(shutdown)+",\"actual_helper_PID\":"+std::to_string(*helper_pid)+",\"actual_helper_exit\":"+(helper_stopped?std::to_string(helper_exit):"null")+
            ",\"new_root_expected_alpha\":141,\"new_root_expected_topmost\":true,\"guest_destruction_used\":false");
        pause_routing_client_pair(fresh,"visual_ownership_actual_new_root_client_after_Exit");
        s.log.check("visual_ownership_exact_new_roots_closed",fresh.close());
        s.log.emit("visual_ownership_component","\"result\":\"PASS\",\"independent_app_alpha_top_ownership\":true,\"closed_new_root_no_restore\":true,"+
            std::string("\"slow_and_single_reversal_no_shake\":true,\"two_shake_restore_old_evidence_not_rerun\":true,\"system_Shake_coexistence\":\"NOT_RUN\",\"whole_M09_or_version_PASS\":false,\"human_uat\":\"NOT_RUN\""));
    }catch(...){pause_routing_failed_case_input(s);try{if(!set.closed)set.close();}catch(...){}
        try{if(s.process&&chain_settings(s)==applied&&!s.left&&!s.control)chain_gui_save(s,original);}catch(...){}throw;}
}
