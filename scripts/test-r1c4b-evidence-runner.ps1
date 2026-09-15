[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-evidence-validation.ps1')
function Copy-B($v){return ($v|ConvertTo-Json -Depth 30 -Compress|ConvertFrom-Json)}
function New-BFixture {
    $rows=[Collections.Generic.List[object]]::new();$script:bg=0;$script:br=0;$script:bq=0
    function Add-B($type,[hashtable]$data){$r=[ordered]@{schema='r1c4b/v1';sequence=$rows.Count+1;type=$type};foreach($k in $data.Keys){$r[$k]=$data[$k]};$rows.Add([pscustomobject]$r)}
    function Wait-B($kind,[bool]$live=$false){Add-B console_wait @{kind=$kind;result='complete';owner=80;waits=1;pumps=1;messages=0;events=2;mode_before=503;mode_after=503;modes_observed=$true;mode_changed=$false;error=0;live_pump=$live}}
    function Receipt-B([int]$member,$event,[int]$time,[bool]$ctrl=$false){++$script:br;Add-B receipt @{receipt=$script:br;member=$member;window_id=$member+1;capability=$member+11;hwnd=$member+100;tid=$member+200;native_time=$time;callback_qpc=$time;event=$event;ctrl_available=($event -eq 'START');ctrl=$ctrl};return $script:br}
    function Quantum-B($gesture,$receipt,$owner,[int]$solver=0,[int]$native=0){++$script:bq;Add-B quantum @{id=$script:bq;gesture=$gesture;first=$receipt;last=$receipt;raw_receipts=1;coalesced=0;solver_calls=$solver;corrections=$native;owner_qpc=$owner;end_qpc=$owner+40;active_inventory=0;active_manager_creates=0}}
    function Sample-B($gesture,$source,$receipt,$tick,$rect,$result,[int]$operation=0){Add-B sample @{gesture=$gesture;source=$source;receipt=$receipt;provider_tick=$tick;operation=$operation;raw=$rect;result=$result;callback_qpc=$tick;owner_qpc=$tick+1}}
    function Graph-B($s){$t=Get-C4ATopology $s;return @{relation_count=$t.RelationCount;ready=$t.Ready;pairs=$t.Pairs;components=$t.Components}}
    Add-B startup @{evidence_kind='synthetic_fixture';implementation_sha=('1'*40);owner_sta=80;qpc_frequency=1000;member_count=3;attraction=10;release=16;speed_limit=2000;screen_magnet=$false;window_magnet=$true;live_magnet=$true;glue_move=$true;glue_resize=$false;magnet_contract='source_only_exact_v1';console_contract='preserved_mode_live_owner_v1'}
    for($i=0;$i -lt 3;++$i){Add-B target_prompt @{member=$i;nonce_id="synthetic-$i";baseline=1;prompt=2};Wait-B target_confirmation;Add-B target_confirmed @{member=$i;confirmation=3;eligibility=4;token=5;baseline_excluded=$true;unique=$true;exact_location=$true;token_issued=$true}}
    Wait-B live_consent;Add-B live_consent @{confirmed=$true;source_correction=$true;glue_move=$true;accepted_restore=$true}
    for($i=0;$i -lt 3;++$i){Add-B binding @{member=$i;group=55;window_id=$i+1;capability=$i+11;consent=$i+21;hwnd=$i+100;pid=90;tid=$i+200}}
    $script:bs=@();$rects=@(@(0,0,120,180),@(180,30,280,150),@(20,210,300,330))
    for($i=0;$i -lt 3;++$i){$script:bs+=[pscustomobject]@{member=$i;visible=$rects[$i];positioning=$rects[$i];pid=90;tid=$i+200;dpi=96;monitor='DISPLAY1';work_area=@(0,0,1000,1000);image='explorer.exe';class='CabinetWClass';current_desktop=$true;exact_location=$true;minimized=$false;maximized=$false}}
    Add-B binding_snapshot @{snapshots=(Copy-B $script:bs)};Wait-B prepare $true
    function Gesture-B([int]$Source,$Raw,$Target=$null,$Edges=@(0,0,0,0),$X=$null,$Y=$null){
        ++$script:bg;$gen=$script:bg;$base=$gen*1000;$initial=Copy-B $script:bs
        $start=Receipt-B $Source START $base;Quantum-B $gen $start ($base+1)
        $loc=Receipt-B $Source LOCATION ($base+20)
        $before=Copy-B $script:bs;$before[$Source].visible=$Raw;$before[$Source].positioning=$Raw
        $script:bs=Copy-B $before;$native=$null -ne $Target;$resize=($Edges -join ',') -ne '0,0,0,0'
        $ack=0;$duplicate=0
        if($native){
            $script:bs[$Source].visible=$Target;$script:bs[$Source].positioning=$Target
            $dx=if($resize){0}else{$Target[0]-$Raw[0]};$dy=if($resize){0}else{$Target[1]-$Raw[1]}
            $sx=$null;$sy=$null
            foreach($axis in @('X','Y')){$data=if($axis -eq 'X'){$X}else{$Y};if($null -eq $data){continue}
                $targetRect=$initial[$data[2]].visible
                $overlap=if($data[0] -in 0,2){[Math]::Max(0,([Math]::Min($Target[3],$targetRect[3])-[Math]::Max($Target[1],$targetRect[1])))}else{[Math]::Max(0,([Math]::Min($Target[2],$targetRect[2])-[Math]::Max($Target[0],$targetRect[0])))}
                $selected=@{target_id=[string]($data[2]+1);kind=$data[3];moving_edge=$data[0];target_edge=$data[1];delta=($Target[$data[0]]-$Raw[$data[0]]);overlap=$overlap}
                if($axis -eq 'X'){$sx=$selected}else{$sy=$selected}
            }
            $xChanged=$Raw[0] -ne $Target[0] -or $Raw[2] -ne $Target[2];$yChanged=$Raw[1] -ne $Target[1] -or $Raw[3] -ne $Target[3]
            Add-B correction @{group=55;gesture=$gen;operation=1;source=$Source;source_id=[string]($Source+1);capability=$Source+11;consent=$Source+21;source_receipt=$loc;watermark=$loc;registration_tick=$base+25;raw=$Raw;corrected=$Target;target_positioning=$Target;dx=$dx;dy=$dy;edges=$Edges;selected_x=$sx;selected_y=$sy;axes=$(if($xChanged -and $yChanged){'XY'}elseif($xChanged){'X'}else{'Y'});preflight=$true;pending_registered=$true;native_attempted=$true;native_success=$true;native_calls=1;flags=$(if($resize){20}else{21});error=0;exact=$true;actual=(Copy-B $script:bs);before=$before;receipt_qpc=$base+20;owner_qpc=$base+21;native_start_qpc=$base+30;native_return_qpc=$base+35;postverify_qpc=$base+40;reason='exact'}
        }
        Sample-B $gen $Source $loc ($base+20) $Raw $(if($native){'proposal'}else{'no_candidate'})
        Quantum-B $gen $loc ($base+21) 1 ([int]$native)
        if($native -and $gen -eq 1){
            $ackReceipt=Receipt-B $Source LOCATION ($base+60);Sample-B $gen $Source $ackReceipt ($base+60) $Target acknowledged 1;Quantum-B $gen $ackReceipt ($base+61);$ack=1
            $dupReceipt=Receipt-B $Source LOCATION ($base+65);Sample-B $gen $Source $dupReceipt ($base+65) $Target duplicate 1;Quantum-B $gen $dupReceipt ($base+66);$duplicate=1
        }
        $end=Receipt-B $Source END ($base+100);Sample-B $gen $Source $end ($base+100) $script:bs[$Source].visible $(if($native){'end_exact_reconciliation'}else{'unchanged'});Quantum-B $gen $end ($base+101)
        Add-B gesture @{gesture=$gen;source=$Source;start=$start;end=$end;ctrl=$false;route=$(if($resize){'MAGNET_RESIZE'}else{'MAGNET_MOVE'});edges=$Edges;completed=$true;initial=$initial;final=(Copy-B $script:bs);raw_receipts=($end-$start+1);meaningful=1;solver_calls=1;motion_suppressed=0;proposals=[int]$native;corrections=[int]$native;acknowledged=$ack;duplicates=$duplicate;ambiguous=0;missing=([int]$native-$ack);x_latches=[int]($null -ne $X);y_latches=[int]($null -ne $Y);latch_releases=([int]($null -ne $X)+[int]($null -ne $Y));reason='none'}
        Add-B relation_graph @{gesture=$gen;snapshots=(Copy-B $script:bs);graph=(Graph-B $script:bs)}
    }
    function Action-B([int]$id,[int]$first){Wait-B magnet_action $true;Add-B action @{action=$id;attempt=1;first_gesture=$first;last_gesture=$script:bg;passed=$true;snapshots=(Copy-B $script:bs);graph=(Graph-B $script:bs)}}
    Gesture-B 2 @(20,187,300,307) @(20,180,300,300) -Y @(1,3,0,1);Action-B 0 1
    Gesture-B 1 @(126,30,226,150) @(120,30,220,150) -X @(0,2,0,1);Action-B 1 2
    Gesture-B 1 @(200,20,300,140)
    Gesture-B 1 @(126,56,226,176) @(120,60,220,180) -X @(0,2,0,1) -Y @(3,1,2,1);Action-B 2 3
    Gesture-B 1 @(120,60,220,140) -Edges @(0,0,0,1);Action-B 3 5
    Gesture-B 1 @(120,60,220,174) @(120,60,220,180) -Edges @(0,0,0,1) -Y @(3,1,2,1);Action-B 4 6
    Gesture-B 1 @(120,80,220,180) -Edges @(0,1,0,0)
    Gesture-B 1 @(120,6,220,180) @(120,0,220,180) -Edges @(0,1,0,0) -Y @(1,1,0,3);Action-B 5 7
    Gesture-B 2 @(20,180,120,300) -Edges @(0,0,1,0);Action-B 6 9
    Action-B 7 10
    Wait-B topology_acceptance $true;$accepted=Copy-B $script:bs
    Add-B accepted @{confirmed=$true;snapshots=$accepted;graph=(Graph-B $accepted);capture_qpc=100000}
    for($source=0;$source -lt 3;++$source){++$script:bg;$gen=$script:bg;$base=110000+$source*1000;$initial=Copy-B $script:bs
        $start=Receipt-B $source START $base $true;Quantum-B $gen $start ($base+1)
        $loc=Receipt-B $source LOCATION ($base+20)
        foreach($s in $script:bs){$s.visible=@(($s.visible[0]+10),($s.visible[1]+20),($s.visible[2]+10),($s.visible[3]+20));$s.positioning=$s.visible}
        Sample-B $gen $source $loc ($base+20) $script:bs[$source].visible glue_route;Quantum-B $gen $loc ($base+21)
        $end=Receipt-B $source END ($base+100);Sample-B $gen $source $end ($base+100) $script:bs[$source].visible unchanged;Quantum-B $gen $end ($base+101)
        Add-B gesture @{gesture=$gen;source=$source;start=$start;end=$end;ctrl=$true;route='EXISTING_C4A_GLUE';edges=@(0,0,0,0);completed=$true;initial=$initial;final=(Copy-B $script:bs);raw_receipts=3;meaningful=1;solver_calls=0;motion_suppressed=0;proposals=0;corrections=0;acknowledged=0;duplicates=0;ambiguous=0;missing=0;x_latches=0;y_latches=0;latch_releases=0;reason='none'}
        Add-B relation_graph @{gesture=$gen;snapshots=(Copy-B $script:bs);graph=(Graph-B $script:bs)}
        $targets=@();for($m=0;$m -lt 3;++$m){if($m -ne $source){$targets+=@{member=$m;target=$script:bs[$m].visible;actual=$script:bs[$m].visible;positioning_target=$script:bs[$m].positioning;actual_positioning=$script:bs[$m].positioning}}}
        Add-B glue_batch @{gesture=$source+1;batch=1;source=$source;source_receipt=$loc;watermark=$loc;source_visible=$script:bs[$source].visible;pre_native_tick=$base+25;preflight=$true;pending_registered=$true;native_attempted=$true;native_success=$true;exact=$true;flags=21;deferred=2;error=0;native_start_qpc=$base+30;native_return_qpc=$base+35;postverify_qpc=$base+40;members=$targets}
        Add-B glue_gesture @{gesture=$source+1;source=$source;start=$start;end=$end;batches=1;exact=$true;roles_cleared=$true;pending_empty=$true;group_ready=$true;initial=$initial;final=(Copy-B $script:bs)}
        Wait-B glue_action $true;Add-B glue_action @{source=$source;passed=$true}
    }
    $targets=@();for($m=0;$m -lt 3;++$m){$targets+=@{member=$m;target=$accepted[$m].visible;actual=$accepted[$m].visible;positioning_target=$accepted[$m].positioning;actual_positioning=$accepted[$m].positioning}}
    Add-B glue_batch @{gesture=0;batch=0;source=3;source_receipt=0;watermark=$script:br;source_visible=@(0,0,0,0);pre_native_tick=120000;preflight=$true;pending_registered=$true;native_attempted=$true;native_success=$true;exact=$true;flags=21;deferred=3;error=0;native_start_qpc=120001;native_return_qpc=120005;postverify_qpc=120010;members=$targets}
    Add-B summary @{restore_exact=$true;final=$accepted;raw_receipts=$script:br;overflow=0;post_failure=0;max_queue=1;active_inventory=0;active_manager_creates=0;vdm_queries=100;glue_missing=6}
    foreach($q in @('magnet_feel','too_sticky','misses','visible_jitter','rigid_body_feel')){Wait-B $q;Add-B subjective @{question=$q;answer=$(if($q -eq 'magnet_feel'){'B'}elseif($q -eq 'rigid_body_feel'){'MOSTLY'}else{'NO'})}}
    Add-B shutdown @{result='PASS';user_windows_closed=$false}
    return Copy-B $rows.ToArray()
}
$tests=0
$base=New-BFixture
try{$valid=Test-C4BRecords $base -AllowSynthetic}catch{Write-Host $_.ScriptStackTrace;throw}
if($valid.Result -cne 'PASS' -or $valid.HumanSeal -cne 'NOT_HUMAN_EVIDENCE'){throw 'positive synthetic fixture failed'}
++ $tests
foreach($fault in @('schema','scope','source','capability','pending','two-native','flags','postverify','positioning','watermark','feedback-operation','feedback-END','final-mismatch','graph','action','storm','inventory','vdm','ctrl-dual-writer','restore','missing-count','subjective','mode')){
    $r=Copy-B $base;$op=@($r|Where-Object type -eq correction)[0];$g=@($r|Where-Object type -eq gesture)[0]
    switch($fault){
        'schema' {$r[0].schema='r1c4a/v1'}
        'scope' {$r[0].screen_magnet=$true}
        'source' {$op.source=0}
        'capability' {$op.capability++}
        'pending' {$op.pending_registered=$false}
        'two-native' {$op.native_calls=2}
        'flags' {$op.flags=0}
        'postverify' {$op.exact=$false}
        'positioning' {$op.actual[$op.source].positioning[0]++}
        'watermark' {$op.watermark=0}
        'feedback-operation' {($r|Where-Object {$_.type -eq 'sample' -and $_.result -eq 'acknowledged'}).operation=99}
        'feedback-END' {$f=$r|Where-Object {$_.type -eq 'sample' -and $_.result -eq 'acknowledged'};($r|Where-Object {$_.type -eq 'receipt' -and $_.receipt -eq $f.receipt}).event='END'}
        'final-mismatch' {$g.final[$g.source].visible[0]++}
        'graph' {($r|Where-Object type -eq relation_graph|Select-Object -First 1).graph.relation_count++}
        'action' {($r|Where-Object {$_.type -eq 'action' -and $_.action -eq 2}).first_gesture=5}
        'storm' {($r|Where-Object type -eq quantum|Select-Object -First 1).corrections=2}
        'inventory' {($r|Where-Object type -eq quantum|Select-Object -First 1).active_inventory=1}
        'vdm' {($r|Where-Object type -eq summary).active_manager_creates=1}
        'ctrl-dual-writer' {$g.ctrl=$true}
        'restore' {($r|Where-Object {$_.type -eq 'glue_batch' -and $_.gesture -eq 0}).members[0].actual[0]++}
        'missing-count' {($r|Where-Object {$_.type -eq 'gesture' -and $_.gesture -eq 2}).missing=0}
        'subjective' {($r|Where-Object {$_.type -eq 'subjective' -and $_.question -eq 'magnet_feel'}).answer='invented'}
        'mode' {($r|Where-Object type -eq console_wait|Select-Object -First 1).mode_changed=$true}
    }
    $pass=$true;try{$null=Test-C4BRecords $r -AllowSynthetic}catch{$pass=$false}
    if($pass){throw "invalid fixture accepted: $fault"};++$tests
}
$pass=$true;try{$null=Test-C4BRecords $base}catch{$pass=$false}
if($pass){throw 'synthetic evidence accepted as human'};++$tests
$blocked=@($base|Where-Object sequence -le 5)
$blocked+=[pscustomobject]@{schema='r1c4b/v1';sequence=6;type='shutdown';result='BLOCKED';reason='target_declined'}
$result=Test-C4BRecords $blocked -AllowSynthetic
if($result.Result -cne 'BLOCKED' -or $result.Reason -cne 'target_declined'){throw 'early block was masked'};++$tests
$root=Join-Path ([IO.Path]::GetTempPath()) ('panebind-c4b-runner-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root|Out-Null
try {
    $path=Join-Path $root 'synthetic.jsonl'
    $base|ForEach-Object {$_|ConvertTo-Json -Depth 30 -Compress}|Set-Content -LiteralPath $path -Encoding UTF8
    $null=Test-C4BEvidence $path -AllowSynthetic;++$tests
    Add-Content -LiteralPath $path -Value '{broken' -Encoding UTF8
    $pass=$true;try{$null=Test-C4BEvidence $path -AllowSynthetic}catch{$pass=$false}
    if($pass){throw 'broken JSONL accepted'};++$tests
    $previousPreference=$ErrorActionPreference
    try {
        $ErrorActionPreference='Continue'
        $guardOutput=& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'run-r1c4b-live-magnet-evidence.ps1') 2>&1
        $guardExit=$LASTEXITCODE
    } finally {$ErrorActionPreference=$previousPreference}
    if($guardExit -eq 0 -or ($guardOutput -join "`n") -notmatch 'No harness launched'){throw 'review guard failed'};++$tests
} finally {
    $absolute=[IO.Path]::GetFullPath($root);$temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if(-not $absolute.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($absolute) -notmatch '^panebind-c4b-runner-[0-9a-f]{32}$'){throw 'unsafe fixture cleanup path'}
    Remove-Item -LiteralPath $absolute -Recurse -Force
}
Write-Host "R1C4B_EVIDENCE_FIXTURES = PASS ($tests; synthetic only)"
