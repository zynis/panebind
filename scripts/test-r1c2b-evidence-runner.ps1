[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$runner = Join-Path $PSScriptRoot 'run-r1c2b-explorer-glue-evidence.ps1'
$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$fixtureRoot = Join-Path $tempBase (
    'panebind-r1c2b-runner-' + [Guid]::NewGuid().ToString('N'))
[void] (New-Item -ItemType Directory -Path $fixtureRoot)
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Write-JsonLines {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [object[]] $Records
    )
    $lines = @($Records | ForEach-Object {
        $_ | ConvertTo-Json -Compress -Depth 12
    })
    [IO.File]::WriteAllLines($Path, $lines, $utf8NoBom)
}

function New-ObserverRecords {
    param(
        [ValidateRange(0, 2)] [int] $ActivePairCount = 0,
        [switch] $IncludePreAuthorityMove
    )

    $base = [DateTimeOffset]::Parse('2026-01-01T00:00:00Z')
    $records = [Collections.Generic.List[object]]::new()
    function Add-ObserverRecord {
        param([int] $OffsetSeconds, [hashtable] $Fields)
        $record = [ordered]@{
            schema_version = 1
            observer_sequence = [uint64] ($records.Count + 1)
            received_at = $base.AddSeconds($OffsetSeconds).ToString(
                "yyyy-MM-ddTHH:mm:ss.fff'Z'")
        }
        foreach ($key in $Fields.Keys) {
            $record[$key] = $Fields[$key]
        }
        [void] $records.Add([pscustomobject] $record)
    }
    function Add-Event {
        param([int] $OffsetSeconds, [string] $WindowId, [string] $Name)
        Add-ObserverRecord $OffsetSeconds ([ordered]@{
            record_kind = 'event'; callback_root_matches = $true
            native_object_id = 0; native_child_id = 0; field_errors = @()
            native_window_id = $WindowId
            native_event = [ordered]@{ name = $Name }
        })
    }

    Add-ObserverRecord 0 ([ordered]@{
        record_kind = 'diagnostic'; diagnostic = 'hook_registration'
        disposition = 'complete'; field_errors = @()
    })
    if ($IncludePreAuthorityMove) {
        Add-Event 100 '0x0000000000000010' 'EVENT_SYSTEM_MOVESIZESTART'
        Add-Event 110 '0x0000000000000010' 'EVENT_OBJECT_LOCATIONCHANGE'
        Add-Event 120 '0x0000000000000010' 'EVENT_SYSTEM_MOVESIZEEND'
    }
    for ($pairIndex = 0; $pairIndex -lt $ActivePairCount; ++$pairIndex) {
        $offset = 231 + ($pairIndex * 4)
        Add-Event $offset '0x0000000000000010' 'EVENT_SYSTEM_MOVESIZESTART'
        Add-Event ($offset + 1) '0x0000000000000010' 'EVENT_OBJECT_LOCATIONCHANGE'
        Add-Event ($offset + 2) '0x0000000000000020' 'EVENT_OBJECT_LOCATIONCHANGE'
        Add-Event ($offset + 3) '0x0000000000000010' 'EVENT_SYSTEM_MOVESIZEEND'
    }
    Add-ObserverRecord 400 ([ordered]@{
        record_kind = 'diagnostic'; diagnostic = 'hook_shutdown'
        disposition = 'complete'; field_errors = @()
    })
    Add-ObserverRecord 410 ([ordered]@{
        record_kind = 'diagnostic'; diagnostic = 'observer_shutdown'
        disposition = 'complete'; field_errors = @()
    })
    return $records.ToArray()
}

function New-Rect {
    param([int] $Left, [int] $Top, [int] $Width = 400, [int] $Height = 400)
    return [pscustomobject]@{
        left = $Left; top = $Top
        right = $Left + $Width; bottom = $Top + $Height
    }
}

function New-Snapshot {
    param([int] $Left = 0, [int] $Top = 0, [int] $Tid = 101)
    return [pscustomobject] [ordered]@{
        visible = New-Rect $Left $Top
        positioning = New-Rect $Left $Top
        pid = 100; tid = $Tid; class = 'CabinetWClass'; dpi = 96
        monitor = '\\.\DISPLAY1'
        monitor_rect = New-Rect 0 0 1200 900
        work_area = New-Rect 0 0 1200 800
        root_top_level = $true; visible_state = $true; cloaked = $false
        minimized = $false; maximized = $false
        current_virtual_desktop = $true; exact_test_location = $true
        target_integrity_rid = 8192; target_session_id = 1
        target_elevated = $false; target_ui_access = $false
        target_app_container = $false
    }
}

function New-LayoutPreviewFields {
    param([int] $Attempt = 1, [switch] $Fits)
    if ($Fits) {
        $leader = [pscustomobject]@{ width = 400; height = 400 }
        $follower = [pscustomobject]@{ width = 400; height = 400 }
        $work = [pscustomobject]@{ width = 1200; height = 800 }
        $horizontalRequired = [pscustomobject]@{ width = 800; height = 400 }
        $verticalRequired = [pscustomobject]@{ width = 400; height = 800 }
        $horizontalExcess = [pscustomobject]@{ width = 0; height = 0 }
        $verticalExcess = [pscustomobject]@{ width = 0; height = 0 }
        $result = 'FIT'
        $reason = 'eligible'
        $orientation = 'horizontal'
        $horizontalFits = $true
        $verticalFits = $true
    } else {
        $leader = [pscustomobject]@{ width = 800; height = 600 }
        $follower = [pscustomobject]@{ width = 800; height = 600 }
        $work = [pscustomobject]@{ width = 1200; height = 900 }
        $horizontalRequired = [pscustomobject]@{ width = 1600; height = 600 }
        $verticalRequired = [pscustomobject]@{ width = 800; height = 1200 }
        $horizontalExcess = [pscustomobject]@{ width = 400; height = 0 }
        $verticalExcess = [pscustomobject]@{ width = 0; height = 300 }
        $result = 'NEEDS_MANUAL_RESIZE'
        $reason = 'unsafe_layout'
        $orientation = $null
        $horizontalFits = $false
        $verticalFits = $false
    }
    return [ordered]@{
        attempt = $Attempt; attempt_limit = 3; result = $result; reason = $reason
        same_monitor = $true; same_dpi = $true
        leader_target_consent_prefix_valid = $true
        follower_target_consent_prefix_valid = $true
        follower_baseline_excluded_leader = $true; pair_distinct = $true
        glue_authority_bound = $false; glue_authority_consumed = $false
        temporary_peer_exception_retained = $false
        native_apply_attempted = $false; event_source_armed = $false
        leader_size = $leader; follower_size = $follower
        work_area_size = $work
        horizontal_required = $horizontalRequired; horizontal_available = $work
        horizontal_excess = $horizontalExcess; horizontal_fits = $horizontalFits
        vertical_required = $verticalRequired; vertical_available = $work
        vertical_excess = $verticalExcess; vertical_fits = $verticalFits
        orientation = $orientation
    }
}

