# Pure offline synthetic prefix fixtures. No historical evidence, GUI, input,
# child/environment executable or Git command is used by this test.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-native-abort-validation.ps1')
$script:fixGChecks=0
$script:fixGFailures=[Collections.Generic.List[string]]::new()
function Check-FixGPrefix([bool]$Pass,[string]$Name){++$script:fixGChecks;if(-not $Pass){$script:fixGFailures.Add($Name)}}
function Copy-FixGPrefix($Value){return ($Value|ConvertTo-Json -Depth 40|ConvertFrom-Json)}
function Reject-FixGPrefix([scriptblock]$Work,[string]$Name){$rejected=$false;try{$null=& $Work}catch{$rejected=$true};Check-FixGPrefix $rejected $Name}
function Add-FixGPrefixRow($Rows,[string]$Type,$Fields,[string]$Scope='gesture'){
    $row=[ordered]@{schema='r1c4b-takeover-owned/v5';sequence=[long]($Rows.Count+1);qpc=([long]1749903002082+([long]$Rows.Count+1)*100);type=$Type;gesture=0;event_scope=$Scope;event_acceptance_eligible=$false}
    foreach($name in $Fields.Keys){$row[$name]=$Fields[$name]};$r=[pscustomobject]$row;$Rows.Add($r);return $r
}
function Add-FixGPrefixAck($Rows,[string]$Phase){
    $request=Add-FixGPrefixRow $Rows 'abort_quiescence_request' @{phase=$Phase;message=32826;posted=$true;request_qpc=([long]1749903002082+([long]$Rows.Count+1)*100-1);source_hwnd=99;source_pid=40001;source_tid=40002;run_nonce=[long]99112233} 'cleanup'
    $ack=Add-FixGPrefixRow $Rows 'abort_writer_quiescence' @{phase='owner_ack';cleanup_phase=$Phase;request_sequence=$request.sequence;ack_tid=40002;ack_qpc=([long]1749903002082+([long]$Rows.Count+1)*100-1);active_operation_id=0;pending_raw_count=0;motion_pending=$false;notice_posted=$false;acceptance_retired=$true;takeover_scope=$false;native_calls=0;acknowledged=$true;source_hwnd=99;source_pid=40001;source_tid=40002;run_nonce=[long]99112233;actual_source_pid=40001;actual_source_tid=40002;actual_source_nonce=[long]99112233;own_identity=$true} 'cleanup'
    $null=Add-FixGPrefixRow $Rows 'abort_quiescence_wait' @{phase=$Phase;request_sequence=$request.sequence;ack_sequence=$ack.sequence;timeout_ms=2000;wait_result=0;quiescent=$true} 'cleanup'
}
function New-FixGPrefixFixture([string]$Operation='Move',[string]$TestMode='controlled_abort',[switch]$WithQueryDetails){
    $rows=[Collections.Generic.List[object]]::new()
    $null=Add-FixGPrefixRow $rows 'startup' @{pid=40001;ui_tid=40002;run_nonce=[long]99112233;gesture_id=1;qpc_frequency=[long]10000000;operation=$Operation;test_mode=$TestMode;mode='free_takeover';fence_contract='ordered_failure_snapshot_v1';cleanup_contract='owned_native_abort_v1';abort_cleanup_tag=0x50424655;foreground_contract='verified_global_foreground_v2';handoff_contract='winevent_end_barrier_v1';diagnostic_contract='separated_authority_v1';authority_contract='product_gesture_v1';input_isolation_contract='post_end_shield_v1';input_correlation='actual_absolute_receipt_v1';human_input=$false;real_explorer=$false;sendinput_in_probe=$true;takeover_geometry_writes=0;synthetic_fixture=$true}
    $null=Add-FixGPrefixRow $rows 'desktop_gate' @{active_unlocked=$true;input_desktop_matches=$true}
    $null=Add-FixGPrefixRow $rows 'guard' @{hwnd=98;pid=40001;tid=40002;positioning_available=$true;positioning=@(50,50,530,470);positioning_error=0;noactivate=$true;margin_px=50;planned_move_bounds=@(100,100,480,300);planned_bottom_resize_bounds=@(100,100,300,420);planned_bounds=@(100,100,480,420);move_trajectory_with_margin_covered=$true;bottom_resize_trajectory_with_margin_covered=$true}
    $null=Add-FixGPrefixRow $rows 'receiver' @{receiver_sequence=1;receiver_qpc=([long]1749903002082+399);receiver_hwnd=97;receiver_pid=40003;receiver_tid=40004;registration_verified=$true;usage_page=1;usage=2;registration_flags=256;keyboard_registered=$false}
    $null=Add-FixGPrefixRow $rows 'show_window' @{startup_flags=0;startup_show=0;requested_show=4;visible=$true}
    $null=Add-FixGPrefixRow $rows 'activation_event' @{message='WM_ACTIVATE';activation_code=1;activation_epoch=0;target=99;foreground_matches=$false}
    $null=Add-FixGPrefixRow $rows 'activation_event' @{message='WM_SETFOCUS';activation_epoch=0;target=99;foreground_matches=$false}
    $null=Add-FixGPrefixRow $rows 'foreground_attempt' @{target=99;foreground=101;source_thread_local_active=99;source_thread_local_focus=99;set_foreground_success=$false;visible=$true}
    $null=Add-FixGPrefixRow $rows 'owned' @{hwnd=99;pid=40001;tid=40002;saved_cursor=@(10,10);work_area=@(0,0,1000,800);dpi=96;positioning=@(100,100,300,300);visible=@(110,100,290,290);positioning_error=0;visible_hresult=0;virtual_screen=@(0,0,1000,800);monitor=1;run_nonce=[long]99112233;actual_source_nonce=[long]99112233}
    $null=Add-FixGPrefixRow $rows 'winevent_hook_installed' @{hook=96;source_hwnd=99;source_pid=40001;source_tid=40002;install_tid=40002;event_min=10;event_max=11;flags=0;dll=0;skip_own_process=$false;skip_own_thread=$false;install_success=$true;error=0}
    $raise=Add-FixGPrefixRow $rows 'activation_visibility' @{target=99;enabled=$true;success=$true;flags=83;topmost_style=$true;visible=$true}
    $fenceFields=@{phase='move';target=99;foreground=101;cursor=@(10,10);activation_point=@(150,150);own_identity=$true;desktop_ready=$true;same_integrity=$true;visible=$true;temporary_topmost=$true;window_from_point_root=99;window_from_point_root_matches=$true;hit_test=1;gui_query_succeeded=$true;foreground_snapshot_stable=$true;gui_flags=0;capture_hwnd=102;menu_owner_hwnd=0;move_size_hwnd=0;foreign_capture_clear=$false;modifiers_clear=$true;button_state_matches=$true;left_down=$false;input_tag=0x50424D41}
    if($WithQueryDetails){foreach($pair in @(@('activation_diagnostic_contract','foreground_gui_activation_v1'),@('foreground_pid',41001),@('foreground_tid',41002),@('foreground_query_before_qpc',($raise.qpc+1)),@('foreground_query_after_qpc',($raise.qpc+30)),@('foreground_after',101),@('foreground_after_pid',41001),@('foreground_after_tid',41002),@('gui_query_tid',41002),@('gui_query_attempted',$true),@('gui_query_start_qpc',($raise.qpc+10)),@('gui_query_finish_qpc',($raise.qpc+20)),@('gui_query_error',0),@('foreground_tuple_stable',$true),@('gui_failure_subreason','CAPTURE_NONZERO'))){$fenceFields[$pair[0]]=$pair[1]}}
    $null=Add-FixGPrefixRow $rows 'activation_fence' $fenceFields
    $null=Add-FixGPrefixRow $rows 'activation_visibility' @{target=99;enabled=$false;success=$true;flags=19;topmost_style=$false;visible=$true}
    $null=Add-FixGPrefixRow $rows 'foreground_bootstrap' @{target=99;set_foreground_attempted=$true;set_foreground_success=$false;activation_click_required=$true;temporary_topmost=$true;activation_point=@(150,150);window_from_point_root_matches=$true;hit_test=1;foreign_capture_clear=$false;sendinput_move_success=$false;sendinput_down_success=$false;sendinput_up_success=$false;wm_activate_seen=$false;wm_setfocus_seen=$false;activation_event_seen=$false;foreground=101;topmost_now=$false;final_foreground_matches=$false;topmost_restored=$true;result='BLOCKED';reason='BLOCKED_BY_FOREIGN_INPUT_CAPTURE'}
    $null=Add-FixGPrefixRow $rows 'blocked' @{reason='BLOCKED_BY_FOREIGN_INPUT_CAPTURE'}
    Add-FixGPrefixAck $rows 'initial'
    $cleanup=@{target=99;guard=98;source_pid=40001;source_tid=40002;actual_source_pid=40001;actual_source_tid=40002;actual_source_nonce=[long]99112233;test_down_owned=$false;own_identity=$true;desktop_ready=$true;source_visible=$true;foreground_hwnd=101;foreground_matches=$false;foreground_pid=41001;foreground_tid=41002;gui_query_succeeded=$true;gui_error=0;capture_hwnd=0;no_foreign_capture=$true;menu_owner_hwnd=0;menu_clear=$true;move_size_hwnd=0;move_size_clear=$true;gui_flags=0;gui_mode_clear=$true;cursor_success=$true;cursor_error=0;cursor=@(10,10);cursor_root=102;cursor_root_owned_or_guard=$false;left_down=$false;buttons_modifiers_clear=$true;foreign_capture_transferred=$false;acceptance_eligible=$false;run_nonce=[long]99112233;stop_requested=$false;source_retired=$false;fixture_scope_active=$true}
    $initial=$cleanup.Clone();$initial.phase='initial';$initial.eligible=$false;$initial.outcome='SKIPPED_NO_AUTHORITY';$null=Add-FixGPrefixRow $rows 'cleanup_input_diagnostic' $initial 'cleanup'
    $null=Add-FixGPrefixRow $rows 'cleanup_final_isolation' @{phase='final';input_scope='cleanup';acceptance_eligible=$false;run_nonce=[long]99112233;source_hwnd=99;source_pid=40001;source_tid=40002;actual_source_pid=40001;actual_source_tid=40002;actual_source_nonce=[long]99112233;source_identity=$true;shield_hwnd=0;shield_pid=40001;shield_tid=40002;actual_shield_pid=0;actual_shield_tid=0;actual_shield_nonce=0;shield_identity=$false;cursor_available=$true;destination=@(10,10);source_positioning_available=$true;source_positioning=@(100,100,300,300);shield_positioning=$null;inside_source=$false;root=102;expected_root=0;root_owned=$false;foreground_hwnd=101;foreground_pid=41001;foreground_tid=41002;foreground_matches=$false;shield_needed=$false;shield_active=$false;shield_noactivate=$false;shield_topmost=$false;shield_visible=$false;shield_never_activated=$true;shield_nonoverlapping=$true;point_valid=$false;failure_class='TestInputIsolationUnavailable'} 'cleanup'
    $null=Add-FixGPrefixRow $rows 'cleanup_skipped_no_authority' @{reason='cleanup_no_reliable_final_no_button_proof';acceptance_eligible=$false} 'cleanup'
    $final=$cleanup.Clone();$final.cleanup_input_release='SKIPPED_NO_AUTHORITY';$final.cleanup_up_attempted=$false;$final.cleanup_up_sent=$false;$final.raw_up_observed=$false;$final.left_button_high_bit=$false;$final.final_capture_hwnd=0;$final.final_cursor=@(10,10);$null=Add-FixGPrefixRow $rows 'cleanup_final' $final 'cleanup'
    Add-FixGPrefixAck $rows 'final'
    $null=Add-FixGPrefixRow $rows 'winevent_hook_removed' @{hook=96;source_hwnd=99;source_pid=40001;source_tid=40002;remove_tid=40002;remove_success=$true;error=0} 'cleanup'
    $null=Add-FixGPrefixRow $rows 'receiver_shutdown' @{receiver_sequence=2;receiver_qpc=([long]1749903002082+([long]$rows.Count+1)*100-1);receiver_hwnd=97;receiver_pid=40003;receiver_tid=40004;registration_removed=$true;window_destroyed=$true} 'cleanup'
    $null=Add-FixGPrefixRow $rows 'shutdown' @{result='BLOCKED';cursor_restored=$false;owned_window_destroyed=$true;guard_window_destroyed=$true;receiver_stopped=$true;external_windows_touched=$false;takeover_geometry_writes=0;winevent_hook_removed=$true;native_drag_after_winevent_end=0;input_shield_created=$false;input_shield_destroyed=$true;input_shield_activated=$false;source_acceptance_sequence=0;run_nonce=[long]99112233;gesture_result='BLOCKED_BY_FOREIGN_INPUT_CAPTURE';cleanup_result='NOT_RUN';fixture_result='BLOCKED'} 'cleanup'
    return ,@($rows.ToArray())
}
function Replace-FixGPrefixRowType($Rows,[string]$Type){$Rows[6].type=$Type;return ,$Rows}

