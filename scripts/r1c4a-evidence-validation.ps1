Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'r1c4a-readiness-validation.ps1')
. (Join-Path $PSScriptRoot 'r1c4a-console-wait-validation.ps1')
. (Join-Path $PSScriptRoot 'r1c4a-blocked-validation.ps1')
. (Join-Path $PSScriptRoot 'r1c4a-capture-validation.ps1')

function Assert-C4A {
    param([bool] $Condition, [string] $Message)
    if (-not $Condition) { throw "R1C4A evidence: $Message" }
}
function Get-C4ARectKey {
    param($Rect)
    Assert-C4A ($null -ne $Rect -and @($Rect).Count -eq 4) 'invalid rectangle'
    foreach ($n in $Rect) {
        Assert-C4A (($n -is [int] -or $n -is [long] -or $n -is [decimal]) -and [decimal]$n -eq [decimal]::Truncate([decimal]$n) -and [decimal]$n -ge [long]::MinValue -and [decimal]$n -le [long]::MaxValue) 'non-integer/out-of-range rectangle'
    }
    Assert-C4A ($Rect[2] -gt $Rect[0] -and $Rect[3] -gt $Rect[1]) 'nonpositive extent'
    Assert-C4A (([decimal]$Rect[2]-[decimal]$Rect[0]) -le [long]::MaxValue -and ([decimal]$Rect[3]-[decimal]$Rect[1]) -le [long]::MaxValue) 'unrepresentable extent'
    return ($Rect -join ',')
}
function Get-C4ATranslated {
    param($Rect, [decimal] $Dx, [decimal] $Dy)
    return (@(([decimal]$Rect[0]+$Dx),([decimal]$Rect[1]+$Dy),([decimal]$Rect[2]+$Dx),([decimal]$Rect[3]+$Dy)) -join ',')
}
function Get-C4APercentile {
    param([object[]] $Values, [double] $Fraction)
    if (-not $Values.Count) { return $null }
    $ordered = @($Values | Sort-Object)
    $index = ($ordered.Count-1)*$Fraction
    $low = [int][Math]::Floor($index); $high = [int][Math]::Ceiling($index)
    return ([double]$ordered[$low]+([double]$ordered[$high]-[double]$ordered[$low])*($index-$low))
}
function Test-C4ARecords {
    param([object[]] $Records)
    Assert-C4A ($Records.Count -ge 2) 'empty/incomplete log'
    for ($i=0;$i -lt $Records.Count;++$i) {
        Assert-C4A ($Records[$i].schema -ceq 'r1c4a/v1' -and $Records[$i].sequence -eq $i+1) 'schema/sequence gap'
    }
    Assert-C4A ($Records[0].type -ceq 'startup' -and $Records[-1].type -ceq 'shutdown') 'lifecycle boundaries'
    Assert-C4A (@($Records | Where-Object type -eq startup).Count -eq 1) 'duplicate startup'
    Assert-C4A (@($Records | Where-Object type -eq shutdown).Count -eq 1) 'duplicate shutdown'
    Assert-C4A ($Records[0].member_count -eq 3 -and $Records[0].interactive_console -eq $true -and $Records[0].qpc_frequency -gt 0) 'fixture/input/QPC contract'
    $frequency=[double]$Records[0].qpc_frequency
    $bindings=@($Records | Where-Object type -eq binding | Sort-Object member)
    $prompts=@($Records | Where-Object type -eq target_prompt | Sort-Object member)
    $confirmed=@($Records | Where-Object type -eq target_confirmed | Sort-Object member)
    $consent=@($Records | Where-Object type -eq group_consent)
    $captureDiagnostics=Test-C4ACaptureRecords $Records
    Assert-C4A ($Records[-1].result -cin @('PASS','BLOCKED')) 'invalid shutdown result'
    $blocked=$null
    if($Records[-1].result -ceq 'BLOCKED') {
        $blocked=Get-C4ABlockedClassification $Records
        $blocked | Add-Member CaptureDiagnostics $captureDiagnostics
        if($blocked.Result -cin @('BLOCKED_DURING_MEMBER_A_PROVISIONING','BLOCKED_DURING_MEMBER_B_PROVISIONING','BLOCKED_DURING_MEMBER_C_PROVISIONING','BLOCKED_DURING_GROUP_CONSENT','BLOCKED_DURING_GROUP_BIND')) {return $blocked}
    }
    Assert-C4A ($bindings.Count -eq 3 -and $prompts.Count -eq 3 -and $confirmed.Count -eq 3) 'exact authorized set'
    Assert-C4A ($consent.Count -eq 1 -and $consent[0].confirmed -eq $true -and $consent[0].input_source -ceq 'interactive_console') 'group consent'
    Assert-C4A (@($bindings.window_id | Sort-Object -Unique).Count -eq 3 -and @($bindings.hwnd | Sort-Object -Unique).Count -eq 3) 'distinct member identity'
    Assert-C4A (@($prompts.nonce_directory | Sort-Object -Unique).Count -eq 3) 'nonce reused'
    $group=$bindings[0].group
    for($i=0;$i -lt 3;++$i) {
        $b=$bindings[$i];$p=$prompts[$i];$c=$confirmed[$i]
        Assert-C4A ($b.member -eq $i -and $p.member -eq $i -and $c.member -eq $i -and $b.group -eq $group -and $group -gt 0) 'binding order/group generation'
        Assert-C4A ($b.window_id -gt 0 -and $b.capability_generation -gt 0 -and $b.consent_generation -gt 0 -and $b.hwnd -gt 0 -and $b.pid -gt 0 -and $b.tid -gt 0) 'empty capability'
        Assert-C4A ($p.baseline_generation -gt 0 -and $p.prompt_generation -gt $p.baseline_generation -and $c.target_confirmation_generation -gt $p.prompt_generation -and $c.eligibility_generation -gt $c.target_confirmation_generation -and $c.token_generation -gt $c.eligibility_generation) 'consent progression'
        Assert-C4A ($c.baseline_exclusion_complete -eq $true -and $c.unique_new_target -eq $true -and $c.exact_location -eq $true -and $c.token_issued -eq $true -and $c.input_source -ceq 'interactive_console') 'target consent proof'
        Assert-C4A ($p.sequence -lt $c.sequence -and $c.sequence -lt $consent[0].sequence -and $consent[0].sequence -lt $b.sequence) 'consent chronology'
    }
    if($blocked -and $blocked.Runtime -ceq 'ENTERED_NOT_PASSED') {
        # Full authorization was checked above. Failed preflight/HDWP may have
        # zero native timestamps; successful-runtime timing gates must not mask
        # that original failure. This return is categorically NOT a PASS.
        return $blocked
    }
    if($blocked -and $blocked.Runtime -ceq 'NOT_STARTED' -and
       'readiness_contract' -in $Records[0].PSObject.Properties.Name -and
       $Records[0].readiness_contract -ceq 'topology_neutral_accepted_baseline_v2') {
        # A failed capture is not a valid preview, but its actual reason must
        # remain visible. This path diagnoses BLOCKED only, never readiness PASS.
        foreach($p in @($Records|Where-Object {$_.type -in @('readiness_preview','readiness_accepted')})) {
            Assert-C4A ($p.native_apply_count -eq 0 -and $p.hdwp_begin_count -eq 0 -and $p.pending_count -eq 0 -and $p.gesture_generation -eq 0 -and $p.leader_present -eq $false -and $p.event_source_running -eq $false) 'activity hidden by pre-accept block'
        }
        return $blocked
    }
    $readiness=Test-C4AReadinessRecords $Records $bindings
    if($blocked){return $blocked}
    if($readiness.Result -ceq 'BLOCKED_BY_LAYOUT_READINESS') {
        Assert-C4AConsoleWaitEvidence $Records -AllowHistoricalBlock
        return $readiness
    }
    Assert-C4AConsoleWaitEvidence $Records
    Assert-C4A ($Records[-1].result -ceq 'PASS') 'failed shutdown'
    Assert-C4A ($Records[-1].user_windows_closed -eq $false) 'user window close'
    $receipts=@($Records|Where-Object type -eq receipt|Sort-Object receipt_sequence)
    $receiptMap=@{}
    for($i=0;$i -lt $receipts.Count;++$i) {
        $r=$receipts[$i];Assert-C4A ($r.receipt_sequence -eq $i+1 -and $r.member -in 0,1,2) 'receipt sequence/member'
        $binding=$bindings[$r.member]
        Assert-C4A ($r.window_id -eq $binding.window_id -and $r.capability_generation -eq $binding.capability_generation -and $r.native_source -eq $binding.hwnd -and $r.native_thread -eq $binding.tid -and $r.callback_qpc -gt 0) 'receipt source capability'
        Assert-C4A ($r.event -cin @('START','LOCATION','END')) 'destroy/unknown event'
        if($r.event -cne 'START'){Assert-C4A ($r.ctrl_available -eq $false) 'input sampled outside START'}
        $receiptMap[[long]$r.receipt_sequence]=$r
    }
    $gestures=@($Records|Where-Object type -eq gesture|Sort-Object gesture)
    Assert-C4A ($gestures.Count -eq 3) 'three completed gestures required'
    $batches=@($Records|Where-Object type -eq batch)
    Assert-C4A (@($batches|Where-Object phase -eq setup).Count -eq 0 -and @($batches|Where-Object phase -eq restore).Count -eq 1) 'no synthetic setup / exact restore count'
    $intervals=[Collections.Generic.List[double]]::new();$native=[Collections.Generic.List[double]]::new()
    $post=[Collections.Generic.List[double]]::new();$receiptOwner=[Collections.Generic.List[double]]::new();$ownerNative=[Collections.Generic.List[double]]::new();$total=[Collections.Generic.List[double]]::new()
    for($i=0;$i -lt 3;++$i) {
        $g=$gestures[$i]
        Assert-C4A ($g.group -eq $group -and $g.gesture -eq $i+1 -and $g.source_member -eq $i) 'dynamic leadership/generation'
        Assert-C4A ($g.exact -eq $true -and $g.roles_cleared -eq $true -and $g.pending_empty -eq $true -and $g.group_ready -eq $true) 'END proof'
        Assert-C4A ($g.starts -eq 1 -and $g.ends -eq 1 -and $g.locations -gt 0 -and $g.callback_ctrl -eq $true) 'activation/native lifecycle'
        $start=$receiptMap[[long]$g.start_receipt];$end=$receiptMap[[long]$g.end_receipt]
        Assert-C4A ($start.event -ceq 'START' -and $end.event -ceq 'END' -and $start.member -eq $i -and $end.member -eq $i -and $start.ctrl_available -eq $true -and $start.ctrl -eq $true) 'authentic member START/END'
        Assert-C4A ($g.callback_qpc -eq $start.callback_qpc -and $g.decision_qpc -ge $g.callback_qpc) 'activation timing'
        if($i){Assert-C4A ($g.start_receipt -gt $gestures[$i-1].end_receipt) 'overlapping gestures'}
        Assert-C4A (@($g.initial).Count -eq 3 -and @($g.final).Count -eq 3) 'three member geometry'
        $dx=[decimal]$g.final[$i].visible[0]-[decimal]$g.initial[$i].visible[0]
        $dy=[decimal]$g.final[$i].visible[1]-[decimal]$g.initial[$i].visible[1]
        for($m=0;$m -lt 3;++$m) {
            Assert-C4A ((Get-C4ARectKey $g.final[$m].visible) -ceq (Get-C4ATranslated $g.initial[$m].visible $dx $dy)) 'final rigid-body delta'
            Assert-C4A ($g.initial[$m].dpi -eq $g.initial[0].dpi -and $g.initial[$m].monitor -ceq $g.initial[0].monitor -and $g.initial[$m].current_desktop -eq $true -and $g.initial[$m].exact_location -eq $true) 'member eligibility'
            Assert-C4A ([IO.Path]::GetFileName($g.initial[$m].image) -ieq 'explorer.exe') 'not Explorer'
            if($i){Assert-C4A ((Get-C4ARectKey $g.initial[$m].visible) -ceq (Get-C4ARectKey $gestures[$i-1].final[$m].visible)) 're-layout between gestures'}
        }
        $ops=@($batches|Where-Object {$_.phase -ceq 'active' -and $_.gesture -eq $g.gesture}|Sort-Object batch)
        Assert-C4A ($ops.Count -eq $g.batches -and $ops.Count -gt 0) 'gesture batch count'
        $realtime=0;$previous=0
        for($n=0;$n -lt $ops.Count;++$n) {
            $op=$ops[$n];$trigger=$receiptMap[[long]$op.source_receipt]
            Assert-C4A ($op.batch -eq $n+1 -and $op.group -eq $group -and $op.source_member -eq $i -and $op.watermark -ge $op.source_receipt) 'batch generation/watermark'
            Assert-C4A ($trigger.member -eq $i -and $trigger.event -cin @('LOCATION','END') -and $op.source_receipt -gt $g.start_receipt -and $op.source_receipt -le $g.end_receipt) 'follower recursion/source'
            Assert-C4A ($op.receipt_qpc -eq $trigger.callback_qpc -and $op.owner_qpc -ge $op.receipt_qpc -and $op.native_start_qpc -ge $op.owner_qpc -and $op.native_return_qpc -ge $op.native_start_qpc -and $op.postverify_qpc -ge $op.native_return_qpc) 'timing order'
            if($trigger.event -ceq 'LOCATION' -and $op.native_start_qpc -lt $end.callback_qpc){++$realtime}
            Assert-C4A (@($op.members).Count -eq 2 -and @($op.members.member|Sort-Object -Unique).Count -eq 2 -and $i -notin $op.members.member) 'exact two followers'
            $ox=[decimal]$op.source_visible[0]-[decimal]$g.initial[$i].visible[0];$oy=[decimal]$op.source_visible[1]-[decimal]$g.initial[$i].visible[1]
            foreach($member in $op.members){Assert-C4A ((Get-C4ARectKey $member.target) -ceq (Get-C4ATranslated $g.initial[$member.member].visible $ox $oy)) 'initial-relative target math'}
            if($previous){$intervals.Add(($op.native_start_qpc-$previous)*1000.0/$frequency)};$previous=$op.native_start_qpc
            $native.Add(($op.native_return_qpc-$op.native_start_qpc)*1000.0/$frequency)
            $post.Add(($op.postverify_qpc-$op.native_return_qpc)*1000.0/$frequency)
            $receiptOwner.Add(($op.owner_qpc-$op.receipt_qpc)*1000.0/$frequency)
            $ownerNative.Add(($op.native_start_qpc-$op.owner_qpc)*1000.0/$frequency)
            $total.Add(($op.postverify_qpc-$op.receipt_qpc)*1000.0/$frequency)
        }
        Assert-C4A ($realtime -gt 0) 'no actual pre-END realtime batch'
    }
    foreach($op in $batches) {
        Assert-C4A ($op.phase -cin @('active','restore')) 'unknown native batch phase'
        Assert-C4A ($op.native_path -ceq 'HDWP' -and $op.native_flags -eq 21 -and $op.all_preflight -eq $true -and $op.all_pending_registered -eq $true -and $op.native_attempted -eq $true -and $op.native_succeeded -eq $true -and $op.all_postverify -eq $true -and $op.error -eq 0 -and $op.deferred_count -eq @($op.members).Count) 'native batch gate'
        foreach($m in $op.members){Assert-C4A ($m.exact -eq $true -and (Get-C4ARectKey $m.target) -ceq (Get-C4ARectKey $m.actual) -and (Get-C4ARectKey $m.positioning_target) -ceq (Get-C4ARectKey $m.actual_positioning)) 'individual postverify'}
    }
    $restore=@($batches|Where-Object phase -eq restore)[0]
    Assert-C4A (@($restore.members).Count -eq 3 -and (@($restore.members.member|Sort-Object) -join ',') -ceq '0,1,2') 'restore exact authorized member set'
    foreach($m in $restore.members){Assert-C4A ((Get-C4ARectKey $m.actual) -ceq (Get-C4ARectKey $readiness.AcceptedSnapshots[$m.member].visible) -and (Get-C4ARectKey $m.actual_positioning) -ceq (Get-C4ARectKey $readiness.AcceptedSnapshots[$m.member].positioning)) 'restore differs from accepted baseline'}
    $acked=@{}
    foreach($f in @($Records|Where-Object type -eq feedback)) {
        if($f.result -ceq 'acknowledged') {
            $key="$($f.gesture)/$($f.batch)/$($f.member)";Assert-C4A (-not $acked.ContainsKey($key)) 'duplicate ACK'
            $match=@($batches|Where-Object {$_.gesture -eq $f.gesture -and $_.batch -eq $f.batch -and $_.phase -ceq 'active'})
            Assert-C4A ($match.Count -eq 1 -and $f.member -in $match[0].members.member -and $f.receipt_sequence -gt $match[0].watermark) 'member-specific feedback generation'
            $r=$receiptMap[[long]$f.receipt_sequence];Assert-C4A ($r.member -eq $f.member -and $r.event -ceq 'LOCATION') 'feedback source'
            $g=$gestures[[int]$f.gesture-1]
            Assert-C4A ($f.receipt_sequence -gt $g.start_receipt -and $f.receipt_sequence -le $g.end_receipt) 'cross-gesture feedback'
            $target=@($match[0].members|Where-Object member -eq $f.member)[0]
            Assert-C4A ((Get-C4ARectKey $f.observed_visible) -ceq (Get-C4ARectKey $target.target)) 'feedback exact geometry'
            $tickDelta=([decimal]$r.native_time-[decimal]$match[0].pre_native_tick+4294967296)%4294967296
            Assert-C4A ($tickDelta -gt 0 -and $tickDelta -lt 2147483648) 'stale native feedback time'
            $acked[$key]=$true
        } else {Assert-C4A ($f.result -cin @('stale_or_start_tick_ambiguous','pre_native_or_tick_ambiguous','exact_duplicate_or_noop')) 'unexpected feedback'}
    }
    $summary=@($Records|Where-Object type -eq summary)
    Assert-C4A ($summary.Count -eq 1 -and $summary[0].accepted -eq $receipts.Count -and $summary[0].overflow -eq 0 -and $summary[0].post_failure -eq 0 -and $summary[0].poisoned -eq $false -and $summary[0].running -eq $false -and $summary[0].restore_exact -eq $true -and $summary[0].vdm_queries -gt 0) 'queue/error/restore gate'
    $active=@($batches|Where-Object phase -eq active)
    Assert-C4A ($summary[0].reconciled_missing -eq 2*$active.Count-$acked.Count) 'missing-event reconciliation count'
    $subjective=@($Records|Where-Object type -eq subjective)
    Assert-C4A ($subjective.Count -eq 1 -and $subjective[0].grade -cin @('A','B','C','D','E') -and $subjective[0].rigid_body_feel -cin @('YES','MOSTLY','NO') -and $subjective[0].input_source -ceq 'interactive_console') 'subjective evidence'
    $quantumTimes=@($Records|Where-Object type -eq quantum|ForEach-Object {Assert-C4A ($_.end_qpc -ge $_.owner_qpc) 'quantum timing';($_.end_qpc-$_.owner_qpc)*1000.0/$frequency})
    $metrics=[ordered]@{}
    foreach($item in @(@('batch_interval',$intervals),@('quantum',$quantumTimes),@('receipt_owner',$receiptOwner),@('owner_native',$ownerNative),@('HDWP',$native),@('postverify_all',$post),@('receipt_postverify',$total))) {
        $metrics[$item[0]]=@{p50_ms=(Get-C4APercentile @($item[1]) 0.5);p95_ms=(Get-C4APercentile @($item[1]) 0.95)}
    }
    return [pscustomobject]@{Result='PASS';GroupGeneration=$group;ReadinessPreviewAttempts=$readiness.PreviewAttempts;AcceptedBaseline='fresh_setup_capture';Gestures=3;ActiveBatches=$active.Count;Acknowledged=$acked.Count;ReconciledMissing=$summary[0].reconciled_missing;MaxQueue=$summary[0].max_depth;Grade=$subjective[0].grade;RigidBodyFeel=$subjective[0].rigid_body_feel;Metrics=$metrics}
}
function Test-C4AEvidence {
    param([string] $Path)
    Assert-C4A (Test-Path -LiteralPath $Path -PathType Leaf) 'file missing'
    $records=[Collections.Generic.List[object]]::new()
    $reader=[IO.StreamReader]::new([IO.Path]::GetFullPath($Path),[Text.UTF8Encoding]::new($false,$true))
    try {
        while($null -ne ($line=$reader.ReadLine())) {
            Assert-C4A (-not [string]::IsNullOrWhiteSpace($line)) 'blank JSONL line'
            $records.Add(($line|ConvertFrom-Json -ErrorAction Stop))
        }
    } finally {$reader.Dispose()}
    return Test-C4ARecords $records.ToArray()
}
