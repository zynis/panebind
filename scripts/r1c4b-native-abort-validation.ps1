Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# v5 is a new test cleanup contract. The frozen v4/Fix E validator remains
# authoritative for old files; no schema projection, monkey patch or replay edit.
. (Join-Path $PSScriptRoot 'r1c4b-normal-v5-primitives.ps1')

function Assert-FixF([bool]$Value,[string]$Reason){if(-not $Value){throw [IO.InvalidDataException]::new("Fix F v5: $Reason")}}
function Get-FixFOne($Rows,[string]$Type){$r=@(Get-IsolationRows $Rows $Type);Assert-FixF ($r.Count -eq 1) "one $Type required";return $r[0]}
function Get-FixFRef($Rows,$Sequence,[string]$Type){Assert-IsolationInt64 $Sequence 'record reference';$r=@($Rows|Where-Object sequence -eq $Sequence);Assert-FixF ($r.Count -eq 1 -and $r[0].type -ceq $Type) "actual $Type reference";return $r[0]}
function Test-FixFBool($Value,[string]$Field){Assert-AutoBoolean $Value $Field;return $Value}
function Get-FixFState($Value){if($null -eq $Value){return 'UNKNOWN'};Assert-AutoBoolean $Value 'predicate boolean';if($Value){return 'PASS'}else{return 'FAIL'}}
function Test-FixFOtherClear($States){
    $unknown=$false;$active=$false
    foreach($name in @('ctrl','shift','alt','lwin','rwin','rbutton','mbutton','xbutton1','xbutton2','escape')){
        $v=Get-AutoField $States $name
        if($null -eq $v){$unknown=$true}else{Assert-AutoBoolean $v "other $name";$active=$active -or $v}
    }
    if($active){return $false};if($unknown){return $null};return $true
}
function Assert-FixFNumericTree($Value){
    if($null -eq $Value){return}
    if($Value -is [array]){foreach($v in $Value){Assert-FixFNumericTree $v};return}
    if($Value -isnot [pscustomobject]){return}
    foreach($p in $Value.PSObject.Properties){
        if($p.Name -match '(^qpc$|_qpc$|^qpc_frequency$|^sequence$|_sequence$|^(receiver|raw)_watermark$)' -and $null -ne $p.Value){Assert-IsolationInt64 $p.Value "v5 $($p.Name)"}
        Assert-FixFNumericTree $p.Value
    }
}
function Test-FixFEnvelope($Rows,[switch]$AllowSynthetic){
    Assert-FixF ($Rows.Count -ge 2 -and $Rows[0].type -ceq 'startup' -and $Rows[-1].type -ceq 'shutdown') 'startup/shutdown'
    $s=$Rows[0]
    foreach($name in @('pid','ui_tid','qpc_frequency','run_nonce','gesture_id')){Assert-IsolationInt64 (Get-AutoField $s $name) "startup $name";Assert-FixF ($s.$name -gt 0) "positive $name"}
    foreach($name in @('abort_cleanup_tag','takeover_geometry_writes')){Assert-IsolationInt64 (Get-AutoField $s $name) "startup $name"}
    Assert-FixF ($s.operation -cin @('Move','BottomResize') -and $s.test_mode -cin @('controlled_abort','normal') -and $s.mode -ceq 'free_takeover' -and $s.fence_contract -ceq 'ordered_failure_snapshot_v1' -and $s.cleanup_contract -ceq 'owned_native_abort_v1' -and $s.abort_cleanup_tag -eq 0x50424655) 'explicit new contract'
    Assert-FixF ($s.foreground_contract -ceq 'verified_global_foreground_v2' -and $s.handoff_contract -ceq 'winevent_end_barrier_v1' -and $s.diagnostic_contract -ceq 'separated_authority_v1' -and $s.authority_contract -ceq 'product_gesture_v1' -and $s.input_isolation_contract -ceq 'post_end_shield_v1' -and $s.input_correlation -ceq 'actual_absolute_receipt_v1') 'unchanged product/input contracts'
    foreach($name in @('human_input','real_explorer','sendinput_in_probe')){Assert-AutoBoolean (Get-AutoField $s $name) "startup $name"}
    Assert-FixF (-not $s.human_input -and -not $s.real_explorer -and $s.sendinput_in_probe -and $s.takeover_geometry_writes -eq 0) 'owned test-only startup'
    if((Get-AutoField $s 'synthetic_fixture') -eq $true){Assert-FixF ([bool]$AllowSynthetic) 'synthetic fixtures are not empirical evidence'}
    $allowed=@('startup','shutdown','desktop_gate','guard','owned','receiver','receiver_shutdown','receiver_error','show_window','foreground_attempt','foreground_ready','foreground_bootstrap','activation_event','activation_button','activation_visibility','activation_fence','activation_input','activation_release','input_fence','input','destination_fence','cleanup_input_diagnostic','cleanup_release','cleanup_raw_up','cleanup_final_isolation','cleanup_final','cleanup_skipped_no_authority','raw_preflight_begin','raw_preflight_outcome','raw_preflight_complete','raw_input','raw_cursor_status','raw_wait_begin','raw_wait_result','raw_motion_correlation','LEGACY_MOVE','native_button_down','path','sample','path_complete','ENTER','DRAG','EXIT','CAPTURE_CHANGED','POSITION_CHANGED','cancel_begin','cancel_return','end_wait','cancel_message','intent_anchor','handoff_begin','writer_begin','writer_result','raw_quantum','handoff_complete','takeover_end','source_final_acceptance','takeover_failure','blocked','winevent_hook_installed','winevent_hook_removed','winevent_callback','winevent_match','winevent_error','winevent_barrier_snapshot','winevent_end_wait','gesture_armed','product_handoff_preflight','handoff_lag_candidate','input_isolation_setup_plan','input_shield_created','input_shield_position','input_isolation_point','input_isolation_ready','synthetic_input_isolation','writer_input_isolation','input_shield_activation','input_shield_destroyed','cursor_restore_skipped','source_work_post_skipped','input_fence_diagnostic','ledger_input_intent','test_down_ledger','abort_owner_work_skipped','abort_quiescence_request','abort_writer_quiescence','abort_quiescence_wait','abort_cleanup_authority','abort_cleanup_release','abort_cleanup_raw_up','abort_cleanup_end_wait','abort_cleanup_final','abort_cleanup_skipped','controlled_abort_fault')
    $g=if($s.operation -ceq 'Move'){1}else{2}
    for($i=0;$i -lt $Rows.Count;++$i){
        $r=$Rows[$i];Assert-FixFNumericTree $r
        Assert-IsolationInt64 (Get-AutoField $r 'sequence') 'sequence';Assert-IsolationInt64 (Get-AutoField $r 'qpc') 'QPC';Assert-IsolationInt64 (Get-AutoField $r 'gesture') 'gesture'
        Assert-FixF ($r.schema -ceq 'r1c4b-takeover-owned/v5' -and $r.sequence -eq [long]$i+1 -and $r.gesture -in @(0,$g) -and $r.type -cin $allowed -and ($i -eq 0 -or $r.qpc -ge $Rows[$i-1].qpc)) 'schema/contiguous sequence/ordered QPC/known type/gesture'
        Assert-AutoBoolean (Get-AutoField $r 'event_acceptance_eligible') 'event acceptance scope'
        Assert-FixF ($r.event_scope -cin @('gesture','cleanup') -and ($r.event_scope -cne 'cleanup' -or -not $r.event_acceptance_eligible)) 'cleanup scope cannot authorize acceptance'
    }
    Assert-FixF (@(Get-IsolationRows $Rows 'startup').Count -eq 1 -and @(Get-IsolationRows $Rows 'shutdown').Count -eq 1) 'unique lifecycle'
    return $s
}
function Test-FixFFenceDiagnostic($d,$Rows){
    $names=@('OWNER_RUNNING','LOG_HEALTHY','DESKTOP_READY','SOURCE_IDENTITY','FOREGROUND','NO_FOREIGN_CAPTURE_TRANSFER','CURSOR_QUERY','CURSOR_POSITION','LEFT_STATE','OTHER_INPUT_CLEAR','GUI_QUERY','CANCELLED_GUI_SAFE','INPUT_ROOT_OWNED','CAPTURE')
    Assert-FixF ((@($d.predicates.PSObject.Properties.Name) -join ',') -ceq ($names -join ',')) 'ordered exhaustive predicates'
    Assert-FixF ($d.query_start_qpc -gt 0 -and $d.query_finish_qpc -ge $d.query_start_qpc -and $d.query_finish_qpc -le $d.qpc -and $d.operation -ceq $Rows[0].operation) 'ordered diagnostic query range'
    Assert-AutoPoint $d.expected_cursor 'expected fence cursor';Assert-AutoBoolean $d.expected_left_down 'expected left'
    foreach($name in @('priming','post_cancel','cancel_pending','passed')){Assert-AutoBoolean (Get-AutoField $d $name) "fence $name"}
    foreach($name in @('source_hwnd','source_pid','source_tid','actual_source_pid','actual_source_tid','actual_source_nonce','run_nonce','foreground_hwnd','foreground_pid','foreground_tid','capture_hwnd','menu_owner_hwnd','move_size_hwnd','gui_flags','root')){if($null -ne (Get-AutoField $d $name)){Assert-IsolationInt64 $d.$name "fence $name"}}
    Assert-FixF ($d.source_pid -eq $Rows[0].pid -and $d.source_tid -eq $Rows[0].ui_tid -and $d.run_nonce -eq $Rows[0].run_nonce) 'fence run identity'
    $owned=@(Get-IsolationRows $Rows 'owned');Assert-FixF ($owned.Count -eq 1 -and $d.source_hwnd -eq $owned[0].hwnd) 'exact fence source HWND'
    $queryNames=@('desktop','identity','foreground','cursor','buttons','gui','root')
    Assert-FixF ((@($d.queries.PSObject.Properties.Name) -join ',') -ceq ($queryNames -join ',')) 'complete ordered query inventory'
    [long]$queryClock=$d.query_start_qpc
    foreach($q in $d.queries.PSObject.Properties){
        Assert-AutoBoolean $q.Value.evaluated 'query evaluated'
        if($q.Value.evaluated){Assert-FixF ($q.Value.start_qpc -ge $queryClock -and $q.Value.finish_qpc -ge $q.Value.start_qpc -and $q.Value.finish_qpc -le $d.query_finish_qpc) 'per-query ordered time range';$queryClock=$q.Value.finish_qpc}
        else{Assert-FixF ($null -eq $q.Value.start_qpc -and $null -eq $q.Value.finish_qpc) 'unevaluated query has no invented timestamps'}
    }
    $expectedEvaluated=@{
        desktop=($d.owner_running -eq $true -and $d.log_healthy -eq $true)
        identity=($d.desktop_ready -eq $true);foreground=($d.desktop_ready -eq $true)
        cursor=($d.source_identity -eq $true -and $d.foreground_hwnd -eq $d.source_hwnd)
        buttons=($d.source_identity -eq $true -and $d.foreground_hwnd -eq $d.source_hwnd)
        gui=($d.source_identity -eq $true -and $d.foreground_hwnd -eq $d.source_hwnd)
        root=($d.cursor_query_succeeded -eq $true)
    }
    foreach($name in $queryNames){Assert-FixF ($d.queries.$name.evaluated -eq $expectedEvaluated[$name]) "actual safe query branch $name"}
    foreach($pair in @(@('desktop','desktop_ready'),@('identity','source_identity'),@('foreground','foreground_hwnd'),@('cursor','cursor_query_succeeded'),@('buttons','left_down'),@('gui','gui_query_succeeded'),@('root','root'))){
        if(-not $d.queries.($pair[0]).evaluated){Assert-FixF ($null -eq (Get-AutoField $d $pair[1])) 'unqueried field is null, not default false/PASS'}
    }
    $values=@{}
    foreach($pair in @(@('OWNER_RUNNING','owner_running'),@('LOG_HEALTHY','log_healthy'),@('DESKTOP_READY','desktop_ready'),@('SOURCE_IDENTITY','source_identity'),@('NO_FOREIGN_CAPTURE_TRANSFER','foreign_capture_clear'),@('CURSOR_QUERY','cursor_query_succeeded'),@('GUI_QUERY','gui_query_succeeded'))){$values[$pair[0]]=Get-FixFState (Get-AutoField $d $pair[1])}
    $identity=$d.actual_source_pid -eq $d.source_pid -and $d.actual_source_tid -eq $d.source_tid -and $d.actual_source_nonce -eq $d.run_nonce
    if($null -ne $d.source_identity){Assert-FixF ($d.source_identity -eq $identity) 'source identity summary'}
    $values.FOREGROUND=if($null -eq $d.foreground_hwnd){'UNKNOWN'}else{Get-FixFState ($d.foreground_hwnd -eq $d.source_hwnd)}
    $values.CURSOR_POSITION=if($d.cursor_query_succeeded -eq $true){Assert-AutoPoint $d.actual_cursor 'actual cursor';Get-FixFState ([Math]::Abs([long]$d.actual_cursor[0]-[long]$d.expected_cursor[0]) -le 1 -and [Math]::Abs([long]$d.actual_cursor[1]-[long]$d.expected_cursor[1]) -le 1)}else{'UNKNOWN'}
    $values.LEFT_STATE=if($null -eq $d.left_down){'UNKNOWN'}else{Assert-AutoBoolean $d.left_down 'actual left';Get-FixFState ($d.left_down -eq $d.expected_left_down)}
    $values.OTHER_INPUT_CLEAR=Get-FixFState (Test-FixFOtherClear $d.other_states)
    $noMove=$d.move_size_hwnd -eq 0 -and ($d.gui_flags -band 2) -eq 0
    $ownPending=$d.cancel_pending -and $d.move_size_hwnd -eq $d.source_hwnd
    $safe=$d.gui_query_succeeded -eq $true -and $d.capture_hwnd -eq 0 -and $d.menu_owner_hwnd -eq 0 -and ($d.gui_flags -band 28) -eq 0 -and ($noMove -or $ownPending)
    $values.CANCELLED_GUI_SAFE=if($d.gui_query_succeeded -eq $true){Get-FixFState $safe}else{'UNKNOWN'}
    $roots=@($d.source_hwnd)
    $endCallbacks=@($Rows|Where-Object {$_.type -ceq 'winevent_callback' -and $_.event -eq 11 -and $_.sequence -lt $d.sequence}|ForEach-Object sequence)
    $winEnds=@($Rows|Where-Object {$_.type -ceq 'winevent_match' -and $_.accepted -and $_.callback_record_sequence -in $endCallbacks -and $_.sequence -lt $d.sequence})
    if($winEnds.Count){$shield=@(Get-IsolationRows $Rows 'input_shield_created');$roots+=@($shield|Where-Object sequence -lt $d.sequence|ForEach-Object shield_hwnd)}else{$roots+=@(Get-IsolationRows $Rows 'guard'|ForEach-Object hwnd)}
    $values.INPUT_ROOT_OWNED=if($null -eq $d.root){'UNKNOWN'}else{Get-FixFState ($d.root -in $roots -and $d.root -gt 0)}
    $values.CAPTURE=if($d.gui_query_succeeded -eq $true -and $d.cursor_query_succeeded -eq $true){Get-FixFState ($d.capture_hwnd -eq $d.source_hwnd -or ($d.priming -and $d.capture_hwnd -eq 0 -and $d.root -eq $d.source_hwnd))}else{'UNKNOWN'}
    $required=@($names[0..10]);if($d.post_cancel -or $d.gesture -eq 0){$required+=,'CANCELLED_GUI_SAFE';if($d.expected_left_down){$required+=,'INPUT_ROOT_OWNED'}}elseif($d.expected_left_down){$required+=,'CAPTURE'}
    Assert-FixF ((@($d.required_predicates) -join ',') -ceq ($required -join ',')) 'unchanged applicable fence predicates'
    foreach($pair in @(@('DESKTOP_READY','desktop'),@('SOURCE_IDENTITY','identity'),@('FOREGROUND','foreground'),@('CURSOR_QUERY','cursor'),@('CURSOR_POSITION','cursor'),@('LEFT_STATE','buttons'),@('OTHER_INPUT_CLEAR','buttons'),@('GUI_QUERY','gui'))){if(-not $d.queries.($pair[1]).evaluated){$values[$pair[0]]='NOT_EVALUATED'}}
    foreach($name in @('CANCELLED_GUI_SAFE','INPUT_ROOT_OWNED','CAPTURE')){
        if($name -cnotin $required -or -not $d.queries.gui.evaluated){$values[$name]='NOT_EVALUATED'}
    }
    $failed=@();$first='NONE'
    foreach($name in $names){
        $state=Get-AutoField $d.predicates $name
        Assert-FixF ($state -cin @('PASS','FAIL','UNKNOWN','NOT_EVALUATED')) 'explicit predicate state'
        Assert-FixF ($state -ceq $values[$name]) "predicate $name not derived from actual fields/query status"
        if($name -cin $required -and $state -cne 'PASS'){if($first -ceq 'NONE'){$first=$name};if($state -ceq 'FAIL'){$failed+=,$name}}
    }
    Assert-FixF ($d.first_failed_predicate -ceq $first -and ((@($d.failed_predicates) -join ',') -ceq ($failed -join ',')) -and $d.passed -eq ($first -ceq 'NONE')) 'independently derived first/all failures'
    $reason=switch($first){'NONE'{'none'};'OWNER_RUNNING'{'owner_message_wait_failed'};'LOG_HEALTHY'{'evidence_capture_failed'};'DESKTOP_READY'{'BLOCKED_BY_INTERACTIVE_DESKTOP'};'SOURCE_IDENTITY'{'BLOCKED_BY_FOREGROUND'};'FOREGROUND'{'BLOCKED_BY_FOREGROUND'};'NO_FOREIGN_CAPTURE_TRANSFER'{'BLOCKED_BY_FOREIGN_INPUT_CAPTURE'};'CANCELLED_GUI_SAFE'{'BLOCKED_BY_FOREIGN_INPUT_CAPTURE'};'GUI_QUERY'{'BLOCKED_BY_GUI_STATE'};'INPUT_ROOT_OWNED'{if($winEnds.Count){'TestInputIsolationUnavailable'}else{'BLOCKED_BY_INPUT_HIT_AUTHORITY'}};default{'BLOCKED_BY_INPUT_INTERFERENCE'}}
    $subreason=switch($first){'CURSOR_QUERY'{'CURSOR_QUERY_FAILED'};'CURSOR_POSITION'{'CURSOR_DEVIATION'};'LEFT_STATE'{'LEFT_STATE_MISMATCH'};'OTHER_INPUT_CLEAR'{'OTHER_INPUT_ACTIVE'};'CAPTURE'{'CAPTURE_MISMATCH'};default{$first}}
    Assert-FixF ($d.failure_reason -ceq $reason -and $d.failure_subreason -ceq $subreason) 'precise failure class/subpredicate from recorded facts'
    $aliases=@($Rows|Where-Object {$_.type -ceq 'input_fence' -and $_.diagnostic_sequence -eq $d.sequence})
    Assert-FixF ($aliases.Count -eq [int]$d.passed) 'failed diagnostic must not become a success alias'
    if($d.passed){Assert-FixF ($aliases[0].sequence -gt $d.sequence -and (Test-AutoPoint $aliases[0].cursor $d.actual_cursor) -and $aliases[0].left_down -eq $d.left_down) 'success alias actual query'}
    return $d.passed
}
function Test-FixFPendingLedger($l,$Rows,$s){
    foreach($name in @('down_sent','raw_down_matches','native_down_matches','native_enter_matches','matching_up_seen','up_command_sent','ledger_pending')){Assert-AutoBoolean (Get-AutoField $l $name) "ledger $name"}
    $i=Get-FixFRef $Rows $l.down_input_sequence 'input'
    foreach($name in @('flags','actual_mouse_flags','input_tag','sent','error')){Assert-IsolationInt64 (Get-AutoField $i $name) "DOWN API $name"}
    $snapshotStart=if($null -ne (Get-AutoField $l 'query_start_qpc')){$l.query_start_qpc}else{$l.qpc}
    Assert-FixF ($i.injection_return_qpc -le $snapshotStart -and $i.sequence -lt $l.sequence -and $i.qpc -le $snapshotStart) 'DOWN API result actually known before authority query'
    $intent=Get-FixFRef $Rows $i.ledger_intent_sequence 'ledger_input_intent'
    Assert-FixF ($i.gesture -gt 0 -and $i.flags -eq 2 -and $i.actual_mouse_flags -eq 2 -and $i.input_tag -eq 0x50424D41 -and $intent.intent_purpose -ceq 'native_down' -and $intent.flags -eq 2 -and $intent.run_nonce -eq $s.run_nonce -and $intent.sequence -lt $i.sequence -and $intent.qpc -le $i.injection_start_qpc -and $intent.receiver_watermark -eq $i.receiver_watermark) 'provisional DOWN intent before API'
    Assert-FixF ($l.down_start_qpc -eq $i.injection_start_qpc -and $l.down_return_qpc -eq $i.injection_return_qpc -and $l.down_return_qpc -ge $l.down_start_qpc -and $l.receiver_watermark -eq $i.receiver_watermark -and $l.down_sent -eq ($i.sent -eq 1 -and $i.error -eq 0)) 'actual DOWN API reference'
    $raw=@($Rows|Where-Object {$_.type -ceq 'raw_input' -and $_.receiver_sequence -eq $l.raw_down_receiver_sequence -and $_.receiver_qpc -eq $l.raw_down_receiver_qpc})
    $rawMatch=$raw.Count -eq 1 -and $raw[0].sequence -lt $l.sequence -and $raw[0].qpc -le $snapshotStart -and $raw[0].receiver_sequence -gt $l.receiver_watermark -and $raw[0].receiver_qpc -ge $i.injection_start_qpc -and $raw[0].receiver_qpc -le $snapshotStart -and $raw[0].button_flags -eq 1 -and $raw[0].test_tag_matches -and $raw[0].extra_information -eq 0x50424D41 -and -not $raw[0].device_handle_present -and $raw[0].raw_scope -ceq 'gesture'
    Assert-FixF ($l.raw_down_matches -eq $rawMatch) 'actual Raw DOWN correlation, not parent log order'
    $n=Get-FixFRef $Rows $l.native_down_sequence 'native_button_down';$e=Get-FixFRef $Rows $l.native_enter_sequence 'ENTER';$owned=Get-FixFOne $Rows 'owned'
    $downMatch=$n.gesture -gt 0 -and $n.target -eq $owned.hwnd -and $n.receipt_qpc -eq $l.native_down_qpc -and $n.receipt_qpc -ge $i.injection_start_qpc -and $n.sequence -lt $l.sequence -and $n.qpc -le $snapshotStart
    $enterMatch=$e.receipt_qpc -eq $l.native_enter_qpc -and $e.receipt_qpc -ge $n.receipt_qpc -and $e.sequence -lt $l.sequence -and $e.qpc -le $snapshotStart -and $e.gesture -gt 0
    Assert-FixF ($l.native_down_matches -eq $downMatch -and $l.native_enter_matches -eq $enterMatch) 'native DOWN/ENTER actual references'
    Assert-IsolationInt64 (Get-AutoField $l 'ledger_snapshot_qpc') 'actual ledger snapshot QPC'
    Assert-IsolationInt64 (Get-AutoField $l 'ledger_receiver_sequence') 'ledger observed receiver boundary'
    $asOf=$l.ledger_snapshot_qpc
    Assert-FixF ($asOf -gt 0 -and $asOf -le $l.qpc -and $l.ledger_receiver_sequence -ge $l.receiver_watermark) 'actual locked ledger observation boundary'
    if($null -ne (Get-AutoField $l 'query_finish_qpc')){Assert-FixF ($asOf -ge $l.query_start_qpc -and $asOf -le $l.query_finish_qpc) 'ledger snapshot in authority query interval'}
    $later=@($Rows|Where-Object {$_.type -ceq 'raw_input' -and $_.receiver_sequence -gt $l.receiver_watermark -and $_.ledger_observed_qpc -le $asOf})
    [long]$observedBoundary=$l.receiver_watermark
    foreach($r in $later){if($r.receiver_sequence -gt $observedBoundary){$observedBoundary=$r.receiver_sequence}}
    Assert-FixF ($l.ledger_receiver_sequence -eq $observedBoundary) 'processed serial independently matches parent observation times, not early child receipt'
    # A cleanup final authority can contain its real cleanup UP; it settles the
    # pending DOWN, never retroactively makes that packet normal acceptance.
    $foreign=@($later|Where-Object { $_.button_flags -ne 0 -and -not $_.test_tag_matches -and -not $_.cleanup_tag_matches }).Count
    $unmatched=0;$up=$false;$matchedUps=0
    foreach($transition in $later|Where-Object {$_.button_flags -ne 0}){
        if($transition.receiver_sequence -eq $l.raw_down_receiver_sequence -and $rawMatch){continue}
        $upInputs=@($Rows|Where-Object {$_.type -cin @('input','abort_cleanup_release') -and $_.gesture -gt 0 -and $_.flags -eq 4 -and $_.sent -eq 1 -and $_.error -eq 0 -and $_.input_tag -eq $transition.extra_information -and $_.receiver_watermark -lt $transition.receiver_sequence -and $_.injection_start_qpc -le $transition.receiver_qpc -and $_.injection_start_qpc -gt $i.injection_start_qpc})
        if($transition.button_flags -eq 2 -and -not $transition.device_handle_present -and $upInputs.Count -eq 1 -and $matchedUps -eq 0){$up=$true;$matchedUps++}else{$unmatched++}
    }
    Assert-FixF ($l.non_test_button_transitions -eq $foreign -and $l.unmatched_button_transitions -eq $unmatched -and $l.matching_up_seen -eq $up) 'button ownership cannot be inferred from current high bit/tag alone'
    foreach($name in @('non_test_button_transitions','unmatched_button_transitions','non_test_movements')){Assert-IsolationInt64 (Get-AutoField $l $name) "ledger $name"}
    $movements=@($later|Where-Object {$_.cursor_sampled -and -not $_.test_tag_matches -and -not $_.cleanup_tag_matches}).Count
    Assert-FixF ($l.non_test_movements -eq $movements) 'all observed non-test movement retained separately from authority'
    $upSent=@($Rows|Where-Object {$_.type -cin @('input','abort_cleanup_release') -and $_.gesture -gt 0 -and $_.flags -eq 4 -and $_.sent -eq 1 -and $_.injection_start_qpc -gt $i.injection_start_qpc -and $_.injection_return_qpc -le $asOf}).Count -gt 0
    Assert-FixF ($l.up_command_sent -eq $upSent) 'actual UP already sent forbids a duplicate even before its receipt'
    $pending=$l.down_sent -and $rawMatch -and $downMatch -and $enterMatch -and -not $up -and -not $upSent -and $foreign -eq 0 -and $unmatched -eq 0
    Assert-FixF ($l.ledger_pending -eq $pending) 'pending ledger independently derived'
    return $pending
}
function Test-FixFQuiescence($Rows,$s){
    $requests=@(Get-IsolationRows $Rows 'abort_quiescence_request');$acks=@(Get-IsolationRows $Rows 'abort_writer_quiescence');$waits=@(Get-IsolationRows $Rows 'abort_quiescence_wait')
    Assert-FixF ($requests.Count -eq 2 -and $acks.Count -eq 2 -and $waits.Count -eq 2) 'exactly two actual owner ACK rounds'
    $result=@{}
    foreach($phase in @('initial','final')){
    $request=@($requests|Where-Object phase -ceq $phase);$ack=@($acks|Where-Object cleanup_phase -ceq $phase);$wait=@($waits|Where-Object phase -ceq $phase)
    Assert-FixF ($request.Count -eq 1 -and $ack.Count -eq 1 -and $wait.Count -eq 1) 'unique quiescence phase'
    $request=$request[0];$ack=$ack[0];$wait=$wait[0]
    foreach($name in @('posted')){Assert-AutoBoolean (Get-AutoField $request $name) "request $name"}
    foreach($name in @('motion_pending','notice_posted','acceptance_retired','takeover_scope','acknowledged','own_identity')){Assert-AutoBoolean (Get-AutoField $ack $name) "owner ACK $name"}
    Assert-AutoBoolean $wait.quiescent 'wait quiescent'
    foreach($name in @('message','source_hwnd','source_pid','source_tid','run_nonce')){Assert-IsolationInt64 (Get-AutoField $request $name) "quiescence request $name"}
    foreach($name in @('ack_tid','active_operation_id','pending_raw_count','native_calls','source_hwnd','source_pid','source_tid','run_nonce','actual_source_pid','actual_source_tid','actual_source_nonce')){Assert-IsolationInt64 (Get-AutoField $ack $name) "owner ACK $name"}
    Assert-FixF ($ack.own_identity -and $ack.actual_source_pid -eq $s.pid -and $ack.actual_source_tid -eq $s.ui_tid -and $ack.actual_source_nonce -eq $s.run_nonce) 'ACK actual source generation remains owned'
    foreach($name in @('timeout_ms','wait_result')){Assert-IsolationInt64 (Get-AutoField $wait $name) "quiescence wait $name"}
    Assert-FixF ($request.posted -and $request.request_qpc -le $request.qpc -and $ack.phase -ceq 'owner_ack' -and $ack.request_sequence -eq $request.sequence -and $ack.ack_tid -eq $s.ui_tid -and $ack.ack_qpc -ge $request.request_qpc -and $ack.ack_qpc -le $ack.qpc -and $ack.acknowledged -and $ack.active_operation_id -eq 0 -and $ack.pending_raw_count -eq 0 -and -not $ack.motion_pending -and -not $ack.notice_posted -and $ack.acceptance_retired -and -not $ack.takeover_scope -and $ack.native_calls -eq 0) 'real owner-thread ACK and retired writers'
    $owned=Get-FixFOne $Rows 'owned'
    foreach($r in @($request,$ack)){Assert-FixF ($r.source_hwnd -eq $owned.hwnd -and $r.source_pid -eq $s.pid -and $r.source_tid -eq $s.ui_tid -and $r.run_nonce -eq $s.run_nonce) 'quiescence source generation'}
    Assert-FixF ($wait.request_sequence -eq $request.sequence -and $wait.ack_sequence -eq $ack.sequence -and $wait.timeout_ms -eq 2000 -and $wait.wait_result -eq 0 -and $wait.quiescent -and $wait.sequence -gt $ack.sequence) 'bounded observed quiescence wait'
    $result[$phase]=$ack
    }
    Assert-FixF ($result.initial.sequence -lt $result.final.sequence -and $result.initial.ack_qpc -lt $result.final.ack_qpc) 'distinct ordered owner ACK rounds'
    return $result
}
function Test-FixFAbortAuthority($a,$Rows,$s,$ack,$initial){
    $owned=Get-FixFOne $Rows 'owned'
    Assert-FixF ($a.phase -cin @('initial','api_boundary','final') -and $a.query_start_qpc -gt 0 -and $a.query_finish_qpc -ge $a.query_start_qpc -and $a.query_finish_qpc -le $a.qpc) 'fresh ordered cleanup query'
    Assert-FixF ($a.source_hwnd -eq $owned.hwnd -and $a.source_pid -eq $s.pid -and $a.source_tid -eq $s.ui_tid -and $a.run_nonce -eq $s.run_nonce) 'cleanup target generation'
    foreach($name in @('source_hwnd','source_pid','source_tid','actual_source_pid','actual_source_tid','actual_source_nonce','run_nonce','foreground_hwnd','foreground_pid','foreground_tid','capture_hwnd','menu_owner_hwnd','move_size_hwnd','gui_flags','cursor_root')){if($null -ne (Get-AutoField $a $name)){Assert-IsolationInt64 $a.$name "authority $name"}}
    foreach($name in @('fixture_scope_active','own_identity','desktop_ready','source_visible','foreground_matches','gui_query_succeeded','expected_native_mode','menu_clear','cursor_success','root_is_source','receiver_healthy','log_healthy','foreign_capture_transferred','mouse_buttons_swapped','input_mapping_supported','writer_quiescent','acceptance_retired','api_boundary_stable','eligible','acceptance_eligible')){Assert-AutoBoolean (Get-AutoField $a $name) "abort authority $name"}
    $identity=$a.actual_source_pid -eq $s.pid -and $a.actual_source_tid -eq $s.ui_tid -and $a.actual_source_nonce -eq $s.run_nonce
    $fg=$a.foreground_hwnd -eq $owned.hwnd -and $a.foreground_pid -eq $s.pid -and $a.foreground_tid -eq $s.ui_tid
    $mode=$a.gui_query_succeeded -and ($a.gui_flags -band 30) -eq 2
    $capture=$a.gui_query_succeeded -and $a.capture_hwnd -eq $owned.hwnd;$move=$a.gui_query_succeeded -and $a.move_size_hwnd -eq $owned.hwnd
    $menu=$a.gui_query_succeeded -and $a.menu_owner_hwnd -eq 0 -and ($a.gui_flags -band 28) -eq 0
    $root=$a.cursor_success -and $a.cursor_root -eq $owned.hwnd
    $clear=Test-FixFOtherClear $a.other_states
    if($null -ne $a.left_down){Assert-AutoBoolean $a.left_down 'authority actual left'}
    if($null -ne $a.other_input_clear){Assert-AutoBoolean $a.other_input_clear 'authority other clear'}
    Assert-FixF ($a.own_identity -eq $identity -and $a.foreground_matches -eq $fg -and $a.expected_native_mode -eq $mode -and $a.menu_clear -eq $menu -and $a.root_is_source -eq $root -and $a.other_input_clear -eq $clear -and $a.input_mapping_supported -eq (-not $a.mouse_buttons_swapped)) 'cleanup raw identity/GUI/root/input summaries'
    if($a.cursor_success){Assert-AutoPoint $a.cursor 'cleanup current cursor'}else{Assert-FixF ($null -eq $a.cursor) 'unknown cleanup cursor is null'}
    Assert-FixF (-not $a.acceptance_eligible -and $a.event_scope -ceq 'cleanup' -and $a.quiescence_ack_sequence -eq $ack.sequence -and $a.sequence -gt $ack.sequence) 'dedicated cleanup scope + ACK reference'
    $pending=Test-FixFPendingLedger $a $Rows $s
    $stable=$true
    if($a.phase -cne 'initial'){
        Assert-FixF ($null -ne $initial -and $a.initial_authority_sequence -eq $initial.sequence -and $a.query_start_qpc -gt $initial.query_finish_qpc) 'second independent fresh boundary'
        if($a.phase -ceq 'api_boundary'){$stable=(Test-EndPointExact $a.cursor $initial.cursor) -and $a.down_input_sequence -eq $initial.down_input_sequence -and $a.source_hwnd -eq $initial.source_hwnd -and $a.foreground_hwnd -eq $initial.foreground_hwnd -and $a.capture_hwnd -eq $initial.capture_hwnd -and $a.move_size_hwnd -eq $initial.move_size_hwnd -and $a.gui_flags -eq $initial.gui_flags}
    }
    Assert-FixF ($a.api_boundary_stable -eq $stable) 'changed API boundary must deny UP'
    $eligible=$pending -and $a.fixture_scope_active -and $identity -and $a.desktop_ready -and $a.source_visible -and $fg -and $capture -and $move -and $mode -and $menu -and $root -and $a.left_down -eq $true -and $clear -eq $true -and $a.receiver_healthy -and $a.log_healthy -and -not $a.foreign_capture_transferred -and $a.writer_quiescent -and $a.acceptance_retired -and $a.input_mapping_supported -and $stable
    Assert-FixF ($a.eligible -eq $eligible) 'independently derived TestAbortCleanupAuthority'
    return $eligible
}
function Test-FixFControlledAbort($Rows,$s){
    $last=$Rows[-1];$owned=Get-FixFOne $Rows 'owned';$guard=Get-FixFOne $Rows 'guard'
    Assert-FixF ($owned.pid -eq $s.pid -and $owned.tid -eq $s.ui_tid -and $owned.run_nonce -eq $s.run_nonce -and $owned.actual_source_nonce -eq $s.run_nonce -and $guard.pid -eq $s.pid -and $guard.tid -eq $s.ui_tid) 'fresh owned source and guard'
    $desktop=Get-FixFOne $Rows 'desktop_gate';Assert-FixF ($desktop.active_unlocked -and $desktop.input_desktop_matches) 'native desktop context'
    $bootstrap=Test-TakeoverGlobalForegroundV2 $Rows @($owned) $true;Assert-FixF ($bootstrap.result -ceq 'PASS') 'actual global foreground verified'
    $raw=Test-EndHandoffRawEvidence $Rows $s @($owned) $true;Assert-FixF ($raw.Healthy -and $raw.Gate -ceq 'PASS') 'receiver/preflight healthy'
    $win=Test-IsolationWinEvents $Rows @($owned) $s $true;Assert-FixF ($win.Installed -and $win.Removed -and $null -ne $win.Start -and $null -ne $win.End) 'actual own hook START/END cleanup witness'
    $fault=Get-FixFOne $Rows 'controlled_abort_fault';$sample=Get-FixFRef $Rows $fault.sample_sequence 'sample';$enter=Get-FixFRef $Rows $fault.native_enter_sequence 'ENTER';$down=Get-FixFRef $Rows $fault.native_down_sequence 'native_button_down'
    Assert-AutoBoolean $fault.guard_passed 'fault guard';Assert-AutoBoolean $sample.post_cancel 'sample post cancel';Assert-AutoBoolean $sample.left_down 'sample left'
    Assert-FixF ($fault.fault -ceq 'after_sample1_before_cancel' -and $fault.reason -ceq 'BLOCKED_BY_TEST_FAULT' -and $fault.guard_passed -and $fault.non_test_movements -eq 0 -and $fault.non_test_button_transitions -eq 0 -and $fault.unmatched_button_transitions -eq 0 -and $sample.index -eq 1 -and -not $sample.post_cancel -and $sample.left_down -and $sample.sequence -gt $enter.sequence -and $sample.sequence -gt $down.sequence -and $fault.sequence -gt $sample.sequence) 'expected fault boundary does not swallow real interference'
    Assert-FixF (@(Get-IsolationRows $Rows 'sample').Count -eq 1 -and @(Get-IsolationRows $Rows 'ENTER').Count -eq 1) 'one real ENTER/sample1 only'
    foreach($type in @('cancel_begin','cancel_return','cancel_message','writer_begin','writer_result','handoff_begin','handoff_complete','raw_quantum','takeover_end','source_final_acceptance','input_isolation_ready','input_shield_created','takeover_failure','receiver_error','winevent_error')){Assert-FixF (@(Get-IsolationRows $Rows $type).Count -eq 0) "abort must not authorize $type"}
    $blocked=Get-FixFOne $Rows 'blocked';Assert-FixF ($blocked.reason -ceq 'BLOCKED_BY_TEST_FAULT' -and $blocked.sequence -gt $fault.sequence) 'gesture remains actually BLOCKED'
    $acks=Test-FixFQuiescence $Rows $s;$ack=$acks.initial
    $authority=@(Get-IsolationRows $Rows 'abort_cleanup_authority');Assert-FixF ($authority.Count -eq 3) 'initial/boundary/final snapshots'
    $initial=@($authority|Where-Object phase -ceq 'initial');$boundary=@($authority|Where-Object phase -ceq 'api_boundary');$final=@($authority|Where-Object phase -ceq 'final')
    Assert-FixF ($initial.Count -eq 1 -and $boundary.Count -eq 1 -and $final.Count -eq 1) 'unique abort authority phases'
    Assert-FixF (Test-FixFAbortAuthority $initial[0] $Rows $s $ack $null) 'initial authority'
    Assert-FixF (Test-FixFAbortAuthority $boundary[0] $Rows $s $ack $initial[0]) 'API boundary authority'
    $release=Get-FixFOne $Rows 'abort_cleanup_release';$intent=Get-FixFRef $Rows $release.ledger_intent_sequence 'ledger_input_intent'
    foreach($name in @('flags','actual_mouse_flags','normalized_dx','normalized_dy','input_tag','sent','error')){Assert-IsolationInt64 (Get-AutoField $release $name) "cleanup API $name"}
    Assert-AutoBoolean $release.acceptance_eligible 'cleanup release acceptance'
    Assert-FixF ($release.flags -eq 4 -and $release.actual_mouse_flags -eq 4 -and $release.normalized_dx -eq 0 -and $release.normalized_dy -eq 0 -and $release.input_tag -eq $s.abort_cleanup_tag -and $release.sent -eq 1 -and $release.error -eq 0 -and $release.initial_authority_sequence -eq $initial[0].sequence -and $release.boundary_authority_sequence -eq $boundary[0].sequence -and $release.injection_start_qpc -ge $boundary[0].query_finish_qpc -and $release.injection_return_qpc -ge $release.injection_start_qpc -and $release.injection_return_qpc -le $release.qpc -and $intent.intent_purpose -ceq 'abort_up' -and $intent.flags -eq 4 -and $intent.input_tag -eq $release.input_tag -and $intent.receiver_watermark -eq $release.receiver_watermark -and $intent.qpc -le $release.injection_start_qpc -and $intent.sequence -lt $release.sequence) 'exactly one cleanup-only UP with fresh authority and actual API'
    Assert-FixF (@(Get-IsolationRows $Rows 'cleanup_release').Count -eq 0 -and @(Get-IsolationRows $Rows 'abort_cleanup_skipped').Count -eq 0 -and @($Rows|Where-Object {$_.type -ceq 'input' -and $_.gesture -gt 0 -and $_.flags -eq 4}).Count -eq 0) 'no second or normal gesture UP'
    $proof=Get-FixFOne $Rows 'abort_cleanup_raw_up';$packet=@($Rows|Where-Object {$_.type -ceq 'raw_input' -and $_.receiver_sequence -eq $proof.receiver_sequence -and $_.receiver_qpc -eq $proof.receiver_qpc})
    foreach($name in @('wait_result','timeout_ms','input_tag')){Assert-IsolationInt64 (Get-AutoField $proof $name) "cleanup Raw proof $name"}
    Assert-AutoBoolean $proof.raw_up_observed 'actual cleanup Raw UP observed';Assert-AutoBoolean $proof.acceptance_eligible 'Raw proof acceptance'
    Assert-FixF ($proof.wait_result -eq 0 -and $proof.timeout_ms -eq 2000 -and $proof.raw_up_observed -and $proof.receiver_watermark -eq $release.receiver_watermark -and $proof.injection_start_qpc -eq $release.injection_start_qpc -and $proof.input_tag -eq $release.input_tag -and $packet.Count -eq 1) 'Raw UP actual reference'
    $u=$packet[0];Assert-FixF ($u.receiver_sequence -gt $release.receiver_watermark -and $u.receiver_qpc -ge $release.injection_start_qpc -and $u.button_flags -eq 2 -and $u.extra_information -eq $s.abort_cleanup_tag -and $u.cleanup_tag_matches -and -not $u.test_tag_matches -and -not $u.device_handle_present -and $u.raw_scope -ceq 'cleanup' -and -not $u.acceptance_eligible -and $u.event_scope -ceq 'cleanup') 'not old/preflight/normal UP'
    $end=Get-FixFOne $Rows 'abort_cleanup_end_wait';$exit=Get-FixFRef $Rows $end.native_exit_sequence 'EXIT'
    foreach($name in @('native_wait_result','winevent_wait_result','timeout_ms')){Assert-IsolationInt64 (Get-AutoField $end $name) "cleanup END wait $name"}
    Assert-AutoBoolean $end.acceptance_eligible 'END wait acceptance'
    Assert-FixF ($end.native_wait_result -eq 0 -and $end.winevent_wait_result -eq 0 -and $end.timeout_ms -eq 2000 -and $exit.receipt_qpc -eq $end.native_exit_qpc -and $exit.receipt_qpc -ge $release.injection_start_qpc -and $exit.owner_capture -eq 0 -and $exit.event_scope -ceq 'cleanup' -and -not $exit.event_acceptance_eligible -and $end.winevent_end_sequence -eq $win.End.sequence -and $end.winevent_end_qpc -eq $win.End.callback_qpc -and $win.End.callback_qpc -ge $release.injection_start_qpc -and $win.End.event_scope -ceq 'cleanup' -and -not $win.End.event_acceptance_eligible) 'actual native EXIT and independently matched WinEvent END in cleanup scope'
    Assert-FixF ($final[0].initial_ack_sequence -eq $ack.sequence -and $acks.final.sequence -gt $end.sequence) 'second actual ACK after cleanup end before final snapshot'
    $null=Test-FixFAbortAuthority $final[0] $Rows $s $acks.final $initial[0]
    $f=$final[0];$completion=Get-FixFOne $Rows 'abort_cleanup_final'
    foreach($name in @('final_left_down','pending_work','acceptance_eligible')){Assert-AutoBoolean (Get-AutoField $completion $name) "final $name"}
    Assert-FixF ($f.sequence -gt $proof.sequence -and $f.sequence -gt $end.sequence -and $f.fixture_scope_active -and $f.own_identity -and $f.desktop_ready -and $f.source_visible -and $f.foreground_matches -and $f.gui_query_succeeded -and $f.capture_hwnd -eq 0 -and $f.move_size_hwnd -eq 0 -and $f.menu_owner_hwnd -eq 0 -and ($f.gui_flags -band 30) -eq 0 -and $f.cursor_success -and $f.root_is_source -and $f.left_down -eq $false -and $f.other_input_clear -and $f.receiver_healthy -and $f.log_healthy -and -not $f.foreign_capture_transferred -and $f.input_mapping_supported -and $f.writer_quiescent -and $f.acceptance_retired -and -not $f.ledger_pending -and $f.matching_up_seen) 'final valid UP, GUI ended, ledger settled and no writer'
    foreach($name in @('cleanup_up_attempted','cleanup_up_sent','raw_up_observed','native_exit_observed','winevent_end_observed','final_capture_clear','final_mode_clear','ledger_settled','writer_quiescent','acceptance_retired')){Assert-FixF (Test-FixFBool (Get-AutoField $completion $name) $name) "actual completion $name"}
    Assert-FixF ($completion.cleanup_result -ceq 'PASS' -and $completion.final_authority_sequence -eq $f.sequence -and $completion.final_left_down -eq $false -and -not $completion.pending_work -and -not $completion.acceptance_eligible) 'cleanup PASS does not imply takeover PASS'
    foreach($r in $Rows|Where-Object {$_.sequence -ge $ack.sequence -and $_.type -notin @('shutdown','receiver_shutdown','winevent_hook_removed')}){Assert-FixF ($r.event_scope -ceq 'cleanup' -and -not $r.event_acceptance_eligible) 'retired event ingress cannot be acceptance eligible'}
    $receiver=Get-FixFOne $Rows 'receiver_shutdown';$hook=Get-FixFOne $Rows 'winevent_hook_removed'
    Assert-FixF ($receiver.sequence -gt $completion.sequence -and $hook.sequence -gt $completion.sequence -and $last.result -ceq 'BLOCKED' -and $last.owned_window_destroyed -and $last.guard_window_destroyed -and $last.receiver_stopped -and $last.winevent_hook_removed -and -not $last.external_windows_touched -and -not $last.cursor_restored -and $last.takeover_geometry_writes -eq 0 -and $last.source_acceptance_sequence -eq 0 -and -not $last.input_shield_created -and $last.input_shield_destroyed -and -not $last.input_shield_activated -and $last.run_nonce -eq $s.run_nonce) 'ordered resource cleanup, no acceptance or foreign effect'
    # Non-test movement is not by itself an authority denial, but the expected
    # deterministic fixture cannot swallow any extra real input during the run.
    Assert-FixF (@($raw.Packets|Where-Object {$_.gesture -gt 0 -and $_.cursor_sampled -and -not $_.test_tag_matches -and -not $_.cleanup_tag_matches}).Count -eq 0) 'unexpected real movement during deterministic fixture'
    return [pscustomobject]@{Result='PASS';FixtureResult='PASS_EXPECTED_ABORT';GestureResult='BLOCKED_BY_TEST_FAULT';CleanupResult='PASS';TakeoverAcceptance='NOT_RUN';ContractVerified=$true;Operation=$s.operation;TestMode=$s.test_mode;Reasons=@('BLOCKED_BY_TEST_FAULT');FinalLeftDown=$false;SyntheticFixture=((Get-AutoField $s 'synthetic_fixture') -eq $true)}
}
function Test-FixFInputReliabilityRecords([object[]]$Rows,[switch]$AllowSynthetic){
    $s=Test-FixFEnvelope $Rows -AllowSynthetic:$AllowSynthetic
    $diagnostics=@(Get-IsolationRows $Rows 'input_fence_diagnostic');Assert-FixF ($diagnostics.Count -gt 0) 'diagnostics required'
    foreach($d in $diagnostics){$null=Test-FixFFenceDiagnostic $d $Rows}
    Assert-FixF (@(Get-IsolationRows $Rows 'input_fence').Count -eq @($diagnostics|Where-Object passed -eq $true).Count) 'every success alias references new actual diagnostic'
    $failed=@($diagnostics|Where-Object passed -eq $false)
    if($failed.Count){
        # Valid observed rejection is not a passing fixture, nor JSON corruption.
        # No incomplete cleanup/acceptance claim is upgraded by this result.
        return [pscustomobject]@{Result='BLOCKED';FixtureResult='BLOCKED';GestureResult=$failed[0].failure_reason;CleanupResult='UNKNOWN';TakeoverAcceptance='NOT_RUN';ContractVerified=$false;Operation=$s.operation;TestMode=$s.test_mode;Reasons=@(($failed[0].failure_reason+':'+$failed[0].failure_subreason));FailureDiagnosticSequence=$failed[0].sequence;FailureDiagnosticVerified=$true;FinalLeftDown=$null}
    }
    foreach($packet in @(Get-IsolationRows $Rows 'raw_input')){
        Assert-AutoInteger $packet.extra_information 'actual Raw extra information';Assert-AutoBoolean $packet.cleanup_tag_matches 'cleanup tag match'
        Assert-IsolationInt64 $packet.ledger_observed_qpc 'actual parent Raw processing QPC'
        Assert-FixF ($packet.ledger_observed_qpc -ge $packet.receiver_qpc -and $packet.ledger_observed_qpc -le $packet.qpc) 'child receipt / parent observation / record clock ordering'
        Assert-AutoBoolean $packet.acceptance_eligible 'Raw acceptance';Assert-AutoBoolean $packet.test_tag_matches 'Raw test tag match'
        Assert-FixF ($packet.cleanup_tag_matches -eq ($packet.extra_information -eq $s.abort_cleanup_tag) -and $packet.test_tag_matches -eq ($packet.extra_information -eq 0x50424D41)) 'actual tag numeric values, not runtime assertions'
    }
    $ledgerRows=@(Get-IsolationRows $Rows 'test_down_ledger');Assert-FixF ($ledgerRows.Count -gt 0) 'actual DOWN ledger required'
    foreach($ledger in $ledgerRows){$null=Test-FixFPendingLedger $ledger $Rows $s}
    if($s.test_mode -ceq 'controlled_abort'){return Test-FixFControlledAbort $Rows $s}
    foreach($r in $Rows){Assert-FixF ($r.type -cnotlike 'abort_*' -and $r.type -cne 'controlled_abort_fault' -and $r.event_scope -ceq 'gesture') 'normal single cannot consume cleanup events'}
    $normal=Test-FixFNormalOwnedRecords $Rows
    Assert-FixF ($normal.Result -ceq 'PASS') 'unchanged full normal acceptance required'
    return [pscustomobject]@{Result='PASS';FixtureResult='PASS';GestureResult='PASS';CleanupResult='NOT_NEEDED';TakeoverAcceptance='PASS';ContractVerified=$true;Operation=$s.operation;TestMode=$s.test_mode;Reasons=@();NormalProof=$normal;FinalLeftDown=$normal.FinalLeftDown;SyntheticFixture=((Get-AutoField $s 'synthetic_fixture') -eq $true)}
}
function Test-FixFInputReliabilityEvidence([string]$Path,[switch]$AllowSynthetic){
    $lines=@(Get-Content -LiteralPath $Path -Encoding UTF8);Assert-FixF ($lines.Count -gt 0 -and @($lines|Where-Object {[string]::IsNullOrWhiteSpace($_)}).Count -eq 0) 'nonempty JSONL records'
    $rows=@($lines|ForEach-Object {$_|ConvertFrom-Json -ErrorAction Stop})
    return Test-FixFInputReliabilityRecords $rows -AllowSynthetic:$AllowSynthetic
}
