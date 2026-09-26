Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-end-diagnostics-validation.ps1')
$checks=0
function Check-EndDiagnostic([bool]$Value,[string]$Reason){if(-not $Value){throw "end diagnostic fixture: $Reason"};$script:checks++}
function Copy-EndDiagnostic($Value){return ($Value|ConvertTo-Json -Depth 20 -Compress|ConvertFrom-Json)}
function New-EndDiagnosticProof {
    return [pscustomobject]@{end_observed=$true;winevent_end_observed=$true;own_identity=$true;desktop_ready=$true;source_visible=$true;foreground_matches=$true;gui_query_succeeded=$true;capture_clear=$true;menu_clear=$true;move_size_clear=$true;gui_in_movesize_clear=$true;cursor_success=$true;cursor_root_owned_or_guard=$true;left_down=$true;raw_up_seen=$false;receiver_healthy=$true;foreign_capture_transferred=$false;dpi_matches=$true;monitor_matches=$true;actual_positioning_available=$true;actual_visible_available=$true;terminal_positioning_available=$true;terminal_visible_available=$true;terminal_positioning_exact=$true;terminal_visible_exact=$true;native_drag_after_cancel_return=0;native_drag_after_end=0;native_drag_after_winevent_end=0;unowned_geometry_changes=0;takeover_healthy=$true;buttons_modifiers_clear=$true}
}
$proof=New-EndDiagnosticProof
Check-EndDiagnostic (@(Get-EndDiagnosticFailureChecks $proof).Count -eq 0) 'F all exact independent handoff facts pass'
foreach($case in @(
    @('end_observed','MissingNativeEnd'),@('winevent_end_observed','MissingWinEventEnd'),@('own_identity','IdentityChanged'),@('desktop_ready','DesktopUnavailable'),@('source_visible','SourceNotVisible'),@('foreground_matches','ForegroundChanged'),@('gui_query_succeeded','GuiQueryFailed'),@('capture_clear','CaptureStillOwned'),@('menu_clear','MenuStillActive'),@('move_size_clear','MoveSizeStillActive'),@('gui_in_movesize_clear','MoveSizeStillActive'),@('cursor_success','CursorUnavailable'),@('cursor_root_owned_or_guard','CursorOutsideOwnedInputGuard'),@('left_down','ButtonReleasedBeforeHandoff'),@('receiver_healthy','ReceiverUnhealthy'),@('dpi_matches','DpiChanged'),@('monitor_matches','MonitorChanged'),@('actual_positioning_available','ActualPositioningUnavailable'),@('actual_visible_available','ActualVisibleUnavailable'),@('terminal_positioning_available','TerminalPositioningUnavailable'),@('terminal_visible_available','TerminalVisibleUnavailable'),@('terminal_positioning_exact','TerminalPositioningMismatch'),@('terminal_visible_exact','TerminalVisibleMismatch'),@('takeover_healthy','TakeoverAlreadyUnhealthy'),@('buttons_modifiers_clear','InputInterference')
)){
    $bad=Copy-EndDiagnostic $proof;$bad.($case[0])=$false;$failed=@(Get-EndDiagnosticFailureChecks $bad)
    Check-EndDiagnostic ($failed.Count -eq 1 -and $failed[0] -ceq $case[1]) "A compound proof independently isolates $($case[1])"
}
$bad=Copy-EndDiagnostic $proof;$bad.cursor_root_owned_or_guard=$false
Check-EndDiagnostic ((Get-EndDiagnosticDisposition (Get-EndDiagnosticFailureChecks $bad) $true) -ceq 'BLOCKED') 'H outside guard alone is an authority blocker, not geometry rejection'
$bad=Copy-EndDiagnostic $proof;$bad.terminal_visible_exact=$false
Check-EndDiagnostic ((Get-EndDiagnosticDisposition (Get-EndDiagnosticFailureChecks $bad) $true) -ceq 'FAIL') 'I true positioning exact plus post-barrier visible mismatch is a counterexample'
$bad.terminal_positioning_exact=$false
Check-EndDiagnostic (@(Get-EndDiagnosticFailureChecks $bad)[0] -ceq 'BothTerminalGeometryMismatch') 'J both geometry mismatches have precise combined classification'
$bad=Copy-EndDiagnostic $proof;$bad.actual_positioning_available=$false;$bad.terminal_positioning_exact=$false
Check-EndDiagnostic ((Get-EndDiagnosticFailureChecks $bad) -join ',' -ceq 'ActualPositioningUnavailable') 'missing actual capture is not a geometry-mismatch counterexample'
$bad=Copy-EndDiagnostic $proof;$bad.native_drag_after_end=1;$bad.takeover_healthy=$false
Check-EndDiagnostic (((Get-EndDiagnosticFailureChecks $bad) -join ',') -ceq 'NativeDragAfterEnd,TakeoverAlreadyUnhealthy') 'K actual drag cause is not hidden by aggregate unhealthy'
Check-EndDiagnostic (Test-EndDiagnosticTickFresh 2 4294967294) 'bounded DWORD wrap preserves a fresh event'
Check-EndDiagnostic (-not (Test-EndDiagnosticTickFresh 4294967294 2)) 'late callback cannot relabel an old generated event as fresh'
# Reuse only an explicit list of pure in-memory synthetic generators from the
# frozen Fix B test source. Its tests/top-level workflow are not executed.
$tokens=$null;$parseErrors=$null
$fixtureAst=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'test-r1c4b-end-handoff.ps1'),[ref]$tokens,[ref]$parseErrors)
Check-EndDiagnostic ($parseErrors.Count -eq 0) 'frozen B fixture source parses'
$helpers=@('Add-EndRow','End-Geometry','End-Authority','End-Diagnostic','Add-EndInput','Add-EndRaw','Add-EndUpFence','Add-EndWriter','New-EndHandoffFixture','Renumber-EndFixture','Stop-EndFixtureAt')
foreach($name in $helpers){$function=$fixtureAst.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name},$true);if($null -eq $function){throw "missing pure fixture helper $name"};Invoke-Expression $function.Extent.Text}
function Complete-CProof($CursorPoint,$Native,$Source,$Guard,$Win=$null){
    $d=New-EndDiagnosticProof
    foreach($name in @('target','guard','source_pid','source_tid','gesture_id','actual_source_pid','actual_source_tid','end_sequence','end_qpc','winevent_end_sequence','winevent_end_qpc','foreground_hwnd','foreground_pid','foreground_tid','capture_hwnd','menu_owner_hwnd','move_size_hwnd','gui_flags','gui_error','cursor_root','cursor_error','dpi','frozen_dpi','monitor','frozen_monitor','positioning_error','visible_hresult')){$d|Add-Member -NotePropertyName $name -NotePropertyValue 0}
    $d.target=$Source.hwnd;$d.guard=$Guard.hwnd;$d.source_pid=$Source.pid;$d.source_tid=$Source.tid;$d.gesture_id=1;$d.actual_source_pid=$Source.pid;$d.actual_source_tid=$Source.tid;$d.foreground_hwnd=$Source.hwnd;$d.foreground_pid=$Source.pid;$d.foreground_tid=$Source.tid;$d.cursor_root=$Source.hwnd;$d.dpi=$Source.dpi;$d.frozen_dpi=$Source.dpi;$d.monitor=$Source.monitor;$d.frozen_monitor=$Source.monitor;$d.end_sequence=$Native.sequence;$d.end_qpc=$Native.receipt_qpc
    $d.winevent_end_observed=$null -ne $Win;if($null -ne $Win){$d.winevent_end_sequence=$Win.sequence;$d.winevent_end_qpc=$Win.callback_qpc}
    foreach($pair in @(@('foreground_snapshot_stable',$true),@('winevent_healthy',$true),@('log_healthy',$true),@('takeover_scope',$true),@('handoff_ready',$false),@('cleanup_observation',$false),@('cursor',$CursorPoint),@('terminal_positioning',@($Native.positioning)),@('terminal_visible',@($Native.visible)),@('actual_positioning',@($Native.positioning)),@('actual_visible',@($Native.visible)))){$d|Add-Member -NotePropertyName $pair[0] -NotePropertyValue $pair[1]}
    $failed=@(Get-EndDiagnosticFailureChecks $d);$d|Add-Member -NotePropertyName failed_checks -NotePropertyValue $failed;$d|Add-Member -NotePropertyName failure_class -NotePropertyValue $(if($failed.Count){$failed[0]}else{'None'});$d|Add-Member -NotePropertyName write_authorized -NotePropertyValue ($null -ne $Win -and $failed.Count -eq 0)
    return $d
}
function Add-CFixtureFields([string]$Type,[long]$Qpc,$Fields){
    $r=[ordered]@{schema='r1c4b-takeover-owned/v3';sequence=0;gesture=$script:cGesture;qpc=$Qpc;type=$Type};foreach($name in $Fields.PSObject.Properties.Name){$r[$name]=$Fields.$name};return [pscustomobject]$r
}
function New-CFixture([ValidateSet('Move','BottomResize')][string]$Operation='BottomResize'){
    $base=Copy-EndDiagnostic (New-EndHandoffFixture $Operation);$source=$base|Where-Object type -eq owned;$guard=$base|Where-Object type -eq guard;$path=$base|Where-Object type -eq path;$exit=$base|Where-Object type -eq EXIT;$enter=$base|Where-Object type -eq ENTER;$down=$base|Where-Object type -eq native_button_down;$handoff=$base|Where-Object type -eq handoff_begin
    $script:cGesture=if($Operation -eq 'Move'){1}else{2};$armQpc=$path.qpc+1;$armTick=1000
    $base[0].handoff_contract='winevent_end_barrier_v1';$base[0]|Add-Member -NotePropertyName diagnostic_contract -NotePropertyValue 'structured_handoff_v1';$base[0]|Add-Member -NotePropertyName gesture_id -NotePropertyValue 1
    $installed=Add-CFixtureFields 'winevent_hook_installed' ($source.qpc+1) ([pscustomobject]@{hook=700;source_hwnd=300;source_pid=100;source_tid=101;install_tid=101;event_min=10;event_max=11;flags=0;dll=0;skip_own_process=$false;skip_own_thread=$false;install_success=$true;error=0})
    $armed=Add-CFixtureFields 'gesture_armed' ($path.qpc+2) ([pscustomobject]@{gesture_id=1;arm_qpc=$armQpc;arm_tick=$armTick;source_hwnd=300;source_pid=100;source_tid=101})
    $start=Add-CFixtureFields 'winevent_callback' ($enter.qpc+1) ([pscustomobject]@{hook=700;event=10;hwnd=300;event_thread=101;event_time=1001;object_id=0;child_id=0;callback_tid=101;callback_sequence=1;callback_qpc=$enter.qpc+1;gesture_id=1;arm_qpc=$armQpc;arm_tick=$armTick})
    $startMatch=Add-CFixtureFields 'winevent_match' ($enter.qpc+2) ([pscustomobject]@{callback_sequence=1;callback_record_sequence=0;gesture_id=1;source_identity=$true;actual_source_pid=100;actual_source_tid=101;accepted=$true;reason='None';matched_start_callback_sequence=1})
    $nativeProof=Add-CFixtureFields 'handoff_preflight' ($exit.qpc+1) (Complete-CProof $handoff.current_cursor $exit $source $guard);$nativeProof|Add-Member -NotePropertyName phase -NotePropertyValue 'native_end'
    $finish=Add-CFixtureFields 'winevent_callback' ($exit.qpc+3) ([pscustomobject]@{hook=700;event=11;hwnd=300;event_thread=101;event_time=1002;object_id=0;child_id=0;callback_tid=101;callback_sequence=2;callback_qpc=$exit.qpc+2;gesture_id=1;arm_qpc=$armQpc;arm_tick=$armTick})
    $endMatch=Add-CFixtureFields 'winevent_match' ($exit.qpc+4) ([pscustomobject]@{callback_sequence=2;callback_record_sequence=0;gesture_id=1;source_identity=$true;actual_source_pid=100;actual_source_tid=101;accepted=$true;reason='None';matched_start_callback_sequence=1})
    $finalProof=Add-CFixtureFields 'handoff_preflight' ($exit.qpc+5) (Complete-CProof $handoff.current_cursor $exit $source $guard $finish);$finalProof|Add-Member -NotePropertyName phase -NotePropertyValue 'winevent_end'
    $receiverEnd=$base|Where-Object type -eq receiver_shutdown
    $remove=Add-CFixtureFields 'winevent_hook_removed' ($receiverEnd.qpc-1) ([pscustomobject]@{hook=700;source_hwnd=300;source_pid=100;source_tid=101;remove_tid=101;remove_success=$true;error=0})
    $rows=[Collections.Generic.List[object]]::new();$map=@{}
    foreach($r in $base){
        $original=$r.sequence;$r.schema='r1c4b-takeover-owned/v3'
        if($r.type -eq 'receiver_shutdown'){$rows.Add($remove)}
        $rows.Add($r);$map[[string]$original]=$r
        if($r.type -eq 'owned'){$rows.Add($installed)}
        if($r.type -eq 'path'){$rows.Add($armed)}
        if($r.type -eq 'ENTER'){$rows.Add($start);$rows.Add($startMatch)}
        if($r.type -eq 'EXIT'){$rows.Add($nativeProof);$rows.Add($finish);$rows.Add($endMatch);$rows.Add($finalProof)}
        if($r.type -eq 'DRAG'){$r|Add-Member -NotePropertyName receipt_qpc -NotePropertyValue ($r.qpc-1)}
        if($r.type -eq 'POSITION_CHANGED'){$r|Add-Member -NotePropertyName position_receipt_qpc -NotePropertyValue ($r.qpc-1);$r|Add-Member -NotePropertyName winevent_audit_active -NotePropertyValue $r.after_end;$r|Add-Member -NotePropertyName cleanup_observation -NotePropertyValue $false}
        if($r.type -eq 'raw_input'){$r|Add-Member -NotePropertyName raw_scope -NotePropertyValue 'gesture';$r|Add-Member -NotePropertyName acceptance_eligible -NotePropertyValue ($r.sequence -gt $path.sequence);$r|Add-Member -NotePropertyName cleanup_observation -NotePropertyValue $false;$r|Add-Member -NotePropertyName gesture_id -NotePropertyValue $(if($r.sequence -gt $path.sequence){1}else{0})}
    }
    $rows=Renumber-EndFixture @($rows.ToArray())
    foreach($r in $base){foreach($name in @('native_enter_sequence','native_down_sequence','end_sequence')){$value=Get-AutoField $r $name;if($null -ne $value){$r.$name=$map[[string]$value].sequence}}}
    $startMatch.callback_record_sequence=$start.sequence;$endMatch.callback_record_sequence=$finish.sequence
    $nativeProof.end_sequence=$exit.sequence;$finalProof.end_sequence=$exit.sequence;$finalProof.winevent_end_sequence=$finish.sequence
    foreach($r in $rows|Where-Object type -eq writer_begin){$r|Add-Member -NotePropertyName winevent_end_qpc -NotePropertyValue $finish.callback_qpc;$r|Add-Member -NotePropertyName winevent_end_sequence -NotePropertyValue $finish.sequence;$r|Add-Member -NotePropertyName handoff_preflight_sequence -NotePropertyValue $finalProof.sequence}
    return ,$rows
}
$resize=New-CFixture BottomResize
# These auxiliary records mirror the frozen native formatter and are audited
# separately from the immutable B shared-primitive view.
$exit=$resize|Where-Object type -eq EXIT;$end=$resize|Where-Object {$_.type -eq 'winevent_callback' -and $_.event -eq 11};$d=$resize|Where-Object {$_.type -eq 'handoff_preflight' -and $_.phase -eq 'winevent_end'}
$snapshot=Add-CFixtureFields 'winevent_barrier_snapshot' ($d.qpc-1) ([pscustomobject]@{winevent_end_sequence=$end.sequence;winevent_end_qpc=$end.callback_qpc;native_drag_after_winevent_end=0;unowned_geometry_changes=0;positioning=@($exit.positioning);visible=@($exit.visible);positioning_error=0;visible_hresult=0})
$wait=Add-CFixtureFields 'winevent_end_wait' $d.qpc ([pscustomobject]@{started_qpc=$exit.qpc;finished_qpc=$d.qpc;timeout_ms=3000;wait_result=0;winevent_end_observed=$true;winevent_healthy=$true})
$withAux=@();foreach($row in $resize){if($row.sequence -eq $d.sequence){$withAux+=,$snapshot;$withAux+=,$wait};$withAux+=,$row}
# The references are repaired with the same explicit original-to-view mapping
# used by the fixture generator below; no callback timestamp is substituted.
$oldMap=@{};for($i=0;$i -lt $withAux.Count;++$i){if($withAux[$i].sequence -gt 0){$oldMap[[string]$withAux[$i].sequence]=$i+1};$withAux[$i].sequence=$i+1}
foreach($row in $withAux){foreach($name in @('native_enter_sequence','native_down_sequence','end_sequence','winevent_end_sequence','handoff_preflight_sequence','callback_record_sequence')){$value=Get-AutoField $row $name;if($null -ne $value -and $value -gt 0){$row.$name=$oldMap[[string]$value]}}};$resize=$withAux
$mapped=Test-EndDiagnosticsOwnedRecords $resize -AllowSynthetic
Check-EndDiagnostic ($mapped.Result -ceq 'PASS' -and $mapped.FinalFailureClass -ceq 'None' -and $mapped.WinEventBarrier -ceq 'PASS' -and $mapped.FirstNativeStartQpc -gt $mapped.MatchedEndCallbackQpc -and $mapped.ContinuationQuanta -eq 18) 'F/N full v3 BottomResize wire proves matching callback barrier and original anchor intent'
$move=New-CFixture Move
$mapped=Test-EndDiagnosticsOwnedRecords $move -AllowSynthetic
Check-EndDiagnostic ($mapped.Result -ceq 'PASS' -and $mapped.Takeover -ceq 'PASS') 'v3 Move regression complete wire, not real Windows observation'
$caught=$false;try{$null=Test-EndDiagnosticsOwnedRecords $move}catch{$caught=$true};Check-EndDiagnostic $caught 'default rejects synthetic native acceptance'
function Refresh-CClassification($Proof){
    $failed=@(Get-EndDiagnosticFailureChecks $Proof);$Proof.failed_checks=$failed;$Proof.failure_class=if($failed.Count){$failed[0]}else{'None'}
    $Proof.write_authorized=$Proof.phase -ceq 'winevent_end' -and $failed.Count -eq 0 -and $Proof.takeover_scope -and -not $Proof.handoff_ready -and -not $Proof.cleanup_observation
}
function Renumber-CFixture($Rows){
    $map=@{};for($i=0;$i -lt $Rows.Count;++$i){if($Rows[$i].sequence -gt 0){$map[[string]$Rows[$i].sequence]=$i+1};$Rows[$i].sequence=$i+1}
    foreach($r in $Rows){foreach($field in @('native_enter_sequence','native_down_sequence','end_sequence','winevent_end_sequence','handoff_preflight_sequence','callback_record_sequence','preflight_sequence')){$v=Get-AutoField $r $field;if($null -ne $v -and $v -gt 0 -and $map.ContainsKey([string]$v)){$r.$field=$map[[string]$v]}}}
    return ,$Rows
}
function Stop-CFixture($Rows,[long]$At,[string]$Reason){
    $prefix=@($Rows|Where-Object sequence -le $At);$clock=$prefix[-1].qpc;$script:cGesture=$prefix[-1].gesture
    $serial=($prefix|Where-Object type -eq raw_input|Select-Object -Last 1).receiver_sequence
    $measured=@($prefix|Where-Object type -eq writer_result|Measure-Object native_calls -Sum);$writes=if($measured.Count){$measured[0].Sum}else{0}
    $remove=Copy-EndDiagnostic ($Rows|Where-Object type -eq winevent_hook_removed);$remove.sequence=0;$remove.qpc=$clock+20
    $prefix+=,(Add-CFixtureFields 'blocked' ($clock+10) ([pscustomobject]@{reason=$Reason}))
    $prefix+=,$remove
    $prefix+=,(Add-CFixtureFields 'receiver_shutdown' ($clock+30) ([pscustomobject]@{receiver_sequence=$serial+1;receiver_qpc=$clock+29;receiver_hwnd=400;receiver_pid=200;receiver_tid=201;registration_removed=$true;window_destroyed=$true}))
    $prefix+=,(Add-CFixtureFields 'shutdown' ($clock+40) ([pscustomobject]@{result='BLOCKED';cursor_restored=$false;owned_window_destroyed=$true;guard_window_destroyed=$true;receiver_stopped=$true;external_windows_touched=$false;takeover_geometry_writes=[long]$writes}))
    return ,(Renumber-CFixture $prefix)
}
function Reject-CFixture($Rows,[string]$Reason){
    $rejected=$false;try{$r=Test-EndDiagnosticsOwnedRecords $Rows -AllowSynthetic;$rejected=$r.Result -cne 'PASS'}catch{$rejected=$true};Check-EndDiagnostic $rejected $Reason
}
function Get-CWin($Rows){return Test-EndDiagnosticWinEvents $Rows @(Get-EndDiagnosticRows $Rows 'owned') $Rows[0] ($Rows[-1].result -ceq 'BLOCKED')}
function Get-CPreflight($Rows){return Test-EndDiagnosticPreflights $Rows @(Get-EndDiagnosticRows $Rows 'owned') @(Get-EndDiagnosticRows $Rows 'guard') $Rows[0] (Get-CWin $Rows)}

