Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

# Reuse only the reviewed foreground evidence functions; this file does not
# launch the old probe, register input, or send native messages.
. (Join-Path $PSScriptRoot 'r1c4b-auto-owned-modal-validation.ps1')

function Assert-Takeover([bool]$Value,[string]$Reason){
    if(-not $Value){throw "owned takeover: $Reason"}
}

function Get-TakeoverRawBackgroundGate(
    [bool]$RegistrationObserved,
    [bool]$RegistrationSucceeded,
    [bool]$ReceiverIdentityValid,
    [bool]$BackgroundConfirmed,
    [bool]$MovementObserved,
    [bool]$UpObserved,
    [bool]$EvidenceHealthy,
    [bool]$StimulusComplete
){
    # A fully observed owned stimulus can fail this automatic Raw preflight.
    # An unattempted/incomplete stimulus remains BLOCKED, not a device claim.
    if($RegistrationObserved -and $RegistrationSucceeded -and
       $ReceiverIdentityValid -and $BackgroundConfirmed -and
       $MovementObserved -and $UpObserved -and $EvidenceHealthy){return 'PASS'}
    if($StimulusComplete -and $RegistrationObserved -and $RegistrationSucceeded -and
       $ReceiverIdentityValid -and $EvidenceHealthy){return 'FAIL'}
    return 'BLOCKED'
}

function Get-TakeoverMechanicalCancel(
    [int]$CancelCalls,
    [bool]$EnterObserved,
    [bool]$DragObserved,
    [bool]$ButtonHeldAtCancel,
    [bool]$AuthorityValid,
    [bool]$ApiCompleted,
    [bool]$ExitObserved,
    [bool]$CaptureChangeObserved,
    [bool]$CaptureReleased,
    [int]$RemainingCursorSamples,
    [int]$LaterNativeDragCount,
    [int]$LaterGeometryChangeCount,
    [bool]$ObservationComplete,
    [bool]$EvidenceHealthy,
    [bool]$VerifiedCancellationDeadlineFailure
){
    Assert-Takeover ($CancelCalls -ge 0 -and $RemainingCursorSamples -ge 0 -and
        $LaterNativeDragCount -ge 0 -and $LaterGeometryChangeCount -ge 0) 'negative cancellation counter'
    Assert-Takeover ($CancelCalls -le 1) 'more than one explicit WM_CANCELMODE'
    # An empty/blocked phase or incomplete native entry cannot become FAIL.
    if($CancelCalls -ne 1 -or -not $EnterObserved -or -not $DragObserved -or
       -not $ButtonHeldAtCancel -or -not $AuthorityValid -or -not $EvidenceHealthy){return 'UNKNOWN'}
    # These are positive, context-validated counterexamples, not inferences
    # from a timeout, a successful SendMessage return, or absent Raw Input.
    if($LaterNativeDragCount -gt 0 -or $LaterGeometryChangeCount -gt 0){return 'FAIL'}
    if($ApiCompleted -and $VerifiedCancellationDeadlineFailure){return 'FAIL'}
    if($ApiCompleted -and $ExitObserved -and $CaptureChangeObserved -and
       $CaptureReleased -and $RemainingCursorSamples -ge 15 -and
       $ObservationComplete){return 'PASS'}
    return 'UNKNOWN'
}

function Get-TakeoverCompleteCancel(
    [ValidateSet('PASS','FAIL','UNKNOWN')][string]$Mechanical,
    [ValidateSet('PASS','FAIL','BLOCKED')][string]$RawBackground,
    [bool]$RawContinuity,
    [bool]$RawUpAfterCancel,
    [bool]$EvidenceHealthy
){
    if($Mechanical -ceq 'FAIL'){return 'FAIL'}
    if($Mechanical -ceq 'PASS' -and $RawBackground -ceq 'PASS' -and
       $RawContinuity -and $RawUpAfterCancel -and $EvidenceHealthy){return 'PASS'}
    return 'UNKNOWN'
}

function Test-TakeoverEnvelope([object[]]$Records){
    Assert-Takeover ($Records.Count -ge 2) 'empty evidence'
    for($i=0;$i -lt $Records.Count;++$i){
        $row=$Records[$i]
        Assert-Takeover ($row.schema -ceq 'r1c4b-takeover-owned/v1') 'schema'
        Assert-AutoInteger $row.sequence 'sequence must be integer'
        Assert-AutoInteger $row.gesture 'gesture must be integer'
        Assert-AutoInteger $row.qpc 'QPC must be integer'
        Assert-Takeover ($row.sequence -eq $i+1 -and $row.gesture -ge 0 -and $row.qpc -ge 0) 'sequence/gesture/QPC'
        if($i){Assert-Takeover ($row.qpc -ge $Records[$i-1].qpc) 'QPC order'}
    }
    Assert-Takeover ($Records[0].type -ceq 'startup' -and $Records[-1].type -ceq 'shutdown' -and
        @($Records|Where-Object type -ceq startup).Count -eq 1 -and
        @($Records|Where-Object type -ceq shutdown).Count -eq 1) 'lifecycle envelope'
}

