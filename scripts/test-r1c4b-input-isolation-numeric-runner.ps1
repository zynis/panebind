Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# Numeric-only synthetic tests. Extract actual helpers, never a runner's
# top-level program. All metadata, hashes, validator output and log rows are
# in-memory stubs; no original evidence, child CLI, Git, GUI or input is used.
$checks=0
function Check-NumericRunner([bool]$Value,[string]$Reason){
    if(-not $Value){throw "numeric runner fixture: $Reason"}
    $script:checks++
}
function One-NumericNode($Ast,[scriptblock]$Predicate,[string]$Label){
    $nodes=@($Ast.FindAll($Predicate,$true))
    Check-NumericRunner ($nodes.Count -eq 1) "unique $Label"
    return $nodes[0]
}
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'run-r1c4b-input-isolation-gates.ps1'),[ref]$tokens,[ref]$errors)
Check-NumericRunner (@($errors).Count -eq 0) 'aggregate parses'
foreach($name in @('Assert-IsolationGate','Read-IsolationGateMetadata','Get-IsolationQuantile')){
    $node=One-NumericNode $ast {param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name} $name
    $allowed=@('Assert-IsolationGate','Get-Content','ConvertFrom-Json','Get-FileHash','Test-InputIsolationOwnedEvidence','Join-Path','Add-Member','ForEach-Object')
    Check-NumericRunner (@($node.Body.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -cnotin $allowed},$true)).Count -eq 0) "$name uses only pure/stubbed seams"
    Check-NumericRunner ($node.Extent.Text -notmatch '\[Math\]::(?:Max|Min)\(') "$name has no overload-sensitive Max/Min"
    $narrow=@($node.Body.FindAll({param($n) $n -is [Management.Automation.Language.ConvertExpressionAst] -and $n.Type.TypeName.FullName -in @('int','Int32','System.Int32')},$true))
    if($name -ceq 'Get-IsolationQuantile'){
        Check-NumericRunner ($narrow.Count -eq 2 -and @($narrow|Where-Object {$_.Child.Extent.Text -notmatch '^\[Math\]::(?:Floor|Ceiling)\(\$index\)$'}).Count -eq 0) 'Int32 conversions are only bounded array indices'
    }else{Check-NumericRunner ($narrow.Count -eq 0) "$name never narrows QPC/ticks to Int32"}
    . ([scriptblock]::Create($node.Extent.Text))
}
$initialization=One-NumericNode $ast {param($n) $n -is [Management.Automation.Language.ForEachStatementAst] -and $n.Variable.Extent.Text -ceq '$metricName'} 'actual metric initialization'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Join-Path $repo 'uat/r1c4b-input-isolation'
$id='a'*32
$metadataPath=Join-Path $root ("synthetic-Debug-BottomResize-$id.metadata.json")
$logPath=Join-Path $root ("synthetic-Debug-BottomResize-$id.jsonl")
$binaryHash='A'*64;$logHash='B'*64;$head='1'*40
$fixtureMetadata=$null;$fixtureVerdict=$null;$fixtureRows=@()
# Load the serialization module before installing command-name stubs: its
# first-use auto-import must not replace Get-Content/Get-FileHash afterwards.
$null=[pscustomobject]@{synthetic_only=$true}|ConvertTo-Json -Compress
function Get-Content([string]$LiteralPath,[string]$Encoding){
    if($LiteralPath -ceq $script:metadataPath){return ($script:fixtureMetadata|ConvertTo-Json -Depth 12 -Compress)}
    if($LiteralPath -ceq $script:logPath){foreach($row in $script:fixtureRows){$row|ConvertTo-Json -Depth 8 -Compress};return}
    throw 'numeric fixture attempted an unregistered read'
}
function Get-FileHash([string]$LiteralPath,[string]$Algorithm){
    if($Algorithm -cne 'SHA256'){throw 'numeric fixture hash algorithm changed'}
    if($LiteralPath.EndsWith('panebind-magnet-takeover-probe.exe',[StringComparison]::Ordinal)){return [pscustomobject]@{Hash=$script:binaryHash}}
    if($LiteralPath -ceq $script:logPath){return [pscustomobject]@{Hash=$script:logHash}}
    throw 'numeric fixture attempted an unregistered hash'
}
function Test-InputIsolationOwnedEvidence([string]$Path){
    if($Path -cne $script:logPath){throw 'numeric fixture validator path mismatch'}
    return $script:fixtureVerdict
}
foreach($seam in @('Get-Content','Get-FileHash','Test-InputIsolationOwnedEvidence')){
    Check-NumericRunner ((Get-Command $seam).CommandType -eq 'Function') "$seam remains an in-memory function seam"
}
function Reset-NumericRunnerFixture([long]$Start,[long]$End){
    $script:state=[ordered]@{ExecutedHEAD=$script:head;TimingMilliseconds=@{};TimingStatistics=@{}}
    . ([scriptblock]::Create($script:initialization.Extent.Text))
    $script:nonces=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $script:fixtureMetadata=[pscustomobject]@{Schema='r1c4b-input-isolation-run/v1';Configuration='Debug';Operation='BottomResize';ExecutedHEAD=$script:head;AfterHEAD=$script:head;WorktreeDirty=$false;AfterWorktreeDirty=$false;ImplementationUnchanged=$true;BinarySHA256=$script:binaryHash;AfterBinarySHA256=$script:binaryHash;EvidencePath=$script:logPath;RunId=$script:id;LogSHA256=$script:logHash;ProbeExitCode=0;CurrentContractsVerified=$true}
    $script:fixtureVerdict=[pscustomobject]@{Result='PASS';Operation='BottomResize';ProductHandoffAuthority='PASS';TestInputIsolation='PASS';RunNonce=[long]197157086769502;TimingTickSamples=[pscustomobject]@{}}
    $script:fixtureRows=@([pscustomobject]@{type='startup';sequence=[long]1;qpc=$Start;qpc_frequency=[long]10000000;run_nonce=[long]197157086769502},[pscustomobject]@{type='shutdown';sequence=[long]2;qpc=$End})
}
foreach($start in @([long]2147483647,[long]2147483648,[long]3000000000,[long]900000000000,[long]1134816073663,[long]1500000000000)){
    [long]$end=$start+[long]10000
    Reset-NumericRunnerFixture $start $end
    $m=Read-IsolationGateMetadata $metadataPath Debug BottomResize
    Check-NumericRunner ($m.ValidatedStartQpc -eq $start -and $m.ValidatedEndQpc -eq $end) "exact QPC boundary $start"
    Check-NumericRunner ([long]$m.ValidatedEndQpc-[long]$m.ValidatedStartQpc -eq [long]10000) "Int64 duration at $start"
    Check-NumericRunner ($m.ValidatedQpcFrequency -eq 10000000) "frequency survives at $start"
    if($start -gt [int]::MaxValue){Check-NumericRunner ($m.ValidatedStartQpc -is [long] -and $m.ValidatedEndQpc -is [long]) "large clock values are Int64 at $start"}
}
Reset-NumericRunnerFixture 1134816073663 1134816083663
$fixtureVerdict.TimingTickSamples=[pscustomobject]@{NativeExitToWinEventEnd=@([long]2147483647,[long]2147483648,[long]3000000000,[long]900000000000,[long]1134816073663,[long]1500000000000);HandoffWriteDuration=[long]1134816073663;RawReceiptToNativeWrite=@();RawReceiptToOwnerQuantum=$null}
Check-NumericRunner (@($fixtureVerdict.TimingTickSamples.NativeExitToWinEventEnd|Where-Object {$_ -isnot [long]}).Count -eq 0 -and $fixtureVerdict.TimingTickSamples.HandoffWriteDuration -is [long]) 'fixture tick arrays/scalar are genuinely Int64'
$null=Read-IsolationGateMetadata $metadataPath Debug BottomResize
Check-NumericRunner ($state.TimingMilliseconds.Count -eq 7) 'all seven metric arrays are present'
Check-NumericRunner ($state.TimingMilliseconds.NativeExitToWinEventEnd.Count -eq 6) 'all boundary tick samples are consumed'
foreach($index in 0..5){
    [decimal]$expected=[decimal]$fixtureVerdict.TimingTickSamples.NativeExitToWinEventEnd[$index]*[decimal]1000/[decimal]10000000
    Check-NumericRunner ([Math]::Abs([double]$state.TimingMilliseconds.NativeExitToWinEventEnd[$index]-[double]$expected) -lt 0.0000001) "no overflow converting tick sample $index to ms"
}
Check-NumericRunner ($state.TimingMilliseconds.HandoffWriteDuration.Count -eq 1 -and [Math]::Abs($state.TimingMilliseconds.HandoffWriteDuration[0]-113481607.3663) -lt 0.0000001) 'large scalar becomes one ms sample'
Check-NumericRunner ($state.TimingMilliseconds.RawReceiptToNativeWrite.Count -eq 0 -and $state.TimingMilliseconds.RawReceiptToOwnerQuantum.Count -eq 0) 'empty/null do not become zero samples'
$sorted=@($state.TimingMilliseconds.NativeExitToWinEventEnd|Sort-Object)
Check-NumericRunner ([Math]::Abs((Get-IsolationQuantile $sorted 0.50)-45150000.0) -lt 0.0000001) 'large pipeline p50 is exact within binary floating precision'
Check-NumericRunner ([Math]::Abs((Get-IsolationQuantile $sorted 0.95)-140870401.841575) -lt 0.0000001) 'large pipeline p95 linear interpolation has no overflow'
foreach($fraction in @(0.50,0.95)){
    Check-NumericRunner ($null -eq (Get-IsolationQuantile @() $fraction)) "empty p$fraction is null"
    Check-NumericRunner ((Get-IsolationQuantile @(113481607.3663) $fraction) -eq 113481607.3663) "scalar p$fraction is preserved"
}
Check-NumericRunner ((Get-IsolationQuantile @(-1.0,1.0,4.0) 0.50) -eq 1.0) 'signed latency samples are not silently clamped'
Write-Host "input-isolation numeric runner synthetic_only=true checks=$checks PASS; actual helper extraction; no child CLI/Git/GUI/input/original evidence"