# A: the diagnostic is reconstructed from all facts, including a compound
# failure; assigning only the native failure_class can never change acceptance.
$bad=New-CFixture;$d=$bad|Where-Object {$_.type -eq 'handoff_preflight' -and $_.phase -eq 'winevent_end'}
$d.cursor_root=999;$d.cursor_root_owned_or_guard=$false;$d.actual_visible=@(111,100,729,530);$d.terminal_visible_exact=$false;Refresh-CClassification $d
$bad=Stop-CFixture $bad $d.sequence 'compound_diagnostic'
$r=Test-EndDiagnosticsOwnedRecords $bad -AllowSynthetic
Check-EndDiagnostic ($r.FinalFailedChecks -join ',' -ceq 'CursorOutsideOwnedInputGuard,TerminalVisibleMismatch' -and $r.Result -ceq 'FAIL' -and -not $r.UniqueGuardFailure) 'A full wire splits compound authority and factual visible failure'
($bad|Where-Object {$_.type -eq 'handoff_preflight' -and $_.phase -eq 'winevent_end'}).failure_class='None';Reject-CFixture $bad 'A forged aggregate reason cannot conceal independent checks'

# B/C/D and late END: assert the exact rejection reason from raw callback facts.
foreach($case in @(@('hwnd',999,'WrongWindow'),@('event_thread',999,'WrongEventThread'),@('callback_tid',999,'WrongCallbackThread'),@('hook',999,'WrongHook'),@('event_time',999,'EventBeforeGesture'),@('gesture_id',2,'WrongGesture'))){
    $bad=New-CFixture;$cb=$bad|Where-Object {$_.type -eq 'winevent_callback' -and $_.event -eq 11};$m=$bad|Where-Object {$_.type -eq 'winevent_match' -and $_.callback_sequence -eq 2}
    $cb.($case[0])=$case[1];$m.accepted=$false;$m.reason=$case[2]
    $win=Get-CWin $bad;Check-EndDiagnostic ($null -eq $win.End -and $win.Facts[-1].Accepted -eq $false) "B/C late/wrong callback rejected as $($case[2])"
    $m.accepted=$true;Reject-CFixture $bad "B/C native claims cannot promote $($case[2])"
}
$bad=New-CFixture;$start=$bad|Where-Object {$_.type -eq 'winevent_callback' -and $_.event -eq 10};$start.event=11
($bad|Where-Object {$_.type -eq 'winevent_match' -and $_.callback_sequence -eq 1}).accepted=$false
($bad|Where-Object {$_.type -eq 'winevent_match' -and $_.callback_sequence -eq 1}).reason='EndWithoutStart'
($bad|Where-Object {$_.type -eq 'winevent_match' -and $_.callback_sequence -eq 1}).matched_start_callback_sequence=0
($bad|Where-Object {$_.type -eq 'winevent_match' -and $_.callback_sequence -eq 2}).accepted=$false
($bad|Where-Object {$_.type -eq 'winevent_match' -and $_.callback_sequence -eq 2}).reason='EndWithoutStart'
($bad|Where-Object {$_.type -eq 'winevent_match' -and $_.callback_sequence -eq 2}).matched_start_callback_sequence=0
Check-EndDiagnostic ($null -eq (Get-CWin $bad).End) 'D END without the real matched START cannot authorize'

