[CmdletBinding(DefaultParameterSetName='Run')]
param(
    [Parameter(ParameterSetName='Run')][string]$BuildDirectory='out/r1c4b-live-magnet-debug',
    [Parameter(ParameterSetName='Run')][switch]$IndependentReviewPassed,
    [Parameter(Mandatory=$true,ParameterSetName='Validate')][string]$ValidateEvidencePath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-evidence-validation.ps1')
if($PSCmdlet.ParameterSetName -eq 'Validate'){
    $result=Test-C4BEvidence $ValidateEvidencePath;$result|ConvertTo-Json -Depth 12
    if($result.Result -ceq 'BLOCKED'){exit 2};exit 0
}
if(-not $IndependentReviewPassed){throw 'STOP: independent C4B review must PASS. No harness launched.'}
# Amendment 001 hard gate. Architecture is unresolved and no validated real
# Explorer aggregate artifact exists. No flag or claimed PASS may bypass this.
# Replace this fail-closed guard only with the independently recomputing,
# SHA-bound artifact verifier after the architecture gate authorizes it.
throw 'STOP: automated Explorer gate has not passed (architecture unresolved). No harness launched.'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Push-Location $repo
try {
    $sha=git rev-parse HEAD;if($LASTEXITCODE -ne 0){throw 'Git HEAD unavailable'}
    if(git status --porcelain){throw 'C4B UAT requires a clean implementation worktree'}
    $build=if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $repo $BuildDirectory}
    $exe=Join-Path $build 'src/platform/windows/Debug/panebind-explorer-live-magnet-harness.exe'
    if(-not(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'C4B Debug harness missing'}
    $identity=(& $exe --build-identity)|ConvertFrom-Json
    if($LASTEXITCODE -ne 0 -or -not $identity.debug -or $identity.implementation_sha -cne $sha){throw 'Debug binary does not match frozen HEAD; rebuild after commit'}
    $hash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    $evidence=Join-Path $repo 'uat/r1c4b';New-Item -ItemType Directory -Path $evidence -Force|Out-Null
    $prefix=Join-Path $evidence ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-'+[Guid]::NewGuid().ToString('N'))
    $log=$prefix+'.jsonl';git check-ignore -- $log|Out-Null
    if($LASTEXITCODE -ne 0){throw 'Evidence must be Git-ignored'}
    Write-Host "C4B implementation: $sha"
    Write-Host "Local evidence: $log"
    & $exe --interactive-consent-test --evidence-log $log --implementation-sha $sha
    $harnessExit=$LASTEXITCODE
    $after=git rev-parse HEAD;$dirty=@(git status --porcelain).Count -gt 0
    $afterHash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    [ordered]@{ImplementationSHA=$sha;AfterSHA=$after;BinarySHA256=$hash;AfterBinarySHA256=$afterHash;AfterDirty=$dirty;HarnessExitCode=$harnessExit;Configuration='Debug';IndependentReviewPassed=$true}|ConvertTo-Json|Set-Content -LiteralPath ($prefix+'.metadata.json') -Encoding UTF8
    if($sha -cne $after -or $hash -cne $afterHash -or $dirty){throw 'Implementation or binary changed during UAT; no seal'}
    $result=Test-C4BEvidence $log;$result|ConvertTo-Json -Depth 12
    if($result.Result -ceq 'BLOCKED'){if($harnessExit -ne 2){throw 'BLOCKED exit mismatch'};exit 2}
    if($harnessExit -ne 0){throw 'Harness did not complete successfully'}
} finally {Pop-Location}
