[CmdletBinding()]
param(
    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string] $BuildDirectory = 'out/r1c2b-debug',

    [Parameter()]
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Debug',

    [Parameter()]
    [ValidateRange(30, 300)]
    [int] $GlueTimeoutSeconds = 120,

    [Parameter()]
    [ValidateRange(120, 1800)]
    [int] $ObserveSeconds = 300
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($ObserveSeconds -lt ($GlueTimeoutSeconds + 30)) {
    throw 'ObserveSeconds 必须比 GlueTimeoutSeconds 至少多 30 秒，以覆盖授权和 cleanup。'
}

$repositoryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path -Path $PSScriptRoot -ChildPath '..'))
$buildRoot = if ([System.IO.Path]::IsPathRooted($BuildDirectory)) {
    [System.IO.Path]::GetFullPath($BuildDirectory)
} else {
    [System.IO.Path]::GetFullPath(
        (Join-Path -Path $repositoryRoot -ChildPath $BuildDirectory))
}

function Resolve-R1C2BExecutable {
    param(
        [Parameter(Mandatory = $true)]
        [string] $FileName
    )

    $platformOutput = Join-Path -Path $buildRoot -ChildPath 'src/platform/windows'
    $candidates = @(
        (Join-Path -Path $platformOutput -ChildPath "$Configuration/$FileName"),
        (Join-Path -Path $platformOutput -ChildPath $FileName),
        (Join-Path -Path $buildRoot -ChildPath "$Configuration/$FileName"),
        (Join-Path -Path $buildRoot -ChildPath $FileName)
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    throw "找不到必需的可执行文件：$FileName"
}

function Read-StrictJsonLines {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path
    )

    $records = [Collections.Generic.List[object]]::new()
    foreach ($line in [IO.File]::ReadLines($Path)) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            throw "JSONL 含空行：$Path"
        }
        try {
            $records.Add(($line | ConvertFrom-Json -ErrorAction Stop))
        } catch {
            throw "JSONL 解析失败：$Path"
        }
    }
    if ($records.Count -eq 0) {
        throw "JSONL 为空：$Path"
    }
    return $records.ToArray()
}

function Assert-UniqueRecord {
    param(
        [Parameter(Mandatory = $true)]
        [object[]] $Records,

        [Parameter(Mandatory = $true)]
        [string] $Kind
    )

    $matches = @($Records | Where-Object { $_.record_kind -eq $Kind })
    if ($matches.Count -ne 1) {
        throw "Harness record_kind '$Kind' 必须恰好出现一次，实际 $($matches.Count)。"
    }
    return $matches[0]
}

function Format-NativeWindowId {
    param(
        [Parameter(Mandatory = $true)]
        [uint64] $Value
    )
    return ('0x{0:x16}' -f $Value)
}

function Get-RectKey {
    param(
        [Parameter(Mandatory = $true)]
        [object] $Rect
    )
    return ('{0},{1},{2},{3}' -f
        [int64] $Rect.left,
        [int64] $Rect.top,
        [int64] $Rect.right,
        [int64] $Rect.bottom)
}

if (-not (Test-Path -LiteralPath $buildRoot -PathType Container)) {
    throw "构建目录不存在：$buildRoot"
}

$observerPath = Resolve-R1C2BExecutable -FileName 'panebind-observer.exe'
$harnessPath = Resolve-R1C2BExecutable -FileName 'panebind-explorer-glue-harness.exe'
$evidenceDirectory = Join-Path -Path $repositoryRoot -ChildPath 'uat/r1c2b'
[void] (New-Item -ItemType Directory -Path $evidenceDirectory -Force)
$timestamp = [DateTime]::UtcNow.ToString(
    'yyyyMMddTHHmmssfffZ',
    [Globalization.CultureInfo]::InvariantCulture)
$observerStdout = Join-Path $evidenceDirectory "$timestamp-glue-observer.stdout.jsonl"
$observerStderr = Join-Path $evidenceDirectory "$timestamp-glue-observer.stderr.log"
$harnessEvidence = Join-Path $evidenceDirectory "$timestamp-glue-harness.jsonl"

