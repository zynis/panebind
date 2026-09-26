[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [ValidateSet('Move','BottomResize')][string]$Operation='BottomResize',
    [string]$BuildDirectory,
    [ValidatePattern('^[a-f0-9]{32}$')][string]$RunId=([Guid]::NewGuid().ToString('N'))
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-input-isolation-validation.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $BuildDirectory){$BuildDirectory='out/r1c4b-live-magnet-'+$Configuration.ToLowerInvariant()}
$build=if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $repo $BuildDirectory}
$exe=Join-Path $build "src/platform/windows/$Configuration/panebind-magnet-takeover-probe.exe"
if(-not(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'Build the explicit Fix D test-only probe first'}
$root=Join-Path $repo 'uat/r1c4b-input-isolation'
New-Item -ItemType Directory -Path $root -Force|Out-Null
$prefix=Join-Path $root ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-'+$Configuration+'-'+$Operation+'-'+$RunId)
$path=$prefix+'.jsonl'
Push-Location $repo
try{
    git check-ignore -- $path|Out-Null
    if($LASTEXITCODE -ne 0){throw 'Evidence must remain ignored'}
    $sha=git rev-parse HEAD
    if($LASTEXITCODE -ne 0){throw 'HEAD unavailable'}
    $statusRows=@(git status --porcelain)
    if($LASTEXITCODE -ne 0){throw 'Working tree status unavailable'}
    if($statusRows.Count){throw 'Require a clean implementation checkpoint before any test input'}
    $hash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    $gesture=if($Operation -ceq 'Move'){'move'}else{'bottom-resize'}
    Write-Host "Fix D $Configuration $Operation owned-only separated-authority probe: $path"
    & $exe --run-owned-input-isolation-test --gesture $gesture --evidence-log $path
    $code=$LASTEXITCODE
    try{$result=Test-InputIsolationOwnedEvidence -Path $path}
    catch{$result=[pscustomobject]@{Result='INVALID_EVIDENCE';Reasons=@($_.Exception.Message)}}
    $after=git rev-parse HEAD
    if($LASTEXITCODE -ne 0){throw 'After HEAD unavailable'}
    $afterRows=@(git status --porcelain)
    if($LASTEXITCODE -ne 0){throw 'After working tree status unavailable'}
    $afterHash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    $contracts=((Get-AutoField $result 'ForegroundContract') -ceq 'verified_global_foreground_v2' -and
        (Get-AutoField $result 'HandoffContract') -ceq 'winevent_end_barrier_v1' -and
        (Get-AutoField $result 'DiagnosticContract') -ceq 'separated_authority_v1' -and
        (Get-AutoField $result 'AuthorityContract') -ceq 'product_gesture_v1' -and
        (Get-AutoField $result 'IsolationContract') -ceq 'post_end_shield_v1' -and
        (Get-AutoField $result 'Operation') -ceq $Operation)
    $metadata=[ordered]@{
        Schema='r1c4b-input-isolation-run/v1';Stage='free_takeover';Configuration=$Configuration;Operation=$Operation;RunId=$RunId;EvidencePath=$path
        ExecutedHEAD=$sha;WorktreeDirty=$false;AfterHEAD=$after;AfterWorktreeDirty=($afterRows.Count -gt 0)
        BinarySHA256=$hash;AfterBinarySHA256=$afterHash
        LogSHA256=$(if(Test-Path -LiteralPath $path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}else{$null})
        ProbeExitCode=$code;Result=$result;CurrentContractsVerified=$contracts
        ImplementationUnchanged=($sha -ceq $after -and $afterRows.Count -eq 0 -and $hash -ceq $afterHash)
    }
    $metadata|ConvertTo-Json -Depth 20|Set-Content -LiteralPath ($prefix+'.metadata.json') -Encoding UTF8
    $metadata|ConvertTo-Json -Depth 20|Write-Host
    if(-not $metadata.ImplementationUnchanged -or -not $contracts -or $result.Result -ceq 'INVALID_EVIDENCE'){exit 1}
    if($code -ne 0 -or $result.Result -cne 'PASS'){exit 2}
    # Exactly one fresh operation. No implicit Move, repetition or retry.
    exit 0
}finally{Pop-Location}