# E: a complete native EXIT is only the initial observation, not public END.
$bad=New-CFixture;$d=$bad|Where-Object {$_.type -eq 'handoff_preflight' -and $_.phase -eq 'native_end'};$bad=Stop-CFixture $bad $d.sequence 'missing_public_end'
$r=Test-EndDiagnosticsOwnedRecords $bad -AllowSynthetic
Check-EndDiagnostic ($r.Result -ceq 'BLOCKED' -and $r.Architecture -ceq 'UNRESOLVED' -and $r.WinEventBarrier -ceq 'FAIL' -and $r.NativeWrites -eq 0 -and $r.InitialFailureClass -ceq 'MissingWinEventEnd') 'E native-only END never becomes cancellation failure or takeover PASS'

foreach($case in @('movesize','guard','visible','positioning','actual-p-unavailable','actual-v-unavailable','terminal-p-unavailable','terminal-v-unavailable')){
    $bad=New-CFixture;$d=$bad|Where-Object {$_.type -eq 'handoff_preflight' -and $_.phase -eq 'winevent_end'}
    switch($case){
        'movesize'{$d.move_size_hwnd=300;$d.gui_flags=2;$d.move_size_clear=$false;$d.gui_in_movesize_clear=$false}
        'guard'{$d.cursor_root=999;$d.cursor_root_owned_or_guard=$false}
        'visible'{$d.actual_visible[3]++;$d.terminal_visible_exact=$false}
        'positioning'{$d.actual_positioning[0]++;$d.terminal_positioning_exact=$false}
        'actual-p-unavailable'{$d.actual_positioning=$null;$d.actual_positioning_available=$false;$d.terminal_positioning_exact=$false;$d.positioning_error=5}
        'actual-v-unavailable'{$d.actual_visible=$null;$d.actual_visible_available=$false;$d.terminal_visible_exact=$false;$d.visible_hresult=-1}
        'terminal-p-unavailable'{($bad|Where-Object type -eq EXIT).positioning=$null;foreach($p in $bad|Where-Object type -eq handoff_preflight){$p.terminal_positioning=$null;$p.terminal_positioning_available=$false;$p.terminal_positioning_exact=$false;Refresh-CClassification $p}}
        'terminal-v-unavailable'{($bad|Where-Object type -eq EXIT).visible=$null;foreach($p in $bad|Where-Object type -eq handoff_preflight){$p.terminal_visible=$null;$p.terminal_visible_available=$false;$p.terminal_visible_exact=$false;Refresh-CClassification $p}}
    }
    Refresh-CClassification $d;$bad=Stop-CFixture $bad $d.sequence $case;$r=Test-EndDiagnosticsOwnedRecords $bad -AllowSynthetic
    $geometry=$case -in @('visible','positioning')
    Check-EndDiagnostic ($r.Result -ceq $(if($geometry){'FAIL'}else{'BLOCKED'}) -and $r.Architecture -ceq $(if($geometry){'REJECTED_AT_END_BARRIER'}else{'UNRESOLVED'})) "G/H/I/J capture or authority distinction $case"
    if($case -eq 'guard'){Check-EndDiagnostic $r.UniqueGuardFailure 'H guard correction permitted only for the unique guard failure'}
}

