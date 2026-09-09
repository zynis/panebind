[CmdletBinding()]
param([switch] $DefinitionsOnly)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$c3aDefinitionsOnly = $DefinitionsOnly
# Reuse deterministic data builders, never historical human evidence files.
. (Join-Path $PSScriptRoot 'test-r1c2b-evidence-runner.ps1') -DefinitionsOnly
. (Join-Path $PSScriptRoot 'r1c3a-evidence-validation.ps1')
$runner = Join-Path $PSScriptRoot 'run-r1c3a-explorer-ctrl-glue-evidence.ps1'
$fixtureRoot = Join-Path $tempBase ('panebind-r1c3a-runner-' + [Guid]::NewGuid().ToString('N'))
if (-not $c3aDefinitionsOnly) { [void] (New-Item -ItemType Directory -Path $fixtureRoot) }

function Add-Fields {
    param([object] $Record, [hashtable] $Fields)
    $Record | Add-Member -Force -NotePropertyMembers $Fields
}

function New-CtrlHarnessRecords {
    param([bool] $CallbackDown = $true, [bool] $OwnerDown = $true,
          [bool] $LeftDown = $true, [bool] $RightDown = $false,
          [switch] $NoSetup, [switch] $NoRestore, [switch] $SingleApply,
          [switch] $PostEndCallback)
    $records = @(New-PassHarnessRecords -NoSetup:$NoSetup -NoRestore:$NoRestore `
        -MissingFeedback:(!$CallbackDown) -SingleApply:$SingleApply -PostEndCallback:$PostEndCallback)
    $startup = @($records | Where-Object { $_.record_kind -eq 'startup' })[0]
    Add-Fields $startup @{
        activation_input_source = 'ctrl_move_start'; glue_console_confirmation_required = $false
        timing_clock = 'query_performance_counter'; qpc_frequency_hz = 1000000
        timing_capacity = 4096; activation_capacity = 8; operation_capacity = 512
        timing_evidence_overflow = $false; activation_evidence_overflow = $false
    }
    $facts = @($records | Where-Object { $_.record_kind -eq 'facts' })[0]
    $summary = @($records | Where-Object { $_.record_kind -eq 'summary' })[0]
    Add-Fields $facts @{
        glue_consent_confirmed = $false; fixture_pair_authorized = $true
        timing_evidence_overflow = $false; activation_evidence_overflow = $false; timing_valid = $true
        ctrl_move_activation_required = $true; qpc_frequency_hz = 1000000
        activation_generation = [int]$CallbackDown; pair_authority_generation = 9
    }
    Add-Fields $summary @{
        activation_count = [int]$CallbackDown; activation_attempt_count = 1
        ctrl_activation_evidence_gate = $(if ($CallbackDown) { 'PASS' } else { 'CTRL_NOT_DOWN_AT_START' })
        timing_evidence_overflow = $false; activation_evidence_overflow = $false
        timing_valid = $true; activation_generation = [int]$CallbackDown
        activation_input_source = 'ctrl_move_start'; glue_console_confirmation_required = $false
        keyboard_content_collection = $false
    }
    $authority = @($records | Where-Object { $_.record_kind -eq 'glue_authority' })[0]
    $authority.record_kind = 'pair_authority'
    foreach ($name in @('prompt_generation', 'confirmation_generation', 'authority_generation')) {
        $authority.PSObject.Properties.Remove($name)
    }
    Add-Fields $authority @{
        result = 'PASS'; input_source = 'target_consent_fixture'
        activation_input_source = 'ctrl_move_start'; glue_console_confirmation_required = $false
        pair_preview_generation = 6; pair_authority_generation = 9; glue_authority_id = 1
        leader_window_id = 1; follower_window_id = 2
        leader_capability_generation = 1; follower_capability_generation = 1
        leader_consent_generation = 5; follower_consent_generation = 5
    }
    $records = @($records | Where-Object {
        $_.record_kind -notin @('glue_consent_prompt', 'glue_consent_confirmation') -and
        -not ($_.record_kind -eq 'glue_step' -and $_.step -eq 'glue_consent_prompt')
    })
    $drag = @($records | Where-Object { $_.record_kind -eq 'drag_prompt' })[0]
    Add-Fields $drag @{ activation_input_source = 'ctrl_move_start' }
    foreach ($receipt in @($records | Where-Object { $_.record_kind -eq 'event_receipt' })) {
        Add-Fields $receipt @{
            callback_qpc = [long]($receipt.processing_quantum_id * 100000 + $receipt.receipt_sequence * 10)
            ctrl_sample_available = $false; ctrl_down_at_callback_delivery = $false
            left_ctrl_down = $false; right_ctrl_down = $false
        }
    }
    $start = @($records | Where-Object { $_.record_kind -eq 'event_receipt' -and $_.event_kind -eq 'move_resize_started' })[0]
    Add-Fields $start @{
        ctrl_sample_available = $true
        ctrl_down_at_callback_delivery = $CallbackDown; left_ctrl_down = $LeftDown; right_ctrl_down = $RightDown
    }
    foreach ($quantum in @($records | Where-Object { $_.record_kind -eq 'processing_quantum' })) {
        $origin = [long]($quantum.processing_quantum_id * 100000)
        Add-Fields $quantum @{
            owner_drain_start_qpc = $origin + 10000; sample_start_qpc = $origin + 12000
            sample_complete_qpc = $origin + 13000; behavior_decision_qpc = $origin + 14000
        }
        if ($quantum.inactive_discard) {
            Add-Fields $quantum @{ sample_start_qpc = 0; sample_complete_qpc = 0; behavior_decision_qpc = 0 }
        }
    }
    foreach ($operation in @($records | Where-Object { $_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower' })) {
        $origin = [long]($operation.processing_quantum_id * 100000)
        Add-Fields $operation @{
            activation_generation = 1; behavior_decision_qpc = $origin + 15000
            native_apply_start_qpc = $origin + 40000
            native_api_return_qpc = $origin + 41000; postverify_complete_qpc = $origin + 45000
        }
    }
    foreach ($trace in @($records | Where-Object { $_.record_kind -eq 'internal_trace' })) {
        Add-Fields $trace @{ ack_qpc = $(if ($trace.decision -eq 'feedback_acknowledged') {
            [long]($trace.processing_quantum_id * 100000 + 15000)
        } else { 0 }) }
    }
    $attempt = [pscustomobject]@{
        record_kind = 'activation_attempt'; attempt_generation = 1
        activation_generation = [int]$CallbackDown; pair_authority_generation = 9
        leader_start_receipt_sequence = $start.receipt_sequence
        leader_capability_generation = 1; follower_capability_generation = 1
        glue_authority_id = 1; role = 'leader'; leader_window_id = 1; follower_window_id = 2
        leader_consent_generation = 5; follower_consent_generation = 5
        ctrl_sample_available = $true; owner_ctrl_sample_available = $true
        glue_session_generation = 10; callback_qpc = $start.callback_qpc
        owner_processing_qpc = 111000; decision_qpc = 114000
        ctrl_down_at_callback_delivery = $CallbackDown; left_ctrl_down = $LeftDown
        right_ctrl_down = $RightDown; ctrl_down_at_owner_processing = $OwnerDown
        decision = $(if ($CallbackDown) { 'ACTIVATED' } else { 'NOT_ACTIVATED' })
        reason = $(if ($CallbackDown) { 'ctrl_at_leader_start' } else { 'CTRL_NOT_DOWN_AT_START' })
        activation_source = 'ctrl_at_leader_start'; input_source = 'ctrl_move_start'
    }
    if (-not $CallbackDown) {
        $trace = @($records | Where-Object { $_.record_kind -eq 'internal_trace' })[0]
        Add-Fields $trace @{
            trace_sequence = 1; event_sequence = 0; event_kind = $null; decision = 'armed'
            processing_quantum_id = 0; sampled_geometry_generation = 0
            native_event_timestamp_ms = 0; visible = $null; sampled_visible_rect = $null
        }
        $records = @($records | Where-Object {
            $_.record_kind -ne 'internal_trace' -and $_.record_kind -ne 'feedback_reconciliation' -and
            -not ($_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower')
        })
        Add-Fields $facts @{
            behavior_state = 'armed'; follower_final = $facts.follower_layout
            follower_native_apply_count = 0; suppressed_feedback_count = 0
            missing_feedback_count = 0; reconciled_feedback_count = 0; max_pending_depth = 0
        }
        $restore = @($records | Where-Object { $_.record_kind -eq 'operation' -and $_.phase -eq 'restore' -and $_.role -eq 'follower' })[0]
        $restore.before = $facts.follower_layout
        Add-Fields $summary @{
            result = 'BLOCKED'; runtime_gate = 'BLOCKED'; reason = 'CTRL_NOT_DOWN_AT_START'
            glue_reason = 'ctrl_not_down_at_start'; behavior_state = 'armed'
            leader_start_count = 0; leader_location_count = 0; leader_end_count = 0
            follower_feedback_count = 0; follower_native_apply_count = 0; active_follower_operation_count = 0
            all_active_follower_operations_exact = $false; suppressed_feedback_count = 0
            missing_feedback_count = 0; reconciled_feedback_count = 0
            acknowledged_operation_count = 0; reconciled_operation_count = 0
            feedback_operation_correlation_valid = $false; feedback_suppression_evidence = 'not_reached'
            max_pending_depth = 0; distinct_follower_target_count = 0; follower_applies_before_end_count = 0
            realtime_follow_evidence_gate = 'NOT_REACHED'
        }
        $run = @($records | Where-Object { $_.record_kind -eq 'glue_step' -and $_.step -eq 'run_until_terminal' })[0]
        Add-Fields $run @{ result = 'BLOCKED'; reason = 'ctrl_not_down_at_start' }
        $before = @($records | Where-Object { $_.record_kind -notin @('operation', 'facts', 'summary', 'shutdown') })
        $after = @($records | Where-Object { $_.record_kind -in @('operation', 'facts', 'summary', 'shutdown') })
        $records = $before + @($trace) + $after
    }
    $before = @($records | Where-Object { $_.record_kind -notin @('internal_trace', 'operation', 'feedback_reconciliation', 'facts', 'summary', 'shutdown') })
    $after = @($records | Where-Object { $_.record_kind -in @('internal_trace', 'operation', 'feedback_reconciliation', 'facts', 'summary', 'shutdown') })
    $records = $before + @($attempt) + $after
    $base = [DateTimeOffset]::Parse('2026-01-01T00:00:00Z')
    for ($index = 0; $index -lt $records.Count; ++$index) {
        Add-Fields $records[$index] @{
            schema_version = 1; schema_name = 'panebind.r1c3a.explorer_ctrl_glue'; harness_sequence = $index + 1
            recorded_at = $base.AddSeconds(($index + 1) * 10).ToString("yyyy-MM-ddTHH:mm:ss.fff'Z'")
        }
    }
    return $records
}

function Test-CtrlFixture {
    param([string] $Name, [object[]] $Records, [int] $HarnessExit = 0,
          [int] $ExpectedExit = 0, [string] $Marker = 'evidence outcome: PASS',
          [object[]] $Observer = @(New-ObserverRecords -ActivePairCount 1))
    $prefix = Write-Fixture $Name $Records $Observer
    Assert-RunnerOutcome $Name $prefix $HarnessExit $ExpectedExit $Marker
}

function Set-FixtureSequence {
    param([object[]] $Records)
    $base = [DateTimeOffset]::Parse('2026-01-01T00:00:00Z')
    for ($index = 0; $index -lt $Records.Count; ++$index) {
        Add-Fields $Records[$index] @{
            harness_sequence = $index + 1
            recorded_at = $base.AddSeconds(($index + 1) * 10).ToString("yyyy-MM-ddTHH:mm:ss.fff'Z'")
        }
    }
    return $Records
}

function New-DeferredAckHarnessRecords {
    $records = @(New-CtrlHarnessRecords)
    $feedback = @($records | Where-Object {
        $_.record_kind -eq 'internal_trace' -and $_.decision -eq 'feedback_acknowledged'
    })[0]
    $feedback.decision = 'feedback_observed_pending_result'
    $feedback.ack_qpc = 0
    $deferred = $feedback | ConvertTo-Json -Depth 12 | ConvertFrom-Json
    Add-Fields $deferred @{
        event_sequence = 0; event_kind = $null; role = 'leader'
        native_event_timestamp_ms = 0; sampled_visible_rect = $null
        decision = 'feedback_acknowledged'; ack_qpc = 318000
    }
    $operation = @($records | Where-Object {
        $_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower'
    })[0]
    # The feedback callback arrives during postvalidation; the exact native
    # receipt becomes available later. This is a pending ACK, not a new event.
    $operation.postverify_complete_qpc = 317000
    $result = [Collections.Generic.List[object]]::new()
    foreach ($record in $records) {
        [void]$result.Add($record)
        if ([object]::ReferenceEquals($record, $feedback)) { [void]$result.Add($deferred) }
    }
    $traceSequence = 0
    foreach ($record in $result) {
        if ($record.record_kind -eq 'internal_trace') { $record.trace_sequence = ++$traceSequence }
    }
    return @(Set-FixtureSequence $result.ToArray())
}

if ($c3aDefinitionsOnly) { return }
try {
    Test-CtrlFixture 'CTRL_DOWN' (New-CtrlHarnessRecords)
    Test-CtrlFixture 'CALLBACK_DOWN_OWNER_UP' (New-CtrlHarnessRecords -OwnerDown $false)
    Test-CtrlFixture 'RIGHT_CTRL' (New-CtrlHarnessRecords -LeftDown $false -RightDown $true)
    Test-CtrlFixture 'BOTH_CTRL' (New-CtrlHarnessRecords -RightDown $true)
    Test-CtrlFixture 'NOOP_SETUP' (New-CtrlHarnessRecords -NoSetup)
    Test-CtrlFixture 'NOOP_RESTORE' (New-CtrlHarnessRecords -NoRestore)
    Test-CtrlFixture 'VALID_TERMINAL_CALLBACK_TAIL' (New-CtrlHarnessRecords -PostEndCallback)
    Test-CtrlFixture 'DEFERRED_PROVISIONAL_ACK' (New-DeferredAckHarnessRecords)
    Test-CtrlFixture 'ONE_APPLY_NOT_REALTIME' (New-CtrlHarnessRecords -SingleApply) 2 2 'INSUFFICIENT_REALTIME_FOLLOW'
    $noFollowerObserver = @(New-ObserverRecords -ActivePairCount 1 | Where-Object {
        $_.record_kind -ne 'event' -or $_.native_window_id -ne '0x0000000000000020'
    })
    for ($index = 0; $index -lt $noFollowerObserver.Count; ++$index) { $noFollowerObserver[$index].observer_sequence = $index + 1 }
    Test-CtrlFixture 'CTRL_UP' (New-CtrlHarnessRecords -CallbackDown $false -OwnerDown $false -LeftDown $false) 2 2 'SAFE_BLOCKED / CTRL_NOT_DOWN_AT_START' $noFollowerObserver
    Test-CtrlFixture 'CTRL_AFTER_START' (New-CtrlHarnessRecords -CallbackDown $false -OwnerDown $true -LeftDown $false) 2 2 'SAFE_BLOCKED / CTRL_NOT_DOWN_AT_START' $noFollowerObserver

    $mutations = [ordered]@{
        'FORGED_CALLBACK' = { param($r) ($r | Where-Object record_kind -eq 'activation_attempt').ctrl_down_at_callback_delivery = $false }
        'DUPLICATE_ACTIVATION' = { param($r) ($r | Where-Object record_kind -eq 'summary').activation_count = 2 }
        'ZERO_ACTIVATION_GENERATION' = { param($r) ($r | Where-Object record_kind -eq 'activation_attempt').activation_generation = 0 }
        'WRONG_PAIR_GENERATION' = { param($r) ($r | Where-Object record_kind -eq 'activation_attempt').pair_authority_generation = 99 }
        'WRONG_TARGET_GENERATION' = { param($r) ($r | Where-Object record_kind -eq 'activation_attempt').follower_capability_generation = 99 }
        'WRONG_START_SEQUENCE' = { param($r) ($r | Where-Object record_kind -eq 'activation_attempt').leader_start_receipt_sequence = 2 }
        'WRONG_OPERATION_ACTIVATION' = { param($r) ($r | Where-Object { $_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower' } | Select-Object -First 1).activation_generation = 99 }
        'WRONG_SESSION_GENERATION' = { param($r) ($r | Where-Object record_kind -eq 'activation_attempt').glue_session_generation = 11 }
        'FAKE_CONSOLE_ACTIVATION' = { param($r) ($r | Where-Object record_kind -eq 'activation_attempt').input_source = 'interactive_console' }
        'MISSING_QPC' = { param($r) ($r | Where-Object record_kind -eq 'event_receipt' | Select-Object -First 1).PSObject.Properties.Remove('callback_qpc') }
        'NEGATIVE_QPC' = { param($r) ($r | Where-Object record_kind -eq 'event_receipt' | Select-Object -First 1).callback_qpc = -1 }
        'FRACTIONAL_QPC' = { param($r) ($r | Where-Object record_kind -eq 'event_receipt' | Select-Object -First 1).callback_qpc = 0.5 }
        'ZERO_FREQUENCY' = { param($r) ($r | Where-Object record_kind -eq 'startup').qpc_frequency_hz = 0 }
        'CALLBACK_REGRESSION' = { param($r) @($r | Where-Object record_kind -eq 'event_receipt')[1].callback_qpc = 1 }
        'DRAIN_BEFORE_RECEIPT' = { param($r) ($r | Where-Object record_kind -eq 'processing_quantum' | Select-Object -First 1).owner_drain_start_qpc = 1 }
        'SAMPLE_ORDER' = { param($r) ($r | Where-Object record_kind -eq 'processing_quantum' | Select-Object -First 1).sample_complete_qpc = 1 }
        'NATIVE_RETURN_ORDER' = { param($r) ($r | Where-Object { $_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower' } | Select-Object -First 1).native_api_return_qpc = 1 }
        'OPERATION_DECISION_ORDER' = { param($r) ($r | Where-Object { $_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower' } | Select-Object -First 1).behavior_decision_qpc = 1 }
        'POSTVERIFY_ORDER' = { param($r) ($r | Where-Object { $_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower' } | Select-Object -First 1).postverify_complete_qpc = 1 }
        'ACK_ORDER' = { param($r) ($r | Where-Object { $_.record_kind -eq 'internal_trace' -and $_.decision -eq 'feedback_acknowledged' }).ack_qpc = 1 }
        'TIMING_OVERFLOW' = { param($r) ($r | Where-Object record_kind -eq 'summary').timing_evidence_overflow = $true }
        'ACTIVATION_OVERFLOW' = { param($r) ($r | Where-Object record_kind -eq 'summary').activation_evidence_overflow = $true }
        'CAPACITY_EXCEEDED' = { param($r) ($r | Where-Object record_kind -eq 'startup').timing_capacity = 1 }
        'UNBOUNDED_CAPACITY' = { param($r) ($r | Where-Object record_kind -eq 'startup').timing_capacity = 999999 }
        'FAKE_BOOLEAN' = { param($r) ($r | Where-Object record_kind -eq 'activation_attempt').ctrl_down_at_callback_delivery = 'true' }
        'SAMPLE_UNAVAILABLE' = { param($r) ($r | Where-Object record_kind -eq 'activation_attempt').ctrl_sample_available = $false }
        'OWNER_SAMPLE_UNAVAILABLE' = { param($r) ($r | Where-Object record_kind -eq 'activation_attempt').owner_ctrl_sample_available = $false }
        'CTRL_SAMPLED_ON_LOCATION' = { param($r) @($r | Where-Object record_kind -eq 'event_receipt')[1].ctrl_sample_available = $true }
        'WRONG_ATTEMPT_ROLE' = { param($r) ($r | Where-Object record_kind -eq 'activation_attempt').role = 'follower' }
        'WRONG_CONSENT_GENERATION' = { param($r) ($r | Where-Object record_kind -eq 'activation_attempt').follower_consent_generation = 99 }
        'WRONG_AUTHORITY_ID' = { param($r) ($r | Where-Object record_kind -eq 'activation_attempt').glue_authority_id = 99 }
        'WRONG_WINDOW_ID' = { param($r) ($r | Where-Object record_kind -eq 'activation_attempt').leader_window_id = 2 }
        'WRONG_ACCEPT_REASON' = { param($r) ($r | Where-Object record_kind -eq 'activation_attempt').reason = 'activated' }
        'TIMING_VALID_FALSE' = { param($r) ($r | Where-Object record_kind -eq 'facts').timing_valid = $false }
        'FACTS_FREQUENCY_MISMATCH' = { param($r) ($r | Where-Object record_kind -eq 'facts').qpc_frequency_hz = 1 }
        'FACTS_ACTIVATION_MISMATCH' = { param($r) ($r | Where-Object record_kind -eq 'facts').activation_generation = 2 }
        'FACTS_OVERFLOW' = { param($r) ($r | Where-Object record_kind -eq 'facts').timing_evidence_overflow = $true }
        'STARTUP_OVERFLOW' = { param($r) ($r | Where-Object record_kind -eq 'startup').activation_evidence_overflow = $true }
        'RESTORE_BROKEN' = { param($r) ($r | Where-Object record_kind -eq 'facts').follower_restored.visible.left += 1 }
        'SAFETY_BROKEN' = { param($r) ($r | Where-Object record_kind -eq 'summary').user_preexisting_windows_touched = $true }
    }
    foreach ($name in $mutations.Keys) {
        $records = @(New-CtrlHarnessRecords)
        & $mutations[$name] $records
        Test-CtrlFixture $name $records 0 1 'INVALID_EVIDENCE'
    }
    $records = @(New-CtrlHarnessRecords)
    $attempt = ($records | Where-Object record_kind -eq 'activation_attempt') | ConvertTo-Json -Depth 12 | ConvertFrom-Json
    $records = @($records[0..($records.Count - 3)]) + @($attempt) + @($records[-2], $records[-1])
    Test-CtrlFixture 'DUPLICATE_ATTEMPT_RECORD' (Set-FixtureSequence $records) 0 1 'INVALID_EVIDENCE'
    $records = @(New-CtrlHarnessRecords | Where-Object record_kind -ne 'activation_attempt')
    Test-CtrlFixture 'MISSING_ACTIVATION_ATTEMPT' (Set-FixtureSequence $records) 0 1 'INVALID_EVIDENCE'
    $records = @(New-DeferredAckHarnessRecords)
    ($records | Where-Object { $_.record_kind -eq 'internal_trace' -and $_.decision -eq 'feedback_observed_pending_result' }).ack_qpc = 316000
    Test-CtrlFixture 'PROVISIONAL_IS_NOT_ACK' $records 0 1 'INVALID_EVIDENCE'
    $records = @(New-DeferredAckHarnessRecords)
    ($records | Where-Object { $_.record_kind -eq 'internal_trace' -and $_.decision -eq 'feedback_acknowledged' }).behavior_operation_generation = 99
    Test-CtrlFixture 'DEFERRED_ACK_WRONG_OPERATION' $records 0 1 'INVALID_EVIDENCE'
    $records = @(New-CtrlHarnessRecords -CallbackDown $false -LeftDown $false)
    ($records | Where-Object record_kind -eq 'facts').follower_final = New-Snapshot 650 230 102
    Test-CtrlFixture 'NO_CTRL_FOLLOWER_MOVED' $records 2 1 'INVALID_EVIDENCE' $noFollowerObserver
    $records = @(New-CtrlHarnessRecords -CallbackDown $false -LeftDown $false)
    ($records | Where-Object record_kind -eq 'summary').leader_restored_exact = $false
    Test-CtrlFixture 'NO_CTRL_RESTORE_BROKEN' $records 2 1 'INVALID_EVIDENCE' $noFollowerObserver
    $records = @(New-CtrlHarnessRecords -CallbackDown $false -LeftDown $false)
    ($records | Where-Object record_kind -eq 'internal_trace').decision = 'activated'
    Test-CtrlFixture 'NO_CTRL_LATE_CORE_ACTIVATION' $records 2 1 'INVALID_EVIDENCE' $noFollowerObserver
    $records = @(New-CtrlHarnessRecords -CallbackDown $false -LeftDown $false)
    $operation = @(New-CtrlHarnessRecords | Where-Object { $_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower' })[0]
    $operation.operation_id = 99
    $records = @($records[0..($records.Count - 4)]) + @($operation) + @($records[-3], $records[-2], $records[-1])
    Test-CtrlFixture 'NO_CTRL_NATIVE_APPLY' (Set-FixtureSequence $records) 2 1 'INVALID_EVIDENCE' $noFollowerObserver
    $observer = @(New-ObserverRecords -ActivePairCount 1)
    $observer[-1].disposition = 'incomplete'
    Test-CtrlFixture 'OBSERVER_INCOMPLETE' (New-CtrlHarnessRecords) 0 1 'INVALID_EVIDENCE' $observer

    $stats = Get-TimingStatistics @(1..20)
    if ($stats.min -ne 1 -or $stats.p50 -ne 10.5 -or $stats.p95 -ne 19 -or $stats.max -ne 20 -or $stats.count -ne 20) {
        throw 'Median/nearest-rank p95 calculation is incorrect.'
    }
    if ((Get-TimingStatistics @()).status -ne 'insufficient samples' -or
        (Get-TimingStatistics @(7)).status -ne 'insufficient samples' -or
        (Get-TimingStatistics @(7)).p95 -ne 7) { throw 'Small-sample diagnostic is incorrect.' }
    foreach ($badValue in @([double]::NaN, [double]::PositiveInfinity, -1.0)) {
        $rejected = $false
        try { [void](Get-TimingStatistics @($badValue)) } catch { $rejected = $true }
        if (-not $rejected) { throw 'Nonfinite/negative duration was accepted.' }
    }
    Write-Output 'TIMING_PERCENTILES_AND_BOUNDS fixture: PASS'
    Write-Output 'R1-C3A evidence runner fixtures: PASS'
} finally {
    $resolvedFixtureRoot = [IO.Path]::GetFullPath($fixtureRoot)
    if (-not $resolvedFixtureRoot.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase) -or
        $resolvedFixtureRoot -eq $tempBase) { throw 'Refusing cleanup outside the validated fixture directory.' }
    if (Test-Path -LiteralPath $resolvedFixtureRoot) { Remove-Item -LiteralPath $resolvedFixtureRoot -Recurse -Force }
}
