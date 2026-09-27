Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# Read-only scalar, geometry, native receipt and exact WinEvent primitives.
# The old aggregate validators and historical evidence are never changed.
. (Join-Path $PSScriptRoot 'r1c4b-end-diagnostics-validation.ps1')

function Assert-InputIsolation([bool]$Value,[string]$Reason){if(-not $Value){throw [InvalidOperationException]::new("owned input isolation: $Reason")}}
function Get-IsolationRows($Rows,[string]$Type){return @($Rows|Where-Object type -ceq $Type)}
function Assert-IsolationInt64($Value,[string]$Reason){
    # ConvertFrom-Json returns Int32 for small JSON integers and Int64 for QPC.
    # Validate before casting: floating-point, strings, bools and arrays must
    # never be rounded/coerced into clock or sequence evidence.
    if(-not (($Value -is [int] -or $Value -is [long]) -and $Value -ge 0)){throw [IO.InvalidDataException]::new("owned input isolation: $Reason must be a nonnegative signed Int64 integer")}
}
function Get-IsolationQpcDelta($Later,$Earlier,[string]$Reason){
    Assert-IsolationInt64 $Later "$Reason later QPC";Assert-IsolationInt64 $Earlier "$Reason earlier QPC"
    # A cross-stream latency can be signed. Ordering is enforced only by the
    # existing row/native receipt predicates, not by a new latency gate.
    return [long]([long]$Later-[long]$Earlier)
}
function Assert-IsolationNumericFields($Row){
    foreach($field in $Row.PSObject.Properties){
        if($field.Name -match '(^qpc$|_qpc$|^qpc_frequency$|^sequence$|_sequence$|^(receiver|raw)_watermark$)'){
            Assert-IsolationInt64 $field.Value "v4 $($Row.type).$($field.Name)"
        }elseif($field.Name -in @('arm_tick','event_time')){
            Assert-IsolationInt64 $field.Value "v4 $($Row.type).$($field.Name)"
            Assert-InputIsolation ($field.Value -le [long][UInt32]::MaxValue) 'DWORD event/arm tick'
        }
    }
}
function Get-ProductGestureAuthorityChecks($Proof){
    # Deliberately no cursor, root, guard or shield input in this classifier.
    $checks=[Collections.Generic.List[string]]::new()
    foreach($check in @(@('end_observed','MissingNativeEnd'),@('winevent_end_observed','MissingWinEventEnd'),@('own_identity','IdentityChanged'),@('desktop_ready','DesktopUnavailable'),@('source_visible','SourceNotVisible'),@('foreground_matches','ForegroundChanged'),@('gui_query_succeeded','GuiQueryFailed'))){if(-not (Get-AutoField $Proof $check[0])){$checks.Add($check[1])}}
    if($Proof.gui_query_succeeded){
        if(-not $Proof.capture_clear){$checks.Add('CaptureStillOwned')}
        if(-not $Proof.menu_clear){$checks.Add('MenuStillActive')}
        if(-not $Proof.move_size_clear -or -not $Proof.gui_in_movesize_clear){$checks.Add('MoveSizeStillActive')}
    }
    if(-not $Proof.left_down){$checks.Add('ButtonReleasedBeforeHandoff')}
    if(-not $Proof.buttons_modifiers_clear){$checks.Add('InputInterference')}
    if($Proof.raw_up_seen){$checks.Add('RawUpAlreadyObserved')}
    if(-not $Proof.receiver_healthy){$checks.Add('ReceiverUnhealthy')}
    if($Proof.foreign_capture_transferred){$checks.Add('ForeignCaptureTransferred')}
    if($Proof.own_identity -and -not $Proof.dpi_matches){$checks.Add('DpiChanged')}
    if($Proof.own_identity -and -not $Proof.monitor_matches){$checks.Add('MonitorChanged')}
    foreach($check in @(@('actual_positioning_available','ActualPositioningUnavailable'),@('actual_visible_available','ActualVisibleUnavailable'),@('terminal_positioning_available','TerminalPositioningUnavailable'),@('terminal_visible_available','TerminalVisibleUnavailable'))){if(-not (Get-AutoField $Proof $check[0])){$checks.Add($check[1])}}
    if($Proof.actual_positioning_available -and $Proof.actual_visible_available -and $Proof.terminal_positioning_available -and $Proof.terminal_visible_available -and -not $Proof.terminal_positioning_exact -and -not $Proof.terminal_visible_exact){$checks.Add('BothTerminalGeometryMismatch')}
    else{
        if($Proof.actual_positioning_available -and $Proof.terminal_positioning_available -and -not $Proof.terminal_positioning_exact){$checks.Add('TerminalPositioningMismatch')}
        if($Proof.actual_visible_available -and $Proof.terminal_visible_available -and -not $Proof.terminal_visible_exact){$checks.Add('TerminalVisibleMismatch')}
    }
    if($Proof.native_drag_after_cancel_return -ne 0){$checks.Add('NativeDragAfterCancelReturn')}
    if($Proof.native_drag_after_end -ne 0 -or $Proof.native_drag_after_winevent_end -ne 0){$checks.Add('NativeDragAfterEnd')}
    if($Proof.unowned_geometry_changes -ne 0){$checks.Add('UnownedGeometryChange')}
    if(-not $Proof.takeover_healthy){$checks.Add('TakeoverAlreadyUnhealthy')}
    return $checks.ToArray()
}

function Test-IsolationPointInside($Point,$Rect){
    Assert-AutoPoint $Point 'isolation point';$null=Get-AutoRect $Rect
    return $Point[0] -ge $Rect[0] -and $Point[0] -lt $Rect[2] -and $Point[1] -ge $Rect[1] -and $Point[1] -lt $Rect[3]
}
function Test-IsolationRectsOverlap($A,$B){
    $null=Get-AutoRect $A;$null=Get-AutoRect $B
    return $A[0] -lt $B[2] -and $A[2] -gt $B[0] -and $A[1] -lt $B[3] -and $A[3] -gt $B[1]
}
function Get-TestSyntheticIsolationChecks($Proof){
    $checks=[Collections.Generic.List[string]]::new()
    foreach($c in @(@('cursor_available','CursorUnavailable'),@('source_identity','SourceIdentityChanged'),@('desktop_ready','DesktopUnavailable'),@('foreground_matches','ForegroundChanged'),@('native_end_observed','MissingNativeEnd'),@('winevent_end_observed','MissingWinEventEnd'),@('product_authority_passed','ProductAuthorityUnavailable'))){if(-not (Get-AutoField $Proof $c[0])){$checks.Add($c[1])}}
    if($Proof.shield_needed){
        foreach($c in @(@('shield_identity','ShieldIdentityChanged'),@('shield_noactivate','ShieldCanActivate'),@('shield_topmost','ShieldNotTopmost'),@('shield_nonoverlap','ShieldOverlapsSource'),@('shield_created_after_product','ShieldCreatedBeforeAuthority'))){if(-not (Get-AutoField $Proof $c[0])){$checks.Add($c[1])}}
    }
    if(-not $Proof.current_point_owned){$checks.Add('CurrentPointNotTestOwned')}
    if(-not $Proof.all_planned_points_owned){$checks.Add('PlannedPointNotTestOwned')}
    return $checks.ToArray()
}

function Test-IsolationIdentity($Actual,$Expected){
    Assert-InputIsolation (@($Actual).Count -eq 4 -and @($Expected).Count -eq 4) 'identity tuple'
    foreach($n in @($Actual)+@($Expected)){Assert-AutoInteger $n 'identity HWND/PID/TID/nonce'}
    return @($Expected|Where-Object {$_ -le 0}).Count -eq 0 -and ($Actual -join ',') -ceq ($Expected -join ',')
}
function Test-IsolationWriteTiming($Entry,$NativeEnd,$WinEventEnd,$Ready){Assert-IsolationInt64 $Entry 'write entry QPC';Assert-IsolationInt64 $NativeEnd 'native END QPC';Assert-IsolationInt64 $WinEventEnd 'WinEvent END QPC';Assert-IsolationInt64 $Ready 'ready QPC';return $NativeEnd -gt 0 -and $WinEventEnd -gt 0 -and $Ready -gt $NativeEnd -and $Ready -gt $WinEventEnd -and $Entry -gt $Ready}
function Test-IsolationDestroyTiming($Destroyed,$Accepted,$RawUp){Assert-IsolationInt64 $Destroyed 'destroy QPC';Assert-IsolationInt64 $Accepted 'accepted QPC';Assert-IsolationInt64 $RawUp 'Raw UP QPC';return $Accepted -gt 0 -and $RawUp -gt 0 -and $Destroyed -gt $Accepted -and $Destroyed -gt $RawUp}
function Get-IsolationWriteActor([string]$Actor,[long]$Target,[long]$Source,[long]$Shield){
    if($Actor -ceq 'source' -and $Target -eq $Source -and $Source -gt 0){return 'source'}
    if($Actor -ceq 'shield' -and $Target -eq $Shield -and $Shield -gt 0 -and $Target -ne $Source){return 'shield'}
    throw 'owned input isolation: actor/target mismatched or unowned write'
}

function Test-FixCSeparatedAuthorityEvidence([string]$Path){
    $rows=@(Get-Content -LiteralPath $Path -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json -ErrorAction Stop})
    # First validate the original v3 contract and all actual witness references.
    # Its original aggregate verdict is preserved, not upgraded in-place.
    $historical=Test-EndDiagnosticsOwnedRecords $rows
    $final=@($rows|Where-Object {$_.type -ceq 'handoff_preflight' -and $_.phase -ceq 'winevent_end'})
    $product='UNKNOWN';$isolation='UNKNOWN';$checks=@()
    if($final.Count -eq 1){
        $checks=@(Get-ProductGestureAuthorityChecks $final[0]);$product=if($checks.Count){'FAIL'}else{'PASS'}
        $isolation=if($final[0].cursor_success -and $final[0].cursor_root_owned_or_guard){'PASS'}else{'BLOCKED'}
    }
    return [pscustomobject]@{HistoricalSchema='r1c4b-takeover-owned/v3';HistoricalVerdict=$historical.Result;ProductHandoffAuthority=$product;TestInputIsolation=$isolation;ProductFailedChecks=$checks;WinEventWitness=$(if($historical.MatchedEndCallbackSequence -gt 0){'PASS'}else{'UNKNOWN'});PostEndGeometryStability=$(if($final.Count -eq 1 -and $final[0].actual_positioning_available -and $final[0].actual_visible_available -and $final[0].terminal_positioning_available -and $final[0].terminal_visible_available -and $final[0].terminal_positioning_exact -and $final[0].terminal_visible_exact -and $final[0].native_drag_after_end -eq 0 -and $final[0].native_drag_after_winevent_end -eq 0 -and $final[0].unowned_geometry_changes -eq 0){'PASS'}else{'UNKNOWN'});Takeover=$historical.Takeover;Architecture=$historical.Architecture;HistoricalEvidenceUnchanged=$true}
}

function Test-IsolationEnvelope($Rows){
    Assert-InputIsolation ($Rows.Count -ge 2 -and $Rows[0].type -ceq 'startup' -and $Rows[-1].type -ceq 'shutdown') 'missing startup/shutdown'
    for($i=0;$i -lt $Rows.Count;++$i){$r=$Rows[$i];foreach($field in @('sequence','gesture','qpc')){Assert-AutoInteger (Get-AutoField $r $field) "v4 $field"};Assert-IsolationNumericFields $r;Assert-InputIsolation ($r.schema -ceq 'r1c4b-takeover-owned/v4' -and $r.sequence -eq [long]$i+1 -and $r.gesture -ge 0 -and $r.qpc -ge 0 -and ($i -eq 0 -or $r.qpc -ge $Rows[$i-1].qpc)) 'schema/sequence/QPC'}
    Assert-InputIsolation (@(Get-IsolationRows $Rows 'startup').Count -eq 1 -and @(Get-IsolationRows $Rows 'shutdown').Count -eq 1) 'duplicate lifecycle'
}

