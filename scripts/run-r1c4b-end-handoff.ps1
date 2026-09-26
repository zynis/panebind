[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [ValidateSet('Move','BottomResize')][string]$Operation='Move',
    [string]$BuildDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-end-handoff-validation.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $BuildDirectory){$BuildDirectory='out/r1c4b-live-magnet-'+$Configuration.ToLowerInvariant()}
$build=if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $repo $BuildDirectory}
$exe=Join-Path $build "src/platform/windows/$Configuration/panebind-magnet-takeover-probe.exe"
if(-not(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'Build the explicit owned END-barrier probe first'}
$root=Join-Path $repo 'uat/r1c4b-end-handoff'
New-Item -ItemType Directory -Path $root -Force|Out-Null
$prefix=Join-Path $root ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-'+$Configuration+'-'+$Operation+'-'+[Guid]::NewGuid().ToString('N'))
$path=$prefix+'.jsonl'
Push-Location $repo
try{
    git check-ignore -- $path|Out-Null
    if($LASTEXITCODE -ne 0){throw 'Evidence must remain ignored'}
    $sha=git rev-parse HEAD
    if($LASTEXITCODE -ne 0){throw 'HEAD unavailable'}
    $dirty=@(git status --porcelain).Count -gt 0
    $hash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    $gesture=if($Operation -ceq 'Move'){'move'}else{'bottom-resize'}
    Write-Host "Fix B $Configuration $Operation owned-only END-barrier probe: $path"
    & $exe --run-owned-takeover-test --gesture $gesture --evidence-log $path
    $code=$LASTEXITCODE
    $result=$null
    try{$result=Test-EndHandoffOwnedEvidence -Path $path}
    catch{$result=[pscustomobject]@{Result='INVALID_EVIDENCE';Reasons=@($_.Exception.Message)}}
    $after=git rev-parse HEAD
    $afterHash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    $contractVerified=((Get-AutoField $result 'ForegroundContract') -ceq 'verified_global_foreground_v2' -and (Get-AutoField $result 'HandoffContract') -ceq 'end_barrier_v1')
    $metadata=[ordered]@{
        Schema='r1c4b-end-handoff-run/v1';Stage='free_takeover';Configuration=$Configuration;Operation=$Operation
        ExecutedHEAD=$sha;WorktreeDirty=$dirty;AfterHEAD=$after
        BinarySHA256=$hash;AfterBinarySHA256=$afterHash
        LogSHA256=$(if(Test-Path -LiteralPath $path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}else{$null})
        ProbeExitCode=$code;Result=$result;CurrentContractsVerified=$contractVerified
        ImplementationUnchanged=($sha -ceq $after -and $hash -ceq $afterHash)
    }
    $metadata|ConvertTo-Json -Depth 12|Set-Content -LiteralPath ($prefix+'.metadata.json') -Encoding UTF8
    $metadata|ConvertTo-Json -Depth 12|Write-Host
    if(-not $metadata.ImplementationUnchanged -or -not $contractVerified -or $result.Result -ceq 'INVALID_EVIDENCE'){exit 1}
    if($code -ne 0 -or $result.Result -cne 'PASS'){exit 2}
    # This runner executes one fresh owned gesture only. It never starts the
    # next gesture/repetition, Explorer, product input, or human UAT.
    exit 0
}finally{Pop-Location}
