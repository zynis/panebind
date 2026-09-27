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
$singleHelpers=@('Get-ReliabilityField','Test-ReliabilityTrue','Test-ReliabilityFalse','Get-ReliabilitySnapshot','Test-ReliabilitySameSnapshot','Test-ReliabilityEnvironmentContext','Get-ReliabilityButtonState','Get-ReliabilityButtonsObservation','Test-ReliabilityEnvironment','Get-ReliabilityExitCode','Set-ReliabilityFirstFailure','Complete-ReliabilityRun','Get-ReliabilityNativeSummary','Get-ReliabilityAfterIdentity')
$gateHelpers=@('Get-ReliabilityPlan','Assert-ReliabilityGate','Test-ReliabilityBlockedVerdict','Get-ReliabilityAttemptButtonState','Read-ReliabilityRun')
$allowed=@($singleHelpers)+@($gateHelpers)+@('Assert-IsolationInt64','Get-FileHash','Get-Content','ConvertFrom-Json','Join-Path','Split-Path','ForEach-Object','Where-Object','Add-Member','Test-FixFInputReliabilityEvidence','Invoke-ReliabilityPostObservation','git')
foreach($pair in @(@($single,$singleHelpers),@($gates,$gateHelpers),@($numeric,@('Assert-IsolationInt64')))){
    foreach($name in $pair[1]){
        $node=One-ReliabilityAst $pair[0] {param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name} "helper $name"
        Check-Reliability (@($node.Body.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -cnotin $allowed},$true)).Count -eq 0) "$name uses only pure/memory seams"
        . ([scriptblock]::Create($node.Extent.Text))
    }
}
$native=One-ReliabilityAst $single {param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.CommandElements[0].Extent.Text -ceq '$probe'} 'single native invocation'
$envCalls=@($single.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.CommandElements[0].Extent.Text -ceq '$environment'},$true))
Check-Reliability ($envCalls.Count -eq 1 -and $envCalls[0].Extent.EndOffset -lt $native.Extent.StartOffset) 'one pre invocation before native'
$postHelper=One-ReliabilityAst $single {param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq 'Invoke-ReliabilityPostObservation'} 'one post helper'
Check-Reliability (@($postHelper.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.CommandElements[0].Extent.Text -ceq '$Environment'},$true)).Count -eq 1) 'one post invocation site'
$completeSite=One-ReliabilityAst $single {param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -ceq 'Complete-ReliabilityRun'} 'completion call'
Check-Reliability ($completeSite.Extent.StartOffset -gt $native.Extent.EndOffset) 'synchronous native ends before finalization; no retry'
foreach($needle in @('Require a clean checkpoint before environment/probe execution','Expected aggregate checkpoint changed before environment/probe execution','Current input state is not reliably READY','Implementation changed before native input','Checkpoint changed before native input')){
    $guard=One-ReliabilityAst $single {param($n) $n -is [Management.Automation.Language.ThrowStatementAst] -and $n.Extent.Text.Contains($needle)} "guard $needle"
    Check-Reliability ($guard.Extent.EndOffset -lt $native.Extent.StartOffset) "guard before native: $needle"
}
$clean=One-ReliabilityAst $single {param($n) $n -is [Management.Automation.Language.ThrowStatementAst] -and $n.Extent.Text.Contains('Require a clean checkpoint before')} 'clean before environment'
Check-Reliability ($clean.Extent.EndOffset -lt $envCalls[0].Extent.StartOffset) 'dirty checkpoint executes no readonly/native CLI'
$expectedGuard=One-ReliabilityAst $single {param($n) $n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text -ceq '$ExpectedHEAD -and $head -cne $ExpectedHEAD'} 'single expected HEAD guard'
Check-Reliability ($expectedGuard.Extent.EndOffset -lt $envCalls[0].Extent.StartOffset) 'aggregate expected HEAD verified before first readonly CLI'
Check-Reliability ($single.Extent.Text.Contains('}catch{$metadata.NativeError=$_.Exception.Message;') -and $single.Extent.Text.Contains("`$metadata.NativeProcessState='EXITED'")) 'native invocation error retained before mandatory post'
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
$failure=One-ReliabilityAst $gates {param($n) $n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text -ceq '$paths.Count -ne 1 -or -not (Test-ReliabilityTrue $item.ProbeInvoked)'} 'first failure branch'
Check-Reliability (@($failure.FindAll({param($n) $n -is [Management.Automation.Language.ThrowStatementAst]},$true)).Count -eq 1) 'first child failure throws STOP'
foreach($name in @('MetadataSHA256','LogSHA256','FixtureResult','GestureResult','CleanupResult','TakeoverAcceptance','FinalButtonState')){
    Check-Reliability ($gates.Extent.Text.Substring($child.Extent.EndOffset,$failure.Extent.StartOffset-$child.Extent.EndOffset).Contains($name)) "failure item captures $name before STOP"
}
$plan=@(Get-ReliabilityPlan)
$expected=@('Debug/Move/controlled-abort','Debug/BottomResize/controlled-abort','Debug/Move/normal','Debug/BottomResize/normal','Release/Move/controlled-abort','Release/BottomResize/controlled-abort','Release/Move/normal','Release/BottomResize/normal')
Check-Reliability ($plan.Count -eq 8) 'exact eight fixtures'
for($i=0;$i -lt $expected.Count;++$i){Check-Reliability ($plan[$i].Number -eq $i+1 -and ($plan[$i].Configuration+'/'+$plan[$i].Operation+'/'+$plan[$i].Mode) -ceq $expected[$i]) "immutable ordered fixture $($i+1)"}
function New-ReliabilityContext([long]$Start){
    return [pscustomobject]@{start_qpc=$Start;finish_qpc=($Start+10);input_name='Default';thread_name='Default';station_name='WinSta0';input_query_succeeded=$true;thread_query_succeeded=$true;station_query_succeeded=$true;session_query_succeeded=$true;session_level=1;session_id=1;session_state=0;session_flags=1;foreground_hwnd=123456;foreground_pid=2222;foreground_tid=3333;caller_integrity=8192;foreground_integrity=8192;caller_integrity_known=$true;foreground_integrity_known=$true;desktop_hook_access_verified=$true;valid_context=$true;context_complete=$true;foreground_identity_query_succeeded=$true}
}
function New-ReliabilityEnvironment{
    [long]$base=1134816073663;$keys=[ordered]@{};$offset=11
    foreach($name in @('left','right','middle','x1','x2','ctrl','shift','alt','lwin','rwin','escape')){
        $keys[$name]=[pscustomobject]@{start_qpc=($base+$offset);finish_qpc=($base+$offset+1);query_attempted=$true;state='UP';high_bit_down=$false};$offset+=2
    }
    $first=[pscustomobject]@{hwnd=123456;pid=2222;tid=3333;start_qpc=($base+100);finish_qpc=($base+110);identity_query_attempted=$true;identity_query_succeeded=$true;error=0}
    $last=[pscustomobject]@{hwnd=123456;pid=2222;tid=3333;start_qpc=($base+150);finish_qpc=($base+160);identity_query_attempted=$true;identity_query_succeeded=$true;error=0}
    $gui=[pscustomobject]@{query_tid=3333;query_attempted=$true;query_start_qpc=($base+120);query_finish_qpc=($base+130);query_succeeded=$true;error=0;foreground_before=$first;foreground_after=$last;foreground_tuple_stable=$true;capture_hwnd=0;menu_owner_hwnd=0;move_size_hwnd=0;gui_flags=0}
    $predicates=[ordered]@{};foreach($name in @('QPC_VALID','BEFORE_CONTEXT_VALID','AFTER_CONTEXT_VALID','OBSERVATION_CONTEXT_STABLE','BUTTONS_OBSERVED_UP','INPUT_MAPPING_SUPPORTED','FOREGROUND_GUI_QUERY','FOREGROUND_GUI_TUPLE_STABLE','CAPTURE_CLEAR','MENU_CLEAR','MOVE_SIZE_CLEAR','DISALLOWED_GUI_FLAGS_CLEAR')){$predicates[$name]='PASS'}
    return [pscustomobject]@{schema='r1c4b-readonly-input-environment/v2';startup_contract='foreground_gui_readiness_v1';result='READY';startup_readiness='READY';startup_ready=$true;buttons_observed_up=$true;buttons_observation='OBSERVED_UP';readiness_predicates=[pscustomobject]$predicates;first_failed_predicate='NONE';failed_predicates=@();foreground_gui=$gui;read_only=$true;atomic_snapshot=$false;context_reliable=$true;all_required_inputs_up=$true;mouse_buttons_swapped=$false;qpc_frequency=10000000;before=(New-ReliabilityContext $base);after=(New-ReliabilityContext ($base+200));keys=[pscustomobject]$keys}
}
$goodEnv=New-ReliabilityEnvironment
Check-Reliability (Test-ReliabilityEnvironment $goodEnv) 'reliable eleven UP with real-sized QPC'
Check-Reliability ((Get-ReliabilityButtonState $goodEnv) -ceq 'OBSERVED_UP') 'reliable current LEFT UP observation'
$legacy=Copy-ReliabilityFixture $goodEnv;$legacy.schema='r1c4b-readonly-input-environment/v1'
Check-Reliability (-not (Test-ReliabilityEnvironment $legacy)) 'historical v1 READY never authorizes modern startup'
Check-Reliability ((Get-ReliabilityButtonsObservation $legacy) -ceq 'OBSERVED_UP') 'historical v1 complete eleven UP remains interpretable'
foreach($name in @('capture_hwnd','menu_owner_hwnd','move_size_hwnd','gui_flags')){
    $v=Copy-ReliabilityFixture $goodEnv;$v.foreground_gui.$name=2
    Check-Reliability (-not (Test-ReliabilityEnvironment $v)) "raw GUI state defeats cached READY $name"
    Check-Reliability ((Get-ReliabilityButtonsObservation $v) -ceq 'OBSERVED_UP') "GUI blocker does not erase reliable UP $name"
}
$v=Copy-ReliabilityFixture $goodEnv;$v.foreground_gui.gui_flags=1
Check-Reliability (Test-ReliabilityEnvironment $v) 'caret blinking is not a prohibited GUI mode'
$v=Copy-ReliabilityFixture $goodEnv;$v.foreground_gui.query_succeeded=$false;$v.foreground_gui.error=5
foreach($name in @('capture_hwnd','menu_owner_hwnd','move_size_hwnd','gui_flags')){$v.foreground_gui.$name=$null}
Check-Reliability (-not (Test-ReliabilityEnvironment $v)) 'GUI query failure never authorizes startup'
Check-Reliability ((Get-ReliabilityButtonsObservation $v) -ceq 'OBSERVED_UP') 'stable tuple and reliable keys survive GUI query failure'
foreach($name in @('hwnd','pid','tid')){
    $v=Copy-ReliabilityFixture $goodEnv;$v.foreground_gui.foreground_after.$name++
    Check-Reliability ((Get-ReliabilityButtonsObservation $v) -ceq 'UNKNOWN') "intervening foreground tuple change invalidates UP $name"
}
$v=Copy-ReliabilityFixture $goodEnv;$v.foreground_gui.query_tid++
Check-Reliability (-not (Test-ReliabilityEnvironment $v)) 'actual queried thread must equal foreground TID'
$v=Copy-ReliabilityFixture $goodEnv;$v.foreground_gui.foreground_before.start_qpc=$v.keys.escape.finish_qpc-1
Check-Reliability ((Get-ReliabilityButtonsObservation $v) -ceq 'UNKNOWN') 'keys must precede GUI observation'
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
$memory=@{};$hashReads=@{};$mutateHash=$null;$validatorCalls=0;$failHash=$null;$trackFinalization=$false;$trace=[Collections.Generic.List[string]]::new();$validatorFailure=$false
function Memory-ReliabilityKey([string]$Path){return [IO.Path]::GetFullPath($Path)}
function Set-ReliabilityMemory([string]$Path,[string]$Text,[string]$Hash=('A'*64)){$script:memory[(Memory-ReliabilityKey $Path)]=[pscustomobject]@{Text=$Text;Hash=$Hash}}
function Get-Content([string]$LiteralPath,[switch]$Raw,[string]$Encoding){
    $key=Memory-ReliabilityKey $LiteralPath
    if(-not $memory.ContainsKey($key)){throw "No in-memory content seam: $key"}
    if($Raw){return $memory[$key].Text};return $memory[$key].Text -split "\r?\n"|Where-Object {$_}
}
function Get-FileHash([string]$LiteralPath,[string]$Algorithm){
    $key=Memory-ReliabilityKey $LiteralPath
    if($trackFinalization){$trace.Add('hash:'+ $key)}
    if($key -ceq $failHash){throw 'Synthetic individual hash read failure'}
    if($Algorithm -cne 'SHA256' -or -not $memory.ContainsKey($key)){throw "No in-memory SHA256 seam: $key"}
    if(-not $hashReads.ContainsKey($key)){$hashReads[$key]=0};$hashReads[$key]++
    $hash=$memory[$key].Hash
    if($key -ceq $mutateHash -and $hashReads[$key] -gt 1){$hash='F'*64}
    return [pscustomobject]@{Hash=$hash}
}
function Test-FixFInputReliabilityEvidence([string]$Path){
    if((Memory-ReliabilityKey $Path) -cne $log){throw 'Unexpected validator seam path'}
    if($trackFinalization){$trace.Add('validator')}
    if($validatorFailure){throw 'Synthetic validator exception'}
    $script:validatorCalls++;return $fixtureVerdict
}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'));$root=Join-Path $repo 'uat/r1c4b-fixg'
$snapshotNode=One-ReliabilityAst $single {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$files'} 'hash inventory expression'
foreach($Configuration in @('Debug','Release')){
    foreach($relative in @(& ([scriptblock]::Create($snapshotNode.Right.Extent.Text)))){Set-ReliabilityMemory (Join-Path $repo $relative) '' ('C'*64)}
}
$state=[ordered]@{ExecutedHEAD=('a'*40);Identity=@{}}
foreach($configuration in @('Debug','Release')){$state.Identity[$configuration]=Get-ReliabilitySnapshot $repo $configuration;Check-Reliability ($state.Identity[$configuration].Count -eq 19) "complete $configuration source/import/binary inventory"}
Check-Reliability (Test-ReliabilitySameSnapshot $state.Identity.Debug $state.Identity.Debug) 'identical inventory accepted'
$other=[ordered]@{};foreach($k in $state.Identity.Debug.Keys){$other[$k]=$state.Identity.Debug[$k]};$other[@($other.Keys)[0]]='D'*64
Check-Reliability (-not (Test-ReliabilitySameSnapshot $state.Identity.Debug $other)) 'one changed hash rejected'
$runId='1'*32;$directory=Join-Path $root ('synthetic-Debug-Move-controlled-abort-'+$runId)
$metadataPath=Join-Path $directory 'run.metadata.json';$log=Join-Path $directory 'probe.jsonl'
$pre=Join-Path $directory 'environment-pre.json';$post=Join-Path $directory 'environment-post.json'
$fixtureMetadata=[pscustomobject]@{Schema='r1c4b-fixg-input-reliability-run/v2';RunId=$runId;Configuration='Debug';Operation='Move';TestMode='controlled_abort';ExpectedHEAD=$state.ExecutedHEAD;ExecutedHEAD=$state.ExecutedHEAD;AfterHEAD=$state.ExecutedHEAD;ProbeInvoked=$true;RunnerExitCode=0;ImplementationUnchanged=$true;BeforeIdentity=$state.Identity.Debug;AfterIdentity=$state.Identity.Debug;ProbeExitCode=2;EnvironmentPrePath=$pre;EnvironmentPostPath=$post;EnvironmentPreExitCode=0;EnvironmentPostExitCode=0;EnvironmentPreReady=$true;EnvironmentPostReady=$true;EnvironmentPreSHA256=('A'*64);EnvironmentPostSHA256=('A'*64);EvidencePath=$log;LogSHA256=('B'*64);Result=$abort;NativeProcessState='EXITED';NativeSummaryStatus='RAW_RECORDED';LogHashStatus='COLLECTED';AfterIdentityStatus='COLLECTED';AfterHEADStatus='COLLECTED';AfterWorktreeStatus='COLLECTED';AfterHashStatus='COLLECTED';AfterWorktree=@();PostObservationStatus='COLLECTED';EvidenceValidationResult='COMPLETED'}
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
Check-Reliability ((Get-ReliabilityAttemptButtonState $v $directory $runId) -ceq 'OBSERVED_DOWN') 'before-GUI BLOCKED retains current pre required-input DOWN without post'
$v=Copy-ReliabilityFixture $fixtureMetadata;$v.EnvironmentPostSHA256=$null;Reset-ReliabilityMemory $v
Check-Reliability ((Get-ReliabilityAttemptButtonState $v $directory $runId) -ceq 'UNKNOWN') 'after-native absent post proof cannot reuse pre UP'
$v=Copy-ReliabilityFixture $goodEnv;$v.context_reliable=$false;Reset-ReliabilityMemory $fixtureMetadata $goodEnv $v
Check-Reliability ((Get-ReliabilityAttemptButtonState $fixtureMetadata $directory $runId) -ceq 'UNKNOWN') 'post invalid context remains UNKNOWN'
Reset-ReliabilityMemory;$mutateHash=Memory-ReliabilityKey $post
Check-Reliability ((Get-ReliabilityAttemptButtonState $fixtureMetadata $directory $runId) -ceq 'UNKNOWN') 'post readonly hash mutation cannot claim observed UP'
Check-Reliability ((Get-ReliabilityAttemptButtonState $fixtureMetadata $directory ('2'*32)) -ceq 'UNKNOWN') 'diagnostic observation also bound to current run identity'
# An independently verified prefix is BLOCKED, never fixture PASS, including
# when the post GUI context blocks startup but all required inputs are UP.
$prefix=[pscustomobject]@{Result='BLOCKED';EvidenceIntegrity='VALID';PrefixValidation='VERIFIED_BLOCKED_BEFORE_INPUT';ExecutionResult='BLOCKED';PrefixKind='BOOTSTRAP_BLOCKED_BEFORE_ANY_TEST_INPUT';BlockPhase='FOREGROUND_BOOTSTRAP';NativeBlockReason='BLOCKED_BY_FOREIGN_INPUT_CAPTURE';Operation='Move';TestMode='controlled_abort';FixtureResult='BLOCKED';GestureResult='NOT_RUN';CleanupResult='NOT_RUN';TakeoverAcceptance='NOT_RUN';InputAttempted=$false;TestDownPending=$false;ContractVerified=$false;FirstFailurePhase='FOREGROUND_BOOTSTRAP';FirstFailureRecordSequences=@(12,14,15);Reasons=@('BLOCKED_BY_FOREIGN_INPUT_CAPTURE')}
$v=Copy-ReliabilityFixture $fixtureMetadata;$v.RunnerExitCode=2;$v.Result=$prefix;$v.EnvironmentPostExitCode=2;$v.EnvironmentPostReady=$false
$postBlocked=Copy-ReliabilityFixture $goodEnv;$postBlocked.foreground_gui.capture_hwnd=4444;$postBlocked.startup_ready=$false;$postBlocked.startup_readiness='BLOCKED';$postBlocked.result='BLOCKED';$postBlocked.first_failed_predicate='CAPTURE_CLEAR';$postBlocked.failed_predicates=@('CAPTURE_CLEAR');$postBlocked.readiness_predicates.CAPTURE_CLEAR='FAIL'
$fixtureVerdict=$prefix;Reset-ReliabilityMemory $v $goodEnv $postBlocked
$m=Read-ReliabilityRun $metadataPath $plan[0] $runId
Check-Reliability ($m.IndependentlyValidatedResult -ceq 'VERIFIED_BLOCKED' -and $m.RunnerExitCode -eq 2 -and $m.Result.FixtureResult -ceq 'BLOCKED' -and $m.Result.CleanupResult -ceq 'NOT_RUN') 'fresh independent blocked proof preserves STOP2 and NOT_RUN layers'
$passAssignment=One-ReliabilityAst $gates {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$item.Passed'} 'only fixture PASS assignment'
$blockedStop=One-ReliabilityAst $gates {param($n) $n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text -ceq "`$m.IndependentlyValidatedResult -cne 'PASS'"} 'independent BLOCKED stop'
Check-Reliability ($blockedStop.Extent.EndOffset -lt $passAssignment.Extent.StartOffset -and @($blockedStop.FindAll({param($n) $n -is [Management.Automation.Language.ThrowStatementAst]},$true)).Count -eq 1) 'valid BLOCKED throws STOP before PASS/count/next child'
$v.Result.ContractVerified=$true;Reset-ReliabilityMemory $v $goodEnv $postBlocked
Reject-Reliability {Read-ReliabilityRun $metadataPath $plan[0] $runId} 'cached prefix cannot turn ContractVerified into fixture PASS'

# Import the actual post helper only after installing its strictly in-memory
# environment function. Complete-ReliabilityRun and AfterIdentity are the
# production helpers; no runner program, native process or real Git is called.
. ([scriptblock]::Create($postHelper.Extent.Text))
$headFailure=$false;$statusFailure=$false;$postFailure=$false;$postCalls=0
function Invoke-ReliabilityEnvironmentSeam{
    $script:postCalls++;$trace.Add('post');$script:LASTEXITCODE=0
}
function git{
    if(($args -join ' ') -ceq 'rev-parse HEAD'){
        $trace.Add('HEAD');if($headFailure){$script:LASTEXITCODE=1;return};$script:LASTEXITCODE=0;return $state.ExecutedHEAD
    }
    if(($args -join ' ') -ceq 'status --porcelain'){
        $trace.Add('status');if($statusFailure){$script:LASTEXITCODE=1;return};$script:LASTEXITCODE=0;return
    }
    throw 'Synthetic Git seam accepts only two read-only commands'
}
function New-ReliabilityFinalizationFixture{
    $Configuration='Debug';$Operation='Move';$Mode='controlled-abort';$RunId=$runId;$ExpectedHEAD=$state.ExecutedHEAD;$path=$log
    $f=& ([scriptblock]::Create($metadata.Right.Extent.Text))
    $f.ExecutedHEAD=$state.ExecutedHEAD;$f.BeforeIdentity=$state.Identity.Debug;$f.ProbeInvoked=$true;$f.ProbeExitCode=2;$f.NativeProcessState='EXITED'
    return $f
}
function Reset-ReliabilityFinalization{
    $script:headFailure=$false;$script:statusFailure=$false;$script:postFailure=$false;$script:validatorFailure=$false;$script:failHash=$null;$script:postCalls=0;$script:trackFinalization=$true;$script:trace=[Collections.Generic.List[string]]::new();$script:fixtureVerdict=$prefix
    Reset-ReliabilityMemory $fixtureMetadata $goodEnv $goodEnv
    Set-ReliabilityMemory $log ('{"sequence":1,"type":"startup","run_nonce":900000000000}'+"`n"+'{"sequence":12,"type":"activation_fence"}'+"`n"+'{"sequence":14,"type":"foreground_bootstrap"}'+"`n"+'{"sequence":15,"type":"blocked","reason":"BLOCKED_BY_FOREIGN_INPUT_CAPTURE"}'+"`n"+'{"sequence":28,"type":"shutdown"}') ('B'*64)
}
Reset-ReliabilityFinalization
$final=New-ReliabilityFinalizationFixture
Complete-ReliabilityRun $final $repo Debug Invoke-ReliabilityEnvironmentSeam $post $log
Check-Reliability ($postCalls -eq 1 -and $trace[0] -ceq 'post' -and $trace.IndexOf('HEAD') -gt 0 -and $trace.IndexOf('status') -gt $trace.IndexOf('HEAD') -and $trace[$trace.Count-1] -ceq 'validator') 'actual finalization orders one post then independent identity then validator'
Check-Reliability ($final.RunnerExitCode -eq 2 -and $final.NativeBlockReason -ceq 'BLOCKED_BY_FOREIGN_INPUT_CAPTURE' -and $final.EvidenceValidationResult -ceq 'COMPLETED' -and $final.FirstFailureStage -ceq 'FOREGROUND_BOOTSTRAP') 'valid prefix remains BLOCKED with original reason and sequence references'
Complete-ReliabilityRun $final $repo Debug Invoke-ReliabilityEnvironmentSeam $post $log
Check-Reliability ($postCalls -eq 1) 'completed post observation never repeated'
foreach($failure in @('validator','post','HEAD','status','hash')){
    Reset-ReliabilityFinalization;$final=New-ReliabilityFinalizationFixture
    switch($failure){
        validator {$validatorFailure=$true}
        post {$memory.Remove((Memory-ReliabilityKey $post))}
        HEAD {$headFailure=$true}
        status {$statusFailure=$true}
        hash {$failHash=Memory-ReliabilityKey (Join-Path $repo @($state.Identity.Debug.Keys)[0])}
    }
    Complete-ReliabilityRun $final $repo Debug Invoke-ReliabilityEnvironmentSeam $post $log
    Check-Reliability ($postCalls -eq 1 -and $trace.Contains('HEAD') -and $trace.Contains('status') -and $trace.Contains('validator') -and $final.RunnerExitCode -eq 2) "other independent finalization steps run after $failure exception"
    Check-Reliability ($final.NativeBlockReason -ceq 'BLOCKED_BY_FOREIGN_INPUT_CAPTURE' -and $final.NativeBlockRecordSequences[0] -eq 15 -and $final.RawEvidenceReferences.ActivationFence[0] -eq 12) "raw native blocker and references survive $failure error"
    if($failure -ceq 'validator'){Check-Reliability ($final.ValidatorError -ceq 'Synthetic validator exception' -and $final.AfterIdentityStatus -ceq 'COLLECTED' -and (Test-ReliabilityTrue $final.ImplementationUnchanged)) 'validator exception preserves successful after identity'}
    if($failure -ceq 'post'){Check-Reliability ($final.PostObservationStatus -ceq 'ERROR' -and $final.AfterIdentityStatus -ceq 'COLLECTED' -and $final.EvidenceValidationResult -ceq 'COMPLETED') 'post read failure preserves identity and evidence validation'}
    if($failure -cin @('HEAD','status','hash')){Check-Reliability ($final.AfterIdentityStatus -ceq 'ERROR' -and $final.ImplementationUnchanged -ceq 'UNKNOWN') "unavailable identity is UNKNOWN after $failure : status=$($final.AfterIdentityStatus); unchanged=$($final.ImplementationUnchanged); hasherror=$($final.AfterHashError); failedpath=$failHash"}
    if($failure -ceq 'hash'){Check-Reliability ($final.AfterIdentity.Count -eq 19 -and @($final.AfterIdentity.Values|Where-Object {$_ -ceq 'NOT_AVAILABLE'}).Count -eq 1) 'one hash failure preserves all other hashes and explicit missing entry'}
}
Reset-ReliabilityFinalization;$final=New-ReliabilityFinalizationFixture;$validatorFailure=$true;$headFailure=$true;$memory.Remove((Memory-ReliabilityKey $post))
Complete-ReliabilityRun $final $repo Debug Invoke-ReliabilityEnvironmentSeam $post $log
Check-Reliability ($final.ValidatorError -and $final.PostObservationError -and $final.AfterIdentityError -and $final.NativeBlockReason -ceq 'BLOCKED_BY_FOREIGN_INPUT_CAPTURE' -and $final.RunnerExitCode -eq 2) 'simultaneous native blocker/post/identity/validator errors coexist'
Reset-ReliabilityFinalization;$final=New-ReliabilityFinalizationFixture;$fixtureVerdict=$abort
Set-ReliabilityMemory $log ('{"sequence":1,"type":"startup","run_nonce":900000000000}'+"`n"+'{"sequence":28,"type":"shutdown"}') ('B'*64)
Complete-ReliabilityRun $final $repo Debug Invoke-ReliabilityEnvironmentSeam $post $log
Check-Reliability ($final.RunnerExitCode -eq 0 -and (Test-ReliabilityTrue $final.ImplementationUnchanged)) 'complete expected-abort verdict and successful independent finalization maps to runner0'
Reset-ReliabilityFinalization;$final=New-ReliabilityFinalizationFixture;$fixtureVerdict=$abort;$failHash=Memory-ReliabilityKey $log
Complete-ReliabilityRun $final $repo Debug Invoke-ReliabilityEnvironmentSeam $post $log
Check-Reliability ($final.LogHashStatus -ceq 'ERROR' -and $final.NativeBlockReason -ceq 'BLOCKED_BY_FOREIGN_INPUT_CAPTURE' -and $final.RunnerExitCode -eq 2) 'raw log hash error preserves native reason and prevents runner PASS'
Reset-ReliabilityFinalization;$final=New-ReliabilityFinalizationFixture;$fixtureVerdict=$abort
$changePath=Memory-ReliabilityKey (Join-Path $repo @($state.Identity.Debug.Keys)[0]);$memory[$changePath].Hash='D'*64
Complete-ReliabilityRun $final $repo Debug Invoke-ReliabilityEnvironmentSeam $post $log
Check-Reliability ($final.AfterIdentityStatus -ceq 'COLLECTED' -and (Test-ReliabilityFalse $final.ImplementationUnchanged) -and $final.RunnerExitCode -eq 2) 'collected changed identity is false, distinct from UNKNOWN and true'
$memory[$changePath].Hash='C'*64
foreach($ast in @($single,$gates)){
    Check-Reliability ($ast.Extent.Text.Contains("MetadataWriteStatus='ERROR'") -and $ast.Extent.Text.Contains('metadata write failed:') -or $ast.Extent.Text.Contains('inventory write failed:')) 'metadata write failure explicitly reports evidence error'
}
# Execute only the actual guarded write statement with a throwing memory seam.
function Write-ReliabilityNewJson{throw 'Synthetic evidence write failure'}
$writeErrors=[Collections.Generic.List[string]]::new()
function Write-Error([string]$Message,[string]$ErrorAction){$writeErrors.Add($Message)}
foreach($pair in @(@($single,'$metadata','RunnerExitCode'),@($gates,'$state','AggregateExitCode'))){
    $writeGuard=One-ReliabilityAst $pair[0] {param($n) $n -is [Management.Automation.Language.TryStatementAst] -and $n.Body.Extent.Text.Contains("MetadataWriteStatus='WRITTEN'") -and $n.Body.Extent.Text.Contains('Write-ReliabilityNewJson') -and -not $n.Body.Extent.Text.Contains("MetadataWriteStatus='ERROR'")} 'guarded evidence writer'
    if($pair[1] -ceq '$metadata'){$metadata=[ordered]@{MetadataWriteStatus='NOT_RUN';MetadataWriteError=$null;RunnerExitCode=0};$metadataPath='MEMORY_ONLY'}else{$state=[ordered]@{MetadataWriteStatus='NOT_RUN';MetadataWriteError=$null;AggregateExitCode=0};$summary='MEMORY_ONLY'}
    & ([scriptblock]::Create($writeGuard.Extent.Text))
    $saved=if($pair[1] -ceq '$metadata'){$metadata}else{$state}
    Check-Reliability ($saved.MetadataWriteStatus -ceq 'ERROR' -and $saved.MetadataWriteError -ceq 'Synthetic evidence write failure' -and $saved[$pair[2]] -eq 2) 'actual guarded metadata failure returns explicit nonzero evidence error'
}
Check-Reliability ($writeErrors.Count -eq 2) 'both evidence write failures were reported'
Write-Host "input reliability runner synthetic_only=true checks=$checks PASS"
