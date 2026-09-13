[CmdletBinding(DefaultParameterSetName='Run')]
param(
    [Parameter(ParameterSetName='Run')][string] $BuildDirectory='out/r1c4a-debug',
    [Parameter(ParameterSetName='Run')][switch] $IndependentReviewPassed,
    [Parameter(Mandatory=$true,ParameterSetName='Validate')][string] $ValidateEvidencePath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4a-evidence-validation.ps1')
if($PSCmdlet.ParameterSetName -eq 'Validate') {
    Test-C4AEvidence $ValidateEvidencePath | ConvertTo-Json -Depth 8
    exit 0
}
if(-not $IndependentReviewPassed){throw 'STOP: C4A independent review must PASS before human UAT. No harness launched.'}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Push-Location $repo
try {
    $sha=git rev-parse HEAD
    if($LASTEXITCODE -ne 0){throw 'Git HEAD unavailable'}
    $dirty=git status --porcelain
    if($dirty){throw 'UAT requires a clean implementation worktree'}
    $build=if([IO.Path]::IsPathRooted($BuildDirectory)){[IO.Path]::GetFullPath($BuildDirectory)}else{[IO.Path]::GetFullPath((Join-Path $repo $BuildDirectory))}
    $exe=Join-Path $build 'src/platform/windows/Debug/panebind-explorer-group-harness.exe'
    if(-not (Test-Path -LiteralPath $exe -PathType Leaf)){throw 'Debug C4A harness missing; build/test before UAT'}
    $evidence=Join-Path $repo 'uat/r1c4a'
    New-Item -ItemType Directory -Path $evidence -Force | Out-Null
    $prefix=Join-Path $evidence ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-'+[Guid]::NewGuid().ToString('N'))
    $log=$prefix+'.jsonl'
    git check-ignore -- $log | Out-Null
    if($LASTEXITCODE -ne 0){throw 'UAT evidence must be ignored'}
    $binaryHash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    Write-Host "C4A implementation: $sha"
    Write-Host "Evidence: $log"
    & $exe --interactive-consent-test --evidence-log $log
    $harnessExit=$LASTEXITCODE
    $after=git rev-parse HEAD
    $afterHash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    $afterDirty=git status --porcelain
    [ordered]@{ImplementationSHA=$sha;AfterSHA=$after;HarnessSHA256=$binaryHash;AfterHarnessSHA256=$afterHash;AfterDirty=(@($afterDirty).Count -gt 0);HarnessExitCode=$harnessExit;BuildDirectory=$build;Configuration='Debug';IndependentReviewPassed=$true} | ConvertTo-Json | Set-Content -LiteralPath ($prefix+'.metadata.json') -Encoding UTF8
    if($sha -cne $after){throw 'Implementation changed during UAT'}
    if($afterDirty -or $binaryHash -cne $afterHash){throw 'Worktree or harness binary changed during UAT'}
    if($harnessExit -ne 0){throw "Harness BLOCKED (exit $harnessExit); evidence retained. Do not seal."}
    Test-C4AEvidence $log | ConvertTo-Json -Depth 8
} finally {Pop-Location}