function Test-IsolationWinEvents($Rows,$Owned,$Startup,[bool]$Blocked){
    $installed=@(Get-IsolationRows $Rows 'winevent_hook_installed');$removed=@(Get-IsolationRows $Rows 'winevent_hook_removed');$callbacks=@(Get-IsolationRows $Rows 'winevent_callback');$matches=@(Get-IsolationRows $Rows 'winevent_match');$armed=@(Get-IsolationRows $Rows 'gesture_armed');$down=@(Get-IsolationRows $Rows 'native_button_down')
    Assert-InputIsolation ($installed.Count -le 1 -and $removed.Count -le 1 -and $armed.Count -le 1) 'duplicate hook/gesture epoch'
    foreach($h in $installed){
        foreach($n in @('hook','source_hwnd','source_pid','source_tid','install_tid','event_min','event_max','flags','dll','error')){Assert-AutoInteger (Get-AutoField $h $n) "hook $n"}
        foreach($n in @('install_success','skip_own_process','skip_own_thread')){Assert-AutoBoolean (Get-AutoField $h $n) "hook $n"}
        Assert-InputIsolation ($Owned.Count -eq 1 -and $h.source_hwnd -eq $Owned[0].hwnd -and $h.source_pid -eq $Startup.pid -and $h.source_tid -eq $Startup.ui_tid -and $h.install_tid -eq $Startup.ui_tid -and $h.event_min -eq 10 -and $h.event_max -eq 11 -and $h.flags -eq 0 -and $h.dll -eq 0 -and -not $h.skip_own_process -and -not $h.skip_own_thread -and $h.install_success -eq ($h.hook -gt 0)) 'not exact-source out-of-context own-observing hook'
        if($h.install_success){Assert-InputIsolation ($h.error -eq 0 -and $removed.Count -eq 1) 'successful hook was not removed'}else{Assert-InputIsolation ($Blocked -and $removed.Count -eq 0 -and $callbacks.Count -eq 0) 'failed hook produced witnesses'}
    }
    foreach($h in $removed){Assert-AutoBoolean $h.remove_success 'hook removed';Assert-InputIsolation ($installed.Count -eq 1 -and $h.sequence -gt $installed[0].sequence -and $h.hook -eq $installed[0].hook -and $h.source_hwnd -eq $installed[0].source_hwnd -and $h.source_pid -eq $Startup.pid -and $h.source_tid -eq $Startup.ui_tid -and $h.remove_tid -eq $Startup.ui_tid -and $h.remove_success -and $h.error -eq 0) 'hook cleanup changed source/thread or failed'}
    foreach($a in $armed){foreach($n in @('gesture_id','arm_qpc','arm_tick','source_hwnd','source_pid','source_tid')){Assert-AutoInteger (Get-AutoField $a $n) "gesture arm $n"};Assert-InputIsolation ($Owned.Count -eq 1 -and $a.gesture_id -eq 1 -and $a.source_hwnd -eq $Owned[0].hwnd -and $a.source_pid -eq $Startup.pid -and $a.source_tid -eq $Startup.ui_tid -and $a.arm_qpc -gt 0 -and $a.arm_qpc -le $a.qpc) 'gesture arm identity/QPC'}
    $start=$null;$finish=$null;[long]$serial=0
    foreach($c in $callbacks){
        foreach($n in @('hook','event','hwnd','event_thread','event_time','object_id','child_id','callback_tid','callback_sequence','callback_qpc','gesture_id','arm_qpc','arm_tick')){Assert-AutoInteger (Get-AutoField $c $n) "callback $n"}
        Assert-InputIsolation ($installed.Count -eq 1 -and $installed[0].install_success -and $c.callback_sequence -eq ++$serial -and $c.callback_qpc -le $c.qpc -and $c.sequence -gt $installed[0].sequence -and $c.sequence -lt $removed[0].sequence) 'callback sequence/QPC/lifecycle'
        $m=@($matches|Where-Object callback_sequence -eq $c.callback_sequence);Assert-InputIsolation ($m.Count -eq 1 -and $m[0].sequence -gt $c.sequence) 'callback has no unique match';$m=$m[0]
        foreach($n in @('callback_record_sequence','gesture_id','matched_start_callback_sequence','actual_source_pid','actual_source_tid','actual_source_nonce','run_nonce')){Assert-AutoInteger (Get-AutoField $m $n) "callback match $n"}
        Assert-AutoBoolean $m.accepted 'match accepted';Assert-AutoBoolean $m.source_identity 'match source identity'
        $identity=$m.actual_source_pid -eq $Startup.pid -and $m.actual_source_tid -eq $Startup.ui_tid -and $m.actual_source_nonce -eq $Startup.run_nonce
        Assert-InputIsolation ($m.run_nonce -eq $Startup.run_nonce -and $m.callback_record_sequence -eq $c.sequence -and $m.gesture_id -eq 1 -and $m.source_identity -eq $identity) 'match exact generation/reference'
        $reason='None';$accepted=$false
        if($c.hook -ne $installed[0].hook){$reason='WrongHook'}elseif($c.hwnd -ne $Owned[0].hwnd){$reason='WrongWindow'}elseif($c.event_thread -ne $Startup.ui_tid){$reason='WrongEventThread'}elseif($c.callback_tid -ne $Startup.ui_tid){$reason='WrongCallbackThread'}elseif(-not $identity){$reason='SourceIdentityChanged'}elseif($armed.Count -ne 1 -or $c.gesture_id -ne 1 -or $c.arm_qpc -ne $armed[0].arm_qpc -or $c.arm_tick -ne $armed[0].arm_tick){$reason='WrongGesture'}elseif($c.callback_qpc -le $c.arm_qpc -or $down.Count -ne 1 -or $down[0].sequence -gt $m.sequence){$reason='CallbackBeforeGesture'}elseif(-not (Test-EndDiagnosticTickFresh $c.event_time $c.arm_tick)){$reason='EventBeforeGesture'}
        elseif($c.event -eq 10){if($null -ne $start){$reason='StartAlreadyObserved'}else{$start=$c;$accepted=$true}}
        elseif($c.event -eq 11){if($null -eq $start){$reason='EndWithoutStart'}elseif($null -ne $finish){$reason='EndAlreadyObserved'}elseif($c.callback_sequence -le $start.callback_sequence -or $c.callback_qpc -le $start.callback_qpc -or -not (Test-EndDiagnosticTickFresh $c.event_time $start.event_time)){$reason='EventBeforeStart'}else{$finish=$c;$accepted=$true}}
        else{$reason='InvalidEvent'}
        Assert-InputIsolation ($m.accepted -eq $accepted -and $m.reason -ceq $reason -and $m.matched_start_callback_sequence -eq $(if($null -ne $start){$start.callback_sequence}else{0})) 'native WinEvent matcher verdict not independently derived'
    }
    Assert-InputIsolation ($matches.Count -eq $callbacks.Count) 'orphan WinEvent match'
    return [pscustomobject]@{Installed=($installed.Count -eq 1 -and $installed[0].install_success);Removed=($removed.Count -eq 1 -and $removed[0].remove_success);Start=$start;End=$finish}
}

function Test-IsolationProductPreflights($Rows,$Owned,$Startup,$WinEvent){
    $proofs=@(Get-IsolationRows $Rows 'product_handoff_preflight');$facts=@()
    Assert-InputIsolation ($proofs.Count -le 2) 'product preflight polling/retry'
    $bools=@('end_observed','winevent_end_observed','own_identity','desktop_ready','source_visible','foreground_matches','foreground_snapshot_stable','gui_query_succeeded','capture_clear','menu_clear','move_size_clear','gui_in_movesize_clear','buttons_modifiers_clear','left_down','raw_up_seen','receiver_healthy','takeover_healthy','foreign_capture_transferred','dpi_matches','monitor_matches','actual_positioning_available','actual_visible_available','terminal_positioning_available','terminal_visible_available','terminal_positioning_exact','terminal_visible_exact','winevent_healthy','log_healthy','product_authority','product_gate_passed','write_authorized','cursor_root_required_for_product')
    $ints=@('target','source_pid','source_tid','gesture_id','actual_source_pid','actual_source_tid','actual_source_nonce','run_nonce','end_sequence','end_qpc','winevent_end_sequence','winevent_end_qpc','foreground_hwnd','foreground_pid','foreground_tid','capture_hwnd','menu_owner_hwnd','move_size_hwnd','gui_flags','gui_error','dpi','frozen_dpi','monitor','frozen_monitor','positioning_error','visible_hresult','native_drag_after_cancel_return','native_drag_after_end','native_drag_after_winevent_end','unowned_geometry_changes')
    foreach($d in $proofs){
        Assert-InputIsolation ($Owned.Count -eq 1 -and $d.phase -cin @('native_end','winevent_end') -and @($proofs|Where-Object phase -ceq $d.phase).Count -eq 1) 'product proof phase/owned frame'
        foreach($name in $bools){Assert-AutoBoolean (Get-AutoField $d $name) "product $name"};foreach($name in $ints){Assert-AutoInteger (Get-AutoField $d $name) "product $name"}
        Assert-InputIsolation ($d.target -eq $Owned[0].hwnd -and $d.source_pid -eq $Startup.pid -and $d.source_tid -eq $Startup.ui_tid -and $d.gesture_id -eq 1 -and $d.run_nonce -eq $Startup.run_nonce -and $d.own_identity -eq ($d.actual_source_pid -eq $Startup.pid -and $d.actual_source_tid -eq $Startup.ui_tid -and $d.actual_source_nonce -eq $Startup.run_nonce) -and $d.frozen_dpi -eq $Owned[0].dpi -and $d.frozen_monitor -eq $Owned[0].monitor) 'product exact source/frozen context'
        foreach($name in @('native_drag_after_cancel_return','native_drag_after_end','native_drag_after_winevent_end','unowned_geometry_changes')){Assert-InputIsolation ($d.$name -ge 0) 'negative product counter'}
        Assert-InputIsolation (-not $d.cursor_root_required_for_product -and -not $d.write_authorized) 'product proof alone authorized input or source writes'
        $native=@($Rows|Where-Object {$_.type -ceq 'EXIT' -and $_.sequence -lt $d.sequence});Assert-InputIsolation ($native.Count -le 1) 'ambiguous native EXIT'
        $hasNative=$native.Count -eq 1;$hasWin=$null -ne $WinEvent.End -and $WinEvent.End.sequence -lt $d.sequence
        Assert-InputIsolation ($d.end_observed -eq $hasNative -and $d.winevent_end_observed -eq $hasWin -and $d.end_sequence -eq $(if($hasNative){$native[0].sequence}else{0}) -and $d.end_qpc -eq $(if($hasNative){$native[0].receipt_qpc}else{0}) -and $d.winevent_end_sequence -eq $(if($hasWin){$WinEvent.End.sequence}else{0}) -and $d.winevent_end_qpc -eq $(if($hasWin){$WinEvent.End.callback_qpc}else{0})) 'product END references are not actual receipts'
        $foreground=$d.foreground_snapshot_stable -and $d.foreground_hwnd -eq $Owned[0].hwnd -and $d.foreground_pid -eq $Startup.pid -and $d.foreground_tid -eq $Startup.ui_tid
        Assert-InputIsolation ($d.foreground_matches -eq $foreground -and $d.capture_clear -eq ($d.gui_query_succeeded -and $d.capture_hwnd -eq 0) -and $d.menu_clear -eq ($d.gui_query_succeeded -and $d.menu_owner_hwnd -eq 0 -and ($d.gui_flags -band 28) -eq 0) -and $d.move_size_clear -eq ($d.gui_query_succeeded -and $d.move_size_hwnd -eq 0) -and $d.gui_in_movesize_clear -eq ($d.gui_query_succeeded -and ($d.gui_flags -band 2) -eq 0) -and $d.dpi_matches -eq ($d.dpi -eq $Owned[0].dpi -and $Owned[0].dpi -gt 0) -and $d.monitor_matches -eq ($d.monitor -eq $Owned[0].monitor -and $Owned[0].monitor -gt 0)) 'product scalar authority summaries'
        foreach($kind in @('positioning','visible')){
            $actual=Get-AutoField $d ('actual_'+$kind);$terminal=Get-AutoField $d ('terminal_'+$kind);$available=$null -ne $actual;$terminalAvailable=$hasNative -and $null -ne (Get-AutoField $native[0] $kind)
            if($available){$null=Get-AutoRect $actual};if($null -ne $terminal){$null=Get-AutoRect $terminal}
            Assert-InputIsolation ((Get-AutoField $d ('actual_'+$kind+'_available')) -eq $available -and (Get-AutoField $d ('terminal_'+$kind+'_available')) -eq $terminalAvailable -and (Test-EndRectSame $terminal $(if($terminalAvailable){Get-AutoField $native[0] $kind}else{$null})) -and (Get-AutoField $d ('terminal_'+$kind+'_exact')) -eq ($available -and $terminalAvailable -and (Test-AutoRect $actual $terminal))) 'product actual/terminal geometry independently captured'
        }
        $drags=@($Rows|Where-Object {$_.type -ceq 'DRAG' -and $_.sequence -lt $d.sequence});foreach($g in $drags){Assert-AutoInteger $g.receipt_qpc 'product native DRAG receipt'}
        $returned=@($Rows|Where-Object {$_.type -ceq 'cancel_return' -and $_.sequence -lt $d.sequence})
        $afterReturn=if($returned.Count){@($drags|Where-Object receipt_qpc -gt $returned[0].returned_qpc).Count}else{0};$afterNative=if($hasNative){@($drags|Where-Object receipt_qpc -gt $native[0].receipt_qpc).Count}else{0};$afterWin=if($hasWin){@($drags|Where-Object receipt_qpc -gt $WinEvent.End.callback_qpc).Count}else{0}
        Assert-InputIsolation ($d.native_drag_after_cancel_return -eq $afterReturn -and $d.native_drag_after_end -eq $afterNative -and $d.native_drag_after_winevent_end -eq $afterWin) 'product native counters lack receipts'
        [long]$changes=0
        if($hasNative -and $hasWin){$anchor=@(Get-IsolationRows $Rows 'intent_anchor');$previousP=if($anchor.Count){$anchor[0].start_positioning}else{$Owned[0].positioning};$previousV=if($anchor.Count){$anchor[0].start_visible}else{$Owned[0].visible};foreach($p in @($Rows|Where-Object {$_.type -ceq 'POSITION_CHANGED' -and $_.sequence -lt $d.sequence -and $_.operation_id -eq 0})){Assert-AutoInteger $p.position_receipt_qpc 'product positioning receipt';if($null -eq $p.positioning -or $null -eq $p.visible){continue};if($p.position_receipt_qpc -gt $WinEvent.End.callback_qpc -and (-not (Test-AutoRect $p.positioning $previousP) -or -not (Test-AutoRect $p.visible $previousV))){++$changes};$previousP=$p.positioning;$previousV=$p.visible}}
        Assert-InputIsolation ($d.unowned_geometry_changes -eq $changes) 'product unowned changes not independently counted'
        $checks=@(Get-ProductGestureAuthorityChecks $d);$first=if($checks.Count){$checks[0]}else{'None'};$pass=$checks.Count -eq 0
        Assert-InputIsolation (($d.failed_checks -join ',') -ceq ($checks -join ',') -and ($d.product_failed_checks -join ',') -ceq ($checks -join ',') -and $d.failure_class -ceq $first -and $d.product_failure_class -ceq $first -and $d.product_authority -eq $pass -and $d.product_gate_passed -eq $pass) 'product verdict was not independently recomputed'
        $facts+=[pscustomobject]@{Phase=$d.phase;Sequence=$d.sequence;Qpc=$d.qpc;Pass=$pass;Checks=$checks;Proof=$d}
    }
    return ,$facts
}

function Test-IsolationWriterAuthority($Proof,$Owned,$Startup,[bool]$Held,[switch]$TerminalUp){
    foreach($name in @('target','source_pid','source_tid','actual_source_pid','actual_source_tid','actual_source_nonce','run_nonce','foreground','foreground_pid','foreground_tid','capture_hwnd','menu_owner_hwnd','move_size_hwnd','gui_flags','dpi','monitor')){Assert-AutoInteger (Get-AutoField $Proof $name) "source authority $name"}
    foreach($name in @('own_identity','desktop_ready','source_visible','gui_query_succeeded','buttons_modifiers_clear','left_down','receiver_healthy','raw_up_seen','product_authority','cursor_data_ready','cursor_root_required_for_product')){Assert-AutoBoolean (Get-AutoField $Proof $name) "source authority $name"}
    Assert-InputIsolation ($Owned.Count -eq 1 -and $Proof.target -eq $Owned[0].hwnd -and $Proof.source_pid -eq $Startup.pid -and $Proof.source_tid -eq $Startup.ui_tid -and $Proof.run_nonce -eq $Startup.run_nonce -and $Proof.own_identity -eq ($Proof.actual_source_pid -eq $Startup.pid -and $Proof.actual_source_tid -eq $Startup.ui_tid -and $Proof.actual_source_nonce -eq $Startup.run_nonce)) 'writer exact generation/source binding'
    Assert-InputIsolation (-not $Proof.cursor_root_required_for_product) 'test root entered product writer authority'
    # Cursor sampling is a separate geometry-data capability; a native summary
    # can only reject, never substitute for the independently bound primitives.
    return $Proof.product_authority -and $Proof.cursor_data_ready -and $Proof.own_identity -and $Proof.desktop_ready -and $Proof.source_visible -and $Proof.foreground -eq $Owned[0].hwnd -and $Proof.foreground_pid -eq $Startup.pid -and $Proof.foreground_tid -eq $Startup.ui_tid -and $Proof.gui_query_succeeded -and $Proof.capture_hwnd -eq 0 -and $Proof.menu_owner_hwnd -eq 0 -and $Proof.move_size_hwnd -eq 0 -and ($Proof.gui_flags -band 30) -eq 0 -and $Proof.buttons_modifiers_clear -and ($TerminalUp -or $Proof.left_down -eq $Held) -and $Proof.receiver_healthy -and $Proof.dpi -eq $Owned[0].dpi -and $Proof.monitor -eq $Owned[0].monitor
}