Write-Output '交互 Glue Harness 将在当前控制台前台运行。'
Write-Output '脚本不会发送按键、创建、导航、关闭或挑选 Explorer。'
Write-Output '请严格区分 Harness 依次打印的 Leader 和 Follower 路径。'
Write-Output "Glue drag timeout: $GlueTimeoutSeconds 秒"
Write-Output "Observer 将在 $ObserveSeconds 秒后自然退出；它只记录外部审计证据。"
Write-Output "Harness evidence: $harnessEvidence"
Write-Output "Observer evidence: $observerStdout"

$observerProcess = $null
$harnessExitCode = $null
$launchFailure = $null
$observerAliveAtHarnessExit = $false
try {
    $observerProcess = Start-Process `
        -FilePath $observerPath `
        -ArgumentList @(
            '--observe-seconds',
            $ObserveSeconds.ToString(
                [Globalization.CultureInfo]::InvariantCulture)
        ) `
        -WorkingDirectory $repositoryRoot `
        -WindowStyle Hidden `
        -RedirectStandardOutput $observerStdout `
        -RedirectStandardError $observerStderr `
        -PassThru
    [void] $observerProcess.Handle

    Start-Sleep -Milliseconds 750
    $observerProcess.Refresh()
    if ($observerProcess.HasExited) {
        throw 'Observer 未能保持运行到交互 Harness 启动。'
    }

    Push-Location -LiteralPath $repositoryRoot
    try {
        # Deliberately run in the inherited foreground console. Do not pipe or
        # redirect stdin/stdout: every consent must originate in ReadConsoleW.
        & $harnessPath `
            '--interactive-consent-test' `
            '--evidence-log' `
            $harnessEvidence `
            '--timeout-seconds' `
            $GlueTimeoutSeconds
        $harnessExitCode = $LASTEXITCODE
        $observerProcess.Refresh()
        $observerAliveAtHarnessExit = -not $observerProcess.HasExited
    } finally {
        Pop-Location
    }
} catch {
    $launchFailure = $_
} finally {
    # The observer is bounded and must exit naturally. Never stop/kill the
    # observer, Harness, Explorer, or any other application from this runner.
    if ($null -ne $observerProcess) {
        if ($null -ne $harnessExitCode -and -not $observerProcess.HasExited) {
            Write-Output 'Glue Harness 已结束。'
            Write-Output '正在等待 Observer 自然完成剩余审计并执行严格校验，请勿关闭当前窗口……'
        }
        $observerProcess.WaitForExit()
        $observerProcess.Refresh()
    }
}

if ($null -ne $launchFailure) {
    throw $launchFailure
}
if ($null -eq $harnessExitCode) {
    throw 'Harness 未返回退出码。'
}
if (-not $observerAliveAtHarnessExit) {
    throw 'Observer 在 Harness 完成前已结束；外部证据覆盖不完整。'
}
if (-not (Test-Path -LiteralPath $harnessEvidence -PathType Leaf) -or
    (Get-Item -LiteralPath $harnessEvidence).Length -eq 0) {
    throw 'Harness evidence 缺失或为空。'
}
if (-not (Test-Path -LiteralPath $observerStdout -PathType Leaf) -or
    (Get-Item -LiteralPath $observerStdout).Length -eq 0) {
    throw 'Observer evidence 缺失或为空。'
}
if (-not (Test-Path -LiteralPath $observerStderr -PathType Leaf) -or
    (Get-Item -LiteralPath $observerStderr).Length -ne 0) {
    throw 'Observer stderr 缺失或非空。'
}

