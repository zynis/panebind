Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# Shared Fix B math/receipt utilities are read-only. Historical files and
# evidence stay v2; v3 contracts and diagnostics are independently checked here.
. (Join-Path $PSScriptRoot 'r1c4b-end-handoff-validation.ps1')

function Assert-EndDiagnostic([bool]$Value,[string]$Reason){if(-not $Value){throw "owned END diagnostic: $Reason"}}
function Get-EndDiagnosticRows($Rows,[string]$Type){return @($Rows|Where-Object type -ceq $Type)}
function Get-EndDiagnosticFailureChecks($Proof){
    # Ordered factual checks, not a relabeling of native failure_class.
    $checks=[Collections.Generic.List[string]]::new()
    if(-not $Proof.end_observed){$checks.Add('MissingNativeEnd')}
    if(-not $Proof.winevent_end_observed){$checks.Add('MissingWinEventEnd')}
    if(-not $Proof.own_identity){$checks.Add('IdentityChanged')}
    if(-not $Proof.desktop_ready){$checks.Add('DesktopUnavailable')}
    if(-not $Proof.source_visible){$checks.Add('SourceNotVisible')}
    if(-not $Proof.foreground_matches){$checks.Add('ForegroundChanged')}
    if(-not $Proof.gui_query_succeeded){$checks.Add('GuiQueryFailed')}
    if($Proof.gui_query_succeeded -and -not $Proof.capture_clear){$checks.Add('CaptureStillOwned')}
    if($Proof.gui_query_succeeded -and -not $Proof.menu_clear){$checks.Add('MenuStillActive')}
    if($Proof.gui_query_succeeded -and (-not $Proof.move_size_clear -or -not $Proof.gui_in_movesize_clear)){$checks.Add('MoveSizeStillActive')}
    if(-not $Proof.cursor_success){$checks.Add('CursorUnavailable')}
    elseif(-not $Proof.cursor_root_owned_or_guard){$checks.Add('CursorOutsideOwnedInputGuard')}
    if(-not $Proof.left_down){$checks.Add('ButtonReleasedBeforeHandoff')}
    if(-not $Proof.buttons_modifiers_clear){$checks.Add('InputInterference')}
    if($Proof.raw_up_seen){$checks.Add('RawUpAlreadyObserved')}
    if(-not $Proof.receiver_healthy){$checks.Add('ReceiverUnhealthy')}
    if($Proof.foreign_capture_transferred){$checks.Add('ForeignCaptureTransferred')}
    if($Proof.own_identity -and -not $Proof.dpi_matches){$checks.Add('DpiChanged')}
    if($Proof.own_identity -and -not $Proof.monitor_matches){$checks.Add('MonitorChanged')}
    if(-not $Proof.actual_positioning_available){$checks.Add('ActualPositioningUnavailable')}
    if(-not $Proof.actual_visible_available){$checks.Add('ActualVisibleUnavailable')}
    if(-not $Proof.terminal_positioning_available){$checks.Add('TerminalPositioningUnavailable')}
    if(-not $Proof.terminal_visible_available){$checks.Add('TerminalVisibleUnavailable')}
    if($Proof.actual_positioning_available -and $Proof.actual_visible_available -and
       $Proof.terminal_positioning_available -and $Proof.terminal_visible_available -and
       -not $Proof.terminal_positioning_exact -and -not $Proof.terminal_visible_exact){$checks.Add('BothTerminalGeometryMismatch')}
    else{
        if($Proof.actual_positioning_available -and $Proof.terminal_positioning_available -and -not $Proof.terminal_positioning_exact){$checks.Add('TerminalPositioningMismatch')}
        if($Proof.actual_visible_available -and $Proof.terminal_visible_available -and -not $Proof.terminal_visible_exact){$checks.Add('TerminalVisibleMismatch')}
    }
    if($Proof.native_drag_after_cancel_return -gt 0){$checks.Add('NativeDragAfterCancelReturn')}
    if($Proof.native_drag_after_end -gt 0 -or $Proof.native_drag_after_winevent_end -gt 0){$checks.Add('NativeDragAfterEnd')}
    if($Proof.unowned_geometry_changes -gt 0){$checks.Add('UnownedGeometryChange')}
    if(-not $Proof.takeover_healthy){$checks.Add('TakeoverAlreadyUnhealthy')}
    return $checks.ToArray()
}
function Get-EndDiagnosticDisposition($Checks,[bool]$MatchedEnd){
    if(@($Checks|Where-Object {$null -ne $_}).Count -eq 0){return 'PASS'}
    if($MatchedEnd -and @($Checks|Where-Object {$_ -cin @('TerminalPositioningMismatch','TerminalVisibleMismatch','BothTerminalGeometryMismatch','NativeDragAfterCancelReturn','NativeDragAfterEnd','UnownedGeometryChange')}).Count){return 'FAIL'}
    return 'BLOCKED'
}
function Test-EndDiagnosticTickFresh([long]$EventTick,[long]$ArmTick){
    Assert-EndDiagnostic ($EventTick -ge 0 -and $EventTick -le [UInt32]::MaxValue -and $ArmTick -ge 0 -and $ArmTick -le [UInt32]::MaxValue) 'DWORD event/arm tick'
    $elapsed=([decimal]$EventTick-[decimal]$ArmTick+[decimal]4294967296)%[decimal]4294967296
    return $elapsed -lt [decimal]2147483648
}
function Test-EndDiagnosticEnvelope($Rows){
    Assert-EndDiagnostic ($Rows.Count -ge 2) 'empty evidence'
    for($i=0;$i -lt $Rows.Count;++$i){
        $row=$Rows[$i];Assert-EndDiagnostic ($row.schema -ceq 'r1c4b-takeover-owned/v3') 'schema'
        foreach($name in @('sequence','gesture','qpc')){Assert-AutoInteger (Get-AutoField $row $name) "diagnostic $name"}
        Assert-EndDiagnostic ($row.sequence -eq $i+1 -and $row.gesture -ge 0 -and $row.qpc -ge 0 -and ($i -eq 0 -or $row.qpc -ge $Rows[$i-1].qpc)) 'sequence/gesture/QPC'
    }
    Assert-EndDiagnostic ($Rows[0].type -ceq 'startup' -and $Rows[-1].type -ceq 'shutdown' -and @(Get-EndDiagnosticRows $Rows 'startup').Count -eq 1 -and @(Get-EndDiagnosticRows $Rows 'shutdown').Count -eq 1) 'lifecycle envelope'
}