# K: use an actual DRAG receipt, not the summary counter alone.
$bad=New-CFixture;$d=$bad|Where-Object {$_.type -eq 'handoff_preflight' -and $_.phase -eq 'winevent_end'}
$g=Copy-EndDiagnostic ($bad|Where-Object type -eq DRAG|Select-Object -Last 1);$g.sequence=0;$g.qpc=$d.qpc-1;$g.receipt_qpc=$d.qpc-2
$insert=@();foreach($row in $bad){if($row.sequence -eq $d.sequence){$insert+=,$g};$insert+=,$row};$bad=Renumber-CFixture $insert
$d.native_drag_after_cancel_return=1;$d.native_drag_after_end=1;$d.native_drag_after_winevent_end=1;$d.takeover_healthy=$false;Refresh-CClassification $d
$bad=Stop-CFixture $bad $d.sequence 'post_public_end_native_drag';$r=Test-EndDiagnosticsOwnedRecords $bad -AllowSynthetic
Check-EndDiagnostic ($r.Result -ceq 'FAIL' -and $r.NativeDragAfterWinEventEnd -eq 1 -and $r.FinalFailedChecks -contains 'NativeDragAfterEnd') 'K actual DRAG after matched public END is not aggregate-only evidence'

# L: native EXIT is deliberately earlier than public END; a write between the
# two cannot be made legal merely by the later log row order.
$bad=New-CFixture;$first=$bad|Where-Object {$_.type -eq 'writer_result' -and $_.operation_id -eq 1};$end=$bad|Where-Object {$_.type -eq 'winevent_callback' -and $_.event -eq 11}
$first.native_start_qpc=$end.callback_qpc;$first.native_return_qpc=$end.callback_qpc+1
Reject-CFixture $bad 'L first native start must be strictly after matching callback QPC; impossible native clock evidence is invalid, never PASS'

