[CmdletBinding()]
param([ValidateSet('Test','ProbeStub','ChildStub','AggregateStub')][string]$Layer='Test')
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# Console-process stand-ins only: no native probe, HWND, SendInput, Git,
# original evidence, console/global settings, file writes or retry.
$powerShell=Join-Path $PSHOME 'powershell.exe'
if($Layer -ceq 'ProbeStub'){
    [pscustomobject]@{Layer='ProbeStub';SimulatedNativeExit=2;ActualNativeProbeExecuted=$false}|ConvertTo-Json -Compress
    exit 2
}
if($Layer -ceq 'ChildStub'){
    $output=@(& $powerShell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $PSCommandPath -Layer ProbeStub)
    $code=$LASTEXITCODE
    $output|Write-Output
    [pscustomobject]@{Layer='ChildStub';ProbeStubExitCode=$code;Result='BLOCKED';ActualNativeProbeExecuted=$false}|ConvertTo-Json -Compress
    if($code -ne 0){exit 2}
    exit 0
}
if($Layer -ceq 'AggregateStub'){
    $output=@(& $powerShell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $PSCommandPath -Layer ChildStub)
    $code=$LASTEXITCODE
    $output|Write-Output
    [pscustomobject]@{Layer='AggregateStub';ChildExitCode=$code;Result='STOPPED';Retry=$false;ActualNativeProbeExecuted=$false}|ConvertTo-Json -Compress
    if($code -ne 0){exit 2}
    exit 0
}
$checks=0
function Check-ExitPropagation([bool]$Ok,[string]$Reason){if(-not $Ok){throw "exit fixture: $Reason"};$script:checks++}
$codes=@{}
foreach($case in @('ProbeStub','ChildStub','AggregateStub')){
    $rows=@(& $powerShell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $PSCommandPath -Layer $case)
    $code=$LASTEXITCODE
    Check-ExitPropagation ($code -eq 2) "$case explicit -File process exit remains 2"
    $json=@($rows|ForEach-Object {$_|ConvertFrom-Json})
    Check-ExitPropagation ($json[-1].Layer -ceq $case -and -not $json[-1].ActualNativeProbeExecuted) "$case is a fake-only console layer"
    $codes[$case]=$code
    if($case -ceq 'AggregateStub'){Check-ExitPropagation ($json[-1].ChildExitCode -eq 2 -and $json[-1].Result -ceq 'STOPPED' -and -not $json[-1].Retry) 'child2 and STOPPED are retained without retry'}
}
# An explicit, known local command string transported as UTF-16LE. No user
# parameters or secret/environment data are interpolated into shell code.
$command="& '"+$powerShell.Replace("'","''")+"' -NoProfile -NonInteractive -ExecutionPolicy Bypass -File '"+$PSCommandPath.Replace("'","''")+"' -Layer AggregateStub"
$encoded=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
$outer=@(& $powerShell -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand $encoded)
$outerCode=$LASTEXITCODE
Check-ExitPropagation ($outerCode -eq 1) 'PowerShell -Command implicit nonzero normalization is 1'
$outerJson=@($outer|ForEach-Object {$_|ConvertFrom-Json})
Check-ExitPropagation ($outerJson[-1].ChildExitCode -eq 2 -and $outerJson[-1].Result -ceq 'STOPPED') 'outer1 does not change inner JSON verdict/child2'
$explicit=$command+[Environment]::NewLine+'exit $LASTEXITCODE'
$encodedExplicit=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($explicit))
$null=& $powerShell -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand $encodedExplicit
$explicitCode=$LASTEXITCODE
Check-ExitPropagation ($explicitCode -eq 2) 'explicit outer exit LASTEXITCODE preserves 2'
foreach($script in @('run-r1c4b-input-isolation.ps1','run-r1c4b-input-isolation-gates.ps1')){
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $script),[ref]$tokens,[ref]$errors)
    Check-ExitPropagation (@($errors).Count -eq 0) "historical $script parses"
    $terminal=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.ExitStatementAst] -and $n.Pipeline.Extent.Text -ceq '2'},$true))
    Check-ExitPropagation ($terminal.Count -eq 1) "historical $script explicitly returns failure 2 (not executed)"
}
[pscustomobject]@{Schema='r1c4b-exit-propagation-synthetic/v1';Checks=$checks;Result='PASS';ProbeStubProcessExit=$codes.ProbeStub;ChildRunnerStubExit=$codes.ChildStub;AggregateStubExit=$codes.AggregateStub;OuterImplicitPowerShellCommandExit=$outerCode;OuterExplicitPowerShellCommandExit=$explicitCode;ActualNativeProbeExecuted=$false;HistoricalToolWrapperCause='UNKNOWN';RepositoryMappingErrorConfirmed=$false}|ConvertTo-Json -Compress
exit 0
