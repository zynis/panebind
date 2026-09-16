Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-evidence-validation.ps1')
function New-PlacementFixture {
    $d=[pscustomobject]@{capture_succeeded=$true;native_success=$true;win32_error=0;source_member=1;requested_visible=@(1279,651,2366,1492);requested_positioning=@(1268,651,2377,1503);actual_visible=@(1284,653,2371,1494);actual_positioning=@(1273,653,2382,1505);visible_exact=$false;positioning_exact=$false;visible_edge_delta=@(5,2,5,2);positioning_edge_delta=@(5,2,5,2);other_members_exact=$true;source_context_exact=$true;receipt_health=$true;failure_class='BothGeometryMismatch'}
    return [pscustomobject]@{native_attempted=$true;native_success=$true;error=0;source=1;gesture=2;watermark=365;native_return_qpc=100;actual=@($null,[pscustomobject]@{visible=$d.actual_visible;positioning=$d.actual_positioning},$null);before=@($null,$null,$null);raw=@(1284,651,2371,1492);corrected=$d.requested_visible;target_positioning=$d.requested_positioning;postverify=$d;postverify_failure=$d;capture_failure=$null;exact=$false;next_receipt_after_correction=$null;next_location_after_correction=$null;end_receipt_after_correction=$null}
}
$tests=0
$op=New-PlacementFixture;Test-C4BPostverify $op @();++$tests
foreach($fault in @('fake-capture','delta','actual','exact','class','failure-object','chronology')){
    $op=New-PlacementFixture
    switch($fault){
        'fake-capture' {$op.capture_failure=[pscustomobject]@{reason='capture_not_run'}}
        'delta' {$op.postverify.visible_edge_delta[0]=0}
        'actual' {$op.postverify.actual_visible=@(0,0,2,2)}
        'exact' {$op.postverify.positioning_exact=$true}
        'class' {$op.postverify.failure_class='VisibleMismatch'}
        'failure-object' {$op.postverify_failure=$null}
        'chronology' {$op.next_location_after_correction=366}
    }
    $rejected=$false;try {Test-C4BPostverify $op @()}catch{$rejected=$true}
    if(-not $rejected){throw "invalid placement accepted: $fault"};++$tests
}
$op=New-PlacementFixture
$rows=@(
    [pscustomobject]@{type='receipt';member=2;receipt=366;callback_qpc=101;event='LOCATION'},
    [pscustomobject]@{type='receipt';member=1;receipt=367;callback_qpc=102;event='LOCATION'},
    [pscustomobject]@{type='sample';source=1;gesture=2;receipt=367;callback_qpc=102;raw=$op.raw},
    [pscustomobject]@{type='receipt';member=1;receipt=368;callback_qpc=103;event='END'},
    [pscustomobject]@{type='receipt';member=1;receipt=369;callback_qpc=104;event='START'})
$link=Get-C4BCorrectionChronology $op $rows
if($link.NextReceipt -ne 367 -or $link.NextLocation -ne 367 -or $link.EndReceipt -ne 368 -or $link.GeometryObservation -cne 'PRIOR_RAW_OBSERVED_NOT_CAUSAL_PROOF'){throw 'chronology wrong'};++$tests
$op.next_receipt_after_correction=367;$op.next_location_after_correction=367;$op.end_receipt_after_correction=368
Test-C4BPostverify $op $rows;++$tests
$rows[2].raw=$op.corrected;$link=Get-C4BCorrectionChronology $op $rows
if($link.GeometryObservation -cne 'CORRECTED_GEOMETRY_OBSERVED'){throw 'corrected observation lost'};++$tests
$link=Get-C4BCorrectionChronology $op @($rows[-1],$rows[1]);if($null -ne $link.EndReceipt){throw 'invented END'};++$tests
# The declared new contract must reject an attempted native write without its
# diagnostic even on the early BLOCKED path. Legacy human log stays readable.
$startup=[pscustomobject]@{schema='r1c4b/v1';sequence=1;type='startup';evidence_kind='synthetic_fixture';implementation_sha=('a'*40);owner_sta=1;qpc_frequency=1;member_count=3;attraction=10;release=16;speed_limit=2000;screen_magnet=$false;window_magnet=$true;live_magnet=$true;glue_move=$true;glue_resize=$false;magnet_contract='source_only_exact_v1';console_contract='preserved_mode_live_owner_v1';postverify_contract='placement_v1'}
$records=@($startup)
for($i=0;$i -lt 3;++$i){$records+=[pscustomobject]@{schema='r1c4b/v1';sequence=$i+2;type='binding'}}
$records+=[pscustomobject]@{schema='r1c4b/v1';sequence=5;type='correction';native_attempted=$true;pending_registered=$true;preflight=$true;native_calls=1;flags=21}
$records+=[pscustomobject]@{schema='r1c4b/v1';sequence=6;type='shutdown';result='BLOCKED';reason='magnet_postverify_failed'}
$rejected=$false;try{$null=Test-C4BRecords $records -AllowSynthetic}catch{$rejected=$true};if(-not $rejected){throw 'new contract missing postverify accepted'};++$tests
$old=$ErrorActionPreference
try {$ErrorActionPreference='Continue';$output=& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'run-r1c4b-live-magnet-evidence.ps1') -IndependentReviewPassed 2>&1;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$old}
if($code -eq 0 -or ($output -join "`n") -notmatch 'automated Explorer gate has not passed'){throw 'human automatic-gate guard absent'};++$tests
Write-Host "R1C4B_POSTVERIFY_FIXTURES = PASS ($tests; synthetic only)"
