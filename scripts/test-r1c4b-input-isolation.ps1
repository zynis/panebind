Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-input-isolation-validation.ps1')
# All fixtures are synthetic only. No GUI, window/input API or native probe.
$checks=0
function Check-Isolation([bool]$Value,[string]$Reason){if(-not $Value){throw "input isolation fixture: $Reason"};$script:checks++}
function Copy-IsolationFixture($Value){return ($Value|ConvertTo-Json -Depth 24 -Compress|ConvertFrom-Json)}
function New-IsolationProductProof {
    return [pscustomobject]@{end_observed=$true;winevent_end_observed=$true;own_identity=$true;desktop_ready=$true;source_visible=$true;foreground_matches=$true;gui_query_succeeded=$true;capture_clear=$true;menu_clear=$true;move_size_clear=$true;gui_in_movesize_clear=$true;left_down=$true;buttons_modifiers_clear=$true;raw_up_seen=$false;receiver_healthy=$true;foreign_capture_transferred=$false;dpi_matches=$true;monitor_matches=$true;actual_positioning_available=$true;actual_visible_available=$true;terminal_positioning_available=$true;terminal_visible_available=$true;terminal_positioning_exact=$true;terminal_visible_exact=$true;native_drag_after_cancel_return=0;native_drag_after_end=0;native_drag_after_winevent_end=0;unowned_geometry_changes=0;takeover_healthy=$true}
}
function New-IsolationTestProof {
    return [pscustomobject]@{cursor_available=$true;source_identity=$true;desktop_ready=$true;foreground_matches=$true;native_end_observed=$true;winevent_end_observed=$true;product_authority_passed=$true;shield_needed=$true;shield_identity=$true;shield_noactivate=$true;shield_topmost=$true;shield_nonoverlap=$true;shield_created_after_product=$true;current_point_owned=$true;all_planned_points_owned=$true}
}
$p=New-IsolationProductProof
$p|Add-Member -NotePropertyName cursor_root_owned_or_guard -NotePropertyValue $false
$p|Add-Member -NotePropertyName cursor_success -NotePropertyValue $false
Check-Isolation (@(Get-ProductGestureAuthorityChecks $p).Count -eq 0) 'A product authority never depends on foreign cursor root or test cursor sampling'
$t=New-IsolationTestProof;$t.current_point_owned=$false
Check-Isolation (@(Get-TestSyntheticIsolationChecks $t) -join ',' -ceq 'CurrentPointNotTestOwned') 'A separate test input isolation remains blocked'
$t=New-IsolationTestProof
Check-Isolation (@(Get-TestSyntheticIsolationChecks $t).Count -eq 0) 'B exact post-END test-only shield can establish isolation'
foreach($case in @(@('shield_created_after_product','ShieldCreatedBeforeAuthority'),@('foreground_matches','ForegroundChanged'),@('shield_nonoverlap','ShieldOverlapsSource'),@('all_planned_points_owned','PlannedPointNotTestOwned'),@('shield_identity','ShieldIdentityChanged'),@('shield_noactivate','ShieldCanActivate'),@('shield_topmost','ShieldNotTopmost'))){$bad=Copy-IsolationFixture $t;$bad.($case[0])=$false;Check-Isolation (@(Get-TestSyntheticIsolationChecks $bad) -join ',' -ceq $case[1]) "C–G distinct isolation failure $($case[1])"}
Check-Isolation (-not (Test-IsolationRectsOverlap @(250,540,351,711) @(100,100,740,540))) 'E touching bottom boundary is not client/caption overlap'
Check-Isolation (Test-IsolationRectsOverlap @(250,539,351,711) @(100,100,740,540)) 'E one-pixel overlap is still rejected'
Check-Isolation (Test-IsolationPointInside @(300,539) @(100,100,740,540)) 'half-open source border includes final inside pixel'
Check-Isolation (-not (Test-IsolationPointInside @(300,540) @(100,100,740,540))) 'half-open source bottom belongs outside the source'
foreach($case in @(@('end_observed','MissingNativeEnd'),@('winevent_end_observed','MissingWinEventEnd'),@('own_identity','IdentityChanged'),@('desktop_ready','DesktopUnavailable'),@('source_visible','SourceNotVisible'),@('foreground_matches','ForegroundChanged'),@('gui_query_succeeded','GuiQueryFailed'),@('capture_clear','CaptureStillOwned'),@('menu_clear','MenuStillActive'),@('move_size_clear','MoveSizeStillActive'),@('left_down','ButtonReleasedBeforeHandoff'),@('receiver_healthy','ReceiverUnhealthy'),@('terminal_positioning_exact','TerminalPositioningMismatch'),@('terminal_visible_exact','TerminalVisibleMismatch'))){$bad=Copy-IsolationFixture $p;$bad.($case[0])=$false;Check-Isolation (@(Get-ProductGestureAuthorityChecks $bad) -join ',' -ceq $case[1]) "product factual check $($case[1])"}
Check-Isolation (Test-IsolationIdentity @(500,100,101,9001) @(500,100,101,9001)) 'G exact shield generation tuple'
foreach($index in @(0,1,2,3)){$bad=@(500,100,101,9001);$bad[$index]++;Check-Isolation (-not (Test-IsolationIdentity $bad @(500,100,101,9001))) "G wrong exact identity member $index cannot be authorized"}
Check-Isolation (-not (Test-IsolationWriteTiming 100 50 70 100)) 'H source first native entry strictly follows ready'
Check-Isolation (-not (Test-IsolationWriteTiming 101 50 100 70)) 'C/H readiness cannot precede matching public END'
Check-Isolation (Test-IsolationWriteTiming 101 50 70 100) 'H independent END and isolation-ready barriers'
Check-Isolation ((Get-IsolationWriteActor 'shield' 500 300 500) -ceq 'shield' -and (Get-IsolationWriteActor 'source' 300 300 500) -ceq 'source') 'I shield setup operation is never counted as source geometry write'
$caught=$false;try{$null=Get-IsolationWriteActor 'source' 500 300 500}catch{$caught=$true};Check-Isolation $caught 'I cannot disguise a shield operation as a source write'
Check-Isolation (-not (Test-IsolationDestroyTiming 100 101 99)) 'J shield cannot be removed before accepted source completion'
Check-Isolation (-not (Test-IsolationDestroyTiming 100 99 101)) 'J shield cannot be removed before real Raw UP'
Check-Isolation (Test-IsolationDestroyTiming 102 101 99) 'K exact shield destruction after both acceptance and Raw UP'
# Load only named pure fixture functions, never old script top-level workflows.
foreach($file in @('test-r1c4b-end-handoff.ps1','test-r1c4b-end-diagnostics.ps1')){
    $tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $file),[ref]$tokens,[ref]$errors);Check-Isolation ($errors.Count -eq 0) "immutable pure fixture source parses $file"
    $names=if($file -like '*end-handoff*'){@('Add-EndRow','End-Geometry','End-Authority','End-Diagnostic','Add-EndInput','Add-EndRaw','Add-EndUpFence','Add-EndWriter','New-EndHandoffFixture','Renumber-EndFixture')}else{@('Copy-EndDiagnostic','New-EndDiagnosticProof','Complete-CProof','Add-CFixtureFields','New-CFixture')}
    foreach($name in $names){$f=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name},$true);if($null -eq $f){throw "missing pure fixture $name"};Invoke-Expression $f.Extent.Text}
}
function Set-DField($Row,[string]$Name,$Value){$Row|Add-Member -NotePropertyName $Name -NotePropertyValue $Value -Force}
function New-DRow([string]$Type,$Fields){$r=[ordered]@{schema='r1c4b-takeover-owned/v4';sequence=0;gesture=$script:dg;qpc=0;type=$Type};foreach($name in $Fields.PSObject.Properties.Name){$r[$name]=$Fields.$name};return [pscustomobject]$r}
function D-OwnerFields($Row,[bool]$Up=$false){foreach($pair in @(@('run_nonce',9001),@('actual_source_pid',100),@('actual_source_tid',101),@('actual_source_nonce',9001),@('cursor_root_required_for_product',$false),@('cursor_data_ready',$true),@('product_authority',$true))){Set-DField $Row $pair[0] $pair[1]};if($Up){$Row.raw_up_seen=$true;$Row.left_down=$false}}
function New-DPoint($SourceP,$ShieldP,$Destination,[bool]$Needed){
    $inside=Test-IsolationPointInside $Destination $SourceP;$shield=if($Needed){500}else{0};$root=if($inside){300}else{$shield}
    return [pscustomobject]@{run_nonce=9001;source_hwnd=300;source_pid=100;source_tid=101;actual_source_pid=100;actual_source_tid=101;actual_source_nonce=9001;source_identity=$true;shield_hwnd=$shield;shield_pid=100;shield_tid=101;actual_shield_pid=$(if($Needed){100}else{0});actual_shield_tid=$(if($Needed){101}else{0});actual_shield_nonce=$(if($Needed){9001}else{0});shield_identity=$Needed;cursor_available=$true;destination=@($Destination);source_positioning_available=$true;source_positioning=@($SourceP);shield_positioning=$(if($Needed){@($ShieldP)}else{$null});inside_source=$inside;root=$root;expected_root=$root;root_owned=$true;foreground_hwnd=300;foreground_pid=100;foreground_tid=101;foreground_matches=$true;shield_needed=$Needed;shield_active=$Needed;shield_noactivate=$Needed;shield_topmost=$Needed;shield_visible=$Needed;shield_never_activated=$true;shield_nonoverlapping=$true;point_valid=$true;failure_class='None'}
}
function New-DPosition($SourceP,$SourceV,[int]$Operation,[bool]$Needed){
    $rect=@(250,[Math]::Max(540,$SourceP[3]),351,710);$p=New-DPoint $SourceP $rect @(300,659) $Needed
    foreach($pair in @(@('actor','shield'),@('source_operation_id',$Operation),@('source_postverify_sequence',0),@('target',500),@('insert_after',-1),@('flags',80),@('native_start_qpc',0),@('native_return_qpc',0),@('native_success',$true),@('error',0),@('requested_positioning',$rect),@('actual_positioning',$rect),@('terminal_source_positioning',@(100,100,740,540)),@('source_positioning_before',@($SourceP)),@('source_positioning_after',@($SourceP)),@('source_visible_before',@($SourceV)),@('source_visible_after',@($SourceV)),@('source_unchanged',$true),@('foreground_before',300),@('foreground_after',300),@('foreground_unchanged',$true),@('planned_min_x',300),@('planned_max_x',300),@('planned_min_y',551),@('planned_max_y',659),@('margin_px',50),@('top_margin_clipped',$true),@('work_area',@(0,0,1920,1080)),@('work_area_contained',$true))){Set-DField $p $pair[0] $pair[1]}
    return New-DRow 'input_shield_position' $p
}
function Repair-DReferences($Rows){
    for($index=0;$index -lt $Rows.Count;++$index){$Rows[$index].sequence=$index+1;$Rows[$index].qpc=1000+($index+1)*100}
    $armed=$Rows|Where-Object type -eq gesture_armed;$enter=$Rows|Where-Object type -eq ENTER;$down=$Rows|Where-Object type -eq native_button_down;$exit=$Rows|Where-Object type -eq EXIT;$win=$Rows|Where-Object {$_.type -eq 'winevent_callback' -and $_.event -eq 11};$final=$Rows|Where-Object {$_.type -eq 'product_handoff_preflight' -and $_.phase -eq 'winevent_end'};$ready=$Rows|Where-Object type -eq input_isolation_ready
    foreach($r in $Rows){
        switch($r.type){
            'gesture_armed'{$r.arm_qpc=$r.qpc-1}
            'input'{$r.injection_start_qpc=$r.qpc-90;$r.injection_return_qpc=$r.qpc-80}
            'raw_input'{$r.receiver_qpc=$r.qpc-1}
            'receiver'{$r.receiver_qpc=$r.qpc-1}
            'receiver_shutdown'{$r.receiver_qpc=$r.qpc-1}
            'ENTER'{$r.receipt_qpc=$r.qpc-1}
            'DRAG'{$r.receipt_qpc=$r.qpc-1}
            'EXIT'{$r.receipt_qpc=$r.qpc-1;$r.end_sequence=$r.sequence}
            'POSITION_CHANGED'{$r.position_receipt_qpc=$r.qpc-1}
            'winevent_callback'{$r.callback_qpc=$r.qpc-1;$r.arm_qpc=$armed.qpc-1}
            'winevent_match'{$r.callback_record_sequence=($Rows|Where-Object {$_.type -eq 'winevent_callback' -and $_.callback_sequence -eq $r.callback_sequence}).sequence}
            'intent_anchor'{$r.native_enter_sequence=$enter.sequence;$r.native_enter_qpc=$enter.qpc-1;$r.native_down_sequence=$down.sequence}
            'cancel_return'{$r.issued_qpc=($Rows|Where-Object type -eq cancel_begin).qpc-1;$r.returned_qpc=$r.qpc-1}
            'raw_wait_begin'{$previous=$Rows|Where-Object {$_.type -eq 'input' -and $_.sequence -lt $r.sequence}|Select-Object -Last 1;$r.input_start_qpc=$previous.injection_start_qpc}
            'raw_motion_correlation'{$packet=$Rows|Where-Object {$_.type -eq 'raw_input' -and $_.receiver_sequence -eq $r.receiver_sequence};$r.snapshot_cursor_matches=$packet.cursor_success -and (Test-AutoPoint $packet.cursor ($Rows|Where-Object {$_.type -eq 'input' -and $_.sequence -lt $r.sequence}|Select-Object -Last 1).point)}
            'raw_preflight_outcome'{$up=$Rows|Where-Object {$_.type -eq 'input' -and $_.gesture -eq 0 -and $_.flags -eq 4};$r.up_wait_started_qpc=$up.injection_return_qpc+1;$r.up_wait_finished_qpc=$r.qpc-1}
            'product_handoff_preflight'{$r.end_sequence=$exit.sequence;$r.end_qpc=$exit.qpc-1;if($r.phase -eq 'winevent_end'){$r.winevent_end_sequence=$win.sequence;$r.winevent_end_qpc=$win.qpc-1}}
            'input_isolation_setup_plan'{$r.product_preflight_sequence=$final.sequence}
            'input_shield_created'{$r.create_start_qpc=$r.qpc-90;$r.create_return_qpc=$r.qpc-80;$r.product_preflight_sequence=$final.sequence;$r.winevent_end_qpc=$win.qpc-1}
            'input_shield_position'{$r.native_start_qpc=$r.qpc-90;$r.native_return_qpc=$r.qpc-80;if($r.source_operation_id){$r.source_postverify_sequence=($Rows|Where-Object {$_.type -eq 'writer_result' -and $_.operation_id -eq $r.source_operation_id}).sequence}}
            'input_isolation_ready'{$r.product_preflight_sequence=$final.sequence;$r.end_qpc=$exit.qpc-1;$r.winevent_end_qpc=$win.qpc-1;$r.isolation_ready_qpc=$r.qpc-1}
            'handoff_begin'{$r.end_sequence=$exit.sequence;$r.end_qpc=$exit.qpc-1}
            'writer_input_isolation'{$r.product_preflight_sequence=$final.sequence;$r.input_isolation_ready_sequence=$ready.sequence}
            'writer_begin'{$r.end_qpc=$exit.qpc-1;$r.winevent_end_sequence=$win.sequence;$r.winevent_end_qpc=$win.qpc-1;$r.handoff_preflight_sequence=$final.sequence;$r.product_preflight_sequence=$final.sequence;$r.input_isolation_ready_sequence=$ready.sequence;$r.input_isolation_ready_qpc=$ready.qpc-1;$r.writer_input_isolation_sequence=($Rows|Where-Object {$_.type -eq 'writer_input_isolation' -and $_.operation_id -eq $r.operation_id}).sequence;if($r.kind -eq 'raw_movement'){$r.raw_trigger_qpc=($Rows|Where-Object {$_.type -eq 'raw_input' -and $_.receiver_sequence -eq $r.raw_last_sequence}).qpc-1}}
            'writer_result'{$b=$Rows|Where-Object {$_.type -eq 'writer_begin' -and $_.operation_id -eq $r.operation_id};$r.native_start_qpc=if($r.native_calls){$b.qpc+1}else{0};$r.native_return_qpc=if($r.native_calls){$b.qpc+2}else{0}}
            'raw_quantum'{$r.raw_trigger_qpc=($Rows|Where-Object {$_.type -eq 'raw_input' -and $_.receiver_sequence -eq $r.raw_last_sequence}).qpc-1}
            'handoff_complete'{$r.end_qpc=$exit.qpc-1}
            'synthetic_input_isolation'{$r.input_isolation_ready_sequence=$ready.sequence}
            'takeover_end'{$r.raw_up_receiver_qpc=($Rows|Where-Object {$_.type -eq 'raw_input' -and $_.receiver_sequence -eq $r.raw_up_receiver_sequence}).qpc-1}
            'source_final_acceptance'{$r.takeover_end_sequence=($Rows|Where-Object type -eq takeover_end).sequence;$r.raw_up_receiver_qpc=($Rows|Where-Object {$_.type -eq 'raw_input' -and $_.receiver_sequence -eq $r.raw_up_receiver_sequence}).qpc-1}
            'input_shield_destroyed'{$r.source_acceptance_sequence=($Rows|Where-Object type -eq source_final_acceptance).sequence;$r.raw_up_receiver_qpc=($Rows|Where-Object type -eq takeover_end).raw_up_receiver_qpc;$r.destroy_start_qpc=$r.qpc-90;$r.destroy_return_qpc=$r.qpc-80}
            'shutdown'{$r.source_acceptance_sequence=($Rows|Where-Object type -eq source_final_acceptance).sequence}
        }
    }
    return ,$Rows
}
function New-DWire([ValidateSet('Move','BottomResize')][string]$Operation='BottomResize'){
    $base=New-CFixture $Operation;$script:dg=if($Operation -eq 'Move'){1}else{2};$needed=$Operation -eq 'BottomResize';$rows=[Collections.Generic.List[object]]::new();$frame=@(100,100,740,540);$visible=@(111,100,729,529);$shieldRect=@(250,540,351,710)
    $base[0].diagnostic_contract='separated_authority_v1';foreach($pair in @(@('authority_contract','product_gesture_v1'),@('input_isolation_contract','post_end_shield_v1'),@('run_nonce',9001))){Set-DField $base[0] $pair[0] $pair[1]}
    foreach($r in $base){
        $r.schema='r1c4b-takeover-owned/v4'
        if($r.type -eq 'input' -and $r.restoring_cursor){continue};if($r.type -eq 'cursor_restore_begin'){continue}
        if($r.type -eq 'input' -and $r.gesture -eq $script:dg -and $r.flags -eq 4){$r.point=@(($base|Where-Object {$_.type -eq 'sample' -and $_.index -eq 20}).cursor)}
        if($r.type -eq 'owned'){Set-DField $r run_nonce 9001;Set-DField $r actual_source_nonce 9001}
        if($r.type -eq 'winevent_match'){Set-DField $r run_nonce 9001;Set-DField $r actual_source_nonce 9001}
        if($r.type -eq 'handoff_preflight'){
            $r.type='product_handoff_preflight';$r.write_authorized=$false;Set-DField $r cursor_root_required_for_product $false;Set-DField $r run_nonce 9001;Set-DField $r actual_source_nonce 9001
            if($needed){$r.cursor_root=999;$r.cursor_root_owned_or_guard=$false}
            $failed=@(Get-ProductGestureAuthorityChecks $r);$pass=$failed.Count -eq 0;$r.failed_checks=$failed;$r.failure_class=if($pass){'None'}else{$failed[0]};Set-DField $r product_failed_checks $failed;Set-DField $r product_failure_class $r.failure_class;Set-DField $r product_authority $pass;Set-DField $r product_gate_passed $pass
        }
        if($r.type -in @('handoff_begin','writer_begin','takeover_end')){D-OwnerFields $r ($r.type -eq 'takeover_end')}
        if($r.type -eq 'input' -and $r.gesture -eq $script:dg -and ($base|Where-Object type -eq EXIT).sequence -lt $r.sequence){
            $p=New-DPoint $frame $shieldRect $r.point $needed;foreach($pair in @(@('phase','api_boundary'),@('flags',$r.flags),@('input_scope','gesture'),@('acceptance_eligible',$true),@('input_isolation_ready_sequence',0))){Set-DField $p $pair[0] $pair[1]};$rows.Add((New-DRow 'synthetic_input_isolation' $p))
        }
        if($r.type -eq 'writer_begin'){
            $p=New-DPoint $frame $shieldRect $r.cursor $needed;foreach($pair in @(@('operation_id',$r.operation_id),@('quantum_id',$r.quantum_id),@('kind',$r.kind),@('writer_product_authority',$true),@('product_preflight_sequence',0),@('input_isolation_ready_sequence',0))){Set-DField $p $pair[0] $pair[1]};$rows.Add((New-DRow 'writer_input_isolation' $p))
            foreach($pair in @(@('actor','source'),@('product_preflight_sequence',0),@('input_isolation_ready_sequence',0),@('input_isolation_ready_qpc',0),@('writer_input_isolation_sequence',0))){Set-DField $r $pair[0] $pair[1]};$r.cursor_root=$p.root
        }
        if($r.type -eq 'input_fence' -and $r.post_cancel -and $needed){$r.window_from_point_root=if(Test-IsolationPointInside $r.cursor $frame){300}else{500}}
        if($r.type -eq 'shutdown'){foreach($pair in @(@('input_shield_created',$needed),@('input_shield_destroyed',$true),@('input_shield_activated',$false),@('source_acceptance_sequence',0),@('run_nonce',9001))){Set-DField $r $pair[0] $pair[1]};$r.cursor_restored=$false}
        $rows.Add($r)
        if($r.type -eq 'product_handoff_preflight' -and $r.phase -eq 'winevent_end'){
            $path=$base|Where-Object type -eq path;$planned=Get-IsolationPlannedPoints $path $r.cursor
            $rows.Add((New-DRow 'input_isolation_setup_plan' ([pscustomobject]@{run_nonce=9001;product_preflight_sequence=0;planned_current_cursor=@($r.cursor);actual_current_cursor=@($r.cursor);current_cursor_matches_plan=$true;planned_points=$planned;work_area=@(0,0,1920,1080);points_work_area_contained=$true;shield_plan_valid=$true;shield_plan_needed=$needed;shield_plan_positioning=$(if($needed){@($shieldRect)}else{$null});shield_plan_work_area_contained=$true;test_data_ready=$true;failure_class='None'})))
            if($needed){$rows.Add((New-DRow 'input_shield_created' ([pscustomobject]@{actor='shield';shield_hwnd=500;shield_pid=100;shield_tid=101;run_nonce=9001;actual_shield_pid=100;actual_shield_tid=101;actual_shield_nonce=9001;nonce_error=0;create_tid=101;create_start_qpc=0;create_return_qpc=0;style=2147483648;exstyle=134217728;parent=0;product_preflight_sequence=0;winevent_end_qpc=0;create_positioning=@($shieldRect)})));$rows.Add((New-DPosition $frame $visible 0 $needed))}
            for($index=2;$index -le 20;++$index){$p=New-DPoint $frame $shieldRect $planned[$index-2] $needed;Set-DField $p phase 'setup';Set-DField $p index $index;$rows.Add((New-DRow 'input_isolation_point' $p))}
            $rows.Add((New-DRow 'input_isolation_ready' ([pscustomobject]@{checked_count=19;shield_needed=$needed;shield_state=$(if($needed){'READY'}else{'NOT_NEEDED'});run_nonce=9001;product_preflight_sequence=0;end_qpc=0;winevent_end_qpc=0;isolation_ready_qpc=0;test_input_isolation=$true})))
        }
        if($r.type -eq 'writer_result'){Set-DField $r actor 'source';$frame=@($r.full_positioning);$visible=@($r.full_visible);if($needed){$shieldRect=@(250,$frame[3],351,710);$rows.Add((New-DPosition $frame $visible $r.operation_id $needed))}}
        if($r.type -eq 'path_complete'){
            Set-DField $r final_left_down $false;$t=Copy-IsolationFixture ($base|Where-Object type -eq takeover_end);$t.type='source_final_acceptance';$t.sequence=0;D-OwnerFields $t $true;Set-DField $t takeover_end_sequence 0;Set-DField $t final_left_down $false;$rows.Add($t)
            if($needed){$rows.Add((New-DRow 'input_shield_destroyed' ([pscustomobject]@{actor='shield';shield_hwnd=500;shield_pid=100;shield_tid=101;run_nonce=9001;actual_shield_pid=100;actual_shield_tid=101;actual_shield_nonce=9001;own_identity=$true;reason='acceptance';source_acceptance_sequence=0;raw_up_receiver_qpc=0;destroy_start_qpc=0;destroy_return_qpc=0;destroy_success=$true;window_absent=$true})))}
            $rows.Add((New-DRow 'cursor_restore_skipped' ([pscustomobject]@{reason='strict_test_input_isolation';point=@(10,10);cursor_restored=$false;synthetic_input_sent=$false})))
        }
    }
    return ,(Repair-DReferences @($rows.ToArray()))
}
$resizeWire=New-DWire BottomResize
$mapped=Test-InputIsolationOwnedRecords $resizeWire -AllowSynthetic
Check-Isolation ($mapped.Result -ceq 'PASS' -and $mapped.ProductHandoffAuthority -ceq 'PASS' -and $mapped.TestInputIsolation -ceq 'PASS' -and $mapped.PostEndInputShield -ceq 'PASS' -and $mapped.NativeWrites -eq 19 -and $mapped.ShieldNativeCalls -eq 20 -and $mapped.ContinuationQuanta -eq 18 -and $mapped.FinalLeftDown -eq $false) 'B/K complete BottomResize v4 wire independently proves full original anchor, actual Raw UP and shield teardown'
$moveWire=New-DWire Move;$mapped=Test-InputIsolationOwnedRecords $moveWire -AllowSynthetic
Check-Isolation ($mapped.Result -ceq 'PASS' -and $mapped.PostEndInputShield -ceq 'NOT_NEEDED') 'Move never mechanically creates a shield when all points resolve source'
function Reject-DWire($Rows,[string]$Reason){$rejected=$false;try{$r=Test-InputIsolationOwnedRecords $Rows -AllowSynthetic;$rejected=$r.Result -cne 'PASS'}catch{$rejected=$true};Check-Isolation $rejected $Reason}
function Stop-DWire($Rows,[long]$At,[string]$Reason){
    $prefix=@($Rows|Where-Object sequence -le $At);$script:dg=$prefix[-1].gesture;$clock=$prefix[-1].qpc;$serial=($prefix|Where-Object type -eq raw_input|Select-Object -Last 1).receiver_sequence;$writes=@($prefix|Where-Object type -eq writer_result|Measure-Object native_calls -Sum);$count=if($writes.Count){$writes[0].Sum}else{0}
    $extra=@((New-DRow 'blocked' ([pscustomobject]@{reason=$Reason})))
    if(@($prefix|Where-Object type -eq input_shield_created).Count -and -not @($prefix|Where-Object type -eq input_shield_destroyed).Count){$extra+=,(New-DRow 'input_shield_destroyed' ([pscustomobject]@{actor='shield';shield_hwnd=500;shield_pid=100;shield_tid=101;run_nonce=9001;actual_shield_pid=100;actual_shield_tid=101;actual_shield_nonce=9001;own_identity=$true;reason='failure';source_acceptance_sequence=0;raw_up_receiver_qpc=0;destroy_start_qpc=$clock+1;destroy_return_qpc=$clock+2;destroy_success=$true;window_absent=$true}))}
    $remove=Copy-IsolationFixture ($Rows|Where-Object type -eq winevent_hook_removed);$extra+=,$remove
    $extra+=,(New-DRow 'receiver_shutdown' ([pscustomobject]@{receiver_sequence=$serial+1;receiver_qpc=0;receiver_hwnd=400;receiver_pid=200;receiver_tid=201;registration_removed=$true;window_destroyed=$true}))
    $extra+=,(New-DRow 'shutdown' ([pscustomobject]@{result='BLOCKED';cursor_restored=$false;owned_window_destroyed=$true;guard_window_destroyed=$true;receiver_stopped=$true;external_windows_touched=$false;takeover_geometry_writes=[long]$count;winevent_hook_removed=$true;native_drag_after_winevent_end=0;input_shield_created=(@($prefix|Where-Object type -eq input_shield_created).Count -gt 0);input_shield_destroyed=$true;input_shield_activated=(@($prefix|Where-Object type -eq input_shield_activation).Count -gt 0);source_acceptance_sequence=0;run_nonce=9001}))
    foreach($r in $extra){$r.sequence=$prefix.Count+1;$clock+=100;$r.qpc=$clock;if($r.type -eq 'receiver_shutdown'){$r.receiver_qpc=$clock-1};$prefix+=,$r}
    return ,$prefix
}