function Test-IsolationSourceWriters($Rows,$Owned,$Startup,$Win,$Product,$Isolation,$Raw){
    $anchor=@(Get-IsolationRows $Rows 'intent_anchor');$native=@(Get-IsolationRows $Rows 'EXIT');$enter=@(Get-IsolationRows $Rows 'ENTER');$down=@(Get-IsolationRows $Rows 'native_button_down');$path=@(Get-IsolationRows $Rows 'path')
    foreach($set in @($anchor,$native,$enter,$down,$path)){Assert-InputIsolation ($set.Count -le 1) 'duplicate source native anchor'}
    if($anchor.Count){$a=$anchor[0];Assert-InputIsolation ($enter.Count -eq 1 -and $down.Count -eq 1 -and $path.Count -eq 1 -and $a.native_enter_sequence -eq $enter[0].sequence -and $a.native_enter_qpc -eq $enter[0].receipt_qpc -and $a.native_down_sequence -eq $down[0].sequence -and $a.operation -ceq $Startup.operation -and (Test-EndPointExact $a.pointer_down $down[0].cursor) -and (Test-AutoPoint $a.pointer_down $path[0].start) -and (Test-AutoRect $a.start_positioning $enter[0].positioning) -and (Test-AutoRect $a.start_visible $enter[0].visible)) 'original intent was not frozen at actual native DOWN/ENTER'}
    $begins=@(Get-IsolationRows $Rows 'writer_begin');$results=@(Get-IsolationRows $Rows 'writer_result');$quanta=@(Get-IsolationRows $Rows 'raw_quantum');$handoff=@(Get-IsolationRows $Rows 'handoff_begin');$handoffDone=@(Get-IsolationRows $Rows 'handoff_complete')
    Assert-InputIsolation ($results.Count -eq $begins.Count -and $handoff.Count -le 1 -and $handoffDone.Count -le 1) 'orphan or duplicate writer lifecycle'
    $failure=$false;[long]$beforeBarrier=0;[long]$nativeCalls=0;[long]$handoffCalls=0;[long]$remaining=0;$fullTargets=$true;[long]$lastId=0;[long]$lastQuantum=0;[long]$usedRaw=0;$expectedP=$null;$expectedV=$null;$operations=@{};$latencies=[Collections.Generic.List[long]]::new();$rawOwnerTicks=[Collections.Generic.List[long]]::new()
    $handoffGate='NOT_RUN';[long]$firstWrite=0;[long]$lastReturn=0;[long]$handoffStart=0;$handoffDuration=$null
    if($native.Count){$expectedP=$native[0].positioning;$expectedV=$native[0].visible}
    if($handoff.Count){
        $h=$handoff[0];Assert-InputIsolation ($anchor.Count -eq 1 -and $native.Count -eq 1 -and $h.end_sequence -eq $native[0].sequence -and $h.end_qpc -eq $native[0].receipt_qpc) 'handoff does not bind native EXIT/original START'
        $intended=Get-EndHandoffIntended $Startup.operation $anchor[0].start_positioning $anchor[0].start_visible $anchor[0].pointer_down $h.current_cursor
        if(-not (Test-AutoRect $h.intended_positioning $intended.Positioning) -or -not (Test-AutoRect $h.intended_visible $intended.Visible) -or -not (Test-EndPointExact $h.cursor_delta $intended.Delta)){$failure=$true;$fullTargets=$false}
        if(-not (Test-AutoRect $h.actual_handoff_positioning $expectedP) -or -not (Test-AutoRect $h.actual_handoff_visible $expectedV)){$failure=$true}
        $usedRaw=$h.raw_watermark
    }
    foreach($b in $begins){
        foreach($name in @('operation_id','quantum_id','raw_first_sequence','raw_last_sequence','raw_trigger_qpc','native_calls','end_qpc','winevent_end_sequence','winevent_end_qpc','product_preflight_sequence','input_isolation_ready_sequence','input_isolation_ready_qpc','writer_input_isolation_sequence')){Assert-AutoInteger (Get-AutoField $b $name) "writer $name"}
        Assert-InputIsolation ($b.actor -ceq 'source' -and $anchor.Count -eq 1 -and $native.Count -eq 1 -and $b.operation_id -gt $lastId -and $b.native_calls -in @(0,1) -and $b.kind -cin @('handoff','raw_movement')) 'source writer identity/actor/kind'
        $r=@($results|Where-Object operation_id -eq $b.operation_id);Assert-InputIsolation ($r.Count -eq 1 -and $r[0].sequence -gt $b.sequence -and $r[0].quantum_id -eq $b.quantum_id -and $r[0].kind -ceq $b.kind -and $r[0].native_calls -eq $b.native_calls) 'bounded writer result pairing';$r=$r[0]
        foreach($name in @('native_start_qpc','native_return_qpc','error','native_calls','positioning_error','visible_hresult')){Assert-AutoInteger (Get-AutoField $r $name) "writer result $name"}
        foreach($name in @('native_success','positioning_exact','visible_exact','postverify_exact')){Assert-AutoBoolean (Get-AutoField $r $name) "writer result $name"}
        $authorized=Test-IsolationWriterAuthority $b $Owned $Startup $true
        if(-not $authorized -or $b.raw_up_seen){$failure=$true}
        $gated=$Product.Count -eq 1 -and $Product[0].Pass -and $null -ne $Isolation.Ready -and $Isolation.Pass -and $null -ne $Win.End
        if(-not $gated){++$beforeBarrier;$failure=$true}
        else{
            if($b.product_preflight_sequence -ne $Product[0].Sequence -or $b.input_isolation_ready_sequence -ne $Isolation.Ready.sequence -or $b.input_isolation_ready_qpc -ne $Isolation.Ready.isolation_ready_qpc -or $b.winevent_end_sequence -ne $Win.End.sequence -or $b.winevent_end_qpc -ne $Win.End.callback_qpc -or $b.end_qpc -ne $native[0].receipt_qpc -or $b.sequence -le $Isolation.Ready.sequence){++$beforeBarrier;$failure=$true}
        }
        $testProof=@($Rows|Where-Object {$_.type -ceq 'writer_input_isolation' -and $_.sequence -eq $b.writer_input_isolation_sequence})
        Assert-InputIsolation ($testProof.Count -eq 1 -and $testProof[0].sequence -lt $b.sequence -and $testProof[0].operation_id -eq $b.operation_id -and $testProof[0].quantum_id -eq $b.quantum_id -and $testProof[0].kind -ceq $b.kind -and $testProof[0].input_isolation_ready_sequence -eq $b.input_isolation_ready_sequence -and (Test-EndPointExact $testProof[0].destination $b.cursor) -and (Test-AutoRect $testProof[0].source_positioning $b.before_positioning)) 'source writer did not bind a fresh test-only owner-cursor isolation proof'
        if(-not (Test-IsolationPointProof $testProof[0] $Rows $Owned $Startup $Isolation.Shield).Pass){$failure=$true}
        $target=Get-EndHandoffIntended $Startup.operation $anchor[0].start_positioning $anchor[0].start_visible $anchor[0].pointer_down $b.cursor
        $full=(Test-AutoRect $b.intended_positioning $target.Positioning) -and (Test-AutoRect $b.intended_visible $target.Visible) -and (Test-EndPointExact $b.cursor_delta $target.Delta)
        if(-not $full){$failure=$true;$fullTargets=$false}
        if(-not (Test-AutoRect $b.before_positioning $expectedP) -or -not (Test-AutoRect $b.before_visible $expectedV) -or -not (Test-AutoRect $b.expected_before_positioning $expectedP) -or -not (Test-AutoRect $b.expected_before_visible $expectedV)){$failure=$true}
        if($b.native_calls){
            ++$nativeCalls;Assert-InputIsolation ($r.native_start_qpc -ge $b.qpc -and $r.native_start_qpc -le $r.native_return_qpc -and $r.native_return_qpc -le $r.qpc) 'source native clock order'
            if(-not $firstWrite){$firstWrite=[long]$r.native_start_qpc}
            [long]$currentReturn=$r.native_return_qpc
            if($currentReturn -gt $lastReturn){$lastReturn=$currentReturn}
            if(-not $gated -or $r.native_start_qpc -le $Win.End.callback_qpc -or $r.native_start_qpc -le $native[0].receipt_qpc -or $r.native_start_qpc -le $Isolation.Ready.isolation_ready_qpc){++$beforeBarrier;$failure=$true}
            if(-not $r.native_success -or $r.error -ne 0){$failure=$true}
        }else{Assert-InputIsolation ($r.native_start_qpc -eq 0 -and $r.native_return_qpc -eq 0 -and $r.native_success -and $r.error -eq 0) 'zero-call operation claims a native write'}
        $exact=Test-EndPostverifyDiagnostic $r $b
        $exact=$exact -and (Test-AutoRect $r.positioning $target.Positioning) -and (Test-AutoRect $r.visible $target.Visible) -and (Test-AutoRect $r.full_positioning $target.Positioning) -and (Test-AutoRect $r.full_visible $target.Visible)
        if(-not $exact){$failure=$true}
        if($b.kind -ceq 'handoff'){
            Assert-InputIsolation ($b.quantum_id -eq 0 -and $b.raw_first_sequence -eq 0 -and $b.raw_last_sequence -eq 0 -and $b.raw_trigger_qpc -eq 0 -and $handoff.Count -eq 1) 'handoff was mislabeled a raw quantum'
            $handoffCalls+=$b.native_calls;$mismatch=-not (Test-AutoRect $b.before_positioning $target.Positioning) -or -not (Test-AutoRect $b.before_visible $target.Visible)
            $handoffGate=if($full -and $exact -and $authorized -and $b.native_calls -eq [int]$mismatch -and $handoffCalls -le 1){'PASS'}else{'FAIL'}
            if($b.native_calls){$handoffStart=[long]$r.native_start_qpc;$handoffDuration=Get-IsolationQpcDelta $r.native_return_qpc $r.native_start_qpc 'handoff native duration'}
        }else{
            Assert-InputIsolation ($handoffDone.Count -eq 1 -and $b.sequence -gt $handoffDone[0].sequence -and $b.quantum_id -eq $lastQuantum+1 -and $b.raw_first_sequence -gt $usedRaw -and $b.raw_last_sequence -ge $b.raw_first_sequence) 'raw quantum does not consume a fresh ordered batch'
            $packets=@($Raw|Where-Object {$_.receiver_sequence -ge $b.raw_first_sequence -and $_.receiver_sequence -le $b.raw_last_sequence -and $_.cursor_sampled});$q=@($quanta|Where-Object operation_id -eq $b.operation_id)
            Assert-InputIsolation ($packets.Count -gt 0 -and $q.Count -eq 1 -and $q[0].sequence -gt $r.sequence -and $q[0].quantum_id -eq $b.quantum_id -and $q[0].coalesced_count -eq $packets.Count -and $q[0].native_calls -eq $b.native_calls -and $q[0].raw_first_sequence -eq $b.raw_first_sequence -and $q[0].raw_last_sequence -eq $b.raw_last_sequence -and $q[0].raw_trigger_qpc -eq $b.raw_trigger_qpc -and $b.raw_trigger_qpc -eq $packets[-1].receiver_qpc -and $b.raw_last_sequence -eq $packets[-1].receiver_sequence -and $b.raw_trigger_qpc -le $b.qpc -and (Test-AutoPoint $q[0].cursor $b.cursor)) 'quantum is not the actual raw-triggered owner batch'
            foreach($packet in $packets){
                $inputRows=@($Rows|Where-Object {$_.type -ceq 'input' -and $_.flags -eq 1 -and $_.sent -eq 1 -and -not $_.restoring_cursor -and (Test-TakeoverMotionReceipt $packet $_ $true $true)})
                if($packet.raw_scope -cne 'gesture' -or -not $packet.acceptance_eligible -or ($packet.button_flags -band 2) -ne 0 -or -not $packet.test_tag_matches -or $inputRows.Count -eq 0){$failure=$true}
            }
            $rawOwnerTicks.Add((Get-IsolationQpcDelta $b.qpc $b.raw_trigger_qpc 'Raw receipt to owner quantum'));if($b.native_calls){$latencies.Add((Get-IsolationQpcDelta $r.native_start_qpc $b.raw_trigger_qpc 'Raw receipt to native writer'))}
            $usedRaw=$b.raw_last_sequence;$lastQuantum=$b.quantum_id;++$remaining
        }
        $operations[[string]$b.operation_id]=[pscustomobject]@{Begin=$b;Result=$r;Target=$target};$expectedP=$r.positioning;$expectedV=$r.visible;$lastId=$b.operation_id
    }
    Assert-InputIsolation ($quanta.Count -eq @($begins|Where-Object kind -ceq raw_movement).Count) 'orphan raw quantum'
    if(@($begins|Where-Object kind -ceq handoff).Count -gt 1){$failure=$true;$handoffGate='FAIL'}
    if($handoffDone.Count){$h=$handoffDone[0];Assert-InputIsolation ($operations.ContainsKey([string]$h.operation_id) -and $operations[[string]$h.operation_id].Begin.kind -ceq 'handoff' -and $h.sequence -gt $operations[[string]$h.operation_id].Result.sequence -and $h.native_calls -eq $operations[[string]$h.operation_id].Begin.native_calls -and $h.end_qpc -eq $native[0].receipt_qpc) 'handoff completion has no actual source operation';if(-not $h.exact){$failure=$true}}
    return [pscustomobject]@{Failure=$failure;BarrierViolations=$beforeBarrier;NativeCalls=$nativeCalls;HandoffCalls=$handoffCalls;Remaining=$remaining;FullTargets=$fullTargets;Handoff=$handoffGate;FirstWriteQpc=$firstWrite;LastNativeReturnQpc=$lastReturn;HandoffStartQpc=$handoffStart;HandoffDuration=$handoffDuration;RawWriteTicks=@($latencies.ToArray());RawOwnerTicks=@($rawOwnerTicks.ToArray());Operations=$operations;ExpectedPositioning=$expectedP;ExpectedVisible=$expectedV}
}

