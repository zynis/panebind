[CmdletBinding()]
param([Parameter(Mandatory)][string]$ArtifactPath)
# One fixed historical Fix H batch. No native/environment/input process is launched.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-native-abort-validation.ps1')
. (Join-Path $PSScriptRoot 'r1c4b-owned-stability-model.ps1')
. (Join-Path $PSScriptRoot 'r1c4b-owned-stability-statistics.ps1')
foreach($definition in @(Get-OwnedStabilityDefinitions)){. $definition}

$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Join-Path $repo 'uat/r1c4b-fixg' # frozen Read-ReliabilityRun's historical root
$batchId='c8e0d10d38c344879d4554be4e019742'
$evidenceSHA='01633ff31776b42be9fc2383ed301d9884e80019'
$batch=Join-Path $repo ('uat/r1c4b-fixh/20260927T174945663Z-'+$batchId)
$correctionRoot=Join-Path $repo 'uat/r1c4b-fixh-correction'
$originalControl=[ordered]@{
    'plan.json'='8CF16B8E015891CD3C85B9703E4E105BB31C8EBFCCF19E18C16A2D1C1125576A'
    'progress.jsonl'='09CD57F8A96784652F5686DFF9892CF73700BDB583645F4B212627D4783E4EE4'
    'inventory.json'='465252DE89F6CFACC1D6640DFD2015498F4575A915955AF92B3C0DAD5C63EE18'
    'statistics.json'='061A1E0FC864D11DB00298D9794F24BD5FAAE0034303D421DF4E793537A57F6E'
    'summary.json'='1F9FABF6C342EF55514E0564E19473A71C89B554955C92A366C531CB2FBB282D'
    'finalization.json'='9B65284133AB9F7AB52B9769E6468A97062DCF3FC72918402FEEDB6D7CE16898'
    'prerequisites.json'='6DE0652D9F6FF996BED3F29D373D5F244514E3895058535F86AE2161B003EEA0'
}
$originalPackages=[ordered]@{
    'fixh-evidence-001.zip'='8CC702DCD26EA1A057D9624D757928658C7E805DD3BD1044E1304628A146AB3E'
    'fixh-evidence-002.zip'='DB3ACB784977282B978263650993C3339BCC005EE3B6235FDC35EF82EB3C59B1'
    'fixh-evidence-003.zip'='64BA71F932612D8D3920BD6FC785B19E41ABDEF723BE5D534CF9ACBC8E34D49E'
    'package-manifest.json'='508E775B0BE44A3497C5B521C91120C3A84351AFD8FFBF0B63ED85417D169DC1'
}
$originals=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
$nonces=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$auditSourceFiles=@(
    'scripts/replay-r1c4b-fixh-evidence.ps1',
    'scripts/r1c4b-owned-stability-model.ps1',
    'scripts/r1c4b-owned-stability-statistics.ps1',
    'scripts/package-r1c4b-fixh-correction.ps1',
    'scripts/run-r1c4b-input-reliability-gates.ps1',
    'scripts/run-r1c4b-input-reliability.ps1',
    'scripts/r1c4b-native-abort-validation.ps1'
)
$result=[ordered]@{
    Schema='r1c4b-fixh-corrected-replay/v1';Result='INVALID_EVIDENCE';Error=$null
    OriginalBatchId=$batchId;EvidenceImplementationSHA=$evidenceSHA
    AuditorImplementationSHA='NOT_AVAILABLE';AuditorCodeBindings=@();OriginalPlanPath=(Join-Path $batch 'plan.json')
    OriginalPlanSHA256=$originalControl['plan.json'];OriginalControlBindings=@();OriginalPackageBindings=@()
    OriginalGitBlobProofs=@();OriginalReportBinding=$null;OriginalFilesVerifiedBefore=0;OriginalFilesVerifiedAfter=0;OriginalFilesUnchanged=$false
    OriginalPrerequisitesVerified=$false;OriginalMachineSummary='UNTRUSTED';OriginalAcceptance='INVALID_EVIDENCE'
    Planned=100;ProgressRecordsVerified=0;AttemptsVerified=0;NormalProofsVerified=0;RunIdsUnique=0;NoncesUnique=0
    HistoricalCountersMatch=$null;CounterCorrections=@();CorrectedCounts=@();StatisticsGroups=@()
    FinalButtonObservation=$null;CurrentButtonStateClaim='NOT_OBSERVED_NOW'
    NewGuiInvocations=0;NewInputEvents=0;RetryCount=0;OwnedFreeTakeoverGate='NOT_PASSED'
    RawInputTakeoverArchitecture='UNRESOLVED';ExplorerExecuted=$false
}

