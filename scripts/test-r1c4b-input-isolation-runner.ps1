Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# Synthetic runner policy tests only. Never evaluate a runner top-level program,
# launch a CLI/probe, send input, query windows, execute real Git or read UAT.
# Metadata/hash/validator/Git results are in-memory stubs, not real evidence.
$checks=0
function Check-Runner([bool]$Value,[string]$Reason){if(-not $Value){throw "input isolation runner fixture: $Reason"};$script:checks++}
function Read-RunnerAst([string]$Name){
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $Name),[ref]$tokens,[ref]$errors)
    Check-Runner (@($errors).Count -eq 0) "parse $Name"
    return $ast
}
function One-RunnerNode($Ast,[scriptblock]$Predicate,[string]$Label){
    $nodes=@($Ast.FindAll($Predicate,$true));Check-Runner ($nodes.Count -eq 1) "unique $Label";return $nodes[0]
}
function Get-AutoField($Value,[string]$Name){$p=$Value.PSObject.Properties[$Name];if($null -eq $p){return $null};return $p.Value}
function Copy-RunnerFixture($Value){return ($Value|ConvertTo-Json -Depth 20 -Compress|ConvertFrom-Json)}
function Reject-Runner([scriptblock]$Action,[string]$Reason,[string]$Expected=''){
    $rejected=$false
    try{$null=& $Action}catch{$rejected=$true;if($Expected){Check-Runner ($_.Exception.Message.Contains($Expected)) "$Reason rejection reason"}}
    Check-Runner $rejected $Reason
}
$single=Read-RunnerAst 'run-r1c4b-input-isolation.ps1'
$gates=Read-RunnerAst 'run-r1c4b-input-isolation-gates.ps1'
$contracts=One-RunnerNode $single {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$contracts'} 'single contract expression'
Check-Runner (@($contracts.Right.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -cne 'Get-AutoField'},$true)).Count -eq 0) 'single contract expression is pure'
$contractExpression=[scriptblock]::Create($contracts.Right.Extent.Text)
$Operation='BottomResize'
$result=[pscustomobject]@{ForegroundContract='verified_global_foreground_v2';HandoffContract='winevent_end_barrier_v1';DiagnosticContract='separated_authority_v1';AuthorityContract='product_gesture_v1';IsolationContract='post_end_shield_v1';Operation=$Operation}
$validContract=Copy-RunnerFixture $result
Check-Runner (& $contractExpression) 'single exact contracts and operation'
foreach($field in @('ForegroundContract','HandoffContract','DiagnosticContract','AuthorityContract','IsolationContract','Operation')){$result=Copy-RunnerFixture $validContract;$result.$field='wrong';Check-Runner (-not (& $contractExpression)) "single rejects mismatched $field"}
$result=Copy-RunnerFixture $validContract;$result.PSObject.Properties.Remove('Operation');Check-Runner (-not (& $contractExpression)) 'missing operation is not accepted'
function Test-CleanBeforeInput($Ast){
    $calls=@($Ast.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.InvocationOperator -eq [Management.Automation.Language.TokenKind]::Ampersand -and $n.CommandElements[0].Extent.Text -ceq '$exe'},$true))
    $guards=@($Ast.FindAll({param($n) $n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text -ceq '$statusRows.Count'},$true))
    return $calls.Count -eq 1 -and $guards.Count -eq 1 -and @($guards[0].FindAll({param($n) $n -is [Management.Automation.Language.ThrowStatementAst]},$true)).Count -eq 1 -and $guards[0].Extent.EndOffset -lt $calls[0].Extent.StartOffset
}
Check-Runner (Test-CleanBeforeInput $single) 'clean guard precedes only native invocation'
$guard=One-RunnerNode $single {param($n) $n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text -ceq '$statusRows.Count'} 'single clean guard'
$t=$null;$e=$null
$removed=[Management.Automation.Language.Parser]::ParseInput($single.Extent.Text.Replace($guard.Extent.Text,''),[ref]$t,[ref]$e)
Check-Runner (-not (Test-CleanBeforeInput $removed)) 'removed clean guard is detected'
$late=[Management.Automation.Language.Parser]::ParseInput(($single.Extent.Text.Replace($guard.Extent.Text,'')+"`n"+$guard.Extent.Text),[ref]$t,[ref]$e)
Check-Runner (-not (Test-CleanBeforeInput $late)) 'late clean guard is detected'
$metadata=One-RunnerNode $single {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$metadata'} 'single metadata'
foreach($field in @('ExecutedHEAD','AfterHEAD','WorktreeDirty','AfterWorktreeDirty','BinarySHA256','AfterBinarySHA256','ValidatorScriptSHA256','AfterValidatorScriptSHA256','LogSHA256','RunId','EvidencePath','ImplementationUnchanged')){Check-Runner ($metadata.Right.Extent.Text -match ('\b'+$field+'=')) "single preserves $field"}
$metricAssignment=One-RunnerNode $gates {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$metricNames'} 'metric names'
$metricNames=@(& ([scriptblock]::Create($metricAssignment.Right.Extent.Text)))
Check-Runner ($metricNames.Count -eq 7) 'seven metrics retained'
# Load serializer before installing command-name stubs (module auto-import
# must never overwrite an in-memory Get-Content/Get-FileHash function).
$null=[pscustomobject]@{synthetic_only=$true}|ConvertTo-Json -Compress
$helperNames=@('Assert-IsolationGate','Convert-IsolationGateInt64','Convert-IsolationGateMilliseconds','Get-IsolationQuantile','Get-IsolationGateClock','New-IsolationStatistics','Add-IsolationMeasurements','Update-IsolationStatistics','Assert-IsolationVerdict','Bind-IsolationClock','Read-IsolationGateMetadata','Read-IsolationResizeRevalidation')
$allowed=@($helperNames)+@('Get-AutoField','Get-Content','ConvertFrom-Json','Get-FileHash','Test-InputIsolationOwnedEvidence','Join-Path','Add-Member','ForEach-Object','Sort-Object','git','Out-Null')
foreach($name in $helperNames){
    $definition=One-RunnerNode $gates {param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name} "helper $name"
    Check-Runner (@($definition.Body.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -cnotin $allowed},$true)).Count -eq 0) "$name uses only pure/stubbed seams"
    . ([scriptblock]::Create($definition.Extent.Text))
}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'));$root=Join-Path $repo 'uat/r1c4b-input-isolation'
$fixtureHead='2'*40;$fixtureValidatorHash='F'*64;$fixtureArtifactHash='E'*64
$originalLogHash='BB1E1F90D58A0B21C5A0B3F65F3A3DDB3994388C96FFE4C6A0F3ED5EF174B76F'
$originalMetadataHash='D1B7C425CDB53A6356BC9B23C72376857BBE12996234F7032A5846B9B15BEB09'
$originalBinaryHash='43289B87236C08E68EAC2A23BCA1AD9923454DDB21EBF4DE84447083933DEC6D'
$originalSourceHash='CEBB558B403B90D3F1A55C521358884BB6C838624EA6C0BC8C5BA481192BC2FB'
$oldPrefix=Join-Path $root '20260926T201554647Z-Debug-BottomResize-3e6236754a004decb816079530b3861b'
$oldLog=$oldPrefix+'.jsonl';$oldMetadataPath=$oldPrefix+'.metadata.json';$artifactPath=$oldPrefix+'.revalidation.json'
$fixtureMetadataPath=Join-Path $root ('synthetic-Debug-Move-'+('a'*32)+'.metadata.json')
$fixtureLogPath=Join-Path $root ('synthetic-Debug-Move-'+('a'*32)+'.jsonl')
$debugBinary=Join-Path $repo 'out/r1c4b-live-magnet-debug/src/platform/windows/Debug/panebind-magnet-takeover-probe.exe'
$sourcePath=Join-Path $repo 'src/platform/windows/operations/magnet_takeover_probe.cpp'
$validatorPath=Join-Path $PSScriptRoot 'r1c4b-input-isolation-validation.ps1'
# AST-created scriptblocks have an empty automatic PSScriptRoot. Provide the
# one registered source-file context in memory; production helpers are unchanged.
function Join-Path([string]$Path,[string]$ChildPath){
    if([string]::IsNullOrEmpty($Path) -and $ChildPath -ceq 'r1c4b-input-isolation-validation.ps1'){return $script:validatorPath}
    return [IO.Path]::GetFullPath([IO.Path]::Combine($Path,$ChildPath))
}
function Get-Content([string]$LiteralPath,[string]$Encoding){
    if($LiteralPath -ceq $script:fixtureMetadataPath){return ($script:fixtureMetadata|ConvertTo-Json -Depth 20 -Compress)}
    if($LiteralPath -ceq $script:oldMetadataPath){return ($script:oldMetadata|ConvertTo-Json -Depth 20 -Compress)}
    if($LiteralPath -ceq $script:artifactPath){return ($script:artifact|ConvertTo-Json -Depth 20 -Compress)}
    $rows=if($LiteralPath -ceq $script:fixtureLogPath){$script:fixtureRows}elseif($LiteralPath -ceq $script:oldLog){$script:oldRows}else{throw 'fixture attempted an unregistered read'}
    foreach($row in $rows){$row|ConvertTo-Json -Depth 10 -Compress}
}
function Get-FileHash([string]$LiteralPath,[string]$Algorithm){
    if($Algorithm -cne 'SHA256' -or -not $script:hashes.ContainsKey($LiteralPath)){throw "fixture attempted an unregistered hash: $LiteralPath"}
    return [pscustomobject]@{Hash=$script:hashes[$LiteralPath]}
}
function Test-InputIsolationOwnedEvidence([string]$Path){
    ++$script:validatorCalls
    if($Path -ceq $script:fixtureLogPath){return $script:fixtureVerdict}
    if($Path -ceq $script:oldLog){return $script:resizeVerdict}
    throw 'fixture validator path mismatch'
}
function git{
    $script:gitCalls+=@(($args -join ' '))
    $script:LASTEXITCODE=if($args[0] -ceq 'merge-base'){$script:ancestorExit}elseif($args[0] -ceq 'diff'){$script:nativeDiffExit}else{throw 'fixture attempted unregistered Git'}
}
foreach($seam in @('Get-Content','Get-FileHash','Test-InputIsolationOwnedEvidence','git')){Check-Runner ((Get-Command $seam).CommandType -eq 'Function') "$seam is an in-memory seam"}
function New-RunnerVerdict([string]$Operation,[long]$Nonce){
    return [pscustomobject]@{Result='PASS';Operation=$Operation;ProductHandoffAuthority='PASS';TestInputIsolation='PASS';Cancel='PASS_WITH_TERMINAL_SETTLEMENT';Handoff='PASS';Takeover='PASS';RunNonce=$Nonce;ForegroundContract='verified_global_foreground_v2';HandoffContract='winevent_end_barrier_v1';DiagnosticContract='separated_authority_v1';AuthorityContract='product_gesture_v1';IsolationContract='post_end_shield_v1';TimingTickSamples=[pscustomobject]@{};NativeWrites=[long]19;HandoffNativeCalls=[long]1;ShieldNativeCalls=[long]20;RawPackets=[long]27;NativeDragAfterWinEventEnd=[long]0;ContinuationQuanta=[long]18}
}
function Reset-RunnerFixture{
    $script:state=[ordered]@{ExecutedHEAD=$script:fixtureHead;ValidatorScriptSHA256=$script:fixtureValidatorHash;BinarySHA256=@{Debug=$script:originalBinaryHash;Release='C'*64};Statistics=@{}}
    foreach($configuration in @('Debug','Release')){foreach($operation in @('Move','BottomResize')){$script:state.Statistics[$configuration+$operation]=New-IsolationStatistics $configuration $operation}}
    $script:nonces=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $script:fixtureMetadata=[pscustomobject]@{Schema='r1c4b-input-isolation-run/v1';Configuration='Debug';Operation='Move';ExecutedHEAD=$script:fixtureHead;AfterHEAD=$script:fixtureHead;WorktreeDirty=$false;AfterWorktreeDirty=$false;ImplementationUnchanged=$true;BinarySHA256=$script:originalBinaryHash;AfterBinarySHA256=$script:originalBinaryHash;ValidatorScriptSHA256=$script:fixtureValidatorHash;AfterValidatorScriptSHA256=$script:fixtureValidatorHash;EvidencePath=$script:fixtureLogPath;RunId='a'*32;LogSHA256='B'*64;ProbeExitCode=0;CurrentContractsVerified=$true;Result=[pscustomobject]@{Result='PASS';Reasons=@()}}
    $script:fixtureVerdict=New-RunnerVerdict Move 2
    $script:resizeVerdict=New-RunnerVerdict BottomResize 1
    $script:fixtureRows=@([pscustomobject]@{type='startup';sequence=[long]1;qpc=[long]1134817000000;qpc_frequency=[long]10000000;run_nonce=[long]2},[pscustomobject]@{type='raw_quantum';sequence=[long]2;qpc=[long]1134817000100;native_calls=[long]1},[pscustomobject]@{type='shutdown';sequence=[long]3;qpc=[long]1134817001000})
    $script:oldRows=@(for($i=1;$i -le 460;++$i){[pscustomobject]@{type=$(if($i -eq 1){'startup'}elseif($i -eq 460){'shutdown'}else{'synthetic'});sequence=[long]$i;qpc=([long]1134810000000+[long]$i*[long]1000);qpc_frequency=[long]10000000;run_nonce=[long]1}})
    $script:oldMetadata=[pscustomobject]@{Schema='r1c4b-input-isolation-run/v1';Configuration='Debug';Operation='BottomResize';ExecutedHEAD='a67d3049c1027e8b2b82f8e122041b71ff5043ca';AfterHEAD='a67d3049c1027e8b2b82f8e122041b71ff5043ca';WorktreeDirty=$false;AfterWorktreeDirty=$false;ImplementationUnchanged=$true;ProbeExitCode=0;CurrentContractsVerified=$false;Result=[pscustomobject]@{Result='INVALID_EVIDENCE'};EvidencePath=$script:oldLog;LogSHA256=$script:originalLogHash;BinarySHA256=$script:originalBinaryHash;AfterBinarySHA256=$script:originalBinaryHash;RunId='3e6236754a004decb816079530b3861b'}
    $script:artifact=[pscustomobject]@{schema='r1c4b-input-isolation-revalidation/v1';original_evidence_path=$script:oldLog;original_evidence_sha256=$script:originalLogHash;original_evidence_sha256_after=$script:originalLogHash;original_metadata_path=$script:oldMetadataPath;original_metadata_sha256=$script:originalMetadataHash;original_metadata_sha256_after=$script:originalMetadataHash;evidence_implementation_sha='a67d3049c1027e8b2b82f8e122041b71ff5043ca';validator_fix_sha='1'*40;validator_script_sha256=$script:fixtureValidatorHash;replay_time='2000-01-01T00:00:00Z';original_result='INVALID_EVIDENCE';replay_result='PASS';semantic_contract_version='post_end_shield_v1';numeric_fix_only=$true;first_semantic_failure='NONE';replay_verdict=$script:resizeVerdict;CorrectedValidatorVerified=$true;ValidatorWorktreeClean=$true;OriginalBinarySHA256=$script:originalBinaryHash;AfterBinarySHA256=$script:originalBinaryHash;RuntimeSourceSHA256=$script:originalSourceHash;AfterRuntimeSourceSHA256=$script:originalSourceHash}
    $script:hashes=@{};$script:hashes[$script:fixtureMetadataPath]='D'*64;$script:hashes[$script:fixtureLogPath]='B'*64;$script:hashes[$script:oldLog]=$script:originalLogHash;$script:hashes[$script:oldMetadataPath]=$script:originalMetadataHash;$script:hashes[$script:artifactPath]=$script:fixtureArtifactHash;$script:hashes[$script:debugBinary]=$script:originalBinaryHash;$script:hashes[$script:validatorPath]=$script:fixtureValidatorHash;$script:hashes[$script:sourcePath]=$script:originalSourceHash
    $script:ancestorExit=0;$script:nativeDiffExit=0;$script:gitCalls=@();$script:validatorCalls=0
}

