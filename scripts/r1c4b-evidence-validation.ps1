Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4a-evidence-validation.ps1')
function Assert-C4B([bool]$Condition,[string]$Message){if(-not $Condition){throw "R1C4B evidence: $Message"}}
function Test-C4BMembers($Snapshots,$Bindings){
    Assert-C4B (@($Snapshots).Count -eq 3) 'three actual members required'
    for($i=0;$i -lt 3;++$i){$s=$Snapshots[$i];$null=Get-C4ARectKey $s.visible;$null=Get-C4ARectKey $s.positioning;$null=Get-C4ARectKey $s.work_area
        Assert-C4B ($s.member -eq $i -and $s.pid -eq $Bindings[$i].pid -and $s.tid -eq $Bindings[$i].tid -and $s.dpi -gt 0 -and $s.dpi -eq $Snapshots[0].dpi -and $s.monitor -ceq $Snapshots[0].monitor) 'member identity/monitor/DPI'
        Assert-C4B ($s.image -ieq 'explorer.exe' -and $s.class -cin @('CabinetWClass','ExploreWClass') -and $s.current_desktop -eq $true -and $s.exact_location -eq $true -and -not $s.minimized -and -not $s.maximized) 'member eligibility'
    }
}
function Test-C4BGraph($Graph,$Snapshots){
    $truth=Get-C4ATopology $Snapshots
    Assert-C4B ($Graph.relation_count -eq $truth.RelationCount -and $Graph.ready -eq $truth.Ready -and @($Graph.pairs).Count -eq 3 -and @($Graph.components).Count -eq 3) 'graph is not actual geometry truth'
    for($i=0;$i -lt 3;++$i){foreach($field in @('first','second','relation','first_edge','second_edge','signed_gap','orthogonal_overlap')){Assert-C4B ($Graph.pairs[$i].$field -eq $truth.Pairs[$i].$field) "graph pair $field"};Assert-C4B (($Graph.components[$i] -join ',') -ceq ($truth.Components[$i] -join ',')) 'graph component'}
}
function Get-C4BEdge($Rect,[int]$Edge){return [decimal]$Rect[$Edge]}
function Test-C4BRelation($Snapshots,[int]$First,[int]$Second){return @((Get-C4ATopology $Snapshots).Pairs|Where-Object {$_.first -eq $First -and $_.second -eq $Second -and $_.relation}).Count -eq 1}
function Test-C4BActionProof([int]$Id,$Row,$Gestures,$Corrections){
    $gs=@($Gestures|Where-Object {$_.gesture -ge $Row.first_gesture -and $_.gesture -le $Row.last_gesture -and $_.completed -and -not $_.ctrl})
    $ops=@($Corrections|Where-Object {$_.gesture -ge $Row.first_gesture -and $_.gesture -le $Row.last_gesture -and $_.exact})
    $s=$Row.snapshots
    $move=@($gs|Where-Object {$_.route -ceq 'MAGNET_MOVE' -and $_.source -eq $(if($Id -eq 0){2}else{1})}).Count -gt 0
    $moveOps=@($ops|Where-Object {$_.source -eq $(if($Id -eq 0){2}else{1}) -and ($_.edges -join ',') -ceq '0,0,0,0'})
    $bottom=@($gs|Where-Object {$_.source -eq 1 -and $_.route -ceq 'MAGNET_RESIZE' -and ($_.edges -join ',') -ceq '0,0,0,1'}).Count -gt 0
    $top=@($gs|Where-Object {$_.source -eq 1 -and $_.route -ceq 'MAGNET_RESIZE' -and ($_.edges -join ',') -ceq '0,1,0,0'}).Count -gt 0
    switch($Id){
        0 {return $move -and $moveOps.Count -gt 0 -and (Test-C4BRelation $s 0 2)}
        1 {return $move -and $moveOps.Count -gt 0 -and (Test-C4BRelation $s 0 1)}
        2 {return $move -and @($moveOps|Where-Object axes -eq XY).Count -gt 0 -and (Get-C4ATopology $s).RelationCount -eq 3 -and $s[1].visible[0] -eq $s[0].visible[2] -and $s[1].visible[3] -eq $s[2].visible[1]}
        3 {return $bottom -and $s[1].visible[3] -lt $s[2].visible[1] -and (Test-C4BRelation $s 0 1)}
        4 {return $bottom -and @($ops|Where-Object {$_.source -eq 1 -and ($_.edges -join ',') -ceq '0,0,0,1'}).Count -gt 0 -and (Test-C4BRelation $s 1 2)}
        5 {return $top -and @($ops|Where-Object {$_.source -eq 1 -and ($_.edges -join ',') -ceq '0,1,0,0'}).Count -gt 0 -and (Test-C4BRelation $s 1 2) -and $s[1].visible[1] -eq $s[0].visible[1] -and $s[1].visible[3] -eq $s[2].visible[1]}
        6 {return @($gs|Where-Object {$_.source -eq 2 -and ($_.edges -join ',') -ceq '0,0,1,0' -and (Test-C4BRelation $_.initial 1 2) -and -not (Test-C4BRelation $_.final 1 2)}).Count -gt 0 -and -not (Test-C4BRelation $s 1 2)}
        7 {return (Get-C4ATopology $s).Ready}
    }
    return $false
}
function Test-C4BRecords {
    param([object[]]$Records,[switch]$AllowSynthetic)
    Assert-C4B ($Records.Count -ge 2) 'empty/incomplete log'
    for($i=0;$i -lt $Records.Count;++$i){Assert-C4B ($Records[$i].schema -ceq 'r1c4b/v1' -and $Records[$i].sequence -eq $i+1) 'schema/sequence'}
    Assert-C4B ($Records[0].type -ceq 'startup' -and $Records[-1].type -ceq 'shutdown' -and @($Records|Where-Object type -eq startup).Count -eq 1 -and @($Records|Where-Object type -eq shutdown).Count -eq 1) 'startup/shutdown lifecycle'
    $startup=$Records[0];$shutdown=$Records[-1]
    Assert-C4B ($startup.evidence_kind -ceq 'human_interactive' -or ($AllowSynthetic -and $startup.evidence_kind -ceq 'synthetic_fixture')) 'synthetic/unknown data cannot be human evidence'
    Assert-C4B ($startup.implementation_sha -cmatch '^[0-9a-f]{40}$' -and $startup.owner_sta -gt 0 -and $startup.qpc_frequency -gt 0 -and $startup.member_count -eq 3) 'build/owner/QPC contract'
    Assert-C4B ($startup.attraction -eq 10 -and $startup.release -eq 16 -and $startup.speed_limit -eq 2000 -and -not $startup.screen_magnet -and $startup.window_magnet -and $startup.live_magnet -and $startup.glue_move -and -not $startup.glue_resize -and $startup.magnet_contract -ceq 'source_only_exact_v1' -and $startup.console_contract -ceq 'preserved_mode_live_owner_v1') 'live feature scope contract'
    $allowed=@('startup','shutdown','target_prompt','target_confirmed','live_consent','binding','binding_snapshot','console_wait','action','accepted','glue_action','receipt','quantum','sample','correction','gesture','relation_graph','glue_gesture','glue_batch','glue_feedback','summary','subjective','capture_diagnostic')
    Assert-C4B (@($Records|Where-Object {$_.type -cnotin $allowed}).Count -eq 0) 'unknown record type'
    $bindings=@($Records|Where-Object type -eq binding|Sort-Object member)
    $corrections=@($Records|Where-Object type -eq correction)
    foreach($op in $corrections){if($op.native_attempted){Assert-C4B ($op.pending_registered -and $op.preflight -and $op.native_calls -eq 1 -and $op.flags -in 20,21 -and $bindings.Count -eq 3) 'native authority/pending/single-call violation'}}
    if($shutdown.result -ceq 'BLOCKED') {
        Assert-C4B ('reason' -in $shutdown.PSObject.Properties.Name -and -not [string]::IsNullOrWhiteSpace($shutdown.reason)) 'blocked reason missing'
        return [pscustomobject]@{Result='BLOCKED';Reason=$shutdown.reason;Bindings=$bindings.Count;NativeCorrections=@($corrections|Where-Object native_attempted -eq $true).Count;CaptureDiagnostics=@($Records|Where-Object type -eq capture_diagnostic)}
    }
    Assert-C4B ($shutdown.result -ceq 'PASS' -and $shutdown.user_windows_closed -eq $false) 'shutdown result/closure'
    $prompts=@($Records|Where-Object type -eq target_prompt|Sort-Object member);$confirmed=@($Records|Where-Object type -eq target_confirmed|Sort-Object member);$consent=@($Records|Where-Object type -eq live_consent)
    Assert-C4B ($bindings.Count -eq 3 -and $prompts.Count -eq 3 -and $confirmed.Count -eq 3 -and $consent.Count -eq 1) 'exact consented set'
    Assert-C4B ($consent[0].confirmed -and $consent[0].source_correction -and $consent[0].glue_move -and $consent[0].accepted_restore) 'separate live consent missing'
    Assert-C4B (@($bindings.hwnd|Sort-Object -Unique).Count -eq 3 -and @($bindings.window_id|Sort-Object -Unique).Count -eq 3 -and @($prompts.nonce_id|Sort-Object -Unique).Count -eq 3) 'reused frame/id/nonce'
    $group=$bindings[0].group
    for($i=0;$i -lt 3;++$i){$p=$prompts[$i];$c=$confirmed[$i];$b=$bindings[$i]
        Assert-C4B ($b.member -eq $i -and $p.member -eq $i -and $c.member -eq $i -and $b.group -eq $group -and $group -gt 0 -and $b.hwnd -gt 0 -and $b.window_id -gt 0 -and $b.capability -gt 0 -and $b.consent -gt 0) 'binding identity'
        Assert-C4B ($p.baseline -gt 0 -and $p.prompt -gt $p.baseline -and $c.confirmation -gt $p.prompt -and $c.eligibility -gt $c.confirmation -and $c.token -gt $c.eligibility -and $c.baseline_excluded -and $c.unique -and $c.exact_location -and $c.token_issued) 'target consent proof'
        Assert-C4B ($p.sequence -lt $c.sequence -and $c.sequence -lt $consent[0].sequence -and $consent[0].sequence -lt $b.sequence) 'consent chronology'
    }
    $accepted=@($Records|Where-Object type -eq accepted);Assert-C4B ($accepted.Count -eq 1 -and $accepted[0].confirmed -and $accepted[0].capture_qpc -gt 0) 'accepted baseline'
    Test-C4BMembers $accepted[0].snapshots $bindings;Test-C4BGraph $accepted[0].graph $accepted[0].snapshots;Assert-C4B $accepted[0].graph.ready 'accepted component not three'
    foreach($wait in @($Records|Where-Object type -eq console_wait)){
        Assert-C4B ($wait.result -ceq 'complete' -and $wait.error -eq 0 -and $wait.owner -eq $startup.owner_sta -and $wait.modes_observed -and -not $wait.mode_changed -and $wait.mode_before -eq $wait.mode_after -and $wait.waits -gt 0 -and $wait.pumps -gt 0 -and $wait.messages -le $(if($wait.live_pump){8}else{64})*$wait.pumps) 'console safety/pump contract'
        foreach($field in @('text','line','clipboard','clipboard_contents','user_file','url')){Assert-C4B ($field -notin $wait.PSObject.Properties.Name) 'console contents logged'}
    }
    foreach($kind in @('prepare','magnet_action','topology_acceptance','glue_action')){Assert-C4B (@($Records|Where-Object {$_.type -eq 'console_wait' -and $_.kind -ceq $kind -and $_.live_pump}).Count -gt 0) 'live input pump missing'}
    $receipts=@($Records|Where-Object type -eq receipt|Sort-Object receipt);$map=@{}
    for($i=0;$i -lt $receipts.Count;++$i){$r=$receipts[$i];Assert-C4B ($r.receipt -eq $i+1 -and $r.member -in 0,1,2 -and $r.callback_qpc -gt 0 -and $r.event -cin @('START','LOCATION','END')) 'raw receipt lifecycle'
        $b=$bindings[$r.member];Assert-C4B ($r.window_id -eq $b.window_id -and $r.capability -eq $b.capability -and $r.hwnd -eq $b.hwnd -and $r.tid -eq $b.tid) 'raw receipt capability'
        Assert-C4B ($r.ctrl_available -eq ($r.event -ceq 'START')) 'Ctrl sampled outside START';$map[[long]$r.receipt]=$r
    }
    $gestures=@($Records|Where-Object type -eq gesture|Sort-Object gesture);$gmap=@{}
    for($i=0;$i -lt $gestures.Count;++$i){$g=$gestures[$i];Assert-C4B ($g.gesture -eq $i+1 -and $g.completed -and $g.source -in 0,1,2 -and $g.end -gt $g.start) 'gesture identity/completion'
        $start=$map[[long]$g.start];$end=$map[[long]$g.end];Assert-C4B ($start.event -ceq 'START' -and $end.event -ceq 'END' -and $start.member -eq $g.source -and $end.member -eq $g.source -and $start.ctrl -eq $g.ctrl) 'gesture authentic START/END'
        if($i){Assert-C4B ($g.start -gt $gestures[$i-1].end) 'overlapping gestures'}
        Test-C4BMembers $g.initial $bindings;Test-C4BMembers $g.final $bindings
        if($g.ctrl){Assert-C4B ($g.route -ceq 'EXISTING_C4A_GLUE' -and $g.solver_calls -eq 0 -and $g.corrections -eq 0 -and $start.callback_qpc -gt $accepted[0].capture_qpc) 'Ctrl route/dual writer'}
        else {Assert-C4B ($g.route -cin @('MAGNET_MOVE','MAGNET_RESIZE','CLASSIFYING') -and $end.callback_qpc -lt $accepted[0].capture_qpc) 'formation routing/phase'
            for($m=0;$m -lt 3;++$m){if($m -ne $g.source){Assert-C4B ((Get-C4ARectKey $g.initial[$m].visible) -ceq (Get-C4ARectKey $g.final[$m].visible)) 'target mutated during Magnet'}}
        }
        $graphs=@($Records|Where-Object {$_.type -eq 'relation_graph' -and $_.gesture -eq $g.gesture});Assert-C4B ($graphs.Count -eq 1) 'END graph missing'
        Test-C4BGraph $graphs[0].graph $graphs[0].snapshots
        Assert-C4B ((Get-C4AReadinessSnapshotKey $graphs[0].snapshots) -ceq (Get-C4AReadinessSnapshotKey $g.final)) 'graph not final actual snapshot';$gmap[[long]$g.gesture]=$g
    }
    $opmap=@{};$intervals=@();$lastNative=@{};$metrics=@{receipt_owner=@();owner_native=@();native=@();native_postverify=@();receipt_postverify=@()};$realtimeMove=0;$realtimeResize=0;$xy=0
    foreach($op in $corrections){$g=$gmap[[long]$op.gesture];$b=$bindings[$op.source];$key="$($op.gesture)/$($op.operation)"
        Assert-C4B ($null -ne $g -and -not $g.ctrl -and $op.source -eq $g.source -and $op.group -eq $group -and $op.source_id -ceq [string]$b.window_id -and $op.capability -eq $b.capability -and $op.consent -eq $b.consent -and -not $opmap.ContainsKey($key)) 'correction authority/generation'
        $trigger=$map[[long]$op.source_receipt];Assert-C4B ($trigger.member -eq $op.source -and $trigger.event -cin @('LOCATION','END') -and $op.source_receipt -gt $g.start -and $op.source_receipt -le $g.end) 'correction raw source'
        $opmap[$key]=$op
        if(-not $op.native_attempted){Assert-C4B ($op.native_calls -eq 0 -and -not $op.pending_registered -and $op.reason -cin @('raw_sample_superseded','outside_supported_work_area')) 'unexpected skipped correction';continue}
        Assert-C4B ($op.native_success -and $op.exact -and $op.error -eq 0 -and $op.watermark -ge $op.source_receipt -and $op.registration_tick -gt 0 -and $op.axes -cin @('X','Y','XY')) 'native exact/fence/axes'
        Test-C4BMembers $op.before $bindings;Test-C4BMembers $op.actual $bindings
        Assert-C4B ((Get-C4ARectKey $op.before[$op.source].visible) -ceq (Get-C4ARectKey $op.raw) -and (Get-C4ARectKey $op.actual[$op.source].visible) -ceq (Get-C4ARectKey $op.corrected) -and (Get-C4ARectKey $op.actual[$op.source].positioning) -ceq (Get-C4ARectKey $op.target_positioning)) 'exact actual requested rectangles'
        for($edge=0;$edge -lt 4;++$edge){Assert-C4B ([decimal]$op.target_positioning[$edge] -eq ([decimal]$op.corrected[$edge]+[decimal]$op.before[$op.source].positioning[$edge]-[decimal]$op.raw[$edge])) 'visible/positioning inset bridge'}
        $resize=($op.edges -join ',') -cne '0,0,0,0';Assert-C4B ($op.flags -eq $(if($resize){20}else{21}) -and @($op.edges).Count -eq 4 -and ($op.edges[0]+$op.edges[2]) -le 1 -and ($op.edges[1]+$op.edges[3]) -le 1) 'resize edge/native flags'
        if($resize){Assert-C4B ($g.route -ceq 'MAGNET_RESIZE' -and $op.dx -eq 0 -and $op.dy -eq 0 -and ($g.edges -join ',') -ceq ($op.edges -join ',')) 'resize routing/mask';for($edge=0;$edge -lt 4;++$edge){if(-not $op.edges[$edge]){Assert-C4B ($op.raw[$edge] -eq $op.corrected[$edge] -and $g.initial[$op.source].visible[$edge] -eq $op.raw[$edge]) 'nonparticipating resize edge changed'}}}
        else {Assert-C4B ($g.route -ceq 'MAGNET_MOVE' -and (Get-C4ATranslated $op.raw $op.dx $op.dy) -ceq (Get-C4ARectKey $op.corrected) -and ([decimal]$op.raw[2]-$op.raw[0]) -eq ([decimal]$g.initial[$op.source].visible[2]-$g.initial[$op.source].visible[0]) -and ([decimal]$op.raw[3]-$op.raw[1]) -eq ([decimal]$g.initial[$op.source].visible[3]-$g.initial[$op.source].visible[1])) 'Move not one rigid delta'}
        $x=$op.raw[0] -ne $op.corrected[0] -or $op.raw[2] -ne $op.corrected[2];$y=$op.raw[1] -ne $op.corrected[1] -or $op.raw[3] -ne $op.corrected[3]
        Assert-C4B ($op.axes -ceq $(if($x -and $y){'XY'}elseif($x){'X'}elseif($y){'Y'}else{'NONE'})) 'axes lie'
        Assert-C4B ((-not $x -or $null -ne $op.selected_x) -and (-not $y -or $null -ne $op.selected_y)) 'changed axis lacks selected constraint'
        foreach($selected in @($op.selected_x,$op.selected_y)){if($null -eq $selected){continue}
            $target=@($bindings|Where-Object {[string]$_.window_id -ceq $selected.target_id});Assert-C4B ($target.Count -eq 1 -and $target[0].member -ne $op.source -and $selected.kind -in 1,2,3,4) 'selected non-source window target'
            $targetRect=$g.initial[$target[0].member].visible
            Assert-C4B ((Get-C4BEdge $op.corrected $selected.moving_edge) -eq (Get-C4BEdge $targetRect $selected.target_edge) -and [Math]::Abs([decimal]$selected.delta) -le 16 -and ([decimal]$targetRect[$selected.target_edge]-[decimal]$op.raw[$selected.moving_edge]) -eq $selected.delta) 'selected equality/distance'
        }
        for($m=0;$m -lt 3;++$m){if($m -ne $op.source){Assert-C4B ((Get-C4ARectKey $op.before[$m].visible) -ceq (Get-C4ARectKey $g.initial[$m].visible) -and (Get-C4ARectKey $op.actual[$m].visible) -ceq (Get-C4ARectKey $op.before[$m].visible)) 'Magnet wrote/moved target'}}
        Assert-C4B ($op.receipt_qpc -eq $trigger.callback_qpc -and $op.owner_qpc -ge $op.receipt_qpc -and $op.native_start_qpc -ge $op.owner_qpc -and $op.native_return_qpc -ge $op.native_start_qpc -and $op.postverify_qpc -ge $op.native_return_qpc) 'native timing order'
        if($op.native_start_qpc -lt $map[[long]$g.end].callback_qpc){if($resize){++$realtimeResize}else{++$realtimeMove}}
        if($op.axes -ceq 'XY' -and -not $resize){++$xy}
        $f=[double]$startup.qpc_frequency
        if($lastNative.ContainsKey([long]$g.gesture)){$intervals+=($op.native_start_qpc-$lastNative[[long]$g.gesture])*1000/$f};$lastNative[[long]$g.gesture]=$op.native_start_qpc
        $metrics.receipt_owner+=($op.owner_qpc-$op.receipt_qpc)*1000/$f;$metrics.owner_native+=($op.native_start_qpc-$op.owner_qpc)*1000/$f;$metrics.native+=($op.native_return_qpc-$op.native_start_qpc)*1000/$f;$metrics.native_postverify+=($op.postverify_qpc-$op.native_return_qpc)*1000/$f;$metrics.receipt_postverify+=($op.postverify_qpc-$op.receipt_qpc)*1000/$f
    }
    Assert-C4B ($realtimeMove -gt 0 -and $realtimeResize -gt 0 -and $xy -gt 0) 'real pre-END Move/Resize and combined XY corrections required'
    $acks=@{};$duplicates=0
    foreach($s in @($Records|Where-Object type -eq sample)){Assert-C4B ($s.source -eq $gmap[[long]$s.gesture].source -and $s.callback_qpc -eq $map[[long]$s.receipt].callback_qpc) 'sample source'
        if($s.result -cin @('acknowledged','duplicate')){$key="$($s.gesture)/$($s.operation)";Assert-C4B $opmap.ContainsKey($key) 'feedback operation missing';$op=$opmap[$key]
            Assert-C4B ($op.exact -and $map[[long]$s.receipt].event -ceq 'LOCATION' -and $s.receipt -gt $op.watermark -and $s.provider_tick -gt $op.registration_tick -and (Get-C4ARectKey $s.raw) -ceq (Get-C4ARectKey $op.corrected)) 'feedback fence/geometry/event'
            if($s.result -ceq 'acknowledged'){Assert-C4B (-not $acks.ContainsKey($key)) 'duplicate ACK';$acks[$key]=$true}else{Assert-C4B $acks.ContainsKey($key) 'duplicate without ACK';++$duplicates}
        }
    }
    foreach($g in $gestures){$ops=@($corrections|Where-Object {$_.gesture -eq $g.gesture -and $_.exact});$ga=@($acks.Keys|Where-Object {$_.StartsWith("$($g.gesture)/")}).Count
        Assert-C4B ($g.corrections -eq $ops.Count -and $g.acknowledged -eq $ga -and $g.missing -eq $ops.Count-$ga -and $g.solver_calls -le $g.meaningful) 'gesture operation/feedback counters'
        if(-not $g.ctrl){$last=@($Records|Where-Object {$_.type -eq 'sample' -and $_.gesture -eq $g.gesture}|Sort-Object receipt|Select-Object -Last 1)
            $expected=$g.initial[$g.source].visible
            if($last.Count){$expected=$last[0].raw;$lastOp=@($ops|Where-Object source_receipt -eq $last[0].receipt);if($lastOp.Count){Assert-C4B ($lastOp.Count -eq 1) 'multiple writes for one raw sample';$expected=$lastOp[0].corrected}}
            Assert-C4B ((Get-C4ARectKey $g.final[$g.source].visible) -ceq (Get-C4ARectKey $expected)) 'missing feedback with nonexact final geometry'
        }
    }
    foreach($q in @($Records|Where-Object type -eq quantum)){Assert-C4B ($q.solver_calls -le 1 -and $q.corrections -le 1 -and $q.active_inventory -eq 0 -and $q.active_manager_creates -eq 0 -and $q.end_qpc -ge $q.owner_qpc) 'quantum storm/performance gate'}
    for($a=0;$a -lt 8;++$a){$rows=@($Records|Where-Object {$_.type -eq 'action' -and $_.action -eq $a});Assert-C4B ($rows.Count -ge 1 -and $rows.Count -le 5 -and $rows[-1].passed -and $rows[-1].sequence -lt $accepted[0].sequence) 'bounded successful action required'
        for($i=0;$i -lt $rows.Count;++$i){Assert-C4B ($rows[$i].attempt -eq $i+1) 'action attempt order';Test-C4BMembers $rows[$i].snapshots $bindings;Test-C4BGraph $rows[$i].graph $rows[$i].snapshots
            Assert-C4B ($rows[$i].passed -eq (Test-C4BActionProof $a $rows[$i] $gestures $corrections)) 'action result lacks geometry/event/correction proof'
        }
    }
    $glue=@($Records|Where-Object type -eq glue_gesture|Sort-Object gesture);Assert-C4B ($glue.Count -eq 3) 'A/B/C Glue gestures required'
    $batches=@($Records|Where-Object type -eq glue_batch)
    for($i=0;$i -lt 3;++$i){$g=$glue[$i];Assert-C4B ($g.gesture -eq $i+1 -and $g.source -eq $i -and $g.exact -and $g.roles_cleared -and $g.pending_empty -and $g.group_ready -and $g.batches -gt 0) 'dynamic Glue completion'
        $dx=[decimal]$g.final[$i].visible[0]-[decimal]$g.initial[$i].visible[0];$dy=[decimal]$g.final[$i].visible[1]-[decimal]$g.initial[$i].visible[1]
        for($m=0;$m -lt 3;++$m){Assert-C4B ((Get-C4ARectKey $g.final[$m].visible) -ceq (Get-C4ATranslated $g.initial[$m].visible $dx $dy)) 'Glue rigid geometry'}
        $ops=@($batches|Where-Object gesture -eq $g.gesture);Assert-C4B ($ops.Count -eq $g.batches) 'Glue batch count'
        foreach($op in $ops){Assert-C4B ($op.source -eq $i -and $op.deferred -eq 2 -and $i -notin $op.members.member -and @($op.members.member|Sort-Object -Unique).Count -eq 2) 'Glue writes only two followers'
            $trigger=$map[[long]$op.source_receipt];Assert-C4B ($trigger.member -eq $i -and $trigger.event -cin @('LOCATION','END') -and $op.source_receipt -gt $g.start -and $op.source_receipt -le $g.end -and $op.watermark -ge $op.source_receipt) 'Glue source provenance'
            $ox=[decimal]$op.source_visible[0]-[decimal]$g.initial[$i].visible[0];$oy=[decimal]$op.source_visible[1]-[decimal]$g.initial[$i].visible[1]
            foreach($m in $op.members){Assert-C4B ((Get-C4ARectKey $m.target) -ceq (Get-C4ATranslated $g.initial[$m.member].visible $ox $oy)) 'Glue initial-relative targets'}
        }
    }
    foreach($op in $batches){Assert-C4B ($op.preflight -and $op.pending_registered -and $op.native_attempted -and $op.native_success -and $op.exact -and $op.error -eq 0 -and $op.flags -eq 21) 'Glue native batch safety';foreach($m in $op.members){Assert-C4B ((Get-C4ARectKey $m.actual) -ceq (Get-C4ARectKey $m.target) -and (Get-C4ARectKey $m.actual_positioning) -ceq (Get-C4ARectKey $m.positioning_target)) 'Glue postverify'}}
    $restore=@($batches|Where-Object gesture -eq 0);Assert-C4B ($restore.Count -eq 1 -and (($restore[0].members.member|Sort-Object) -join ',') -ceq '0,1,2') 'exact restore members'
    foreach($m in $restore[0].members){Assert-C4B ((Get-C4ARectKey $m.actual) -ceq (Get-C4ARectKey $accepted[0].snapshots[$m.member].visible) -and (Get-C4ARectKey $m.actual_positioning) -ceq (Get-C4ARectKey $accepted[0].snapshots[$m.member].positioning)) 'restore accepted baseline'}
    $summary=@($Records|Where-Object type -eq summary);Assert-C4B ($summary.Count -eq 1 -and $summary[0].restore_exact -and $summary[0].raw_receipts -eq $receipts.Count -and $summary[0].overflow -eq 0 -and $summary[0].post_failure -eq 0 -and $summary[0].active_inventory -eq 0 -and $summary[0].active_manager_creates -eq 0 -and $summary[0].vdm_queries -gt 0) 'summary queue/performance/restore'
    $glueAck=@{}
    foreach($f in @($Records|Where-Object type -eq glue_feedback)){
        if($f.result -ceq 'acknowledged'){
            $key="$($f.gesture)/$($f.batch)/$($f.member)";Assert-C4B (-not $glueAck.ContainsKey($key)) 'duplicate Glue ACK'
            $op=@($batches|Where-Object {$_.gesture -eq $f.gesture -and $_.batch -eq $f.batch});Assert-C4B ($op.Count -eq 1 -and $f.member -in $op[0].members.member -and $f.receipt -gt $op[0].watermark) 'Glue feedback generation'
            $r=$map[[long]$f.receipt];$target=@($op[0].members|Where-Object member -eq $f.member)[0]
            $delta=([decimal]$r.native_time-[decimal]$op[0].pre_native_tick+4294967296)%4294967296
            Assert-C4B ($r.member -eq $f.member -and $r.event -ceq 'LOCATION' -and $delta -gt 0 -and $delta -lt 2147483648 -and (Get-C4ARectKey $f.observed) -ceq (Get-C4ARectKey $target.target)) 'Glue member feedback geometry/tick'
            $glueAck[$key]=$true
        }else{Assert-C4B ($f.result -cin @('stale_or_start_tick_ambiguous','pre_native_or_tick_ambiguous','exact_duplicate_or_noop')) 'unexpected Glue feedback'}
    }
    Assert-C4B ($summary[0].glue_missing -eq 2*@($batches|Where-Object gesture -gt 0).Count-$glueAck.Count) 'Glue missing reconciliation'
    Assert-C4B ((Get-C4AReadinessSnapshotKey $summary[0].final) -ceq (Get-C4AReadinessSnapshotKey $accepted[0].snapshots)) 'final snapshot not accepted baseline'
    $answers=@($Records|Where-Object type -eq subjective);Assert-C4B ($answers.Count -eq 5) 'five human subjective answers'
    foreach($question in @('magnet_feel','too_sticky','misses','visible_jitter','rigid_body_feel')){$answer=@($answers|Where-Object question -eq $question);Assert-C4B ($answer.Count -eq 1 -and $answer[0].answer -cin $(if($question -eq 'magnet_feel'){@('A','B','C','D','E')}elseif($question -eq 'rigid_body_feel'){@('YES','MOSTLY','NO')}else{@('YES','NO')})) 'invalid subjective choice'}
    $metrics.correction_interval=$intervals;$percentiles=[ordered]@{};foreach($key in $metrics.Keys){$percentiles[$key]=@{p50_ms=(Get-C4APercentile @($metrics[$key]) 0.5);p95_ms=(Get-C4APercentile @($metrics[$key]) 0.95)}}
    return [pscustomobject]@{Result='PASS';HumanSeal=$(if($AllowSynthetic){'NOT_HUMAN_EVIDENCE'}else{'REVIEW_REQUIRED'});ImplementationSHA=$startup.implementation_sha;Gestures=$gestures.Count;NativeCorrections=@($corrections|Where-Object native_attempted -eq $true).Count;MaxQueue=$summary[0].max_queue;Metrics=$percentiles}
}
function Test-C4BEvidence {
    param([string]$Path,[switch]$AllowSynthetic)
    Assert-C4B (Test-Path -LiteralPath $Path -PathType Leaf) 'evidence missing';$rows=[Collections.Generic.List[object]]::new()
    $reader=[IO.StreamReader]::new([IO.Path]::GetFullPath($Path),[Text.UTF8Encoding]::new($false,$true))
    try{while($null -ne ($line=$reader.ReadLine())){Assert-C4B (-not [string]::IsNullOrWhiteSpace($line)) 'blank JSONL';$rows.Add(($line|ConvertFrom-Json -ErrorAction Stop))}}finally{$reader.Dispose()}
    return Test-C4BRecords $rows.ToArray() -AllowSynthetic:$AllowSynthetic
}