function Test-EndDiagnosticGuardPlan($Owned,$Guard,[bool]$Blocked){
    $names=@('margin_px','planned_move_bounds','planned_bottom_resize_bounds','planned_bounds','move_trajectory_with_margin_covered','bottom_resize_trajectory_with_margin_covered')
    foreach($g in $Guard){
        $present=@($names|Where-Object {$null -ne $g.PSObject.Properties[$_]})
        if(-not $present.Count){continue} # Historical v3 logs remain unchanged.
        Assert-EndDiagnostic ($present.Count -eq $names.Count) 'partial optional guard plan'
        Assert-AutoInteger $g.margin_px 'guard margin';Assert-EndDiagnostic ($g.margin_px -eq 50) 'guard margin must remain the authorized 50px'
        foreach($name in @('planned_move_bounds','planned_bottom_resize_bounds','planned_bounds')){$null=Get-AutoRect (Get-AutoField $g $name);foreach($n in (Get-AutoField $g $name)){Assert-AutoInteger $n 'guard plan rectangle'}}
        foreach($name in @('positioning_available','move_trajectory_with_margin_covered','bottom_resize_trajectory_with_margin_covered')){Assert-AutoBoolean (Get-AutoField $g $name) "guard coverage $name"}
        Assert-EndDiagnostic ($g.positioning_available -eq ($null -ne $g.positioning)) 'guard actual rectangle availability'
        if($g.positioning_available){$null=Get-AutoRect $g.positioning;foreach($n in $g.positioning){Assert-AutoInteger $n 'guard actual rectangle'}}
        # The guard is created before the source. A blocked creation prefix
        # cannot supply an owned initial frame for independent plan arithmetic.
        if($Owned.Count -eq 0){Assert-EndDiagnostic $Blocked 'guard plan without owned initial geometry';continue}
        Assert-EndDiagnostic ($Owned.Count -eq 1) 'ambiguous owned guard-plan anchor'
        $p=$Owned[0].positioning;$null=Get-AutoRect $p
        $move=@([long]$p[0],[long]$p[1],([long]$p[2]+180),[long]$p[3]);$resize=@([long]$p[0],[long]$p[1],[long]$p[2],([long]$p[3]+120));$combined=@([long]$p[0],[long]$p[1],([long]$p[2]+180),([long]$p[3]+120))
        Assert-EndDiagnostic ((Test-AutoRect $g.planned_move_bounds $move) -and (Test-AutoRect $g.planned_bottom_resize_bounds $resize) -and (Test-AutoRect $g.planned_bounds $combined)) 'guard plan was not derived from the original owned frame and fixed trajectory'
        $actual=$g.positioning;$coverage=@()
        foreach($plan in @($move,$resize)){$coverage+=($g.positioning_available -and $actual[0] -le $plan[0]-50 -and $actual[1] -le $plan[1]-50 -and $actual[2] -ge $plan[2]+50 -and $actual[3] -ge $plan[3]+50)}
        Assert-EndDiagnostic ($g.move_trajectory_with_margin_covered -eq $coverage[0] -and $g.bottom_resize_trajectory_with_margin_covered -eq $coverage[1]) 'guard coverage flags do not match the actual rectangle'
    }
}

function Test-EndDiagnosticWinEvents($Rows,$Owned,$Startup,[bool]$Blocked){
    $installed=@(Get-EndDiagnosticRows $Rows 'winevent_hook_installed');$removed=@(Get-EndDiagnosticRows $Rows 'winevent_hook_removed');$callbacks=@(Get-EndDiagnosticRows $Rows 'winevent_callback');$matches=@(Get-EndDiagnosticRows $Rows 'winevent_match')
    Assert-EndDiagnostic ($installed.Count -le 1 -and $removed.Count -le 1) 'duplicate hook lifecycle'
    foreach($h in $installed){
        foreach($name in @('hook','source_hwnd','source_pid','source_tid','install_tid','event_min','event_max','flags','dll')){Assert-AutoInteger (Get-AutoField $h $name) "hook installed $name"}
        Assert-AutoBoolean $h.install_success 'hook installed native result'
        Assert-AutoBoolean $h.skip_own_process 'hook skip own process';Assert-AutoBoolean $h.skip_own_thread 'hook skip own thread';Assert-AutoInteger $h.error 'hook install error'
        Assert-EndDiagnostic ($Owned.Count -eq 1 -and $h.source_hwnd -eq $Owned[0].hwnd -and $h.source_pid -eq $Startup.pid -and $h.source_tid -eq $Startup.ui_tid -and $h.install_tid -eq $Startup.ui_tid -and $h.event_min -eq 10 -and $h.event_max -eq 11 -and $h.flags -eq 0 -and $h.dll -eq 0 -and $h.install_success -eq ($h.hook -gt 0)) 'not exact-source OUTOFCONTEXT no-DLL hook'
        Assert-EndDiagnostic (-not $h.skip_own_process -and -not $h.skip_own_thread -and (-not $h.install_success -or $h.error -eq 0)) 'hook own-observation or successful-error mismatch'
        if($h.install_success){Assert-EndDiagnostic ($removed.Count -eq 1) 'installed hook leaked'}else{Assert-EndDiagnostic ($Blocked -and $callbacks.Count -eq 0 -and $removed.Count -eq 0) 'failed hook was used'}
    }
    foreach($h in $removed){
        foreach($name in @('hook','source_hwnd','source_pid','source_tid','remove_tid')){Assert-AutoInteger (Get-AutoField $h $name) "hook removed $name"}
        Assert-AutoBoolean $h.remove_success 'hook removal result'
        Assert-AutoInteger $h.error 'hook removal error';if($h.remove_success){Assert-EndDiagnostic ($h.error -eq 0) 'successful unhook native error'}
        Assert-EndDiagnostic ($installed.Count -eq 1 -and $installed[0].install_success -and $h.hook -eq $installed[0].hook -and $h.source_hwnd -eq $installed[0].source_hwnd -and $h.source_pid -eq $Startup.pid -and $h.source_tid -eq $Startup.ui_tid -and $h.remove_tid -eq $installed[0].install_tid -and $h.sequence -gt $installed[0].sequence) 'hook removed on wrong identity/thread/order'
        if(-not $h.remove_success){Assert-EndDiagnostic $Blocked 'unhook failure silently accepted'}
    }
    if(-not $installed.Count){Assert-EndDiagnostic ($Blocked -and $callbacks.Count -eq 0 -and $matches.Count -eq 0 -and $removed.Count -eq 0) 'callback without its hook'}
    $start=$null;$finish=$null;$serial=0;$facts=@();$path=@(Get-EndDiagnosticRows $Rows 'path');$down=@(Get-EndDiagnosticRows $Rows 'native_button_down');$armedRows=@(Get-EndDiagnosticRows $Rows 'gesture_armed')
    Assert-EndDiagnostic ($armedRows.Count -le 1) 'multiple gesture arms'
    foreach($a in $armedRows){foreach($name in @('gesture_id','arm_qpc','arm_tick','source_hwnd','source_pid','source_tid')){Assert-AutoInteger (Get-AutoField $a $name) "gesture arm $name"};Assert-EndDiagnostic ($a.gesture_id -eq $Startup.gesture_id -and $a.source_hwnd -eq $Owned[0].hwnd -and $a.source_pid -eq $Startup.pid -and $a.source_tid -eq $Startup.ui_tid -and $a.arm_qpc -le $a.qpc -and $a.arm_qpc -gt 0) 'gesture arm binding'}
    foreach($c in $callbacks){
        foreach($name in @('hook','event','hwnd','event_thread','event_time','object_id','child_id','callback_tid','callback_sequence','callback_qpc','gesture_id','arm_qpc','arm_tick')){Assert-AutoInteger (Get-AutoField $c $name) "WinEvent callback $name"}
        Assert-EndDiagnostic ($installed.Count -eq 1 -and $installed[0].install_success -and $c.callback_sequence -eq ++$serial -and $c.callback_qpc -le $c.qpc -and $c.sequence -gt $installed[0].sequence -and ($removed.Count -eq 0 -or $c.sequence -lt $removed[0].sequence)) 'callback sequence/lifecycle/QPC'
        $match=@($matches|Where-Object callback_sequence -eq $c.callback_sequence)
        Assert-EndDiagnostic ($match.Count -eq 1 -and $match[0].sequence -gt $c.sequence) 'callback lacks one match decision'
        $m=$match[0];foreach($name in @('callback_record_sequence','gesture_id','matched_start_callback_sequence','actual_source_pid','actual_source_tid')){Assert-AutoInteger (Get-AutoField $m $name) "match $name"};Assert-AutoBoolean $m.accepted 'match native acceptance';Assert-AutoBoolean $m.source_identity 'match source identity'
        $identity=$m.actual_source_pid -eq $Startup.pid -and $m.actual_source_tid -eq $Startup.ui_tid
        Assert-EndDiagnostic ($m.callback_record_sequence -eq $c.sequence -and $m.gesture_id -eq $Startup.gesture_id -and $m.source_identity -eq $identity) 'match identity/reference forged'
        $reason='None';$accepted=$false
        if($c.hook -ne $installed[0].hook){$reason='WrongHook'}elseif($c.hwnd -ne $Owned[0].hwnd){$reason='WrongWindow'}elseif($c.event_thread -ne $Startup.ui_tid){$reason='WrongEventThread'}elseif($c.callback_tid -ne $installed[0].install_tid){$reason='WrongCallbackThread'}elseif(-not $identity){$reason='SourceIdentityChanged'}
        elseif($armedRows.Count -ne 1 -or $c.gesture_id -eq 0 -or $c.gesture_id -ne $Startup.gesture_id -or $c.arm_qpc -ne $armedRows[0].arm_qpc -or $c.arm_tick -ne $armedRows[0].arm_tick){$reason='WrongGesture'}
        elseif($c.callback_qpc -le $c.arm_qpc -or $down.Count -ne 1 -or $down[0].sequence -gt $m.sequence){$reason='CallbackBeforeGesture'}elseif(-not (Test-EndDiagnosticTickFresh $c.event_time $c.arm_tick)){$reason='EventBeforeGesture'}
        elseif($c.event -eq 10){if($null -ne $start){$reason='StartAlreadyObserved'}else{$accepted=$true;$start=$c}}
        elseif($c.event -eq 11){if($null -eq $start){$reason='EndWithoutStart'}elseif($null -ne $finish){$reason='EndAlreadyObserved'}elseif($c.callback_sequence -le $start.callback_sequence -or $c.callback_qpc -le $start.callback_qpc -or -not (Test-EndDiagnosticTickFresh $c.event_time $start.event_time)){$reason='EventBeforeStart'}else{$accepted=$true;$finish=$c}}
        else{$reason='InvalidEvent'}
        # A rejected callback can never be promoted by the preflight summary.
        Assert-EndDiagnostic ($m.accepted -eq $accepted -and $m.reason -ceq $reason) 'forged WinEvent matching summary'
        $expectedStart=if($null -ne $start){$start.callback_sequence}else{0}
        Assert-EndDiagnostic ($m.matched_start_callback_sequence -eq $expectedStart) 'WinEvent END references a different gesture START'
        $facts+=[pscustomobject]@{CallbackSequence=$c.callback_sequence;Sequence=$c.sequence;Event=$c.event;Accepted=$accepted;CallbackQpc=$c.callback_qpc}
    }
    Assert-EndDiagnostic ($matches.Count -eq $callbacks.Count) 'orphan WinEvent match'
    return [pscustomobject]@{Installed=($installed.Count -eq 1 -and $installed[0].install_success);Removed=($removed.Count -eq 1 -and $removed[0].remove_success);Start=$start;End=$finish;Facts=$facts}
}