$observerRecords = @(Read-StrictJsonLines -Path $observerStdout)
if (@($observerRecords | Where-Object { [int] $_.schema_version -ne 1 }).Count -ne 0) {
    throw 'Observer schema_version 不符合当前证据契约。'
}
for ($index = 0; $index -lt $observerRecords.Count; ++$index) {
    $sequence = $observerRecords[$index].PSObject.Properties['observer_sequence']
    if ($null -eq $sequence -or
        [uint64] $sequence.Value -ne [uint64] ($index + 1)) {
        throw 'Observer sequence 不连续或缺失。'
    }
}
$hookRegistration = @($observerRecords | Where-Object {
    $_.record_kind -eq 'diagnostic' -and
    $_.diagnostic -eq 'hook_registration' -and
    $_.disposition -eq 'complete'
})
$hookShutdown = @($observerRecords | Where-Object {
    $_.record_kind -eq 'diagnostic' -and
    $_.diagnostic -eq 'hook_shutdown' -and
    $_.disposition -eq 'complete'
})
$observerShutdown = @($observerRecords | Where-Object {
    $_.record_kind -eq 'diagnostic' -and
    $_.diagnostic -eq 'observer_shutdown' -and
    $_.disposition -eq 'complete'
})
$observerFailures = @($observerRecords | Where-Object {
    $_.record_kind -eq 'diagnostic' -and
    ($_.diagnostic -eq 'event_queue_overflow' -or
     $_.diagnostic -eq 'queue_notification_failure' -or
     $_.disposition -eq 'incomplete')
})
if ($hookRegistration.Count -ne 1 -or $hookShutdown.Count -ne 1 -or
    $observerShutdown.Count -ne 1 -or $observerFailures.Count -ne 0) {
    throw 'Observer startup/shutdown 或 queue/drop/post failure gate 未通过。'
}

$harnessRecords = @(Read-StrictJsonLines -Path $harnessEvidence)
if (@($harnessRecords | Where-Object {
        [int] $_.schema_version -ne 1 -or
        $_.schema_name -ne 'panebind.r1c2b.explorer_glue'
    }).Count -ne 0) {
    throw 'Harness schema 不符合 R1-C2B evidence contract。'
}
for ($index = 0; $index -lt $harnessRecords.Count; ++$index) {
    $sequence = $harnessRecords[$index].PSObject.Properties['harness_sequence']
    if ($null -eq $sequence -or
        [uint64] $sequence.Value -ne [uint64] ($index + 1)) {
        throw 'Harness physical JSONL sequence 不连续或缺失。'
    }
}

$startup = Assert-UniqueRecord -Records $harnessRecords -Kind 'startup'
$summary = Assert-UniqueRecord -Records $harnessRecords -Kind 'summary'
$shutdown = Assert-UniqueRecord -Records $harnessRecords -Kind 'shutdown'
if ($startup.input_source -ne 'interactive_console' -or
    $startup.synthetic_input -ne $false -or
    $startup.r0_observer_runtime_dependency -ne $false -or
    [int] $startup.target_window_count -ne 2 -or
    $shutdown.disposition -ne 'complete') {
    throw 'Harness startup/shutdown authority contract 未通过。'
}

$nonceTargets = @($harnessRecords | Where-Object { $_.record_kind -eq 'nonce_target' })
$baselines = @($harnessRecords | Where-Object { $_.record_kind -eq 'baseline' })
$targetPrompts = @($harnessRecords | Where-Object { $_.record_kind -eq 'target_consent_prompt' })
$targetConfirmations = @($harnessRecords | Where-Object { $_.record_kind -eq 'target_consent_confirmation' })
$candidates = @($harnessRecords | Where-Object { $_.record_kind -eq 'candidate_selection' })
$nativeTargets = @($harnessRecords | Where-Object { $_.record_kind -eq 'native_target_identity' })
foreach ($records in @($nonceTargets, $baselines, $targetPrompts, $targetConfirmations, $candidates, $nativeTargets)) {
    if ($records.Count -ne 2 -or
        @($records | Where-Object { $_.role -eq 'leader' }).Count -ne 1 -or
        @($records | Where-Object { $_.role -eq 'follower' }).Count -ne 1) {
        throw 'Leader/Follower nonce、baseline、consent 或 candidate 记录不唯一。'
    }
}
$leaderNonce = @($nonceTargets | Where-Object { $_.role -eq 'leader' })[0]
$followerNonce = @($nonceTargets | Where-Object { $_.role -eq 'follower' })[0]
if (-not ([string] $leaderNonce.target_id).StartsWith(
        'leader-', [StringComparison]::Ordinal) -or
    -not ([string] $followerNonce.target_id).StartsWith(
        'follower-', [StringComparison]::Ordinal) -or
    $leaderNonce.target_id -eq $followerNonce.target_id -or
    $leaderNonce.created_empty -ne $true -or
    $followerNonce.created_empty -ne $true -or
    $leaderNonce.full_path_redacted -ne $true -or
    $followerNonce.full_path_redacted -ne $true) {
    throw 'Leader/Follower nonce identity、role prefix 或 privacy gate 未通过。'
}
if (@($targetPrompts | Where-Object {
        $_.result -ne 'READY' -or $_.input_source -ne 'interactive_console'
    }).Count -ne 0 -or
    @($targetConfirmations | Where-Object {
        $_.result -ne 'CONFIRMED' -or
        $_.input_source -ne 'interactive_console' -or
        $_.native_apply_attempted -ne $false
    }).Count -ne 0 -or
    @($candidates | Where-Object {
        $_.result -ne 'PASS' -or $_.authority_kind -ne 'user_consent' -or
        $_.unique_new_target -ne $true -or
        $_.exact_target_location -ne $true -or
        $_.preexisting_exact_location_detected -ne $false -or
        [uint64] $_.baseline_generation -ge [uint64] $_.target_prompt_generation -or
        [uint64] $_.target_prompt_generation -ge [uint64] $_.target_confirmation_generation -or
        [uint64] $_.target_confirmation_generation -ge [uint64] $_.eligibility_generation -or
        [uint64] $_.eligibility_generation -ge [uint64] $_.token_generation
    }).Count -ne 0) {
    throw '两个 target 的真实 Console consent/generation/eligibility gate 未通过。'
}
foreach ($role in @('leader', 'follower')) {
    $baseline = @($baselines | Where-Object { $_.role -eq $role })[0]
    $prompt = @($targetPrompts | Where-Object { $_.role -eq $role })[0]
    $candidate = @($candidates | Where-Object { $_.role -eq $role })[0]
    if ($baseline.result -ne 'PASS' -or
        $baseline.baseline_exclusion_complete -ne $true -or
        $baseline.target_directory_contract_verified -ne $true -or
        [uint64] $baseline.baseline_generation -ne
            [uint64] $candidate.baseline_generation -or
        [uint64] $prompt.generation -ne
            [uint64] $candidate.target_prompt_generation) {
        throw "$role target 的 baseline/prompt generation 交叉验证失败。"
    }
}

