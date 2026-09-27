[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-native-abort-validation.ps1')
# Import ONLY the fixed helpers from our reviewed single-run file, never its
# program. There is no definitions-only/live bypass option on that CLI.
$single=Join-Path $PSScriptRoot 'run-r1c4b-input-reliability.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($single,[ref]$tokens,[ref]$errors)
if(@($errors).Count){throw 'Single-run helper source does not parse'}
foreach($name in @('Get-ReliabilityField','Test-ReliabilityTrue','Test-ReliabilityFalse','Get-ReliabilitySnapshot','Test-ReliabilitySameSnapshot','Test-ReliabilityEnvironmentContext','Get-ReliabilityButtonState','Test-ReliabilityEnvironment','Get-ReliabilityExitCode','Write-ReliabilityNewJson')){
    $nodes=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name},$true))
    if($nodes.Count -ne 1){throw 'Single-run fixed helper missing/duplicated'}
    . ([scriptblock]::Create($nodes[0].Extent.Text))
}
function Get-ReliabilityPlan{
    return @(
        [pscustomobject]@{Number=1;Configuration='Debug';Operation='Move';Mode='controlled-abort'},
        [pscustomobject]@{Number=2;Configuration='Debug';Operation='BottomResize';Mode='controlled-abort'},
        [pscustomobject]@{Number=3;Configuration='Debug';Operation='Move';Mode='normal'},
        [pscustomobject]@{Number=4;Configuration='Debug';Operation='BottomResize';Mode='normal'},
        [pscustomobject]@{Number=5;Configuration='Release';Operation='Move';Mode='controlled-abort'},
        [pscustomobject]@{Number=6;Configuration='Release';Operation='BottomResize';Mode='controlled-abort'},
        [pscustomobject]@{Number=7;Configuration='Release';Operation='Move';Mode='normal'},
        [pscustomobject]@{Number=8;Configuration='Release';Operation='BottomResize';Mode='normal'}
    )
}
function Assert-ReliabilityGate([bool]$Value,[string]$Reason){if(-not $Value){throw $Reason}}
function Get-ReliabilityAttemptButtonState($Value,[string]$Directory,[string]$RunId){
    # Diagnostic only. Never carry a preceding fixture's UP into this failed
    # attempt and never use an embedded metadata state as input authority.
    try{
        if($Value.Schema -cne 'r1c4b-input-reliability-run/v1' -or $Value.RunId -cne $RunId -or -not $Directory.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or -not $Directory.EndsWith('-'+$RunId,[StringComparison]::Ordinal)){return 'UNKNOWN'}
        $part=if(Test-ReliabilityTrue $Value.ProbeInvoked){'Post'}elseif(Test-ReliabilityFalse $Value.ProbeInvoked){'Pre'}else{return 'UNKNOWN'}
        $environmentPath=Join-Path $Directory ('environment-'+$part.ToLowerInvariant()+'.json')
        $recordedPath=Get-ReliabilityField $Value ('Environment'+$part+'Path');$hash=Get-ReliabilityField $Value ('Environment'+$part+'SHA256')
        if($null -eq $recordedPath -or [IO.Path]::GetFullPath($recordedPath) -cne $environmentPath -or $hash -cne (Get-FileHash -LiteralPath $environmentPath -Algorithm SHA256).Hash){return 'UNKNOWN'}
        $env=Get-Content -LiteralPath $environmentPath -Raw -Encoding UTF8|ConvertFrom-Json
        $observed=Get-ReliabilityButtonState $env
        if($hash -cne (Get-FileHash -LiteralPath $environmentPath -Algorithm SHA256).Hash){return 'UNKNOWN'}
        return $observed
    }catch{return 'UNKNOWN'}
}
function Read-ReliabilityRun([string]$Path,$Step,[string]$RunId){
    $metadataHash=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    $m=Get-Content -LiteralPath $Path -Raw -Encoding UTF8|ConvertFrom-Json
    $directory=[IO.Path]::GetFullPath((Split-Path -Parent $Path))
    $canonical=if($Step.Mode -ceq 'controlled-abort'){'controlled_abort'}else{'normal'}
    Assert-ReliabilityGate ($directory.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -and $directory.EndsWith('-'+$RunId,[StringComparison]::Ordinal) -and [IO.Path]::GetFileName($Path) -ceq 'run.metadata.json') 'Run metadata directory/identity mismatch'
    Assert-ReliabilityGate ($m.Schema -ceq 'r1c4b-input-reliability-run/v1' -and $m.RunId -ceq $RunId -and $m.Configuration -ceq $Step.Configuration -and $m.Operation -ceq $Step.Operation -and $m.TestMode -ceq $canonical -and $m.ExpectedHEAD -ceq $state.ExecutedHEAD -and $m.ExecutedHEAD -ceq $state.ExecutedHEAD -and $m.AfterHEAD -ceq $state.ExecutedHEAD -and (Test-ReliabilityTrue $m.ProbeInvoked) -and ($m.RunnerExitCode -is [int] -or $m.RunnerExitCode -is [long]) -and $m.RunnerExitCode -eq 0 -and (Test-ReliabilityTrue $m.ImplementationUnchanged)) 'Run/configuration/checkpoint is not a complete validated fixture'
    $current=Get-ReliabilitySnapshot $repo $Step.Configuration
    Assert-ReliabilityGate (@($m.BeforeIdentity.PSObject.Properties).Count -eq $current.Count -and @($m.AfterIdentity.PSObject.Properties).Count -eq $current.Count) 'Missing implementation hash inventory'
    foreach($name in $current.Keys){
        Assert-ReliabilityGate ((Get-ReliabilityField $m.BeforeIdentity $name) -ceq $current[$name] -and (Get-ReliabilityField $m.AfterIdentity $name) -ceq $current[$name] -and $current[$name] -ceq $state.Identity[$Step.Configuration][$name]) 'Source/validator/import/binary identity mismatch'
    }
    foreach($part in @('Pre','Post')){
        $environmentPath=Get-ReliabilityField $m ('Environment'+$part+'Path')
        $expected=Join-Path $directory ('environment-'+$part.ToLowerInvariant()+'.json')
        $environmentExit=Get-ReliabilityField $m ('Environment'+$part+'ExitCode')
        Assert-ReliabilityGate ([IO.Path]::GetFullPath($environmentPath) -ceq $expected -and ($environmentExit -is [int] -or $environmentExit -is [long]) -and $environmentExit -eq 0 -and (Test-ReliabilityTrue (Get-ReliabilityField $m ('Environment'+$part+'Ready'))) -and (Get-ReliabilityField $m ('Environment'+$part+'SHA256')) -ceq (Get-FileHash -LiteralPath $environmentPath -Algorithm SHA256).Hash) 'Environment evidence identity/READY mismatch'
        $env=Get-Content -LiteralPath $environmentPath -Raw -Encoding UTF8|ConvertFrom-Json
        Assert-ReliabilityGate (Test-ReliabilityEnvironment $env) 'Input release is not reliably observed; no next run'
    }
    $log=Join-Path $directory 'probe.jsonl'
    Assert-ReliabilityGate ([IO.Path]::GetFullPath($m.EvidencePath) -ceq $log -and $m.LogSHA256 -ceq (Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash) 'Raw evidence hash/path mismatch'
    $verdict=Test-FixFInputReliabilityEvidence -Path $log
    Assert-ReliabilityGate ((Get-ReliabilityExitCode $Step.Operation $Step.Mode $m.ProbeExitCode $verdict $true $true) -eq 0) 'Fresh independent v5 fixture verification failed'
    Assert-ReliabilityGate ($m.Result.FixtureResult -ceq $verdict.FixtureResult -and $m.Result.GestureResult -ceq $verdict.GestureResult -and $m.Result.CleanupResult -ceq $verdict.CleanupResult -and $m.Result.TakeoverAcceptance -ceq $verdict.TakeoverAcceptance) 'Cached verdict differs from independent raw proof'
    $rows=@(Get-Content -LiteralPath $log -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json})
    Assert-ReliabilityGate ($rows.Count -gt 1 -and $rows[0].run_nonce -gt 0 -and $nonces.Add([string]$rows[0].run_nonce)) 'Fixture nonce missing/reused'
    Assert-ReliabilityGate ($m.LogSHA256 -ceq (Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash -and $metadataHash -ceq (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash) 'Evidence changed during independent validation'
    $m|Add-Member -NotePropertyName ValidatedNonce -NotePropertyValue $rows[0].run_nonce
    return $m
}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'));$root=Join-Path $repo 'uat/r1c4b-fixf'
$summary=Join-Path $root ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-bounded-eight-'+[Guid]::NewGuid().ToString('N')+'.json')
$state=[ordered]@{Schema='r1c4b-input-reliability-eight/v1';ExecutedHEAD=$null;Identity=@{};Result='NOT_RUN';Runs=@();FirstUnexpectedFailure=$null;ExpectedAbortPassed=0;NormalPassed=0;Formal='NOT_RUN';FinalButtonState='UNKNOWN'}
$nonces=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
Push-Location $repo
try{
    $state.ExecutedHEAD=git rev-parse HEAD;$headCode=$LASTEXITCODE;$status=@(git status --porcelain)
    Assert-ReliabilityGate ($headCode -eq 0 -and $LASTEXITCODE -eq 0 -and $status.Count -eq 0) 'Require a clean implementation checkpoint'
    git check-ignore -- $summary|Out-Null;Assert-ReliabilityGate ($LASTEXITCODE -eq 0) 'Fix F inventory must remain ignored'
    New-Item -ItemType Directory -Path $root -Force|Out-Null
    foreach($configuration in @('Debug','Release')){$state.Identity[$configuration]=Get-ReliabilitySnapshot $repo $configuration}
    $state.Result='RUNNING'
    foreach($step in @(Get-ReliabilityPlan)){
        $runId=[Guid]::NewGuid().ToString('N')
        $item=[pscustomobject]@{Number=$step.Number;Configuration=$step.Configuration;Operation=$step.Operation;Mode=$step.Mode;RunId=$runId;ChildExitCode=$null;ProbeExitCode=$null;MetadataPath=$null;MetadataSHA256=$null;LogSHA256=$null;FixtureResult=$null;GestureResult=$null;CleanupResult=$null;TakeoverAcceptance=$null;FinalButtonState='UNKNOWN';Passed=$false;Reasons=@();Error=$null}
        $state.Runs+=@($item)
        $currentHEAD=git rev-parse HEAD;$currentHeadCode=$LASTEXITCODE;$currentStatus=@(git status --porcelain);$currentStatusCode=$LASTEXITCODE
        Assert-ReliabilityGate ($currentHeadCode -eq 0 -and $currentStatusCode -eq 0 -and $currentStatus.Count -eq 0 -and $currentHEAD -ceq $state.ExecutedHEAD) 'Checkpoint changed before next child; no environment/native execution'
        Assert-ReliabilityGate (Test-ReliabilitySameSnapshot $state.Identity[$step.Configuration] (Get-ReliabilitySnapshot $repo $step.Configuration)) 'Implementation changed before next child; no environment/native execution'
        $output=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $single -Configuration $step.Configuration -Operation $step.Operation -Mode $step.Mode -RunId $runId -ExpectedHEAD $state.ExecutedHEAD 2>&1)
        $item.ChildExitCode=$LASTEXITCODE
        $directories=@(Get-ChildItem -LiteralPath $root -Directory -Filter ('*-'+$runId))
        $paths=@($directories|ForEach-Object {Join-Path $_.FullName 'run.metadata.json'}|Where-Object {Test-Path -LiteralPath $_ -PathType Leaf})
        if($paths.Count -eq 1){
            $item.MetadataPath=$paths[0];$item.MetadataSHA256=(Get-FileHash -LiteralPath $paths[0] -Algorithm SHA256).Hash;$attempt=Get-Content -LiteralPath $paths[0] -Raw -Encoding UTF8|ConvertFrom-Json
            $item.ProbeExitCode=$attempt.ProbeExitCode;$item.Reasons=@($attempt.Reason)+@(Get-ReliabilityField $attempt 'NativeError')+@(Get-ReliabilityField $attempt.Result 'Reasons')
            $attemptLog=Join-Path (Split-Path -Parent $paths[0]) 'probe.jsonl'
            if(Test-Path -LiteralPath $attemptLog -PathType Leaf){$item.LogSHA256=(Get-FileHash -LiteralPath $attemptLog -Algorithm SHA256).Hash}
            foreach($name in @('FixtureResult','GestureResult','CleanupResult','TakeoverAcceptance')){$item.$name=Get-ReliabilityField $attempt.Result $name}
            $item.FinalButtonState=Get-ReliabilityAttemptButtonState $attempt ([IO.Path]::GetFullPath((Split-Path -Parent $paths[0]))) $runId
        }
        if($item.ChildExitCode -ne 0 -or $paths.Count -ne 1){$item.Error="child_exit=$($item.ChildExitCode); metadata_count=$($paths.Count)";$output|Write-Output;throw 'First unexpected failure; STOP entire bounded plan; no retry'}
        $m=Read-ReliabilityRun $paths[0] $step $runId
        $item.MetadataSHA256=(Get-FileHash -LiteralPath $paths[0] -Algorithm SHA256).Hash;$item.LogSHA256=$m.LogSHA256
        foreach($name in @('FixtureResult','GestureResult','CleanupResult','TakeoverAcceptance')){$item.$name=Get-ReliabilityField $m.Result $name}
        $item.Passed=$true
        if($step.Mode -ceq 'controlled-abort'){++$state.ExpectedAbortPassed}else{++$state.NormalPassed}
        $item.FinalButtonState='OBSERVED_UP';$state.FinalButtonState=$item.FinalButtonState
        Write-Host "Fix F bounded $($step.Number)/8 $($step.Configuration) $($step.Operation) $($step.Mode): $($item.FixtureResult)"
    }
    $state.Result='PASS'
}catch{
    $state.Result='STOPPED';$state.Reason=$_.Exception.Message
    if(@($state.Runs).Count){$state.FirstUnexpectedFailure=$state.Runs[-1];$state.FirstUnexpectedFailure.Error=$(if($state.FirstUnexpectedFailure.Error){$state.FirstUnexpectedFailure.Error+'; '+$_.Exception.Message}else{$_.Exception.Message})}
    # A prior run's observed UP is not a claim about a failed current run.
    $state.FinalButtonState=if(@($state.Runs).Count){$state.Runs[-1].FinalButtonState}else{'UNKNOWN'}
    Write-Host "STOP: $($state.Reason)"
}finally{
    if(Test-Path -LiteralPath $root -PathType Container){Write-ReliabilityNewJson $summary $state;Write-Host "Fix F bounded inventory: $summary"}
    Pop-Location
}
if($state.Result -cne 'PASS'){exit 2}
exit 0
