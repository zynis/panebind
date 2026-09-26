Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

# Read-only reuse of reviewed scalar/rectangle, foreground, and Raw receipt
# helpers. This validator never runs a probe or reclassifies historical v1 logs.
. (Join-Path $PSScriptRoot 'r1c4b-takeover-owned-validation.ps1')

function Assert-EndHandoff([bool]$Value,[string]$Reason){
    if(-not $Value){throw "owned END handoff: $Reason"}
}
function Get-EndHandoffRows($Rows,[string]$Type){return @($Rows|Where-Object type -ceq $Type)}
function Test-EndPointExact($A,$B){Assert-AutoPoint $A 'exact point';Assert-AutoPoint $B 'exact point';return ($A -join ',') -ceq ($B -join ',')}
function Test-EndRectMatch($Value,$Target){if($null -eq $Value){return $false};return Test-AutoRect $Value $Target}
function Test-EndRectSame($A,$B){if($null -eq $A -or $null -eq $B){return $null -eq $A -and $null -eq $B};return Test-AutoRect $A $B}

function Test-EndPostverifyDiagnostic($Result,$Begin){
    $d=$Result.diagnostic
    foreach($name in @('native_success','capture_succeeded','visible_exact','positioning_exact','other_members_exact','receipt_health','source_context_exact')){Assert-AutoBoolean (Get-AutoField $d $name) "postverify diagnostic $name"}
    foreach($name in @('win32_error','source_member')){Assert-AutoInteger (Get-AutoField $d $name) "postverify diagnostic $name"}
    Assert-EndHandoff ($d.native_success -eq $Result.native_success -and $d.win32_error -eq $Result.error -and $d.source_member -eq 0 -and (Test-AutoRect $d.requested_positioning $Begin.intended_positioning) -and (Test-AutoRect $d.requested_visible $Begin.intended_visible) -and (Test-EndRectSame $d.actual_positioning $Result.positioning) -and (Test-EndRectSame $d.actual_visible $Result.visible)) 'diagnostic does not describe the actual submitted operation/capture'
    $pExact=Test-EndRectMatch $d.actual_positioning $d.requested_positioning;$vExact=Test-EndRectMatch $d.actual_visible $d.requested_visible
    Assert-EndHandoff ($d.positioning_exact -eq $pExact -and $d.visible_exact -eq $vExact -and $d.capture_succeeded -eq ($Result.positioning_error -eq 0 -and $Result.visible_hresult -ge 0)) 'forged diagnostic capture/exact bits'
    foreach($kind in @('positioning','visible')){
        $actual=Get-AutoField $d ('actual_'+$kind);$delta=Get-AutoField $d ($kind+'_edge_delta');$requested=Get-AutoField $d ('requested_'+$kind)
        if($null -eq $actual){Assert-EndHandoff ($null -eq $delta) 'edge delta without capture'}
        else{
            Assert-EndHandoff (@($delta).Count -eq 4) 'missing exact diagnostic edge delta'
            for($i=0;$i -lt 4;++$i){Assert-AutoInteger $delta[$i] 'edge delta';Assert-EndHandoff ($delta[$i] -eq [long]$actual[$i]-[long]$requested[$i]) 'forged postverify edge delta'}
        }
    }
    $failure=if(-not $d.native_success){'NativeCallFailed'}elseif(-not $d.capture_succeeded){'CaptureFailed'}elseif(-not $pExact -and -not $vExact){'BothGeometryMismatch'}elseif(-not $pExact){'PositioningMismatch'}elseif(-not $vExact){'VisibleMismatch'}elseif(-not $d.source_context_exact){'SourceContextChanged'}elseif(-not $d.other_members_exact){'OtherMemberChanged'}elseif(-not $d.receipt_health){'ReceiptHealthFailed'}else{'None'}
    Assert-EndHandoff ($d.failure_class -ceq $failure -and $Result.positioning_exact -eq $pExact -and $Result.visible_exact -eq $vExact) 'forged diagnostic failure class'
    $full=(Test-EndRectMatch $Result.full_positioning $Begin.intended_positioning) -and (Test-EndRectMatch $Result.full_visible $Begin.intended_visible)
    $exact=$failure -ceq 'None' -and $full
    Assert-EndHandoff ($Result.postverify_exact -eq $exact) 'forged strict immediate/full postverify summary'
    return $exact
}

function Get-EndHandoffIntended(
    [ValidateSet('Move','BottomResize')][string]$Operation,
    $StartPositioning,$StartVisible,$PointerDown,$CurrentCursor
){
    $null=Get-AutoRect $StartPositioning;$null=Get-AutoRect $StartVisible
    foreach($value in @($StartPositioning)+@($StartVisible)){Assert-AutoInteger $value 'original integer rectangle'}
    $p=$StartPositioning;$v=$StartVisible
    Assert-AutoPoint $PointerDown 'original pointer down';Assert-AutoPoint $CurrentCursor 'current cursor'
    $dx=[long]$CurrentCursor[0]-[long]$PointerDown[0];$dy=[long]$CurrentCursor[1]-[long]$PointerDown[1]
    if($Operation -ceq 'Move'){
        $targetP=@(([long]$p[0]+$dx),([long]$p[1]+$dy),([long]$p[2]+$dx),([long]$p[3]+$dy))
        $targetV=@(([long]$v[0]+$dx),([long]$v[1]+$dy),([long]$v[2]+$dx),([long]$v[3]+$dy))
    }else{
        # The fixed frame bridge is checked from the true START rectangles;
        # no visible/positioning offsets are guessed from a later result.
        $targetP=@([long]$p[0],[long]$p[1],[long]$p[2],([long]$p[3]+$dy))
        $targetV=@([long]$v[0],[long]$v[1],[long]$v[2],([long]$v[3]+$dy))
    }
    $null=Get-AutoRect $targetP;$null=Get-AutoRect $targetV
    return [pscustomobject]@{Positioning=$targetP;Visible=$targetV;Delta=@($dx,$dy)}
}

function Get-EndHandoffBarrierGate(
    [int]$CancelCalls,[bool]$NativeEnter,[bool]$NativeDrag,
    [bool]$AuthorityValid,[bool]$CancelReturned,[bool]$RealExit,
    [bool]$CaptureReleased,[bool]$LeftHeldAtExit,[bool]$GuiClearAfterExit,
    [bool]$RawHealthy,[bool]$RawContinuation,[int]$NativeDragAfterReturn,
    [int]$UnattributedGeometryAfterEnd,[int]$WritesBeforeEnd
){
    foreach($n in @($CancelCalls,$NativeDragAfterReturn,$UnattributedGeometryAfterEnd,$WritesBeforeEnd)){Assert-EndHandoff ($n -ge 0) 'negative authority counter'}
    if(-not $AuthorityValid -or -not $NativeEnter -or -not $NativeDrag -or $CancelCalls -eq 0){return 'UNKNOWN'}
    if($CancelCalls -gt 1 -or $NativeDragAfterReturn -gt 0 -or $UnattributedGeometryAfterEnd -gt 0 -or $WritesBeforeEnd -gt 0){return 'FAIL'}
    if($CancelReturned -and $RealExit -and $CaptureReleased -and $LeftHeldAtExit -and $GuiClearAfterExit -and $RawHealthy -and $RawContinuation){return 'PASS_WITH_TERMINAL_SETTLEMENT'}
    return 'UNKNOWN'
}

function Get-EndHandoffReconciliationGate(
    [bool]$EndObserved,[bool]$AuthorityValid,$ActualPositioning,$ActualVisible,
    $IntendedPositioning,$IntendedVisible,[int]$NativeCalls,
    [long]$EndQpc,[long]$NativeStartQpc,[bool]$TargetUsesFullDelta,[bool]$PostverifyExact
){
    Assert-EndHandoff ($NativeCalls -ge 0) 'negative handoff write count'
    if(-not $EndObserved -or -not $AuthorityValid){return 'NOT_RUN'}
    $mismatch=-not (Test-AutoRect $ActualPositioning $IntendedPositioning) -or -not (Test-AutoRect $ActualVisible $IntendedVisible)
    if(-not $TargetUsesFullDelta -or $NativeCalls -ne [int]$mismatch -or ($NativeCalls -gt 0 -and $NativeStartQpc -le $EndQpc) -or -not $PostverifyExact){return 'FAIL'}
    return 'PASS'
}