$leaderNative = @($nativeTargets | Where-Object { $_.role -eq 'leader' })[0]
$followerNative = @($nativeTargets | Where-Object { $_.role -eq 'follower' })[0]
if ([uint64] $leaderNative.native_key -eq [uint64] $followerNative.native_key -or
    $leaderNative.process -ne 'explorer.exe' -or
    $followerNative.process -ne 'explorer.exe' -or
    [uint64] $leaderNative.capability_generation -eq 0 -or
    [uint64] $followerNative.capability_generation -eq 0) {
    throw 'Leader/Follower native identity 必须 distinct 且 generation 非零。'
}

$pair = Assert-UniqueRecord -Records $harnessRecords -Kind 'pair_validation'
$gluePrompt = Assert-UniqueRecord -Records $harnessRecords -Kind 'glue_consent_prompt'
$glueConfirmation = Assert-UniqueRecord -Records $harnessRecords -Kind 'glue_consent_confirmation'
$glueAuthority = Assert-UniqueRecord -Records $harnessRecords -Kind 'glue_authority'
$glueNativeBindings = Assert-UniqueRecord -Records $harnessRecords -Kind 'glue_native_bindings'
if ($pair.result -ne 'PASS' -or
    $pair.leader_target_consent_prefix_valid -ne $true -or
    $pair.follower_target_consent_prefix_valid -ne $true -or
    $pair.follower_baseline_excluded_leader -ne $true -or
    $pair.pair_distinct -ne $true -or
    $pair.same_monitor_and_dpi -ne $true -or
    $pair.test_layout_planned -ne $true -or
    $pair.leader_original.class -ne 'CabinetWClass' -or
    $pair.follower_original.class -ne 'CabinetWClass' -or
    $pair.leader_original.root_top_level -ne $true -or
    $pair.follower_original.root_top_level -ne $true -or
    $pair.leader_original.visible_state -ne $true -or
    $pair.follower_original.visible_state -ne $true -or
    $pair.leader_original.minimized -ne $false -or
    $pair.follower_original.minimized -ne $false -or
    $pair.leader_original.maximized -ne $false -or
    $pair.follower_original.maximized -ne $false -or
    $pair.leader_original.target_elevated -ne $false -or
    $pair.follower_original.target_elevated -ne $false -or
    $pair.leader_original.target_ui_access -ne $false -or
    $pair.follower_original.target_ui_access -ne $false -or
    $pair.leader_original.target_app_container -ne $false -or
    $pair.follower_original.target_app_container -ne $false -or
    $gluePrompt.result -ne 'READY' -or
    $gluePrompt.input_source -ne 'interactive_console' -or
    $glueConfirmation.result -ne 'CONFIRMED' -or
    $glueConfirmation.input_source -ne 'interactive_console' -or
    $glueConfirmation.native_apply_attempted -ne $false -or
    $glueAuthority.result -ne 'PASS' -or
    [uint64] $gluePrompt.generation -ne [uint64] $glueAuthority.prompt_generation -or
    [uint64] $glueAuthority.pair_preview_generation -ge [uint64] $glueAuthority.prompt_generation -or
    [uint64] $glueAuthority.prompt_generation -ge [uint64] $glueAuthority.confirmation_generation -or
    [uint64] $glueAuthority.confirmation_generation -ge [uint64] $glueAuthority.authority_generation) {
    throw 'Pair/Glue consent authority gate 未通过。'
}
if ($glueNativeBindings.session_bindings_present -ne $true -or
    [uint64] $glueNativeBindings.leader_native_key -ne
        [uint64] $leaderNative.native_key -or
    [uint64] $glueNativeBindings.follower_native_key -ne
        [uint64] $followerNative.native_key -or
    [uint32] $glueNativeBindings.leader_pid -ne [uint32] $leaderNative.pid -or
    [uint32] $glueNativeBindings.follower_pid -ne [uint32] $followerNative.pid) {
    throw 'Glue native binding 与两次 consent target identity 不一致。'
}

