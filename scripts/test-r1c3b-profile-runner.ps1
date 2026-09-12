[CmdletBinding()]
param([switch] $DefinitionsOnly)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$c3bDefinitionsOnly = $DefinitionsOnly
. (Join-Path $PSScriptRoot 'test-r1c3a-evidence-runner.ps1') -DefinitionsOnly
. (Join-Path $PSScriptRoot 'r1c3b-profile-validation.ps1')
# Import only the two pure assertion definitions used by the profile validator;
# never evaluate the shared runner's process-launching top-level program.
$assertTokens=$null; $assertErrors=$null
$assertAst=[Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $PSScriptRoot 'run-r1c2b-explorer-glue-evidence.ps1'),[ref]$assertTokens,[ref]$assertErrors)
if ($assertErrors.Count) { throw 'Shared runner parse failed.' }
foreach ($definition in $assertAst.FindAll({
    param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -in @('Assert-UniqueRecord','Assert-RecordProperties')
},$false)) { . ([scriptblock]::Create($definition.Extent.Text)) }
$runner = Join-Path $PSScriptRoot 'run-r1c3b-smoothness-profile.ps1'
$fixtureRoot = Join-Path $tempBase ('panebind-r1c3b-runner-' + [Guid]::NewGuid().ToString('N'))
if (-not $c3bDefinitionsOnly) { [void](New-Item -ItemType Directory -Path $fixtureRoot) }

