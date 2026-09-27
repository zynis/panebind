# Pure in-memory model tests. No GUI/native probe, evidence file, input,
# upstream inspection, or function override. Complete v5 wires below are
# freshly generated synthetic models, never historical or empirical evidence.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-native-abort-validation.ps1')
$script:fixFChecks=0
$script:fixFFailures=[Collections.Generic.List[string]]::new()
$script:fixFBase=[long]1134816073663
function Check-FixFModel([bool]$Condition,[string]$Name){
    ++$script:fixFChecks
    if(-not $Condition){$script:fixFFailures.Add($Name)}
}
function Reject-FixFModel([scriptblock]$Work,[string]$Name){
    $rejected=$false
    try{$null=& $Work}catch{$rejected=$true}
    Check-FixFModel $rejected $Name
}
function Deny-FixFModel([scriptblock]$Work,[string]$Name){
    $denied=$false
    try{$result=& $Work;$denied=$result -ne $true}catch{$denied=$true}
    Check-FixFModel $denied $Name
}
function Copy-FixFModel($Value){return ($Value|ConvertTo-Json -Depth 30|ConvertFrom-Json)}
function New-FixFStartup {
    return [pscustomobject][ordered]@{
        schema='r1c4b-takeover-owned/v5';sequence=1;qpc=$script:fixFBase;type='startup';gesture=0
        pid=40001;ui_tid=40002;run_nonce=[long]99112233;gesture_id=1;qpc_frequency=[long]10000000
        operation='Move';test_mode='controlled_abort';mode='free_takeover'
        fence_contract='ordered_failure_snapshot_v1';cleanup_contract='owned_native_abort_v1';abort_cleanup_tag=0x50424655
        foreground_contract='verified_global_foreground_v2';handoff_contract='winevent_end_barrier_v1'
        diagnostic_contract='separated_authority_v1';authority_contract='product_gesture_v1';input_isolation_contract='post_end_shield_v1';input_correlation='actual_absolute_receipt_v1'
        human_input=$false;real_explorer=$false;sendinput_in_probe=$true;takeover_geometry_writes=0
        synthetic_fixture=$true;event_scope='gesture';event_acceptance_eligible=$false
    }
}
function New-FixFEnvelopeModel {
    return @((New-FixFStartup),[pscustomobject]@{
        schema='r1c4b-takeover-owned/v5';sequence=2;qpc=($script:fixFBase+1);type='shutdown';gesture=1
        event_scope='gesture';event_acceptance_eligible=$false
    })
}
function New-FixFOtherStates {
    return [pscustomobject][ordered]@{ctrl=$false;shift=$false;alt=$false;lwin=$false;rwin=$false;rbutton=$false;mbutton=$false;xbutton1=$false;xbutton2=$false;escape=$false}
}
function New-FixFLedgerModel {
    $b=$script:fixFBase
    $rows=@(
        (New-FixFStartup),
        [pscustomobject]@{type='owned';sequence=2;qpc=($b+5);hwnd=99;pid=40001;tid=40002;run_nonce=[long]99112233;actual_source_nonce=[long]99112233},
        [pscustomobject]@{type='ledger_input_intent';sequence=3;qpc=($b+10);intent_purpose='native_down';flags=2;run_nonce=[long]99112233;input_tag=0x50424D41;receiver_watermark=1},
        [pscustomobject]@{type='input';sequence=4;qpc=($b+41);gesture=1;flags=2;actual_mouse_flags=2;input_tag=0x50424D41;ledger_intent_sequence=3;receiver_watermark=1;injection_start_qpc=($b+20);injection_return_qpc=($b+40);sent=1;error=0},
        [pscustomobject]@{type='native_button_down';sequence=5;qpc=($b+50);receipt_qpc=($b+50);gesture=1;target=99;cursor=@(13,23);cursor_success=$true;hit_test=2},
        [pscustomobject]@{type='ENTER';sequence=6;qpc=($b+65);receipt_qpc=($b+60);gesture=1},
        [pscustomobject]@{type='raw_input';sequence=7;qpc=($b+70);receiver_sequence=2;receiver_qpc=($b+30);ledger_observed_qpc=($b+69);button_flags=1;test_tag_matches=$true;cleanup_tag_matches=$false;extra_information=0x50424D41;device_handle_present=$false;raw_scope='gesture';cursor_sampled=$false}
    )
    $ledger=[pscustomobject]@{
        sequence=9;qpc=($b+130);query_start_qpc=($b+110);query_finish_qpc=($b+125);ledger_snapshot_qpc=($b+124);ledger_receiver_sequence=2
        down_input_sequence=4;down_start_qpc=($b+20);down_return_qpc=($b+40);receiver_watermark=1;down_sent=$true
        raw_down_receiver_sequence=2;raw_down_receiver_qpc=($b+30);raw_down_matches=$true
        native_down_sequence=5;native_down_qpc=($b+50);native_down_matches=$true
        native_enter_sequence=6;native_enter_qpc=($b+60);native_enter_matches=$true
        non_test_movements=0;non_test_button_transitions=0;unmatched_button_transitions=0;matching_up_seen=$false;up_command_sent=$false;ledger_pending=$true
    }
    return [pscustomobject]@{Rows=$rows;Ledger=$ledger;Startup=$rows[0]}
}
function New-FixFQuiescenceModel {
    $b=$script:fixFBase
    $rows=@(
        [pscustomobject]@{type='owned';sequence=2;qpc=($b+5);hwnd=99},
        [pscustomobject]@{type='abort_quiescence_request';sequence=7;qpc=($b+80);phase='initial';request_qpc=($b+75);posted=$true;message=32788;source_hwnd=99;source_pid=40001;source_tid=40002;run_nonce=[long]99112233},
        [pscustomobject]@{type='abort_writer_quiescence';sequence=8;qpc=($b+100);phase='owner_ack';cleanup_phase='initial';request_sequence=7;ack_tid=40002;ack_qpc=($b+95);acknowledged=$true;active_operation_id=0;pending_raw_count=0;motion_pending=$false;notice_posted=$false;acceptance_retired=$true;takeover_scope=$false;native_calls=0;source_hwnd=99;source_pid=40001;source_tid=40002;run_nonce=[long]99112233},
        [pscustomobject]@{type='abort_quiescence_wait';sequence=9;qpc=($b+105);phase='initial';request_sequence=7;ack_sequence=8;timeout_ms=2000;wait_result=0;quiescent=$true},
        [pscustomobject]@{type='abort_quiescence_request';sequence=10;qpc=($b+180);phase='final';request_qpc=($b+175);posted=$true;message=32788;source_hwnd=99;source_pid=40001;source_tid=40002;run_nonce=[long]99112233},
        [pscustomobject]@{type='abort_writer_quiescence';sequence=11;qpc=($b+200);phase='owner_ack';cleanup_phase='final';request_sequence=10;ack_tid=40002;ack_qpc=($b+195);acknowledged=$true;active_operation_id=0;pending_raw_count=0;motion_pending=$false;notice_posted=$false;acceptance_retired=$true;takeover_scope=$false;native_calls=0;source_hwnd=99;source_pid=40001;source_tid=40002;run_nonce=[long]99112233},
        [pscustomobject]@{type='abort_quiescence_wait';sequence=12;qpc=($b+205);phase='final';request_sequence=10;ack_sequence=11;timeout_ms=2000;wait_result=0;quiescent=$true}
    )
    foreach($ack in @($rows|Where-Object type -ceq 'abort_writer_quiescence')){foreach($pair in @(@('actual_source_pid',40001),@('actual_source_tid',40002),@('actual_source_nonce',[long]99112233),@('own_identity',$true))){$ack|Add-Member -NotePropertyName $pair[0] -NotePropertyValue $pair[1]}}
    return $rows
}
function New-FixFAuthorityModel {
    $model=New-FixFLedgerModel;$a=Copy-FixFModel $model.Ledger
    $facts=[ordered]@{
        phase='initial';source_hwnd=99;source_pid=40001;source_tid=40002;run_nonce=[long]99112233
        actual_source_pid=40001;actual_source_tid=40002;actual_source_nonce=[long]99112233
        fixture_scope_active=$true;own_identity=$true;desktop_ready=$true;source_visible=$true
        foreground_hwnd=99;foreground_pid=40001;foreground_tid=40002;foreground_matches=$true
        gui_query_succeeded=$true;capture_hwnd=99;move_size_hwnd=99;menu_owner_hwnd=0;gui_flags=2;expected_native_mode=$true;menu_clear=$true
        cursor_success=$true;cursor=@(13,23);cursor_root=99;root_is_source=$true;left_down=$true
        other_states=(New-FixFOtherStates);other_input_clear=$true;receiver_healthy=$true;log_healthy=$true
        foreign_capture_transferred=$false;mouse_buttons_swapped=$false;input_mapping_supported=$true
        writer_quiescent=$true;acceptance_retired=$true;api_boundary_stable=$true;eligible=$true
        acceptance_eligible=$false;event_scope='cleanup';quiescence_ack_sequence=8;initial_authority_sequence=0
    }
    foreach($name in $facts.Keys){$a|Add-Member -NotePropertyName $name -NotePropertyValue $facts[$name]}
    return [pscustomobject]@{Rows=$model.Rows;Startup=$model.Startup;Authority=$a;Ack=@(New-FixFQuiescenceModel|Where-Object type -ceq 'abort_writer_quiescence')[0]}
}
function New-FixFFenceModel {
    $b=$script:fixFBase;$names=@('OWNER_RUNNING','LOG_HEALTHY','DESKTOP_READY','SOURCE_IDENTITY','FOREGROUND','NO_FOREIGN_CAPTURE_TRANSFER','CURSOR_QUERY','CURSOR_POSITION','LEFT_STATE','OTHER_INPUT_CLEAR','GUI_QUERY','CANCELLED_GUI_SAFE','INPUT_ROOT_OWNED','CAPTURE')
    $queries=[ordered]@{};$offset=151
    foreach($name in @('desktop','identity','foreground','cursor','buttons','gui','root')){$queries[$name]=[pscustomobject]@{evaluated=$true;start_qpc=($b+$offset);finish_qpc=($b+$offset+2)};$offset+=4}
    $predicates=[ordered]@{};foreach($name in $names){$predicates[$name]=if($name -in @('CANCELLED_GUI_SAFE','INPUT_ROOT_OWNED')){'NOT_EVALUATED'}else{'PASS'}}
    $d=[pscustomobject]@{
        type='input_fence_diagnostic';sequence=10;qpc=($b+220);gesture=1;operation='Move'
        query_start_qpc=($b+150);query_finish_qpc=($b+200);queries=[pscustomobject]$queries
        source_hwnd=99;source_pid=40001;source_tid=40002;run_nonce=[long]99112233
        actual_source_pid=40001;actual_source_tid=40002;actual_source_nonce=[long]99112233
        owner_running=$true;log_healthy=$true;desktop_ready=$true;source_identity=$true;foreign_capture_clear=$true
        foreground_hwnd=99;cursor_query_succeeded=$true;actual_cursor=@(13,23);expected_cursor=@(13,23)
        expected_left_down=$true;left_down=$true;other_states=(New-FixFOtherStates)
        gui_query_succeeded=$true;capture_hwnd=99;menu_owner_hwnd=0;move_size_hwnd=99;gui_flags=2;root=99
        priming=$false;post_cancel=$false;cancel_pending=$false
        predicates=[pscustomobject]$predicates;required_predicates=@($names[0..10])+@('CAPTURE')
        first_failed_predicate='NONE';failed_predicates=@();passed=$true;failure_reason='none';failure_subreason='NONE'
    }
    $rows=@((New-FixFStartup),[pscustomobject]@{type='owned';sequence=2;hwnd=99},$d,[pscustomobject]@{type='input_fence';sequence=11;diagnostic_sequence=10;cursor=@(13,23);left_down=$true})
    return [pscustomobject]@{Rows=$rows;Diagnostic=$d}
}

