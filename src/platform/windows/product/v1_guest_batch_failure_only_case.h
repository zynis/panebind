#pragma once

// TEST ONLY: the existing second-member native failure and actual survivor
// recovery, once. This does not replay group bounds, max/grid or Full, and it
// does not require/claim that the outer SetWindowPos returned FALSE.
void owned_batch_failure_only_case(State& s){
    settings_guest_scope(s);const auto original=chain_settings(s);
    if((s.suite!="BatchFailureOnly"&&s.suite!="InputFollowupOnly")||!original.enabled||!original.rules.empty()||s.left||s.control)
        throw std::runtime_error("batch_failure_exact_released_unexcluded_guest_required");
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    chain_check_product(s,pid,birth);
    s.log.check("I10_batch_failure_exact_product_before_case",!binary.empty()&&binary==hash(s.input_product),
        "\"product_PID\":"+std::to_string(pid)+",\"product_birth\":"+std::to_string(birth)+",\"product_sha256\":"+quote(binary));
    auto config=original;config.glue=true;config.magnet_move=false;config.magnet_resize=false;config.docking=false;
    config.preview=false;config.feedback=false;config.shake=false;config.drag_transparency=false;
    const auto applied=chain_gui_save(s,config);
    try{
        owned_second_member_native_failure_case(s);
        chain_check_product(s,pid,birth);
        s.log.check("I10_batch_failure_same_product_after_survivor_recovery",!s.left&&!s.control&&hash(s.product)==binary,
            "\"same_product_PID\":"+std::to_string(pid)+",\"same_product_birth\":"+std::to_string(birth)+
            ",\"native_FALSE_not_required\":true,\"actual_native_outcome_recorded_by_original_helper\":true");
        const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        s.log.check("I10_batch_failure_original_configuration_restored",restored==expected);
        chain_check_product(s,pid,birth);
        s.log.emit("I10_batch_failure_only_complete","\"result\":\"ACTUAL_SECOND_MEMBER_NATIVE_IDENTITY_LOSS_AND_SURVIVOR_RECOVERY_OBSERVED\","
            "\"same_product_PID\":"+std::to_string(pid)+",\"same_product_birth\":"+std::to_string(birth)+
            ",\"existing_failure_helper_runs\":1,\"product_restarted\":false,\"original_configuration_restored\":true,"
            "\"group_bounds_or_max_grid_replayed\":false,\"whole_I10_PASS\":false,\"Full_suite\":\"NOT_RUN\",\"human_uat\":\"NOT_RUN\"");
    }catch(...){pause_routing_failed_case_input(s);
        try{if(s.process&&!s.left&&!s.control&&GetProcessId(s.process)==pid&&process_birth(s.process)==birth&&
            WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&chain_settings(s)==applied)chain_gui_save(s,original);}catch(...){}
        throw;
    }
}