# M/N/O retain the original START frame bridge and full pointer-down delta.
$bad=New-CFixture;$sample=$bad|Where-Object {$_.type -eq 'sample' -and $_.index -eq 3};$sample.positioning[0]++;Reject-CFixture $bad 'M BottomResize cannot alter a frozen non-bottom edge'
$bad=New-CFixture;$h=$bad|Where-Object type -eq handoff_begin;$h.intended_positioning=@($h.actual_handoff_positioning);$h.intended_visible=@($h.actual_handoff_visible)
Reject-CFixture $bad 'O actual handoff/EXIT baseline cannot replace original intent anchor'
$bad=New-CFixture;($bad|Where-Object type -eq intent_anchor).pointer_down[1]++;Reject-CFixture $bad 'N raw/current targets bind the real native DOWN, not a new post-END anchor'

foreach($case in @('hook-leak','hook-thread','hook-flags','raw-cleanup-up','raw-retired-motion','native-phase-authorized','forged-counters','forged-checks')){
    $bad=New-CFixture
    switch($case){
        'hook-leak'{$bad=@($bad|Where-Object type -ne winevent_hook_removed);$bad=Renumber-CFixture $bad}
        'hook-thread'{($bad|Where-Object type -eq winevent_hook_removed).remove_tid=999}
        'hook-flags'{($bad|Where-Object type -eq winevent_hook_installed).flags=2}
        'raw-cleanup-up'{$t=$bad|Where-Object type -eq takeover_end;$p=$bad|Where-Object {$_.type -eq 'raw_input' -and $_.receiver_sequence -eq $t.raw_up_receiver_sequence};$p.raw_scope='cleanup';$p.cleanup_observation=$true;$p.acceptance_eligible=$false}
        'raw-retired-motion'{$q=$bad|Where-Object type -eq raw_quantum|Select-Object -First 1;($bad|Where-Object {$_.type -eq 'raw_input' -and $_.receiver_sequence -eq $q.raw_last_sequence}).acceptance_eligible=$false}
        'native-phase-authorized'{($bad|Where-Object {$_.type -eq 'handoff_preflight' -and $_.phase -eq 'native_end'}).write_authorized=$true}
        'forged-counters'{($bad|Where-Object {$_.type -eq 'handoff_preflight' -and $_.phase -eq 'winevent_end'}).native_drag_after_winevent_end=1}
        'forged-checks'{($bad|Where-Object {$_.type -eq 'handoff_preflight' -and $_.phase -eq 'winevent_end'}).failed_checks=@('MissingWinEventEnd')}
    }
    Reject-CFixture $bad "strict hook/scope/classifier regression $case"
}