function Get-FixtureWatermark {
    param([long] $Tick)
    $seen = @($script:c3bRaw | Where-Object { $_.callback_qpc -le $Tick })
    if ($seen.Count) { return [long]$seen[-1].receipt_sequence }
    return 0
}
function Add-ProfileSpan {
    param([string] $Stage, [long] $Begin, [long] $End, [long] $Quantum,
          [long] $Parent=0, [long] $Generation=0, [long] $Source=0, [string] $Role='leader')
    $span = [pscustomobject]@{
        record_kind='profile_span'; span_id=$script:c3bSpans.Count+1; parent_span_id=$Parent
        stage=$Stage; quantum_id=$Quantum; operation_generation=$Generation; source_receipt=$Source; role=$Role
        begin_qpc=$Begin; end_qpc=$End
        begin_receipt_watermark=(Get-FixtureWatermark $Begin); end_receipt_watermark=(Get-FixtureWatermark $End)
    }
    [void]$script:c3bSpans.Add($span)
    return $span
}
function Add-ProfileCapture {
    param([string] $Stage,[long] $Begin,[long] $End,[long] $Quantum,[long] $Parent,
          [long] $Generation,[long] $Source,[string] $Role)
    $capture=Add-ProfileSpan $Stage $Begin $End $Quantum $Parent $Generation $Source $Role
    $full=Add-ProfileSpan 'full_validation' ($Begin+1) ($End-1) $Quantum $capture.span_id $Generation $Source $Role
    $parts=@('token_ledger','shell_observation','shell_inventory','shell_location','native_identity',
        'process_image','window_structure','window_state','virtual_desktop','process_security',
        'positioning_bounds','visible_frame','monitor_dpi','eligibility_finalize')
    $width=[long][Math]::Floor(($End-$Begin-4)/$parts.Count)
    for ($i=0;$i -lt $parts.Count;++$i) {
        $at=$Begin+2+$i*$width
        [void](Add-ProfileSpan $parts[$i] $at ($at+$width-1) $Quantum $full.span_id $Generation $Source $Role)
    }
}
function New-ProfileHarnessRecords {
    param([bool] $CallbackDown=$true, [switch] $UnobservedDispatch,
          [switch] $DelayedDispatch, [ValidateSet('None','Native','Postverify')] [string] $Arrival='None',
          [switch] $PostEndCallback)
    $records=@(New-CtrlHarnessRecords -CallbackDown:$CallbackDown -OwnerDown:$CallbackDown -LeftDown:$CallbackDown -PostEndCallback:$PostEndCallback)
    $script:c3bRaw=@($records | Where-Object record_kind -eq 'event_receipt')
    $baseQuanta=@($records | Where-Object record_kind -eq 'processing_quantum')
    $ops=@($records | Where-Object { $_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower' })
    if ($Arrival -ne 'None') {
        $script:c3bRaw[3].callback_qpc = if ($Arrival -eq 'Native') { 240500 } else { 243000 }
        $ops[0].post_native_receipt_watermark=4
    }
    $script:c3bSpans=[Collections.Generic.List[object]]::new()
    $profileQuanta=[Collections.Generic.List[object]]::new()
    $notifications=[Collections.Generic.List[object]]::new()
    $high=0
    $attempt=$records | Where-Object record_kind -eq 'activation_attempt'
    $attempt.owner_processing_qpc=113520
    $attempt.decision_qpc=113600
    $baseQuanta[0].behavior_decision_qpc=113650
    foreach ($q in $baseQuanta) {
        $id=[long]$q.processing_quantum_id; $origin=$id*100000
        $members=@($script:c3bRaw | Where-Object processing_quantum_id -eq $id)
        $nid=$notifications.Count+1
        foreach ($r in $members) {
            Add-Fields $r @{notification_id=$nid;notification_inherited=($r.receipt_sequence -ne $members[0].receipt_sequence)}
        }
        $observed=-not ($UnobservedDispatch -or $q.inactive_discard)
        $dispatch=if (-not $observed) {0} elseif ($DelayedDispatch) {$origin+90000} else {$origin+8000}
        [void]$notifications.Add([pscustomobject]@{
            record_kind='profile_notification'; notification_id=$nid; trigger_receipt=$members[0].receipt_sequence
            callback_qpc=$members[0].callback_qpc; owner_notification_qpc=$members[0].callback_qpc+1
            owner_message_dispatch_qpc=$dispatch; post_succeeded=$true; dispatch_observed=$observed
        })
        if ($q.inactive_discard) { continue }
        $high=[Math]::Max($high,$members.Count)
        $source=if ($q.selected_leader_sequence) {$q.selected_leader_sequence} else {$q.first_receipt_sequence}
        $root=Add-ProfileSpan 'quantum' ($origin+9100) ($origin+79900) $id
        [void](Add-ProfileSpan 'drain' ($origin+10000) ($origin+10900) $id $root.span_id)
        Add-ProfileCapture 'leader_capture' ($origin+12000) ($origin+12400) $id $root.span_id 0 $source 'leader'
        Add-ProfileCapture 'follower_capture' ($origin+12500) ($origin+12900) $id $root.span_id 0 $source 'follower'
        $eventRole=$members[-1].role
        $policy=Add-ProfileSpan 'event_policy' ($origin+13500) ($origin+70000) $id $root.span_id 0 $source $eventRole
        if ($id -eq 1) {
            [void](Add-ProfileSpan 'activation_policy' ($origin+13510) ($origin+13700) $id $policy.span_id 0 $source 'leader')
            if ($CallbackDown) { [void](Add-ProfileSpan 'start_geometry' ($origin+13800) ($origin+13900) $id $policy.span_id 0 $source 'leader') }
        }
        if ($CallbackDown) {
            if ($eventRole -eq 'follower') {
                $feedback=Add-ProfileSpan 'feedback_policy' ($origin+14000) ($origin+16000) $id $policy.span_id 0 $source $eventRole
                [void](Add-ProfileSpan 'core_decision' ($origin+14050) ($origin+14900) $id $feedback.span_id 0 $source $eventRole)
            } else {
                [void](Add-ProfileSpan 'core_decision' ($origin+14000) ($origin+14900) $id $policy.span_id 0 $source $eventRole)
            }
        }
        foreach ($op in @($ops | Where-Object processing_quantum_id -eq $id)) {
            $gen=$op.behavior_operation_generation; $src=$op.source_leader_sequence
            $operation=Add-ProfileSpan 'operation' ($origin+16000) ($origin+46000) $id $policy.span_id $gen $src 'follower'
            [void](Add-ProfileSpan 'operation_prepare' ($origin+16010) ($origin+16900) $id $operation.span_id $gen $src 'follower')
            Add-ProfileCapture 'prepare_validation' ($origin+17000) ($origin+27000) $id $operation.span_id $gen $src 'follower'
            [void](Add-ProfileSpan 'positioning_plan' ($origin+27500) ($origin+28000) $id $operation.span_id $gen $src 'follower')
            Add-ProfileCapture 'immediate_validation' ($origin+28500) ($origin+39000) $id $operation.span_id $gen $src 'follower'
            [void](Add-ProfileSpan 'pending_registration' ($origin+39500) ($origin+39900) $id $operation.span_id $gen $src 'follower')
            [void](Add-ProfileSpan 'native_placement' ($origin+40100) ($origin+40900) $id $operation.span_id $gen $src 'follower')
            $post=Add-ProfileSpan 'postverify' ($origin+41100) ($origin+44900) $id $operation.span_id $gen $src 'follower'
            Add-ProfileCapture 'postverify_validation' ($origin+41200) ($origin+44000) $id $post.span_id $gen $src 'follower'
            [void](Add-ProfileSpan 'exact_comparison' ($origin+44100) ($origin+44600) $id $post.span_id $gen $src 'follower')
            [void](Add-ProfileSpan 'receipt_finalize' ($origin+44700) ($origin+44800) $id $post.span_id $gen $src 'follower')
            [void](Add-ProfileSpan 'receipt_finalize' ($origin+45100) ($origin+45900) $id $operation.span_id $gen $src 'follower')
            [void](Add-ProfileSpan 'operation_result_policy' ($origin+47000) ($origin+48000) $id $policy.span_id $gen $src 'follower')
        }
        if ($q.contains_leader_end -and $CallbackDown) {
            Add-ProfileCapture 'end_leader_capture' ($origin+20000) ($origin+30000) $id $policy.span_id 0 $source 'leader'
            Add-ProfileCapture 'end_follower_capture' ($origin+31000) ($origin+41000) $id $policy.span_id 0 $source 'follower'
            [void](Add-ProfileSpan 'end_reconciliation' ($origin+42000) ($origin+43000) $id $policy.span_id 0 $source 'leader')
        }
        $beginWatermark=Get-FixtureWatermark ($origin+9000); $endWatermark=Get-FixtureWatermark ($origin+80000)
        $nativeArrivals=0; $postArrivals=0
        foreach ($s in @($script:c3bSpans | Where-Object quantum_id -eq $id)) {
            if ($s.stage -eq 'native_placement') {$nativeArrivals += $s.end_receipt_watermark-$s.begin_receipt_watermark}
            if ($s.stage -eq 'postverify') {$postArrivals += $s.end_receipt_watermark-$s.begin_receipt_watermark}
        }
        $previous=if ($profileQuanta.Count) {$profileQuanta[$profileQuanta.Count-1]} else {$null}
        [void]$profileQuanta.Add([pscustomobject]@{
            record_kind='profile_quantum'; quantum_id=$id; first_receipt_sequence=$q.first_receipt_sequence
            last_receipt_sequence=$q.last_receipt_sequence; raw_receipt_count=$q.receipt_count
            leader_location_count=$q.leader_location_count; follower_location_count=$q.follower_location_count
            coalesced_leader_location_count=@($members | Where-Object coalesced).Count
            first_callback_receipt_qpc=$members[0].callback_qpc; quantum_start_qpc=$origin+9000
            drain_complete_qpc=$origin+11000; quantum_end_qpc=$origin+80000
            queue_depth_before_drain=$q.receipt_count; queue_depth_after_drain=0; queue_depth_at_end=$endWatermark-$beginWatermark
            max_queue_depth=$high; begin_receipt_watermark=$beginWatermark; end_receipt_watermark=$endWatermark
            receipts_arrived_during_quantum=$endWatermark-$beginWatermark
            receipts_arrived_during_native=$nativeArrivals; receipts_arrived_during_postverify=$postArrivals
            previous_quantum_id=$(if ($null -eq $previous) {0} else {$previous.quantum_id})
            receipts_arrived_during_previous_quantum=$(if ($null -eq $previous) {0} else {$previous.receipts_arrived_during_quantum})
            receipts_arrived_during_previous_native_operation=$(if ($null -eq $previous) {0} else {$previous.receipts_arrived_during_native})
            receipts_arrived_during_previous_postverify=$(if ($null -eq $previous) {0} else {$previous.receipts_arrived_during_postverify})
        })
    }
    $startup=$records|Where-Object record_kind -eq startup
    Add-Fields $startup @{profiling_enabled=$true;external_observer_enabled=$true}
    $facts=$records|Where-Object record_kind -eq facts; $summary=$records|Where-Object record_kind -eq summary
    $facts.max_event_queue_depth=$high; $summary.max_event_queue_depth=$high
    Add-Fields $summary @{timing_profile_gate='PASS'}
    $status=[pscustomobject]@{
        record_kind='profile_status';profiling_enabled=$true;external_observer_enabled=$true
        glue_session_generation=$facts.glue_session_generation;activation_generation=$facts.activation_generation
        span_capacity=32768;quantum_capacity=4096;notification_capacity=4096
        span_count=$script:c3bSpans.Count;quantum_count=$profileQuanta.Count;notification_count=$notifications.Count
        profile_qpc_reads=2*$script:c3bSpans.Count+3*$profileQuanta.Count+$notifications.Count+@($notifications|Where-Object dispatch_observed).Count
        profile_overflow=$false;profile_invalid=$false;timing_profile_gate='PASS'
    }
    $before=@($records|Where-Object {$_.record_kind -notin @('feedback_reconciliation','facts','summary','shutdown')})
    $after=@($records|Where-Object {$_.record_kind -in @('feedback_reconciliation','facts','summary','shutdown')})
    $result=@(Set-FixtureSequence ($before+$script:c3bSpans.ToArray()+$profileQuanta.ToArray()+$notifications.ToArray()+@($status)+$after))
    foreach ($r in $result) { Add-Fields $r @{schema_version=1;schema_name='panebind.r1c3b.explorer_glue_profile'} }
    return $result
}
function Test-ProfileDirect {
    param([string] $Name,[object[]] $Records,[bool] $ShouldPass)
    $passed=$false; $reason=''
    try {
        $output=@(Assert-StageProfileEvidence $Records ($Records|Where-Object record_kind -eq startup) ($Records|Where-Object record_kind -eq summary) ($Records|Where-Object record_kind -eq facts))
        $passed=$true
    } catch { $reason=$_.Exception.Message }
    if ($passed -ne $ShouldPass) { throw ("{0}: unexpected profile result; {1}" -f $Name,$reason) }
    if ($ShouldPass -and @($output|Where-Object {$_ -match '^TOP OBSERVED HOT STAGES'}).Count -ne 1) {throw 'Missing data-driven hotspot output.'}
    if ($ShouldPass) {
        if (@($output|Where-Object {$_ -match '^OP_PROFILE .*total_ticks=30000 '}).Count -ne 2 -or
            @($output|Where-Object {$_ -match '^PROFILE leader_capture .*count=5; p50=0[.]4;'}).Count -ne 1) {
            throw 'Per-operation partition or capture percentile result is incorrect.'
        }
    }
    Write-Output "$Name profile fixture: PASS"
}
if ($c3bDefinitionsOnly) { return }
try {
    foreach ($mode in @('Normal','Unobserved','Delayed','Native','Postverify','Tail')) {
        $r=@(New-ProfileHarnessRecords -UnobservedDispatch:($mode -eq 'Unobserved') -DelayedDispatch:($mode -eq 'Delayed') -Arrival:$(if ($mode -in @('Native','Postverify')) {$mode} else {'None'}) -PostEndCallback:($mode -eq 'Tail'))
        Test-CtrlFixture "PROFILE_$mode" $r
    }
    $mutations=[ordered]@{
        'MissingStatus'={param($r) ($r|Where-Object record_kind -eq profile_status).record_kind='missing'}
        'Overflow'={param($r) ($r|Where-Object record_kind -eq profile_status).profile_overflow=$true}
        'InvalidClock'={param($r) ($r|Where-Object record_kind -eq profile_status).profile_invalid=$true}
        'BadCapacity'={param($r) ($r|Where-Object record_kind -eq profile_status).span_capacity=999999}
        'BadCount'={param($r) ($r|Where-Object record_kind -eq profile_status).span_count=1}
        'WrongSession'={param($r) ($r|Where-Object record_kind -eq profile_status).glue_session_generation=99}
        'ObserverOff'={param($r) ($r|Where-Object record_kind -eq startup).external_observer_enabled=$false}
        'ProfileOff'={param($r) ($r|Where-Object record_kind -eq startup).profiling_enabled=$false}
        'CounterForged'={param($r) ($r|Where-Object record_kind -eq profile_status).profile_qpc_reads=1}
        'DuplicateSpan'={param($r) @($r|Where-Object record_kind -eq profile_span)[1].span_id=1}
        'UnknownStage'={param($r) @($r|Where-Object record_kind -eq profile_span)[1].stage='cache_everything'}
        'WrongQuantum'={param($r) @($r|Where-Object record_kind -eq profile_span)[1].quantum_id=999}
        'ParentCycle'={param($r) @($r|Where-Object record_kind -eq profile_span)[1].parent_span_id=2}
        'MissingPoint'={param($r) @($r|Where-Object record_kind -eq profile_span)[1].PSObject.Properties.Remove('begin_qpc')}
        'NegativePoint'={param($r) @($r|Where-Object record_kind -eq profile_span)[1].begin_qpc=-1}
        'FractionalPoint'={param($r) @($r|Where-Object record_kind -eq profile_span)[1].begin_qpc=1.5}
        'ReverseSpan'={param($r) @($r|Where-Object record_kind -eq profile_span)[1].end_qpc=1}
        'Overlap'={param($r) @($r|Where-Object {$_.record_kind -eq 'profile_span' -and $_.stage -eq 'follower_capture'})[0].begin_qpc=112100}
        'MissingFullCapture'={param($r) @($r|Where-Object {$_.record_kind -eq 'profile_span' -and $_.stage -eq 'full_validation'})[0].stage='event_policy'}
        'RemovedShell'={param($r) @($r|Where-Object {$_.record_kind -eq 'profile_span' -and $_.stage -eq 'shell_inventory'})[0].stage='process_security'}
        'OpGeneration'={param($r) @($r|Where-Object {$_.record_kind -eq 'profile_span' -and $_.stage -eq 'native_placement'})[0].operation_generation=99}
        'OpSource'={param($r) @($r|Where-Object {$_.record_kind -eq 'profile_span' -and $_.stage -eq 'native_placement'})[0].source_receipt=1}
        'OpNativeOrder'={param($r) @($r|Where-Object {$_.record_kind -eq 'profile_span' -and $_.stage -eq 'native_placement'})[0].begin_qpc=239000}
        'QueueBefore'={param($r) @($r|Where-Object record_kind -eq profile_quantum)[0].queue_depth_before_drain=99}
        'QueueAfter'={param($r) @($r|Where-Object record_kind -eq profile_quantum)[0].queue_depth_after_drain=1}
        'QueueEnd'={param($r) @($r|Where-Object record_kind -eq profile_quantum)[0].queue_depth_at_end=1}
        'Coalesced'={param($r) @($r|Where-Object record_kind -eq profile_quantum)[1].coalesced_leader_location_count=99}
        'Backlog'={param($r) @($r|Where-Object record_kind -eq profile_quantum)[1].receipts_arrived_during_previous_quantum=99}
        'NativeBacklog'={param($r) @($r|Where-Object record_kind -eq profile_quantum)[1].receipts_arrived_during_native=99}
        'MissingNotification'={param($r) @($r|Where-Object record_kind -eq profile_notification)[0].notification_id=99}
        'NotificationTrigger'={param($r) @($r|Where-Object record_kind -eq profile_notification)[0].trigger_receipt=2}
        'FakeDispatch'={param($r) @($r|Where-Object record_kind -eq profile_notification)[0].dispatch_observed=$false}
        'DispatchOrder'={param($r) @($r|Where-Object record_kind -eq profile_notification)[0].owner_message_dispatch_qpc=1}
        'PostFailure'={param($r) @($r|Where-Object record_kind -eq profile_notification)[0].post_succeeded=$false}
        'InheritedLie'={param($r) @($r|Where-Object record_kind -eq event_receipt)[0].notification_inherited=$true}
    }
    foreach ($name in $mutations.Keys) {
        $r=@(New-ProfileHarnessRecords); & $mutations[$name] $r
        Test-ProfileDirect $name $r $false
    }
    $r=@(New-ProfileHarnessRecords)
    ($r|Where-Object record_kind -eq summary).user_preexisting_windows_touched=$true
    Test-CtrlFixture 'PROFILE_SAFETY_NOT_WEAKENED' $r 0 1 'INVALID_EVIDENCE'
    $r=@(New-ProfileHarnessRecords)
    ($r|Where-Object record_kind -eq profile_status).profile_overflow=$true
    Test-CtrlFixture 'PROFILE_OVERFLOW_END_TO_END' $r 0 1 'TIMING_PROFILE_GATE: FAIL'
    Test-ProfileDirect 'ValidLargestAndExclusivePartition' (New-ProfileHarnessRecords) $true
    $r=@(New-ProfileHarnessRecords)
    $capture=@($r|Where-Object {$_.record_kind -eq 'profile_span' -and $_.stage -eq 'prepare_validation'})[0]
    $full=@($r|Where-Object {$_.record_kind -eq 'profile_span' -and $_.parent_span_id -eq $capture.span_id})[0]
    $cursor=[long]$full.begin_qpc+1
    foreach ($child in @($r|Where-Object {$_.record_kind -eq 'profile_span' -and $_.parent_span_id -eq $full.span_id})) {
        $width=if ($child.stage -eq 'process_image') {5000} else {100}
        $child.begin_qpc=$cursor; $child.end_qpc=$cursor+$width; $cursor += $width+1
    }
    $profileOutput=@(Assert-StageProfileEvidence $r ($r|Where-Object record_kind -eq startup) ($r|Where-Object record_kind -eq summary) ($r|Where-Object record_kind -eq facts))
    if (@($profileOutput|Where-Object {$_ -match '^LARGEST process_image: 1/2 operations$'}).Count -ne 1) {
        throw 'Largest-stage classification is hard-coded or ignores changed matched-operation evidence.'
    }
    Write-Output 'DataDrivenLargestStage profile fixture: PASS'
    $observer=@(New-ObserverRecords -ActivePairCount 1 | Where-Object {
        $_.record_kind -ne 'event' -or $_.native_window_id -ne '0x0000000000000020'
    })
    for ($i=0;$i -lt $observer.Count;++$i) {$observer[$i].observer_sequence=$i+1}
    $r=@(New-ProfileHarnessRecords -CallbackDown:$false)
    Test-CtrlFixture 'PROFILE_NO_CTRL' $r 2 2 'SAFE_BLOCKED / CTRL_NOT_DOWN_AT_START' $observer
    ($r|Where-Object record_kind -eq activation_attempt).ctrl_down_at_owner_processing=$true
    Test-CtrlFixture 'PROFILE_LATE_CTRL' $r 2 2 'SAFE_BLOCKED / CTRL_NOT_DOWN_AT_START' $observer
    Write-Output 'R1-C3B profile runner fixtures: PASS'
} finally {
    $resolvedFixtureRoot=[IO.Path]::GetFullPath($fixtureRoot)
    if (-not $resolvedFixtureRoot.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase) -or $resolvedFixtureRoot -eq $tempBase) {
        throw 'Refusing cleanup outside validated fixture directory.'
    }
    if (Test-Path -LiteralPath $resolvedFixtureRoot) { Remove-Item -LiteralPath $resolvedFixtureRoot -Recurse -Force }
}
