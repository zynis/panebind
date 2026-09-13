[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4a-evidence-validation.ps1')
function Copy-C4AFixture($Value){return ($Value|ConvertTo-Json -Depth 20 -Compress|ConvertFrom-Json)}
function New-C4AFixture {
    $r=[Collections.Generic.List[object]]::new()
    function Add-Record([string]$type,[hashtable]$fields) {
        $record=[ordered]@{schema='r1c4a/v1';sequence=$r.Count+1;type=$type}
        foreach($key in $fields.Keys){$record[$key]=$fields[$key]}
        $r.Add([pscustomobject]$record)
    }
    Add-Record startup @{pid=1;qpc_frequency=1000;member_count=3;interactive_console=$true}
    for($i=0;$i -lt 3;++$i){
        Add-Record target_prompt @{member=$i;nonce_directory="C:\nonce-$i";baseline_generation=1;prompt_generation=2}
        Add-Record target_confirmed @{member=$i;target_confirmation_generation=3;eligibility_generation=4;token_generation=5;baseline_exclusion_complete=$true;unique_new_target=$true;exact_location=$true;token_issued=$true;input_source='interactive_console'}
    }
    Add-Record group_consent @{confirmed=$true;input_source='interactive_console'}
    for($i=0;$i -lt 3;++$i){Add-Record binding @{member=$i;window_id=$i+1;capability_generation=$i+11;consent_generation=$i+21;hwnd=$i+100;pid=90;tid=$i+200;group=55}}
    $original=@()
    $rects=@(@(0,0,100,100),@(100,0,200,100),@(0,100,100,200))
    for($i=0;$i -lt 3;++$i){$original+= [pscustomobject]@{member=$i;visible=$rects[$i];positioning=$rects[$i];pid=90;tid=$i+200;dpi=96;monitor='DISPLAY1';work_area=@(0,0,1000,1000);image='C:\Windows\explorer.exe';class='CabinetWClass';current_desktop=$true;exact_location=$true;minimized=$false;maximized=$false}}
    Add-Record readiness @{ready=$true;original=$original;width_deficit=0;height_deficit=0}
    $gestures=@();$batches=@();$current=Copy-C4AFixture $original
    for($i=0;$i -lt 3;++$i){
        $base=1000*($i+1);$start=3*$i+1
        Add-Record receipt @{member=$i;window_id=$i+1;capability_generation=$i+11;native_source=$i+100;event='START';receipt_sequence=$start;native_thread=$i+200;native_time=$base;callback_qpc=$base;ctrl_available=$true;ctrl=$true}
        Add-Record receipt @{member=$i;window_id=$i+1;capability_generation=$i+11;native_source=$i+100;event='LOCATION';receipt_sequence=$start+1;native_thread=$i+200;native_time=$base+20;callback_qpc=$base+20;ctrl_available=$false;ctrl=$false}
        Add-Record receipt @{member=$i;window_id=$i+1;capability_generation=$i+11;native_source=$i+100;event='END';receipt_sequence=$start+2;native_thread=$i+200;native_time=$base+100;callback_qpc=$base+100;ctrl_available=$false;ctrl=$false}
        Add-Record quantum @{id=$i+1;gesture=$i+1;first_receipt=$start;last_receipt=$start+2;receipt_count=3;coalesced=0;owner_qpc=$base+21;end_qpc=$base+50}
        $before=Copy-C4AFixture $current
        foreach($m in $current){$m.visible=@(($m.visible[0]+10),($m.visible[1]+20),($m.visible[2]+10),($m.visible[3]+20));$m.positioning=$m.visible}
        $targets=@();for($m=0;$m -lt 3;++$m){if($m -ne $i){$targets+=[pscustomobject]@{member=$m;target=$current[$m].visible;actual=$current[$m].visible;positioning_target=$current[$m].positioning;actual_positioning=$current[$m].positioning;exact=$true}}}
        $batches+=@{group=55;gesture=$i+1;batch=1;phase='active';source_member=$i;source_receipt=$start+1;watermark=$start+1;pre_native_tick=$base+25;receipt_qpc=$base+20;owner_qpc=$base+21;native_start_qpc=$base+30;native_return_qpc=$base+35;postverify_qpc=$base+40;native_path='HDWP';native_flags=21;all_preflight=$true;all_pending_registered=$true;native_attempted=$true;native_succeeded=$true;deferred_count=2;stage=4;error=0;all_postverify=$true;reason='exact';source_visible=$current[$i].visible;before=$before;members=$targets}
        $gestures+=@{group=55;gesture=$i+1;source_member=$i;start_receipt=$start;end_receipt=$start+2;starts=1;locations=1;ends=1;batches=1;callback_ctrl=$true;owner_ctrl=$true;callback_qpc=$base;decision_qpc=$base+1;initial=$before;final=(Copy-C4AFixture $current);exact=$true;roles_cleared=$true;pending_empty=$true;group_ready=$true}
    }
    foreach($phase in @('setup','restore')){
        $targets=@();for($m=0;$m -lt 3;++$m){$targets+=[pscustomobject]@{member=$m;target=$original[$m].visible;actual=$original[$m].visible;positioning_target=$original[$m].positioning;actual_positioning=$original[$m].positioning;exact=$true}}
        Add-Record batch @{group=55;gesture=0;batch=0;phase=$phase;native_path='HDWP';native_flags=21;all_preflight=$true;all_pending_registered=$true;native_attempted=$true;native_succeeded=$true;deferred_count=3;error=0;all_postverify=$true;members=$targets}
    }
    foreach($batch in $batches){Add-Record batch $batch}
    foreach($g in $gestures){Add-Record gesture $g}
    Add-Record summary @{accepted=9;ignored=0;overflow=0;post_failure=0;max_depth=3;poisoned=$false;running=$false;vdm_queries=50;reconciled_missing=6;restore_exact=$true;reason='none'}
    Add-Record subjective @{grade='B';rigid_body_feel='MOSTLY';input_source='interactive_console'}
    Add-Record shutdown @{result='PASS';user_windows_closed=$false}
    return $r.ToArray()
}
$tests=0
function Add-C4AFeedbackFixture {
    param([object[]]$Records,[switch]$Reverse)
    foreach($r in $Records){
        if($r.type -eq 'receipt' -and $r.receipt_sequence -ge 3){$r.receipt_sequence+=2}
        if($r.type -eq 'gesture'){
            if($r.start_receipt -ge 3){$r.start_receipt+=2};$r.end_receipt+=2
        }
        if($r.type -eq 'batch' -and $r.phase -eq 'active'){
            if($r.source_receipt -ge 3){$r.source_receipt+=2};if($r.watermark -ge 3){$r.watermark+=2}
        }
        if($r.type -eq 'quantum'){
            if($r.first_receipt -ge 3){$r.first_receipt+=2};$r.last_receipt+=2
            if($r.gesture -eq 1){$r.receipt_count+=2}
        }
    }
    $extras=@()
    $order=if($Reverse){@(2,1)}else{@(1,2)}
    $firstBatch=@($Records|Where-Object {$_.type -eq 'batch' -and $_.phase -eq 'active' -and $_.gesture -eq 1})[0]
    for($n=0;$n -lt 2;++$n){
        $m=$order[$n];$seq=$n+3
        $extras+=[pscustomobject]@{schema='r1c4a/v1';sequence=0;type='receipt';member=$m;window_id=$m+1;capability_generation=$m+11;native_source=$m+100;event='LOCATION';receipt_sequence=$seq;native_thread=$m+200;native_time=1046+$n;callback_qpc=1046+$n;ctrl_available=$false;ctrl=$false}
        $target=@($firstBatch.members|Where-Object member -eq $m)[0]
        $extras+=[pscustomobject]@{schema='r1c4a/v1';sequence=0;type='feedback';member=$m;gesture=1;batch=1;receipt_sequence=$seq;result='acknowledged';observed_visible=$target.target}
    }
    ($Records|Where-Object type -eq summary).accepted=11
    ($Records|Where-Object type -eq summary).reconciled_missing=4
    $all=@($Records[0..($Records.Count-2)])+$extras+@($Records[-1])
    for($n=0;$n -lt $all.Count;++$n){$all[$n].sequence=$n+1}
    return $all
}
function Check-Fixture([string]$Name,[scriptblock]$Mutate,[bool]$Expected=$false){
    $records=New-C4AFixture
    & $Mutate $records
    $pass=$true
    try{$result=Test-C4ARecords $records}catch{$pass=$false;if($Expected){Write-Host $_}}
    if($pass -ne $Expected){throw "C4A fixture mismatch: $Name (accepted=$pass)"}
    ++$script:tests
    Write-Host "PASS $Name"
}
Check-Fixture valid {} $true
Check-Fixture sequence {param($r)$r[2].sequence=900}
Check-Fixture schema {param($r)$r[2].schema='wrong'}
Check-Fixture shutdown {param($r)$r[-1].result='BLOCKED'}
Check-Fixture preexisting {param($r)($r|Where-Object type -eq target_confirmed|Select-Object -First 1).baseline_exclusion_complete=$false}
Check-Fixture nonce-reuse {param($r)$p=@($r|Where-Object type -eq target_prompt);$p[1].nonce_directory=$p[0].nonce_directory}
Check-Fixture consent {param($r)($r|Where-Object type -eq group_consent).confirmed=$false}
Check-Fixture hwnd-reuse {param($r)$b=@($r|Where-Object type -eq binding);$b[1].hwnd=$b[0].hwnd}
Check-Fixture member-count {param($r)$r[0].member_count=2}
Check-Fixture source-capability {param($r)($r|Where-Object type -eq receipt|Select-Object -First 1).capability_generation=900}
Check-Fixture permanent-leader {param($r)($r|Where-Object {$_.type -eq 'gesture' -and $_.gesture -eq 2}).source_member=0}
Check-Fixture group-generation {param($r)($r|Where-Object {$_.type -eq 'gesture' -and $_.gesture -eq 2}).group=900}
Check-Fixture gesture-generation {param($r)($r|Where-Object {$_.type -eq 'gesture' -and $_.gesture -eq 2}).gesture=1}
Check-Fixture ctrl {param($r)($r|Where-Object type -eq gesture|Select-Object -First 1).callback_ctrl=$false}
Check-Fixture roles-retained {param($r)($r|Where-Object type -eq gesture|Select-Object -First 1).roles_cleared=$false}
Check-Fixture pending-retained {param($r)($r|Where-Object type -eq gesture|Select-Object -First 1).pending_empty=$false}
Check-Fixture drift {param($r)($r|Where-Object type -eq gesture|Select-Object -First 1).final[1].visible[0]+=1}
Check-Fixture pair-fallback {param($r)$op=$r|Where-Object {$_.type -eq 'batch' -and $_.phase -eq 'active'}|Select-Object -First 1;$op.members=@($op.members[0])}
Check-Fixture native-serial {param($r)($r|Where-Object type -eq batch|Select-Object -First 1).native_path='SetWindowPos'}
Check-Fixture preflight {param($r)($r|Where-Object type -eq batch|Select-Object -First 1).all_preflight=$false}
Check-Fixture pending-registration {param($r)($r|Where-Object type -eq batch|Select-Object -First 1).all_pending_registered=$false}
Check-Fixture native-failure {param($r)($r|Where-Object type -eq batch|Select-Object -First 1).native_succeeded=$false}
Check-Fixture postverify {param($r)($r|Where-Object type -eq batch|Select-Object -First 1).members[1].actual=@(1,2,101,102)}
Check-Fixture zorder {param($r)($r|Where-Object type -eq batch|Select-Object -First 1).native_flags=17}
Check-Fixture queue-overflow {param($r)($r|Where-Object type -eq summary).overflow=1}
Check-Fixture post-failure {param($r)($r|Where-Object type -eq summary).post_failure=1}
Check-Fixture missing-fabricated {param($r)($r|Where-Object type -eq summary).reconciled_missing=0}
Check-Fixture wrong-executable {param($r)($r|Where-Object type -eq gesture|Select-Object -First 1).initial[1].image='C:\EXCEL.EXE'}
Check-Fixture mixed-dpi {param($r)($r|Where-Object type -eq gesture|Select-Object -First 1).initial[1].dpi=192}
Check-Fixture not-current-desktop {param($r)($r|Where-Object type -eq gesture|Select-Object -First 1).initial[1].current_desktop=$false}
Check-Fixture no-subjective {param($r)($r|Where-Object type -eq subjective).rigid_body_feel='unknown'}
foreach($reverse in @($false,$true)) {
    $records=Add-C4AFeedbackFixture (New-C4AFixture) -Reverse:$reverse
    $null=Test-C4ARecords $records
    ++$tests;Write-Host "PASS member-feedback-order reverse=$reverse"
}
foreach($fault in @('wrong-geometry','wrong-batch','stale-gesture','stale-native-time','duplicate-ACK')){
    $records=Add-C4AFeedbackFixture (New-C4AFixture)
    $feedback=@($records|Where-Object type -eq feedback)
    switch($fault){
        'wrong-geometry' {$feedback[0].observed_visible=$feedback[1].observed_visible}
        'wrong-batch' {$feedback[0].batch=99}
        'stale-gesture' {$feedback[0].gesture=2}
        'stale-native-time' {($records|Where-Object {$_.type -eq 'receipt' -and $_.receipt_sequence -eq 3}).native_time=1025}
        'duplicate-ACK' {$feedback[1].member=$feedback[0].member;$feedback[1].receipt_sequence=$feedback[0].receipt_sequence}
    }
    $accepted=$true;try{$null=Test-C4ARecords $records}catch{$accepted=$false}
    if($accepted){throw "invalid feedback accepted: $fault"}
    ++$tests;Write-Host "PASS $fault"
}
$tempBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$fixtureRoot=Join-Path $tempBase ('panebind-c4a-jsonl-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
try {
    $path=Join-Path $fixtureRoot 'fixture.jsonl'
    $lines=@(New-C4AFixture|ForEach-Object {$_|ConvertTo-Json -Depth 20 -Compress})
    $lines|Set-Content -LiteralPath $path -Encoding UTF8
    $null=Test-C4AEvidence $path;++$tests;Write-Host 'PASS JSONL file replay'
    foreach($invalid in @('', '{bad json')) {
        @($lines)+@($invalid)|Set-Content -LiteralPath $path -Encoding UTF8
        $accepted=$true;try{$null=Test-C4AEvidence $path}catch{$accepted=$false}
        if($accepted){throw 'invalid JSONL accepted'}
        ++$tests;Write-Host 'PASS invalid JSONL rejected'
    }
} finally {
    $resolved=[IO.Path]::GetFullPath($fixtureRoot)
    if(-not $resolved.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($resolved) -notlike 'panebind-c4a-jsonl-*'){throw 'unsafe fixture cleanup target'}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
Write-Host "R1C4A_RUNNER_FIXTURES = PASS ($tests)"