function Test-EndDiagnosticPreflights($Rows,$Owned,$Guard,$Startup,$WinEvent){
    $diagnostics=@(Get-EndDiagnosticRows $Rows 'handoff_preflight');$facts=@()
    Assert-EndDiagnostic ($diagnostics.Count -le 2) 'preflight timer retry or duplicate phase'
    $bools=@('end_observed','winevent_end_observed','own_identity','desktop_ready','source_visible','foreground_matches','foreground_snapshot_stable','gui_query_succeeded','capture_clear','menu_clear','move_size_clear','gui_in_movesize_clear','cursor_success','cursor_root_owned_or_guard','buttons_modifiers_clear','left_down','raw_up_seen','receiver_healthy','takeover_healthy','foreign_capture_transferred','dpi_matches','monitor_matches','actual_positioning_available','actual_visible_available','terminal_positioning_available','terminal_visible_available','terminal_positioning_exact','terminal_visible_exact','winevent_healthy','log_healthy','write_authorized','takeover_scope','handoff_ready','cleanup_observation')
    $ints=@('target','guard','source_pid','source_tid','gesture_id','actual_source_pid','actual_source_tid','end_sequence','end_qpc','winevent_end_sequence','winevent_end_qpc','foreground_hwnd','foreground_pid','foreground_tid','capture_hwnd','menu_owner_hwnd','move_size_hwnd','gui_flags','gui_error','cursor_root','cursor_error','dpi','frozen_dpi','monitor','frozen_monitor','positioning_error','visible_hresult','native_drag_after_cancel_return','native_drag_after_end','native_drag_after_winevent_end','unowned_geometry_changes')
    foreach($d in $diagnostics){
        Assert-EndDiagnostic ($Owned.Count -eq 1 -and $Guard.Count -eq 1 -and $d.phase -cin @('native_end','winevent_end') -and @($diagnostics|Where-Object phase -ceq $d.phase).Count -eq 1) 'preflight phase/identity'
        foreach($name in $bools){Assert-AutoBoolean (Get-AutoField $d $name) "preflight $name"}
        foreach($name in $ints){Assert-AutoInteger (Get-AutoField $d $name) "preflight $name"}
        foreach($name in @('native_drag_after_cancel_return','native_drag_after_end','native_drag_after_winevent_end','unowned_geometry_changes')){Assert-EndDiagnostic ($d.$name -ge 0) 'negative diagnostic counter'}
        if($d.cursor_success){Assert-AutoPoint $d.cursor 'preflight actual cursor diagnostic'}else{Assert-EndDiagnostic ($null -eq $d.cursor) 'cursor without successful capture'}
        Assert-EndDiagnostic ($d.target -eq $Owned[0].hwnd -and $d.guard -eq $Guard[0].hwnd -and $d.source_pid -eq $Startup.pid -and $d.source_tid -eq $Startup.ui_tid -and $d.gesture_id -eq $Startup.gesture_id -and $d.own_identity -eq ($d.actual_source_pid -eq $Startup.pid -and $d.actual_source_tid -eq $Startup.ui_tid) -and $d.frozen_dpi -eq $Owned[0].dpi -and $d.frozen_monitor -eq $Owned[0].monitor) 'preflight source/frozen binding'
        $native=@($Rows|Where-Object {$_.type -ceq 'EXIT' -and $_.sequence -lt $d.sequence})
        Assert-EndDiagnostic ($native.Count -le 1) 'ambiguous native END'
        $hasNative=$native.Count -eq 1;$hasWin=$null -ne $WinEvent.End -and $WinEvent.End.sequence -lt $d.sequence
        Assert-EndDiagnostic ($d.end_observed -eq $hasNative -and $d.winevent_end_observed -eq $hasWin) 'unproved native/WinEvent barrier bits'
        Assert-EndDiagnostic ($d.end_sequence -eq $(if($hasNative){$native[0].sequence}else{0}) -and $d.end_qpc -eq $(if($hasNative){$native[0].receipt_qpc}else{0}) -and $d.winevent_end_sequence -eq $(if($hasWin){$WinEvent.End.sequence}else{0}) -and $d.winevent_end_qpc -eq $(if($hasWin){$WinEvent.End.callback_qpc}else{0})) 'preflight references wrong barrier receipt'
        $foreground=$d.foreground_snapshot_stable -and $d.foreground_hwnd -eq $Owned[0].hwnd -and $d.foreground_pid -eq $Startup.pid -and $d.foreground_tid -eq $Startup.ui_tid
        $capture=$d.gui_query_succeeded -and $d.capture_hwnd -eq 0;$menu=$d.gui_query_succeeded -and $d.menu_owner_hwnd -eq 0 -and ($d.gui_flags -band 28) -eq 0;$move=$d.gui_query_succeeded -and $d.move_size_hwnd -eq 0;$mode=$d.gui_query_succeeded -and ($d.gui_flags -band 2) -eq 0
        $root=$d.cursor_success -and $d.cursor_root -in @($Owned[0].hwnd,$Guard[0].hwnd)
        Assert-EndDiagnostic ($d.foreground_matches -eq $foreground -and $d.capture_clear -eq $capture -and $d.menu_clear -eq $menu -and $d.move_size_clear -eq $move -and $d.gui_in_movesize_clear -eq $mode -and $d.cursor_root_owned_or_guard -eq $root -and $d.dpi_matches -eq ($d.dpi -eq $Owned[0].dpi) -and $d.monitor_matches -eq ($d.monitor -eq $Owned[0].monitor)) 'forged scalar authority derivation'
        foreach($kind in @('positioning','visible')){
            $actual=Get-AutoField $d ('actual_'+$kind);$terminal=Get-AutoField $d ('terminal_'+$kind)
            $actualAvailable=$null -ne $actual;$terminalAvailable=$hasNative -and $null -ne (Get-AutoField $native[0] $kind)
            if($actualAvailable){$null=Get-AutoRect $actual;foreach($n in $actual){Assert-AutoInteger $n 'diagnostic rectangle'}}
            if($null -ne $terminal){$null=Get-AutoRect $terminal;foreach($n in $terminal){Assert-AutoInteger $n 'diagnostic terminal rectangle'}}
            Assert-EndDiagnostic ((Get-AutoField $d ('actual_'+$kind+'_available')) -eq $actualAvailable -and (Get-AutoField $d ('terminal_'+$kind+'_available')) -eq $terminalAvailable -and (Test-EndRectSame $terminal $(if($terminalAvailable){Get-AutoField $native[0] $kind}else{$null}))) 'terminal/actual availability or baseline falsified'
            $exact=$actualAvailable -and $terminalAvailable -and (Test-AutoRect $actual $terminal)
            Assert-EndDiagnostic ((Get-AutoField $d ('terminal_'+$kind+'_exact')) -eq $exact) 'forged diagnostic terminal exact'
        }
        $drag=@($Rows|Where-Object {$_.type -ceq 'DRAG' -and $_.qpc -le $d.qpc});$ret=@($Rows|Where-Object {$_.type -ceq 'cancel_return' -and $_.sequence -lt $d.sequence})
        foreach($g in $drag){Assert-AutoInteger $g.receipt_qpc 'native DRAG receipt';Assert-EndDiagnostic ($g.receipt_qpc -le $g.qpc) 'native DRAG receipt time'}
        $afterReturn=if($ret.Count){@($drag|Where-Object receipt_qpc -gt $ret[0].returned_qpc).Count}else{0};$afterNative=if($hasNative){@($drag|Where-Object receipt_qpc -gt $native[0].receipt_qpc).Count}else{0};$afterWin=if($hasWin){@($drag|Where-Object receipt_qpc -gt $WinEvent.End.callback_qpc).Count}else{0}
        Assert-EndDiagnostic ($d.native_drag_after_cancel_return -eq $afterReturn -and $d.native_drag_after_end -eq $afterNative -and $d.native_drag_after_winevent_end -eq $afterWin) 'native drag diagnostic not independently counted'
        $unowned=0
        if($hasWin -and $hasNative){
            $anchor=@(Get-EndDiagnosticRows $Rows 'intent_anchor');$previousP=if($anchor.Count){$anchor[0].start_positioning}else{$Owned[0].positioning};$previousV=if($anchor.Count){$anchor[0].start_visible}else{$Owned[0].visible}
            foreach($p in @($Rows|Where-Object {$_.type -ceq 'POSITION_CHANGED' -and $_.qpc -le $d.qpc -and $_.operation_id -eq 0})){
                Assert-AutoInteger $p.position_receipt_qpc 'native position receipt';Assert-EndDiagnostic ($p.position_receipt_qpc -le $p.qpc) 'native position receipt QPC'
                if($null -eq $p.positioning -or $null -eq $p.visible){continue}
                if($p.position_receipt_qpc -gt $WinEvent.End.callback_qpc -and (-not (Test-EndRectSame $p.positioning $previousP) -or -not (Test-EndRectSame $p.visible $previousV))){++$unowned}
                $previousP=$p.positioning;$previousV=$p.visible
            }
        }
        Assert-EndDiagnostic ($d.unowned_geometry_changes -eq $unowned) 'unowned geometry diagnostic not independently counted'
        $checks=@(Get-EndDiagnosticFailureChecks $d);$first=if($checks.Count){$checks[0]}else{'None'}
        Assert-EndDiagnostic (@($d.failed_checks).Count -eq $checks.Count -and ($d.failed_checks -join ',') -ceq ($checks -join ',') -and $d.failure_class -ceq $first) 'native failure_class/failed_checks not independently derived'
        Assert-EndDiagnostic ($d.write_authorized -eq ($d.phase -ceq 'winevent_end' -and $checks.Count -eq 0 -and $d.takeover_scope -and -not $d.handoff_ready -and -not $d.cleanup_observation)) 'native-only, retired, cleanup or failed diagnostic authorized a write'
        $facts+=[pscustomobject]@{Phase=$d.phase;Sequence=$d.sequence;Qpc=$d.qpc;FailureClass=$first;FailedChecks=$checks;Disposition=(Get-EndDiagnosticDisposition $checks $hasWin);UniqueGuardFailure=($checks.Count -eq 1 -and $first -ceq 'CursorOutsideOwnedInputGuard');Proof=$d}
    }
    return ,$facts
}

