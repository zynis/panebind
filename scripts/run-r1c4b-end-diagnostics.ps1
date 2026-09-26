[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [ValidateSet('Move','BottomResize')][string]$Operation='BottomResize',
    [string]$BuildDirectory,
    [ValidatePattern('^[a-f0-9]{32}$')][string]$RunId=([Guid]::NewGuid().ToString('N'))
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-end-diagnostics-validation.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $BuildDirectory){$BuildDirectory='out/r1c4b-live-magnet-'+$Configuration.ToLowerInvariant()}
$build=if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $repo $BuildDirectory}
$exe=Join-Path $build "src/platform/windows/$Configuration/panebind-magnet-takeover-probe.exe"
if(-not(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'Build the explicit Fix C test-only probe first'}
$root=Join-Path $repo 'uat/r1c4b-end-diagnostics'
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
    $dirty=$statusRows.Count -gt 0
    if($dirty){throw 'Require a clean implementation checkpoint before any test input'}
    $hash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    $gesture=if($Operation -ceq 'Move'){'move'}else{'bottom-resize'}
    Write-Host "Fix C $Configuration $Operation owned-only WinEvent END diagnostic: $path"
    & $exe --run-owned-end-diagnostics-test --gesture $gesture --evidence-log $path
    $code=$LASTEXITCODE
    $result=$null
    try{$result=Test-EndDiagnosticsOwnedEvidence -Path $path}
    catch{$result=[pscustomobject]@{Result='INVALID_EVIDENCE';Reasons=@($_.Exception.Message)}}
    $after=git rev-parse HEAD
    if($LASTEXITCODE -ne 0){throw 'After HEAD unavailable'}
    $afterStatusRows=@(git status --porcelain)
    if($LASTEXITCODE -ne 0){throw 'After working tree status unavailable'}
    $afterDirty=$afterStatusRows.Count -gt 0
    $afterHash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    $contracts=((Get-AutoField $result 'ForegroundContract') -ceq 'verified_global_foreground_v2' -and
        (Get-AutoField $result 'HandoffContract') -ceq 'winevent_end_barrier_v1' -and
        (Get-AutoField $result 'DiagnosticContract') -ceq 'structured_handoff_v1')
    $metadata=[ordered]@{
        Schema='r1c4b-end-diagnostics-run/v1';Stage='free_takeover';Configuration=$Configuration;Operation=$Operation;RunId=$RunId;EvidencePath=$path
        ExecutedHEAD=$sha;WorktreeDirty=$dirty;AfterHEAD=$after;AfterWorktreeDirty=$afterDirty
        BinarySHA256=$hash;AfterBinarySHA256=$afterHash
        LogSHA256=$(if(Test-Path -LiteralPath $path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}else{$null})
        ProbeExitCode=$code;Result=$result;CurrentContractsVerified=$contracts
        ImplementationUnchanged=($sha -ceq $after -and -not $dirty -and -not $afterDirty -and $hash -ceq $afterHash)
    }
    $metadata|ConvertTo-Json -Depth 16|Set-Content -LiteralPath ($prefix+'.metadata.json') -Encoding UTF8
    $metadata|ConvertTo-Json -Depth 16|Write-Host
    if(-not $metadata.ImplementationUnchanged -or -not $contracts -or $result.Result -ceq 'INVALID_EVIDENCE'){exit 1}
    if($code -ne 0 -or $result.Result -cne 'PASS'){exit 2}
    # One operation, one fresh process/source/receiver/hook. The caller owns
    # staged gates and must stop on the first failure; no retry or next stage.
    exit 0
}finally{Pop-Location}
