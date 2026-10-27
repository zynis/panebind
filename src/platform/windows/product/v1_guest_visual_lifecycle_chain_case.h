#pragma once

// One existing I06 sequence plus its same-instance final Exit. Include after
// product_chains/geometry_history and the shared GUI/cleanup helpers. No new
// product API, alternative input path or old ownership-suite replay.
void owned_visual_lifecycle_chain_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);
    if(s.suite!="VisualLifecycleChainOnly"||s.left||s.control||!chain_settings(s).enabled)
        throw std::runtime_error("I06_exact_released_enabled_owned_guest_required");
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    chain_check_product(s,pid,birth);
    Handle held;SetLastError(ERROR_SUCCESS);
    const bool duplicated=DuplicateHandle(GetCurrentProcess(),s.process,GetCurrentProcess(),&held.value,
        PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,0)!=FALSE;
    const DWORD duplicate_error=duplicated?ERROR_SUCCESS:GetLastError();
    s.log.check("I06_exact_product_handle_retained",duplicated&&birth&&GetProcessId(held.value)==pid&&
        process_birth(held.value)==birth&&WaitForSingleObject(held.value,0)==WAIT_TIMEOUT&&!binary.empty()&&binary==hash(s.input_product),
        "\"actual_product_PID\":"+std::to_string(pid)+",\"actual_product_birth\":"+std::to_string(birth)+
        ",\"product_sha256\":"+quote(binary)+",\"DuplicateHandle_return\":"+(duplicated?"true":"false")+
        ",\"DuplicateHandle_error_on_failure\":"+(duplicated?"null":std::to_string(duplicate_error)));
    const HWND source=s.frames[0].hwnd;std::array<Rect,3> initial{};std::array<bool,3> shown{};
    for(std::size_t n=0;n<3;++n){const auto actual=visible(s.frames[n].hwnd);
        if(s.explorer[n]||s.applications[n]||!own(s,s.frames[n].hwnd)||!actual)
            throw std::runtime_error("I06_only_original_exact_owned_roots_required");
        initial[n]=*actual;shown[n]=IsWindowVisible(s.frames[n].hwnd)!=FALSE;
        if(n)ShowWindow(s.frames[n].hwnd,SW_HIDE);
    }
    const auto work=chain_work(source);
    if(work.width()<1000||work.height()<700)throw std::runtime_error("I06_actual_owned_work_area_too_small");
    const Rect base{work.l+160,work.t+140,work.l+560,work.t+440};
    s.log.check("I06_exact_owned_test_layout",own(s,source)&&fixture_rect(source,base),
        "\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+",\"test_setup_not_product_placement\":true");
    const auto start_mark=read_product(s).size();bool final_exit_completed=false;
    chain_i06_owned_core(s,[&](HWND actual_source,const panebind::core::v1::Settings& original,const NativeVisual& fixture_original){
        chain_check_product(s,pid,birth);
        s.log.check("I06_existing_sequence_three_actual_NormalUp",actual_source==source&&own(s,source)&&!s.left&&!s.control&&[&]{
            unsigned count{};std::vector<std::uint64_t> generations;std::istringstream rows(product_log_since(s,start_mark));std::string row;
            while(std::getline(rows,row))if(geometry_history_receipt(s,row,"gesture_end")){
                const auto generation=e::number(row,"\"generation\":");
                if(!generation||!*generation||e::unique_value(row,"\"normal_up\":")!="true"||e::unique_value(row,"\"end\":")!="0"||
                    e::unique_value(row,"\"pending_retired\":")!="true"||actual_up(s,start_mark,*generation).empty()||
                    std::find(generations.begin(),generations.end(),*generation)!=generations.end())return false;
                generations.push_back(*generation);++count;
            }return count==3;
        }(),"\"same_product_PID\":"+std::to_string(pid)+",\"same_product_birth\":"+std::to_string(birth)+
            ",\"shared_up_requires_actual_matching_UP_and_resource_cleanup\":true,\"operation_order\":\"DragGlass_UP_Shake_UP_Shake_UP_Pause_Resume\"");
        visual_exact(s,source,173,false,"I06_before_configuration_restore_actual_baseline_tuple");
        // Restore the configuration through the real GUI, not the fixture's
        // alpha/topmost tuple. Exit must preserve the actual app baseline.
        const auto restored=chain_gui_save(s,original);auto expected_config=original;expected_config.generation=restored.generation;
        s.log.check("I06_original_configuration_restored_before_same_instance_Exit",restored==expected_config);
        visual_exact(s,source,173,false,"I06_configuration_restore_did_not_replace_app_tuple");
        chain_idle_facts(s);chain_check_product(s,pid,birth);
        const HWND preview=chain_preview(s);DWORD preview_pid{};const auto preview_tid=preview?GetWindowThreadProcessId(preview,&preview_pid):0;
        s.log.check("I06_actual_preview_hidden_before_Exit",preview&&preview_tid&&preview_pid==pid&&!IsWindowVisible(preview));
        const auto stop_mark=read_product(s).size();const auto driver_mark=s.log.mark();
        cleanup(s); // Existing real public --quit, held exit and client check.
        DWORD code=STILL_ACTIVE;const bool exited=WaitForSingleObject(held.value,0)==WAIT_OBJECT_0;
        const bool code_known=exited&&GetExitCodeProcess(held.value,&code)!=FALSE;
        const auto shutdown=record(s,stop_mark,"shutdown");std::string stopped;
        {std::istringstream rows(s.log.since(driver_mark));std::string row;
            while(std::getline(rows,row))if(row.ends_with('}')&&row.find("\"type\":\"product_stop\"")!=row.npos)stopped=row;}
        GUITHREADINFO native{sizeof(native)};
        const bool source_idle=own(s,source)&&GetGUIThreadInfo(s.owner,&native)&&!native.hwndCapture&&!native.hwndMoveSize&&!native.hwndMenuOwner&&
            !(native.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE));
        s.log.check("I06_same_product_normal_Exit_actual_resources_and_tuple",!s.process&&exited&&code_known&&code==0&&process_birth(held.value)==birth&&
            e::unique_value(stopped,"\"quit_command_completed\":")=="true"&&e::unique_value(stopped,"\"process_exit_observed\":")=="true"&&
            e::unique_value(stopped,"\"product_exit_code\":")=="0"&&geometry_history_receipt(s,shutdown,"shutdown")&&
            e::unique_value(shutdown,"\"own_hooks_stopped\":")=="true"&&e::unique_value(shutdown,"\"own_overlay_destroyed\":")=="true"&&
            e::unique_value(shutdown,"\"native_worker_exited\":")=="true"&&e::unique_value(shutdown,"\"issued_native_completion_unknown\":")=="false"&&
            e::unique_value(shutdown,"\"visual_recovery_stopped\":")=="true"&&e::unique_value(shutdown,"\"visual_restore_confirmed\":")=="true"&&
            e::unique_value(shutdown,"\"own_input_resources_stopped\":")=="true"&&input_resource_stop_observed(shutdown)&&
            !IsWindow(preview)&&source_idle&&!s.left&&!s.control&&native_visual_is(source,173,false)&&hash(s.product)==binary,
            "\"same_product_PID\":"+std::to_string(pid)+",\"same_product_birth\":"+std::to_string(birth)+
            ",\"held_process_exit_observed\":"+(exited?"true":"false")+",\"actual_exit_code\":"+(code_known?std::to_string(code):"null")+
            ",\"actual_shutdown\":"+quote(shutdown)+",\"actual_public_quit_receipt\":"+quote(stopped)+
            ",\"preview_window_destroyed\":"+(!IsWindow(preview)?"true":"false")+",\"actual_source_GUI_idle\":"+(source_idle?"true":"false")+
            ",\"expected_app_alpha\":173,\"expected_app_topmost\":false,\"app_tuple_not_manually_restored_before_Exit\":true,\"guest_destruction_used\":false");
        final_exit_completed=true;
        // Test-fixture restoration happens only AFTER the independent Exit
        // verdict. It cannot satisfy the product's visual-restoration oracle.
        chain_restore_app_visual(s,source,fixture_original);
    });
    s.log.check("I06_final_callback_completed_without_product_restart",final_exit_completed&&!s.process&&WaitForSingleObject(held.value,0)==WAIT_OBJECT_0);
    for(std::size_t n=0;n<3;++n){s.log.check("I06_post_Exit_test_fixture_geometry_restored",own(s,s.frames[n].hwnd)&&fixture_rect(s.frames[n].hwnd,initial[n]));
        ShowWindow(s.frames[n].hwnd,shown[n]?SW_SHOWNOACTIVATE:SW_HIDE);}
    s.log.emit("I06_visual_lifecycle_chain_complete","\"result\":\"OWNED_VISUAL_SEQUENCE_AND_SAME_INSTANCE_NORMAL_EXIT_OBSERVED\","
        "\"same_product_PID\":"+std::to_string(pid)+",\"same_product_birth\":"+std::to_string(birth)+
        ",\"existing_visual_sequence_runs\":1,\"product_restarted\":false,\"old_ownership_suite_replayed\":false,\"system_Shake_coexistence\":\"NOT_RUN\","
        "\"application_matrix\":\"NOT_RUN\",\"whole_version_PASS\":false,\"human_uat\":\"NOT_RUN\"");
}

