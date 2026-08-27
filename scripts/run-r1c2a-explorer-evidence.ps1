[CmdletBinding()]
param(
    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string] $BuildDirectory = 'out/r1c2a-debug',

    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string] $Configuration = 'Debug',

    [Parameter(Mandatory = $true)]
    [ValidateRange(1, 86400)]
    [int] $ObserveSeconds,

    [Parameter(Mandatory = $true)]
    [ValidateRange(1, 86400)]
    [int] $HarnessHoldSeconds
)

throw 'DEPRECATED_DISABLED: automatic Explorer provisioning evidence is disabled. Use scripts/run-r1c2a-explorer-consent-evidence.ps1 for the foreground human-consent UAT.'

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# The harness can spend up to two eight-second bounded readiness phases, then
# hold the translated window and spend up to three seconds on exact graceful
# close. Keep a small startup/flush margin so the observer cannot terminate
# before the complete translation/restore lifecycle has been recorded.
$minimumObserveSeconds = $HarnessHoldSeconds + 25
if ($ObserveSeconds -lt $minimumObserveSeconds) {
    throw "ObserveSeconds must be at least HarnessHoldSeconds + 25 ($minimumObserveSeconds seconds)."
}

$repositoryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path -Path $PSScriptRoot -ChildPath '..'))

if ([System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $buildRoot = [System.IO.Path]::GetFullPath($BuildDirectory)
} else {
    $buildRoot = [System.IO.Path]::GetFullPath(
        (Join-Path -Path $repositoryRoot -ChildPath $BuildDirectory))
}

if (-not (Test-Path -LiteralPath $buildRoot -PathType Container)) {
    throw "Build directory does not exist: $buildRoot"
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

    $expectedPaths = $candidates -join [Environment]::NewLine
    throw "Required executable was not found. Checked:$([Environment]::NewLine)$expectedPaths"
}

$observerPath = Resolve-R1C2AExecutable -FileName 'panebind-observer.exe'
$harnessPath = Resolve-R1C2AExecutable -FileName 'panebind-explorer-harness.exe'

$evidenceDirectory = Join-Path -Path $repositoryRoot -ChildPath 'uat/r1c2a'
[void] (New-Item -ItemType Directory -Path $evidenceDirectory -Force)

$timestamp = [DateTime]::UtcNow.ToString(
    'yyyyMMddTHHmmssfffZ',
    [Globalization.CultureInfo]::InvariantCulture)
$observerStdout = Join-Path $evidenceDirectory "$timestamp-observer.stdout.jsonl"
$observerStderr = Join-Path $evidenceDirectory "$timestamp-observer.stderr.log"
$harnessStdout = Join-Path $evidenceDirectory "$timestamp-harness.stdout.jsonl"
$harnessStderr = Join-Path $evidenceDirectory "$timestamp-harness.stderr.log"

Write-Output "Evidence directory: $evidenceDirectory"
Write-Output "Observer stdout: $observerStdout"
Write-Output "Observer stderr: $observerStderr"
Write-Output "Harness stdout: $harnessStdout"
Write-Output "Harness stderr: $harnessStderr"

$observerProcess = $null
$harnessProcess = $null
$launchFailure = $null

try {
    $observerProcess = Start-Process `
        -FilePath $observerPath `
        -ArgumentList @('--observe-seconds', $ObserveSeconds.ToString(
            [Globalization.CultureInfo]::InvariantCulture)) `
        -WorkingDirectory $repositoryRoot `
        -WindowStyle Hidden `
        -RedirectStandardOutput $observerStdout `
        -RedirectStandardError $observerStderr `
        -PassThru

    # Windows PowerShell can lose ExitCode for a quickly exiting process with
    # redirected streams unless its native process handle is materialized
    # while the process is still running.
    [void] $observerProcess.Handle

    # Give the observer a bounded opportunity to install its hooks before the
    # Explorer harness starts provisioning its isolated test target.
    Start-Sleep -Milliseconds 750

    $harnessProcess = Start-Process `
        -FilePath $harnessPath `
        -ArgumentList @(
            '--self-test',
            '--hold-seconds',
            $HarnessHoldSeconds.ToString(
                [Globalization.CultureInfo]::InvariantCulture)
        ) `
        -WorkingDirectory $repositoryRoot `
        -WindowStyle Hidden `
        -RedirectStandardOutput $harnessStdout `
        -RedirectStandardError $harnessStderr `
        -PassThru
    [void] $harnessProcess.Handle
} catch {
    $launchFailure = $_
} finally {
    # Wait only for the two PaneBind processes started above. This evidence
    # runner never terminates either process or any Explorer process.
    if ($null -ne $harnessProcess) {
        $harnessProcess.WaitForExit()
        $harnessProcess.Refresh()
    }
    if ($null -ne $observerProcess) {
        $observerProcess.WaitForExit()
        $observerProcess.Refresh()
    }
}

if ($null -ne $launchFailure) {
    throw $launchFailure
}

$observerExitCode = $observerProcess.ExitCode
$harnessExitCode = $harnessProcess.ExitCode

if ($null -eq $observerExitCode -or $null -eq $harnessExitCode) {
    throw "An evidence process exit code was unavailable after WaitForExit/Refresh. observer=$observerExitCode, harness=$harnessExitCode"
}

Write-Output "Observer exit code: $observerExitCode"
Write-Output "Harness exit code: $harnessExitCode"

if ($observerExitCode -ne 0 -or $harnessExitCode -ne 0) {
    throw "R1-C2A evidence processes failed: observer=$observerExitCode, harness=$harnessExitCode"
}