# A–K use the full actual v4 wire and retain the original source writer math.
foreach($case in @('pre-end-create','foreground-change','source-overlap','planned-foreign','wrong-shield-pid','wrong-shield-tid','wrong-shield-nonce','source-write-before-ready','actor-confusion','early-teardown','before-raw-up-teardown','later-foreign-writer','wrong-source-nonce','wrong-source-pid','wrong-guard-tid','foreign-restore','forged-current-plan','outside-workarea','missing-point','missing-up','wrong-bottom-edge','exit-intent-anchor','cleanup-up-as-acceptance','forged-nonoverlap','wrong-source-postverify')){
    $bad=Copy-IsolationFixture $resizeWire
    switch($case){
        'pre-end-create'{$c=$bad|Where-Object type -eq input_shield_created;$c.create_start_qpc=($bad|Where-Object {$_.type -eq 'winevent_callback' -and $_.event -eq 11}).callback_qpc;$bad=Stop-DWire $bad $c.sequence $case}
        'foreground-change'{$p=$bad|Where-Object type -eq input_shield_position|Select-Object -First 1;$p.foreground_after=999;$p.foreground_unchanged=$false;$p.foreground_hwnd=999;$p.foreground_pid=555;$p.foreground_tid=556;$p.foreground_matches=$false;$p.point_valid=$false;$p.failure_class='TestInputIsolationUnavailable';$bad=Stop-DWire $bad $p.sequence $case}
        'source-overlap'{$p=$bad|Where-Object type -eq input_shield_position|Select-Object -First 1;$p.actual_positioning[1]=539;$p.shield_positioning[1]=539;$p.shield_nonoverlapping=$false;$p.point_valid=$false;$p.failure_class='TestInputIsolationUnavailable';$bad=Stop-DWire $bad $p.sequence $case}
        'planned-foreign'{$p=$bad|Where-Object {$_.type -eq 'input_isolation_point' -and $_.index -eq 5};$p.root=999;$p.root_owned=$false;$p.point_valid=$false;$p.failure_class='TestInputIsolationUnavailable';$bad=Stop-DWire $bad $p.sequence $case}
        'wrong-shield-pid'{($bad|Where-Object type -eq input_shield_created).actual_shield_pid=999}
        'wrong-shield-tid'{($bad|Where-Object type -eq input_shield_created).actual_shield_tid=999}
        'wrong-shield-nonce'{($bad|Where-Object type -eq input_shield_created).actual_shield_nonce=9002}
        'source-write-before-ready'{$r=$bad|Where-Object {$_.type -eq 'writer_result' -and $_.operation_id -eq 1};$r.native_start_qpc=($bad|Where-Object type -eq input_isolation_ready).isolation_ready_qpc}
        'actor-confusion'{($bad|Where-Object type -eq input_shield_position|Select-Object -First 1).actor='source'}
        'early-teardown'{($bad|Where-Object type -eq input_shield_destroyed).destroy_start_qpc=($bad|Where-Object type -eq source_final_acceptance).qpc}
        'before-raw-up-teardown'{($bad|Where-Object type -eq input_shield_destroyed).destroy_start_qpc=($bad|Where-Object type -eq takeover_end).raw_up_receiver_qpc}
        'later-foreign-writer'{$p=$bad|Where-Object {$_.type -eq 'writer_input_isolation' -and $_.operation_id -eq 2};$p.root=999;$p.root_owned=$false;$p.point_valid=$false;$p.failure_class='TestInputIsolationUnavailable';$bad=Stop-DWire $bad $p.sequence $case}
        'wrong-source-nonce'{($bad|Where-Object {$_.type -eq 'writer_begin' -and $_.operation_id -eq 1}).actual_source_nonce=9002}
        'wrong-source-pid'{($bad|Where-Object type -eq owned).pid=999}
        'wrong-guard-tid'{($bad|Where-Object type -eq guard).tid=999}
        'foreign-restore'{$r=$bad|Where-Object {$_.type -eq 'input' -and $_.gesture -eq 2 -and $_.flags -eq 1}|Select-Object -Last 1;$r.restoring_cursor=$true;$r.point=@(10,10)}
        'forged-current-plan'{($bad|Where-Object type -eq input_isolation_setup_plan).actual_current_cursor=@(300,900)}
        'outside-workarea'{$p=$bad|Where-Object type -eq input_shield_position|Select-Object -First 1;$p.actual_positioning[3]=1081;$p.shield_positioning[3]=1081;$p.work_area_contained=$false;$bad=Stop-DWire $bad $p.sequence $case}
        'missing-point'{$bad=@($bad|Where-Object {-not ($_.type -eq 'input_isolation_point' -and $_.index -eq 7)});for($i=0;$i -lt $bad.Count;++$i){$bad[$i].sequence=$i+1}}
        'missing-up'{($bad|Where-Object {$_.type -eq 'raw_input' -and ($_.button_flags -band 2) -ne 0}|Select-Object -Last 1).button_flags=0}
        'wrong-bottom-edge'{($bad|Where-Object {$_.type -eq 'sample' -and $_.index -eq 3}).positioning[0]++}
        'exit-intent-anchor'{($bad|Where-Object type -eq intent_anchor).pointer_down=@(300,551)}
        'cleanup-up-as-acceptance'{$t=$bad|Where-Object type -eq takeover_end;$p=$bad|Where-Object {$_.type -eq 'raw_input' -and $_.receiver_sequence -eq $t.raw_up_receiver_sequence};$p.raw_scope='cleanup';$p.cleanup_observation=$true;$p.acceptance_eligible=$false}
        'forged-nonoverlap'{($bad|Where-Object type -eq input_shield_position|Select-Object -First 1).shield_nonoverlapping=$false}
        'wrong-source-postverify'{($bad|Where-Object {$_.type -eq 'input_shield_position' -and $_.source_operation_id -eq 1}).source_postverify_sequence=1}
    }
    Reject-DWire $bad "full v4 A–K negative $case"
    if($case -in @('pre-end-create','foreground-change','source-overlap','planned-foreign','later-foreign-writer')){
        $r=Test-InputIsolationOwnedRecords $bad -AllowSynthetic
        Check-Isolation ($r.Result -ceq 'FAIL' -and $r.ProductHandoffAuthority -ceq 'PASS' -and $r.TestInputIsolation -ceq 'FAIL' -and $r.Architecture -ceq 'UNRESOLVED') "actual typed $case fails new isolation, not product geometry authority"
        if($case -eq 'later-foreign-writer'){Check-Isolation ($r.NativeWrites -eq 1 -and $r.ContinuationQuanta -eq 0) 'later foreign owner-cursor proof prevents the next source write even after setup/input isolation passed'}
    }
}
$caught=$false;try{$null=Test-InputIsolationOwnedRecords $resizeWire}catch{$caught=$true};Check-Isolation $caught 'synthetic wire can never be accepted as empirical evidence by default'
function New-DCleanupState([bool]$Held,[bool]$Retired=$false){
    return [pscustomobject]@{target=300;guard=301;source_pid=100;source_tid=101;actual_source_pid=100;actual_source_tid=101;run_nonce=9001;actual_source_nonce=9001;test_down_owned=$true;own_identity=$true;desktop_ready=$true;source_visible=$true;foreground_hwnd=300;foreground_matches=$true;foreground_pid=100;foreground_tid=101;gui_query_succeeded=$true;gui_error=0;capture_hwnd=0;no_foreign_capture=$true;menu_owner_hwnd=0;menu_clear=$true;move_size_hwnd=0;move_size_clear=$true;gui_flags=0;gui_mode_clear=$true;cursor_success=$true;cursor_error=0;cursor=@(300,551);cursor_root=500;cursor_root_owned_or_guard=$true;left_down=$Held;buttons_modifiers_clear=$true;foreign_capture_transferred=$false;acceptance_eligible=$false;stop_requested=$Retired;source_retired=$Retired;fixture_scope_active=(-not $Retired)}
}
function Add-DCleanup($Rows,[ValidateSet('PASS','SKIPPED','NOT_NEEDED')][string]$Mode){
    $prefix=@($Rows|Where-Object {$_.type -notin @('input_shield_destroyed','winevent_hook_removed','receiver_shutdown','shutdown')});$clock=$prefix[-1].qpc;$script:dg=2;$serial=($prefix|Where-Object type -eq raw_input|Select-Object -Last 1).receiver_sequence
    $initial=New-DCleanupState ($Mode -ne 'NOT_NEEDED') ($Mode -eq 'SKIPPED');Set-DField $initial phase 'initial';Set-DField $initial eligible ($Mode -eq 'PASS');Set-DField $initial outcome $(if($Mode -eq 'PASS'){'ELIGIBLE'}elseif($Mode -eq 'NOT_NEEDED'){'NOT_NEEDED'}else{'SKIPPED_NO_AUTHORITY'})
    $extra=@((New-DRow 'cleanup_input_diagnostic' $initial));$sent=$Mode -eq 'PASS'
    if($sent){
        $proof=New-DPoint @(100,100,740,540) @(250,540,351,710) @(300,551) $true;foreach($pair in @(@('phase','api_boundary'),@('flags',4),@('input_scope','cleanup'),@('acceptance_eligible',$false),@('input_isolation_ready_sequence',($prefix|Where-Object type -eq input_isolation_ready).sequence))){Set-DField $proof $pair[0] $pair[1]};$extra+=,(New-DRow 'synthetic_input_isolation' $proof)
        $boundary=Copy-IsolationFixture $initial;$boundary.phase='api_boundary';Set-DField $boundary cursor_matches_initial $true;$extra+=,(New-DRow 'cleanup_input_diagnostic' $boundary)
        $release=Copy-IsolationFixture $boundary;foreach($pair in @(@('cleanup_up_attempted',$true),@('cleanup_up_sent',$true),@('sent',1),@('error',0),@('flags',4),@('input_tag',0x50424D41),@('receiver_watermark',$serial),@('injection_start_qpc',($clock+301)),@('injection_return_qpc',($clock+302)))){Set-DField $release $pair[0] $pair[1]};$extra+=,(New-DRow 'cleanup_release' $release)
        $raw=Copy-IsolationFixture ($prefix|Where-Object type -eq raw_input|Select-Object -Last 1);$raw.type='raw_input';$raw.receiver_sequence=++$serial;$raw.receiver_qpc=$clock+499;$raw.raw_flags=0;$raw.dx=0;$raw.dy=0;$raw.button_flags=2;$raw.cursor_sampled=$false;$raw.cursor_success=$false;$raw.cursor=$null;$raw.left_down=$true;$raw.raw_scope='cleanup';$raw.cleanup_observation=$true;$raw.acceptance_eligible=$false;$extra+=,$raw
        $extra+=,(New-DRow 'cleanup_raw_up' ([pscustomobject]@{raw_up_observed=$true;wait_result=0;timeout_ms=2000;receiver_sequence=$serial;receiver_qpc=$clock+499;receiver_watermark=$serial-1;injection_start_qpc=$clock+301;acceptance_eligible=$false}))
    }
    $final=New-DCleanupState ($Mode -eq 'SKIPPED') ($Mode -eq 'SKIPPED')
    $p=New-DPoint @(100,100,740,540) @(250,540,351,710) @(300,551) $true;Set-DField $p phase 'final';Set-DField $p input_scope 'cleanup';Set-DField $p acceptance_eligible $false;$extra+=,(New-DRow 'cleanup_final_isolation' $p)
    if($Mode -eq 'SKIPPED'){$extra+=,(New-DRow 'cleanup_skipped_no_authority' ([pscustomobject]@{reason='cleanup_initial_authority_failed';acceptance_eligible=$false}))}
    foreach($pair in @(@('cleanup_input_release',$(if($Mode -eq 'SKIPPED'){'SKIPPED_NO_AUTHORITY'}else{$Mode})),@('cleanup_up_attempted',$sent),@('cleanup_up_sent',$sent),@('raw_up_observed',$sent),@('left_button_high_bit',$final.left_down),@('final_capture_hwnd',0),@('final_cursor',@($final.cursor)))){Set-DField $final $pair[0] $pair[1]};$extra+=,(New-DRow 'cleanup_final' $final)
    foreach($r in $extra){$clock+=100;$r.sequence=$prefix.Count+1;$r.qpc=$clock;$prefix+=,$r}
    foreach($old in $Rows|Where-Object {$_.type -in @('input_shield_destroyed','winevent_hook_removed','receiver_shutdown','shutdown')}){$r=Copy-IsolationFixture $old;$clock+=100;$r.sequence=$prefix.Count+1;$r.qpc=$clock;if($r.type -eq 'input_shield_destroyed'){$r.destroy_start_qpc=$clock-90;$r.destroy_return_qpc=$clock-80};if($r.type -eq 'receiver_shutdown'){$r.receiver_sequence=$serial+1;$r.receiver_qpc=$clock-1};$prefix+=,$r}
    return ,$prefix
}
$blockedWire=Stop-DWire $resizeWire ($resizeWire|Where-Object type -eq input_isolation_ready).sequence 'missing_handoff_for_fixture'
foreach($mode in @('PASS','SKIPPED','NOT_NEEDED')){
    $wire=Add-DCleanup $blockedWire $mode;$r=Test-InputIsolationOwnedRecords $wire -AllowSynthetic
    Check-Isolation ($r.Result -ceq 'BLOCKED' -and $r.Takeover -ceq 'NOT_RUN' -and $r.CleanupInputRelease -ceq $(if($mode -eq 'SKIPPED'){'SKIPPED_NO_AUTHORITY'}else{$mode}) -and -not $r.CleanupAcceptanceEligible -and $r.NativeWrites -eq 0) "actual cleanup $mode never supplies accepted Raw UP or source geometry"
}
$wire=Add-DCleanup $blockedWire PASS;($wire|Where-Object type -eq cleanup_release).fixture_scope_active=$false;Reject-DWire $wire 'retired fixture cannot inject cleanup UP even with old point/root proof'
$wire=Add-DCleanup $blockedWire PASS;($wire|Where-Object type -eq cleanup_raw_up).receiver_sequence=999;Reject-DWire $wire 'cleanup UP requires actual independent receiver packet, not SendInput success'
$wire=Add-DCleanup $blockedWire PASS;($wire|Where-Object type -eq cleanup_final).left_down=$true;Reject-DWire $wire 'cleanup final physical state cannot be fabricated from receipt'