function Test-EndDiagnosticCleanup($Rows,$Owned,$Guard,$Startup){
    $diag=@(Get-EndDiagnosticRows $Rows 'cleanup_input_diagnostic');$release=@(Get-EndDiagnosticRows $Rows 'cleanup_release');$rawProof=@(Get-EndDiagnosticRows $Rows 'cleanup_raw_up');$final=@(Get-EndDiagnosticRows $Rows 'cleanup_final');$skip=@(Get-EndDiagnosticRows $Rows 'cleanup_skipped_no_authority')
    Assert-EndDiagnostic ($diag.Count -le 2 -and $release.Count -le 1 -and $rawProof.Count -le 1 -and $final.Count -le 1 -and $skip.Count -le 1) 'cleanup retry/multiple input'
    foreach($row in @($diag)+@($release)+@($rawProof)+@($final)+@($skip)){Assert-AutoBoolean $row.acceptance_eligible 'cleanup acceptance eligibility';Assert-EndDiagnostic (-not $row.acceptance_eligible) 'cleanup relabeled as gesture acceptance'}
    $aliases=@();$eligibleFacts=@()
    foreach($d in @($diag)+@($final)+@($release)){
        foreach($name in @('target','guard','source_pid','source_tid','actual_source_pid','actual_source_tid','foreground_hwnd','foreground_pid','foreground_tid','gui_error','capture_hwnd','menu_owner_hwnd','move_size_hwnd','gui_flags','cursor_error','cursor_root')){Assert-AutoInteger (Get-AutoField $d $name) "cleanup $name"}
        foreach($name in @('test_down_owned','own_identity','desktop_ready','source_visible','foreground_matches','gui_query_succeeded','no_foreign_capture','menu_clear','move_size_clear','gui_mode_clear','cursor_success','cursor_root_owned_or_guard','left_down','buttons_modifiers_clear','foreign_capture_transferred')){Assert-AutoBoolean (Get-AutoField $d $name) "cleanup $name"}
        Assert-EndDiagnostic ($Owned.Count -eq 1 -and $Guard.Count -eq 1 -and $d.target -eq $Owned[0].hwnd -and $d.guard -eq $Guard[0].hwnd -and $d.source_pid -eq $Startup.pid -and $d.source_tid -eq $Startup.ui_tid -and $d.own_identity -eq ($d.actual_source_pid -eq $Startup.pid -and $d.actual_source_tid -eq $Startup.ui_tid)) 'cleanup bound identity'
        if($d.cursor_success){Assert-AutoPoint $d.cursor 'cleanup cursor'}else{Assert-EndDiagnostic ($null -eq $d.cursor) 'cleanup cursor without capture'}
        $foreground=$d.foreground_hwnd -eq $Owned[0].hwnd -and $d.foreground_pid -eq $Startup.pid -and $d.foreground_tid -eq $Startup.ui_tid
        # Snapshot stability is represented by native foreground_matches;
        # a true summary still requires the exact independently bound tuple.
        Assert-EndDiagnostic (-not $d.foreground_matches -or $foreground) 'cleanup forged global foreground tuple'
        $capture=$d.gui_query_succeeded -and $d.capture_hwnd -in @(0,$Owned[0].hwnd);$menu=$d.gui_query_succeeded -and $d.menu_owner_hwnd -eq 0 -and ($d.gui_flags -band 28) -eq 0;$move=$d.gui_query_succeeded -and $d.move_size_hwnd -eq 0;$mode=$d.gui_query_succeeded -and ($d.gui_flags -band 30) -eq 0;$root=$d.cursor_success -and $d.cursor_root -in @($Owned[0].hwnd,$Guard[0].hwnd)
        Assert-EndDiagnostic ($d.no_foreign_capture -eq $capture -and $d.menu_clear -eq $menu -and $d.move_size_clear -eq $move -and $d.gui_mode_clear -eq $mode -and $d.cursor_root_owned_or_guard -eq $root) 'cleanup GUI/root derivation forged'
        $eligible=$d.test_down_owned -and $d.own_identity -and $d.desktop_ready -and $d.source_visible -and $d.foreground_matches -and $d.gui_query_succeeded -and $capture -and $menu -and $move -and $mode -and $root -and $d.left_down -and $d.buttons_modifiers_clear -and -not $d.foreign_capture_transferred
        if($d.type -ceq 'cleanup_release'){Assert-EndDiagnostic $eligible 'cleanup release lost its stated authority'}
        if($d.type -ceq 'cleanup_input_diagnostic'){
            Assert-EndDiagnostic ($d.phase -cin @('initial','api_boundary') -and @($diag|Where-Object phase -ceq $d.phase).Count -eq 1) 'cleanup diagnostic phase'
            if($d.phase -ceq 'api_boundary'){
                $initial=@($diag|Where-Object phase -ceq initial);Assert-EndDiagnostic ($initial.Count -eq 1 -and $initial[0].sequence -lt $d.sequence) 'cleanup boundary before initial'
                $same=$d.cursor_success -and $initial[0].cursor_success -and (Test-AutoPoint $d.cursor $initial[0].cursor)
                Assert-AutoBoolean $d.cursor_matches_initial 'cleanup cursor boundary';Assert-EndDiagnostic ($d.cursor_matches_initial -eq $same) 'cleanup cursor summary forged';$eligible=$eligible -and $same
            }
            Assert-AutoBoolean $d.eligible 'cleanup eligible';Assert-EndDiagnostic ($d.eligible -eq $eligible) 'cleanup eligibility not independently proven'
            $reliable=$d.own_identity -and $d.desktop_ready -and $d.source_visible -and $d.foreground_matches -and $d.gui_query_succeeded -and $capture -and $menu -and $move -and $mode -and -not $d.foreign_capture_transferred
            $outcome=if($eligible){'ELIGIBLE'}elseif($d.phase -ceq 'initial' -and -not $d.left_down -and $reliable){'NOT_NEEDED'}else{'SKIPPED_NO_AUTHORITY'}
            Assert-EndDiagnostic ($d.outcome -ceq $outcome) 'cleanup initial/boundary outcome not factual'
            $eligibleFacts+=[pscustomobject]@{Phase=$d.phase;Eligible=$eligible;Proof=$d}
            if($d.phase -ceq 'api_boundary'){
                $alias=($d|ConvertTo-Json -Depth 12 -Compress|ConvertFrom-Json);$alias.type='cleanup_fence'
                foreach($pair in @(@('foreground',$d.foreground_hwnd),@('visible',$d.source_visible),@('modifiers_clear',$d.buttons_modifiers_clear),@('root',$d.cursor_root))){$alias|Add-Member -NotePropertyName $pair[0] -NotePropertyValue $pair[1] -Force};$aliases+=,$alias
            }
        }
    }
    if($release.Count){
        $r=$release[0];$boundary=@($eligibleFacts|Where-Object Phase -ceq api_boundary)
        foreach($name in @('sent','flags','error','input_tag','receiver_watermark','injection_start_qpc','injection_return_qpc')){Assert-AutoInteger (Get-AutoField $r $name) "cleanup release $name"}
        foreach($name in @('cleanup_up_attempted','cleanup_up_sent')){Assert-AutoBoolean (Get-AutoField $r $name) "cleanup release $name"}
        Assert-EndDiagnostic ($boundary.Count -eq 1 -and $boundary[0].Eligible -and $r.sequence -gt $boundary[0].Proof.sequence -and $boundary[0].Proof.qpc -le $r.injection_start_qpc -and $r.injection_start_qpc -le $r.injection_return_qpc -and $r.injection_return_qpc -le $r.qpc -and $r.flags -eq 4 -and $r.input_tag -eq 0x50424D41 -and $r.sent -in @(0,1) -and $r.cleanup_up_attempted -and $r.cleanup_up_sent -eq ($r.sent -eq 1) -and ($r.sent -ne 1 -or $r.error -eq 0)) 'cleanup input without one fresh safe authority boundary'
    }
    $observed=$false;$cleanupSerials=@()
    if($rawProof.Count){
        $u=$rawProof[0];Assert-EndDiagnostic ($release.Count -eq 1 -and $u.sequence -gt $release[0].sequence -and $u.timeout_ms -eq 2000 -and $u.receiver_watermark -eq $release[0].receiver_watermark -and $u.injection_start_qpc -eq $release[0].injection_start_qpc) 'cleanup Raw UP lifecycle/reference'
        Assert-AutoBoolean $u.raw_up_observed 'cleanup real Raw UP'
        $packet=@($Rows|Where-Object {$_.type -ceq 'raw_input' -and $_.receiver_sequence -eq $u.receiver_sequence -and $_.receiver_qpc -eq $u.receiver_qpc})
        foreach($name in @('wait_result','timeout_ms','receiver_sequence','receiver_qpc','receiver_watermark','injection_start_qpc')){Assert-AutoInteger (Get-AutoField $u $name) "cleanup Raw UP $name"}
        $observed=$packet.Count -eq 1 -and $u.wait_result -eq 0 -and $packet[0].raw_scope -ceq 'cleanup' -and -not $packet[0].acceptance_eligible -and (Test-TakeoverUpReceipt $packet[0] $release[0] $true)
        Assert-EndDiagnostic ($u.raw_up_observed -eq $observed) 'cleanup UP inferred from SendInput success'
        if($observed){$cleanupSerials+=,$u.receiver_sequence}
    }
    if($release.Count){
        foreach($p in @($Rows|Where-Object {$_.type -ceq 'raw_input' -and $_.receiver_sequence -gt $release[0].receiver_watermark -and $_.receiver_qpc -ge $release[0].injection_start_qpc -and ($_.button_flags -band 2) -ne 0})){$cleanupSerials+=,$p.receiver_sequence}
    }
    foreach($t in @(Get-EndDiagnosticRows $Rows 'takeover_end')){Assert-EndDiagnostic ($t.raw_up_receiver_sequence -notin $cleanupSerials) 'cleanup UP was used for gesture END acceptance'}
    $status=if($Rows[-1].result -ceq 'BLOCKED'){'NOT_OBSERVED'}else{'NOT_NEEDED'}
    if($diag.Count -or $release.Count -or $rawProof.Count){Assert-EndDiagnostic ($final.Count -eq 1) 'cleanup lacks final independent snapshot'}
    if($final.Count){
        $f=$final[0];$initial=@($diag|Where-Object phase -ceq initial);Assert-EndDiagnostic ($initial.Count -eq 1 -and $f.sequence -gt $initial[0].sequence) 'cleanup final without initial'
        foreach($name in @('cleanup_up_attempted','cleanup_up_sent','raw_up_observed','left_button_high_bit')){Assert-AutoBoolean (Get-AutoField $f $name) "cleanup final $name"}
        $attempt=$release.Count -eq 1;$sent=$attempt -and $release[0].sent -eq 1;$needed=$initial[0].left_down
        $released=$sent -and $observed -and -not $f.left_down -and $f.own_identity -and $f.desktop_ready -and $f.source_visible -and $f.foreground_matches -and $f.gui_query_succeeded -and $f.capture_hwnd -eq 0 -and $f.menu_clear -and $f.move_size_clear -and $f.gui_mode_clear -and $f.cursor_success -and $f.cursor_root_owned_or_guard -and $f.buttons_modifiers_clear -and -not $f.foreign_capture_transferred
        $reliable=$true
        foreach($p in @($initial[0],$f)){$reliable=$reliable -and $p.own_identity -and $p.desktop_ready -and $p.source_visible -and $p.foreground_matches -and $p.gui_query_succeeded -and $p.no_foreign_capture -and $p.menu_clear -and $p.move_size_clear -and $p.gui_mode_clear -and -not $p.foreign_capture_transferred}
        $notNeeded=$reliable -and -not $initial[0].left_down -and -not $f.left_down
        $status=if($attempt){if($released){'PASS'}else{'FAILED'}}elseif($notNeeded){'NOT_NEEDED'}else{'SKIPPED_NO_AUTHORITY'}
        Assert-EndDiagnostic (($status -cne 'SKIPPED_NO_AUTHORITY') -or $skip.Count -eq 1) 'no-authority cleanup lacks explicit skipped evidence'
        $sameCursor=if($f.cursor_success){Test-EndPointExact $f.final_cursor $f.cursor}else{$null -eq $f.final_cursor}
        Assert-EndDiagnostic ($f.cleanup_input_release -ceq $status -and $f.cleanup_up_attempted -eq $attempt -and $f.cleanup_up_sent -eq $sent -and $f.raw_up_observed -eq $observed -and $f.left_button_high_bit -eq $f.left_down -and $sameCursor) 'cleanup final summary forged'
        Assert-EndDiagnostic ((-not $f.gui_query_succeeded -and $null -eq $f.final_capture_hwnd) -or ($f.gui_query_succeeded -and $f.final_capture_hwnd -eq $f.capture_hwnd)) 'cleanup capture summary forged'
    }
    return [pscustomobject]@{Status=$status;Aliases=$aliases;Serials=@($cleanupSerials|Select-Object -Unique);Final=$(if($final.Count){$final[0]}else{$null});AcceptanceEligible=$false}
}