# State and numeric primitives: UNKNOWN is not false/PASS; real QPC magnitude.
Check-FixFModel ((Get-FixFState $null) -ceq 'UNKNOWN') 'null predicate remains UNKNOWN'
Check-FixFModel ((Get-FixFState $false) -ceq 'FAIL') 'false predicate FAIL'
Check-FixFModel ((Get-FixFState $true) -ceq 'PASS') 'true predicate PASS'
Reject-FixFModel {Get-FixFState 'true'} 'string predicate cannot be truthy'
$other=New-FixFOtherStates;Check-FixFModel (Test-FixFOtherClear $other) 'all other inputs clear'
$other.ctrl=$null;Check-FixFModel ($null -eq (Test-FixFOtherClear $other)) 'unknown modifier stays unknown'
$other.shift=$true;Check-FixFModel ((Test-FixFOtherClear $other) -eq $false) 'known active modifier denies despite unknown'
Reject-FixFModel {Test-FixFOtherClear ([pscustomobject]@{ctrl='false'})} 'typed modifier facts required'
foreach($value in @([long]2147483647,[long]2147483648,[long]3000000000,[long]900000000000,[long]1134816073663,[long]1500000000000,[long]9000000000000)){
    $tree=[pscustomobject]@{nested=[pscustomobject]@{receipt_qpc=$value;receiver_sequence=[long]3}}
    Assert-FixFNumericTree $tree;Check-FixFModel $true ('Int64 nested QPC '+$value)
}
foreach($value in @(-1,1.5,[double]3000000000,[decimal]9223372036854775808,'1134816073663',$true,@(1,2))){
    $tree=[pscustomobject]@{nested=[pscustomobject]@{receipt_qpc=$value}}
    Reject-FixFModel {Assert-FixFNumericTree $tree} ('invalid raw QPC type '+$value.GetType().Name)
}

# Envelope-only models intentionally contain no fake complete runtime proof.
$envelope=New-FixFEnvelopeModel;$startup=Test-FixFEnvelope $envelope -AllowSynthetic
Check-FixFModel ($startup.qpc -eq $script:fixFBase) 'explicit v5 synthetic envelope at real QPC'
Reject-FixFModel {Test-FixFEnvelope $envelope} 'synthetic cannot become empirical'
foreach($change in @('legacy_schema','missing_contract','cleanup_acceptance','string_scope_bool','reverse_clock','fraction_sequence','unknown_record','normal_product_change')){
    $rows=Copy-FixFModel $envelope
    switch($change){
        legacy_schema {$rows[0].schema='r1c4b-takeover-owned/v4'}
        missing_contract {$rows[0].PSObject.Properties.Remove('cleanup_contract')}
        cleanup_acceptance {$rows[1].event_scope='cleanup';$rows[1].event_acceptance_eligible=$true}
        string_scope_bool {$rows[1].event_acceptance_eligible='false'}
        reverse_clock {$rows[1].qpc=$script:fixFBase-1}
        fraction_sequence {$rows[1].sequence=2.5}
        unknown_record {$rows[1].type='guessed_end'}
        normal_product_change {$rows[0].authority_contract='weakened_product'}
    }
    Reject-FixFModel {Test-FixFEnvelope $rows -AllowSynthetic} ('v5 envelope rejects '+$change)
}