function Assert-IsolationInputReceipt($Request,$Owned){
    foreach($name in @('flags','sent','error','input_tag','injection_start_qpc','injection_return_qpc','normalized_dx','normalized_dy','actual_mouse_flags','receiver_watermark')){Assert-AutoInteger (Get-AutoField $Request $name) "actual INPUT $name"}
    Assert-AutoBoolean $Request.restoring_cursor 'restore flag';Assert-InputIsolation (-not $Request.restoring_cursor) 'v4 must not inject a cursor-restore MOVE';Assert-AutoPoint $Request.point 'actual INPUT point'
    Assert-InputIsolation ($Request.flags -in @(1,2,4) -and $Request.sent -in @(0,1) -and $Request.input_tag -eq 0x50424D41 -and $Request.receiver_watermark -ge 1 -and $Request.injection_start_qpc -le $Request.injection_return_qpc -and $Request.injection_return_qpc -le $Request.qpc -and ($Request.sent -ne 1 -or $Request.error -eq 0) -and (Test-AutoRect $Request.virtual_screen $Owned[0].virtual_screen)) 'actual INPUT scope/clock/screen'
    if($Request.flags -eq 1){
        $s=$Request.virtual_screen;$width=[decimal]$s[2]-$s[0];$height=[decimal]$s[3]-$s[1];Assert-InputIsolation (Test-IsolationPointInside $Request.point $s) 'INPUT point outside virtual screen'
        $dx=[Math]::Floor((2*([decimal]$Request.point[0]-$s[0])+1)*65536/(2*$width));$dy=[Math]::Floor((2*([decimal]$Request.point[1]-$s[1])+1)*65536/(2*$height))
        Assert-InputIsolation ($Request.actual_mouse_flags -eq 57345 -and $Request.normalized_dx -eq $dx -and $Request.normalized_dy -eq $dy) 'INPUT normalized coordinates are not the requested pixel centre'
    }else{Assert-InputIsolation ($Request.actual_mouse_flags -eq $Request.flags -and $Request.normalized_dx -eq 0 -and $Request.normalized_dy -eq 0) 'button INPUT includes unauthorized movement'}
}

function Test-IsolationNativeLifecycle($Rows,$Owned,$Startup,$Raw,$Bootstrap){
    $path=@(Get-IsolationRows $Rows 'path');$enter=@(Get-IsolationRows $Rows 'ENTER');$down=@(Get-IsolationRows $Rows 'native_button_down');$cancel=@(Get-IsolationRows $Rows 'cancel_begin');$returned=@(Get-IsolationRows $Rows 'cancel_return');$exit=@(Get-IsolationRows $Rows 'EXIT');$drag=@(Get-IsolationRows $Rows 'DRAG')
    foreach($a in @($path,$enter,$down,$cancel,$returned,$exit)){Assert-InputIsolation ($a.Count -le 1) 'duplicate one-gesture native lifecycle'}
    $gesture=if($Startup.operation -ceq 'Move'){1}else{2};[long]$settlement=0;[long]$afterReturn=0;[long]$afterEnd=0
    if($path.Count){
        Assert-InputIsolation ($Owned.Count -eq 1 -and $Raw.Gate -ceq 'PASS' -and $path[0].sequence -gt $Raw.CompleteSequence -and $path[0].samples -eq 20 -and $path[0].cancel_after_sample -eq 2 -and $path[0].interval_ms -eq 30 -and $path[0].hit_test -eq $(if($gesture -eq 1){2}else{15}) -and $path[0].foreground -eq $Owned[0].hwnd) 'native phase before proven background preflight or wrong path'
    }
    foreach($d in $drag){Assert-AutoInteger $d.receipt_qpc 'native drag receipt';Assert-InputIsolation ($d.event -ceq $(if($gesture -eq 1){'WM_MOVING'}else{'WM_SIZING'}) -and ($gesture -eq 1 -or $d.edge -eq 6) -and $d.receipt_qpc -le $d.qpc) 'real native operation/edge/receipt'}
    if($cancel.Count){
        $c=$cancel[0];Assert-InputIsolation ($path.Count -eq 1 -and $enter.Count -eq 1 -and $down.Count -eq 1 -and $c.target -eq $Owned[0].hwnd -and $c.source_tid -eq $Startup.ui_tid -and $c.capture_before -eq $Owned[0].hwnd -and $c.left_down -and $down[0].sequence -lt $enter[0].sequence -and $enter[0].sequence -lt $c.sequence -and @($drag|Where-Object sequence -lt $c.sequence).Count -ge 2) 'cancel lacks a real exact-owned native ENTER/DOWN/DRAG'
        foreach($index in @(1,2)){
            $p=@(($path[0].start[0]+($path[0].end[0]-$path[0].start[0])*$index/20),($path[0].start[1]+($path[0].end[1]-$path[0].start[1])*$index/20))
            $requests=@($Rows|Where-Object {$_.type -ceq 'input' -and $_.gesture -eq $gesture -and $_.flags -eq 1 -and $_.sent -eq 1 -and $_.sequence -lt $c.sequence -and (Test-AutoPoint $_.point $p)})
            Assert-InputIsolation ($requests.Count -eq 1 -and @($Raw.Packets|Where-Object {Test-TakeoverMotionReceipt $_ $requests[0] $true $true}).Count -ge 1) 'native first two movements are not actual correlated test INPUT'
        }
    }
    if($returned.Count){
        $r=$returned[0];Assert-InputIsolation ($cancel.Count -eq 1 -and $r.sequence -gt $cancel[0].sequence -and $r.issued_qpc -le $cancel[0].qpc -and $r.returned_qpc -ge $cancel[0].qpc -and $r.returned_qpc -le $r.qpc -and (-not $r.transport_success -or $r.error -eq 0)) 'cancel actual API clock/error'
        $messages=@($Rows|Where-Object {$_.type -ceq 'cancel_message' -and $_.sequence -gt $cancel[0].sequence -and $_.sequence -lt $r.sequence -and $_.target -eq $Owned[0].hwnd -and $_.left_down})
        if($r.transport_success){Assert-InputIsolation ($messages.Count -eq 1) 'cancel recipient message missing'}
        $afterReturn=@($drag|Where-Object receipt_qpc -gt $r.returned_qpc).Count
    }
    if($exit.Count){$x=$exit[0];Assert-InputIsolation ($cancel.Count -eq 1 -and $x.sequence -gt $cancel[0].sequence -and $x.end_sequence -eq $x.sequence -and $x.receipt_qpc -le $x.qpc) 'native EXIT is not the real post-cancel receipt';$afterEnd=@($drag|Where-Object receipt_qpc -gt $x.receipt_qpc).Count;if($returned.Count){$settlement=@($Rows|Where-Object {$_.type -ceq 'POSITION_CHANGED' -and $_.operation_id -eq 0 -and $_.position_receipt_qpc -gt $returned[0].returned_qpc -and $_.position_receipt_qpc -lt $x.receipt_qpc}).Count}}
    $captureClear=$exit.Count -eq 1 -and $exit[0].owner_capture -eq 0 -and @($Rows|Where-Object {$_.type -ceq 'CAPTURE_CHANGED' -and $_.new_capture -eq 0 -and $_.owner_capture -eq 0 -and $cancel.Count -eq 1 -and $_.sequence -gt $cancel[0].sequence}).Count -gt 0
    return [pscustomobject]@{Path=$(if($path.Count){$path[0]}else{$null});Exit=$(if($exit.Count){$exit[0]}else{$null});CancelCalls=$cancel.Count;Returned=($returned.Count -eq 1 -and $returned[0].transport_success);RealEnter=($enter.Count -eq 1);RealDrag=($drag.Count -ge 2);CaptureClear=$captureClear;HeldExit=($exit.Count -eq 1 -and $exit[0].left_down);Settlements=$settlement;AfterReturn=$afterReturn;AfterNativeEnd=$afterEnd}
}

function Test-IsolationPointProof($Row,$Rows,$Owned,$Startup,$Shield,[bool]$PriorHealthy=$true){
    foreach($n in @('run_nonce','source_hwnd','source_pid','source_tid','actual_source_pid','actual_source_tid','actual_source_nonce','shield_hwnd','shield_pid','shield_tid','actual_shield_pid','actual_shield_tid','actual_shield_nonce','root','expected_root','foreground_hwnd','foreground_pid','foreground_tid')){Assert-AutoInteger (Get-AutoField $Row $n) "isolation snapshot $n"}
    foreach($n in @('source_identity','shield_identity','cursor_available','source_positioning_available','inside_source','root_owned','foreground_matches','shield_needed','shield_active','shield_noactivate','shield_topmost','shield_visible','shield_never_activated','shield_nonoverlapping','point_valid')){Assert-AutoBoolean (Get-AutoField $Row $n) "isolation snapshot $n"}
    Assert-AutoPoint $Row.destination 'fresh destination'
    $retired=@($Rows|Where-Object {$_.type -ceq 'input_shield_destroyed' -and $_.sequence -lt $Row.sequence}).Count -gt 0
    $present=$null -ne $Shield -and $Shield.sequence -lt $Row.sequence -and -not $retired
    Assert-InputIsolation ($Owned.Count -eq 1 -and $Row.run_nonce -eq $Startup.run_nonce -and $Row.source_hwnd -eq $Owned[0].hwnd -and $Row.source_pid -eq $Startup.pid -and $Row.source_tid -eq $Startup.ui_tid -and $Row.shield_pid -eq $Startup.pid -and $Row.shield_tid -eq $Startup.ui_tid -and $Row.shield_hwnd -eq $(if($present){$Shield.shield_hwnd}else{0})) 'isolation source/shield lifecycle binding'
    $sourceIdentity=Test-IsolationIdentity @($Row.source_hwnd,$Row.actual_source_pid,$Row.actual_source_tid,$Row.actual_source_nonce) @($Owned[0].hwnd,$Startup.pid,$Startup.ui_tid,$Startup.run_nonce)
    $shieldIdentity=if($present){Test-IsolationIdentity @($Row.shield_hwnd,$Row.actual_shield_pid,$Row.actual_shield_tid,$Row.actual_shield_nonce) @($Shield.shield_hwnd,$Startup.pid,$Startup.ui_tid,$Startup.run_nonce)}else{$false}
    $sourceAvailable=$null -ne $Row.source_positioning;Assert-InputIsolation ($Row.source_positioning_available -eq $sourceAvailable) 'isolation source capture availability'
    if($sourceAvailable){$null=Get-AutoRect $Row.source_positioning};if($null -ne $Row.shield_positioning){$null=Get-AutoRect $Row.shield_positioning}
    $inside=$sourceAvailable -and (Test-IsolationPointInside $Row.destination $Row.source_positioning)
    $foreground=$Row.foreground_hwnd -eq $Owned[0].hwnd -and $Row.foreground_pid -eq $Startup.pid -and $Row.foreground_tid -eq $Startup.ui_tid
    $rootOwned=($Row.root -eq $Owned[0].hwnd -and $sourceIdentity) -or ($null -ne $Shield -and $Row.root -eq $Shield.shield_hwnd -and $shieldIdentity -and $Row.shield_active)
    $overlap=$sourceAvailable -and $null -ne $Row.shield_positioning -and (Test-IsolationRectsOverlap $Row.source_positioning $Row.shield_positioning)
    Assert-InputIsolation ($Row.source_identity -eq $sourceIdentity -and $Row.shield_identity -eq $shieldIdentity -and $Row.inside_source -eq $inside -and $Row.expected_root -eq $(if($inside){$Owned[0].hwnd}else{$Row.shield_hwnd}) -and (-not $Row.foreground_matches -or $foreground) -and $Row.root_owned -eq $rootOwned -and $Row.shield_nonoverlapping -eq (-not $overlap)) 'isolation summary is not the independently bound tuple/region/root'
    $activated=@($Rows|Where-Object {$_.type -ceq 'input_shield_activation' -and $_.sequence -lt $Row.sequence}).Count -gt 0
    Assert-InputIsolation ($Row.shield_never_activated -eq (-not $activated)) 'shield activation/focus hidden by summary'
    if($present){
        $position=@($Rows|Where-Object {$_.type -ceq 'input_shield_position' -and $_.sequence -le $Row.sequence}|Select-Object -Last 1)
        $active=$position.Count -eq 1 -and $position[0].native_success
        Assert-InputIsolation ($Row.shield_active -eq $active -and (-not $active -or (Test-EndRectSame $Row.shield_positioning $position[0].actual_positioning))) 'shield snapshot has no current measured placement'
    }else{Assert-InputIsolation (-not $Row.shield_active) 'retired/not-created shield remained active'}
    $pass=$Row.cursor_available -and $sourceAvailable -and $sourceIdentity -and $Row.foreground_matches
    if($Row.shield_needed){$pass=$pass -and $null -ne $Shield -and $Row.shield_active -and $Row.shield_noactivate -and $Row.shield_topmost -and $Row.shield_visible -and -not $activated -and $shieldIdentity -and -not $overlap}
    $pointOwned=if($inside){$Row.root -eq $Owned[0].hwnd}else{$Row.shield_needed -and $null -ne $Row.shield_positioning -and (Test-IsolationPointInside $Row.destination $Row.shield_positioning) -and $null -ne $Shield -and $Row.root -eq $Shield.shield_hwnd}
    $pass=$pass -and $pointOwned -and $PriorHealthy
    Assert-InputIsolation ($Row.point_valid -eq $pass -and $Row.failure_class -ceq $(if($pass){'None'}else{'TestInputIsolationUnavailable'})) 'isolation verdict not independently recomputed'
    return [pscustomobject]@{Pass=$pass;InsideSource=$inside;PointOwned=$pointOwned;SourceIdentity=$sourceIdentity;ShieldIdentity=$shieldIdentity;SourcePositioning=$Row.source_positioning;ShieldPositioning=$Row.shield_positioning}
}

