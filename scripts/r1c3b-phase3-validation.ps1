# Adds VDM lifetime/freshness checks after all unchanged correctness/frame gates.
function Assert-VdmProfileEvidence {
    param([object[]] $Records)
    $startup=Assert-UniqueRecord $Records 'startup'
    Assert-EvidenceBoolean $startup @('vdm_reuse_enabled','virtual_desktop_result_cache')
    if (-not $startup.vdm_reuse_enabled -or $startup.virtual_desktop_result_cache) {throw 'VDM reuse/result-cache contract violated.'}
    $status=Assert-UniqueRecord $Records 'validation_status'
    foreach($key in @('retained_manager_create_calls','retained_manager_release_calls')) {
        if ((Get-EvidenceInteger $status $key) -ne 1) {throw 'Retained VDM must be created and released exactly once.'}
    }
    $validations=@($Records|Where-Object record_kind -eq validation)
    $spans=@($Records|Where-Object record_kind -eq profile_span)
    $children=@{}; $spanMap=@{}
    foreach($s in $spans) {
        $spanMap[[uint64]$s.span_id]=$s
        $p=[uint64]$s.parent_span_id
        if(-not $children.ContainsKey($p)){$children[$p]=[Collections.Generic.List[object]]::new()}
        $children[$p].Add($s)
    }
    $createTotals=@{}; $queryTotals=@{}
    foreach($phase in @('setup','start','active','end','restore','invalidation')) {$createTotals[$phase]=[uint64]0; $queryTotals[$phase]=[uint64]0}
    foreach($v in $validations) {
        Assert-EvidenceBoolean $v @('retained_vdm')
        $creates=Get-EvidenceInteger $v 'manager_create_calls' -AllowZero
        $queries=Get-EvidenceInteger $v 'virtual_desktop_query_calls' -AllowZero
        if($v.succeeded -and $queries -ne 1) {throw 'A successful validation skipped or repeated its fresh desktop query.'}
        if($v.retained_vdm -and $creates -ne 0) {throw 'Retained manager was recreated during validation.'}
        if($v.succeeded -and -not $v.retained_vdm -and $creates -ne 1) {throw 'Ephemeral provisioning VDM count mismatch.'}
        if($v.validation_phase -ne 'setup' -and -not $v.retained_vdm) {throw 'Private Glue validation did not reuse its manager.'}
        $createTotals[$v.validation_phase]+=$creates; $queryTotals[$v.validation_phase]+=$queries
        if($v.span_id -gt 0) {
            if(-not $v.retained_vdm){throw 'A profiled Glue validation used ephemeral VDM.'}
            $vd=@($children[[uint64]$v.span_id]|Where-Object stage -eq virtual_desktop)
            if($vd.Count -ne 1){throw 'Missing VDM parent validation stage.'}
            $parts=@($children[[uint64]$vd[0].span_id])
            if($parts.Count -ne 1 -or $parts[0].stage -ne 'virtual_desktop_query' -or
                $parts[0].operation_generation -ne $v.operation_generation -or $parts[0].quantum_id -ne $v.quantum_id) {
                throw 'VDM query stage missing, duplicated, recreated, or uncorrelated.'
            }
        }
    }
    foreach($s in @($spans|Where-Object {$_.stage -in @('virtual_desktop_query','virtual_desktop_manager_acquire')})) {
        $parent=$spanMap[[uint64]$s.parent_span_id]
        if($null -eq $parent -or $parent.stage -ne 'virtual_desktop' -or $s.stage -eq 'virtual_desktop_manager_acquire') {throw 'Unexpected profiled manager acquire/query outside VDM validation.'}
    }
    foreach($phase in $createTotals.Keys) {
        $creates=Get-EvidenceInteger $status ('manager_create_calls_'+$phase) -AllowZero
        $queries=Get-EvidenceInteger $status ('virtual_desktop_query_calls_'+$phase) -AllowZero
        $releases=Get-EvidenceInteger $status ('manager_release_calls_'+$phase) -AllowZero
        $extra=if($phase -eq 'setup'){1}else{0}
        $closing=if($phase -eq 'restore'){1}else{0}
        if($creates -ne ($createTotals[$phase]+$extra) -or $queries -ne $queryTotals[$phase] -or
            $releases -ne ($createTotals[$phase]+$closing)) {throw 'VDM phase counters or Release lifecycle do not reconcile.'}
        Write-Output "manager_create_calls_${phase}=$creates; virtual_desktop_query_calls_${phase}=$queries; manager_release_calls_${phase}=$releases"
    }
    $summary=Assert-UniqueRecord $Records 'summary'
    if($status.manager_create_calls_active -ne 0 -or ($summary.activation_count -gt 0 -and $status.virtual_desktop_query_calls_active -eq 0)) {throw 'Active create zero / fresh query gate failed.'}
    Write-Output 'VDM_LIFETIME_FRESHNESS_GATE: PASS; retained create/release=1/1; each required validation queries freshly'
}