$steps = @($harnessRecords | Where-Object { $_.record_kind -eq 'glue_step' })
foreach ($stepName in @('setup_test_layout', 'arm_event_source', 'run_until_terminal')) {
    $step = @($steps | Where-Object { $_.step -eq $stepName })
    if ($step.Count -ne 1 -or $step[0].result -ne 'PASS') {
        throw "Glue step '$stepName' 未唯一 PASS。"
    }
}

$facts = Assert-UniqueRecord -Records $harnessRecords -Kind 'facts'
$trace = @($harnessRecords | Where-Object { $_.record_kind -eq 'internal_trace' })
if ($trace.Count -eq 0) {
    throw 'Internal behavior trace 为空。'
}
for ($index = 0; $index -lt $trace.Count; ++$index) {
    if ([uint64] $trace[$index].trace_sequence -ne [uint64] ($index + 1) -or
        [uint64] $trace[$index].glue_session_generation -eq 0 -or
        [uint64] $trace[$index].glue_session_generation -ne
            [uint64] $facts.glue_session_generation) {
        throw 'Internal trace_sequence 不连续。'
    }
}
$lastEventSequence = [uint64] 0
foreach ($item in $trace) {
    $eventSequence = [uint64] $item.event_sequence
    if ($eventSequence -ne 0) {
        if ($eventSequence -le $lastEventSequence) {
            throw 'Internal native event receipt sequence 非严格单调。'
        }
        $lastEventSequence = $eventSequence
    }
}
$leaderStart = @($trace | Where-Object { $_.role -eq 'leader' -and $_.event_kind -eq 'move_resize_started' })
$leaderLocation = @($trace | Where-Object { $_.role -eq 'leader' -and $_.event_kind -eq 'geometry_changed' })
$leaderEnd = @($trace | Where-Object { $_.role -eq 'leader' -and $_.event_kind -eq 'move_resize_ended' })
$followerFeedback = @($trace | Where-Object { $_.role -eq 'follower' -and $_.event_kind -eq 'geometry_changed' })
if ($leaderStart.Count -ne 1 -or $leaderLocation.Count -lt 1 -or
    $leaderEnd.Count -ne 1 -or
    @($trace | Where-Object { $_.decision -eq 'activated' }).Count -ne 1 -or
    @($trace | Where-Object { $_.decision -eq 'completing' }).Count -ne 1 -or
    @($trace | Where-Object { $_.decision -eq 'completed' }).Count -ne 1 -or
    @($trace | Where-Object {
        $_.role -eq 'follower' -and $_.decision -eq 'follower_move_requested'
    }).Count -ne 0) {
    throw 'Internal START/LOCATION/END、completion 或 no-recursion trace gate 未通过。'
}