function Get-IsolationPlannedPoints($Path,$Current){
    Assert-AutoPoint $Current 'initial post-END cursor';$points=@();$points+=,@($Current)
    for($index=3;$index -le 20;++$index){$points+=,@(($Path.start[0]+($Path.end[0]-$Path.start[0])*$index/20),($Path.start[1]+($Path.end[1]-$Path.start[1])*$index/20))}
    return ,$points
}
function Test-IsolationRectContained($Outer,$Inner){
    if($null -eq $Outer -or $null -eq $Inner){return $false};$null=Get-AutoRect $Outer;$null=Get-AutoRect $Inner
    return $Inner[0] -ge $Outer[0] -and $Inner[1] -ge $Outer[1] -and $Inner[2] -le $Outer[2] -and $Inner[3] -le $Outer[3]
}
function Get-IsolationShieldPlan($Terminal,$Source,$Points){
    $minX=($Points|ForEach-Object {$_[0]}|Measure-Object -Minimum).Minimum;$maxX=($Points|ForEach-Object {$_[0]}|Measure-Object -Maximum).Maximum;$minY=($Points|ForEach-Object {$_[1]}|Measure-Object -Minimum).Minimum;$maxY=($Points|ForEach-Object {$_[1]}|Measure-Object -Maximum).Maximum
    $needed=@($Points|Where-Object {-not (Test-IsolationPointInside $_ $Source)}).Count -gt 0;$valid=$true;$rect=$null
    foreach($value in @($Terminal)+@($Source)+@($Points|ForEach-Object {$_[0];$_[1]})){if($value -lt -2147483648 -or $value -gt 2147483647){$valid=$false}}
    if($needed){$rect=@(($minX-50),([Math]::Max($Terminal[3],$Source[3])),($maxX+51),($maxY+51));foreach($n in $rect){if($n -lt -2147483648 -or $n -gt 2147483647){$valid=$false}};if($rect[0] -ge $rect[2] -or $rect[1] -ge $rect[3]){$valid=$false}else{if(Test-IsolationRectsOverlap $Source $rect){$valid=$false};foreach($point in $Points){if(-not (Test-IsolationPointInside $point $Source) -and -not (Test-IsolationPointInside $point $rect)){$valid=$false}}}}
    return [pscustomobject]@{Valid=$valid;Needed=$needed;Rect=$(if($valid -and $needed){$rect}else{$null});MinX=$minX;MaxX=$maxX;MinY=$minY;MaxY=$maxY}
}

function Test-IsolationShieldLifecycle($Rows,$Owned,$Startup,$Win,$Product){
    $created=@(Get-IsolationRows $Rows 'input_shield_created');$positions=@(Get-IsolationRows $Rows 'input_shield_position');$points=@(Get-IsolationRows $Rows 'input_isolation_point');$ready=@(Get-IsolationRows $Rows 'input_isolation_ready');$destroy=@(Get-IsolationRows $Rows 'input_shield_destroyed');$activation=@(Get-IsolationRows $Rows 'input_shield_activation');$path=@(Get-IsolationRows $Rows 'path');$plans=@(Get-IsolationRows $Rows 'input_isolation_setup_plan')
    Assert-InputIsolation ($created.Count -le 1 -and $ready.Count -le 1 -and $destroy.Count -le 1 -and $points.Count -le 19 -and $plans.Count -le 1) 'shield retry/duplicate lifecycle'
    $shield=if($created.Count){$created[0]}else{$null};$failure=$false;$setupPoints=@();$minX=0;$maxX=0;$minY=0;$maxY=0;$needed=$false
    if($Product.Count -eq 1 -and $path.Count -eq 1 -and $Product[0].Proof.cursor_success){
        $planned=Get-IsolationPlannedPoints $path[0] $Product[0].Proof.cursor
        $minX=($planned|ForEach-Object {$_[0]}|Measure-Object -Minimum).Minimum;$maxX=($planned|ForEach-Object {$_[0]}|Measure-Object -Maximum).Maximum;$minY=($planned|ForEach-Object {$_[1]}|Measure-Object -Minimum).Minimum;$maxY=($planned|ForEach-Object {$_[1]}|Measure-Object -Maximum).Maximum
        $needed=@($planned|Where-Object {-not (Test-IsolationPointInside $_ $Product[0].Proof.actual_positioning)}).Count -gt 0
    }else{$planned=@()}
    foreach($p in $plans){
        Assert-InputIsolation ($Product.Count -eq 1 -and $Product[0].Pass -and $planned.Count -eq 19 -and $p.run_nonce -eq $Startup.run_nonce -and $p.product_preflight_sequence -eq $Product[0].Sequence -and $p.sequence -gt $Product[0].Sequence -and (Test-AutoRect $p.work_area $Owned[0].work_area) -and (Test-EndPointExact $p.actual_current_cursor $Product[0].Proof.cursor)) 'setup plan is not fresh after independent product authority'
        $current=@(($path[0].start[0]+($path[0].end[0]-$path[0].start[0])*2/20),($path[0].start[1]+($path[0].end[1]-$path[0].start[1])*2/20))
        $matches=Test-AutoPoint $p.actual_current_cursor $current;$contained=@($planned|Where-Object {-not (Test-IsolationPointInside $_ $Owned[0].work_area)}).Count -eq 0;$plan=Get-IsolationShieldPlan $Product[0].Proof.terminal_positioning $Product[0].Proof.actual_positioning $planned;$planContained=$plan.Valid -and (-not $plan.Needed -or (Test-IsolationRectContained $Owned[0].work_area $plan.Rect));$dataReady=$matches -and $contained -and $planContained
        foreach($n in @('current_cursor_matches_plan','points_work_area_contained','shield_plan_valid','shield_plan_needed','shield_plan_work_area_contained','test_data_ready')){Assert-AutoBoolean (Get-AutoField $p $n) "setup plan $n"}
        Assert-InputIsolation ((Test-EndPointExact $p.planned_current_cursor $current) -and @($p.planned_points).Count -eq 19) 'setup plan original-anchor point count/current'
        for($index=0;$index -lt 19;++$index){Assert-InputIsolation (Test-EndPointExact $p.planned_points[$index] $planned[$index]) 'setup plan altered a remaining original trajectory point'}
        Assert-InputIsolation ($p.current_cursor_matches_plan -eq $matches -and $p.points_work_area_contained -eq $contained -and $p.shield_plan_valid -eq $plan.Valid -and $p.shield_plan_needed -eq $plan.Needed -and (Test-EndRectSame $p.shield_plan_positioning $plan.Rect) -and $p.shield_plan_work_area_contained -eq $planContained -and $p.test_data_ready -eq $dataReady -and $p.failure_class -ceq $(if($dataReady){'None'}else{'TestInputIsolationUnavailable'})) 'setup plan bounds/workarea/data gate not independently recomputed'
        if(-not $dataReady){$failure=$true;Assert-InputIsolation ($created.Count -eq 0 -and $positions.Count -eq 0 -and $ready.Count -eq 0 -and @(Get-IsolationRows $Rows 'writer_begin').Count -eq 0) 'failed bounded plan still created shield or wrote source'}
    }
    if($created.Count -or $points.Count -or $ready.Count){Assert-InputIsolation ($plans.Count -eq 1 -and $plans[0].test_data_ready) 'shield/test observations before bounded setup plan'}
    foreach($c in $created){
        foreach($n in @('shield_hwnd','shield_pid','shield_tid','run_nonce','actual_shield_pid','actual_shield_tid','actual_shield_nonce','nonce_error','create_tid','create_start_qpc','create_return_qpc','style','exstyle','parent','product_preflight_sequence','winevent_end_qpc')){Assert-AutoInteger (Get-AutoField $c $n) "shield create $n"}
        Assert-InputIsolation ($c.actor -ceq 'shield') 'shield create actor'
        $exact=Test-IsolationIdentity @($c.shield_hwnd,$c.actual_shield_pid,$c.actual_shield_tid,$c.actual_shield_nonce) @($c.shield_hwnd,$Startup.pid,$Startup.ui_tid,$Startup.run_nonce)
        if(-not $exact -or $c.run_nonce -ne $Startup.run_nonce -or $c.shield_pid -ne $Startup.pid -or $c.shield_tid -ne $Startup.ui_tid -or $c.create_tid -ne $Startup.ui_tid -or $c.style -ne 2147483648 -or $c.exstyle -ne 134217728 -or $c.parent -ne 0 -or $c.nonce_error -ne 0){$failure=$true}
        if($Product.Count -ne 1 -or -not $Product[0].Pass -or $null -eq $Win.End -or $c.product_preflight_sequence -ne $Product[0].Sequence -or $c.winevent_end_qpc -ne $Win.End.callback_qpc -or $c.create_start_qpc -le $Product[0].Qpc -or $c.create_start_qpc -le $Win.End.callback_qpc){$failure=$true}
        Assert-InputIsolation ($c.create_start_qpc -le $c.create_return_qpc -and $c.create_return_qpc -le $c.qpc) 'shield create clock'
        Assert-InputIsolation ($c.sequence -gt $plans[0].sequence -and (Test-AutoRect $c.create_positioning $plans[0].shield_plan_positioning) -and (Test-IsolationRectContained $Owned[0].work_area $c.create_positioning)) 'shield created outside its exact bounded nonoverlap plan'
    }
    foreach($a in $activation){Assert-InputIsolation ($null -ne $shield -and $a.shield_hwnd -eq $shield.shield_hwnd -and $a.run_nonce -eq $Startup.run_nonce -and $a.message -in @(6,7)) 'shield activation record identity';$failure=$true}
    foreach($p in $positions){
        foreach($n in @('source_operation_id','source_postverify_sequence','target','insert_after','flags','native_start_qpc','native_return_qpc','error','planned_min_x','planned_max_x','planned_min_y','planned_max_y','margin_px')){Assert-AutoInteger (Get-AutoField $p $n) "shield position $n"}
        foreach($n in @('native_success','source_unchanged','foreground_unchanged','top_margin_clipped')){Assert-AutoBoolean (Get-AutoField $p $n) "shield position $n"}
        Assert-InputIsolation ($null -ne $shield -and (Get-IsolationWriteActor $p.actor $p.target $Owned[0].hwnd $shield.shield_hwnd) -ceq 'shield' -and $p.sequence -gt $shield.sequence -and $p.native_start_qpc -ge $shield.create_return_qpc -and $p.native_start_qpc -le $p.native_return_qpc -and $p.native_return_qpc -le $p.qpc) 'shield position actor/native clock'
        $source=$p.source_positioning_before;$null=Get-AutoRect $source
        $expected=@(($minX-50),([Math]::Max($Product[0].Proof.terminal_positioning[3],$source[3])),($maxX+51),($maxY+51))
        Assert-InputIsolation ((Test-AutoRect $p.requested_positioning $expected) -and (Test-AutoRect $p.terminal_source_positioning $Product[0].Proof.terminal_positioning) -and $p.planned_min_x -eq $minX -and $p.planned_max_x -eq $maxX -and $p.planned_min_y -eq $minY -and $p.planned_max_y -eq $maxY -and $p.margin_px -eq 50 -and $p.top_margin_clipped -eq ($expected[1] -gt $minY-50)) 'shield frozen trajectory arithmetic/clip not independently proved'
        if($p.source_operation_id -eq 0){Assert-InputIsolation ($p.source_postverify_sequence -eq 0 -and (Test-AutoRect $source $Product[0].Proof.actual_positioning)) 'initial shield modified the terminal frame'}
        else{$result=@($Rows|Where-Object {$_.type -ceq 'writer_result' -and $_.operation_id -eq $p.source_operation_id -and $_.sequence -eq $p.source_postverify_sequence});Assert-InputIsolation ($result.Count -eq 1 -and $result[0].sequence -lt $p.sequence -and (Test-AutoRect $source $result[0].full_positioning)) 'dynamic shield not bound to the actual source postverify'}
        $unchanged=$null -ne $p.source_positioning_after -and $null -ne $p.source_visible_after -and (Test-EndRectSame $p.source_positioning_before $p.source_positioning_after) -and (Test-EndRectSame $p.source_visible_before $p.source_visible_after)
        Assert-InputIsolation ($p.source_unchanged -eq $unchanged -and $p.foreground_unchanged -eq ($p.foreground_before -eq $Owned[0].hwnd -and $p.foreground_after -eq $p.foreground_before)) 'shield source/foreground unchanged summary'
        $contained=(Test-IsolationRectContained $Owned[0].work_area $p.requested_positioning) -and (Test-IsolationRectContained $Owned[0].work_area $p.actual_positioning)
        Assert-AutoBoolean $p.work_area_contained 'position workarea';Assert-InputIsolation ((Test-AutoRect $p.work_area $Owned[0].work_area) -and $p.work_area_contained -eq $contained) 'shield workarea coverage forged'
        if(-not $contained){$failure=$true}
        if(-not $p.native_success -or $p.error -ne 0 -or $p.flags -ne 80 -or $p.insert_after -ne -1 -or $null -eq $p.actual_positioning -or -not (Test-AutoRect $p.actual_positioning $expected) -or -not $unchanged -or -not $p.foreground_unchanged -or (Test-IsolationRectsOverlap $expected $source)){$failure=$true}
        $proof=Test-IsolationPointProof $p $Rows $Owned $Startup $shield; if(-not $proof.Pass){$failure=$true}
    }
    foreach($p in $points){
        Assert-InputIsolation ($p.phase -ceq 'setup' -and $p.index -ge 2 -and $p.index -le 20 -and @($points|Where-Object index -eq $p.index).Count -eq 1 -and $planned.Count -eq 19 -and (Test-EndPointExact $p.destination $planned[$p.index-2])) 'setup point is not current + every planned remaining sample'
        $proof=Test-IsolationPointProof $p $Rows $Owned $Startup $shield;$setupPoints+=,$proof
        Assert-InputIsolation ($Product.Count -eq 1 -and $p.sequence -gt $Product[0].Sequence -and $p.qpc -gt $Product[0].Proof.winevent_end_qpc) 'setup hit test occurred before matching END/product proof'
        if(-not $proof.Pass){$failure=$true}
    }
    $pass=$false
    foreach($r in $ready){
        foreach($n in @('checked_count','run_nonce','product_preflight_sequence','end_qpc','winevent_end_qpc','isolation_ready_qpc')){Assert-AutoInteger (Get-AutoField $r $n) "isolation ready $n"};foreach($n in @('shield_needed','test_input_isolation')){Assert-AutoBoolean (Get-AutoField $r $n) "isolation ready $n"}
        $pass=-not $failure -and $Product.Count -eq 1 -and $Product[0].Pass -and $null -ne $Win.End -and $points.Count -eq 19 -and @($setupPoints|Where-Object Pass -eq $false).Count -eq 0 -and $r.checked_count -eq 19 -and $r.run_nonce -eq $Startup.run_nonce -and $r.product_preflight_sequence -eq $Product[0].Sequence -and $r.end_qpc -eq $Product[0].Proof.end_qpc -and $r.winevent_end_qpc -eq $Win.End.callback_qpc -and $r.isolation_ready_qpc -gt $Product[0].Qpc -and $r.isolation_ready_qpc -gt $Win.End.callback_qpc -and $r.isolation_ready_qpc -le $r.qpc -and $r.sequence -gt $points[-1].sequence -and $r.shield_needed -eq $needed -and $r.shield_state -ceq $(if($needed){'READY'}else{'NOT_NEEDED'}) -and $created.Count -eq [int]$needed
        Assert-InputIsolation ($r.test_input_isolation -eq $pass) 'readiness was not independently established by 19 actual point observations'
    }
    foreach($d in $destroy){
        foreach($n in @('shield_hwnd','shield_pid','shield_tid','run_nonce','actual_shield_pid','actual_shield_tid','actual_shield_nonce','source_acceptance_sequence','raw_up_receiver_qpc','destroy_start_qpc','destroy_return_qpc')){Assert-AutoInteger (Get-AutoField $d $n) "shield destroyed $n"};foreach($n in @('own_identity','destroy_success','window_absent')){Assert-AutoBoolean (Get-AutoField $d $n) "shield destroyed $n"}
        $exact=$null -ne $shield -and (Test-IsolationIdentity @($d.shield_hwnd,$d.actual_shield_pid,$d.actual_shield_tid,$d.actual_shield_nonce) @($shield.shield_hwnd,$Startup.pid,$Startup.ui_tid,$Startup.run_nonce))
        Assert-InputIsolation ($d.actor -ceq 'shield' -and $d.run_nonce -eq $Startup.run_nonce -and $d.own_identity -eq $exact -and $d.reason -cin @('acceptance','failure') -and $d.destroy_start_qpc -le $d.destroy_return_qpc -and $d.destroy_return_qpc -le $d.qpc) 'shield teardown identity/clock'
        if(-not $exact -or -not $d.destroy_success -or -not $d.window_absent){$failure=$true}
        if($d.reason -ceq 'acceptance'){$accepted=@($Rows|Where-Object {$_.type -ceq 'source_final_acceptance' -and $_.sequence -eq $d.source_acceptance_sequence});if($accepted.Count -ne 1 -or -not (Test-IsolationDestroyTiming $d.destroy_start_qpc $accepted[0].qpc $d.raw_up_receiver_qpc)){$failure=$true}}
        else{Assert-InputIsolation ($d.source_acceptance_sequence -eq 0 -and $d.raw_up_receiver_qpc -eq 0 -and $Rows[-1].result -ceq 'BLOCKED') 'failure cleanup falsely claimed source acceptance'}
    }
    if($created.Count -and $destroy.Count -ne 1){$failure=$true}
    if($created.Count){
        foreach($r in @(Get-IsolationRows $Rows 'writer_result')){if($r.postverify_exact){$p=@($positions|Where-Object source_operation_id -eq $r.operation_id);Assert-InputIsolation ($p.Count -eq 1 -and $p[0].sequence -gt $r.sequence -and $p[0].source_postverify_sequence -eq $r.sequence) 'successful source result omitted or repeated its mandatory dynamic shield placement'}}
    }
    return [pscustomobject]@{Pass=$pass;Failure=$failure;Ready=$(if($ready.Count){$ready[0]}else{$null});Shield=$shield;Destroy=$(if($destroy.Count){$destroy[0]}else{$null});Needed=$needed;ShieldNativeCalls=$positions.Count;Positions=$positions;PlannedPoints=$planned}
}