function Test-EndDiagnosticAuxiliary($Rows,$WinEvent,$Diagnostics){
    $waits=@(Get-EndDiagnosticRows $Rows 'winevent_end_wait')
    Assert-EndDiagnostic ($waits.Count -le 1) 'public END observation retried'
    foreach($w in $waits){
        foreach($name in @('started_qpc','finished_qpc','timeout_ms','wait_result')){Assert-AutoInteger (Get-AutoField $w $name) "public END wait $name"}
        foreach($name in @('winevent_end_observed','winevent_healthy')){Assert-AutoBoolean (Get-AutoField $w $name) "public END wait $name"}
        $seen=$null -ne $WinEvent.End -and $WinEvent.End.callback_qpc -le $w.finished_qpc
        Assert-EndDiagnostic ($w.started_qpc -le $w.finished_qpc -and $w.finished_qpc -le $w.qpc -and $w.timeout_ms -eq 3000 -and $w.wait_result -in @(0,258,4294967295) -and $w.winevent_end_observed -eq $seen) 'public END wait is not its actual bounded observation'
    }
    foreach($r in @(Get-EndDiagnosticRows $Rows 'winevent_barrier_snapshot')){
        foreach($name in @('winevent_end_sequence','winevent_end_qpc','native_drag_after_winevent_end','unowned_geometry_changes','positioning_error','visible_hresult')){Assert-AutoInteger (Get-AutoField $r $name) "barrier snapshot $name"}
        $seen=$null -ne $WinEvent.End -and $WinEvent.End.sequence -lt $r.sequence
        Assert-EndDiagnostic ($r.winevent_end_sequence -eq $(if($seen){$WinEvent.End.sequence}else{0}) -and $r.winevent_end_qpc -eq $(if($seen){$WinEvent.End.callback_qpc}else{0}) -and $r.native_drag_after_winevent_end -ge 0 -and $r.unowned_geometry_changes -ge 0) 'barrier snapshot receipt/counters'
        foreach($name in @('positioning','visible')){if($null -ne (Get-AutoField $r $name)){$null=Get-AutoRect (Get-AutoField $r $name)}}
    }
    foreach($r in @(Get-EndDiagnosticRows $Rows 'handoff_lag_candidate')){
        Assert-AutoInteger $r.preflight_sequence 'visible lag diagnostic reference'
        $d=@($Diagnostics|Where-Object Sequence -eq $r.preflight_sequence)
        Assert-EndDiagnostic ($d.Count -eq 1 -and $d[0].Sequence -lt $r.sequence -and $d[0].Phase -ceq $r.phase -and $r.reason -ceq 'DWM_VISIBLE_TERMINAL_LAG_CANDIDATE' -and $d[0].Proof.terminal_positioning_exact -and $d[0].Proof.actual_visible_available -and $d[0].Proof.terminal_visible_available -and -not $d[0].Proof.terminal_visible_exact) 'lag candidate is not a proved positioning-exact visible mismatch'
    }
    foreach($r in @(Get-EndDiagnosticRows $Rows 'winevent_error')){Assert-EndDiagnostic ($r.reason -ceq 'winevent_ingress_unhealthy') 'unknown public event ingress error'}
    $armed=@(Get-EndDiagnosticRows $Rows 'gesture_armed');$cleanup=@(Get-EndDiagnosticRows $Rows 'cleanup_input_diagnostic')
    foreach($p in @(Get-EndDiagnosticRows $Rows 'raw_input')){
        Assert-AutoBoolean $p.acceptance_eligible 'Raw acceptance scope';Assert-AutoBoolean $p.cleanup_observation 'Raw cleanup scope';Assert-AutoInteger $p.gesture_id 'Raw gesture epoch'
        Assert-EndDiagnostic ($p.raw_scope -cin @('gesture','cleanup') -and ($p.raw_scope -ceq 'cleanup') -eq $p.cleanup_observation -and (-not $p.acceptance_eligible -or ($p.raw_scope -ceq 'gesture' -and $p.gesture_id -eq 1 -and $armed.Count -eq 1))) 'Raw scope promoted preflight, cleanup or stale epoch into acceptance'
        # Parent ingress can label a queued child packet during the initial
        # snapshot capture, before its diagnostic row is emitted. Preserve its
        # serial, but only actual API/QPC-correlated cleanup UP can prove release.
        if($p.raw_scope -ceq 'cleanup'){Assert-EndDiagnostic (-not $p.acceptance_eligible -and ($cleanup.Count -gt 0 -or @(Get-EndDiagnosticRows $Rows 'cleanup_skipped_no_authority').Count -gt 0)) 'cleanup packet has no cleanup observation epoch'}
    }
    foreach($q in @(Get-EndDiagnosticRows $Rows 'raw_quantum')){
        $packets=@($Rows|Where-Object {$_.type -ceq 'raw_input' -and $_.receiver_sequence -ge $q.raw_first_sequence -and $_.receiver_sequence -le $q.raw_last_sequence})
        Assert-EndDiagnostic ($packets.Count -gt 0 -and @($packets|Where-Object {$_.raw_scope -cne 'gesture' -or -not $_.acceptance_eligible -or ($_.button_flags -band 2) -ne 0}).Count -eq 0) 'raw quantum used cleanup, retired scope or UP packet'
    }
    foreach($t in @(Get-EndDiagnosticRows $Rows 'takeover_end')){
        $packet=@($Rows|Where-Object {$_.type -ceq 'raw_input' -and $_.receiver_sequence -eq $t.raw_up_receiver_sequence -and $_.receiver_qpc -eq $t.raw_up_receiver_qpc})
        Assert-EndDiagnostic ($packet.Count -eq 1 -and $packet[0].raw_scope -ceq 'gesture' -and $packet[0].acceptance_eligible) 'gesture completion used a cleanup or retired UP'
    }
}