function Test-EndHandoffEnvelope([object[]]$Rows){
    Assert-EndHandoff ($Rows.Count -ge 2) 'empty evidence'
    for($i=0;$i -lt $Rows.Count;++$i){
        $r=$Rows[$i]
        Assert-EndHandoff ($r.schema -ceq 'r1c4b-takeover-owned/v2') 'schema'
        foreach($field in @('sequence','gesture','qpc')){Assert-AutoInteger (Get-AutoField $r $field) "END $field"}
        Assert-EndHandoff ($r.sequence -eq $i+1 -and $r.gesture -ge 0 -and $r.qpc -ge 0 -and ($i -eq 0 -or $r.qpc -ge $Rows[$i-1].qpc)) 'sequence/gesture/QPC order'
    }
    Assert-EndHandoff ($Rows[0].type -ceq 'startup' -and $Rows[-1].type -ceq 'shutdown' -and @(Get-EndHandoffRows $Rows 'startup').Count -eq 1 -and @(Get-EndHandoffRows $Rows 'shutdown').Count -eq 1) 'lifecycle envelope'
}

function Get-EndHandoffQuantumGate(
    [bool]$AuthorityValid,[bool]$RealEnd,[bool]$FreshRawMovement,
    [bool]$RawUpAlreadyObserved,[bool]$TargetUsesOriginalAnchor,
    [int]$NativeCalls,[long]$EndQpc,[long]$NativeStartQpc,
    [bool]$PostverifyExact
){
    Assert-EndHandoff ($NativeCalls -ge 0) 'negative quantum write count'
    if(-not $AuthorityValid -or -not $RealEnd -or -not $FreshRawMovement){return 'NOT_RUN'}
    if($RawUpAlreadyObserved -or -not $TargetUsesOriginalAnchor -or $NativeCalls -gt 1 -or
       ($NativeCalls -gt 0 -and $NativeStartQpc -le $EndQpc) -or -not $PostverifyExact){return 'FAIL'}
    return 'PASS'
}

function Get-EndHandoffCompletionGate(
    [bool]$ObservationComplete,[bool]$RawUpObserved,[bool]$FinalCursorValid,
    [bool]$FinalGeometryExact,[bool]$PendingWrite,[bool]$GuiClear,
    [int]$ContinuationMovements,[int]$NativeDragAfterEnd
){
    Assert-EndHandoff ($ContinuationMovements -ge 0 -and $NativeDragAfterEnd -ge 0) 'negative completion counter'
    if(-not $ObservationComplete){return 'NOT_RUN'}
    if(-not $RawUpObserved -or -not $FinalCursorValid -or -not $FinalGeometryExact -or $PendingWrite -or
       -not $GuiClear -or $ContinuationMovements -lt 15 -or $NativeDragAfterEnd -gt 0){return 'FAIL'}
    return 'PASS'
}

function Test-EndHandoffAuthority($Proof,$Owned,$Guard,$Startup,[bool]$Held,[switch]$AllowTerminalUpLag){
    foreach($name in @('target','source_pid','source_tid','foreground','foreground_pid','foreground_tid','capture_hwnd','menu_owner_hwnd','move_size_hwnd','gui_flags','cursor_root','guard','dpi','monitor')){Assert-AutoInteger (Get-AutoField $Proof $name) "writer authority $name"}
    foreach($name in @('own_identity','desktop_ready','source_visible','gui_query_succeeded','buttons_modifiers_clear','left_down','receiver_healthy','raw_up_seen')){Assert-AutoBoolean (Get-AutoField $Proof $name) "writer authority $name"}
    Assert-EndHandoff ($Owned.Count -eq 1 -and $Guard.Count -eq 1 -and $Proof.target -eq $Owned[0].hwnd -and $Proof.source_pid -eq $Startup.pid -and $Proof.source_tid -eq $Startup.ui_tid -and $Proof.guard -eq $Guard[0].hwnd) 'writer is not bound to the exact fresh owned source/guard'
    return $Proof.own_identity -and $Proof.desktop_ready -and $Proof.source_visible -and $Proof.foreground -eq $Owned[0].hwnd -and $Proof.foreground_pid -eq $Startup.pid -and $Proof.foreground_tid -eq $Startup.ui_tid -and $Proof.gui_query_succeeded -and $Proof.capture_hwnd -eq 0 -and $Proof.menu_owner_hwnd -eq 0 -and $Proof.move_size_hwnd -eq 0 -and ($Proof.gui_flags -band 30) -eq 0 -and $Proof.buttons_modifiers_clear -and ($AllowTerminalUpLag -or $Proof.left_down -eq $Held) -and $Proof.receiver_healthy -and $Proof.cursor_root -in @($Owned[0].hwnd,$Guard[0].hwnd) -and $Proof.dpi -eq $Owned[0].dpi -and $Proof.monitor -eq $Owned[0].monitor
}

function Test-EndHandoffInputEvidence($Rows,$Owned,$Guard,$Bootstrap,[bool]$Blocked){
    # The existing strict input fences remain valid. Cursor restoration has a
    # new one-gesture completion boundary and is checked independently below.
    $ordinary=@($Rows|Where-Object {-not ($_.type -ceq 'input' -and $_.restoring_cursor)})
    $inputState=Test-TakeoverInputs $ordinary $Owned $Guard $Bootstrap $Blocked
    $restores=@($Rows|Where-Object {$_.type -ceq 'input' -and $_.restoring_cursor})
    Assert-EndHandoff ($restores.Count -le 1) 'cursor restoration retried'
    foreach($request in $restores){
        foreach($name in @('flags','sent','error','input_tag','injection_start_qpc','injection_return_qpc','normalized_dx','normalized_dy','actual_mouse_flags','receiver_watermark')){Assert-AutoInteger (Get-AutoField $request $name) "restoration $name"}
        $proof=@($Rows|Where-Object {$_.type -ceq 'input_fence' -and $_.sequence -lt $request.sequence})
        $begin=@($Rows|Where-Object {$_.type -ceq 'cursor_restore_begin' -and $_.sequence -lt $request.sequence})
        $terminal=@(Get-EndHandoffRows $Rows 'takeover_end');$complete=@(Get-EndHandoffRows $Rows 'path_complete')
        Assert-EndHandoff ($proof.Count -gt 0 -and $begin.Count -eq 1 -and $terminal.Count -eq 1 -and $complete.Count -eq 1 -and $terminal[0].sequence -lt $begin[0].sequence -and $complete[0].sequence -lt $begin[0].sequence -and -not $inputState.PendingButton) 'cursor restored before the single real takeover END'
        $f=$proof[-1]
        Assert-EndHandoff ($request.flags -eq 1 -and $request.sent -in @(0,1) -and $request.input_tag -eq 0x50424D41 -and $request.actual_mouse_flags -eq 57345 -and $f.target -eq $Owned[0].hwnd -and $f.foreground -eq $Owned[0].hwnd -and -not $f.left_down -and $f.gui_query_succeeded -and $f.capture_hwnd -eq 0 -and $f.menu_owner_hwnd -eq 0 -and $f.move_size_hwnd -eq 0 -and ($f.gui_flags -band 30) -eq 0 -and (Test-AutoPoint $request.point $Owned[0].saved_cursor) -and (Test-AutoPoint $begin[0].point $Owned[0].saved_cursor)) 'restoration is not the authorized no-button saved-point MOVE'
        Assert-EndHandoff ($f.qpc -le $request.injection_start_qpc -and $request.injection_start_qpc -le $request.injection_return_qpc -and $request.injection_return_qpc -le $request.qpc -and (Test-AutoRect $request.virtual_screen $Owned[0].virtual_screen)) 'restoration clock/screen mismatch'
        $screen=$request.virtual_screen;$width=[decimal]$screen[2]-$screen[0];$height=[decimal]$screen[3]-$screen[1]
        $dx=[Math]::Floor((2*([decimal]$request.point[0]-$screen[0])+1)*65536/(2*$width));$dy=[Math]::Floor((2*([decimal]$request.point[1]-$screen[1])+1)*65536/(2*$height))
        Assert-EndHandoff ($request.normalized_dx -eq $dx -and $request.normalized_dy -eq $dy -and $request.receiver_watermark -ge 1) 'restoration normalized INPUT mismatch'
        if($request.sent -eq 1){Assert-EndHandoff ($request.error -eq 0) 'restoration native error'}else{Assert-EndHandoff $Blocked 'failed restoration without blocked shutdown'}
    }
    return $inputState
}

