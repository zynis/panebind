[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PassedResizeMetadata,
    [Parameter(Mandatory=$true)][string]$PassedMoveMetadata
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-input-isolation-validation.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Join-Path $repo 'uat/r1c4b-input-isolation'
$summaryPath=Join-Path $root ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-gates-'+[Guid]::NewGuid().ToString('N')+'.json')
$state=[ordered]@{Schema='r1c4b-input-isolation-gates/v1';Result='NOT_RUN';ExecutedHEAD=$null;InitialEvidence=@();DebugSmoke=0;ReleaseSmoke=0;DebugFormal=0;ReleaseFormal=0;Runs=@();FirstFailure=$null;TimingMilliseconds=@{};TimingStatistics=@{}}
foreach($metricName in @('NativeExitToWinEventEnd','WinEventEndToIsolationReady','IsolationReadyToHandoffWrite','WinEventEndToHandoffWrite','HandoffWriteDuration','RawReceiptToOwnerQuantum','RawReceiptToNativeWrite')){$state.TimingMilliseconds[$metricName]=@()}
$nonces=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
function Assert-IsolationGate([bool]$Condition,[string]$Reason){if(-not $Condition){throw $Reason}}
function Save-IsolationInventory{
    # Only ignored runtime evidence. Never overwrites historical A/B/C logs.
    $state|ConvertTo-Json -Depth 24|Set-Content -LiteralPath $summaryPath -Encoding UTF8
}
function Read-IsolationGateMetadata([string]$Path,[string]$Configuration,[string]$Operation){
    $full=[IO.Path]::GetFullPath($Path)
    Assert-IsolationGate ($full.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Metadata must be local Fix D evidence'
    $m=Get-Content -LiteralPath $full -Encoding UTF8|ConvertFrom-Json
    Assert-IsolationGate ($m.Schema -ceq 'r1c4b-input-isolation-run/v1' -and $m.Configuration -ceq $Configuration -and $m.Operation -ceq $Operation) 'Wrong evidence mode/configuration/operation'
    Assert-IsolationGate ($m.ExecutedHEAD -ceq $state.ExecutedHEAD -and $m.AfterHEAD -ceq $state.ExecutedHEAD -and -not $m.WorktreeDirty -and -not $m.AfterWorktreeDirty -and $m.ImplementationUnchanged) 'Implementation identity changed or dirty'
    $binary=Join-Path $repo ("out/r1c4b-live-magnet-"+$Configuration.ToLowerInvariant()+"/src/platform/windows/$Configuration/panebind-magnet-takeover-probe.exe")
    Assert-IsolationGate ($m.BinarySHA256 -ceq (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash -and $m.AfterBinarySHA256 -ceq $m.BinarySHA256) 'Binary identity mismatch'
    $log=[IO.Path]::GetFullPath($m.EvidencePath)
    Assert-IsolationGate ($log.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Log must remain local owned evidence'
    Assert-IsolationGate ($m.RunId -cmatch '^[a-f0-9]{32}$' -and [IO.Path]::GetFileName($log).EndsWith('-'+$m.RunId+'.jsonl',[StringComparison]::Ordinal) -and [IO.Path]::GetFileName($full).EndsWith('-'+$m.RunId+'.metadata.json',[StringComparison]::Ordinal)) 'Run ID must bind log and metadata'
    Assert-IsolationGate ($m.LogSHA256 -ceq (Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash) 'Raw log identity mismatch'
    $verdict=Test-InputIsolationOwnedEvidence -Path $log
    Assert-IsolationGate ($m.ProbeExitCode -eq 0 -and $m.CurrentContractsVerified -and $verdict.Result -ceq 'PASS' -and $verdict.Operation -ceq $Operation -and $verdict.ProductHandoffAuthority -ceq 'PASS' -and $verdict.TestInputIsolation -ceq 'PASS') 'Not an independently complete separated-authority PASS'
    $rows=@(Get-Content -LiteralPath $log -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json})
    Assert-IsolationGate ($rows[0].qpc_frequency -gt 0 -and $rows[0].run_nonce -ceq $verdict.RunNonce -and $nonces.Add([string]$verdict.RunNonce)) 'Missing or reused test generation'
    $m|Add-Member -NotePropertyName ValidatedStartQpc -NotePropertyValue $rows[0].qpc
    $m|Add-Member -NotePropertyName ValidatedEndQpc -NotePropertyValue $rows[-1].qpc
    $m|Add-Member -NotePropertyName ValidatedQpcFrequency -NotePropertyValue $rows[0].qpc_frequency
    foreach($metric in $verdict.TimingTickSamples.PSObject.Properties){
        if(-not $state.TimingMilliseconds.ContainsKey($metric.Name)){$state.TimingMilliseconds[$metric.Name]=@()}
        foreach($ticks in @($metric.Value)){
            if($null -ne $ticks){$state.TimingMilliseconds[$metric.Name]+=@([double]$ticks*1000.0/[double]$rows[0].qpc_frequency)}
        }
    }
    return $m
}
function Get-IsolationQuantile($Sorted,[double]$Fraction){
    if($Sorted.Count -eq 0){return $null}
    $index=($Sorted.Count-1)*$Fraction;$low=[int][Math]::Floor($index);$high=[int][Math]::Ceiling($index)
    return [double]$Sorted[$low]+([double]$Sorted[$high]-[double]$Sorted[$low])*($index-$low)
}
Push-Location $repo
try{
    $state.ExecutedHEAD=git rev-parse HEAD
    Assert-IsolationGate ($LASTEXITCODE -eq 0) 'HEAD unavailable'
    $statusRows=@(git status --porcelain)
    Assert-IsolationGate ($LASTEXITCODE -eq 0 -and $statusRows.Count -eq 0) 'Require a clean implementation checkpoint'
    git check-ignore -- $summaryPath|Out-Null
    Assert-IsolationGate ($LASTEXITCODE -eq 0) 'Aggregate evidence must remain ignored'
    New-Item -ItemType Directory -Path $root -Force|Out-Null
    $resize=Read-IsolationGateMetadata $PassedResizeMetadata 'Debug' 'BottomResize'
    $move=Read-IsolationGateMetadata $PassedMoveMetadata 'Debug' 'Move'
    Assert-IsolationGate ($resize.EvidencePath -cne $move.EvidencePath -and $resize.ValidatedQpcFrequency -eq $move.ValidatedQpcFrequency -and $resize.ValidatedEndQpc -lt $move.ValidatedStartQpc) 'Initial complete Resize must precede distinct Move regression'
    $state.InitialEvidence=@(
        [pscustomobject]@{Operation='BottomResize';MetadataPath=[IO.Path]::GetFullPath($PassedResizeMetadata);MetadataSHA256=(Get-FileHash -LiteralPath $PassedResizeMetadata -Algorithm SHA256).Hash;EvidencePath=$resize.EvidencePath;LogSHA256=$resize.LogSHA256},
        [pscustomobject]@{Operation='Move';MetadataPath=[IO.Path]::GetFullPath($PassedMoveMetadata);MetadataSHA256=(Get-FileHash -LiteralPath $PassedMoveMetadata -Algorithm SHA256).Hash;EvidencePath=$move.EvidencePath;LogSHA256=$move.LogSHA256}
    )
    $state.Result='RUNNING';Save-IsolationInventory
    foreach($stage in @(@{Name='DebugSmoke';Configuration='Debug';Count=5},@{Name='ReleaseSmoke';Configuration='Release';Count=5},@{Name='DebugFormal';Configuration='Debug';Count=20},@{Name='ReleaseFormal';Configuration='Release';Count=20})){
        for($pair=1;$pair -le $stage.Count;++$pair){
            foreach($operation in @('Move','BottomResize')){
                $runId=[Guid]::NewGuid().ToString('N')
                $output=& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'run-r1c4b-input-isolation.ps1') -Configuration $stage.Configuration -Operation $operation -RunId $runId 2>&1
                $code=$LASTEXITCODE;$files=@(Get-ChildItem -LiteralPath $root -Filter ('*-'+$runId+'.metadata.json'))
                $item=[ordered]@{Stage=$stage.Name;Pair=$pair;Operation=$operation;ExitCode=$code;MetadataPath=$(if($files.Count -eq 1){$files[0].FullName}else{$null})}
                $state.Runs+=@([pscustomobject]$item);Save-IsolationInventory
                if($code -ne 0 -or $files.Count -ne 1){$state.FirstFailure=[pscustomobject]$item;$output|Write-Output;throw 'First failing operation: STOP; no retry or next stage'}
                $null=Read-IsolationGateMetadata $files[0].FullName $stage.Configuration $operation
                Write-Host "$($stage.Name) $pair/$($stage.Count) $operation PASS (fresh source/receiver/hook; shield only when needed)"
            }
            $state[$stage.Name]=$pair;Save-IsolationInventory
        }
    }
    $state.Result='PASS'
}catch{
    $state.Result='STOPPED';$state.Reason=$_.Exception.Message
    if($null -eq $state.FirstFailure -and @($state.Runs).Count){$state.FirstFailure=$state.Runs[-1]}
    Write-Host "STOP: $($state.Reason)"
}finally{
    foreach($metric in $state.TimingMilliseconds.Keys){
        $sorted=@($state.TimingMilliseconds[$metric]|Sort-Object)
        $state.TimingStatistics[$metric]=[ordered]@{Count=$sorted.Count;P50Ms=(Get-IsolationQuantile $sorted 0.50);P95Ms=(Get-IsolationQuantile $sorted 0.95);QuantileMethod='linear_interpolation';NoSLA=$true}
    }
    Save-IsolationInventory;Write-Host "Fix D gate inventory: $summaryPath";Pop-Location
}
if($state.Result -cne 'PASS'){exit 2}
exit 0
