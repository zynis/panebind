Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# Synthetic runner policy tests only. Never evaluate a runner top-level program,
# launch a CLI/probe, send input, query windows, execute Git, or read real UAT.
# Metadata/hash/validator results below are in-memory stubs, not real evidence.
$checks=0
function Check-Runner([bool]$Value,[string]$Reason){
    if(-not $Value){throw "input isolation runner fixture: $Reason"}
    $script:checks++
}
function Read-RunnerAst([string]$Name){
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $Name),[ref]$tokens,[ref]$errors)
    Check-Runner (@($errors).Count -eq 0) "parse $Name"
    return $ast
}
function One-RunnerNode($Ast,[scriptblock]$Predicate,[string]$Label){
    $nodes=@($Ast.FindAll($Predicate,$true))
    Check-Runner ($nodes.Count -eq 1) "unique $Label"
    return $nodes[0]
}
function Get-AutoField($Value,[string]$Name){
    $property=$Value.PSObject.Properties[$Name]
    if($null -eq $property){return $null}
    return $property.Value
}
function Copy-RunnerFixture($Value){return ($Value|ConvertTo-Json -Depth 12 -Compress|ConvertFrom-Json)}
function Reject-Runner([scriptblock]$Action,[string]$Reason,[string]$Expected){
    $rejected=$false
    try{$null=& $Action}catch{
        $rejected=$true
        if($Expected){Check-Runner ($_.Exception.Message.Contains($Expected)) "$Reason rejection reason"}
    }
    Check-Runner $rejected $Reason
}

$single=Read-RunnerAst 'run-r1c4b-input-isolation.ps1'
$gates=Read-RunnerAst 'run-r1c4b-input-isolation-gates.ps1'
$contracts=One-RunnerNode $single {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$contracts'} 'single contract expression'
Check-Runner (@($contracts.Right.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -cne 'Get-AutoField'},$true)).Count -eq 0) 'contract expression calls only the pure field seam'
$contractExpression=[scriptblock]::Create($contracts.Right.Extent.Text)
$Operation='BottomResize'
$result=[pscustomobject]@{ForegroundContract='verified_global_foreground_v2';HandoffContract='winevent_end_barrier_v1';DiagnosticContract='separated_authority_v1';AuthorityContract='product_gesture_v1';IsolationContract='post_end_shield_v1';Operation=$Operation}
$validContract=Copy-RunnerFixture $result
Check-Runner (& $contractExpression) 'single exact contracts and operation'
foreach($field in @('ForegroundContract','HandoffContract','DiagnosticContract','AuthorityContract','IsolationContract','Operation')){
    $result=Copy-RunnerFixture $validContract;$result.$field='wrong'
    Check-Runner (-not (& $contractExpression)) "single rejects mismatched $field"
}
$result=Copy-RunnerFixture $validContract;$result.Operation='Move'
Check-Runner (-not (& $contractExpression)) 'a valid Move cannot be relabeled as requested BottomResize'
$result=Copy-RunnerFixture $validContract;$result.PSObject.Properties.Remove('Operation')
Check-Runner (-not (& $contractExpression)) 'missing actual operation is not accepted'