function Test-EndHandoffRawEvidence($Rows,$Startup,$Owned,[bool]$Blocked){
    $registration=@(Get-EndHandoffRows $Rows 'receiver');$stopped=@(Get-EndHandoffRows $Rows 'receiver_shutdown');$errors=@(Get-EndHandoffRows $Rows 'receiver_error');$raw=@(Get-EndHandoffRows $Rows 'raw_input')
    Assert-EndHandoff ($registration.Count -le 1 -and $stopped.Count -le 1) 'duplicate receiver lifecycle'
    $serial=0;$clock=0
    foreach($r in @($Rows|Where-Object {$_.type -cin @('receiver','raw_input','receiver_shutdown') -or ($_.type -ceq 'receiver_error' -and $null -ne (Get-AutoField $_ 'receiver_sequence'))})){
        Assert-EndHandoff ($registration.Count -eq 1) 'receiver data without registration'
        foreach($name in @('receiver_sequence','receiver_qpc','receiver_hwnd','receiver_pid','receiver_tid')){Assert-AutoInteger (Get-AutoField $r $name) "receiver $name"}
        Assert-EndHandoff ($r.receiver_sequence -eq ++$serial -and $r.receiver_qpc -ge $clock -and $r.receiver_qpc -le $r.qpc -and $r.receiver_pid -eq $registration[0].receiver_pid -and $r.receiver_tid -eq $registration[0].receiver_tid -and $r.receiver_hwnd -eq $registration[0].receiver_hwnd) 'receiver sequence/QPC/exact identity'
        $clock=$r.receiver_qpc
    }
    if($registration.Count){
        $r=$registration[0]
        Assert-AutoBoolean $r.registration_verified 'receiver registration proof';Assert-AutoBoolean $r.keyboard_registered 'keyboard registration'
        Assert-EndHandoff ($r.receiver_pid -ne $Startup.pid -and $r.receiver_pid -gt 0 -and $r.receiver_tid -gt 0 -and $r.receiver_hwnd -gt 0 -and $r.registration_flags -eq 256 -and $r.usage_page -eq 1 -and $r.usage -eq 2 -and -not $r.keyboard_registered -and $stopped.Count -eq 1 -and $stopped[0].registration_removed -and $stopped[0].window_destroyed -and $Rows[-1].receiver_stopped) 'receiver is not a separate INPUTSINK mouse-only process or cleanup incomplete'
    }else{Assert-EndHandoff ($raw.Count -eq 0 -and $stopped.Count -eq 0) 'Raw without receiver'}
    foreach($r in $raw){
        foreach($name in @('input_code','raw_flags','dx','dy','button_flags','foreground_hwnd','foreground_pid')){Assert-AutoInteger (Get-AutoField $r $name) "Raw $name"}
        foreach($name in @('cursor_sampled','cursor_success','left_down','device_handle_present','test_tag_matches')){Assert-AutoBoolean (Get-AutoField $r $name) "Raw $name"}
        Assert-EndHandoff ($Owned.Count -eq 1 -and $r.input_code -eq 1 -and $r.foreground_hwnd -eq $Owned[0].hwnd -and $r.foreground_pid -eq $Startup.pid -and $r.foreground_pid -ne $r.receiver_pid -and $r.raw_flags -ge 0 -and $r.raw_flags -le 65535 -and $r.button_flags -ge 0 -and $r.button_flags -le 65535) 'Raw is not exact background mouse evidence'
        $movement=(($r.raw_flags -band 1) -ne 0 -or $r.dx -ne 0 -or $r.dy -ne 0)
        Assert-EndHandoff ($r.cursor_sampled -eq $movement) 'Raw movement sampling forged'
        if($r.cursor_sampled -and $r.cursor_success){Assert-AutoPoint $r.cursor 'actual Raw cursor diagnostic'}else{Assert-EndHandoff ($null -eq $r.cursor) 'Raw cursor without capture'}
    }
    foreach($status in @(Get-EndHandoffRows $Rows 'raw_cursor_status')){
        $packet=@($raw|Where-Object receiver_sequence -eq $status.receiver_sequence)
        Assert-EndHandoff ($packet.Count -eq 1 -and $status.receiver_pid -eq $packet[0].receiver_pid -and $status.receiver_tid -eq $packet[0].receiver_tid -and $status.receiver_hwnd -eq $packet[0].receiver_hwnd -and $status.receiver_qpc -eq $packet[0].receiver_qpc) 'cursor status is not the referenced real Raw packet'
        Assert-AutoInteger $status.cursor_error 'Raw cursor diagnostic error'
    }
    $waits=@(Get-EndHandoffRows $Rows 'raw_wait_begin')
    for($i=0;$i -lt $waits.Count;++$i){
        $b=$waits[$i];$limit=if($i+1 -lt $waits.Count){$waits[$i+1].sequence}else{$Rows[-1].sequence}
        $request=@($Rows|Where-Object {$_.type -ceq 'input' -and $_.flags -eq 1 -and $_.injection_start_qpc -eq $b.input_start_qpc -and $_.sequence -lt $b.sequence})
        $result=@($Rows|Where-Object {$_.type -ceq 'raw_wait_result' -and $_.sequence -gt $b.sequence -and $_.sequence -lt $limit})
        Assert-EndHandoff ($request.Count -eq 1 -and $result.Count -eq 1 -and $b.timeout_ms -eq 2000 -and (Test-AutoPoint $b.expected_cursor $request[0].point)) 'Raw wait without actual INPUT'
        $correlation=@($Rows|Where-Object {$_.type -ceq 'raw_motion_correlation' -and $_.sequence -gt $result[0].sequence -and $_.sequence -lt $limit})
        if($result[0].wait_result -eq 0){
            Assert-EndHandoff ($correlation.Count -eq 1) 'successful wait lacks correlation'
            $c=$correlation[0];$packet=@($raw|Where-Object receiver_sequence -eq $c.receiver_sequence)
            Assert-AutoBoolean $c.delivered 'Raw delivered';Assert-AutoBoolean $c.snapshot_cursor_matches 'Raw snapshot diagnostic'
            Assert-EndHandoff ($packet.Count -eq 1 -and $packet[0].sequence -lt $c.sequence -and $c.delivered -eq (Test-TakeoverMotionReceipt $packet[0] $request[0] $true $false) -and $c.snapshot_cursor_matches -eq ($packet[0].cursor_success -and (Test-AutoPoint $packet[0].cursor $request[0].point))) 'Raw delivery/cursor diagnostic not independently correlated'
            if(-not $c.delivered){Assert-EndHandoff $Blocked 'uncorrelated delivery did not stop'}
        }else{Assert-EndHandoff ($Blocked -and $correlation.Count -eq 0) 'failed wait continued'}
    }
    $begin=@(Get-EndHandoffRows $Rows 'raw_preflight_begin');$done=@(Get-EndHandoffRows $Rows 'raw_preflight_complete');$outcome=@(Get-EndHandoffRows $Rows 'raw_preflight_outcome')
    Assert-EndHandoff ($begin.Count -le 1 -and $done.Count -le 1 -and $outcome.Count -le 1) 'duplicate Raw preflight'
    $gate='BLOCKED'
    if($done.Count -or $outcome.Count){
        $boundary=if($outcome.Count){$outcome[0]}else{$done[0]}
        Assert-EndHandoff ($begin.Count -eq 1 -and $boundary.gesture -eq 0 -and $begin[0].sequence -lt $boundary.sequence) 'Raw preflight lifecycle'
        $stimulus=@($Rows|Where-Object {$_.type -ceq 'input' -and $_.sequence -gt $begin[0].sequence -and $_.sequence -lt $boundary.sequence})
        Assert-EndHandoff ($stimulus.Count -eq 4 -and ($stimulus.flags -join ',') -ceq '1,2,1,4' -and @($stimulus|Where-Object sent -ne 1).Count -eq 0) 'Raw preflight incomplete actual stimulus'
        $first=@($raw|Where-Object {$_.gesture -eq 0 -and $_.receiver_qpc -le $stimulus[1].injection_start_qpc -and (Test-TakeoverMotionReceipt $_ $stimulus[0] $true $false)})
        $held=@($raw|Where-Object {$_.gesture -eq 0 -and $_.receiver_qpc -le $stimulus[3].injection_start_qpc -and (Test-TakeoverMotionReceipt $_ $stimulus[2] $true $true)})
        $up=@($raw|Where-Object {$_.gesture -eq 0 -and $_.sequence -lt $boundary.sequence -and (Test-TakeoverUpReceipt $_ $stimulus[3] $true)})
        $fence=@($Rows|Where-Object {$_.type -ceq 'input_fence' -and $_.sequence -gt $stimulus[3].sequence -and $_.sequence -lt $boundary.sequence -and -not $_.left_down -and $_.foreground -eq $Owned[0].hwnd -and $_.gui_query_succeeded -and $_.capture_hwnd -eq 0 -and $_.menu_owner_hwnd -eq 0 -and $_.move_size_hwnd -eq 0 -and ($_.gui_flags -band 30) -eq 0 -and (Test-AutoPoint $_.cursor $stimulus[2].point)})
        Assert-EndHandoff ($fence.Count -gt 0 -and (Test-AutoPoint $stimulus[0].point $begin[0].point) -and $stimulus[2].point[0]-$begin[0].point[0] -eq 12 -and $stimulus[2].point[1] -eq $begin[0].point[1]) 'Raw preflight final driver fence/path'
        $complete=$false
        if($outcome.Count){
            $o=$outcome[0]
            foreach($name in @('attempted','actual_move_verified','actual_held_move_verified','actual_up_verified','input_delivery_verified','observation_completed','receiver_healthy','raw_up_observed')){Assert-AutoBoolean (Get-AutoField $o $name) "Raw outcome $name"}
            foreach($name in @('up_wait_result','up_wait_started_qpc','up_wait_finished_qpc','up_timeout_ms')){Assert-AutoInteger (Get-AutoField $o $name) "Raw outcome $name"}
            $complete=$o.attempted -and $o.actual_move_verified -and $o.actual_held_move_verified -and $o.actual_up_verified -and $o.input_delivery_verified -and $o.observation_completed -and $o.receiver_healthy
            Assert-EndHandoff ($complete -and $o.up_timeout_ms -eq 2000 -and $o.up_wait_result -in @(0,258) -and $o.up_wait_started_qpc -ge $stimulus[3].injection_return_qpc -and $o.up_wait_finished_qpc -ge $o.up_wait_started_qpc -and $o.up_wait_finished_qpc -le $o.qpc -and $o.raw_up_observed -eq ($up.Count -gt 0) -and $o.raw_up_observed -eq ($o.up_wait_result -eq 0)) 'Raw outcome is not proven by the actual delivery lifecycle'
            if($o.up_wait_result -eq 258){Assert-EndHandoff ([decimal]($o.up_wait_finished_qpc-$o.up_wait_started_qpc)*1000 -ge [decimal]$o.up_timeout_ms*$Startup.qpc_frequency) 'forged Raw UP deadline'}
        }
        $gate=Get-TakeoverRawBackgroundGate -RegistrationObserved ($registration.Count -eq 1) -RegistrationSucceeded ($registration.Count -eq 1 -and $registration[0].registration_verified) -ReceiverIdentityValid ($registration.Count -eq 1) -BackgroundConfirmed ($raw.Count -gt 0) -MovementObserved ($first.Count -gt 0 -and $held.Count -gt 0) -UpObserved ($up.Count -gt 0) -EvidenceHealthy ($errors.Count -eq 0 -and $Rows[-1].receiver_stopped) -StimulusComplete $complete
        if($done.Count){
            Assert-EndHandoff ($gate -ceq 'PASS' -and $done[0].sequence -ge $boundary.sequence -and $done[0].movement_count -ge 2 -and $done[0].movement_count -le @($raw|Where-Object cursor_sampled).Count -and $done[0].up_count -ge 1 -and $done[0].up_count -le @($raw|Where-Object {($_.button_flags -band 2) -ne 0}).Count) 'preflight PASS marker without true movement/UP'
        }else{Assert-EndHandoff ($Blocked -and $gate -ceq 'FAIL') 'completed stimulus missing proof did not fail/stop'}
    }
    return [pscustomobject]@{Gate=$gate;Packets=$raw;Healthy=($errors.Count -eq 0);CompleteSequence=$(if($done.Count){$done[0].sequence}else{0})}
}