function Get-VdmFamilyShare {
    param([object[]] $Records)
    $spans=@($Records|Where-Object record_kind -eq profile_span)
    $childTicks=@{}; $roots=@{}
    foreach($s in $spans) {
        $p=[uint64]$s.parent_span_id
        if(-not $childTicks.ContainsKey($p)){$childTicks[$p]=[decimal]0}
        $childTicks[$p]+=([decimal]$s.end_qpc-[decimal]$s.begin_qpc)
        if($s.stage -eq 'operation'){$roots[[uint64]$s.operation_generation]=$s}
    }
    $total=[decimal]0; foreach($r in $roots.Values){$total+=([decimal]$r.end_qpc-[decimal]$r.begin_qpc)}
    $family=[decimal]0
    foreach($s in $spans) {
        if($s.operation_generation -eq 0 -or $s.stage -notin @('virtual_desktop','virtual_desktop_manager_acquire','virtual_desktop_query')) {continue}
        $root=$roots[[uint64]$s.operation_generation]
        if($null -eq $root -or $s.begin_qpc -lt $root.begin_qpc -or $s.end_qpc -gt $root.end_qpc){continue}
        $ticks=[decimal]$s.end_qpc-[decimal]$s.begin_qpc
        if($childTicks.ContainsKey([uint64]$s.span_id)){$ticks-=$childTicks[[uint64]$s.span_id]}
        if($ticks -lt 0){throw 'Negative VDM exclusive time.'}; $family+=$ticks
    }
    if($total -eq 0){return [decimal]0}; return 100*$family/$total
}
function Write-Phase3BaselineComparison {
    param([object[]] $Records,[string] $RepositoryRoot)
    $phase1=Get-Content -Raw -Encoding UTF8 (Join-Path $RepositoryRoot 'docs/reports/R1C3B_PHASE1_DEBUG_PROFILE_BASELINE.json')|ConvertFrom-Json
    $phase2=Get-Content -Raw -Encoding UTF8 (Join-Path $RepositoryRoot 'docs/reports/R1C3B_PHASE2_DEBUG_PROFILE_BASELINE.json')|ConvertFrom-Json
    if($phase1.source_harness_sha256 -ne '8BAC49C1F2D7AC4F113651C123A50EBAE5208E0EF7EF3E5A8891825797A62322' -or
        $phase2.source_harness_sha256 -ne 'A982CE289FC600C87FDAFF863D32540911A2A262D8B2792B82805FCBB8307F1F' -or
        $phase2.source_configuration -ne 'Debug'){throw 'Historical comparison provenance mismatch.'}
    $current=Get-Phase2ComparisonMetrics $Records -Phase2
    Write-Output 'PHASE1 DEBUG vs PHASE2 DEBUG (C+) vs PHASE3 DEBUG; no raw event-count speed claim'
    foreach($key in $current.Keys) {
        foreach($baseline in @($phase1,$phase2)){
            $v=$baseline.metrics.PSObject.Properties[$key]
            if($null -eq $v -or [double]::IsNaN([double]$v.Value) -or [double]::IsInfinity([double]$v.Value) -or [double]$v.Value -lt 0){throw 'Invalid baseline metric.'}
        }
        Write-Output ('COMPARE {0}: {1} -> {2} -> {3}' -f $key,$phase1.metrics.$key,$phase2.metrics.$key,$current[$key])
    }
    $status=Assert-UniqueRecord $Records 'validation_status'
    $share=Get-VdmFamilyShare $Records
    Write-Output ('PHASE2 DEBUG vs PHASE3 DEBUG VDM-family exclusive share: {0}% -> {1}% (disjoint parent+child exclusive costs)' -f $phase2.virtual_desktop_family_exclusive_share_percent,$share)
    Write-Output ('VDM active create/query: phase2 inferred from source+validation={0}/{1}; phase3 measured={2}/{3}' -f $phase2.active_manager_create_calls_inferred,$phase2.active_virtual_desktop_query_calls_inferred,$status.manager_create_calls_active,$status.virtual_desktop_query_calls_active)
    Write-Output 'Human smoothness: A / B / C+ / B-C / C / D / E or free description; PENDING_UAT, not inferred from timings.'
}
