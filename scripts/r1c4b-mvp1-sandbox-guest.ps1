[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$inputDir = 'C:\PaneBindMVP1\Input'
$outputDir = 'C:\PaneBindMVP1\Output'
$summary = [ordered]@{
    schema = 'r1c4b-mvp1-sandbox-guest/v1'
    status = 'NOT_READY'
    run_id = $null
    account_class = $null
    session_id = $null
    user_interactive = $null
    preflight = @()
    scenarios = @()
    error = $null
}
$utf8 = New-Object Text.UTF8Encoding($false)
$summaryPath = $null

function Save-Summary {
    if ($script:summaryPath) {
        [IO.File]::WriteAllText($script:summaryPath, ($script:summary | ConvertTo-Json -Depth 6), $script:utf8)
    }
}

try {
    # The output mapping is per-run and writable. Record a startup fact before
    # any identity or input-marker check, so early fail-closed exits still have
    # a reason on the host. If even this write fails, no execution proceeds.
    if (-not (Test-Path -LiteralPath $outputDir -PathType Container)) {
        throw "Required output mapping missing: $outputDir"
    }
    $summaryPath = Join-Path $outputDir 'guest-summary.json'
    Save-Summary
    $identityName = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    $summary.account_class = if ($identityName -match '(^|\\)WDAGUtilityAccount$') { 'WDAGUtilityAccount' } else { 'OTHER' }
    $summary.session_id = (Get-Process -Id $PID).SessionId
    $summary.user_interactive = [Environment]::UserInteractive
    Save-Summary
    if ($summary.account_class -ne 'WDAGUtilityAccount' -or $summary.session_id -le 0 -or
        -not $summary.user_interactive) {
        throw 'Guest must be the logged-in WDAGUtilityAccount in an interactive nonzero session.'
    }
    foreach ($path in @($inputDir, $outputDir)) {
        if (-not (Test-Path -LiteralPath $path -PathType Container)) { throw "Required mapping missing: $path" }
    }
    $inputMarker = Join-Path $inputDir 'run-id.txt'
    $outputMarker = Join-Path $outputDir 'run-id.txt'
    $inputBytes = [IO.File]::ReadAllBytes($inputMarker)
    $outputBytes = [IO.File]::ReadAllBytes($outputMarker)
    if ($inputBytes.Length -ne 32 -or $outputBytes.Length -ne 32) { throw 'Run marker length is invalid.' }
    $runId = [Text.Encoding]::ASCII.GetString($inputBytes)
    if ($runId -cnotmatch '^[0-9a-f]{32}$' -or
        [Text.Encoding]::ASCII.GetString($outputBytes) -cne $runId) {
        throw 'Input and output mapping run markers do not agree.'
    }
    $summary.run_id = $runId
    Save-Summary
    $manifest = Get-Content -LiteralPath (Join-Path $inputDir 'manifest.json') -Raw | ConvertFrom-Json
    if ($manifest.run_id -cne $runId -or $manifest.configuration -cne 'Release-MT-x64') {
        throw 'Guest manifest does not match the package.'
    }
    $probe = Join-Path $inputDir 'panebind-owned-overlay-probe.exe'
    $preflight = Join-Path $inputDir 'panebind-test-input-environment.exe'
    if ((Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash -cne $manifest.probe_sha256 -or
        (Get-FileHash -LiteralPath $preflight -Algorithm SHA256).Hash -cne $manifest.preflight_sha256 -or
        (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash -cne $manifest.guest_script_sha256) {
        throw 'Guest input SHA256 differs from the read-only package manifest.'
    }
    $summary.status = 'PREFLIGHT'
    Save-Summary

    # Each experiment is a separate owned process. A failed or ambiguous run
    # prevents further synthetic input in this same disposable guest.
    foreach ($scenario in @('normal', 'early-up', 'setup-fail', 'stop', 'writer-stall')) {
        $preflightLog = Join-Path $outputDir "$runId-preflight-$scenario.jsonl"
        & $preflight --check-input-state --evidence-log $preflightLog *> (Join-Path $outputDir "$runId-preflight-$scenario.console.txt")
        $preflightExit = $LASTEXITCODE
        $preflightResult = $null
        if (Test-Path -LiteralPath $preflightLog -PathType Leaf) {
            $preflightResult = Get-Content -LiteralPath $preflightLog -Raw | ConvertFrom-Json
        }
        $summary.preflight += [ordered]@{
            scenario = $scenario
            exit_code = $preflightExit
            readiness = if ($preflightResult) { $preflightResult.result } else { 'NO_EVIDENCE' }
            evidence = $preflightLog
        }
        Save-Summary
        if ($preflightExit -ne 0 -or -not $preflightResult -or $preflightResult.result -ne 'READY') {
            throw "Input environment was not READY before $scenario; no further input was sent."
        }

        $evidence = Join-Path $outputDir "$runId-$scenario.jsonl"
        $consoleLog = Join-Path $outputDir "$runId-$scenario.console.txt"
        & $probe --run-owned-overlay-probe --scenario $scenario --evidence-log $evidence --sandbox-run-id $runId *> $consoleLog
        $probeExit = $LASTEXITCODE
        $hasEvidence = (Test-Path -LiteralPath $evidence -PathType Leaf) -and
            ((Get-Item -LiteralPath $evidence).Length -gt 0)
        $summary.scenarios += [ordered]@{
            scenario = $scenario
            exit_code = $probeExit
            evidence = $evidence
            nonempty_evidence = $hasEvidence
        }
        Save-Summary
        if ($probeExit -ne 0 -or -not $hasEvidence) {
            throw "Owned scenario $scenario failed or produced no evidence; remaining scenarios were not run."
        }
    }
    $summary.status = 'COMPLETED_PROBE_EXITS_ZERO'
    Save-Summary
    exit 0
} catch {
    $summary.status = 'STOPPED'
    $summary.error = $_.Exception.Message
    try { Save-Summary } catch { }
    exit 2
}