function Test-IsolationInputs($Rows,$Owned,$Guard,$Startup,$Bootstrap,$Win,$Isolation,[bool]$Blocked){
    $pre=@($Rows|Where-Object {$null -eq $Win.End -or $_.sequence -lt $Win.End.sequence})
    # This is an incomplete pre-END prefix, not a forged completed old verdict.
    $prefix=Test-TakeoverInputs $pre $Owned $Guard $Bootstrap $true
    $down=$prefix.PendingButton;$expected=$(if($Owned.Count){@($Owned[0].saved_cursor)}else{$null});[long]$previous=0
    $all=@(Get-IsolationRows $Rows 'input');foreach($request in $all){Assert-IsolationInputReceipt $request $Owned;if($request.sent -eq 0){Assert-InputIsolation $Blocked 'failed INPUT did not stop'};if($null -eq $Win.End -or $request.sequence -lt $Win.End.sequence){if($request.sent -eq 1 -and $request.flags -eq 1){$expected=@($request.point)};$previous=$request.sequence}}
    $failure=$false;$post=@($all|Where-Object {$null -ne $Win.End -and $_.sequence -gt $Win.End.sequence})
    foreach($request in $post){
        Assert-InputIsolation ($request.flags -in @(1,4) -and -not $request.restoring_cursor -and $Isolation.Pass -and $null -ne $Isolation.Ready -and $request.sequence -gt $Isolation.Ready.sequence) 'post-END input before isolation or unauthorized DOWN/restore'
        $fences=@($Rows|Where-Object {$_.type -ceq 'input_fence' -and $_.sequence -gt $previous -and $_.sequence -lt $request.sequence});Assert-InputIsolation ($fences.Count -gt 0) 'post-END input lacks current driver fence';$f=$fences[-1]
        Assert-InputIsolation ($f.foreground -eq $Owned[0].hwnd -and $f.target -eq $Owned[0].hwnd -and $f.gui_query_succeeded -and $f.capture_hwnd -eq 0 -and $f.menu_owner_hwnd -eq 0 -and $f.move_size_hwnd -eq 0 -and ($f.gui_flags -band 30) -eq 0 -and $f.left_down -eq $down -and (Test-AutoPoint $f.cursor $expected) -and (Test-AutoPoint $f.expected_cursor $expected)) 'post-END fresh current cursor/GUI/button fence'
        $proofs=@($Rows|Where-Object {$_.type -ceq 'synthetic_input_isolation' -and $_.input_scope -ceq 'gesture' -and $_.sequence -gt $previous -and $_.sequence -lt $request.sequence})
        Assert-InputIsolation ($proofs.Count -eq 1) 'post-END input lacks exactly one fresh destination observation';$p=$proofs[0]
        Assert-AutoBoolean $p.acceptance_eligible 'input eligibility';Assert-InputIsolation ($p.phase -ceq 'api_boundary' -and $p.flags -eq $request.flags -and $p.acceptance_eligible -and $p.input_isolation_ready_sequence -eq $Isolation.Ready.sequence -and $p.qpc -le $request.injection_start_qpc -and (Test-EndPointExact $p.destination $request.point)) 'input does not bind its actual flags/destination/fresh proof'
        $proof=Test-IsolationPointProof $p $Rows $Owned $Startup $Isolation.Shield
        $currentProof=@($Rows|Where-Object {$_.type -ceq 'writer_result' -and $_.sequence -lt $p.sequence}|Select-Object -Last 1)
        if($currentProof.Count){Assert-InputIsolation ((Test-AutoRect $p.source_positioning $currentProof[0].full_positioning)) 'input snapshot is not the last actually accepted source rect'}
        $currentInside=Test-IsolationPointInside $f.cursor $p.source_positioning;$expectedRoot=if($currentInside){$Owned[0].hwnd}elseif($null -ne $Isolation.Shield){$Isolation.Shield.shield_hwnd}else{0}
        Assert-InputIsolation ($expectedRoot -gt 0 -and $f.window_from_point_root -eq $expectedRoot -and $f.qpc -le $request.injection_start_qpc) 'held current cursor is not exact source/shield before input'
        if(-not $proof.Pass){$failure=$true;Assert-InputIsolation ($request.sent -eq 0) 'input was sent despite invalid isolation'}
        Assert-InputIsolation ($down -and ($request.flags -ne 4 -or $down)) 'post-END INPUT order'
        if($request.sent -eq 1){if($request.flags -eq 1){$expected=@($request.point)}else{$down=$false}}
        $previous=$request.sequence
    }
    foreach($p in @(Get-IsolationRows $Rows 'synthetic_input_isolation')){
        Assert-InputIsolation ($p.phase -ceq 'api_boundary' -and $p.flags -in @(1,4) -and $p.input_scope -cin @('gesture','cleanup')) 'unknown synthetic isolation scope'
        $proof=Test-IsolationPointProof $p $Rows $Owned $Startup $Isolation.Shield
        if(-not $proof.Pass){$failure=$true;Assert-InputIsolation (@($Rows|Where-Object {$_.type -ceq 'input' -and $_.injection_start_qpc -ge $p.qpc -and $_.sequence -gt $p.sequence}).Count -eq 0) 'continued input after failed fresh isolation'}
    }
    return [pscustomobject]@{PendingButton=$down;ExpectedCursor=$expected;Failure=$failure;PostInputs=$post}
}

function Test-IsolationGeometryReceipts($Rows,$Win,$Writers){
    $native=@(Get-IsolationRows $Rows 'EXIT');[long]$unowned=0
    if(-not $native.Count){return 0};$currentP=$native[0].positioning;$currentV=$native[0].visible
    foreach($p in @(Get-IsolationRows $Rows 'POSITION_CHANGED')){
        foreach($n in @('operation_id','position_receipt_qpc')){Assert-AutoInteger (Get-AutoField $p $n) "source position receipt $n"}
        Assert-InputIsolation ($p.position_receipt_qpc -le $p.qpc) 'native position receipt clock'
        if($null -eq $Win.End -or $p.position_receipt_qpc -le $Win.End.callback_qpc){Assert-InputIsolation ($p.operation_id -eq 0) 'source writer before matching END';continue}
        if($p.operation_id -gt 0){
            Assert-InputIsolation $Writers.Operations.ContainsKey([string]$p.operation_id) 'source callback claims an unknown source operation';$op=$Writers.Operations[[string]$p.operation_id]
            Assert-InputIsolation ($op.Begin.native_calls -eq 1 -and $p.qpc -ge $op.Begin.qpc -and $p.qpc -le $op.Result.qpc) 'source callback outside its source native call'
            if(-not (Test-AutoRect $p.positioning $op.Target.Positioning)){++$unowned};$currentP=$op.Target.Positioning;$currentV=$op.Target.Visible
        }else{
            $last=@($Rows|Where-Object {$_.type -ceq 'writer_result' -and $_.qpc -le $p.qpc}|Select-Object -Last 1);if($last.Count){$currentP=$last[0].positioning;$currentV=$last[0].visible}
            if(-not (Test-EndRectSame $p.positioning $currentP) -or -not (Test-EndRectSame $p.visible $currentV)){++$unowned}
        }
        if($p.unexpected_change){++$unowned}
    }
    return $unowned
}

function Test-IsolationAcceptance($Rows,$Owned,$Startup,$Native,$Win,$Product,$Isolation,$Writers,$InputState,$Raw){
    $end=@(Get-IsolationRows $Rows 'takeover_end');$pathDone=@(Get-IsolationRows $Rows 'path_complete');$accepted=@(Get-IsolationRows $Rows 'source_final_acceptance');$skip=@(Get-IsolationRows $Rows 'cursor_restore_skipped')
    foreach($a in @($end,$pathDone,$accepted,$skip)){Assert-InputIsolation ($a.Count -le 1) 'duplicate terminal lifecycle'}
    $failure=$false;$up=$null;$upValid=$false;$exact=$false;$finalLeft=$null;$gate='NOT_RUN'
    foreach($sample in @(Get-IsolationRows $Rows 'sample')){
        Assert-InputIsolation ($null -ne $Native.Path -and $sample.index -ge 1 -and $sample.index -le 20 -and @($Rows|Where-Object {$_.type -ceq 'sample' -and $_.index -eq $sample.index}).Count -eq 1) 'sample index/lifecycle'
        $planned=@(($Native.Path.start[0]+($Native.Path.end[0]-$Native.Path.start[0])*$sample.index/20),($Native.Path.start[1]+($Native.Path.end[1]-$Native.Path.start[1])*$sample.index/20))
        Assert-InputIsolation ((Test-AutoPoint $sample.cursor $planned) -and $sample.left_down -and $sample.post_cancel -eq ($sample.index -gt 2)) 'actual sample does not follow the fixed trajectory'
        if($sample.index -ge 3){$anchor=@(Get-IsolationRows $Rows 'intent_anchor')[0];$desired=Get-EndHandoffIntended $Startup.operation $anchor.start_positioning $anchor.start_visible $anchor.pointer_down $sample.cursor;if(-not (Test-AutoRect $sample.positioning $desired.Positioning) -or -not (Test-AutoRect $sample.visible $desired.Visible) -or -not (Test-AutoRect $sample.intended_positioning $desired.Positioning) -or -not (Test-AutoRect $sample.intended_visible $desired.Visible)){$failure=$true}}
    }
    foreach($t in $end){
        $packet=@($Raw|Where-Object {$_.receiver_sequence -eq $t.raw_up_receiver_sequence -and $_.receiver_qpc -eq $t.raw_up_receiver_qpc});$requests=@($InputState.PostInputs|Where-Object {$_.flags -eq 4 -and $_.sent -eq 1})
        $upValid=$packet.Count -eq 1 -and $requests.Count -eq 1 -and $packet[0].raw_scope -ceq 'gesture' -and $packet[0].acceptance_eligible -and (Test-TakeoverUpReceipt $packet[0] $requests[0] $true);if($packet.Count){$up=$packet[0]}
        $anchor=@(Get-IsolationRows $Rows 'intent_anchor');Assert-InputIsolation ($anchor.Count -eq 1 -and $Writers.Handoff -ceq 'PASS') 'terminal candidate without handoff/original anchor'
        $desired=Get-EndHandoffIntended $Startup.operation $anchor[0].start_positioning $anchor[0].start_visible $anchor[0].pointer_down $t.final_cursor
        $authority=Test-IsolationWriterAuthority $t $Owned $Startup $false -TerminalUp
        $exact=$authority -and $t.raw_up_seen -and (Test-AutoRect $t.positioning $desired.Positioning) -and (Test-AutoRect $t.visible $desired.Visible) -and (Test-AutoRect $t.intended_positioning $desired.Positioning) -and (Test-AutoRect $t.intended_visible $desired.Visible)
        Assert-InputIsolation ($t.exact -eq $exact -and $t.native_drag_after_end -eq $Native.AfterNativeEnd) 'terminal candidate summary falsified'
        if(-not $upValid -or -not $exact -or $t.pending_write -or $t.pending_motion -or $Writers.Remaining -lt 18 -or ($null -ne $up -and $Writers.LastNativeReturnQpc -ge $up.receiver_qpc)){$failure=$true}
        $gate=if($failure){'FAIL'}else{'NOT_RUN'}
    }
    foreach($a in $accepted){
        Assert-InputIsolation ($end.Count -eq 1 -and $pathDone.Count -eq 1 -and $a.takeover_end_sequence -eq $end[0].sequence -and $a.sequence -gt $end[0].sequence -and $a.sequence -gt $pathDone[0].sequence -and $a.raw_up_receiver_sequence -eq $end[0].raw_up_receiver_sequence -and $a.raw_up_receiver_qpc -eq $end[0].raw_up_receiver_qpc) 'final acceptance precedes candidate/path/fresh release'
        $authority=Test-IsolationWriterAuthority $a $Owned $Startup $false
        $anchor=@(Get-IsolationRows $Rows 'intent_anchor')[0];$desired=Get-EndHandoffIntended $Startup.operation $anchor.start_positioning $anchor.start_visible $anchor.pointer_down $a.final_cursor
        $fences=@($Rows|Where-Object {$_.type -ceq 'input_fence' -and $_.sequence -gt $end[0].sequence -and $_.sequence -lt $pathDone[0].sequence -and -not $_.left_down -and $_.foreground -eq $Owned[0].hwnd -and $_.gui_query_succeeded -and $_.capture_hwnd -eq 0 -and $_.menu_owner_hwnd -eq 0 -and $_.move_size_hwnd -eq 0 -and ($_.gui_flags -band 30) -eq 0 -and (Test-AutoPoint $_.cursor $a.final_cursor)})
        $finalLeft=$a.final_left_down
        $acceptExact=$authority -and -not $finalLeft -and $a.raw_up_seen -and (Test-AutoRect $a.positioning $desired.Positioning) -and (Test-AutoRect $a.visible $desired.Visible) -and (Test-AutoRect $a.intended_positioning $desired.Positioning) -and (Test-AutoRect $a.intended_visible $desired.Visible)
        Assert-InputIsolation ($a.exact -eq $acceptExact -and $pathDone[0].final_left_down -eq $false) 'final acceptance does not describe actual geometry/release'
        if(-not $acceptExact -or $a.pending_write -or $a.pending_motion -or $fences.Count -eq 0 -or -not (Test-AutoRect $pathDone[0].positioning $desired.Positioning) -or -not (Test-AutoRect $pathDone[0].visible $desired.Visible) -or $failure -or $Writers.Failure -or -not $InputState.ExpectedCursor -or -not (Test-AutoPoint $a.final_cursor $InputState.ExpectedCursor)){$failure=$true}
        $gate=if($failure){'FAIL'}else{'PASS'}
    }
    foreach($s in $skip){Assert-AutoBoolean $s.cursor_restored 'cursor restore skipped';Assert-AutoBoolean $s.synthetic_input_sent 'no restore input';Assert-InputIsolation ($accepted.Count -eq 1 -and $s.sequence -gt $accepted[0].sequence -and $s.reason -ceq 'strict_test_input_isolation' -and -not $s.cursor_restored -and -not $s.synthetic_input_sent -and (Test-AutoPoint $s.point $Owned[0].saved_cursor)) 'cursor restore skip falsely claims a restore or precedes acceptance'}
    if($Rows[-1].result -cne 'BLOCKED'){Assert-InputIsolation ($accepted.Count -eq 1 -and $skip.Count -eq 1 -and -not $Rows[-1].cursor_restored) 'completed v4 requires actual final acceptance and explicit no-restore policy'}
    return [pscustomobject]@{Gate=$gate;Failure=$failure;RawUp=$up;FinalLeftDown=$finalLeft;Accepted=$(if($accepted.Count){$accepted[0]}else{$null})}
}

