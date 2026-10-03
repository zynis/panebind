[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$inputDir = 'C:\PaneBindMVP1\Input'
$outputDir = 'C:\PaneBindMVP1\Output'
$summary = [ordered]@{
    schema = 'r1c4b-mvp1-shield-sandbox-guest/v3'
    status = 'NOT_READY'
    run_id = $null
    source_commit = $null
    account_class = $null
    session_id = $null
    user_interactive = $null
    preflight = @()
    owned_scenarios = @()
    contrast = $null
    explorer_stages = @()
    explorer_started = $false
    explorer_command = $null
    explorer_debug_command = $null
    error = $null
}
$utf8 = New-Object Text.UTF8Encoding($false)
$summaryPath = $null

function Save-Summary {
    if ($script:summaryPath) {
        [IO.File]::WriteAllText($script:summaryPath, ($script:summary | ConvertTo-Json -Depth 7), $script:utf8)
    }
}

try {
    # Host refusal precedes every filesystem write. This runner is not a
    # general host-side automation entrypoint.
    $identityName = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    $isGuestAccount = $identityName -ieq 'WDAGUtilityAccount' -or
        $identityName.EndsWith('\WDAGUtilityAccount', [StringComparison]::OrdinalIgnoreCase)
    $summary.account_class = if ($isGuestAccount) { 'WDAGUtilityAccount' } else { 'OTHER' }
    $summary.session_id = (Get-Process -Id $PID).SessionId
    $summary.user_interactive = [Environment]::UserInteractive
    if ($summary.account_class -ne 'WDAGUtilityAccount' -or $summary.session_id -le 0 -or
        -not $summary.user_interactive) {
        throw 'Guest must be the logged-in WDAGUtilityAccount in an interactive nonzero session.'
    }
    if (-not (Test-Path -LiteralPath $outputDir -PathType Container)) {
        throw "Required output mapping missing: $outputDir"
    }
    $newSummaryPath = Join-Path $outputDir 'guest-owned-summary.json'
    if (Test-Path -LiteralPath $newSummaryPath) {
        throw 'Owned summary already exists; this RunId cannot overwrite historical evidence.'
    }
    $summaryPath = $newSummaryPath
    Save-Summary
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
    if ($manifest.schema -cne 'r1c4b-mvp1-shield-sandbox-package/v2' -or
        $manifest.run_id -cne $runId -or
        $manifest.source_commit -cnotmatch '^[0-9a-f]{40}$' -or
        $manifest.configuration -cne 'Release-and-Debug-MT-x64') {
        throw 'Guest manifest does not match the exact package.'
    }
    $summary.source_commit = $manifest.source_commit
    foreach ($item in @(
        @{ Path = $inputMarker; Hash = $manifest.input_marker_sha256 },
        @{ Path = $outputMarker; Hash = $manifest.output_marker_sha256 },
        @{ Path = $PSCommandPath; Hash = $manifest.guest_script_sha256 },
        @{ Path = (Join-Path $inputDir 'explorer-driver.ps1'); Hash = $manifest.explorer_driver_sha256 }
    )) {
        if ((Get-FileHash -LiteralPath $item.Path -Algorithm SHA256).Hash -cne $item.Hash) {
            throw "Guest package hash differs: $($item.Path)"
        }
    }
    $owned = Join-Path $inputDir 'panebind-owned-shield-validation.exe'
    $debugOwned = Join-Path (Join-Path $inputDir 'Debug') 'panebind-owned-shield-validation.exe'
    $debugExplorer = Join-Path (Join-Path $inputDir 'Debug') 'panebind-explorer-mvp1.exe'
    $explorer = Join-Path $inputDir 'panebind-explorer-mvp1.exe'
    $preflight = Join-Path $inputDir 'panebind-test-input-environment.exe'
    $contrast = Join-Path $inputDir 'panebind-shield-topmost-contrast.exe'
    foreach ($name in @('panebind-owned-shield-validation.exe',
                        'panebind-explorer-mvp1.exe',
                        'panebind-test-input-environment.exe',
                        'panebind-shield-topmost-contrast.exe')) {
        $path = Join-Path $inputDir $name
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne
            $manifest.binaries_sha256.$name) {
            throw "Guest binary SHA256 differs: $name"
        }
    }
    if ((Get-FileHash -LiteralPath $debugOwned -Algorithm SHA256).Hash -cne
        $manifest.debug_owned_sha256) {
        throw 'Guest Debug owned SHA256 differs from the read-only manifest.'
    }
    if ((Get-FileHash -LiteralPath $debugExplorer -Algorithm SHA256).Hash -cne
        $manifest.debug_explorer_sha256) {
        throw 'Guest Debug Explorer SHA256 differs from the read-only manifest.'
    }
    foreach ($path in @($owned, $debugOwned, $explorer, $debugExplorer, $contrast)) {
        $identityText = (& $path --build-identity | Out-String)
        $identityExit = $LASTEXITCODE
        if ($identityExit -ne 0) { throw "Build identity failed: $path" }
        $identity = $identityText | ConvertFrom-Json -ErrorAction Stop
        if ($identity.implementation_sha -cne $manifest.source_commit) {
            throw "Build identity differs from manifest: $path"
        }
    }
    $summary.explorer_command = "$explorer --sandbox-run-id $runId --evidence-log $outputDir\$runId-explorer-mvp1.jsonl"
    $summary.explorer_debug_command = "$debugExplorer --sandbox-run-id $runId --evidence-log $outputDir\$runId-explorer-mvp1-debug.jsonl"
    $summary.status = 'PREFLIGHT'
    Save-Summary

    # Only owned automation runs at Sandbox logon. Explorer and the three-frame
    # candidate are a separate stage after the owned evidence is reviewed.
    $cases = @(
        @{ configuration = 'debug'; scenario = 'legacy-control'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'capture-stop'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'capture-early-up'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'capture-fail'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'capture-lost'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'normal'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'writer-stall'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'early-up'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'setup-fail'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'stop'; executable = $debugOwned },
        @{ configuration = 'release'; scenario = 'normal'; executable = $owned }
    )
    foreach ($testCase in $cases) {
        $configuration = $testCase.configuration
        $scenario = $testCase.scenario
        $ownedExecutable = $testCase.executable
        $preflightLog = Join-Path $outputDir "$runId-preflight-$configuration-$scenario.jsonl"
        & $preflight --check-guest-input-state --sandbox-run-id $runId --evidence-log $preflightLog *> (Join-Path $outputDir "$runId-preflight-$configuration-$scenario.console.txt")
        $preflightExit = $LASTEXITCODE
        $preflightResult = $null
        if (Test-Path -LiteralPath $preflightLog -PathType Leaf) {
            $preflightResult = Get-Content -LiteralPath $preflightLog -Raw | ConvertFrom-Json
        }
        $summary.preflight += [ordered]@{
            configuration = $configuration
            scenario = $scenario
            exit_code = $preflightExit
            mode = 'owned_foreground_bootstrap'
            readiness = if ($preflightResult) { $preflightResult.result } else { 'NO_EVIDENCE' }
            foreground_owned = if ($preflightResult) { $preflightResult.owned_foreground_bootstrap.foreground_owned } else { $null }
            evidence = $preflightLog
        }
        Save-Summary
        if ($preflightExit -ne 0 -or -not $preflightResult -or $preflightResult.result -ne 'READY' -or
            $preflightResult.schema -cne 'r1c4b-owned-foreground-input-environment/v1' -or
            $preflightResult.read_only -ne $false -or
            -not $preflightResult.owned_foreground_bootstrap.foreground_owned -or
            -not $preflightResult.owned_foreground_bootstrap.destroyed -or
            -not $preflightResult.owned_foreground_bootstrap.unregistered) {
            throw "Input environment was not READY before $configuration/$scenario; no further input was sent."
        }

        $evidence = Join-Path $outputDir "$runId-$configuration-$scenario.jsonl"
        $consoleLog = Join-Path $outputDir "$runId-$configuration-$scenario.console.txt"
        & $ownedExecutable --run-owned-shield-validation --scenario $scenario --evidence-log $evidence --sandbox-run-id $runId *> $consoleLog
        $ownedExit = $LASTEXITCODE
        $hasEvidence = (Test-Path -LiteralPath $evidence -PathType Leaf) -and
            ((Get-Item -LiteralPath $evidence).Length -gt 0)
        $summary.owned_scenarios += [ordered]@{
            configuration = $configuration
            scenario = $scenario
            exit_code = $ownedExit
            evidence = $evidence
            nonempty_evidence = $hasEvidence
        }
        Save-Summary
        $rows = @()
        if ($hasEvidence) {
            foreach ($line in [IO.File]::ReadAllLines($evidence)) {
                try { $rows += ($line | ConvertFrom-Json -ErrorAction Stop) }
                catch { throw "Malformed owned JSONL in $configuration/$scenario; remaining input stopped." }
            }
        }
        $ownedSequence = 0
        foreach ($row in $rows) {
            $ownedSequence++
            if ($row.schema -cne 'r1c4b-owned-product-shield/v1' -or
                $row.sequence -ne $ownedSequence) {
                throw "Owned JSONL identity/sequence invalid in $configuration/$scenario."
            }
        }
        $ownedStartup = @($rows | Where-Object { $_.type -ceq 'startup' })
        $shutdown = @($rows | Where-Object { $_.type -ceq 'shutdown' })
        if ($ownedExit -ne 0 -or -not $hasEvidence -or
            $ownedStartup.Count -ne 1 -or
            $ownedStartup[0].implementation_sha -cne $manifest.source_commit -or
            $ownedStartup[0].scenario -cne $scenario -or
            $shutdown.Count -ne 1 -or $shutdown[0].accepted -ne $true) {
            # Run the smallest self-owned comparison in this SAME guest only
            # for the exact observed shield Style readback failure. Contrast
            # is not a product PASS and cannot unblock Explorer input.
            $style = @($rows | Where-Object {
                $_.type -ceq 'resource_or_context' -and
                $_.setup_stage -eq 8 -and $_.readback_failure -eq 3
            })
            if ($configuration -ceq 'debug' -and $scenario -ceq 'normal' -and
                $style.Count -gt 0) {
                $contrastEvidence = Join-Path $outputDir "$runId-topmost-contrast.jsonl"
                & $contrast --run-topmost-contrast --sandbox-run-id $runId --evidence-log $contrastEvidence *> (Join-Path $outputDir "$runId-topmost-contrast.console.txt")
                $contrastExit = $LASTEXITCODE
                $summary.contrast = [ordered]@{
                    purpose = 'self_owned_same_guest_style_comparison_not_product_pass'
                    exit_code = $contrastExit
                    evidence = $contrastEvidence
                    nonempty_evidence = (Test-Path -LiteralPath $contrastEvidence -PathType Leaf) -and
                        ((Get-Item -LiteralPath $contrastEvidence).Length -gt 0)
                }
                Save-Summary
            }
            throw "Owned scenario $configuration/$scenario failed or produced no evidence; remaining scenarios were not run."
        }
    }
    $driver = Join-Path $inputDir 'explorer-driver.ps1'
    foreach ($configuration in @('debug', 'release')) {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $driver -Configuration $configuration -RunId $runId *> (Join-Path $outputDir "$runId-explorer-driver-$configuration.console.txt")
        $driverExit = $LASTEXITCODE
        $driverEvidence = Join-Path $outputDir "$runId-explorer-driver-$configuration.jsonl"
        $productEvidence = Join-Path $outputDir $(if ($configuration -ceq 'debug') {
            "$runId-explorer-mvp1-debug.jsonl"
        } else { "$runId-explorer-mvp1.jsonl" })
        $driverRows = @()
        if (Test-Path -LiteralPath $driverEvidence -PathType Leaf) {
            foreach ($line in [IO.File]::ReadAllLines($driverEvidence)) {
                if ([string]::IsNullOrWhiteSpace($line)) {
                    throw "Empty Explorer driver JSONL line in $configuration."
                }
                try { $driverRows += ($line | ConvertFrom-Json -ErrorAction Stop) }
                catch { throw "Malformed Explorer driver JSONL in $configuration." }
            }
        }
        $driverSequence = 0
        foreach ($row in $driverRows) {
            $driverSequence++
            if ($row.schema -cne 'r1c4b-mvp1-explorer-guest-driver/v1' -or
                $row.sequence -ne $driverSequence -or $row.run_id -cne $runId -or
                $row.configuration -cne $configuration) {
                throw "Explorer driver JSONL identity/sequence invalid in $configuration."
            }
        }
        $driverStartup = @($driverRows | Where-Object { $_.type -ceq 'startup' })
        $driverShutdown = @($driverRows | Where-Object { $_.type -ceq 'shutdown' })
        $productRows = @()
        if (Test-Path -LiteralPath $productEvidence -PathType Leaf) {
            foreach ($line in [IO.File]::ReadAllLines($productEvidence)) {
                if ([string]::IsNullOrWhiteSpace($line)) {
                    throw "Empty Explorer product JSONL line in $configuration."
                }
                try { $productRows += ($line | ConvertFrom-Json -ErrorAction Stop) }
                catch { throw "Malformed Explorer product JSONL in $configuration." }
            }
        }
        $sequence = 0
        foreach ($row in $productRows) {
            $sequence++
            if ($row.schema -cne 'mvp1/entry-v1' -or $row.sequence -ne $sequence) {
                throw "Explorer product JSONL schema/sequence invalid in $configuration."
            }
        }
        $startup = @($productRows | Where-Object { $_.type -ceq 'startup' })
        $bindings = @($productRows | Where-Object { $_.type -ceq 'binding' })
        $productConsent = @($productRows | Where-Object { $_.type -ceq 'product_consent' })
        $productShutdown = @($productRows | Where-Object { $_.type -ceq 'shutdown' })
        $productComplete = $startup.Count -eq 1 -and
            $startup[0].implementation_sha -ceq $manifest.source_commit -and
            $startup[0].run_id -ceq $runId -and
            $startup[0].automated_guest_driver -eq $true -and
            $bindings.Count -eq 3 -and
            @($bindings | ForEach-Object { $_.member } | Sort-Object -Unique).Count -eq 3 -and
            $productConsent.Count -eq 1 -and
            $productConsent[0].input_source -ceq 'automated_guest_driver' -and
            $productShutdown.Count -eq 1 -and
            $productShutdown[0].result -ceq 'STOPPED' -and
            $productShutdown[0].resources_stopped -eq $true -and
            $productShutdown[0].gesture_events_recorded -eq $true
        $summary.explorer_stages += [ordered]@{
            configuration = $configuration
            driver_exit = $driverExit
            driver_evidence = $driverEvidence
            product_evidence = $productEvidence
            driver_completed = $driverShutdown.Count -eq 1 -and
                $driverShutdown[0].result -ceq 'AUTOMATED_BASIC_FLOW_RECORDED'
            product_complete = $productComplete
        }
        Save-Summary
        if ($driverExit -ne 0 -or $driverShutdown.Count -ne 1 -or
            $driverShutdown[0].result -cne 'AUTOMATED_BASIC_FLOW_RECORDED' -or
            $driverStartup.Count -ne 1 -or
            $driverStartup[0].implementation_sha -cne $manifest.source_commit -or
            -not $productComplete) {
            throw "Explorer $configuration automated flow did not complete; remaining input stopped."
        }
    }
    $summary.status = 'AUTOMATED_PROCESSES_EXITED_ZERO_REVIEW_REQUIRED'
    Save-Summary
    exit 0
} catch {
    $summary.status = 'STOPPED'
    $summary.error = $_.Exception.Message
    try { Save-Summary } catch { }
    exit 2
}