function Test-CleanBeforeInput($Ast){
    $invocations=@($Ast.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.InvocationOperator -eq [Management.Automation.Language.TokenKind]::Ampersand -and $n.CommandElements[0].Extent.Text -ceq '$exe'},$true))
    $guards=@($Ast.FindAll({param($n) $n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text -ceq '$statusRows.Count'},$true))
    if($invocations.Count -ne 1 -or $guards.Count -ne 1){return $false}
    $throws=@($guards[0].Clauses[0].Item2.FindAll({param($n) $n -is [Management.Automation.Language.ThrowStatementAst]},$true))
    return $throws.Count -eq 1 -and $guards[0].Extent.EndOffset -lt $invocations[0].Extent.StartOffset
}
Check-Runner (Test-CleanBeforeInput $single) 'dirty-checkpoint throw precedes the only native invocation'
$dirtyGuard=One-RunnerNode $single {param($n) $n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text -ceq '$statusRows.Count'} 'single dirty guard'
$tokens=$null;$errors=$null
$removedGuard=[Management.Automation.Language.Parser]::ParseInput($single.Extent.Text.Replace($dirtyGuard.Extent.Text,''),[ref]$tokens,[ref]$errors)
Check-Runner (-not (Test-CleanBeforeInput $removedGuard)) 'AST audit rejects a removed clean-before-input guard'
$lateGuard=[Management.Automation.Language.Parser]::ParseInput(($single.Extent.Text.Replace($dirtyGuard.Extent.Text,'')+"`n"+$dirtyGuard.Extent.Text),[ref]$tokens,[ref]$errors)
Check-Runner (-not (Test-CleanBeforeInput $lateGuard)) 'AST audit rejects a clean guard moved after input'
$status=One-RunnerNode $single {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$statusRows'} 'single before status'
$statusError=One-RunnerNode $single {param($n) $n -is [Management.Automation.Language.ThrowStatementAst] -and $n.Extent.Text.Contains('Working tree status unavailable')} 'single status error'
Check-Runner ($status.Extent.EndOffset -lt $statusError.Extent.StartOffset -and $statusError.Extent.EndOffset -lt $dirtyGuard.Extent.StartOffset) 'status query failure is checked before the clean guard'
$afterStatusError=One-RunnerNode $single {param($n) $n -is [Management.Automation.Language.ThrowStatementAst] -and $n.Extent.Text.Contains('After working tree status unavailable')} 'single after-status error'
Check-Runner ($afterStatusError.Extent.StartOffset -gt $dirtyGuard.Extent.EndOffset) 'post-run status errors remain fail closed'
$metadata=One-RunnerNode $single {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$metadata'} 'single metadata'
foreach($field in @('ExecutedHEAD','AfterHEAD','WorktreeDirty','AfterWorktreeDirty','BinarySHA256','AfterBinarySHA256','LogSHA256','RunId','EvidencePath','ImplementationUnchanged')){
    Check-Runner ($metadata.Right.Extent.Text -match ('\b'+$field+'=')) "single preserves $field"
}
foreach($ast in @($single,$gates)){
    $directory=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$root'},$true))
    Check-Runner ($directory.Count -eq 1 -and $directory[0].Right.Extent.Text.Contains("'uat/r1c4b-input-isolation'")) 'runner has a separate fixed Fix D evidence directory'
}

# Import only these three actual helper definitions. The aggregate program,
# Save-IsolationInventory (a writer), CLI loop, and validator are not imported.
foreach($name in @('Assert-IsolationGate','Read-IsolationGateMetadata','Get-IsolationQuantile')){
    $definition=One-RunnerNode $gates {param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name} "pure/stubbed helper $name"
    $allowed=@('Assert-IsolationGate','Get-Content','ConvertFrom-Json','Get-FileHash','Test-InputIsolationOwnedEvidence','Join-Path','Add-Member','ForEach-Object')
    Check-Runner (@($definition.Body.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -cnotin $allowed},$true)).Count -eq 0) "helper $name calls only pure or stubbed seams"
    . ([scriptblock]::Create($definition.Extent.Text))
}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Join-Path $repo 'uat/r1c4b-input-isolation'
$fixtureBinaryHash='A'*64;$fixtureLogHash='B'*64;$fixtureHead='1'*40
$fixtureMetadata=$null;$fixtureVerdict=$null;$fixtureRows=@();$fixtureMetadataPath=''
# These script-local commands cannot access disk, a process, Git, or real logs.
function Get-Content([string]$LiteralPath,[string]$Encoding){
    if($LiteralPath -ceq $script:fixtureMetadataPath){return ($script:fixtureMetadata|ConvertTo-Json -Depth 12 -Compress)}
    if($LiteralPath -ceq $script:fixtureMetadata.EvidencePath){foreach($row in $script:fixtureRows){$row|ConvertTo-Json -Depth 6 -Compress};return}
    throw 'fixture attempted an unregistered read'
}
function Get-FileHash([string]$LiteralPath,[string]$Algorithm){
    if($Algorithm -cne 'SHA256'){throw 'fixture hash algorithm changed'}
    if($LiteralPath.EndsWith('panebind-magnet-takeover-probe.exe',[StringComparison]::Ordinal)){return [pscustomobject]@{Hash=$script:fixtureBinaryHash}}
    if($LiteralPath -ceq $script:fixtureMetadata.EvidencePath){return [pscustomobject]@{Hash=$script:fixtureLogHash}}
    throw 'fixture attempted an unregistered hash'
}
function Test-InputIsolationOwnedEvidence([string]$Path){
    if($Path -cne $script:fixtureMetadata.EvidencePath){throw 'fixture validator path mismatch'}
    return $script:fixtureVerdict
}
$metricInitialization=One-RunnerNode $gates {param($n) $n -is [Management.Automation.Language.ForEachStatementAst] -and $n.Variable.Extent.Text -ceq '$metricName'} 'seven metric initialization'
function Reset-RunnerFixture {
    $script:state=[ordered]@{ExecutedHEAD=$script:fixtureHead;TimingMilliseconds=@{};TimingStatistics=@{}}
    . ([scriptblock]::Create($script:metricInitialization.Extent.Text))
    $script:nonces=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $id='a'*32
    $script:fixtureMetadataPath=Join-Path $script:root ("20000101T000000000Z-Debug-BottomResize-$id.metadata.json")
    $log=Join-Path $script:root ("20000101T000000000Z-Debug-BottomResize-$id.jsonl")
    $script:fixtureMetadata=[pscustomobject]@{Schema='r1c4b-input-isolation-run/v1';Configuration='Debug';Operation='BottomResize';ExecutedHEAD=$script:fixtureHead;AfterHEAD=$script:fixtureHead;WorktreeDirty=$false;AfterWorktreeDirty=$false;ImplementationUnchanged=$true;BinarySHA256=$script:fixtureBinaryHash;AfterBinarySHA256=$script:fixtureBinaryHash;EvidencePath=$log;RunId=$id;LogSHA256=$script:fixtureLogHash;ProbeExitCode=0;CurrentContractsVerified=$true}
    $script:fixtureVerdict=[pscustomobject]@{Result='PASS';Operation='BottomResize';ProductHandoffAuthority='PASS';TestInputIsolation='PASS';RunNonce='synthetic-generation';TimingTickSamples=[pscustomobject]@{}}
    $script:fixtureRows=@([pscustomobject]@{type='startup';qpc=10;qpc_frequency=10000000;run_nonce='synthetic-generation'},[pscustomobject]@{type='shutdown';qpc=20})
}
Reset-RunnerFixture
$accepted=Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize
Check-Runner ($accepted.ValidatedStartQpc -eq 10 -and $accepted.ValidatedEndQpc -eq 20 -and $accepted.ValidatedQpcFrequency -eq 10000000) 'actual metadata helper binds validated clock values (stub fixture only)'
foreach($field in @('ExecutedHEAD','AfterHEAD','BinarySHA256','AfterBinarySHA256','LogSHA256')){
    Reset-RunnerFixture;$fixtureMetadata.$field='mismatch'
    Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize} "reject $field" ''
}
foreach($field in @('WorktreeDirty','AfterWorktreeDirty')){
    Reset-RunnerFixture;$fixtureMetadata.$field=$true
    Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize} "reject $field" 'Implementation identity changed or dirty'
}
foreach($field in @('ImplementationUnchanged','CurrentContractsVerified')){
    Reset-RunnerFixture;$fixtureMetadata.$field=$false
    Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize} "reject false $field" ''
}
foreach($field in @('Schema','Configuration','Operation')){
    Reset-RunnerFixture;$fixtureMetadata.$field='wrong'
    Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize} "reject metadata $field" 'Wrong evidence mode/configuration/operation'
}
Reset-RunnerFixture;$fixtureMetadataPath=Join-Path $repo ('outside-'+('a'*32)+'.metadata.json')
Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize} 'reject outside metadata path' 'Metadata must be local Fix D evidence'
Reset-RunnerFixture;$fixtureMetadata.EvidencePath=Join-Path $repo ('outside-'+('a'*32)+'.jsonl')
Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize} 'reject outside raw log path' 'Log must remain local owned evidence'
Reset-RunnerFixture;$fixtureMetadata.RunId='A'*32
Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize} 'reject invalid RunId format' 'Run ID must bind log and metadata'
Reset-RunnerFixture;$fixtureMetadata.EvidencePath=Join-Path $root ('wrong-'+('b'*32)+'.jsonl')
Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize} 'reject raw-log RunId mismatch' 'Run ID must bind log and metadata'
Reset-RunnerFixture;$fixtureMetadataPath=Join-Path $root ('wrong-'+('b'*32)+'.metadata.json')
Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize} 'reject metadata-file RunId mismatch' 'Run ID must bind log and metadata'
Reset-RunnerFixture;$fixtureMetadata.ProbeExitCode=2
Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize} 'reject failed probe' 'Not an independently complete separated-authority PASS'
foreach($field in @('Result','Operation','ProductHandoffAuthority','TestInputIsolation')){
    Reset-RunnerFixture;$fixtureVerdict.$field='wrong'
    Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize} "reject independent $field" 'Not an independently complete separated-authority PASS'
}
Reset-RunnerFixture;$fixtureRows[0].qpc_frequency=0
Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize} 'reject invalid QPC frequency' 'Missing or reused test generation'
Reset-RunnerFixture;$fixtureRows[0].run_nonce='stale-generation'
Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize} 'reject mismatched generation' 'Missing or reused test generation'
Reset-RunnerFixture;$null=Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize
Reject-Runner {Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize} 'reject reused generation' 'Missing or reused test generation'

