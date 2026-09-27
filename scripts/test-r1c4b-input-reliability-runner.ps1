Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# Synthetic AST/in-memory runner regression only. Never evaluate runner
# programs, invoke child/native/environment CLI, Git, GUI or original UAT.
$checks=0
function Check-Reliability([bool]$Value,[string]$Reason){if(-not $Value){throw "reliability runner fixture: $Reason"};$script:checks++}
function Reject-Reliability([scriptblock]$Action,[string]$Reason){
    $rejected=$false;try{$null=& $Action}catch{$rejected=$true}
    Check-Reliability $rejected $Reason
}
function Reject-ReliabilityEnvironment($Value,[string]$Reason){
    $accepted=$false;try{$accepted=Test-ReliabilityEnvironment $Value}catch{$accepted=$false}
    Check-Reliability (-not $accepted) $Reason
}
function Read-ReliabilityAst([string]$Name){
    $t=$null;$e=$null;$a=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $Name),[ref]$t,[ref]$e)
    Check-Reliability (@($e).Count -eq 0) "parse $Name";return $a
}
function One-ReliabilityAst($Ast,[scriptblock]$Predicate,[string]$Reason){
    $n=@($Ast.FindAll($Predicate,$true));Check-Reliability ($n.Count -eq 1) "unique $Reason";return $n[0]
}
function Copy-ReliabilityFixture($Value){return $Value|ConvertTo-Json -Depth 32 -Compress|ConvertFrom-Json}
$single=Read-ReliabilityAst 'run-r1c4b-input-reliability.ps1'
$gates=Read-ReliabilityAst 'run-r1c4b-input-reliability-gates.ps1'
$numeric=Read-ReliabilityAst 'r1c4b-input-isolation-validation.ps1'
$singleHelpers=@('Get-ReliabilityField','Test-ReliabilityTrue','Test-ReliabilityFalse','Get-ReliabilitySnapshot','Test-ReliabilitySameSnapshot','Test-ReliabilityEnvironmentContext','Get-ReliabilityButtonState','Test-ReliabilityEnvironment','Get-ReliabilityExitCode')
$gateHelpers=@('Get-ReliabilityPlan','Assert-ReliabilityGate','Get-ReliabilityAttemptButtonState','Read-ReliabilityRun')
$allowed=@($singleHelpers)+@($gateHelpers)+@('Assert-IsolationInt64','Get-FileHash','Get-Content','ConvertFrom-Json','Join-Path','Split-Path','ForEach-Object','Add-Member','Test-FixFInputReliabilityEvidence')
foreach($pair in @(@($single,$singleHelpers),@($gates,$gateHelpers),@($numeric,@('Assert-IsolationInt64')))){
    foreach($name in $pair[1]){
        $node=One-ReliabilityAst $pair[0] {param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name} "helper $name"
        Check-Reliability (@($node.Body.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -cnotin $allowed},$true)).Count -eq 0) "$name uses only pure/memory seams"
        . ([scriptblock]::Create($node.Extent.Text))
    }
}
$native=One-ReliabilityAst $single {param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.CommandElements[0].Extent.Text -ceq '$probe'} 'single native invocation'
$envCalls=@($single.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.CommandElements[0].Extent.Text -ceq '$environment'},$true))
Check-Reliability ($envCalls.Count -eq 2 -and $envCalls[0].Extent.EndOffset -lt $native.Extent.StartOffset -and $envCalls[1].Extent.StartOffset -gt $native.Extent.EndOffset) 'exact pre/native/post order; no retry'
foreach($needle in @('Require a clean checkpoint before environment/probe execution','Expected aggregate checkpoint changed before environment/probe execution','Current input state is not reliably READY','Implementation changed before native input','Checkpoint changed before native input')){
    $guard=One-ReliabilityAst $single {param($n) $n -is [Management.Automation.Language.ThrowStatementAst] -and $n.Extent.Text.Contains($needle)} "guard $needle"
    Check-Reliability ($guard.Extent.EndOffset -lt $native.Extent.StartOffset) "guard before native: $needle"
}
$clean=One-ReliabilityAst $single {param($n) $n -is [Management.Automation.Language.ThrowStatementAst] -and $n.Extent.Text.Contains('Require a clean checkpoint before')} 'clean before environment'
Check-Reliability ($clean.Extent.EndOffset -lt $envCalls[0].Extent.StartOffset) 'dirty checkpoint executes no readonly/native CLI'
$expectedGuard=One-ReliabilityAst $single {param($n) $n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text -ceq '$ExpectedHEAD -and $head -cne $ExpectedHEAD'} 'single expected HEAD guard'
Check-Reliability ($expectedGuard.Extent.EndOffset -lt $envCalls[0].Extent.StartOffset) 'aggregate expected HEAD verified before first readonly CLI'
Check-Reliability ($native.Parent.Parent -is [Management.Automation.Language.TryStatementAst] -or $single.Extent.Text.Contains('}catch{$metadata.NativeError=$_.Exception.Message}')) 'native invocation error retained before mandatory post'
foreach($ast in @($single,$gates)){
    Check-Reliability (@($ast.FindAll({param($n) $n -is [Management.Automation.Language.WhileStatementAst] -or $n -is [Management.Automation.Language.DoWhileStatementAst] -or $n -is [Management.Automation.Language.DoUntilStatementAst]},$true)).Count -eq 0) 'no retry/poll loop'
    Check-Reliability ($ast.Extent.Text -notmatch '\b(Start-Process|SendInput|SetCursorPos|SetForegroundWindow|AttachThreadInput)\b') 'no direct input/window mutation'
}
Check-Reliability ($gates.ParamBlock.Parameters.Count -eq 0) 'aggregate cannot choose operation/count/repeat'
$metadata=One-ReliabilityAst $single {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$metadata'} 'single metadata'
foreach($name in @('ProbeExitCode','NativeError','EnvironmentPreData','EnvironmentPostData','EnvironmentPreSHA256','EnvironmentPostSHA256','FinalButtonState','FinalButtonObservation','BeforeIdentity','AfterIdentity','RunnerExitCode','LogSHA256','Reason')){
    Check-Reliability ($metadata.Right.Extent.Text.Contains($name+'=')) "metadata preserves $name"
}
$writer=One-ReliabilityAst $single {param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq 'Write-ReliabilityNewJson'} 'exclusive writer'
Check-Reliability ($writer.Extent.Text.Contains('[IO.FileMode]::CreateNew')) 'metadata uses CREATE_NEW'
Check-Reliability ($single.Extent.Text.Contains('if($createdDirectory)')) 'collision cannot write another run metadata'
$child=One-ReliabilityAst $gates {param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -ceq 'powershell.exe'} 'one aggregate child site'
$attemptAppend=One-ReliabilityAst $gates {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$state.Runs'} 'attempt before child'
Check-Reliability ($attemptAppend.Extent.EndOffset -lt $child.Extent.StartOffset) 'attempt recorded even on child failure'
Check-Reliability ($child.Extent.Text.Contains('-ExpectedHEAD $state.ExecutedHEAD')) 'aggregate child pins fixed initial HEAD'
foreach($needle in @('Checkpoint changed before next child','Implementation changed before next child')){
    $guard=One-ReliabilityAst $gates {param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -ceq 'Assert-ReliabilityGate' -and $n.Extent.Text.Contains($needle)} "aggregate guard $needle"
    Check-Reliability ($guard.Extent.EndOffset -lt $child.Extent.StartOffset -and $guard.Extent.StartOffset -gt $attemptAppend.Extent.EndOffset) 'fresh guard rejects before child and preserves attempt'
}
$failure=One-ReliabilityAst $gates {param($n) $n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text -ceq '$item.ChildExitCode -ne 0 -or $paths.Count -ne 1'} 'first failure branch'
Check-Reliability (@($failure.FindAll({param($n) $n -is [Management.Automation.Language.ThrowStatementAst]},$true)).Count -eq 1) 'first child failure throws STOP'
foreach($name in @('MetadataSHA256','LogSHA256','FixtureResult','GestureResult','CleanupResult','TakeoverAcceptance','FinalButtonState')){
    Check-Reliability ($gates.Extent.Text.Substring($child.Extent.EndOffset,$failure.Extent.StartOffset-$child.Extent.EndOffset).Contains($name)) "failure item captures $name before STOP"
}
$plan=@(Get-ReliabilityPlan)
$expected=@('Debug/Move/controlled-abort','Debug/BottomResize/controlled-abort','Debug/Move/normal','Debug/BottomResize/normal','Release/Move/controlled-abort','Release/BottomResize/controlled-abort','Release/Move/normal','Release/BottomResize/normal')
Check-Reliability ($plan.Count -eq 8) 'exact eight fixtures'
for($i=0;$i -lt $expected.Count;++$i){Check-Reliability ($plan[$i].Number -eq $i+1 -and ($plan[$i].Configuration+'/'+$plan[$i].Operation+'/'+$plan[$i].Mode) -ceq $expected[$i]) "immutable ordered fixture $($i+1)"}
function New-ReliabilityContext([long]$Start){
    return [pscustomobject]@{start_qpc=$Start;finish_qpc=($Start+10);input_name='Default';thread_name='Default';station_name='WinSta0';input_query_succeeded=$true;thread_query_succeeded=$true;station_query_succeeded=$true;session_query_succeeded=$true;session_level=1;session_id=1;session_state=0;session_flags=1;foreground_hwnd=123456;foreground_pid=2222;foreground_tid=3333;caller_integrity=8192;foreground_integrity=8192;caller_integrity_known=$true;foreground_integrity_known=$true;desktop_hook_access_verified=$true;valid_context=$true}
}
function New-ReliabilityEnvironment{
    [long]$base=1134816073663;$keys=[ordered]@{};$offset=11
    foreach($name in @('left','right','middle','x1','x2','ctrl','shift','alt','lwin','rwin','escape')){
        $keys[$name]=[pscustomobject]@{start_qpc=($base+$offset);finish_qpc=($base+$offset+1);query_attempted=$true;state='UP';high_bit_down=$false};$offset+=2
    }
    return [pscustomobject]@{schema='r1c4b-readonly-input-environment/v1';result='READY';read_only=$true;atomic_snapshot=$false;context_reliable=$true;all_required_inputs_up=$true;mouse_buttons_swapped=$false;qpc_frequency=10000000;before=(New-ReliabilityContext $base);after=(New-ReliabilityContext ($base+200));keys=[pscustomobject]$keys}
}
$goodEnv=New-ReliabilityEnvironment
Check-Reliability (Test-ReliabilityEnvironment $goodEnv) 'reliable eleven UP with real-sized QPC'
Check-Reliability ((Get-ReliabilityButtonState $goodEnv) -ceq 'OBSERVED_UP') 'reliable current LEFT UP observation'
foreach($name in @('read_only','context_reliable','all_required_inputs_up')){
    $v=Copy-ReliabilityFixture $goodEnv;$v.$name='true'
    Check-Reliability (-not (Test-ReliabilityEnvironment $v)) "string true rejects $name"
}
foreach($name in @('atomic_snapshot','mouse_buttons_swapped')){
    $v=Copy-ReliabilityFixture $goodEnv;$v.$name='false'
    Check-Reliability (-not (Test-ReliabilityEnvironment $v)) "string false rejects $name"
}
foreach($name in @('foreground_hwnd','foreground_pid','foreground_tid','caller_integrity','foreground_integrity','session_level','session_id','session_state','session_flags','start_qpc','finish_qpc')){
    foreach($bad in @('8192',8192.0,$true,$null)){
        $v=Copy-ReliabilityFixture $goodEnv;$v.before.$name=$bad;$v.after.$name=$bad
        Reject-ReliabilityEnvironment $v "typed context rejects $name/$($null -eq $bad)"
    }
}
foreach($context in @('before','after')){
    foreach($name in @('valid_context','caller_integrity_known','foreground_integrity_known','desktop_hook_access_verified','input_query_succeeded','thread_query_succeeded','station_query_succeeded','session_query_succeeded')){
        $v=Copy-ReliabilityFixture $goodEnv;$v.$context.$name=$false
        Check-Reliability (-not (Test-ReliabilityEnvironment $v)) "missing authority $context/$name"
        Check-Reliability ((Get-ReliabilityButtonState $v) -ceq 'UNKNOWN') "invalid context never reports UP $context/$name"
    }
}
foreach($name in @('foreground_hwnd','foreground_pid','foreground_tid','caller_integrity','foreground_integrity','session_id','input_name','thread_name','station_name')){
    $v=Copy-ReliabilityFixture $goodEnv
    if($v.after.$name -is [string]){$v.after.$name+='changed'}else{$v.after.$name++}
    Check-Reliability (-not (Test-ReliabilityEnvironment $v)) "before/after identity instability $name"
}
foreach($name in @('left','right','middle','x1','x2','ctrl','shift','alt','lwin','rwin','escape')){
    $v=Copy-ReliabilityFixture $goodEnv;$v.keys.$name.state='DOWN';$v.keys.$name.high_bit_down=$true
    Check-Reliability (-not (Test-ReliabilityEnvironment $v)) "any input DOWN blocks: $name"
}
$v=Copy-ReliabilityFixture $goodEnv;$v.result='BLOCKED';$v.all_required_inputs_up=$false;$v.keys.right.state='DOWN';$v.keys.right.high_bit_down=$true
Check-Reliability ((Get-ReliabilityButtonState $v) -ceq 'OBSERVED_UP' -and -not (Test-ReliabilityEnvironment $v)) 'reliable LEFT UP on blocked context is diagnostic, not READY'
$v.keys.left.state='DOWN';$v.keys.left.high_bit_down=$true
Check-Reliability ((Get-ReliabilityButtonState $v) -ceq 'OBSERVED_DOWN') 'reliable LEFT DOWN retained'
$v=Copy-ReliabilityFixture $goodEnv;$v.before.valid_context=$false;$v.keys.left.high_bit_down=$false
Check-Reliability ((Get-ReliabilityButtonState $v) -ceq 'UNKNOWN') 'invalid desktop zero is UNKNOWN not UP'
foreach($bad in @('UP','UNKNOWN')){
    $v=Copy-ReliabilityFixture $goodEnv;$v.keys.left.state=$bad;$v.keys.left.high_bit_down=$null
    Check-Reliability ((Get-ReliabilityButtonState $v) -ceq 'UNKNOWN') 'missing high-bit proof UNKNOWN'
}
foreach($name in @('start_qpc','finish_qpc')){
    $v=Copy-ReliabilityFixture $goodEnv;$v.keys.left.$name='1134816073663'
    Reject-Reliability {Test-ReliabilityEnvironment $v} "key clock string rejected $name"
    Check-Reliability ((Get-ReliabilityButtonState $v) -ceq 'UNKNOWN') 'invalid key clock observation UNKNOWN'
}
$v=Copy-ReliabilityFixture $goodEnv;$v.keys.left.finish_qpc=$v.after.start_qpc+1
Check-Reliability (-not (Test-ReliabilityEnvironment $v)) 'key sample outside context rejected'
$v=Copy-ReliabilityFixture $goodEnv;$v.keys.PSObject.Properties.Remove('escape')
Check-Reliability (-not (Test-ReliabilityEnvironment $v)) 'missing eleventh key rejects READY'
$v=Copy-ReliabilityFixture $goodEnv;$v.before.session_level=2;$v.after.session_level=2
Check-Reliability (-not (Test-ReliabilityEnvironment $v)) 'only actual WTS level1 context is eligible'
Check-Reliability ((Get-ReliabilityButtonState $v) -ceq 'UNKNOWN') 'level2 cannot be relabelled reliable UP'
$v=Copy-ReliabilityFixture $goodEnv;$v.qpc_frequency='10000000'
Reject-Reliability {Test-ReliabilityEnvironment $v} 'frequency must be typed integer'
$abort=[pscustomobject]@{Result='PASS';ContractVerified=$true;Operation='Move';TestMode='controlled_abort';FixtureResult='PASS_EXPECTED_ABORT';GestureResult='BLOCKED_BY_TEST_FAULT';CleanupResult='PASS';TakeoverAcceptance='NOT_RUN';Reasons=@('BLOCKED_BY_TEST_FAULT')}
$normal=[pscustomobject]@{Result='PASS';ContractVerified=$true;Operation='Move';TestMode='normal';FixtureResult='PASS';GestureResult='PASS';CleanupResult='NOT_NEEDED';TakeoverAcceptance='PASS';Reasons=@()}
Check-Reliability ((Get-ReliabilityExitCode Move controlled-abort 2 $abort $true $true) -eq 0) 'native2 maps runner0 only on complete expected-abort PASS'
Check-Reliability ((Get-ReliabilityExitCode Move normal 0 $normal $true $true) -eq 0) 'normal native0/full PASS maps0'
foreach($mode in @('controlled-abort','normal')){
    $proof=if($mode -ceq 'normal'){$normal}else{$abort};$code=if($mode -ceq 'normal'){0}else{2}
    foreach($bad in @([string]$code,[double]$code,$true,$null,1,3)){
        Check-Reliability ((Get-ReliabilityExitCode Move $mode $bad $proof $true $true) -eq 2) "exit cannot coerce $mode"
    }
    foreach($field in @('Result','Operation','TestMode','FixtureResult','GestureResult','TakeoverAcceptance')){
        $v=Copy-ReliabilityFixture $proof;$v.$field='wrong'
        Check-Reliability ((Get-ReliabilityExitCode Move $mode $code $v $true $true) -eq 2) "strict proof $mode/$field"
    }
    foreach($bad in @($false,'true',$null)){
        $v=Copy-ReliabilityFixture $proof;$v.ContractVerified=$bad
        Check-Reliability ((Get-ReliabilityExitCode Move $mode $code $v $true $true) -eq 2) 'ContractVerified must be true bool'
    }
    Check-Reliability ((Get-ReliabilityExitCode Move $mode $code $proof $false $true) -eq 2) 'environment not READY stops'
    Check-Reliability ((Get-ReliabilityExitCode Move $mode $code $proof $true $false) -eq 2) 'identity changed stops'
}
$v=Copy-ReliabilityFixture $abort;$v.CleanupResult='UNKNOWN'
Check-Reliability ((Get-ReliabilityExitCode Move controlled-abort 2 $v $true $true) -eq 2) 'native2 is not swallowed without cleanup PASS'
$v=Copy-ReliabilityFixture $abort;$v.Result='BLOCKED';$v.FixtureResult='BLOCKED';$v.ContractVerified=$false
Check-Reliability ((Get-ReliabilityExitCode Move controlled-abort 2 $v $true $true) -eq 2) 'ordinary BLOCKED/native2 is not expected-abort PASS'
# Preload JSON module BEFORE installing strictly in-memory command seams.
$null=$goodEnv|ConvertTo-Json -Depth 32 -Compress
$memory=@{};$hashReads=@{};$mutateHash=$null;$validatorCalls=0
function Memory-ReliabilityKey([string]$Path){return [IO.Path]::GetFullPath($Path)}
function Set-ReliabilityMemory([string]$Path,[string]$Text,[string]$Hash=('A'*64)){$script:memory[(Memory-ReliabilityKey $Path)]=[pscustomobject]@{Text=$Text;Hash=$Hash}}
function Get-Content([string]$LiteralPath,[switch]$Raw,[string]$Encoding){
    $key=Memory-ReliabilityKey $LiteralPath
    if(-not $memory.ContainsKey($key)){throw "No in-memory content seam: $key"}
    if($Raw){return $memory[$key].Text};return $memory[$key].Text -split "\r?\n"|Where-Object {$_}
}
function Get-FileHash([string]$LiteralPath,[string]$Algorithm){
    $key=Memory-ReliabilityKey $LiteralPath
    if($Algorithm -cne 'SHA256' -or -not $memory.ContainsKey($key)){throw "No in-memory SHA256 seam: $key"}
    if(-not $hashReads.ContainsKey($key)){$hashReads[$key]=0};$hashReads[$key]++
    $hash=$memory[$key].Hash
    if($key -ceq $mutateHash -and $hashReads[$key] -gt 1){$hash='F'*64}
    return [pscustomobject]@{Hash=$hash}
}
function Test-FixFInputReliabilityEvidence([string]$Path){
    if((Memory-ReliabilityKey $Path) -cne $log){throw 'Unexpected validator seam path'}
    $script:validatorCalls++;return $fixtureVerdict
}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'));$root=Join-Path $repo 'uat/r1c4b-fixf'
$snapshotNode=One-ReliabilityAst $single {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$files'} 'hash inventory expression'
foreach($Configuration in @('Debug','Release')){
    foreach($relative in @(& ([scriptblock]::Create($snapshotNode.Right.Extent.Text)))){Set-ReliabilityMemory (Join-Path $repo $relative) '' ('C'*64)}
}
$state=[ordered]@{ExecutedHEAD=('a'*40);Identity=@{}}
foreach($configuration in @('Debug','Release')){$state.Identity[$configuration]=Get-ReliabilitySnapshot $repo $configuration;Check-Reliability ($state.Identity[$configuration].Count -eq 19) "complete $configuration source/import/binary inventory"}
Check-Reliability (Test-ReliabilitySameSnapshot $state.Identity.Debug $state.Identity.Debug) 'identical inventory accepted'
$other=[ordered]@{};foreach($k in $state.Identity.Debug.Keys){$other[$k]=$state.Identity.Debug[$k]};$other[$other.Keys[0]]='D'*64
Check-Reliability (-not (Test-ReliabilitySameSnapshot $state.Identity.Debug $other)) 'one changed hash rejected'
$runId='1'*32;$directory=Join-Path $root ('synthetic-Debug-Move-controlled-abort-'+$runId)
$metadataPath=Join-Path $directory 'run.metadata.json';$log=Join-Path $directory 'probe.jsonl'
$pre=Join-Path $directory 'environment-pre.json';$post=Join-Path $directory 'environment-post.json'
$fixtureMetadata=[pscustomobject]@{Schema='r1c4b-input-reliability-run/v1';RunId=$runId;Configuration='Debug';Operation='Move';TestMode='controlled_abort';ExpectedHEAD=$state.ExecutedHEAD;ExecutedHEAD=$state.ExecutedHEAD;AfterHEAD=$state.ExecutedHEAD;ProbeInvoked=$true;RunnerExitCode=0;ImplementationUnchanged=$true;BeforeIdentity=$state.Identity.Debug;AfterIdentity=$state.Identity.Debug;ProbeExitCode=2;EnvironmentPrePath=$pre;EnvironmentPostPath=$post;EnvironmentPreExitCode=0;EnvironmentPostExitCode=0;EnvironmentPreReady=$true;EnvironmentPostReady=$true;EnvironmentPreSHA256=('A'*64);EnvironmentPostSHA256=('A'*64);EvidencePath=$log;LogSHA256=('B'*64);Result=$abort}
$fixtureVerdict=$abort
function Reset-ReliabilityMemory($Metadata=$fixtureMetadata,$PreValue=$goodEnv,$PostValue=$goodEnv){
    Set-ReliabilityMemory $metadataPath ($Metadata|ConvertTo-Json -Depth 32 -Compress) ('E'*64)
    Set-ReliabilityMemory $pre ($PreValue|ConvertTo-Json -Depth 32 -Compress)
    Set-ReliabilityMemory $post ($PostValue|ConvertTo-Json -Depth 32 -Compress)
    Set-ReliabilityMemory $log ('{"run_nonce":900000000000}'+"`n"+'{"type":"synthetic_row"}') ('B'*64)
    $script:hashReads=@{};$script:mutateHash=$null
    $script:nonces=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
}
Reset-ReliabilityMemory
$m=Read-ReliabilityRun $metadataPath $plan[0] $runId
Check-Reliability ($m.ValidatedNonce -eq 900000000000 -and $validatorCalls -eq 1) 'fresh actual helper validates memory-only complete fixture and Int64 nonce'
Reject-Reliability {Read-ReliabilityRun $metadataPath $plan[0] $runId} 'nonce reuse stops'
foreach($field in @('RunId','Configuration','Operation','TestMode','ExpectedHEAD','ExecutedHEAD','AfterHEAD','EvidencePath','LogSHA256','EnvironmentPrePath','EnvironmentPostPath','EnvironmentPreSHA256','EnvironmentPostSHA256')){
    $v=Copy-ReliabilityFixture $fixtureMetadata;$v.$field='wrong';Reset-ReliabilityMemory $v
    Reject-Reliability {Read-ReliabilityRun $metadataPath $plan[0] $runId} "bound metadata rejects $field"
}
foreach($field in @('ProbeInvoked','ImplementationUnchanged','EnvironmentPreReady','EnvironmentPostReady')){
    foreach($bad in @($false,'true')){
        $v=Copy-ReliabilityFixture $fixtureMetadata;$v.$field=$bad;Reset-ReliabilityMemory $v
        Reject-Reliability {Read-ReliabilityRun $metadataPath $plan[0] $runId} "strict metadata bool $field"
    }
}
foreach($field in @('RunnerExitCode','ProbeExitCode','EnvironmentPreExitCode','EnvironmentPostExitCode')){
    $v=Copy-ReliabilityFixture $fixtureMetadata;$v.$field=[string]$v.$field;Reset-ReliabilityMemory $v
    Reject-Reliability {Read-ReliabilityRun $metadataPath $plan[0] $runId} "strict metadata integer $field"
}
$v=Copy-ReliabilityFixture $fixtureMetadata;$property=$v.BeforeIdentity.PSObject.Properties|Select-Object -First 1;$property.Value='D'*64;Reset-ReliabilityMemory $v
Reject-Reliability {Read-ReliabilityRun $metadataPath $plan[0] $runId} 'hash inventory modified rejected'
$v=Copy-ReliabilityFixture $fixtureMetadata;$v.Result.CleanupResult='FAIL';Reset-ReliabilityMemory $v
Reject-Reliability {Read-ReliabilityRun $metadataPath $plan[0] $runId} 'cached four-field verdict cannot replace fresh raw proof'
$v=Copy-ReliabilityFixture $goodEnv;$v.context_reliable=$false;Reset-ReliabilityMemory $fixtureMetadata $goodEnv $v
Reject-Reliability {Read-ReliabilityRun $metadataPath $plan[0] $runId} 'post UNKNOWN forbids next run'
foreach($path in @($metadataPath,$log)){
    Reset-ReliabilityMemory;$mutateHash=Memory-ReliabilityKey $path
    Reject-Reliability {Read-ReliabilityRun $metadataPath $plan[0] $runId} 'evidence mutation during independent validation rejected'
}
Reset-ReliabilityMemory
Check-Reliability ((Get-ReliabilityAttemptButtonState $fixtureMetadata $directory $runId) -ceq 'OBSERVED_UP') 'failed fixture retains independent current post UP diagnostically'
$v=Copy-ReliabilityFixture $fixtureMetadata;$v.ProbeInvoked=$false;$v.EnvironmentPostSHA256=$null
$blocked=Copy-ReliabilityFixture $goodEnv;$blocked.result='BLOCKED';$blocked.all_required_inputs_up=$false;$blocked.keys.right.state='DOWN';$blocked.keys.right.high_bit_down=$true
Reset-ReliabilityMemory $v $blocked $goodEnv
Check-Reliability ((Get-ReliabilityAttemptButtonState $v $directory $runId) -ceq 'OBSERVED_UP') 'before-GUI BLOCKED retains reliable current pre LEFT UP without post'
$v=Copy-ReliabilityFixture $fixtureMetadata;$v.EnvironmentPostSHA256=$null;Reset-ReliabilityMemory $v
Check-Reliability ((Get-ReliabilityAttemptButtonState $v $directory $runId) -ceq 'UNKNOWN') 'after-native absent post proof cannot reuse pre UP'
$v=Copy-ReliabilityFixture $goodEnv;$v.context_reliable=$false;Reset-ReliabilityMemory $fixtureMetadata $goodEnv $v
Check-Reliability ((Get-ReliabilityAttemptButtonState $fixtureMetadata $directory $runId) -ceq 'UNKNOWN') 'post invalid context remains UNKNOWN'
Reset-ReliabilityMemory;$mutateHash=Memory-ReliabilityKey $post
Check-Reliability ((Get-ReliabilityAttemptButtonState $fixtureMetadata $directory $runId) -ceq 'UNKNOWN') 'post readonly hash mutation cannot claim observed UP'
Check-Reliability ((Get-ReliabilityAttemptButtonState $fixtureMetadata $directory ('2'*32)) -ceq 'UNKNOWN') 'diagnostic observation also bound to current run identity'
Write-Host "input reliability runner synthetic_only=true checks=$checks PASS"