function New-BaseHarnessBuilder {
    $records = [Collections.Generic.List[object]]::new()
    $base = [DateTimeOffset]::Parse('2026-01-01T00:00:00Z')
    $add = {
        param([string] $Kind, [hashtable] $Fields)
        $record = [ordered]@{
            schema_version = 1
            schema_name = 'panebind.r1c2b.explorer_glue'
            harness_sequence = [uint64] ($records.Count + 1)
            record_kind = $Kind
            recorded_at = $base.AddSeconds(
                ($records.Count + 1) * 10).ToString(
                    "yyyy-MM-ddTHH:mm:ss.fff'Z'")
        }
        foreach ($key in $Fields.Keys) {
            $record[$key] = $Fields[$key]
        }
        [void] $records.Add([pscustomobject] $record)
    }.GetNewClosure()

    & $add 'startup' ([ordered]@{
        input_source = 'interactive_console'; synthetic_input = $false
        r0_observer_runtime_dependency = $false; target_window_count = 2
        layout_readiness_preview_supported = $true
        layout_readiness_attempt_limit = 3
    })
    & $add 'nonce_target' ([ordered]@{
        role = 'leader'; target_id = 'leader-fixture'
        created_empty = $true; full_path_redacted = $true
    })
    & $add 'nonce_target' ([ordered]@{
        role = 'follower'; target_id = 'follower-fixture'
        created_empty = $true; full_path_redacted = $true
    })

    foreach ($role in @('leader', 'follower')) {
        & $add 'baseline' ([ordered]@{
            role = $role; result = 'PASS'; baseline_generation = 1
            baseline_exclusion_complete = $true
            target_directory_contract_verified = $true
        })
        & $add 'target_consent_prompt' ([ordered]@{
            role = $role; result = 'READY'; generation = 2
            input_source = 'interactive_console'
        })
        & $add 'target_consent_confirmation' ([ordered]@{
            role = $role; result = 'CONFIRMED'
            input_source = 'interactive_console'; native_apply_attempted = $false
        })
        & $add 'candidate_selection' ([ordered]@{
            role = $role; result = 'PASS'; authority_kind = 'user_consent'
            unique_new_target = $true; exact_target_location = $true
            preexisting_exact_location_detected = $false
            baseline_generation = 1; target_prompt_generation = 2
            target_confirmation_generation = 3; eligibility_generation = 4
            token_generation = 5
        })
        & $add 'native_target_identity' ([ordered]@{
            role = $role; process = 'explorer.exe'
            native_key = [uint64] $(if ($role -eq 'leader') { 16 } else { 32 })
            pid = 100; tid = $(if ($role -eq 'leader') { 101 } else { 102 })
            capability_generation = 1; consent_generation = 5
        })
    }

    return [pscustomobject]@{ Records = $records; Add = $add }
}

function Add-SafeSummary {
    param([scriptblock] $Add, [switch] $Contradictory)
    if ($Contradictory) {
        & $Add 'operation' ([ordered]@{
            phase = 'active_follower'; role = 'follower'
            native_apply_attempted = $true; exact_receipt = $true
        })
    }
    & $Add 'summary' ([ordered]@{
        result = 'BLOCKED'; reason = 'pair_validation_blocked'
        glue_reason = 'unsafe_layout'; glue_stage = 'pair_validation'
        implementation_ready = $true; runtime_gate = 'BLOCKED'
        layout_readiness_preview_supported = $true
        layout_preview_attempt_count = 3; layout_preview_fit = $false
        layout_preview_side_effect_free = $true
        behavior_state = 'idle'; leader_start_count = 0
        leader_location_count = 0; leader_end_count = 0
        follower_feedback_count = 0; follower_native_apply_count = 0
        active_follower_operation_count = 0; follower_noop_count = 0
        suppressed_feedback_count = 0; duplicate_feedback_count = 0
        missing_feedback_count = 0; reconciled_feedback_count = 0
        acknowledged_operation_count = 0; reconciled_operation_count = 0
        feedback_operation_correlation_valid = $false
        trace_generation_valid = $false
        feedback_suppression_evidence = 'not_reached'
        unexpected_feedback_count = 0; recursive_follower_operation_count = 0
        all_active_follower_operations_exact = $false; queue_overflow = $false
        max_event_queue_depth = 0; max_pending_depth = 0
        event_source_armed = $false; event_source_stopped = $false
        event_source_lifecycle_clean = $false
        topology_frozen_exact_pair = $false
        leader_restored_exact = $false; follower_restored_exact = $false
        user_preexisting_windows_touched = $false
        other_third_party_control = $false; global_input_control = $false
        user_windows_close_attempted = $false
        r0_observer_runtime_dependency = $false
        r0_observer_semantics_changed = $false
    })
    & $Add 'shutdown' ([ordered]@{ disposition = 'complete' })
}

