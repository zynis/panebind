Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-end-handoff-validation.ps1')

# Synthetic contract tests only: no GUI, native messages, input, or window API.
$checks=0
function Check-EndHandoff([bool]$Value,[string]$Name){if(-not $Value){throw "END handoff fixture: $Name"};$script:checks++}
$startP=@(100,100,740,540);$startV=@(111,100,729,529);$pointer=@(300,109)
$intent=Get-EndHandoffIntended Move $startP $startV $pointer @(327,109)
Check-EndHandoff ((Test-AutoRect $intent.Positioning @(127,100,767,540)) -and (Test-AutoRect $intent.Visible @(138,100,756,529))) 'full START-to-current Move delta'
$resize=Get-EndHandoffIntended BottomResize $startP $startV @(300,539) @(317,566)
Check-EndHandoff ((Test-AutoRect $resize.Positioning @(100,100,740,567)) -and (Test-AutoRect $resize.Visible @(111,100,729,556))) 'Bottom Resize freezes other edges and the original checked frame bridge'
$barrier=@{CancelCalls=1;NativeEnter=$true;NativeDrag=$true;AuthorityValid=$true;CancelReturned=$true;RealExit=$true;CaptureReleased=$true;LeftHeldAtExit=$true;GuiClearAfterExit=$true;RawHealthy=$true;RawContinuation=$true;NativeDragAfterReturn=0;UnattributedGeometryAfterEnd=0;WritesBeforeEnd=0}
Check-EndHandoff ((Get-EndHandoffBarrierGate @barrier) -ceq 'PASS_WITH_TERMINAL_SETTLEMENT') 'A terminal restoration before real END is allowed'
foreach($field in @('UnattributedGeometryAfterEnd','WritesBeforeEnd','NativeDragAfterReturn')){
    $bad=@{};foreach($key in $barrier.Keys){$bad[$key]=$barrier[$key]};$bad[$field]=1
    Check-EndHandoff ((Get-EndHandoffBarrierGate @bad) -ceq 'FAIL') "B/C/F actual authority counterexample $field"
}
$reconcile=@{EndObserved=$true;AuthorityValid=$true;ActualPositioning=$startP;ActualVisible=$startV;IntendedPositioning=$intent.Positioning;IntendedVisible=$intent.Visible;NativeCalls=1;EndQpc=1000;NativeStartQpc=1001;TargetUsesFullDelta=$true;PostverifyExact=$true}
Check-EndHandoff ((Get-EndHandoffReconciliationGate @reconcile) -ceq 'PASS') 'E full original intent plus one post-END reconciliation'
$reconcile.TargetUsesFullDelta=$false
Check-EndHandoff ((Get-EndHandoffReconciliationGate @reconcile) -ceq 'FAIL') 'D EXIT/post-END-only delta loses original displacement'
$reconcile.TargetUsesFullDelta=$true;$reconcile.NativeCalls=2
Check-EndHandoff ((Get-EndHandoffReconciliationGate @reconcile) -ceq 'FAIL') 'H multiple handoff writes before next Raw are forbidden'
$reconcile.NativeCalls=1;$reconcile.NativeStartQpc=1000
Check-EndHandoff ((Get-EndHandoffReconciliationGate @reconcile) -ceq 'FAIL') 'C first write must be strictly after END'
$reconcile.NativeStartQpc=1001;$reconcile.ActualPositioning=$intent.Positioning;$reconcile.ActualVisible=$intent.Visible;$reconcile.NativeCalls=0
Check-EndHandoff ((Get-EndHandoffReconciliationGate @reconcile) -ceq 'PASS') 'G already-exact stable post-END geometry requires zero writes'
foreach($field in @('AuthorityValid','NativeEnter','NativeDrag','RealExit','RawHealthy','RawContinuation','CaptureReleased','LeftHeldAtExit','GuiClearAfterExit')){
    $bad=@{};foreach($key in $barrier.Keys){$bad[$key]=$barrier[$key]};$bad[$field]=$false
    Check-EndHandoff ((Get-EndHandoffBarrierGate @bad) -ceq 'UNKNOWN') "missing fact $field is not positive cancellation failure"
}
Check-EndHandoff ((Get-EndHandoffBarrierGate) -ceq 'UNKNOWN') 'no native data cannot FAIL or PASS'
$quantum=@{AuthorityValid=$true;RealEnd=$true;FreshRawMovement=$true;RawUpAlreadyObserved=$false;TargetUsesOriginalAnchor=$true;NativeCalls=1;EndQpc=1000;NativeStartQpc=1001;PostverifyExact=$true}
Check-EndHandoff ((Get-EndHandoffQuantumGate @quantum) -ceq 'PASS') 'fresh Raw movement triggers at most one original-anchor write'
foreach($kind in @('up','recursive-write','cumulative-anchor','pre-end','postverify')){
    $bad=@{};foreach($key in $quantum.Keys){$bad[$key]=$quantum[$key]}
    switch($kind){'up'{$bad.RawUpAlreadyObserved=$true};'recursive-write'{$bad.NativeCalls=2};'cumulative-anchor'{$bad.TargetUsesOriginalAnchor=$false};'pre-end'{$bad.NativeStartQpc=1000};'postverify'{$bad.PostverifyExact=$false}}
    Check-EndHandoff ((Get-EndHandoffQuantumGate @bad) -ceq 'FAIL') "quantum failure $kind"
}
$completion=@{ObservationComplete=$true;RawUpObserved=$true;FinalCursorValid=$true;FinalGeometryExact=$true;PendingWrite=$false;GuiClear=$true;ContinuationMovements=18;NativeDragAfterEnd=0}
Check-EndHandoff ((Get-EndHandoffCompletionGate @completion) -ceq 'PASS') 'actual Raw UP terminates exact 18-movement continuation'
foreach($kind in @('missing-up','bad-cursor','wrong-final-rect','pending-write','native-mode','short-path','native-drag')){
    $bad=@{};foreach($key in $completion.Keys){$bad[$key]=$completion[$key]}
    switch($kind){'missing-up'{$bad.RawUpObserved=$false};'bad-cursor'{$bad.FinalCursorValid=$false};'wrong-final-rect'{$bad.FinalGeometryExact=$false};'pending-write'{$bad.PendingWrite=$true};'native-mode'{$bad.GuiClear=$false};'short-path'{$bad.ContinuationMovements=14};'native-drag'{$bad.NativeDragAfterEnd=1}}
    Check-EndHandoff ((Get-EndHandoffCompletionGate @bad) -ceq 'FAIL') "complete takeover failure $kind"
}
Check-EndHandoff ((Get-EndHandoffCompletionGate) -ceq 'NOT_RUN') 'no takeover lifecycle cannot be accepted or rejected'
function Copy-EndFixture($Rows){return ($Rows|ConvertTo-Json -Depth 15 -Compress|ConvertFrom-Json)}
function Add-EndRow([string]$Type,$Fields=@{}){
    $script:endClock+=100
    $row=[ordered]@{schema='r1c4b-takeover-owned/v2';sequence=$script:endRows.Count+1;gesture=$script:endGesture;qpc=$script:endClock;type=$Type}
    foreach($name in $Fields.Keys){$row[$name]=$Fields[$name]}
    $script:endRows.Add([pscustomobject]$row)
    return $script:endRows[-1]
}
function End-Geometry($P){return @{positioning=@($P);visible=@(($P[0]+11),$P[1],($P[2]-11),($P[3]-11));positioning_error=0;visible_hresult=0}}
function End-Authority {
    return @{target=300;source_pid=100;source_tid=101;own_identity=$true;desktop_ready=$true;source_visible=$true;foreground=300;foreground_pid=100;foreground_tid=101;gui_query_succeeded=$true;capture_hwnd=0;menu_owner_hwnd=0;move_size_hwnd=0;gui_flags=0;buttons_modifiers_clear=$true;left_down=$true;receiver_healthy=$true;raw_up_seen=$false;cursor_root=300;guard=301;dpi=96;monitor=1}
}
function End-Diagnostic($ActualP,$ActualV,$RequestedP,$RequestedV){
    $pExact=Test-AutoRect $ActualP $RequestedP;$vExact=Test-AutoRect $ActualV $RequestedV
    $dp=@();$dv=@();for($i=0;$i -lt 4;++$i){$dp+=([long]$ActualP[$i]-[long]$RequestedP[$i]);$dv+=([long]$ActualV[$i]-[long]$RequestedV[$i])}
    return [pscustomobject]@{capture_succeeded=$true;native_success=$true;win32_error=0;source_member=0;requested_positioning=@($RequestedP);requested_visible=@($RequestedV);actual_positioning=@($ActualP);actual_visible=@($ActualV);positioning_exact=$pExact;visible_exact=$vExact;positioning_edge_delta=$dp;visible_edge_delta=$dv;other_members_exact=$true;receipt_health=$true;source_context_exact=$true;failure_class=$(if(-not $pExact -and -not $vExact){'BothGeometryMismatch'}elseif(-not $pExact){'PositioningMismatch'}elseif(-not $vExact){'VisibleMismatch'}else{'None'})}
}
function Add-EndInput([int]$Flags,$Point,[bool]$Post=$false,[bool]$Priming=$false,[bool]$Restore=$false){
    $capture=if($script:endGesture -gt 0 -and $script:endLeft -and -not $Post){300}else{0}
    $null=Add-EndRow 'input_fence' @{cursor=@($script:endCursor);expected_cursor=@($script:endCursor);foreground=300;target=300;left_down=$script:endLeft;cancel_pending=$false;post_cancel=$Post;priming=$Priming;gui_query_succeeded=$true;capture_hwnd=$capture;window_from_point_root=300;gui_flags=$(if($capture){2}else{0});menu_owner_hwnd=0;move_size_hwnd=$(if($capture){300}else{0})}
    if($Restore){$null=Add-EndRow 'cursor_restore_begin' @{point=@($Point);target=300}}
    elseif($Flags -eq 1 -and ($script:endGesture -eq 0 -or $Post)){$null=Add-EndRow 'destination_fence' @{point=@($Point);root=300;target=300;foreground=300;capture_hwnd=0;menu_owner_hwnd=0;move_size_hwnd=0;gui_flags=0;cancel_pending=$false;left_down=$script:endLeft}}
    $dx=if($Flags -eq 1){[long][Math]::Floor((2*[decimal]$Point[0]+1)*65536/(2*1920))}else{0};$dy=if($Flags -eq 1){[long][Math]::Floor((2*[decimal]$Point[1]+1)*65536/(2*1200))}else{0}
    $r=Add-EndRow 'input' @{flags=$Flags;point=@($Point);sent=1;error=0;input_tag=0x50424D41;injection_start_qpc=$script:endClock+10;injection_return_qpc=$script:endClock+20;restoring_cursor=$Restore;normalized_dx=$dx;normalized_dy=$dy;actual_mouse_flags=$(if($Flags -eq 1){57345}else{$Flags});receiver_watermark=$script:endSerial;virtual_screen=@(0,0,1920,1200)}
    $script:endPreviousCursor=@($script:endCursor);$script:endLastInput=$r
    if($Flags -eq 1){$script:endCursor=@($Point)}
    if($Flags -eq 2){$script:endLeft=$true};if($Flags -eq 4){$script:endLeft=$false}
    return $r
}
function Add-EndRaw([bool]$Movement,[int]$Buttons=0){
    ++$script:endSerial
    $r=Add-EndRow 'raw_input' @{receiver_sequence=$script:endSerial;receiver_qpc=$script:endClock+99;receiver_hwnd=400;receiver_pid=200;receiver_tid=201;input_code=1;raw_flags=$(if($Movement){11}else{0});dx=$(if($Movement){$script:endLastInput.normalized_dx}else{0});dy=$(if($Movement){$script:endLastInput.normalized_dy}else{0});button_flags=$Buttons;cursor_sampled=$Movement;cursor_success=$Movement;cursor=$(if($Movement){@($script:endPreviousCursor)}else{$null});left_down=$(if($Buttons -band 2){$true}else{$script:endLeft});foreground_hwnd=300;foreground_pid=100;device_handle_present=$false;test_tag_matches=$true}
    if($Movement){
        ++$script:endMovement
        $null=Add-EndRow 'raw_wait_begin' @{expected_cursor=@($script:endCursor);input_start_qpc=$script:endLastInput.injection_start_qpc;timeout_ms=2000}
        $null=Add-EndRow 'raw_wait_result' @{wait_result=0;raw_packets=($script:endSerial-1);movement_count=$script:endMovement}
        $null=Add-EndRow 'raw_motion_correlation' @{receiver_sequence=$script:endSerial;delivered=$true;snapshot_cursor_matches=$false}
    }
    return $r
}
function Add-EndUpFence {
    $null=Add-EndRow 'input_fence' @{cursor=@($script:endCursor);expected_cursor=@($script:endCursor);foreground=300;target=300;left_down=$false;cancel_pending=$false;post_cancel=($script:endGesture -gt 0);priming=$false;gui_query_succeeded=$true;capture_hwnd=0;window_from_point_root=300;gui_flags=0;menu_owner_hwnd=0;move_size_hwnd=0}
}
function Add-EndWriter([string]$Kind,$Raw=$null){
    $desired=Get-EndHandoffIntended $script:endOperation $script:endStartP $script:endStartV $script:endPointer $script:endCursor
    $fields=End-Authority;$fields.operation_id=++$script:endOperationId;$fields.quantum_id=$(if($Kind -eq 'handoff'){0}else{$script:endQuantum++;$script:endQuantum});$fields.kind=$Kind;$fields.raw_first_sequence=$(if($Kind -eq 'handoff'){0}else{$Raw.receiver_sequence});$fields.raw_last_sequence=$fields.raw_first_sequence;$fields.raw_trigger_qpc=$(if($Kind -eq 'handoff'){0}else{$Raw.receiver_qpc});$fields.cursor=@($script:endCursor);$fields.cursor_delta=$desired.Delta;$fields.intended_positioning=$desired.Positioning;$fields.intended_visible=$desired.Visible;$fields.before_positioning=@($script:endP);$fields.before_visible=@($script:endV);$fields.expected_before_positioning=@($script:endP);$fields.expected_before_visible=@($script:endV);$fields.native_calls=[int](-not (Test-AutoRect $script:endP $desired.Positioning) -or -not (Test-AutoRect $script:endV $desired.Visible));$fields.end_qpc=$script:endExit.receipt_qpc
    $b=Add-EndRow 'writer_begin' $fields
    $start=if($b.native_calls){$script:endClock+1}else{0};$return=if($b.native_calls){$script:endClock+2}else{0}
    if($b.native_calls){
        $script:endWrites++;$script:endP=@($desired.Positioning);$script:endV=@($desired.Visible)
        $change=End-Geometry $script:endP;$change.operation_id=$b.operation_id;$change.after_end=$true;$change.unexpected_change=$false
        $null=Add-EndRow 'POSITION_CHANGED' $change
    }
    $fields=End-Geometry $script:endP;$fields.operation_id=$b.operation_id;$fields.quantum_id=$b.quantum_id;$fields.kind=$Kind;$fields.native_calls=$b.native_calls;$fields.native_success=$true;$fields.error=0;$fields.native_start_qpc=$start;$fields.native_return_qpc=$return;$fields.positioning_exact=$true;$fields.visible_exact=$true;$fields.postverify_exact=$true;$fields.diagnostic=End-Diagnostic $script:endP $script:endV $desired.Positioning $desired.Visible;$fields.full_positioning=@($script:endP);$fields.full_visible=@($script:endV)
    $r=Add-EndRow 'writer_result' $fields
    if($Kind -eq 'handoff'){$null=Add-EndRow 'handoff_complete' @{operation_id=$b.operation_id;native_calls=$b.native_calls;exact=$true;end_qpc=$script:endExit.receipt_qpc}}
    else{$null=Add-EndRow 'raw_quantum' @{quantum_id=$b.quantum_id;raw_first_sequence=$b.raw_first_sequence;raw_last_sequence=$b.raw_last_sequence;raw_trigger_qpc=$b.raw_trigger_qpc;coalesced_count=1;cursor=@($script:endCursor);operation_id=$b.operation_id;native_calls=$b.native_calls}}
    return $r
}
function New-EndHandoffFixture([ValidateSet('Move','BottomResize')][string]$Operation='Move'){
    $script:endRows=[Collections.Generic.List[object]]::new();$script:endClock=1000;$script:endGesture=0;$script:endSerial=1;$script:endLeft=$false;$script:endCursor=@(10,10);$script:endPreviousCursor=@(10,10);$script:endMovement=0;$script:endOperation=$Operation;$script:endOperationId=0;$script:endQuantum=0;$script:endWrites=0
    $script:endP=@(100,100,740,540);$script:endV=@(111,100,729,529);$script:endStartP=@($script:endP);$script:endStartV=@($script:endV)
    $null=Add-EndRow 'startup' @{evidence_kind='automated_owned_end_handoff';human_input=$false;real_explorer=$false;sendinput_in_probe=$true;mode='free_takeover';handoff_contract='end_barrier_v1';operation=$Operation;takeover_geometry_writes=0;foreground_contract='verified_global_foreground_v2';input_correlation='actual_absolute_receipt_v1';pid=100;ui_tid=101;qpc_frequency=10000000;synthetic_fixture=$true}
    $null=Add-EndRow 'desktop_gate' @{active_unlocked=$true;input_desktop_matches=$true}
    $null=Add-EndRow 'guard' @{hwnd=301;pid=100;tid=101}
    $null=Add-EndRow 'receiver' @{receiver_sequence=1;receiver_qpc=$script:endClock+99;receiver_hwnd=400;receiver_pid=200;receiver_tid=201;registration_verified=$true;usage_page=1;usage=2;registration_flags=256;keyboard_registered=$false}
    $null=Add-EndRow 'foreground_attempt' @{target=300;foreground=300;source_thread_local_active=300;source_thread_local_focus=300;set_foreground_success=$true;visible=$true}
    $o=End-Geometry $script:endP;$o.hwnd=300;$o.pid=100;$o.tid=101;$o.saved_cursor=@(10,10);$o.work_area=@(0,0,1920,1080);$o.virtual_screen=@(0,0,1920,1200);$o.dpi=96;$o.monitor=1
    $null=Add-EndRow 'owned' $o
    $null=Add-EndRow 'foreground_ready' @{target=300;source_pid=100;source_tid=101;own_identity=$true;desktop_ready=$true;visible=$true;foreground=300;foreground_pid=100;foreground_tid=101;foreground_snapshot_stable=$true;gui_query_succeeded=$true;gui_flags=0;capture_hwnd=0;menu_owner_hwnd=0;move_size_hwnd=0;buttons_modifiers_clear=$true;topmost_now=$false;source_thread_local_active=300;source_thread_local_focus=300;success=$true}
    $null=Add-EndRow 'foreground_bootstrap' @{target=300;set_foreground_attempted=$true;set_foreground_success=$true;activation_click_required=$false;temporary_topmost=$false;activation_point=$null;window_from_point_root_matches=$false;hit_test=$null;foreign_capture_clear=$false;sendinput_move_success=$false;sendinput_down_success=$false;sendinput_up_success=$false;wm_activate_seen=$false;wm_setfocus_seen=$false;activation_event_seen=$false;foreground=300;topmost_now=$false;final_foreground_matches=$true;topmost_restored=$true;result='PASS';reason='none'}
    $null=Add-EndRow 'raw_preflight_begin' @{point=@(300,300)}
    $null=Add-EndInput 1 @(300,300);$null=Add-EndRaw $true
    $null=Add-EndInput 2 @(0,0);$null=Add-EndRaw $false 1
    $null=Add-EndInput 1 @(312,300);$null=Add-EndRaw $true
    $up=Add-EndInput 4 @(0,0);$null=Add-EndRaw $false 2;Add-EndUpFence
    $null=Add-EndRow 'raw_preflight_outcome' @{attempted=$true;actual_move_verified=$true;actual_held_move_verified=$true;actual_up_verified=$true;input_delivery_verified=$true;observation_completed=$true;receiver_healthy=$true;up_wait_result=0;up_wait_started_qpc=$up.injection_return_qpc;up_wait_finished_qpc=$up.injection_return_qpc+1;up_timeout_ms=2000;raw_up_observed=$true}
    $null=Add-EndRow 'raw_preflight_complete' @{movement_count=2;up_count=1}
    $script:endGesture=if($Operation -eq 'Move'){1}else{2};$script:endPointer=@(300,$(if($Operation -eq 'Move'){109}else{539}));$pathEnd=@(($script:endPointer[0]+$(if($Operation -eq 'Move'){180}else{0})),($script:endPointer[1]+$(if($Operation -eq 'Move'){0}else{120})))
    $null=Add-EndInput 1 $script:endPointer;$null=Add-EndRaw $true
    $p=End-Geometry $script:endP;$p.hit_test=if($Operation -eq 'Move'){2}else{15};$p.start=@($script:endPointer);$p.end=$pathEnd;$p.samples=20;$p.cancel_after_sample=2;$p.interval_ms=30;$p.foreground=300
    $path=Add-EndRow 'path' $p
    $null=Add-EndInput 2 @(0,0);$null=Add-EndRaw $false 1
    $down=Add-EndRow 'native_button_down' @{target=300;hit_test=$path.hit_test;cursor=@($script:endPointer)}
    for($index=1;$index -le 20;++$index){
        $next=@(($script:endPointer[0]+($pathEnd[0]-$script:endPointer[0])*$index/20),($script:endPointer[1]+($pathEnd[1]-$script:endPointer[1])*$index/20))
        $null=Add-EndInput 1 $next ($index -gt 2) ($index -eq 1);$packet=Add-EndRaw $true
        if($index -eq 1){
            $ent=End-Geometry $script:endP;$ent.owner_capture=300;$ent.receipt_qpc=$script:endClock+99
            $enter=Add-EndRow 'ENTER' $ent
            $null=Add-EndRow 'intent_anchor' @{native_enter_sequence=$enter.sequence;native_enter_qpc=$enter.receipt_qpc;native_down_sequence=$down.sequence;pointer_down=@($script:endPointer);start_positioning=@($script:endStartP);start_visible=@($script:endStartV);operation=$Operation}
        }
        $desired=Get-EndHandoffIntended $Operation $script:endStartP $script:endStartV $script:endPointer $next
        if($index -le 2){
            $d=End-Geometry $script:endP;$d.event=if($Operation -eq 'Move'){'WM_MOVING'}else{'WM_SIZING'};$d.edge=if($Operation -eq 'Move'){9}else{6};$d.cursor=$next;$d.proposed=$desired.Positioning;$d.after_cancel=$false
            $null=Add-EndRow 'DRAG' $d
            $script:endP=@($desired.Positioning);$script:endV=@($desired.Visible)
            $change=End-Geometry $script:endP;$change.operation_id=0;$change.after_end=$false;$change.unexpected_change=$false
            $null=Add-EndRow 'POSITION_CHANGED' $change
        }else{$null=Add-EndWriter 'raw_movement' $packet}
        $sample=End-Geometry $script:endP;$sample.index=$index;$sample.cursor=$next;$sample.left_down=$true;$sample.post_cancel=$index -gt 2
        if($index -ge 3){$sample.intended_positioning=$desired.Positioning;$sample.intended_visible=$desired.Visible;$sample.exact=$true}
        $null=Add-EndRow 'sample' $sample
        if($index -eq 2){
            $issued=$script:endClock+10
            $null=Add-EndRow 'cancel_begin' @{target=300;source_tid=101;capture_before=300;left_down=$true}
            $msg=End-Geometry $script:endP;$msg.target=300;$msg.owner_capture=300;$msg.left_down=$true;$null=Add-EndRow 'cancel_message' $msg
            $changed=End-Geometry $script:endP;$changed.new_capture=0;$changed.owner_capture=0;$null=Add-EndRow 'CAPTURE_CHANGED' $changed
            $ret=End-Geometry $script:endP;$ret.transport_success=$true;$ret.error=0;$ret.recipient_result=0;$ret.issued_qpc=$issued;$ret.returned_qpc=$script:endClock+10;$ret.gui_query_succeeded=$true;$ret.capture_after=0;$ret.left_down=$true
            $null=Add-EndRow 'cancel_return' $ret
            $script:endP=@($script:endStartP);$script:endV=@($script:endStartV)
            $settled=End-Geometry $script:endP;$settled.operation_id=0;$settled.after_end=$false;$settled.unexpected_change=$false;$null=Add-EndRow 'POSITION_CHANGED' $settled
            $x=End-Geometry $script:endP;$x.owner_capture=0;$x.left_down=$true;$x.receipt_qpc=$script:endClock+99;$x.end_sequence=$script:endRows.Count+1
            $script:endExit=Add-EndRow 'EXIT' $x
            $h=End-Authority;$h.end_sequence=$script:endExit.sequence;$h.end_qpc=$script:endExit.receipt_qpc;$h.current_cursor=@($script:endCursor);$h.cursor_delta=$desired.Delta;$h.actual_handoff_positioning=@($script:endP);$h.actual_handoff_visible=@($script:endV);$h.intended_positioning=$desired.Positioning;$h.intended_visible=$desired.Visible;$h.raw_watermark=$script:endSerial
            $null=Add-EndRow 'handoff_begin' $h
            $null=Add-EndWriter 'handoff'
        }
    }
    $null=Add-EndInput 4 @(0,0) $true;$up=Add-EndRaw $false 2
    $endTarget=Get-EndHandoffIntended $Operation $script:endStartP $script:endStartV $script:endPointer $script:endCursor
    $t=End-Geometry $script:endP;$t.raw_up_receiver_sequence=$up.receiver_sequence;$t.raw_up_receiver_qpc=$up.receiver_qpc;$t.final_cursor=@($script:endCursor);$t.intended_positioning=$endTarget.Positioning;$t.intended_visible=$endTarget.Visible;$t.exact=$true;$t.pending_write=$false;$t.pending_motion=$false;$t.native_drag_after_end=0;$t.unowned_geometry_changes=0
    $proof=End-Authority;foreach($name in $proof.Keys){$t[$name]=$proof[$name]};$t.raw_up_seen=$true
    $null=Add-EndRow 'takeover_end' $t;Add-EndUpFence;$null=Add-EndRow 'path_complete' (End-Geometry $script:endP)
    $null=Add-EndInput 1 @(10,10) $true $false $true
    ++$script:endSerial
    $null=Add-EndRow 'receiver_shutdown' @{receiver_sequence=$script:endSerial;receiver_qpc=$script:endClock+99;receiver_hwnd=400;receiver_pid=200;receiver_tid=201;registration_removed=$true;window_destroyed=$true}
    $null=Add-EndRow 'shutdown' @{result='CAPTURED_NOT_ACCEPTED';cursor_restored=$true;owned_window_destroyed=$true;guard_window_destroyed=$true;receiver_stopped=$true;external_windows_touched=$false;takeover_geometry_writes=$script:endWrites}
    return ,@($script:endRows.ToArray())
}
$moveFixture=New-EndHandoffFixture Move
$mapped=Test-EndHandoffOwnedRecords $moveFixture -AllowSynthetic
Check-EndHandoff ($mapped.Result -ceq 'PASS' -and $mapped.Cancel -ceq 'PASS_WITH_TERMINAL_SETTLEMENT' -and $mapped.Handoff -ceq 'PASS' -and $mapped.Takeover -ceq 'PASS' -and $mapped.ContinuationQuanta -eq 18 -and $mapped.NativeWrites -eq 19 -and $mapped.TerminalSettlements -eq 1) 'A/E/G full synthetic Move wire lifecycle permits pre-END restoration then original intent continuation'
$resizeFixture=New-EndHandoffFixture BottomResize
$mapped=Test-EndHandoffOwnedRecords $resizeFixture -AllowSynthetic
Check-EndHandoff ($mapped.Result -ceq 'PASS' -and $mapped.Cancel -ceq 'PASS_WITH_TERMINAL_SETTLEMENT' -and $mapped.Takeover -ceq 'PASS' -and $mapped.FullGestureTargets) 'full synthetic BottomResize wire lifecycle retains original nonparticipating edges'
$caught=$false;try{$null=Test-EndHandoffOwnedRecords $moveFixture}catch{$caught=$true};Check-EndHandoff $caught 'synthetic fixture rejected as native acceptance by default'
function Renumber-EndFixture($Rows){for($i=0;$i -lt $Rows.Count;++$i){$Rows[$i].sequence=$i+1};return ,$Rows}
function Stop-EndFixtureAt($Rows,[long]$Sequence,[string]$Reason){
    $rows=@($Rows|Where-Object sequence -le $Sequence);$clock=$rows[-1].qpc;$gesture=$rows[-1].gesture
    $cursor=@(($rows|Where-Object {$_.type -eq 'input' -and $_.flags -eq 1}|Select-Object -Last 1).point)
    $serial=($rows|Where-Object type -eq raw_input|Select-Object -Last 1).receiver_sequence
    $writes=($rows|Where-Object type -eq writer_result|Measure-Object native_calls -Sum).Sum
    $extra=@(
        @{type='blocked';reason=$Reason},
        @{type='cleanup_fence';target=300;foreground=300;own_identity=$true;desktop_ready=$true;visible=$true;cursor=$cursor;left_down=$true;modifiers_clear=$true;capture_hwnd=0;root=300;menu_owner_hwnd=0;move_size_hwnd=0;gui_flags=0},
        @{type='cleanup_release';sent=1;error=0;input_tag=0x50424D41;injection_start_qpc=$clock+210;injection_return_qpc=$clock+220;target=300;cursor=$cursor;capture_hwnd=0;root=300},
        @{type='receiver_shutdown';receiver_sequence=$serial+1;receiver_qpc=$clock+399;receiver_hwnd=400;receiver_pid=200;receiver_tid=201;registration_removed=$true;window_destroyed=$true},
        @{type='shutdown';result='BLOCKED';cursor_restored=$false;owned_window_destroyed=$true;guard_window_destroyed=$true;receiver_stopped=$true;external_windows_touched=$false;takeover_geometry_writes=$writes}
    )
    foreach($part in $extra){$clock+=100;$row=[ordered]@{schema='r1c4b-takeover-owned/v2';sequence=0;gesture=$gesture;qpc=$clock};foreach($name in $part.Keys){$row[$name]=$part[$name]};$rows+=,[pscustomobject]$row}
    return ,(Renumber-EndFixture $rows)
}
$bad=Copy-EndFixture $moveFixture
$exit=$bad|Where-Object type -eq EXIT
$change=[pscustomobject]@{schema='r1c4b-takeover-owned/v2';sequence=0;gesture=1;qpc=$exit.qpc+1;type='POSITION_CHANGED';operation_id=0;after_end=$true;unexpected_change=$true;positioning=@(101,100,741,540);visible=@(112,100,730,529);positioning_error=0;visible_hresult=0}
$rows=[Collections.Generic.List[object]]::new();foreach($r in $bad){$rows.Add($r);if($r.type -eq 'EXIT'){$rows.Add($change)}}
$bad=Renumber-EndFixture @($rows.ToArray());($bad|Where-Object type -eq takeover_end).unowned_geometry_changes=1
$mapped=Test-EndHandoffOwnedRecords $bad -AllowSynthetic
Check-EndHandoff ($mapped.Result -ceq 'FAIL' -and $mapped.Architecture -ceq 'REJECTED_AT_END_BARRIER' -and $mapped.UnownedGeometryAfterEnd -eq 1) 'B actual unattributed terminal restoration after END is forbidden'
$bad=Copy-EndFixture $moveFixture
$drag=$bad|Where-Object {$_.type -eq 'sample' -and $_.index -eq 3};$drag.type='DRAG';$drag|Add-Member -NotePropertyName event -NotePropertyValue 'WM_MOVING';$drag|Add-Member -NotePropertyName edge -NotePropertyValue 9
($bad|Where-Object type -eq takeover_end).native_drag_after_end=1
$mapped=Test-EndHandoffOwnedRecords $bad -AllowSynthetic
Check-EndHandoff ($mapped.Result -ceq 'FAIL' -and $mapped.Architecture -ceq 'REJECTED_AT_END_BARRIER' -and $mapped.NativeDragAfterEnd -eq 1) 'F real native MOVING after END is a barrier counterexample'
$bad=Copy-EndFixture $moveFixture
$b=$bad|Where-Object {$_.type -eq 'writer_begin' -and $_.kind -eq 'handoff'};$r=$bad|Where-Object {$_.type -eq 'writer_result' -and $_.kind -eq 'handoff'};$h=$bad|Where-Object type -eq handoff_begin
# The wrong candidate loses all pre-cancel displacement and needs zero writes
# according to its own (incorrect) EXIT-based target. Independent math rejects it.
$h.intended_positioning=@(100,100,740,540);$h.intended_visible=@(111,100,729,529);$b.intended_positioning=@(100,100,740,540);$b.intended_visible=@(111,100,729,529);$b.native_calls=0
$r.native_calls=0;$r.native_start_qpc=0;$r.native_return_qpc=0;$r.positioning=@(100,100,740,540);$r.visible=@(111,100,729,529);$r.full_positioning=@(100,100,740,540);$r.full_visible=@(111,100,729,529);$r.positioning_exact=$true;$r.visible_exact=$true;$r.postverify_exact=$true;$r.diagnostic=End-Diagnostic $r.positioning $r.visible $b.intended_positioning $b.intended_visible
$bad=@($bad|Where-Object {-not ($_.type -eq 'POSITION_CHANGED' -and $_.operation_id -eq 1)})
$bad=Stop-EndFixtureAt $bad $r.sequence 'wrong_exit_intent_anchor'
$mapped=Test-EndHandoffOwnedRecords $bad -AllowSynthetic
Check-EndHandoff ($mapped.Result -ceq 'FAIL' -and $mapped.Architecture -ceq 'REJECTED_AT_TAKEOVER_WRITER' -and -not $mapped.FullGestureTargets -and $mapped.NativeWrites -eq 0) 'D lost pre-cancel delta is a writer failure, never compensated from EXIT intent'
$bad=Copy-EndFixture $moveFixture
$firstResult=$bad|Where-Object {$_.type -eq 'writer_result' -and $_.operation_id -eq 1}
$extraBegin=Copy-EndFixture ($bad|Where-Object {$_.type -eq 'writer_begin' -and $_.operation_id -eq 1});$extraResult=Copy-EndFixture $firstResult
foreach($row in $bad){if($row.type -cin @('writer_begin','writer_result','raw_quantum','POSITION_CHANGED') -and $null -ne (Get-AutoField $row 'operation_id') -and $row.operation_id -gt 1){$row.operation_id++}}
$extraBegin.operation_id=2;$extraBegin.qpc=$firstResult.qpc+1;$extraBegin.before_positioning=@($firstResult.positioning);$extraBegin.before_visible=@($firstResult.visible);$extraBegin.expected_before_positioning=@($firstResult.positioning);$extraBegin.expected_before_visible=@($firstResult.visible)
$extraResult.operation_id=2;$extraResult.qpc=$firstResult.qpc+4;$extraResult.native_start_qpc=$firstResult.qpc+2;$extraResult.native_return_qpc=$firstResult.qpc+3
$rows=[Collections.Generic.List[object]]::new();foreach($row in $bad){$rows.Add($row);if($row.sequence -eq $firstResult.sequence){$rows.Add($extraBegin);$rows.Add($extraResult)}}
$bad=Renumber-EndFixture @($rows.ToArray());($bad|Where-Object type -eq shutdown).takeover_geometry_writes++
$mapped=Test-EndHandoffOwnedRecords $bad -AllowSynthetic
Check-EndHandoff ($mapped.Result -ceq 'FAIL' -and $mapped.Architecture -ceq 'REJECTED_AT_TAKEOVER_WRITER' -and $mapped.Handoff -ceq 'FAIL' -and $mapped.HandoffNativeCalls -eq 2) 'H two reconciliation calls before any new Raw movement cannot be accepted'
foreach($kind in @('wrong-schema','legacy-contract','source-local-policy','wrong-receiver-pid','foreground-raw','wrong-input-tag','foreign-destination','wrong-start-anchor','wrong-down-anchor','foreign-authority','monitor-changed','raw-up-seen','stale-raw-quantum','duplicate-operation','normalized-receipt','forged-postverify','restore-before-end')){
    $bad=Copy-EndFixture $moveFixture
    switch($kind){
        'wrong-schema' {$bad[0].schema='r1c4b-takeover-owned/v1'}
        'legacy-contract' {$bad[0].handoff_contract='return_baseline_v1'}
        'source-local-policy' {$bad[0]|Add-Member -NotePropertyName local_activation_policy -NotePropertyValue 'own_background_reset_v1'}
        'wrong-receiver-pid' {($bad|Where-Object type -eq raw_input|Select-Object -First 1).receiver_pid=100}
        'foreground-raw' {($bad|Where-Object type -eq raw_input|Select-Object -First 1).input_code=0}
        'wrong-input-tag' {($bad|Where-Object type -eq input|Select-Object -First 1).input_tag=0}
        'foreign-destination' {($bad|Where-Object type -eq destination_fence|Select-Object -First 1).root=999}
        'wrong-start-anchor' {($bad|Where-Object type -eq intent_anchor).start_positioning[0]++}
        'wrong-down-anchor' {($bad|Where-Object type -eq intent_anchor).pointer_down[0]++}
        'foreign-authority' {($bad|Where-Object type -eq writer_begin|Select-Object -First 1).target=999}
        'monitor-changed' {($bad|Where-Object type -eq writer_begin|Select-Object -First 1).monitor=2}
        'raw-up-seen' {($bad|Where-Object type -eq writer_begin|Select-Object -First 1).raw_up_seen=$true}
        'stale-raw-quantum' {($bad|Where-Object {$_.type -eq 'writer_begin' -and $_.kind -eq 'raw_movement'}|Select-Object -First 1).raw_first_sequence=1}
        'duplicate-operation' {($bad|Where-Object {$_.type -eq 'writer_begin' -and $_.operation_id -eq 2}).operation_id=1}
        'normalized-receipt' {($bad|Where-Object type -eq input|Select-Object -First 1).normalized_dx++}
        'forged-postverify' {($bad|Where-Object type -eq writer_result|Select-Object -First 1).positioning[0]++}
        'restore-before-end' {($bad|Where-Object type -eq takeover_end).type='LEGACY_MOVE'}
    }
    $rejected=$false;try{$mapped=Test-EndHandoffOwnedRecords $bad -AllowSynthetic;$rejected=$mapped.Result -ceq 'FAIL'}catch{$rejected=$true}
    Check-EndHandoff $rejected "strict new mapper rejects $kind"
}
$early=@(
    [pscustomobject]@{schema='r1c4b-takeover-owned/v2';sequence=1;gesture=0;qpc=10;type='startup';evidence_kind='automated_owned_end_handoff';human_input=$false;real_explorer=$false;sendinput_in_probe=$true;mode='free_takeover';handoff_contract='end_barrier_v1';operation='Move';takeover_geometry_writes=0;foreground_contract='verified_global_foreground_v2';input_correlation='actual_absolute_receipt_v1';pid=100;ui_tid=101;qpc_frequency=10000000;synthetic_fixture=$true},
    [pscustomobject]@{schema='r1c4b-takeover-owned/v2';sequence=2;gesture=0;qpc=11;type='foreground_bootstrap';target=0;set_foreground_attempted=$false;set_foreground_success=$false;activation_click_required=$false;temporary_topmost=$false;activation_point=$null;window_from_point_root_matches=$false;hit_test=$null;foreign_capture_clear=$false;sendinput_move_success=$false;sendinput_down_success=$false;sendinput_up_success=$false;wm_activate_seen=$false;wm_setfocus_seen=$false;activation_event_seen=$false;foreground=500;topmost_now=$false;final_foreground_matches=$false;topmost_restored=$true;result='BLOCKED';reason='BLOCKED_BY_INTERACTIVE_DESKTOP'},
    [pscustomobject]@{schema='r1c4b-takeover-owned/v2';sequence=3;gesture=0;qpc=12;type='blocked';reason='BLOCKED_BY_INTERACTIVE_DESKTOP'},
    [pscustomobject]@{schema='r1c4b-takeover-owned/v2';sequence=4;gesture=0;qpc=13;type='shutdown';result='BLOCKED';cursor_restored=$false;owned_window_destroyed=$true;guard_window_destroyed=$true;receiver_stopped=$false;external_windows_touched=$false;takeover_geometry_writes=0}
)
$mapped=Test-EndHandoffOwnedRecords $early -AllowSynthetic
Check-EndHandoff ($mapped.Result -ceq 'BLOCKED' -and $mapped.Cancel -ceq 'UNKNOWN' -and $mapped.Handoff -ceq 'NOT_RUN' -and $mapped.Takeover -ceq 'NOT_RUN' -and $mapped.Architecture -ceq 'UNRESOLVED' -and $mapped.NativeWrites -eq 0) 'legal desktop-blocked prefix cannot become cancellation failure or empirical PASS'
Write-Host "end-handoff synthetic_only=true checks=$checks PASS"
