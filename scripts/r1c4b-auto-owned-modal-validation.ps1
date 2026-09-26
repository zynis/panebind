Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
function Assert-AutoModal([bool]$Value,[string]$Reason){if(-not $Value){throw "auto owned modal: $Reason"}}
function Get-AutoRect($Value){
    Assert-AutoModal (@($Value).Count -eq 4) 'rectangle required'
    foreach($n in $Value){Assert-AutoModal ($n -is [ValueType] -and [decimal]$n -eq [Math]::Truncate([decimal]$n)) 'integer rectangle'}
    Assert-AutoModal ($Value[2] -gt $Value[0] -and $Value[3] -gt $Value[1]) 'positive rectangle'
    return ($Value -join ',')
}
function Test-AutoRect($A,$B){return (Get-AutoRect $A) -ceq (Get-AutoRect $B)}
function Get-AutoField($Row,[string]$Name){
    $property=$Row.PSObject.Properties[$Name]
    if($null -eq $property){return $null};return $property.Value
}
function Assert-AutoBoolean($Value,[string]$Reason){Assert-AutoModal ($Value -is [bool]) $Reason}
function Assert-AutoInteger($Value,[string]$Reason){
    Assert-AutoModal ($Value -is [ValueType] -and $Value -isnot [bool] -and [decimal]$Value -eq [Math]::Truncate([decimal]$Value)) $Reason
}
function Assert-AutoPoint($Value,[string]$Reason){
    Assert-AutoModal (@($Value).Count -eq 2) $Reason
    foreach($n in $Value){Assert-AutoInteger $n $Reason}
}
function Test-AutoPoint($A,$B){
    Assert-AutoPoint $A 'integer point required';Assert-AutoPoint $B 'integer point required'
    return [Math]::Abs($A[0]-$B[0]) -le 1 -and [Math]::Abs($A[1]-$B[1]) -le 1
}
function Assert-AutoActivationProof($F,$Owned,$Point,[bool]$Held){
    Assert-AutoModal ($Owned.Count -eq 1 -and $F.target -eq $Owned[0].hwnd -and $F.own_identity -and $F.desktop_ready -and $F.same_integrity -and $F.visible -and $F.temporary_topmost -and $F.modifiers_clear -and $F.button_state_matches -and $F.gui_query_succeeded -and $F.foreground_snapshot_stable) 'activation authority'
    Assert-AutoModal ($F.window_from_point_root -eq $Owned[0].hwnd -and $F.window_from_point_root_matches -and $F.hit_test -eq 1 -and $F.foreign_capture_clear) 'activation point/GUI authority'
    Assert-AutoModal ($F.input_tag -eq 0x50424D41 -and $F.left_down -eq $Held -and (Test-AutoPoint $F.activation_point $Point)) 'activation tag/button/point state'
    if($F.phase -cne 'move'){Assert-AutoModal (Test-AutoPoint $F.cursor $Point) 'activation cursor after move'}
}
function Test-AutoNormalInputs($Records,$Owned,[bool]$Blocked,$Bootstrap){
    $inputs=@($Records|Where-Object type -ceq input);$previous=0;$down=$false;$failed=$false;$primed=@{}
    foreach($inputRow in $inputs){
        Assert-AutoModal (-not $failed) 'normal input after SendInput failure'
        Assert-AutoModal ($Owned.Count -eq 1) 'normal input without owned identity'
        if($null -ne $Bootstrap){Assert-AutoModal ($Bootstrap.result -ceq 'PASS' -and $Bootstrap.sequence -lt $inputRow.sequence) 'normal input before successful bootstrap'}
        $proof=@($Records|Where-Object {$_.type -ceq 'input_fence' -and $_.sequence -gt $previous -and $_.sequence -lt $inputRow.sequence})
        Assert-AutoModal ($proof.Count -gt 0) 'input without fresh fence'
        $f=$proof[-1]
        Assert-AutoBoolean $f.left_down 'normal fence button state must be boolean'
        Assert-AutoModal ($f.foreground -eq $Owned[0].hwnd -and $f.target -eq $Owned[0].hwnd -and (Test-AutoPoint $f.cursor $f.expected_cursor) -and $f.left_down -eq $down) 'normal input fence/button state'
        if($null -ne $Bootstrap){Assert-AutoModal ($f.sequence -gt $Bootstrap.sequence) 'normal input reused activation authority'}
        Assert-AutoModal ($inputRow.flags -in @(1,2,4)) 'unexpected normal input flags'
        if($null -ne $Bootstrap){
            Assert-AutoModal ($inputRow.input_tag -eq 0x50424D41) 'normal input tag'
            Assert-AutoBoolean $f.priming 'normal priming state must be boolean'
            Assert-AutoBoolean $f.gui_query_succeeded 'normal GUI query must be boolean'
            if($down){Assert-AutoModal $f.gui_query_succeeded 'normal held input without GUI proof'}
            Assert-AutoInteger $f.capture_hwnd 'normal capture HWND must be integer';Assert-AutoInteger $f.window_from_point_root 'normal root HWND must be integer'
            Assert-AutoModal ($f.qpc -le $inputRow.injection_start_qpc -and $inputRow.injection_start_qpc -le $inputRow.injection_return_qpc -and $inputRow.injection_return_qpc -le $inputRow.qpc) 'normal input clock order'
            if($f.priming){
                Assert-AutoModal ($down -and $inputRow.flags -eq 1 -and -not $primed.ContainsKey($inputRow.gesture) -and $f.capture_hwnd -in @(0,$Owned[0].hwnd) -and $f.window_from_point_root -eq $Owned[0].hwnd) 'invalid/repeated priming fence'
                $path=@($Records|Where-Object {$_.type -ceq 'path' -and $_.gesture -eq $inputRow.gesture -and $_.sequence -lt $f.sequence})
                Assert-AutoModal ($path.Count -eq 1 -and @($Records|Where-Object {$_.gesture -eq $inputRow.gesture -and $_.type -ceq 'ENTER' -and $_.sequence -lt $f.sequence}).Count -eq 0) 'priming outside pre-ENTER path'
                Assert-AutoModal (@($inputs|Where-Object {$_.gesture -eq $inputRow.gesture -and $_.flags -eq 1 -and $_.sequence -gt $path[0].sequence -and $_.sequence -lt $inputRow.sequence}).Count -eq 0) 'priming after first move sample'
                $button=@($inputs|Where-Object {$_.gesture -eq $inputRow.gesture -and $_.flags -eq 2 -and $_.sequence -gt $path[0].sequence -and $_.sequence -lt $inputRow.sequence})
                $nativeDown=@($Records|Where-Object {$_.type -ceq 'native_button_down' -and $_.gesture -eq $inputRow.gesture -and $_.sequence -gt $path[0].sequence -and $_.sequence -lt $f.sequence})
                Assert-AutoModal ($button.Count -eq 1 -and $nativeDown.Count -eq 1 -and $nativeDown[0].target -eq $Owned[0].hwnd -and $nativeDown[0].hit_test -eq $path[0].hit_test -and $nativeDown[0].qpc -ge $button[0].injection_start_qpc) 'priming without real native button down'
                $primed[$inputRow.gesture]=$true
            }elseif($down){Assert-AutoModal ($f.capture_hwnd -eq $Owned[0].hwnd) 'normal drag without exact capture'}
        }
        Assert-AutoInteger $inputRow.sent 'normal input sent must be integer';Assert-AutoInteger $inputRow.error 'normal input error must be integer'
        Assert-AutoModal ($inputRow.sent -in @(0,1)) 'normal input sent count'
        Assert-AutoModal (($inputRow.flags -ne 2 -or -not $down) -and ($inputRow.flags -ne 4 -or $down)) 'normal down/up order'
        if($inputRow.sent -eq 1){
            Assert-AutoModal ($inputRow.error -eq 0) 'successful normal input has error'
            if($inputRow.flags -eq 2){$down=$true};if($inputRow.flags -eq 4){$down=$false}
        }else{Assert-AutoModal $Blocked 'SendInput failure';$failed=$true}
        $previous=$inputRow.sequence
    }
    # Even an unused recorded normal fence must not claim foreign authority.
    foreach($f in @($Records|Where-Object type -ceq input_fence)){
        Assert-AutoModal ($Owned.Count -eq 1 -and $f.foreground -eq $Owned[0].hwnd -and $f.target -eq $Owned[0].hwnd -and (Test-AutoPoint $f.cursor $f.expected_cursor)) 'input fence'
        Assert-AutoBoolean $f.left_down 'normal fence button state must be boolean'
    }
}
function Test-AutoForegroundBootstrap($Records,$Owned,[bool]$Blocked){
    $s=$Records[0];$contract=Get-AutoField $s 'foreground_contract'
    $rows=@($Records|Where-Object {$_.type -cin @('foreground_bootstrap','activation_fence','activation_input','activation_event','activation_visibility','activation_button','activation_release')})
    if($null -eq $contract){Assert-AutoModal ($rows.Count -eq 0) 'activation evidence without contract';return $null}
    Assert-AutoModal ($contract -ceq 'verified_activation_v1') 'unknown foreground contract'
    $summaries=@($rows|Where-Object type -ceq foreground_bootstrap)
    Assert-AutoModal ($summaries.Count -eq 1) 'missing/duplicate foreground bootstrap'
    $b=$summaries[0]
    foreach($name in @('set_foreground_attempted','set_foreground_success','activation_click_required','temporary_topmost','window_from_point_root_matches','foreign_capture_clear','sendinput_move_success','sendinput_down_success','sendinput_up_success','wm_activate_seen','wm_setfocus_seen','activation_event_seen','final_foreground_matches','topmost_restored','topmost_now')){
        Assert-AutoBoolean (Get-AutoField $b $name) "bootstrap $name must be boolean"
    }
    Assert-AutoInteger $b.target 'bootstrap target must be integer'
    Assert-AutoInteger $b.foreground 'bootstrap foreground must be integer'
    Assert-AutoModal ($b.final_foreground_matches -eq ($b.target -gt 0 -and $b.foreground -eq $b.target)) 'forged final foreground summary'
    if($b.topmost_restored){Assert-AutoModal (-not $b.topmost_now) 'forged restored topmost summary'}
    Assert-AutoModal ($b.result -cin @('PASS','BLOCKED')) 'bootstrap result'
    Assert-AutoModal ($b.gesture -eq 0) 'bootstrap inside modal gesture'
    Assert-AutoModal (($Owned.Count -eq 1 -and $b.target -eq $Owned[0].hwnd) -or ($Owned.Count -eq 0 -and $b.target -eq 0 -and $b.result -ceq 'BLOCKED')) 'bootstrap owned identity'
    $attempt=@($Records|Where-Object type -ceq foreground_attempt)
    Assert-AutoModal ($attempt.Count -eq [int]$b.set_foreground_attempted) 'forged foreground attempt count'
    if($attempt.Count){
        Assert-AutoBoolean $attempt[0].set_foreground_success 'foreground API result must be boolean'
        Assert-AutoModal ($attempt[0].target -eq $b.target -and $attempt[0].sequence -lt $b.sequence -and $attempt[0].set_foreground_success -eq $b.set_foreground_success) 'forged foreground API summary'
    }
    $directVerified=$attempt.Count -eq 1 -and $attempt[0].set_foreground_success -and $attempt[0].foreground -eq $b.target
    $b|Add-Member -NotePropertyName validated_direct_success -NotePropertyValue $directVerified -Force
    $inputs=@($rows|Where-Object type -ceq activation_input)
    $events=@($rows|Where-Object type -ceq activation_event)
    $releases=@($rows|Where-Object type -ceq activation_release)
    Assert-AutoModal ($releases.Count -le 1) 'activation cleanup retry'
    $visibility=@($rows|Where-Object type -ceq activation_visibility)
    $raises=@($visibility|Where-Object enabled -eq $true);$restores=@($visibility|Where-Object enabled -eq $false)
    Assert-AutoModal ($raises.Count -le 1 -and $restores.Count -le 1) 'visibility retries/duplicate restore'
    foreach($v in $visibility){
        foreach($name in @('enabled','success','topmost_style','visible')){Assert-AutoBoolean (Get-AutoField $v $name) "visibility $name must be boolean"}
        Assert-AutoModal ($Owned.Count -eq 1 -and $v.target -eq $Owned[0].hwnd -and $v.gesture -eq 0 -and $v.sequence -lt $b.sequence -and $v.flags -eq $(if($v.enabled){83}else{19})) 'visibility identity/order/flags'
        if($v.success){Assert-AutoModal ($v.topmost_style -eq $v.enabled) 'visibility style does not match API result'}
    }
    if($inputs.Count){Assert-AutoModal ($raises.Count -eq 1 -and $raises[0].success -and $raises[0].topmost_style -and $raises[0].visible -and $raises[0].sequence -lt $inputs[0].sequence) 'activation input without visibility fence'}
    if(-not $b.activation_click_required){Assert-AutoModal ($visibility.Count -eq 0) 'direct path used topmost bootstrap'}
    if($b.activation_click_required -and $b.result -ceq 'PASS'){
        Assert-AutoModal ($raises.Count -eq 1 -and $restores.Count -eq 1 -and $restores[0].success -and -not $restores[0].topmost_style -and $restores[0].visible -and $restores[0].sequence -gt $inputs[-1].sequence) 'bootstrap topmost restore missing/failed'
    }
    $b|Add-Member -NotePropertyName validated_activation_attempts -NotePropertyValue $inputs.Count -Force
    Assert-AutoModal ($inputs.Count -le 3) 'activation retries/extra input'
    $allProofs=@($rows|Where-Object type -ceq activation_fence)
    Assert-AutoModal ($allProofs.Count -le 3+$releases.Count) 'activation proof retries'
    foreach($f in $allProofs){
        foreach($name in @('own_identity','desktop_ready','same_integrity','visible','temporary_topmost','window_from_point_root_matches','foreign_capture_clear','modifiers_clear','button_state_matches','left_down','gui_query_succeeded','foreground_snapshot_stable')){Assert-AutoBoolean (Get-AutoField $f $name) "activation fence $name must be boolean"}
        foreach($name in @('target','foreground','window_from_point_root','gui_flags','capture_hwnd','menu_owner_hwnd','move_size_hwnd','input_tag','hit_test')){Assert-AutoInteger (Get-AutoField $f $name) "activation fence $name must be integer"}
        Assert-AutoModal ($Owned.Count -eq 1 -and $f.target -eq $Owned[0].hwnd -and $f.gesture -eq 0 -and $f.phase -cin @('move','down','up') -and $f.sequence -lt $b.sequence) 'activation proof identity/phase'
        Assert-AutoModal ($f.window_from_point_root_matches -eq ($f.own_identity -and $f.window_from_point_root -eq $f.target)) 'forged root-match proof'
        $clear=$f.gui_query_succeeded -and $f.foreground_snapshot_stable -and ($f.gui_flags -band 30) -eq 0 -and $f.capture_hwnd -eq 0 -and $f.menu_owner_hwnd -eq 0 -and $f.move_size_hwnd -eq 0
        Assert-AutoModal ($f.foreign_capture_clear -eq $clear) 'forged foreign GUI state proof'
    }
    if($allProofs.Count){
        $lastProof=$allProofs[-1]
        Assert-AutoModal ($b.window_from_point_root_matches -eq $lastProof.window_from_point_root_matches -and $b.hit_test -eq $lastProof.hit_test -and $b.foreign_capture_clear -eq $lastProof.foreign_capture_clear) 'forged last activation proof summary'
    }
    $validEvents=@()
    foreach($e in $events){
        Assert-AutoModal ($Owned.Count -eq 1 -and $e.target -eq $Owned[0].hwnd -and $e.activation_epoch -in @(0,1) -and $e.message -cin @('WM_ACTIVATE','WM_SETFOCUS')) 'activation event identity/epoch/message'
        Assert-AutoBoolean $e.foreground_matches 'activation event foreground must be boolean'
        if($e.message -ceq 'WM_ACTIVATE'){
            Assert-AutoModal ($e.activation_code -in @(1,2)) 'activation code'
            if($e.activation_epoch -eq 1 -and $e.activation_code -in @(1,2)){$validEvents+=$e}
        }elseif($e.activation_epoch -eq 1){$validEvents+=$e}
    }
    $previous=0;$failed=$false;$phases=@('move','down','up');$flags=@(1,2,4)
    for($i=0;$i -lt $inputs.Count;++$i){
        $inputRow=$inputs[$i]
        Assert-AutoModal (-not $failed -and $b.activation_click_required -and $inputRow.gesture -eq 0 -and $inputRow.sequence -lt $b.sequence -and $inputRow.phase -ceq $phases[$i] -and $inputRow.flags -eq $flags[$i]) 'activation input phase/order'
        Assert-AutoPoint $inputRow.point 'activation input point'
        Assert-AutoInteger $inputRow.sent 'activation sent must be integer';Assert-AutoInteger $inputRow.error 'activation error must be integer'
        Assert-AutoModal ($inputRow.sent -in @(0,1)) 'activation SendInput count'
        $proof=@($rows|Where-Object {$_.type -ceq 'activation_fence' -and $_.sequence -gt $previous -and $_.sequence -lt $inputRow.sequence -and $_.phase -ceq $inputRow.phase})
        Assert-AutoModal ($proof.Count -eq 1) 'missing/duplicate fresh activation fence'
        $f=$proof[0]
        foreach($name in @('own_identity','desktop_ready','same_integrity','visible','temporary_topmost','window_from_point_root_matches','foreign_capture_clear','modifiers_clear','button_state_matches','left_down','gui_query_succeeded','foreground_snapshot_stable')){
            Assert-AutoBoolean (Get-AutoField $f $name) "activation fence $name must be boolean"
        }
        foreach($name in @('target','foreground','window_from_point_root','gui_flags','capture_hwnd','menu_owner_hwnd','move_size_hwnd','input_tag','hit_test')){Assert-AutoInteger (Get-AutoField $f $name) "activation fence $name must be integer"}
        Assert-AutoActivationProof $f $Owned $b.activation_point ($i -eq 2)
        Assert-AutoModal ($inputRow.input_tag -eq 0x50424D41 -and $f.qpc -le $inputRow.injection_start_qpc -and $inputRow.injection_start_qpc -le $inputRow.injection_return_qpc -and $inputRow.injection_return_qpc -le $inputRow.qpc) 'activation input tag/clock order'
        Assert-AutoModal ((Test-AutoPoint $f.activation_point $b.activation_point) -and (Test-AutoPoint $inputRow.point $b.activation_point)) 'activation point changed'
        if($i -gt 0){Assert-AutoModal (Test-AutoPoint $f.cursor $b.activation_point) 'activation cursor after move'}
        if($inputRow.sent -eq 1){Assert-AutoModal ($inputRow.error -eq 0) 'successful activation input has error'}else{$failed=$true;Assert-AutoModal ($b.result -ceq 'BLOCKED' -and $Blocked) 'activation SendInput failure without block'}
        $previous=$inputRow.sequence
    }
    $sentMove=$inputs.Count -ge 1 -and $inputs[0].sent -eq 1
    $sentDown=$inputs.Count -ge 2 -and $inputs[1].sent -eq 1
    $sentUp=$inputs.Count -ge 3 -and $inputs[2].sent -eq 1
    Assert-AutoModal ($b.sendinput_move_success -eq $sentMove -and $b.sendinput_down_success -eq $sentDown -and $b.sendinput_up_success -eq $sentUp) 'forged activation input summary'
    foreach($release in $releases){
        Assert-AutoBoolean $release.attempted 'cleanup attempted must be boolean';Assert-AutoBoolean $release.button_release_pending 'cleanup pending must be boolean';Assert-AutoInteger $release.sent 'cleanup sent must be integer'
        Assert-AutoModal ($Blocked -and $b.result -ceq 'BLOCKED' -and $sentDown -and -not $sentUp -and $release.gesture -eq 0 -and $release.sequence -gt $inputs[-1].sequence -and $release.sequence -lt $b.sequence -and $release.sent -in @(0,1)) 'cleanup without pending owned DOWN'
        $proof=@($allProofs|Where-Object {$_.phase -ceq 'up' -and $_.sequence -gt $inputs[-1].sequence -and $_.sequence -lt $release.sequence})
        if($proof.Count -eq 0){
            # The native proof collector itself may throw before it can emit a
            # fence. This exact branch skipped SendInput and preserves the
            # original blocker; it supplies no input authority of its own.
            Assert-AutoModal (-not $release.attempted -and $release.sent -eq 0 -and $release.button_release_pending -and $release.reason -ceq 'BLOCKED_BY_ACTIVATION_CLEANUP_PROOF_FAILURE') 'cleanup without fresh activation proof'
        }
        if($release.attempted){
            $cleanupProof=$proof[-1]
            Assert-AutoActivationProof $cleanupProof $Owned $b.activation_point $true
            Assert-AutoModal ($release.input_tag -eq 0x50424D41 -and (Test-AutoPoint $release.point $b.activation_point) -and $cleanupProof.qpc -le $release.injection_start_qpc -and $release.injection_start_qpc -le $release.injection_return_qpc -and $release.injection_return_qpc -le $release.qpc) 'cleanup tag/point/clock'
            Assert-AutoModal ($release.button_release_pending -eq ($release.sent -eq 0) -and ($release.sent -eq 0 -or $release.error -eq 0)) 'cleanup result/pending mismatch'
        }else{Assert-AutoModal ($release.sent -eq 0 -and $release.button_release_pending -and $release.reason -cin @('BLOCKED_BY_ACTIVATION_IDENTITY','BLOCKED_BY_INTERACTIVE_DESKTOP','BLOCKED_BY_ACTIVATION_VISIBILITY','BLOCKED_BY_ACTIVATION_HIT_TEST','BLOCKED_BY_FOREIGN_INPUT_CAPTURE','BLOCKED_BY_INPUT_INTERFERENCE','BLOCKED_BY_ACTIVATION_CLEANUP_PROOF_FAILURE')) 'unsafe/unknown skipped cleanup'}
        Assert-AutoModal (@($restores|Where-Object {$_.sequence -lt $release.sequence}).Count -eq 0) 'cleanup after visibility authority retired'
    }
    foreach($button in @($rows|Where-Object type -ceq activation_button)){
        Assert-AutoModal ($Owned.Count -eq 1 -and $button.target -eq $Owned[0].hwnd -and $button.gesture -eq 0 -and $button.message -cin @('WM_LBUTTONDOWN','WM_LBUTTONUP') -and $button.sequence -lt $b.sequence) 'client button receipt identity'
        $operation=@(if($button.message -ceq 'WM_LBUTTONDOWN'){$inputs|Where-Object {$_.phase -ceq 'down' -and $_.sent -eq 1}}else{$inputs|Where-Object {$_.phase -ceq 'up' -and $_.sent -eq 1};$releases|Where-Object {$_.attempted -and $_.sent -eq 1}})
        Assert-AutoModal ($operation.Count -eq 1 -and $button.qpc -ge $operation[0].injection_start_qpc) 'button receipt before successful input call'
    }
    if($b.activation_click_required -and $b.result -ceq 'PASS'){
        foreach($message in @('WM_LBUTTONDOWN','WM_LBUTTONUP')){Assert-AutoModal (@($rows|Where-Object {$_.type -ceq 'activation_button' -and $_.message -ceq $message}).Count -eq 1) 'missing/duplicate actual activation button receipt'}
    }
    # SendInput may deliver WM_ACTIVATE before its result row is written.
    # Associate the WndProc callback with the DOWN API start, not that row.
    $downStart=if($sentDown){$inputs[1].injection_start_qpc}else{[long]::MaxValue}
    $observed=if($b.activation_click_required){
        @($validEvents|Where-Object {$_.qpc -ge $downStart -and $_.sequence -lt $b.sequence})
    }else{
        @($events|Where-Object {$_.activation_epoch -eq 0 -and $_.sequence -lt $b.sequence -and ($_.message -ceq 'WM_SETFOCUS' -or $_.activation_code -in @(1,2))})
    }
    $activate=@($observed|Where-Object message -ceq WM_ACTIVATE).Count -gt 0
    $focus=@($observed|Where-Object message -ceq WM_SETFOCUS).Count -gt 0
    Assert-AutoModal ($b.wm_activate_seen -eq $activate -and $b.wm_setfocus_seen -eq $focus) 'forged activation callback summary'
    $eventObserved=$b.activation_click_required -and ($activate -or $focus)
    Assert-AutoModal ((-not $b.activation_event_seen -or $eventObserved) -and ($b.result -cne 'PASS' -or $b.activation_event_seen -eq $eventObserved)) 'forged activation event summary'
    foreach($r in @($Records|Where-Object {$_.type -cin @('path','ENTER','T0','T1','T2','T3','path_complete','input')})){
        Assert-AutoModal ($b.result -ceq 'PASS' -and $r.sequence -gt $b.sequence) 'modal/input before successful foreground bootstrap'
    }
    if(-not $b.activation_click_required){
        Assert-AutoModal ($inputs.Count -eq 0 -and -not $b.temporary_topmost -and $null -eq $b.activation_point -and $null -eq $b.hit_test) 'direct bootstrap used activation exception'
        if($b.result -ceq 'PASS'){Assert-AutoModal ($b.set_foreground_attempted -and $directVerified -and $b.final_foreground_matches -and $b.topmost_restored) 'direct foreground not verified'}
    }else{
        Assert-AutoModal ($b.set_foreground_attempted -and -not $directVerified) 'fallback without unverified foreground attempt'
        if($b.result -ceq 'PASS'){
            Assert-AutoModal ($inputs.Count -eq 3 -and $sentMove -and $sentDown -and $sentUp -and $b.temporary_topmost -and $b.window_from_point_root_matches -and $b.hit_test -eq 1 -and $b.foreign_capture_clear -and ($activate -or $focus) -and $b.final_foreground_matches -and $b.topmost_restored) 'activation bootstrap not verified'
        }
    }
    if($b.result -ceq 'BLOCKED'){Assert-AutoModal ($Blocked -and -not [string]::IsNullOrWhiteSpace($b.reason)) 'bootstrap block without blocked shutdown/reason'}
    return $b
}
function Add-AutoBootstrapSummary($Result,$Bootstrap){
    $summary=if($null -eq $Bootstrap){
        @{ForegroundBootstrap='LEGACY_NOT_RECORDED';DirectAttempted=0;DirectSucceeded=0;ActivationRequired=0;ActivationAttempted=0;ActivationSucceeded=0;ActivationInputs=0}
    }else{
        @{ForegroundBootstrap=$Bootstrap.result;DirectAttempted=[int]$Bootstrap.set_foreground_attempted;DirectSucceeded=[int]$Bootstrap.validated_direct_success;ActivationRequired=[int]$Bootstrap.activation_click_required;ActivationAttempted=[int]($Bootstrap.validated_activation_attempts -gt 0);ActivationSucceeded=[int]($Bootstrap.activation_click_required -and $Bootstrap.result -ceq 'PASS');ActivationInputs=$Bootstrap.validated_activation_attempts}
    }
    foreach($key in $summary.Keys){$Result|Add-Member -NotePropertyName $key -NotePropertyValue $summary[$key]}
    return $Result
}
function Get-AutoTrajectory($Rect,[int]$Gesture,$Start,$Cursor,[int]$Pulse=0){
    $r=@($Rect|ForEach-Object {[long]$_})
    if($Gesture -eq 1){$dx=[long]$Cursor[0]-[long]$Start[0];$dy=[long]$Cursor[1]-[long]$Start[1]+$Pulse;$r[0]+=$dx;$r[2]+=$dx;$r[1]+=$dy;$r[3]+=$dy}
    else {$r[3]+=[long]$Cursor[1]-[long]$Start[1]+$Pulse}
    return ,$r
}
function Get-AutoModalAuthority($Path,$T0,$T1,$T2,$T3,$Complete,[int]$Gesture){
    $target=Get-AutoTrajectory $T0.positioning $Gesture @(0,0) @(0,0) 7
    $visibleTarget=Get-AutoTrajectory $T0.visible $Gesture @(0,0) @(0,0) 7
    Assert-AutoModal ((Test-AutoRect $target $T0.target_positioning) -and (Test-AutoRect $visibleTarget $T0.target_visible)) 'correction is not the prescribed +7 pulse'
    $px=Test-AutoRect $T1.positioning $target;$vx=Test-AutoRect $T1.visible $visibleTarget
    Assert-AutoModal ($T1.positioning_exact -eq $px -and $T1.visible_exact -eq $vx) 'forged T1 exact flag'
    if(-not $T1.native_success){return 'UNKNOWN'}
    if(-not $px){return 'IMMEDIATE_REJECTED'}
    $rawNext=Get-AutoTrajectory $Path.positioning $Gesture $Path.start $T2.cursor
    $pulseNext=Get-AutoTrajectory $Path.positioning $Gesture $Path.start $T2.cursor 7
    $rawFinal=Get-AutoTrajectory $Path.positioning $Gesture $Path.start $Path.end
    $pulseFinal=Get-AutoTrajectory $Path.positioning $Gesture $Path.start $Path.end 7
    $pulseFinalVisible=Get-AutoTrajectory $Path.visible $Gesture $Path.start $Path.end 7
    if((Test-AutoRect $T2.proposed $rawNext) -and (Test-AutoRect $T3.positioning $rawFinal) -and (Test-AutoRect $Complete.positioning $rawFinal)){return 'REASSERTED'}
    if((Test-AutoRect $T2.proposed $pulseNext) -and (Test-AutoRect $T3.positioning $pulseFinal) -and (Test-AutoRect $Complete.positioning $pulseFinal) -and (Test-AutoRect $Complete.visible $pulseFinalVisible)){
        if(-not $vx){return 'DWM_ASYNC_ONLY'};return 'STABLE'
    }
    return 'UNKNOWN'
}
function Test-AutoModalRecords([object[]]$Records){
    Assert-AutoModal ($Records.Count -ge 2) 'empty evidence'
    for($i=0;$i -lt $Records.Count;++$i){Assert-AutoModal ($Records[$i].schema -ceq 'r1c4b-auto-owned-modal/v1' -and $Records[$i].sequence -eq $i+1) 'schema/sequence';if($i){Assert-AutoModal ($Records[$i].qpc -ge $Records[$i-1].qpc) 'QPC order'}}
    Assert-AutoModal ($Records[0].type -ceq 'startup' -and $Records[-1].type -ceq 'shutdown' -and @($Records|Where-Object type -eq startup).Count -eq 1 -and @($Records|Where-Object type -eq shutdown).Count -eq 1) 'lifecycle envelope'
    $s=$Records[0];$end=$Records[-1]
    Assert-AutoModal ($s.evidence_kind -ceq 'automated_owned_modal' -and $s.sendinput_in_probe -eq $true -and $s.human_input -eq $false -and $s.real_explorer -eq $false -and $s.qpc_frequency -gt 0) 'evidence authority'
    $owned=@($Records|Where-Object type -eq owned);$desktop=@($Records|Where-Object type -eq desktop_gate)
    Assert-AutoModal ($owned.Count -le 1 -and $desktop.Count -le 1) 'duplicate owned/desktop evidence'
    if($owned.Count){Assert-AutoModal ($owned[0].pid -eq $s.pid -and $owned[0].tid -eq $s.ui_tid -and $owned[0].hwnd -gt 0) 'owned identity'}
    Assert-AutoBoolean $end.external_windows_touched 'external touch flag must be boolean'
    Assert-AutoBoolean $end.owned_window_destroyed 'owned cleanup flag must be boolean'
    $bootstrap=Test-AutoForegroundBootstrap $Records $owned ($end.result -ceq 'BLOCKED')
    Test-AutoNormalInputs $Records $owned ($end.result -ceq 'BLOCKED') $bootstrap
    if($end.result -ceq 'BLOCKED'){
        $blocked=@($Records|Where-Object type -eq blocked)
        Assert-AutoModal ($blocked.Count -gt 0 -and @($blocked|Where-Object {[string]::IsNullOrWhiteSpace($_.reason)}).Count -eq 0 -and $end.external_windows_touched -eq $false -and $end.owned_window_destroyed -eq $true) 'blocked reason/safety missing'
        return Add-AutoBootstrapSummary ([pscustomobject]@{Result='BLOCKED';Move='UNKNOWN';Resize='UNKNOWN';Reasons=@($blocked|ForEach-Object {$_.reason})}) $bootstrap
    }
    Assert-AutoModal ($end.result -ceq 'CAPTURED_NOT_ACCEPTED' -and $end.cursor_restored -eq $true -and $end.owned_window_destroyed -eq $true -and $end.external_windows_touched -eq $false) 'completion safety'
    Assert-AutoModal (@($Records|Where-Object type -eq blocked).Count -eq 0) 'blocked event in completed evidence'
    Assert-AutoModal ($owned.Count -eq 1 -and $owned[0].pid -eq $s.pid -and $owned[0].tid -eq $s.ui_tid -and $owned[0].hwnd -gt 0 -and $desktop.Count -eq 1 -and $desktop[0].active_unlocked -eq $true -and $desktop[0].input_desktop_matches -eq $true) 'owned identity/desktop'
    $classes=@()
    $previousComplete=$null
    for($g=1;$g -le 2;++$g){
        $rows=@($Records|Where-Object gesture -eq $g)
        $one=@{};foreach($type in @('path','ENTER','T0','T1','T2','T3','path_complete')){
            $matches=@($rows|Where-Object type -ceq $type);Assert-AutoModal ($matches.Count -eq 1) "gesture $g missing/duplicate $type";$one[$type]=$matches[0]
        }
        $p=$one.path;$t0=$one.T0;$t1=$one.T1;$t2=$one.T2;$t3=$one.T3;$complete=$one.path_complete
        if($null -ne $previousComplete){Assert-AutoModal ((Test-AutoRect $p.positioning $previousComplete.positioning) -and (Test-AutoRect $p.visible $previousComplete.visible)) 'unexplained between-gesture geometry change'}
        Assert-AutoModal ($p.hit_test -eq $(if($g -eq 1){2}else{15}) -and $p.samples -eq 20 -and $p.interval_ms -eq 30 -and $p.planned_duration_ms -eq 600 -and $p.foreground -eq $owned[0].hwnd) 'input path contract'
        Assert-AutoModal ($p.end[0]-$p.start[0] -eq $(if($g -eq 1){180}else{0}) -and $p.end[1]-$p.start[1] -eq $(if($g -eq 1){0}else{120})) 'path displacement'
        Assert-AutoModal ($p.sequence -lt $one.ENTER.sequence -and $one.ENTER.sequence -lt $t0.sequence -and $t0.sequence -lt $t1.sequence -and $t1.sequence -lt $t2.sequence -and $t2.sequence -lt $t3.sequence -and $t3.sequence -lt $complete.sequence) 'native lifecycle order'
        Assert-AutoModal ($t1.native_calls -eq 1 -and $t1.flags -eq $(if($g -eq 1){21}else{20}) -and $t0.drag_active -eq $true -and $t3.corrected -eq $true -and $t3.next_drag_seen -eq $true) 'single modal correction'
        Assert-AutoModal ($t0.qpc -le $t1.native_start_qpc -and $t1.native_start_qpc -le $t1.native_return_qpc -and $t1.native_return_qpc -le $t1.qpc) 'native clock order'
        $drags=@($rows|Where-Object {$_.type -cin @('DRAG','T2')});Assert-AutoModal ($drags.Count -ge 20) 'missing real drag callbacks'
        foreach($d in $drags){Assert-AutoModal ($d.event -ceq $(if($g -eq 1){'WM_MOVING'}else{'WM_SIZING'}) -and ($g -eq 1 -or $d.edge -eq 6)) 'wrong native operation/edge'}
        $inputs=@($rows|Where-Object {$_.type -eq 'input' -and $_.sequence -gt $p.sequence -and $_.sequence -lt $complete.sequence})
        Assert-AutoModal ($inputs.Count -eq 22 -and $inputs[0].flags -eq 2 -and $inputs[-1].flags -eq 4) 'down / samples / up'
        for($i=1;$i -le 20;++$i){Assert-AutoModal ($inputs[$i].flags -eq 1 -and $inputs[$i].point[0] -eq $p.start[0]+($p.end[0]-$p.start[0])*$i/20 -and $inputs[$i].point[1] -eq $p.start[1]+($p.end[1]-$p.start[1])*$i/20) 'generated path sample'}
        foreach($r in @($p,$t0,$t1,$t2,$t3,$complete)){$null=Get-AutoRect $r.positioning;$null=Get-AutoRect $r.visible;Assert-AutoModal ($r.positioning_error -eq 0 -and $r.visible_hresult -ge 0) 'capture error'}
        $area=$owned[0].work_area;Assert-AutoModal ($t0.target_positioning[0] -ge $area[0]+100 -and $t0.target_positioning[1] -ge $area[1]+100 -and $t0.target_positioning[2] -le $area[2]-100 -and $t0.target_positioning[3] -le $area[3]-100) 'pulse outside safe work area'
        $classification=Get-AutoModalAuthority $p $t0 $t1 $t2 $t3 $complete $g
        if($classification -in @('STABLE','DWM_ASYNC_ONLY')){
            foreach($d in @($drags|Where-Object {$_.sequence -gt $t1.sequence})){
                if(-not (Test-AutoRect $d.proposed (Get-AutoTrajectory $p.positioning $g $p.start $d.cursor 7))){$classification='UNKNOWN'}
            }
        }
        $classes+=$classification
        $previousComplete=$complete
    }
    return Add-AutoBootstrapSummary ([pscustomobject]@{Result=$(if($classes -contains 'UNKNOWN'){'FAIL'}else{'CAPTURED'});Move=$classes[0];Resize=$classes[1];Reasons=@()}) $bootstrap
}
function Test-AutoModalEvidence([string]$Path){
    $rows=@(Get-Content -LiteralPath $Path -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json -ErrorAction Stop})
    return Test-AutoModalRecords $rows
}
