#pragma once

// TEST ONLY. One native capability control before product startup, followed
// by the existing positive-shake DOWN/phase/UP path. No system setting writes.
struct GuestSystemShakeSetting {
    LSTATUS result{ERROR_FILE_NOT_FOUND};DWORD type{},bytes{sizeof(DWORD)},value{};
    static GuestSystemShakeSetting read(const wchar_t* path,const wchar_t* name){
        GuestSystemShakeSetting out;
        out.result=RegGetValueW(HKEY_CURRENT_USER,path,name,RRF_RT_REG_DWORD,&out.type,&out.value,&out.bytes);
        return out;
    }
    bool explicit_one()const{return result==ERROR_SUCCESS&&type==REG_DWORD&&bytes==sizeof(DWORD)&&value==1;}
    std::string json()const{
        const bool known=result==ERROR_SUCCESS&&type==REG_DWORD&&bytes==sizeof(DWORD);
        return "{\"read_result\":"+std::to_string(result)+",\"type\":"+std::to_string(type)+
            ",\"bytes\":"+std::to_string(bytes)+",\"known_DWORD\":"+(known?"true":"false")+
            ",\"value\":"+(known?std::to_string(value):"null")+"}";
    }
};
struct GuestSystemShakeSettings {
    GuestSystemShakeSetting policy=GuestSystemShakeSetting::read(
        L"Software\\Policies\\Microsoft\\Windows\\Explorer",L"NoWindowMinimizingShortcuts");
    GuestSystemShakeSetting preference=GuestSystemShakeSetting::read(
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",L"DisallowShaking");
    std::string json()const{return "{\"HKCU_NoWindowMinimizingShortcuts\":"+policy.json()+
        ",\"HKCU_DisallowShaking_raw_preference\":"+preference.json()+
        ",\"missing_value_implies_enabled\":false,\"raw_preference_is_not_Settings_UI_proof\":true}";}
};
void system_shake_scope(State& s,bool product_running){
    std::array<wchar_t,128> user{};DWORD count=static_cast<DWORD>(user.size());
    if(s.suite!="SystemShakeCoexistenceOnly"||!desktop()||!GetUserNameW(user.data(),&count)||
       !equal(user.data(),L"WDAGUtilityAccount")||!medium(GetCurrentProcess())||
       !equal(s.output.wstring(),L"C:\\PaneBindV1\\Output")||!marker(s.output/L"panebind-v1-output.marker",s.run)||
       s.run.size()!=32||!std::all_of(s.run.begin(),s.run.end(),[](wchar_t c){return(c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');})||
       !std::all_of(s.frames.begin(),s.frames.end(),[&](const auto& f){return own(s,f.hwnd);}))
        throw std::runtime_error("system_shake_exact_Medium_disposable_guest_and_original_roots_required");
    if(product_running)settings_guest_scope(s);
    else if(s.process||s.product_pid||FindWindowW(L"PaneBind.V1.Application",L"PaneBind"))
        throw std::runtime_error("system_shake_native_control_requires_product_never_started_not_paused");
}
std::array<bool,2> system_shake_companions(State& s,const char* stage){
    std::array<bool,2> minimized{};std::string rows="[";
    for(std::size_t n=1;n<3;++n){const HWND h=s.frames[n].hwnd;
        if(!own(s,h))throw std::runtime_error("system_shake_companion_identity_lost");
        minimized[n-1]=IsIconic(h)!=FALSE;
        if(n>1)rows+=',';rows+="{\"member\":"+std::to_string(n)+",\"HWND\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(h))+",\"exact_owned\":true,\"IsIconic\":"+
            (minimized[n-1]?"true":"false")+",\"IsWindowVisible\":"+(IsWindowVisible(h)?"true":"false")+"}";
    }
    rows+=']';s.log.emit("system_shake_companion_readback","\"stage\":"+quote(stage)+",\"members\":"+rows);
    return minimized;
}
void system_shake_layout(State& s){
    MONITORINFO m{sizeof(m)};const HWND source=s.frames[0].hwnd;
    if(!own(s,source)||!GetMonitorInfoW(MonitorFromWindow(source,MONITOR_DEFAULTTONULL),&m)||
       m.rcWork.right-m.rcWork.left<1100||m.rcWork.bottom-m.rcWork.top<760)
        throw std::runtime_error("system_shake_actual_work_area_cannot_fit_bounded_trajectory");
    const LONG x=m.rcWork.left+(m.rcWork.right-m.rcWork.left-400)/2;
    const LONG y=m.rcWork.top+(m.rcWork.bottom-m.rcWork.top-300)/2;
    const std::array<Rect,3> expected{{{x,y,x+400,y+300},
        {m.rcWork.left+24,m.rcWork.top+24,m.rcWork.left+424,m.rcWork.top+324},
        {m.rcWork.left+24,m.rcWork.bottom-324,m.rcWork.left+424,m.rcWork.bottom-24}}};
    for(std::size_t n=0;n<3;++n){const HWND h=s.frames[n].hwnd;
        if(!own(s,h))throw std::runtime_error("system_shake_layout_identity_lost");
        s.log.check("system_shake_independent_owned_layout",!IsIconic(h)&&fixture_rect(h,expected[n]),
            "\"member\":"+std::to_string(n)+",\"actual_expected\":"+rect(expected[n])+",\"fixture_setup_not_product_placement\":true");
    }
}
bool system_shake_native_control(State& s,std::string& configuration_observed){
    system_shake_scope(s,false);
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    unsigned visited{},matches{};bool complete=false;std::string pids="[";
    if(snapshot.value!=INVALID_HANDLE_VALUE&&Process32FirstW(snapshot.value,&entry))do{
        if(++visited>4096)break;
        if(equal(entry.szExeFile,L"panebind.exe")){if(matches++)pids+=',';pids+=std::to_string(entry.th32ProcessID);}
        SetLastError(ERROR_SUCCESS);
        if(!Process32NextW(snapshot.value,&entry)){complete=GetLastError()==ERROR_NO_MORE_FILES;break;}
    }while(true);
    pids+=']';
    s.log.check("system_shake_native_no_product_process_before_input",complete&&matches==0,
        "\"actual_bounded_process_snapshot_complete\":"+std::string(complete?"true":"false")+
        ",\"matching_product_PIDs\":"+pids+",\"this_flow_product_create_requests\":0,\"product_pause_not_used\":true");
    const GuestSystemShakeSettings settings;
    configuration_observed=settings.json();
    s.log.emit("system_shake_readonly_configuration","\"settings\":"+settings.json()+
        ",\"system_setting_write_attempted\":false,\"product_launched\":false");
    if(settings.policy.explicit_one()||settings.preference.explicit_one()){
        s.log.emit("system_shake_capability","\"result\":\"BLOCKED_DISABLED\",\"disabled_by_policy\":"+
            std::string(settings.policy.explicit_one()?"true":"false")+",\"raw_preference_reports_disabled\":"+
            (settings.preference.explicit_one()?"true":"false")+",\"native_input_sent\":false,\"product_result\":\"NOT_RUN\"");
        return false;
    }
    system_shake_layout(s);const auto before=system_shake_companions(s,"native_before_DOWN");
    s.log.check("system_shake_native_companions_initially_not_minimized",!before[0]&&!before[1]);
    const HWND h=s.frames[0].hwnd;foreground(s,h);
    const auto point=hit_point(h,HTCAPTION);const auto start=visible(h);
    s.log.check("system_shake_native_real_caption_hit",point&&start&&own(s,h)&&GetForegroundWindow()==h&&WindowFromPoint(*point)==h);
    cursor(s,*point);const auto starts=s.frames[0].starts.load(),ends=s.frames[0].ends.load();
    const auto offsets=positive_shake_offsets(h);bool actual_minimization=false,UP_attempted=false;
    GuestUpHookObservation observer;
    try{
        button(s,true);
        for(std::size_t n=0;n<offsets.size();++n){
            system_shake_scope(s,false);
            if(!s.left||!(GetAsyncKeyState(VK_LBUTTON)&0x8000)||GetForegroundWindow()!=h)
                throw std::runtime_error("system_shake_native_held_or_foreground_context_lost");
            Sleep(60);cursor(s,{point->x+offsets[n],point->y});
            GUITHREADINFO gui{sizeof(gui)};
            const bool actual=wait([&]{return own(s,h)&&GetForegroundWindow()==h&&
                s.frames[0].starts.load()==starts+1&&GetGUIThreadInfo(s.owner,&gui)&&
                gui.hwndMoveSize==h&&gui.hwndCapture==h&&visible(h).has_value();},1000);
            s.log.check("system_shake_native_actual_Move_sample",actual,
                "\"sample\":"+std::to_string(n)+",\"raw_DOWN_anchor\":["+std::to_string(point->x)+","+std::to_string(point->y)+
                "],\"total_dx\":"+std::to_string(offsets[n])+",\"actual_visible\":"+(visible(h)?rect(*visible(h)):"null")+
                ",\"cursor_intent_from_original_DOWN\":"+rect(translated(*start,offsets[n],0))+
                ",\"native_live_geometry_not_assumed_when_full_window_drag_setting_unknown\":true"+
                ",\"actual_native_START_count\":"+std::to_string(s.frames[0].starts.load()-starts)+
                ",\"actual_capture\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndCapture))+
                ",\"actual_move_size\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndMoveSize))+
                ",\"product_was_never_launched\":true,\"input_pause_ms\":60");
            const auto iconic=system_shake_companions(s,"native_held_sample");
            actual_minimization=actual_minimization||(iconic[0]&&iconic[1]);
        }
        s.log.check("system_shake_native_unique_UP_observer_ready",observer.start());
        UP_attempted=true;button(s,false);
        GUITHREADINFO after{sizeof(after)};
        const bool ended=wait([&]{return own(s,h)&&s.frames[0].ends.load()==ends+1&&
            GetGUIThreadInfo(s.owner,&after)&&!after.hwndCapture&&!after.hwndMoveSize;},1000);
        const bool unhooked=observer.stop();const auto hook=observer.json();
        s.log.check("system_shake_native_actual_UP_END_and_release",ended&&unhooked&&
            hook.find("\"actual_left_UP_callbacks\":1,")!=hook.npos&&
            (number(hook,"\"last_flags\":")&LLMHF_INJECTED)!=0&&
            number(hook,"\"last_extra_information\":")==0x50425631&&
            number(hook,"\"last_qpc\":")>=s.left_up_send_qpc&&s.left_up_send_qpc&&
            hook.find("\"CallNext_returned\":true")!=hook.npos&&hook.find("\"CallNext_result\":0,")!=hook.npos&&
            s.frames[0].native_end_qpc.load()>=s.left_up_send_qpc&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000),
            "\"single_planned_UP\":true,\"actual_UP_hook\":"+hook+",\"native_END_count\":"+
            std::to_string(s.frames[0].ends.load()-ends)+",\"not_product_NormalUp\":true");
        if(!actual_minimization)actual_minimization=wait([&]{
            if(!own(s,s.frames[1].hwnd)||!own(s,s.frames[2].hwnd))throw std::runtime_error("system_shake_companion_identity_lost");
            return IsIconic(s.frames[1].hwnd)&&IsIconic(s.frames[2].hwnd);},1000);
        const auto iconic=system_shake_companions(s,"native_after_UP");
        actual_minimization=actual_minimization||(iconic[0]&&iconic[1]);
        for(std::size_t n=1;n<3;++n){const HWND companion=s.frames[n].hwnd;
            if(!own(s,companion))throw std::runtime_error("system_shake_restore_not_exact_companion");
            const bool attempted=IsIconic(companion)!=FALSE;
            const bool requested=!attempted||ShowWindowAsync(companion,SW_RESTORE)!=FALSE;
            const bool restored=requested&&wait([&]{return own(s,companion)&&!IsIconic(companion)&&IsWindowVisible(companion);},1000);
            s.log.check("system_shake_test_owned_companion_restore",restored,
                "\"member\":"+std::to_string(n)+",\"native_restore_attempted\":"+(attempted?"true":"false")+
                ",\"request_return\":"+(requested?"true":"false")+",\"actor\":\"test_only_exact_owned_setup\",\"not_product_cleanup\":true");
        }
    }catch(...){
        if(s.left&&!UP_attempted){try{UP_attempted=true;button(s,false);}catch(...){};}
        s.log.emit("system_shake_native_control_aborted","\"original_failure_preserved\":true,\"product_result\":\"NOT_RUN\"");
        throw;
    }
    s.log.emit("system_shake_capability","\"result\":"+quote(actual_minimization?"AVAILABLE_ACTUAL_NATIVE_EFFECT":"BLOCKED_NOT_OBSERVED_UNKNOWN")+
        ",\"two_exact_companions_observed_minimized\":"+(actual_minimization?"true":"false")+
        ",\"absence_of_effect_is_not_proof_of_permanent_Sandbox_incompatibility\":true,\"no_retry\":true");
    return actual_minimization;
}
void owned_system_shake_coexistence_case(State& s,const std::string& native_configuration){
    system_shake_scope(s,true);
    const auto original_settings=chain_settings(s);auto desired=original_settings;
    desired.attraction_dip=0;desired.drag_transparency=false;desired.shake=true;desired.shake_alpha=190;
    desired.shake_amplitude_dip=24;desired.shake_reversals=3;desired.shake_window_ms=2000;
    visual_gui_save(s,desired);system_shake_layout(s);
    const HWND h=s.frames[0].hwnd;own_visual_setup(s,h,173,false);
    const auto offsets=positive_shake_offsets(h);const auto pid=s.product_pid;
    const auto settings_before=GuestSystemShakeSettings{}.json();
    s.log.check("system_shake_native_and_product_configuration_equal",settings_before==native_configuration,
        "\"native_configuration\":"+native_configuration+",\"product_configuration\":"+settings_before);
    for(unsigned turn=0;turn<2;++turn){
        const auto start=visible(h);if(!start)throw std::runtime_error("system_shake_source_geometry_unknown");
        auto g=down(s,0);unsigned placements_observed{};
        for(std::size_t n=0;n<offsets.size();++n){
            phase(s,0,g,offsets[n],0,translated(*start,offsets[n],0),"system_shake_product_real_positive_sample",true);++placements_observed;
            const auto iconic=system_shake_companions(s,"product_held_after_exact_placement");
            s.log.check("system_shake_no_native_minimizing_side_effect",!iconic[0]&&!iconic[1],
                "\"generation\":"+std::to_string(g.generation)+",\"same_product_PID\":"+std::to_string(pid));
        }
        const BYTE alpha=turn?173:190;const bool top=turn==0;
        visual_exact(s,h,alpha,top,turn?"system_shake_second_gesture_restores_original173_tuple":"system_shake_first_gesture_sets190_topmost");
        const auto source_key=number(record(s,g.offset,"gesture_start",g.generation),"\"key\":");
        const auto mutations=[&]{unsigned count{};std::istringstream rows(product_log_since(s,g.offset));std::string row;
            while(std::getline(rows,row))if(row.find("\"type\":\"visual_mutation\"")!=row.npos&&
                number(row,"\"process_id\":")==pid&&number(row,"\"key\":")==source_key&&
                number(row,"\"generation\":")==g.generation&&row.find("\"native_attempted\":true")!=row.npos&&
                row.find("\"native_completion_known\":true")!=row.npos&&row.find("\"visual_exact\":true")!=row.npos)++count;
            return count;};
        const bool actual_mutations=source_key&&wait([&]{return mutations()>=2;},1000);
        s.log.check("system_shake_one_real_tuple_toggle_per_gesture",placements_observed==5&&actual_mutations&&mutations()==2,
            "\"generation\":"+std::to_string(g.generation)+",\"key\":"+std::to_string(source_key)+
            ",\"actual_native_visual_mutations\":"+std::to_string(mutations()));
        up(s,0,g,translated(*start,offsets.back(),0));
        visual_exact(s,h,alpha,top,"system_shake_tuple_preserved_after_real_NormalUp");
        s.log.check("system_shake_no_extra_visual_mutation_after_NormalUp",mutations()==2);
        const auto iconic=system_shake_companions(s,"product_after_NormalUp");
        s.log.check("system_shake_NormalUp_did_not_minimize_companions",!iconic[0]&&!iconic[1]);
        independent_entry_ready(s,"system_shake.after_NormalUp",false);
    }
    const auto settings_after=GuestSystemShakeSettings{}.json();
    s.log.check("system_shake_readonly_configuration_unchanged",settings_before==settings_after,
        "\"settings_before\":"+settings_before+",\"settings_after\":"+settings_after+",\"writes_attempted\":false");
    visual_gui_save(s,original_settings);
}
void system_shake_coexistence_flow(State& s){
    s.log.emit("actual_suite","\"suite\":\"SystemShakeCoexistenceOnly\",\"native_control_before_product_startup\":true,\"Full_is_default\":true");
    std::string native_configuration;const bool available=system_shake_native_control(s,native_configuration);
    if(!available){s.result=78;s.log.emit("system_shake_coexistence_summary",
        "\"result\":\"BLOCKED\",\"actual_native_control_available\":false,\"product_result\":\"NOT_RUN\",\"M09_F05_PASS\":false,\"whole_version_PASS\":false,\"human_uat\":\"NOT_RUN\"");return;}
    s.log.check("production_create_no_window",launch(s,L"--diagnostic",false),"\"creation_flags\":134217728");
    s.log.check("production_gui_startup",wait([&]{return number(record(s,0,"startup"),"\"process_id\":")==s.product_pid;},8000));
    const auto route=record(s,0,"input_route");
    s.log.check("system_shake_actual_product_capture_route",route.find("\"guest_consumed_down_capture\":true")!=route.npos,"\"actual_route\":"+quote(route));
    independent_entry_ready(s,"system_shake.before_product_gestures",true);
    owned_system_shake_coexistence_case(s,native_configuration);cleanup(s);s.result=s.log.failed?1:0;
    s.log.emit("system_shake_coexistence_summary","\"result\":"+quote(s.log.failed?"FAIL":"PASS_SCOPED")+
        ",\"actual_native_control_available\":true,\"same_product_two_real_shake_gestures\":true,\"native_control_is_not_product_proof\":true,\"Full_suite\":\"NOT_RUN\",\"whole_version_PASS\":false,\"human_uat\":\"NOT_RUN\"");
}
