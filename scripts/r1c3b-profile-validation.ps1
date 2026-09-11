# Phase 1 diagnostics only; called AFTER all frozen C3A correctness gates.
# These intervals are CPU-side observations, never presentation latency/FPS.
function Write-ProfileStatistic {
    param([string] $Name, [object[]] $Ticks, [uint64] $Frequency)
    $values = [double[]]@($Ticks | ForEach-Object { [double]([decimal]$_ * 1000 / $Frequency) })
    $stats = Get-TimingStatistics -Samples $values
    Write-Output ("PROFILE {0}: count={1}; p50={2}; p95={3}; max={4} ms; {5}" -f
        $Name, $stats.count, $stats.p50, $stats.p95, $stats.max, $stats.status)
}

function Assert-StageProfileEvidence {
    param([object[]] $Records, [object] $Startup, [object] $Summary, [object] $Facts, [switch] $Phase2, [switch] $Phase3)
    $frequency = Get-EvidenceInteger $Startup 'qpc_frequency_hz'
    $status = Assert-UniqueRecord $Records 'profile_status'
    foreach ($r in @($Startup,$status)) {
        Assert-EvidenceBoolean $r @('profiling_enabled','external_observer_enabled')
        if (-not $r.profiling_enabled -or -not $r.external_observer_enabled) {
            throw 'Phase 1 accepted profile requires profiling and independent external Observer ON.'
        }
    }
    Assert-EvidenceBoolean $status @('profile_overflow','profile_invalid')
    if ($status.profile_overflow -or $status.profile_invalid -or
        $status.timing_profile_gate -ne 'PASS' -or $Summary.timing_profile_gate -ne 'PASS' -or
        $status.glue_session_generation -ne $Facts.glue_session_generation -or
        $status.activation_generation -ne $Facts.activation_generation) {
        throw 'Profile failure, incomplete scope or mismatched runtime generation.'
    }
    $spans = @($Records | Where-Object record_kind -eq 'profile_span')
    $quanta = @($Records | Where-Object record_kind -eq 'profile_quantum')
    $notifications = @($Records | Where-Object record_kind -eq 'profile_notification')
    foreach ($kind in @('span','quantum','notification')) {
        $count = Get-EvidenceInteger $status ($kind + '_count')
        $capacity = Get-EvidenceInteger $status ($kind + '_capacity')
        $expectedCapacity = if ($kind -eq 'span') { 32768 } else { 4096 }
        $actualCount = switch ($kind) { 'span' { $spans.Count } 'quantum' { $quanta.Count } 'notification' { $notifications.Count } }
        if ($capacity -ne $expectedCapacity -or $count -ne $actualCount -or $count -gt $capacity) {
            throw 'Profile count/capacity mismatch, missing data or unbounded storage.'
        }
    }
    [void](Get-EvidenceInteger $status 'profile_qpc_reads')
    $raw = @($Records | Where-Object record_kind -eq 'event_receipt')
    $baseQuanta = @($Records | Where-Object { $_.record_kind -eq 'processing_quantum' -and -not $_.inactive_discard })
    $operations = @($Records | Where-Object { $_.record_kind -eq 'operation' -and $_.phase -eq 'active_follower' })
    $rawMap = @{}; $baseMap = @{}; $qMap = @{}; $opMap = @{}
    foreach ($r in $raw) { $rawMap[[uint64]$r.receipt_sequence] = $r }
    foreach ($q in $baseQuanta) { $baseMap[[uint64]$q.processing_quantum_id] = $q }
    foreach ($op in $operations) { $opMap[[uint64]$op.behavior_operation_generation] = $op }
    if ($quanta.Count -ne $baseQuanta.Count) { throw 'Every live quantum needs exactly one profile envelope.' }
    $previous = $null
    foreach ($q in $quanta) {
        foreach ($name in @('quantum_id','first_receipt_sequence','last_receipt_sequence',
            'raw_receipt_count','first_callback_receipt_qpc','quantum_start_qpc',
            'drain_complete_qpc','quantum_end_qpc','begin_receipt_watermark','end_receipt_watermark')) {
            [void](Get-EvidenceInteger $q $name)
        }
        foreach ($name in @('leader_location_count','follower_location_count','coalesced_leader_location_count',
            'queue_depth_before_drain','queue_depth_after_drain','queue_depth_at_end','max_queue_depth',
            'receipts_arrived_during_quantum','receipts_arrived_during_native','receipts_arrived_during_postverify',
            'previous_quantum_id','receipts_arrived_during_previous_quantum',
            'receipts_arrived_during_previous_native_operation','receipts_arrived_during_previous_postverify')) {
            [void](Get-EvidenceInteger $q $name -AllowZero)
        }
        $id = [uint64]$q.quantum_id
        if ($qMap.ContainsKey($id) -or -not $baseMap.ContainsKey($id)) { throw 'Duplicate/orphan profile quantum.' }
        $base = $baseMap[$id]; $qMap[$id] = $q
        $receipts = @($raw | Where-Object processing_quantum_id -eq $id)
        $coalesced = @($receipts | Where-Object { $_.role -eq 'leader' -and $_.event_kind -eq 'geometry_changed' -and $_.coalesced }).Count
        if ($q.first_receipt_sequence -ne $base.first_receipt_sequence -or
            $q.last_receipt_sequence -ne $base.last_receipt_sequence -or $q.raw_receipt_count -ne $base.receipt_count -or
            $q.leader_location_count -ne $base.leader_location_count -or $q.follower_location_count -ne $base.follower_location_count -or
            $q.coalesced_leader_location_count -ne $coalesced -or $q.coalesced_leader_location_count -gt $q.leader_location_count -or
            $q.first_callback_receipt_qpc -ne $receipts[0].callback_qpc -or
            $q.first_callback_receipt_qpc -gt $q.quantum_start_qpc -or
            $q.quantum_start_qpc -gt $base.owner_drain_start_qpc -or
            $base.owner_drain_start_qpc -gt $q.drain_complete_qpc -or
            $q.drain_complete_qpc -gt $base.sample_start_qpc -or
            $base.behavior_decision_qpc -gt $q.quantum_end_qpc -or
            $q.queue_depth_before_drain -ne $q.raw_receipt_count -or $q.queue_depth_after_drain -ne 0 -or
            $q.max_queue_depth -gt 512 -or $q.queue_depth_at_end -gt $q.max_queue_depth -or
            $q.queue_depth_before_drain -gt $q.max_queue_depth -or
            $q.begin_receipt_watermark -ne $q.last_receipt_sequence -or
            $q.end_receipt_watermark -lt $q.begin_receipt_watermark -or
            $q.end_receipt_watermark -gt $raw.Count -or
            $q.receipts_arrived_during_quantum -ne ($q.end_receipt_watermark - $q.begin_receipt_watermark) -or
            $q.queue_depth_at_end -ne $q.receipts_arrived_during_quantum) {
            throw 'Quantum timeline/queue-depth/coalescing/watermark arithmetic is invalid.'
        }
        $previousId = if ($null -eq $previous) { 0 } else { $previous.quantum_id }
        $previousArrivals = if ($null -eq $previous) { 0 } else { $previous.receipts_arrived_during_quantum }
        $previousNative = if ($null -eq $previous) { 0 } else { $previous.receipts_arrived_during_native }
        $previousPost = if ($null -eq $previous) { 0 } else { $previous.receipts_arrived_during_postverify }
        if ($q.previous_quantum_id -ne $previousId -or
            $q.receipts_arrived_during_previous_quantum -ne $previousArrivals -or
            $q.receipts_arrived_during_previous_native_operation -ne $previousNative -or
            $q.receipts_arrived_during_previous_postverify -ne $previousPost -or
            ($null -ne $previous -and $previous.quantum_end_qpc -gt $q.quantum_start_qpc)) {
            throw 'Previous-quantum backlog attribution is invalid.'
        }
        $previous = $q
    }
    $stages = @('quantum','drain','leader_capture','follower_capture','full_validation',
        'token_ledger','shell_observation','shell_inventory','shell_location','native_identity',
        'process_image','window_structure','window_state','virtual_desktop','process_security',
        'positioning_bounds','visible_frame','monitor_dpi','eligibility_finalize','event_policy',
        'activation_policy','start_geometry','core_decision','feedback_policy','operation',
        'operation_prepare','prepare_validation','positioning_plan','immediate_validation',
        'pending_registration','native_placement','postverify','postverify_validation',
        'exact_comparison','receipt_finalize','operation_result_policy','end_leader_capture',
        'end_follower_capture','end_reconciliation')
    if ($Phase2) { $stages += @('consent_bound_validation','global_inventory_fallback','validation_invalidation','pair_witness') }
    if ($Phase3) { $stages += @('virtual_desktop_manager_acquire','virtual_desktop_query') }
    $spanMap = @{}; $children = @{}; $exclusive = @{}
    $nextId = [uint64]1
    foreach ($s in $spans) {
        foreach ($name in @('span_id','quantum_id','begin_qpc','end_qpc')) { [void](Get-EvidenceInteger $s $name) }
        foreach ($name in @('parent_span_id','operation_generation','source_receipt',
            'begin_receipt_watermark','end_receipt_watermark')) { [void](Get-EvidenceInteger $s $name -AllowZero) }
        $id = [uint64]$s.span_id; $parent = [uint64]$s.parent_span_id; $qid = [uint64]$s.quantum_id
        if ($id -ne $nextId -or $stages -notcontains $s.stage -or $s.role -notin @('leader','follower') -or
            -not $qMap.ContainsKey($qid) -or $parent -ge $id -or $s.end_qpc -lt $s.begin_qpc -or
            $s.end_receipt_watermark -lt $s.begin_receipt_watermark -or $s.end_receipt_watermark -gt $raw.Count) {
            throw 'Span IDs, enums, parent relation or monotonic timestamps are invalid.'
        }
        ++$nextId
        $q = $qMap[$qid]
        if ($s.begin_qpc -lt $q.quantum_start_qpc -or $s.end_qpc -gt $q.quantum_end_qpc) { throw 'Span escapes its quantum.' }
        if ($parent) {
            if (-not $spanMap.ContainsKey($parent)) { throw 'Missing parent span.' }
            $p = $spanMap[$parent]
            if ($p.quantum_id -ne $qid -or $s.begin_qpc -lt $p.begin_qpc -or $s.end_qpc -gt $p.end_qpc -or
                $s.begin_receipt_watermark -lt $p.begin_receipt_watermark -or $s.end_receipt_watermark -gt $p.end_receipt_watermark -or
                ($p.operation_generation -ne 0 -and $s.operation_generation -ne $p.operation_generation)) {
                throw 'Child span escapes parent time/quantum/operation.'
            }
        } elseif ($s.stage -ne 'quantum') { throw 'Only a quantum may be a root span.' }
        if ($s.source_receipt -ne 0 -and (-not $rawMap.ContainsKey([uint64]$s.source_receipt) -or
            $rawMap[[uint64]$s.source_receipt].processing_quantum_id -ne $qid)) { throw 'Span source receipt is orphaned.' }
        if ($s.operation_generation -ne 0) {
            $op = $opMap[[uint64]$s.operation_generation]
            if ($null -eq $op -or $op.processing_quantum_id -ne $qid -or
                $op.source_leader_sequence -ne $s.source_receipt -or $s.role -ne 'follower') {
                throw 'Stage is not bound to its exact active operation/Leader source.'
            }
        }
        $spanMap[$id] = $s
        for ($sequence = [uint64]$s.begin_receipt_watermark + 1; $sequence -le [uint64]$s.end_receipt_watermark; ++$sequence) {
            $r = $rawMap[$sequence]
            if ($r.callback_qpc -lt $s.begin_qpc -or $r.callback_qpc -gt $s.end_qpc) {
                throw 'Delivered receipt watermark contradicts the measured span clock.'
            }
        }
        if (-not $children.ContainsKey($parent)) { $children[$parent] = [Collections.Generic.List[object]]::new() }
        [void]$children[$parent].Add($s)
    }
    foreach ($s in $spans) {
        $duration = [decimal]$s.end_qpc - [decimal]$s.begin_qpc
        $lastEnd = [decimal]$s.begin_qpc
        $id = [uint64]$s.span_id
        if ($children.ContainsKey($id)) {
            foreach ($child in $children[$id]) {
                if ($child.begin_qpc -lt $lastEnd) { throw 'Sibling spans overlap; exclusive costs cannot be added.' }
                $duration -= ([decimal]$child.end_qpc - [decimal]$child.begin_qpc)
                $lastEnd = [decimal]$child.end_qpc
            }
        }
        if ($duration -lt 0) { throw 'Negative exclusive duration.' }
        $exclusive[$id] = $duration
    }
    foreach ($q in $quanta) {
        $qs = @($spans | Where-Object quantum_id -eq $q.quantum_id)
        foreach ($kind in @('quantum','drain','leader_capture','follower_capture')) {
            if (@($qs | Where-Object stage -eq $kind).Count -ne 1) { throw "Missing/duplicate required $kind stage." }
        }
        $nativeArrivals = [decimal]0; $postArrivals = [decimal]0
        foreach ($s in $qs) {
            if ($s.stage -eq 'native_placement') { $nativeArrivals += $s.end_receipt_watermark - $s.begin_receipt_watermark }
            if ($s.stage -eq 'postverify') { $postArrivals += $s.end_receipt_watermark - $s.begin_receipt_watermark }
        }
        if ($nativeArrivals -ne $q.receipts_arrived_during_native -or
            $postArrivals -ne $q.receipts_arrived_during_postverify) { throw 'Native/postverify backlog does not match measured spans.' }
        if ($q.leader_location_count -gt 0 -and @($qs | Where-Object stage -eq 'core_decision').Count -eq 0 -and
            $Summary.activation_count -ne 0) { throw 'Active Leader quantum lacks a measured Core decision.' }
    }
    foreach ($r in @($raw | Where-Object { -not $_.coalesced -and -not $_.discarded_after_end })) {
        $decisions=@($spans | Where-Object { $_.stage -eq 'core_decision' -and
            $_.source_receipt -eq $r.receipt_sequence -and $_.operation_generation -eq 0 })
        $expected=if ($Summary.activation_count -eq 1) {1} else {0}
        if ($decisions.Count -ne $expected -or ($expected -eq 1 -and $decisions[0].role -ne $r.role)) {
            throw 'Core decision profiling does not match the exact processed receipt set.'
        }
    }
    # Every successful full validation must retain the same measured predicates.
    $captureParts = @('token_ledger','shell_observation','shell_inventory','shell_location',
        'native_identity','process_image','window_structure','window_state','virtual_desktop',
        'process_security','positioning_bounds','visible_frame','monitor_dpi','eligibility_finalize')
    foreach ($capture in @($spans | Where-Object { $_.stage -eq 'full_validation' -or ($Phase2 -and $_.stage -eq 'consent_bound_validation') })) {
        $parts = @($children[[uint64]$capture.span_id])
        $requiredParts = if ($capture.stage -eq 'consent_bound_validation') { @($captureParts | Where-Object { $_ -ne 'shell_inventory' }) } else { $captureParts }
        if (($parts.stage -join '|') -ne ($requiredParts -join '|')) { throw 'Full-validation stage order or coverage changed.' }
        foreach ($kind in $requiredParts) {
            if (@($parts | Where-Object stage -eq $kind).Count -ne 1) { throw "Full capture lacks measured $kind." }
        }
    }
    foreach ($capture in @($spans | Where-Object { $_.stage -in @(
        'leader_capture','follower_capture','prepare_validation','immediate_validation',
        'postverify_validation','end_leader_capture','end_follower_capture') })) {
        $full = @($children[[uint64]$capture.span_id] | Where-Object { $_.stage -eq 'full_validation' -or ($Phase2 -and $_.stage -eq 'consent_bound_validation') })
        if ($full.Count -ne 1 -or $full[0].role -ne $capture.role) { throw 'Capture lacks its exact role-bound full validation.' }
    }
    if (@($spans | Where-Object stage -eq 'activation_policy').Count -ne 1) { throw 'Missing/duplicate START activation profiling.' }
    if ($Summary.activation_count -eq 1) {
        foreach ($kind in @('end_leader_capture','end_follower_capture','end_reconciliation')) {
            if (@($spans | Where-Object stage -eq $kind).Count -ne 1) { throw 'Missing final reconciliation profiling.' }
        }
    }
    $operationRows = [Collections.Generic.List[object]]::new()
    $largestCounts = @{}; $matchedStageTotals = @{}
    foreach ($op in $operations) {
        $os = @($spans | Where-Object operation_generation -eq $op.behavior_operation_generation)
        foreach ($kind in @('operation','prepare_validation','immediate_validation','pending_registration',
            'native_placement','postverify','postverify_validation','exact_comparison')) {
            if (@($os | Where-Object stage -eq $kind).Count -ne 1) { throw "Operation lacks unique $kind." }
        }
        $root = @($os | Where-Object stage -eq 'operation')[0]
        $native = @($os | Where-Object stage -eq 'native_placement')[0]
        $post = @($os | Where-Object stage -eq 'postverify')[0]
        if ($native.begin_qpc -lt $op.native_apply_start_qpc -or $native.end_qpc -gt $op.native_api_return_qpc -or
            $post.begin_qpc -lt $op.native_api_return_qpc -or $post.end_qpc -gt $op.postverify_complete_qpc -or
            $root.end_qpc -lt $op.postverify_complete_qpc -or
            $root.begin_qpc -lt $op.behavior_decision_qpc) { throw 'Profile and existing operation timeline disagree.' }
        $totals = @{}
        foreach ($s in $os) {
            # Operation-result/ACK work may be later and is a separate interval.
            if ($s.begin_qpc -lt $root.begin_qpc -or $s.end_qpc -gt $root.end_qpc) { continue }
            $ticks = $exclusive[[uint64]$s.span_id]
            if (-not $totals.ContainsKey($s.stage)) { $totals[$s.stage] = [decimal]0 }
            $totals[$s.stage] += $ticks
        }
        $total = [decimal]0
        foreach ($name in $totals.Keys) {
            $total += $totals[$name]
            if (-not $matchedStageTotals.ContainsKey($name)) { $matchedStageTotals[$name] = [decimal]0 }
            $matchedStageTotals[$name] += $totals[$name]
        }
        if ($total -ne ([decimal]$root.end_qpc - [decimal]$root.begin_qpc)) { throw 'Exclusive operation partition does not close.' }
        $largest = @($totals.GetEnumerator() | Sort-Object -Property @{Expression='Value';Descending=$true},Name)[0]
        if (-not $largestCounts.ContainsKey($largest.Key)) { $largestCounts[$largest.Key] = 0 }
        ++$largestCounts[$largest.Key]
        $parts = @($totals.GetEnumerator() | Sort-Object Name | ForEach-Object { '{0}={1}' -f $_.Key, $_.Value })
        [void]$operationRows.Add([pscustomobject]@{ generation=$op.behavior_operation_generation; quantum=$op.processing_quantum_id
            source=$op.source_leader_sequence; total_ticks=$total; largest=$largest.Key; parts=($parts -join ',') })
    }
    $notificationMap = @{}; $edgeTicks = @(); $dispatchTicks = @(); $drainTicks = @()
    $nextId = [uint64]1; $inherited = 0; $unobserved = 0; $drainedFirst = 0
    foreach ($n in $notifications) {
        foreach ($name in @('notification_id','trigger_receipt','callback_qpc','owner_notification_qpc')) { [void](Get-EvidenceInteger $n $name) }
        [void](Get-EvidenceInteger $n 'owner_message_dispatch_qpc' -AllowZero)
        Assert-EvidenceBoolean $n @('post_succeeded','dispatch_observed')
        $r = $rawMap[[uint64]$n.trigger_receipt]
        if ($n.notification_id -ne $nextId -or $null -eq $r -or $r.notification_id -ne $nextId -or
            $r.notification_inherited -or $n.callback_qpc -ne $r.callback_qpc -or
            -not $n.post_succeeded -or $n.owner_notification_qpc -lt $n.callback_qpc -or
            $n.dispatch_observed -ne ($n.owner_message_dispatch_qpc -gt 0)) { throw 'Notification edge provenance is invalid.' }
        ++$nextId; $notificationMap[[uint64]$n.notification_id] = $n
        $edgeTicks += ([decimal]$n.owner_notification_qpc - [decimal]$n.callback_qpc)
        if ($n.dispatch_observed) {
            if ($n.owner_message_dispatch_qpc -lt $n.owner_notification_qpc) { throw 'Dispatch precedes notification.' }
            $dispatchTicks += ([decimal]$n.owner_message_dispatch_qpc - [decimal]$n.owner_notification_qpc)
            if ($baseMap.ContainsKey([uint64]$r.processing_quantum_id)) {
                $drain = $baseMap[[uint64]$r.processing_quantum_id].owner_drain_start_qpc
                if ($drain -ge $n.owner_message_dispatch_qpc) { $drainTicks += ([decimal]$drain - [decimal]$n.owner_message_dispatch_qpc) }
                else { ++$drainedFirst }
            }
        } else { ++$unobserved }
    }
    foreach ($r in $raw) {
        [void](Get-EvidenceInteger $r 'notification_id')
        Assert-EvidenceBoolean $r @('notification_inherited')
        $n = $notificationMap[[uint64]$r.notification_id]
        if ($null -eq $n -or $n.trigger_receipt -gt $r.receipt_sequence -or
            $r.notification_inherited -ne ($n.trigger_receipt -ne $r.receipt_sequence)) { throw 'Receipt pending-notification inheritance is false.' }
        if ($r.notification_inherited) { ++$inherited }
    }
    $observedDispatches = @($notifications | Where-Object dispatch_observed).Count
    if ($status.profile_qpc_reads -ne (2 * $spans.Count + 3 * $quanta.Count + $notifications.Count + $observedDispatches)) {
        throw 'Profile QPC read count does not match emitted scopes/quantum/notification points.'
    }
    Write-Output 'QUEUE / SCHEDULING (delivered callbacks, not OS generation)'
    Write-ProfileStatistic 'callback->notification (post edges only)' $edgeTicks $frequency
    Write-ProfileStatistic 'notification->actual dispatch' $dispatchTicks $frequency
    Write-ProfileStatistic 'dispatch->drain (only dispatch observed before drain)' $drainTicks $frequency
    Write-Output "notification inherited/already pending=$inherited; dispatch NOT OBSERVED=$unobserved; drain before dispatch=$drainedFirst"
    foreach ($q in $quanta) {
        Write-Output ("QUEUE quantum={0} depth before/after/end={1}/{2}/{3}; max={4}; coalesced={5}; previous quantum/native/postverify arrivals={6}/{7}/{8}" -f
            $q.quantum_id,$q.queue_depth_before_drain,$q.queue_depth_after_drain,$q.queue_depth_at_end,
            $q.max_queue_depth,$q.coalesced_leader_location_count,$q.receipts_arrived_during_previous_quantum,
            $q.receipts_arrived_during_previous_native_operation,$q.receipts_arrived_during_previous_postverify)
    }
    foreach ($stage in $stages) {
        $found = @($spans | Where-Object stage -eq $stage)
        if ($found.Count) {
            Write-ProfileStatistic "$stage inclusive (invocation grain)" @($found | ForEach-Object { [decimal]$_.end_qpc - [decimal]$_.begin_qpc }) $frequency
            if ($stage -eq 'core_decision') {
                foreach ($role in @('leader','follower')) {
                    Write-ProfileStatistic "core_decision/$role" @($found | Where-Object role -eq $role | ForEach-Object { [decimal]$_.end_qpc-[decimal]$_.begin_qpc }) $frequency
                }
            }
        }
    }
    $feedbackLatency = @($Records | Where-Object { $_.record_kind -eq 'internal_trace' -and $_.decision -in @('feedback_acknowledged','duplicate_feedback_suppressed') } |
        Where-Object { $_.event_sequence -gt 0 } | ForEach-Object {
            [decimal]$_.ack_qpc - [decimal]$rawMap[[uint64]$_.event_sequence].callback_qpc
        })
    Write-ProfileStatistic 'feedback callback->ACK/suppress' $feedbackLatency $frequency
    $hot = @{}
    foreach ($s in $spans) {
        if (-not $hot.ContainsKey($s.stage)) { $hot[$s.stage] = [decimal]0 }
        $hot[$s.stage] += $exclusive[[uint64]$s.span_id]
    }
    Write-Output 'TOP OBSERVED HOT STAGES (exclusive measured scope time; no overlapping parent sums)'
    $hot.GetEnumerator() | Sort-Object -Property @{Expression='Value';Descending=$true},Name | Select-Object -First 8 | ForEach-Object {
        Write-Output ("HOT {0}: {1} ms exclusive" -f $_.Key, ([decimal]$_.Value * 1000 / $frequency))
    }
    foreach ($row in $operationRows) {
        Write-Output ("OP_PROFILE generation={0} quantum={1} source={2} total_ticks={3} largest={4}; exclusive_ticks: {5}" -f
            $row.generation,$row.quantum,$row.source,$row.total_ticks,$row.largest,$row.parts)
    }
    Write-Output 'MATCHED OPERATION AGGREGATE: operation-local exclusive intervals only; source-quantum captures are shared context, NOT added again.'
    $aggregate = [decimal]0
    foreach ($value in $matchedStageTotals.Values) { $aggregate += $value }
    foreach ($entry in @($matchedStageTotals.GetEnumerator() | Sort-Object Name)) {
        $share = if ($aggregate -gt 0) { 100 * [decimal]$entry.Value / $aggregate } else { 0 }
        Write-Output ("MATCHED {0}: ticks={1}; share={2}%" -f $entry.Key,$entry.Value,$share)
    }
    foreach ($entry in @($largestCounts.GetEnumerator() | Sort-Object Name)) {
        Write-Output ("LARGEST {0}: {1}/{2} operations" -f $entry.Key,$entry.Value,$operations.Count)
    }
    Write-Output 'TIMING_PROFILE_GATE: PASS (consistency, not smoothness SLA; no percentile addition)'
    Write-Output 'EXTERNAL_OBSERVER_PERTURBATION: UNRESOLVED; CPU-side timing is not DWM presentation/FPS.'
}
