[CmdletBinding()]
param(
    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string] $BuildDirectory = 'out/r1c2a-debug'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path -Path $PSScriptRoot -ChildPath '..'))

if ([System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $buildRoot = [System.IO.Path]::GetFullPath($BuildDirectory)
} else {
    $buildRoot = [System.IO.Path]::GetFullPath(
        (Join-Path -Path $repositoryRoot -ChildPath $BuildDirectory))
}

$evidenceDirectory = Join-Path -Path $repositoryRoot -ChildPath 'uat/r1c2a'
[void] (New-Item -ItemType Directory -Path $evidenceDirectory -Force)

$timestamp = [DateTime]::UtcNow.ToString(
    'yyyyMMddTHHmmssfffZ',
    [Globalization.CultureInfo]::InvariantCulture)
$aggregatePath = Join-Path `
    -Path $evidenceDirectory `
    -ChildPath "$timestamp-provisioning-recovery.aggregate.json"
$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)

function Resolve-R1C2AProvisionHarness {
    $fileName = 'panebind-explorer-harness.exe'
    $platformOutput = Join-Path -Path $buildRoot -ChildPath 'src/platform/windows'
    $candidates = @(
        (Join-Path -Path $platformOutput -ChildPath "Debug/$fileName"),
        (Join-Path -Path $buildRoot -ChildPath "Debug/$fileName")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    throw 'HARNESS_NOT_FOUND'
}

function Get-RequiredJsonProperty {
    param(
        [Parameter(Mandatory = $true)]
        [psobject] $Object,

        [Parameter(Mandatory = $true)]
        [string] $Name
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw 'SUMMARY_PROPERTY_MISSING'
    }
    return $property.Value
}

function Read-StrictProvisionSummary {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path,

        [Parameter(Mandatory = $true)]
        [ValidateRange(1, 3)]
        [int] $ExpectedAttempt
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw 'STDOUT_MISSING'
    }
    $lines = [System.IO.File]::ReadAllLines($Path, $strictUtf8)
    if ($lines.Count -eq 0) {
        throw 'STDOUT_EMPTY'
    }

    $summaries = [System.Collections.Generic.List[object]]::new()
    foreach ($line in $lines) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            throw 'JSONL_BLANK_RECORD'
        }
        try {
            $record = $line | ConvertFrom-Json -ErrorAction Stop
        } catch {
            throw 'JSONL_INVALID'
        }
        if ($null -eq $record -or $record -is [System.Array]) {
            throw 'JSONL_RECORD_NOT_OBJECT'
        }
        if ((Get-RequiredJsonProperty -Object $record -Name 'schema_version') `
                -ne 2) {
            throw 'JSONL_SCHEMA_MISMATCH'
        }
        if ((Get-RequiredJsonProperty -Object $record -Name 'record_kind') `
                -eq 'summary') {
            $summaries.Add($record)
        }
    }

    if ($summaries.Count -ne 1) {
        throw 'SUMMARY_COUNT_INVALID'
    }
    $summary = $summaries[0]
    if ((Get-RequiredJsonProperty -Object $summary -Name 'mode') `
            -ne 'provision_only' -or
        (Get-RequiredJsonProperty -Object $summary -Name 'attempt_index') `
            -ne $ExpectedAttempt -or
        (Get-RequiredJsonProperty -Object $summary -Name 'result') -ne 'PASS' -or
        (Get-RequiredJsonProperty -Object $summary `
            -Name 'registration_subscription') -ne 'PASS' -or
        (Get-RequiredJsonProperty -Object $summary `
            -Name 'browser_subscription') -ne 'PASS' -or
        (Get-RequiredJsonProperty -Object $summary `
            -Name 'matching_registration_count') -ne 1 -or
        (Get-RequiredJsonProperty -Object $summary `
            -Name 'unresolved_registration_count') -ne 0 -or
        (Get-RequiredJsonProperty -Object $summary `
            -Name 'shared_window_identity_conflict_count') -ne 0 -or
        (Get-RequiredJsonProperty -Object $summary `
            -Name 'browser_identity_query_failure_count') -ne 0 -or
        (Get-RequiredJsonProperty -Object $summary `
            -Name 'capture_read_only') -ne 'PASS' -or
        (Get-RequiredJsonProperty -Object $summary `
            -Name 'stale_token_preflight') -ne 'PASS' -or
        (Get-RequiredJsonProperty -Object $summary `
            -Name 'native_translation_count') -ne 0 -or
        (Get-RequiredJsonProperty -Object $summary -Name 'safe_cleanup') `
            -ne 'PASS' -or
        (Get-RequiredJsonProperty -Object $summary `
            -Name 'attributable_orphan_count') -ne 0) {
        throw 'SUMMARY_GATE_FAILED'
    }

    $registrationRevoked = Get-RequiredJsonProperty `
        -Object $summary `
        -Name 'matching_registration_revoked'
    $hwndInvalidated = Get-RequiredJsonProperty `
        -Object $summary `
        -Name 'exact_hwnd_invalidated'
    if ($registrationRevoked -isnot [bool] -or
        $hwndInvalidated -isnot [bool] -or
        (-not $registrationRevoked -and -not $hwndInvalidated)) {
        throw 'SUMMARY_CLEANUP_PROOF_FAILED'
    }

    $requiredTrueFacts = @(
        'baseline_exclusion_complete',
        'canonical_iunknown_identity_matches',
        'hwnd_three_way_match',
        'exact_target_location',
        'token_issued'
    )
    foreach ($name in $requiredTrueFacts) {
        $value = Get-RequiredJsonProperty -Object $summary -Name $name
        if ($value -isnot [bool] -or -not $value) {
            throw 'SUMMARY_POSITIVE_ATTRIBUTION_FAILED'
        }
    }
    $preexisting = Get-RequiredJsonProperty `
        -Object $summary `
        -Name 'target_hwnd_preexisting'
    $navigationAmbiguous = Get-RequiredJsonProperty `
        -Object $summary `
        -Name 'browser_navigation_history_ambiguous'
    $existingTouched = Get-RequiredJsonProperty `
        -Object $summary `
        -Name 'user_existing_windows_touched'
    $otherControl = Get-RequiredJsonProperty `
        -Object $summary `
        -Name 'other_third_party_control'
    if ($preexisting -isnot [bool] -or $preexisting -or
        $navigationAmbiguous -isnot [bool] -or $navigationAmbiguous -or
        $existingTouched -isnot [bool] -or $existingTouched -or
        $otherControl -isnot [bool] -or $otherControl) {
        throw 'SUMMARY_SAFETY_FAILED'
    }
    return $summary
}

$attempts = [System.Collections.Generic.List[object]]::new()
$gate = 'BLOCKED'
$harnessPath = $null
$setupSucceeded = $false
try {
    if (-not (Test-Path -LiteralPath $buildRoot -PathType Container)) {
        throw 'BUILD_DIRECTORY_NOT_FOUND'
    }
    $harnessPath = Resolve-R1C2AProvisionHarness
    $setupSucceeded = $true
} catch {
    $attempts.Add([ordered]@{
        attempt_index = 1
        status = 'BLOCKED'
        failure = 'SETUP_FAILED'
    })
}

if ($setupSucceeded) {
    foreach ($attemptIndex in 1..3) {
        $stdoutPath = Join-Path `
            -Path $evidenceDirectory `
            -ChildPath "$timestamp-provision-attempt-$attemptIndex.stdout.jsonl"
        $stderrPath = Join-Path `
            -Path $evidenceDirectory `
            -ChildPath "$timestamp-provision-attempt-$attemptIndex.stderr.log"
        $attemptRecord = [ordered]@{
            attempt_index = $attemptIndex
            status = 'BLOCKED'
            stdout_file = [System.IO.Path]::GetFileName($stdoutPath)
            stderr_file = [System.IO.Path]::GetFileName($stderrPath)
            process_exit_code = $null
            validation = 'NOT_RUN'
        }
        $process = $null
        try {
            $process = Start-Process `
                -FilePath $harnessPath `
                -ArgumentList @(
                    '--provision-only',
                    '--attempt-index',
                    $attemptIndex.ToString(
                        [Globalization.CultureInfo]::InvariantCulture)
                ) `
                -WorkingDirectory $repositoryRoot `
                -WindowStyle Hidden `
                -RedirectStandardOutput $stdoutPath `
                -RedirectStandardError $stderrPath `
                -PassThru
            [void] $process.Handle
            $process.WaitForExit()
            $process.Refresh()
            $attemptRecord.process_exit_code = $process.ExitCode

            if (-not (Test-Path -LiteralPath $stderrPath -PathType Leaf) -or
                (Get-Item -LiteralPath $stderrPath).Length -ne 0) {
                throw 'HARNESS_STDERR_NOT_EMPTY'
            }

            $summary = Read-StrictProvisionSummary `
                -Path $stdoutPath `
                -ExpectedAttempt $attemptIndex
            if ($process.ExitCode -ne 0) {
                throw 'HARNESS_EXIT_NONZERO'
            }
            $attemptRecord.status = 'PASS'
            $attemptRecord.validation = 'PASS'
            $attemptRecord.matching_registration_count = `
                $summary.matching_registration_count
            $attemptRecord.safe_cleanup = $summary.safe_cleanup
            $attemptRecord.attributable_orphan_count = `
                $summary.attributable_orphan_count
        } catch {
            $attemptRecord.validation = 'FAILED'
        }
        $attempts.Add($attemptRecord)
        if ($attemptRecord.status -ne 'PASS') {
            break
        }
    }

    if ($attempts.Count -eq 3 -and
        @($attempts | Where-Object { $_.status -eq 'PASS' }).Count -eq 3) {
        $gate = 'PASS'
    }
}