# Real receipt may precede INPUT parent log, but must bind the actual API.
$ledger=New-FixFLedgerModel
Check-FixFModel (Test-FixFPendingLedger $ledger.Ledger $ledger.Rows $ledger.Startup) 'actual Raw DOWN within API interval has pending ownership'
foreach($change in @('missing_raw','missing_native_enter','wrong_raw_ref','foreign_transition','cleanup_tag_down','future_api_return','raw_after_snapshot','unconfirmed_down','up_command_already_sent','matching_up')){
    $m=Copy-FixFModel $ledger
    switch($change){
        missing_raw {$m.Rows=@($m.Rows|Where-Object type -cne 'raw_input')}
        missing_native_enter {$m.Rows=@($m.Rows|Where-Object type -cne 'ENTER')}
        wrong_raw_ref {$m.Ledger.raw_down_receiver_sequence=88}
        foreign_transition {$r=Copy-FixFModel $m.Rows[-1];$r.sequence=8;$r.receiver_sequence=3;$r.receiver_qpc=$script:fixFBase+90;$r.ledger_observed_qpc=$script:fixFBase+91;$r.button_flags=1;$r.test_tag_matches=$false;$r.extra_information=0;$m.Rows+=,$r}
        cleanup_tag_down {$r=Copy-FixFModel $m.Rows[-1];$r.sequence=8;$r.receiver_sequence=3;$r.receiver_qpc=$script:fixFBase+90;$r.ledger_observed_qpc=$script:fixFBase+91;$r.button_flags=1;$r.test_tag_matches=$false;$r.cleanup_tag_matches=$true;$r.extra_information=0x50424655;$r.raw_scope='cleanup';$m.Rows+=,$r}
        future_api_return {$m.Rows[3].injection_return_qpc=$script:fixFBase+120;$m.Ledger.down_return_qpc=$script:fixFBase+120}
        raw_after_snapshot {$m.Rows[6].ledger_observed_qpc=$script:fixFBase+128;$m.Rows[6].qpc=$script:fixFBase+129}
        unconfirmed_down {$m.Rows[3].sent=0;$m.Ledger.down_sent=$false;$m.Ledger.ledger_pending=$false}
        up_command_already_sent {$m.Ledger.up_command_sent=$true;$m.Ledger.ledger_pending=$false}
        matching_up {$r=Copy-FixFModel $m.Rows[-1];$r.sequence=8;$r.receiver_sequence=3;$r.receiver_qpc=$script:fixFBase+90;$r.ledger_observed_qpc=$script:fixFBase+91;$r.button_flags=2;$m.Rows+=,$r;$m.Ledger.matching_up_seen=$true;$m.Ledger.ledger_pending=$false}
    }
    Deny-FixFModel {Test-FixFPendingLedger $m.Ledger $m.Rows $m.Startup} ('pending DOWN denies '+$change)
}

$quiescence=New-FixFQuiescenceModel;$s=New-FixFStartup
$ack=Test-FixFQuiescence $quiescence $s
Check-FixFModel ($ack.initial.ack_tid -eq $s.ui_tid -and $ack.final.ack_tid -eq $s.ui_tid) 'two real owner ACK rounds, zero active/pending work'
foreach($change in @('missing_ack','active_writer','pending_motion','wrong_owner','timeout','string_posted','string_ack','string_retired','string_active_count','fraction_pending_count')){
    $rows=Copy-FixFModel $quiescence
    $request=@($rows|Where-Object type -ceq 'abort_quiescence_request')[0]
    $ownerAck=@($rows|Where-Object type -ceq 'abort_writer_quiescence')[0]
    $waitRow=@($rows|Where-Object type -ceq 'abort_quiescence_wait')[0]
    switch($change){
        missing_ack {$rows=@($rows|Where-Object type -cne 'abort_writer_quiescence')}
        active_writer {$ownerAck.active_operation_id=1}
        pending_motion {$ownerAck.motion_pending=$true}
        wrong_owner {$ownerAck.ack_tid=999}
        timeout {$waitRow.wait_result=258}
        string_posted {$request.posted='false'}
        string_ack {$ownerAck.acknowledged='false'}
        string_retired {$ownerAck.acceptance_retired='false'}
        string_active_count {$ownerAck.active_operation_id='0'}
        fraction_pending_count {$ownerAck.pending_raw_count=[double]0}
    }
    Reject-FixFModel {Test-FixFQuiescence $rows $s} ('quiescence rejects '+$change)
}

$model=New-FixFAuthorityModel
Check-FixFModel (Test-FixFAbortAuthority $model.Authority $model.Rows $model.Startup $model.Ack $null) 'exact active-native cleanup authority'
$m=Copy-FixFModel $model;$m.Authority.cursor=@(200,300)
Check-FixFModel (Test-FixFAbortAuthority $m.Authority $m.Rows $m.Startup $m.Ack $null) 'old planned cursor is not a cleanup requirement when actual root remains source'
foreach($change in @('foreign_capture','foreign_fg','foreign_root','nonce','menu','desktop','receiver','foreign_capture_history','released','unknown_left','string_left','string_other','active_writer','retired_fixture','polluted_scope')){
    $m=Copy-FixFModel $model
    switch($change){
        foreign_capture {$m.Authority.capture_hwnd=123}
        foreign_fg {$m.Authority.foreground_hwnd=123}
        foreign_root {$m.Authority.cursor_root=123}
        nonce {$m.Authority.actual_source_nonce=123}
        menu {$m.Authority.menu_owner_hwnd=123}
        desktop {$m.Authority.desktop_ready=$false;$m.Authority.eligible=$false}
        receiver {$m.Authority.receiver_healthy=$false;$m.Authority.eligible=$false}
        foreign_capture_history {$m.Authority.foreign_capture_transferred=$true;$m.Authority.eligible=$false}
        released {$m.Authority.left_down=$false;$m.Authority.eligible=$false}
        unknown_left {$m.Authority.left_down=$null;$m.Authority.eligible=$false}
        string_left {$m.Authority.left_down='true'}
        string_other {$m.Authority.other_input_clear='true'}
        active_writer {$m.Authority.writer_quiescent=$false;$m.Authority.eligible=$false}
        retired_fixture {$m.Authority.fixture_scope_active=$false;$m.Authority.eligible=$false}
        polluted_scope {$m.Authority.acceptance_eligible=$true}
    }
    Deny-FixFModel {Test-FixFAbortAuthority $m.Authority $m.Rows $m.Startup $m.Ack $null} ('abort authority denies '+$change)
}
$m=Copy-FixFModel $model;$initial=Copy-FixFModel $m.Authority
$m.Authority.phase='api_boundary';$m.Authority.sequence=10;$m.Authority.qpc=$script:fixFBase+160;$m.Authority.query_start_qpc=$script:fixFBase+140;$m.Authority.query_finish_qpc=$script:fixFBase+155;$m.Authority.ledger_snapshot_qpc=$script:fixFBase+154;$m.Authority.initial_authority_sequence=$initial.sequence
Check-FixFModel (Test-FixFAbortAuthority $m.Authority $m.Rows $m.Startup $m.Ack $initial) 'second fresh unchanged API boundary'
$m.Authority.capture_hwnd=0;$m.Authority.expected_native_mode=$false;$m.Authority.api_boundary_stable=$false;$m.Authority.eligible=$false
Deny-FixFModel {Test-FixFAbortAuthority $m.Authority $m.Rows $m.Startup $m.Ack $initial} 'API boundary change denies without refresh/retry'

