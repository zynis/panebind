Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-auto-owned-modal-validation.ps1')
$checks=0
foreach($g in @(1,2)){
    foreach($kind in @('STABLE','REASSERTED','IMMEDIATE_REJECTED','DWM_ASYNC_ONLY','UNKNOWN')){
        $initial=@(100,100,700,500);$v=@(110,100,690,490);$start=@(400,110);$next=if($g -eq 1){@(418,110)}else{@(400,122)};$end=if($g -eq 1){@(580,110)}else{@(400,230)}
        $p=[pscustomobject]@{positioning=$initial;visible=$v;start=$start;end=$end}
        $t0=[pscustomobject]@{positioning=$initial;visible=$v;target_positioning=(Get-AutoTrajectory $initial $g @(0,0) @(0,0) 7);target_visible=(Get-AutoTrajectory $v $g @(0,0) @(0,0) 7)}
        $t1=[pscustomobject]@{positioning=$t0.target_positioning;visible=$t0.target_visible;positioning_exact=$true;visible_exact=$true;native_success=$true}
        $retained=if($kind -eq 'REASSERTED'){0}else{7}
        $t2=[pscustomobject]@{cursor=$next;proposed=(Get-AutoTrajectory $initial $g $start $next $retained)}
        $t3=[pscustomobject]@{positioning=(Get-AutoTrajectory $initial $g $start $end $retained)}
        $complete=[pscustomobject]@{positioning=$t3.positioning;visible=(Get-AutoTrajectory $v $g $start $end $retained)}
        if($kind -eq 'IMMEDIATE_REJECTED'){$t1.positioning=$initial;$t1.positioning_exact=$false}
        if($kind -eq 'DWM_ASYNC_ONLY'){$t1.visible=$v;$t1.visible_exact=$false}
        if($kind -eq 'UNKNOWN'){$t2.proposed=@(110,110,710,510)}
        $actual=Get-AutoModalAuthority $p $t0 $t1 $t2 $t3 $complete $g
        if($actual -cne $kind){throw "$g $kind became $actual"};++$checks
        $t1.positioning_exact=-not $t1.positioning_exact
        $reject=$false;try{$null=Get-AutoModalAuthority $p $t0 $t1 $t2 $t3 $complete $g}catch{$reject=$true};if(-not $reject){throw 'false exact flag accepted'};++$checks
    }
}
$reject=$false;try{$null=Test-AutoModalRecords @()}catch{$reject=$true};if(-not $reject){throw 'empty accepted'};++$checks
function New-AutoModalFixture {
    $rows=[Collections.Generic.List[object]]::new();$g=0
    function Add-Row([string]$type,[hashtable]$fields){
        $row=[ordered]@{schema='r1c4b-auto-owned-modal/v1';sequence=$rows.Count+1;type=$type;gesture=$g;qpc=($rows.Count+1)*100}
        foreach($key in $fields.Keys){$row[$key]=$fields[$key]};$rows.Add([pscustomobject]$row)
    }
    function Add-Input([int]$flags,$p){Add-Row 'input_fence' @{foreground=99;target=99;cursor=$p;expected_cursor=$p;left_down=($flags -ne 2)};Add-Row 'input' @{flags=$flags;point=$p;sent=1;error=0}}
    function Geometry-Fields($p,$v){return @{positioning=$p;visible=$v;positioning_error=0;visible_hresult=0}}
    Add-Row 'startup' @{evidence_kind='automated_owned_modal';sendinput_in_probe=$true;human_input=$false;real_explorer=$false;qpc_frequency=10000000;pid=1;ui_tid=2}
    Add-Row 'desktop_gate' @{active_unlocked=$true;input_desktop_matches=$true}
    Add-Row 'owned' @{hwnd=99;pid=1;tid=2;work_area=@(0,0,2000,1500)}
    $initial=@(100,100,700,500);$visible=@(110,100,690,490)
    for($g=1;$g -le 2;++$g){
        $start=@(400,110);$end=if($g -eq 1){@(580,110)}else{@(400,230)}
        $path=Geometry-Fields $initial $visible;$path.start=$start;$path.end=$end;$path.samples=20;$path.interval_ms=30;$path.planned_duration_ms=600;$path.foreground=99;$path.hit_test=if($g -eq 1){2}else{15}
        Add-Row 'path' $path;Add-Input 2 $start;Add-Row 'ENTER' @{}
        $t0=Geometry-Fields $initial $visible;$t0.drag_active=$true;$t0.target_positioning=Get-AutoTrajectory $initial $g @(0,0) @(0,0) 7;$t0.target_visible=Get-AutoTrajectory $visible $g @(0,0) @(0,0) 7
        Add-Row 'T0' $t0
        $t1=Geometry-Fields $t0.target_positioning $t0.target_visible;$t1.native_success=$true;$t1.error=0;$t1.flags=if($g -eq 1){21}else{20};$t1.native_calls=1;$t1.positioning_exact=$true;$t1.visible_exact=$true;$t1.native_start_qpc=$rows.Count*100+1;$t1.native_return_qpc=$rows.Count*100+2
        Add-Row 'T1' $t1
        for($i=1;$i -le 20;++$i){
            $cursor=@(($start[0]+($end[0]-$start[0])*$i/20),($start[1]+($end[1]-$start[1])*$i/20))
            Add-Input 1 $cursor
            $drag=Geometry-Fields (Get-AutoTrajectory $initial $g $start $cursor) (Get-AutoTrajectory $visible $g $start $cursor)
            $drag.cursor=$cursor;$drag.proposed=$drag.positioning;$drag.event=if($g -eq 1){'WM_MOVING'}else{'WM_SIZING'};$drag.edge=if($g -eq 1){0}else{6}
            Add-Row $(if($i -eq 1){'T2'}else{'DRAG'}) $drag
        }
        Add-Input 4 $end
        $final=Geometry-Fields (Get-AutoTrajectory $initial $g $start $end) (Get-AutoTrajectory $visible $g $start $end);$final.corrected=$true;$final.next_drag_seen=$true
        Add-Row 'T3' $final;Add-Row 'path_complete' $final
        $initial=$final.positioning;$visible=$final.visible
    }
    Add-Row 'shutdown' @{result='CAPTURED_NOT_ACCEPTED';cursor_restored=$true;owned_window_destroyed=$true;external_windows_touched=$false}
    return ,$rows.ToArray()
}
$base=New-AutoModalFixture
$result=Test-AutoModalRecords $base
if($result.Result -cne 'CAPTURED' -or $result.Move -cne 'REASSERTED' -or $result.Resize -cne 'REASSERTED'){throw 'valid full fixture failed'};++$checks
foreach($fault in @('sequence','schema','human','missing-enter','missing-t2','native-calls','target','false-exact','foreground','missing-fence','input-failed','input-path','wrong-edge','unsafe-margin','capture','timing','incomplete','restore','cross-gesture')){
    $rows=($base|ConvertTo-Json -Depth 15 -Compress|ConvertFrom-Json)
    $t1=$rows|Where-Object type -eq T1|Select-Object -First 1
    switch($fault){
        'sequence' {$rows[2].sequence=0}
        'schema' {$rows[0].schema='r1c4b/v1'}
        'human' {$rows[0].human_input=$true}
        'missing-enter' {($rows|Where-Object type -eq ENTER|Select-Object -First 1).type='not_enter'}
        'missing-t2' {($rows|Where-Object type -eq T2|Select-Object -First 1).type='DRAG'}
        'native-calls' {$t1.native_calls=2}
        'target' {($rows|Where-Object type -eq T0|Select-Object -First 1).target_positioning[0]++}
        'false-exact' {$t1.positioning_exact=$false}
        'foreground' {($rows|Where-Object type -eq input_fence|Select-Object -First 1).foreground=88}
        'missing-fence' {($rows|Where-Object type -eq input_fence|Select-Object -First 1).type='not_fence'}
        'input-failed' {($rows|Where-Object type -eq input|Select-Object -First 1).sent=0}
        'input-path' {($rows|Where-Object {$_.type -eq 'input' -and $_.flags -eq 1}|Select-Object -First 1).point[0]++}
        'wrong-edge' {($rows|Where-Object {$_.type -eq 'T2' -and $_.gesture -eq 2}).edge=3}
        'unsafe-margin' {($rows|Where-Object type -eq owned).work_area[0]=200}
        'capture' {$t1.visible_hresult=-1}
        'timing' {$t1.native_start_qpc=$t1.qpc+1}
        'incomplete' {$rows[-1].result='PASS'}
        'restore' {$rows[-1].cursor_restored=$false}
        'cross-gesture' {($rows|Where-Object {$_.type -eq 'path' -and $_.gesture -eq 2}).positioning[0]++}
    }
    $reject=$false;try{$null=Test-AutoModalRecords $rows}catch{$reject=$true};if(-not $reject){throw "invalid full fixture accepted: $fault"};++$checks
}
Write-Output "AUTO_MODAL_CLASSIFIER_FIXTURES = PASS ($checks; synthetic only)"