$operations = @($harnessRecords | Where-Object { $_.record_kind -eq 'operation' })
$activeFollower = @($operations | Where-Object {
    $_.phase -eq 'active_follower' -and $_.role -eq 'follower'
})
if ($activeFollower.Count -lt 1 -or
    @($activeFollower | Where-Object {
        $_.native_apply_attempted -ne $true -or $_.exact_receipt -ne $true
    }).Count -ne 0 -or
    @($operations | Where-Object { $_.exact_receipt -ne $true }).Count -ne 0) {
    throw 'Follower native apply 或 setup/restore operation receipt 未全部 exact。'
}

$operationGenerations = @($activeFollower | ForEach-Object {
    [uint64] $_.behavior_operation_generation
})
if (@($operationGenerations | Where-Object { $_ -eq 0 }).Count -ne 0 -or
    @($operationGenerations | Group-Object | Where-Object { $_.Count -ne 1 }).Count -ne 0) {
    throw 'Active follower operation generation 必须非零且唯一。'
}
$reconciliations = @($harnessRecords | Where-Object {
    $_.record_kind -eq 'feedback_reconciliation'
})
if ($reconciliations.Count -ne $activeFollower.Count) {
    throw '每个 active follower operation 必须有唯一 reconciliation record。'
}
$acknowledgedReconciliations = 0
$snapshotReconciliations = 0
foreach ($operation in $activeFollower) {
    $operationGeneration = [uint64] $operation.behavior_operation_generation
    $sourceSequence = [uint64] $operation.source_leader_sequence
    $requestedKey = Get-RectKey -Rect $operation.requested_visible
    $actualKey = Get-RectKey -Rect $operation.actual.visible
    $commands = @($trace | Where-Object {
        $_.role -eq 'leader' -and
        $_.event_kind -eq 'geometry_changed' -and
        $_.decision -eq 'follower_move_requested' -and
        [uint64] $_.event_sequence -eq $sourceSequence -and
        [uint64] $_.behavior_operation_generation -eq $operationGeneration -and
        (Get-RectKey -Rect $_.visible) -eq $requestedKey
    })
    $reconciliation = @($reconciliations | Where-Object {
        [uint64] $_.operation_generation -eq $operationGeneration
    })
    if ($sourceSequence -eq 0 -or $commands.Count -ne 1 -or
        $reconciliation.Count -ne 1 -or
        [uint64] $reconciliation[0].source_leader_sequence -ne $sourceSequence -or
        [int] $reconciliation[0].command_trace_match_count -ne 1 -or
        $reconciliation[0].exact_operation_receipt -ne $true -or
        (Get-RectKey -Rect $reconciliation[0].expected_visible) -ne $requestedKey -or
        (Get-RectKey -Rect $reconciliation[0].actual_visible) -ne $actualKey -or
        $actualKey -ne $requestedKey) {
        throw "Operation generation $operationGeneration 的 command/receipt/geometry 关联失败。"
    }

    if ($reconciliation[0].disposition -eq 'acknowledged_self_feedback') {
        ++$acknowledgedReconciliations
        if ($null -eq $reconciliation[0].feedback_event_sequence) {
            throw "Operation generation $operationGeneration 缺少 feedback event sequence。"
        }
        $feedbackSequence = [uint64] $reconciliation[0].feedback_event_sequence
        $feedbackTrace = @($trace | Where-Object {
            $_.role -eq 'follower' -and
            $_.event_kind -eq 'geometry_changed' -and
            [uint64] $_.event_sequence -eq $feedbackSequence -and
            (Get-RectKey -Rect $_.visible) -eq $requestedKey -and
            ($_.decision -eq 'feedback_acknowledged' -or
             $_.decision -eq 'feedback_observed_pending_result')
        })
        if ($feedbackSequence -le $sourceSequence -or $feedbackTrace.Count -ne 1) {
            throw "Operation generation $operationGeneration 的 acknowledged feedback 关联失败。"
        }
    } elseif ($reconciliation[0].disposition -eq
              'reconciled_by_operation_receipt_and_final_snapshot') {
        ++$snapshotReconciliations
        if ($null -ne $reconciliation[0].feedback_event_sequence) {
            throw "Operation generation $operationGeneration 的 missing feedback 不应伪造 event ACK。"
        }
    } else {
        throw "Operation generation $operationGeneration 的 reconciliation disposition 非法。"
    }
}

