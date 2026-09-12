[CmdletBinding()]
param([switch] $DefinitionsOnly)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$frameDefinitionsOnly=$DefinitionsOnly
. (Join-Path $PSScriptRoot 'test-r1c3b-profile-runner.ps1') -DefinitionsOnly
. (Join-Path $PSScriptRoot 'r1c3b-phase2-validation.ps1')
$runner=Join-Path $PSScriptRoot 'run-r1c3b-phase2-optimized-profile.ps1'
$fixtureRoot=Join-Path $tempBase ('panebind-r1c3b-frame-runner-'+[Guid]::NewGuid().ToString('N'))
$script:frameFixtureCount=0

function New-FrameProfileRecords {
    param([bool] $Ctrl=$true, [switch] $Delayed, [ValidateSet('None','Native','Postverify')][string] $Arrival='None')
    $records=@(New-ProfileHarnessRecords -CallbackDown:$Ctrl -DelayedDispatch:$Delayed -Arrival $Arrival)
    $baseQ=@{}; foreach ($q in @($records|Where-Object record_kind -eq processing_quantum)) {$baseQ[[long]$q.processing_quantum_id]=$q}
    $ops=@($records|Where-Object { $_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower' })
    # Same production tree shape: one independent Leader witness before the
    # Follower's final immediate check. This fixture never calls Shell/native.
    foreach ($op in $ops) {
        $qid=[long]$op.processing_quantum_id; $origin=$qid*100000
        $root=@($script:c3bSpans|Where-Object { $_.stage -eq 'operation' -and $_.operation_generation -eq $op.behavior_operation_generation })[0]
        Add-ProfileCapture 'pair_witness' ($origin+28100) ($origin+28400) $qid $root.span_id $op.behavior_operation_generation $op.source_leader_sequence 'follower'
    }
    $spans=@($script:c3bSpans.ToArray())
    $removed=@{}
    foreach ($s in @($spans|Where-Object stage -eq full_validation)) {
        if ($Ctrl -and $s.quantum_id -gt 1 -and -not $baseQ[[long]$s.quantum_id].contains_leader_end) {
            $s.stage='consent_bound_validation'
            foreach ($child in @($spans|Where-Object { $_.parent_span_id -eq $s.span_id -and $_.stage -eq 'shell_inventory' })) {$removed[[long]$child.span_id]=$true}
        }
    }
    $spans=@($spans|Where-Object {-not $removed.ContainsKey([long]$_.span_id)}|Sort-Object begin_qpc,@{Expression='end_qpc';Descending=$true},span_id)
    $ids=@{}; for ($i=0;$i -lt $spans.Count;++$i) {$ids[[long]$spans[$i].span_id]=$i+1}
    foreach ($s in $spans) { $s.span_id=$ids[[long]$s.span_id]; if ($s.parent_span_id) {$s.parent_span_id=$ids[[long]$s.parent_span_id]} }
    $spanMap=@{}; foreach ($s in $spans) {$spanMap[[long]$s.span_id]=$s}
    $targets=@{}; foreach ($t in @($records|Where-Object record_kind -eq native_target_identity)) {$targets[$t.role]=$t}
    $validations=[Collections.Generic.List[object]]::new()
    foreach ($s in @($spans|Where-Object {$_.stage -in @('full_validation','consent_bound_validation')})) {
        $fast=$s.stage -eq 'consent_bound_validation'
        $phase=if ($baseQ[[long]$s.quantum_id].contains_leader_end) {'end'} elseif ($s.quantum_id -eq 1) {'start'} elseif ($Ctrl) {'active'} else {'setup'}
        $role=if ($spanMap[[long]$s.parent_span_id].stage -eq 'pair_witness') {'leader'} else {$s.role}
        $t=$targets[$role]
        [void]$validations.Add([pscustomobject]@{
            record_kind='validation';span_id=$s.span_id;quantum_id=$s.quantum_id;operation_generation=$s.operation_generation;source_receipt=$s.source_receipt
            target_native_key=$t.native_key;target_role=$role;capability_generation=$t.capability_generation;consent_generation=$t.consent_generation
            validation_phase=$phase;validation_mode=$(if ($fast) {'CONSENT_BOUND_FRAME_FAST'} else {'FULL_GLOBAL_INVENTORY'})
            legal_reason=$(if ($fast) {'accepted_ctrl_start_private_frame_pair_canonical_anchor'} else {'full_boundary_validation'})
            global_inventory_calls=$(if ($fast) {0} else {1});succeeded=$true;invalidation_reason='none'
        })
    }
    foreach ($phase in @('setup','restore')) {
        foreach ($role in @('leader','follower')) {
            $t=$targets[$role]
            [void]$validations.Add([pscustomobject]@{
                record_kind='validation';span_id=0;quantum_id=0;operation_generation=0;source_receipt=0
                target_native_key=$t.native_key;target_role=$role;capability_generation=$t.capability_generation;consent_generation=$t.consent_generation
                validation_phase=$phase;validation_mode='FULL_GLOBAL_INVENTORY';legal_reason='full_boundary_validation'
                global_inventory_calls=1;succeeded=$true;invalidation_reason='none'
            })
        }
    }
    $vstatus=[pscustomobject]@{record_kind='validation_status';validation_record_count=$validations.Count;validation_record_capacity=8192;validation_overflow=$false;active_native_after_invalidation=0}
    foreach ($phase in @('setup','start','active','end','restore','invalidation')) {
        $total=0; foreach ($v in @($validations|Where-Object validation_phase -eq $phase)) {$total+=$v.global_inventory_calls}
        Add-Fields $vstatus @{('global_inventory_calls_'+$phase)=$total}
    }
    $status=$records|Where-Object record_kind -eq profile_status
    $status.span_count=$spans.Count
    $status.profile_qpc_reads=2*$spans.Count+3*$status.quantum_count+$status.notification_count+@($records|Where-Object {$_.record_kind -eq 'profile_notification' -and $_.dispatch_observed}).Count
    $startup=$records|Where-Object record_kind -eq startup
    Add-Fields $startup @{build_configuration='Debug';consent_bound_contract='r1c3b_frame_authority_v1'}
    $before=@($records|Where-Object {$_.record_kind -notin @('profile_span','summary','shutdown')})
    $after=@($records|Where-Object {$_.record_kind -in @('summary','shutdown')})
    $all=@(Set-FixtureSequence ($before+$spans+$validations.ToArray()+@($vstatus)+$after))
    foreach ($r in $all) {Add-Fields $r @{schema_name='panebind.r1c3b2.explorer_glue_profile';schema_version=1}}
    return $all
}

function Test-FrameDirect {
    param([string] $Name,[object[]] $Records,[bool] $Pass=$true)
    $actual=$false; $reason=''
    try {
        $out=@(Assert-StageProfileEvidence $Records ($Records|Where-Object record_kind -eq startup) ($Records|Where-Object record_kind -eq summary) ($Records|Where-Object record_kind -eq facts) -Phase2)
        $out+=@(Assert-ConsentBoundProfileEvidence $Records)
        $actual=$true
    } catch {$reason=$_.Exception.Message}
    if ($actual -ne $Pass) {throw "$Name unexpected result: $reason"}
    ++$script:frameFixtureCount
    Write-Output "$Name frame-profile fixture: PASS"
}

if ($frameDefinitionsOnly) {return}
[void](New-Item -ItemType Directory -Path $fixtureRoot)
try {
    foreach ($mode in @('Normal','Delayed','Native','Postverify')) {
        $r=@(New-FrameProfileRecords -Delayed:($mode -eq 'Delayed') -Arrival:$(if ($mode -in @('Native','Postverify')) {$mode} else {'None'}))
        Test-FrameDirect $mode $r
        Test-CtrlFixture "FRAME_$mode" $r
        ++$script:frameFixtureCount
    }
    $mutations=[ordered]@{
        MissingStatus={param($r) ($r|Where-Object record_kind -eq validation_status).record_kind='missing'}
        Count={param($r) ($r|Where-Object record_kind -eq validation_status).validation_record_count++}
        Capacity={param($r) ($r|Where-Object record_kind -eq validation_status).validation_record_capacity=99}
        Overflow={param($r) ($r|Where-Object record_kind -eq validation_status).validation_overflow=$true}
        AfterInvalidation={param($r) ($r|Where-Object record_kind -eq validation_status).active_native_after_invalidation=1}
        ActiveCalls={param($r) ($r|Where-Object record_kind -eq validation_status).global_inventory_calls_active=1}
        RestoreCalls={param($r) ($r|Where-Object record_kind -eq validation_status).global_inventory_calls_restore=0}
        UnknownMode={param($r) @($r|Where-Object { $_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active' })[0].validation_mode='TIME_CACHE'}
        FalseReason={param($r) @($r|Where-Object { $_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active' })[0].legal_reason='hwnd_exists'}
        HidePhase={param($r) @($r|Where-Object { $_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active' })[0].validation_phase='setup'}
        HideSpan={param($r) @($r|Where-Object { $_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active' })[0].span_id=0}
        InvocationCount={param($r) @($r|Where-Object { $_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active' })[0].global_inventory_calls=1}
        DifferentFrame={param($r) @($r|Where-Object { $_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active' })[0].target_native_key=999}
        Generation={param($r) @($r|Where-Object { $_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active' })[0].capability_generation++}
        Consent={param($r) @($r|Where-Object { $_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active' })[0].consent_generation++}
        LeaderMissing={param($r) @($r|Where-Object { $_.record_kind -eq 'validation' -and $_.operation_generation -gt 0 -and $_.target_role -eq 'leader' })[0].record_kind='missing'}
        Navigation={param($r) @($r|Where-Object { $_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active' })[0].invalidation_reason='browser_navigation_changed'}
        Quit={param($r) @($r|Where-Object { $_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active' })[0].invalidation_reason='browser_quit'}
        Rehost={param($r) @($r|Where-Object { $_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active' })[0].invalidation_reason='hwnd_changed'}
        Location={param($r) @($r|Where-Object { $_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active' })[0].invalidation_reason='location_changed'}
        Stream={param($r) @($r|Where-Object { $_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active' })[0].invalidation_reason='browser_stream_invalid'}
        MissingLocationStage={param($r) @($r|Where-Object { $_.record_kind -eq 'profile_span' -and $_.stage -eq 'shell_location' })[0].stage='token_ledger'}
        MissingPostverify={param($r) @($r|Where-Object { $_.record_kind -eq 'profile_span' -and $_.stage -eq 'postverify_validation' })[0].stage='operation_prepare'}
        DuplicateValidation={param($r) $v=@($r|Where-Object { $_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active' }); $v[1].span_id=$v[0].span_id}
        UnaccountedInventory={param($r) @($r|Where-Object { $_.record_kind -eq 'profile_span' -and $_.stage -eq 'positioning_plan' })[0].stage='shell_inventory'}
        ReleaseComparison={param($r) ($r|Where-Object record_kind -eq startup).build_configuration='Release'}
    }
    foreach ($name in $mutations.Keys) {$r=@(New-FrameProfileRecords); & $mutations[$name] $r; Test-FrameDirect $name $r $false}
    $r=@(New-FrameProfileRecords); ($r|Where-Object record_kind -eq validation_status).global_inventory_calls_active=1
    Test-CtrlFixture 'FRAME_INVALID_ACTIVE_E2E' $r 0 1 'INVALID_EVIDENCE'; ++$script:frameFixtureCount
    $r=@(New-FrameProfileRecords)
    ($r|Where-Object record_kind -eq summary).user_preexisting_windows_touched=$true
    Test-CtrlFixture 'FRAME_SAFETY_E2E' $r 0 1 'INVALID_EVIDENCE'; ++$script:frameFixtureCount
    $r=@(New-FrameProfileRecords -Ctrl:$false)
    Test-FrameDirect 'NoCtrl' $r
    $observer=@(New-ObserverRecords -ActivePairCount 1|Where-Object {$_.record_kind -ne 'event' -or $_.native_window_id -ne '0x0000000000000020'})
    for($i=0;$i -lt $observer.Count;++$i){$observer[$i].observer_sequence=$i+1}
    Test-CtrlFixture 'FRAME_NO_CTRL_E2E' $r 2 2 'SAFE_BLOCKED / CTRL_NOT_DOWN_AT_START' $observer; ++$script:frameFixtureCount
    ($r|Where-Object record_kind -eq activation_attempt).ctrl_down_at_owner_processing=$true
    Test-CtrlFixture 'FRAME_LATE_CTRL_E2E' $r 2 2 'SAFE_BLOCKED / CTRL_NOT_DOWN_AT_START' $observer; ++$script:frameFixtureCount
    $m=Get-Phase2ComparisonMetrics (New-FrameProfileRecords) -Phase2
    if ($m.shell_inventory_active_calls -ne 0 -or $m.shell_inventory_active_exclusive_ms -ne 0 -or $m.follower_apply_count -ne 2) {throw 'Count comparison fixture failed'}
    ++$script:frameFixtureCount
    $r=@(New-FrameProfileRecords)
    $metricsOutput=@(Assert-ConsentBoundProfileEvidence $r)
    if (@($metricsOutput|Where-Object {$_ -match '^LARGEST_STEADY_ACTIVE shell_inventory: 0/2'}).Count -ne 1) {throw 'Missing steady-active hotspot output'}
    ++$script:frameFixtureCount
    Write-Output "R1-C3B frame profile runner fixtures: PASS; count=$script:frameFixtureCount"
} finally {
    $resolved=[IO.Path]::GetFullPath($fixtureRoot)
    if (-not $resolved.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase) -or $resolved -eq $tempBase) {throw 'Unsafe fixture cleanup path'}
    if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