function Test-IsolationCleanup($Rows,$Owned,$Guard,$Startup,$Win,$Isolation){
    $diag=@(Get-IsolationRows $Rows 'cleanup_input_diagnostic');$release=@(Get-IsolationRows $Rows 'cleanup_release');$rawProof=@(Get-IsolationRows $Rows 'cleanup_raw_up');$final=@(Get-IsolationRows $Rows 'cleanup_final');$finalProof=@(Get-IsolationRows $Rows 'cleanup_final_isolation');$skip=@(Get-IsolationRows $Rows 'cleanup_skipped_no_authority')
    Assert-InputIsolation ($diag.Count -le 2 -and $release.Count -le 1 -and $rawProof.Count -le 1 -and $final.Count -le 1 -and $finalProof.Count -le 1 -and $skip.Count -le 1) 'cleanup was retried or multiple UP sent'
    foreach($r in @($diag)+@($release)+@($rawProof)+@($final)+@($finalProof)+@($skip)){Assert-AutoBoolean $r.acceptance_eligible 'cleanup scope';Assert-InputIsolation (-not $r.acceptance_eligible) 'cleanup counted as gesture acceptance'}
    $facts=@()
    foreach($d in @($diag)+@($release)+@($final)){
        foreach($n in @('target','guard','source_pid','source_tid','actual_source_pid','actual_source_tid','actual_source_nonce','run_nonce','foreground_hwnd','foreground_pid','foreground_tid','gui_error','capture_hwnd','menu_owner_hwnd','move_size_hwnd','gui_flags','cursor_root','cursor_error')){Assert-AutoInteger (Get-AutoField $d $n) "cleanup $n"}
        foreach($n in @('test_down_owned','own_identity','desktop_ready','source_visible','foreground_matches','gui_query_succeeded','no_foreign_capture','menu_clear','move_size_clear','gui_mode_clear','cursor_success','cursor_root_owned_or_guard','left_down','buttons_modifiers_clear','foreign_capture_transferred','stop_requested','source_retired','fixture_scope_active')){Assert-AutoBoolean (Get-AutoField $d $n) "cleanup $n"}
        Assert-InputIsolation ($d.fixture_scope_active -eq (-not $d.stop_requested -and -not $d.source_retired)) 'cleanup fixture lifetime flags'
        Assert-InputIsolation ($Owned.Count -eq 1 -and $d.target -eq $Owned[0].hwnd -and $d.source_pid -eq $Startup.pid -and $d.source_tid -eq $Startup.ui_tid -and $d.run_nonce -eq $Startup.run_nonce -and $d.own_identity -eq ($d.actual_source_pid -eq $Startup.pid -and $d.actual_source_tid -eq $Startup.ui_tid -and $d.actual_source_nonce -eq $Startup.run_nonce)) 'cleanup exact source nonce/identity'
        $capture=$d.gui_query_succeeded -and $d.capture_hwnd -in @(0,$Owned[0].hwnd);$menu=$d.gui_query_succeeded -and $d.menu_owner_hwnd -eq 0 -and ($d.gui_flags -band 28) -eq 0;$move=$d.gui_query_succeeded -and $d.move_size_hwnd -eq 0;$mode=$d.gui_query_succeeded -and ($d.gui_flags -band 30) -eq 0
        $roots=@($Owned[0].hwnd);if($null -ne $Win.End -and $d.sequence -gt $Win.End.sequence){if($null -ne $Isolation.Shield){$roots+=,$Isolation.Shield.shield_hwnd}}elseif($Guard.Count){$roots+=,$Guard[0].hwnd}
        $root=$d.cursor_success -and $d.cursor_root -in $roots
        Assert-InputIsolation ($d.no_foreign_capture -eq $capture -and $d.menu_clear -eq $menu -and $d.move_size_clear -eq $move -and $d.gui_mode_clear -eq $mode -and $d.cursor_root_owned_or_guard -eq $root -and (-not $d.foreground_matches -or ($d.foreground_hwnd -eq $Owned[0].hwnd -and $d.foreground_pid -eq $Startup.pid -and $d.foreground_tid -eq $Startup.ui_tid))) 'cleanup GUI/root/foreground summaries'
        if($d.cursor_success){Assert-AutoPoint $d.cursor 'cleanup actual cursor'}else{Assert-InputIsolation ($null -eq $d.cursor) 'cleanup cursor without capture'}
        $reliable=$d.fixture_scope_active -and $d.own_identity -and $d.desktop_ready -and $d.source_visible -and $d.foreground_matches -and $capture -and $menu -and $move -and $mode -and -not $d.foreign_capture_transferred
        $eligible=$d.test_down_owned -and $reliable -and $root -and $d.left_down -and $d.buttons_modifiers_clear
        if($d.type -ceq 'cleanup_input_diagnostic'){
            Assert-InputIsolation ($d.phase -cin @('initial','api_boundary') -and @($diag|Where-Object phase -ceq $d.phase).Count -eq 1) 'cleanup diagnostic phase'
            if($d.phase -ceq 'api_boundary'){
                $initial=@($diag|Where-Object phase -ceq initial);Assert-InputIsolation ($initial.Count -eq 1 -and $initial[0].sequence -lt $d.sequence) 'cleanup boundary before initial'
                $same=$d.cursor_success -and $initial[0].cursor_success -and (Test-AutoPoint $d.cursor $initial[0].cursor)
                $proof=@($Rows|Where-Object {$_.type -ceq 'synthetic_input_isolation' -and $_.input_scope -ceq 'cleanup' -and $_.sequence -gt $initial[0].sequence -and $_.sequence -lt $d.sequence})
                $isolated=$false;if($proof.Count){Assert-InputIsolation ($proof.Count -eq 1 -and $proof[0].flags -eq 4 -and -not $proof[0].acceptance_eligible -and (Test-EndPointExact $proof[0].destination $d.cursor)) 'cleanup UP scope/destination';$isolated=(Test-IsolationPointProof $proof[0] $Rows $Owned $Startup $Isolation.Shield).Pass}
                Assert-InputIsolation ($d.cursor_matches_initial -eq $same) 'cleanup cursor changed between independent boundaries';$eligible=$eligible -and $same -and $isolated
            }
            $outcome=if($eligible){'ELIGIBLE'}elseif($d.phase -ceq 'initial' -and -not $d.left_down -and $reliable){'NOT_NEEDED'}else{'SKIPPED_NO_AUTHORITY'}
            Assert-InputIsolation ($d.eligible -eq $eligible -and $d.outcome -ceq $outcome) 'cleanup authority not independently established'
        }
        $facts+=[pscustomobject]@{Row=$d;Reliable=$reliable;Eligible=$eligible}
    }
    $observed=$false;$attempt=$release.Count -eq 1;$sent=$false
    if($attempt){
        $r=$release[0];$boundary=@($facts|Where-Object {$_.Row.type -ceq 'cleanup_input_diagnostic' -and $_.Row.phase -ceq 'api_boundary'})
        Assert-InputIsolation ($boundary.Count -eq 1 -and $boundary[0].Eligible -and $r.sequence -gt $boundary[0].Row.sequence -and $r.flags -eq 4 -and $r.input_tag -eq 0x50424D41 -and $r.sent -in @(0,1) -and $r.cleanup_up_attempted -and $r.cleanup_up_sent -eq ($r.sent -eq 1) -and $boundary[0].Row.qpc -le $r.injection_start_qpc -and $r.injection_start_qpc -le $r.injection_return_qpc -and $r.injection_return_qpc -le $r.qpc -and ($r.sent -ne 1 -or $r.error -eq 0)) 'cleanup API occurred without exact fresh safe proof';$sent=$r.sent -eq 1
    }
    if($rawProof.Count){$u=$rawProof[0];Assert-InputIsolation ($attempt -and $u.timeout_ms -eq 2000 -and $u.receiver_watermark -eq $release[0].receiver_watermark -and $u.injection_start_qpc -eq $release[0].injection_start_qpc -and $u.sequence -gt $release[0].sequence) 'cleanup raw proof reference';$packet=@($Rows|Where-Object {$_.type -ceq 'raw_input' -and $_.receiver_sequence -eq $u.receiver_sequence -and $_.receiver_qpc -eq $u.receiver_qpc});$observed=$packet.Count -eq 1 -and $u.wait_result -eq 0 -and $packet[0].raw_scope -ceq 'cleanup' -and -not $packet[0].acceptance_eligible -and (Test-TakeoverUpReceipt $packet[0] $release[0] $true);Assert-InputIsolation ($u.raw_up_observed -eq $observed) 'cleanup Raw UP inferred from injection return'}
    $status=if($Rows[-1].result -ceq 'BLOCKED'){'NOT_OBSERVED'}else{'NOT_NEEDED'};$left=$null
    if($diag.Count){Assert-InputIsolation ($final.Count -eq 1 -and $finalProof.Count -eq 1) 'cleanup missing final independent snapshots'}
    if($final.Count){
        $f=$final[0];$initial=@($facts|Where-Object {$_.Row.type -ceq 'cleanup_input_diagnostic' -and $_.Row.phase -ceq 'initial'});$ff=@($facts|Where-Object {$_.Row.type -ceq 'cleanup_final'});Assert-InputIsolation ($initial.Count -eq 1 -and $ff.Count -eq 1) 'cleanup final without initial'
        $fp=$finalProof[0];$samePoint=if($f.cursor_success){Test-EndPointExact $fp.destination $f.cursor}else{-not $fp.cursor_available};Assert-InputIsolation ($fp.phase -ceq 'final' -and $fp.input_scope -ceq 'cleanup' -and $fp.sequence -lt $f.sequence -and $samePoint) 'cleanup final isolation snapshot reference'
        $isolated=(Test-IsolationPointProof $fp $Rows $Owned $Startup $Isolation.Shield).Pass
        $released=$sent -and $observed -and -not $f.left_down -and $ff[0].Reliable -and $f.capture_hwnd -eq 0 -and $f.cursor_success -and $f.cursor_root_owned_or_guard -and $f.buttons_modifiers_clear -and $isolated
        $notNeeded=$initial[0].Reliable -and $ff[0].Reliable -and -not $initial[0].Row.left_down -and -not $f.left_down
        $status=if($attempt){if($released){'PASS'}else{'FAILED'}}elseif($notNeeded){'NOT_NEEDED'}else{'SKIPPED_NO_AUTHORITY'};$left=$f.left_down
        Assert-InputIsolation ($f.cleanup_input_release -ceq $status -and $f.cleanup_up_attempted -eq $attempt -and $f.cleanup_up_sent -eq $sent -and $f.raw_up_observed -eq $observed -and $f.left_button_high_bit -eq $f.left_down -and ($status -cne 'SKIPPED_NO_AUTHORITY' -or $skip.Count -eq 1)) 'cleanup final summary not independently proven'
    }
    foreach($t in @(Get-IsolationRows $Rows 'takeover_end')){if($rawProof.Count){Assert-InputIsolation ($t.raw_up_receiver_sequence -ne $rawProof[0].receiver_sequence -or $t.raw_up_receiver_qpc -ne $rawProof[0].receiver_qpc) 'cleanup UP used for gesture acceptance'}}
    return [pscustomobject]@{Status=$status;AcceptanceEligible=$false;CurrentLeftDown=$left;InputSent=$sent}
}

