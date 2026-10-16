[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$inputDir = 'C:\PaneBindMVP1\Input'
$outputDir = 'C:\PaneBindMVP1\Output'
$summary = [ordered]@{
    schema = 'r1c4b-mvp1-shield-sandbox-guest/v4'
    status = 'NOT_READY'
    run_id = $null
    source_commit = $null
    explorer_guest_mode = $null
    account_class = $null
    session_id = $null
    user_interactive = $null
    preflight = @()
    owned_scenarios = @()
    owned_prerequisite = $null
    contrast = $null
    explorer_stages = @()
    explorer_started = $false
    explorer_identity_precondition = $null
    explorer_not_run_reason = $null
    explorer_command = $null
    explorer_debug_command = $null
    error = $null
}
$utf8 = New-Object Text.UTF8Encoding($false)
$summaryPath = $null

function Save-Summary {
    if ($script:summaryPath) {
        [IO.File]::WriteAllText($script:summaryPath, ($script:summary | ConvertTo-Json -Depth 9), $script:utf8)
    }
}
function Get-ExplorerConfigurations([string]$Mode) {
    if ($Mode -ceq 'AutomatedValidation') { return @('debug', 'release') }
    if ($Mode -ceq 'BindOnlyIndependentReview') { return @('release') }
    if ($Mode -ceq 'CommandStopValidation' -or
        $Mode -ceq 'CommandDisconnectValidation') { return @('debug') }
    throw 'Exact guest Explorer mode is unknown.'
}
function New-GuestDriverStartInfo([string]$Driver,[string]$Configuration,
                                 [string]$RunId,[string]$Mode) {
    $start=[Diagnostics.ProcessStartInfo]::new()
    $start.FileName=Join-Path $PSHOME 'powershell.exe'
    $start.Arguments="-NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$Driver`" -Configuration $Configuration -RunId $RunId -Mode $Mode"
    $start.UseShellExecute=$false
    $start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true
    $start.RedirectStandardError=$true
    return $start
}
function Test-CommandChannelCleanup($Rows,[string]$Mode) {
    if($Mode -cnotin @('CommandStopValidation','CommandDisconnectValidation')) { return $false }
    $shutdown=@($Rows | Where-Object { $_.type -ceq 'shutdown' })
    $cleanup=@($Rows | Where-Object {
        $_.type -ceq 'gesture_event' -and $_.event -ceq 'resource' -and
        $_.reason -ceq 'stop_cleanup'
    })
    $status=@($Rows | Where-Object { $_.type -ceq 'status' })
    if($shutdown.Count -ne 1 -or $cleanup.Count -ne 1 -or $status.Count -lt 2 -or
        [long]$shutdown[0].sequence -le [long]$cleanup[0].sequence -or
        @($Rows | Where-Object {
            $_.type -ceq 'gesture_event' -and $_.event -in @('down','start','writer')
        }).Count -ne 0) { return $false }
    foreach($field in @('resources_stopped','gesture_events_recorded')) {
        if($shutdown[0].$field -isnot [bool] -or $shutdown[0].$field -ne $true) { return $false }
    }
    if($Mode -ceq 'CommandDisconnectValidation') {
        if($shutdown[0].result -cne 'BLOCKED' -or $shutdown[0].reason -cnotin
            @('guest_command_disconnected','guest_command_driver_exited')) { return $false }
    }elseif($shutdown[0].result -cne 'STOPPED' -or
        $shutdown[0].source -cne 'guest_pipe_command') { return $false }
    foreach($field in @('succeeded','overlay_destroyed','hotkey_unregistered',
        'receiver_destroyed','raw_registration_removed','winevent_unhooked',
        'classes_unregistered','association_detached')) {
        if($cleanup[0].$field -isnot [bool] -or $cleanup[0].$field -ne $true) { return $false }
    }
    return $true
}
function Test-BindOnlyProductCleanup($Rows) {
    $shutdown = @($Rows | Where-Object { $_.type -ceq 'shutdown' })
    $cleanup = @($Rows | Where-Object {
        $_.type -ceq 'gesture_event' -and $_.event -ceq 'resource' -and
        $_.reason -ceq 'stop_cleanup'
    })
    if ($shutdown.Count -ne 1 -or $cleanup.Count -ne 1 -or
        $shutdown[0].result -cne 'STOPPED' -or
        [long]$cleanup[0].sequence -le 0 -or
        [long]$shutdown[0].sequence -le [long]$cleanup[0].sequence) { return $false }
    foreach ($field in @('resources_stopped', 'gesture_events_recorded')) {
        if ($shutdown[0].$field -isnot [bool] -or $shutdown[0].$field -ne $true) {
            return $false
        }
    }
    foreach ($field in @('succeeded', 'overlay_destroyed', 'hotkey_unregistered',
        'receiver_destroyed', 'raw_registration_removed', 'winevent_unhooked',
        'classes_unregistered', 'association_detached')) {
        if ($cleanup[0].$field -isnot [bool] -or $cleanup[0].$field -ne $true) {
            return $false
        }
    }
    return $true
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
    if ($manifest.schema -cne 'r1c4b-mvp1-shield-sandbox-package/v3' -or
        $manifest.run_id -cne $runId -or
        $manifest.source_commit -cnotmatch '^[0-9a-f]{40}$' -or
        $manifest.configuration -cne 'Release-and-Debug-MT-x64' -or
        $manifest.explorer_test_exception -cne 'exact_sandbox_same_sid_session_high' -or
        @('AutomatedValidation','BindOnlyIndependentReview',
            'CommandStopValidation','CommandDisconnectValidation','ConsumedDownFeasibility') -cnotcontains
            [string]$manifest.explorer_guest_mode -or
        $manifest.independent_review_configuration -cne 'release') {
        throw 'Guest manifest does not match the exact package.'
    }
    $expectedChannel=if($manifest.explorer_guest_mode -ceq 'BindOnlyIndependentReview') {
        'console'
    } else { 'guest_named_pipe' }
    if($manifest.explorer_command_channel -cne $expectedChannel) {
        throw 'Guest manifest command channel is not exact.'
    }
    $summary.source_commit = $manifest.source_commit
    $summary.explorer_guest_mode = $manifest.explorer_guest_mode
    $summary.explorer_command_channel = $manifest.explorer_command_channel
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
    $summary.explorer_command = "$explorer --sandbox-run-id $runId --evidence-log $outputDir\$runId-explorer-mvp1.jsonl --sandbox-high-integrity-test-exception"
    $summary.explorer_debug_command = "$debugExplorer --sandbox-run-id $runId --evidence-log $outputDir\$runId-explorer-mvp1-debug.jsonl --sandbox-high-integrity-test-exception"
    $summary.status = 'PREFLIGHT'
    Save-Summary

    # The queue-association hypothesis must close a real NormalUp first, then
    # one follow-up gesture on the SAME owned process/resources. No Explorer
    # input is sent unless these and the targeted cleanup risks all pass.
    $cases = @(
        @{ configuration = 'debug'; scenario = 'normal-repeat'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'association-stop-before-call'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'capture-stop'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'writer-stall'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'source-pause'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'capture-early-up'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'capture-fail'; executable = $debugOwned },
        @{ configuration = 'debug'; scenario = 'capture-lost'; executable = $debugOwned },
        @{ configuration = 'release'; scenario = 'normal'; executable = $owned }
    )
    $consumedDownOnly = $manifest.explorer_guest_mode -ceq 'ConsumedDownFeasibility'
    if ($consumedDownOnly) {
        # This is a new platform hypothesis, not a rerun of the old native-END
        # route and not V1 Move acceptance. Never reuse the historical nine.
        $cases = @(
            @{ configuration = 'debug'; scenario = 'consumed-down-normal-repeat'; executable = $debugOwned },
            @{ configuration = 'debug'; scenario = 'consumed-down-early-up'; executable = $debugOwned },
            @{ configuration = 'debug'; scenario = 'consumed-down-stop'; executable = $debugOwned },
            @{ configuration = 'debug'; scenario = 'consumed-down-source-pause'; executable = $debugOwned }
        )
        $summary.owned_prerequisite = [ordered]@{ result='NEW_CONSUMED_DOWN_ROUTE_NOT_PREVIOUS_NATIVE_END_EVIDENCE' }
    } elseif ($manifest.owned_prerequisite.result -ceq 'REUSED_PRIOR_EVIDENCE') {
        $prior = $manifest.owned_prerequisite
        if ($prior.run_id -cne '050736d561fe4b3c941e9f194b3f51aa' -or
            $prior.source_commit -cne '3ef009f5e72c5eebdbb64b3c3e0cd6c52c1b2d52' -or
            $prior.summary_sha256 -cne '23201480D2A815C58EF7105EE34A366385F759C026B02C6721F013A8AD7C5287' -or
            $prior.shared_source_unchanged -ne $true -or
            $prior.current_package_owned_execution -cne 'NOT_RUN' -or
            @($prior.raw_evidence).Count -ne 9) {
            throw 'Owned prior prerequisite record is incomplete; cannot skip current owned input.'
        }
        foreach ($case in $cases) {
            $record = @($prior.raw_evidence | Where-Object {
                $_.configuration -ceq $case.configuration -and $_.scenario -ceq $case.scenario
            })
            if ($record.Count -ne 1 -or $record[0].sha256 -cnotmatch '^[A-F0-9]{64}$' -or
                $record[0].rows -le 1 -or $record[0].file -cne
                    ($prior.run_id + '-' + $case.configuration + '-' + $case.scenario + '.jsonl')) {
                throw 'Owned prior prerequisite does not bind every exact reviewed scenario.'
            }
        }
        $summary.owned_prerequisite = $prior
        $summary.owned_scenarios = @($prior.raw_evidence | ForEach-Object {
            [ordered]@{ configuration = $_.configuration; scenario = $_.scenario;
                result = 'REUSED_PRIOR_EVIDENCE'; prior_run_id = $prior.run_id;
                prior_source_commit = $prior.source_commit; evidence_sha256 = $_.sha256;
                current_package_execution = 'NOT_RUN' }
        })
        $cases = @()
        Save-Summary
    } elseif ($manifest.owned_prerequisite.result -ceq 'RUN_CURRENT_PACKAGE') {
        $summary.owned_prerequisite = $manifest.owned_prerequisite
    } else { throw 'Owned prerequisite mode is unknown.' }
    foreach ($testCase in $cases) {
        $configuration = $testCase.configuration
        $scenario = $testCase.scenario
        $ownedExecutable = $testCase.executable
        # The unchanged exact preflight executable accepts this original
        # suffix. The summary below records the ACTUAL new probe scenario.
        $preflightScenario = if ($consumedDownOnly) {
            switch -CaseSensitive ($scenario) {
                'consumed-down-normal-repeat' { 'normal' }
                'consumed-down-early-up' { 'early-up' }
                'consumed-down-stop' { 'stop' }
                'consumed-down-source-pause' { 'source-pause' }
                default { throw 'Unknown consumed-DOWN preflight mapping.' }
            }
        } else { $scenario }
        $preflightLog = Join-Path $outputDir "$runId-preflight-$configuration-$preflightScenario.jsonl"
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
    if ($consumedDownOnly) {
        $summary.status = 'CONSUMED_DOWN_FEASIBILITY_PASSED'
        $summary.explorer_not_run_reason = 'new_owned_input_resource_hypothesis_only_no_V1_product_or_Explorer_PASS'
        Save-Summary
        exit 0
    }
    # Always collect a CURRENT guest input/identity observation, including when
    # owned was reused. The historical High result is not launch authority.
    # This bootstrap is guest test-only, not an Explorer/product operation.
    # Use an EXISTING strict probe contract, not a new filename that the
    # native guest guard rejects. The suffix names the allowed evidence path;
    # summary scenario below describes the ACTUAL current-input check, not an
    # owned Normal run. If owned ran now, use its otherwise-unused allowed
    # legacy-control preflight suffix so CREATE_NEW never overwrites evidence.
    $preflightSuffix = if ($manifest.owned_prerequisite.result -ceq 'REUSED_PRIOR_EVIDENCE') {
        '-preflight-release-normal.jsonl'
    } else { '-preflight-debug-legacy-control.jsonl' }
    $preflightLog = Join-Path $outputDir ($runId + $preflightSuffix)
    & $preflight --check-guest-input-state --sandbox-run-id $runId --evidence-log $preflightLog *> (Join-Path $outputDir "$runId-preflight-explorer.console.txt")
    $preflightExit = $LASTEXITCODE
    $preflightResult = if (Test-Path -LiteralPath $preflightLog -PathType Leaf) {
        Get-Content -LiteralPath $preflightLog -Raw | ConvertFrom-Json -ErrorAction Stop
    } else { $null }
    $summary.preflight += [ordered]@{ configuration = 'explorer'; scenario = 'current_input_identity';
        exit_code = $preflightExit; evidence = $preflightLog;
        readiness = if ($preflightResult) { $preflightResult.result } else { 'NO_EVIDENCE' } }
    Save-Summary
    if ($preflightExit -ne 0 -or -not $preflightResult -or $preflightResult.result -cne 'READY' -or
        $preflightResult.schema -cne 'r1c4b-owned-foreground-input-environment/v1' -or
        $preflightResult.run_id -cne $runId -or $preflightResult.read_only -ne $false -or
        $preflightResult.owned_foreground_bootstrap.foreground_owned -ne $true -or
        $preflightResult.owned_foreground_bootstrap.destroyed -ne $true -or
        $preflightResult.owned_foreground_bootstrap.unregistered -ne $true) {
        throw 'Current guest input preflight was not READY; no Explorer input was sent.'
    }
    # This explicit test policy requires actual High, not Medium/UNKNOWN/System.
    # The product independently verifies its exact guest RunId capability and
    # each new frame's SID/session/native token at binding and every operation.
    $integrityBeforeKnown = $preflightResult.before.caller_integrity_known -eq $true -and
        ($preflightResult.before.caller_integrity -is [int] -or
         $preflightResult.before.caller_integrity -is [long])
    $integrityAfterKnown = $preflightResult.after.caller_integrity_known -eq $true -and
        ($preflightResult.after.caller_integrity -is [int] -or
         $preflightResult.after.caller_integrity -is [long])
    $integrityBefore = if ($integrityBeforeKnown) {
        $preflightResult.before.caller_integrity
    } else { $null }
    $integrityAfter = if ($integrityAfterKnown) {
        $preflightResult.after.caller_integrity
    } else { $null }
    $summary.explorer_identity_precondition = [ordered]@{
        evidence = $preflightLog
        caller_integrity_before_known = $integrityBeforeKnown
        caller_integrity_after_known = $integrityAfterKnown
        caller_integrity_before = $integrityBefore
        caller_integrity_after = $integrityAfter
        required_high_rid = 12288
        requested_test_exception = $manifest.explorer_test_exception
        current_run_id = $runId
        ordinary_product_medium_gate_changed = $false
        actual_explorer_controller_token_checked = $false
        product_authority_granted = $false
    }
    if (-not $integrityBeforeKnown -or -not $integrityAfterKnown -or
        $integrityBefore -ne 12288 -or $integrityAfter -ne 12288) {
        $summary.explorer_not_run_reason = if (-not $integrityBeforeKnown -or
                -not $integrityAfterKnown) {
            'Explorer_controller_integrity_UNKNOWN'
        } else { 'Explorer_exact_high_guest_test_identity_required' }
        $summary.explorer_stages += [ordered]@{
            configurations = @('debug', 'release')
            result = 'NOT_RUN'
            reason = $summary.explorer_not_run_reason
        }
        Save-Summary
        throw "Owned prerequisite recorded; Explorer NOT_RUN: $($summary.explorer_not_run_reason)."
    }
    Save-Summary
    $driver = Join-Path $inputDir 'explorer-driver.ps1'
    # Reuse the already-tested product reader for a LIVE bind-only product.
    # Import only this read function, never the driver's startup/native/input.
    $readerTokens = $null; $readerErrors = $null
    $readerAst = [Management.Automation.Language.Parser]::ParseFile($driver,
        [ref]$readerTokens,[ref]$readerErrors)
    $readerFunction = $readerAst.Find({param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -ceq 'Product-Rows'
    },$true)
    if ($readerErrors.Count -ne 0 -or -not $readerFunction) {
        throw 'Verified driver live product reader is unavailable.'
    }
    . ([scriptblock]::Create($readerFunction.Extent.Text))
    foreach ($configuration in @(Get-ExplorerConfigurations $manifest.explorer_guest_mode)) {
        if ($manifest.explorer_command_channel -ceq 'guest_named_pipe') {
            $start=New-GuestDriverStartInfo $driver $configuration $runId $manifest.explorer_guest_mode
            $process=[Diagnostics.Process]::new()
            $process.StartInfo=$start
            if(-not $process.Start()) { throw 'No-console guest driver did not start.' }
            # Drain redirected FILE pipes, never console input, concurrently
            # with the one bounded child wait so diagnostic output cannot fill
            # a pipe and prevent driver/product cleanup.
            $stdout=$process.StandardOutput.ReadToEndAsync()
            $stderr=$process.StandardError.ReadToEndAsync()
            $summary.explorer_stages += [ordered]@{
                configuration=$configuration; phase='driver_creation';
                driver_pid=$process.Id; create_no_window=$start.CreateNoWindow;
                use_shell_execute=$start.UseShellExecute; arguments=$start.Arguments;
                requested_creation_semantics='CREATE_NO_WINDOW_NOT_SW_HIDE';
                actual_console_observation='recorded_by_driver_and_product';
                scope='new_driver_and_product_only_not_all_guest_consoles'
            }
            Save-Summary
            if(-not $process.WaitForExit(300000)) {
                throw 'Guest driver bounded wait expired; no functional or cleanup PASS.'
            }
            $driverExit=$process.ExitCode
            $consolePath=Join-Path $outputDir "$runId-explorer-driver-$configuration.console.txt"
            $consoleStream=[IO.FileStream]::new($consolePath,[IO.FileMode]::CreateNew,
                [IO.FileAccess]::Write,[IO.FileShare]::Read)
            $consoleWriter=[IO.StreamWriter]::new($consoleStream,$utf8)
            try { $consoleWriter.Write($stdout.Result);$consoleWriter.Write($stderr.Result) }
            finally { $consoleWriter.Dispose();$process.Dispose() }
        } else {
            & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $driver -Configuration $configuration -RunId $runId -Mode $manifest.explorer_guest_mode *> (Join-Path $outputDir "$runId-explorer-driver-$configuration.console.txt")
            $driverExit=$LASTEXITCODE
        }
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
        if($manifest.explorer_guest_mode -ceq 'CommandDisconnectValidation') {
            # Driver exit is only a stimulus. Observe the exact product's
            # independent cleanup afterward; never promote child exit 0.
            $requested=@($driverRows | Where-Object {
                $_.type -ceq 'shutdown' -and $_.result -ceq 'COMMAND_DRIVER_EXIT_REQUESTED'
            })
            if($driverExit -ne 0 -or $requested.Count -ne 1 -or
                [int]$requested[0].product_pid -le 0) {
                throw 'Command disconnect driver did not record its exact exit stimulus.'
            }
            $remaining=Get-Process -Id ([int]$requested[0].product_pid) -ErrorAction SilentlyContinue
            if($remaining) {
                if($remaining.SessionId -ne $summary.session_id -or
                    -not [string]::Equals($remaining.Path,$(if($configuration -ceq 'debug'){
                        $debugExplorer
                    }else{$explorer}),[StringComparison]::OrdinalIgnoreCase)) {
                    throw 'Command disconnect product identity changed.'
                }
                Wait-Process -InputObject $remaining -Timeout 15 -ErrorAction Stop
            }
        }
        if (Test-Path -LiteralPath $productEvidence -PathType Leaf) {
            $productLog = $productEvidence
            $productRows = @(Product-Rows)
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
        if ($manifest.explorer_guest_mode -ceq 'BindOnlyIndependentReview') {
            $review = @($driverRows | Where-Object { $_.type -ceq 'review_bound' })
            if ($driverExit -ne 0 -or $driverStartup.Count -ne 1 -or
                $driverStartup[0].implementation_sha -cne $manifest.source_commit -or
                $startup.Count -ne 1 -or $startup[0].implementation_sha -cne $manifest.source_commit -or
                $startup[0].run_id -cne $runId -or $startup[0].automated_guest_driver -ne $true -or
                $bindings.Count -ne 3 -or $productConsent.Count -ne 1 -or
                $productConsent[0].input_source -cne 'automated_guest_driver' -or
                $review.Count -ne 1 -or $review[0].configuration -cne 'release' -or
                $review[0].automatic_gestures_sent -ne $false -or
                $review[0].functional_acceptance -cne 'NOT_CLAIMED' -or
                $review[0].product_pid -le 0 -or $productShutdown.Count -ne 0) {
                throw 'Bind-only preparation did not establish the exact running three-window product.'
            }
            $reviewPid = [int]$review[0].product_pid
            $process = Get-Process -Id $reviewPid -ErrorAction Stop
            if ($process.SessionId -ne $summary.session_id -or
                -not [string]::Equals($process.Path,$explorer,[StringComparison]::OrdinalIgnoreCase)) {
                throw 'Bind-only running product identity changed before review.'
            }
            $summary.explorer_stages += [ordered]@{
                configuration = 'release'; result = 'BOUND_FOR_INDEPENDENT_REVIEW';
                product_pid = $reviewPid; product_evidence = $productEvidence;
                driver_evidence = $driverEvidence; automatic_gestures_sent = $false;
                functional_acceptance = 'NOT_CLAIMED'; human_uat = 'NOT_RUN';
                status_command = 'S'; stop_command = 'Q'; gesture_stop_hotkey = 'Ctrl+Shift+F11'
            }
            $summary.status = 'BOUND_FOR_INDEPENDENT_REVIEW'
            Save-Summary
            # One bounded process-exit wait, not input polling or a resident
            # driver. It never closes host/guest applications on timeout.
            Wait-Process -InputObject $process -Timeout 1800 -ErrorAction Stop
            $finishedRows = @([IO.File]::ReadAllLines($productEvidence) | ForEach-Object {
                $_ | ConvertFrom-Json -ErrorAction Stop
            })
            $finishedShutdown = @($finishedRows | Where-Object { $_.type -ceq 'shutdown' })
            $reviewCleanupConfirmed = Test-BindOnlyProductCleanup $finishedRows
            $summary.explorer_stages += [ordered]@{
                configuration = 'release'; result = 'BIND_ONLY_PRODUCT_EXITED_REVIEW_REQUIRED';
                actual_product_shutdown = if ($finishedShutdown.Count -eq 1) { $finishedShutdown[0].result } else { $null };
                actual_resources_stopped = if ($finishedShutdown.Count -eq 1) { $finishedShutdown[0].resources_stopped } else { $null };
                actual_cleanup_confirmed = $reviewCleanupConfirmed;
                automatic_functional_validation = 'NOT_RUN'; human_uat = 'NOT_RUN'
            }
            Save-Summary
            if (-not $reviewCleanupConfirmed) {
                throw 'Bind-only product exited without complete actual shutdown/resource cleanup evidence.'
            }
            $summary.status = 'STOPPED'
            Save-Summary
            exit 0
        }
        if($manifest.explorer_guest_mode -in @('CommandStopValidation','CommandDisconnectValidation')) {
            $disconnect=$manifest.explorer_guest_mode -ceq 'CommandDisconnectValidation'
            $expectedResult=if($disconnect){'COMMAND_DRIVER_EXIT_REQUESTED'}else{'COMMAND_STOP_RECORDED'}
            $validTerminal=Test-CommandChannelCleanup $productRows $manifest.explorer_guest_mode
            if($driverExit -ne 0 -or $driverStartup.Count -ne 1 -or
                $driverStartup[0].implementation_sha -cne $manifest.source_commit -or
                $driverShutdown.Count -ne 1 -or $driverShutdown[0].result -cne $expectedResult -or
                $startup.Count -ne 1 -or $startup[0].implementation_sha -cne $manifest.source_commit -or
                $startup[0].run_id -cne $runId -or $startup[0].command_channel -cne 'guest_named_pipe' -or
                $startup[0].process_has_console -ne $false -or $bindings.Count -ne 3 -or
                $productConsent.Count -ne 1 -or $productConsent[0].input_source -cne 'automated_guest_driver' -or
                -not $validTerminal) {
                throw 'Actual no-gesture command channel/cleanup evidence is incomplete.'
            }
            $summary.explorer_stages += [ordered]@{configuration=$configuration;
                result='COMMAND_CHANNEL_CLEANUP_RECORDED'; mode=$manifest.explorer_guest_mode;
                automatic_gestures_sent=$false; product_terminal=$productShutdown[0].result;
                product_cleanup_confirmed=$true; human_uat='NOT_RUN'}
            $summary.status='COMMAND_CHANNEL_CLEANUP_RECORDED'
            Save-Summary
            exit 0
        }
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