function New-SafeHarnessRecords {
    param([switch] $Contradictory)
    $builder = New-BaseHarnessBuilder
    foreach ($attempt in 1..3) {
        & $builder.Add 'pair_layout_preview' `
            (New-LayoutPreviewFields -Attempt $attempt)
        if ($attempt -lt 3) {
            & $builder.Add 'pair_layout_recheck_confirmation' ([ordered]@{
                attempt = $attempt; result = 'CONFIRMED'
                input_source = 'interactive_console'; native_apply_attempted = $false
            })
        }
    }
    & $builder.Add 'pair_validation' ([ordered]@{
        result = 'BLOCKED'; reason = 'unsafe_layout'
        leader_target_consent_prefix_valid = $true
        follower_target_consent_prefix_valid = $true
        follower_baseline_excluded_leader = $true; pair_distinct = $true
        same_monitor_and_dpi = $true; test_layout_planned = $false
        leader_original = $null; follower_original = $null
    })
    Add-SafeSummary -Add $builder.Add -Contradictory:$Contradictory
    return $builder.Records.ToArray()
}

function New-LegacyPassHarnessRecords {
    param([switch] $NoSetup, [switch] $NoRestore)
    if ($NoSetup -and $NoRestore) {
        throw 'Successful fixture requires a non-zero final total delta.'
    }
    $builder = New-BaseHarnessBuilder
    $add = $builder.Add
    $leaderLayout = New-Snapshot 200 200 101
    $followerLayout = New-Snapshot 600 200 102
    $leaderOriginal = if ($NoSetup) {
        $leaderLayout
    } else {
        New-Snapshot 100 100 101
    }
    $followerOriginal = if ($NoSetup) {
        $followerLayout
    } else {
        New-Snapshot 500 100 102
    }
    $leaderFinal = if ($NoRestore) {
        $leaderOriginal
    } else {
        New-Snapshot 250 230 101
    }
    $followerFinal = if ($NoRestore) {
        $followerOriginal
    } else {
        New-Snapshot 650 230 102
    }
    $commandRect = $followerFinal.visible
    $operationState = [pscustomobject]@{ Id = 0 }
    $addOperation = {
        param(
            [string] $Phase, [string] $Role, [object] $Before,
            [object] $Requested, [object] $Actual,
            [uint64] $Generation = 0, [uint64] $SourceSequence = 0
        )
        ++$operationState.Id
        & $add 'operation' ([ordered]@{
            phase = $Phase; role = $Role
            behavior_operation_generation = $Generation
            source_leader_sequence = $SourceSequence
            operation_id = $operationState.Id; reason_code = 0; stage_code = 0
            native_apply_attempted = $true; native_outcome_known = $true
            cleanup_operation = ($Phase -eq 'restore'); exact_receipt = $true
            before = $Before; requested_visible = $Requested.visible
            requested_positioning = $Requested.positioning; actual = $Actual
            size_preserved = $true; identity_stable = $true
            location_stable = $true; monitor_and_dpi_stable = $true
        })
    }.GetNewClosure()

    & $add 'pair_layout_preview' (New-LayoutPreviewFields -Fits)
    & $add 'pair_validation' ([ordered]@{
        result = 'PASS'; reason = 'eligible'
        leader_target_consent_prefix_valid = $true
        follower_target_consent_prefix_valid = $true
        follower_baseline_excluded_leader = $true; pair_distinct = $true
        same_monitor_and_dpi = $true; test_layout_planned = $true
        leader_original = $leaderOriginal; follower_original = $followerOriginal
    })
    & $add 'glue_step' ([ordered]@{
        step = 'glue_consent_prompt'; result = 'PASS'
        reason = 'eligible'; stage = 'consent'
    })
    & $add 'glue_consent_prompt' ([ordered]@{
        result = 'READY'; generation = 7; input_source = 'interactive_console'
    })
    & $add 'glue_consent_confirmation' ([ordered]@{
        result = 'CONFIRMED'; input_source = 'interactive_console'
        native_apply_attempted = $false
    })
    & $add 'glue_authority' ([ordered]@{
        result = 'PASS'; pair_preview_generation = 6; prompt_generation = 7
        confirmation_generation = 8; authority_generation = 9
    })
    & $add 'glue_native_bindings' ([ordered]@{
        session_bindings_present = $true; leader_native_key = [uint64] 16
        follower_native_key = [uint64] 32; leader_pid = 100; follower_pid = 100
    })
    & $add 'glue_step' ([ordered]@{
        step = 'setup_test_layout'; result = 'PASS'
        reason = 'eligible'; stage = 'layout'
    })
    & $add 'glue_step' ([ordered]@{
        step = 'arm_event_source'; result = 'PASS'
        reason = 'eligible'; stage = 'event_source'
    })
    & $add 'drag_prompt' ([ordered]@{
        role = 'leader'; timeout_seconds = 120
        console_input_after_arm = $false
    })
    & $add 'glue_step' ([ordered]@{
        step = 'run_until_terminal'; result = 'PASS'
        reason = 'eligible'; stage = 'cleanup'
    })
    & $add 'internal_trace' ([ordered]@{
        trace_sequence = 1; glue_session_generation = 10; event_sequence = 1
        role = 'leader'; event_kind = 'move_resize_started'; decision = 'activated'
        abort_reason = $null; visible = $leaderLayout.visible
        behavior_operation_generation = 0
    })
    & $add 'internal_trace' ([ordered]@{
        trace_sequence = 2; glue_session_generation = 10; event_sequence = 2
        role = 'leader'; event_kind = 'geometry_changed'
        decision = 'follower_move_requested'; abort_reason = $null
        visible = $commandRect
        behavior_operation_generation = 1
    })
    & $add 'internal_trace' ([ordered]@{
        trace_sequence = 3; glue_session_generation = 10; event_sequence = 3
        role = 'follower'; event_kind = 'geometry_changed'
        decision = 'feedback_acknowledged'; abort_reason = $null
        visible = $commandRect
        behavior_operation_generation = 1
    })
    & $add 'internal_trace' ([ordered]@{
        trace_sequence = 4; glue_session_generation = 10; event_sequence = 4
        role = 'leader'; event_kind = 'move_resize_ended'; decision = 'completing'
        abort_reason = $null; visible = $leaderFinal.visible
        behavior_operation_generation = 0
    })
    & $add 'internal_trace' ([ordered]@{
        trace_sequence = 5; glue_session_generation = 10; event_sequence = 0
        role = 'leader'; event_kind = $null; decision = 'completed'
        abort_reason = $null; visible = $null; behavior_operation_generation = 0
    })
    if (-not $NoSetup) {
        & $addOperation 'setup' 'leader' $leaderOriginal $leaderLayout $leaderLayout
        & $addOperation 'setup' 'follower' $followerOriginal $followerLayout `
            $followerLayout
    }
    & $addOperation 'active_follower' 'follower' $followerLayout $followerFinal `
        $followerFinal 1 2
    if (-not $NoRestore) {
        & $addOperation 'restore' 'follower' $followerFinal $followerOriginal `
            $followerOriginal
        & $addOperation 'restore' 'leader' $leaderFinal $leaderOriginal `
            $leaderOriginal
    }
    & $add 'feedback_reconciliation' ([ordered]@{
        operation_generation = 1; source_leader_sequence = 2
        command_trace_match_count = 1; exact_operation_receipt = $true
        expected_visible = $commandRect; actual_visible = $commandRect
        disposition = 'acknowledged_self_feedback'; feedback_event_sequence = 3
    })
    & $add 'facts' ([ordered]@{
        glue_session_generation = 10; behavior_state = 'completed'
        behavior_abort_reason = $null; glue_consent_confirmed = $true
        follower_baseline_excluded_leader = $true; test_layout_exact = $true
        topology_exact_two_window_component = $true
        event_source_armed = $true; event_source_stopped = $true
        event_source_lifecycle_clean = $true
        leader_restore_attempted = (-not $NoRestore)
        follower_restore_attempted = (-not $NoRestore)
        leader_restored_exact = $true; follower_restored_exact = $true
        user_windows_close_attempted = $false; follower_native_apply_count = 1
        follower_noop_count = 0
        suppressed_feedback_count = 1; duplicate_feedback_count = 0
        missing_feedback_count = 0; reconciled_feedback_count = 0
        unexpected_feedback_count = 0; max_pending_depth = 1
        pending_capacity = 16; max_event_queue_depth = 2; event_queue_capacity = 512
        leader_original = $leaderOriginal; follower_original = $followerOriginal
        leader_layout = $leaderLayout; follower_layout = $followerLayout
        leader_final = $leaderFinal; follower_final = $followerFinal
        leader_restored = $leaderOriginal; follower_restored = $followerOriginal
    })
    & $add 'summary' ([ordered]@{
        result = 'PASS'; reason = 'pass'; glue_reason = 'eligible'
        glue_stage = 'cleanup'; runtime_gate = 'PASS'; implementation_ready = $true
        layout_readiness_preview_supported = $true
        layout_preview_attempt_count = 1; layout_preview_fit = $true
        layout_preview_side_effect_free = $true
        behavior_state = 'completed'; leader_start_count = 1
        leader_location_count = 1; leader_end_count = 1
        follower_feedback_count = 1; follower_native_apply_count = 1
        follower_noop_count = 0
        active_follower_operation_count = 1; all_active_follower_operations_exact = $true
        suppressed_feedback_count = 1; duplicate_feedback_count = 0
        missing_feedback_count = 0; reconciled_feedback_count = 0
        acknowledged_operation_count = 1; reconciled_operation_count = 0
        feedback_operation_correlation_valid = $true; trace_generation_valid = $true
        feedback_suppression_evidence = 'observed_and_suppressed'
        unexpected_feedback_count = 0; recursive_follower_operation_count = 0
        queue_overflow = $false; max_event_queue_depth = 2; max_pending_depth = 1
        event_source_armed = $true; event_source_stopped = $true
        event_source_lifecycle_clean = $true; topology_frozen_exact_pair = $true
        leader_restored_exact = $true; follower_restored_exact = $true
        user_preexisting_windows_touched = $false
        other_third_party_control = $false; global_input_control = $false
        user_windows_close_attempted = $false
        r0_observer_runtime_dependency = $false; r0_observer_semantics_changed = $false
    })
    & $add 'shutdown' ([ordered]@{ disposition = 'complete' })
    return $builder.Records.ToArray()
}

function New-PassHarnessRecords {
    param(
        [switch] $NoSetup, [switch] $NoRestore,
        [ValidateRange(1, 150)] [int] $RawLocationCount = 3,
        [switch] $SingleApply, [switch] $EndAlreadyReceived,
        [switch] $MissingFeedback, [switch] $EndSuffix,
        [switch] $PostEndCallback
    )
    $legacy = @(New-LegacyPassHarnessRecords -NoSetup:$NoSetup -NoRestore:$NoRestore)
    $facts = @($legacy | Where-Object { $_.record_kind -eq 'facts' })[0]
    $summary = @($legacy | Where-Object { $_.record_kind -eq 'summary' })[0]
    $oldActive = @($legacy | Where-Object {
        $_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower'
    })[0]
    $receipts = [Collections.Generic.List[object]]::new()
    $quanta = [Collections.Generic.List[object]]::new()
    $trace = [Collections.Generic.List[object]]::new()
    $active = [Collections.Generic.List[object]]::new()
    $reconciliations = [Collections.Generic.List[object]]::new()
    $addQuantum = {
        param([object[]] $Events, [object] $LeaderRect, [object] $FollowerRect)
        $quantumId = $quanta.Count + 1
        $first = $receipts.Count + 1
        foreach ($event in $Events) {
            [void] $receipts.Add([pscustomobject] [ordered]@{
                record_kind = 'event_receipt'; receipt_sequence = $receipts.Count + 1
                processing_quantum_id = $quantumId; role = $event.role
                event_kind = $event.kind; native_event_timestamp_ms = 1000 + $receipts.Count
                coalesced = $event.coalesced; discarded_after_end = $false
            })
        }
        $leaderLocations = @($receipts | Where-Object {
            $_.processing_quantum_id -eq $quantumId -and
            $_.role -eq 'leader' -and $_.event_kind -eq 'geometry_changed'
        })
        $selected = if ($leaderLocations.Count -gt 0) {
            $leaderLocations[-1].receipt_sequence
        } else { 0 }
        [void] $quanta.Add([pscustomobject] [ordered]@{
            record_kind = 'processing_quantum'; processing_quantum_id = $quantumId
            first_receipt_sequence = $first; last_receipt_sequence = $receipts.Count
            receipt_count = $Events.Count; leader_location_count = $leaderLocations.Count
            follower_location_count = @($Events | Where-Object {
                $_.role -eq 'follower' -and $_.kind -eq 'geometry_changed'
            }).Count
            selected_leader_sequence = $selected; sampled_geometry_generation = $quantumId
            leader_visible_rect = $LeaderRect; follower_visible_rect = $FollowerRect
            contains_leader_end = (@($Events | Where-Object {
                $_.role -eq 'leader' -and $_.kind -eq 'move_resize_ended'
            }).Count -gt 0)
            geometry_semantics = 'live_geometry_at_processing_quantum'
            inactive_discard = $false
        })
        return $receipts[-1]
    }.GetNewClosure()
    $addTrace = {
        param([object] $Receipt, [string] $Decision, [object] $Visible,
              [object] $Sample, [uint64] $Generation = 0)
        [void] $trace.Add([pscustomobject] [ordered]@{
            record_kind = 'internal_trace'; trace_sequence = $trace.Count + 1
            glue_session_generation = 10
            event_sequence = $(if ($null -ne $Receipt) { $Receipt.receipt_sequence } else { 0 })
            processing_quantum_id = $(if ($null -ne $Receipt) { $Receipt.processing_quantum_id } else { 0 })
            sampled_geometry_generation = $(if ($null -ne $Receipt) { $Receipt.processing_quantum_id } else { 0 })
            native_event_timestamp_ms = $(if ($null -ne $Receipt) { $Receipt.native_event_timestamp_ms } else { 0 })
            role = $(if ($null -ne $Receipt) { $Receipt.role } else { 'leader' })
            event_kind = $(if ($null -ne $Receipt) { $Receipt.event_kind } else { $null })
            decision = $Decision; abort_reason = $null; visible = $Visible
            sampled_visible_rect = $Sample; behavior_operation_generation = $Generation
        })
    }.GetNewClosure()
    $event = [pscustomobject]@{ role = 'leader'; kind = 'move_resize_started'; coalesced = $false }
    $receipt = & $addQuantum @($event) $facts.leader_layout.visible $facts.follower_layout.visible
    & $addTrace $receipt 'activated' $facts.leader_layout.visible $facts.leader_layout.visible
    $applyCount = if ($SingleApply -or $RawLocationCount -eq 1) { 1 } else { 2 }
    $previousFollower = $facts.follower_layout
    for ($step = 1; $step -le $applyCount; ++$step) {
        $leaderSample = if ($step -eq $applyCount) { $facts.leader_final } else {
            New-Snapshot 225 215 101
        }
        $target = if ($step -eq $applyCount) { $facts.follower_final } else {
            New-Snapshot 625 215 102
        }
        $count = if ($step -eq 1) { $RawLocationCount - ($applyCount - 1) } else { 1 }
        $events = @(for ($index = 0; $index -lt $count; ++$index) {
            [pscustomobject]@{
                role = 'leader'; kind = 'geometry_changed'; coalesced = ($index -lt $count - 1)
            }
        })
        $receipt = & $addQuantum $events $leaderSample.visible $previousFollower.visible
        & $addTrace $receipt 'follower_move_requested' $target.visible $leaderSample.visible $step
        $operation = $oldActive | ConvertTo-Json -Depth 12 | ConvertFrom-Json
        $operation.before = $previousFollower
        $operation.actual = $target
        $operation.requested_visible = $target.visible
        $operation.requested_positioning = $target.positioning
        $operation.behavior_operation_generation = $step
        $operation.source_leader_sequence = $receipt.receipt_sequence
        $operation | Add-Member -NotePropertyMembers @{
            processing_quantum_id = $receipt.processing_quantum_id
            sampled_geometry_generation = $receipt.processing_quantum_id
            pre_native_receipt_watermark = $receipt.receipt_sequence
            post_native_receipt_watermark = $receipt.receipt_sequence
        }
        [void] $active.Add($operation)
        $sourceSequence = $receipt.receipt_sequence
        $feedbackSequence = $null
        if ($step -eq 1 -and -not $MissingFeedback) {
            $event = [pscustomobject]@{ role = 'follower'; kind = 'geometry_changed'; coalesced = $false }
            $feedbackReceipt = & $addQuantum @($event) $null $target.visible
            & $addTrace $feedbackReceipt 'feedback_acknowledged' $target.visible $target.visible $step
            $feedbackSequence = $feedbackReceipt.receipt_sequence
        }
        [void] $reconciliations.Add([pscustomobject] [ordered]@{
            record_kind = 'feedback_reconciliation'; operation_generation = $step
            source_leader_sequence = $sourceSequence; command_trace_match_count = 1
            exact_operation_receipt = $true; expected_visible = $target.visible
            actual_visible = $target.visible
            disposition = $(if ($null -ne $feedbackSequence) { 'acknowledged_self_feedback' }
                else { 'reconciled_by_operation_receipt_and_final_snapshot' })
            feedback_event_sequence = $feedbackSequence
        })
        $previousFollower = $target
    }
    $event = [pscustomobject]@{ role = 'leader'; kind = 'move_resize_ended'; coalesced = $false }
    $receipt = & $addQuantum @($event) $facts.leader_final.visible $facts.follower_final.visible
    & $addTrace $receipt 'completing' $facts.leader_final.visible $facts.leader_final.visible
    & $addTrace $null 'completed' $null $null
    if ($EndSuffix) {
        [void] $receipts.Add([pscustomobject] [ordered]@{
            record_kind = 'event_receipt'; receipt_sequence = $receipts.Count + 1
            processing_quantum_id = $receipt.processing_quantum_id; role = 'leader'
            event_kind = 'geometry_changed'; native_event_timestamp_ms = 1000 + $receipts.Count
            coalesced = $false; discarded_after_end = $true
        })
        $quanta[-1].last_receipt_sequence = $receipts.Count
        ++$quanta[-1].receipt_count
    }
    if ($EndAlreadyReceived) {
        foreach ($operation in $active) {
            $operation.post_native_receipt_watermark = $receipt.receipt_sequence
        }
    }
    if ($PostEndCallback) {
        $discardSequence = $receipts.Count + 1
        $discardQuantumId = $quanta.Count + 1
        [void] $receipts.Add([pscustomobject] [ordered]@{
            record_kind = 'event_receipt'; receipt_sequence = $discardSequence
            processing_quantum_id = $discardQuantumId; role = 'follower'
            event_kind = 'geometry_changed'; native_event_timestamp_ms = 1000 + $receipts.Count
            coalesced = $false; discarded_after_end = $true
        })
        [void] $quanta.Add([pscustomobject] [ordered]@{
            record_kind = 'processing_quantum'; processing_quantum_id = $discardQuantumId
            first_receipt_sequence = $discardSequence; last_receipt_sequence = $discardSequence
            receipt_count = 1; leader_location_count = 0; follower_location_count = 0
            selected_leader_sequence = 0; sampled_geometry_generation = 0
            leader_visible_rect = $null; follower_visible_rect = $null
            contains_leader_end = $false; inactive_discard = $true
            geometry_semantics = 'live_geometry_at_processing_quantum'
        })
        # The final native postvalidation reentered callback delivery. Its
        # watermark sees END and a late follower receipt; neither becomes ACK.
        $active[-1].post_native_receipt_watermark = $discardSequence
    }
    $acknowledged = if ($MissingFeedback) { 0 } else { 1 }
    foreach ($record in @($facts, $summary)) {
        $record.follower_native_apply_count = $applyCount
        $record.suppressed_feedback_count = $acknowledged
        $record.missing_feedback_count = $applyCount - $acknowledged
        $record.reconciled_feedback_count = $applyCount - $acknowledged
        $record.max_pending_depth = $applyCount
        $record.max_event_queue_depth = $RawLocationCount
    }
    $summary.leader_location_count = $applyCount
    $summary.follower_feedback_count = $acknowledged
    $summary.active_follower_operation_count = $applyCount
    $summary.acknowledged_operation_count = $acknowledged
    $summary.reconciled_operation_count = $applyCount - $acknowledged
    $summary.feedback_suppression_evidence = if ($MissingFeedback) {
        'no_feedback_event_reconciled'
    } else { 'observed_and_suppressed' }
    $gate = if ($RawLocationCount -lt 3) { 'INSUFFICIENT_DRAG_EVIDENCE' }
        elseif ($applyCount -lt 2 -or $EndAlreadyReceived) { 'INSUFFICIENT_REALTIME_FOLLOW' }
        else { 'PASS' }
    $summary.result = if ($gate -eq 'PASS') { 'PASS' } else { 'BLOCKED' }
    $summary.runtime_gate = $summary.result
    $summary.reason = if ($gate -eq 'PASS') { 'pass' } else { $gate }
    $summary | Add-Member -NotePropertyMembers @{
        safety_gate = 'PASS'; final_geometry_gate = 'PASS'; realtime_follow_evidence_gate = $gate
        leader_raw_start_count = 1; leader_raw_location_count = $RawLocationCount; leader_raw_end_count = 1
        leader_processing_quantum_count = $applyCount; distinct_leader_geometry_sample_count = $applyCount
        distinct_follower_target_count = $applyCount
        follower_applies_before_end_count = $(if ($EndAlreadyReceived) { 0 }
            elseif ($PostEndCallback) { $applyCount - 1 } else { $applyCount })
    }
    $facts | Add-Member -Force -NotePropertyName 'accepted_event_count' `
        -NotePropertyValue $receipts.Count
    $operations = @($legacy | Where-Object {
        $_.record_kind -eq 'operation' -and $_.phase -eq 'setup'
    }) + $active.ToArray() + @($legacy | Where-Object {
        $_.record_kind -eq 'operation' -and $_.phase -eq 'restore'
    })
    for ($index = 0; $index -lt $operations.Count; ++$index) {
        $operations[$index].operation_id = $index + 1
    }
    $records = @($legacy | Where-Object {
        @('internal_trace', 'operation', 'feedback_reconciliation', 'facts', 'summary', 'shutdown') -notcontains $_.record_kind
    }) + $receipts.ToArray() + $quanta.ToArray() + $trace.ToArray() + $operations +
        $reconciliations.ToArray() + @($facts, $summary) + @($legacy | Where-Object { $_.record_kind -eq 'shutdown' })
    $base = [DateTimeOffset]::Parse('2026-01-01T00:00:00Z')
    for ($index = 0; $index -lt $records.Count; ++$index) {
        $records[$index] | Add-Member -Force -NotePropertyMembers @{
            schema_version = 1; schema_name = 'panebind.r1c2b.explorer_glue'
            harness_sequence = $index + 1
            recorded_at = $base.AddSeconds(($index + 1) * 10).ToString("yyyy-MM-ddTHH:mm:ss.fff'Z'")
        }
    }
    return $records
}