function Test-InputIsolationOwnedRecords([object[]]$Rows,[switch]$AllowSynthetic){
    Test-IsolationEnvelope $Rows;$s=$Rows[0];$last=$Rows[-1];$blocked=$last.result -ceq 'BLOCKED';$gesture=if($s.operation -ceq 'Move'){1}else{2}
    foreach($n in @('pid','ui_tid','qpc_frequency','takeover_geometry_writes','run_nonce','gesture_id')){Assert-AutoInteger (Get-AutoField $s $n) "startup $n"}
    foreach($n in @('human_input','real_explorer','sendinput_in_probe')){Assert-AutoBoolean (Get-AutoField $s $n) "startup $n"}
    Assert-InputIsolation ($s.operation -cin @('Move','BottomResize') -and $s.mode -ceq 'free_takeover' -and $s.foreground_contract -ceq 'verified_global_foreground_v2' -and $s.handoff_contract -ceq 'winevent_end_barrier_v1' -and $s.diagnostic_contract -ceq 'separated_authority_v1' -and $s.authority_contract -ceq 'product_gesture_v1' -and $s.input_isolation_contract -ceq 'post_end_shield_v1' -and $s.input_correlation -ceq 'actual_absolute_receipt_v1' -and $s.run_nonce -gt 0 -and $s.gesture_id -eq 1 -and $s.qpc_frequency -gt 0 -and $s.takeover_geometry_writes -eq 0 -and -not $s.human_input -and -not $s.real_explorer -and $s.sendinput_in_probe) 'not current fresh owned v4 split-authority contract'
    if((Get-AutoField $s 'synthetic_fixture') -eq $true){Assert-InputIsolation ([bool]$AllowSynthetic) 'synthetic fixture is not empirical evidence'}
    $allowed=@('startup','shutdown','desktop_gate','guard','owned','receiver','receiver_shutdown','receiver_error','show_window','foreground_attempt','foreground_ready','foreground_bootstrap','activation_event','activation_button','activation_visibility','activation_fence','activation_input','activation_release','input_fence','input','destination_fence','cleanup_input_diagnostic','cleanup_release','cleanup_raw_up','cleanup_final_isolation','cleanup_final','cleanup_skipped_no_authority','raw_preflight_begin','raw_preflight_outcome','raw_preflight_complete','raw_input','raw_cursor_status','raw_wait_begin','raw_wait_result','raw_motion_correlation','LEGACY_MOVE','native_button_down','path','sample','path_complete','ENTER','DRAG','EXIT','CAPTURE_CHANGED','POSITION_CHANGED','cancel_begin','cancel_return','end_wait','cancel_message','intent_anchor','handoff_begin','writer_begin','writer_result','raw_quantum','handoff_complete','takeover_end','source_final_acceptance','takeover_failure','blocked','winevent_hook_installed','winevent_hook_removed','winevent_callback','winevent_match','winevent_error','winevent_barrier_snapshot','winevent_end_wait','gesture_armed','product_handoff_preflight','handoff_lag_candidate','input_isolation_setup_plan','input_shield_created','input_shield_position','input_isolation_point','input_isolation_ready','synthetic_input_isolation','writer_input_isolation','input_shield_activation','input_shield_destroyed','cursor_restore_skipped')
    $allowed+=,'source_work_post_skipped'
    foreach($r in $Rows){Assert-InputIsolation ($r.type -cin $allowed -and $r.gesture -in @(0,$gesture)) 'unknown record or wrong fresh gesture'}
    foreach($r in @(Get-IsolationRows $Rows 'source_work_post_skipped')){
        foreach($n in @('message','source_hwnd','source_pid','source_tid','actual_source_pid','actual_source_tid','actual_source_nonce','run_nonce')){Assert-AutoInteger (Get-AutoField $r $n) "skipped work $n"};foreach($n in @('own_identity','stop_requested','source_retired')){Assert-AutoBoolean (Get-AutoField $r $n) "skipped work $n"}
        $exact=$r.source_hwnd -gt 0 -and $r.actual_source_pid -eq $s.pid -and $r.actual_source_tid -eq $s.ui_tid -and $r.actual_source_nonce -eq $s.run_nonce
        Assert-InputIsolation ($blocked -and $r.run_nonce -eq $s.run_nonce -and $r.source_pid -eq $s.pid -and $r.source_tid -eq $s.ui_tid -and $r.own_identity -eq $exact -and ($r.stop_requested -or $r.source_retired -or -not $exact)) 'skipped source work did not fail closed on lifetime/identity'
    }
    foreach($n in @('cursor_restored','owned_window_destroyed','guard_window_destroyed','receiver_stopped','external_windows_touched','input_shield_created','input_shield_destroyed','input_shield_activated')){Assert-AutoBoolean (Get-AutoField $last $n) "shutdown $n"}
    Assert-InputIsolation ($last.result -cin @('CAPTURED_NOT_ACCEPTED','BLOCKED') -and $last.owned_window_destroyed -and $last.guard_window_destroyed -and -not $last.external_windows_touched -and $last.run_nonce -eq $s.run_nonce -and -not $last.cursor_restored) 'unsafe resource cleanup or unauthorized cursor restore'
    $owned=@(Get-IsolationRows $Rows 'owned');$guard=@(Get-IsolationRows $Rows 'guard');$desktop=@(Get-IsolationRows $Rows 'desktop_gate');Assert-InputIsolation ($owned.Count -le 1 -and $guard.Count -le 1 -and $desktop.Count -le 1) 'duplicate initial identities'
    foreach($o in $owned){Assert-InputIsolation ($o.hwnd -gt 0 -and $o.pid -eq $s.pid -and $o.tid -eq $s.ui_tid -and $o.run_nonce -eq $s.run_nonce -and $o.actual_source_nonce -eq $s.run_nonce -and $o.dpi -gt 0 -and $o.monitor -gt 0) 'owned source generation/context';Assert-TakeoverGeometry $o;$null=Get-AutoRect $o.virtual_screen}
    foreach($g in $guard){Assert-InputIsolation ($g.hwnd -gt 0 -and $g.pid -eq $s.pid -and $g.tid -eq $s.ui_tid -and ($owned.Count -eq 0 -or $g.hwnd -ne $owned[0].hwnd)) 'guard exact PID/TID'}
    foreach($d in $desktop){Assert-InputIsolation ($d.active_unlocked -and $d.input_desktop_matches) 'desktop gate'}
    $bootstrap=Test-TakeoverGlobalForegroundV2 $Rows $owned $blocked
    $raw=Test-EndHandoffRawEvidence $Rows $s $owned $blocked
    $win=Test-IsolationWinEvents $Rows $owned $s $blocked
    $products=Test-IsolationProductPreflights $Rows $owned $s $win;$product=@($products|Where-Object Phase -ceq winevent_end)
    $isolation=Test-IsolationShieldLifecycle $Rows $owned $s $win $product
    $inputState=Test-IsolationInputs $Rows $owned $guard $s $bootstrap $win $isolation $blocked
    $native=Test-IsolationNativeLifecycle $Rows $owned $s $raw $bootstrap
    $writers=Test-IsolationSourceWriters $Rows $owned $s $win $product $isolation $raw.Packets
    $unowned=Test-IsolationGeometryReceipts $Rows $win $writers
    $acceptance=Test-IsolationAcceptance $Rows $owned $s $native $win $product $isolation $writers $inputState $raw.Packets
    $cleanup=Test-IsolationCleanup $Rows $owned $guard $s $win $isolation
    Test-EndDiagnosticAuxiliary $Rows $win $products
    foreach($p in @(Get-IsolationRows $Rows 'writer_input_isolation')){
        $proof=Test-IsolationPointProof $p $Rows $owned $s $isolation.Shield
        $b=@($Rows|Where-Object {$_.type -ceq 'writer_begin' -and $_.writer_input_isolation_sequence -eq $p.sequence})
        if($proof.Pass){Assert-InputIsolation ($b.Count -eq 1) 'passed writer test proof has no unique source operation'}else{$isolation.Failure=$true;Assert-InputIsolation ($b.Count -eq 0) 'source write followed failed owner-cursor test proof'}
    }
    $afterWin=if($null -ne $win.End){@(Get-IsolationRows $Rows 'DRAG'|Where-Object receipt_qpc -gt $win.End.callback_qpc).Count}else{0}
    $productPass=$product.Count -eq 1 -and $product[0].Pass
    $barrierCounterexample=$afterWin -gt 0 -or $unowned -gt 0 -or $writers.BarrierViolations -gt 0 -or ($product.Count -eq 1 -and (Get-EndDiagnosticDisposition $product[0].Checks ($null -ne $win.End)) -ceq 'FAIL')
    $architecture='UNRESOLVED';$result=if($raw.Gate -ceq 'FAIL'){'FAIL'}else{'BLOCKED'}
    if($barrierCounterexample){$result='FAIL';$architecture='REJECTED_AT_END_BARRIER'}
    elseif($writers.Failure -or $acceptance.Failure){$result='FAIL';$architecture='REJECTED_AT_TAKEOVER_WRITER'}
    elseif($isolation.Failure -or $inputState.Failure){$result='FAIL'}
    elseif($productPass -and $isolation.Pass -and $writers.Handoff -ceq 'PASS' -and $acceptance.Gate -ceq 'PASS' -and -not $blocked -and $win.Installed -and $win.Removed -and $raw.Gate -ceq 'PASS'){$result='PASS';$architecture='VALID_AT_OWNED_STAGE'}
    if($isolation.Shield -and $isolation.Destroy -and $isolation.Destroy.reason -ceq 'acceptance'){Assert-InputIsolation ($null -ne $acceptance.RawUp -and $isolation.Destroy.raw_up_receiver_qpc -eq $acceptance.RawUp.receiver_qpc) 'shield teardown does not bind actual accepted Raw UP'}
    Assert-InputIsolation ($last.takeover_geometry_writes -eq $writers.NativeCalls -and $last.input_shield_created -eq ($null -ne $isolation.Shield) -and $last.input_shield_destroyed -eq ($null -eq $isolation.Shield -or ($null -ne $isolation.Destroy -and $isolation.Destroy.destroy_success -and $isolation.Destroy.window_absent)) -and $last.input_shield_activated -eq (@(Get-IsolationRows $Rows 'input_shield_activation').Count -gt 0) -and $last.source_acceptance_sequence -eq $(if($null -ne $acceptance.Accepted){$acceptance.Accepted.sequence}else{0})) 'shutdown source/shield counters or acceptance references differ from actual records'
    $cancel=Get-EndHandoffBarrierGate -CancelCalls $native.CancelCalls -NativeEnter $native.RealEnter -NativeDrag $native.RealDrag -AuthorityValid ($owned.Count -eq 1 -and $bootstrap.result -ceq 'PASS') -CancelReturned $native.Returned -RealExit ($null -ne $native.Exit) -CaptureReleased $native.CaptureClear -LeftHeldAtExit $native.HeldExit -GuiClearAfterExit $productPass -RawHealthy $raw.Healthy -RawContinuation ($acceptance.Gate -ceq 'PASS' -and $writers.Remaining -ge 18) -NativeDragAfterReturn $native.AfterReturn -UnattributedGeometryAfterEnd $unowned -WritesBeforeEnd $writers.BarrierViolations
    $timings=[pscustomobject]@{NativeExitToWinEventEnd=@();WinEventEndToIsolationReady=@();IsolationReadyToHandoffWrite=@();WinEventEndToHandoffWrite=@();HandoffWriteDuration=@();RawReceiptToOwnerQuantum=@($writers.RawOwnerTicks);RawReceiptToNativeWrite=@($writers.RawWriteTicks)}
    if($null -ne $native.Exit -and $null -ne $win.End){$timings.NativeExitToWinEventEnd=@((Get-IsolationQpcDelta $win.End.callback_qpc $native.Exit.receipt_qpc 'native EXIT to WinEvent END'))}
    if($null -ne $isolation.Ready -and $null -ne $win.End){$timings.WinEventEndToIsolationReady=@((Get-IsolationQpcDelta $isolation.Ready.isolation_ready_qpc $win.End.callback_qpc 'WinEvent END to isolation ready'))}
    if($writers.HandoffStartQpc){$timings.IsolationReadyToHandoffWrite=@((Get-IsolationQpcDelta $writers.HandoffStartQpc $isolation.Ready.isolation_ready_qpc 'isolation ready to handoff write'));$timings.WinEventEndToHandoffWrite=@((Get-IsolationQpcDelta $writers.HandoffStartQpc $win.End.callback_qpc 'WinEvent END to handoff write'));$timings.HandoffWriteDuration=@([long]$writers.HandoffDuration)}
    $testGate=if($isolation.Failure -or $inputState.Failure){'FAIL'}elseif($isolation.Pass){'PASS'}else{'NOT_RUN'}
    $reasons=@(Get-IsolationRows $Rows 'blocked'|ForEach-Object reason);if($blocked){Assert-InputIsolation ($reasons.Count -gt 0) 'blocked result without reason'}
    return [pscustomobject]@{Result=$result;DiagnosedResult=$result;Operation=$s.operation;ForegroundContract=$s.foreground_contract;HandoffContract=$s.handoff_contract;DiagnosticContract=$s.diagnostic_contract;AuthorityContract=$s.authority_contract;IsolationContract=$s.input_isolation_contract;RunNonce=$s.run_nonce;ContractVerified=$true;ProductAuthoritySeparated='PASS';TestIsolationSeparated='PASS';ProductHandoffAuthority=$(if($productPass){'PASS'}elseif($product.Count){'FAIL'}else{'UNKNOWN'});TestInputIsolation=$testGate;PostEndInputShield=$(if($isolation.Failure){'FAIL'}elseif($isolation.Pass -and $isolation.Needed){'PASS'}elseif($isolation.Pass){'NOT_NEEDED'}else{'NOT_RUN'});WinEventWitness=$(if($null -ne $win.End){'PASS'}else{'UNKNOWN'});PostEndGeometryStability=$(if($barrierCounterexample){'FAIL'}elseif($productPass){'PASS'}else{'UNKNOWN'});RawBackground=$raw.Gate;Cancel=$cancel;Handoff=$writers.Handoff;Takeover=$acceptance.Gate;PreReleaseControl=$(if($acceptance.Gate -ceq 'PASS'){'PASS'}else{'NOT_RUN'});Architecture=$architecture;NativeWrites=$writers.NativeCalls;HandoffNativeCalls=$writers.HandoffCalls;ShieldNativeCalls=$isolation.ShieldNativeCalls;RawPackets=$raw.Packets.Count;RawMovementPackets=@($raw.Packets|Where-Object cursor_sampled).Count;RawUpPackets=@($raw.Packets|Where-Object {($_.button_flags -band 2) -ne 0}).Count;ContinuationQuanta=$writers.Remaining;NativeDragAfterWinEventEnd=$afterWin;UnownedGeometryAfterEnd=$unowned;TerminalSettlements=$native.Settlements;FullGestureTargets=$writers.FullTargets;CleanupInputRelease=$cleanup.Status;CleanupAcceptanceEligible=$false;PendingButton=($inputState.PendingButton -and -not $cleanup.InputSent);CleanupCurrentLeftDown=$cleanup.CurrentLeftDown;FinalLeftDown=$acceptance.FinalLeftDown;TimingTickSamples=$timings;QpcFrequency=$s.qpc_frequency;Reasons=$reasons;SyntheticFixture=((Get-AutoField $s 'synthetic_fixture') -eq $true)}
}

function Test-InputIsolationOwnedEvidence([string]$Path,[switch]$AllowSynthetic){
    $rows=@(Get-Content -LiteralPath $Path -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json -ErrorAction Stop})
    return Test-InputIsolationOwnedRecords $rows -AllowSynthetic:$AllowSynthetic
}