if ($facts.behavior_state -ne 'completed' -or
    $null -ne $facts.behavior_abort_reason -or
    $facts.glue_consent_confirmed -ne $true -or
    $facts.follower_baseline_excluded_leader -ne $true -or
    $facts.test_layout_exact -ne $true -or
    $facts.topology_exact_two_window_component -ne $true -or
    $facts.event_source_armed -ne $true -or
    $facts.event_source_stopped -ne $true -or
    $facts.event_source_lifecycle_clean -ne $true -or
    $facts.leader_restored_exact -ne $true -or
    $facts.follower_restored_exact -ne $true -or
    $facts.user_windows_close_attempted -ne $false -or
    [int] $facts.follower_native_apply_count -ne $activeFollower.Count -or
    [int] $facts.suppressed_feedback_count -ne $followerFeedback.Count -or
    [int] $facts.duplicate_feedback_count -ne
        @($trace | Where-Object {
            $_.decision -eq 'duplicate_feedback_suppressed'
        }).Count -or
    [int] $facts.missing_feedback_count -ne $snapshotReconciliations -or
    [int] $facts.reconciled_feedback_count -ne $snapshotReconciliations -or
    ($acknowledgedReconciliations + $snapshotReconciliations) -ne
        $activeFollower.Count -or
    [int] $facts.unexpected_feedback_count -ne 0 -or
    [int] $facts.max_pending_depth -gt [int] $facts.pending_capacity -or
    [int] $facts.max_event_queue_depth -gt [int] $facts.event_queue_capacity) {
    throw 'Final facts behavior/suppression/cleanup gate 未通过。'
}

$expectedFeedbackEvidence = if ($followerFeedback.Count -gt 0) {
    'observed_and_suppressed'
} else {
    'no_feedback_event_reconciled'
}
if ($summary.result -ne 'PASS' -or $summary.runtime_gate -ne 'PASS' -or
    $summary.implementation_ready -ne $true -or
    $summary.behavior_state -ne 'completed' -or
    [int] $summary.leader_start_count -ne $leaderStart.Count -or
    [int] $summary.leader_location_count -ne $leaderLocation.Count -or
    [int] $summary.leader_end_count -ne $leaderEnd.Count -or
    [int] $summary.follower_feedback_count -ne $followerFeedback.Count -or
    [int] $summary.follower_native_apply_count -ne $activeFollower.Count -or
    [int] $summary.active_follower_operation_count -ne $activeFollower.Count -or
    $summary.all_active_follower_operations_exact -ne $true -or
    [int] $summary.suppressed_feedback_count -ne $followerFeedback.Count -or
    [int] $summary.duplicate_feedback_count -ne
        [int] $facts.duplicate_feedback_count -or
    [int] $summary.missing_feedback_count -ne $snapshotReconciliations -or
    [int] $summary.reconciled_feedback_count -ne $snapshotReconciliations -or
    [int] $summary.acknowledged_operation_count -ne
        $acknowledgedReconciliations -or
    [int] $summary.reconciled_operation_count -ne $snapshotReconciliations -or
    $summary.feedback_operation_correlation_valid -ne $true -or
    $summary.trace_generation_valid -ne $true -or
    $summary.feedback_suppression_evidence -ne $expectedFeedbackEvidence -or
    [int] $summary.unexpected_feedback_count -ne 0 -or
    [int] $summary.recursive_follower_operation_count -ne 0 -or
    $summary.queue_overflow -ne $false -or
    $summary.event_source_stopped -ne $true -or
    $summary.event_source_lifecycle_clean -ne $true -or
    $summary.topology_frozen_exact_pair -ne $true -or
    $summary.leader_restored_exact -ne $true -or
    $summary.follower_restored_exact -ne $true -or
    $summary.user_preexisting_windows_touched -ne $false -or
    $summary.other_third_party_control -ne $false -or
    $summary.global_input_control -ne $false -or
    $summary.user_windows_close_attempted -ne $false -or
    $summary.r0_observer_runtime_dependency -ne $false -or
    $summary.r0_observer_semantics_changed -ne $false) {
    throw 'Harness final Runtime/Safety summary gate 未通过。'
}