function Test-EndHandoffOwnedRecords([object[]]$Rows,[switch]$AllowSynthetic){
    Test-EndHandoffEnvelope $Rows
    $s=$Rows[0];$end=$Rows[-1];$blocked=$end.result -ceq 'BLOCKED';$operation=$s.operation;$gesture=if($operation -ceq 'Move'){1}else{2}
    foreach($name in @('pid','ui_tid','qpc_frequency','takeover_geometry_writes')){Assert-AutoInteger (Get-AutoField $s $name) "startup $name"}
    foreach($name in @('human_input','real_explorer','sendinput_in_probe')){Assert-AutoBoolean (Get-AutoField $s $name) "startup $name"}
    Assert-EndHandoff ($s.evidence_kind -ceq 'automated_owned_end_handoff' -and $operation -cin @('Move','BottomResize') -and $s.mode -ceq 'free_takeover' -and $s.handoff_contract -ceq 'end_barrier_v1' -and $s.foreground_contract -ceq 'verified_global_foreground_v2' -and $s.input_correlation -ceq 'actual_absolute_receipt_v1' -and $s.takeover_geometry_writes -eq 0 -and -not $s.human_input -and -not $s.real_explorer -and $s.sendinput_in_probe -and $s.qpc_frequency -gt 0) 'not the fresh owned test-only END handoff scope'
    if((Get-AutoField $s 'synthetic_fixture') -eq $true){Assert-EndHandoff ([bool]$AllowSynthetic) 'synthetic fixture is not empirical acceptance'}
    Assert-EndHandoff ($end.result -cin @('BLOCKED','CAPTURED_NOT_ACCEPTED')) 'shutdown result'
    foreach($name in @('owned_window_destroyed','guard_window_destroyed','receiver_stopped','external_windows_touched','cursor_restored')){Assert-AutoBoolean (Get-AutoField $end $name) "shutdown $name"}
    Assert-EndHandoff ($end.owned_window_destroyed -and $end.guard_window_destroyed -and -not $end.external_windows_touched) 'unsafe owned cleanup'
    $allowed=@('startup','shutdown','desktop_gate','guard','owned','receiver','receiver_shutdown','receiver_error','show_window','foreground_attempt','foreground_ready','foreground_bootstrap','activation_event','activation_button','activation_visibility','activation_fence','activation_input','activation_release','input_fence','input','destination_fence','cursor_restore_begin','cleanup_fence','cleanup_release','raw_preflight_begin','raw_preflight_outcome','raw_preflight_complete','raw_input','raw_cursor_status','raw_wait_begin','raw_wait_result','raw_motion_correlation','LEGACY_MOVE','native_button_down','path','sample','path_complete','ENTER','DRAG','EXIT','CAPTURE_CHANGED','POSITION_CHANGED','cancel_begin','cancel_return','cancel_exit_wait','end_wait','cancel_message','cancel_confirmed','intent_anchor','handoff_begin','writer_begin','writer_result','raw_quantum','handoff_complete','takeover_end','takeover_failure','blocked')
    foreach($row in $Rows){Assert-EndHandoff ($row.type -cin $allowed -and $row.gesture -in @(0,$gesture)) 'unknown record or wrong fresh-window gesture'}
    $owned=@(Get-EndHandoffRows $Rows 'owned');$guard=@(Get-EndHandoffRows $Rows 'guard');$desktop=@(Get-EndHandoffRows $Rows 'desktop_gate')
    Assert-EndHandoff ($owned.Count -le 1 -and $guard.Count -le 1 -and $desktop.Count -le 1) 'duplicate window/desktop identity'
    if($owned.Count){
        foreach($name in @('hwnd','pid','tid','dpi','monitor')){Assert-AutoInteger (Get-AutoField $owned[0] $name) "owned $name"}
        Assert-EndHandoff ($owned[0].pid -eq $s.pid -and $owned[0].tid -eq $s.ui_tid -and $owned[0].hwnd -gt 0 -and $owned[0].dpi -gt 0 -and $owned[0].monitor -gt 0) 'owned identity/monitor/DPI'
        Assert-TakeoverGeometry $owned[0];$null=Get-AutoRect $owned[0].virtual_screen
    }
    if($guard.Count){Assert-EndHandoff ($guard[0].pid -eq $s.pid -and $guard[0].tid -eq $s.ui_tid -and $guard[0].hwnd -gt 0 -and ($owned.Count -eq 0 -or $guard[0].hwnd -ne $owned[0].hwnd)) 'guard identity'}
    if($desktop.Count){Assert-EndHandoff ($desktop[0].active_unlocked -and $desktop[0].input_desktop_matches) 'desktop proof'}
    $boot=Test-TakeoverGlobalForegroundV2 $Rows $owned $blocked
    $inputState=Test-EndHandoffInputEvidence $Rows $owned $guard $boot $blocked
    $rawEvidence=Test-EndHandoffRawEvidence $Rows $s $owned $blocked;$raw=@($rawEvidence.Packets)
    $path=@(Get-EndHandoffRows $Rows 'path');$enter=@(Get-EndHandoffRows $Rows 'ENTER');$down=@(Get-EndHandoffRows $Rows 'native_button_down');$drag=@(Get-EndHandoffRows $Rows 'DRAG');$cancel=@(Get-EndHandoffRows $Rows 'cancel_begin');$returned=@(Get-EndHandoffRows $Rows 'cancel_return');$exit=@(Get-EndHandoffRows $Rows 'EXIT');$anchor=@(Get-EndHandoffRows $Rows 'intent_anchor');$handoff=@(Get-EndHandoffRows $Rows 'handoff_begin');$handoffDone=@(Get-EndHandoffRows $Rows 'handoff_complete');$terminal=@(Get-EndHandoffRows $Rows 'takeover_end');$pathDone=@(Get-EndHandoffRows $Rows 'path_complete')
    foreach($a in @($path,$enter,$down,$cancel,$returned,$exit,$anchor,$handoff,$handoffDone,$terminal,$pathDone)){Assert-EndHandoff ($a.Count -le 1) 'duplicate one-gesture lifecycle'}
    $begin=@(Get-EndHandoffRows $Rows 'writer_begin');$result=@(Get-EndHandoffRows $Rows 'writer_result');$quantum=@(Get-EndHandoffRows $Rows 'raw_quantum')
    $nativeCalls=0;$writerFailure=$false;$barrierFailure=$false;$barrier='UNKNOWN';$reconciliation='NOT_RUN';$takeover='NOT_RUN';$preRelease='NOT_RUN';$writesBeforeEnd=0;$nativeAfterReturn=0;$nativeAfterEnd=0;$unownedAfterEnd=0;$terminalSettlements=0;$fullTargets=$true;$remaining=0;$handoffNativeCalls=0;$latencies=[ordered]@{}
    if($path.Count -or $cancel.Count){Assert-EndHandoff ($rawEvidence.Gate -ceq 'PASS' -and $path.Count -eq 1 -and $path[0].sequence -gt $rawEvidence.CompleteSequence) 'native phase before real background preflight'}
    if($path.Count){Assert-EndHandoff ($path[0].gesture -eq $gesture -and $path[0].samples -eq 20 -and $path[0].cancel_after_sample -eq 2 -and $path[0].interval_ms -eq 30 -and $path[0].hit_test -eq $(if($gesture -eq 1){2}else{15}) -and $path[0].foreground -eq $owned[0].hwnd) 'native path contract'}
    foreach($d in $drag){Assert-EndHandoff ($d.gesture -eq $gesture -and $d.event -ceq $(if($gesture -eq 1){'WM_MOVING'}else{'WM_SIZING'}) -and ($gesture -eq 1 -or $d.edge -eq 6)) 'wrong native drag operation/edge'}
    if($cancel.Count){
        Assert-EndHandoff ($enter.Count -eq 1 -and $down.Count -eq 1 -and $cancel[0].target -eq $owned[0].hwnd -and $cancel[0].source_tid -eq $s.ui_tid -and $cancel[0].capture_before -eq $owned[0].hwnd -and $cancel[0].left_down -and $enter[0].sequence -lt $cancel[0].sequence -and @($drag|Where-Object sequence -lt $cancel[0].sequence).Count -ge 2) 'cancel did not follow exact real native START/DRAG'
        Assert-AutoInteger $enter[0].receipt_qpc 'ENTER receipt';Assert-EndHandoff ($enter[0].receipt_qpc -le $enter[0].qpc) 'ENTER native receipt clock'
        if($returned.Count){
            foreach($name in @('transport_success','gui_query_succeeded','left_down')){Assert-AutoBoolean (Get-AutoField $returned[0] $name) "cancel return $name"}
            foreach($name in @('issued_qpc','returned_qpc','error','recipient_result','capture_after')){Assert-AutoInteger (Get-AutoField $returned[0] $name) "cancel return $name"}
            Assert-EndHandoff ($returned[0].sequence -gt $cancel[0].sequence -and $returned[0].issued_qpc -le $cancel[0].qpc -and $returned[0].returned_qpc -ge $cancel[0].qpc -and $returned[0].returned_qpc -le $returned[0].qpc -and (-not $returned[0].transport_success -or $returned[0].error -eq 0)) 'cancel transport clock/error'
            $message=@($Rows|Where-Object {$_.type -ceq 'cancel_message' -and $_.sequence -gt $cancel[0].sequence -and $_.sequence -lt $returned[0].sequence -and $_.target -eq $owned[0].hwnd -and $_.left_down})
            if($returned[0].transport_success){Assert-EndHandoff ($message.Count -eq 1) 'successful cancel lacks actual recipient message'}
            $nativeAfterReturn=@($drag|Where-Object qpc -gt $returned[0].returned_qpc).Count
        }
    }
    $endWait=@(Get-EndHandoffRows $Rows 'end_wait');Assert-EndHandoff ($endWait.Count -le 1) 'duplicate bounded END wait'
    foreach($w in $endWait){
        foreach($name in @('started_qpc','finished_qpc','timeout_ms','wait_result')){Assert-AutoInteger (Get-AutoField $w $name) "END wait $name"}
        Assert-EndHandoff ($returned.Count -eq 1 -and $w.started_qpc -ge $returned[0].returned_qpc -and $w.finished_qpc -ge $w.started_qpc -and $w.finished_qpc -le $w.qpc -and $w.timeout_ms -eq 2000) 'END wait boundary'
        if($w.wait_result -eq 0){Assert-EndHandoff ($exit.Count -eq 1 -and $exit[0].receipt_qpc -le $w.finished_qpc) 'successful END wait lacks actual native EXIT'}
        else{Assert-EndHandoff $blocked 'failed END wait did not stop';if($w.wait_result -eq 258){Assert-EndHandoff ([decimal]($w.finished_qpc-$w.started_qpc)*1000 -ge [decimal]$w.timeout_ms*$s.qpc_frequency) 'forged END timeout'}}
    }
    if($exit.Count){
        foreach($name in @('receipt_qpc','end_sequence','owner_capture')){Assert-AutoInteger (Get-AutoField $exit[0] $name) "END $name"}
        Assert-AutoBoolean $exit[0].left_down 'END held witness';Assert-TakeoverGeometry $exit[0]
        Assert-EndHandoff ($cancel.Count -eq 1 -and $exit[0].sequence -gt $cancel[0].sequence -and $exit[0].receipt_qpc -le $exit[0].qpc -and $exit[0].end_sequence -eq $exit[0].sequence) 'not a real post-cancel END barrier'
        $nativeAfterEnd=@($drag|Where-Object qpc -gt $exit[0].receipt_qpc).Count
    }
    if($anchor.Count){
        $a=$anchor[0]
        foreach($name in @('native_enter_sequence','native_enter_qpc','native_down_sequence')){Assert-AutoInteger (Get-AutoField $a $name) "intent anchor $name"}
        Assert-AutoPoint $a.pointer_down 'frozen down anchor';$null=Get-AutoRect $a.start_positioning;$null=Get-AutoRect $a.start_visible
        Assert-EndHandoff ($enter.Count -eq 1 -and $down.Count -eq 1 -and $a.gesture -eq $gesture -and $a.operation -ceq $operation -and $a.native_enter_sequence -eq $enter[0].sequence -and $a.native_enter_qpc -eq $enter[0].receipt_qpc -and $a.native_down_sequence -eq $down[0].sequence -and (Test-EndPointExact $a.pointer_down $down[0].cursor) -and (Test-AutoPoint $down[0].cursor $path[0].start) -and (Test-AutoRect $a.start_positioning $enter[0].positioning) -and (Test-AutoRect $a.start_visible $enter[0].visible)) 'intent anchor was not frozen from real START/down'
    }
    if($handoff.Count){
        $h=$handoff[0];Assert-EndHandoff ($exit.Count -eq 1 -and $anchor.Count -eq 1) 'handoff without native END/original intent'
        foreach($name in @('end_sequence','end_qpc','raw_watermark')){Assert-AutoInteger (Get-AutoField $h $name) "handoff $name"}
        Assert-AutoPoint $h.current_cursor 'handoff actual cursor';Assert-AutoPoint $h.cursor_delta 'handoff full delta'
        $authority=Test-EndHandoffAuthority $h $owned $guard $s $true
        Assert-EndHandoff ($h.end_sequence -eq $exit[0].sequence -and $h.end_qpc -eq $exit[0].receipt_qpc -and $h.raw_watermark -ge 1 -and $h.raw_watermark -le (@($raw|Where-Object receiver_qpc -le $h.qpc|Measure-Object receiver_sequence -Maximum).Maximum)) 'handoff does not refer to real END/receiver watermark'
        $intended=Get-EndHandoffIntended $operation $anchor[0].start_positioning $anchor[0].start_visible $anchor[0].pointer_down $h.current_cursor
        if(-not (Test-AutoRect $h.intended_positioning $intended.Positioning) -or -not (Test-AutoRect $h.intended_visible $intended.Visible) -or -not (Test-EndPointExact $h.cursor_delta $intended.Delta)){$fullTargets=$false;$writerFailure=$true}
        if(-not (Test-AutoRect $h.actual_handoff_positioning $exit[0].positioning) -or -not (Test-AutoRect $h.actual_handoff_visible $exit[0].visible)){$barrierFailure=$true}
        if(-not $authority -or $h.raw_up_seen){$writerFailure=$true}
    }
    $previousId=0;$previousQuantum=0;$usedRaw=$(if($handoff.Count){$handoff[0].raw_watermark}else{0});$expectedP=$(if($exit.Count){$exit[0].positioning}else{$null});$expectedV=$(if($exit.Count){$exit[0].visible}else{$null});$operationTargets=@{}
    foreach($b in $begin){
        foreach($name in @('operation_id','quantum_id','raw_first_sequence','raw_last_sequence','raw_trigger_qpc','native_calls','end_qpc')){Assert-AutoInteger (Get-AutoField $b $name) "writer begin $name"}
        Assert-EndHandoff ($b.operation_id -gt $previousId -and $b.kind -cin @('handoff','raw_movement') -and $b.native_calls -in @(0,1) -and $b.gesture -eq $gesture -and $anchor.Count -eq 1 -and $exit.Count -eq 1) 'writer id/kind/call count/scope'
        $r=@($result|Where-Object operation_id -eq $b.operation_id);Assert-EndHandoff ($r.Count -eq 1 -and $r[0].sequence -gt $b.sequence -and $r[0].quantum_id -eq $b.quantum_id -and $r[0].kind -ceq $b.kind -and $r[0].native_calls -eq $b.native_calls) 'writer result is not one actual bounded operation'
        $r=$r[0]
        foreach($name in @('native_success','positioning_exact','visible_exact','postverify_exact')){Assert-AutoBoolean (Get-AutoField $r $name) "writer result $name"}
        foreach($name in @('error','native_start_qpc','native_return_qpc','positioning_error','visible_hresult')){Assert-AutoInteger (Get-AutoField $r $name) "writer result $name"}
        Assert-AutoPoint $b.cursor 'writer actual cursor';Assert-AutoPoint $b.cursor_delta 'writer full delta'
        $authority=Test-EndHandoffAuthority $b $owned $guard $s $true
        $target=Get-EndHandoffIntended $operation $anchor[0].start_positioning $anchor[0].start_visible $anchor[0].pointer_down $b.cursor
        $full=(Test-AutoRect $b.intended_positioning $target.Positioning) -and (Test-AutoRect $b.intended_visible $target.Visible) -and (Test-EndPointExact $b.cursor_delta $target.Delta)
        if(-not $full){$fullTargets=$false;$writerFailure=$true}
        if($b.end_qpc -ne $exit[0].receipt_qpc -or -not $authority -or $b.raw_up_seen){$writerFailure=$true}
        if(-not (Test-AutoRect $b.expected_before_positioning $expectedP) -or -not (Test-AutoRect $b.expected_before_visible $expectedV)){$writerFailure=$true}
        if(-not (Test-AutoRect $b.before_positioning $expectedP) -or -not (Test-AutoRect $b.before_visible $expectedV)){$barrierFailure=$true}
        if($b.native_calls){
            ++$nativeCalls
            Assert-EndHandoff ($r.native_start_qpc -ge $b.qpc -and $r.native_return_qpc -ge $r.native_start_qpc -and $r.native_return_qpc -le $r.qpc) 'writer native clock order'
            if($r.native_start_qpc -le $exit[0].receipt_qpc){++$writesBeforeEnd;$barrierFailure=$true}
            if(-not $r.native_success -or $r.error -ne 0){$writerFailure=$true}
            if(@($raw|Where-Object {$_.gesture -eq $gesture -and ($_.button_flags -band 2) -ne 0 -and $_.receiver_qpc -le $r.native_return_qpc}).Count -gt 0){$writerFailure=$true}
        }else{Assert-EndHandoff ($r.native_start_qpc -eq 0 -and $r.native_return_qpc -eq 0 -and $r.native_success -and $r.error -eq 0) 'zero-write quantum claims a native call'}
        $nativeExact=Test-EndPostverifyDiagnostic $r $b
        $semanticExact=$nativeExact -and (Test-EndRectMatch $r.positioning $target.Positioning) -and (Test-EndRectMatch $r.visible $target.Visible) -and (Test-EndRectMatch $r.full_positioning $target.Positioning) -and (Test-EndRectMatch $r.full_visible $target.Visible)
        if(-not $semanticExact){$writerFailure=$true}
        if($b.kind -ceq 'handoff'){
            $handoffNativeCalls+=$b.native_calls
            Assert-EndHandoff ($b.quantum_id -eq 0 -and $b.raw_first_sequence -eq 0 -and $b.raw_last_sequence -eq 0 -and $b.raw_trigger_qpc -eq 0 -and $handoff.Count -eq 1) 'handoff falsely claims a Raw movement quantum'
            $reconciliation=Get-EndHandoffReconciliationGate -EndObserved $true -AuthorityValid $authority -ActualPositioning $handoff[0].actual_handoff_positioning -ActualVisible $handoff[0].actual_handoff_visible -IntendedPositioning $target.Positioning -IntendedVisible $target.Visible -NativeCalls $b.native_calls -EndQpc $exit[0].receipt_qpc -NativeStartQpc $r.native_start_qpc -TargetUsesFullDelta $full -PostverifyExact $semanticExact
            if($reconciliation -ceq 'FAIL'){$writerFailure=$true}
            if($b.native_calls){$latencies.ExitToHandoffStartTicks=$r.native_start_qpc-$exit[0].receipt_qpc;$latencies.HandoffNativeDurationTicks=$r.native_return_qpc-$r.native_start_qpc}
        }else{
            Assert-EndHandoff ($handoffDone.Count -eq 1 -and $b.sequence -gt $handoffDone[0].sequence -and $b.quantum_id -eq $previousQuantum+1 -and $b.raw_first_sequence -gt $usedRaw -and $b.raw_last_sequence -ge $b.raw_first_sequence) 'quantum did not consume fresh ordered movement after handoff'
            $batch=@($raw|Where-Object {$_.receiver_sequence -ge $b.raw_first_sequence -and $_.receiver_sequence -le $b.raw_last_sequence -and $_.cursor_sampled})
            $q=@($quantum|Where-Object operation_id -eq $b.operation_id)
            Assert-EndHandoff ($q.Count -eq 1 -and $q[0].sequence -gt $r.sequence -and $q[0].quantum_id -eq $b.quantum_id -and $q[0].native_calls -eq $b.native_calls -and $q[0].raw_first_sequence -eq $b.raw_first_sequence -and $q[0].raw_last_sequence -eq $b.raw_last_sequence -and $q[0].raw_trigger_qpc -eq $b.raw_trigger_qpc -and $q[0].coalesced_count -eq $batch.Count -and $batch.Count -gt 0 -and $batch[-1].receiver_sequence -eq $b.raw_last_sequence -and $b.raw_trigger_qpc -eq $batch[-1].receiver_qpc -and $b.raw_trigger_qpc -le $b.qpc -and (Test-AutoPoint $q[0].cursor $b.cursor)) 'quantum lacks the exact actual movement batch'
            $delivered=$true
            foreach($packet in $batch){
                $actual=@($Rows|Where-Object {$_.type -ceq 'input' -and $_.flags -eq 1 -and -not $_.restoring_cursor -and $_.gesture -eq $gesture -and (Test-TakeoverMotionReceipt $packet $_ $true $true)})
                if(-not $packet.test_tag_matches -or ($packet.raw_flags -band 3) -ne 3 -or $actual.Count -eq 0){$delivered=$false}
            }
            $gate=Get-EndHandoffQuantumGate -AuthorityValid $authority -RealEnd $true -FreshRawMovement $delivered -RawUpAlreadyObserved $b.raw_up_seen -TargetUsesOriginalAnchor $full -NativeCalls $b.native_calls -EndQpc $exit[0].receipt_qpc -NativeStartQpc $r.native_start_qpc -PostverifyExact $semanticExact
            if($gate -cne 'PASS'){$writerFailure=$true}
            $usedRaw=$b.raw_last_sequence;$previousQuantum=$b.quantum_id;++$remaining
        }
        $operationTargets[[string]$b.operation_id]=[pscustomobject]@{Positioning=$b.intended_positioning;Visible=$b.intended_visible;Begin=$b;Result=$r}
        $expectedP=$r.positioning;$expectedV=$r.visible;$previousId=$b.operation_id
    }
    Assert-EndHandoff ($result.Count -eq $begin.Count -and $quantum.Count -eq @($begin|Where-Object kind -ceq raw_movement).Count) 'orphan writer result/quantum'
    $handoffOperations=@($begin|Where-Object kind -ceq handoff)
    if($handoffOperations.Count -gt 1){$writerFailure=$true;$reconciliation='FAIL'}
    if($handoffDone.Count){
        $d=$handoffDone[0];Assert-AutoBoolean $d.exact 'handoff completion exact'
        Assert-EndHandoff ($handoffOperations.Count -ge 1 -and $d.operation_id -eq $handoffOperations[0].operation_id -and $d.native_calls -eq $handoffOperations[0].native_calls -and $d.end_qpc -eq $exit[0].receipt_qpc -and $d.sequence -gt $operationTargets[[string]$d.operation_id].Result.sequence) 'handoff completion without the actual operation'
        if(-not $d.exact){$writerFailure=$true}
    }
    if($exit.Count){
        $currentP=$exit[0].positioning;$currentV=$exit[0].visible
        foreach($change in @(Get-EndHandoffRows $Rows 'POSITION_CHANGED')){
            Assert-TakeoverGeometry $change
            foreach($name in @('operation_id')){Assert-AutoInteger (Get-AutoField $change $name) "position change $name"}
            foreach($name in @('after_end','unexpected_change')){Assert-AutoBoolean (Get-AutoField $change $name) "position change $name"}
            Assert-EndHandoff ($change.after_end -eq ($change.qpc -gt $exit[0].receipt_qpc)) 'forged native END-relative geometry tag'
            if($returned.Count -and $change.qpc -gt $returned[0].returned_qpc -and $change.qpc -le $exit[0].receipt_qpc){++$terminalSettlements}
            if($change.qpc -le $exit[0].receipt_qpc){Assert-EndHandoff ($change.operation_id -eq 0) 'takeover writer attributed before native END';continue}
            if($change.operation_id -gt 0){
                $key=[string]$change.operation_id;Assert-EndHandoff $operationTargets.ContainsKey($key) 'position change refers to nonexistent owned writer'
                $op=$operationTargets[$key]
                Assert-EndHandoff ($op.Begin.native_calls -eq 1 -and $change.qpc -ge $op.Begin.qpc -and $change.qpc -le $op.Result.qpc) 'position change outside its sole-writer operation'
                # A synchronous WINDOWPOS callback is inside the owned call;
                # strict visible policy is checked at immediate/full return,
                # not against an intermediate DWM sample before API return.
                if(-not (Test-AutoRect $change.positioning $op.Positioning)){$writerFailure=$true}
                $currentP=$op.Positioning;$currentV=$op.Visible
            }else{
                $prior=@($result|Where-Object qpc -le $change.qpc|Select-Object -Last 1)
                if($prior.Count){$currentP=$prior[0].positioning;$currentV=$prior[0].visible}
                if(-not (Test-AutoRect $change.positioning $currentP) -or -not (Test-AutoRect $change.visible $currentV)){++$unownedAfterEnd;$barrierFailure=$true}
            }
            if($change.unexpected_change){$barrierFailure=$true}
        }
    }
    $postRaw=@(if($exit.Count){$raw|Where-Object {$_.gesture -eq $gesture -and $_.cursor_sampled -and $_.receiver_qpc -gt $exit[0].receipt_qpc}})
    $guiClear=$handoff.Count -eq 1 -and (Test-EndHandoffAuthority $handoff[0] $owned $guard $s $true)
    $captureReleased=$exit.Count -eq 1 -and $exit[0].owner_capture -eq 0 -and @(Get-EndHandoffRows $Rows 'CAPTURE_CHANGED'|Where-Object {$_.new_capture -eq 0 -and $_.owner_capture -eq 0 -and $cancel.Count -eq 1 -and $_.sequence -gt $cancel[0].sequence}).Count -gt 0
    $barrier=Get-EndHandoffBarrierGate -CancelCalls $cancel.Count -NativeEnter ($enter.Count -eq 1) -NativeDrag ($drag.Count -gt 0) -AuthorityValid ($owned.Count -eq 1 -and $boot.result -ceq 'PASS') -CancelReturned ($returned.Count -eq 1 -and $returned[0].transport_success) -RealExit ($exit.Count -eq 1) -CaptureReleased $captureReleased -LeftHeldAtExit ($exit.Count -eq 1 -and $exit[0].left_down) -GuiClearAfterExit $guiClear -RawHealthy $rawEvidence.Healthy -RawContinuation ($postRaw.Count -gt 0) -NativeDragAfterReturn $nativeAfterReturn -UnattributedGeometryAfterEnd $unownedAfterEnd -WritesBeforeEnd $writesBeforeEnd
    if($barrierFailure){$barrier='FAIL'}
    if($terminal.Count){
        $t=$terminal[0];Assert-EndHandoff ($anchor.Count -eq 1 -and $handoffDone.Count -eq 1 -and $exit.Count -eq 1) 'takeover ended without handoff'
        foreach($name in @('raw_up_receiver_sequence','raw_up_receiver_qpc','native_drag_after_end','unowned_geometry_changes')){Assert-AutoInteger (Get-AutoField $t $name) "takeover END $name"}
        foreach($name in @('exact','pending_write','pending_motion')){Assert-AutoBoolean (Get-AutoField $t $name) "takeover END $name"}
        Assert-AutoPoint $t.final_cursor 'final true cursor';Assert-TakeoverGeometry $t
        $up=@($raw|Where-Object {$_.receiver_sequence -eq $t.raw_up_receiver_sequence -and $_.receiver_qpc -eq $t.raw_up_receiver_qpc -and $_.gesture -eq $gesture -and ($_.button_flags -band 2) -ne 0})
        $upCall=@($Rows|Where-Object {$_.type -ceq 'input' -and $_.gesture -eq $gesture -and $_.flags -eq 4 -and $_.sent -eq 1})
        $upValid=$up.Count -eq 1 -and $upCall.Count -eq 1 -and (Test-TakeoverUpReceipt $up[0] $upCall[0] $true)
        $finalTarget=Get-EndHandoffIntended $operation $anchor[0].start_positioning $anchor[0].start_visible $anchor[0].pointer_down $t.final_cursor
        $terminalAuthority=Test-EndHandoffAuthority $t $owned $guard $s $false -AllowTerminalUpLag
        $exact=(Test-AutoRect $t.positioning $finalTarget.Positioning) -and (Test-AutoRect $t.visible $finalTarget.Visible) -and (Test-AutoRect $t.intended_positioning $finalTarget.Positioning) -and (Test-AutoRect $t.intended_visible $finalTarget.Visible) -and $terminalAuthority -and $t.raw_up_seen
        Assert-EndHandoff ($t.exact -eq $exact -and $t.native_drag_after_end -eq $nativeAfterEnd -and $t.unowned_geometry_changes -eq $unownedAfterEnd) 'forged final geometry/native counters'
        $lastNative=@($result|Where-Object native_calls -eq 1|Sort-Object native_return_qpc|Select-Object -Last 1)
        if($upValid -and $lastNative.Count -and $lastNative[0].native_return_qpc -ge $up[0].receiver_qpc){$writerFailure=$true}
        $f=@($Rows|Where-Object {$_.type -ceq 'input_fence' -and $_.gesture -eq $gesture -and $pathDone.Count -eq 1 -and $_.sequence -lt $pathDone[0].sequence -and $upCall.Count -eq 1 -and $_.sequence -gt $upCall[0].sequence -and -not $_.left_down -and $_.gui_query_succeeded -and $_.capture_hwnd -eq 0 -and $_.menu_owner_hwnd -eq 0 -and $_.move_size_hwnd -eq 0 -and ($_.gui_flags -band 30) -eq 0 -and (Test-AutoPoint $_.cursor $t.final_cursor)})
        $takeover=Get-EndHandoffCompletionGate -ObservationComplete $true -RawUpObserved $upValid -FinalCursorValid ($f.Count -gt 0) -FinalGeometryExact $exact -PendingWrite ($t.pending_write -or $t.pending_motion) -GuiClear ($f.Count -gt 0) -ContinuationMovements $remaining -NativeDragAfterEnd $nativeAfterEnd
        if($takeover -ceq 'FAIL'){$writerFailure=$true}
        if($pathDone.Count){Assert-TakeoverGeometry $pathDone[0];if(-not (Test-AutoRect $pathDone[0].positioning $finalTarget.Positioning) -or -not (Test-AutoRect $pathDone[0].visible $finalTarget.Visible)){$writerFailure=$true}}
        else{$writerFailure=$true}
        if($takeover -ceq 'PASS' -and -not $writerFailure){$preRelease='PASS'}
    }
    foreach($sample in @(Get-EndHandoffRows $Rows 'sample')){
        Assert-AutoInteger $sample.index 'sample index';Assert-EndHandoff ($path.Count -eq 1 -and $sample.index -ge 1 -and $sample.index -le 20 -and @($Rows|Where-Object {$_.type -ceq 'sample' -and $_.index -eq $sample.index}).Count -eq 1) 'sample path/index'
        Assert-TakeoverGeometry $sample;Assert-AutoPoint $sample.cursor 'actual sample cursor'
        $planned=@(($path[0].start[0]+($path[0].end[0]-$path[0].start[0])*$sample.index/20),($path[0].start[1]+($path[0].end[1]-$path[0].start[1])*$sample.index/20))
        Assert-EndHandoff ((Test-AutoPoint $sample.cursor $planned) -and $sample.left_down -and $sample.post_cancel -eq ($sample.index -gt 2)) 'sample actual cursor/button/phase'
        if($sample.index -ge 3){
            $desired=Get-EndHandoffIntended $operation $anchor[0].start_positioning $anchor[0].start_visible $anchor[0].pointer_down $sample.cursor
            if(-not (Test-AutoRect $sample.positioning $desired.Positioning) -or -not (Test-AutoRect $sample.visible $desired.Visible) -or -not (Test-AutoRect $sample.intended_positioning $desired.Positioning) -or -not (Test-AutoRect $sample.intended_visible $desired.Visible)){$writerFailure=$true}
        }
    }
    if($writerFailure){$takeover='FAIL';$preRelease='FAIL'}
    foreach($failure in @(Get-EndHandoffRows $Rows 'takeover_failure')){
        Assert-EndHandoff ($blocked -and $failure.phase -cin @('handoff','raw_quantum') -and -not [string]::IsNullOrWhiteSpace($failure.reason)) 'invalid owner takeover failure record'
        if($handoff.Count -or $begin.Count){$writerFailure=$true;$takeover='FAIL';$preRelease='FAIL';if($failure.phase -ceq 'handoff'){$reconciliation='FAIL'}}
    }
    if($barrier -ceq 'FAIL'){$architecture='REJECTED_AT_END_BARRIER';$overall='FAIL'}
    elseif($writerFailure){$architecture='REJECTED_AT_TAKEOVER_WRITER';$overall='FAIL'}
    elseif($barrier -ceq 'PASS_WITH_TERMINAL_SETTLEMENT' -and $reconciliation -ceq 'PASS' -and $takeover -ceq 'PASS'){$architecture='VALID_AT_OWNED_STAGE';$overall='PASS'}
    else{$architecture='UNRESOLVED';$overall=if($rawEvidence.Gate -ceq 'FAIL'){'FAIL'}elseif($blocked){'BLOCKED'}else{'FAIL'}}
    if(-not $blocked){Assert-EndHandoff ($end.cursor_restored -and $end.receiver_stopped -and $terminal.Count -eq 1 -and $pathDone.Count -eq 1) 'complete run lacks final cleanup/lifecycle'}
    if($null -ne (Get-AutoField $end 'takeover_geometry_writes')){Assert-EndHandoff ($end.takeover_geometry_writes -eq $nativeCalls) 'shutdown write counter differs from actual attempted calls'}
    $reasons=@(Get-EndHandoffRows $Rows 'blocked'|ForEach-Object reason)
    if($blocked){Assert-EndHandoff ($reasons.Count -gt 0 -and @($reasons|Where-Object {[string]::IsNullOrWhiteSpace($_)}).Count -eq 0) 'blocked shutdown without reason'}
    if($returned.Count -and $exit.Count){$latencies.CancelReturnToEndTicks=$exit[0].receipt_qpc-$returned[0].returned_qpc}
    return [pscustomobject]@{Result=$overall;Operation=$operation;ForegroundContract=$s.foreground_contract;HandoffContract=$s.handoff_contract;RawBackground=$rawEvidence.Gate;Cancel=$barrier;Handoff=$reconciliation;Takeover=$takeover;PreReleaseControl=$preRelease;Architecture=$architecture;NativeWrites=$nativeCalls;HandoffNativeCalls=$handoffNativeCalls;RawPackets=$raw.Count;RawMovementPackets=@($raw|Where-Object cursor_sampled).Count;RawUpPackets=@($raw|Where-Object {($_.button_flags -band 2) -ne 0}).Count;ContinuationQuanta=$remaining;TerminalSettlements=$terminalSettlements;NativeDragAfterReturn=$nativeAfterReturn;NativeDragAfterEnd=$nativeAfterEnd;UnownedGeometryAfterEnd=$unownedAfterEnd;FullGestureTargets=$fullTargets;ExitRectUsedAsIntentAnchor=$false;PendingButton=$inputState.PendingButton;LatencyTicks=[pscustomobject]$latencies;Reasons=$reasons;SyntheticFixture=((Get-AutoField $s 'synthetic_fixture') -eq $true)}
}

function Test-EndHandoffOwnedEvidence([string]$Path,[switch]$AllowSynthetic){
    $rows=@(Get-Content -LiteralPath $Path -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json -ErrorAction Stop})
    return Test-EndHandoffOwnedRecords $rows -AllowSynthetic:$AllowSynthetic
}