# Mandatory terminal references are separate facts, never inferred from sent1.
foreach($type in @('abort_cleanup_raw_up','EXIT','abort_cleanup_final')){
    Reject-FixFModel {Get-FixFOne @() $type} ('missing mandatory '+$type)
    Reject-FixFModel {Get-FixFRef @([pscustomobject]@{sequence=10;type='raw_preflight_complete'}) ([long]10) $type} ('wrong old/preflight terminal ref '+$type)
}
Reject-FixFModel {Test-FixFNormalOwnedRecords @((New-FixFStartup),[pscustomobject]@{type='abort_cleanup_release';gesture=1},[pscustomobject]@{type='shutdown';gesture=1})} 'cleanup cannot pollute original normal acceptance'

# Actual ordered-query wire. Successful original branches remain unchanged;
# failures retain facts and cannot acquire a success alias.
$fence=New-FixFFenceModel
Check-FixFModel (Test-FixFFenceDiagnostic $fence.Diagnostic $fence.Rows) 'complete ordered fence success'
$f=Copy-FixFModel $fence;$f.Rows=@($f.Rows|Where-Object type -cne 'input_fence');$f.Diagnostic.actual_cursor=@(18,23);$f.Diagnostic.predicates.CURSOR_POSITION='FAIL';$f.Diagnostic.first_failed_predicate='CURSOR_POSITION';$f.Diagnostic.failed_predicates=@('CURSOR_POSITION');$f.Diagnostic.passed=$false;$f.Diagnostic.failure_reason='BLOCKED_BY_INPUT_INTERFERENCE';$f.Diagnostic.failure_subreason='CURSOR_DEVIATION'
Check-FixFModel ((Test-FixFFenceDiagnostic $f.Diagnostic $f.Rows) -eq $false) 'cursor-deviation failure remains diagnosable without success alias'
$f.Diagnostic.left_down=$false;$f.Diagnostic.predicates.LEFT_STATE='FAIL';$f.Diagnostic.failed_predicates=@('CURSOR_POSITION','LEFT_STATE')
Check-FixFModel ((Test-FixFFenceDiagnostic $f.Diagnostic $f.Rows) -eq $false) 'all known failures preserved in original first-failure order'
foreach($change in @('forged_pass','incomplete_failures','first_failure_swap','alias_after_failure')){
    $m=Copy-FixFModel $f
    switch($change){
        forged_pass {$m.Diagnostic.passed=$true}
        incomplete_failures {$m.Diagnostic.failed_predicates=@('CURSOR_POSITION')}
        first_failure_swap {$m.Diagnostic.first_failed_predicate='LEFT_STATE'}
        alias_after_failure {$m.Rows+=,[pscustomobject]@{type='input_fence';sequence=11;diagnostic_sequence=10;cursor=@(18,23);left_down=$false}}
    }
    Reject-FixFModel {Test-FixFFenceDiagnostic $m.Diagnostic $m.Rows} ('failed fence rejects '+$change)
}
$f=Copy-FixFModel $fence;$f.Rows=@($f.Rows|Where-Object type -cne 'input_fence');$f.Diagnostic.cursor_query_succeeded=$false;$f.Diagnostic.actual_cursor=$null;$f.Diagnostic.root=$null;$f.Diagnostic.queries.root.evaluated=$false;$f.Diagnostic.queries.root.start_qpc=$null;$f.Diagnostic.queries.root.finish_qpc=$null;$f.Diagnostic.predicates.CURSOR_QUERY='FAIL';$f.Diagnostic.predicates.CURSOR_POSITION='UNKNOWN';$f.Diagnostic.predicates.CAPTURE='UNKNOWN';$f.Diagnostic.first_failed_predicate='CURSOR_QUERY';$f.Diagnostic.failed_predicates=@('CURSOR_QUERY');$f.Diagnostic.passed=$false;$f.Diagnostic.failure_reason='BLOCKED_BY_INPUT_INTERFERENCE';$f.Diagnostic.failure_subreason='CURSOR_QUERY_FAILED'
Check-FixFModel ((Test-FixFFenceDiagnostic $f.Diagnostic $f.Rows) -eq $false) 'failed cursor query: known failure, unavailable position UNKNOWN, root unqueried'
$f=Copy-FixFModel $fence;$f.Rows=@($f.Rows|Where-Object type -cne 'input_fence');$f.Diagnostic.gui_query_succeeded=$false;$f.Diagnostic.capture_hwnd=$null;$f.Diagnostic.menu_owner_hwnd=$null;$f.Diagnostic.move_size_hwnd=$null;$f.Diagnostic.gui_flags=$null;$f.Diagnostic.predicates.GUI_QUERY='FAIL';$f.Diagnostic.predicates.CAPTURE='UNKNOWN';$f.Diagnostic.first_failed_predicate='GUI_QUERY';$f.Diagnostic.failed_predicates=@('GUI_QUERY');$f.Diagnostic.passed=$false;$f.Diagnostic.failure_reason='BLOCKED_BY_GUI_STATE';$f.Diagnostic.failure_subreason='GUI_QUERY'
Check-FixFModel ((Test-FixFFenceDiagnostic $f.Diagnostic $f.Rows) -eq $false) 'failed GUI query never defaults capture/mode to clear'
foreach($change in @('empty_queries','missing_query','unevaluated_known','reverse_query','foreign_source','wrong_nonce','wrong_predicate_order','string_passed','integer_passed')){
    $f=Copy-FixFModel $fence
    switch($change){
        empty_queries {$f.Diagnostic.queries=[pscustomobject]@{}}
        missing_query {$f.Diagnostic.queries.PSObject.Properties.Remove('gui')}
        unevaluated_known {$f.Diagnostic.queries.cursor.evaluated=$false;$f.Diagnostic.queries.cursor.start_qpc=$null;$f.Diagnostic.queries.cursor.finish_qpc=$null}
        reverse_query {$f.Diagnostic.queries.identity.finish_qpc=$f.Diagnostic.queries.identity.start_qpc-1}
        foreign_source {$f.Diagnostic.source_hwnd=123;$f.Diagnostic.foreground_hwnd=123;$f.Diagnostic.capture_hwnd=123;$f.Diagnostic.move_size_hwnd=123;$f.Diagnostic.root=123}
        wrong_nonce {$f.Diagnostic.actual_source_nonce=999}
        wrong_predicate_order {$f.Diagnostic.predicates=[pscustomobject]@{CAPTURE='PASS'}}
        string_passed {$f.Diagnostic.passed='true'}
        integer_passed {$f.Diagnostic.passed=1}
    }
    Reject-FixFModel {Test-FixFFenceDiagnostic $f.Diagnostic $f.Rows} ('fence rejects '+$change)
}

