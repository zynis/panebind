[CmdletBinding()]
param(
    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string] $BuildDirectory = 'out/r1c2a-debug',

    [Parameter()]
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Debug',

    [Parameter(Mandatory = $true)]
    [ValidateRange(30, 86400)]
    [int] $ObserveSeconds
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path -Path $PSScriptRoot -ChildPath '..'))
$buildRoot = if ([System.IO.Path]::IsPathRooted($BuildDirectory)) {
    [System.IO.Path]::GetFullPath($BuildDirectory)
} else {
    [System.IO.Path]::GetFullPath(
        (Join-Path -Path $repositoryRoot -ChildPath $BuildDirectory))
}

function Resolve-R1C2AExecutable {
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

if (-not (Test-Path -LiteralPath $buildRoot -PathType Container)) {
    throw "构建目录不存在：$buildRoot"
}

$observerPath = Resolve-R1C2AExecutable -FileName 'panebind-observer.exe'
$harnessPath = Resolve-R1C2AExecutable -FileName 'panebind-explorer-harness.exe'
$evidenceDirectory = Join-Path -Path $repositoryRoot -ChildPath 'uat/r1c2a'
[void] (New-Item -ItemType Directory -Path $evidenceDirectory -Force)
$timestamp = [DateTime]::UtcNow.ToString(
    'yyyyMMddTHHmmssfffZ',
    [Globalization.CultureInfo]::InvariantCulture)
$observerStdout = Join-Path $evidenceDirectory "$timestamp-consent-observer.stdout.jsonl"
$observerStderr = Join-Path $evidenceDirectory "$timestamp-consent-observer.stderr.log"
$harnessEvidence = Join-Path $evidenceDirectory "$timestamp-consent-harness.jsonl"

Write-Output '交互测试将在当前控制台前台运行。脚本不会发送按键、创建或关闭 Explorer。'
Write-Output "Observer 将在 $ObserveSeconds 秒后自然退出；请在此时间内完成两次明确授权。"
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
        throw 'Observer 未能保持运行到交互 harness 启动。'
    }

    Push-Location -LiteralPath $repositoryRoot
    try {
        # Deliberately invoke in the current console. Do not redirect or pipe
        # stdin/stdout: ReadConsoleW must observe a real human console input.
        & $harnessPath `
            '--interactive-consent-test' `
            '--evidence-log' `
            $harnessEvidence
        $harnessExitCode = $LASTEXITCODE
        $observerProcess.Refresh()
        $observerAliveAtHarnessExit = -not $observerProcess.HasExited
    } finally {
        Pop-Location
    }
} catch {
    $launchFailure = $_
} finally {
    # Observer has a bounded duration and exits naturally. This runner never
    # stops or kills it, the harness, Explorer, or any other application.
    if ($null -ne $observerProcess) {
        $observerProcess.WaitForExit()
        $observerProcess.Refresh()
    }
}

if ($null -ne $launchFailure) {
    throw $launchFailure
}
if (-not $observerAliveAtHarnessExit) {
    throw 'Observer 在交互 harness 完成前已结束；operation evidence 覆盖不完整。'
}
if ($null -eq $harnessExitCode -or $harnessExitCode -ne 0) {
    throw "交互授权 harness 失败或仍被阻断：exit=$harnessExitCode"
}
if ($null -eq $observerProcess.ExitCode -or $observerProcess.ExitCode -ne 0) {
    throw "Observer 失败：exit=$($observerProcess.ExitCode)"
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

$observerRecords = @(Read-StrictJsonLines -Path $observerStdout)
if (@($observerRecords | Where-Object {
        [int] $_.schema_version -ne 1
    }).Count -ne 0) {
    throw 'Observer schema_version 不符合当前证据契约。'
}
for ($index = 0; $index -lt $observerRecords.Count; ++$index) {
    $sequenceProperty = $observerRecords[$index].PSObject.Properties[
        'observer_sequence']
    if ($null -eq $sequenceProperty -or
        [uint64] $sequenceProperty.Value -ne [uint64] ($index + 1)) {
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
    ($_.record_kind -eq 'diagnostic' -and
        ($_.diagnostic -eq 'event_queue_overflow' -or
         $_.diagnostic -eq 'queue_notification_failure' -or
         $_.disposition -eq 'incomplete'))
})
if ($hookRegistration.Count -ne 1 -or $hookShutdown.Count -ne 1 -or
    $observerShutdown.Count -ne 1 -or $observerFailures.Count -ne 0) {
    throw 'Observer startup/shutdown 或 queue/drop/post failure gate 未通过。'
}

$harnessRecords = @(Read-StrictJsonLines -Path $harnessEvidence)
if (@($harnessRecords | Where-Object {
        [int] $_.schema_version -ne 3
    }).Count -ne 0) {
    throw 'Harness schema_version 不符合当前证据契约。'
}
$summaries = @($harnessRecords | Where-Object {
    $_.record_kind -eq 'summary'
})
$primary = @($harnessRecords | Where-Object {
    $_.record_kind -eq 'operation' -and
    $_.phase -eq 'single_translation'
})
$restore = @($harnessRecords | Where-Object {
    $_.record_kind -eq 'operation' -and $_.phase -eq 'restore'
})
$baseline = @($harnessRecords | Where-Object {
    $_.record_kind -eq 'baseline' -and $_.result -eq 'PASS'
})
$candidate = @($harnessRecords | Where-Object {
    $_.record_kind -eq 'candidate_selection' -and $_.result -eq 'PASS'
})
$targetConsent = @($harnessRecords | Where-Object {
    $_.record_kind -eq 'target_consent_confirmation' -and
    $_.result -eq 'CONFIRMED'
})
$moveConsent = @($harnessRecords | Where-Object {
    $_.record_kind -eq 'move_consent_confirmation' -and
    $_.result -eq 'CONFIRMED'
})
$primaryMarker = @($harnessRecords | Where-Object {
    $_.record_kind -eq 'operation_marker' -and
    $_.phase -eq 'single_translation_start'
})
$restoreMarker = @($harnessRecords | Where-Object {
    $_.record_kind -eq 'operation_marker' -and
    $_.phase -eq 'restore_start'
})
if ($summaries.Count -ne 1 -or $primary.Count -ne 1 -or
    $restore.Count -ne 1 -or $baseline.Count -ne 1 -or
    $candidate.Count -ne 1 -or $targetConsent.Count -ne 1 -or
    $moveConsent.Count -ne 1 -or $primaryMarker.Count -ne 1 -or
    $restoreMarker.Count -ne 1) {
    throw 'Harness evidence 缺少唯一 baseline/candidate/consent/operation/summary 记录。'
}
$summary = $summaries[0]
if ($summary.result -ne 'PASS' -or
    $summary.implementation_ready -ne $true -or
    $summary.eligibility_gate -ne 'PASS' -or
    $summary.runtime_gate -ne 'PASS' -or
    [int] $summary.native_translation_count -ne 1 -or
    [int] $summary.restore_native_apply_count -ne 1 -or
    $summary.auto_close_attempted -ne $false -or
    $summary.user_existing_windows_touched -ne $false -or
    $summary.other_third_party_control -ne $false -or
    $targetConsent[0].input_source -ne 'interactive_console' -or
    $moveConsent[0].input_source -ne 'interactive_console' -or
    $candidate[0].authority_kind -ne 'user_consent' -or
    $primary[0].reason -ne 'eligible' -or
    $primary[0].native_apply_attempted -ne $true -or
    $primary[0].exact_receipt -ne $true -or
    $restore[0].reason -ne 'eligible' -or
    $restore[0].native_apply_attempted -ne $true -or
    $restore[0].exact_receipt -ne $true -or
    [uint64] $primaryMarker[0].native_hwnd -ne
        [uint64] $candidate[0].native_hwnd -or
    [uint64] $restoreMarker[0].native_hwnd -ne
        [uint64] $candidate[0].native_hwnd -or
    [uint32] $primaryMarker[0].pid -ne [uint32] $candidate[0].pid -or
    [uint32] $restoreMarker[0].pid -ne [uint32] $candidate[0].pid -or
    [uint64] $restoreMarker[0].tick_ms -lt
        [uint64] $primaryMarker[0].tick_ms -or
    [uint64] $candidate[0].baseline_generation -ge
        [uint64] $candidate[0].target_prompt_generation -or
    [uint64] $candidate[0].target_prompt_generation -ge
        [uint64] $candidate[0].target_confirmation_generation -or
    [uint64] $candidate[0].target_confirmation_generation -ge
        [uint64] $candidate[0].eligibility_generation -or
    [uint64] $candidate[0].eligibility_generation -ge
        [uint64] $candidate[0].consent_token_generation -or
    [uint64] $candidate[0].consent_token_generation -ge
        [uint64] $moveConsent[0].move_prompt_generation -or
    [uint64] $moveConsent[0].move_prompt_generation -ge
        [uint64] $moveConsent[0].move_confirmation_generation) {
    throw 'Harness runtime/restore/安全 summary gate 未通过。'
}

Write-Output '交互 harness exit: 0'
Write-Output 'Observer exit: 0'
Write-Output '原始 evidence 已保存到 uat/r1c2a/。'
