# Phase 2 supplements (never replaces) frozen correctness and span-forest gates.
function Assert-ConsentBoundProfileEvidence {
    param([object[]] $Records)
    $startup=Assert-UniqueRecord $Records 'startup'
    if ($startup.build_configuration -ne 'Debug' -or $startup.consent_bound_contract -ne 'r1c3b_frame_authority_v1') {
        throw 'Phase 2 comparison requires the explicit Debug consent-bound profile.'
    }
    $status=Assert-UniqueRecord $Records 'validation_status'
    $validations=@($Records | Where-Object record_kind -eq validation)
    Assert-EvidenceBoolean $status @('validation_overflow')
    if ($status.validation_overflow -or $status.validation_record_capacity -ne 8192 -or
        $status.validation_record_count -ne $validations.Count -or $validations.Count -gt 8192 -or
        $status.active_native_after_invalidation -ne 0) { throw 'Invalid/overflowed validation evidence or write after invalidation.' }
    $spans=@($Records | Where-Object record_kind -eq profile_span)
    $map=@{}; foreach ($s in $spans) { $map[[uint64]$s.span_id]=$s }
    $quanta=@{}; foreach ($q in @($Records | Where-Object record_kind -eq processing_quantum)) { $quanta[[uint64]$q.processing_quantum_id]=$q }
    $startAttempt=Assert-UniqueRecord $Records 'activation_attempt'
    $activated=$startAttempt.activation_generation -ne 0
    $startReceipt=@($Records|Where-Object { $_.record_kind -eq 'event_receipt' -and $_.receipt_sequence -eq $startAttempt.leader_start_receipt_sequence })[0]
    $identities=@{}; foreach ($i in @($Records|Where-Object record_kind -eq native_target_identity)) { $identities[$i.role]=$i }
    $seen=@{}; $phaseTotals=@{}; $active=0
    foreach ($phase in @('setup','start','active','end','restore','invalidation')) {
        $phaseTotals[$phase]=[uint64]0
        [void](Get-EvidenceInteger $status ('global_inventory_calls_'+$phase) -AllowZero)
    }
    foreach ($v in $validations) {
        foreach ($name in @('span_id','quantum_id','operation_generation','source_receipt','target_native_key',
            'capability_generation','consent_generation','global_inventory_calls')) { [void](Get-EvidenceInteger $v $name -AllowZero) }
        Assert-EvidenceBoolean $v @('succeeded')
        if (-not $phaseTotals.ContainsKey($v.validation_phase)) { throw 'Unknown validation phase.' }
        $phaseTotals[$v.validation_phase]+=[uint64]$v.global_inventory_calls
        if ($v.validation_mode -notin @('FULL_GLOBAL_INVENTORY','CONSENT_BOUND_FRAME_FAST')) { throw 'Unsupported validation mode.' }
        $fast=$v.validation_mode -eq 'CONSENT_BOUND_FRAME_FAST'
        $reason=if ($fast) {'accepted_ctrl_start_private_frame_pair_canonical_anchor'} else {'full_boundary_validation'}
        if ($v.legal_reason -ne $reason -or ($fast -and ($v.validation_phase -ne 'active' -or $v.global_inventory_calls -ne 0))) {
            throw 'Fast mode legality/phase/zero-inventory contract failed.'
        }
        if ($v.validation_phase -ne 'setup' -and (-not $v.succeeded -or $v.invalidation_reason -ne 'none')) {
            throw ('Invalidated run cannot pass positive smoothness: '+$v.invalidation_reason)
        }
        if (-not $v.span_id) {
            if ($fast -or $v.quantum_id -ne 0 -or $v.operation_generation -ne 0 -or
                $v.validation_phase -notin @('setup','restore','invalidation')) { throw 'Unmeasured active/boundary validation hidden as span zero.' }
            continue
        }
        $id=[uint64]$v.span_id
        if ($seen.ContainsKey($id) -or -not $map.ContainsKey($id)) { throw 'Duplicate/orphan validation span.' }
        $seen[$id]=$true; $s=$map[$id]
        if ($s.stage -ne $(if ($fast) {'consent_bound_validation'} else {'full_validation'}) -or
            $s.quantum_id -ne $v.quantum_id -or $s.operation_generation -ne $v.operation_generation -or
            $s.source_receipt -ne $v.source_receipt) { throw 'Validation does not join its measured invocation.' }
        $target=$identities[$v.target_role]
        $expectedRole=if ($map[[uint64]$s.parent_span_id].stage -eq 'pair_witness') {'leader'} else {$s.role}
        if ($v.target_role -ne $expectedRole) {throw 'Validation role does not match its pipeline witness slot.'}
        if ($null -eq $target -or $target.native_key -ne $v.target_native_key -or
            $target.capability_generation -ne $v.capability_generation -or $target.consent_generation -ne $v.consent_generation) {
            throw 'Validation target/object authority was substituted.'
        }
        $q=$quanta[[uint64]$v.quantum_id]
        $hasStart=@($Records | Where-Object { $_.record_kind -eq 'event_receipt' -and
            $_.processing_quantum_id -eq $v.quantum_id -and $_.receipt_sequence -eq $startAttempt.leader_start_receipt_sequence }).Count -gt 0
        $expectedPhase=if ($q.contains_leader_end) {'end'} elseif ($hasStart) {'start'}
            elseif ($activated -and $v.quantum_id -gt $startReceipt.processing_quantum_id) {'active'} else {'setup'}
        if ($v.validation_phase -ne $expectedPhase -or ($expectedPhase -eq 'active' -and -not $fast)) {
            throw 'Validation phase hides a steady active inventory request.'
        }
        $inventory=@($spans | Where-Object { $_.parent_span_id -eq $id -and $_.stage -eq 'shell_inventory' })
        if ($v.global_inventory_calls -ne $inventory.Count -or (-not $fast -and $inventory.Count -ne 1)) {
            throw 'Measured inventory invocation does not match request counter.'
        }
        if ($fast) { ++$active }
    }
    foreach ($s in @($spans | Where-Object { $_.stage -in @('full_validation','consent_bound_validation') })) {
        if (-not $seen.ContainsKey([uint64]$s.span_id)) { throw 'Unaccounted live validation invocation.' }
    }
    foreach ($s in @($spans|Where-Object stage -eq shell_inventory)) {
        $parent=[uint64]$s.parent_span_id
        if (-not $seen.ContainsKey($parent) -or $map[$parent].stage -ne 'full_validation') {
            throw 'Unaccounted global inventory span outside full boundary validation.'
        }
    }
    foreach ($phase in $phaseTotals.Keys) {
        $calls=$status.('global_inventory_calls_'+$phase)
        if (($phase -eq 'setup' -and $calls -lt $phaseTotals[$phase]) -or
            ($phase -ne 'setup' -and $calls -ne $phaseTotals[$phase])) { throw 'Inventory phase totals do not reconcile.' }
        Write-Output "global_inventory_calls_${phase} = $calls"
    }
    if (($activated -and $active -eq 0) -or (-not $activated -and $active -ne 0) -or $status.global_inventory_calls_active -ne 0 -or
        $status.global_inventory_calls_start -eq 0 -or $status.global_inventory_calls_end -eq 0 -or
        $status.global_inventory_calls_restore -eq 0) { throw 'No proven active fast path or missing full boundaries.' }
    foreach ($op in @($Records | Where-Object { $_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower' })) {
        $v=@($validations|Where-Object operation_generation -eq $op.behavior_operation_generation)
        if ($v.Count -ne 4 -or @($v|Where-Object target_role -eq leader).Count -ne 1 -or
            @($v|Where-Object target_role -eq follower).Count -ne 3 -or
            @($v.validation_mode|Select-Object -Unique).Count -ne 1) { throw 'Operation lacks independent Leader + prepare/immediate/postverify Follower witnesses.' }
        Write-Output "OP_VALIDATION generation=$($op.behavior_operation_generation) mode=$($v[0].validation_mode) phase=$($v[0].validation_phase) reason=$($v[0].legal_reason)"
    }
    $largest=@{}; $steadyOps=0
    foreach ($op in @($Records | Where-Object { $_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower' })) {
        $v=@($validations|Where-Object operation_generation -eq $op.behavior_operation_generation)
        if ($v[0].validation_phase -ne 'active') {continue}
        ++$steadyOps; $totals=@{}
        foreach ($s in @($spans|Where-Object operation_generation -eq $op.behavior_operation_generation)) {
            $ticks=[decimal]$s.end_qpc-[decimal]$s.begin_qpc
            foreach ($child in @($spans|Where-Object parent_span_id -eq $s.span_id)) {$ticks-=([decimal]$child.end_qpc-[decimal]$child.begin_qpc)}
            if (-not $totals.ContainsKey($s.stage)) {$totals[$s.stage]=[decimal]0}; $totals[$s.stage]+=$ticks
        }
        $top=@($totals.GetEnumerator()|Sort-Object @{Expression='Value';Descending=$true},Name)[0].Key
        if (-not $largest.ContainsKey($top)) {$largest[$top]=0}; ++$largest[$top]
    }
    foreach ($entry in @($largest.GetEnumerator()|Sort-Object Name)) {Write-Output "LARGEST_STEADY_ACTIVE $($entry.Key): $($entry.Value)/$steadyOps operations (exclusive)"}
    $inventoryLargest=if ($largest.ContainsKey('shell_inventory')) {$largest['shell_inventory']} else {0}
    if ($inventoryLargest -ne 0) {throw 'Global inventory is still a steady-active hotspot.'}
    Write-Output "LARGEST_STEADY_ACTIVE shell_inventory: $inventoryLargest/$steadyOps operations"
    if ($activated) { Write-Output 'CONSENT_BOUND_FRAME_FAST_PATH_EVIDENCE_GATE: PASS; active inventory=0; no invalidation; original authority only' }
    else { Write-Output 'CONSENT_BOUND_FRAME_FAST_PATH_EVIDENCE_GATE: NOT_ACTIVATED; no active operations' }
}

function Get-Phase2ComparisonMetrics {
    param([object[]] $Records, [switch] $Phase2)
    $freq=[decimal]($Records|Where-Object record_kind -eq startup).qpc_frequency_hz
    $spans=@($Records|Where-Object record_kind -eq profile_span)
    $raw=@{}; foreach ($r in @($Records|Where-Object record_kind -eq event_receipt)) { $raw[[uint64]$r.receipt_sequence]=$r }
    $qs=@{}; foreach ($q in @($Records|Where-Object record_kind -eq processing_quantum)) {$qs[[uint64]$q.processing_quantum_id]=$q}
    $pq=@($Records|Where-Object record_kind -eq profile_quantum)
    $attempt=$Records|Where-Object record_kind -eq activation_attempt
    $startQ=$raw[[uint64]$attempt.leader_start_receipt_sequence].processing_quantum_id
    $ops=@($Records|Where-Object { $_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower' })
    $exclusive=[decimal]0
    foreach ($s in @($spans|Where-Object stage -eq shell_inventory)) {
        $ticks=[decimal]$s.end_qpc-[decimal]$s.begin_qpc
        foreach ($c in @($spans|Where-Object parent_span_id -eq $s.span_id)) { $ticks-=([decimal]$c.end_qpc-[decimal]$c.begin_qpc) }
        if ($ticks -lt 0) {throw 'Negative exclusive inventory duration.'}; $exclusive+=$ticks
    }
    $steady=@($spans|Where-Object { $_.stage -eq 'shell_inventory' -and
        -not $qs[[uint64]$_.quantum_id].contains_leader_end -and $_.quantum_id -gt $startQ -and $attempt.activation_generation -ne 0 })
    $activeExclusive=[decimal]0
    foreach ($s in $steady) {
        $ticks=[decimal]$s.end_qpc-[decimal]$s.begin_qpc
        foreach ($child in @($spans|Where-Object parent_span_id -eq $s.span_id)) { $ticks-=([decimal]$child.end_qpc-[decimal]$child.begin_qpc) }
        $activeExclusive+=$ticks
    }
    $m=[ordered]@{
        'shell_inventory_active_calls'=$(if ($Phase2) {($Records|Where-Object record_kind -eq validation_status).global_inventory_calls_active} else {$steady.Count})
        'shell_inventory_exclusive_total_ms'=1000*$exclusive/$freq
        'shell_inventory_active_exclusive_ms'=1000*$activeExclusive/$freq
        'max_queue_depth'=($pq|Measure-Object max_queue_depth -Maximum).Maximum
        'coalesced_leader_events'=($pq|Measure-Object coalesced_leader_location_count -Sum).Sum
        'arrivals_during_previous_quantum'=($pq|Measure-Object receipts_arrived_during_previous_quantum -Sum).Sum
        'follower_apply_count'=$ops.Count
    }
    $samples=@{quantum=@();'apply_interval'=@();'receipt_to_owner'=@();'owner_to_native'=@();'native_call'=@();'postverify_only'=@();'native_start_to_postverify'=@();'receipt_to_postverify'=@()}
    foreach ($q in @($spans|Where-Object stage -eq quantum)) {$samples.quantum+=1000*([decimal]$q.end_qpc-[decimal]$q.begin_qpc)/$freq}
    foreach ($r in $raw.Values) { if (-not $r.discarded_after_end) {$samples.receipt_to_owner+=1000*([decimal]$qs[[uint64]$r.processing_quantum_id].owner_drain_start_qpc-[decimal]$r.callback_qpc)/$freq} }
    $last=$null
    foreach ($o in $ops) {
        if ($null -ne $last) {$samples.apply_interval+=1000*([decimal]$o.native_apply_start_qpc-$last)/$freq}; $last=[decimal]$o.native_apply_start_qpc
        $samples.owner_to_native+=1000*([decimal]$o.native_apply_start_qpc-[decimal]$qs[[uint64]$o.processing_quantum_id].owner_drain_start_qpc)/$freq
        $samples.native_call+=1000*([decimal]$o.native_api_return_qpc-[decimal]$o.native_apply_start_qpc)/$freq
        $samples.postverify_only+=1000*([decimal]$o.postverify_complete_qpc-[decimal]$o.native_api_return_qpc)/$freq
        $samples.native_start_to_postverify+=1000*([decimal]$o.postverify_complete_qpc-[decimal]$o.native_apply_start_qpc)/$freq
        $samples.receipt_to_postverify+=1000*([decimal]$o.postverify_complete_qpc-[decimal]$raw[[uint64]$o.source_leader_sequence].callback_qpc)/$freq
    }
    foreach ($key in $samples.Keys) {
        $stats=Get-TimingStatistics -Samples ([double[]]$samples[$key])
        $m[$key+'_p50_ms']=$stats.p50; $m[$key+'_p95_ms']=$stats.p95
    }
    return $m
}
function Write-Phase2BaselineComparison {
    param([object[]] $Records,[string] $RepositoryRoot)
    $path=Join-Path $RepositoryRoot 'docs/reports/R1C3B_PHASE1_DEBUG_PROFILE_BASELINE.json'
    $baseline=Get-Content -Raw -Encoding UTF8 -LiteralPath $path | ConvertFrom-Json
    if ($baseline.schema_version -ne 1 -or $baseline.source_configuration -ne 'Debug' -or
        $baseline.source_prefix -ne '20260909T143539511Z' -or $baseline.source_harness_sha256 -ne
        '8BAC49C1F2D7AC4F113651C123A50EBAE5208E0EF7EF3E5A8891825797A62322') { throw 'Frozen derived Phase 1 baseline provenance changed.' }
    $new=Get-Phase2ComparisonMetrics $Records -Phase2
    Write-Output 'PHASE1 DEBUG BASELINE vs PHASE2 DEBUG (per-operation/quantum grain; NOT raw event rate or FPS)'
    Write-Output "PHASE1 operation-local shell_inventory share=$($baseline.operation_local_shell_inventory_share_percent)%"
    foreach ($key in $new.Keys) {
        $value=$baseline.metrics.PSObject.Properties[$key]
        if ($null -eq $value -or [double]::IsNaN([double]$value.Value) -or [double]::IsInfinity([double]$value.Value) -or
            [double]$value.Value -lt 0) { throw 'Missing/invalid baseline metric.' }
        Write-Output ("COMPARE {0}: phase1={1}; phase2={2}" -f $key,$value.Value,$new[$key])
    }
    Write-Output 'Human rating A/B required for subjective improvement; C => SMOOTHNESS_RUNTIME_GATE=FAIL; no automatic second-hotspot rewrite.'
}