foreach($operation in @('Move','BottomResize')){foreach($mode in @('controlled_abort','normal')){
    $rows=New-FixGPrefixFixture $operation $mode;$v=Test-FixFInputReliabilityRecords $rows -AllowSynthetic
    Check-FixGPrefix ($v.EvidenceIntegrity -ceq 'VALID' -and $v.PrefixValidation -ceq 'VERIFIED_BLOCKED_BEFORE_INPUT' -and $v.Result -ceq 'BLOCKED' -and $v.ExecutionResult -ceq 'BLOCKED' -and $v.FixtureResult -ceq 'BLOCKED' -and $v.GestureResult -ceq 'NOT_RUN' -and $v.CleanupResult -ceq 'NOT_RUN' -and $v.TakeoverAcceptance -ceq 'NOT_RUN' -and -not $v.ContractVerified -and -not $v.InputAttempted -and -not $v.TestDownPending -and $null -eq $v.FinalLeftDown -and $v.NativeLeftFlag -eq $false -and $v.FinalButtonObservation -ceq 'REQUIRES_INDEPENDENT_POST' -and $v.SyntheticFixture) "$operation $mode verified prefix never fixture PASS or reliable final button proof"
    Check-FixGPrefix ($v.EvidenceSequenceReferences.ActivationFence -eq 12 -and $v.EvidenceSequenceReferences.HookRemoved -eq 26 -and $v.EvidenceSequenceReferences.ReceiverShutdown -eq 27 -and $v.ResourceProof.IndependentSourceDestroyReceipt -ceq 'NOT_AVAILABLE') "$operation $mode references actual receipts and admits absent destroy receipt"
}}
$rows=New-FixGPrefixFixture -WithQueryDetails;$v=Test-FixFInputReliabilityRecords $rows -AllowSynthetic;Check-FixGPrefix ($v.PrefixValidation -ceq 'VERIFIED_BLOCKED_BEFORE_INPUT') 'additive actual queried tuple supports nonzero capture'
Reject-FixGPrefix {Test-FixFInputReliabilityRecords (New-FixGPrefixFixture)} 'synthetic prefix cannot become empirical'
foreach($type in @('input','activation_input','activation_release','cleanup_release','abort_cleanup_release','ledger_input_intent','test_down_ledger','native_button_down','ENTER','controlled_abort_fault','cancel_begin','writer_begin','handoff_begin','source_final_acceptance','input_shield_created','raw_input','receiver_error','winevent_error','foreground_ready','source_work_post_skipped')){
    $bad=Replace-FixGPrefixRowType (New-FixGPrefixFixture) $type
    Reject-FixGPrefix {Test-FixFInputReliabilityRecords $bad -AllowSynthetic} "complete contiguous prefix rejects $type even without later diagnostic"
}
foreach($change in @('shutdown_missing','sequence_gap','duplicate_startup','duplicate_source','nonce','source_pid','gesture_id','guard_tid','guard_coverage','guard_forged_plan','receiver_identity','receiver_serial','receiver_not_removed','hook_not_removed','hook_identity','source_not_destroyed','topmost_not_restored','topmost_receipt_failed','query_failed','foreground_changed','capture_zero','capture_source','capture_unknown','menu','move_size','modal_flags','string_gui_bool','down_pending','activation_move_summary','activation_down_summary','activation_up_summary','ack_wrong_nonce','ack_active_writer','ack_pending','ack_missing','early_teardown','cleanup_sent','fixture_pass','cleanup_pass','acceptance_count','unknown_event')){
    $bad=New-FixGPrefixFixture
    switch($change){
        shutdown_missing {$bad=$bad[0..26]}
        sequence_gap {$bad[11].sequence++}
        duplicate_startup {$bad[6].type='startup'}
        duplicate_source {$bad[6].type='owned'}
        nonce {$bad[8].actual_source_nonce++}
        source_pid {$bad[8].pid++}
        gesture_id {$bad[0].gesture_id=2}
        guard_tid {$bad[2].tid++}
        guard_coverage {$bad[2].positioning=@(50,50,120,120)}
        guard_forged_plan {$bad[2].planned_move_bounds[2]++}
        receiver_identity {$bad[26].receiver_pid++}
        receiver_serial {$bad[26].receiver_sequence++}
        receiver_not_removed {$bad[26].registration_removed=$false}
        hook_not_removed {$bad[25].remove_success=$false}
        hook_identity {$bad[25].hook++}
        source_not_destroyed {$bad[27].owned_window_destroyed=$false}
        topmost_not_restored {$bad[13].topmost_restored=$false}
        topmost_receipt_failed {$bad[12].success=$false}
        query_failed {$bad[11].gui_query_succeeded=$false}
        foreground_changed {$bad[11].foreground_snapshot_stable=$false}
        capture_zero {$bad[11].capture_hwnd=0}
        capture_source {$bad[11].capture_hwnd=99}
        capture_unknown {$bad[11].capture_hwnd=$null}
        menu {$bad[11].menu_owner_hwnd=102}
        move_size {$bad[11].move_size_hwnd=102}
        modal_flags {$bad[11].gui_flags=2}
        string_gui_bool {$bad[11].gui_query_succeeded='true'}
        down_pending {$bad[18].test_down_owned=$true}
        activation_move_summary {$bad[13].sendinput_move_success=$true}
        activation_down_summary {$bad[13].sendinput_down_success=$true}
        activation_up_summary {$bad[13].sendinput_up_success=$true}
        ack_wrong_nonce {$bad[16].actual_source_nonce++}
        ack_active_writer {$bad[16].active_operation_id=1}
        ack_pending {$bad[23].pending_raw_count=1}
        ack_missing {$bad[16].acknowledged=$false}
        early_teardown {$bad[25].sequence=$bad[23].sequence}
        cleanup_sent {$bad[21].cleanup_up_sent=$true}
        fixture_pass {$bad[27].fixture_result='PASS'}
        cleanup_pass {$bad[27].cleanup_result='PASS'}
        acceptance_count {$bad[27].source_acceptance_sequence=1}
        unknown_event {$bad[6].type='unhandled_bootstrap_event'}
    }
    Reject-FixGPrefix {Test-FixFInputReliabilityRecords $bad -AllowSynthetic} "bootstrap rejection: $change"
}
foreach($change in @('wrong_query_tid','query_error','query_time_reversed','tuple_changed','forged_subreason','missing_query_qpc','missing_contract_with_changed_tuple','null_contract_with_changed_tuple')){
    $bad=New-FixGPrefixFixture -WithQueryDetails
    switch($change){wrong_query_tid {$bad[11].gui_query_tid++};query_error {$bad[11].gui_query_error=5};query_time_reversed {$bad[11].gui_query_finish_qpc=$bad[11].gui_query_start_qpc-1};tuple_changed {$bad[11].foreground_after_pid++};forged_subreason {$bad[11].gui_failure_subreason='GUI_QUERY_FAILED'};missing_query_qpc {$bad[11].PSObject.Properties.Remove('gui_query_start_qpc')};missing_contract_with_changed_tuple {$bad[11].PSObject.Properties.Remove('activation_diagnostic_contract');$bad[11].foreground_after_pid++};null_contract_with_changed_tuple {$bad[11].activation_diagnostic_contract=$null;$bad[11].foreground_after_pid++}}
    Reject-FixGPrefix {Test-FixFInputReliabilityRecords $bad -AllowSynthetic} "additive query rejects $change"
}
foreach($badValue in @('1749903002082',[double]1749903002082,[decimal]9223372036854775808,-1)){$bad=New-FixGPrefixFixture;$bad[11].qpc=$badValue;Reject-FixGPrefix {Test-FixFInputReliabilityRecords $bad -AllowSynthetic} 'QPC type/range preserved'}
Write-Host "Fix G offline prefix checks: $script:fixGChecks; failures: $($script:fixGFailures.Count)"
foreach($failure in $script:fixGFailures){Write-Host "FAIL: $failure"}
if($script:fixGFailures.Count){exit 1}
Write-Host 'PASS: independent synthetic bootstrap prefix validation; empirical cleanup/takeover NOT_RUN'
exit 0
