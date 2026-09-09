# R1-C3A-only validators. Loaded by the fixed Ctrl profile, never by R1-C2B.
# All geometry, target, operation, restore, feedback and Observer gates remain
# in the shared runner. No records are invented or rewritten for validation.

function Get-EvidenceInteger {
    param([object] $Record, [string] $Name, [switch] $AllowZero)
    Assert-RecordProperties $Record @($Name) 'Ctrl/timing integer'
    $value = $Record.$Name
    if (($value -isnot [int] -and $value -isnot [long] -and
         $value -isnot [uint32] -and $value -isnot [uint64]) -or
        $value -lt 0 -or (-not $AllowZero -and $value -eq 0)) {
        throw "$Name must be a positive integer (zero only when specified)."
    }
    return [uint64] $value
}

function Assert-EvidenceBoolean {
    param([object] $Record, [string[]] $Names)
    Assert-RecordProperties $Record $Names 'Ctrl/timing boolean'
    foreach ($name in $Names) {
        if ($Record.$name -isnot [bool]) { throw "$name must be a JSON boolean." }
    }
}

function Assert-CtrlActivationEvidence {
    param([object[]] $Records, [object] $Startup, [object] $Summary,
          [object] $Leader, [object] $Follower)
    Assert-RecordProperties $Startup @('activation_input_source',
        'glue_console_confirmation_required', 'timing_clock', 'qpc_frequency_hz',
        'timing_capacity', 'activation_capacity', 'operation_capacity') 'Ctrl startup'
    Assert-EvidenceBoolean $Startup @('glue_console_confirmation_required')
    if ($Startup.activation_input_source -ne 'ctrl_move_start' -or
        $Startup.glue_console_confirmation_required -ne $false -or
        $Startup.timing_clock -ne 'query_performance_counter') {
        throw 'Ctrl startup input/clock contract is invalid.'
    }
    [void] (Get-EvidenceInteger $Startup 'qpc_frequency_hz')
    $pairAuthority = Assert-UniqueRecord $Records 'pair_authority'
    Assert-RecordProperties $pairAuthority @('result', 'input_source',
        'activation_input_source', 'glue_console_confirmation_required',
        'pair_preview_generation', 'pair_authority_generation', 'glue_authority_id',
        'leader_window_id', 'follower_window_id',
        'leader_capability_generation', 'follower_capability_generation',
        'leader_consent_generation', 'follower_consent_generation') 'pair_authority'
    Assert-EvidenceBoolean $pairAuthority @('glue_console_confirmation_required')
    foreach ($name in @('pair_preview_generation', 'pair_authority_generation',
        'glue_authority_id', 'leader_window_id', 'follower_window_id',
        'leader_capability_generation', 'follower_capability_generation',
        'leader_consent_generation', 'follower_consent_generation')) {
        [void] (Get-EvidenceInteger $pairAuthority $name)
    }
    if ($pairAuthority.result -ne 'PASS' -or
        $pairAuthority.input_source -ne 'target_consent_fixture' -or
        $pairAuthority.activation_input_source -ne 'ctrl_move_start' -or
        $pairAuthority.glue_console_confirmation_required -ne $false -or
        $pairAuthority.pair_preview_generation -ge $pairAuthority.pair_authority_generation -or
        $pairAuthority.leader_window_id -eq $pairAuthority.follower_window_id -or
        $pairAuthority.leader_capability_generation -ne $Leader.capability_generation -or
        $pairAuthority.follower_capability_generation -ne $Follower.capability_generation -or
        $pairAuthority.leader_consent_generation -ne $Leader.consent_generation -or
        $pairAuthority.follower_consent_generation -ne $Follower.consent_generation) {
        throw 'Ctrl pair authority does not match the independently authorized targets.'
    }
    $receipts = @($Records | Where-Object { $_.record_kind -eq 'event_receipt' })
    $starts = @($receipts | Where-Object {
        $_.role -eq 'leader' -and $_.event_kind -eq 'move_resize_started'
    })
    $attempts = @($Records | Where-Object { $_.record_kind -eq 'activation_attempt' })
    if ($starts.Count -ne 1 -or $attempts.Count -ne 1) {
        throw 'Exactly one Leader START and one activation attempt are required.'
    }
    $attempt = $attempts[0]
    Assert-RecordProperties $attempt @('attempt_generation', 'activation_generation',
        'pair_authority_generation', 'leader_start_receipt_sequence',
        'leader_capability_generation', 'follower_capability_generation',
        'glue_authority_id', 'role', 'leader_window_id', 'follower_window_id',
        'leader_consent_generation', 'follower_consent_generation',
        'glue_session_generation', 'callback_qpc', 'owner_processing_qpc', 'decision_qpc',
        'ctrl_sample_available', 'owner_ctrl_sample_available',
        'ctrl_down_at_callback_delivery', 'left_ctrl_down', 'right_ctrl_down',
        'ctrl_down_at_owner_processing', 'decision', 'reason',
        'activation_source', 'input_source') 'activation_attempt'
    Assert-EvidenceBoolean $attempt @('ctrl_down_at_callback_delivery',
        'left_ctrl_down', 'right_ctrl_down', 'ctrl_down_at_owner_processing',
        'ctrl_sample_available', 'owner_ctrl_sample_available')
    Assert-EvidenceBoolean $starts[0] @('ctrl_down_at_callback_delivery',
        'left_ctrl_down', 'right_ctrl_down', 'ctrl_sample_available')
    foreach ($name in @('attempt_generation', 'pair_authority_generation',
        'leader_start_receipt_sequence', 'leader_capability_generation',
        'follower_capability_generation', 'glue_session_generation', 'callback_qpc',
        'owner_processing_qpc', 'decision_qpc')) {
        [void] (Get-EvidenceInteger $attempt $name)
    }
    [void] (Get-EvidenceInteger $attempt 'activation_generation' -AllowZero)
    # VK_CONTROL is the authority fact. The separately read left/right high
    # bits are diagnostics, not an atomic keyboard snapshot or a second policy.
    if ($attempt.attempt_generation -ne 1 -or
        $attempt.pair_authority_generation -ne $pairAuthority.pair_authority_generation -or
        $attempt.glue_authority_id -ne $pairAuthority.glue_authority_id -or
        $attempt.role -ne 'leader' -or
        $attempt.leader_window_id -ne $pairAuthority.leader_window_id -or
        $attempt.follower_window_id -ne $pairAuthority.follower_window_id -or
        $attempt.leader_consent_generation -ne $pairAuthority.leader_consent_generation -or
        $attempt.follower_consent_generation -ne $pairAuthority.follower_consent_generation -or
        $attempt.ctrl_sample_available -ne $true -or
        $attempt.owner_ctrl_sample_available -ne $true -or
        $starts[0].ctrl_sample_available -ne $true -or
        $attempt.leader_capability_generation -ne $pairAuthority.leader_capability_generation -or
        $attempt.follower_capability_generation -ne $pairAuthority.follower_capability_generation -or
        $attempt.leader_start_receipt_sequence -ne $starts[0].receipt_sequence -or
        $attempt.callback_qpc -ne $starts[0].callback_qpc -or
        $attempt.ctrl_down_at_callback_delivery -ne $starts[0].ctrl_down_at_callback_delivery -or
        $attempt.left_ctrl_down -ne $starts[0].left_ctrl_down -or
        $attempt.right_ctrl_down -ne $starts[0].right_ctrl_down -or
        $attempt.callback_qpc -gt $attempt.owner_processing_qpc -or
        $attempt.owner_processing_qpc -gt $attempt.decision_qpc -or
        $attempt.activation_source -ne 'ctrl_at_leader_start' -or
        $attempt.input_source -ne 'ctrl_move_start') {
        throw 'Ctrl attempt/START/target-generation/clock linkage is invalid.'
    }
    $notActivated = -not $attempt.ctrl_down_at_callback_delivery
    if ($notActivated) {
        if ($attempt.decision -ne 'NOT_ACTIVATED' -or
            $attempt.reason -ne 'CTRL_NOT_DOWN_AT_START' -or
            $attempt.activation_generation -ne 0) {
            throw 'Ctrl UP at START must remain NOT_ACTIVATED for the entire drag.'
        }
    } elseif ($attempt.decision -ne 'ACTIVATED' -or
              $attempt.reason -ne 'ctrl_at_leader_start' -or
              $attempt.activation_generation -ne $attempt.attempt_generation) {
        throw 'Ctrl DOWN at START must have exactly one accepted activation generation.'
    }
    Assert-RecordProperties $Summary @('activation_count', 'activation_attempt_count',
        'activation_generation', 'ctrl_activation_evidence_gate',
        'activation_input_source', 'glue_console_confirmation_required',
        'keyboard_content_collection') 'Ctrl summary'
    Assert-EvidenceBoolean $Summary @('glue_console_confirmation_required', 'keyboard_content_collection')
    if ($Summary.activation_attempt_count -ne 1 -or
        $Summary.activation_generation -ne $attempt.activation_generation -or
        $Summary.activation_input_source -ne 'ctrl_move_start' -or
        $Summary.glue_console_confirmation_required -ne $false -or
        $Summary.keyboard_content_collection -ne $false -or
        $Summary.activation_count -ne [int] (-not $notActivated) -or
        $Summary.ctrl_activation_evidence_gate -ne $(if ($notActivated) {
            'CTRL_NOT_DOWN_AT_START'
        } else { 'PASS' })) {
        throw 'Ctrl summary does not match the independently parsed START attempt.'
    }
    return [pscustomobject]@{
        NotActivated = $notActivated
        Attempt = $attempt
        PairAuthority = $pairAuthority
    }
}

