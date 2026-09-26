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
function Copy-AutoFixture($Rows){return ,($Rows|ConvertTo-Json -Depth 25 -Compress|ConvertFrom-Json)}
function New-AutoBootstrapFixture([bool]$Fallback){
    $rows=Copy-AutoFixture (New-AutoModalFixture)
    $rows[0]|Add-Member -NotePropertyName foreground_contract -NotePropertyValue verified_activation_v1
    foreach($r in @($rows|Where-Object type -eq input)){
        $r|Add-Member -NotePropertyName input_tag -NotePropertyValue 0x50424D41
        $r|Add-Member -NotePropertyName injection_start_qpc -NotePropertyValue ($r.qpc-1)
        $r|Add-Member -NotePropertyName injection_return_qpc -NotePropertyValue $r.qpc
    }
    foreach($r in @($rows|Where-Object type -eq input_fence)){
        $r|Add-Member -NotePropertyName priming -NotePropertyValue $false
        $r|Add-Member -NotePropertyName capture_hwnd -NotePropertyValue $(if($r.left_down){99}else{0})
        $r|Add-Member -NotePropertyName window_from_point_root -NotePropertyValue 99
        $r|Add-Member -NotePropertyName gui_query_succeeded -NotePropertyValue $true
    }
    $bootRows=[Collections.Generic.List[object]]::new()
    function Add-BootRow([string]$Type,[hashtable]$Fields){
        $row=[ordered]@{schema='r1c4b-auto-owned-modal/v1';sequence=0;type=$Type;gesture=0;qpc=301+$bootRows.Count}
        foreach($key in $Fields.Keys){$row[$key]=$Fields[$key]};$bootRows.Add([pscustomobject]$row)
    }
    $activationPoint=@(400,300)
    Add-BootRow 'foreground_attempt' @{target=99;foreground=$(if($Fallback){88}else{99});set_foreground_success=(-not $Fallback);visible=$true}
    if($Fallback){
        Add-BootRow 'activation_visibility' @{target=99;enabled=$true;success=$true;flags=83;topmost_style=$true;visible=$true}
        for($phaseIndex=0;$phaseIndex -lt 3;++$phaseIndex){
            $phase=@('move','down','up')[$phaseIndex]
            Add-BootRow 'activation_fence' @{phase=$phase;target=99;foreground=$(if($phaseIndex -eq 2){99}else{88});cursor=$(if($phaseIndex -eq 0){@(0,0)}else{$activationPoint});activation_point=$activationPoint;own_identity=$true;desktop_ready=$true;same_integrity=$true;visible=$true;temporary_topmost=$true;window_from_point_root_matches=$true;window_from_point_root=99;hit_test=1;foreign_capture_clear=$true;gui_query_succeeded=$true;foreground_snapshot_stable=$true;gui_flags=0;capture_hwnd=0;menu_owner_hwnd=0;move_size_hwnd=0;modifiers_clear=$true;button_state_matches=$true;left_down=($phaseIndex -eq 2);input_tag=0x50424D41}
            $nativeStart=302+$bootRows.Count-1
            if($phaseIndex -eq 1){
                # Real SendInput callbacks may precede the input result row.
                Add-BootRow 'activation_event' @{message='WM_ACTIVATE';activation_code=2;activation_epoch=1;target=99;foreground_matches=$true}
                Add-BootRow 'activation_event' @{message='WM_SETFOCUS';activation_epoch=1;target=99;foreground_matches=$true}
                Add-BootRow 'activation_button' @{message='WM_LBUTTONDOWN';target=99}
            }
            if($phaseIndex -eq 2){Add-BootRow 'activation_button' @{message='WM_LBUTTONUP';target=99}}
            Add-BootRow 'activation_input' @{phase=$phase;flags=@(1,2,4)[$phaseIndex];point=$activationPoint;sent=1;error=0;input_tag=0x50424D41;injection_start_qpc=$nativeStart;injection_return_qpc=301+$bootRows.Count}
        }
        Add-BootRow 'activation_visibility' @{target=99;enabled=$false;success=$true;flags=19;topmost_style=$false;visible=$true}
    }
    Add-BootRow 'foreground_bootstrap' @{target=99;set_foreground_attempted=$true;set_foreground_success=(-not $Fallback);activation_click_required=$Fallback;temporary_topmost=$Fallback;activation_point=$(if($Fallback){$activationPoint}else{$null});window_from_point_root_matches=$Fallback;hit_test=$(if($Fallback){1}else{$null});foreign_capture_clear=$true;sendinput_move_success=$Fallback;sendinput_down_success=$Fallback;sendinput_up_success=$Fallback;wm_activate_seen=$Fallback;wm_setfocus_seen=$Fallback;activation_event_seen=$Fallback;foreground=99;topmost_now=$false;final_foreground_matches=$true;topmost_restored=$true;result='PASS';reason='verified'}
    $all=@($rows[0..2])+@($bootRows.ToArray())+@($rows[3..($rows.Count-1)])
    for($i=0;$i -lt $all.Count;++$i){$all[$i].sequence=$i+1}
    return ,$all
}
function Convert-AutoBootstrapBlocked($Rows,[string]$Reason){
    $result=@($Rows|Where-Object {$_.type -cin @('startup','desktop_gate','owned','foreground_attempt','activation_fence','activation_input','activation_event','activation_button','activation_release','activation_visibility','foreground_bootstrap')})
    $b=$result|Where-Object type -eq foreground_bootstrap
    $b.result='BLOCKED';$b.reason=$Reason
    $result+=[pscustomobject]@{schema='r1c4b-auto-owned-modal/v1';sequence=0;type='blocked';gesture=0;qpc=350;reason=$Reason}
    $result+=[pscustomobject]@{schema='r1c4b-auto-owned-modal/v1';sequence=0;type='shutdown';gesture=0;qpc=351;result='BLOCKED';cursor_restored=$false;owned_window_destroyed=$true;external_windows_touched=$false}
    for($i=0;$i -lt $result.Count;++$i){$result[$i].sequence=$i+1}
    return ,$result
}
function Reset-AutoBootstrapSummary($Rows){
    $b=$Rows|Where-Object type -eq foreground_bootstrap
    $inputs=@($Rows|Where-Object type -eq activation_input)
    $b.sendinput_move_success=@($inputs|Where-Object {$_.phase -eq 'move' -and $_.sent -eq 1}).Count -gt 0
    $b.sendinput_down_success=@($inputs|Where-Object {$_.phase -eq 'down' -and $_.sent -eq 1}).Count -gt 0
    $b.sendinput_up_success=@($inputs|Where-Object {$_.phase -eq 'up' -and $_.sent -eq 1}).Count -gt 0
    $b.wm_activate_seen=@($Rows|Where-Object {$_.type -eq 'activation_event' -and $_.message -eq 'WM_ACTIVATE'}).Count -gt 0
    $b.wm_setfocus_seen=@($Rows|Where-Object {$_.type -eq 'activation_event' -and $_.message -eq 'WM_SETFOCUS'}).Count -gt 0
    $b.activation_event_seen=$b.wm_activate_seen -or $b.wm_setfocus_seen
}
# A/B exercise the entire independent validator, including the original modal
# classifier. A successful driver never turns the REASSERTED model into STABLE.
foreach($fallback in @($false,$true)){
    $fixture=New-AutoBootstrapFixture $fallback;$result=Test-AutoModalRecords $fixture
    if($result.Result -cne 'CAPTURED' -or $result.Move -cne 'REASSERTED' -or $result.Resize -cne 'REASSERTED' -or $result.ForegroundBootstrap -cne 'PASS'){throw 'A/B bootstrap fixture failed'}
    if($result.DirectAttempted -ne 1 -or $result.DirectSucceeded -ne [int](-not $fallback) -or $result.ActivationRequired -ne [int]$fallback -or $result.ActivationSucceeded -ne [int]$fallback -or $result.ActivationInputs -ne $(if($fallback){3}else{0})){throw 'A/B independently recomputed statistics failed'}
    ++$checks
}
# C/D/E: zero click, including an optional successful cursor move before the
# second hit-test fails. F: all three possible SendInput failure boundaries.
foreach($case in @('C','D','E','C-after-move','F-move','F-down','F-up','G','H')){
    $fixture=New-AutoBootstrapFixture $true
    $b=$fixture|Where-Object type -eq foreground_bootstrap
    switch($case){
        'C' {$first=$fixture|Where-Object type -eq activation_fence|Select-Object -First 1;$first.window_from_point_root=88;$first.window_from_point_root_matches=$false;$b.window_from_point_root_matches=$false}
        'D' {($fixture|Where-Object type -eq activation_fence|Select-Object -First 1).hit_test=2;$b.hit_test=2}
        'E' {$first=$fixture|Where-Object type -eq activation_fence|Select-Object -First 1;$first.capture_hwnd=88;$first.foreign_capture_clear=$false;$b.foreign_capture_clear=$false}
        'C-after-move' {$first=$fixture|Where-Object {$_.type -eq 'activation_fence' -and $_.phase -eq 'down'};$first.window_from_point_root=88;$first.window_from_point_root_matches=$false;$b.window_from_point_root_matches=$false}
        'F-move' {($fixture|Where-Object {$_.type -eq 'activation_input' -and $_.phase -eq 'move'}).sent=0}
        'F-down' {($fixture|Where-Object {$_.type -eq 'activation_input' -and $_.phase -eq 'down'}).sent=0}
        'F-up' {($fixture|Where-Object {$_.type -eq 'activation_input' -and $_.phase -eq 'up'}).sent=0}
        'G' {$fixture=@($fixture|Where-Object type -ne activation_event)}
        'H' {$b.final_foreground_matches=$false;$b.foreground=88}
    }
    if($case -cin @('C','D','E')){$fixture=@($fixture|Where-Object {$_.type -cnotin @('activation_input','activation_event','activation_button') -and ($_.type -cne 'activation_fence' -or $_.phase -ceq 'move')})}
    if($case -ceq 'C-after-move'){$fixture=@($fixture|Where-Object {$_.type -cnotin @('activation_event','activation_button') -and ($_.type -cnotin @('activation_input','activation_fence') -or $_.phase -ceq 'move' -or ($_.type -ceq 'activation_fence' -and $_.phase -ceq 'down'))})}
    if($case -cin @('F-move','F-down')){
        $keep=@('move');if($case -ceq 'F-down'){$keep+=@('down')}
        $fixture=@($fixture|Where-Object {$_.type -cnotin @('activation_event','activation_button') -and ($_.type -cnotin @('activation_input','activation_fence') -or $_.phase -cin $keep)})
    }
    if($case -ceq 'F-up'){$fixture=@($fixture|Where-Object {$_.type -cne 'activation_button' -or $_.message -cne 'WM_LBUTTONUP'})}
    Reset-AutoBootstrapSummary $fixture
    $fixture=Convert-AutoBootstrapBlocked $fixture ('BLOCKED_'+$case)
    $result=Test-AutoModalRecords $fixture
    if($result.Result -cne 'BLOCKED' -or $result.ActivationSucceeded -ne 0 -or $result.ForegroundBootstrap -cne 'BLOCKED'){throw "valid blocked bootstrap rejected: $case"}
    if($case -cin @('C','D','E','C-after-move') -and @($fixture|Where-Object {$_.type -eq 'activation_input' -and $_.flags -in @(2,4)}).Count -ne 0){throw 'zero-click fixture error'}
    if($case -ceq 'F-move' -and $result.ActivationAttempted -ne 1){throw 'failed activation attempt excluded from statistics'}
    ++$checks
}
$fallbackBase=New-AutoBootstrapFixture $true
foreach($fault in @('missing-bootstrap','duplicate-bootstrap','missing-contract','wrong-contract','string-boolean','wrong-target','wrong-root','wrong-hit','foreign-capture','foreign-menu','foreign-modal','gui-query-failed','foreground-unstable','modifier','button-state','phase-order','extra-activation-input','missing-proof','stale-proof','point-changed','cursor-wrong','input-tag','normal-input-tag','input-error','input-clock','pre-down-event','wrong-event-target','wrong-event-epoch','inactive-event','forged-input-summary','forged-event-summary','wrong-final-foreground','topmost-not-restored','bootstrap-after-path','activation-used-for-drag','normal-button-state','normal-no-fresh-fence','blocked-illegal-normal-input','blocked-illegal-activation-input','normal-after-input-failure')){
    $fixture=Copy-AutoFixture $fallbackBase
    $b=$fixture|Where-Object type -eq foreground_bootstrap
    $f=$fixture|Where-Object type -eq activation_fence|Select-Object -First 1
    $a=$fixture|Where-Object type -eq activation_input|Select-Object -First 1
    $nf=$fixture|Where-Object type -eq input_fence|Select-Object -First 1
    $ni=$fixture|Where-Object type -eq input|Select-Object -First 1
    switch($fault){
        'missing-bootstrap' {$b.type='not_bootstrap'}
        'duplicate-bootstrap' {$fixture[-2].type='foreground_bootstrap'}
        'missing-contract' {$fixture[0].PSObject.Properties.Remove('foreground_contract')}
        'wrong-contract' {$fixture[0].foreground_contract='unsafe'}
        'string-boolean' {$f.own_identity='true'}
        'wrong-target' {$f.target=88}
        'wrong-root' {$f.window_from_point_root=88}
        'wrong-hit' {$f.hit_test=2}
        'foreign-capture' {$f.capture_hwnd=88}
        'foreign-menu' {$f.menu_owner_hwnd=88}
        'foreign-modal' {$f.gui_flags=2}
        'gui-query-failed' {$f.gui_query_succeeded=$false}
        'foreground-unstable' {$f.foreground_snapshot_stable=$false}
        'modifier' {$f.modifiers_clear=$false}
        'button-state' {$f.left_down=$true}
        'phase-order' {$a.phase='down'}
        'extra-activation-input' {$fixture[-2].type='activation_input'}
        'missing-proof' {$f.type='not_fence'}
        'stale-proof' {($fixture|Where-Object {$_.type -eq 'activation_fence' -and $_.phase -eq 'down'}).type='not_fence'}
        'point-changed' {$a.point=@(401,302)}
        'cursor-wrong' {($fixture|Where-Object {$_.type -eq 'activation_fence' -and $_.phase -eq 'up'}).cursor=@(0,0)}
        'input-tag' {$a.input_tag=0}
        'normal-input-tag' {$ni.input_tag=0}
        'input-error' {$a.error=5}
        'input-clock' {$a.injection_start_qpc=$a.qpc+1}
        'pre-down-event' {foreach($e in @($fixture|Where-Object type -eq activation_event)){$e.activation_epoch=0}}
        'wrong-event-target' {($fixture|Where-Object type -eq activation_event|Select-Object -First 1).target=88}
        'wrong-event-epoch' {($fixture|Where-Object type -eq activation_event|Select-Object -First 1).activation_epoch=2}
        'inactive-event' {($fixture|Where-Object {$_.type -eq 'activation_event' -and $_.message -eq 'WM_ACTIVATE'}).activation_code=0}
        'forged-input-summary' {$b.sendinput_down_success=$false}
        'forged-event-summary' {$b.activation_event_seen=$false}
        'wrong-final-foreground' {$b.final_foreground_matches=$false}
        'topmost-not-restored' {$b.topmost_restored=$false}
        'bootstrap-after-path' {$b.sequence=($fixture|Where-Object type -eq path|Select-Object -First 1).sequence+1}
        'activation-used-for-drag' {$nf.type='activation_fence'}
        'normal-button-state' {$nf.left_down=$true}
        'normal-no-fresh-fence' {$nf.type='not_fence'}
        'blocked-illegal-normal-input' {$nf.foreground=88;$fixture[-1].result='BLOCKED';$fixture[-2].type='blocked';$fixture[-2]|Add-Member -NotePropertyName reason -NotePropertyValue unsafe}
        'blocked-illegal-activation-input' {$f.window_from_point_root=88;$fixture=Convert-AutoBootstrapBlocked $fixture 'unsafe'}
        'normal-after-input-failure' {$ni.sent=0;$fixture[-1].result='BLOCKED';$fixture[-2].type='blocked';$fixture[-2]|Add-Member -NotePropertyName reason -NotePropertyValue unsafe}
    }
    $reject=$false;try{$null=Test-AutoModalRecords $fixture}catch{$reject=$true};if(-not $reject){throw "invalid bootstrap fixture accepted: $fault"};++$checks
}
# Legacy blocked logs still work, but cannot hide an already unsafe normal input.
$legacy=Copy-AutoFixture (New-AutoModalFixture)
$legacy[-1].result='BLOCKED';$legacy[-2].type='blocked';$legacy[-2]|Add-Member -NotePropertyName reason -NotePropertyValue legacy_stop
if((Test-AutoModalRecords $legacy).Result -cne 'BLOCKED'){throw 'legacy blocked compatibility'};++$checks
($legacy|Where-Object type -eq input_fence|Select-Object -First 1).foreground=88
$reject=$false;try{$null=Test-AutoModalRecords $legacy}catch{$reject=$true};if(-not $reject){throw 'legacy BLOCKED hid unsafe normal input'};++$checks
foreach($fault in @('raise-missing','raise-style','raise-flags','restore-missing','restore-style','restore-failed','final-raw-foreground','final-topmost-style','attempt-result','attempt-missing','summary-hit-test')){
    $fixture=Copy-AutoFixture $fallbackBase
    $b=$fixture|Where-Object type -eq foreground_bootstrap
    $raise=$fixture|Where-Object {$_.type -eq 'activation_visibility' -and $_.enabled}
    $restore=$fixture|Where-Object {$_.type -eq 'activation_visibility' -and -not $_.enabled}
    switch($fault){
        'raise-missing' {$raise.type='not_visibility'}
        'raise-style' {$raise.topmost_style=$false}
        'raise-flags' {$raise.flags=3}
        'restore-missing' {$restore.type='not_visibility'}
        'restore-style' {$restore.topmost_style=$true}
        'restore-failed' {$restore.success=$false}
        'final-raw-foreground' {$b.foreground=88}
        'final-topmost-style' {$b.topmost_now=$true}
        'attempt-result' {($fixture|Where-Object type -eq foreground_attempt).set_foreground_success=$true}
        'attempt-missing' {($fixture|Where-Object type -eq foreground_attempt).type='not_attempt'}
        'summary-hit-test' {$b.hit_test=2}
    }
    $reject=$false;try{$null=Test-AutoModalRecords $fixture}catch{$reject=$true};if(-not $reject){throw "invalid visibility/summary fixture accepted: $fault"};++$checks
}
# Direct activation callbacks are epoch 0 and do not borrow fallback authority.
$fixture=New-AutoBootstrapFixture $false
$b=$fixture|Where-Object type -eq foreground_bootstrap
$b.wm_activate_seen=$true;$b.wm_setfocus_seen=$true
$eventRows=@(
    [pscustomobject]@{schema='r1c4b-auto-owned-modal/v1';sequence=0;type='activation_event';gesture=0;qpc=$b.qpc;message='WM_ACTIVATE';activation_code=1;activation_epoch=0;target=99;foreground_matches=$true},
    [pscustomobject]@{schema='r1c4b-auto-owned-modal/v1';sequence=0;type='activation_event';gesture=0;qpc=$b.qpc;message='WM_SETFOCUS';activation_epoch=0;target=99;foreground_matches=$true}
)
$index=$b.sequence-1;$fixture=@($fixture[0..($index-1)])+$eventRows+@($fixture[$index..($fixture.Count-1)])
for($i=0;$i -lt $fixture.Count;++$i){$fixture[$i].sequence=$i+1}
if((Test-AutoModalRecords $fixture).ForegroundBootstrap -cne 'PASS'){throw 'direct epoch0 callbacks rejected'};++$checks
$fixture=New-AutoBootstrapFixture $true
($fixture|Where-Object type -eq foreground_attempt).set_foreground_success=$true
($fixture|Where-Object type -eq foreground_bootstrap).set_foreground_success=$true
$result=Test-AutoModalRecords $fixture
if($result.ForegroundBootstrap -cne 'PASS' -or $result.DirectSucceeded -ne 0){throw 'API TRUE without fresh foreground misclassified'};++$checks
# A pre-ENTER first native sample may prime the modal loop, with the same
# exact foreground requirement and a real owned nonclient DOWN receipt.
$primingBase=New-AutoBootstrapFixture $false
$firstMove=$primingBase|Where-Object {$_.type -eq 'input' -and $_.gesture -eq 1 -and $_.flags -eq 1}|Select-Object -First 1
$primeFence=$primingBase|Where-Object {$_.type -eq 'input_fence' -and $_.sequence -eq $firstMove.sequence-1}
$primeFence.priming=$true;$primeFence.capture_hwnd=0
$nativeDown=[pscustomobject]@{schema='r1c4b-auto-owned-modal/v1';sequence=0;type='native_button_down';gesture=1;qpc=0;target=99;hit_test=2}
$expanded=[Collections.Generic.List[object]]::new()
foreach($r in $primingBase){
    if($r -eq $primeFence -or $r -eq $firstMove){continue}
    $expanded.Add($r)
    if($r.type -eq 'input' -and $r.gesture -eq 1 -and $r.flags -eq 2){$expanded.Add($nativeDown);$expanded.Add($primeFence);$expanded.Add($firstMove)}
}
$primingBase=$expanded.ToArray()
for($i=0;$i -lt $primingBase.Count;++$i){
    $r=$primingBase[$i];$r.sequence=$i+1;$r.qpc=($i+1)*100
    if($r.type -eq 'input'){$r.injection_start_qpc=$r.qpc-1;$r.injection_return_qpc=$r.qpc}
    if($r.type -eq 'T1'){$r.native_start_qpc=$r.qpc-2;$r.native_return_qpc=$r.qpc-1}
}
if((Test-AutoModalRecords $primingBase).Result -cne 'CAPTURED'){throw 'valid first-sample priming rejected'};++$checks
foreach($fault in @('foreign-capture','foreign-root','foreign-foreground','missing-native-down','wrong-native-down','second-priming','post-enter-priming','non-move-priming','pre-api-native-down')){
    $fixture=Copy-AutoFixture $primingBase
    $f=$fixture|Where-Object {$_.type -eq 'input_fence' -and $_.priming}|Select-Object -First 1
    $receipt=$fixture|Where-Object type -eq native_button_down
    switch($fault){
        'foreign-capture' {$f.capture_hwnd=88}
        'foreign-root' {$f.window_from_point_root=88}
        'foreign-foreground' {$f.foreground=88}
        'missing-native-down' {$receipt.type='not_button_down'}
        'wrong-native-down' {$receipt.hit_test=15}
        'second-priming' {$next=$fixture|Where-Object {$_.type -eq 'input_fence' -and $_.gesture -eq 1 -and $_.left_down -and -not $_.priming}|Select-Object -First 1;$next.priming=$true;$next.capture_hwnd=0}
        'post-enter-priming' {$f.priming=$false;$f.capture_hwnd=99;$next=$fixture|Where-Object {$_.type -eq 'input_fence' -and $_.gesture -eq 1 -and $_.left_down -and $_.sequence -gt $f.sequence}|Select-Object -First 1;$next.priming=$true;$next.capture_hwnd=0}
        'non-move-priming' {($fixture|Where-Object {$_.type -eq 'input_fence' -and $_.gesture -eq 1 -and -not $_.left_down}|Select-Object -First 1).priming=$true}
        'pre-api-native-down' {$down=$fixture|Where-Object {$_.type -eq 'input' -and $_.gesture -eq 1 -and $_.flags -eq 2};$receipt.qpc=$down.injection_start_qpc-1}
    }
    $reject=$false;try{$null=Test-AutoModalRecords $fixture}catch{$reject=$true};if(-not $reject){throw "invalid priming fixture accepted: $fault"};++$checks
}
function New-AutoActivationCleanupFixture([bool]$Attempted,[int]$Sent=0){
    $fixture=New-AutoBootstrapFixture $true
    $up=$fixture|Where-Object {$_.type -eq 'activation_input' -and $_.phase -eq 'up'};$up.sent=0
    $fixture=@($fixture|Where-Object {$_.type -cne 'activation_button' -or $_.message -cne 'WM_LBUTTONUP'})
    Reset-AutoBootstrapSummary $fixture
    $b=$fixture|Where-Object type -eq foreground_bootstrap;$b.activation_event_seen=$false
    $fixture=Convert-AutoBootstrapBlocked $fixture 'BLOCKED_BY_SENDINPUT'
    $proof=Copy-AutoFixture @($fixture|Where-Object {$_.type -eq 'activation_fence' -and $_.phase -eq 'up'})
    $proof=$proof[0];$proof.qpc=$up.qpc
    if(-not $Attempted){$proof.window_from_point_root=88;$proof.window_from_point_root_matches=$false;$b.window_from_point_root_matches=$false}
    $release=[pscustomobject]@{schema='r1c4b-auto-owned-modal/v1';sequence=0;type='activation_release';gesture=0;qpc=$up.qpc;attempted=$Attempted;sent=$Sent;error=0;input_tag=0x50424D41;point=@(400,300);injection_start_qpc=$up.qpc;injection_return_qpc=$up.qpc;button_release_pending=($Sent -eq 0);reason='BLOCKED_BY_ACTIVATION_HIT_TEST'}
    $expanded=[Collections.Generic.List[object]]::new()
    foreach($r in $fixture){$expanded.Add($r);if($r -eq $up){$expanded.Add($proof);$expanded.Add($release)}}
    $fixture=$expanded.ToArray();for($i=0;$i -lt $fixture.Count;++$i){$fixture[$i].sequence=$i+1}
    return ,$fixture
}
foreach($case in @('sent','failed','guard-rejected')){
    $fixture=New-AutoActivationCleanupFixture ($case -cne 'guard-rejected') $(if($case -ceq 'sent'){1}else{0})
    $result=Test-AutoModalRecords $fixture
    if($result.Result -cne 'BLOCKED' -or $result.ActivationSucceeded -ne 0 -or $result.ActivationInputs -ne 3){throw "cleanup hid original failure: $case"};++$checks
}
$cleanupBase=New-AutoActivationCleanupFixture $true 1
foreach($fault in @('missing-proof','wrong-root','foreign-capture','wrong-point','wrong-tag','wrong-clock','wrong-pending','success-with-error','release-without-down','release-after-restore','duplicate-release','false-attempted','wrong-client-target','wrong-client-message','missing-client-down','normal-gui-query-failed')){
    $fixture=Copy-AutoFixture $cleanupBase
    $release=$fixture|Where-Object type -eq activation_release
    $proof=$fixture|Where-Object {$_.type -eq 'activation_fence' -and $_.sequence -eq $release.sequence-1}
    switch($fault){
        'missing-proof' {$proof.type='not_fence'}
        'wrong-root' {$proof.window_from_point_root=88;$proof.window_from_point_root_matches=$false;($fixture|Where-Object type -eq foreground_bootstrap).window_from_point_root_matches=$false}
        'foreign-capture' {$proof.capture_hwnd=88;$proof.foreign_capture_clear=$false;($fixture|Where-Object type -eq foreground_bootstrap).foreign_capture_clear=$false}
        'wrong-point' {$release.point=@(0,0)}
        'wrong-tag' {$release.input_tag=0}
        'wrong-clock' {$release.injection_start_qpc=$release.qpc+1}
        'wrong-pending' {$release.button_release_pending=$true}
        'success-with-error' {$release.error=5}
        'release-without-down' {($fixture|Where-Object {$_.type -eq 'activation_input' -and $_.phase -eq 'down'}).sent=0}
        'release-after-restore' {($fixture|Where-Object {$_.type -eq 'activation_visibility' -and -not $_.enabled}).sequence=$release.sequence-1}
        'duplicate-release' {$fixture[-2].type='activation_release'}
        'false-attempted' {$release.attempted=$false}
        'wrong-client-target' {($fixture|Where-Object type -eq activation_button|Select-Object -First 1).target=88}
        'wrong-client-message' {($fixture|Where-Object type -eq activation_button|Select-Object -First 1).message='WM_KEYDOWN'}
        'missing-client-down' {$fixture=Copy-AutoFixture $fallbackBase;($fixture|Where-Object {$_.type -eq 'activation_button' -and $_.message -eq 'WM_LBUTTONDOWN'}).type='not_button'}
        'normal-gui-query-failed' {$fixture=Copy-AutoFixture $fallbackBase;($fixture|Where-Object {$_.type -eq 'input_fence' -and $_.left_down}|Select-Object -First 1).gui_query_succeeded=$false}
    }
    $reject=$false;try{$null=Test-AutoModalRecords $fixture}catch{$reject=$true};if(-not $reject){throw "invalid cleanup/button fixture accepted: $fault"};++$checks
}
$skipBase=New-AutoActivationCleanupFixture $true 0
$release=$skipBase|Where-Object type -eq activation_release
$proof=$skipBase|Where-Object {$_.type -eq 'activation_fence' -and $_.sequence -eq $release.sequence-1}
$skipBase=@($skipBase|Where-Object {$_ -ne $proof})
$release.attempted=$false;$release.reason='BLOCKED_BY_ACTIVATION_CLEANUP_PROOF_FAILURE'
for($i=0;$i -lt $skipBase.Count;++$i){$skipBase[$i].sequence=$i+1}
$result=Test-AutoModalRecords $skipBase
if($result.Result -cne 'BLOCKED' -or $result.ActivationSucceeded -ne 0 -or $result.Reasons -cnotcontains 'BLOCKED_BY_SENDINPUT'){throw 'no-input proof exception masked original blocker'};++$checks
foreach($fault in @('attempted','sent','pending','unknown-reason','no-down','not-blocked','up-already-succeeded')){
    $fixture=Copy-AutoFixture $skipBase;$release=$fixture|Where-Object type -eq activation_release
    switch($fault){
        'attempted' {$release.attempted=$true}
        'sent' {$release.sent=1}
        'pending' {$release.button_release_pending=$false}
        'unknown-reason' {$release.reason='unknown'}
        'no-down' {($fixture|Where-Object {$_.type -eq 'activation_input' -and $_.phase -eq 'down'}).sent=0}
        'not-blocked' {$fixture[-1].result='CAPTURED_NOT_ACCEPTED'}
        'up-already-succeeded' {($fixture|Where-Object {$_.type -eq 'activation_input' -and $_.phase -eq 'up'}).sent=1;($fixture|Where-Object type -eq foreground_bootstrap).sendinput_up_success=$true}
    }
    $reject=$false;try{$null=Test-AutoModalRecords $fixture}catch{$reject=$true};if(-not $reject){throw "invalid no-proof cleanup accepted: $fault"};++$checks
}
Write-Output "AUTO_MODAL_CLASSIFIER_FIXTURES = PASS ($checks; synthetic only)"