function Get-TakeoverRows($Records,[string]$Type){return @($Records|Where-Object type -ceq $Type)}
function Assert-TakeoverGeometry($Row){
    $null=Get-AutoRect $Row.positioning;$null=Get-AutoRect $Row.visible
    Assert-Takeover ($Row.positioning_error -eq 0 -and $Row.visible_hresult -ge 0) 'geometry capture failed'
}
function Test-TakeoverMotionReceipt($Raw,$Request,[bool]$AbsolutePolicy,[bool]$ExpectedHeld){
    if($Raw.receiver_qpc -lt $Request.injection_start_qpc -or -not $Raw.cursor_sampled -or -not $Raw.cursor_success){return $false}
    if($AbsolutePolicy){
        # Delivery proves the received test input, not that a screen-cursor
        # snapshot already reflects that input. No raw delta is integrated.
        return $Raw.receiver_sequence -gt $Request.receiver_watermark -and
            $Raw.test_tag_matches -and ($Raw.raw_flags -band 3) -eq 3 -and
            $Raw.dx -eq $Request.normalized_dx -and $Raw.dy -eq $Request.normalized_dy
    }
    return $Raw.left_down -eq $ExpectedHeld -and (Test-AutoPoint $Raw.cursor $Request.point)
}
function Test-TakeoverUpReceipt($Raw,$Request,[bool]$AbsolutePolicy){
    if(($Raw.button_flags -band 2) -eq 0 -or $Raw.receiver_qpc -lt $Request.injection_start_qpc){return $false}
    if($AbsolutePolicy){return $Raw.receiver_sequence -gt $Request.receiver_watermark -and $Raw.test_tag_matches}
    return -not $Raw.left_down
}
function Test-TakeoverLocalActivationPreparation($Records,$Owned,$Guard,[bool]$Blocked){
    $policy=Get-AutoField $Records[0] 'local_activation_policy';$preparations=@(Get-TakeoverRows $Records 'local_activation_preparation')
    if($null -eq $policy){Assert-Takeover ($preparations.Count -eq 0) 'unversioned local activation mutation';return}
    Assert-Takeover ($policy -ceq 'own_background_reset_v1' -and $preparations.Count -le 1) 'local activation policy/count'
    $attempt=@(Get-TakeoverRows $Records 'foreground_attempt')
    if(-not $attempt.Count){Assert-Takeover ($Blocked -and $preparations.Count -eq 0) 'local preparation before foreground attempt';return}
    Assert-Takeover ($attempt.Count -eq 1) 'duplicate foreground attempt'
    $show=@(Get-TakeoverRows $Records 'show_window');Assert-Takeover ($show.Count -eq 1 -and $show[0].requested_show -eq 4) 'local-reset bootstrap used an activating ShowWindow command'
    if($attempt[0].set_foreground_success){Assert-Takeover ($preparations.Count -eq 0) 'local mutation after direct foreground success';return}
    Assert-Takeover ($preparations.Count -eq 1 -and $Owned.Count -eq 1 -and $Guard.Count -eq 1) 'denied foreground lacks local preparation proof'
    $p=$preparations[0];$s=$Records[0]
    foreach($name in @('target','guard','source_pid','source_tid','before_active','before_focus','after_active','after_focus','foreground_before','foreground_after','gui_flags','capture_hwnd','menu_owner_hwnd','move_size_hwnd','focus_return','focus_error','active_return','active_error')){Assert-AutoInteger (Get-AutoField $p $name) "local preparation $name"}
    foreach($name in @('attempted','authorized','global_unchanged','local_cleared','success','gui_query_succeeded','buttons_modifiers_clear','active_called','own_identity_before','desktop_ready','own_identity_after')){Assert-AutoBoolean (Get-AutoField $p $name) "local preparation $name"}
    Assert-Takeover ($p.gesture -eq 0 -and $p.target -eq $Owned[0].hwnd -and $p.guard -eq $Guard[0].hwnd -and $p.source_pid -eq $s.pid -and $p.source_tid -eq $s.ui_tid -and $p.sequence -gt $attempt[0].sequence -and $p.sequence -gt $Owned[0].sequence) 'local reset not on exact owner UI thread/bound target'
    $local=@(0,$Owned[0].hwnd,$Guard[0].hwnd)
    $authorized=$p.own_identity_before -and $p.desktop_ready -and $p.foreground_before -gt 0 -and $p.foreground_before -notin $local -and $p.before_active -in $local -and $p.before_focus -in $local -and $p.gui_query_succeeded -and $p.capture_hwnd -eq 0 -and $p.menu_owner_hwnd -eq 0 -and $p.move_size_hwnd -eq 0 -and ($p.gui_flags -band 30) -eq 0 -and $p.buttons_modifiers_clear
    $attempted=$authorized -and ($p.before_active -ne 0 -or $p.before_focus -ne 0)
    $unchanged=$p.foreground_before -eq $p.foreground_after;$cleared=$p.after_active -eq 0 -and $p.after_focus -eq 0
    $success=$authorized -and $unchanged -and $cleared -and $p.own_identity_after -and (-not $attempted -or $p.active_called)
    Assert-Takeover ($p.authorized -eq $authorized -and $p.attempted -eq $attempted -and $p.global_unchanged -eq $unchanged -and $p.local_cleared -eq $cleared -and $p.success -eq $success -and (-not $p.active_called -or $p.attempted)) 'forged local preparation summary'
    if($p.success -and $p.attempted){Assert-Takeover $p.active_called 'successful active reset without active API call'}
    if(-not $p.attempted){Assert-Takeover ($p.focus_return -eq 0 -and $p.focus_error -eq 0 -and $p.active_return -eq 0 -and $p.active_error -eq 0 -and -not $p.active_called) 'unattempted reset has native call results'}
    $activation=@(Get-TakeoverRows $Records 'activation_input')
    if($activation.Count){Assert-Takeover ($p.success -and @($activation|Where-Object sequence -le $p.sequence).Count -eq 0) 'click before authorized successful local preparation'}
    if(-not $p.success){Assert-Takeover ($Blocked -and $activation.Count -eq 0) 'failed local preparation continued input'}
}
function Assert-TakeoverDiagnosticHandle($Value,[string]$Name){
    Assert-AutoInteger $Value "diagnostic $Name"
    Assert-Takeover ($Value -ge 0 -and [decimal]$Value -le [decimal][UInt64]::MaxValue) "bounded diagnostic $Name"
}
function Test-TakeoverGlobalForegroundV2($Records,$Owned,[bool]$Blocked){
    Assert-Takeover ((Get-AutoField $Records[0] 'local_activation_policy') -eq $null -and @(Get-TakeoverRows $Records 'local_activation_preparation').Count -eq 0) 'v2 used the removed local activation prerequisite'
    $summaries=@(Get-TakeoverRows $Records 'foreground_bootstrap');Assert-Takeover ($summaries.Count -eq 1) 'v2 missing/duplicate bootstrap'
    $b=$summaries[0]
    foreach($name in @('set_foreground_attempted','set_foreground_success','activation_click_required','temporary_topmost','window_from_point_root_matches','foreign_capture_clear','sendinput_move_success','sendinput_down_success','sendinput_up_success','wm_activate_seen','wm_setfocus_seen','activation_event_seen','final_foreground_matches','topmost_restored','topmost_now')){Assert-AutoBoolean (Get-AutoField $b $name) "v2 bootstrap $name"}
    Assert-Takeover ($b.result -cin @('PASS','BLOCKED') -and $b.gesture -eq 0 -and (($Owned.Count -eq 1 -and $b.target -eq $Owned[0].hwnd) -or ($Blocked -and $Owned.Count -eq 0 -and $b.target -eq 0))) 'v2 bootstrap scope'
    Assert-Takeover ($b.final_foreground_matches -eq ($b.target -gt 0 -and $b.foreground -eq $b.target) -and (-not $b.topmost_restored -or -not $b.topmost_now)) 'v2 forged final foreground/style summary'
    $attempt=@(Get-TakeoverRows $Records 'foreground_attempt')
    Assert-Takeover ($attempt.Count -eq [int]$b.set_foreground_attempted) 'v2 attempt count'
    foreach($a in $attempt){
        Assert-AutoBoolean $a.set_foreground_success 'v2 setter result'
        Assert-AutoInteger $a.target 'v2 setter target';Assert-AutoInteger $a.foreground 'v2 setter foreground'
        Assert-Takeover ($a.target -eq $b.target -and $a.sequence -lt $b.sequence -and $a.set_foreground_success -eq $b.set_foreground_success) 'v2 attempt identity/order'
        Assert-TakeoverDiagnosticHandle $a.source_thread_local_active 'source thread active';Assert-TakeoverDiagnosticHandle $a.source_thread_local_focus 'source thread focus'
    }
    $ready=@(Get-TakeoverRows $Records 'foreground_ready');Assert-Takeover ($ready.Count -le 1) 'v2 duplicate foreground proof'
    $validReady=$false
    if($ready.Count){
        $r=$ready[0]
        foreach($name in @('own_identity','desktop_ready','visible','gui_query_succeeded','buttons_modifiers_clear','foreground_snapshot_stable','topmost_now','success')){Assert-AutoBoolean (Get-AutoField $r $name) "v2 ready $name"}
        foreach($name in @('target','source_pid','source_tid','foreground','foreground_pid','foreground_tid','gui_flags','capture_hwnd','menu_owner_hwnd','move_size_hwnd')){Assert-AutoInteger (Get-AutoField $r $name) "v2 ready $name"}
        Assert-TakeoverDiagnosticHandle $r.source_thread_local_active 'ready thread active';Assert-TakeoverDiagnosticHandle $r.source_thread_local_focus 'ready thread focus'
        Assert-Takeover ($Owned.Count -eq 1 -and $attempt.Count -eq 1 -and $r.gesture -eq 0 -and $r.target -eq $Owned[0].hwnd -and $r.source_pid -eq $Owned[0].pid -and $r.source_tid -eq $Owned[0].tid -and $r.sequence -gt $Owned[0].sequence -and $r.sequence -gt $attempt[0].sequence -and $r.sequence -lt $b.sequence) 'v2 ready identity/order'
        $validReady=$r.own_identity -and $r.desktop_ready -and $r.visible -and $r.foreground -eq $r.target -and $r.foreground_pid -eq $r.source_pid -and $r.foreground_tid -eq $r.source_tid -and $r.foreground_snapshot_stable -and $r.gui_query_succeeded -and $r.capture_hwnd -eq 0 -and $r.menu_owner_hwnd -eq 0 -and $r.move_size_hwnd -eq 0 -and ($r.gui_flags -band 30) -eq 0 -and $r.buttons_modifiers_clear -and -not $r.topmost_now
        Assert-Takeover ($r.success -eq $validReady) 'v2 forged ready success'
    }
    $inputs=@(Get-TakeoverRows $Records 'activation_input');$proofs=@(Get-TakeoverRows $Records 'activation_fence');$releases=@(Get-TakeoverRows $Records 'activation_release')
    Assert-Takeover ($inputs.Count -le 3 -and $releases.Count -le 1) 'v2 extra click/release'
    $visibility=@(Get-TakeoverRows $Records 'activation_visibility');$raise=@($visibility|Where-Object enabled -eq $true);$restore=@($visibility|Where-Object enabled -eq $false)
    Assert-Takeover ($raise.Count -le 1 -and $restore.Count -le 1) 'v2 visibility retries'
    foreach($v in $visibility){
        foreach($name in @('enabled','success','topmost_style','visible')){Assert-AutoBoolean (Get-AutoField $v $name) "v2 visibility $name"}
        foreach($name in @('target','flags')){Assert-AutoInteger (Get-AutoField $v $name) "v2 visibility $name"}
        Assert-Takeover ($Owned.Count -eq 1 -and $v.target -eq $b.target -and $v.gesture -eq 0 -and $v.sequence -lt $b.sequence -and $v.flags -eq $(if($v.enabled){83}else{19})) 'v2 visibility fence'
        if($v.success){Assert-Takeover ($v.topmost_style -eq $v.enabled) 'v2 visibility style mismatch'}
    }
    $previous=0;$failed=$false;$phases=@('move','down','up');$flags=@(1,2,4)
    for($i=0;$i -lt $inputs.Count;++$i){
        $request=$inputs[$i]
        foreach($name in @('flags','sent','error','input_tag','injection_start_qpc','injection_return_qpc')){Assert-AutoInteger (Get-AutoField $request $name) "v2 click $name"}
        Assert-Takeover (-not $failed -and $b.activation_click_required -and $request.gesture -eq 0 -and $request.phase -ceq $phases[$i] -and $request.flags -eq $flags[$i] -and $request.sent -in @(0,1) -and $request.sequence -lt $b.sequence) 'v2 activation order/scope'
        $f=@($proofs|Where-Object {$_.phase -ceq $request.phase -and $_.sequence -gt $previous -and $_.sequence -lt $request.sequence})
        Assert-Takeover ($f.Count -eq 1 -and $raise.Count -eq 1 -and $raise[0].success -and $raise[0].topmost_style -and $raise[0].visible -and $raise[0].sequence -lt $request.sequence) 'v2 click without fresh visibility/proof'
        foreach($name in @('own_identity','desktop_ready','same_integrity','visible','temporary_topmost','window_from_point_root_matches','foreign_capture_clear','modifiers_clear','button_state_matches','left_down','gui_query_succeeded','foreground_snapshot_stable')){Assert-AutoBoolean (Get-AutoField $f[0] $name) "v2 activation $name"}
        $clear=$f[0].gui_query_succeeded -and $f[0].foreground_snapshot_stable -and ($f[0].gui_flags -band 30) -eq 0 -and $f[0].capture_hwnd -eq 0 -and $f[0].menu_owner_hwnd -eq 0 -and $f[0].move_size_hwnd -eq 0
        Assert-Takeover ($f[0].foreign_capture_clear -eq $clear -and $f[0].window_from_point_root_matches -eq ($f[0].own_identity -and $f[0].window_from_point_root -eq $b.target)) 'v2 forged click GUI/root proof'
        Assert-AutoActivationProof $f[0] $Owned $b.activation_point ($i -eq 2)
        Assert-Takeover ($request.input_tag -eq 0x50424D41 -and (Test-AutoPoint $request.point $b.activation_point) -and $f[0].qpc -le $request.injection_start_qpc -and $request.injection_start_qpc -le $request.injection_return_qpc -and $request.injection_return_qpc -le $request.qpc) 'v2 click tag/time/point'
        if($request.sent -eq 1){Assert-Takeover ($request.error -eq 0) 'v2 click native error'}else{$failed=$true;Assert-Takeover $Blocked 'v2 input failure without block'}
        $previous=$request.sequence
    }
    $sentMove=$inputs.Count -gt 0 -and $inputs[0].sent -eq 1;$sentDown=$inputs.Count -gt 1 -and $inputs[1].sent -eq 1;$sentUp=$inputs.Count -gt 2 -and $inputs[2].sent -eq 1
    Assert-Takeover ($b.sendinput_move_success -eq $sentMove -and $b.sendinput_down_success -eq $sentDown -and $b.sendinput_up_success -eq $sentUp) 'v2 forged click summary'
    foreach($release in $releases){
        Assert-Takeover ($Blocked -and $sentDown -and -not $sentUp -and $release.sequence -gt $inputs[-1].sequence -and $release.sequence -lt $b.sequence -and $release.sent -in @(0,1)) 'v2 release without pending button'
        if($release.attempted){
            $f=@($proofs|Where-Object {$_.phase -ceq 'up' -and $_.sequence -gt $inputs[-1].sequence -and $_.sequence -lt $release.sequence})
            Assert-Takeover ($f.Count -ge 1) 'v2 cleanup lacks proof';Assert-AutoActivationProof $f[-1] $Owned $b.activation_point $true
            Assert-Takeover ($release.input_tag -eq 0x50424D41 -and $f[-1].qpc -le $release.injection_start_qpc -and $release.injection_start_qpc -le $release.injection_return_qpc -and $release.injection_return_qpc -le $release.qpc -and (Test-AutoPoint $release.point $b.activation_point)) 'v2 cleanup tag/time'
        }else{Assert-Takeover ($release.sent -eq 0 -and $release.button_release_pending -and -not [string]::IsNullOrWhiteSpace($release.reason)) 'v2 unsafe skipped cleanup'}
        Assert-Takeover (@($restore|Where-Object sequence -lt $release.sequence).Count -eq 0) 'v2 retired visibility before cleanup'
    }
    $buttons=@(Get-TakeoverRows $Records 'activation_button')
    foreach($button in $buttons){
        Assert-Takeover ($button.target -eq $b.target -and $button.gesture -eq 0 -and $button.message -cin @('WM_LBUTTONDOWN','WM_LBUTTONUP') -and $button.sequence -lt $b.sequence) 'v2 actual click receipt identity'
        $calls=@(if($button.message -ceq 'WM_LBUTTONDOWN'){$inputs|Where-Object {$_.phase -ceq 'down' -and $_.sent -eq 1}}else{$inputs|Where-Object {$_.phase -ceq 'up' -and $_.sent -eq 1};$releases|Where-Object {$_.attempted -and $_.sent -eq 1}})
        Assert-Takeover ($calls.Count -eq 1 -and $button.qpc -ge $calls[0].injection_start_qpc) 'v2 receipt without actual tagged call'
    }
    if($b.result -ceq 'PASS'){
        Assert-Takeover ($validReady -and $b.final_foreground_matches -and $b.topmost_restored) 'v2 no fresh exact global foreground authority'
        if($b.activation_click_required){
            Assert-Takeover (-not $b.set_foreground_success -and $sentMove -and $sentDown -and $sentUp -and $raise.Count -eq 1 -and $restore.Count -eq 1 -and $restore[0].success -and -not $restore[0].topmost_style -and $restore[0].visible -and $restore[0].sequence -gt $inputs[-1].sequence -and $restore[0].sequence -lt $ready[0].sequence) 'v2 fallback input/restore completion'
            foreach($message in @('WM_LBUTTONDOWN','WM_LBUTTONUP')){Assert-Takeover (@($buttons|Where-Object message -ceq $message).Count -eq 1) 'v2 missing/duplicate intended click receipt'}
        }else{Assert-Takeover ($b.set_foreground_success -and $inputs.Count -eq 0 -and $visibility.Count -eq 0) 'v2 direct path not verified'}
    }else{Assert-Takeover ($Blocked -and -not [string]::IsNullOrWhiteSpace($b.reason)) 'v2 blocked without reason'}
    foreach($r in @($Records|Where-Object {$_.type -cin @('path','ENTER','DRAG','input')})){Assert-Takeover ($b.result -ceq 'PASS' -and $r.sequence -gt $b.sequence) 'v2 native/input before global foreground proof'}
    $b|Add-Member -NotePropertyName validated_direct_success -NotePropertyValue ($b.set_foreground_success -and $validReady) -Force
    $b|Add-Member -NotePropertyName validated_activation_attempts -NotePropertyValue $inputs.Count -Force
    return $b
}
function Test-TakeoverInputs($Records,$Owned,$Guard,$Bootstrap,[bool]$Blocked){
    $previous=0;$down=$false;$failed=$false;$primed=@{}
    $absolute=(Get-AutoField $Records[0] 'input_correlation') -ceq 'actual_absolute_receipt_v1'
    $expected=if($Owned.Count -eq 1){if($Bootstrap.activation_click_required){$Bootstrap.activation_point}else{$Owned[0].saved_cursor}}else{$null}
    foreach($inputRow in @(Get-TakeoverRows $Records 'input')){
        Assert-Takeover (-not $failed -and $Owned.Count -eq 1 -and $Bootstrap.result -ceq 'PASS' -and $Bootstrap.sequence -lt $inputRow.sequence) 'input without active owned foreground authority'
        $proof=@($Records|Where-Object {$_.type -ceq 'input_fence' -and $_.sequence -gt $previous -and $_.sequence -lt $inputRow.sequence})
        Assert-Takeover ($proof.Count -gt 0) 'input without fresh fence'
        $f=$proof[-1]
        $pendingValue=Get-AutoField $f 'cancel_pending';$pending=$false
        if($null -ne $pendingValue){Assert-AutoBoolean $pendingValue 'pending cancellation flag';$pending=$pendingValue}
        foreach($name in @('left_down','post_cancel','priming','gui_query_succeeded')){Assert-AutoBoolean (Get-AutoField $f $name) "takeover fence $name"}
        foreach($name in @('foreground','target','gui_flags','menu_owner_hwnd','move_size_hwnd','capture_hwnd','window_from_point_root')){Assert-AutoInteger (Get-AutoField $f $name) "takeover fence $name"}
        Assert-Takeover ($f.gesture -eq $inputRow.gesture -and $f.gui_query_succeeded -and $f.foreground -eq $Owned[0].hwnd -and $f.target -eq $Owned[0].hwnd -and $f.left_down -eq $down -and (Test-AutoPoint $f.cursor $f.expected_cursor)) 'input identity/cursor/button/GUI proof'
        Assert-Takeover (Test-AutoPoint $f.expected_cursor $expected) 'fence does not reconcile the preceding actual test input'
        Assert-AutoInteger $inputRow.flags 'input flags';Assert-AutoInteger $inputRow.sent 'input sent';Assert-AutoInteger $inputRow.error 'input error'
        Assert-Takeover ($inputRow.flags -in @(1,2,4) -and $inputRow.sent -in @(0,1) -and $inputRow.input_tag -eq 0x50424D41) 'input kind/tag/count'
        Assert-Takeover ($f.qpc -le $inputRow.injection_start_qpc -and $inputRow.injection_start_qpc -le $inputRow.injection_return_qpc -and $inputRow.injection_return_qpc -le $inputRow.qpc) 'input clock order'
        Assert-AutoBoolean $inputRow.restoring_cursor 'restoring cursor flag'
        if($absolute){
            foreach($name in @('normalized_dx','normalized_dy','actual_mouse_flags','receiver_watermark')){Assert-AutoInteger (Get-AutoField $inputRow $name) "absolute input $name"}
            Assert-Takeover ($inputRow.receiver_watermark -ge 1 -and (Test-AutoRect $inputRow.virtual_screen $Owned[0].virtual_screen)) 'input screen/watermark'
            if($inputRow.flags -eq 1){
                $screen=$inputRow.virtual_screen;$width=[decimal]$screen[2]-[decimal]$screen[0];$height=[decimal]$screen[3]-[decimal]$screen[1]
                Assert-Takeover ($width -gt 1 -and $height -gt 1 -and $inputRow.point[0] -ge $screen[0] -and $inputRow.point[0] -lt $screen[2] -and $inputRow.point[1] -ge $screen[1] -and $inputRow.point[1] -lt $screen[3]) 'input outside virtual desktop'
                $dx=[Math]::Floor((2*([decimal]$inputRow.point[0]-[decimal]$screen[0])+1)*65536/(2*$width));$dy=[Math]::Floor((2*([decimal]$inputRow.point[1]-[decimal]$screen[1])+1)*65536/(2*$height))
                Assert-Takeover ($inputRow.actual_mouse_flags -eq 57345 -and $inputRow.normalized_dx -eq $dx -and $inputRow.normalized_dy -eq $dy) 'normalized INPUT is not the requested virtual-screen pixel centre'
            }else{Assert-Takeover ($inputRow.actual_mouse_flags -eq $inputRow.flags -and $inputRow.normalized_dx -eq 0 -and $inputRow.normalized_dy -eq 0) 'button INPUT contains unapproved movement'}
        }
        if($inputRow.restoring_cursor){
            $restore=@($Records|Where-Object {$_.type -ceq 'cursor_restore_begin' -and $_.sequence -lt $inputRow.sequence -and $_.sequence -gt $previous})
            Assert-Takeover ($inputRow.flags -eq 1 -and -not $down -and $restore.Count -eq 1 -and $restore[0].target -eq $Owned[0].hwnd -and (Test-AutoPoint $inputRow.point $Owned[0].saved_cursor) -and (Test-AutoPoint $restore[0].point $Owned[0].saved_cursor) -and @($Records|Where-Object {$_.type -ceq 'path_complete' -and $_.sequence -lt $restore[0].sequence}).Count -eq 2) 'unauthorized cursor restoration'
        }elseif($inputRow.flags -eq 1 -and ($inputRow.gesture -eq 0 -or $f.post_cancel)){
            $destination=@($Records|Where-Object {$_.type -ceq 'destination_fence' -and $_.sequence -gt $f.sequence -and $_.sequence -lt $inputRow.sequence})
            Assert-Takeover ($destination.Count -eq 1 -and $Guard.Count -eq 1) 'movement without fresh destination authority'
            $d=$destination[0]
            $destinationPending=Get-AutoField $d 'cancel_pending';if($null -eq $destinationPending){$destinationPending=$false}else{Assert-AutoBoolean $destinationPending 'destination pending flag'}
            Assert-Takeover ($destinationPending -eq $pending) 'destination changed pending authority'
            $guiSafe=($d.move_size_hwnd -eq 0 -and ($d.gui_flags -band 30) -eq 0) -or ($pending -and $d.move_size_hwnd -eq $Owned[0].hwnd -and ($d.gui_flags -band 28) -eq 0)
            Assert-Takeover ($d.target -eq $Owned[0].hwnd -and $d.foreground -eq $Owned[0].hwnd -and $d.root -in @($Owned[0].hwnd,$Guard[0].hwnd) -and $d.capture_hwnd -eq 0 -and $d.menu_owner_hwnd -eq 0 -and $guiSafe -and $d.left_down -eq $down -and (Test-AutoPoint $d.point $inputRow.point) -and $d.qpc -le $inputRow.injection_start_qpc) 'destination proof failed'
        }
        if($pending){
            $returned=@($Records|Where-Object {$_.type -ceq 'cancel_return' -and $_.gesture -eq $inputRow.gesture -and $_.sequence -lt $f.sequence})
            $path=@($Records|Where-Object {$_.type -ceq 'path' -and $_.gesture -eq $inputRow.gesture -and $_.sequence -lt $f.sequence})
            Assert-Takeover ($inputRow.gesture -in @(1,2) -and $down -and $inputRow.flags -eq 1 -and $f.post_cancel -and $returned.Count -eq 1 -and $path.Count -eq 1 -and $returned[0].transport_success -and $returned[0].gui_query_succeeded -and $returned[0].capture_after -eq 0 -and $returned[0].left_down) 'pending input without completed exact cancel call'
            $priorMoves=@($Records|Where-Object {$_.type -ceq 'input' -and $_.gesture -eq $inputRow.gesture -and $_.flags -eq 1 -and $_.sequence -gt $returned[0].sequence -and $_.sequence -lt $inputRow.sequence})
            $third=@(($path[0].start[0]+($path[0].end[0]-$path[0].start[0])*3/20),($path[0].start[1]+($path[0].end[1]-$path[0].start[1])*3/20))
            Assert-Takeover ($priorMoves.Count -eq 0 -and (Test-AutoPoint $inputRow.point $third) -and @($Records|Where-Object {$_.type -ceq 'cancel_confirmed' -and $_.gesture -eq $inputRow.gesture -and $_.sequence -lt $f.sequence}).Count -eq 0) 'pending authority outside the single planned third MOVE'
        }
        if($inputRow.gesture -eq 0 -or $f.post_cancel){
            $guiSafe=($f.move_size_hwnd -eq 0 -and ($f.gui_flags -band 30) -eq 0) -or ($pending -and $f.move_size_hwnd -eq $Owned[0].hwnd -and ($f.gui_flags -band 28) -eq 0)
            Assert-Takeover ($f.capture_hwnd -eq 0 -and $f.menu_owner_hwnd -eq 0 -and $guiSafe) 'uncaptured input has foreign GUI state'
            if($down){Assert-Takeover ($Guard.Count -eq 1 -and $f.window_from_point_root -in @($Owned[0].hwnd,$Guard[0].hwnd)) 'held postcancel input outside owned windows'}
        }elseif($down){
            if($f.priming){
                Assert-Takeover ($inputRow.flags -eq 1 -and -not $primed.ContainsKey($inputRow.gesture) -and $f.capture_hwnd -in @(0,$Owned[0].hwnd) -and $f.window_from_point_root -eq $Owned[0].hwnd) 'invalid priming fence'
                Assert-Takeover (@($Records|Where-Object {$_.type -ceq 'ENTER' -and $_.gesture -eq $inputRow.gesture -and $_.sequence -lt $f.sequence}).Count -eq 0) 'priming after ENTER'
                $nativeDown=@($Records|Where-Object {$_.type -ceq 'native_button_down' -and $_.gesture -eq $inputRow.gesture -and $_.sequence -lt $f.sequence})
                Assert-Takeover ($nativeDown.Count -eq 1 -and $nativeDown[0].target -eq $Owned[0].hwnd) 'priming without real native DOWN'
                $primed[$inputRow.gesture]=$true
            }else{Assert-Takeover ($f.capture_hwnd -eq $Owned[0].hwnd) 'native held input without owned capture'}
        }
        if($f.post_cancel -and $inputRow.gesture -gt 0){
            Assert-Takeover (@($Records|Where-Object {$_.type -ceq 'cancel_return' -and $_.gesture -eq $inputRow.gesture -and $_.sequence -lt $f.sequence}).Count -eq 1) 'postcancel input before cancel call returned'
        }
        Assert-Takeover (($inputRow.flags -ne 2 -or -not $down) -and ($inputRow.flags -ne 4 -or $down)) 'input DOWN/UP order'
        if($inputRow.sent -eq 1){
            Assert-Takeover ($inputRow.error -eq 0) 'successful input has native error'
            if($inputRow.flags -eq 2){$down=$true};if($inputRow.flags -eq 4){$down=$false}
            if($inputRow.flags -eq 1){$expected=$inputRow.point}
        }else{Assert-Takeover $Blocked 'failed input without blocked shutdown';$failed=$true}
        $previous=$inputRow.sequence
    }
    foreach($cleanup in @(Get-TakeoverRows $Records 'cleanup_release')){
        Assert-Takeover ($Blocked -and $down -and $cleanup.sent -in @(0,1)) 'cleanup release without pending DOWN'
        $proof=@($Records|Where-Object {$_.type -ceq 'cleanup_fence' -and $_.sequence -gt $previous -and $_.sequence -lt $cleanup.sequence})
        Assert-Takeover ($proof.Count -eq 1) 'cleanup release without fresh proof'
        $f=$proof[0]
        Assert-Takeover ($Owned.Count -eq 1 -and $f.target -eq $Owned[0].hwnd -and $f.foreground -eq $Owned[0].hwnd -and $f.own_identity -and $f.desktop_ready -and $f.visible -and $f.left_down -and $f.modifiers_clear) 'cleanup authority'
        Assert-Takeover ($f.menu_owner_hwnd -eq 0 -and $f.move_size_hwnd -in @(0,$Owned[0].hwnd) -and ($f.gui_flags -band 28) -eq 0 -and (($f.capture_hwnd -eq $Owned[0].hwnd) -or ($f.capture_hwnd -eq 0 -and $Guard.Count -eq 1 -and $f.root -in @($Owned[0].hwnd,$Guard[0].hwnd)))) 'cleanup capture/root proof'
        Assert-Takeover ($cleanup.target -eq $Owned[0].hwnd -and $cleanup.input_tag -eq 0x50424D41 -and $f.qpc -le $cleanup.injection_start_qpc -and $cleanup.injection_start_qpc -le $cleanup.injection_return_qpc -and $cleanup.injection_return_qpc -le $cleanup.qpc) 'cleanup tag/clock'
        if($cleanup.sent -eq 1){Assert-Takeover ($cleanup.error -eq 0) 'cleanup native error';$down=$false}
        $previous=$cleanup.sequence
    }
    if(-not $Blocked){Assert-Takeover (-not $down) 'completed evidence retains held input'}
    return [pscustomobject]@{PendingButton=$down;Inputs=@(Get-TakeoverRows $Records 'input').Count}
}