$leaderNativeId = Format-NativeWindowId -Value ([uint64] $leaderNative.native_key)
$followerNativeId = Format-NativeWindowId -Value ([uint64] $followerNative.native_key)
$leaderObserverEvents = @($observerRecords | Where-Object {
    $_.record_kind -eq 'event' -and $_.native_window_id -eq $leaderNativeId
})
$followerObserverEvents = @($observerRecords | Where-Object {
    $_.record_kind -eq 'event' -and $_.native_window_id -eq $followerNativeId
})
$externalLeaderStart = @($leaderObserverEvents | Where-Object {
    $_.native_event.name -eq 'EVENT_SYSTEM_MOVESIZESTART'
})
$externalLeaderEnd = @($leaderObserverEvents | Where-Object {
    $_.native_event.name -eq 'EVENT_SYSTEM_MOVESIZEEND'
})
if ($externalLeaderStart.Count -ne 1 -or $externalLeaderEnd.Count -ne 1) {
    throw 'External Observer 未唯一观察 Leader START/END。'
}
$externalStartSequence = [uint64] $externalLeaderStart[0].observer_sequence
$externalEndSequence = [uint64] $externalLeaderEnd[0].observer_sequence
$externalLeaderLocation = @($leaderObserverEvents | Where-Object {
    $_.native_event.name -eq 'EVENT_OBJECT_LOCATIONCHANGE' -and
    [uint64] $_.observer_sequence -gt $externalStartSequence -and
    [uint64] $_.observer_sequence -lt $externalEndSequence
})
$externalFollowerLocation = @($followerObserverEvents | Where-Object {
    $_.native_event.name -eq 'EVENT_OBJECT_LOCATIONCHANGE' -and
    [uint64] $_.observer_sequence -gt $externalStartSequence -and
    [uint64] $_.observer_sequence -lt $externalEndSequence
})
if ([uint64] $hookRegistration[0].observer_sequence -ge $externalStartSequence -or
    $externalStartSequence -ge $externalEndSequence -or
    $externalLeaderLocation.Count -lt 1) {
    throw 'External Observer hook readiness 或 active Leader event coverage 不完整。'
}
if (($externalFollowerLocation.Count -eq 0) -ne
    ($followerFeedback.Count -eq 0)) {
    throw 'Internal Event Source 与 external Observer 对 follower feedback 是否存在的结论冲突。'
}
$targetObserverEvents = @($leaderObserverEvents + $followerObserverEvents)
$activeTargetObserverEvents = @($targetObserverEvents | Where-Object {
    [uint64] $_.observer_sequence -ge $externalStartSequence -and
    [uint64] $_.observer_sequence -le $externalEndSequence
})
if (@($activeTargetObserverEvents | Where-Object {
        $_.callback_root_matches -ne $true -or
        $_.native_object_id -ne 0 -or $_.native_child_id -ne 0 -or
        @($_.field_errors).Count -ne 0
    }).Count -ne 0 -or
    @($activeTargetObserverEvents | Where-Object {
        $_.native_event.name -eq 'EVENT_OBJECT_DESTROY'
    }).Count -ne 0) {
    throw 'External active-session target identity/object/field-error/destroy gate 未通过。'
}

if ($null -eq $observerProcess.ExitCode -or $observerProcess.ExitCode -ne 0) {
    throw "Observer 失败：exit=$($observerProcess.ExitCode)"
}
if ($harnessExitCode -ne 0) {
    throw "Glue Harness 未通过：exit=$harnessExitCode, reason=$($summary.reason)"
}

Write-Output "R1-C2B $Configuration Explorer Glue evidence gate: PASS"
Write-Output "Leader START/LOCATION/END: 1/$($externalLeaderLocation.Count)/1"
Write-Output "Follower active LOCATION: $($externalFollowerLocation.Count)"
Write-Output "Follower native applies: $($summary.follower_native_apply_count)"
Write-Output "Suppressed/duplicate/missing: $($summary.suppressed_feedback_count)/$($summary.duplicate_feedback_count)/$($summary.missing_feedback_count)"
Write-Output '两个 Explorer 均未自动关闭；请确认已自行关闭测试窗口。'
Write-Output '原始 evidence 已保存到 uat/r1c2b/。'