function Convert-EndDiagnosticCommonView($Rows,$WinEvent,$Cleanup){
    # Explicit in-memory projection of unchanged common primitives. No native
    # receipt, rectangle, Raw packet/serial or input outcome is manufactured.
    $new=@('winevent_hook_installed','winevent_hook_removed','winevent_callback','winevent_match','winevent_error','winevent_barrier_snapshot','winevent_end_wait','gesture_armed','handoff_preflight','handoff_lag_candidate','cleanup_input_diagnostic','cleanup_raw_up','cleanup_final','cleanup_skipped_no_authority')
    $view=[Collections.Generic.List[object]]::new();$mapping=@{}
    $native=@(Get-EndDiagnosticRows $Rows 'EXIT');$ordinaryDown=@($Rows|Where-Object {$_.type -ceq 'input' -and $_.flags -eq 2 -and $_.sent -eq 1})
    foreach($original in $Rows){
        $alias=@($Cleanup.Aliases|Where-Object sequence -eq $original.sequence)
        if($alias.Count){$row=$alias[0]}elseif($original.type -cin $new){continue}else{$row=$original}
        if($row.type -ceq 'cleanup_release' -and $ordinaryDown.Count -eq 0){continue}
        if($row.type -ceq 'POSITION_CHANGED' -and $null -ne $WinEvent.End -and $native.Count -eq 1 -and $row.qpc -gt $native[0].receipt_qpc -and $row.qpc -le $WinEvent.End.callback_qpc){continue}
        $row=$row|ConvertTo-Json -Depth 20 -Compress|ConvertFrom-Json
        $mapping[[string]$original.sequence]=$view.Count+1;$row.sequence=$view.Count+1;$row.schema='r1c4b-takeover-owned/v2';$view.Add($row)
    }
    foreach($row in $view){
        foreach($field in @('native_enter_sequence','native_down_sequence','end_sequence')){
            $value=Get-AutoField $row $field
            if($null -ne $value -and $value -gt 0){Assert-EndDiagnostic $mapping.ContainsKey([string]$value) 'projection lost original native receipt reference';$row.$field=$mapping[[string]$value]}
        }
    }
    $view[0].handoff_contract='end_barrier_v1'
    return ,@($view.ToArray())
}