function Test-TakeoverOwnedRecords([object[]]$Records,[switch]$AllowSynthetic){
    Test-TakeoverEnvelope $Records
    $s=$Records[0];$end=$Records[-1];$blocked=$end.result -ceq 'BLOCKED'
    $policy=Get-AutoField $s 'input_correlation';$absolute=$policy -ceq 'actual_absolute_receipt_v1';$globalV2=$s.foreground_contract -ceq 'verified_global_foreground_v2'
    Assert-Takeover ($null -eq $policy -or $absolute) 'unknown input correlation policy'
    foreach($name in @('human_input','real_explorer','sendinput_in_probe')){Assert-AutoBoolean (Get-AutoField $s $name) "startup $name"}
    foreach($name in @('pid','ui_tid','qpc_frequency','takeover_geometry_writes')){Assert-AutoInteger (Get-AutoField $s $name) "startup $name"}
    Assert-Takeover ($s.evidence_kind -ceq 'automated_owned_cancel' -and $s.mode -ceq 'cancel_only' -and $s.human_input -eq $false -and $s.real_explorer -eq $false -and $s.sendinput_in_probe -eq $true -and $s.takeover_geometry_writes -eq 0 -and $s.qpc_frequency -gt 0) 'scope/authority'
    if((Get-AutoField $s 'synthetic_fixture') -eq $true){Assert-Takeover ([bool]$AllowSynthetic) 'synthetic evidence is not native acceptance'}
    Assert-Takeover ($end.result -cin @('BLOCKED','CAPTURED_NOT_ACCEPTED')) 'shutdown result'
    foreach($name in @('owned_window_destroyed','guard_window_destroyed','receiver_stopped','external_windows_touched','cursor_restored')){Assert-AutoBoolean (Get-AutoField $end $name) "shutdown $name"}
    Assert-Takeover ($end.owned_window_destroyed -and $end.guard_window_destroyed -and -not $end.external_windows_touched) 'unsafe owned cleanup'
    $allowed=@('startup','shutdown','desktop_gate','guard','owned','receiver','receiver_shutdown','receiver_error','show_window','foreground_attempt','foreground_ready','foreground_bootstrap','activation_event','activation_button','activation_visibility','activation_fence','activation_input','activation_release','local_activation_preparation','input_fence','input','destination_fence','cursor_restore_begin','cleanup_fence','cleanup_release','raw_preflight_begin','raw_preflight_outcome','raw_preflight_complete','raw_input','raw_cursor_status','raw_wait_begin','raw_wait_result','raw_motion_correlation','LEGACY_MOVE','native_button_down','path','sample','path_complete','ENTER','DRAG','EXIT','CAPTURE_CHANGED','POSITION_CHANGED','cancel_begin','cancel_return','cancel_exit_wait','cancel_message','cancel_confirmed','blocked')
    foreach($r in $Records){
        Assert-Takeover ($r.type -cin $allowed) 'unknown record/geometry writer in cancel-only evidence'
        foreach($name in @('native_calls','takeover_geometry_writes','geometry_writes')){
            $value=Get-AutoField $r $name;if($null -ne $value){Assert-AutoInteger $value "counter $name";Assert-Takeover ($value -eq 0) 'geometry writes in cancel-only mode'}
        }
    }
    $owned=@(Get-TakeoverRows $Records 'owned');$guard=@(Get-TakeoverRows $Records 'guard');$receiver=@(Get-TakeoverRows $Records 'receiver');$desktop=@(Get-TakeoverRows $Records 'desktop_gate')
    Assert-Takeover ($owned.Count -le 1 -and $guard.Count -le 1 -and $receiver.Count -le 1 -and $desktop.Count -le 1) 'duplicate identity records'
    if($owned.Count){Assert-Takeover ($owned[0].pid -eq $s.pid -and $owned[0].tid -eq $s.ui_tid -and $owned[0].hwnd -gt 0) 'owned identity';Assert-TakeoverGeometry $owned[0]}
    if($guard.Count){Assert-Takeover ($guard[0].pid -eq $s.pid -and $guard[0].tid -eq $s.ui_tid -and $guard[0].hwnd -gt 0 -and ($owned.Count -eq 0 -or $guard[0].hwnd -ne $owned[0].hwnd)) 'guard identity'}
    if($desktop.Count){Assert-Takeover ($desktop[0].active_unlocked -eq $true -and $desktop[0].input_desktop_matches -eq $true) 'desktop evidence'}
    if($globalV2){$bootstrap=Test-TakeoverGlobalForegroundV2 $Records $owned $blocked}
    else{Test-TakeoverLocalActivationPreparation $Records $owned $guard $blocked;$bootstrap=Test-AutoForegroundBootstrap $Records $owned $blocked}
    $inputs=Test-TakeoverInputs $Records $owned $guard $bootstrap $blocked
    $serial=0;$receiverClock=0
    $raw=@(Get-TakeoverRows $Records 'raw_input');$receiverEnd=@(Get-TakeoverRows $Records 'receiver_shutdown');$receiverErrors=@(Get-TakeoverRows $Records 'receiver_error')
    foreach($r in @($Records|Where-Object {$_.type -cin @('receiver','raw_input','receiver_shutdown') -or ($_.type -ceq 'receiver_error' -and $null -ne (Get-AutoField $_ 'receiver_sequence'))})){
        Assert-Takeover ($receiver.Count -eq 1) 'receiver stream without registration'
        foreach($name in @('receiver_sequence','receiver_qpc','receiver_hwnd','receiver_pid','receiver_tid')){Assert-AutoInteger (Get-AutoField $r $name) "receiver $name"}
        Assert-Takeover ($r.receiver_sequence -eq ++$serial -and $r.receiver_qpc -ge $receiverClock -and $r.receiver_qpc -le $r.qpc -and $r.receiver_pid -eq $receiver[0].receiver_pid -and $r.receiver_tid -eq $receiver[0].receiver_tid -and $r.receiver_hwnd -eq $receiver[0].receiver_hwnd) 'receiver sequence/clock/identity'
        $receiverClock=$r.receiver_qpc
    }
    if($receiver.Count){
        $r=$receiver[0]
        Assert-Takeover ($r.receiver_pid -ne $s.pid -and $r.receiver_pid -gt 0 -and $r.receiver_tid -gt 0 -and $r.receiver_hwnd -gt 0 -and $r.usage_page -eq 1 -and $r.usage -eq 2 -and $r.registration_flags -eq 256 -and $r.keyboard_registered -eq $false) 'mouse-only background receiver identity/registration'
        Assert-AutoBoolean $r.registration_verified 'registration verification'
        Assert-Takeover ($receiverEnd.Count -eq 1 -and $receiverEnd[0].registration_removed -eq $true -and $receiverEnd[0].window_destroyed -eq $true -and $end.receiver_stopped) 'receiver cleanup incomplete'
    }else{Assert-Takeover ($raw.Count -eq 0 -and $receiverEnd.Count -eq 0) 'raw data without receiver'}
    foreach($r in $raw){
        foreach($name in @('input_code','raw_flags','dx','dy','button_flags','foreground_hwnd','foreground_pid')){Assert-AutoInteger (Get-AutoField $r $name) "raw $name"}
        foreach($name in @('cursor_sampled','cursor_success','left_down','device_handle_present','test_tag_matches')){Assert-AutoBoolean (Get-AutoField $r $name) "raw $name"}
        Assert-Takeover ($owned.Count -eq 1 -and $r.input_code -eq 1 -and $r.foreground_pid -eq $s.pid -and $r.foreground_pid -ne $r.receiver_pid -and $r.foreground_hwnd -eq $owned[0].hwnd -and $r.raw_flags -ge 0 -and $r.raw_flags -le 65535 -and $r.button_flags -ge 0 -and $r.button_flags -le 65535) 'raw input background/context proof'
        $sampled=(($r.raw_flags -band 1) -ne 0 -or $r.dx -ne 0 -or $r.dy -ne 0)
        Assert-Takeover ($r.cursor_sampled -eq $sampled) 'forged event-triggered cursor sampling'
        if($r.cursor_sampled -and $r.cursor_success){Assert-AutoPoint $r.cursor 'raw cursor'}else{Assert-Takeover ($null -eq $r.cursor) 'raw cursor without a successful event sample'}
    }
    foreach($status in @(Get-TakeoverRows $Records 'raw_cursor_status')){
        $packet=@($raw|Where-Object receiver_sequence -eq $status.receiver_sequence)
        Assert-Takeover ($packet.Count -eq 1 -and $status.receiver_pid -eq $packet[0].receiver_pid -and $status.receiver_tid -eq $packet[0].receiver_tid -and $status.receiver_hwnd -eq $packet[0].receiver_hwnd -and $status.receiver_qpc -eq $packet[0].receiver_qpc) 'cursor diagnostic without actual raw packet'
        Assert-AutoInteger $status.cursor_error 'raw cursor diagnostic error'
    }
    if($absolute){
        $waitBegins=@(Get-TakeoverRows $Records 'raw_wait_begin')
        for($wi=0;$wi -lt $waitBegins.Count;++$wi){
            $begin=$waitBegins[$wi];$limit=if($wi+1 -lt $waitBegins.Count){$waitBegins[$wi+1].sequence}else{$end.sequence}
            $request=@($Records|Where-Object {$_.type -ceq 'input' -and $_.flags -eq 1 -and $_.sequence -lt $begin.sequence -and $_.injection_start_qpc -eq $begin.input_start_qpc})
            $response=@($Records|Where-Object {$_.type -ceq 'raw_wait_result' -and $_.sequence -gt $begin.sequence -and $_.sequence -lt $limit})
            Assert-Takeover ($request.Count -eq 1 -and $response.Count -eq 1 -and $begin.timeout_ms -eq 2000 -and (Test-AutoPoint $begin.expected_cursor $request[0].point)) 'raw wait without exact actual INPUT receipt'
            $correlation=@($Records|Where-Object {$_.type -ceq 'raw_motion_correlation' -and $_.sequence -gt $response[0].sequence -and $_.sequence -lt $limit})
            if($response[0].wait_result -eq 0){
                Assert-Takeover ($correlation.Count -eq 1) 'successful raw wait lacks actual packet correlation'
                $c=$correlation[0];$packet=@($raw|Where-Object receiver_sequence -eq $c.receiver_sequence)
                Assert-AutoBoolean $c.delivered 'delivery correlation boolean';Assert-AutoBoolean $c.snapshot_cursor_matches 'cursor snapshot correlation boolean'
                Assert-Takeover ($packet.Count -eq 1 -and $packet[0].sequence -lt $c.sequence) 'correlation refers to absent raw packet'
                $delivered=Test-TakeoverMotionReceipt $packet[0] $request[0] $true $false
                $snapshot=$packet[0].cursor_sampled -and $packet[0].cursor_success -and (Test-AutoPoint $packet[0].cursor $request[0].point)
                Assert-Takeover ($c.delivered -eq $delivered -and $c.snapshot_cursor_matches -eq $snapshot) 'forged delivery/screen snapshot correlation'
                if(-not $c.delivered){Assert-Takeover $blocked 'undelivered raw motion without block'}
            }else{Assert-Takeover ($blocked -and $correlation.Count -eq 0) 'failed raw wait continued'}
        }
    }
    $beg=@(Get-TakeoverRows $Records 'raw_preflight_begin');$done=@(Get-TakeoverRows $Records 'raw_preflight_complete');$outcome=@(Get-TakeoverRows $Records 'raw_preflight_outcome')
    Assert-Takeover ($beg.Count -le 1 -and $done.Count -le 1 -and $outcome.Count -le 1) 'duplicate raw preflight'
    Assert-Takeover ($globalV2 -or $outcome.Count -eq 0) 'unversioned raw preflight outcome'
    $rawGate='BLOCKED';$preflightProven=$false
    if($done.Count -or $outcome.Count){
        $boundary=if($outcome.Count){$outcome[0]}else{$done[0]}
        Assert-Takeover ($beg.Count -eq 1 -and $beg[0].gesture -eq 0 -and $boundary.gesture -eq 0 -and $beg[0].sequence -lt $boundary.sequence) 'raw preflight lifecycle'
        if($done.Count){Assert-Takeover ($done[0].gesture -eq 0 -and $done[0].sequence -ge $boundary.sequence) 'raw preflight completion order'}
        $preInputs=@($Records|Where-Object {$_.type -ceq 'input' -and $_.sequence -gt $beg[0].sequence -and $_.sequence -lt $boundary.sequence})
        Assert-Takeover ($preInputs.Count -eq 4 -and ($preInputs.flags -join ',') -ceq '1,2,1,4' -and @($preInputs|Where-Object sent -ne 1).Count -eq 0) 'preflight move/down/held-move/up'
        Assert-Takeover ((Test-AutoPoint $preInputs[0].point $beg[0].point) -and $preInputs[2].point[0]-$beg[0].point[0] -eq 12 -and $preInputs[2].point[1] -eq $beg[0].point[1]) 'preflight safe finite path'
        $first=@($raw|Where-Object {$_.gesture -eq 0 -and $_.receiver_qpc -le $preInputs[1].injection_start_qpc -and (Test-TakeoverMotionReceipt $_ $preInputs[0] $absolute $false)})
        $held=@($raw|Where-Object {$_.gesture -eq 0 -and $_.receiver_qpc -le $preInputs[3].injection_start_qpc -and (Test-TakeoverMotionReceipt $_ $preInputs[2] $absolute $true)})
        $up=@($raw|Where-Object {$_.gesture -eq 0 -and $_.sequence -lt $boundary.sequence -and (Test-TakeoverUpReceipt $_ $preInputs[3] $absolute)})
        $upFence=@($Records|Where-Object {$_.type -ceq 'input_fence' -and $_.sequence -gt $preInputs[3].sequence -and $_.sequence -lt $boundary.sequence -and $_.left_down -eq $false -and $_.foreground -eq $owned[0].hwnd -and (Test-AutoPoint $_.cursor $preInputs[2].point)})
        if($absolute){Assert-Takeover ($upFence.Count -gt 0) 'raw UP delivery without later actual driver-up fence'}
        $stimulusComplete=$false
        if($outcome.Count){
            $o=$outcome[0]
            foreach($name in @('attempted','actual_move_verified','actual_held_move_verified','actual_up_verified','input_delivery_verified','observation_completed','receiver_healthy','raw_up_observed')){Assert-AutoBoolean (Get-AutoField $o $name) "preflight outcome $name"}
            foreach($name in @('up_wait_result','up_wait_started_qpc','up_wait_finished_qpc','up_timeout_ms')){Assert-AutoInteger (Get-AutoField $o $name) "preflight outcome $name"}
            Assert-Takeover ($o.up_wait_result -in @(0,258) -and $o.up_timeout_ms -eq 2000 -and $o.up_wait_started_qpc -ge $preInputs[3].injection_return_qpc -and $o.up_wait_finished_qpc -ge $o.up_wait_started_qpc -and $o.up_wait_finished_qpc -le $o.qpc) 'preflight outcome wait boundary'
            if($o.up_wait_result -eq 258){Assert-Takeover ([decimal]($o.up_wait_finished_qpc-$o.up_wait_started_qpc)*1000 -ge [decimal]$o.up_timeout_ms*$s.qpc_frequency) 'forged raw UP timeout'}
            $f=$upFence[-1]
            Assert-Takeover ($f.target -eq $owned[0].hwnd -and $f.gui_query_succeeded -and $f.capture_hwnd -eq 0 -and $f.menu_owner_hwnd -eq 0 -and $f.move_size_hwnd -eq 0 -and ($f.gui_flags -band 30) -eq 0 -and (Test-AutoPoint $f.expected_cursor $preInputs[2].point)) 'preflight outcome lacks clean exact final fence'
            $stimulusComplete=$o.attempted -and $o.actual_move_verified -and $o.actual_held_move_verified -and $o.actual_up_verified -and $o.input_delivery_verified -and $o.observation_completed -and $o.receiver_healthy
            Assert-Takeover ($stimulusComplete -and $o.raw_up_observed -eq ($up.Count -gt 0) -and $o.raw_up_observed -eq ($o.up_wait_result -eq 0)) 'forged/incomplete actual preflight outcome'
        }
        if($done.Count){
            Assert-AutoInteger $done[0].movement_count 'preflight movement count';Assert-AutoInteger $done[0].up_count 'preflight up count'
            Assert-Takeover ($done[0].movement_count -ge 2 -and $done[0].movement_count -le @($raw|Where-Object cursor_sampled).Count -and $done[0].up_count -ge 1 -and $done[0].up_count -le @($raw|Where-Object {($_.button_flags -band 2) -ne 0}).Count) 'preflight summary without actual raw evidence'
        }
        $rawGate=Get-TakeoverRawBackgroundGate -RegistrationObserved ($receiver.Count -eq 1) -RegistrationSucceeded ($receiver.Count -eq 1 -and $receiver[0].registration_verified) -ReceiverIdentityValid ($receiver.Count -eq 1) -BackgroundConfirmed ($raw.Count -gt 0) -MovementObserved ($first.Count -gt 0 -and $held.Count -gt 0) -UpObserved ($up.Count -gt 0) -EvidenceHealthy ($receiverErrors.Count -eq 0 -and $end.receiver_stopped) -StimulusComplete $stimulusComplete
        $preflightProven=$rawGate -ceq 'PASS'
        if($done.Count){Assert-Takeover $preflightProven 'complete preflight lacks real background movement/up'}
        else{Assert-Takeover ($blocked -and $rawGate -ceq 'FAIL') 'preflight outcome without completion lacks a reliable negative result'}
    }
    $mechanical=@('UNKNOWN','UNKNOWN');$complete=@('UNKNOWN','UNKNOWN');$facts=@()
    for($g=1;$g -le 2;++$g){
        $rows=@($Records|Where-Object gesture -eq $g)
        $path=@(Get-TakeoverRows $rows 'path');$cancel=@(Get-TakeoverRows $rows 'cancel_begin');$return=@(Get-TakeoverRows $rows 'cancel_return');$confirmed=@(Get-TakeoverRows $rows 'cancel_confirmed');$exit=@(Get-TakeoverRows $rows 'EXIT');$final=@(Get-TakeoverRows $rows 'path_complete');$enter=@(Get-TakeoverRows $rows 'ENTER');$drag=@(Get-TakeoverRows $rows 'DRAG');$message=@(Get-TakeoverRows $rows 'cancel_message');$changed=@(Get-TakeoverRows $rows 'CAPTURE_CHANGED');$samples=@(Get-TakeoverRows $rows 'sample');$wait=@(Get-TakeoverRows $rows 'cancel_exit_wait')
        foreach($collection in @($path,$cancel,$return,$confirmed,$exit,$final,$enter,$wait)){Assert-Takeover ($collection.Count -le 1) 'duplicate native lifecycle/cancel record'}
        if($path.Count -or $cancel.Count){Assert-Takeover ($preflightProven -and $path.Count -eq 1 -and $path[0].sequence -gt $done[0].sequence) 'native stage before proven background Raw Input'}
        foreach($d in $drag){Assert-Takeover ($d.event -ceq $(if($g -eq 1){'WM_MOVING'}else{'WM_SIZING'}) -and ($g -eq 1 -or $d.edge -eq 6)) 'wrong native operation/edge'}
        if(-not $cancel.Count){continue}
        Assert-Takeover ($path[0].samples -eq 20 -and $path[0].cancel_after_sample -eq 2 -and $path[0].interval_ms -eq 30 -and $path[0].hit_test -eq $(if($g -eq 1){2}else{15}) -and $path[0].foreground -eq $owned[0].hwnd) 'cancel input path contract'
        Assert-Takeover ($cancel[0].target -eq $owned[0].hwnd -and $cancel[0].source_tid -eq $s.ui_tid -and $cancel[0].capture_before -eq $owned[0].hwnd -and $cancel[0].left_down -eq $true) 'cancel exact target/capture/button authority'
        Assert-Takeover ($enter.Count -eq 1 -and $enter[0].sequence -gt $path[0].sequence -and $enter[0].sequence -lt $cancel[0].sequence -and @($drag|Where-Object sequence -lt $cancel[0].sequence).Count -ge 2) 'cancel before true native ENTER/DRAG'
        if($return.Count){
            Assert-Takeover ($return[0].sequence -gt $cancel[0].sequence -and $return[0].issued_qpc -le $cancel[0].qpc -and $cancel[0].qpc -le $return[0].returned_qpc -and $return[0].returned_qpc -le $return[0].qpc) 'cancel clock order'
            foreach($name in @('transport_success','gui_query_succeeded','left_down')){Assert-AutoBoolean (Get-AutoField $return[0] $name) "cancel return $name"}
            Assert-AutoInteger $return[0].recipient_result 'cancel recipient result';Assert-AutoInteger $return[0].capture_after 'cancel capture after'
            if($return[0].transport_success){Assert-Takeover ($return[0].error -eq 0) 'successful cancel transport has error'}
        }
        $issuedMessages=@($message|Where-Object {$_.sequence -gt $cancel[0].sequence -and ($return.Count -eq 0 -or $_.sequence -lt $return[0].sequence)})
        foreach($msg in $message){Assert-Takeover ($msg.target -eq $owned[0].hwnd) 'cancel recipient identity'}
        if($return.Count -and $return[0].transport_success){Assert-Takeover ($issuedMessages.Count -eq 1 -and $issuedMessages[0].left_down) 'explicit cancel recipient context'}
        $knownMode=$false;$verifiedDeadlineFailure=$false
        if($wait.Count){
            $w=$wait[0]
            foreach($name in @('started_qpc','finished_qpc','timeout_ms','wait_result','target','source_tid','foreground','capture_hwnd','move_size_hwnd','gui_flags')){Assert-AutoInteger (Get-AutoField $w $name) "cancel deadline $name"}
            foreach($name in @('gui_query_succeeded','source_identity','left_down')){Assert-AutoBoolean (Get-AutoField $w $name) "cancel deadline $name"}
            Assert-Takeover ($return.Count -eq 1 -and $w.sequence -gt $return[0].sequence -and $w.started_qpc -ge $return[0].returned_qpc -and $w.finished_qpc -ge $w.started_qpc -and $w.finished_qpc -le $w.qpc -and $w.timeout_ms -eq 2000 -and $w.target -eq $owned[0].hwnd -and $w.source_tid -eq $s.ui_tid) 'cancel deadline operation boundary'
            if($w.wait_result -eq 258){Assert-Takeover ([decimal]($w.finished_qpc-$w.started_qpc)*1000 -ge [decimal]$w.timeout_ms*$s.qpc_frequency) 'forged timeout duration'}
            if($null -ne (Get-AutoField $w 'observation_wakeup_sample')){
                $third=@(($path[0].start[0]+($path[0].end[0]-$path[0].start[0])*3/20),($path[0].start[1]+($path[0].end[1]-$path[0].start[1])*3/20))
                $wakeup=@($rows|Where-Object {$_.type -ceq 'input' -and $_.flags -eq 1 -and $_.sent -eq 1 -and $_.injection_start_qpc -gt $return[0].returned_qpc -and $_.injection_return_qpc -lt $w.started_qpc -and (Test-AutoPoint $_.point $third)})
                Assert-Takeover ($w.observation_wakeup_sample -eq 3 -and $wakeup.Count -eq 1) 'cancel exit wait lacks the single planned third MOVE'
            }
            $knownMode=$w.wait_result -eq 258 -and $w.gui_query_succeeded -and $w.source_identity -and $w.foreground -eq $owned[0].hwnd -and $w.left_down -and ($w.gui_flags -band 2) -ne 0 -and $w.move_size_hwnd -eq $owned[0].hwnd -and $return[0].transport_success -and $issuedMessages.Count -eq 1 -and @($exit|Where-Object sequence -lt $w.sequence).Count -eq 0
            if($globalV2){
                foreach($name in @('desktop_ready','visible','buttons_modifiers_clear','cursor_success','cursor_matches_expected','receiver_healthy')){Assert-AutoBoolean (Get-AutoField $w $name) "v2 cancel deadline $name"}
                Assert-AutoInteger $w.menu_owner_hwnd 'v2 deadline menu'
                Assert-AutoPoint $w.cursor 'v2 deadline cursor';Assert-AutoPoint $w.expected_cursor 'v2 deadline expected cursor'
                Assert-Takeover ($w.cursor_matches_expected -eq ($w.cursor_success -and (Test-AutoPoint $w.cursor $w.expected_cursor))) 'forged cancel deadline cursor proof'
                $actualWake=@($rows|Where-Object {$_.type -ceq 'input' -and $_.flags -eq 1 -and $_.sent -eq 1 -and $_.injection_start_qpc -gt $return[0].returned_qpc -and $_.injection_return_qpc -lt $w.started_qpc -and (Test-AutoPoint $_.point $w.expected_cursor)})
                $wakeRaw=@(if($actualWake.Count -eq 1){$raw|Where-Object {$_.gesture -eq $g -and $_.receiver_qpc -le $w.started_qpc -and (Test-TakeoverMotionReceipt $_ $actualWake[0] $absolute $true)}})
                # Fix A permits one complete negative. A timer alone, stale
                # source context or unobserved next movement still proves none.
                $verifiedDeadlineFailure=$knownMode -and $rawGate -ceq 'PASS' -and $w.desktop_ready -and $w.visible -and $w.buttons_modifiers_clear -and $w.cursor_matches_expected -and $w.receiver_healthy -and $w.menu_owner_hwnd -eq 0 -and $w.capture_hwnd -in @(0,$owned[0].hwnd) -and ($w.gui_flags -band 28) -eq 0 -and $actualWake.Count -eq 1 -and $wakeRaw.Count -gt 0 -and (Get-AutoField $w 'observation_wakeup_sample') -eq 3
            }
        }
        foreach($sample in $samples){
            Assert-AutoInteger $sample.index 'sample index';Assert-AutoBoolean $sample.post_cancel 'sample postcancel';Assert-AutoBoolean $sample.left_down 'sample button';Assert-TakeoverGeometry $sample
            Assert-Takeover ($sample.index -ge 1 -and $sample.index -le 20 -and @($samples|Where-Object index -eq $sample.index).Count -eq 1) 'sample index/uniqueness'
            $expected=@(($path[0].start[0]+($path[0].end[0]-$path[0].start[0])*$sample.index/20),($path[0].start[1]+($path[0].end[1]-$path[0].start[1])*$sample.index/20))
            Assert-Takeover ((Test-AutoPoint $sample.cursor $expected) -and $sample.left_down -and $sample.post_cancel -eq ($sample.index -gt 2)) 'sample cursor/button/phase'
        }
        $post=@($samples|Where-Object post_cancel);$laterDrag=0;$geometryChanges=0;$stableVisible=$true;$continuity=$false;$rawUp=$false
        if($return.Count){
            $postMoves=@($rows|Where-Object {$_.type -ceq 'input' -and $_.flags -eq 1 -and $_.sent -eq 1 -and -not $_.restoring_cursor -and $_.injection_start_qpc -gt $return[0].returned_qpc})
            foreach($d in @($drag|Where-Object qpc -gt $return[0].returned_qpc)){
                $stimulus=@($postMoves|Where-Object {$_.injection_start_qpc -le $d.qpc -and (Test-AutoPoint $_.point $d.cursor)})
                if($stimulus.Count){++$laterDrag}
            }
        }
        $returnGeometry=$return.Count -eq 1 -and $null -ne (Get-AutoField $return[0] 'positioning')
        if($returnGeometry){
            Assert-TakeoverGeometry $return[0]
            foreach($r in @($rows|Where-Object {$_.qpc -gt $return[0].returned_qpc -and $_.type -cin @('POSITION_CHANGED','sample','cancel_confirmed','path_complete')})){
                if(@($postMoves|Where-Object injection_start_qpc -le $r.qpc).Count -eq 0){continue}
                Assert-TakeoverGeometry $r
                if(-not (Test-AutoRect $r.positioning $return[0].positioning)){++$geometryChanges}
                if(-not (Test-AutoRect $r.visible $return[0].visible)){$stableVisible=$false}
            }
        }
        if($confirmed.Count){
            Assert-Takeover ($return.Count -eq 1 -and $exit.Count -eq 1 -and $exit[0].sequence -gt $cancel[0].sequence -and $exit[0].sequence -lt $confirmed[0].sequence -and $exit[0].left_down -and $exit[0].owner_capture -eq 0 -and $return[0].capture_after -eq 0 -and $return[0].gui_query_succeeded) 'confirmation without actual EXIT/released capture while down'
            Assert-TakeoverGeometry $confirmed[0]
            foreach($r in @($rows|Where-Object {$_.sequence -gt $confirmed[0].sequence -and $_.type -cin @('sample','POSITION_CHANGED','path_complete')})){
                Assert-TakeoverGeometry $r
                if(-not $returnGeometry){
                    if(-not (Test-AutoRect $r.positioning $confirmed[0].positioning)){++$geometryChanges}
                    if(-not (Test-AutoRect $r.visible $confirmed[0].visible)){$stableVisible=$false}
                }
            }
            $correlated=0
            foreach($sample in $post){
                $move=@($rows|Where-Object {$_.type -ceq 'input' -and $_.flags -eq 1 -and $_.sequence -lt $sample.sequence -and (Test-AutoPoint $_.point $sample.cursor)})
                Assert-Takeover ($move.Count -eq 1) 'postcancel sample without exact finite MOVE'
                $witness=@($raw|Where-Object {$_.gesture -eq $g -and $_.receiver_qpc -le $sample.qpc -and (Test-TakeoverMotionReceipt $_ $move[0] $absolute $true)})
                if($witness.Count){++$correlated}
            }
            $continuity=$post.Count -eq 18 -and $correlated -eq 18
            $upCall=@($rows|Where-Object {$_.type -ceq 'input' -and $_.flags -eq 4 -and $_.sent -eq 1 -and $_.sequence -gt $confirmed[0].sequence})
            if($upCall.Count -eq 1 -and $final.Count -eq 1){$rawUp=@($raw|Where-Object {$_.gesture -eq $g -and $_.sequence -lt $final[0].sequence -and (Test-TakeoverUpReceipt $_ $upCall[0] $absolute)}).Count -gt 0
                if($absolute){$rawUp=$rawUp -and @($rows|Where-Object {$_.type -ceq 'input_fence' -and $_.sequence -gt $upCall[0].sequence -and $_.sequence -lt $final[0].sequence -and $_.left_down -eq $false -and (Test-AutoPoint $_.cursor $path[0].end)}).Count -gt 0}}
        }
        $mechanical[$g-1]=Get-TakeoverMechanicalCancel -CancelCalls $cancel.Count -EnterObserved ($enter.Count -eq 1) -DragObserved ($drag.Count -gt 0) -ButtonHeldAtCancel $cancel[0].left_down -AuthorityValid $true -ApiCompleted ($return.Count -eq 1 -and $return[0].transport_success) -ExitObserved ($confirmed.Count -eq 1) -CaptureChangeObserved (@($changed|Where-Object {$_.sequence -gt $cancel[0].sequence -and $_.new_capture -eq 0 -and $_.owner_capture -eq 0}).Count -gt 0) -CaptureReleased ($confirmed.Count -eq 1) -RemainingCursorSamples $post.Count -LaterNativeDragCount $laterDrag -LaterGeometryChangeCount $geometryChanges -ObservationComplete ($samples.Count -eq 20 -and $final.Count -eq 1) -EvidenceHealthy ($receiverErrors.Count -eq 0) -VerifiedCancellationDeadlineFailure $verifiedDeadlineFailure
        $complete[$g-1]=Get-TakeoverCompleteCancel -Mechanical $mechanical[$g-1] -RawBackground $rawGate -RawContinuity $continuity -RawUpAfterCancel $rawUp -EvidenceHealthy ($stableVisible -and $receiverErrors.Count -eq 0)
        $facts+=[pscustomobject]@{Gesture=$g;Mechanical=$mechanical[$g-1];Complete=$complete[$g-1];RemainingSamples=$post.Count;RawContinuity=$continuity;RawUp=$rawUp;LaterNativeDrag=$laterDrag;GeometryChanges=$geometryChanges;VisibleStable=$stableVisible;KnownNativeModeStillActive=$knownMode;VerifiedCancellationDeadlineFailure=$verifiedDeadlineFailure}
    }
    $architecture=if($complete -contains 'FAIL'){'REJECTED_AT_CANCEL_STAGE'}else{'UNRESOLVED'}
    $result=if($rawGate -ceq 'FAIL' -or $complete -contains 'FAIL'){'FAIL'}elseif($blocked){'BLOCKED'}elseif($rawGate -ceq 'PASS' -and $complete[0] -ceq 'PASS' -and $complete[1] -ceq 'PASS'){'CAPTURED'}else{'FAIL'}
    if(-not $blocked){Assert-Takeover ($end.cursor_restored -and $end.receiver_stopped) 'completed evidence missing cleanup'}
    $reasons=@(Get-TakeoverRows $Records 'blocked'|ForEach-Object {$_.reason})
    if($blocked){Assert-Takeover ($reasons.Count -gt 0 -and @($reasons|Where-Object {[string]::IsNullOrWhiteSpace($_)}).Count -eq 0) 'blocked shutdown without reason'}
    return [pscustomobject]@{Result=$result;ForegroundContract=$s.foreground_contract;RawBackground=$rawGate;MechanicalMove=$mechanical[0];MechanicalResize=$mechanical[1];CancelMove=$complete[0];CancelResize=$complete[1];Architecture=$architecture;TakeoverMove='NOT_RUN';TakeoverResize='NOT_RUN';GeometryWrites=0;RawPackets=$raw.Count;RawMovementPackets=@($raw|Where-Object cursor_sampled).Count;RawUpPackets=@($raw|Where-Object {($_.button_flags -band 2) -ne 0}).Count;PendingButton=$inputs.PendingButton;Facts=$facts;Reasons=$reasons;SyntheticFixture=((Get-AutoField $s 'synthetic_fixture') -eq $true)}
}

function Test-TakeoverOwnedEvidence([string]$Path,[switch]$AllowSynthetic){
    $rows=@(Get-Content -LiteralPath $Path -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json -ErrorAction Stop})
    return Test-TakeoverOwnedRecords $rows -AllowSynthetic:$AllowSynthetic
}
