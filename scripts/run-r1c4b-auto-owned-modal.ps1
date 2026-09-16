[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [ValidateRange(1,20)][int]$Repetitions=20,
    [string]$BuildDirectory,
    [switch]$DevelopmentRun
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-auto-owned-modal-validation.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $BuildDirectory){$BuildDirectory=if($Configuration -eq 'Debug'){'out/r1c4b-live-magnet-debug'}else{'out/r1c4b-live-magnet-release'}}
$build=if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $repo $BuildDirectory}
$exe=Join-Path $build "src/platform/windows/$Configuration/panebind-magnet-auto-modal-probe.exe"
if(-not(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'Build the explicit auto modal probe first'}
if(-not $DevelopmentRun -and $Repetitions -ne 20){throw 'Formal owned gate requires 20 repetitions'}
$root=Join-Path $repo 'uat/r1c4b-auto-owned'
New-Item -ItemType Directory -Path $root -Force|Out-Null
$prefix=Join-Path $root ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-'+$Configuration+'-'+[Guid]::NewGuid().ToString('N'))
Push-Location $repo
try {
    git check-ignore -- ($prefix+'.jsonl')|Out-Null;if($LASTEXITCODE -ne 0){throw 'Evidence is not ignored'}
    $sha=git rev-parse HEAD;$dirty=@(git status --porcelain).Count -gt 0
    $hash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    $runs=@()
    for($i=1;$i -le $Repetitions;++$i){
        $path=$prefix+'-'+$i+'.jsonl';Write-Host "Owned modal $Configuration $i/$Repetitions : $path"
        & $exe --run-owned-input-test --evidence-log $path
        $code=$LASTEXITCODE
        try {$result=Test-AutoModalEvidence $path}catch{$result=[pscustomobject]@{Result='FAIL';Move='UNKNOWN';Resize='UNKNOWN';Reasons=@($_.Exception.Message)}}
        $run=[pscustomobject]@{Repetition=$i;Path=$path;SHA256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash;ExitCode=$code;Result=$result.Result;Move=$result.Move;Resize=$result.Resize;Reasons=$result.Reasons}
        $runs+=$run;$run|ConvertTo-Json -Depth 6 -Compress|Write-Host
        if($code -ne 0 -or $result.Result -ne 'CAPTURED'){break} # no retry / further input after failure
    }
    $unchanged=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ceq $hash
    $captured=@($runs|Where-Object Result -eq CAPTURED).Count
    $consistent=@($runs.Move|Select-Object -Unique).Count -eq 1 -and @($runs.Resize|Select-Object -Unique).Count -eq 1
    $report=[ordered]@{Schema='r1c4b-auto-owned-aggregate/v1';Configuration=$Configuration;DevelopmentRun=[bool]$DevelopmentRun;StartingHEAD=$sha;WorktreeDirty=$dirty;BinarySHA256=$hash;BinaryUnchanged=$unchanged;Requested=$Repetitions;Attempted=$runs.Count;Captured=$captured;Consistent=$consistent;DriverGate=$(if($unchanged -and $captured -eq $Repetitions -and $consistent){'PASS'}else{'FAIL_OR_BLOCKED'});Runs=$runs}
    $report|ConvertTo-Json -Depth 12|Set-Content -LiteralPath ($prefix+'.aggregate.json') -Encoding UTF8
    Write-Host "AGGREGATE: $prefix.aggregate.json"
    if($report.DriverGate -ne 'PASS'){exit 2}
} finally {Pop-Location}
