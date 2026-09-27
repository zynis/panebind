[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [ValidateSet('Move','BottomResize')][string]$Operation='Move',
    [ValidateSet('controlled-abort','normal')][string]$Mode='controlled-abort',
    [ValidatePattern('^[a-f0-9]{32}$')][string]$RunId=([Guid]::NewGuid().ToString('N')),
    [ValidatePattern('^$|^[a-f0-9]{40}$')][string]$ExpectedHEAD=''
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-native-abort-validation.ps1')
function Get-ReliabilityField($Value,[string]$Name){
    if($null -eq $Value){return $null}
    if($Value -is [Collections.IDictionary]){return $Value[$Name]}
    $p=$Value.PSObject.Properties[$Name];if($null -eq $p){return $null};return $p.Value
}
function Test-ReliabilityTrue($Value){return $Value -is [bool] -and $Value}
function Test-ReliabilityFalse($Value){return $Value -is [bool] -and -not $Value}
function Get-ReliabilitySnapshot([string]$Repo,[string]$Configuration,$Errors=$null){
    $files=@(
        'src/platform/windows/operations/magnet_takeover_probe.cpp',
        'src/platform/windows/operations/test_input_environment_probe.cpp',
        'src/platform/windows/operations/test_foreground_bootstrap_model.h',
        'src/platform/windows/operations/window_rect_adjustment.h',
        'src/platform/windows/operations/magnet_postverify_diagnostic.h',
        'src/platform/windows/operations/magnet_handoff_preflight_diagnostic.h',
        'src/platform/windows/operations/magnet_input_isolation_diagnostic.h',
        'src/platform/windows/operations/magnet_owned_abort_diagnostic.h',
        'scripts/r1c4b-native-abort-validation.ps1','scripts/r1c4b-normal-v5-primitives.ps1',
        'scripts/r1c4b-input-isolation-validation.ps1','scripts/r1c4b-end-diagnostics-validation.ps1',
        'scripts/r1c4b-end-handoff-validation.ps1','scripts/r1c4b-takeover-owned-validation.ps1',
        'scripts/r1c4b-auto-owned-modal-validation.ps1',
        'scripts/run-r1c4b-input-reliability.ps1','scripts/run-r1c4b-input-reliability-gates.ps1',
        "out/r1c4b-live-magnet-$($Configuration.ToLowerInvariant())/src/platform/windows/$Configuration/panebind-magnet-takeover-probe.exe",
        "out/r1c4b-live-magnet-$($Configuration.ToLowerInvariant())/src/platform/windows/$Configuration/panebind-test-input-environment.exe"
    )
    $result=[ordered]@{}
    foreach($file in $files){
        try{$result[$file]=(Get-FileHash -LiteralPath (Join-Path $Repo $file) -Algorithm SHA256).Hash}catch{
            if($null -eq $Errors){throw}
            $result[$file]='NOT_AVAILABLE';$Errors.Add($file+': '+$_.Exception.Message)
        }
    }
    return $result
}
function Test-ReliabilitySameSnapshot($Before,$After){
    $a=@($Before.Keys);$b=@($After.Keys)
    if($a.Count -ne $b.Count){return $false}
    foreach($name in $a){if(-not $After.Contains($name) -or $Before[$name] -cne $After[$name]){return $false}}
    return $true
}
function Test-ReliabilityEnvironmentContext($Value){
    # v1 is retained only for interpreting immutable historical observations.
    if((Get-ReliabilityField $Value 'schema') -cnotin @('r1c4b-readonly-input-environment/v1','r1c4b-readonly-input-environment/v2')){return $false}
    foreach($name in @('read_only','context_reliable')){if(-not (Test-ReliabilityTrue (Get-ReliabilityField $Value $name))){return $false}}
    if(-not (Test-ReliabilityFalse (Get-ReliabilityField $Value 'atomic_snapshot'))){return $false}
    $before=Get-ReliabilityField $Value 'before';$after=Get-ReliabilityField $Value 'after'
    if(-not (Test-ReliabilityTrue (Get-ReliabilityField $before 'valid_context')) -or -not (Test-ReliabilityTrue (Get-ReliabilityField $after 'valid_context'))){return $false}
    foreach($name in @('foreground_hwnd','foreground_pid','foreground_tid','caller_integrity','foreground_integrity','session_id','input_name','thread_name','station_name')){
        $a=Get-ReliabilityField $before $name;$b=Get-ReliabilityField $after $name
        if($null -eq $a -or $null -eq $b -or $a -cne $b){return $false}
    }
    foreach($context in @($before,$after)){
        foreach($name in @('caller_integrity_known','foreground_integrity_known','desktop_hook_access_verified','input_query_succeeded','thread_query_succeeded','station_query_succeeded','session_query_succeeded')){if(-not (Test-ReliabilityTrue (Get-ReliabilityField $context $name))){return $false}}
        foreach($name in @('foreground_hwnd','foreground_pid','foreground_tid','caller_integrity','foreground_integrity','session_level','session_id','session_state','session_flags','start_qpc','finish_qpc')){Assert-IsolationInt64 (Get-ReliabilityField $context $name) "environment $name"}
        if($context.input_name -cne 'Default' -or $context.thread_name -cne 'Default' -or $context.station_name -cne 'WinSta0' -or $context.session_level -ne 1 -or $context.session_state -ne 0 -or $context.session_flags -ne 1 -or $context.foreground_hwnd -le 0 -or $context.foreground_pid -le 0 -or $context.foreground_tid -le 0 -or $context.caller_integrity -lt $context.foreground_integrity){return $false}
        if($context.start_qpc -le 0 -or $context.finish_qpc -lt $context.start_qpc){return $false}
    }
    Assert-IsolationInt64 $Value.qpc_frequency 'environment frequency'
    if($Value.qpc_frequency -le 0 -or $after.start_qpc -lt $before.finish_qpc){return $false}
    if($Value.schema -ceq 'r1c4b-readonly-input-environment/v2'){
        foreach($context in @($before,$after)){
            foreach($name in @('context_complete','foreground_identity_query_succeeded')){if(-not (Test-ReliabilityTrue (Get-ReliabilityField $context $name))){return $false}}
        }
        $gui=Get-ReliabilityField $Value 'foreground_gui'
        if(-not (Test-ReliabilityTrue (Get-ReliabilityField $gui 'foreground_tuple_stable'))){return $false}
        $previous=$before.finish_qpc
        foreach($tuple in @((Get-ReliabilityField $gui 'foreground_before'),(Get-ReliabilityField $gui 'foreground_after'))){
            foreach($name in @('hwnd','pid','tid','start_qpc','finish_qpc','error')){Assert-IsolationInt64 (Get-ReliabilityField $tuple $name) "environment GUI tuple $name"}
            if(-not (Test-ReliabilityTrue (Get-ReliabilityField $tuple 'identity_query_attempted')) -or -not (Test-ReliabilityTrue (Get-ReliabilityField $tuple 'identity_query_succeeded')) -or $tuple.error -ne 0 -or $tuple.start_qpc -lt $previous -or $tuple.finish_qpc -lt $tuple.start_qpc -or $tuple.hwnd -ne $before.foreground_hwnd -or $tuple.pid -ne $before.foreground_pid -or $tuple.tid -ne $before.foreground_tid){return $false}
            $previous=$tuple.finish_qpc
        }
        if($after.start_qpc -lt $previous){return $false}
        if(Test-ReliabilityTrue (Get-ReliabilityField $gui 'query_attempted')){
            foreach($name in @('query_start_qpc','query_finish_qpc')){Assert-IsolationInt64 (Get-ReliabilityField $gui $name) "environment GUI $name"}
            if($gui.query_start_qpc -lt $gui.foreground_before.finish_qpc -or $gui.query_finish_qpc -lt $gui.query_start_qpc -or $gui.foreground_after.start_qpc -lt $gui.query_finish_qpc){return $false}
        }
    }
    return $true
}
function Get-ReliabilityButtonState($Value){
    # A current reliable LEFT observation is diagnostic only, never READY or
    # fixture acceptance. Invalid context/zero/missing proof remains UNKNOWN.
    try{
        if(-not (Test-ReliabilityEnvironmentContext $Value)){return 'UNKNOWN'}
        $key=Get-ReliabilityField (Get-ReliabilityField $Value 'keys') 'left'
        if(-not (Test-ReliabilityTrue (Get-ReliabilityField $key 'query_attempted'))){return 'UNKNOWN'}
        foreach($clock in @('start_qpc','finish_qpc')){Assert-IsolationInt64 (Get-ReliabilityField $key $clock) "left observation $clock"}
        if($key.start_qpc -lt $Value.before.finish_qpc -or $key.finish_qpc -lt $key.start_qpc -or $key.finish_qpc -gt $Value.after.start_qpc){return 'UNKNOWN'}
        if($key.state -ceq 'UP' -and (Test-ReliabilityFalse $key.high_bit_down)){return 'OBSERVED_UP'}
        if($key.state -ceq 'DOWN' -and (Test-ReliabilityTrue $key.high_bit_down)){return 'OBSERVED_DOWN'}
    }catch{return 'UNKNOWN'}
    return 'UNKNOWN'
}
function Get-ReliabilityButtonsObservation($Value){
    if(-not (Test-ReliabilityEnvironmentContext $Value)){return 'UNKNOWN'}
    $before=$Value.before;$after=$Value.after
    $keys=Get-ReliabilityField $Value 'keys'
    $names=@('left','right','middle','x1','x2','ctrl','shift','alt','lwin','rwin','escape')
    if($null -eq $keys -or @($keys.PSObject.Properties).Count -ne $names.Count){return 'UNKNOWN'}
    $down=$false;$previous=$before.finish_qpc
    foreach($name in $names){
        $key=Get-ReliabilityField $keys $name
        if(-not (Test-ReliabilityTrue (Get-ReliabilityField $key 'query_attempted'))){return 'UNKNOWN'}
        foreach($clock in @('start_qpc','finish_qpc')){Assert-IsolationInt64 (Get-ReliabilityField $key $clock) "environment key $clock"}
        if($key.start_qpc -lt $previous -or $key.finish_qpc -lt $key.start_qpc -or $key.finish_qpc -gt $after.start_qpc){return 'UNKNOWN'}
        if($Value.schema -ceq 'r1c4b-readonly-input-environment/v2' -and $key.finish_qpc -gt $Value.foreground_gui.foreground_before.start_qpc){return 'UNKNOWN'}
        $previous=$key.finish_qpc
        if($key.state -ceq 'DOWN' -and (Test-ReliabilityTrue $key.high_bit_down)){$down=$true}
        elseif($key.state -cne 'UP' -or -not (Test-ReliabilityFalse $key.high_bit_down)){return 'UNKNOWN'}
    }
    if($down){return 'OBSERVED_DOWN'}
    return 'OBSERVED_UP'
}
function Test-ReliabilityEnvironment($Value){
    # A v1 READY never authorizes a modern startup. Recompute GUI proof from
    # the actual foreground thread and ordered raw observations, not aliases.
    if((Get-ReliabilityField $Value 'schema') -cne 'r1c4b-readonly-input-environment/v2' -or (Get-ReliabilityField $Value 'startup_contract') -cne 'foreground_gui_readiness_v1'){return $false}
    if((Get-ReliabilityButtonsObservation $Value) -cne 'OBSERVED_UP' -or (Get-ReliabilityField $Value 'result') -cne 'READY' -or (Get-ReliabilityField $Value 'startup_readiness') -cne 'READY' -or -not (Test-ReliabilityTrue (Get-ReliabilityField $Value 'startup_ready')) -or -not (Test-ReliabilityTrue (Get-ReliabilityField $Value 'buttons_observed_up')) -or (Get-ReliabilityField $Value 'buttons_observation') -cne 'OBSERVED_UP' -or -not (Test-ReliabilityTrue (Get-ReliabilityField $Value 'all_required_inputs_up')) -or -not (Test-ReliabilityFalse (Get-ReliabilityField $Value 'mouse_buttons_swapped'))){return $false}
    $gui=Get-ReliabilityField $Value 'foreground_gui'
    if(-not (Test-ReliabilityTrue (Get-ReliabilityField $gui 'query_attempted')) -or -not (Test-ReliabilityTrue (Get-ReliabilityField $gui 'query_succeeded')) -or -not (Test-ReliabilityTrue (Get-ReliabilityField $gui 'foreground_tuple_stable'))){return $false}
    foreach($name in @('query_tid','query_start_qpc','query_finish_qpc','error','capture_hwnd','menu_owner_hwnd','move_size_hwnd','gui_flags')){Assert-IsolationInt64 (Get-ReliabilityField $gui $name) "foreground GUI $name"}
    if($gui.error -ne 0 -or $gui.capture_hwnd -ne 0 -or $gui.menu_owner_hwnd -ne 0 -or $gui.move_size_hwnd -ne 0 -or $gui.gui_flags -lt 0 -or $gui.gui_flags -gt 4294967295 -or ($gui.gui_flags -band 30) -ne 0){return $false}
    $first=$gui.foreground_before;$last=$gui.foreground_after
    foreach($tuple in @($first,$last)){
        foreach($name in @('hwnd','pid','tid','start_qpc','finish_qpc')){Assert-IsolationInt64 (Get-ReliabilityField $tuple $name) "GUI tuple $name"}
        if($tuple.hwnd -le 0 -or $tuple.pid -le 0 -or $tuple.tid -le 0 -or $tuple.start_qpc -le 0 -or $tuple.finish_qpc -lt $tuple.start_qpc){return $false}
        if($tuple.hwnd -ne $Value.before.foreground_hwnd -or $tuple.pid -ne $Value.before.foreground_pid -or $tuple.tid -ne $Value.before.foreground_tid){return $false}
    }
    if($gui.query_tid -ne $first.tid -or $gui.query_start_qpc -lt $first.finish_qpc -or $gui.query_finish_qpc -lt $gui.query_start_qpc -or $last.start_qpc -lt $gui.query_finish_qpc -or $Value.after.start_qpc -lt $last.finish_qpc){return $false}
    foreach($key in $Value.keys.PSObject.Properties){if($first.start_qpc -lt $key.Value.finish_qpc){return $false}}
    $predicates=Get-ReliabilityField $Value 'readiness_predicates'
    $names=@('QPC_VALID','BEFORE_CONTEXT_VALID','AFTER_CONTEXT_VALID','OBSERVATION_CONTEXT_STABLE','BUTTONS_OBSERVED_UP','INPUT_MAPPING_SUPPORTED','FOREGROUND_GUI_QUERY','FOREGROUND_GUI_TUPLE_STABLE','CAPTURE_CLEAR','MENU_CLEAR','MOVE_SIZE_CLEAR','DISALLOWED_GUI_FLAGS_CLEAR')
    if($null -eq $predicates -or @($predicates.PSObject.Properties).Count -ne $names.Count){return $false}
    foreach($name in $names){if((Get-ReliabilityField $predicates $name) -cne 'PASS'){return $false}}
    if((Get-ReliabilityField $Value 'first_failed_predicate') -cne 'NONE' -or @(Get-ReliabilityField $Value 'failed_predicates').Count -ne 0){return $false}
    return $true
}
function Get-ReliabilityExitCode([string]$Operation,[string]$Mode,$ProbeExit,$Verdict,[bool]$EnvironmentReady,$ImplementationUnchanged){
    $canonical=if($Mode -ceq 'controlled-abort'){'controlled_abort'}else{'normal'}
    if(-not ($ProbeExit -is [int] -or $ProbeExit -is [long]) -or -not $EnvironmentReady -or -not (Test-ReliabilityTrue $ImplementationUnchanged) -or -not (Test-ReliabilityTrue (Get-ReliabilityField $Verdict 'ContractVerified')) -or (Get-ReliabilityField $Verdict 'Result') -cne 'PASS' -or (Get-ReliabilityField $Verdict 'Operation') -cne $Operation -or (Get-ReliabilityField $Verdict 'TestMode') -cne $canonical){return 2}
    if($Mode -ceq 'controlled-abort'){
        if($ProbeExit -eq 2 -and $Verdict.FixtureResult -ceq 'PASS_EXPECTED_ABORT' -and $Verdict.GestureResult -ceq 'BLOCKED_BY_TEST_FAULT' -and $Verdict.CleanupResult -ceq 'PASS' -and $Verdict.TakeoverAcceptance -ceq 'NOT_RUN'){return 0}
    }elseif($ProbeExit -eq 0 -and $Verdict.FixtureResult -ceq 'PASS' -and $Verdict.GestureResult -ceq 'PASS' -and $Verdict.TakeoverAcceptance -ceq 'PASS'){return 0}
    return 2
}
function Write-ReliabilityNewJson([string]$Path,$Value){
    $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($Value|ConvertTo-Json -Depth 32)+[Environment]::NewLine)
    $stream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    try{$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
}
function Set-ReliabilityFirstFailure($Metadata,[string]$Stage,[string]$Reason,$References=@()){
    if($Metadata.FirstFailureStage -ceq 'NONE'){$Metadata.FirstFailureStage=$Stage;$Metadata.FirstFailureReason=$Reason;$Metadata.FirstFailureRecordSequences=@($References)}
}
function Invoke-ReliabilityPostObservation($Metadata,[string]$Environment,[string]$Post){
    & $Environment --check-input-state --evidence-log $Post
    $Metadata.EnvironmentPostExitCode=$LASTEXITCODE
    $data=Get-Content -LiteralPath $Post -Raw -Encoding UTF8|ConvertFrom-Json
    $Metadata.EnvironmentPostData=$data
    $Metadata.EnvironmentPostSHA256=(Get-FileHash -LiteralPath $Post -Algorithm SHA256).Hash
    $Metadata.FinalButtonState=Get-ReliabilityButtonsObservation $data
    $Metadata.FinalButtonObservation='READ_ONLY_POST'
    $Metadata.EnvironmentPostReady=$Metadata.EnvironmentPostExitCode -eq 0 -and (Test-ReliabilityEnvironment $data)
    if($Metadata.FinalButtonState -ceq 'UNKNOWN'){throw 'Post input observation is not reliable'}
}
function Get-ReliabilityAfterIdentity($Metadata,[string]$Repo,[string]$Configuration){
    # HEAD, worktree and file hashes are independent. Missing one must not
    # prevent collection of the other two, and UNKNOWN never means unchanged.
    try{
        $Metadata.AfterHEAD=git rev-parse HEAD
        if($LASTEXITCODE -ne 0 -or $Metadata.AfterHEAD -cnotmatch '^[a-f0-9]{40}$'){throw 'AfterHEAD unavailable'}
        $Metadata.AfterHEADStatus='COLLECTED'
    }catch{$Metadata.AfterHEADStatus='ERROR';$Metadata.AfterHEADError=$_.Exception.Message}
    try{
        $Metadata.AfterWorktree=@(git status --porcelain)
        if($LASTEXITCODE -ne 0){throw 'After worktree unavailable'}
        $Metadata.AfterWorktreeStatus='COLLECTED'
    }catch{$Metadata.AfterWorktreeStatus='ERROR';$Metadata.AfterWorktreeError=$_.Exception.Message}
    try{
        $hashErrors=[Collections.Generic.List[string]]::new()
        $Metadata.AfterIdentity=Get-ReliabilitySnapshot $Repo $Configuration $hashErrors
        if($hashErrors.Count){throw ($hashErrors -join '; ')}
        $Metadata.AfterHashStatus='COLLECTED'
    }catch{$Metadata.AfterHashStatus='ERROR';$Metadata.AfterHashError=$_.Exception.Message}
    if($Metadata.AfterHEADStatus -ceq 'COLLECTED' -and $Metadata.AfterWorktreeStatus -ceq 'COLLECTED' -and $Metadata.AfterHashStatus -ceq 'COLLECTED'){
        $Metadata.AfterIdentityStatus='COLLECTED'
        $Metadata.ImplementationUnchanged=$Metadata.AfterHEAD -ceq $Metadata.ExecutedHEAD -and $Metadata.AfterWorktree.Count -eq 0 -and (Test-ReliabilitySameSnapshot $Metadata.BeforeIdentity $Metadata.AfterIdentity)
    }else{
        $Metadata.AfterIdentityStatus='ERROR';$Metadata.ImplementationUnchanged='UNKNOWN'
        $Metadata.AfterIdentityError=@($Metadata.AfterHEADError,$Metadata.AfterWorktreeError,$Metadata.AfterHashError|Where-Object {$_}) -join '; '
    }
}
function Get-ReliabilityNativeSummary($Metadata,[string]$Path){
    # These are explicitly raw references, not a trusted BLOCKED classification.
    # Preserve native reason even when the independent validator cannot finish.
    try{$Metadata.LogSHA256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash;$Metadata.LogHashStatus='COLLECTED'}catch{$Metadata.LogHashStatus='ERROR';$Metadata.LogHashError=$_.Exception.Message}
    $rows=@();$parseErrors=@()
    foreach($line in @(Get-Content -LiteralPath $Path -Encoding UTF8)){
        try{$rows+=@($line|ConvertFrom-Json)}catch{$parseErrors+=@($_.Exception.Message)}
    }
    $blocked=@($rows|Where-Object { (Get-ReliabilityField $_ 'type') -ceq 'blocked' })
    $Metadata.NativeBlockReasons=@($blocked|ForEach-Object {Get-ReliabilityField $_ 'reason'})
    $Metadata.NativeBlockRecordSequences=@($blocked|ForEach-Object {Get-ReliabilityField $_ 'sequence'})
    if($Metadata.NativeBlockReasons.Count){$Metadata.NativeBlockReason=$Metadata.NativeBlockReasons[0]}
    $Metadata.RawEvidenceReferences=[pscustomobject]@{Path=$Path;Startup=@($rows|Where-Object type -ceq startup|ForEach-Object sequence);Shutdown=@($rows|Where-Object type -ceq shutdown|ForEach-Object sequence);Blocked=$Metadata.NativeBlockRecordSequences;ActivationFence=@($rows|Where-Object type -ceq activation_fence|ForEach-Object sequence)}
    $Metadata.NativeSummaryStatus='RAW_RECORDED'
    if($parseErrors.Count){$Metadata.NativeSummaryStatus='RAW_RECORDED_WITH_ERRORS';$Metadata.NativeSummaryError=$parseErrors -join '; '}
}
function Complete-ReliabilityRun($Metadata,[string]$Repo,[string]$Configuration,[string]$Environment,[string]$Post,[string]$Path){
    # Called only after synchronous native invocation has returned or failed.
    # Exactly one post attempt, then identity, then independent validation.
    if($Metadata.PostObservationStatus -ceq 'NOT_RUN'){
        $Metadata.PostObservationStatus='ATTEMPTED'
        try{Invoke-ReliabilityPostObservation $Metadata $Environment $Post;$Metadata.PostObservationStatus='COLLECTED'}catch{$Metadata.PostObservationStatus='ERROR';$Metadata.PostObservationError=$_.Exception.Message}
    }
    try{Get-ReliabilityAfterIdentity $Metadata $Repo $Configuration}catch{$Metadata.AfterIdentityStatus='ERROR';$Metadata.AfterIdentityError=$_.Exception.Message;$Metadata.ImplementationUnchanged='UNKNOWN'}
    try{Get-ReliabilityNativeSummary $Metadata $Path}catch{$Metadata.NativeSummaryStatus='ERROR';$Metadata.NativeSummaryError=$_.Exception.Message}
    try{
        $Metadata.Result=Test-FixFInputReliabilityEvidence -Path $Path
        $Metadata.EvidenceValidationResult='COMPLETED'
        $Metadata.EvidenceIntegrity='VALID';$Metadata.ExecutionResult=$Metadata.Result.Result
        foreach($name in @('EvidenceIntegrity','PrefixValidation','ExecutionResult','BlockPhase','FixtureResult','GestureResult','CleanupResult','TakeoverAcceptance','InputAttempted','TestDownPending','ContractVerified')){
            $value=Get-ReliabilityField $Metadata.Result $name;if($null -ne $value){$Metadata[$name]=$value}
        }
        if((Get-ReliabilityField $Metadata.Result 'NativeBlockReason')){$Metadata.NativeBlockReason=$Metadata.Result.NativeBlockReason}
        if((Get-ReliabilityField $Metadata.Result 'FirstFailurePhase')){Set-ReliabilityFirstFailure $Metadata $Metadata.Result.FirstFailurePhase $Metadata.NativeBlockReason (Get-ReliabilityField $Metadata.Result 'FirstFailureRecordSequences')}
    }catch{$Metadata.EvidenceValidationResult='INVALID_EVIDENCE';$Metadata.EvidenceIntegrity='INVALID_EVIDENCE';$Metadata.ValidatorError=$_.Exception.Message}
    if($Metadata.NativeBlockReason -cne 'NOT_AVAILABLE'){Set-ReliabilityFirstFailure $Metadata 'NATIVE_RECORDED_BLOCK' $Metadata.NativeBlockReason $Metadata.NativeBlockRecordSequences}
    foreach($step in @(@('POST_OBSERVATION',$Metadata.PostObservationError),@('AFTER_IDENTITY',$Metadata.AfterIdentityError),@('EVIDENCE_HASH',$Metadata.LogHashError),@('EVIDENCE_VALIDATION',$Metadata.ValidatorError))){if($step[1]){Set-ReliabilityFirstFailure $Metadata $step[0] $step[1]}}
    if($Metadata.PostObservationStatus -ceq 'COLLECTED' -and -not $Metadata.EnvironmentPostReady){Set-ReliabilityFirstFailure $Metadata 'READ_ONLY_POST' ([string](Get-ReliabilityField $Metadata.EnvironmentPostData 'first_failed_predicate'))}
    if(-not (Test-ReliabilityTrue $Metadata.ImplementationUnchanged)){Set-ReliabilityFirstFailure $Metadata 'IMPLEMENTATION_IDENTITY' 'ImplementationUnchanged is false or UNKNOWN'}
    $Metadata.RunnerExitCode=Get-ReliabilityExitCode $Metadata.Operation $(if($Metadata.TestMode -ceq 'controlled_abort'){'controlled-abort'}else{'normal'}) $Metadata.ProbeExitCode $Metadata.Result $Metadata.EnvironmentPostReady $Metadata.ImplementationUnchanged
    if($Metadata.NativeError -or $Metadata.PostObservationStatus -cne 'COLLECTED' -or $Metadata.AfterIdentityStatus -cne 'COLLECTED' -or $Metadata.LogHashStatus -cne 'COLLECTED' -or $Metadata.NativeSummaryStatus -cne 'RAW_RECORDED' -or $Metadata.EvidenceValidationResult -cne 'COMPLETED'){$Metadata.RunnerExitCode=2}
    if($Metadata.RunnerExitCode -ne 0){$Metadata.Reason='Independent fixture/native/environment/identity validation failed; STOP';Set-ReliabilityFirstFailure $Metadata 'FIXTURE_VALIDATION' $Metadata.Reason}
}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Join-Path $repo 'uat/r1c4b-fixg'
$directory=Join-Path $root ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-'+$Configuration+'-'+$Operation+'-'+$Mode+'-'+$RunId)
$path=Join-Path $directory 'probe.jsonl';$metadataPath=Join-Path $directory 'run.metadata.json'
$pre=Join-Path $directory 'environment-pre.json';$post=Join-Path $directory 'environment-post.json'
$bin=Join-Path $repo ("out/r1c4b-live-magnet-"+$Configuration.ToLowerInvariant()+"/src/platform/windows/$Configuration")
$probe=Join-Path $bin 'panebind-magnet-takeover-probe.exe';$environment=Join-Path $bin 'panebind-test-input-environment.exe'
$metadata=[ordered]@{Schema='r1c4b-fixg-input-reliability-run/v2';Configuration=$Configuration;Operation=$Operation;TestMode=$(if($Mode -ceq 'controlled-abort'){'controlled_abort'}else{'normal'});RunId=$RunId;ExpectedHEAD=$ExpectedHEAD;EvidencePath=$path;ExecutedHEAD='NOT_AVAILABLE';AfterHEAD='NOT_AVAILABLE';BeforeIdentity=$null;AfterIdentity=$null;AfterWorktree=$null;ImplementationUnchanged='UNKNOWN';AfterHEADStatus='NOT_RUN';AfterHEADError=$null;AfterWorktreeStatus='NOT_RUN';AfterWorktreeError=$null;AfterHashStatus='NOT_RUN';AfterHashError=$null;AfterIdentityStatus='NOT_RUN';AfterIdentityError=$null;ProbeInvoked=$false;ProbeExitCode='NOT_AVAILABLE';NativeError=$null;NativeProcessState='NOT_RUN';NativeBlockReason='NOT_AVAILABLE';NativeBlockReasons=@();NativeBlockRecordSequences=@();NativeSummaryStatus='NOT_RUN';NativeSummaryError=$null;RawEvidenceReferences=$null;Result=$null;EvidenceIntegrity='NOT_VERIFIED';EvidenceValidationResult='NOT_RUN';ValidatorError=$null;PrefixValidation='NOT_RUN';ExecutionResult='NOT_RUN';BlockPhase='NONE';FixtureResult='NOT_RUN';GestureResult='NOT_RUN';CleanupResult='NOT_RUN';TakeoverAcceptance='NOT_RUN';InputAttempted='UNKNOWN';TestDownPending='UNKNOWN';ContractVerified=$false;EnvironmentPrePath=$pre;EnvironmentPostPath=$post;EnvironmentPreSHA256=$null;EnvironmentPostSHA256=$null;EnvironmentPreExitCode=$null;EnvironmentPostExitCode=$null;EnvironmentPreReady=$false;EnvironmentPostReady=$false;EnvironmentPreData=$null;EnvironmentPostData=$null;PostObservationStatus='NOT_RUN';PostObservationError=$null;FinalButtonState='UNKNOWN';FinalButtonObservation='NONE';FirstFailureStage='NONE';FirstFailureReason=$null;FirstFailureRecordSequences=@();Reason=$null;RunnerExitCode=2;LogSHA256=$null;LogHashStatus='NOT_RUN';LogHashError=$null;MetadataWriteStatus='NOT_RUN';MetadataWriteError=$null}
$createdDirectory=$false
Push-Location $repo
try{
    $head=git rev-parse HEAD;if($LASTEXITCODE -ne 0){throw 'HEAD unavailable'}
    $status=@(git status --porcelain);if($LASTEXITCODE -ne 0 -or $status.Count){throw 'Require a clean checkpoint before environment/probe execution'}
    if($ExpectedHEAD -and $head -cne $ExpectedHEAD){throw 'Expected aggregate checkpoint changed before environment/probe execution'}
    git check-ignore -- $path|Out-Null;if($LASTEXITCODE -ne 0){throw 'Fix G evidence must remain ignored'}
    $metadata.ExecutedHEAD=$head;$metadata.BeforeIdentity=Get-ReliabilitySnapshot $repo $Configuration
    New-Item -ItemType Directory -Path $root -Force|Out-Null
    New-Item -ItemType Directory -Path $directory|Out-Null
    $createdDirectory=$true
    & $environment --check-input-state --evidence-log $pre
    $preCode=$LASTEXITCODE;$metadata.EnvironmentPreExitCode=$preCode;$preData=Get-Content -LiteralPath $pre -Raw -Encoding UTF8|ConvertFrom-Json
    $metadata.EnvironmentPreData=$preData;$metadata.FinalButtonState=Get-ReliabilityButtonsObservation $preData;$metadata.FinalButtonObservation='READ_ONLY_PRE'
    $metadata.EnvironmentPreSHA256=(Get-FileHash -LiteralPath $pre -Algorithm SHA256).Hash
    $metadata.EnvironmentPreReady=$preCode -eq 0 -and (Test-ReliabilityEnvironment $preData)
    if(-not $metadata.EnvironmentPreReady){Set-ReliabilityFirstFailure $metadata 'READ_ONLY_PREFLIGHT' ([string](Get-ReliabilityField $preData 'first_failed_predicate'));throw 'Current input state is not reliably READY; no test input authorized'}
    $now=Get-ReliabilitySnapshot $repo $Configuration
    if(-not (Test-ReliabilitySameSnapshot $metadata.BeforeIdentity $now)){throw 'Implementation changed before native input'}
    $freshHead=git rev-parse HEAD;$headCode=$LASTEXITCODE;$freshStatus=@(git status --porcelain)
    if($headCode -ne 0 -or $LASTEXITCODE -ne 0 -or $freshHead -cne $head -or $freshStatus.Count){throw 'Checkpoint changed before native input'}
    $gesture=if($Operation -ceq 'Move'){'move'}else{'bottom-resize'}
    $metadata.ProbeInvoked=$true
    $metadata.FinalButtonState='UNKNOWN';$metadata.FinalButtonObservation='NATIVE_WITHOUT_POST_PROOF'
    try{
        & $probe --run-owned-input-reliability-test --gesture $gesture --mode $Mode --evidence-log $path
        $metadata.ProbeExitCode=$LASTEXITCODE
        $metadata.NativeProcessState='EXITED'
    }catch{$metadata.NativeError=$_.Exception.Message;$metadata.NativeProcessState='INVOCATION_ERROR';Set-ReliabilityFirstFailure $metadata 'NATIVE_INVOCATION' $metadata.NativeError}
    Complete-ReliabilityRun $metadata $repo $Configuration $environment $post $path
}catch{
    $metadata.Reason=$_.Exception.Message;$metadata.RunnerExitCode=2;Set-ReliabilityFirstFailure $metadata 'RUNNER' $metadata.Reason
    if($createdDirectory -and -not $metadata.ProbeInvoked){
        # No native run means no post observation. Still collect the current
        # checkpoint independently so a preflight stop has explicit identity.
        try{Get-ReliabilityAfterIdentity $metadata $repo $Configuration}catch{$metadata.AfterIdentityStatus='ERROR';$metadata.AfterIdentityError=$_.Exception.Message;$metadata.ImplementationUnchanged='UNKNOWN'}
    }
}finally{
    try{
        if($createdDirectory){
            try{$metadata.MetadataWriteStatus='WRITTEN';Write-ReliabilityNewJson $metadataPath $metadata}catch{$metadata.MetadataWriteStatus='ERROR';$metadata.MetadataWriteError=$_.Exception.Message;$metadata.RunnerExitCode=2;Write-Error ('Fix G metadata write failed: '+$metadata.MetadataWriteError) -ErrorAction Continue}
        }
        $metadata|ConvertTo-Json -Depth 32|Write-Host
    }finally{Pop-Location}
}
exit $metadata.RunnerExitCode