function New-LegacyDowngradeHarnessRecords {
    $records = @(New-SafeHarnessRecords | Where-Object {
        $_.record_kind -ne 'pair_layout_preview' -and
        $_.record_kind -ne 'pair_layout_recheck_confirmation'
    })
    $startup = @($records | Where-Object { $_.record_kind -eq 'startup' })[0]
    $summary = @($records | Where-Object { $_.record_kind -eq 'summary' })[0]
    $startup.PSObject.Properties.Remove('layout_readiness_preview_supported')
    $startup.PSObject.Properties.Remove('layout_readiness_attempt_limit')
    $summary.PSObject.Properties.Remove('layout_readiness_preview_supported')
    $summary.PSObject.Properties.Remove('layout_preview_attempt_count')
    $summary.PSObject.Properties.Remove('layout_preview_fit')
    $summary.PSObject.Properties.Remove('layout_preview_side_effect_free')
    $summary.feedback_suppression_evidence = 'no_feedback_event_reconciled'
    $base = [DateTimeOffset]::Parse('2026-01-01T00:00:00Z')
    for ($index = 0; $index -lt $records.Count; ++$index) {
        $records[$index].harness_sequence = $index + 1
        $records[$index].recorded_at = $base.AddSeconds(
            ($index + 1) * 10).ToString("yyyy-MM-ddTHH:mm:ss.fff'Z'")
    }
    return $records
}

