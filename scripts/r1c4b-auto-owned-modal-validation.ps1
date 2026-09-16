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
    if($end.result -ceq 'BLOCKED'){
        $blocked=@($Records|Where-Object type -eq blocked)
        Assert-AutoModal ($blocked.Count -gt 0 -and @($blocked|Where-Object {[string]::IsNullOrWhiteSpace($_.reason)}).Count -eq 0 -and $end.external_windows_touched -eq $false -and $end.owned_window_destroyed -eq $true) 'blocked reason/safety missing'
        return [pscustomobject]@{Result='BLOCKED';Move='UNKNOWN';Resize='UNKNOWN';Reasons=@($blocked|ForEach-Object {$_.reason})}
    }
    Assert-AutoModal ($end.result -ceq 'CAPTURED_NOT_ACCEPTED' -and $end.cursor_restored -eq $true -and $end.owned_window_destroyed -eq $true -and $end.external_windows_touched -eq $false) 'completion safety'
    Assert-AutoModal (@($Records|Where-Object type -eq blocked).Count -eq 0) 'blocked event in completed evidence'
    $owned=@($Records|Where-Object type -eq owned);$desktop=@($Records|Where-Object type -eq desktop_gate)
    Assert-AutoModal ($owned.Count -eq 1 -and $owned[0].pid -eq $s.pid -and $owned[0].tid -eq $s.ui_tid -and $owned[0].hwnd -gt 0 -and $desktop.Count -eq 1 -and $desktop[0].active_unlocked -eq $true -and $desktop[0].input_desktop_matches -eq $true) 'owned identity/desktop'
    foreach($f in @($Records|Where-Object type -eq input_fence)){
        Assert-AutoModal ($f.foreground -eq $owned[0].hwnd -and $f.target -eq $owned[0].hwnd -and [Math]::Abs($f.cursor[0]-$f.expected_cursor[0]) -le 1 -and [Math]::Abs($f.cursor[1]-$f.expected_cursor[1]) -le 1) 'input fence'
    }
    $previousInput=0
    foreach($inputRow in @($Records|Where-Object type -eq input)){
        Assert-AutoModal ($inputRow.sent -eq 1 -and $inputRow.error -eq 0) 'SendInput failure'
        Assert-AutoModal (@($Records|Where-Object {$_.type -eq 'input_fence' -and $_.sequence -gt $previousInput -and $_.sequence -lt $inputRow.sequence}).Count -gt 0) 'input without fresh fence'
        $previousInput=$inputRow.sequence
    }
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
    return [pscustomobject]@{Result=$(if($classes -contains 'UNKNOWN'){'FAIL'}else{'CAPTURED'});Move=$classes[0];Resize=$classes[1];Reasons=@()}
}
function Test-AutoModalEvidence([string]$Path){
    $rows=@(Get-Content -LiteralPath $Path -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json -ErrorAction Stop})
    return Test-AutoModalRecords $rows
}
