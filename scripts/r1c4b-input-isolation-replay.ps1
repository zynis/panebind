Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# Assessment helpers only. No process, window, input, Git or evidence writes.
. (Join-Path $PSScriptRoot 'r1c4b-input-isolation-validation.ps1')

function Assert-IsolationReplayData([bool]$Condition,[string]$Reason){
    if(-not $Condition){throw [IO.InvalidDataException]::new("owned evidence replay: $Reason")}
}
function Get-IsolationReplayAssessment($Metadata,$Verdict,[long]$RowCount){
    Assert-IsolationReplayData ($Metadata.Schema -ceq 'r1c4b-input-isolation-run/v1' -and $Metadata.Configuration -ceq 'Debug' -and $Metadata.Operation -ceq 'BottomResize') 'wrong original run contract'
    Assert-IsolationReplayData ($Metadata.ExecutedHEAD -ceq 'a67d3049c1027e8b2b82f8e122041b71ff5043ca' -and $Metadata.AfterHEAD -ceq $Metadata.ExecutedHEAD -and -not $Metadata.WorktreeDirty -and -not $Metadata.AfterWorktreeDirty -and $Metadata.ImplementationUnchanged) 'original implementation binding'
    Assert-IsolationReplayData ($Metadata.ProbeExitCode -eq 0 -and $Metadata.Result.Result -ceq 'INVALID_EVIDENCE' -and -not $Metadata.CurrentContractsVerified) 'original failed pipeline must remain historical'
    Assert-IsolationReplayData ($Metadata.BinarySHA256 -ceq '43289B87236C08E68EAC2A23BCA1AD9923454DDB21EBF4DE84447083933DEC6D' -and $Metadata.AfterBinarySHA256 -ceq $Metadata.BinarySHA256 -and $Metadata.LogSHA256 -ceq 'BB1E1F90D58A0B21C5A0B3F65F3A3DDB3994388C96FFE4C6A0F3ED5EF174B76F') 'original binary/log binding'
    Assert-IsolationReplayData ($RowCount -eq 460) 'original evidence row count'
    Assert-IsolationReplayData ($Verdict.ContractVerified -and -not $Verdict.SyntheticFixture -and $Verdict.Operation -ceq 'BottomResize' -and $Verdict.ForegroundContract -ceq 'verified_global_foreground_v2' -and $Verdict.HandoffContract -ceq 'winevent_end_barrier_v1' -and $Verdict.DiagnosticContract -ceq 'separated_authority_v1' -and $Verdict.AuthorityContract -ceq 'product_gesture_v1' -and $Verdict.IsolationContract -ceq 'post_end_shield_v1') 'corrected validator contract'
    $gates=[ordered]@{Result='PASS';ProductHandoffAuthority='PASS';TestInputIsolation='PASS';PostEndInputShield='PASS';WinEventWitness='PASS';PostEndGeometryStability='PASS';RawBackground='PASS';Cancel='PASS_WITH_TERMINAL_SETTLEMENT';Handoff='PASS';Takeover='PASS';PreReleaseControl='PASS';Architecture='VALID_AT_OWNED_STAGE'}
    foreach($gate in $gates.GetEnumerator()){
        if((Get-AutoField $Verdict $gate.Key) -cne $gate.Value){return [pscustomobject]@{ReplayResult='FAIL';CorrectedValidatorVerified=$false;FirstSemanticFailure=($gate.Key+'='+[string](Get-AutoField $Verdict $gate.Key))}}
    }
    if($Verdict.ContinuationQuanta -lt 18 -or $Verdict.NativeDragAfterWinEventEnd -ne 0 -or $Verdict.UnownedGeometryAfterEnd -ne 0 -or -not $Verdict.FullGestureTargets -or $Verdict.FinalLeftDown -ne $false -or $Verdict.PendingButton){return [pscustomobject]@{ReplayResult='FAIL';CorrectedValidatorVerified=$false;FirstSemanticFailure='complete Resize quanta/geometry/release gate'}}
    return [pscustomobject]@{ReplayResult='PASS';CorrectedValidatorVerified=$true;FirstSemanticFailure='NONE'}
}
function Get-IsolationReplayExceptionAssessment($Exception){
    # Semantic assertions use the new validator's explicit diagnostic type.
    # Unrecognized exceptions, missing fields and numeric/type errors are invalid
    # evidence, not a made-up native geometry rejection.
    $semantic=$Exception -is [InvalidOperationException] -and $Exception.Message.StartsWith('owned input isolation:',[StringComparison]::Ordinal)
    return [pscustomobject]@{ReplayResult=$(if($semantic){'FAIL'}else{'INVALID_EVIDENCE'});CorrectedValidatorVerified=$false;FirstSemanticFailure=$(if($semantic){$Exception.Message}else{'NONE'});Exception=$Exception.Message;ExceptionType=$Exception.GetType().FullName}
}
