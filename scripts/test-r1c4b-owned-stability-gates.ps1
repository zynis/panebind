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
# Handwritten expected counts for the three supported states. Exact UNKNOWN
# stays distinct; the separate coercion regression below demonstrates the old
# string-true/numeric-one counterexample rather than assuming UNKNOWN matched.
$neverLaunched=New-OwnedStabilityAttempt $plan[0]
$emptyFacts=@(Get-OwnedStabilityCounts $plan @($neverLaunched))[0]
Check-OwnedGate ($emptyFacts.Attempts -eq 1 -and $emptyFacts.PreflightAttempts -eq 0 -and $emptyFacts.ProbeInvocations -eq 0 -and $emptyFacts.ProbeInvocationsUnknown -eq 0 -and $emptyFacts.TargetGesturesEntered -eq 0 -and $emptyFacts.TargetGesturesUnknown -eq 0 -and $emptyFacts.NotRun -eq 5) 'Boolean false child/preflight/probe/target facts do not count as started'
$unknownAttempt=New-OwnedStabilityAttempt $plan[0]
$unknownAttempt.ChildInvoked=$true;$unknownAttempt.PreflightAttempted=$true
$unknownAttempt.ProbeInvoked='UNKNOWN';$unknownAttempt.TargetGestureEntered='UNKNOWN';$unknownAttempt.Result='INVALID_EVIDENCE'
$counts=@(Get-OwnedStabilityCounts $plan @($unknownAttempt));$first=$counts[0]
Check-OwnedGate ($first.Planned -eq 5 -and $first.Attempts -eq 1 -and $first.PreflightAttempts -eq 1 -and $first.ProbeInvocations -eq 0 -and $first.ProbeInvocationsUnknown -eq 1 -and $first.TargetGesturesEntered -eq 0 -and $first.TargetGesturesUnknown -eq 1 -and $first.InvalidEvidence -eq 1 -and $first.NotRun -eq 4) 'exact UNKNOWN is unknown, not Boolean true; handwritten 0/1 counters'
$roundtrip=Copy-OwnedGate @($unknownAttempt)
$counts=@(Get-OwnedStabilityCounts $plan $roundtrip);$first=$counts[0]
Check-OwnedGate ($roundtrip[0].ProbeInvoked -is [string] -and $roundtrip[0].TargetGestureEntered -is [string] -and $first.ProbeInvocations -eq 0 -and $first.ProbeInvocationsUnknown -eq 1 -and $first.TargetGesturesEntered -eq 0 -and $first.TargetGesturesUnknown -eq 1) 'JSON roundtrip retains exact tri-state counts'
$falseAttempt=New-OwnedStabilityAttempt $plan[1]
$falseAttempt.ChildInvoked=$true;$falseAttempt.PreflightAttempted=$true;$falseAttempt.ProbeInvoked=$false;$falseAttempt.TargetGestureEntered=$false;$falseAttempt.Result='BLOCKED'
$mixed=@(Get-OwnedStabilityCounts $plan @($unknownAttempt,$falseAttempt))[0]
Check-OwnedGate ($mixed.Attempts -eq 2 -and $mixed.PreflightAttempts -eq 2 -and $mixed.ProbeInvocations -eq 0 -and $mixed.ProbeInvocationsUnknown -eq 1 -and $mixed.TargetGesturesEntered -eq 0 -and $mixed.TargetGesturesUnknown -eq 1 -and $mixed.Blocked -eq 1 -and $mixed.NotRun -eq 3) 'known false and unknown remain disjoint in same group'
$trueAttempt=New-OwnedStabilityAttempt $plan[2]
$trueAttempt.ChildInvoked=$true;$trueAttempt.PreflightAttempted=$true;$trueAttempt.ProbeInvoked=$true;$trueAttempt.TargetGestureEntered=$true;$trueAttempt.Result='PASS'
$triple=@(Get-OwnedStabilityCounts $plan @($unknownAttempt,$falseAttempt,$trueAttempt))[0]
Check-OwnedGate ($triple.ProbeInvocations -eq 1 -and $triple.ProbeInvocationsUnknown -eq 1 -and $triple.TargetGesturesEntered -eq 1 -and $triple.TargetGesturesUnknown -eq 1 -and $triple.Passed -eq 1 -and $triple.NotRun -eq 2) 'Boolean true/false and exact UNKNOWN count independently'
$originalWrongProbeUnknown=@($trueAttempt|Where-Object ProbeInvoked -ceq 'UNKNOWN').Count
$originalWrongTargetUnknown=@($trueAttempt|Where-Object TargetGestureEntered -ceq 'UNKNOWN').Count
$correctTrue=@(Get-OwnedStabilityCounts $plan @($trueAttempt))[0]
Check-OwnedGate ($originalWrongProbeUnknown -eq 1 -and $originalWrongTargetUnknown -eq 1 -and $correctTrue.ProbeInvocationsUnknown -eq 0 -and $correctTrue.TargetGesturesUnknown -eq 0) 'original Boolean true-to-UNKNOWN defect is reproduced and corrected'
foreach($coerced in @('true',1)){
    $v=Copy-OwnedGate @($trueAttempt);$v[0].ProbeInvoked=$coerced;$v[0].TargetGestureEntered=$coerced;$v[0].Result='INVALID_EVIDENCE'
    $oldProbe=@($v|Where-Object ProbeInvoked -eq $true).Count;$oldTarget=@($v|Where-Object TargetGestureEntered -eq $true).Count
    Check-OwnedGate ($oldProbe -eq 1 -and $oldTarget -eq 1) "pre-repair counterexample silently counted coerced $coerced as two known true facts"
    Reject-OwnedGate {Get-OwnedStabilityCounts $plan $v} "coerced $coerced must be rejected, not counted"
}
foreach($field in @('ProbeInvoked','TargetGestureEntered')){
    foreach($bad in @($null,0,1,'true','false','unknown','Unknown','UNKNOWN ',@())){
        $v=Copy-OwnedGate @($unknownAttempt);$v[0].$field=$bad
        Reject-OwnedGate {Get-OwnedStabilityCounts $plan $v} "invalid tri-state $field/$bad"
    }
    $v=Copy-OwnedGate @($unknownAttempt);$v[0].PSObject.Properties.Remove($field)
    Reject-OwnedGate {Get-OwnedStabilityCounts $plan $v} "missing tri-state $field"
}
foreach($field in @('PreflightAttempted','ChildInvoked')){
    foreach($bad in @($null,0,1,'true','false','UNKNOWN')){
        $v=Copy-OwnedGate @($unknownAttempt);$v[0].$field=$bad
        Reject-OwnedGate {Get-OwnedStabilityCounts $plan $v} "invalid required Boolean $field/$bad"
    }
    $v=Copy-OwnedGate @($unknownAttempt);$v[0].PSObject.Properties.Remove($field)
    Reject-OwnedGate {Get-OwnedStabilityCounts $plan $v} "missing required Boolean $field"
}
foreach($fault in @('pre_without_child','probe_without_pre','probe_without_child','target_without_probe','target_unknown_without_probe','known_target_from_unknown_probe','false_target_from_unknown_probe','pass_without_probe','pass_without_target')){
    $v=New-OwnedStabilityAttempt $plan[0];$v.ChildInvoked=$true;$v.PreflightAttempted=$true;$v.ProbeInvoked=$true;$v.TargetGestureEntered=$true;$v.Result='INVALID_EVIDENCE'
    switch($fault){
        pre_without_child {$v.ChildInvoked=$false;$v.PreflightAttempted=$true;$v.ProbeInvoked=$false;$v.TargetGestureEntered=$false}
        probe_without_pre {$v.PreflightAttempted=$false}
        probe_without_child {$v.ChildInvoked=$false;$v.PreflightAttempted=$false}
        target_without_probe {$v.ProbeInvoked=$false}
        target_unknown_without_probe {$v.ProbeInvoked=$false;$v.TargetGestureEntered='UNKNOWN'}
        known_target_from_unknown_probe {$v.ProbeInvoked='UNKNOWN'}
        false_target_from_unknown_probe {$v.ProbeInvoked='UNKNOWN';$v.TargetGestureEntered=$false}
        pass_without_probe {$v.ProbeInvoked=$false;$v.TargetGestureEntered=$false;$v.Result='PASS'}
        pass_without_target {$v.TargetGestureEntered=$false;$v.Result='PASS'}
    }
    Reject-OwnedGate {Get-OwnedStabilityCounts $plan @($v)} "mutually exclusive count facts $fault"
}
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
