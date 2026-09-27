Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# Actual pure model/AST with memory fake child; no runner program, native,
# environment executable, original UAT, GUI, input or Git is executed.
. (Join-Path $PSScriptRoot 'r1c4b-owned-stability-model.ps1')
$checks=0
function Check-OwnedGate([bool]$Value,[string]$Reason){if(-not $Value){throw ('owned stability fixture: '+$Reason)};$script:checks++}
function Reject-OwnedGate([scriptblock]$Action,[string]$Reason){$rejected=$false;try{$null=& $Action}catch{$rejected=$true};Check-OwnedGate $rejected $Reason}
function Copy-OwnedGate($Value){return $Value|ConvertTo-Json -Depth 40 -Compress|ConvertFrom-Json}
foreach($name in @('Get-ReliabilityField','Test-ReliabilityTrue','Test-ReliabilityFalse')){
    $t=$null;$e=$null;$a=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'run-r1c4b-input-reliability.ps1'),[ref]$t,[ref]$e)
    $node=@($a.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name},$true))[0]
    . ([scriptblock]::Create($node.Extent.Text))
}
$plan=@(Get-OwnedStabilitySteps);Test-OwnedStabilityPlan $plan
Check-OwnedGate ($plan.Count -eq 100 -and @($plan|Where-Object Phase -ceq smoke).Count -eq 20 -and @($plan|Where-Object Phase -ceq formal).Count -eq 80) 'exact 20+80 normal plan'
foreach($group in 1..8){$rows=@($plan|Where-Object GroupNumber -eq $group);Check-OwnedGate ($rows.Count -eq $(if($group -le 4){5}else{20}) -and $rows[0].Repetition -eq 1 -and $rows[-1].Repetition -eq $rows.Count -and @($rows|Where-Object Mode -cne normal).Count -eq 0) "fixed group $group"}
foreach($fault in @('old_mode','duplicate_id','swapped','count')){
    $bad=Copy-OwnedGate $plan
    switch($fault){old_mode {$bad[0].Mode='controlled-abort'};duplicate_id {$bad[1].RunId=$bad[0].RunId};swapped {$bad[20].Configuration='Release'};count {$bad=$bad[0..98]}}
    Reject-OwnedGate {Test-OwnedStabilityPlan $bad} $fault
}
foreach($failureAt in @(0,1,20,21,54,100)){
    foreach($classification in @('BLOCKED','FAIL','INVALID_EVIDENCE')){
        $state=[ordered]@{Attempts=@();FinalButtonState='UNKNOWN';FirstUnexpectedFailure=$null;BatchResult='NOT_RUN';LoopResult='NOT_RUN'}
        $calls=[Collections.Generic.List[int]]::new();$progress=[Collections.Generic.List[string]]::new()
        Invoke-OwnedStabilityProgression $plan $state {
            param($attempt)
            $calls.Add($attempt.Number);$attempt.ChildInvoked=$true;$attempt.PreflightAttempted=$true;$attempt.ProbeInvoked=$true;$attempt.TargetGestureEntered=$true
            if($attempt.Number -eq $failureAt){$attempt.Result=$classification;$attempt.FinalButtonState='UNKNOWN'}else{$attempt.Result='PASS';$attempt.FinalButtonState='OBSERVED_UP'}
        } {param($type,$attempt) $progress.Add($type+':'+$attempt.Number)}
        $expected=if($failureAt){$failureAt}else{100}
        Check-OwnedGate ($calls.Count -eq $expected -and $state.Attempts.Count -eq $expected -and $progress.Count -eq 2*$expected) "first-failure $classification at $failureAt preserves exact attempts"
        Check-OwnedGate (@($calls|Where-Object {$_ -gt $expected}).Count -eq 0) 'no retry or remaining child'
        if($failureAt -le 20 -and $failureAt){Check-OwnedGate (@($state.Attempts|Where-Object Phase -ceq formal).Count -eq 0) 'any smoke failure starts zero formal'}
        if($failureAt){Check-OwnedGate ($state.FinalButtonState -ceq 'UNKNOWN' -and $state.FirstUnexpectedFailure.Number -eq $failureAt) 'current failure never inherits prior UP'}else{Check-OwnedGate ($state.LoopResult -ceq 'ALL_PLANNED_PASS' -and $state.BatchResult -ceq 'PENDING_INDEPENDENT_SUMMARY') 'loop finish never alone declares PASS'}
        $counts=@(Get-OwnedStabilityCounts $plan $state.Attempts)
        Check-OwnedGate (($counts|Measure-Object Passed -Sum).Sum -eq $(if($failureAt){$expected-1}else{100})) 'success numerator excludes failed gesture'
    }
}
$state=[ordered]@{Attempts=@();FinalButtonState='UNKNOWN';FirstUnexpectedFailure=$null;BatchResult='NOT_RUN';LoopResult='NOT_RUN'};$called=0
Invoke-OwnedStabilityProgression $plan $state {param($attempt) $script:called++;throw 'synthetic identity/validator/post exception'} {param($type,$attempt)}
Check-OwnedGate ($called -eq 1 -and $state.Attempts.Count -eq 1 -and $state.Attempts[0].Result -ceq 'INVALID_EVIDENCE') 'callback exception preserves attempt and stops'
Reject-OwnedGate {Assert-OwnedStabilityNextStep $plan @() $plan[20]} 'formal cannot start without all smoke'
$good=[pscustomobject]@{IndependentlyValidatedResult='PASS';RunnerExitCode=0;IndependentVerdict=[pscustomobject]@{Result='PASS';TestMode='normal';Operation='Move';FixtureResult='PASS';GestureResult='PASS';CleanupResult='NOT_NEEDED';TakeoverAcceptance='PASS';ContractVerified=$true;SyntheticFixture=$false;NormalProof=[pscustomobject]@{Result='PASS'}}}
Assert-OwnedStabilityNormalVerdict $good Move;Check-OwnedGate $true 'full current normal proof accepted'
foreach($fault in @('mode','abort','cached','operation','contract','synthetic','normalproof')){
    $bad=Copy-OwnedGate $good
    switch($fault){mode {$bad.IndependentVerdict.TestMode='controlled_abort'};abort {$bad.IndependentVerdict.FixtureResult='PASS_EXPECTED_ABORT'};cached {$bad.IndependentlyValidatedResult='NOT_RUN'};operation {$bad.IndependentVerdict.Operation='BottomResize'};contract {$bad.IndependentVerdict.ContractVerified='true'};synthetic {$bad.IndependentVerdict.SyntheticFixture=$true};normalproof {$bad.IndependentVerdict.NormalProof=$null}}
    Reject-OwnedGate {Assert-OwnedStabilityNormalVerdict $bad Move} "exit0 not sufficient $fault"
}
foreach($file in @('run-r1c4b-owned-stability-gates.ps1','verify-r1c4b-owned-stability-summary.ps1','r1c4b-owned-stability-model.ps1')){
    $tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $file),[ref]$tokens,[ref]$errors)
    Check-OwnedGate (@($errors).Count -eq 0) "parse $file"
    Check-OwnedGate ($ast.Extent.Text -notmatch '\b(Start-Process|SendInput|SetCursorPos|SetForegroundWindow|AttachThreadInput|Start-Sleep)\b') 'no input/window/retry implementation'
    if($file -ceq 'run-r1c4b-owned-stability-gates.ps1'){
        $child=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -ceq 'powershell.exe' -and $n.Extent.Text.Contains('-Mode normal')},$true))
        Check-OwnedGate ($child.Count -eq 1 -and $child[0].Extent.Text.Contains('-ExpectedHEAD $state.ExecutedHEAD')) 'only one fixed v5 normal child site with frozen HEAD'
        Check-OwnedGate ($ast.ParamBlock.Parameters.Count -eq 0) 'no count/resume/retry/AllowSynthetic live option'
        Check-OwnedGate ($ast.Extent.Text.Contains('MetadataWriteStatus=''ERROR''') -and $ast.Extent.Text.Contains('finalization.json') -and $ast.Extent.Text.Contains('$summaryExit=2')) 'write failures preserve receipt and nonzero exit'
    }
    if($file -ceq 'verify-r1c4b-owned-stability-summary.ps1'){
        Check-OwnedGate ($ast.Extent.Text.Contains('Read-ReliabilityRun') -and $ast.Extent.Text.Contains('Get-OwnedStabilityStoppedResult') -and $ast.Extent.Text.Contains('Get-OwnedStabilityMeasurement')) 'independent raw paths, not old aggregate program'
    }
}
Write-Host "Fix H pure progression checks=$checks PASS; no child/native/environment/Git/GUI/input"
