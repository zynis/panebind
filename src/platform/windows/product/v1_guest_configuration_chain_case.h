#pragma once

// TEST ONLY. Included after the existing settings/rule/geometry helpers in
// v1_guest_fixture.cpp. No private product command, fake GUI notification or
// configuration write is used except the existing exact-Run corruption case.
void configuration_chain_excluded_Ctrl_attempt(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);
    const auto excluded=settings_guest_configuration(s);
    if(s.suite!="ConfigurationChainOnly"||excluded.rules.empty()||s.left||s.control)
        throw std::runtime_error("configuration_chain_actual_excluded_idle_required");
    settings_guest_effect_layout(s);
    // Recreate the same three-member adjacency while the rule remains active.
    // This is owned fixture setup, never a product placement or group result.
    const std::array<Rect,3> adjacent{{{120,80,520,440},{520,80,780,260},{520,260,780,440}}};
    for(std::size_t n=0;n<adjacent.size();++n)
        if(!own(s,s.frames[n].hwnd)||!fixture_rect(s.frames[n].hwnd,adjacent[n]))
            throw std::runtime_error("configuration_chain_excluded_owned_setup_failed");
    const auto before_status=geometry_history_status(s);
    const auto before_calls=e::number(before_status,"\"native_callsite_claims\":");
    const auto mark=read_product(s).size();
    key(s,VK_CONTROL,true);
    settings_guest_native_move(s,36,21,"configuration_chain_excluded_Ctrl_Move");
    key(s,VK_CONTROL,false);
    const auto after_status=geometry_history_status(s);
    const auto after_calls=e::number(after_status,"\"native_callsite_claims\":");
    s.log.check("configuration_chain_excluded_source_and_stale_group_zero_writer",
        before_calls&&after_calls&&*before_calls==*after_calls&&
        record(s,mark,"gesture_start").empty()&&record(s,mark,"placement").empty()&&
        !s.left&&!s.control&&settings_guest_configuration(s)==excluded&&
        own(s,s.frames[1].hwnd)&&own(s,s.frames[2].hwnd)&&
        visible(s.frames[1].hwnd)==adjacent[1]&&visible(s.frames[2].hwnd)==adjacent[2],
        "\"actual_Ctrl_DOWN_and_UP_used\":true,\"native_source_Move_and_END_observed\":true,"
        "\"excluded_source_enhanced_START\":false,\"excluded_follower_geometry_unchanged\":true,"
        "\"fresh_status_before\":"+quote(before_status)+",\"fresh_status_after\":"+quote(after_status)+
        ",\"native_passthrough_not_product_NormalUp\":true");
}