# P and cleanup matrix: all rows are real-format synthetic receipts. Cleanup
# preserves receiver serials but never supplies accepted gesture END.
function New-CCleanupProof([bool]$Held,[bool]$OwnedDown=$true){
    return [pscustomobject]@{target=300;guard=301;source_pid=100;source_tid=101;actual_source_pid=100;actual_source_tid=101;test_down_owned=$OwnedDown;own_identity=$true;desktop_ready=$true;source_visible=$true;foreground_hwnd=300;foreground_matches=$true;foreground_pid=100;foreground_tid=101;gui_query_succeeded=$true;gui_error=0;capture_hwnd=0;no_foreign_capture=$true;menu_owner_hwnd=0;menu_clear=$true;move_size_hwnd=0;move_size_clear=$true;gui_flags=0;gui_mode_clear=$true;cursor_success=$true;cursor_error=0;cursor=@(300,551);cursor_root=300;cursor_root_owned_or_guard=$true;left_down=$Held;buttons_modifiers_clear=$true;foreign_capture_transferred=$false;acceptance_eligible=$false}
}
function Add-CCleanup($Rows,[ValidateSet('PASS','SKIPPED','NOT_NEEDED','FAILED')][string]$Mode='PASS',[string]$FinalFailure=''){
    $body=@($Rows|Where-Object {$_.type -notin @('winevent_hook_removed','receiver_shutdown','shutdown')});$clock=$body[-1].qpc;$script:cGesture=$body[-1].gesture
    $held=$Mode -ne 'NOT_NEEDED';$initial=New-CCleanupProof $held;$initial|Add-Member -NotePropertyName phase -NotePropertyValue 'initial';$initial|Add-Member -NotePropertyName eligible -NotePropertyValue ($Mode -in @('PASS','FAILED'));$initial|Add-Member -NotePropertyName outcome -NotePropertyValue $(if($Mode -in @('PASS','FAILED')){'ELIGIBLE'}elseif($Mode -eq 'NOT_NEEDED'){'NOT_NEEDED'}else{'SKIPPED_NO_AUTHORITY'})
    if($Mode -eq 'SKIPPED'){$initial.cursor_root=999;$initial.cursor_root_owned_or_guard=$false}
    $body+=,(Add-CFixtureFields 'cleanup_input_diagnostic' ($clock+10) $initial)
    $lastRaw=$body|Where-Object type -eq raw_input|Select-Object -Last 1;$serial=$lastRaw.receiver_sequence;$sent=$Mode -in @('PASS','FAILED')
    if($sent){
        $boundary=Copy-EndDiagnostic $initial;$boundary.phase='api_boundary';$boundary|Add-Member -NotePropertyName cursor_matches_initial -NotePropertyValue $true;$body+=,(Add-CFixtureFields 'cleanup_input_diagnostic' ($clock+20) $boundary)
        $release=Copy-EndDiagnostic $boundary;foreach($pair in @(@('cleanup_up_attempted',$true),@('cleanup_up_sent',$true),@('sent',1),@('error',0),@('flags',4),@('input_tag',0x50424D41),@('receiver_watermark',$serial),@('injection_start_qpc',($clock+21)),@('injection_return_qpc',($clock+22)))){$release|Add-Member -NotePropertyName $pair[0] -NotePropertyValue $pair[1] -Force}
        $body+=,(Add-CFixtureFields 'cleanup_release' ($clock+30) $release)
        $packet=Copy-EndDiagnostic $lastRaw;$packet.sequence=0;$packet.qpc=$clock+40;$packet.receiver_sequence=++$serial;$packet.receiver_qpc=$clock+35;$packet.raw_flags=0;$packet.dx=0;$packet.dy=0;$packet.button_flags=2;$packet.cursor_sampled=$false;$packet.cursor_success=$false;$packet.cursor=$null;$packet.left_down=$true;$packet.raw_scope='cleanup';$packet.cleanup_observation=$true;$packet.acceptance_eligible=$false;$body+=,$packet
        $body+=,(Add-CFixtureFields 'cleanup_raw_up' ($clock+50) ([pscustomobject]@{raw_up_observed=$true;wait_result=0;timeout_ms=2000;receiver_sequence=$serial;receiver_qpc=$clock+35;receiver_watermark=$serial-1;injection_start_qpc=$clock+21;acceptance_eligible=$false}))
    }
    $final=New-CCleanupProof ($Mode -eq 'SKIPPED');$status=if($Mode -eq 'SKIPPED'){'SKIPPED_NO_AUTHORITY'}elseif($Mode -eq 'NOT_NEEDED'){'NOT_NEEDED'}elseif($Mode -eq 'FAILED'){'FAILED'}else{'PASS'}
    if($Mode -eq 'FAILED'){$final.source_visible=$false}
    if($FinalFailure){switch($FinalFailure){'invisible'{$final.source_visible=$false};'foreign'{$final.foreign_capture_transferred=$true};'unknown-desktop'{$initial.desktop_ready=$false;$initial.eligible=$false;$initial.outcome='SKIPPED_NO_AUTHORITY';$final.desktop_ready=$false};'new-button'{$final.left_down=$true}};$status=if($sent){'FAILED'}else{'SKIPPED_NO_AUTHORITY'}}
    if($status -eq 'SKIPPED_NO_AUTHORITY'){$body+=,(Add-CFixtureFields 'cleanup_skipped_no_authority' ($clock+60) ([pscustomobject]@{reason='cleanup_no_reliable_final_no_button_proof';acceptance_eligible=$false}))}
    foreach($pair in @(@('cleanup_input_release',$status),@('cleanup_up_attempted',$sent),@('cleanup_up_sent',$sent),@('raw_up_observed',$sent),@('left_button_high_bit',$final.left_down),@('final_capture_hwnd',$final.capture_hwnd),@('final_cursor',$final.cursor))){$final|Add-Member -NotePropertyName $pair[0] -NotePropertyValue $pair[1]}
    $body+=,(Add-CFixtureFields 'cleanup_final' ($clock+70) $final)
    foreach($old in $Rows|Where-Object {$_.type -in @('winevent_hook_removed','receiver_shutdown','shutdown')}){$r=Copy-EndDiagnostic $old;$r.sequence=0;$r.qpc=$clock+100;$clock+=100;if($r.type -eq 'receiver_shutdown'){$r.receiver_sequence=$serial+1;$r.receiver_qpc=$r.qpc-1};$body+=,$r}
    return ,(Renumber-CFixture $body)
}
$bad=New-CFixture;$d=$bad|Where-Object {$_.type -eq 'handoff_preflight' -and $_.phase -eq 'winevent_end'};$d.cursor_root=999;$d.cursor_root_owned_or_guard=$false;Refresh-CClassification $d;$blocked=Stop-CFixture $bad $d.sequence 'sole_guard_blocker'
$clean=Add-CCleanup $blocked PASS;$r=Test-EndDiagnosticsOwnedRecords $clean -AllowSynthetic
Check-EndDiagnostic ($r.Result -ceq 'BLOCKED' -and $r.CleanupInputRelease -ceq 'PASS' -and -not $r.CleanupAcceptanceEligible -and $r.Takeover -ceq 'NOT_RUN' -and $r.RawUpPackets -eq 2) 'P independently successful cleanup does not convert blocked handoff into accepted gesture'
foreach($case in @(@('SKIPPED',''),@('NOT_NEEDED',''),@('FAILED',''),@('PASS','foreign'),@('PASS','invisible'),@('NOT_NEEDED','unknown-desktop'),@('NOT_NEEDED','new-button'))){
    $wire=Add-CCleanup $blocked $case[0] $case[1];$c=Test-EndDiagnosticCleanup $wire @(Get-EndDiagnosticRows $wire 'owned') @(Get-EndDiagnosticRows $wire 'guard') $wire[0]
    $expected=if($case[0] -eq 'SKIPPED' -or $case[1] -in @('unknown-desktop','new-button')){'SKIPPED_NO_AUTHORITY'}elseif($case[0] -eq 'FAILED' -or $case[1]){'FAILED'}elseif($case[0] -eq 'NOT_NEEDED'){'NOT_NEEDED'}else{'PASS'}
    Check-EndDiagnostic ($c.Status -ceq $expected -and -not $c.AcceptanceEligible) "cleanup two snapshots/real receipt classification $($case -join '/')"
}
$wire=Copy-EndDiagnostic $clean;($wire|Where-Object type -eq cleanup_raw_up).receiver_sequence=999;Reject-CFixture $wire 'cleanup cannot infer Raw UP from successful SendInput'
$wire=Copy-EndDiagnostic $clean;($wire|Where-Object type -eq cleanup_release).test_down_owned=$false;Reject-CFixture $wire 'no owned DOWN means zero authorized cleanup clicks'
$wire=Copy-EndDiagnostic $clean;($wire|Where-Object type -eq cleanup_final).acceptance_eligible=$true;Reject-CFixture $wire 'cleanup can never claim acceptance eligibility'