# A successful UP command with no receipt is still irrevocably single-shot.
$m=New-FixFLedgerModel
$m.Rows+=,[pscustomobject]@{type='input';sequence=8;qpc=($script:fixFBase+100);gesture=1;flags=4;sent=1;error=0;input_tag=0x50424D41;receiver_watermark=2;injection_start_qpc=($script:fixFBase+90);injection_return_qpc=($script:fixFBase+95)}
$m.Ledger.up_command_sent=$true;$m.Ledger.ledger_pending=$false
Check-FixFModel ((Test-FixFPendingLedger $m.Ledger $m.Rows $m.Startup) -eq $false) 'actual UP command sent without Raw UP forbids another cleanup UP'
$late=Copy-FixFModel $m;$lateRaw=Copy-FixFModel $late.Rows[6];$lateRaw.sequence=10;$lateRaw.qpc=$script:fixFBase+151;$lateRaw.receiver_sequence=3;$lateRaw.receiver_qpc=$script:fixFBase+94;$lateRaw.ledger_observed_qpc=$script:fixFBase+150;$lateRaw.button_flags=2;$late.Rows+=,$lateRaw
Check-FixFModel ((Test-FixFPendingLedger $late.Ledger $late.Rows $late.Startup) -eq $false) 'early child UP, late parent observation: snapshot matching false but API already sent still forbids repeat'

