[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [string]$BuildDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-takeover-owned-validation.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $BuildDirectory){$BuildDirectory='out/r1c4b-live-magnet-'+$Configuration.ToLowerInvariant()}
$build=if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $repo $BuildDirectory}
$exe=Join-Path $build "src/platform/windows/$Configuration/panebind-magnet-takeover-probe.exe"
if(-not(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'Build the explicit takeover research probe first'}
$root=Join-Path $repo 'uat/r1c4b-takeover-owned'
New-Item -ItemType Directory -Path $root -Force|Out-Null
$prefix=Join-Path $root ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-'+$Configuration+'-'+[Guid]::NewGuid().ToString('N'))
$path=$prefix+'.jsonl'
Push-Location $repo
try{
    git check-ignore -- $path|Out-Null
    if($LASTEXITCODE -ne 0){throw 'Evidence must remain ignored'}
    $sha=git rev-parse HEAD
    if($LASTEXITCODE -ne 0){throw 'HEAD unavailable'}
    $dirty=@(git status --porcelain).Count -gt 0
    $hash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    Write-Host "Pivot 1 $Configuration cancel-only research: $path"
    & $exe --run-owned-cancel-test --evidence-log $path
    $code=$LASTEXITCODE
    $result=$null
    try{$result=Test-TakeoverOwnedEvidence -Path $path}
    catch{$result=[pscustomobject]@{Result='INVALID_EVIDENCE';Reasons=@($_.Exception.Message)}}
    $after=git rev-parse HEAD
    $afterHash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    $metadata=[ordered]@{
        Schema='r1c4b-takeover-owned-run/v1';Stage='cancel_only';Configuration=$Configuration
        ExecutedHEAD=$sha;WorktreeDirty=$dirty;AfterHEAD=$after
        BinarySHA256=$hash;AfterBinarySHA256=$afterHash
        LogSHA256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        ProbeExitCode=$code;Result=$result
        ImplementationUnchanged=($sha -ceq $after -and $hash -ceq $afterHash)
    }
    $metadata|ConvertTo-Json -Depth 12|Set-Content -LiteralPath ($prefix+'.metadata.json') -Encoding UTF8
    $metadata|ConvertTo-Json -Depth 12|Write-Host
    if(-not $metadata.ImplementationUnchanged -or $result.Result -ceq 'INVALID_EVIDENCE'){exit 1}
    # One observation is never relabeled as the 20/20 free takeover gate.
    if($code -ne 0 -or $result.Result -cne 'CAPTURED'){exit 2}
}finally{Pop-Location}