function Add-FixHOriginal([string]$Path,[string]$ExpectedSHA){
    $absolute=[IO.Path]::GetFullPath($Path)
    Assert-OwnedStability ($absolute.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -and $ExpectedSHA -cmatch '^[A-F0-9]{64}$') 'original reference leaves repository or has invalid hash'
    if($originals.ContainsKey($absolute)){Assert-OwnedStability ($originals[$absolute] -ceq $ExpectedSHA) 'contradictory original hash reference'}
    else{$originals.Add($absolute,$ExpectedSHA)}
    Assert-OwnedStability ((Get-FileHash -LiteralPath $absolute -Algorithm SHA256).Hash -ceq $ExpectedSHA) ('original bytes changed: '+$absolute)
}
function Get-FixHGitBlobSHA256([string]$Commit,[string]$Relative){
    Assert-OwnedStability ($Commit -cmatch '^[a-f0-9]{40}$' -and $Relative -cmatch '^(scripts|src|docs)/[A-Za-z0-9_./-]+$' -and -not $Relative.Contains('..')) 'invalid historical Git blob reference'
    $info=New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName='git.exe';$info.Arguments='-C "'+$repo+'" cat-file blob '+$Commit+':'+$Relative
    $info.UseShellExecute=$false;$info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
    $process=[System.Diagnostics.Process]::Start($info)
    $bytes=New-Object System.IO.MemoryStream
    try{
        $process.StandardOutput.BaseStream.CopyTo($bytes)
        $errorText=$process.StandardError.ReadToEnd();$process.WaitForExit()
        Assert-OwnedStability ($process.ExitCode -eq 0) ('historical Git blob unavailable: '+$Relative+' '+$errorText)
        $sha=[Security.Cryptography.SHA256]::Create()
        try{return [BitConverter]::ToString($sha.ComputeHash($bytes.ToArray())).Replace('-','')}
        finally{$sha.Dispose()}
    }finally{$bytes.Dispose();$process.Dispose()}
}
function Get-FixHAuditorBindings{
    $items=@()
    foreach($relative in $auditSourceFiles){$items+=@([pscustomobject]@{Path=$relative;SHA256=(Get-FileHash -LiteralPath (Join-Path $repo $relative) -Algorithm SHA256).Hash})}
    return $items
}
function Assert-FixHAuditorUnchanged($Bindings,[string]$ExpectedHEAD){
    $now=git -C $repo rev-parse HEAD;$headCode=$LASTEXITCODE;$status=@(git -C $repo status --porcelain)
    Assert-OwnedStability ($headCode -eq 0 -and $LASTEXITCODE -eq 0 -and $now -ceq $ExpectedHEAD -and $status.Count -eq 0) 'auditor checkpoint changed'
    foreach($entry in $Bindings){Assert-OwnedStability ((Get-FileHash -LiteralPath (Join-Path $repo $entry.Path) -Algorithm SHA256).Hash -ceq $entry.SHA256) 'auditor source changed'}
}
function Assert-FixHReconstructedFlags($Cached,$Finished,$Fresh){
    foreach($field in @('ChildInvoked','PreflightAttempted','ProbeInvoked','TargetGestureEntered')){
        foreach($source in @($Cached,$Finished)){
            $property=$source.PSObject.Properties[$field]
            Assert-OwnedStability ($null -ne $property -and $property.Value -is [bool] -and $property.Value -eq $Fresh.$field) ('cached '+$field+' contradicts raw/progress proof')
        }
    }
    Assert-OwnedStability ($Cached.Result -is [string] -and $Cached.Result -ceq 'PASS' -and $Finished.Result -is [string] -and $Finished.Result -ceq 'PASS' -and ($Cached.ChildExitCode -is [int] -or $Cached.ChildExitCode -is [long]) -and $Cached.ChildExitCode -eq 0 -and ($Finished.ChildExitCode -is [int] -or $Finished.ChildExitCode -is [long]) -and $Finished.ChildExitCode -eq 0) 'cached attempt is not a complete normal pass'
}
function Assert-FixHCorrectedGroup($Old,$New,[int]$Expected){
    $oldFields=@($Old.PSObject.Properties.Name);$newFields=@($New.PSObject.Properties.Name)
    Assert-OwnedStability (($oldFields -join ',') -ceq ($newFields -join ',')) 'counter schema changed'
    $other=@('GroupNumber','Phase','Configuration','Operation','Planned','Attempts','PreflightAttempts','ProbeInvocations','TargetGesturesEntered','Passed','Blocked','Failed','InvalidEvidence','NotRun')
    foreach($field in $other){Assert-OwnedStability ($Old.$field -ceq $New.$field) ('unexplained counter difference: '+$field)}
    foreach($field in @('ProbeInvocationsUnknown','TargetGesturesUnknown')){
        Assert-OwnedStability ($Old.$field -eq $Expected -and $New.$field -eq 0) ('unknown counter difference is not the confirmed defect: '+$field)
    }
    foreach($field in @('Planned','Attempts','PreflightAttempts','ProbeInvocations','TargetGesturesEntered','Passed')){
        Assert-OwnedStability ($New.$field -eq $Expected) ('incomplete original normal group: '+$field)
    }
    foreach($field in @('ProbeInvocationsUnknown','TargetGesturesUnknown','Blocked','Failed','InvalidEvidence','NotRun')){
        Assert-OwnedStability ($New.$field -eq 0) ('corrected original group has '+$field)
    }
    Assert-OwnedStability ($New.ProbeInvocations+$New.ProbeInvocationsUnknown -eq $New.Attempts -and $New.TargetGesturesEntered+$New.TargetGesturesUnknown -le $New.ProbeInvocations) 'known/unknown categories overlap'
}