Reset-RunnerFixture
$m=Read-IsolationGateMetadata $fixtureMetadataPath Debug Move
Check-Runner ($m.ValidatedStartQpc -is [long] -and $m.ValidatedEndQpc -is [long] -and $m.ValidatedQpcFrequency -is [long]) 'clock projections are explicitly Int64'
Check-Runner ($state.Statistics.DebugMove.Gestures -eq 1 -and $state.Statistics.DebugMove.MaxSourceWrites -eq 19 -and $state.Statistics.DebugMove.MaxWritesPerQuantum -eq 1 -and $state.Statistics.DebugBottomResize.Gestures -eq 0) 'counts are separated by configuration/operation, shield is not a Source write'
foreach($field in @('ExecutedHEAD','AfterHEAD','BinarySHA256','AfterBinarySHA256','ValidatorScriptSHA256','AfterValidatorScriptSHA256','LogSHA256','Schema','Configuration','Operation')){
    Reset-RunnerFixture;$fixtureMetadata.$field='wrong';Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug Move} "metadata rejects $field"
}
foreach($field in @('WorktreeDirty','AfterWorktreeDirty')){Reset-RunnerFixture;$fixtureMetadata.$field=$true;Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug Move} "metadata rejects $field"}
foreach($field in @('ImplementationUnchanged','CurrentContractsVerified')){Reset-RunnerFixture;$fixtureMetadata.$field=$false;Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug Move} "metadata rejects false $field"}
foreach($field in @('Result','Operation','ProductHandoffAuthority','TestInputIsolation','Cancel','Handoff','Takeover','IsolationContract')){
    Reset-RunnerFixture;$fixtureVerdict.$field='wrong';Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug Move} "fresh verdict rejects $field"
}
Reset-RunnerFixture;$fixtureMetadata.ProbeExitCode=2;Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug Move} 'probe failure rejected'
Reset-RunnerFixture;$fixtureMetadata.Result.Result='FAIL';Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug Move} 'current metadata must itself record PASS'
Reset-RunnerFixture;$fixtureMetadata.RunId='b'*32;Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug Move} 'run ID binding rejected'
Reset-RunnerFixture;$fixtureMetadata.EvidencePath=Join-Path $repo 'outside.jsonl';Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug Move} 'outside log rejected'
Reset-RunnerFixture;$null=Read-IsolationGateMetadata $fixtureMetadataPath Debug Move;Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug Move} 'nonce reuse rejected'
foreach($value in @([long]2147483647,[long]2147483648,[long]3000000000,[long]900000000000,[long]1134816073663,[long]1500000000000)){
    $number=Convert-IsolationGateInt64 $value fixture -NonNegative
    Check-Runner ($number -is [long] -and $number -eq $value) "Int64 boundary $value"
    Reset-RunnerFixture;$fixtureRows[0].qpc=$value;$fixtureRows[1].qpc=$value+[long]10000;$fixtureRows[2].qpc=$value+[long]20000
    $m=Read-IsolationGateMetadata $fixtureMetadataPath Debug Move
    Check-Runner ($m.ValidatedStartQpc -is [long] -and $m.ValidatedEndQpc-$m.ValidatedStartQpc -eq 20000) "large typed clock/difference $value"
}
foreach($bad in @($null,$true,'1134816073663',-1,1.5)){Reject-Runner {Convert-IsolationGateInt64 $bad 'invalid QPC' -NonNegative} "invalid QPC rejected: $bad"}
Reset-RunnerFixture;$fixtureRows[1].qpc=$fixtureRows[0].qpc-1;Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug Move} 'clock reversal rejected'
Reset-RunnerFixture;$fixtureRows[0].qpc_frequency=0;Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug Move} 'zero QPC frequency rejected'
Reset-RunnerFixture;$fixtureRows[1].sequence=1.5;Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug Move} 'noninteger sequence rejected'
Reset-RunnerFixture
$fixtureVerdict.TimingTickSamples=[pscustomobject]@{NativeExitToWinEventEnd=@([long]2147483647,[long]2147483648,[long]3000000000,[long]900000000000,[long]1134816073663,[long]1500000000000);HandoffWriteDuration=[long]1134816073663;RawReceiptToNativeWrite=@();RawReceiptToOwnerQuantum=$null}
$null=Read-IsolationGateMetadata $fixtureMetadataPath Debug Move
$group=$state.Statistics.DebugMove
Check-Runner ($group.TimingTicks.Count -eq 7 -and @($group.TimingTicks.NativeExitToWinEventEnd|Where-Object {$_ -isnot [long]}).Count -eq 0) 'all original samples stay Int64 before interpolation/ms'
Update-IsolationStatistics
Check-Runner ($group.TimingStatistics.NativeExitToWinEventEnd.P50Ticks -eq [decimal]451500000000 -and $group.TimingStatistics.NativeExitToWinEventEnd.P50Ms -eq [decimal]45150000) 'large pipeline median is calculated in ticks then ms'
Check-Runner ($group.TimingStatistics.NativeExitToWinEventEnd.P95Ticks -eq [decimal]1408704018415.75 -and $group.TimingStatistics.NativeExitToWinEventEnd.P95Ms -eq [decimal]140870401.841575) 'large p95 interpolation stays decimal until ms'
Check-Runner ($group.TimingMilliseconds.HandoffWriteDuration.Count -eq 1 -and $group.TimingMilliseconds.HandoffWriteDuration[0] -eq [decimal]113481607.3663) 'scalar tick sample is consumed once'
foreach($key in @('DebugMove','DebugBottomResize','ReleaseMove','ReleaseBottomResize')){
    Check-Runner ($state.Statistics[$key].TimingStatistics.Count -eq 7) "seven metrics retained for $key"
    Check-Runner ($state.Statistics[$key].TimingStatistics.RawReceiptToNativeWrite.Count -eq 0 -and $null -eq $state.Statistics[$key].TimingStatistics.RawReceiptToNativeWrite.P50Ms -and $null -eq $state.Statistics[$key].TimingStatistics.RawReceiptToNativeWrite.P95Ms) "empty/null $key metrics retain null quantiles"
}
foreach($fraction in @(0.50,0.95)){Check-Runner ($null -eq (Get-IsolationQuantile @() $fraction)) "empty quantile $fraction";Check-Runner ((Get-IsolationQuantile ([long]1134816073663) $fraction) -eq [decimal]1134816073663) "scalar quantile $fraction"}
Check-Runner ((Get-IsolationQuantile @([long]-1,[long]1,[long]4) 0.5) -eq 1) 'signed ticks are not clamped'
Reject-Runner {Get-IsolationQuantile @(1.5,2) 0.5} 'noninteger original ticks rejected'