function Test-EndDiagnosticsOwnedRecords([object[]]$Rows,[switch]$AllowSynthetic){
    Test-EndDiagnosticEnvelope $Rows
    $s=$Rows[0];$blocked=$Rows[-1].result -ceq 'BLOCKED'
    Assert-EndDiagnostic ($s.handoff_contract -ceq 'winevent_end_barrier_v1' -and $s.diagnostic_contract -ceq 'structured_handoff_v1' -and $s.foreground_contract -ceq 'verified_global_foreground_v2' -and $s.gesture_id -eq 1 -and $s.operation -cin @('Move','BottomResize') -and $s.mode -ceq 'free_takeover') 'v3 contract/single gesture scope'
    Assert-AutoInteger $s.gesture_id 'startup gesture id'
    if((Get-AutoField $s 'synthetic_fixture') -eq $true){Assert-EndDiagnostic ([bool]$AllowSynthetic) 'synthetic evidence is not empirical acceptance'}
    $owned=@(Get-EndDiagnosticRows $Rows 'owned');$guard=@(Get-EndDiagnosticRows $Rows 'guard')
    Test-EndDiagnosticGuardPlan $owned $guard $blocked
    $win=Test-EndDiagnosticWinEvents $Rows $owned $s $blocked
    $diagnostics=Test-EndDiagnosticPreflights $Rows $owned $guard $s $win
    $cleanup=Test-EndDiagnosticCleanup $Rows $owned $guard $s
    Test-EndDiagnosticAuxiliary $Rows $win $diagnostics
    $view=Convert-EndDiagnosticCommonView $Rows $win $cleanup
    $unavailable=@($view|Where-Object {$_.type -cin @('ENTER','EXIT') -and ($null -eq $_.positioning -or $null -eq $_.visible)})
    if($unavailable.Count){
        Assert-EndDiagnostic ($blocked -and @(Get-EndDiagnosticRows $Rows 'writer_begin').Count -eq 0) 'capture-unavailable native barrier was used to write'
        $boot=Test-TakeoverGlobalForegroundV2 $view $owned $blocked;$inputState=Test-EndHandoffInputEvidence $view $owned $guard $boot $blocked;$raw=Test-EndHandoffRawEvidence $view $s $owned $blocked
        $base=[pscustomobject]@{Result='BLOCKED';RawBackground=$raw.Gate;Cancel='UNKNOWN';Handoff='NOT_RUN';Takeover='NOT_RUN';PreReleaseControl='NOT_RUN';Architecture='UNRESOLVED';NativeWrites=0;HandoffNativeCalls=0;RawPackets=$raw.Packets.Count;RawMovementPackets=@($raw.Packets|Where-Object cursor_sampled).Count;RawUpPackets=@($raw.Packets|Where-Object {($_.button_flags -band 2) -ne 0}).Count;ContinuationQuanta=0;FullGestureTargets=$false;PendingButton=$inputState.PendingButton;Reasons=@(Get-EndDiagnosticRows $Rows 'blocked'|ForEach-Object reason)}
    }else{$base=Test-EndHandoffOwnedRecords $view -AllowSynthetic:$AllowSynthetic}
    $initial=@($diagnostics|Where-Object Phase -ceq native_end);$final=@($diagnostics|Where-Object Phase -ceq winevent_end)
    $begin=@(Get-EndDiagnosticRows $Rows 'writer_begin');$results=@(Get-EndDiagnosticRows $Rows 'writer_result');$first=@($results|Where-Object native_calls -eq 1|Sort-Object native_start_qpc|Select-Object -First 1)
    $firstQpc=if($first.Count){$first[0].native_start_qpc}else{0};$endQpc=if($null -ne $win.End){$win.End.callback_qpc}else{0}
    $barrierViolation=$first.Count -gt 0 -and ($endQpc -eq 0 -or $firstQpc -le $endQpc)
    if($begin.Count){
        if($final.Count -ne 1 -or $final[0].FailedChecks.Count -gt 0 -or -not $final[0].Proof.write_authorized){$barrierViolation=$true}
        else{
            foreach($b in $begin){Assert-EndDiagnostic ($b.sequence -gt $final[0].Sequence -and $b.winevent_end_qpc -eq $endQpc -and $b.winevent_end_sequence -eq $win.End.sequence -and $b.handoff_preflight_sequence -eq $final[0].Sequence) 'writer did not bind the passed fresh public END preflight'}
        }
    }
    $native=@(Get-EndDiagnosticRows $Rows 'EXIT');$afterWin=if($endQpc){@(Get-EndDiagnosticRows $Rows 'DRAG'|Where-Object receipt_qpc -gt $endQpc).Count}else{0}
    $counterexample=$afterWin -gt 0 -or $barrierViolation -or ($final.Count -eq 1 -and $final[0].Disposition -ceq 'FAIL')
    $result=$base.Result;$architecture=$base.Architecture;$barrier=if($final.Count -eq 1 -and $final[0].FailedChecks.Count -eq 0 -and $null -ne $win.End){'PASS'}else{'FAIL'}
    if($counterexample){$result='FAIL';$architecture='REJECTED_AT_END_BARRIER'}
    elseif($final.Count -ne 1 -or $final[0].FailedChecks.Count -gt 0 -or -not $win.Installed -or -not $win.Removed -or @(Get-EndDiagnosticRows $Rows 'winevent_error').Count -gt 0){$result='BLOCKED';$architecture='UNRESOLVED'}
    $initialClass=if($initial.Count){$initial[0].FailureClass}else{'NOT_OBSERVED'};$finalClass=if($final.Count){$final[0].FailureClass}else{'NOT_OBSERVED'}
    return [pscustomobject]@{Result=$result;DiagnosedResult=$result;Operation=$s.operation;ForegroundContract=$s.foreground_contract;HandoffContract=$s.handoff_contract;DiagnosticContract=$s.diagnostic_contract;ContractVerified=$true;StructuredDiagnostic='PASS';InitialFailureClass=$initialClass;FinalFailureClass=$finalClass;InitialFailedChecks=$(if($initial.Count){$initial[0].FailedChecks}else{@()});FinalFailedChecks=$(if($final.Count){$final[0].FailedChecks}else{@()});UniqueGuardFailure=($final.Count -eq 1 -and $final[0].UniqueGuardFailure);WinEventBarrier=$barrier;MatchedStartCallbackSequence=$(if($null -ne $win.Start){$win.Start.callback_sequence}else{0});MatchedEndCallbackSequence=$(if($null -ne $win.End){$win.End.callback_sequence}else{0});MatchedEndCallbackQpc=$endQpc;FirstNativeStartQpc=$firstQpc;NativeExitToWinEventEndTicks=$(if($native.Count -eq 1 -and $endQpc){$endQpc-$native[0].receipt_qpc}else{$null});WinEventEndToFirstWriteTicks=$(if($firstQpc -and $endQpc){$firstQpc-$endQpc}else{$null});NativeDragAfterWinEventEnd=$afterWin;RawBackground=$base.RawBackground;Cancel=$base.Cancel;Handoff=$base.Handoff;Takeover=$base.Takeover;PreReleaseControl=$base.PreReleaseControl;Architecture=$architecture;NativeWrites=$base.NativeWrites;HandoffNativeCalls=$base.HandoffNativeCalls;RawPackets=$base.RawPackets;RawMovementPackets=$base.RawMovementPackets;RawUpPackets=$base.RawUpPackets;ContinuationQuanta=$base.ContinuationQuanta;FullGestureTargets=$base.FullGestureTargets;CleanupInputRelease=$cleanup.Status;CleanupAcceptanceEligible=$false;PendingButton=$base.PendingButton;CleanupCurrentLeftDown=$(if($null -ne $cleanup.Final){$cleanup.Final.left_down}else{$null});Reasons=$base.Reasons;SyntheticFixture=((Get-AutoField $s 'synthetic_fixture') -eq $true)}
}

function Test-EndDiagnosticsOwnedEvidence([string]$Path,[switch]$AllowSynthetic){
    $rows=@(Get-Content -LiteralPath $Path -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json -ErrorAction Stop})
    return Test-EndDiagnosticsOwnedRecords $rows -AllowSynthetic:$AllowSynthetic
}