# Fully legal early desktop block: no receiver/shield/END/input/source writer.
$script:dg=0;$start=Copy-IsolationFixture $resizeWire[0];$start.sequence=1;$start.gesture=0;$start.qpc=10
$boot=Copy-IsolationFixture ($resizeWire|Where-Object type -eq foreground_bootstrap);$boot.sequence=2;$boot.gesture=0;$boot.qpc=11;$boot.target=0;$boot.set_foreground_attempted=$false;$boot.set_foreground_success=$false;$boot.foreground=999;$boot.final_foreground_matches=$false;$boot.result='BLOCKED';$boot.reason='BLOCKED_BY_INTERACTIVE_DESKTOP'
$b=New-DRow 'blocked' ([pscustomobject]@{reason='BLOCKED_BY_INTERACTIVE_DESKTOP'});$b.sequence=3;$b.qpc=12
$end=Copy-IsolationFixture $resizeWire[-1];$end.sequence=4;$end.gesture=0;$end.qpc=13;$end.result='BLOCKED';$end.receiver_stopped=$false;$end.input_shield_created=$false;$end.source_acceptance_sequence=0;$end.takeover_geometry_writes=0
$early=@($start,$boot,$b,$end);$r=Test-InputIsolationOwnedRecords $early -AllowSynthetic
Check-Isolation ($r.Result -ceq 'BLOCKED' -and $r.ProductHandoffAuthority -ceq 'UNKNOWN' -and $r.TestInputIsolation -ceq 'NOT_RUN' -and $r.Takeover -ceq 'NOT_RUN' -and $r.NativeWrites -eq 0 -and $r.RawPackets -eq 0) 'legal v4 blocked prefix is not product failure or empirical success'
foreach($case in @('missing-dynamic-position','stale-shield-snapshot','future-shield-snapshot')){
    $bad=Copy-IsolationFixture $resizeWire
    switch($case){
        'missing-dynamic-position'{($bad|Where-Object {$_.type -eq 'input_shield_position' -and $_.source_operation_id -eq 19}).source_operation_id=18}
        'stale-shield-snapshot'{$p=$bad|Where-Object {$_.type -eq 'writer_input_isolation' -and $_.operation_id -eq 2};$p.shield_positioning[1]=540}
        'future-shield-snapshot'{$p=$bad|Where-Object {$_.type -eq 'input_isolation_point' -and $_.index -eq 2};$p.sequence=($bad|Where-Object type -eq input_shield_created).sequence-1}
    }
    Reject-DWire $bad "latest shield lifecycle rejects $case"
}
Write-Host "input-isolation synthetic_only=true checks=$checks PASS"