# Actual helper consumes PSCustomObject properties: array, scalar, empty, null.
Reset-RunnerFixture
$fixtureVerdict.TimingTickSamples=[pscustomobject]@{NativeExitToWinEventEnd=@([long]10000,[long]20000);HandoffWriteDuration=[long]30000;RawReceiptToNativeWrite=@();RawReceiptToOwnerQuantum=$null}
$null=Read-IsolationGateMetadata $fixtureMetadataPath Debug BottomResize
Check-Runner ($state.TimingMilliseconds.Count -eq 7) 'all seven metrics remain present'
Check-Runner (($state.TimingMilliseconds.NativeExitToWinEventEnd -join ',') -ceq '1,2') 'array ticks become milliseconds using validated frequency'
Check-Runner ($state.TimingMilliseconds.HandoffWriteDuration.Count -eq 1 -and $state.TimingMilliseconds.HandoffWriteDuration[0] -eq 3) 'scalar tick becomes one millisecond sample'
Check-Runner ($state.TimingMilliseconds.RawReceiptToNativeWrite.Count -eq 0 -and $state.TimingMilliseconds.RawReceiptToOwnerQuantum.Count -eq 0) 'empty and null values do not become fake zero samples'
Check-Runner ((Get-IsolationQuantile @(1.0,2.0) 0.50) -eq 1.5 -and (Get-IsolationQuantile @(1.0,2.0) 0.95) -eq 1.95) 'actual linear interpolation p50/p95'
Check-Runner ((Get-IsolationQuantile @(3.0) 0.50) -eq 3.0 -and (Get-IsolationQuantile @(3.0) 0.95) -eq 3.0) 'one sample is not dropped or interpolated from zero'
foreach($name in @('WinEventEndToIsolationReady','IsolationReadyToHandoffWrite','WinEventEndToHandoffWrite','RawReceiptToOwnerQuantum','RawReceiptToNativeWrite')){
    $sorted=@($state.TimingMilliseconds[$name]|Sort-Object)
    Check-Runner ($sorted.Count -eq 0 -and $null -eq (Get-IsolationQuantile $sorted 0.50) -and $null -eq (Get-IsolationQuantile $sorted 0.95)) "empty $name has null quantiles"
}
Check-Runner ((Get-IsolationQuantile @(-1.0,1.0,4.0) 0.50) -eq 1.0) 'signed cross-stream latency is not clamped into a false ordering claim'
$statistics=One-RunnerNode $gates {param($n) $n -is [Management.Automation.Language.ForEachStatementAst] -and $n.Condition.Extent.Text -ceq '$state.TimingMilliseconds.Keys'} 'actual pure timing statistics loop'
Check-Runner (@($statistics.Body.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -cnotin @('Sort-Object','Get-IsolationQuantile')},$true)).Count -eq 0) 'statistics loop cannot invoke inventory writers or a CLI'
$state.TimingMilliseconds.NativeExitToWinEventEnd=@(4.0,1.0,3.0,2.0)
. ([scriptblock]::Create($statistics.Extent.Text))
Check-Runner ($state.TimingStatistics.Count -eq 7 -and $state.TimingStatistics.NativeExitToWinEventEnd.Count -eq 4 -and $state.TimingStatistics.NativeExitToWinEventEnd.P50Ms -eq 2.5 -and [Math]::Abs($state.TimingStatistics.NativeExitToWinEventEnd.P95Ms-3.85) -lt 1e-12) 'actual statistics loop sorts unsorted samples before linear p50/p95'
Check-Runner ($state.TimingStatistics.RawReceiptToNativeWrite.Count -eq 0 -and $null -eq $state.TimingStatistics.RawReceiptToNativeWrite.P50Ms -and $null -eq $state.TimingStatistics.RawReceiptToNativeWrite.P95Ms -and $state.TimingStatistics.RawReceiptToNativeWrite.NoSLA) 'actual empty metric inventory uses null and no SLA'

