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
function Get-ReliabilitySnapshot([string]$Repo,[string]$Configuration){
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
    foreach($file in $files){$result[$file]=(Get-FileHash -LiteralPath (Join-Path $Repo $file) -Algorithm SHA256).Hash}
    return $result
}
function Test-ReliabilitySameSnapshot($Before,$After){
    $a=@($Before.Keys);$b=@($After.Keys)
    if($a.Count -ne $b.Count){return $false}
    foreach($name in $a){if(-not $After.Contains($name) -or $Before[$name] -cne $After[$name]){return $false}}
    return $true
}
function Test-ReliabilityEnvironmentContext($Value){
    if((Get-ReliabilityField $Value 'schema') -cne 'r1c4b-readonly-input-environment/v1'){return $false}
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
function Test-ReliabilityEnvironment($Value){
    if(-not (Test-ReliabilityEnvironmentContext $Value) -or (Get-ReliabilityField $Value 'result') -cne 'READY' -or -not (Test-ReliabilityTrue (Get-ReliabilityField $Value 'all_required_inputs_up')) -or -not (Test-ReliabilityFalse (Get-ReliabilityField $Value 'mouse_buttons_swapped'))){return $false}
    $before=$Value.before;$after=$Value.after
    $keys=Get-ReliabilityField $Value 'keys'
    $names=@('left','right','middle','x1','x2','ctrl','shift','alt','lwin','rwin','escape')
    if($null -eq $keys -or @($keys.PSObject.Properties).Count -ne $names.Count){return $false}
    foreach($name in $names){
        $key=Get-ReliabilityField $keys $name
        if((Get-ReliabilityField $key 'state') -cne 'UP' -or -not (Test-ReliabilityFalse (Get-ReliabilityField $key 'high_bit_down')) -or -not (Test-ReliabilityTrue (Get-ReliabilityField $key 'query_attempted'))){return $false}
        foreach($clock in @('start_qpc','finish_qpc')){Assert-IsolationInt64 (Get-ReliabilityField $key $clock) "environment key $clock"}
        if($key.start_qpc -lt $before.finish_qpc -or $key.finish_qpc -lt $key.start_qpc -or $key.finish_qpc -gt $after.start_qpc){return $false}
    }
    return $true
}
function Get-ReliabilityExitCode([string]$Operation,[string]$Mode,$ProbeExit,$Verdict,[bool]$EnvironmentReady,[bool]$ImplementationUnchanged){
    $canonical=if($Mode -ceq 'controlled-abort'){'controlled_abort'}else{'normal'}
    if(-not ($ProbeExit -is [int] -or $ProbeExit -is [long]) -or -not $EnvironmentReady -or -not $ImplementationUnchanged -or -not (Test-ReliabilityTrue (Get-ReliabilityField $Verdict 'ContractVerified')) -or (Get-ReliabilityField $Verdict 'Result') -cne 'PASS' -or (Get-ReliabilityField $Verdict 'Operation') -cne $Operation -or (Get-ReliabilityField $Verdict 'TestMode') -cne $canonical){return 2}
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
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Join-Path $repo 'uat/r1c4b-fixf'
$directory=Join-Path $root ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-'+$Configuration+'-'+$Operation+'-'+$Mode+'-'+$RunId)
$path=Join-Path $directory 'probe.jsonl';$metadataPath=Join-Path $directory 'run.metadata.json'
$pre=Join-Path $directory 'environment-pre.json';$post=Join-Path $directory 'environment-post.json'
$bin=Join-Path $repo ("out/r1c4b-live-magnet-"+$Configuration.ToLowerInvariant()+"/src/platform/windows/$Configuration")
$probe=Join-Path $bin 'panebind-magnet-takeover-probe.exe';$environment=Join-Path $bin 'panebind-test-input-environment.exe'
$metadata=[ordered]@{Schema='r1c4b-input-reliability-run/v1';Configuration=$Configuration;Operation=$Operation;TestMode=$(if($Mode -ceq 'controlled-abort'){'controlled_abort'}else{'normal'});RunId=$RunId;ExpectedHEAD=$ExpectedHEAD;EvidencePath=$path;ExecutedHEAD=$null;AfterHEAD=$null;BeforeIdentity=$null;AfterIdentity=$null;ImplementationUnchanged=$false;ProbeInvoked=$false;ProbeExitCode=$null;NativeError=$null;Result=$null;EnvironmentPrePath=$pre;EnvironmentPostPath=$post;EnvironmentPreSHA256=$null;EnvironmentPostSHA256=$null;EnvironmentPreExitCode=$null;EnvironmentPostExitCode=$null;EnvironmentPreReady=$false;EnvironmentPostReady=$false;EnvironmentPreData=$null;EnvironmentPostData=$null;FinalButtonState='UNKNOWN';FinalButtonObservation='NONE';Reason=$null;RunnerExitCode=2;LogSHA256=$null}
$createdDirectory=$false
Push-Location $repo
try{
    $head=git rev-parse HEAD;if($LASTEXITCODE -ne 0){throw 'HEAD unavailable'}
    $status=@(git status --porcelain);if($LASTEXITCODE -ne 0 -or $status.Count){throw 'Require a clean checkpoint before environment/probe execution'}
    if($ExpectedHEAD -and $head -cne $ExpectedHEAD){throw 'Expected aggregate checkpoint changed before environment/probe execution'}
    git check-ignore -- $path|Out-Null;if($LASTEXITCODE -ne 0){throw 'Fix F evidence must remain ignored'}
    $metadata.ExecutedHEAD=$head;$metadata.BeforeIdentity=Get-ReliabilitySnapshot $repo $Configuration
    New-Item -ItemType Directory -Path $root -Force|Out-Null
    New-Item -ItemType Directory -Path $directory|Out-Null
    $createdDirectory=$true
    & $environment --check-input-state --evidence-log $pre
    $preCode=$LASTEXITCODE;$metadata.EnvironmentPreExitCode=$preCode;$preData=Get-Content -LiteralPath $pre -Raw -Encoding UTF8|ConvertFrom-Json
    $metadata.EnvironmentPreData=$preData;$metadata.FinalButtonState=Get-ReliabilityButtonState $preData;$metadata.FinalButtonObservation='READ_ONLY_PRE'
    $metadata.EnvironmentPreSHA256=(Get-FileHash -LiteralPath $pre -Algorithm SHA256).Hash
    $metadata.EnvironmentPreReady=$preCode -eq 0 -and (Test-ReliabilityEnvironment $preData)
    if(-not $metadata.EnvironmentPreReady){throw 'Current input state is not reliably READY; no test input authorized'}
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
    }catch{$metadata.NativeError=$_.Exception.Message}
    # A post-read is mandatory even after an unexpected native failure. It
    # only observes; never injects a compensating UP or retries a run.
    & $environment --check-input-state --evidence-log $post
    $postCode=$LASTEXITCODE;$metadata.EnvironmentPostExitCode=$postCode;$postData=Get-Content -LiteralPath $post -Raw -Encoding UTF8|ConvertFrom-Json
    $metadata.EnvironmentPostData=$postData;$metadata.FinalButtonState=Get-ReliabilityButtonState $postData;$metadata.FinalButtonObservation='READ_ONLY_POST'
    $metadata.EnvironmentPostSHA256=(Get-FileHash -LiteralPath $post -Algorithm SHA256).Hash
    $metadata.EnvironmentPostReady=$postCode -eq 0 -and (Test-ReliabilityEnvironment $postData)
    $metadata.Result=Test-FixFInputReliabilityEvidence -Path $path
    $metadata.AfterHEAD=git rev-parse HEAD;$afterCode=$LASTEXITCODE;$afterStatus=@(git status --porcelain);$afterStatusCode=$LASTEXITCODE
    $metadata.AfterIdentity=Get-ReliabilitySnapshot $repo $Configuration
    $metadata.ImplementationUnchanged=$afterCode -eq 0 -and $afterStatusCode -eq 0 -and $afterStatus.Count -eq 0 -and $metadata.AfterHEAD -ceq $head -and (Test-ReliabilitySameSnapshot $metadata.BeforeIdentity $metadata.AfterIdentity)
    $metadata.RunnerExitCode=Get-ReliabilityExitCode $Operation $Mode $metadata.ProbeExitCode $metadata.Result $metadata.EnvironmentPostReady $metadata.ImplementationUnchanged
    if($metadata.RunnerExitCode -ne 0){$metadata.Reason='Independent fixture/native/environment/identity validation failed; STOP'}
}catch{
    $metadata.Reason=$_.Exception.Message;$metadata.RunnerExitCode=2
}finally{
    if($createdDirectory){
        if(Test-Path -LiteralPath $path -PathType Leaf){$metadata.LogSHA256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
        Write-ReliabilityNewJson $metadataPath $metadata
    }
    $metadata|ConvertTo-Json -Depth 32|Write-Host
    Pop-Location
}
exit $metadata.RunnerExitCode