function Get-TimingStatistics {
    param([AllowEmptyCollection()] [double[]] $Samples)
    $ordered = @($Samples | Sort-Object)
    foreach ($value in $ordered) {
        if ([double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -lt 0) {
            throw 'Timing duration must be finite and nonnegative.'
        }
    }
    if ($ordered.Count -eq 0) {
        return [pscustomobject]@{ count = 0; min = $null; p50 = $null; p95 = $null; max = $null; status = 'insufficient samples' }
    }
    # Ordinary median (mean of middle pair for even N); p95 is nearest rank.
    # Neither statistic is compared against an FPS or latency threshold.
    $middle = [int][Math]::Floor($ordered.Count / 2)
    $median = if ($ordered.Count % 2 -eq 0) {
        ($ordered[$middle - 1] + $ordered[$middle]) / 2.0
    } else { $ordered[$middle] }
    return [pscustomobject]@{
        count = $ordered.Count
        min = $ordered[0]
        p50 = $median
        p95 = $ordered[[int][Math]::Ceiling(0.95 * $ordered.Count) - 1]
        max = $ordered[-1]
        status = $(if ($ordered.Count -lt 2) { 'insufficient samples' } else { 'OBSERVED' })
    }
}

function Assert-CtrlTimingEvidence {
    param([object[]] $Records, [object] $Startup, [object] $Summary,
          [object] $Facts, [object] $Activation)
    $frequency = Get-EvidenceInteger $Startup 'qpc_frequency_hz'
    $capacity = Get-EvidenceInteger $Startup 'timing_capacity'
    $activationCapacity = Get-EvidenceInteger $Startup 'activation_capacity'
    $operationCapacity = Get-EvidenceInteger $Startup 'operation_capacity'
    # These are evidence storage ceilings, not rate/latency acceptance targets.
    if ($capacity -ne 4096 -or $activationCapacity -ne 8 -or $operationCapacity -ne 512) {
        throw 'Timing/activation storage capacity exceeds the bounded evidence contract.'
    }
    foreach ($record in @($Startup, $Facts, $Summary)) {
        Assert-EvidenceBoolean $record @('timing_evidence_overflow', 'activation_evidence_overflow')
        if ($record.timing_evidence_overflow -or $record.activation_evidence_overflow) {
            throw 'Timing/activation evidence overflow: preserve diagnostics; acceptance is unavailable.'
        }
    }
    Assert-EvidenceBoolean $Facts @('timing_valid', 'ctrl_move_activation_required')
    Assert-EvidenceBoolean $Summary @('timing_valid')
    Assert-RecordProperties $Facts @('qpc_frequency_hz', 'activation_generation',
        'pair_authority_generation') 'Ctrl timing facts'
    if (-not $Facts.timing_valid -or -not $Summary.timing_valid -or
        -not $Facts.ctrl_move_activation_required -or $Facts.qpc_frequency_hz -ne $frequency -or
        $Facts.activation_generation -ne $Activation.activation_generation -or
        $Facts.pair_authority_generation -ne $Activation.pair_authority_generation) {
        throw 'Timing validity/frequency/activation facts do not match the sealed evidence chain.'
    }
    if ($Summary.timing_evidence_overflow -or $Summary.activation_evidence_overflow) {
        throw 'Timing/activation evidence overflow: preserve diagnostics; acceptance is unavailable.'
    }
    $receipts = @($Records | Where-Object { $_.record_kind -eq 'event_receipt' })
    $quanta = @($Records | Where-Object { $_.record_kind -eq 'processing_quantum' })
    $traces = @($Records | Where-Object { $_.record_kind -eq 'internal_trace' })
    $allOperations = @($Records | Where-Object { $_.record_kind -eq 'operation' })
    $operations = @($allOperations | Where-Object { $_.phase -eq 'active_follower' })
    if ($receipts.Count -gt $capacity -or $quanta.Count -gt $capacity -or
        $traces.Count -gt $capacity -or $allOperations.Count -gt $operationCapacity -or
        @($Records | Where-Object { $_.record_kind -eq 'activation_attempt' }).Count -gt $activationCapacity) {
        throw 'Recorded timing/activation samples exceed advertised storage capacity.'
    }
    if ($Facts.glue_session_generation -ne $Activation.glue_session_generation) {
        throw 'Activation does not bind the recorded Glue session generation.'
    }
    $receiptMap = @{}
    $quantumMap = @{}
    $lastCallback = [uint64] 0
    foreach ($receipt in $receipts) {
        $stamp = Get-EvidenceInteger $receipt 'callback_qpc'
        Assert-EvidenceBoolean $receipt @('ctrl_sample_available',
            'ctrl_down_at_callback_delivery', 'left_ctrl_down', 'right_ctrl_down')
        if (($receipt.role -ne 'leader' -or $receipt.event_kind -ne 'move_resize_started') -and
            ($receipt.ctrl_sample_available -or $receipt.ctrl_down_at_callback_delivery -or
             $receipt.left_ctrl_down -or $receipt.right_ctrl_down)) {
            throw 'Ctrl sampling is limited to the authorized Leader START receipt.'
        }
        if ($stamp -lt $lastCallback) { throw 'Callback QPC sequence regressed.' }
        $lastCallback = $stamp
        $receiptMap[[uint64]$receipt.receipt_sequence] = $receipt
    }
    $durations = [ordered]@{
        'activation-decision' = [Collections.Generic.List[double]]::new()
        'apply-interval' = [Collections.Generic.List[double]]::new()
        'receipt-to-owner' = [Collections.Generic.List[double]]::new()
        'owner-to-native' = [Collections.Generic.List[double]]::new()
        'native-call' = [Collections.Generic.List[double]]::new()
        'native-to-postverify' = [Collections.Generic.List[double]]::new()
        'receipt-to-exact-postverify' = [Collections.Generic.List[double]]::new()
    }
    [void]$durations['activation-decision'].Add(
        1000.0 * ([decimal]$Activation.decision_qpc - [decimal]$Activation.callback_qpc) / $frequency)
    $lastDrain = [uint64] 0
    foreach ($quantum in $quanta) {
        $drain = Get-EvidenceInteger $quantum 'owner_drain_start_qpc'
        if ($drain -lt $lastDrain) { throw 'Owner drain QPC sequence regressed.' }
        $lastDrain = $drain
        $quantumMap[[uint64]$quantum.processing_quantum_id] = $quantum
        foreach ($receipt in @($receipts | Where-Object {
            $_.processing_quantum_id -eq $quantum.processing_quantum_id
        })) {
            if ([uint64]$receipt.callback_qpc -gt $drain) {
                throw 'Receipt callback must not follow the quantum drain start.'
            }
            if (-not $receipt.discarded_after_end) {
                [void]$durations['receipt-to-owner'].Add(
                    1000.0 * ([decimal]$drain - [decimal]$receipt.callback_qpc) / $frequency)
            }
        }
        if (-not $quantum.inactive_discard) {
            $sampleStart = Get-EvidenceInteger $quantum 'sample_start_qpc'
            $sampleEnd = Get-EvidenceInteger $quantum 'sample_complete_qpc'
            $decision = Get-EvidenceInteger $quantum 'behavior_decision_qpc'
            if ($drain -gt $sampleStart -or $sampleStart -gt $sampleEnd -or $sampleEnd -gt $decision) {
                throw 'Quantum drain/sample/behavior timestamp ordering is invalid.'
            }
        }
    }
    $lastNativeStart = $null
    foreach ($operation in $operations) {
        foreach ($name in @('activation_generation', 'behavior_decision_qpc', 'native_apply_start_qpc',
            'native_api_return_qpc', 'postverify_complete_qpc')) {
            [void] (Get-EvidenceInteger $operation $name)
        }
        $quantum = $quantumMap[[uint64]$operation.processing_quantum_id]
        $receipt = $receiptMap[[uint64]$operation.source_leader_sequence]
        if ($null -eq $quantum -or $null -eq $receipt -or
            $operation.activation_generation -ne $Activation.activation_generation -or
            $Activation.activation_generation -eq 0 -or
            $Activation.decision_qpc -gt $operation.native_apply_start_qpc -or
            $quantum.behavior_decision_qpc -gt $operation.native_apply_start_qpc -or
            $quantum.sample_complete_qpc -gt $operation.behavior_decision_qpc -or
            $operation.behavior_decision_qpc -gt $operation.native_apply_start_qpc -or
            $operation.native_apply_start_qpc -gt $operation.native_api_return_qpc -or
            $operation.native_api_return_qpc -gt $operation.postverify_complete_qpc) {
            throw 'Operation activation/quantum/native/postverify timing linkage is invalid.'
        }
        if ($null -ne $lastNativeStart) {
            if ($operation.native_apply_start_qpc -lt $lastNativeStart) {
                throw 'Follower native start QPC sequence regressed.'
            }
            [void]$durations['apply-interval'].Add(
                1000.0 * ([decimal]$operation.native_apply_start_qpc - [decimal]$lastNativeStart) / $frequency)
        }
        $lastNativeStart = $operation.native_apply_start_qpc
        [void]$durations['owner-to-native'].Add(1000.0 *
            ([decimal]$operation.native_apply_start_qpc - [decimal]$quantum.owner_drain_start_qpc) / $frequency)
        [void]$durations['native-call'].Add(1000.0 *
            ([decimal]$operation.native_api_return_qpc - [decimal]$operation.native_apply_start_qpc) / $frequency)
        [void]$durations['native-to-postverify'].Add(1000.0 *
            ([decimal]$operation.postverify_complete_qpc - [decimal]$operation.native_apply_start_qpc) / $frequency)
        [void]$durations['receipt-to-exact-postverify'].Add(1000.0 *
            ([decimal]$operation.postverify_complete_qpc - [decimal]$receipt.callback_qpc) / $frequency)
    }
    foreach ($trace in $traces) {
        [void] (Get-EvidenceInteger $trace 'ack_qpc' -AllowZero)
        if ($trace.decision -notin @('feedback_acknowledged', 'duplicate_feedback_suppressed')) {
            if ($trace.ack_qpc -ne 0) { throw 'Only a completed ACK/duplicate suppression may carry ack_qpc.' }
            continue
        }
        $ack = Get-EvidenceInteger $trace 'ack_qpc'
        $feedbackSequence = [uint64]$trace.event_sequence
        if ($feedbackSequence -eq 0) {
            # A provisional event becomes an ACK only once native postverify
            # completes. The deferred decision has no fresh native receipt.
            $operation = @($operations | Where-Object {
                $_.behavior_operation_generation -eq $trace.behavior_operation_generation
            })
            $reconciliation = @($Records | Where-Object {
                $_.record_kind -eq 'feedback_reconciliation' -and
                $_.operation_generation -eq $trace.behavior_operation_generation
            })
            if ($trace.decision -ne 'feedback_acknowledged' -or $operation.Count -ne 1 -or
                $reconciliation.Count -ne 1 -or
                $reconciliation[0].disposition -ne 'acknowledged_self_feedback' -or
                $null -eq $reconciliation[0].feedback_event_sequence -or
                $operation[0].postverify_complete_qpc -gt $ack) {
                throw 'Deferred ACK lacks its exact completed native operation/reconciliation.'
            }
            $feedbackSequence = [uint64]$reconciliation[0].feedback_event_sequence
            $provisional = @($traces | Where-Object {
                $_.event_sequence -eq $feedbackSequence -and
                $_.decision -eq 'feedback_observed_pending_result' -and
                $_.trace_sequence -lt $trace.trace_sequence -and
                (Get-RectKey $_.visible) -eq (Get-RectKey $operation[0].requested_visible)
            })
            if ($provisional.Count -ne 1) { throw 'Deferred ACK must follow one matching provisional feedback event.' }
        }
        $receipt = $receiptMap[$feedbackSequence]
        $quantum = $quantumMap[[uint64]$trace.processing_quantum_id]
        if ($null -eq $receipt -or $receipt.role -ne 'follower' -or
            $receipt.callback_qpc -gt $ack -or $null -eq $quantum -or
            $quantum.sample_complete_qpc -gt $ack -or $quantum.behavior_decision_qpc -gt $ack) {
            throw 'Feedback ACK precedes or lacks its callback receipt.'
        }
    }
    foreach ($name in $durations.Keys) {
        $stats = Get-TimingStatistics -Samples $durations[$name].ToArray()
        Write-Output ("Timing {0}: {1}; count={2}; min={3}; p50={4}; p95={5}; max={6} ms" -f
            $name, $stats.status, $stats.count, $stats.min, $stats.p50, $stats.p95, $stats.max)
    }
    Write-Output 'TIMING_EVIDENCE_GATE: PASS (consistency only; no latency SLA)'
}
