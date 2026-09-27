[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-native-abort-validation.ps1')
. (Join-Path $PSScriptRoot 'r1c4b-owned-stability-model.ps1')
. (Join-Path $PSScriptRoot 'r1c4b-owned-stability-statistics.ps1')
foreach($definition in @(Get-OwnedStabilityDefinitions)){. $definition}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
# The frozen Read-ReliabilityRun expects its existing run root and state names.
$root=Join-Path $repo 'uat/r1c4b-fixg'
$batchRoot=Join-Path $repo 'uat/r1c4b-fixh'
$batchId=[Guid]::NewGuid().ToString('N')
$directory=Join-Path $batchRoot ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-'+$batchId)
$planPath=Join-Path $directory 'plan.json';$progressPath=Join-Path $directory 'progress.jsonl'
$inventoryPath=Join-Path $directory 'inventory.json';$statisticsPath=Join-Path $directory 'statistics.json';$summaryPath=Join-Path $directory 'summary.json';$prerequisitePath=Join-Path $directory 'prerequisites.json'
$state=[ordered]@{Schema='r1c4b-owned-stability-inventory/v1';BatchId=$batchId;ImplementationSHA='NOT_AVAILABLE';ExecutedHEAD='NOT_AVAILABLE';Identity=@{};OuterIdentity=@{};PlanPath=$planPath;PlanSHA256=$null;ProgressPath=$progressPath;ProgressSHA256=$null;PrerequisitePath=$prerequisitePath;PrerequisiteSHA256=$null;LoopResult='NOT_RUN';BatchResult='INVALID_EVIDENCE';Attempts=@();FirstUnexpectedFailure=$null;FinalButtonState='UNKNOWN';RetryCount=0;Error=$null;Counts=@();MetadataWriteStatus='NOT_RUN';MetadataWriteError=$null;StatisticsError=$null;SummaryError=$null}
$nonces=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$created=$false;$summaryExit=2
$steps=@(Get-OwnedStabilitySteps)
function Invoke-OwnedStabilityChild($Attempt){
    Assert-OwnedStabilityCheckpoint $repo $state $Attempt.Configuration
    $Attempt.ChildInvoked=$true;$Attempt.ProbeInvoked='UNKNOWN';$Attempt.TargetGestureEntered='UNKNOWN'
    $single=Join-Path $PSScriptRoot 'run-r1c4b-input-reliability.ps1'
    # The one and only child site. No extra single, abort, native or old v4 CLI.
    $output=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $single -Configuration $Attempt.Configuration -Operation $Attempt.Operation -Mode normal -RunId $Attempt.RunId -ExpectedHEAD $state.ExecutedHEAD 2>&1)
    $Attempt.ChildExitCode=$LASTEXITCODE;$Attempt.ChildOutput=@($output|ForEach-Object {[string]$_})
    $directories=@(Get-ChildItem -LiteralPath $root -Directory -Filter ('*-'+$Attempt.RunId))
    Assert-OwnedStability ($directories.Count -eq 1) 'missing/ambiguous current attempt directory; preserve output and STOP'
    $Attempt.RunDirectory=$directories[0].FullName;$Attempt.Artifacts=@(Get-OwnedStabilityAttemptArtifacts $Attempt.RunDirectory)
    $Attempt.PreflightAttempted=@($Attempt.Artifacts|Where-Object {$_.Kind -ceq 'Pre' -and $_.Exists}).Count -eq 1
    $metadata=Join-Path $Attempt.RunDirectory 'run.metadata.json';$log=Join-Path $Attempt.RunDirectory 'probe.jsonl'
    $rows=@();$complete=$false
    if(Test-Path -LiteralPath $log -PathType Leaf){
        try{
            $rows=@(Get-Content -LiteralPath $log -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json})
            if(@($rows|Where-Object type -ceq 'startup').Count -eq 1){$Attempt.ProbeInvoked=$true}
            $complete=@($rows|Where-Object type -ceq 'startup').Count -eq 1 -and @($rows|Where-Object type -ceq 'shutdown').Count -eq 1
            if(@($rows|Where-Object type -ceq 'ENTER').Count -gt 0){$Attempt.TargetGestureEntered=$true}elseif($complete){$Attempt.TargetGestureEntered=$false}
        }catch{$Attempt.Reasons+=@('raw parsing: '+$_.Exception.Message)}
    }
    Assert-OwnedStability (Test-Path -LiteralPath $metadata -PathType Leaf) 'current attempt metadata missing; no next child'
    $Attempt.MetadataPath=$metadata;$Attempt.MetadataSHA256=(Get-FileHash -LiteralPath $metadata -Algorithm SHA256).Hash
    $m=Get-Content -LiteralPath $metadata -Raw -Encoding UTF8|ConvertFrom-Json
    $rawProbeInvoked=$Attempt.ProbeInvoked
    foreach($name in @('ProbeExitCode','NativeBlockReason','ValidatorError','PostObservationStatus','PostObservationError','AfterIdentityStatus','AfterIdentityError','ImplementationUnchanged','FirstFailureStage','FirstFailureRecordSequences')){ $Attempt.$name=Get-ReliabilityField $m $name }
    $reportedProbe=Get-ReliabilityField $m 'ProbeInvoked'
    if($rawProbeInvoked -eq $true){Assert-OwnedStability (Test-ReliabilityTrue $reportedProbe) 'metadata contradicts actual startup; STOP'}
    $Attempt.ProbeInvoked=$reportedProbe
    if(Test-ReliabilityFalse $Attempt.ProbeInvoked){$Attempt.TargetGestureEntered=$false}
    if((Get-ReliabilityField $m 'EnvironmentPreExitCode') -is [int] -or (Get-ReliabilityField $m 'EnvironmentPreExitCode') -is [long]){$Attempt.PreflightAttempted=$true}
    $Attempt.FinalButtonState=Get-ReliabilityAttemptButtonState $m $Attempt.RunDirectory $Attempt.RunId
    $Attempt.FinalButtonObservationSource=if(Test-ReliabilityTrue $Attempt.ProbeInvoked){'READ_ONLY_POST'}else{'READ_ONLY_PRE'}
    $Attempt.Reasons=@(Get-ReliabilityField $m 'Reason')+@(Get-ReliabilityField $m.Result 'Reasons')+@(Get-ReliabilityField $m 'NativeError')
    # Non-PASS is still independently examined; never wash a reason string into
    # a passing fixture or use cleanup to complete an interrupted normal.
    if($Attempt.ChildExitCode -ne 0){
        if(Test-ReliabilityFalse $Attempt.ProbeInvoked){
            $prePath=Join-Path $Attempt.RunDirectory 'environment-pre.json'
            if(Test-Path -LiteralPath $prePath){
                $pre=Get-Content -LiteralPath $prePath -Raw -Encoding UTF8|ConvertFrom-Json
                if($Attempt.FinalButtonState -cin @('OBSERVED_UP','OBSERVED_DOWN') -and $pre.schema -ceq 'r1c4b-readonly-input-environment/v2' -and -not (Test-ReliabilityEnvironment $pre)){$Attempt.Result='BLOCKED'}
            }
        }elseif($complete){
            try{
                $v=Test-FixFInputReliabilityEvidence -Path $log;$Attempt.IndependentVerdict=$v
                if($v.Result -ceq 'BLOCKED' -and ((Get-ReliabilityField $v 'EvidenceIntegrity') -ceq 'VALID' -or (Test-ReliabilityTrue (Get-ReliabilityField $v 'FailureDiagnosticVerified')))){$Attempt.Result='BLOCKED'}
                elseif($v.Result -ceq 'FAIL'){$Attempt.Result='FAIL'}
            }catch{$Attempt.Reasons+=@('independent verdict: '+$_.Exception.Message)}
        }
        if($Attempt.Result -ceq 'NOT_RUN'){$Attempt.Result='INVALID_EVIDENCE'}
        throw 'current child nonzero; first-failure STOP'
    }
    $step=[pscustomobject]@{Configuration=$Attempt.Configuration;Operation=$Attempt.Operation;Mode='normal'}
    $validated=Read-ReliabilityRun $metadata $step $Attempt.RunId
    $v=$validated.IndependentVerdict
    Assert-OwnedStabilityNormalVerdict $validated $Attempt.Operation
    $Attempt.IndependentVerdict=$v
    $Attempt.Measurement=Get-OwnedStabilityMeasurement -Rows $rows -Verdict $v -Phase $Attempt.Phase -Configuration $Attempt.Configuration -Operation $Attempt.Operation
    Assert-OwnedStability ($Attempt.Measurement.Status -ceq 'AVAILABLE') 'normal timing/counters could not be independently measured'
    Assert-OwnedStabilityCheckpoint $repo $state $Attempt.Configuration
    $Attempt.Result='PASS';$Attempt.Error=$null
    Write-Host ("Fix H {0} {1} {2} {3}/{4} PASS; overall {5}/100" -f $Attempt.Phase,$Attempt.Configuration,$Attempt.Operation,$Attempt.Repetition,$(if($Attempt.Phase -ceq 'smoke'){5}else{20}),$Attempt.Number)
}
Push-Location $repo
try{
    $head=git rev-parse HEAD;$headCode=$LASTEXITCODE;$status=@(git status --porcelain)
    Assert-OwnedStability ($headCode -eq 0 -and $LASTEXITCODE -eq 0 -and $status.Count -eq 0) 'require clean implementation checkpoint before any child/environment'
    git check-ignore -- $planPath|Out-Null;Assert-OwnedStability ($LASTEXITCODE -eq 0) 'all Fix H artifacts must remain ignored'
    $state.ExecutedHEAD=$head;$state.ImplementationSHA=$head
    foreach($configuration in @('Debug','Release')){$state.Identity[$configuration]=Get-ReliabilitySnapshot $repo $configuration}
    $state.OuterIdentity=Get-OwnedStabilityOuterIdentity $repo
    $null=New-Item -ItemType Directory -Path $directory;$created=$true
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'verify-r1c4b-fixh-prerequisites.ps1') -ArtifactPath $prerequisitePath
    Assert-OwnedStability ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $prerequisitePath)) 'historical prerequisites not verified; no GUI'
    $state.PrerequisiteSHA256=(Get-FileHash -LiteralPath $prerequisitePath -Algorithm SHA256).Hash
    Test-OwnedStabilityPlan $steps
    $plan=[ordered]@{Schema='r1c4b-owned-stability-plan/v1';PlanVersion='v5_normal_20_smoke_then_80_formal_v1';BatchId=$batchId;ImplementationSHA=$head;CreatedUtc=[DateTime]::UtcNow.ToString('o');RunRoot=$root;BatchRoot=$directory;RuntimeIdentity=$state.Identity;OuterIdentity=$state.OuterIdentity;PrerequisitePath=$prerequisitePath;PrerequisiteSHA256=$state.PrerequisiteSHA256;Steps=$steps;MaxProbeInvocations=100;RetryAllowed=$false;ResumeAllowed=$false;StopAtFirstNonPass=$true;ExtraSinglesAllowed=$false;ControlledAbortAllowed=$false}
    Write-ReliabilityNewJson $planPath $plan;$state.PlanSHA256=(Get-FileHash -LiteralPath $planPath -Algorithm SHA256).Hash
    $stream=[IO.File]::Open($progressPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read);$stream.Dispose()
    Write-OwnedStabilityProgress $progressPath 'plan_frozen' $null $state.PlanSHA256
    Write-Host "Fix H frozen plan: $planPath SHA256=$($state.PlanSHA256); at most 100 normal starts; first failure STOP, no retry"
    Invoke-OwnedStabilityProgression $steps $state {param($attempt) Invoke-OwnedStabilityChild $attempt} {param($type,$attempt) Write-OwnedStabilityProgress $progressPath $type $attempt $state.PlanSHA256}
}catch{$state.Error=$_.Exception.Message;if($state.BatchResult -ceq 'PENDING_INDEPENDENT_SUMMARY'){$state.BatchResult='INVALID_EVIDENCE'};Write-Host ('STOP: '+$state.Error)}
finally{
    if($created){
        try{
            $state.Counts=Get-OwnedStabilityCounts $steps $state.Attempts
            $state.ProgressSHA256=(Get-FileHash -LiteralPath $progressPath -Algorithm SHA256).Hash
            $state.MetadataWriteStatus='WRITTEN';Write-ReliabilityNewJson $inventoryPath $state
        }catch{$state.MetadataWriteStatus='ERROR';$state.MetadataWriteError=$_.Exception.Message;$state.BatchResult='INVALID_EVIDENCE';Write-Error ('Fix H inventory write failed: '+$state.MetadataWriteError) -ErrorAction Continue}
        try{
            $groups=@();foreach($group in 1..8){$planned=@($steps|Where-Object GroupNumber -eq $group);$measurements=@($state.Attempts|Where-Object {$_.GroupNumber -eq $group -and $_.Result -ceq 'PASS'}|ForEach-Object Measurement);$groups+=@(Get-OwnedStabilityStatistics -Measurements $measurements -Phase $planned[0].Phase -Configuration $planned[0].Configuration -Operation $planned[0].Operation -PlannedGestures $planned.Count)}
            Write-ReliabilityNewJson $statisticsPath ([ordered]@{Schema='r1c4b-owned-stability-batch-statistics/v1';BatchId=$batchId;ImplementationSHA=$state.ExecutedHEAD;PlanSHA256=$state.PlanSHA256;SuccessfulNormalOnly=$true;Groups=$groups})
        }catch{$state.StatisticsError=$_.Exception.Message;$state.BatchResult='INVALID_EVIDENCE';Write-Error ('Fix H statistics failed: '+$state.StatisticsError) -ErrorAction Continue}
        try{
            & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'verify-r1c4b-owned-stability-summary.ps1') -PlanPath $planPath -InventoryPath $inventoryPath -StatisticsPath $statisticsPath -ArtifactPath $summaryPath
            $summaryExit=$LASTEXITCODE
        }catch{$state.SummaryError=$_.Exception.Message;$summaryExit=2;Write-Error ('Fix H independent summary failed: '+$state.SummaryError) -ErrorAction Continue}
        if($state.MetadataWriteStatus -cne 'WRITTEN' -or $state.StatisticsError -or $state.SummaryError){$summaryExit=2}
        try{
            $receipt=[ordered]@{Schema='r1c4b-owned-stability-finalization/v1';BatchId=$batchId;ImplementationSHA=$state.ExecutedHEAD;InventoryPath=$inventoryPath;StatisticsPath=$statisticsPath;SummaryPath=$summaryPath;InventoryWriteStatus=$state.MetadataWriteStatus;InventoryWriteError=$state.MetadataWriteError;StatisticsError=$state.StatisticsError;SummaryError=$state.SummaryError;SummaryExitCode=$summaryExit;SummaryExists=(Test-Path -LiteralPath $summaryPath);AggregateExitCode=$summaryExit;CreatedUtc=[DateTime]::UtcNow.ToString('o')}
            Write-ReliabilityNewJson (Join-Path $directory 'finalization.json') $receipt
        }catch{Write-Error ('Fix H finalization receipt write failed: '+$_.Exception.Message) -ErrorAction Continue;$summaryExit=2}
        Write-Host "Fix H evidence: $directory; independent_summary_exit=$summaryExit"
    }
    Pop-Location
}
exit $summaryExit
