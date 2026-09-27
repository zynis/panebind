[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PassedResizeRevalidation,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$ExpectedResizeRevalidationSHA256,
    [Parameter(Mandatory=$true)][string]$PassedMoveMetadata
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-input-isolation-validation.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Join-Path $repo 'uat/r1c4b-input-isolation'
$summaryPath=Join-Path $root ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-fixe-gates-'+[Guid]::NewGuid().ToString('N')+'.json')
$state=[ordered]@{Schema='r1c4b-input-isolation-gates/v2';Result='NOT_RUN';ExecutedHEAD=$null;ValidatorScriptSHA256=$null;BinarySHA256=@{};InitialEvidence=@();DebugMoveSmoke=0;DebugResizeSmoke=0;ReleaseMoveSmoke=0;ReleaseResizeSmoke=0;DebugMoveFormal=0;DebugResizeFormal=0;ReleaseMoveFormal=0;ReleaseResizeFormal=0;Runs=@();FirstFailure=$null;Statistics=@{};StatisticsScope='smoke_and_formal_only'}
$metricNames=@('NativeExitToWinEventEnd','WinEventEndToIsolationReady','IsolationReadyToHandoffWrite','WinEventEndToHandoffWrite','HandoffWriteDuration','RawReceiptToOwnerQuantum','RawReceiptToNativeWrite')
$nonces=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
function Assert-IsolationGate([bool]$Condition,[string]$Reason){if(-not $Condition){throw $Reason}}
function Save-IsolationInventory{
    # Create a new ignored inventory; never replace original metadata/replay.
    $state|ConvertTo-Json -Depth 28|Set-Content -LiteralPath $summaryPath -Encoding UTF8
}
function Convert-IsolationGateInt64($Value,[string]$Label,[switch]$NonNegative){
    $numeric=$Value -is [sbyte] -or $Value -is [byte] -or $Value -is [int16] -or $Value -is [uint16] -or $Value -is [int32] -or $Value -is [uint32] -or $Value -is [int64] -or $Value -is [uint64] -or $Value -is [decimal] -or $Value -is [double] -or $Value -is [single]
    Assert-IsolationGate $numeric "$Label must be a signed Int64 integer"
    try{[decimal]$number=$Value}catch{throw "$Label must be a signed Int64 integer"}
    Assert-IsolationGate ($number -eq [decimal]::Truncate($number) -and $number -ge [decimal][long]::MinValue -and $number -le [decimal][long]::MaxValue) "$Label must be a signed Int64 integer"
    [long]$converted=$number
    Assert-IsolationGate (-not $NonNegative -or $converted -ge 0) "$Label must be nonnegative"
    return $converted
}
function Convert-IsolationGateMilliseconds($Ticks,[long]$Frequency){
    if($null -eq $Ticks){return $null}
    Assert-IsolationGate ($Frequency -gt 0) 'QPC frequency must be positive'
    # Samples were already checked as Int64. Only interpolation is fractional.
    return [decimal]$Ticks*[decimal]1000/[decimal]$Frequency
}
function Get-IsolationQuantile($Sorted,[double]$Fraction){
    Assert-IsolationGate ($Fraction -ge 0 -and $Fraction -le 1) 'Quantile fraction must be within [0,1]'
    $values=@(foreach($sample in @($Sorted)){if($null -ne $sample){Convert-IsolationGateInt64 $sample 'percentile tick sample'}})
    if($values.Count -eq 0){return $null}
    # [int] is only a bounded array index, never a QPC/tick accumulator.
    [decimal]$index=([decimal]$values.Count-1)*[decimal]$Fraction
    $low=[int][Math]::Floor($index);$high=[int][Math]::Ceiling($index)
    return [decimal]$values[$low]+([decimal]$values[$high]-[decimal]$values[$low])*($index-$low)
}
function Get-IsolationGateClock($Rows,$Verdict){
    Assert-IsolationGate ($Rows.Count -ge 2) 'Missing validated clock rows'
    [long]$frequency=Convert-IsolationGateInt64 $Rows[0].qpc_frequency 'QPC frequency' -NonNegative
    Assert-IsolationGate ($frequency -gt 0) 'Missing or reused test generation'
    [long]$previous=0;[long]$sequence=0
    foreach($row in $Rows){
        [long]$current=Convert-IsolationGateInt64 $row.qpc 'row QPC' -NonNegative
        [long]$serial=Convert-IsolationGateInt64 $row.sequence 'row sequence' -NonNegative
        ++$sequence
        Assert-IsolationGate ($serial -eq $sequence -and $current -ge $previous) 'Clock reversal or sequence mismatch'
        $previous=$current
    }
    [long]$start=Convert-IsolationGateInt64 $Rows[0].qpc 'start QPC' -NonNegative
    [long]$end=Convert-IsolationGateInt64 $Rows[-1].qpc 'end QPC' -NonNegative
    return [pscustomobject]@{Start=$start;End=$end;Frequency=$frequency}
}
function New-IsolationStatistics([string]$Configuration,[string]$Operation){
    $ticks=@{};$milliseconds=@{};$statistics=@{}
    foreach($name in $metricNames){$ticks[$name]=@();$milliseconds[$name]=@()}
    return [ordered]@{Configuration=$Configuration;Operation=$Operation;QpcFrequency=[long]0;Gestures=[long]0;TimingTicks=$ticks;TimingMilliseconds=$milliseconds;TimingStatistics=$statistics;MaxSourceWrites=[long]0;TotalSourceWrites=[long]0;MaxWritesPerQuantum=[long]0;MaxHandoffWrites=[long]0;TotalHandoffWrites=[long]0;MaxShieldPlacements=[long]0;TotalShieldPlacements=[long]0;MaxRawPackets=[long]0;TotalRawPackets=[long]0;MaxRawQuanta=[long]0;TotalRawQuanta=[long]0;MaxNativeDragAfterEnd=[long]0;TotalNativeDragAfterEnd=[long]0}
}
function Add-IsolationMeasurements($Rows,$Verdict,[string]$Configuration,[string]$Operation,[long]$Frequency,[bool]$RecordStatistics){
    $samples=@{}
    foreach($name in $metricNames){$samples[$name]=@()}
    foreach($metric in $Verdict.TimingTickSamples.PSObject.Properties){
        Assert-IsolationGate ($samples.ContainsKey($metric.Name)) 'Unknown timing metric'
        foreach($ticks in @($metric.Value)){
            if($null -ne $ticks){$samples[$metric.Name]+=@(Convert-IsolationGateInt64 $ticks 'timing tick sample')}
        }
    }
    if(-not $RecordStatistics){return}
    $group=$state.Statistics[$Configuration+$Operation]
    Assert-IsolationGate ($group.QpcFrequency -eq 0 -or $group.QpcFrequency -eq $Frequency) 'Statistics cannot mix QPC frequencies'
    $group.QpcFrequency=$Frequency
    foreach($name in $metricNames){$group.TimingTicks[$name]+=@($samples[$name])}
    foreach($counter in @(@('NativeWrites','SourceWrites'),@('HandoffNativeCalls','HandoffWrites'),@('ShieldNativeCalls','ShieldPlacements'),@('RawPackets','RawPackets'),@('ContinuationQuanta','RawQuanta'),@('NativeDragAfterWinEventEnd','NativeDragAfterEnd'))){
        [long]$count=Convert-IsolationGateInt64 (Get-AutoField $Verdict $counter[0]) $counter[0] -NonNegative
        $max='Max'+$counter[1];$total='Total'+$counter[1]
        if($count -gt $group[$max]){$group[$max]=$count}
        $group[$total]=[long]$group[$total]+$count
    }
    foreach($row in $Rows){
        if($row.type -ceq 'raw_quantum'){
            [long]$writes=Convert-IsolationGateInt64 $row.native_calls 'quantum write count' -NonNegative
            if($writes -gt $group.MaxWritesPerQuantum){$group.MaxWritesPerQuantum=$writes}
        }
    }
    $group.Gestures=[long]$group.Gestures+1
}
function Update-IsolationStatistics{
    foreach($key in $state.Statistics.Keys){
        $group=$state.Statistics[$key]
        foreach($name in $metricNames){
            $sorted=@($group.TimingTicks[$name]|Sort-Object)
            $group.TimingMilliseconds[$name]=@(foreach($ticks in $sorted){Convert-IsolationGateMilliseconds $ticks $group.QpcFrequency})
            $group.TimingStatistics[$name]=[ordered]@{Count=$sorted.Count;P50Ticks=(Get-IsolationQuantile $sorted 0.50);P95Ticks=(Get-IsolationQuantile $sorted 0.95);P50Ms=(Convert-IsolationGateMilliseconds (Get-IsolationQuantile $sorted 0.50) $group.QpcFrequency);P95Ms=(Convert-IsolationGateMilliseconds (Get-IsolationQuantile $sorted 0.95) $group.QpcFrequency);QuantileMethod='linear_interpolation_in_int64_tick_domain';NoSLA=$true}
        }
    }
}
function Assert-IsolationVerdict($Verdict,[string]$Operation){
    Assert-IsolationGate ($Verdict.Result -ceq 'PASS' -and $Verdict.Operation -ceq $Operation -and $Verdict.ProductHandoffAuthority -ceq 'PASS' -and $Verdict.TestInputIsolation -ceq 'PASS' -and $Verdict.Cancel -ceq 'PASS_WITH_TERMINAL_SETTLEMENT' -and $Verdict.Handoff -ceq 'PASS' -and $Verdict.Takeover -ceq 'PASS') 'Not an independently complete separated-authority PASS'
    foreach($contract in @(@('ForegroundContract','verified_global_foreground_v2'),@('HandoffContract','winevent_end_barrier_v1'),@('DiagnosticContract','separated_authority_v1'),@('AuthorityContract','product_gesture_v1'),@('IsolationContract','post_end_shield_v1'))){
        Assert-IsolationGate ((Get-AutoField $Verdict $contract[0]) -ceq $contract[1]) 'Current evidence contract mismatch'
    }
}
function Bind-IsolationClock($Metadata,$Rows,$Verdict,[string]$Configuration,[string]$Operation,[bool]$RecordStatistics){
    $clock=Get-IsolationGateClock $Rows $Verdict
    Assert-IsolationGate ($Rows[0].run_nonce -eq $Verdict.RunNonce -and $nonces.Add([string]$Verdict.RunNonce)) 'Missing or reused test generation'
    $Metadata|Add-Member -NotePropertyName ValidatedStartQpc -NotePropertyValue $clock.Start
    $Metadata|Add-Member -NotePropertyName ValidatedEndQpc -NotePropertyValue $clock.End
    $Metadata|Add-Member -NotePropertyName ValidatedQpcFrequency -NotePropertyValue $clock.Frequency
    Add-IsolationMeasurements $Rows $Verdict $Configuration $Operation $clock.Frequency $RecordStatistics
    return $Metadata
}
function Read-IsolationGateMetadata([string]$Path,[string]$Configuration,[string]$Operation,[bool]$RecordStatistics=$true){
    $full=[IO.Path]::GetFullPath($Path)
    Assert-IsolationGate ($full.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Metadata must be local Fix D evidence'
    $metadataHash=(Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash
    $m=Get-Content -LiteralPath $full -Encoding UTF8|ConvertFrom-Json
    Assert-IsolationGate ($m.Schema -ceq 'r1c4b-input-isolation-run/v1' -and $m.Configuration -ceq $Configuration -and $m.Operation -ceq $Operation) 'Wrong evidence mode/configuration/operation'
    Assert-IsolationGate ($m.ExecutedHEAD -ceq $state.ExecutedHEAD -and $m.AfterHEAD -ceq $state.ExecutedHEAD -and -not $m.WorktreeDirty -and -not $m.AfterWorktreeDirty -and $m.ImplementationUnchanged) 'Implementation identity changed or dirty'
    $binary=Join-Path $repo ("out/r1c4b-live-magnet-"+$Configuration.ToLowerInvariant()+"/src/platform/windows/$Configuration/panebind-magnet-takeover-probe.exe")
    Assert-IsolationGate ($m.BinarySHA256 -ceq $state.BinarySHA256[$Configuration] -and $m.BinarySHA256 -ceq (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash -and $m.AfterBinarySHA256 -ceq $m.BinarySHA256) 'Binary identity mismatch'
    Assert-IsolationGate ($m.ValidatorScriptSHA256 -ceq $state.ValidatorScriptSHA256 -and $m.AfterValidatorScriptSHA256 -ceq $state.ValidatorScriptSHA256 -and (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'r1c4b-input-isolation-validation.ps1') -Algorithm SHA256).Hash -ceq $state.ValidatorScriptSHA256) 'Validator identity mismatch'
    $log=[IO.Path]::GetFullPath($m.EvidencePath)
    Assert-IsolationGate ($log.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Log must remain local owned evidence'
    Assert-IsolationGate ($m.RunId -cmatch '^[a-f0-9]{32}$' -and [IO.Path]::GetFileName($log).EndsWith('-'+$m.RunId+'.jsonl',[StringComparison]::Ordinal) -and [IO.Path]::GetFileName($full).EndsWith('-'+$m.RunId+'.metadata.json',[StringComparison]::Ordinal)) 'Run ID must bind log and metadata'
    Assert-IsolationGate ($m.LogSHA256 -ceq (Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash) 'Raw log identity mismatch'
    Assert-IsolationGate ($m.ProbeExitCode -eq 0 -and $m.CurrentContractsVerified -and (Get-AutoField (Get-AutoField $m 'Result') 'Result') -ceq 'PASS') 'Not an independently complete separated-authority PASS'
    $verdict=Test-InputIsolationOwnedEvidence -Path $log
    Assert-IsolationVerdict $verdict $Operation
    $rows=@(Get-Content -LiteralPath $log -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json})
    Assert-IsolationGate ($m.LogSHA256 -ceq (Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash -and $metadataHash -ceq (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash) 'Evidence changed during independent validation'
    return Bind-IsolationClock $m $rows $verdict $Configuration $Operation $RecordStatistics
}
function Read-IsolationResizeRevalidation([string]$Path,[string]$ExpectedHash){
    $full=[IO.Path]::GetFullPath($Path)
    Assert-IsolationGate ($full.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -and $full.EndsWith('.revalidation.json',[StringComparison]::Ordinal)) 'Replay artifact must be new local Fix E evidence'
    Assert-IsolationGate ((Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash -ceq $ExpectedHash.ToUpperInvariant()) 'Replay artifact SHA256 mismatch'
    $a=Get-Content -LiteralPath $full -Encoding UTF8|ConvertFrom-Json
    $originalLogHash='BB1E1F90D58A0B21C5A0B3F65F3A3DDB3994388C96FFE4C6A0F3ED5EF174B76F'
    $originalMetadataHash='D1B7C425CDB53A6356BC9B23C72376857BBE12996234F7032A5846B9B15BEB09'
    $originalBinaryHash='43289B87236C08E68EAC2A23BCA1AD9923454DDB21EBF4DE84447083933DEC6D'
    $originalSourceHash='CEBB558B403B90D3F1A55C521358884BB6C838624EA6C0BC8C5BA481192BC2FB'
    Assert-IsolationGate ($a.schema -ceq 'r1c4b-input-isolation-revalidation/v1' -and $a.original_result -ceq 'INVALID_EVIDENCE' -and $a.replay_result -ceq 'PASS' -and $a.first_semantic_failure -ceq 'NONE' -and $a.semantic_contract_version -ceq 'post_end_shield_v1' -and $a.numeric_fix_only -is [bool] -and $a.numeric_fix_only -and $a.CorrectedValidatorVerified -is [bool] -and $a.CorrectedValidatorVerified -and $a.ValidatorWorktreeClean -is [bool] -and $a.ValidatorWorktreeClean -and -not [string]::IsNullOrWhiteSpace($a.replay_time)) 'Replay is not a clean corrected numeric-only PASS'
    Assert-IsolationVerdict $a.replay_verdict 'BottomResize'
    Assert-IsolationGate ($a.evidence_implementation_sha -ceq 'a67d3049c1027e8b2b82f8e122041b71ff5043ca' -and $a.validator_fix_sha -cmatch '^[a-f0-9]{40}$' -and $a.original_evidence_sha256 -ceq $originalLogHash -and $a.original_evidence_sha256_after -ceq $originalLogHash -and $a.original_metadata_sha256 -ceq $originalMetadataHash -and $a.original_metadata_sha256_after -ceq $originalMetadataHash -and $a.OriginalBinarySHA256 -ceq $originalBinaryHash -and $a.AfterBinarySHA256 -ceq $originalBinaryHash -and $a.RuntimeSourceSHA256 -ceq $originalSourceHash -and $a.AfterRuntimeSourceSHA256 -ceq $originalSourceHash) 'Replay does not bind immutable Fix D identities'
    Assert-IsolationGate ($a.validator_script_sha256 -ceq $state.ValidatorScriptSHA256 -and (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'r1c4b-input-isolation-validation.ps1') -Algorithm SHA256).Hash -ceq $a.validator_script_sha256) 'Replay validator identity mismatch'
    git merge-base --is-ancestor $a.validator_fix_sha $state.ExecutedHEAD|Out-Null
    Assert-IsolationGate ($LASTEXITCODE -eq 0) 'Validator fix SHA is not an ancestor of current checkpoint'
    git diff --exit-code $a.evidence_implementation_sha $state.ExecutedHEAD -- src/platform/windows|Out-Null
    Assert-IsolationGate ($LASTEXITCODE -eq 0) 'Native runtime sources changed since original Fix D'
    Assert-IsolationGate ((Get-FileHash -LiteralPath (Join-Path $repo 'src/platform/windows/operations/magnet_takeover_probe.cpp') -Algorithm SHA256).Hash -ceq $originalSourceHash -and $state.BinarySHA256.Debug -ceq $originalBinaryHash) 'Current runtime differs from original Fix D'
    $log=[IO.Path]::GetFullPath($a.original_evidence_path);$metadata=[IO.Path]::GetFullPath($a.original_metadata_path)
    Assert-IsolationGate ($log.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -and $metadata.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Original evidence must remain local and immutable'
    Assert-IsolationGate ((Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash -ceq $originalLogHash -and (Get-FileHash -LiteralPath $metadata -Algorithm SHA256).Hash -ceq $originalMetadataHash) 'Original evidence or metadata SHA256 mismatch'
    $m=Get-Content -LiteralPath $metadata -Encoding UTF8|ConvertFrom-Json
    Assert-IsolationGate ($m.Schema -ceq 'r1c4b-input-isolation-run/v1' -and $m.Configuration -ceq 'Debug' -and $m.Operation -ceq 'BottomResize' -and $m.ExecutedHEAD -ceq $a.evidence_implementation_sha -and $m.AfterHEAD -ceq $a.evidence_implementation_sha -and -not $m.WorktreeDirty -and -not $m.AfterWorktreeDirty -and $m.ImplementationUnchanged -and $m.ProbeExitCode -eq 0 -and -not $m.CurrentContractsVerified -and $m.Result.Result -ceq 'INVALID_EVIDENCE' -and [IO.Path]::GetFullPath($m.EvidencePath) -ceq $log -and $m.LogSHA256 -ceq $originalLogHash -and $m.BinarySHA256 -ceq $originalBinaryHash -and $m.AfterBinarySHA256 -ceq $originalBinaryHash -and $m.RunId -ceq '3e6236754a004decb816079530b3861b') 'Original pipeline history was changed or is not the original Resize'
    $binary=Join-Path $repo 'out/r1c4b-live-magnet-debug/src/platform/windows/Debug/panebind-magnet-takeover-probe.exe'
    Assert-IsolationGate ((Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash -ceq $originalBinaryHash) 'Current Debug runtime binary differs from original Fix D'
    # Freshly run the complete corrected validator; artifact/report PASS is insufficient.
    $verdict=Test-InputIsolationOwnedEvidence -Path $log
    Assert-IsolationVerdict $verdict 'BottomResize'
    $rows=@(Get-Content -LiteralPath $log -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json})
    Assert-IsolationGate ($rows.Count -eq 460) 'Original replay must cover all 460 rows'
    Assert-IsolationGate ((Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash -ceq $ExpectedHash.ToUpperInvariant() -and (Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash -ceq $originalLogHash -and (Get-FileHash -LiteralPath $metadata -Algorithm SHA256).Hash -ceq $originalMetadataHash) 'Replay or original evidence changed during validation'
    $m=Bind-IsolationClock $m $rows $verdict 'Debug' 'BottomResize' $false
    $m|Add-Member -NotePropertyName ReplayArtifactPath -NotePropertyValue $full
    $m|Add-Member -NotePropertyName ReplayArtifactSHA256 -NotePropertyValue $ExpectedHash.ToUpperInvariant()
    return $m
}
foreach($configuration in @('Debug','Release')){foreach($operation in @('Move','BottomResize')){$state.Statistics[$configuration+$operation]=New-IsolationStatistics $configuration $operation}}
Push-Location $repo
try{
    $state.ExecutedHEAD=git rev-parse HEAD
    Assert-IsolationGate ($LASTEXITCODE -eq 0) 'HEAD unavailable'
    $statusRows=@(git status --porcelain)
    Assert-IsolationGate ($LASTEXITCODE -eq 0 -and $statusRows.Count -eq 0) 'Require a clean implementation checkpoint'
    git check-ignore -- $summaryPath|Out-Null
    Assert-IsolationGate ($LASTEXITCODE -eq 0) 'Aggregate evidence must remain ignored'
    New-Item -ItemType Directory -Path $root -Force|Out-Null
    $state.ValidatorScriptSHA256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'r1c4b-input-isolation-validation.ps1') -Algorithm SHA256).Hash
    foreach($configuration in @('Debug','Release')){
        $binary=Join-Path $repo ("out/r1c4b-live-magnet-"+$configuration.ToLowerInvariant()+"/src/platform/windows/$configuration/panebind-magnet-takeover-probe.exe")
        $state.BinarySHA256[$configuration]=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
    }
    $pending=[pscustomobject]@{Stage='Prerequisite';Configuration='Debug';Operation='BottomResize';Repetition=0;MetadataPath=$PassedResizeRevalidation;ExitCode=$null;Error=$null;FailureReasons=@()}
    $resize=Read-IsolationResizeRevalidation $PassedResizeRevalidation $ExpectedResizeRevalidationSHA256
    $pending=[pscustomobject]@{Stage='Prerequisite';Configuration='Debug';Operation='Move';Repetition=0;MetadataPath=$PassedMoveMetadata;ExitCode=$null;Error=$null;FailureReasons=@()}
    $move=Read-IsolationGateMetadata $PassedMoveMetadata 'Debug' 'Move' $false
    Assert-IsolationGate ($resize.EvidencePath -cne $move.EvidencePath -and $resize.ValidatedQpcFrequency -eq $move.ValidatedQpcFrequency -and $resize.ValidatedEndQpc -lt $move.ValidatedStartQpc) 'Initial complete Resize replay must precede distinct current Move regression'
    $state.InitialEvidence=@(
        [pscustomobject]@{Operation='BottomResize';ReplayArtifactPath=$resize.ReplayArtifactPath;ReplayArtifactSHA256=$resize.ReplayArtifactSHA256;MetadataPath=[IO.Path]::GetFullPath((Get-Content -LiteralPath $PassedResizeRevalidation -Encoding UTF8|ConvertFrom-Json).original_metadata_path);MetadataSHA256='D1B7C425CDB53A6356BC9B23C72376857BBE12996234F7032A5846B9B15BEB09';EvidencePath=$resize.EvidencePath;LogSHA256=$resize.LogSHA256;OriginalPipelineResult='INVALID_EVIDENCE';CorrectedReplayResult='PASS'},
        [pscustomobject]@{Operation='Move';MetadataPath=[IO.Path]::GetFullPath($PassedMoveMetadata);MetadataSHA256=(Get-FileHash -LiteralPath $PassedMoveMetadata -Algorithm SHA256).Hash;EvidencePath=$move.EvidencePath;LogSHA256=$move.LogSHA256}
    )
    $state.Result='RUNNING';Save-IsolationInventory
    foreach($stage in @(@{Name='DebugMoveSmoke';Configuration='Debug';Operation='Move';Count=5},@{Name='DebugResizeSmoke';Configuration='Debug';Operation='BottomResize';Count=5},@{Name='ReleaseMoveSmoke';Configuration='Release';Operation='Move';Count=5},@{Name='ReleaseResizeSmoke';Configuration='Release';Operation='BottomResize';Count=5},@{Name='DebugMoveFormal';Configuration='Debug';Operation='Move';Count=20},@{Name='DebugResizeFormal';Configuration='Debug';Operation='BottomResize';Count=20},@{Name='ReleaseMoveFormal';Configuration='Release';Operation='Move';Count=20},@{Name='ReleaseResizeFormal';Configuration='Release';Operation='BottomResize';Count=20})){
        if($stage.Name.EndsWith('Formal',[StringComparison]::Ordinal)){
            Assert-IsolationGate ($state.DebugMoveSmoke -eq 5 -and $state.DebugResizeSmoke -eq 5 -and $state.ReleaseMoveSmoke -eq 5 -and $state.ReleaseResizeSmoke -eq 5) 'All four operation-specific smoke groups must pass before formal'
        }
        for($repetition=1;$repetition -le $stage.Count;++$repetition){
            $runId=[Guid]::NewGuid().ToString('N')
            $item=[pscustomobject][ordered]@{Stage=$stage.Name;Configuration=$stage.Configuration;Repetition=$repetition;Operation=$stage.Operation;RunId=$runId;ExitCode=$null;MetadataPath=$null;MetadataResult=$null;Error=$null;FailureReasons=@();FailedOutput=@();Passed=$false}
            $state.Runs+=@($item);Save-IsolationInventory
            $output=& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'run-r1c4b-input-isolation.ps1') -Configuration $stage.Configuration -Operation $stage.Operation -RunId $runId 2>&1
            $code=$LASTEXITCODE;$files=@(Get-ChildItem -LiteralPath $root -Filter ('*-'+$runId+'.metadata.json'))
            $item.ExitCode=$code;$item.MetadataPath=$(if($files.Count -eq 1){$files[0].FullName}else{$null})
            if($files.Count -eq 1){
                try{
                    $attemptMetadata=Get-Content -LiteralPath $files[0].FullName -Encoding UTF8|ConvertFrom-Json
                    $item.MetadataResult=Get-AutoField $attemptMetadata.Result 'Result'
                    $item.FailureReasons=@(Get-AutoField $attemptMetadata.Result 'Reasons')
                }catch{$item.FailureReasons=@($_.Exception.Message)}
            }
            Save-IsolationInventory
            if($code -ne 0 -or $files.Count -ne 1){
                $item.Error="exit=$code; metadata_count=$($files.Count)";$item.FailedOutput=@($output|ForEach-Object {[string]$_})
                $state.FirstFailure=$item;$output|Write-Output;throw 'First failing operation: STOP; no retry or next stage'
            }
            $validated=Read-IsolationGateMetadata $files[0].FullName $stage.Configuration $stage.Operation
            $item.Passed=$true
            $state[$stage.Name]=$repetition
            Save-IsolationInventory
            Write-Host "$($stage.Name) $repetition/$($stage.Count) PASS (fresh owned resources; shield only when needed)"
        }
    }
    $state.Result='PASS'
}catch{
    $state.Result='STOPPED';$state.Reason=$_.Exception.Message
    if($null -eq $state.FirstFailure){if(@($state.Runs).Count){$state.FirstFailure=$state.Runs[-1]}elseif($null -ne (Get-Variable pending -ErrorAction SilentlyContinue)){$state.FirstFailure=$pending}}
    if($null -ne $state.FirstFailure){
        $state.FirstFailure.Error=$(if($state.FirstFailure.Error){$state.FirstFailure.Error+'; '+$_.Exception.Message}else{$_.Exception.Message})
        $state.FirstFailure.FailureReasons+=@($_.Exception.Message)
    }
    Write-Host "STOP: $($state.Reason)"
}finally{
    Update-IsolationStatistics
    Save-IsolationInventory;Write-Host "Fix E gate inventory: $summaryPath";Pop-Location
}
if($state.Result -cne 'PASS'){exit 2}
exit 0