void owned_configuration_chain_case(State& s){
    namespace v=panebind::core::v1;
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);
    const auto original=settings_guest_configuration(s);
    auto defaults=v::default_settings();defaults.generation=original.generation;
    if(s.suite!="ConfigurationChainOnly"||original!=defaults||s.left||s.control||
       std::any_of(s.explorer.begin(),s.explorer.end(),[](const auto& x){return static_cast<bool>(x);})||
       std::any_of(s.applications.begin(),s.applications.end(),[](const auto& x){return static_cast<bool>(x);}))
        throw std::runtime_error("configuration_chain_default_exact_owned_only_required");
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    const auto binary=hash(s.product);const auto product_source=s.product_source;
    if(binary.empty()||binary!=hash(s.input_product))throw std::runtime_error("configuration_chain_product_hash_unknown");
    std::array<Rect,3> initial{};
    for(std::size_t n=0;n<initial.size();++n){const auto r=visible(s.frames[n].hwnd);
        if(!r||!own(s,s.frames[n].hwnd))throw std::runtime_error("configuration_chain_original_owned_root_missing");initial[n]=*r;}
    const auto suffix=s.configuration=="Debug"?L"debug":L"release";
    const auto exported=s.output/(s.run+L"-"+suffix+L"-settings-export.conf");
    if(fs::exists(exported))throw std::runtime_error("configuration_chain_export_requires_fresh_Run_path");
    try{
        // A nondefault valid baseline makes recovery observable rather than
        // accepting defaults that happen to equal the pre-corruption options.
        settings_guest_attraction(s,8);
        const auto group=[&](const char* label){
            settings_guest_effect_layout(s);
            const std::array<Rect,3> adjacent{{{120,80,520,440},{520,80,780,260},{520,260,780,440}}};
            for(std::size_t n=0;n<adjacent.size();++n)
                if(!own(s,s.frames[n].hwnd)||!fixture_rect(s.frames[n].hwnd,adjacent[n]))
                    throw std::runtime_error("configuration_chain_group_owned_setup_failed");
            const auto mark=read_product(s).size();ctrl_move(s,0,24,18,label);
            const auto start=record(s,mark,"gesture_start");const auto generation=e::number(start,"\"generation\":");
            std::vector<std::uint64_t> keys;bool valid=true;std::string row;
            std::istringstream rows(product_log_since(s,mark));
            while(std::getline(rows,row))if(e::quoted(row,"\"type\":","placement")&&
                e::unique_value(row,"\"native_callsite_claimed\":")=="true"){
                const auto key=e::number(row,"\"key\":");
                valid=valid&&generation&&key&&*key&&geometry_history_receipt(s,row,"placement")&&
                    e::number(row,"\"generation\":")==generation&&e::unique_value(row,"\"exact\":")=="true"&&
                    e::unique_value(row,"\"identity_valid\":")=="true";
                if(key&&std::find(keys.begin(),keys.end(),*key)==keys.end())keys.push_back(*key);
            }
            s.log.check(label,valid&&geometry_history_receipt(s,start,"gesture_start")&&
                e::unique_value(start,"\"ctrl\":")=="true"&&e::number(start,"\"group_members\":")==3&&
                keys.size()==3&&!s.left&&!s.control,
                "\"actual_START\":"+quote(start)+",\"actual_native_member_keys\":"+std::to_string(keys.size())+
                ",\"three_exact_owned_members_and_real_NormalUp_required\":true");
            chain_check_product(s,pid,birth);return generation.value_or(0);
        };
        const auto before_exclusion=group("configuration_chain_before_exclusion_actual_three_member_group");
        // The existing helper performs the real Add Current App/Class + Save,
        // ordinary excluded native Move/zero writer, removal and snap recovery.
        settings_guest_exclusion(s,configuration_chain_excluded_Ctrl_attempt);
        const auto restored_group=group("configuration_chain_rule_removed_actual_three_member_group");
        s.log.check("configuration_chain_rule_removed_fresh_generation",
            restored_group>before_exclusion&&settings_guest_configuration(s).rules==original.rules,
            "\"previous_group_not_reused\":true,\"same_product_process\":true");

        const auto baseline=settings_guest_configuration(s);
        const auto config_path=s.product.parent_path()/L"settings.ini";
        const auto before_export=read(config_path);
        auto gui=gui_open(s);settings_guest_file_path(s,gui,true,exported);
        const auto export_bytes=read(exported);const auto export_hash=hash(exported);const auto parsed=v::parse(export_bytes);
        s.log.check("configuration_chain_export_current_valid_options",
            parsed.value&&*parsed.value==baseline&&!export_hash.empty()&&read(config_path)==before_export,
            "\"actual_native_file_dialog\":true,\"saved_runtime_not_modified_draft_exported\":true,"
            "\"export_sha256\":"+quote(export_hash));
        gui_press(s,gui,VK_ESCAPE);
        s.log.check("configuration_chain_export_cancel_keeps_runtime",wait([&]{return !IsWindow(gui.root);})&&
            settings_guest_configuration(s)==baseline&&read(config_path)==before_export);

        settings_guest_attraction(s,0);const auto changed=settings_guest_configuration(s);
        auto wanted=baseline;wanted.attraction_dip=0;wanted.generation=changed.generation;
        s.log.check("configuration_chain_distinct_change_after_export",changed==wanted&&changed.generation>baseline.generation&&
            hash(exported)==export_hash&&read(exported)==export_bytes,"\"actual_GUI_Save_after_export\":true");
        settings_guest_move_effect(s,false,"configuration_chain_changed_zero_distance_actual_free_Move");
        chain_check_product(s,pid,birth);
        gui=gui_open(s);settings_guest_file_path(s,gui,false,exported);gui_page_select(s,gui,1);
        s.log.check("configuration_chain_import_original_valid_draft",gui_text(s,gui,gui_number(s,gui,L"吸引距离"))==L"8"&&
            settings_guest_configuration(s)==changed&&hash(exported)==export_hash);
        gui_save_enter(s,gui);const auto imported=settings_guest_configuration(s);wanted=baseline;wanted.generation=imported.generation;
        s.log.check("configuration_chain_import_restores_original_effective_options",imported==wanted&&
            imported.generation>changed.generation&&read(exported)==export_bytes);
        settings_guest_move_effect(s,true,"configuration_chain_imported_distance_actual_snap_Move");
        chain_check_product(s,pid,birth);

        // Existing guarded helper proves normal stop, writes only this Run's
        // config, verifies the exact bad-byte backup/notice, then uses real GUI
        // to restore the nondefault valid options and actual normal Move/UP.
        settings_guest_corrupt_restart(s);
        const auto recovered=settings_guest_configuration(s);wanted=baseline;wanted.generation=recovered.generation;
        const auto recovered_birth=process_birth(s.process);
        s.log.check("configuration_chain_corrupt_restart_preserves_valid_export_and_options",recovered==wanted&&
            read(exported)==export_bytes&&hash(exported)==export_hash&&s.product_source==product_source&&
            hash(s.product)==binary&&s.process&&GetProcessId(s.process)==s.product_pid&&
            WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&recovered_birth&&recovered_birth!=birth,
            "\"pre_corruption_valid_export_retained\":true,"
            "\"bad_file_backup_and_notice_required_by_existing_case\":true,\"intended_normal_restart_only\":true");
        settings_guest_move_effect(s,true,"configuration_chain_recovered_nondefault_actual_snap_Move");
        settings_guest_attraction(s,original.attraction_dip);
        const auto final=settings_guest_configuration(s);wanted=original;wanted.generation=final.generation;
        s.log.check("configuration_chain_original_options_and_binary_restored",final==wanted&&hash(s.product)==binary&&!s.left&&!s.control);
        const auto idle=geometry_history_status(s);GUITHREADINFO native{sizeof(native)};
        if(!GetGUIThreadInfo(s.owner,&native)||native.hwndCapture||native.hwndMoveSize||native.hwndMenuOwner)
            throw std::runtime_error("configuration_chain_final_owned_GUI_not_idle");
        for(std::size_t n=0;n<initial.size();++n)s.log.check("configuration_chain_original_owned_layout_restored",
            own(s,s.frames[n].hwnd)&&fixture_rect(s.frames[n].hwnd,initial[n]));
        s.log.emit("configuration_chain_component","\"result\":\"PASS_SCOPED\",\"same_candidate_ordered_sequence\":true,"
            "\"actual_rules_native_and_Ctrl_exclusion_import_and_corrupt_restart\":true,\"final_idle_status\":"+quote(idle)+
            ",\"I07_final_verdict\":\"REVIEW_ACTUAL_EVIDENCE\",\"tray\":\"NOT_RUN\",\"five_applications\":\"NOT_RUN\","
            "\"I11_900_seconds\":\"NOT_RUN\",\"whole_version_PASS\":false,\"human_uat\":\"NOT_RUN\"");
    }catch(...){
        // Release only this driver's outstanding input; no second mouse UP.
        if(s.left&&!s.left_up_attempted)try{button(s,false);}catch(...){}
        if(s.control)try{key(s,VK_CONTROL,false);}catch(...){}
        bool restored=false;std::string failure;
        try{
            settings_guest_scope(s);const auto current=settings_guest_configuration(s);
            if(!current.rules.empty()){
                std::wstring exe(32768,L'\0');const auto n=GetModuleFileNameW(nullptr,exe.data(),static_cast<DWORD>(exe.size()));
                if(!n||n>=exe.size())throw std::runtime_error("configuration_chain_cleanup_image_unknown");exe.resize(n);
                const v::Rule expected{utf8(exe),utf8(s.class_name),true,0xffffffffU};
                if(current.rules!=std::vector<v::Rule>{expected})throw std::runtime_error("configuration_chain_unknown_rules_not_overwritten");
                owned_rule_restore_rules_GUI(s,original);
            }
            settings_guest_attraction(s,original.attraction_dip);
            const auto actual=settings_guest_configuration(s);auto wanted=original;wanted.generation=actual.generation;restored=actual==wanted;
        }catch(const std::exception& error){failure=error.what();}
        s.log.emit("configuration_chain_failed_cleanup","\"configuration_restored\":"+std::string(restored?"true":"false")+
            ",\"driver_left_outstanding\":"+(s.left?"true":"false")+",\"driver_Ctrl_outstanding\":"+(s.control?"true":"false")+
            ",\"reason\":"+quote(failure)+",\"original_failure_preserved\":true,\"scenario_PASS_inferred\":false");
        throw;
    }
}
