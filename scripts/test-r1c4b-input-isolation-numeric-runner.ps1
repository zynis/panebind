Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# Numeric-only synthetic tests. Actual AST helpers, no runner top-level,
# original evidence, child CLI, Git, GUI, input or disk-writing seam.
$checks=0
function Check-NumericRunner([bool]$Value,[string]$Reason){if(-not $Value){throw "numeric runner fixture: $Reason"};$script:checks++}
function One-NumericNode($Ast,[scriptblock]$Predicate,[string]$Label){
    $nodes=@($Ast.FindAll($Predicate,$true));Check-NumericRunner ($nodes.Count -eq 1) "unique $Label";return $nodes[0]
}
function Get-AutoField($Value,[string]$Name){$p=$Value.PSObject.Properties[$Name];if($null -eq $p){return $null};return $p.Value}
function Reject-NumericRunner([scriptblock]$Action,[string]$Reason){
    $rejected=$false;try{$null=& $Action}catch{$rejected=$true};Check-NumericRunner $rejected $Reason
}
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'run-r1c4b-input-isolation-gates.ps1'),[ref]$tokens,[ref]$errors)
Check-NumericRunner (@($errors).Count -eq 0) 'aggregate parses'
$names=@('Assert-IsolationGate','Convert-IsolationGateInt64','Convert-IsolationGateMilliseconds','Get-IsolationQuantile','Get-IsolationGateClock','New-IsolationStatistics','Add-IsolationMeasurements','Update-IsolationStatistics','Assert-IsolationVerdict','Bind-IsolationClock','Read-IsolationGateMetadata')
$allowed=@($names)+@('Get-AutoField','Get-Content','ConvertFrom-Json','Get-FileHash','Test-InputIsolationOwnedEvidence','Join-Path','Add-Member','ForEach-Object','Sort-Object')
foreach($name in $names){
    $node=One-NumericNode $ast {param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name} $name
    Check-NumericRunner (@($node.Body.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -cnotin $allowed},$true)).Count -eq 0) "$name only pure/stubbed seams"
    Check-NumericRunner ($node.Extent.Text -notmatch '\[Math\]::(?:Max|Min)\(') "$name no overload-sensitive Max/Min"
    $narrow=@($node.Body.FindAll({param($n) $n -is [Management.Automation.Language.ConvertExpressionAst] -and $n.Type.TypeName.FullName -in @('int','Int32','System.Int32')},$true))
    if($name -ceq 'Get-IsolationQuantile'){
        Check-NumericRunner ($narrow.Count -eq 2 -and @($narrow|Where-Object {$_.Child.Extent.Text -notmatch '^\[Math\]::(?:Floor|Ceiling)\(\$index\)$'}).Count -eq 0) 'only bounded array indices use Int32'
    }else{Check-NumericRunner ($narrow.Count -eq 0) "$name never narrows QPC/ticks to Int32"}
    . ([scriptblock]::Create($node.Extent.Text))
}
$metricAssignment=One-NumericNode $ast {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$metricNames'} 'actual metric names'
$metricNames=@(& ([scriptblock]::Create($metricAssignment.Right.Extent.Text)))
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'));$root=Join-Path $repo 'uat/r1c4b-input-isolation'
$id='a'*32;$metadataPath=Join-Path $root ("synthetic-Debug-BottomResize-$id.metadata.json");$logPath=Join-Path $root ("synthetic-Debug-BottomResize-$id.jsonl")
$binaryHash='A'*64;$logHash='B'*64;$metadataHash='D'*64;$validatorHash='F'*64;$head='1'*40
$validatorPath=Join-Path $PSScriptRoot 'r1c4b-input-isolation-validation.ps1'
# Load serializer before stubs, so module auto-import cannot overwrite them.
$null=[pscustomobject]@{synthetic_only=$true}|ConvertTo-Json -Compress
function Join-Path([string]$Path,[string]$ChildPath){
    if([string]::IsNullOrEmpty($Path) -and $ChildPath -ceq 'r1c4b-input-isolation-validation.ps1'){return $script:validatorPath}
    return [IO.Path]::GetFullPath([IO.Path]::Combine($Path,$ChildPath))
}
function Get-Content([string]$LiteralPath,[string]$Encoding){
    if($LiteralPath -ceq $script:metadataPath){return ($script:fixtureMetadata|ConvertTo-Json -Depth 16 -Compress)}
    if($LiteralPath -ceq $script:logPath){foreach($row in $script:fixtureRows){$row|ConvertTo-Json -Depth 8 -Compress};return}
    throw 'numeric fixture attempted unregistered read'
}
function Get-FileHash([string]$LiteralPath,[string]$Algorithm){
    if($Algorithm -cne 'SHA256'){throw 'numeric hash algorithm changed'}
    $hash=if($LiteralPath.EndsWith('panebind-magnet-takeover-probe.exe',[StringComparison]::Ordinal)){$script:binaryHash}elseif($LiteralPath -ceq $script:logPath){$script:logHash}elseif($LiteralPath -ceq $script:metadataPath){$script:metadataHash}elseif($LiteralPath -ceq $script:validatorPath){$script:validatorHash}else{throw 'numeric fixture attempted unregistered hash'}
    return [pscustomobject]@{Hash=$hash}
}
function Test-InputIsolationOwnedEvidence([string]$Path){if($Path -cne $script:logPath){throw 'numeric validator path mismatch'};return $script:fixtureVerdict}
foreach($seam in @('Get-Content','Get-FileHash','Test-InputIsolationOwnedEvidence','Join-Path')){Check-NumericRunner ((Get-Command $seam).CommandType -eq 'Function') "$seam remains an in-memory seam"}
function Reset-NumericRunnerFixture([long]$Start,[long]$End){
    $script:state=[ordered]@{ExecutedHEAD=$script:head;ValidatorScriptSHA256=$script:validatorHash;BinarySHA256=@{Debug=$script:binaryHash;Release=$script:binaryHash};Statistics=@{}}
    foreach($configuration in @('Debug','Release')){foreach($operation in @('Move','BottomResize')){$script:state.Statistics[$configuration+$operation]=New-IsolationStatistics $configuration $operation}}
    $script:nonces=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $script:fixtureMetadata=[pscustomobject]@{Schema='r1c4b-input-isolation-run/v1';Configuration='Debug';Operation='BottomResize';ExecutedHEAD=$script:head;AfterHEAD=$script:head;WorktreeDirty=$false;AfterWorktreeDirty=$false;ImplementationUnchanged=$true;BinarySHA256=$script:binaryHash;AfterBinarySHA256=$script:binaryHash;ValidatorScriptSHA256=$script:validatorHash;AfterValidatorScriptSHA256=$script:validatorHash;EvidencePath=$script:logPath;RunId=$script:id;LogSHA256=$script:logHash;ProbeExitCode=0;CurrentContractsVerified=$true;Result=[pscustomobject]@{Result='PASS';Reasons=@()}}
    $script:fixtureVerdict=[pscustomobject]@{Result='PASS';Operation='BottomResize';ProductHandoffAuthority='PASS';TestInputIsolation='PASS';Cancel='PASS_WITH_TERMINAL_SETTLEMENT';Handoff='PASS';Takeover='PASS';RunNonce=[long]197157086769502;ForegroundContract='verified_global_foreground_v2';HandoffContract='winevent_end_barrier_v1';DiagnosticContract='separated_authority_v1';AuthorityContract='product_gesture_v1';IsolationContract='post_end_shield_v1';TimingTickSamples=[pscustomobject]@{};NativeWrites=[long]19;HandoffNativeCalls=[long]1;ShieldNativeCalls=[long]20;RawPackets=[long]27;NativeDragAfterWinEventEnd=[long]0;ContinuationQuanta=[long]18}
    $script:fixtureRows=@([pscustomobject]@{type='startup';sequence=[long]1;qpc=$Start;qpc_frequency=[long]10000000;run_nonce=[long]197157086769502},[pscustomobject]@{type='shutdown';sequence=[long]2;qpc=$End})
}
foreach($start in @([long]2147483647,[long]2147483648,[long]3000000000,[long]900000000000,[long]1134816073663,[long]1500000000000)){
    Reset-NumericRunnerFixture $start ($start+[long]10000)
    $m=Read-IsolationGateMetadata $metadataPath Debug BottomResize
    Check-NumericRunner ($m.ValidatedStartQpc -eq $start -and $m.ValidatedEndQpc -eq $start+[long]10000) "exact QPC boundary $start"
    Check-NumericRunner ($m.ValidatedStartQpc -is [long] -and $m.ValidatedEndQpc -is [long] -and $m.ValidatedQpcFrequency -is [long]) "typed clock values $start"
    Check-NumericRunner ($m.ValidatedEndQpc-$m.ValidatedStartQpc -eq [long]10000) "Int64 duration $start"
}
foreach($bad in @($null,$true,'1134816073663',-1,1.5)){Reject-NumericRunner {Convert-IsolationGateInt64 $bad fixture -NonNegative} "invalid QPC rejected $bad"}
Reset-NumericRunnerFixture 1134816073663 1134816083663
$fixtureRows[-1].qpc=$fixtureRows[0].qpc-1;Reject-NumericRunner {Read-IsolationGateMetadata $metadataPath Debug BottomResize} 'clock reversal rejected'
Reset-NumericRunnerFixture 1134816073663 1134816083663
$fixtureVerdict.TimingTickSamples=[pscustomobject]@{NativeExitToWinEventEnd=@([long]2147483647,[long]2147483648,[long]3000000000,[long]900000000000,[long]1134816073663,[long]1500000000000);HandoffWriteDuration=[long]1134816073663;RawReceiptToNativeWrite=@();RawReceiptToOwnerQuantum=$null}
$null=Read-IsolationGateMetadata $metadataPath Debug BottomResize
$group=$state.Statistics.DebugBottomResize
Check-NumericRunner ($group.TimingTicks.Count -eq 7 -and $group.TimingTicks.NativeExitToWinEventEnd.Count -eq 6) 'all seven metric arrays and six boundary samples are retained'
Check-NumericRunner (@($group.TimingTicks.NativeExitToWinEventEnd|Where-Object {$_ -isnot [long]}).Count -eq 0 -and $group.TimingTicks.HandoffWriteDuration[0] -is [long]) 'arrays/scalar stay signed Int64 before ms'
Update-IsolationStatistics
foreach($index in 0..5){
    [decimal]$expected=[decimal]$fixtureVerdict.TimingTickSamples.NativeExitToWinEventEnd[$index]*[decimal]1000/[decimal]10000000
    Check-NumericRunner ($group.TimingMilliseconds.NativeExitToWinEventEnd[$index] -eq $expected) "exact decimal tick-to-ms $index"
}
Check-NumericRunner ($group.TimingStatistics.NativeExitToWinEventEnd.P50Ticks -eq [decimal]451500000000 -and $group.TimingStatistics.NativeExitToWinEventEnd.P50Ms -eq [decimal]45150000) 'large p50 uses ticks first'
Check-NumericRunner ($group.TimingStatistics.NativeExitToWinEventEnd.P95Ticks -eq [decimal]1408704018415.75 -and $group.TimingStatistics.NativeExitToWinEventEnd.P95Ms -eq [decimal]140870401.841575) 'large p95 uses decimal interpolation before ms'
Check-NumericRunner ($group.TimingMilliseconds.HandoffWriteDuration.Count -eq 1 -and $group.TimingMilliseconds.HandoffWriteDuration[0] -eq [decimal]113481607.3663) 'scalar sample is not lost'
foreach($key in @('DebugMove','DebugBottomResize','ReleaseMove','ReleaseBottomResize')){
    Check-NumericRunner ($state.Statistics[$key].TimingStatistics.Count -eq 7) "all metrics present for $key"
    Check-NumericRunner ($state.Statistics[$key].TimingStatistics.RawReceiptToNativeWrite.Count -eq 0 -and $null -eq $state.Statistics[$key].TimingStatistics.RawReceiptToNativeWrite.P50Ms -and $null -eq $state.Statistics[$key].TimingStatistics.RawReceiptToNativeWrite.P95Ms) "empty/null $key produce null percentiles"
}
foreach($fraction in @(0.50,0.95)){Check-NumericRunner ($null -eq (Get-IsolationQuantile @() $fraction)) "empty percentile $fraction";Check-NumericRunner ((Get-IsolationQuantile ([long]1134816073663) $fraction) -eq [decimal]1134816073663) "scalar percentile $fraction"}
Check-NumericRunner ((Get-IsolationQuantile @([long]-1,[long]1,[long]4) 0.50) -eq 1) 'signed latency is not silently clamped'
Write-Host "input-isolation numeric runner synthetic_only=true checks=$checks PASS; actual helper extraction; no child CLI/Git/GUI/input/original evidence"
