[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PassedResizeMetadata,
    [Parameter(Mandatory=$true)][string]$PassedMoveMetadata
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-end-diagnostics-validation.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Join-Path $repo 'uat/r1c4b-end-diagnostics'
$summaryPath=Join-Path $root ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-gates-'+[Guid]::NewGuid().ToString('N')+'.json')
$state=[ordered]@{Schema='r1c4b-end-diagnostics-gates/v1';Result='NOT_RUN';ExecutedHEAD=$null;InitialEvidence=@();DebugSmoke=0;ReleaseSmoke=0;DebugFormal=0;ReleaseFormal=0;Runs=@();FirstFailure=$null}
function Assert-Gate([bool]$Condition,[string]$Reason){if(-not $Condition){throw $Reason}}
function Read-GateMetadata([string]$Path,[string]$Configuration,[string]$Operation){
    $full=[IO.Path]::GetFullPath($Path)
    Assert-Gate ($full.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Initial/run metadata must be local owned evidence'
    $m=Get-Content -LiteralPath $full -Encoding UTF8|ConvertFrom-Json
    Assert-Gate ($m.Schema -ceq 'r1c4b-end-diagnostics-run/v1' -and $m.Configuration -ceq $Configuration -and $m.Operation -ceq $Operation) 'Wrong evidence mode/configuration/operation'
    Assert-Gate ($m.ExecutedHEAD -ceq $state.ExecutedHEAD -and $m.AfterHEAD -ceq $state.ExecutedHEAD -and -not $m.WorktreeDirty -and -not $m.AfterWorktreeDirty -and $m.ImplementationUnchanged) 'Initial/run implementation identity changed or dirty'
    $binary=Join-Path $repo ("out/r1c4b-live-magnet-"+$Configuration.ToLowerInvariant()+"/src/platform/windows/$Configuration/panebind-magnet-takeover-probe.exe")
    Assert-Gate ($m.BinarySHA256 -ceq (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash -and $m.AfterBinarySHA256 -ceq $m.BinarySHA256) 'Binary identity mismatch'
    $log=[IO.Path]::GetFullPath($m.EvidencePath)
    Assert-Gate ($log.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Log must remain local owned evidence'
    Assert-Gate ($m.RunId -cmatch '^[a-f0-9]{32}$' -and [IO.Path]::GetFileName($log).EndsWith('-'+$m.RunId+'.jsonl',[StringComparison]::Ordinal) -and [IO.Path]::GetFileName($full).EndsWith('-'+$m.RunId+'.metadata.json',[StringComparison]::Ordinal)) 'Run ID does not bind log and metadata'
    Assert-Gate ($m.LogSHA256 -ceq (Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash) 'Raw log identity mismatch'
    $verdict=Test-EndDiagnosticsOwnedEvidence -Path $log
    Assert-Gate ($m.ProbeExitCode -eq 0 -and $m.CurrentContractsVerified -and $verdict.Result -ceq 'PASS' -and $verdict.Operation -ceq $Operation) 'Initial/run is not independently complete PASS for the selected operation'
    $rows=@(Get-Content -LiteralPath $log -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json})
    $m|Add-Member -NotePropertyName ValidatedStartQpc -NotePropertyValue $rows[0].qpc
    $m|Add-Member -NotePropertyName ValidatedEndQpc -NotePropertyValue $rows[-1].qpc
    $m|Add-Member -NotePropertyName ValidatedQpcFrequency -NotePropertyValue $rows[0].qpc_frequency
    return $m
}
Push-Location $repo
try{
    $state.ExecutedHEAD=git rev-parse HEAD
    Assert-Gate ($LASTEXITCODE -eq 0) 'HEAD unavailable'
    $statusRows=@(git status --porcelain)
    Assert-Gate ($LASTEXITCODE -eq 0 -and $statusRows.Count -eq 0) 'Require a clean implementation checkpoint'
    git check-ignore -- $summaryPath|Out-Null
    Assert-Gate ($LASTEXITCODE -eq 0) 'Aggregate evidence must remain ignored'
    $resize=Read-GateMetadata $PassedResizeMetadata 'Debug' 'BottomResize'
    $move=Read-GateMetadata $PassedMoveMetadata 'Debug' 'Move'
    Assert-Gate ($resize.EvidencePath -cne $move.EvidencePath -and $resize.ValidatedQpcFrequency -eq $move.ValidatedQpcFrequency -and $resize.ValidatedEndQpc -lt $move.ValidatedStartQpc) 'Distinct initial complete Resize must precede Move regression'
    $state.InitialEvidence=@(
        [pscustomobject]@{Operation='BottomResize';MetadataPath=[IO.Path]::GetFullPath($PassedResizeMetadata);MetadataSHA256=(Get-FileHash -LiteralPath $PassedResizeMetadata -Algorithm SHA256).Hash;EvidencePath=$resize.EvidencePath;LogSHA256=$resize.LogSHA256},
        [pscustomobject]@{Operation='Move';MetadataPath=[IO.Path]::GetFullPath($PassedMoveMetadata);MetadataSHA256=(Get-FileHash -LiteralPath $PassedMoveMetadata -Algorithm SHA256).Hash;EvidencePath=$move.EvidencePath;LogSHA256=$move.LogSHA256}
    )
    $state.Result='RUNNING'
    $stages=@(
        @{Name='DebugSmoke';Configuration='Debug';Count=5},
        @{Name='ReleaseSmoke';Configuration='Release';Count=5},
        @{Name='DebugFormal';Configuration='Debug';Count=20},
        @{Name='ReleaseFormal';Configuration='Release';Count=20}
    )
    foreach($stage in $stages){
        for($pair=1;$pair -le $stage.Count;++$pair){
            foreach($operation in @('Move','BottomResize')){
                $runId=[Guid]::NewGuid().ToString('N')
                $output=& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'run-r1c4b-end-diagnostics.ps1') -Configuration $stage.Configuration -Operation $operation -RunId $runId 2>&1
                $code=$LASTEXITCODE
                $files=@(Get-ChildItem -LiteralPath $root -Filter ('*-'+$runId+'.metadata.json'))
                $item=[ordered]@{Stage=$stage.Name;Pair=$pair;Operation=$operation;ExitCode=$code;MetadataPath=$(if($files.Count -eq 1){$files[0].FullName}else{$null})}
                $state.Runs+=@([pscustomobject]$item)
                if($code -ne 0 -or $files.Count -ne 1){$state.FirstFailure=[pscustomobject]$item;$output|Write-Output;throw 'First failing operation: STOP, no retry or next stage'}
                $null=Read-GateMetadata $files[0].FullName $stage.Configuration $operation
                Write-Host "$($stage.Name) $pair/$($stage.Count) $operation PASS (fresh source/receiver/hook)"
            }
            $state[$stage.Name]=$pair
        }
    }
    $state.Result='PASS'
}catch{
    $state.Result='STOPPED';$state.Reason=$_.Exception.Message
    if($null -eq $state.FirstFailure -and @($state.Runs).Count -gt 0){$state.FirstFailure=$state.Runs[-1]}
    Write-Host "STOP: $($state.Reason)"
}finally{
    # Runtime evidence generation only. No tracked artifact is overwritten.
    $state|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $summaryPath -Encoding UTF8
    Write-Host "Gate inventory: $summaryPath"
    Pop-Location
}
if($state.Result -cne 'PASS'){exit 2}
exit 0