$recordedAttemptIndexes = @($attempts | ForEach-Object { $_.attempt_index })
foreach ($attemptIndex in 1..3) {
    if ($recordedAttemptIndexes -notcontains $attemptIndex) {
        $attempts.Add([ordered]@{
            attempt_index = $attemptIndex
            status = 'NOT_RUN'
        })
    }
}

$orderedAttempts = @(
    $attempts | Sort-Object { [int] $_['attempt_index'] }
)

$aggregate = [ordered]@{
    schema_version = 2
    record_kind = 'provisioning_stability_aggregate'
    recorded_at = [DateTime]::UtcNow.ToString(
        'o',
        [Globalization.CultureInfo]::InvariantCulture)
    build_configuration = 'Debug'
    fixed_attempt_count = 3
    attempts = $orderedAttempts
    provisioning_stability_gate = $gate
    user_existing_windows_touched = $false
    other_third_party_control = $false
}
$aggregateJson = $aggregate | ConvertTo-Json -Depth 8
[System.IO.File]::WriteAllText($aggregatePath, $aggregateJson, $strictUtf8)

Write-Output "Aggregate evidence: $aggregatePath"
foreach ($attempt in $orderedAttempts) {
    Write-Output "Provision-only attempt $($attempt.attempt_index): $($attempt.status)"
}
Write-Output "PROVISIONING_STABILITY_GATE = $gate"

if ($gate -ne 'PASS') {
    exit 1
}