# A fully legal early desktop-blocked v3 prefix creates no window, receiver,
# hook, cancellation, input or writer and cannot be a geometry counterexample.
$s=Copy-EndDiagnostic $resize[0];$s.sequence=1;$s.gesture=0;$s.qpc=10
$b=Copy-EndDiagnostic ($resize|Where-Object type -eq foreground_bootstrap);$b.sequence=2;$b.gesture=0;$b.qpc=11;$b.target=0;$b.set_foreground_attempted=$false;$b.set_foreground_success=$false;$b.foreground=500;$b.final_foreground_matches=$false;$b.result='BLOCKED';$b.reason='BLOCKED_BY_INTERACTIVE_DESKTOP'
$script:cGesture=0;$early=@($s,$b,(Add-CFixtureFields 'blocked' 12 ([pscustomobject]@{reason='BLOCKED_BY_INTERACTIVE_DESKTOP'})),(Add-CFixtureFields 'shutdown' 13 ([pscustomobject]@{result='BLOCKED';cursor_restored=$false;owned_window_destroyed=$true;guard_window_destroyed=$true;receiver_stopped=$false;external_windows_touched=$false;takeover_geometry_writes=0})));$early=Renumber-CFixture $early
$r=Test-EndDiagnosticsOwnedRecords $early -AllowSynthetic
Check-EndDiagnostic ($r.Result -ceq 'BLOCKED' -and $r.Architecture -ceq 'UNRESOLVED' -and $r.Cancel -ceq 'UNKNOWN' -and $r.Takeover -ceq 'NOT_RUN' -and $r.NativeWrites -eq 0 -and $r.RawPackets -eq 0) 'legal blocked prefix remains unknown without forged empirical evidence'

