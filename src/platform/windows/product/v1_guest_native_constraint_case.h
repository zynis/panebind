#pragma once

// TEST ONLY. Two existing real production paths, not another Resize matrix.
// Fixed-size evidence remains historical; this Run does not rerun/award it.
void native_constraint_scope(State& s){
    settings_guest_scope(s);const auto config=chain_settings(s);
    if(s.suite!="NativeConstraintOnly"||!config.enabled||!config.rules.empty()||s.left||s.control)
        throw std::runtime_error("native_constraint_exact_released_unexcluded_guest_required");
}
void owned_native_maximum_constraint_case(State& s){
    native_constraint_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    auto config=original;config.magnet_resize=true;config.magnet_move=false;config.glue=false;config.docking=false;
    config.preview=false;config.feedback=false;config.drag_transparency=false;config.shake=false;
    config.attraction_dip=10;config.release_dip=16;config.speed_dip=2000;
    const auto applied=chain_gui_save(s,config);LayoutFixtures set{s};
    try{
        set.create();const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<1200||work.height()<740)throw std::runtime_error("native_constraint_maximum_work_area_too_small");
        const Rect source{work.l+420,work.t+220,work.l+820,work.t+520};const auto dpi=GetDpiForWindow(set.frames[0].hwnd);
        const auto attraction=MulDiv(10,static_cast<int>(dpi),96);
        s.log.check("native_constraint_maximum_actual_candidate_inside_field",dpi&&attraction>=6&&attraction<30,
            "\"actual_DPI\":"+std::to_string(dpi)+",\"attraction_pixels\":"+std::to_string(attraction)+",\"candidate_gap_from_max\":6");
        const LayoutLimit limit{120,100,work.width(),work.height()};
        resize_magnet_maximum_pullback_and_move(set,source,work,limit,0);
        chain_check_product(s,pid,birth);const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        s.log.check("native_constraint_maximum_original_configuration_restored",restored==expected);
        s.log.check("native_constraint_maximum_known_owned_resources_closed",set.close());
        s.log.emit("native_constraint_maximum_component","\"result\":\"PASS_SCOPED\",\"max_visible_width\":450,\"original_DOWN_pullback_width\":430,"
            "\"real_Resize_UP_and_successor_Move\":true,\"eight_directions\":\"NOT_RUN\",\"fixed_size\":\"NOT_RUN\",\"whole_I10_PASS\":false");
    }catch(...){pause_routing_failed_case_input(s);try{if(!set.closed)set.close();}catch(...){}
        try{if(s.process&&!s.left&&!s.control&&chain_settings(s)==applied)chain_gui_save(s,original);}catch(...){}throw;}
}
void owned_native_grid_constraint_only_case(State& s){
    native_constraint_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    auto config=original;config.glue=true;config.magnet_move=false;config.magnet_resize=false;config.docking=false;
    config.preview=false;config.feedback=false;config.shake=false;config.drag_transparency=false;
    const auto applied=chain_gui_save(s,config);
    try{
        // Actual application WMPOS grid mismatch, abort UP and successor paths
        // stay in this one existing helper. A pre-native FocusLost stays FAIL.
        owned_native_grid_constraint_case(s);
        chain_check_product(s,pid,birth);const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        s.log.check("native_constraint_grid_original_configuration_restored",restored==expected);
    }catch(...){pause_routing_failed_case_input(s);
        try{if(s.process&&!s.left&&!s.control&&chain_settings(s)==applied)chain_gui_save(s,original);}catch(...){}throw;}
}
