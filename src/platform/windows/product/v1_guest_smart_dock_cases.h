#pragma once

// Guest-only M08-F03 gap checks. Include after geometry_history helpers.
// Expected partitions below are independent integers, never smart_dock output.
void owned_smart_dock_remaining_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);
    if(s.suite!="RemainingNativeContractsOnly"||s.left||s.control||!chain_settings(s).enabled)
        throw std::runtime_error("smart_dock_exact_released_enabled_guest_required");
    const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    auto wanted=original;wanted.docking=true;wanted.smart_docking=true;wanted.magnet_move=false;wanted.magnet_resize=false;
    wanted.glue=false;wanted.preview=false;wanted.drag_transparency=false;wanted.shake=false;chain_gui_save(s,wanted);
    LayoutFixtures set{s};set.create();const auto work=chain_work(set.frames[0].hwnd);
    if(work.width()<1100||work.height()<700)throw std::runtime_error("smart_dock_declared_work_area_requirement_missing");
    const LayoutLimit limits{120,100,work.width(),work.height()};
    const Rect source{work.l+120,work.t+140,work.l+520,work.t+440};
    const LONG near_line=work.l+work.width()*2/5,far_line=work.l+work.width()/3;
    const Rect near_left{work.l,work.t,near_line,work.b},far_left{work.l,work.t,far_line,work.b};
    const Rect remaining{near_line,work.t,work.r,work.b},ordinary{work.l+work.width()/2,work.t,work.r,work.b};
    const auto neighbors=[&](const std::vector<Rect>& before,const std::vector<std::optional<Rect>>& positioning,const char* label){
        bool exact=before.size()+1==set.active&&positioning.size()==before.size();
        for(std::size_t n=0;n<before.size();++n)exact=exact&&set.owns(n+1)&&visible(set.frames[n+1].hwnd)==before[n]&&
            positioning[n]&&geometry_history_positioning(set.frames[n+1].hwnd)==positioning[n];
        s.log.check(label,exact,"\"neighbor_count\":"+std::to_string(before.size())+
            ",\"visible_and_positioning_unchanged\":"+(exact?"true":"false")+",\"neighbors_not_repositioned_to_make_free_space\":true");
    };
    const auto neighbor_positioning=[&]{std::vector<std::optional<Rect>> observed;
        for(std::size_t n=1;n<set.active;++n)observed.push_back(geometry_history_positioning(set.frames[n].hwnd));return observed;};
    // Both neighbors are left/work-area anchored and cover the full height.
    // 40% is strictly nearer the ordinary 50% split than 1/3; its remainder
    // has no overlap with either. No key/order-dependent oracle is needed.
    set.setup({source,near_left,far_left},{limits,limits,limits});const auto key=geometry_history_source_key(set,source);
    auto positions=neighbor_positioning();geometry_history_command(set,key,L"DockRight",remaining);
    neighbors({near_left,far_left},positions,"smart_dock_multiple_candidates_nearest_legal_neighbors_unchanged");
    geometry_history_command(set,key,L"Undo",source);neighbors({near_left,far_left},positions,"smart_dock_multiple_candidates_Undo_neighbors_unchanged");
    // This real obstacle intersects the proposed 40%-to-right remainder.
    // Production's documented policy is the ordinary half partition, not an
    // invented empty-space search or relocation of the obstructing window.
    const Rect obstacle{work.r-320,work.t+100,work.r-20,work.t+400};
    set.setup({source,near_left,obstacle},{limits,limits,limits});positions=neighbor_positioning();
    geometry_history_command(set,key,L"DockRight",ordinary);
    neighbors({near_left,obstacle},positions,"smart_dock_ambiguous_overlap_ordinary_fallback_neighbors_unchanged");
    geometry_history_command(set,key,L"Undo",source);neighbors({near_left,obstacle},positions,"smart_dock_fallback_Undo_neighbors_unchanged");
    // A positive, non-overlapping remainder can still violate SOURCE limits.
    // The single production writer must reject before the native call, rather
    // than accepting an application's later min-width clamp as exact success.
    const Rect almost_full{work.l,work.t,work.r-80,work.b},too_narrow{work.r-80,work.t,work.r,work.b};
    const LayoutLimit source_limit{200,150,work.width(),work.height()};
    set.setup({source,almost_full},{source_limit,limits});positions=neighbor_positioning();
    const auto before_positioning=geometry_history_positioning(set.frames[0].hwnd);const auto prior=geometry_history_status(s);
    chain_front(set,0);const auto mark=read_product(s).size();std::string rejection,command;
    const bool sent=launch(s,L"--command DockRight",true);const bool observed=sent&&wait([&]{
        rejection=record(s,mark,"placement");command=record(s,mark,"command");return
            geometry_history_receipt(s,rejection,"placement")&&e::number(rejection,"\"key\":")==key&&e::number(rejection,"\"generation\":")==0&&
            rejection.find("\"target\":"+rect(too_narrow))!=rejection.npos&&e::quoted(rejection,"\"intent_reason\":","command")&&
            e::quoted(rejection,"\"reason\":","batch_preflight_target_violates_size_constraints")&&
            e::unique_value(rejection,"\"declared_track_limits_observed\":")=="true"&&
            e::unique_value(rejection,"\"native_callsite_claimed\":")=="false"&&e::unique_value(rejection,"\"native_attempted\":")=="false"&&
            geometry_history_receipt(s,command,"command")&&e::quoted(command,"\"action\":","DockRight")&&e::number(command,"\"key\":")==key;
    });
    s.log.check("smart_dock_too_narrow_actual_pre_native_rejection",observed,
        "\"independent_requested_visible\":"+rect(too_narrow)+",\"declared_source_min_visible_width\":200,\"requested_width\":80"+
        ",\"actual_rejection\":"+quote(rejection)+",\"actual_command\":"+quote(command)+",\"API_success_not_inferred\":true");
    const auto status=geometry_history_status(s,*e::number(command,"\"sequence\":"));unsigned placements{},native_claims{};bool only_source=true;
    std::istringstream rows(product_log_since(s,mark));std::string row;while(std::getline(rows,row))if(e::quoted(row,"\"type\":","placement")){
        ++placements;only_source=only_source&&geometry_history_receipt(s,row,"placement")&&e::number(row,"\"key\":")==key;
        if(e::unique_value(row,"\"native_callsite_claimed\":")=="true"||e::unique_value(row,"\"native_attempted\":")=="true")++native_claims;
    }
    s.log.check("smart_dock_too_narrow_source_unchanged_zero_native",set.owns(0)&&visible(set.frames[0].hwnd)==source&&
        before_positioning&&geometry_history_positioning(set.frames[0].hwnd)==before_positioning&&placements==1&&native_claims==0&&only_source&&
        e::number(prior,"\"native_callsite_claims\":")==e::number(status,"\"native_callsite_claims\":"),
        "\"new_placement_records\":"+std::to_string(placements)+",\"new_native_claims\":"+std::to_string(native_claims)+
        ",\"prior_status\":"+quote(prior)+",\"after_status\":"+quote(status));
    neighbors({almost_full},positions,"smart_dock_too_narrow_rejection_neighbor_unchanged");
    // Do not Undo a failed command. Re-establish a legal current layout and
    // verify the same instance can perform a fresh command and real Undo.
    set.setup({source,near_left},{limits,limits});positions=neighbor_positioning();
    geometry_history_command(set,key,L"DockRight",remaining);geometry_history_command(set,key,L"Undo",source);
    neighbors({near_left},positions,"smart_dock_after_rejection_legal_command_Undo_neighbor_unchanged");
    const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
    s.log.check("smart_dock_original_configuration_restored",restored==expected);chain_check_product(s,pid,birth);
    s.log.check("smart_dock_exact_test_windows_closed",set.close());
    s.log.emit("smart_dock_remaining_component","\"result\":\"PASS\",\"scope\":\"production_DockRight_nearest_candidates_ordinary_overlap_fallback_source_min_rejection_and_Undo\",\"same_product_instance\":true,\"five_applications\":\"NOT_RUN\",\"whole_feature_or_version_PASS\":false,\"human_uat\":\"NOT_RUN\"");
}