# Optional post-observation guard plan: arithmetic only. Coverage is never a
# substitute for actual WindowFromPoint root ownership in the handoff proof.
$planned=New-CFixture;$g=$planned|Where-Object type -eq guard
foreach($pair in @(@('margin_px',50),@('planned_move_bounds',@(100,100,920,540)),@('planned_bottom_resize_bounds',@(100,100,740,660)),@('planned_bounds',@(100,100,920,660)),@('positioning_available',$true),@('positioning',@(50,50,970,710)),@('move_trajectory_with_margin_covered',$true),@('bottom_resize_trajectory_with_margin_covered',$true))){$g|Add-Member -NotePropertyName $pair[0] -NotePropertyValue $pair[1] -Force}
$r=Test-EndDiagnosticsOwnedRecords $planned -AllowSynthetic
Check-EndDiagnostic ($r.Result -ceq 'PASS') 'optional guard bounds and 50px margin independently match the original owned frame'
foreach($case in @('forged-plan','forged-coverage','wrong-margin','partial-plan')){
    $bad=Copy-EndDiagnostic $planned;$g=$bad|Where-Object type -eq guard
    switch($case){'forged-plan'{$g.planned_move_bounds[2]++};'forged-coverage'{$g.positioning[2]=969};'wrong-margin'{$g.margin_px=49};'partial-plan'{$g.PSObject.Properties.Remove('planned_bounds')}}
    Reject-CFixture $bad "optional guard arithmetic rejects $case"
}
$bad=Copy-EndDiagnostic $planned;$g=$bad|Where-Object type -eq guard;$g.positioning=@(100,100,740,540);$g.move_trajectory_with_margin_covered=$false;$g.bottom_resize_trajectory_with_margin_covered=$false
$null=Test-EndDiagnosticGuardPlan @(Get-EndDiagnosticRows $bad 'owned') @(Get-EndDiagnosticRows $bad 'guard') $false
Check-EndDiagnostic $true 'honest insufficient coverage is a geometric diagnostic, not fabricated root authority'
Write-Host "end-diagnostics synthetic_only=true checks=$checks PASS"