$initial=One-RunnerNode $gates {param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -ceq 'Assert-IsolationGate' -and $n.Extent.Text.Contains('Initial complete Resize must precede')} 'initial ordering predicate'
$orderExpression=[scriptblock]::Create($initial.CommandElements[1].Extent.Text)
$resize=[pscustomobject]@{EvidencePath='fixture-resize';ValidatedQpcFrequency=10000000;ValidatedEndQpc=20}
$move=[pscustomobject]@{EvidencePath='fixture-move';ValidatedQpcFrequency=10000000;ValidatedStartQpc=30}
Check-Runner (& $orderExpression) 'initial complete Resize precedes distinct Move'
foreach($fault in @('same-log','frequency','overlap','equal-time')){
    $resize=[pscustomobject]@{EvidencePath='fixture-resize';ValidatedQpcFrequency=10000000;ValidatedEndQpc=20}
    $move=[pscustomobject]@{EvidencePath='fixture-move';ValidatedQpcFrequency=10000000;ValidatedStartQpc=30}
    switch($fault){'same-log'{$move.EvidencePath=$resize.EvidencePath};'frequency'{$move.ValidatedQpcFrequency=1};'overlap'{$move.ValidatedStartQpc=19};'equal-time'{$move.ValidatedStartQpc=20}}
    Check-Runner (-not (& $orderExpression)) "initial ordering rejects $fault"
}
$stageLoop=One-RunnerNode $gates {param($n) $n -is [Management.Automation.Language.ForEachStatementAst] -and $n.Variable.Extent.Text -ceq '$stage'} 'bounded stage loop'
Check-Runner (@($stageLoop.Condition.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst]},$true)).Count -eq 0) 'stage initializer is pure literal data'
$stages=@(& ([scriptblock]::Create($stageLoop.Condition.Extent.Text)))
Check-Runner (($stages|ForEach-Object {"$($_.Name):$($_.Configuration):$($_.Count)"}) -join ',' -ceq 'DebugSmoke:Debug:5,ReleaseSmoke:Release:5,DebugFormal:Debug:20,ReleaseFormal:Release:20') 'both 5-pair smoke stages precede both 20-pair formal stages'
$operationLoop=One-RunnerNode $stageLoop {param($n) $n -is [Management.Automation.Language.ForEachStatementAst] -and $n.Variable.Extent.Text -ceq '$operation'} 'pair operations'
Check-Runner ((@(& ([scriptblock]::Create($operationLoop.Condition.Extent.Text))) -join ',') -ceq 'Move,BottomResize') 'each complete pair has both operations in explicit order'
$pairLoop=One-RunnerNode $stageLoop {param($n) $n -is [Management.Automation.Language.ForStatementAst]} 'bounded pair loop'
Check-Runner ($pairLoop.Condition.Extent.Text -ceq '$pair -le $stage.Count' -and ($pairLoop.Iterator.Extent.Text -join '') -ceq '++$pair') 'pair repetitions are finite and advance exactly once'
$counter=One-RunnerNode $stageLoop {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$state[$stage.Name]'} 'complete-pair counter'
Check-Runner ($counter.Extent.StartOffset -gt $operationLoop.Extent.EndOffset -and $counter.Right.Extent.Text -ceq '$pair') 'stage count advances only after both operations complete'
$independent=One-RunnerNode $operationLoop {param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -ceq 'Read-IsolationGateMetadata'} 'independent pair verification'
Check-Runner ($independent.Extent.EndOffset -lt $counter.Extent.StartOffset) 'independent verification precedes pair counting'
$failure=One-RunnerNode $operationLoop {param($n) $n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text.Contains('$code -ne 0')} 'first failure branch'
Check-Runner (@($failure.FindAll({param($n) $n -is [Management.Automation.Language.ThrowStatementAst]},$true)).Count -eq 1 -and $failure.Extent.Text.Contains('$state.FirstFailure=') -and $failure.Extent.EndOffset -lt $independent.Extent.StartOffset) 'native/nonunique-metadata failure records first item and throws before verification/counting'
$failureExpression=[scriptblock]::Create($failure.Clauses[0].Item1.Extent.Text)
foreach($case in @(@{Code=0;Files=1;Fails=$false},@{Code=2;Files=1;Fails=$true},@{Code=0;Files=0;Fails=$true},@{Code=0;Files=2;Fails=$true},@{Code=2;Files=0;Fails=$true})){
    $code=$case.Code;$files=@(for($i=0;$i -lt $case.Files;++$i){[pscustomobject]@{fixture=$i}})
    Check-Runner ((& $failureExpression) -eq $case.Fails) "actual failure predicate exit=$code metadata=$($files.Count)"
}
Check-Runner (@($operationLoop.Body.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -ceq 'powershell.exe'},$true)).Count -eq 1) 'aggregate has exactly one child-runner call per operation (not executed)'
Check-Runner (@($operationLoop.Body.FindAll({param($n) $n -is [Management.Automation.Language.ForStatementAst] -or $n -is [Management.Automation.Language.ForEachStatementAst] -or $n -is [Management.Automation.Language.WhileStatementAst] -or $n -is [Management.Automation.Language.DoWhileStatementAst]},$true)).Count -eq 0) 'there is no per-operation retry loop'
$topTry=One-RunnerNode $gates {param($n) $n -is [Management.Automation.Language.TryStatementAst]} 'aggregate terminal catch'
Check-Runner ($topTry.CatchClauses.Count -eq 1 -and $topTry.CatchClauses[0].Body.Extent.Text.Contains("`$state.Result='STOPPED'") -and $topTry.CatchClauses[0].Body.Extent.Text.Contains('$state.FirstFailure=$state.Runs[-1]')) 'independent verification failure is STOPPED and names its last attempted run'
Check-Runner ($initial.Extent.EndOffset -lt $stageLoop.Extent.StartOffset) 'initial two-PASS ordering gate runs before repetitions'
$inventory=One-RunnerNode $gates {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$state.InitialEvidence'} 'initial evidence inventory'
Check-Runner ($inventory.Extent.StartOffset -gt $initial.Extent.EndOffset -and $inventory.Extent.EndOffset -lt $stageLoop.Extent.StartOffset -and $inventory.Right.Extent.Text.Contains('MetadataSHA256=') -and $inventory.Right.Extent.Text.Contains('LogSHA256=')) 'verified initial metadata/log identity is inventoried before the stage loop'
Write-Host "input-isolation-runner synthetic_only=true checks=$checks PASS; no child CLI/GUI/input/Git; metadata validator is stubbed"
