[CmdletBinding()]
param([Parameter(Mandatory)][string]$PlanPath,[Parameter(Mandatory)][string]$InventoryPath,[Parameter(Mandatory)][string]$StatisticsPath,[Parameter(Mandatory)][string]$ArtifactPath)
# Entirely read-only final reconstruction, except its new summary artifact.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-native-abort-validation.ps1')
. (Join-Path $PSScriptRoot 'r1c4b-owned-stability-model.ps1')
. (Join-Path $PSScriptRoot 'r1c4b-owned-stability-statistics.ps1')
foreach($definition in @(Get-OwnedStabilityDefinitions)){. $definition}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'));$root=Join-Path $repo 'uat/r1c4b-fixg'
$summary=[ordered]@{Schema='r1c4b-owned-stability-summary/v1';BatchId='NOT_AVAILABLE';ImplementationSHA='NOT_AVAILABLE';BatchResult='INVALID_EVIDENCE';IndependentSummaryResult='INVALID_EVIDENCE';Error=$null;Counts=@();Groups=@();AttemptsVerified=0;SuccessfulGesturesVerified=0;FirstUnexpectedFailure=$null;RetryCount=0;FinalButtonState='UNKNOWN';FinalButtonObservation=$null;ArtifactBindings=@();CurrentIdentityVerified=$false}
$exitCode=2
try{
    $batchRoot=[IO.Path]::GetFullPath((Join-Path $repo 'uat/r1c4b-fixh'))+[IO.Path]::DirectorySeparatorChar
    foreach($path in @($PlanPath,$InventoryPath,$StatisticsPath,$ArtifactPath)){Assert-OwnedStability ([IO.Path]::GetFullPath($path).StartsWith($batchRoot,[StringComparison]::OrdinalIgnoreCase)) 'summary paths must be in local Fix H evidence'}
    $bindings=@();foreach($path in @($PlanPath,$InventoryPath,$StatisticsPath)){$bindings+=@([pscustomobject]@{Path=[IO.Path]::GetFullPath($path);SHA256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash})}
    $plan=Get-Content -LiteralPath $PlanPath -Raw -Encoding UTF8|ConvertFrom-Json
    $inventory=Get-Content -LiteralPath $InventoryPath -Raw -Encoding UTF8|ConvertFrom-Json
    $statistics=Get-Content -LiteralPath $StatisticsPath -Raw -Encoding UTF8|ConvertFrom-Json
    Assert-OwnedStability ($plan.Schema -ceq 'r1c4b-owned-stability-plan/v1' -and $inventory.Schema -ceq 'r1c4b-owned-stability-inventory/v1' -and $statistics.Schema -ceq 'r1c4b-owned-stability-batch-statistics/v1') 'wrong control schema'
    Test-OwnedStabilityPlan $plan.Steps
    Assert-OwnedStability ($inventory.BatchId -ceq $plan.BatchId -and $statistics.BatchId -ceq $plan.BatchId -and $inventory.ImplementationSHA -ceq $plan.ImplementationSHA -and $inventory.ExecutedHEAD -ceq $plan.ImplementationSHA -and $statistics.ImplementationSHA -ceq $plan.ImplementationSHA -and $inventory.PlanSHA256 -ceq $bindings[0].SHA256 -and $statistics.PlanSHA256 -ceq $bindings[0].SHA256 -and $inventory.RetryCount -eq 0) 'batch/checkpoint/plan identity mismatch'
    $summary.BatchId=$plan.BatchId;$summary.ImplementationSHA=$plan.ImplementationSHA
    $state=[ordered]@{ExecutedHEAD=$plan.ImplementationSHA;Identity=@{};OuterIdentity=[ordered]@{};PlanPath=$PlanPath;PlanSHA256=$bindings[0].SHA256}
    foreach($configuration in @('Debug','Release')){$state.Identity[$configuration]=[ordered]@{};foreach($p in $plan.RuntimeIdentity.$configuration.PSObject.Properties){$state.Identity[$configuration][$p.Name]=$p.Value}}
    foreach($p in $plan.OuterIdentity.PSObject.Properties){$state.OuterIdentity[$p.Name]=$p.Value}
    foreach($configuration in @('Debug','Release')){Assert-OwnedStabilityCheckpoint $repo $state $configuration};$summary.CurrentIdentityVerified=$true
    Assert-OwnedStability ((Get-FileHash -LiteralPath $plan.PrerequisitePath -Algorithm SHA256).Hash -ceq $plan.PrerequisiteSHA256 -and $inventory.PrerequisiteSHA256 -ceq $plan.PrerequisiteSHA256) 'prerequisite reference changed'
    $prerequisite=Get-Content -LiteralPath $plan.PrerequisitePath -Raw -Encoding UTF8|ConvertFrom-Json
    Assert-OwnedStability ($prerequisite.Result -ceq 'PASS') 'unverified prerequisites'
    $progress=@(Get-Content -LiteralPath $inventory.ProgressPath -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json})
    Assert-OwnedStability ((Get-FileHash -LiteralPath $inventory.ProgressPath -Algorithm SHA256).Hash -ceq $inventory.ProgressSHA256 -and $progress.Count -eq 1+2*@($inventory.Attempts).Count -and $progress[0].Type -ceq 'plan_frozen') 'incomplete/changed append-only progress'
    $nonces=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);$attempts=@();$measurements=@();$stopped=$false
    foreach($item in @($inventory.Attempts)){
        Assert-OwnedStability (-not $stopped) 'attempt after first non-PASS'
        $step=$plan.Steps[$attempts.Count];Assert-OwnedStabilityNextStep $plan.Steps $attempts $step
        foreach($name in @('Number','GroupNumber','Phase','Configuration','Operation','Repetition','Mode','RunId')){Assert-OwnedStability ($item.$name -ceq $step.$name) ('attempt/plan mapping '+$name)}
        $started=$progress[1+2*$attempts.Count];$finished=$progress[2+2*$attempts.Count]
        Assert-OwnedStability ($started.Type -ceq 'attempt_started' -and $finished.Type -ceq 'attempt_finished' -and $started.Attempt.RunId -ceq $item.RunId -and $finished.Attempt.RunId -ceq $item.RunId -and $finished.Attempt.Result -ceq $item.Result -and $started.PlanSHA256 -ceq $bindings[0].SHA256 -and $finished.PlanSHA256 -ceq $bindings[0].SHA256) 'progress order/result mismatch'
        foreach($artifact in @($item.Artifacts)){
            if($artifact.Exists){Assert-OwnedStability ((Get-FileHash -LiteralPath $artifact.Path -Algorithm SHA256).Hash -ceq $artifact.SHA256) 'attempt artifact changed'}
            else{Assert-OwnedStability (-not (Test-Path -LiteralPath $artifact.Path)) 'missing artifact was substituted after stop'}
        }
        if($item.Result -ceq 'PASS'){
            Assert-OwnedStability ($item.ChildInvoked -is [bool] -and $item.ChildInvoked -and $item.ChildExitCode -eq 0 -and @($item.Artifacts).Count -eq 4 -and @($item.Artifacts|Where-Object Exists -ne $true).Count -eq 0) 'incomplete passing attempt'
            $fresh=Read-ReliabilityRun $item.MetadataPath ([pscustomobject]@{Configuration=$step.Configuration;Operation=$step.Operation;Mode='normal'}) $step.RunId
            Assert-OwnedStabilityNormalVerdict $fresh $step.Operation
            $rows=@(Get-Content -LiteralPath $fresh.EvidencePath -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json})
            $measurement=Get-OwnedStabilityMeasurement -Rows $rows -Verdict $fresh.IndependentVerdict -Phase $step.Phase -Configuration $step.Configuration -Operation $step.Operation
            Assert-OwnedStability ($measurement.Status -ceq 'AVAILABLE' -and @($rows|Where-Object type -ceq 'ENTER').Count -eq 1) 'missing actual target gesture/measurement'
            Assert-OwnedStability (($measurement|ConvertTo-Json -Depth 32 -Compress) -ceq ($item.Measurement|ConvertTo-Json -Depth 32 -Compress)) 'cached measurement differs from fresh evidence'
            $measurements+=@($measurement);++$summary.SuccessfulGesturesVerified
        }else{
            Assert-OwnedStability ($item.Result -cin @('BLOCKED','FAIL','INVALID_EVIDENCE')) 'unknown termination classification'
            $stopped=$true;$summary.FirstUnexpectedFailure=$item
            # Re-read failed raw/native or preflight proof too, not its cache.
            try{$failed=Get-OwnedStabilityStoppedResult $item $plan;$summary.BatchResult=$failed.Result;$item.IndependentVerdict=$failed}
            catch{$summary.BatchResult='INVALID_EVIDENCE';$summary.Error='stopped attempt independently unverifiable: '+$_.Exception.Message}
        }
        $attempts+=@($item);++$summary.AttemptsVerified
        $summary.FinalButtonState=$item.FinalButtonState
        if($item.MetadataPath -and (Test-Path -LiteralPath $item.MetadataPath)){
            $m=Get-Content -LiteralPath $item.MetadataPath -Raw -Encoding UTF8|ConvertFrom-Json
            $observed=Get-ReliabilityAttemptButtonState $m $item.RunDirectory $item.RunId
            Assert-OwnedStability ($observed -ceq $item.FinalButtonState) 'current failed/successful attempt observation cannot reuse preceding UP'
            $part=if(Test-ReliabilityTrue $m.ProbeInvoked){'Post'}else{'Pre'}
            $summary.FinalButtonObservation=[pscustomobject]@{Source=('READ_ONLY_'+$part.ToUpperInvariant());Path=(Get-ReliabilityField $m ('Environment'+$part+'Path'));SHA256=(Get-ReliabilityField $m ('Environment'+$part+'SHA256'));State=$observed}
        }
    }
    $summary.Counts=Get-OwnedStabilityCounts $plan.Steps $attempts
    Assert-OwnedStability (($summary.Counts|ConvertTo-Json -Depth 16 -Compress) -ceq ($inventory.Counts|ConvertTo-Json -Depth 16 -Compress)) 'counts inconsistent'
    $groups=@();foreach($group in 1..8){$planned=@($plan.Steps|Where-Object GroupNumber -eq $group);$selected=@($measurements|Where-Object {$_.Phase -ceq $planned[0].Phase -and $_.Configuration -ceq $planned[0].Configuration -and $_.Operation -ceq $planned[0].Operation});$groups+=@(Get-OwnedStabilityStatistics -Measurements $selected -Phase $planned[0].Phase -Configuration $planned[0].Configuration -Operation $planned[0].Operation -PlannedGestures $planned.Count)}
    Assert-OwnedStability (($groups|ConvertTo-Json -Depth 32 -Compress) -ceq ($statistics.Groups|ConvertTo-Json -Depth 32 -Compress)) 'statistics inconsistent with independent raw recomputation'
    $summary.Groups=$groups;$summary.ArtifactBindings=$bindings
    foreach($binding in $bindings){Assert-OwnedStability ((Get-FileHash -LiteralPath $binding.Path -Algorithm SHA256).Hash -ceq $binding.SHA256) 'control evidence changed during summary'}
    if(-not $stopped -and $attempts.Count -eq 100 -and $summary.SuccessfulGesturesVerified -eq 100 -and @($summary.Counts|Where-Object {$_.Passed -ne $_.Planned}).Count -eq 0 -and $inventory.LoopResult -ceq 'ALL_PLANNED_PASS'){$summary.BatchResult='PASS';$summary.IndependentSummaryResult='PASS';$exitCode=0}
    else{$summary.IndependentSummaryResult='NOT_PASSED'}
}catch{$summary.Error=$_.Exception.Message;$summary.BatchResult='INVALID_EVIDENCE';$summary.IndependentSummaryResult='INVALID_EVIDENCE';$exitCode=2}
try{Write-ReliabilityNewJson $ArtifactPath $summary}catch{Write-Error ('Fix H summary evidence write failed: '+$_.Exception.Message) -ErrorAction Continue;$exitCode=2}
Write-Host "Fix H independent summary: $($summary.BatchResult); verified=$($summary.SuccessfulGesturesVerified)/100; artifact=$ArtifactPath"
exit $exitCode