Reset-RunnerFixture
$resize=Read-IsolationResizeRevalidation $artifactPath $fixtureArtifactHash
Check-Runner ($validatorCalls -eq 1 -and $gitCalls.Count -eq 2 -and $resize.ReplayArtifactSHA256 -ceq $fixtureArtifactHash) 'artifact prerequisite freshly validates original raw and checks Git seams'
Check-Runner (-not $oldMetadata.CurrentContractsVerified -and $oldMetadata.Result.Result -ceq 'INVALID_EVIDENCE' -and $state.Statistics.DebugBottomResize.Gestures -eq 0) 'immutable pipeline history remains INVALID_EVIDENCE and prerequisites are excluded from repetition statistics'
foreach($field in @('schema','original_result','replay_result','first_semantic_failure','semantic_contract_version','original_evidence_sha256','original_evidence_sha256_after','original_metadata_sha256','original_metadata_sha256_after','evidence_implementation_sha','validator_fix_sha','validator_script_sha256','OriginalBinarySHA256','AfterBinarySHA256','RuntimeSourceSHA256','AfterRuntimeSourceSHA256')){
    Reset-RunnerFixture;$artifact.$field='wrong';Reject-Runner {Read-IsolationResizeRevalidation $artifactPath $fixtureArtifactHash} "replay rejects $field"
}
foreach($field in @('numeric_fix_only','CorrectedValidatorVerified','ValidatorWorktreeClean')){Reset-RunnerFixture;$artifact.$field=$false;Reject-Runner {Read-IsolationResizeRevalidation $artifactPath $fixtureArtifactHash} "replay rejects false $field"}
Reset-RunnerFixture;$artifact.numeric_fix_only='true';Reject-Runner {Read-IsolationResizeRevalidation $artifactPath $fixtureArtifactHash} 'replay booleans cannot be relabeled strings'
foreach($path in @($artifactPath,$oldLog,$oldMetadataPath,$validatorPath,$sourcePath,$debugBinary)){
    Reset-RunnerFixture;$hashes[$path]='0'*64;Reject-Runner {Read-IsolationResizeRevalidation $artifactPath $fixtureArtifactHash} "replay rejects current hash mismatch $path"
}
Reset-RunnerFixture;$ancestorExit=1;Reject-Runner {Read-IsolationResizeRevalidation $artifactPath $fixtureArtifactHash} 'fix SHA must be an ancestor'
Reset-RunnerFixture;$nativeDiffExit=1;Reject-Runner {Read-IsolationResizeRevalidation $artifactPath $fixtureArtifactHash} 'changed native source history rejected'
Reset-RunnerFixture;$oldMetadata.CurrentContractsVerified=$true;Reject-Runner {Read-IsolationResizeRevalidation $artifactPath $fixtureArtifactHash} 'rewritten original metadata rejected'
Reset-RunnerFixture;$oldRows=@($oldRows|Select-Object -First 459);Reject-Runner {Read-IsolationResizeRevalidation $artifactPath $fixtureArtifactHash} 'incomplete original raw rejected'
Reset-RunnerFixture;$artifact.replay_verdict=Copy-RunnerFixture $resizeVerdict;$resizeVerdict.Result='FAIL';Reject-Runner {Read-IsolationResizeRevalidation $artifactPath $fixtureArtifactHash} 'reported replay PASS cannot override fresh semantic failure'
Reset-RunnerFixture;$artifact.replay_verdict=Copy-RunnerFixture $resizeVerdict;$resizeVerdict.Result='INVALID_EVIDENCE';Reject-Runner {Read-IsolationResizeRevalidation $artifactPath $fixtureArtifactHash} 'fresh invalid evidence stops prerequisite'
Reset-RunnerFixture
$resize=Read-IsolationResizeRevalidation $artifactPath $fixtureArtifactHash
$move=Read-IsolationGateMetadata $fixtureMetadataPath Debug Move $false
$initial=One-RunnerNode $gates {param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -ceq 'Assert-IsolationGate' -and $n.Extent.Text.Contains('Initial complete Resize replay must precede')} 'initial replay->Move ordering'
$orderExpression=[scriptblock]::Create($initial.CommandElements[1].Extent.Text)
Check-Runner (& $orderExpression) 'corrected Resize replay precedes current distinct Move'
$move.ValidatedStartQpc=$resize.ValidatedEndQpc;Check-Runner (-not (& $orderExpression)) 'equal clock boundary is not ordered'
$move.ValidatedStartQpc=$resize.ValidatedEndQpc-1;Check-Runner (-not (& $orderExpression)) 'reversed prerequisite clock rejected'
$move.ValidatedStartQpc=$resize.ValidatedEndQpc+1;$move.EvidencePath=$resize.EvidencePath;Check-Runner (-not (& $orderExpression)) 'same raw log cannot be both prerequisites'
$stageLoop=One-RunnerNode $gates {param($n) $n -is [Management.Automation.Language.ForEachStatementAst] -and $n.Variable.Extent.Text -ceq '$stage'} 'eight operation-specific stages'
Check-Runner (@($stageLoop.Condition.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst]},$true)).Count -eq 0) 'stage plan is pure literal data'
$stages=@(& ([scriptblock]::Create($stageLoop.Condition.Extent.Text)))
Check-Runner (($stages|ForEach-Object {"$($_.Name):$($_.Configuration):$($_.Operation):$($_.Count)"}) -join ',' -ceq 'DebugMoveSmoke:Debug:Move:5,DebugResizeSmoke:Debug:BottomResize:5,ReleaseMoveSmoke:Release:Move:5,ReleaseResizeSmoke:Release:BottomResize:5,DebugMoveFormal:Debug:Move:20,DebugResizeFormal:Debug:BottomResize:20,ReleaseMoveFormal:Release:Move:20,ReleaseResizeFormal:Release:BottomResize:20') 'four separate smoke groups precede all four formal groups'
$repetition=One-RunnerNode $stageLoop {param($n) $n -is [Management.Automation.Language.ForStatementAst]} 'bounded repetition'
Check-Runner ($repetition.Condition.Extent.Text -ceq '$repetition -le $stage.Count' -and ($repetition.Iterator.Extent.Text -join '') -ceq '++$repetition') 'each repetition advances once'
$counter=One-RunnerNode $stageLoop {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$state[$stage.Name]'} 'per-operation PASS counter'
$independent=One-RunnerNode $stageLoop {param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -ceq 'Read-IsolationGateMetadata'} 'independent operation verification'
Check-Runner ($counter.Right.Extent.Text -ceq '$repetition' -and $counter.Extent.StartOffset -gt $independent.Extent.EndOffset) 'count advances immediately after that operation independently passes (not after a mixed pair)'
$failure=One-RunnerNode $stageLoop {param($n) $n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text.Contains('$code -ne 0')} 'first failure'
Check-Runner ($failure.Extent.EndOffset -lt $independent.Extent.StartOffset -and $failure.Extent.Text.Contains('$state.FirstFailure=$item') -and @($failure.FindAll({param($n) $n -is [Management.Automation.Language.ThrowStatementAst]},$true)).Count -eq 1) 'first failure stops before verification/count/next repetition'
$failureExpression=[scriptblock]::Create($failure.Clauses[0].Item1.Extent.Text)
foreach($case in @(@{Code=0;Files=1;Fail=$false},@{Code=2;Files=1;Fail=$true},@{Code=0;Files=0;Fail=$true},@{Code=0;Files=2;Fail=$true})){
    $code=$case.Code;$files=@(for($i=0;$i -lt $case.Files;++$i){$i});Check-Runner ((& $failureExpression) -eq $case.Fail) "first failure expression code=$code files=$($files.Count)"
}
Check-Runner (@($repetition.Body.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -ceq 'powershell.exe'},$true)).Count -eq 1) 'one child-runner call per repetition (not executed)'
Check-Runner (@($repetition.Body.FindAll({param($n) $n -is [Management.Automation.Language.ForStatementAst] -or $n -is [Management.Automation.Language.ForEachStatementAst] -or $n -is [Management.Automation.Language.WhileStatementAst] -or $n -is [Management.Automation.Language.DoWhileStatementAst]},$true)).Count -eq 0) 'no retry loop within an operation'
$smokeGate=One-RunnerNode $stageLoop {param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -ceq 'Assert-IsolationGate' -and $n.Extent.Text.Contains('All four operation-specific smoke')} 'formal precondition'
$smokeExpression=[scriptblock]::Create($smokeGate.CommandElements[1].Extent.Text)
$state=[ordered]@{DebugMoveSmoke=5;DebugResizeSmoke=5;ReleaseMoveSmoke=5;ReleaseResizeSmoke=5}
Check-Runner (& $smokeExpression) 'four complete smoke groups authorize formal'
foreach($name in @($state.Keys)){$state[$name]=4;Check-Runner (-not (& $smokeExpression)) "formal rejected without $name";$state[$name]=5}
$topTry=One-RunnerNode $gates {param($n) $n -is [Management.Automation.Language.TryStatementAst] -and $n.Extent.Text.Contains('$state.ExecutedHEAD=git rev-parse HEAD')} 'terminal aggregate catch'
Check-Runner ($topTry.CatchClauses.Count -eq 1 -and $topTry.CatchClauses[0].Body.Extent.Text.Contains('$state.FirstFailure=$state.Runs[-1]') -and $topTry.CatchClauses[0].Body.Extent.Text.Contains("$"+"state.Result='STOPPED'")) 'fresh validator exception is STOPPED and preserves last attempted operation'
Check-Runner ($initial.Extent.EndOffset -lt $stageLoop.Extent.StartOffset) 'replay/current Move gates precede any repetition'
Write-Host "input-isolation-runner synthetic_only=true checks=$checks PASS; no child CLI/GUI/input/real Git/original evidence; validator/Git stubs only"
