[CmdletBinding()]
param(
    [Parameter(ParameterSetName = 'Run')]
    [ValidateNotNullOrEmpty()]
    [string] $BuildDirectory = 'out/r1c2b-debug',

    [Parameter(ParameterSetName = 'Run')]
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Debug',

    [Parameter(ParameterSetName = 'Run')]
    [ValidateRange(30, 300)]
    [int] $GlueTimeoutSeconds = 120,

    [Parameter(ParameterSetName = 'Run')]
    [ValidateRange(120, 1800)]
    [int] $ObserveSeconds = 300,

    # Offline-only seam for deterministic runner fixtures and review of an
    # already captured evidence prefix. It never starts the Observer/Harness.
    [Parameter(Mandatory = $true, ParameterSetName = 'ValidateEvidence')]
    [ValidateNotNullOrEmpty()]
    [string] $ValidateEvidencePrefix,

    [Parameter(Mandatory = $true, ParameterSetName = 'ValidateEvidence')]
    [ValidateRange(0, 255)]
    [int] $ValidationHarnessExitCode,

    [Parameter(ParameterSetName = 'ValidateEvidence')]
    [ValidateRange(0, 255)]
    [int] $ValidationObserverExitCode = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

trap {
    Write-Output 'R1-C2B evidence outcome: INVALID_EVIDENCE'
    [Console]::Error.WriteLine(
        ("Evidence 校验失败：{0}" -f $_.Exception.Message))
    exit 1
}

if ($PSCmdlet.ParameterSetName -eq 'Run' -and
    $ObserveSeconds -lt ($GlueTimeoutSeconds + 30)) {
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

function Assert-RecordProperties {
    param(
        [Parameter(Mandatory = $true)]
        [object] $Record,

        [Parameter(Mandatory = $true)]
        [string[]] $Names,

        [Parameter(Mandatory = $true)]
        [string] $Context,

        [Parameter()]
        [switch] $AllowNull
    )

    foreach ($name in $Names) {
        $property = $Record.PSObject.Properties[$name]
        if ($null -eq $property -or
            (-not $AllowNull -and $null -eq $property.Value)) {
            throw "$Context 缺少必需字段 '$name'。"
        }
    }
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

function Assert-SnapshotProperties {
    param(
        [Parameter(Mandatory = $true)] [object] $Snapshot,
        [Parameter(Mandatory = $true)] [string] $Context
    )
    Assert-RecordProperties -Record $Snapshot -Context $Context -Names @(
        'visible', 'positioning', 'pid', 'tid', 'class', 'dpi', 'monitor',
        'monitor_rect', 'work_area', 'root_top_level', 'visible_state',
        'cloaked', 'minimized', 'maximized', 'current_virtual_desktop',
        'exact_test_location', 'target_integrity_rid', 'target_session_id',
        'target_elevated', 'target_ui_access', 'target_app_container'
    )
}

function Get-SnapshotKey {
    param(
        [Parameter(Mandatory = $true)] [object] $Snapshot,
        [Parameter(Mandatory = $true)] [string] $Context
    )
    Assert-SnapshotProperties -Snapshot $Snapshot -Context $Context
    return (@(
        (Get-RectKey $Snapshot.visible),
        (Get-RectKey $Snapshot.positioning),
        [uint32] $Snapshot.pid,
        [uint32] $Snapshot.tid,
        [string] $Snapshot.class,
        [uint32] $Snapshot.dpi,
        [string] $Snapshot.monitor,
        (Get-RectKey $Snapshot.monitor_rect),
        (Get-RectKey $Snapshot.work_area),
        [bool] $Snapshot.root_top_level,
        [bool] $Snapshot.visible_state,
        [bool] $Snapshot.cloaked,
        [bool] $Snapshot.minimized,
        [bool] $Snapshot.maximized,
        [bool] $Snapshot.current_virtual_desktop,
        [bool] $Snapshot.exact_test_location,
        [uint32] $Snapshot.target_integrity_rid,
        [uint32] $Snapshot.target_session_id,
        [bool] $Snapshot.target_elevated,
        [bool] $Snapshot.target_ui_access,
        [bool] $Snapshot.target_app_container
    ) -join '|')
}

function Get-SnapshotNonGeometryKey {
    param(
        [Parameter(Mandatory = $true)] [object] $Snapshot,
        [Parameter(Mandatory = $true)] [string] $Context
    )
    Assert-SnapshotProperties -Snapshot $Snapshot -Context $Context
    return (@(
        [uint32] $Snapshot.pid, [uint32] $Snapshot.tid,
        [string] $Snapshot.class, [uint32] $Snapshot.dpi,
        [string] $Snapshot.monitor, (Get-RectKey $Snapshot.monitor_rect),
        (Get-RectKey $Snapshot.work_area), [bool] $Snapshot.root_top_level,
        [bool] $Snapshot.visible_state, [bool] $Snapshot.cloaked,
        [bool] $Snapshot.minimized, [bool] $Snapshot.maximized,
        [bool] $Snapshot.current_virtual_desktop,
        [bool] $Snapshot.exact_test_location,
        [uint32] $Snapshot.target_integrity_rid,
        [uint32] $Snapshot.target_session_id, [bool] $Snapshot.target_elevated,
        [bool] $Snapshot.target_ui_access,
        [bool] $Snapshot.target_app_container
    ) -join '|')
}

function Get-RectTranslation {
    param(
        [Parameter(Mandatory = $true)] [object] $From,
        [Parameter(Mandatory = $true)] [object] $To,
        [Parameter(Mandatory = $true)] [string] $Context
    )
    $dx = [int64] $To.left - [int64] $From.left
    $dy = [int64] $To.top - [int64] $From.top
    if ([int64] $To.right - [int64] $From.right -ne $dx -or
        [int64] $To.bottom - [int64] $From.bottom -ne $dy) {
        throw "$Context 不是纯 translation。"
    }
    return [pscustomobject]@{ dx = $dx; dy = $dy }
}

function Get-TranslatedRectKey {
    param(
        [Parameter(Mandatory = $true)] [object] $Rect,
        [Parameter(Mandatory = $true)] [int64] $Dx,
        [Parameter(Mandatory = $true)] [int64] $Dy
    )
    return ('{0},{1},{2},{3}' -f
        ([int64] $Rect.left + $Dx), ([int64] $Rect.top + $Dy),
        ([int64] $Rect.right + $Dx), ([int64] $Rect.bottom + $Dy))
}

function ConvertTo-UtcEvidenceTime {
    param(
        [Parameter(Mandatory = $true)]
        [object] $Record,

        [Parameter(Mandatory = $true)]
        [string] $PropertyName
    )

    $property = $Record.PSObject.Properties[$PropertyName]
    if ($null -eq $property -or
        [string]::IsNullOrWhiteSpace([string] $property.Value)) {
        throw "Evidence record 缺少时间字段 '$PropertyName'。"
    }
    try {
        return [DateTimeOffset]::Parse(
            [string] $property.Value,
            [Globalization.CultureInfo]::InvariantCulture,
            [Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()
    } catch {
        throw "Evidence 时间字段 '$PropertyName' 非合法 ISO-8601：$($property.Value)"
    }
}

function Assert-LayoutPreviewEvidence {
    param(
        [Parameter(Mandatory = $true)]
        [object[]] $Records,

        [Parameter(Mandatory = $true)]
        [object] $Pair,

        [Parameter()]
        [switch] $AllowLegacyMissing
    )

    $previews = @($Records | Where-Object {
        $_.record_kind -eq 'pair_layout_preview'
    })
    $rechecks = @($Records | Where-Object {
        $_.record_kind -eq 'pair_layout_recheck_confirmation'
    })
    if ($previews.Count -eq 0) {
        if ($AllowLegacyMissing) {
            return $null
        }
        throw '缺少 Glue authority 前必需的 pair_layout_preview。'
    }
    if ($previews.Count -gt 3) {
        throw "pair_layout_preview 超过固定三次上限：$($previews.Count)。"
    }

    for ($index = 0; $index -lt $previews.Count; ++$index) {
        $preview = $previews[$index]
        Assert-RecordProperties -Record $preview -Context (
            "pair_layout_preview attempt $($index + 1)") -AllowNull -Names @(
            'attempt', 'attempt_limit', 'result', 'reason',
            'same_monitor', 'same_dpi',
            'leader_target_consent_prefix_valid',
            'follower_target_consent_prefix_valid',
            'follower_baseline_excluded_leader', 'pair_distinct',
            'glue_authority_bound', 'glue_authority_consumed',
            'temporary_peer_exception_retained',
            'native_apply_attempted', 'event_source_armed',
            'leader_size', 'follower_size', 'work_area_size',
            'horizontal_required', 'horizontal_available',
            'horizontal_excess', 'horizontal_fits',
            'vertical_required', 'vertical_available', 'vertical_excess',
            'vertical_fits', 'orientation'
        )
        Assert-RecordProperties -Record $preview -Context (
            "pair_layout_preview attempt $($index + 1)") -Names @(
            'attempt', 'attempt_limit', 'result', 'reason', 'same_monitor',
            'same_dpi', 'leader_target_consent_prefix_valid',
            'follower_target_consent_prefix_valid',
            'follower_baseline_excluded_leader', 'pair_distinct',
            'glue_authority_bound', 'glue_authority_consumed',
            'temporary_peer_exception_retained', 'native_apply_attempted',
            'event_source_armed', 'horizontal_fits', 'vertical_fits'
        )
        if ([int] $preview.attempt -ne ($index + 1) -or
            [int] $preview.attempt_limit -ne 3 -or
            [uint64] $preview.harness_sequence -ge
                [uint64] $Pair.harness_sequence -or
            $preview.leader_target_consent_prefix_valid -ne $true -or
            $preview.follower_target_consent_prefix_valid -ne $true -or
            $preview.follower_baseline_excluded_leader -ne $true -or
            $preview.pair_distinct -ne $true -or
            $preview.glue_authority_bound -ne $false -or
            $preview.glue_authority_consumed -ne $false -or
            $preview.temporary_peer_exception_retained -ne $false -or
            $preview.native_apply_attempted -ne $false -or
            $preview.event_source_armed -ne $false) {
            throw "pair_layout_preview attempt $($index + 1) 的 identity/authority/side-effect gate 未通过。"
        }

        if ($preview.result -eq 'INVALIDATED') {
            if (($preview.reason -ne 'target_changed' -and
                 $preview.reason -ne 'monitor_or_dpi_mismatch' -and
                 $preview.reason -ne 'unsafe_layout') -or
                ($preview.reason -eq 'monitor_or_dpi_mismatch' -and
                 $preview.same_monitor -eq $true -and
                 $preview.same_dpi -eq $true) -or
                ($preview.reason -eq 'unsafe_layout' -and
                 ($preview.same_monitor -ne $true -or
                  $preview.same_dpi -ne $true)) -or
                $null -ne $preview.leader_size -or
                $null -ne $preview.follower_size -or
                $null -ne $preview.work_area_size -or
                $null -ne $preview.horizontal_required -or
                $null -ne $preview.horizontal_available -or
                $null -ne $preview.horizontal_excess -or
                $preview.horizontal_fits -ne $false -or
                $null -ne $preview.vertical_required -or
                $null -ne $preview.vertical_available -or
                $null -ne $preview.vertical_excess -or
                $preview.vertical_fits -ne $false -or
                $null -ne $preview.orientation) {
                throw "pair_layout_preview attempt $($index + 1) 的 INVALIDATED 证据矛盾。"
            }
            continue
        }

        if ($preview.same_monitor -ne $true -or $preview.same_dpi -ne $true) {
            throw "pair_layout_preview attempt $($index + 1) 的 layout geometry 缺少 same-monitor/DPI 前提。"
        }

        $leaderWidth = [int64] $preview.leader_size.width
        $leaderHeight = [int64] $preview.leader_size.height
        $followerWidth = [int64] $preview.follower_size.width
        $followerHeight = [int64] $preview.follower_size.height
        $workWidth = [int64] $preview.work_area_size.width
        $workHeight = [int64] $preview.work_area_size.height
        $horizontalWidth = [int64] $preview.horizontal_required.width
        $horizontalHeight = [int64] $preview.horizontal_required.height
        $verticalWidth = [int64] $preview.vertical_required.width
        $verticalHeight = [int64] $preview.vertical_required.height
        if ($leaderWidth -le 0 -or $leaderHeight -le 0 -or
            $followerWidth -le 0 -or $followerHeight -le 0 -or
            $workWidth -le 0 -or $workHeight -le 0 -or
            $horizontalWidth -ne ($leaderWidth + $followerWidth) -or
            $horizontalHeight -ne [Math]::Max($leaderHeight, $followerHeight) -or
            $verticalWidth -ne [Math]::Max($leaderWidth, $followerWidth) -or
            $verticalHeight -ne ($leaderHeight + $followerHeight) -or
            [int64] $preview.horizontal_available.width -ne $workWidth -or
            [int64] $preview.horizontal_available.height -ne $workHeight -or
            [int64] $preview.vertical_available.width -ne $workWidth -or
            [int64] $preview.vertical_available.height -ne $workHeight) {
            throw "pair_layout_preview attempt $($index + 1) 的实时尺寸或 required/available 数学不一致。"
        }

        $horizontalFits = $horizontalWidth -le $workWidth -and
            $horizontalHeight -le $workHeight
        $verticalFits = $verticalWidth -le $workWidth -and
            $verticalHeight -le $workHeight
        $horizontalExcessWidth = [Math]::Max(
            [int64] 0, $horizontalWidth - $workWidth)
        $horizontalExcessHeight = [Math]::Max(
            [int64] 0, $horizontalHeight - $workHeight)
        $verticalExcessWidth = [Math]::Max(
            [int64] 0, $verticalWidth - $workWidth)
        $verticalExcessHeight = [Math]::Max(
            [int64] 0, $verticalHeight - $workHeight)
        if ($preview.horizontal_fits -ne $horizontalFits -or
            $preview.vertical_fits -ne $verticalFits -or
            [int64] $preview.horizontal_excess.width -ne
                $horizontalExcessWidth -or
            [int64] $preview.horizontal_excess.height -ne
                $horizontalExcessHeight -or
            [int64] $preview.vertical_excess.width -ne $verticalExcessWidth -or
            [int64] $preview.vertical_excess.height -ne $verticalExcessHeight) {
            throw "pair_layout_preview attempt $($index + 1) 的 fits/excess 数学不一致。"
        }

        if ($preview.result -eq 'FIT') {
            $expectedOrientation = if ($horizontalFits) {
                'horizontal'
            } elseif ($verticalFits) {
                'vertical'
            } else {
                $null
            }
            if ($preview.reason -ne 'eligible' -or
                $null -eq $expectedOrientation -or
                $preview.orientation -ne $expectedOrientation) {
                throw "pair_layout_preview attempt $($index + 1) 的 FIT/orientation 非法。"
            }
        } elseif ($preview.result -eq 'NEEDS_MANUAL_RESIZE') {
            if ($preview.reason -ne 'unsafe_layout' -or
                $horizontalFits -or $verticalFits -or
                $null -ne $preview.orientation) {
                throw "pair_layout_preview attempt $($index + 1) 的 NEEDS_MANUAL_RESIZE 证据矛盾。"
            }
        } elseif ($preview.result -ne 'INVALIDATED') {
            throw "pair_layout_preview result 不属于当前可 Seal 集合：$($preview.result)"
        }
    }

    for ($index = 0; $index -lt $previews.Count; ++$index) {
        $attempt = $index + 1
        $preview = $previews[$index]
        $matches = @($rechecks | Where-Object {
            [int] $_.attempt -eq $attempt
        })
        foreach ($match in $matches) {
            Assert-RecordProperties -Record $match `
                -Context "pair_layout_recheck_confirmation attempt $attempt" `
                -Names @('attempt', 'result', 'input_source',
                          'native_apply_attempted')
        }
        $hasLaterPreview = $index -lt ($previews.Count - 1)
        if ($hasLaterPreview) {
            if ($preview.result -ne 'NEEDS_MANUAL_RESIZE' -or
                $matches.Count -ne 1 -or
                $matches[0].result -ne 'CONFIRMED' -or
                $matches[0].input_source -ne 'interactive_console' -or
                $matches[0].native_apply_attempted -ne $false -or
                [uint64] $preview.harness_sequence -ge
                    [uint64] $matches[0].harness_sequence -or
                [uint64] $matches[0].harness_sequence -ge
                    [uint64] $previews[$index + 1].harness_sequence) {
                throw "attempt $attempt 的 manual-resize recheck evidence 不完整。"
            }
        } elseif ($matches.Count -eq 1) {
            if ($preview.result -ne 'NEEDS_MANUAL_RESIZE' -or
                $attempt -ge 3 -or
                $matches[0].result -ne 'DECLINED' -or
                $matches[0].input_source -ne 'interactive_console' -or
                $matches[0].native_apply_attempted -ne $false -or
                [uint64] $preview.harness_sequence -ge
                    [uint64] $matches[0].harness_sequence -or
                [uint64] $matches[0].harness_sequence -ge
                    [uint64] $Pair.harness_sequence) {
                throw "attempt $attempt 的 declined recheck evidence 非法。"
            }
        } elseif ($matches.Count -ne 0 -or
            ($preview.result -eq 'NEEDS_MANUAL_RESIZE' -and $attempt -lt 3)) {
            throw "attempt $attempt 缺少唯一 recheck confirmation。"
        }
    }
    if (@($rechecks | Where-Object {
            [int] $_.attempt -lt 1 -or [int] $_.attempt -gt $previews.Count
        }).Count -ne 0 -or
        @($rechecks | Group-Object attempt | Where-Object {
            $_.Count -ne 1
        }).Count -ne 0) {
        throw 'pair_layout_recheck_confirmation attempt 非法或重复。'
    }
    return $previews[-1]
}

$observerProcess = $null
$observerExitCode = $null
$harnessExitCode = $null
if ($PSCmdlet.ParameterSetName -eq 'Run') {
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
            # Deliberately run in the inherited foreground console. Do not pipe
            # or redirect stdin/stdout: consent must originate in ReadConsoleW.
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
            $observerExitCode = $observerProcess.ExitCode
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
} else {
    $prefix = if ([System.IO.Path]::IsPathRooted($ValidateEvidencePrefix)) {
        [System.IO.Path]::GetFullPath($ValidateEvidencePrefix)
    } else {
        [System.IO.Path]::GetFullPath(
            (Join-Path -Path $repositoryRoot -ChildPath $ValidateEvidencePrefix))
    }
    $observerStdout = "$prefix-glue-observer.stdout.jsonl"
    $observerStderr = "$prefix-glue-observer.stderr.log"
    $harnessEvidence = "$prefix-glue-harness.jsonl"
    $harnessExitCode = $ValidationHarnessExitCode
    $observerExitCode = $ValidationObserverExitCode
    Write-Output "仅离线校验已有 evidence prefix：$prefix"
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
    Assert-RecordProperties -Record $observerRecords[$index] `
        -Context "Observer record $($index + 1)" `
        -Names @('schema_version', 'record_kind', 'observer_sequence')
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
if ([uint64] $hookRegistration[0].observer_sequence -ge
        [uint64] $hookShutdown[0].observer_sequence -or
    [uint64] $hookShutdown[0].observer_sequence -ge
        [uint64] $observerShutdown[0].observer_sequence -or
    [uint64] $observerShutdown[0].observer_sequence -ne
        [uint64] $observerRecords.Count) {
    throw 'Observer hook registration < hook shutdown < observer shutdown 顺序非法。'
}

$harnessRecords = @(Read-StrictJsonLines -Path $harnessEvidence)
if (@($harnessRecords | Where-Object {
        [int] $_.schema_version -ne 1 -or
        $_.schema_name -ne 'panebind.r1c2b.explorer_glue'
    }).Count -ne 0) {
    throw 'Harness schema 不符合 R1-C2B evidence contract。'
}
for ($index = 0; $index -lt $harnessRecords.Count; ++$index) {
    Assert-RecordProperties -Record $harnessRecords[$index] `
        -Context "Harness record $($index + 1)" `
        -Names @('schema_version', 'schema_name', 'harness_sequence',
                 'record_kind', 'recorded_at')
    $sequence = $harnessRecords[$index].PSObject.Properties['harness_sequence']
    if ($null -eq $sequence -or
        [uint64] $sequence.Value -ne [uint64] ($index + 1)) {
        throw 'Harness physical JSONL sequence 不连续或缺失。'
    }
}
$knownHarnessRecordKinds = @(
    'startup', 'nonce_target', 'baseline', 'target_consent_prompt',
    'target_consent_confirmation', 'candidate_selection',
    'native_target_identity', 'pair_layout_preview',
    'pair_layout_recheck_confirmation', 'pair_validation', 'diagnostic',
    'glue_step', 'glue_consent_prompt', 'glue_consent_confirmation',
    'glue_authority', 'glue_native_bindings', 'drag_prompt',
    'internal_trace', 'operation', 'feedback_reconciliation', 'facts',
    'summary', 'shutdown'
)
$unknownKinds = @($harnessRecords | Where-Object {
    $knownHarnessRecordKinds -notcontains $_.record_kind
})
if ($unknownKinds.Count -ne 0) {
    throw "Harness 含未知 record_kind：$($unknownKinds[0].record_kind)"
}

$startup = Assert-UniqueRecord -Records $harnessRecords -Kind 'startup'
$summary = Assert-UniqueRecord -Records $harnessRecords -Kind 'summary'
$shutdown = Assert-UniqueRecord -Records $harnessRecords -Kind 'shutdown'
Assert-RecordProperties -Record $startup -Context 'startup' -Names @(
    'input_source', 'synthetic_input', 'r0_observer_runtime_dependency',
    'target_window_count'
)
Assert-RecordProperties -Record $summary -Context 'summary' -Names @(
    'result', 'reason', 'glue_reason', 'glue_stage',
    'implementation_ready', 'runtime_gate', 'behavior_state',
    'leader_start_count', 'leader_location_count', 'leader_end_count',
    'follower_feedback_count', 'follower_native_apply_count',
    'active_follower_operation_count', 'follower_noop_count',
    'suppressed_feedback_count', 'duplicate_feedback_count',
    'missing_feedback_count', 'reconciled_feedback_count',
    'acknowledged_operation_count', 'reconciled_operation_count',
    'feedback_operation_correlation_valid', 'trace_generation_valid',
    'feedback_suppression_evidence', 'unexpected_feedback_count',
    'recursive_follower_operation_count',
    'all_active_follower_operations_exact', 'queue_overflow',
    'max_event_queue_depth', 'max_pending_depth', 'event_source_armed',
    'event_source_stopped', 'event_source_lifecycle_clean',
    'topology_frozen_exact_pair', 'leader_restored_exact',
    'follower_restored_exact', 'user_preexisting_windows_touched',
    'other_third_party_control', 'global_input_control',
    'user_windows_close_attempted', 'r0_observer_runtime_dependency',
    'r0_observer_semantics_changed'
)
Assert-RecordProperties -Record $shutdown -Context 'shutdown' `
    -Names @('disposition')
if ($startup.input_source -ne 'interactive_console' -or
    $startup.synthetic_input -ne $false -or
    $startup.r0_observer_runtime_dependency -ne $false -or
    [int] $startup.target_window_count -ne 2 -or
    $shutdown.disposition -ne 'complete' -or
    [uint64] $startup.harness_sequence -ne 1 -or
    [uint64] $summary.harness_sequence -ne
        [uint64] ($harnessRecords.Count - 1) -or
    [uint64] $shutdown.harness_sequence -ne [uint64] $harnessRecords.Count) {
    throw 'Harness startup/shutdown authority contract 未通过。'
}
$previewSupportedProperty =
    $startup.PSObject.Properties['layout_readiness_preview_supported']
$previewLimitProperty =
    $startup.PSObject.Properties['layout_readiness_attempt_limit']
$previewContractSupported = $null -ne $previewSupportedProperty
if ($previewContractSupported) {
    if ($previewSupportedProperty.Value -ne $true -or
        $null -eq $previewLimitProperty -or
        [int] $previewLimitProperty.Value -ne 3) {
        throw 'Harness layout readiness preview startup contract 非法。'
    }
} elseif ($null -ne $previewLimitProperty -or
    @($harnessRecords | Where-Object {
        $_.record_kind -eq 'pair_layout_preview' -or
        $_.record_kind -eq 'pair_layout_recheck_confirmation'
    }).Count -ne 0) {
    throw 'Legacy startup 与 layout readiness preview records 矛盾。'
}
if (-not $previewContractSupported) {
    $legacyPrefix = [System.IO.Path]::GetFullPath((Join-Path `
        (Join-Path $repositoryRoot 'uat/r1c2b') `
        '20260903T044532644Z'))
    if ($PSCmdlet.ParameterSetName -ne 'ValidateEvidence' -or
        -not [string]::Equals(
            $prefix, $legacyPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Legacy evidence 仅允许离线校验固定 Attempt 1 prefix。'
    }
    $legacyHashes = @{
        $harnessEvidence =
            'D448272D5ED4391D46B9172FC35A5A5BE04A7A408EAEDC3D26A52A7FFD8B76A7'
        $observerStdout =
            'D73124CA08605243A0E906D1C1393E5D9E42F62EB5A5988534BE21AEC74FB917'
        $observerStderr =
            'E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855'
    }
    foreach ($path in $legacyHashes.Keys) {
        $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash
        if (-not [string]::Equals(
                $actualHash, $legacyHashes[$path],
                [StringComparison]::OrdinalIgnoreCase)) {
            throw "Legacy Attempt 1 SHA-256 不匹配：$path"
        }
    }
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
    foreach ($record in $records) {
        Assert-RecordProperties -Record $record `
            -Context "$($record.record_kind) target record" -Names @('role')
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

    $nonce = @($nonceTargets | Where-Object { $_.role -eq $role })[0]
    $confirmation = @($targetConfirmations | Where-Object {
        $_.role -eq $role
    })[0]
    $native = @($nativeTargets | Where-Object { $_.role -eq $role })[0]
    Assert-RecordProperties -Record $nonce -Context "$role nonce_target" `
        -Names @('target_id', 'created_empty', 'full_path_redacted')
    Assert-RecordProperties -Record $baseline -Context "$role baseline" `
        -Names @('result', 'baseline_generation',
                 'baseline_exclusion_complete',
                 'target_directory_contract_verified')
    Assert-RecordProperties -Record $prompt `
        -Context "$role target_consent_prompt" `
        -Names @('result', 'generation', 'input_source')
    Assert-RecordProperties -Record $confirmation `
        -Context "$role target_consent_confirmation" `
        -Names @('result', 'input_source', 'native_apply_attempted')
    Assert-RecordProperties -Record $candidate `
        -Context "$role candidate_selection" -Names @(
            'result', 'authority_kind', 'unique_new_target',
            'exact_target_location', 'preexisting_exact_location_detected',
            'baseline_generation', 'target_prompt_generation',
            'target_confirmation_generation', 'eligibility_generation',
            'token_generation'
        )
    Assert-RecordProperties -Record $native `
        -Context "$role native_target_identity" -Names @(
            'process', 'native_key', 'pid', 'tid',
            'capability_generation', 'consent_generation'
        )
    if ([uint64] $nonce.harness_sequence -ge
            [uint64] $baseline.harness_sequence -or
        [uint64] $baseline.harness_sequence -ge
            [uint64] $prompt.harness_sequence -or
        [uint64] $prompt.harness_sequence -ge
            [uint64] $confirmation.harness_sequence -or
        [uint64] $confirmation.harness_sequence -ge
            [uint64] $candidate.harness_sequence -or
        [uint64] $candidate.harness_sequence -ge
            [uint64] $native.harness_sequence) {
        throw "$role target 的 evidence record 顺序非法。"
    }
}

$leaderNative = @($nativeTargets | Where-Object { $_.role -eq 'leader' })[0]
$followerNative = @($nativeTargets | Where-Object { $_.role -eq 'follower' })[0]
$followerBaseline = @($baselines | Where-Object { $_.role -eq 'follower' })[0]
if ([uint64] $leaderNative.native_key -eq [uint64] $followerNative.native_key -or
    $leaderNative.process -ne 'explorer.exe' -or
    $followerNative.process -ne 'explorer.exe' -or
    [uint32] $leaderNative.pid -eq 0 -or
    [uint32] $followerNative.pid -eq 0 -or
    [uint32] $leaderNative.tid -eq 0 -or
    [uint32] $followerNative.tid -eq 0 -or
    [uint64] $leaderNative.capability_generation -eq 0 -or
    [uint64] $followerNative.capability_generation -eq 0 -or
    [uint64] $leaderNative.consent_generation -ne
        [uint64] (@($candidates | Where-Object { $_.role -eq 'leader' })[0]).token_generation -or
    [uint64] $followerNative.consent_generation -ne
        [uint64] (@($candidates | Where-Object { $_.role -eq 'follower' })[0]).token_generation) {
    throw 'Leader/Follower native identity 必须 distinct 且 generation 非零。'
}
if ([uint64] $leaderNative.harness_sequence -ge
    [uint64] $followerBaseline.harness_sequence) {
    throw 'Follower baseline 必须在 Leader native target identity 签发后开始。'
}

$pair = Assert-UniqueRecord -Records $harnessRecords -Kind 'pair_validation'
Assert-RecordProperties -Record $pair -Context 'pair_validation' -AllowNull -Names @(
    'result', 'reason', 'leader_target_consent_prefix_valid',
    'follower_target_consent_prefix_valid',
    'follower_baseline_excluded_leader', 'pair_distinct',
    'same_monitor_and_dpi', 'test_layout_planned',
    'leader_original', 'follower_original'
)
Assert-RecordProperties -Record $pair -Context 'pair_validation' -Names @(
    'result', 'reason', 'leader_target_consent_prefix_valid',
    'follower_target_consent_prefix_valid',
    'follower_baseline_excluded_leader', 'pair_distinct',
    'same_monitor_and_dpi', 'test_layout_planned'
)

if ($pair.result -eq 'BLOCKED') {
    # Only explicitly reviewed pre-native reasons enter SAFE_BLOCKED. This is
    # not a generic "anything blocked is safe" fallback.
    $supportedPreNativeBlockers = @(
        'unsafe_layout',
        'target_changed',
        'monitor_or_dpi_mismatch'
    )
    if ($supportedPreNativeBlockers -notcontains $pair.reason -or
        (-not $previewContractSupported -and $pair.reason -ne 'unsafe_layout') -or
        $pair.leader_target_consent_prefix_valid -ne $true -or
        $pair.follower_target_consent_prefix_valid -ne $true -or
        $pair.follower_baseline_excluded_leader -ne $true -or
        $pair.pair_distinct -ne $true -or
        $pair.test_layout_planned -ne $false) {
        throw 'BLOCKED pair_validation 不是受支持且证据完整的 pre-native blocker。'
    }
    if (($pair.reason -eq 'unsafe_layout' -and
         $pair.same_monitor_and_dpi -ne $true) -or
        ($pair.reason -eq 'monitor_or_dpi_mismatch' -and
         $pair.same_monitor_and_dpi -ne $false)) {
        throw "pair_validation reason=$($pair.reason) 与 monitor/DPI facts 矛盾。"
    }

    # Attempt 1 predates the readiness-preview record and remains valid legacy
    # evidence. A post-fix startup makes the preview contract mandatory.
    $finalBlockedPreview = Assert-LayoutPreviewEvidence `
        -Records $harnessRecords -Pair $pair `
        -AllowLegacyMissing:(-not $previewContractSupported)
    $blockedPreviews = @($harnessRecords | Where-Object {
        $_.record_kind -eq 'pair_layout_preview'
    })
    if ($previewContractSupported -and
        ([uint64] $blockedPreviews[0].harness_sequence -le
            [uint64] $followerNative.harness_sequence -or
         ($finalBlockedPreview.result -ne 'FIT' -and
          $finalBlockedPreview.reason -ne $pair.reason) -or
         [int] $summary.layout_preview_attempt_count -ne
            $blockedPreviews.Count -or
         $summary.layout_readiness_preview_supported -ne $true -or
         $summary.layout_preview_fit -ne
            ($finalBlockedPreview.result -eq 'FIT') -or
         $summary.layout_preview_side_effect_free -ne $true)) {
        throw 'Pre-native blocker summary 与 layout readiness preview 证据矛盾。'
    }

    $forbiddenAfterPairKinds = @(
        'glue_consent_prompt',
        'glue_consent_confirmation',
        'glue_authority',
        'glue_native_bindings',
        'glue_step',
        'drag_prompt',
        'operation',
        'feedback_reconciliation',
        'internal_trace',
        'facts'
    )
    $nativeApplyRecords = @($harnessRecords | Where-Object {
        $property = $_.PSObject.Properties['native_apply_attempted']
        $null -ne $property -and $property.Value -eq $true
    })
    if ($nativeApplyRecords.Count -ne 0) {
        throw "Pre-native blocker evidence 含 native_apply_attempted=true，实际 $($nativeApplyRecords.Count) 条。"
    }
    foreach ($kind in $forbiddenAfterPairKinds) {
        $matches = @($harnessRecords | Where-Object {
            $_.record_kind -eq $kind
        })
        if ($matches.Count -ne 0) {
            throw "Pre-native blocker 后不应存在 '$kind'，实际 $($matches.Count)。"
        }
    }

    $expectedBlockedFeedbackEvidence = if ($previewContractSupported) {
        'not_reached'
    } else {
        # Historical Attempt 1 emitted this default before the field was made
        # stage-aware. It is accepted only as legacy pre-native evidence and is
        # never interpreted as runtime feedback observation.
        'no_feedback_event_reconciled'
    }
    if ([uint64] $pair.harness_sequence -ge [uint64] $summary.harness_sequence -or
        $summary.result -ne 'BLOCKED' -or
        $summary.runtime_gate -ne 'BLOCKED' -or
        $summary.implementation_ready -ne $true -or
        $summary.reason -ne 'pair_validation_blocked' -or
        $summary.glue_reason -ne $pair.reason -or
        $summary.glue_stage -ne 'pair_validation' -or
        $summary.behavior_state -ne 'idle' -or
        [int] $summary.leader_start_count -ne 0 -or
        [int] $summary.leader_location_count -ne 0 -or
        [int] $summary.leader_end_count -ne 0 -or
        [int] $summary.follower_feedback_count -ne 0 -or
        [int] $summary.follower_native_apply_count -ne 0 -or
        [int] $summary.active_follower_operation_count -ne 0 -or
        [int] $summary.follower_noop_count -ne 0 -or
        [int] $summary.suppressed_feedback_count -ne 0 -or
        [int] $summary.duplicate_feedback_count -ne 0 -or
        [int] $summary.missing_feedback_count -ne 0 -or
        [int] $summary.reconciled_feedback_count -ne 0 -or
        [int] $summary.acknowledged_operation_count -ne 0 -or
        [int] $summary.reconciled_operation_count -ne 0 -or
        $summary.feedback_operation_correlation_valid -ne $false -or
        $summary.trace_generation_valid -ne $false -or
        $summary.feedback_suppression_evidence -ne
            $expectedBlockedFeedbackEvidence -or
        [int] $summary.unexpected_feedback_count -ne 0 -or
        [int] $summary.recursive_follower_operation_count -ne 0 -or
        $summary.all_active_follower_operations_exact -ne $false -or
        $summary.queue_overflow -ne $false -or
        [int] $summary.max_event_queue_depth -ne 0 -or
        [int] $summary.max_pending_depth -ne 0 -or
        $summary.event_source_armed -ne $false -or
        $summary.event_source_stopped -ne $false -or
        $summary.event_source_lifecycle_clean -ne $false -or
        $summary.topology_frozen_exact_pair -ne $false -or
        $summary.leader_restored_exact -ne $false -or
        $summary.follower_restored_exact -ne $false -or
        $summary.user_preexisting_windows_touched -ne $false -or
        $summary.other_third_party_control -ne $false -or
        $summary.global_input_control -ne $false -or
        $summary.user_windows_close_attempted -ne $false -or
        $summary.r0_observer_runtime_dependency -ne $false -or
        $summary.r0_observer_semantics_changed -ne $false) {
        throw 'Pre-native blocker summary 与 zero-operation 安全状态矛盾。'
    }

    if ($observerExitCode -ne 0) {
        throw "Observer 失败：exit=$observerExitCode"
    }
    if ($harnessExitCode -ne 1) {
        throw "Harness exit=$harnessExitCode 与 SAFE_BLOCKED contract（exit=1）矛盾。"
    }

    Write-Output 'R1-C2B evidence outcome: SAFE_BLOCKED / KNOWN_BLOCKED'
    $humanBlocker = switch ($pair.reason) {
        'unsafe_layout' { 'UnsafeLayout' }
        'target_changed' { 'TargetChanged' }
        'monitor_or_dpi_mismatch' { 'MonitorOrDpiMismatch' }
    }
    Write-Output ("R1-C2B UAT 安全阻断：pair_validation / {0}" -f
        $humanBlocker)
    Write-Output '没有进入 Glue 授权；没有建立 native binding 或 arm event source。'
    Write-Output '没有执行 Glue native apply；没有触碰既有窗口、其他应用或全局输入。'
    if ($pair.reason -eq 'unsafe_layout') {
        Write-Output '原始 evidence 已保留；请按 layout readiness 尺寸提示调整后重新执行 UAT。'
    } elseif ($pair.reason -eq 'monitor_or_dpi_mismatch') {
        Write-Output '原始 evidence 已保留；请将两目标置于同一 monitor/DPI 后重新执行 UAT。'
    } else {
        Write-Output '原始 evidence 已保留；目标身份或位置已变化，请重新开始 UAT。'
    }
    exit 2
}

if ($pair.result -ne 'PASS' -or $pair.reason -ne 'eligible') {
    throw "pair_validation result 非法：$($pair.result)"
}

$finalPassPreview = Assert-LayoutPreviewEvidence `
    -Records $harnessRecords -Pair $pair
if ($finalPassPreview.result -ne 'FIT') {
    throw 'PASS pair_validation 前的最后一次 preview 必须为 FIT。'
}
$passPreviews = @($harnessRecords | Where-Object {
    $_.record_kind -eq 'pair_layout_preview'
})
if (-not $previewContractSupported -or
    [uint64] $passPreviews[0].harness_sequence -le
        [uint64] $followerNative.harness_sequence -or
    $summary.layout_readiness_preview_supported -ne $true -or
    [int] $summary.layout_preview_attempt_count -ne $passPreviews.Count -or
    $summary.layout_preview_fit -ne $true -or
    $summary.layout_preview_side_effect_free -ne $true) {
    throw 'PASS summary 与 mandatory side-effect-free preview 证据矛盾。'
}

$gluePrompt = Assert-UniqueRecord -Records $harnessRecords -Kind 'glue_consent_prompt'
$glueConfirmation = Assert-UniqueRecord -Records $harnessRecords -Kind 'glue_consent_confirmation'
$glueAuthority = Assert-UniqueRecord -Records $harnessRecords -Kind 'glue_authority'
$glueNativeBindings = Assert-UniqueRecord -Records $harnessRecords -Kind 'glue_native_bindings'
Assert-RecordProperties -Record $gluePrompt -Context 'glue_consent_prompt' `
    -Names @('result', 'generation', 'input_source')
Assert-RecordProperties -Record $glueConfirmation `
    -Context 'glue_consent_confirmation' `
    -Names @('result', 'input_source', 'native_apply_attempted')
Assert-RecordProperties -Record $glueAuthority -Context 'glue_authority' `
    -Names @('result', 'pair_preview_generation', 'prompt_generation',
             'confirmation_generation', 'authority_generation')
Assert-RecordProperties -Record $glueNativeBindings `
    -Context 'glue_native_bindings' `
    -Names @('session_bindings_present', 'leader_native_key',
             'follower_native_key', 'leader_pid', 'follower_pid')
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
foreach ($stepName in @('glue_consent_prompt', 'setup_test_layout',
                        'arm_event_source', 'run_until_terminal')) {
    $step = @($steps | Where-Object { $_.step -eq $stepName })
    foreach ($item in $step) {
        Assert-RecordProperties -Record $item -Context "glue_step $stepName" `
            -Names @('step', 'result')
    }
    if ($step.Count -ne 1 -or $step[0].result -ne 'PASS') {
        throw "Glue step '$stepName' 未唯一 PASS。"
    }
}
if ($steps.Count -ne 4) {
    throw "PASS glue_step 集合不闭合，实际 $($steps.Count) 条。"
}
$promptStep = @($steps | Where-Object { $_.step -eq 'glue_consent_prompt' })[0]
$setupStep = @($steps | Where-Object { $_.step -eq 'setup_test_layout' })[0]
$armStep = @($steps | Where-Object { $_.step -eq 'arm_event_source' })[0]
$runStep = @($steps | Where-Object { $_.step -eq 'run_until_terminal' })[0]
$dragPrompt = Assert-UniqueRecord -Records $harnessRecords -Kind 'drag_prompt'
Assert-RecordProperties -Record $dragPrompt -Context 'drag_prompt' `
    -Names @('role', 'timeout_seconds', 'console_input_after_arm')
if ($dragPrompt.role -ne 'leader' -or
    $dragPrompt.console_input_after_arm -ne $false -or
    [int] $dragPrompt.timeout_seconds -lt 30 -or
    [int] $dragPrompt.timeout_seconds -gt 300 -or
    [uint64] $finalPassPreview.harness_sequence -ge
        [uint64] $pair.harness_sequence -or
    [uint64] $pair.harness_sequence -ge
        [uint64] $promptStep.harness_sequence -or
    [uint64] $promptStep.harness_sequence -ge
        [uint64] $gluePrompt.harness_sequence -or
    [uint64] $gluePrompt.harness_sequence -ge
        [uint64] $glueConfirmation.harness_sequence -or
    [uint64] $glueConfirmation.harness_sequence -ge
        [uint64] $glueAuthority.harness_sequence -or
    [uint64] $glueAuthority.harness_sequence -ge
        [uint64] $glueNativeBindings.harness_sequence -or
    [uint64] $glueNativeBindings.harness_sequence -ge
        [uint64] $setupStep.harness_sequence -or
    [uint64] $setupStep.harness_sequence -ge
        [uint64] $armStep.harness_sequence -or
    [uint64] $armStep.harness_sequence -ge
        [uint64] $dragPrompt.harness_sequence -or
    [uint64] $dragPrompt.harness_sequence -ge
        [uint64] $runStep.harness_sequence) {
    throw 'PASS preview/pair/consent/authority/setup/arm/drag/run evidence 顺序非法。'
}

$facts = Assert-UniqueRecord -Records $harnessRecords -Kind 'facts'
Assert-RecordProperties -Record $facts -Context 'facts' -AllowNull -Names @(
    'glue_session_generation', 'behavior_state', 'behavior_abort_reason',
    'glue_consent_confirmed', 'follower_baseline_excluded_leader',
    'test_layout_exact', 'topology_exact_two_window_component',
    'event_source_armed', 'event_source_stopped',
    'event_source_lifecycle_clean', 'leader_restore_attempted',
    'follower_restore_attempted', 'leader_restored_exact',
    'follower_restored_exact', 'user_windows_close_attempted',
    'follower_native_apply_count', 'follower_noop_count',
    'suppressed_feedback_count', 'duplicate_feedback_count',
    'missing_feedback_count', 'reconciled_feedback_count',
    'unexpected_feedback_count', 'max_pending_depth', 'pending_capacity',
    'max_event_queue_depth', 'event_queue_capacity',
    'leader_original', 'follower_original', 'leader_layout',
    'follower_layout', 'leader_final', 'follower_final',
    'leader_restored', 'follower_restored'
)
Assert-RecordProperties -Record $facts -Context 'facts' -Names @(
    'glue_session_generation', 'behavior_state', 'glue_consent_confirmed',
    'follower_baseline_excluded_leader', 'test_layout_exact',
    'topology_exact_two_window_component', 'event_source_armed',
    'event_source_stopped', 'event_source_lifecycle_clean',
    'leader_restore_attempted', 'follower_restore_attempted',
    'leader_restored_exact', 'follower_restored_exact',
    'user_windows_close_attempted', 'follower_native_apply_count',
    'follower_noop_count', 'suppressed_feedback_count',
    'duplicate_feedback_count', 'missing_feedback_count',
    'reconciled_feedback_count', 'unexpected_feedback_count',
    'max_pending_depth', 'pending_capacity', 'max_event_queue_depth',
    'event_queue_capacity', 'leader_original', 'follower_original',
    'leader_layout', 'follower_layout', 'leader_final', 'follower_final',
    'leader_restored', 'follower_restored'
)
$workArea = $pair.leader_original.work_area
$workWidth = [int64] $workArea.right - [int64] $workArea.left
$workHeight = [int64] $workArea.bottom - [int64] $workArea.top
if ((Get-RectKey $pair.follower_original.work_area) -ne
        (Get-RectKey $workArea) -or
    $workWidth -ne [int64] $finalPassPreview.work_area_size.width -or
    $workHeight -ne [int64] $finalPassPreview.work_area_size.height) {
    throw 'Final FIT preview work area 与 pair original snapshots 不一致。'
}
$required = if ($finalPassPreview.orientation -eq 'horizontal') {
    $finalPassPreview.horizontal_required
} else {
    $finalPassPreview.vertical_required
}
$layoutX = [int64] $workArea.left + [int64] [Math]::Floor(
    ($workWidth - [int64] $required.width) / 2)
$layoutY = [int64] $workArea.top + [int64] [Math]::Floor(
    ($workHeight - [int64] $required.height) / 2)
$leaderWidth = [int64] $finalPassPreview.leader_size.width
$leaderHeight = [int64] $finalPassPreview.leader_size.height
$followerWidth = [int64] $finalPassPreview.follower_size.width
$followerHeight = [int64] $finalPassPreview.follower_size.height
if ($finalPassPreview.orientation -eq 'horizontal') {
    $expectedLeaderVisible = [pscustomobject]@{
        left = $layoutX; top = $layoutY
        right = $layoutX + $leaderWidth; bottom = $layoutY + $leaderHeight
    }
    $expectedFollowerVisible = [pscustomobject]@{
        left = $layoutX + $leaderWidth; top = $layoutY
        right = $layoutX + $leaderWidth + $followerWidth
        bottom = $layoutY + $followerHeight
    }
} else {
    $expectedLeaderVisible = [pscustomobject]@{
        left = $layoutX; top = $layoutY
        right = $layoutX + $leaderWidth; bottom = $layoutY + $leaderHeight
    }
    $expectedFollowerVisible = [pscustomobject]@{
        left = $layoutX; top = $layoutY + $leaderHeight
        right = $layoutX + $followerWidth
        bottom = $layoutY + $leaderHeight + $followerHeight
    }
}
foreach ($role in @('leader', 'follower')) {
    $originalProperty = "${role}_original"
    $layoutProperty = "${role}_layout"
    $expectedVisible = if ($role -eq 'leader') {
        $expectedLeaderVisible
    } else {
        $expectedFollowerVisible
    }
    $original = $facts.$originalProperty
    $layout = $facts.$layoutProperty
    $layoutDelta = Get-RectTranslation $original.visible $expectedVisible `
        "$role planner visible"
    if ((Get-RectKey $layout.visible) -ne (Get-RectKey $expectedVisible) -or
        (Get-RectKey $layout.positioning) -ne
            (Get-TranslatedRectKey $original.positioning `
                $layoutDelta.dx $layoutDelta.dy) -or
        (Get-SnapshotNonGeometryKey $layout "$role layout") -ne
            (Get-SnapshotNonGeometryKey $original "$role original")) {
        throw "$role facts layout 与 centered zero-gap planner 不一致。"
    }
}
$leaderVisibleDelta = Get-RectTranslation `
    $facts.leader_layout.visible $facts.leader_final.visible 'Leader final visible'
$leaderPositioningDelta = Get-RectTranslation `
    $facts.leader_layout.positioning $facts.leader_final.positioning `
    'Leader final positioning'
$followerVisibleDelta = Get-RectTranslation `
    $facts.follower_layout.visible $facts.follower_final.visible `
    'Follower final visible'
$followerPositioningDelta = Get-RectTranslation `
    $facts.follower_layout.positioning $facts.follower_final.positioning `
    'Follower final positioning'
if (($leaderVisibleDelta.dx -eq 0 -and $leaderVisibleDelta.dy -eq 0) -or
    $leaderVisibleDelta.dx -ne $leaderPositioningDelta.dx -or
    $leaderVisibleDelta.dy -ne $leaderPositioningDelta.dy -or
    $leaderVisibleDelta.dx -ne $followerVisibleDelta.dx -or
    $leaderVisibleDelta.dy -ne $followerVisibleDelta.dy -or
    $leaderVisibleDelta.dx -ne $followerPositioningDelta.dx -or
    $leaderVisibleDelta.dy -ne $followerPositioningDelta.dy -or
    (Get-SnapshotNonGeometryKey $facts.leader_layout 'leader layout') -ne
        (Get-SnapshotNonGeometryKey $facts.leader_final 'leader final') -or
    (Get-SnapshotNonGeometryKey $facts.follower_layout 'follower layout') -ne
        (Get-SnapshotNonGeometryKey $facts.follower_final 'follower final')) {
    throw 'Leader/Follower final geometry 不是同尺寸、同一非零 total delta。'
}
$trace = @($harnessRecords | Where-Object { $_.record_kind -eq 'internal_trace' })
if ($trace.Count -eq 0) {
    throw 'Internal behavior trace 为空。'
}
for ($index = 0; $index -lt $trace.Count; ++$index) {
    Assert-RecordProperties -Record $trace[$index] -AllowNull `
        -Context "internal_trace $($index + 1)" -Names @(
            'trace_sequence', 'glue_session_generation', 'event_sequence',
            'role', 'event_kind', 'decision', 'abort_reason',
            'behavior_operation_generation', 'visible'
        )
    Assert-RecordProperties -Record $trace[$index] `
        -Context "internal_trace $($index + 1)" -Names @(
            'trace_sequence', 'glue_session_generation', 'event_sequence',
            'role', 'behavior_operation_generation'
        )
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
$followerMoveCommands = @($trace | Where-Object {
    $_.role -eq 'leader' -and $_.event_kind -eq 'geometry_changed' -and
    $_.decision -eq 'follower_move_requested'
})
$allowedFollowerGeometryDecisions = @(
    'feedback_acknowledged',
    'feedback_observed_pending_result',
    'duplicate_feedback_suppressed'
)
if ($leaderStart.Count -ne 1 -or $leaderLocation.Count -lt 1 -or
    $leaderEnd.Count -ne 1 -or
    @($trace | Where-Object { $_.decision -eq 'activated' }).Count -ne 1 -or
    @($trace | Where-Object { $_.decision -eq 'completing' }).Count -ne 1 -or
    @($trace | Where-Object { $_.decision -eq 'completed' }).Count -ne 1 -or
    @($trace | Where-Object {
        $_.role -eq 'follower' -and $_.decision -eq 'follower_move_requested'
    }).Count -ne 0 -or
    @($followerFeedback | Where-Object {
        $allowedFollowerGeometryDecisions -notcontains $_.decision
    }).Count -ne 0) {
    throw 'Internal START/LOCATION/END、completion 或 no-recursion trace gate 未通过。'
}

$operations = @($harnessRecords | Where-Object { $_.record_kind -eq 'operation' })
foreach ($operation in $operations) {
    Assert-RecordProperties -Record $operation -Context 'operation' -Names @(
        'phase', 'role', 'behavior_operation_generation',
        'source_leader_sequence', 'operation_id', 'reason_code', 'stage_code',
        'native_apply_attempted', 'native_outcome_known', 'cleanup_operation',
        'exact_receipt', 'before', 'requested_visible',
        'requested_positioning', 'actual', 'size_preserved',
        'identity_stable', 'location_stable', 'monitor_and_dpi_stable'
    )
    if ([uint64] $operation.operation_id -eq 0 -or
        $operation.native_apply_attempted -ne $true -or
        $operation.native_outcome_known -ne $true -or
        $operation.exact_receipt -ne $true -or
        $null -eq $operation.before -or $null -eq $operation.actual -or
        $null -eq $operation.requested_positioning -or
        $operation.size_preserved -ne $true -or
        $operation.identity_stable -ne $true -or
        $operation.location_stable -ne $true -or
        $operation.monitor_and_dpi_stable -ne $true) {
        throw 'PASS operation 缺少唯一、完整、exact native receipt。'
    }
    Assert-SnapshotProperties -Snapshot $operation.before `
        -Context 'operation.before'
    Assert-SnapshotProperties -Snapshot $operation.actual `
        -Context 'operation.actual'
}
if (@($operations | Group-Object operation_id | Where-Object {
        $_.Count -ne 1
    }).Count -ne 0 -or
    @($operations | Where-Object {
        ($_.phase -eq 'setup' -and
         ($_.role -ne 'leader' -and $_.role -ne 'follower')) -or
        ($_.phase -eq 'restore' -and
         ($_.role -ne 'leader' -and $_.role -ne 'follower')) -or
        ($_.phase -eq 'active_follower' -and $_.role -ne 'follower') -or
        (($_.phase -eq 'setup' -or $_.phase -eq 'restore') -and
         ([uint64] $_.behavior_operation_generation -ne 0 -or
          [uint64] $_.source_leader_sequence -ne 0)) -or
        ($_.phase -eq 'active_follower' -and
         ([uint64] $_.behavior_operation_generation -eq 0 -or
          [uint64] $_.source_leader_sequence -eq 0)) -or
        ($_.phase -ne 'setup' -and $_.phase -ne 'restore' -and
         $_.phase -ne 'active_follower') -or
        ($_.phase -eq 'restore' -and $_.cleanup_operation -ne $true) -or
        ($_.phase -ne 'restore' -and $_.cleanup_operation -ne $false)
    }).Count -ne 0) {
    throw 'Operation phase/role/id/cleanup 集合不闭合。'
}

foreach ($role in @('leader', 'follower')) {
    $originalProperty = "${role}_original"
    $layoutProperty = "${role}_layout"
    $finalProperty = "${role}_final"
    $restoredProperty = "${role}_restored"
    $restoreAttemptedProperty = "${role}_restore_attempted"
    $originalKey = Get-SnapshotKey $facts.$originalProperty `
        "facts.$originalProperty"
    $layoutKey = Get-SnapshotKey $facts.$layoutProperty `
        "facts.$layoutProperty"
    $finalKey = Get-SnapshotKey $facts.$finalProperty `
        "facts.$finalProperty"
    $restoredKey = Get-SnapshotKey $facts.$restoredProperty `
        "facts.$restoredProperty"
    $pairOriginalKey = Get-SnapshotKey $pair.$originalProperty `
        "pair_validation.$originalProperty"
    $expectedSetupCount = if ($originalKey -eq $layoutKey) { 0 } else { 1 }
    $expectedRestoreCount = if ($finalKey -eq $originalKey) { 0 } else { 1 }
    $setupForRole = @($operations | Where-Object {
        $_.phase -eq 'setup' -and $_.role -eq $role
    })
    $restoreForRole = @($operations | Where-Object {
        $_.phase -eq 'restore' -and $_.role -eq $role
    })
    if ($pairOriginalKey -ne $originalKey -or
        $restoredKey -ne $originalKey -or
        $setupForRole.Count -ne $expectedSetupCount -or
        $restoreForRole.Count -ne $expectedRestoreCount -or
        [bool] $facts.$restoreAttemptedProperty -ne
            ($expectedRestoreCount -eq 1)) {
        throw "$role setup/restore operation 集合与 facts snapshots 不闭合。"
    }
    if ($setupForRole.Count -eq 1 -and
        ((Get-SnapshotKey $setupForRole[0].before 'setup.before') -ne
            $originalKey -or
         (Get-SnapshotKey $setupForRole[0].actual 'setup.actual') -ne
            $layoutKey -or
         (Get-RectKey $setupForRole[0].requested_visible) -ne
            (Get-RectKey $facts.$layoutProperty.visible) -or
         (Get-RectKey $setupForRole[0].requested_positioning) -ne
            (Get-RectKey $facts.$layoutProperty.positioning))) {
        throw "$role setup operation geometry 与 original/layout facts 不闭合。"
    }
    if ($restoreForRole.Count -eq 1 -and
        ((Get-SnapshotKey $restoreForRole[0].before 'restore.before') -ne
            $finalKey -or
         (Get-SnapshotKey $restoreForRole[0].actual 'restore.actual') -ne
            $originalKey -or
         (Get-RectKey $restoreForRole[0].requested_visible) -ne
            (Get-RectKey $facts.$originalProperty.visible) -or
         (Get-RectKey $restoreForRole[0].requested_positioning) -ne
            (Get-RectKey $facts.$originalProperty.positioning))) {
        throw "$role restore operation geometry 与 final/original facts 不闭合。"
    }
}

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
$orderedActiveFollower = @($activeFollower | Sort-Object harness_sequence)
$expectedBeforeKey = Get-SnapshotKey $facts.follower_layout `
    'facts.follower_layout'
foreach ($operation in $orderedActiveFollower) {
    if ((Get-SnapshotKey $operation.before 'active_follower.before') -ne
        $expectedBeforeKey) {
        throw 'Active follower operations 的 before→actual 链不连续。'
    }
    $expectedBeforeKey = Get-SnapshotKey $operation.actual `
        'active_follower.actual'
}
if ($expectedBeforeKey -ne
    (Get-SnapshotKey $facts.follower_final 'facts.follower_final')) {
    throw 'Active follower operations 未从 follower_layout 闭合到 follower_final。'
}

$operationGenerations = @($activeFollower | ForEach-Object {
    [uint64] $_.behavior_operation_generation
})
if (@($operationGenerations | Where-Object { $_ -eq 0 }).Count -ne 0 -or
    @($operationGenerations | Group-Object | Where-Object { $_.Count -ne 1 }).Count -ne 0) {
    throw 'Active follower operation generation 必须非零且唯一。'
}
$commandGenerations = @($followerMoveCommands | ForEach-Object {
    [uint64] $_.behavior_operation_generation
})
if ($followerMoveCommands.Count -ne $activeFollower.Count -or
    @($commandGenerations | Where-Object { $_ -eq 0 }).Count -ne 0 -or
    @($commandGenerations | Group-Object | Where-Object {
        $_.Count -ne 1
    }).Count -ne 0 -or
    (($commandGenerations | Sort-Object) -join ',') -ne
        (($operationGenerations | Sort-Object) -join ',')) {
    throw 'FollowerMoveRequested commands 与 active operation generations 不一一对应。'
}
$reconciliations = @($harnessRecords | Where-Object {
    $_.record_kind -eq 'feedback_reconciliation'
})
foreach ($reconciliation in $reconciliations) {
    Assert-RecordProperties -Record $reconciliation -AllowNull `
        -Context 'feedback_reconciliation' -Names @(
            'operation_generation', 'source_leader_sequence',
            'command_trace_match_count', 'exact_operation_receipt',
            'expected_visible', 'actual_visible', 'disposition',
            'feedback_event_sequence'
        )
    Assert-RecordProperties -Record $reconciliation `
        -Context 'feedback_reconciliation' -Names @(
            'operation_generation', 'source_leader_sequence',
            'command_trace_match_count', 'exact_operation_receipt',
            'expected_visible', 'actual_visible', 'disposition'
        )
}
if ($reconciliations.Count -ne $activeFollower.Count) {
    throw '每个 active follower operation 必须有唯一 reconciliation record。'
}
if ([uint64] $runStep.harness_sequence -ge
        [uint64] $trace[0].harness_sequence -or
    [uint64] $trace[-1].harness_sequence -ge
        [uint64] $operations[0].harness_sequence -or
    [uint64] $operations[-1].harness_sequence -ge
        [uint64] $reconciliations[0].harness_sequence -or
    [uint64] $reconciliations[-1].harness_sequence -ge
        [uint64] $facts.harness_sequence -or
    [uint64] $facts.harness_sequence -ge
        [uint64] $summary.harness_sequence) {
    throw 'PASS run < trace < operations < reconciliation < facts < summary 顺序非法。'
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
$acknowledgedFeedbackSequences = @($reconciliations | Where-Object {
    $_.disposition -eq 'acknowledged_self_feedback'
} | ForEach-Object { [uint64] $_.feedback_event_sequence })
if (@($acknowledgedFeedbackSequences | Group-Object | Where-Object {
        $_.Count -ne 1
    }).Count -ne 0) {
    throw 'Acknowledged feedback_event_sequence 必须全局唯一。'
}
$lastAcknowledgedGeometry = $null
foreach ($feedback in @($followerFeedback | Sort-Object {
            [uint64] $_.event_sequence
        })) {
    $geometryKey = Get-RectKey $feedback.visible
    $matchingReconciliations = @($reconciliations | Where-Object {
        $null -ne $_.feedback_event_sequence -and
        [uint64] $_.feedback_event_sequence -eq
            [uint64] $feedback.event_sequence
    })
    if ($feedback.decision -eq 'feedback_acknowledged' -or
        $feedback.decision -eq 'feedback_observed_pending_result') {
        if ($matchingReconciliations.Count -ne 1 -or
            $matchingReconciliations[0].disposition -ne
                'acknowledged_self_feedback' -or
            (Get-RectKey $matchingReconciliations[0].expected_visible) -ne
                $geometryKey) {
            throw 'Follower acknowledged/pending feedback 未对应唯一 reconciliation。'
        }
        $lastAcknowledgedGeometry = $geometryKey
    } elseif ($feedback.decision -eq 'duplicate_feedback_suppressed') {
        if ($matchingReconciliations.Count -ne 0 -or
            $null -eq $lastAcknowledgedGeometry -or
            $geometryKey -ne $lastAcknowledgedGeometry) {
            throw 'Follower duplicate feedback 不等于最近 acknowledged geometry。'
        }
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
$allLeaderObserverEvents = @($observerRecords | Where-Object {
    $_.record_kind -eq 'event' -and $_.native_window_id -eq $leaderNativeId
})
$allFollowerObserverEvents = @($observerRecords | Where-Object {
    $_.record_kind -eq 'event' -and $_.native_window_id -eq $followerNativeId
})
$dragPhaseStart = ConvertTo-UtcEvidenceTime `
    -Record $dragPrompt -PropertyName 'recorded_at'
$dragPhaseEnd = ConvertTo-UtcEvidenceTime `
    -Record $runStep -PropertyName 'recorded_at'
if ($dragPhaseStart -ge $dragPhaseEnd) {
    throw 'drag_prompt/run_until_terminal recorded_at phase boundary 非法。'
}
$leaderObserverEvents = [Collections.Generic.List[object]]::new()
$followerObserverEvents = [Collections.Generic.List[object]]::new()
foreach ($eventRecord in @($allLeaderObserverEvents +
                           $allFollowerObserverEvents)) {
    Assert-RecordProperties -Record $eventRecord `
        -Context 'Target Observer event' `
        -Names @('received_at', 'native_window_id', 'native_event',
                 'callback_root_matches', 'native_object_id',
                 'native_child_id', 'field_errors')
    Assert-RecordProperties -Record $eventRecord.native_event `
        -Context 'Target Observer native_event' -Names @('name')
    $receivedAt = ConvertTo-UtcEvidenceTime `
        -Record $eventRecord -PropertyName 'received_at'
    # R0 OUTOFCONTEXT delivery is independent and may arrive after the Harness
    # records run completion. Use only the authority-phase lower bound; any
    # later extra lifecycle remains invalid under the UAT no-more-input rule.
    if ($receivedAt -ge $dragPhaseStart) {
        if ($eventRecord.native_window_id -eq $leaderNativeId) {
            [void] $leaderObserverEvents.Add($eventRecord)
        } else {
            [void] $followerObserverEvents.Add($eventRecord)
        }
    }
}
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
# The two independent OUTOFCONTEXT hooks do not have a shared END/unhook
# delivery boundary. External Follower LOCATION is retained as an audit count,
# while correctness is gated by the internal exact acknowledgement or explicit
# missing-feedback reconciliation plus both sources' lifecycle/error checks.
$targetObserverEvents = @($leaderObserverEvents.ToArray() +
                          $followerObserverEvents.ToArray())
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

if ($observerExitCode -ne 0) {
    throw "Observer 失败：exit=$observerExitCode"
}
if ($harnessExitCode -ne 0) {
    throw "Glue Harness 未通过：exit=$harnessExitCode, reason=$($summary.reason)"
}

Write-Output 'R1-C2B evidence outcome: PASS'
Write-Output "R1-C2B $Configuration Explorer Glue evidence gate: PASS"
Write-Output "Leader START/LOCATION/END: 1/$($externalLeaderLocation.Count)/1"
Write-Output "Follower active LOCATION: $($externalFollowerLocation.Count)"
Write-Output "Follower native applies: $($summary.follower_native_apply_count)"
Write-Output "Suppressed/duplicate/missing: $($summary.suppressed_feedback_count)/$($summary.duplicate_feedback_count)/$($summary.missing_feedback_count)"
Write-Output '两个 Explorer 均未自动关闭；请确认已自行关闭测试窗口。'
Write-Output '原始 evidence 已保存到 uat/r1c2b/。'
exit 0