# Import only explicitly reviewed pure fixture function definitions, not the
# old test scripts' top-level tests/file-writing branches. Refuse name clashes:
# no validator function is monkey-patched. These build new synthetic records;
# no historical uat log/metadata is read or transformed.
$fixFPureDefinitions=[ordered]@{
    'test-r1c4b-end-handoff.ps1'=@('Add-EndRow','End-Geometry','End-Authority','End-Diagnostic','Add-EndInput','Add-EndRaw','Add-EndUpFence','Add-EndWriter','New-EndHandoffFixture','Renumber-EndFixture')
    'test-r1c4b-end-diagnostics.ps1'=@('Copy-EndDiagnostic','New-EndDiagnosticProof','Complete-CProof','Add-CFixtureFields','New-CFixture')
    'test-r1c4b-input-isolation.ps1'=@('Copy-IsolationFixture','Set-DField','New-DRow','D-OwnerFields','New-DPoint','New-DPosition','Repair-DReferences','New-DWire','Shift-DQpcFixture')
}
foreach($fileName in $fixFPureDefinitions.Keys){
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $fileName),[ref]$tokens,[ref]$errors)
    if($errors.Count){throw 'Pure fixture source syntax invalid'}
    foreach($functionName in $fixFPureDefinitions[$fileName]){
        if(Get-Command $functionName -ErrorAction SilentlyContinue){throw "No function override allowed: $functionName"}
        $definitions=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $functionName},$true))
        if($definitions.Count -ne 1){throw "Missing unique approved fixture generator: $functionName"}
        . ([scriptblock]::Create($definitions[0].Extent.Text))
    }
}
function New-FixFExtraRow([string]$Type,$Fields,[long]$Clock,[int]$Gesture,[string]$Scope='gesture'){
    $row=[ordered]@{schema='r1c4b-takeover-owned/v5';sequence=$script:fixFTempSequence;qpc=$Clock;type=$Type;gesture=$Gesture;event_scope=$Scope;event_acceptance_eligible=($Scope -ceq 'gesture' -and $Gesture -gt 0);acceptance_eligible=($Scope -ceq 'gesture' -and $Gesture -gt 0)}
    --$script:fixFTempSequence
    foreach($p in $Fields.PSObject.Properties){if($p.Name -notin @('schema','sequence','qpc','type','gesture','event_scope','event_acceptance_eligible')){$row[$p.Name]=$p.Value}}
    return [pscustomobject]$row
}
function Repair-FixFRecordReferences($Rows){
    $map=@{};for($n=0;$n -lt $Rows.Count;++$n){$map[[string]$Rows[$n].sequence]=[long]$n+1}
    $references=@('diagnostic_sequence','ledger_intent_sequence','down_input_sequence','native_enter_sequence','native_down_sequence','end_sequence','winevent_end_sequence','handoff_preflight_sequence','callback_record_sequence','product_preflight_sequence','input_isolation_ready_sequence','writer_input_isolation_sequence','source_postverify_sequence','takeover_end_sequence','source_acceptance_sequence','sample_sequence','request_sequence','ack_sequence','quiescence_ack_sequence','initial_ack_sequence','initial_authority_sequence','boundary_authority_sequence','native_exit_sequence','final_authority_sequence')
    foreach($r in $Rows){foreach($name in $references){$value=Get-AutoField $r $name;if($null -ne $value -and $value -ne 0){if(-not $map.ContainsKey([string]$value)){throw "Unknown synthetic record reference $name=$value"};$r.$name=$map[[string]$value]}}}
    for($n=0;$n -lt $Rows.Count;++$n){$Rows[$n].sequence=[long]$n+1};return ,$Rows
}
function New-FixFFullFence($Alias,$Startup,[long]$Clock){
    $d=(New-FixFFenceModel).Diagnostic;$d.sequence=$script:fixFTempSequence;--$script:fixFTempSequence;$d.qpc=$Clock;$d.gesture=$Alias.gesture;$d.operation=$Startup.operation
    foreach($name in @('source_pid','actual_source_pid')){$d.$name=$Startup.pid};foreach($name in @('source_tid','actual_source_tid')){$d.$name=$Startup.ui_tid};$d.run_nonce=$Startup.run_nonce;$d.actual_source_nonce=$Startup.run_nonce;$d.source_hwnd=$Alias.target;$d.foreground_hwnd=$Alias.foreground
    $d.actual_cursor=@($Alias.cursor);$d.expected_cursor=@($Alias.expected_cursor);$d.left_down=$Alias.left_down;$d.expected_left_down=$Alias.left_down;$d.priming=$Alias.priming;$d.post_cancel=$Alias.post_cancel;$d.cancel_pending=$Alias.cancel_pending
    $d.capture_hwnd=$Alias.capture_hwnd;$d.menu_owner_hwnd=$Alias.menu_owner_hwnd;$d.move_size_hwnd=$Alias.move_size_hwnd;$d.gui_flags=$Alias.gui_flags;$d.root=$Alias.window_from_point_root
    $d.query_start_qpc=$Clock-18;$d.query_finish_qpc=$Clock-1;$offset=1
    foreach($q in $d.queries.PSObject.Properties){$q.Value.start_qpc=$d.query_start_qpc+$offset;$q.Value.finish_qpc=$q.Value.start_qpc+1;$offset+=2}
    $d.predicates.CANCELLED_GUI_SAFE='NOT_EVALUATED';$d.predicates.INPUT_ROOT_OWNED='NOT_EVALUATED';$d.predicates.CAPTURE='NOT_EVALUATED';$d.required_predicates=@($d.predicates.PSObject.Properties.Name|Select-Object -First 11)
    if($d.post_cancel -or $d.gesture -eq 0){$d.required_predicates+=,'CANCELLED_GUI_SAFE';$d.predicates.CANCELLED_GUI_SAFE='PASS';if($d.left_down){$d.required_predicates+=,'INPUT_ROOT_OWNED';$d.predicates.INPUT_ROOT_OWNED='PASS'}}elseif($d.left_down){$d.required_predicates+=,'CAPTURE';$d.predicates.CAPTURE='PASS'}
    Set-DField $Alias diagnostic_sequence $d.sequence;Set-DField $d schema 'r1c4b-takeover-owned/v5';Set-DField $d event_scope 'gesture';Set-DField $d event_acceptance_eligible ($d.gesture -gt 0)
    return $d
}
function New-FixFFullLedger($Rows,[long]$Clock,[int]$Gesture){
    $i=@($Rows|Where-Object {$_.type -ceq 'input' -and $_.gesture -gt 0 -and $_.flags -eq 2})[0];$raw=@($Rows|Where-Object {$_.type -ceq 'raw_input' -and $_.gesture -gt 0 -and $_.button_flags -eq 1})[0];$n=@($Rows|Where-Object type -ceq 'native_button_down')[0];$e=@($Rows|Where-Object type -ceq 'ENTER')[0]
    return New-FixFExtraRow 'test_down_ledger' ([pscustomobject]@{query_start_qpc=$Clock-2;query_finish_qpc=$Clock-1;ledger_snapshot_qpc=$Clock-1;ledger_receiver_sequence=@($Rows|Where-Object type -ceq 'raw_input')[-1].receiver_sequence;down_input_sequence=$i.sequence;down_start_qpc=$i.injection_start_qpc;down_return_qpc=$i.injection_return_qpc;receiver_watermark=$i.receiver_watermark;down_sent=$true;raw_down_receiver_sequence=$raw.receiver_sequence;raw_down_receiver_qpc=$raw.receiver_qpc;raw_down_matches=$true;native_down_sequence=$n.sequence;native_down_qpc=$n.receipt_qpc;native_down_matches=$true;native_enter_sequence=$e.sequence;native_enter_qpc=$e.receipt_qpc;native_enter_matches=$true;non_test_movements=0;non_test_button_transitions=0;unmatched_button_transitions=0;matching_up_seen=$false;up_command_sent=$false;ledger_pending=$true}) $Clock $Gesture
}
function New-FixFCompleteNormal([string]$Operation){
    $script:fixFTempSequence=-1;$base=Shift-DQpcFixture (New-DWire $Operation) ([long]1134816073663);$rows=[Collections.Generic.List[object]]::new();$s=$base[0]
    foreach($pair in @(@('test_mode','normal'),@('fence_contract','ordered_failure_snapshot_v1'),@('cleanup_contract','owned_native_abort_v1'),@('abort_cleanup_tag',0x50424655))){Set-DField $s $pair[0] $pair[1]}
    foreach($r in $base){
        $r.schema='r1c4b-takeover-owned/v5';Set-DField $r event_scope 'gesture';Set-DField $r event_acceptance_eligible ($r.gesture -gt 0)
        if($r.type -ceq 'raw_input'){Set-DField $r extra_information 0x50424D41;Set-DField $r cleanup_tag_matches $false;Set-DField $r ledger_observed_qpc ($r.qpc-1)}
        if($r.type -ceq 'shutdown'){Set-DField $r winevent_hook_removed $true}
        if($r.type -ceq 'native_button_down'){Set-DField $r receipt_qpc ($r.qpc-1)}
        if($r.type -ceq 'input_fence'){$rows.Add((New-FixFFullFence $r $s ($r.qpc-1)))}
        if($r.type -ceq 'input' -and $r.flags -in @(2,4)){
            $intent=New-FixFExtraRow 'ledger_input_intent' ([pscustomobject]@{flags=$r.flags;input_tag=$r.input_tag;receiver_watermark=$r.receiver_watermark;run_nonce=$s.run_nonce;intent_purpose=$(if($r.flags -eq 2){'native_down'}else{'gesture_up'})}) ($r.injection_start_qpc-1) $r.gesture
            Set-DField $r ledger_intent_sequence $intent.sequence;$rows.Add($intent)
        }
        $rows.Add($r)
        if($r.type -ceq 'ENTER'){$rows.Add((New-FixFFullLedger @($rows.ToArray()) ($r.qpc+20) $r.gesture))}
    }
    return ,(Repair-FixFRecordReferences @($rows.ToArray()))
}
function Add-FixFAbortRow([string]$Type,$Fields){$script:fixFAbortClock+=100;$r=New-FixFExtraRow $Type $Fields $script:fixFAbortClock $script:fixFAbortGesture 'cleanup';$script:fixFAbortRows.Add($r);return $r}
function Add-FixFAbortAck([string]$Phase,$Startup){
    $fields=[pscustomobject]@{phase=$Phase;request_qpc=($script:fixFAbortClock+99);message=32788;posted=$true;source_hwnd=300;source_pid=$Startup.pid;source_tid=$Startup.ui_tid;run_nonce=$Startup.run_nonce};$request=Add-FixFAbortRow 'abort_quiescence_request' $fields
    $ack=Add-FixFAbortRow 'abort_writer_quiescence' ([pscustomobject]@{phase='owner_ack';cleanup_phase=$Phase;request_sequence=$request.sequence;ack_tid=$Startup.ui_tid;ack_qpc=($script:fixFAbortClock+99);acknowledged=$true;active_operation_id=0;pending_raw_count=0;motion_pending=$false;notice_posted=$false;acceptance_retired=$true;takeover_scope=$false;native_calls=0;source_hwnd=300;source_pid=$Startup.pid;source_tid=$Startup.ui_tid;run_nonce=$Startup.run_nonce;actual_source_pid=$Startup.pid;actual_source_tid=$Startup.ui_tid;actual_source_nonce=$Startup.run_nonce;own_identity=$true})
    $null=Add-FixFAbortRow 'abort_quiescence_wait' ([pscustomobject]@{phase=$Phase;request_sequence=$request.sequence;ack_sequence=$ack.sequence;timeout_ms=2000;wait_result=0;quiescent=$true});return $ack
}
function Add-FixFAbortAuthority([string]$Phase,$Ledger,$Startup,$Ack,$Initial,$Cursor,[bool]$Final=$false){
    $fields=Copy-FixFModel $Ledger;$fields.type='abort_cleanup_authority';$fields.query_start_qpc=$script:fixFAbortClock+75;$fields.query_finish_qpc=$script:fixFAbortClock+90;$fields.ledger_snapshot_qpc=$fields.query_finish_qpc;$fields.ledger_receiver_sequence=@($script:fixFAbortRows|Where-Object type -ceq 'raw_input')[-1].receiver_sequence
    foreach($p in (New-FixFAuthorityModel).Authority.PSObject.Properties){if($p.Name -notin @('sequence','qpc','type','query_start_qpc','query_finish_qpc') -and $null -eq (Get-AutoField $fields $p.Name)){Set-DField $fields $p.Name $p.Value}}
    foreach($name in @('source_pid','actual_source_pid','foreground_pid')){$fields.$name=$Startup.pid};foreach($name in @('source_tid','actual_source_tid','foreground_tid')){$fields.$name=$Startup.ui_tid};$fields.source_hwnd=300;$fields.foreground_hwnd=300;$fields.cursor_root=300;$fields.run_nonce=$Startup.run_nonce;$fields.actual_source_nonce=$Startup.run_nonce;$fields.cursor=@($Cursor);$fields.phase=$Phase;$fields.quiescence_ack_sequence=$Ack.sequence;$fields.initial_authority_sequence=$(if($null -eq $Initial){0}else{$Initial.sequence})
    $fields.capture_hwnd=$(if($Final){0}else{300});$fields.move_size_hwnd=$fields.capture_hwnd;$fields.gui_flags=$(if($Final){0}else{2});$fields.expected_native_mode=-not $Final;$fields.left_down=-not $Final;$fields.eligible=-not $Final;$fields.acceptance_eligible=$false
    if($Final){$fields.matching_up_seen=$true;$fields.up_command_sent=$true;$fields.ledger_pending=$false;Set-DField $fields initial_ack_sequence $Initial.quiescence_ack_sequence}
    $fields.PSObject.Properties.Remove('sequence');$fields.PSObject.Properties.Remove('qpc');$fields.PSObject.Properties.Remove('type');return Add-FixFAbortRow 'abort_cleanup_authority' $fields
}
function New-FixFCompleteAbort([string]$Operation){
    $normal=New-FixFCompleteNormal $Operation;$sample=@($normal|Where-Object {$_.type -ceq 'sample' -and $_.index -eq 1})[0];$prefix=@($normal|Where-Object sequence -le $sample.sequence);$s=$prefix[0];$s.test_mode='controlled_abort';$script:fixFAbortGesture=$sample.gesture;$script:fixFAbortClock=[long]$sample.qpc;$script:fixFAbortRows=[Collections.Generic.List[object]]::new();foreach($r in $prefix){$script:fixFAbortRows.Add($r)}
    $enter=@($prefix|Where-Object type -ceq 'ENTER')[0];$down=@($prefix|Where-Object type -ceq 'native_button_down')[0];$ledger=@($prefix|Where-Object type -ceq 'test_down_ledger')[0]
    $null=Add-FixFAbortRow 'controlled_abort_fault' ([pscustomobject]@{fault='after_sample1_before_cancel';reason='BLOCKED_BY_TEST_FAULT';guard_passed=$true;sample_sequence=$sample.sequence;native_enter_sequence=$enter.sequence;native_down_sequence=$down.sequence;non_test_movements=0;non_test_button_transitions=0;unmatched_button_transitions=0})
    $null=Add-FixFAbortRow 'blocked' ([pscustomobject]@{reason='BLOCKED_BY_TEST_FAULT'})
    $initialAck=Add-FixFAbortAck 'initial' $s;$initial=Add-FixFAbortAuthority 'initial' $ledger $s $initialAck $null $sample.cursor;$boundary=Add-FixFAbortAuthority 'api_boundary' $ledger $s $initialAck $initial $sample.cursor
    $watermark=@($prefix|Where-Object type -ceq 'raw_input')[-1].receiver_sequence
    $intent=Add-FixFAbortRow 'ledger_input_intent' ([pscustomobject]@{intent_purpose='abort_up';flags=4;input_tag=$s.abort_cleanup_tag;receiver_watermark=$watermark;run_nonce=$s.run_nonce})
    $release=Add-FixFAbortRow 'abort_cleanup_release' ([pscustomobject]@{flags=4;actual_mouse_flags=4;normalized_dx=0;normalized_dy=0;input_tag=$s.abort_cleanup_tag;receiver_watermark=$watermark;sent=1;error=0;ledger_intent_sequence=$intent.sequence;initial_authority_sequence=$initial.sequence;boundary_authority_sequence=$boundary.sequence;injection_start_qpc=($script:fixFAbortClock+10);injection_return_qpc=($script:fixFAbortClock+20)})
    $u=Copy-FixFModel (@($prefix|Where-Object type -ceq 'raw_input')[-1]);$u.PSObject.Properties.Remove('sequence');$u.PSObject.Properties.Remove('qpc');$u.PSObject.Properties.Remove('type');$u.receiver_sequence=$watermark+1;$u.receiver_qpc=$script:fixFAbortClock+99;$u.ledger_observed_qpc=$script:fixFAbortClock+99;$u.button_flags=2;$u.raw_flags=0;$u.dx=0;$u.dy=0;$u.cursor_sampled=$false;$u.cursor_success=$false;$u.cursor=$null;$u.test_tag_matches=$false;$u.extra_information=$s.abort_cleanup_tag;$u.cleanup_tag_matches=$true;$u.raw_scope='cleanup';$u.acceptance_eligible=$false;$u.cleanup_observation=$true
    $packet=Add-FixFAbortRow 'raw_input' $u
    $proof=Add-FixFAbortRow 'abort_cleanup_raw_up' ([pscustomobject]@{wait_result=0;timeout_ms=2000;raw_up_observed=$true;receiver_watermark=$watermark;injection_start_qpc=$release.injection_start_qpc;input_tag=$release.input_tag;receiver_sequence=$packet.receiver_sequence;receiver_qpc=$packet.receiver_qpc})
    $exitFields=End-Geometry $sample.positioning;$exitFields.receipt_qpc=$script:fixFAbortClock+99;$exitFields.owner_capture=0;$exitFields.left_down=$false;$exitFields.end_sequence=0;$exit=Add-FixFAbortRow 'EXIT' ([pscustomobject]$exitFields);$exit.end_sequence=$exit.sequence
    $cb=Copy-FixFModel (@($normal|Where-Object {$_.type -ceq 'winevent_callback' -and $_.event -eq 11})[0]);$cb.PSObject.Properties.Remove('sequence');$cb.PSObject.Properties.Remove('qpc');$cb.PSObject.Properties.Remove('type');$cb.callback_qpc=$script:fixFAbortClock+99;$cb=Add-FixFAbortRow 'winevent_callback' $cb
    $match=Copy-FixFModel (@($normal|Where-Object {$_.type -ceq 'winevent_match' -and $_.callback_sequence -eq 2})[0]);$match.PSObject.Properties.Remove('sequence');$match.PSObject.Properties.Remove('qpc');$match.PSObject.Properties.Remove('type');$match.callback_record_sequence=$cb.sequence;$null=Add-FixFAbortRow 'winevent_match' $match
    $end=Add-FixFAbortRow 'abort_cleanup_end_wait' ([pscustomobject]@{native_wait_result=0;winevent_wait_result=0;timeout_ms=2000;native_exit_sequence=$exit.sequence;native_exit_qpc=$exit.receipt_qpc;winevent_end_sequence=$cb.sequence;winevent_end_qpc=$cb.callback_qpc})
    $finalAck=Add-FixFAbortAck 'final' $s;$final=Add-FixFAbortAuthority 'final' $ledger $s $finalAck $initial $sample.cursor $true
    $null=Add-FixFAbortRow 'abort_cleanup_final' ([pscustomobject]@{cleanup_result='PASS';cleanup_up_attempted=$true;cleanup_up_sent=$true;raw_up_observed=$true;native_exit_observed=$true;winevent_end_observed=$true;final_capture_clear=$true;final_mode_clear=$true;ledger_settled=$true;writer_quiescent=$true;acceptance_retired=$true;final_authority_sequence=$final.sequence;final_left_down=$false;pending_work=$false;acceptance_eligible=$false})
    $hook=Copy-FixFModel (@($normal|Where-Object type -ceq 'winevent_hook_removed')[0]);$hook.PSObject.Properties.Remove('sequence');$hook.PSObject.Properties.Remove('qpc');$hook.PSObject.Properties.Remove('type');$null=Add-FixFAbortRow 'winevent_hook_removed' $hook
    $receiver=Copy-FixFModel (@($normal|Where-Object type -ceq 'receiver_shutdown')[0]);$receiver.PSObject.Properties.Remove('sequence');$receiver.PSObject.Properties.Remove('qpc');$receiver.PSObject.Properties.Remove('type');$receiver.receiver_sequence=$packet.receiver_sequence+1;$receiver.receiver_qpc=$script:fixFAbortClock+99;$null=Add-FixFAbortRow 'receiver_shutdown' $receiver
    $last=Copy-FixFModel $normal[-1];$last.PSObject.Properties.Remove('sequence');$last.PSObject.Properties.Remove('qpc');$last.PSObject.Properties.Remove('type');$last.result='BLOCKED';$last.takeover_geometry_writes=0;$last.source_acceptance_sequence=0;$last.input_shield_created=$false;$last.input_shield_destroyed=$true;$last.input_shield_activated=$false;$null=Add-FixFAbortRow 'shutdown' $last
    return ,(Repair-FixFRecordReferences @($script:fixFAbortRows.ToArray()))
}
foreach($operation in @('Move','BottomResize')){
    $normal=Shift-DQpcFixture (New-DWire $operation) ([long]1134816073663)
    foreach($row in $normal){
        $row.schema='r1c4b-takeover-owned/v5'
        Set-DField $row event_scope 'gesture';Set-DField $row event_acceptance_eligible ($row.gesture -gt 0)
        if($row.type -ceq 'raw_input'){Set-DField $row extra_information 0x50424D41;Set-DField $row cleanup_tag_matches $false}
    }
    foreach($pair in @(@('test_mode','normal'),@('fence_contract','ordered_failure_snapshot_v1'),@('cleanup_contract','owned_native_abort_v1'),@('abort_cleanup_tag',0x50424655))){Set-DField $normal[0] $pair[0] $pair[1]}
    $null=Test-FixFEnvelope $normal -AllowSynthetic
    $proof=Test-FixFNormalOwnedRecords $normal
    Check-FixFModel ($proof.Result -ceq 'PASS' -and $proof.Cancel -ceq 'PASS_WITH_TERMINAL_SETTLEMENT' -and $proof.Takeover -ceq 'PASS' -and $proof.NativeWrites -eq 19 -and $proof.HandoffNativeCalls -eq 1 -and $proof.ContinuationQuanta -eq 18 -and $proof.FinalLeftDown -eq $false -and $proof.SyntheticFixture) ('new in-memory '+$operation+' full original normal primitives at real QPC')
    Check-FixFModel ($proof.PostEndInputShield -ceq $(if($operation -ceq 'Move'){'NOT_NEEDED'}else{'PASS'})) ('normal '+$operation+' retains source/shield distinction')
    $bad=Copy-FixFModel $normal;$writer=@($bad|Where-Object type -ceq 'writer_result')[0];$writer.positioning[3]++
    Reject-FixFModel {Test-FixFNormalOwnedRecords $bad} ('normal '+$operation+' strict postverify cannot be weakened')
    $bad=Copy-FixFModel $normal;$up=@($bad|Where-Object {$_.type -ceq 'raw_input' -and $_.gesture -gt 0 -and ($_.button_flags -band 2)})[0];$up.raw_scope='cleanup';$up.acceptance_eligible=$false
    Reject-FixFModel {Test-FixFNormalOwnedRecords $bad} ('normal '+$operation+' cleanup Raw UP cannot count acceptance')
    Write-Host "Testing complete synthetic v5 $operation normal aggregate..."
    $complete=New-FixFCompleteNormal $operation;$full=Test-FixFInputReliabilityRecords $complete -AllowSynthetic
    Check-FixFModel ($full.Result -ceq 'PASS' -and $full.TakeoverAcceptance -ceq 'PASS' -and $full.SyntheticFixture) ('complete new v5 '+$operation+' normal aggregate synthetic PASS')
    Write-Host "Testing complete synthetic v5 $operation controlled-abort aggregate..."
    $abort=New-FixFCompleteAbort $operation;$full=Test-FixFInputReliabilityRecords $abort -AllowSynthetic
    Check-FixFModel ($full.Result -ceq 'PASS' -and $full.FixtureResult -ceq 'PASS_EXPECTED_ABORT' -and $full.GestureResult -ceq 'BLOCKED_BY_TEST_FAULT' -and $full.CleanupResult -ceq 'PASS' -and $full.TakeoverAcceptance -ceq 'NOT_RUN' -and $full.SyntheticFixture) ('complete new v5 '+$operation+' controlled-abort aggregate synthetic PASS')
    foreach($type in @('abort_cleanup_raw_up','EXIT','abort_cleanup_final','input_fence_diagnostic','test_down_ledger')){$bad=Copy-FixFModel $abort;$bad=@($bad|Where-Object type -cne $type);Reject-FixFModel {Test-FixFInputReliabilityRecords $bad -AllowSynthetic} ('aggregate '+$operation+' rejects missing '+$type)}
    $bad=Copy-FixFModel $abort;$up=@($bad|Where-Object {$_.type -ceq 'raw_input' -and $_.cleanup_tag_matches})[0];$up.acceptance_eligible=$true;$up.event_acceptance_eligible=$true
    Reject-FixFModel {Test-FixFInputReliabilityRecords $bad -AllowSynthetic} ('aggregate '+$operation+' rejects cleanup acceptance contamination')
    if($operation -ceq 'Move'){
        # Preserve the envelope/sequence to reach actual raw, ledger and cleanup
        # assertions rather than merely rejecting a missing sequence number.
        foreach($change in @('missing_parent_qpc','parent_before_child','parent_after_record','forged_processed_serial','missing_actual_raw_up','missing_actual_native_exit','missing_actual_winevent_end','end_timeout','missing_final_button','string_final_button','api_boundary_changed','api_contains_move','already_sent_up','source_generation_changed')){
            $bad=Copy-FixFModel $abort
            $release=@($bad|Where-Object type -ceq 'abort_cleanup_release')[0]
            $up=@($bad|Where-Object {$_.type -ceq 'raw_input' -and $_.cleanup_tag_matches})[0]
            $boundary=@($bad|Where-Object {$_.type -ceq 'abort_cleanup_authority' -and $_.phase -ceq 'api_boundary'})[0]
            $final=@($bad|Where-Object {$_.type -ceq 'abort_cleanup_authority' -and $_.phase -ceq 'final'})[0]
            $end=@($bad|Where-Object type -ceq 'abort_cleanup_end_wait')[0]
            switch($change){
                missing_parent_qpc {$up.PSObject.Properties.Remove('ledger_observed_qpc')}
                parent_before_child {$up.ledger_observed_qpc=$up.receiver_qpc-1}
                parent_after_record {$up.ledger_observed_qpc=$up.qpc+1}
                forged_processed_serial {$boundary.ledger_receiver_sequence++}
                missing_actual_raw_up {$up.button_flags=0}
                missing_actual_native_exit {$end.native_exit_sequence=$release.sequence}
                missing_actual_winevent_end {$end.winevent_end_sequence=$release.sequence}
                end_timeout {$end.winevent_wait_result=258}
                missing_final_button {$final.PSObject.Properties.Remove('left_down')}
                string_final_button {$final.left_down='false'}
                api_boundary_changed {$boundary.cursor[0]+=3;$boundary.api_boundary_stable=$false;$boundary.eligible=$false}
                api_contains_move {$release.actual_mouse_flags=5}
                already_sent_up {$boundary.up_command_sent=$true;$boundary.ledger_pending=$false;$boundary.eligible=$false}
                source_generation_changed {$boundary.actual_source_nonce++;$boundary.own_identity=$false;$boundary.eligible=$false}
            }
            Reject-FixFModel {Test-FixFInputReliabilityRecords $bad -AllowSynthetic} ('complete aggregate denies '+$change)
        }
    }
}

Write-Host "Fix F pure model checks: $script:fixFChecks; failures: $($script:fixFFailures.Count)"
foreach($failure in $script:fixFFailures){Write-Host "FAIL: $failure"}
if($script:fixFFailures.Count){exit 1}
Write-Host 'PASS: pure synthetic models and complete v5 normal/abort wires; empirical cleanup NOT_TESTED'
exit 0
