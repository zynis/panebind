[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$EvidencePath)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-evidence-validation.ps1')
$records=@(Get-Content -LiteralPath $EvidencePath -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json -ErrorAction Stop})
$validated=Test-C4BRecords $records
$g=$records|Where-Object {$_.type -eq 'gesture' -and $_.gesture -eq 1}|Select-Object -First 1
$samples=@($records|Where-Object {$_.type -eq 'sample' -and $_.gesture -eq 1})
$ops=@($records|Where-Object {$_.type -eq 'correction' -and $_.gesture -eq 1})
$near=$samples|ForEach-Object {[pscustomobject]@{receipt=$_.receipt;gap=[decimal]$_.raw[1]-[decimal]$g.initial[0].visible[3];result=$_.result}}|Sort-Object {[Math]::Abs($_.gap)}|Select-Object -First 1
$failures=@($records|Where-Object {$_.type -eq 'correction' -and $_.native_attempted}|ForEach-Object {
    $op=$_;$src=$op.source;$actual=$op.actual[$src];$before=$op.before[$src]
    $vx=(Get-C4ARectKey $actual.visible) -ceq (Get-C4ARectKey $op.corrected)
    $px=(Get-C4ARectKey $actual.positioning) -ceq (Get-C4ARectKey $op.target_positioning)
    $trigger=$records|Where-Object {$_.type -eq 'receipt' -and $_.receipt -eq $op.source_receipt}|Select-Object -First 1
    $link=Get-C4BCorrectionChronology $op $records
    [ordered]@{gesture=$op.gesture;operation=$op.operation;source=$src;raw_visible=$op.raw;corrected_visible=$op.corrected;before_visible=$before.visible;before_positioning=$before.positioning;target_positioning=$op.target_positioning;native_attempted=$op.native_attempted;native_success=$op.native_success;native_error=$op.error;actual_visible=$actual.visible;actual_positioning=$actual.positioning;visible_exact=$vx;positioning_exact=$px;visible_delta=@(0..3|ForEach-Object {[decimal]$actual.visible[$_]-[decimal]$op.corrected[$_]});positioning_delta=@(0..3|ForEach-Object {[decimal]$actual.positioning[$_]-[decimal]$op.target_positioning[$_]});trigger_receipt=$op.source_receipt;trigger_kind=$trigger.event;native_start_before_end_callback=$(if($null -eq $link.EndReceipt){'UNKNOWN_NO_END_RECORDED'}else{$end=$records|Where-Object {$_.type -eq 'receipt' -and $_.receipt -eq $link.EndReceipt};$op.native_start_qpc -lt $end.callback_qpc});native_start_qpc=$op.native_start_qpc;native_return_qpc=$op.native_return_qpc;postverify_qpc=$op.postverify_qpc;classification=$(if(-not $op.native_success){'NATIVE_CALL_FAILURE'}elseif(-not $px){'POSITIONING_REJECTED'}elseif(-not $vx){'DWM_VISIBLE_LAG_CANDIDATE_ONLY'}else{'EXACT'});chronology=$link}
})
[ordered]@{EvidenceSHA256=(Get-FileHash -LiteralPath $EvidencePath -Algorithm SHA256).Hash;Records=$records.Count;Result=$validated.Result;Reason=$validated.Reason;RecordTypes=@($records|Group-Object type|Select-Object Name,Count);HumanM1Attempt1=[ordered]@{start=$g.start;end=$g.end;nearest_gap=$near.gap;nearest_receipt=$near.receipt;meaningful=$g.meaningful;solver_calls=$g.solver_calls;motion_suppressed=$g.motion_suppressed;proposals=$g.proposals;native_corrections=@($ops|Where-Object native_attempted -eq $true).Count;skip_reasons=@($ops|Group-Object reason|Select-Object Name,Count)};FailedCorrections=$failures}|ConvertTo-Json -Depth 16