// The generic independent-entry .after gate requires a live product GUI.
// Only this deliberate terminal case may substitute its actual current-phase
// completion records; absence/exception/unknown cleanup never means success.
bool visual_lifecycle_chain_terminal_observed(State& s,std::uint64_t phase_mark){
    namespace e=product::guest::up_evidence_detail;
    if(s.process||s.left||s.control)return false;
    std::uint64_t exit_sequence{},exit_birth{},callback_sequence{},complete_sequence{};
    std::istringstream rows(s.log.since(phase_mark));std::string row;
    while(std::getline(rows,row)){
        if(!row.ends_with('}')||!e::quoted(row,"\"source_sha\":",PANEBIND_BUILD_SHA))continue;
        if(e::quoted(row,"\"type\":","case")&&e::quoted(row,"\"result\":","PASS")){
            if(e::quoted(row,"\"case\":","I06_same_product_normal_Exit_actual_resources_and_tuple")&&
                e::number(row,"\"same_product_PID\":")==s.product_pid){
                exit_sequence=e::number(row,"\"sequence\":").value_or(0);exit_birth=e::number(row,"\"same_product_birth\":").value_or(0);
            }else if(e::quoted(row,"\"case\":","I06_final_callback_completed_without_product_restart"))
                callback_sequence=e::number(row,"\"sequence\":").value_or(0);
        }else if(e::quoted(row,"\"type\":","I06_visual_lifecycle_chain_complete")&&
            e::quoted(row,"\"result\":","OWNED_VISUAL_SEQUENCE_AND_SAME_INSTANCE_NORMAL_EXIT_OBSERVED")&&
            e::number(row,"\"same_product_PID\":")==s.product_pid&&e::number(row,"\"same_product_birth\":")==exit_birth)
            complete_sequence=e::number(row,"\"sequence\":").value_or(0);
    }
    return exit_birth&&exit_sequence&&callback_sequence>exit_sequence&&complete_sequence>callback_sequence;
}