$artifactAbsolute=[IO.Path]::GetFullPath($ArtifactPath)
$artifactAuthorized=$false
try{
    Assert-OwnedStability ($artifactAbsolute.StartsWith(([IO.Path]::GetFullPath($correctionRoot)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase) -and -not (Test-Path -LiteralPath $artifactAbsolute)) 'new audit artifact must be unused under ignored correction root'
    git -C $repo check-ignore -- $artifactAbsolute | Out-Null
    Assert-OwnedStability ($LASTEXITCODE -eq 0) 'audit artifact must remain ignored'
    $artifactAuthorized=$true
    $auditor=git -C $repo rev-parse HEAD;$headCode=$LASTEXITCODE;$status=@(git -C $repo status --porcelain)
    Assert-OwnedStability ($headCode -eq 0 -and $LASTEXITCODE -eq 0 -and $status.Count -eq 0 -and $auditor -cmatch '^[a-f0-9]{40}$') 'require committed clean auditor checkpoint'
    $result.AuditorImplementationSHA=$auditor
    git -C $repo merge-base --is-ancestor $evidenceSHA $auditor | Out-Null
    Assert-OwnedStability ($LASTEXITCODE -eq 0) 'original implementation is not ancestor of auditor'
    Assert-OwnedStability ((git -C $repo cat-file -t $evidenceSHA) -ceq 'commit' -and $LASTEXITCODE -eq 0) 'original implementation Git commit missing'
    $result.AuditorCodeBindings=@(Get-FixHAuditorBindings)
    Assert-FixHAuditorUnchanged $result.AuditorCodeBindings $auditor
    $oldReport='docs/reports/R1C4B_PIVOT1_FIXH_EXECUTION_REPORT.md'
    $reportHash=Get-FixHGitBlobSHA256 'e8f12d5cf25b62c0143efc9e079e95e5228712b0' $oldReport
    $reportPath=Join-Path $repo $oldReport
    Add-FixHOriginal $reportPath $reportHash
    Assert-OwnedStability ((Get-Content -LiteralPath $reportPath -Raw -Encoding UTF8).Contains('FIXH_BATCH_RESULT = INVALID_EVIDENCE')) 'preserved original rejection report missing'
    $result.OriginalReportBinding=[pscustomobject]@{GitCommit='e8f12d5cf25b62c0143efc9e079e95e5228712b0';Path=$reportPath;SHA256=$reportHash;OriginalAcceptance='INVALID_EVIDENCE'}

    foreach($name in $originalControl.Keys){
        $path=Join-Path $batch $name;Add-FixHOriginal $path $originalControl[$name]
        $result.OriginalControlBindings+=@([pscustomobject]@{Name=$name;Path=$path;SHA256=$originalControl[$name]})
    }
    $packageRoot=Join-Path $batch 'review-packages'
    foreach($name in $originalPackages.Keys){
        $path=Join-Path $packageRoot $name;Add-FixHOriginal $path $originalPackages[$name]
        $result.OriginalPackageBindings+=@([pscustomobject]@{Name=$name;Path=$path;SHA256=$originalPackages[$name];Bytes=(Get-Item -LiteralPath $path).Length})
    }
    $plan=Get-Content -LiteralPath (Join-Path $batch 'plan.json') -Raw -Encoding UTF8|ConvertFrom-Json
    $inventory=Get-Content -LiteralPath (Join-Path $batch 'inventory.json') -Raw -Encoding UTF8|ConvertFrom-Json
    $statistics=Get-Content -LiteralPath (Join-Path $batch 'statistics.json') -Raw -Encoding UTF8|ConvertFrom-Json
    $priorSummary=Get-Content -LiteralPath (Join-Path $batch 'summary.json') -Raw -Encoding UTF8|ConvertFrom-Json
    $finalization=Get-Content -LiteralPath (Join-Path $batch 'finalization.json') -Raw -Encoding UTF8|ConvertFrom-Json
    $prerequisite=Get-Content -LiteralPath (Join-Path $batch 'prerequisites.json') -Raw -Encoding UTF8|ConvertFrom-Json
    Assert-OwnedStability ($plan.Schema -ceq 'r1c4b-owned-stability-plan/v1' -and $plan.BatchId -ceq $batchId -and $plan.ImplementationSHA -ceq $evidenceSHA -and $inventory.Schema -ceq 'r1c4b-owned-stability-inventory/v1' -and $statistics.Schema -ceq 'r1c4b-owned-stability-batch-statistics/v1' -and $priorSummary.Schema -ceq 'r1c4b-owned-stability-summary/v1' -and $finalization.Schema -ceq 'r1c4b-owned-stability-finalization/v1' -and $prerequisite.Schema -ceq 'r1c4b-fixh-prerequisites/v1') 'original control schema or implementation differs'
    Assert-OwnedStability ($inventory.BatchId -ceq $batchId -and $inventory.ExecutedHEAD -ceq $evidenceSHA -and $inventory.ImplementationSHA -ceq $evidenceSHA -and $statistics.ImplementationSHA -ceq $evidenceSHA -and $priorSummary.ImplementationSHA -ceq $evidenceSHA -and $inventory.PlanSHA256 -ceq $originalControl['plan.json'] -and $inventory.ProgressSHA256 -ceq $originalControl['progress.jsonl'] -and $statistics.PlanSHA256 -ceq $originalControl['plan.json'] -and $plan.PrerequisiteSHA256 -ceq $originalControl['prerequisites.json'] -and $inventory.PrerequisiteSHA256 -ceq $originalControl['prerequisites.json']) 'old batch hash/reference chain differs'
    Assert-OwnedStability ([IO.Path]::GetFullPath($inventory.PlanPath) -ceq (Join-Path $batch 'plan.json') -and [IO.Path]::GetFullPath($inventory.ProgressPath) -ceq (Join-Path $batch 'progress.jsonl') -and [IO.Path]::GetFullPath($plan.PrerequisitePath) -ceq (Join-Path $batch 'prerequisites.json')) 'old control paths differ'
    Assert-OwnedStability ($inventory.LoopResult -ceq 'ALL_PLANNED_PASS' -and $inventory.RetryCount -eq 0 -and $statistics.BatchId -ceq $batchId -and $finalization.BatchId -ceq $batchId -and $finalization.AggregateExitCode -eq 0 -and $priorSummary.BatchId -ceq $batchId -and $priorSummary.SuccessfulGesturesVerified -eq 100 -and $priorSummary.BatchResult -ceq 'PASS') 'old machine result/controls do not match preserved case'
    Test-OwnedStabilityPlan $plan.Steps
    Assert-OwnedStability (@($inventory.Attempts).Count -eq 100 -and @($inventory.Counts).Count -eq 8 -and @($statistics.Groups).Count -eq 8) 'fixed original 100/8 evidence missing'

    $state=[ordered]@{ExecutedHEAD=$evidenceSHA;Identity=@{}}
    $tracked=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
    foreach($configuration in @('Debug','Release')){
        $expected=$plan.RuntimeIdentity.$configuration
        $current=Get-ReliabilitySnapshot $repo $configuration
        Assert-OwnedStability (@($expected.PSObject.Properties).Count -eq 19 -and $current.Count -eq 19 -and (@($expected.PSObject.Properties.Name) -join ',') -ceq (@($current.Keys) -join ',')) 'old runtime inventory incomplete'
        $state.Identity[$configuration]=[ordered]@{}
        foreach($entry in $expected.PSObject.Properties){
            Assert-OwnedStability ($current[$entry.Name] -ceq $entry.Value) 'frozen runtime/source/binary bytes changed'
            $state.Identity[$configuration][$entry.Name]=$entry.Value
            if($entry.Name -notlike 'out/*'){
                if($tracked.ContainsKey($entry.Name)){Assert-OwnedStability ($tracked[$entry.Name] -ceq $entry.Value) 'two configuration source hashes conflict'}
                else{$tracked.Add($entry.Name,$entry.Value)}
            }
        }
    }
    $outerNames=@('scripts/run-r1c4b-owned-stability-gates.ps1','scripts/r1c4b-owned-stability-model.ps1','scripts/r1c4b-owned-stability-statistics.ps1','scripts/verify-r1c4b-owned-stability-summary.ps1','scripts/verify-r1c4b-fixh-prerequisites.ps1','scripts/package-r1c4b-owned-stability-evidence.ps1')
    Assert-OwnedStability ((@($plan.OuterIdentity.PSObject.Properties.Name) -join ',') -ceq ($outerNames -join ',')) 'original outer source inventory changed'
    foreach($entry in $tracked.GetEnumerator()){
        $gitHash=Get-FixHGitBlobSHA256 $evidenceSHA $entry.Key
        Assert-OwnedStability ($gitHash -ceq $entry.Value) 'original tracked runtime Git blob differs from plan'
        $result.OriginalGitBlobProofs+=@([pscustomobject]@{Path=$entry.Key;GitCommit=$evidenceSHA;SHA256=$gitHash;Role='RUNTIME'})
    }
    foreach($name in $outerNames){
        $gitHash=Get-FixHGitBlobSHA256 $evidenceSHA $name
        Assert-OwnedStability ($gitHash -ceq $plan.OuterIdentity.$name) 'original outer Git blob differs from frozen plan'
        $result.OriginalGitBlobProofs+=@([pscustomobject]@{Path=$name;GitCommit=$evidenceSHA;SHA256=$gitHash;Role='OUTER'})
    }

    Assert-OwnedStability ($prerequisite.Result -ceq 'PASS' -and $prerequisite.FixGImplementationSHA -ceq '5fc0ccb575b82940b78e4a8e4cbac916e12a2b4c' -and $prerequisite.ExpectedAbortVerified -eq 4 -and $prerequisite.NormalVerified -eq 4 -and $prerequisite.NewGuiRuns -eq 0 -and $prerequisite.NewInputCalls -eq 0 -and $prerequisite.HistoricalEvidenceCountsTowardFixH -is [bool] -and -not $prerequisite.HistoricalEvidenceCountsTowardFixH -and @($prerequisite.EvidenceFilesBefore).Count -eq 45 -and @($prerequisite.EvidenceFilesAfter).Count -eq 45 -and @($prerequisite.Files).Count -eq 36 -and @($prerequisite.HistoricalFilesBefore).Count -eq 11 -and @($prerequisite.HistoricalFilesAfter).Count -eq 11) 'original G/F/D/E prerequisites incomplete'
    foreach($collection in @($prerequisite.EvidenceFilesBefore,$prerequisite.EvidenceFilesAfter,$prerequisite.HistoricalFilesBefore,$prerequisite.HistoricalFilesAfter,$prerequisite.Files)){
        foreach($file in $collection){Add-FixHOriginal $file.Path $file.SHA256}
    }
    $result.OriginalPrerequisitesVerified=$true
    $progress=@(Get-Content -LiteralPath (Join-Path $batch 'progress.jsonl') -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json})
    Assert-OwnedStability ($progress.Count -eq 201 -and $progress[0].Schema -ceq 'r1c4b-owned-stability-progress/v1' -and $progress[0].Type -ceq 'plan_frozen' -and $progress[0].PlanSHA256 -ceq $originalControl['plan.json']) 'original progress header/count invalid'
    $freshAttempts=@();$measurements=@();$runIds=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    for($i=0;$i -lt 100;$i++){
        $step=$plan.Steps[$i];$item=$inventory.Attempts[$i]
        $started=$progress[1+2*$i];$finished=$progress[2+2*$i]
        Assert-OwnedStabilityNextStep $plan.Steps $freshAttempts $step
        Assert-OwnedStability ($runIds.Add([string]$step.RunId) -and $started.Type -ceq 'attempt_started' -and $finished.Type -ceq 'attempt_finished' -and $started.PlanSHA256 -ceq $originalControl['plan.json'] -and $finished.PlanSHA256 -ceq $originalControl['plan.json']) 'missing/duplicate/out-of-order progress'
        foreach($name in @('Number','GroupNumber','Phase','Configuration','Operation','Repetition','Mode','RunId')){
            Assert-OwnedStability ($item.$name -ceq $step.$name -and $started.Attempt.$name -ceq $step.$name -and $finished.Attempt.$name -ceq $step.$name) ('fixed step/progress mapping differs: '+$name)
        }
        Assert-OwnedStability ($step.Mode -ceq 'normal' -and $started.Attempt.ChildInvoked -is [bool] -and -not $started.Attempt.ChildInvoked -and $started.Attempt.Result -ceq 'NOT_RUN') 'attempt start does not precede child'
        Assert-OwnedStability (@($item.Artifacts).Count -eq 4 -and $item.ChildExitCode -eq 0 -and $finished.Attempt.Result -ceq 'PASS' -and $finished.Attempt.ChildExitCode -eq 0) 'old normal attempt incomplete'
        foreach($kind in @('Metadata','Probe','Pre','Post')){
            $hits=@($item.Artifacts|Where-Object Kind -ceq $kind)
            Assert-OwnedStability ($hits.Count -eq 1 -and $hits[0].Exists -is [bool] -and $hits[0].Exists) ('missing original '+$kind)
            Add-FixHOriginal $hits[0].Path $hits[0].SHA256
            $fromProgress=@($finished.Attempt.Artifacts|Where-Object Kind -ceq $kind)
            Assert-OwnedStability ($fromProgress.Count -eq 1 -and $fromProgress[0].Path -ceq $hits[0].Path -and $fromProgress[0].SHA256 -ceq $hits[0].SHA256 -and $fromProgress[0].Exists -is [bool] -and $fromProgress[0].Exists) 'finished artifact binding differs'
        }
        $metadata=Join-Path $item.RunDirectory 'run.metadata.json'
        Assert-OwnedStability ([IO.Path]::GetFullPath($item.MetadataPath) -ceq $metadata -and $item.MetadataSHA256 -ceq (@($item.Artifacts|Where-Object Kind -ceq 'Metadata')[0].SHA256)) 'metadata binding differs'
        $validated=Read-ReliabilityRun $metadata ([pscustomobject]@{Configuration=$step.Configuration;Operation=$step.Operation;Mode='normal'}) $step.RunId
        Assert-OwnedStabilityNormalVerdict $validated $step.Operation
        $rows=@(Get-Content -LiteralPath $validated.EvidencePath -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json})
        Assert-OwnedStability (@($rows|Where-Object type -ceq 'startup').Count -eq 1 -and @($rows|Where-Object type -ceq 'ENTER').Count -eq 1 -and @($rows|Where-Object type -ceq 'shutdown').Count -eq 1 -and $validated.ProbeInvoked -is [bool] -and $validated.ProbeInvoked -and $validated.RunnerExitCode -eq 0 -and $validated.EnvironmentPreExitCode -eq 0 -and $validated.EnvironmentPostExitCode -eq 0) 'raw startup/ENTER/metadata/pre/post do not prove normal attempt'
        $measure=Get-OwnedStabilityMeasurement -Rows $rows -Verdict $validated.IndependentVerdict -Phase $step.Phase -Configuration $step.Configuration -Operation $step.Operation
        Assert-OwnedStability ($measure.Status -ceq 'AVAILABLE' -and $measure.RunNonce -eq $validated.ValidatedNonce -and (($measure|ConvertTo-Json -Depth 32 -Compress) -ceq ($item.Measurement|ConvertTo-Json -Depth 32 -Compress))) 'fresh normal measurement differs from original'
        $fresh=New-OwnedStabilityAttempt $step
        $fresh.ChildInvoked=$true;$fresh.PreflightAttempted=$true;$fresh.ProbeInvoked=$true;$fresh.TargetGestureEntered=$true;$fresh.Result='PASS'
        Assert-FixHReconstructedFlags $item $finished.Attempt $fresh
        $freshAttempts+=@($fresh);$measurements+=@($measure)
        if($i -eq 99){
            $observed=Get-ReliabilityAttemptButtonState $validated $item.RunDirectory $step.RunId
            Assert-OwnedStability ($observed -ceq 'OBSERVED_UP' -and $item.FinalButtonState -ceq $observed) 'last original post button observation missing'
            $result.FinalButtonObservation=[pscustomobject]@{Source='ORIGINAL_ATTEMPT_100_READ_ONLY_POST';Path=$validated.EnvironmentPostPath;SHA256=$validated.EnvironmentPostSHA256;State=$observed;CurrentState='NOT_OBSERVED_NOW'}
        }
        $result.AttemptsVerified++;$result.NormalProofsVerified++
        if(($i+1)%20 -eq 0){Write-Host ("Fix H original read-only replay {0}/100 normal proofs PASS" -f ($i+1))}
    }
    $result.ProgressRecordsVerified=201;$result.RunIdsUnique=$runIds.Count;$result.NoncesUnique=$nonces.Count
    Assert-OwnedStability ($result.AttemptsVerified -eq 100 -and $result.NormalProofsVerified -eq 100 -and $runIds.Count -eq 100 -and $nonces.Count -eq 100) 'original 100 lifecycles are not unique and complete'
    $corrected=@();$corrections=@();$freshGroups=@()
    for($group=1;$group -le 8;$group++){
        $steps=@($plan.Steps|Where-Object GroupNumber -eq $group)
        $selected=@($measurements|Where-Object {$_.Phase -ceq $steps[0].Phase -and $_.Configuration -ceq $steps[0].Configuration -and $_.Operation -ceq $steps[0].Operation})
        $freshGroups+=@(Get-OwnedStabilityStatistics -Measurements $selected -Phase $steps[0].Phase -Configuration $steps[0].Configuration -Operation $steps[0].Operation -PlannedGestures $steps.Count)
    }
    $corrected=@(Get-OwnedStabilityCounts $plan.Steps $freshAttempts)
    Assert-OwnedStability ($corrected.Count -eq 8 -and $freshGroups.Count -eq 8 -and (($freshGroups|ConvertTo-Json -Depth 32 -Compress) -ceq ($statistics.Groups|ConvertTo-Json -Depth 32 -Compress))) 'seven metric statistics differ from fresh original raw evidence'
    for($i=0;$i -lt 8;$i++){
        Assert-FixHCorrectedGroup $inventory.Counts[$i] $corrected[$i] @($plan.Steps|Where-Object GroupNumber -eq ($i+1)).Count
        $corrections+=@([pscustomobject]@{GroupNumber=$i+1;Phase=$corrected[$i].Phase;Configuration=$corrected[$i].Configuration;Operation=$corrected[$i].Operation;ProbeInvocationsUnknownOld=$inventory.Counts[$i].ProbeInvocationsUnknown;ProbeInvocationsUnknownCorrected=0;TargetGesturesUnknownOld=$inventory.Counts[$i].TargetGesturesUnknown;TargetGesturesUnknownCorrected=0})
    }
    Assert-OwnedStability (($priorSummary.Counts|ConvertTo-Json -Depth 16 -Compress) -ceq ($inventory.Counts|ConvertTo-Json -Depth 16 -Compress) -and (@($corrections|Measure-Object ProbeInvocationsUnknownOld -Sum).Sum) -eq 100 -and (@($corrections|Measure-Object TargetGesturesUnknownOld -Sum).Sum) -eq 100) 'old bug signature differs from preserved machine summary'
    $result.HistoricalCountersMatch=$false;$result.CounterCorrections=$corrections;$result.CorrectedCounts=$corrected;$result.StatisticsGroups=$freshGroups
    $result.OriginalFilesVerifiedBefore=$originals.Count
    foreach($path in @($originals.Keys)){Assert-OwnedStability ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ceq $originals[$path]) ('original file changed during replay: '+$path)}
    $result.OriginalFilesVerifiedAfter=$originals.Count;$result.OriginalFilesUnchanged=$true
    Assert-FixHAuditorUnchanged $result.AuditorCodeBindings $auditor
    $result.Result='PASS';$result.OwnedFreeTakeoverGate='PASS';$result.RawInputTakeoverArchitecture='VALID_AT_OWNED_STAGE'
}catch{$result.Error=$_.Exception.Message;$result.Result='INVALID_EVIDENCE';$result.OwnedFreeTakeoverGate='NOT_PASSED';$result.RawInputTakeoverArchitecture='UNRESOLVED';Write-Host ('Fix H corrected replay STOP: '+$result.Error)}
if($artifactAuthorized){
    try{
        $null=New-Item -ItemType Directory -Path (Split-Path -Parent $artifactAbsolute) -Force
        Write-ReliabilityNewJson $artifactAbsolute $result
    }catch{Write-Error ('Fix H corrected replay artifact write failed: '+$_.Exception.Message) -ErrorAction Continue;exit 2}
}
Write-Host ("Fix H corrected replay result={0}; verified={1}/100; artifact={2}" -f $result.Result,$result.NormalProofsVerified,$artifactAbsolute)
if($result.Result -ceq 'PASS'){exit 0};exit 2