function New-ContradictoryLayoutHarnessRecords {
    $records = New-PassHarnessRecords
    $facts = @($records | Where-Object { $_.record_kind -eq 'facts' })[0]
    $facts.leader_layout.visible.left =
        [int] $facts.leader_layout.visible.left + 1
    return $records
}

function New-ContradictoryFeedbackHarnessRecords {
    $records = New-PassHarnessRecords
    $feedback = @($records | Where-Object {
        $_.record_kind -eq 'internal_trace' -and $_.role -eq 'follower'
    })[0]
    $feedback.decision = 'duplicate_feedback_suppressed'
    return $records
}

function Write-Fixture {
    param(
        [string] $Name, [object[]] $HarnessRecords,
        [object[]] $ObserverRecords
    )
    $prefix = Join-Path $fixtureRoot $Name
    Write-JsonLines "$prefix-glue-harness.jsonl" $HarnessRecords
    Write-JsonLines "$prefix-glue-observer.stdout.jsonl" $ObserverRecords
    [IO.File]::WriteAllText(
        "$prefix-glue-observer.stderr.log", '', $utf8NoBom)
    return $prefix
}

function Assert-RunnerOutcome {
    param(
        [string] $Name, [string] $Prefix, [int] $HarnessExit,
        [int] $ExpectedExit, [string] $ExpectedMarker
    )
    $savedErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass `
            -File $runner `
            -ValidateEvidencePrefix $Prefix `
            -ValidationHarnessExitCode $HarnessExit 2>&1 | Out-String
        $actualExit = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }
    if ($actualExit -ne $ExpectedExit -or
        $output.IndexOf($ExpectedMarker, [StringComparison]::Ordinal) -lt 0) {
        throw "$Name fixture 失败：exit=$actualExit，output=$output"
    }
    Write-Output "$Name fixture: PASS (runner exit $actualExit)"
}

try {
    $passPrefix = Write-Fixture 'pass' (New-PassHarnessRecords) `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $blockedPrefix = Write-Fixture 'safe-blocked' (New-SafeHarnessRecords) `
        (New-ObserverRecords)
    $invalidPrefix = Write-Fixture 'contradictory-invalid' `
        (New-SafeHarnessRecords -Contradictory) (New-ObserverRecords)
    $missingActivePrefix = Write-Fixture 'missing-active-pair' `
        (New-PassHarnessRecords) (New-ObserverRecords -IncludePreAuthorityMove)
    $multipleActivePrefix = Write-Fixture 'multiple-active-pairs' `
        (New-PassHarnessRecords) `
        (New-ObserverRecords -ActivePairCount 2 -IncludePreAuthorityMove)
    $noSetupPrefix = Write-Fixture 'pass-noop-setup' `
        (New-PassHarnessRecords -NoSetup) `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $noRestorePrefix = Write-Fixture 'pass-noop-restore' `
        (New-PassHarnessRecords -NoRestore) `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $downgradePrefix = Write-Fixture 'legacy-downgrade' `
        (New-LegacyDowngradeHarnessRecords) (New-ObserverRecords)
    $badLayoutPrefix = Write-Fixture 'contradictory-layout' `
        (New-ContradictoryLayoutHarnessRecords) `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $badFeedbackPrefix = Write-Fixture 'contradictory-feedback' `
        (New-ContradictoryFeedbackHarnessRecords) `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $collapsedPrefix = Write-Fixture '31-to-1-safe-blocked' `
        (New-PassHarnessRecords -RawLocationCount 31 -SingleApply -MissingFeedback) `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $shortDragPrefix = Write-Fixture 'insufficient-drag' `
        (New-PassHarnessRecords -RawLocationCount 2) `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $singleDragPrefix = Write-Fixture 'one-location' `
        (New-PassHarnessRecords -RawLocationCount 1) `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $afterEndPrefix = Write-Fixture 'applies-after-end-receipt' `
        (New-PassHarnessRecords -EndAlreadyReceived) `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $missingFeedbackPrefix = Write-Fixture 'multistep-missing-feedback' `
        (New-PassHarnessRecords -MissingFeedback) `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $stressPrefix = Write-Fixture '120-receipts-two-quanta' `
        (New-PassHarnessRecords -RawLocationCount 120 -MissingFeedback) `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $forgedSampleRecords = New-PassHarnessRecords
    $forgedSample = @($forgedSampleRecords | Where-Object {
        $_.record_kind -eq 'internal_trace' -and $_.decision -eq 'follower_move_requested'
    })[0]
    $forgedSample.sampled_visible_rect = New-Rect 999 999
    $forgedSamplePrefix = Write-Fixture 'forged-sampled-geometry' $forgedSampleRecords `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $forgedWatermarkRecords = New-PassHarnessRecords
    $forgedWatermark = @($forgedWatermarkRecords | Where-Object {
        $_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower'
    })[0]
    $forgedWatermark.post_native_receipt_watermark = 0
    $forgedWatermarkPrefix = Write-Fixture 'forged-native-watermark' $forgedWatermarkRecords `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $fakeAckRecords = New-PassHarnessRecords -MissingFeedback
    $fakeAck = @($fakeAckRecords | Where-Object {
        $_.record_kind -eq 'feedback_reconciliation'
    })[0]
    $fakeAck.feedback_event_sequence = 3
    $fakeAckPrefix = Write-Fixture 'missing-feedback-fake-ack' $fakeAckRecords `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $unsafeBlockedRecords = New-PassHarnessRecords -RawLocationCount 31 -SingleApply
    $unsafeSummary = @($unsafeBlockedRecords | Where-Object { $_.record_kind -eq 'summary' })[0]
    $unsafeSummary.leader_restored_exact = $false
    $unsafeBlockedPrefix = Write-Fixture 'blocked-does-not-bypass-safety' $unsafeBlockedRecords `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $endSuffixPrefix = Write-Fixture 'end-suffix-discarded' `
        (New-PassHarnessRecords -EndSuffix) `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $badDiscardRecords = New-PassHarnessRecords -EndSuffix
    $badDiscard = @($badDiscardRecords | Where-Object {
        $_.record_kind -eq 'event_receipt' -and $_.discarded_after_end
    })[0]
    --$badDiscard.processing_quantum_id
    $badDiscardPrefix = Write-Fixture 'discarded-outside-end-quantum' $badDiscardRecords `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $fakeHistoricalRecords = New-PassHarnessRecords
    $fakeHistorical = @($fakeHistoricalRecords | Where-Object {
        $_.record_kind -eq 'event_receipt'
    })[0]
    $fakeHistorical | Add-Member -NotePropertyName 'visible' -NotePropertyValue (New-Rect 200 200)
    $fakeHistoricalPrefix = Write-Fixture 'historical-receipt-geometry' $fakeHistoricalRecords `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $postEndPrefix = Write-Fixture 'post-end-native-callback' `
        (New-PassHarnessRecords -PostEndCallback) `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $badInactiveSampleRecords = New-PassHarnessRecords -PostEndCallback
    $badInactiveSample = @($badInactiveSampleRecords | Where-Object {
        $_.record_kind -eq 'processing_quantum' -and $_.inactive_discard
    })[0]
    $badInactiveSample.sampled_geometry_generation = 99
    $badInactiveSamplePrefix = Write-Fixture 'inactive-quantum-claims-sample' $badInactiveSampleRecords `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)
    $badInactiveReceiptRecords = New-PassHarnessRecords -PostEndCallback
    $badInactiveReceipt = @($badInactiveReceiptRecords | Where-Object {
        $_.record_kind -eq 'event_receipt'
    })[-1]
    $badInactiveReceipt.discarded_after_end = $false
    $badInactiveReceiptPrefix = Write-Fixture 'inactive-quantum-live-receipt' $badInactiveReceiptRecords `
        (New-ObserverRecords -ActivePairCount 1 -IncludePreAuthorityMove)

    Assert-RunnerOutcome 'PASS' $passPrefix 0 0 'evidence gate: PASS'
    Assert-RunnerOutcome 'SAFE_BLOCKED' $blockedPrefix 1 2 'KNOWN_BLOCKED'
    Assert-RunnerOutcome 'INVALID_EVIDENCE' $invalidPrefix 1 1 'INVALID_EVIDENCE'
    Assert-RunnerOutcome 'MISSING_ACTIVE_PAIR' $missingActivePrefix 0 1 `
        'INVALID_EVIDENCE'
    Assert-RunnerOutcome 'MULTIPLE_ACTIVE_PAIRS' $multipleActivePrefix 0 1 `
        'INVALID_EVIDENCE'
    Assert-RunnerOutcome 'PASS_NOOP_SETUP' $noSetupPrefix 0 0 `
        'evidence gate: PASS'
    Assert-RunnerOutcome 'PASS_NOOP_RESTORE' $noRestorePrefix 0 0 `
        'evidence gate: PASS'
    Assert-RunnerOutcome 'LEGACY_DOWNGRADE' $downgradePrefix 1 1 `
        'INVALID_EVIDENCE'
    Assert-RunnerOutcome 'CONTRADICTORY_LAYOUT' $badLayoutPrefix 0 1 `
        'INVALID_EVIDENCE'
    Assert-RunnerOutcome 'CONTRADICTORY_FEEDBACK' $badFeedbackPrefix 0 1 `
        'INVALID_EVIDENCE'
    Assert-RunnerOutcome '31_TO_1_SAFE_BLOCKED' $collapsedPrefix 2 2 `
        'SAFE_BLOCKED / INSUFFICIENT_REALTIME_FOLLOW'
    Assert-RunnerOutcome 'INSUFFICIENT_DRAG' $shortDragPrefix 2 2 `
        'SAFE_BLOCKED / INSUFFICIENT_DRAG_EVIDENCE'
    Assert-RunnerOutcome 'ONE_LOCATION' $singleDragPrefix 2 2 `
        'SAFE_BLOCKED / INSUFFICIENT_DRAG_EVIDENCE'
    Assert-RunnerOutcome 'AFTER_END_RECEIPT' $afterEndPrefix 2 2 `
        'SAFE_BLOCKED / INSUFFICIENT_REALTIME_FOLLOW'
    Assert-RunnerOutcome 'MULTISTEP_MISSING_FEEDBACK' $missingFeedbackPrefix 0 0 `
        'evidence gate: PASS'
    Assert-RunnerOutcome '120_RECEIPTS_TWO_QUANTA' $stressPrefix 0 0 `
        'evidence gate: PASS'
    Assert-RunnerOutcome 'FORGED_SAMPLE' $forgedSamplePrefix 0 1 'INVALID_EVIDENCE'
    Assert-RunnerOutcome 'FORGED_WATERMARK' $forgedWatermarkPrefix 0 1 'INVALID_EVIDENCE'
    Assert-RunnerOutcome 'MISSING_FEEDBACK_FAKE_ACK' $fakeAckPrefix 0 1 'INVALID_EVIDENCE'
    Assert-RunnerOutcome 'BLOCKED_SAFETY_CONTRADICTION' $unsafeBlockedPrefix 2 1 'INVALID_EVIDENCE'
    Assert-RunnerOutcome 'END_SUFFIX_DISCARDED' $endSuffixPrefix 0 0 'evidence gate: PASS'
    Assert-RunnerOutcome 'DISCARDED_WRONG_QUANTUM' $badDiscardPrefix 0 1 'INVALID_EVIDENCE'
    Assert-RunnerOutcome 'HISTORICAL_RECEIPT_GEOMETRY' $fakeHistoricalPrefix 0 1 'INVALID_EVIDENCE'
    Assert-RunnerOutcome 'POST_END_NATIVE_CALLBACK' $postEndPrefix 0 0 'evidence gate: PASS'
    Assert-RunnerOutcome 'INACTIVE_QUANTUM_SAMPLE' $badInactiveSamplePrefix 0 1 'INVALID_EVIDENCE'
    Assert-RunnerOutcome 'INACTIVE_QUANTUM_LIVE_RECEIPT' $badInactiveReceiptPrefix 0 1 'INVALID_EVIDENCE'
    Write-Output 'R1-C2B evidence runner fixtures: PASS'
} finally {
    $resolvedFixtureRoot = [System.IO.Path]::GetFullPath($fixtureRoot)
    if (-not $resolvedFixtureRoot.StartsWith(
            $tempBase, [StringComparison]::OrdinalIgnoreCase) -or
        $resolvedFixtureRoot -eq $tempBase) {
        throw "拒绝清理非临时 fixture 路径：$resolvedFixtureRoot"
    }
    if (Test-Path -LiteralPath $resolvedFixtureRoot) {
        Remove-Item -LiteralPath $resolvedFixtureRoot -Recurse -Force
    }
}
