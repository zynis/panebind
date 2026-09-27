Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# Synthetic pure statistics fixtures only. Never call native, environment,
# aggregate, Git, existing UAT or a disk-writing seam.
$checks=0
function Check-OwnedStatistics([bool]$Value,[string]$Reason){if(-not $Value){throw "owned statistics fixture: $Reason"};$script:checks++}
function Copy-OwnedStatisticsFixture($Value){return $Value|ConvertTo-Json -Depth 24 -Compress|ConvertFrom-Json}
$statisticsPath=Join-Path $PSScriptRoot 'r1c4b-owned-stability-statistics.ps1'
$statisticsTokens=$null;$statisticsErrors=$null
$statisticsAst=[Management.Automation.Language.Parser]::ParseFile($statisticsPath,[ref]$statisticsTokens,[ref]$statisticsErrors)
Check-OwnedStatistics (@($statisticsErrors).Count -eq 0) 'pure helper parses'
$functionNames=@('Get-OwnedStatisticsField','Assert-OwnedStatistics','Convert-OwnedStatisticsInt64','Get-OwnedStatisticsMetrics','Get-OwnedStatisticsRows','Get-OwnedStatisticsQuantile','Get-OwnedStabilityMeasurement','Get-OwnedStabilityStatistics')
$allowed=@($functionNames)+@('Set-StrictMode','Sort-Object','Where-Object','ForEach-Object')
Check-OwnedStatistics (@($statisticsAst.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -cnotin $allowed},$true)).Count -eq 0) 'helper contains only pure calculations'
foreach($name in $functionNames){
    $nodes=@($statisticsAst.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name},$true))
    Check-OwnedStatistics ($nodes.Count -eq 1) "one pure helper $name"
    $narrow=@($nodes[0].FindAll({param($n) $n -is [Management.Automation.Language.ConvertExpressionAst] -and $n.Type.TypeName.FullName -cin @('int','Int32','System.Int32')},$true))
    Check-OwnedStatistics ($narrow.Count -eq $(if($name -ceq 'Get-OwnedStatisticsQuantile'){2}else{0})) "$name only narrows bounded percentile indices"
}
. $statisticsPath
function New-OwnedStatisticsFixture([long]$Nonce=1234567890123,[long]$Frequency=10000000){
    $fixtureRows=[Collections.Generic.List[object]]::new();[long]$start=900000000000
    function Add-OwnedStatisticsRow([string]$Type,$Fields){
        $row=[ordered]@{schema='r1c4b-takeover-owned/v5';sequence=[long]($fixtureRows.Count+1);type=$Type;qpc=($start+[long]($fixtureRows.Count+1)*100)}
        foreach($key in $Fields.Keys){$row[$key]=$Fields[$key]};$fixtureRows.Add([pscustomobject]$row)
    }
    Add-OwnedStatisticsRow startup @{operation='Move';test_mode='normal';qpc_frequency=$Frequency;run_nonce=$Nonce;pid=100;ui_tid=101}
    Add-OwnedStatisticsRow guard @{hwnd=202;pid=100;tid=101}
    Add-OwnedStatisticsRow owned @{hwnd=200;pid=100;tid=101;run_nonce=$Nonce}
    Add-OwnedStatisticsRow receiver @{receiver_hwnd=300;receiver_pid=301;receiver_tid=302}
    Add-OwnedStatisticsRow winevent_hook_installed @{hook=400;install_success=$true}
    Add-OwnedStatisticsRow winevent_callback @{event=11;callback_qpc=($start+550)}
    Add-OwnedStatisticsRow winevent_match @{callback_record_sequence=6;accepted=$true}
    Add-OwnedStatisticsRow writer_begin @{actor='source';kind='handoff';operation_id=1;quantum_id=0;native_calls=1}
    Add-OwnedStatisticsRow writer_result @{kind='handoff';operation_id=1;quantum_id=0;native_calls=1}
    Add-OwnedStatisticsRow raw_input @{button_flags=0}
    Add-OwnedStatisticsRow writer_begin @{actor='source';kind='raw_movement';operation_id=2;quantum_id=1;native_calls=1}
    Add-OwnedStatisticsRow writer_result @{kind='raw_movement';operation_id=2;quantum_id=1;native_calls=1}
    Add-OwnedStatisticsRow raw_quantum @{operation_id=2;quantum_id=1;native_calls=1}
    Add-OwnedStatisticsRow raw_input @{button_flags=2}
    Add-OwnedStatisticsRow source_final_acceptance @{}
    Add-OwnedStatisticsRow winevent_hook_removed @{hook=400;source_hwnd=200;source_pid=100;source_tid=101;remove_success=$true}
    Add-OwnedStatisticsRow receiver_shutdown @{receiver_hwnd=300;receiver_pid=301;receiver_tid=302;registration_removed=$true;window_destroyed=$true}
    Add-OwnedStatisticsRow shutdown @{run_nonce=$Nonce;owned_window_destroyed=$true;guard_window_destroyed=$true;receiver_stopped=$true;winevent_hook_removed=$true;takeover_geometry_writes=2}
    $timings=[ordered]@{};foreach($metric in @(Get-OwnedStatisticsMetrics)){$timings[$metric]=@([long]100)}
    $normal=[pscustomobject]@{Result='PASS';Operation='Move';ContractVerified=$true;RunNonce=$Nonce;QpcFrequency=$Frequency;NativeWrites=2;HandoffNativeCalls=1;ShieldNativeCalls=0;RawPackets=2;ContinuationQuanta=1;NativeDragAfterWinEventEnd=0;PostEndInputShield='NOT_NEEDED';TimingTickSamples=[pscustomobject]$timings}
    $verdict=[pscustomobject]@{Result='PASS';FixtureResult='PASS';GestureResult='PASS';CleanupResult='NOT_NEEDED';TakeoverAcceptance='PASS';Operation='Move';TestMode='normal';ContractVerified=$true;NormalProof=$normal}
    return [pscustomobject]@{Rows=@($fixtureRows.ToArray());Verdict=$verdict}
}
function Measure-OwnedStatisticsFixture($Fixture){return Get-OwnedStabilityMeasurement -Rows $Fixture.Rows -Verdict $Fixture.Verdict -Phase smoke -Configuration Debug -Operation Move}
$fixture=New-OwnedStatisticsFixture
$measurement=Measure-OwnedStatisticsFixture $fixture
Check-OwnedStatistics ($measurement.Status -ceq 'AVAILABLE') "complete synthetic facts available: $($measurement.Reason)"
Check-OwnedStatistics ($measurement.StartQpc -is [long] -and $measurement.StartQpc -gt [int]::MaxValue -and $measurement.RunNonce -is [long] -and $measurement.QpcFrequency -is [long]) 'QPC/frequency/nonce remain signed Int64'
foreach($metric in @(Get-OwnedStatisticsMetrics)){
    Check-OwnedStatistics ($measurement.TimingTickSamples[$metric][0] -is [long] -and $measurement.TimingMilliseconds[$metric][0] -is [decimal] -and $measurement.TimingMilliseconds[$metric][0] -eq [decimal]'0.01') "typed ticks and decimal milliseconds $metric"
}
Check-OwnedStatistics ($measurement.Counters.SourceWrites -eq 2 -and $measurement.Counters.MaxWritesPerQuantum -eq 1 -and $measurement.Counters.HandoffWrites -eq 1 -and $measurement.Counters.RawPackets -eq 2 -and $measurement.Counters.ContinuationQuanta -eq 1) 'actual writer/result/quantum references drive counters'
Check-OwnedStatistics ($measurement.Resources.Source.Status -ceq 'VERIFIED_ABSENT' -and $measurement.Resources.Receiver.Status -ceq 'EXITED' -and $measurement.Resources.Hook.Status -ceq 'REMOVED' -and $measurement.Resources.Shield.Status -ceq 'NOT_NEEDED') 'resource evidence keeps absence distinct from actual receipts'
foreach($bad in @($null,$true,'10000000',1.0,0,-1,[uint64]::MaxValue)){
    $v=Copy-OwnedStatisticsFixture $fixture;$v.Rows[0].qpc_frequency=$bad
    Check-OwnedStatistics ((Measure-OwnedStatisticsFixture $v).Status -ceq 'NOT_AVAILABLE') "bad actual frequency rejected $bad"
}
foreach($bad in @($null,$true,'900000000100',900000000100.0,-1)){
    $v=Copy-OwnedStatisticsFixture $fixture;$v.Rows[0].qpc=$bad
    Check-OwnedStatistics ((Measure-OwnedStatisticsFixture $v).Status -ceq 'NOT_AVAILABLE') "typed row QPC rejects $bad"
}
$v=Copy-OwnedStatisticsFixture $fixture;$v.Rows[8].qpc=$v.Rows[7].qpc-1
Check-OwnedStatistics ((Measure-OwnedStatisticsFixture $v).Status -ceq 'NOT_AVAILABLE') 'actual row clock reversal unavailable'
$v=Copy-OwnedStatisticsFixture $fixture;$v.Rows[8].sequence++
Check-OwnedStatistics ((Measure-OwnedStatisticsFixture $v).Status -ceq 'NOT_AVAILABLE') 'noncontinuous actual sequence unavailable'
$v=Copy-OwnedStatisticsFixture $fixture;$v.Verdict.NormalProof=$null
Check-OwnedStatistics ((Measure-OwnedStatisticsFixture $v).Status -ceq 'NOT_AVAILABLE') 'no fallback to cached outer PASS without fresh NormalProof'
$v=Copy-OwnedStatisticsFixture $fixture;$v.Verdict.FixtureResult='PASS_EXPECTED_ABORT';$v.Verdict.TestMode='controlled_abort'
Check-OwnedStatistics ((Measure-OwnedStatisticsFixture $v).Status -ceq 'NOT_AVAILABLE') 'expected abort is never a normal statistical sample'
$v=Copy-OwnedStatisticsFixture $fixture;$v.Rows[0].schema='r1c4b-takeover-owned/v4'
Check-OwnedStatistics ((Measure-OwnedStatisticsFixture $v).Status -ceq 'NOT_AVAILABLE') 'old v4 rows do not become current samples'
foreach($metric in @(Get-OwnedStatisticsMetrics)){
    $v=Copy-OwnedStatisticsFixture $fixture;$v.Verdict.NormalProof.TimingTickSamples.$metric=$null
    $missing=Measure-OwnedStatisticsFixture $v
    Check-OwnedStatistics ($missing.Status -ceq 'NOT_AVAILABLE' -and $null -eq $missing.TimingMilliseconds -and $null -eq $missing.Counters) "null timing dimension never silently fills zero $metric"
    $v=Copy-OwnedStatisticsFixture $fixture;$v.Verdict.NormalProof.TimingTickSamples.$metric=@([long]100,[long]200)
    Check-OwnedStatistics ((Measure-OwnedStatisticsFixture $v).Status -ceq 'NOT_AVAILABLE') "partial/wrong sample granularity unavailable $metric"
}
$v=Copy-OwnedStatisticsFixture $fixture;$v.Verdict.NormalProof.TimingTickSamples.NativeExitToWinEventEnd=@([long]-123)
$signed=Measure-OwnedStatisticsFixture $v
Check-OwnedStatistics ($signed.Status -ceq 'AVAILABLE' -and $signed.TimingTickSamples.NativeExitToWinEventEnd[0] -eq -123 -and $signed.TimingMilliseconds.NativeExitToWinEventEnd[0] -eq [decimal]'-0.0123') 'accepted signed cross-stream delta is preserved without clamp'
foreach($bad in @($true,'100',100.0,$null,[uint64]::MaxValue)){
    $v=Copy-OwnedStatisticsFixture $fixture;$v.Verdict.NormalProof.TimingTickSamples.NativeExitToWinEventEnd=@($bad)
    Check-OwnedStatistics ((Measure-OwnedStatisticsFixture $v).Status -ceq 'NOT_AVAILABLE') 'timing samples reject null/coercion/out-of-Int64 values'
}
foreach($count in @([long]2147483647,[long]2147483648,[long]3000000000,[long]900000000000,[long]1134816073663,[long]1500000000000,[long]::MaxValue,[long]::MinValue)){
    $v=Copy-OwnedStatisticsFixture $fixture;$v.Verdict.NormalProof.TimingTickSamples.NativeExitToWinEventEnd=@($count)
    $large=Measure-OwnedStatisticsFixture $v
    Check-OwnedStatistics ($large.Status -ceq 'AVAILABLE' -and $large.TimingTickSamples.NativeExitToWinEventEnd[0] -eq $count -and $large.TimingMilliseconds.NativeExitToWinEventEnd[0] -eq [decimal]$count*[decimal]1000/[decimal]10000000) "full signed Int64 timing boundary $count"
}
$v=Copy-OwnedStatisticsFixture $fixture;$v.Rows[7].native_calls=0;$v.Rows[8].native_calls=0;$v.Rows[-1].takeover_geometry_writes=1;$v.Verdict.NormalProof.NativeWrites=1;$v.Verdict.NormalProof.HandoffNativeCalls=0
foreach($metric in @('IsolationReadyToHandoffWrite','WinEventEndToHandoffWrite','HandoffWriteDuration')){$v.Verdict.NormalProof.TimingTickSamples.$metric=@()}
$noWrite=Measure-OwnedStatisticsFixture $v
Check-OwnedStatistics ($noWrite.Status -ceq 'AVAILABLE' -and $noWrite.Counters.HandoffWrites -eq 0 -and $noWrite.TimingMilliseconds.HandoffWriteDuration.Count -eq 0) 'legitimate zero handoff writes preserve empty timing dimension, not null or invented zero latency'
foreach($change in @('proofcounter','writerpair','quantumpair','receiver','hook','sourceabsence')){
    $v=Copy-OwnedStatisticsFixture $fixture
    switch($change){
        proofcounter {$v.Verdict.NormalProof.NativeWrites=99}
        writerpair {$v.Rows[11].operation_id=99}
        quantumpair {$v.Rows[12].native_calls=0}
        receiver {$v.Rows[-2].registration_removed=$false}
        hook {$v.Rows[-3].hook=999}
        sourceabsence {$v.Rows[-1].owned_window_destroyed=$false}
    }
    Check-OwnedStatistics ((Measure-OwnedStatisticsFixture $v).Status -ceq 'NOT_AVAILABLE') "actual counters/resources contradict cached proof $change"
}
$a=New-OwnedStatisticsFixture 10001 10000000;$b=New-OwnedStatisticsFixture 10002 20000000
foreach($metric in @(Get-OwnedStatisticsMetrics)){$a.Verdict.NormalProof.TimingTickSamples.$metric=@([long]10000);$b.Verdict.NormalProof.TimingTickSamples.$metric=@([long]60000)}
$ma=Measure-OwnedStatisticsFixture $a;$mb=Measure-OwnedStatisticsFixture $b
$group=Get-OwnedStabilityStatistics -Measurements @($ma,$mb) -Phase smoke -Configuration Debug -Operation Move -PlannedGestures 2
Check-OwnedStatistics ($group.Status -ceq 'COMPLETE' -and $group.UsableGestures -eq 2 -and $group.QpcFrequencies.Count -eq 2) 'different actual frequencies convert independently before grouping'
foreach($metric in @(Get-OwnedStatisticsMetrics)){
    Check-OwnedStatistics ($group.TimingStatistics[$metric].P50Ms -eq [decimal]2 -and $group.TimingStatistics[$metric].P95Ms -eq [decimal]'2.9' -and $group.TimingStatistics[$metric].Count -eq 2) "fixed decimal linear percentiles across actual frequencies $metric"
}
Check-OwnedStatistics ($group.TimingStatistics.RawReceiptToOwnerQuantum.Granularity -ceq 'raw_owner_quantum' -and $group.TimingStatistics.RawReceiptToNativeWrite.Granularity -ceq 'raw_source_writer' -and $group.TimingStatistics.NativeExitToWinEventEnd.Granularity -ceq 'gesture') 'Raw samples are not labelled independent gestures'
Check-OwnedStatistics ($group.Counters.SourceWrites.Total -eq 4 -and $group.Counters.SourceWrites.MaxPerGesture -eq 2 -and $group.MaxWritesPerQuantum -eq 1 -and $group.Resources.ReceiverExited -eq 2) 'actual per-gesture counter totals and maxima agree'
$largeMeasurements=@();[long]$largeNonce=20000
foreach($ticks in @([long]2147483647,[long]2147483648,[long]3000000000,[long]900000000000,[long]1134816073663,[long]1500000000000)){
    $largeNonce++;$v=New-OwnedStatisticsFixture $largeNonce 10000000;$v.Verdict.NormalProof.TimingTickSamples.NativeExitToWinEventEnd=@($ticks);$largeMeasurements+=@(Measure-OwnedStatisticsFixture $v)
}
$largeGroup=Get-OwnedStabilityStatistics -Measurements $largeMeasurements -Phase smoke -Configuration Debug -Operation Move -PlannedGestures 6
Check-OwnedStatistics ($largeGroup.TimingStatistics.NativeExitToWinEventEnd.P50Ms -is [decimal] -and $largeGroup.TimingStatistics.NativeExitToWinEventEnd.P50Ms -eq [decimal]45150000) 'large-QPC p50 remains exact decimal without Int32 narrowing'
Check-OwnedStatistics ($largeGroup.TimingStatistics.NativeExitToWinEventEnd.P95Ms -is [decimal] -and $largeGroup.TimingStatistics.NativeExitToWinEventEnd.P95Ms -eq [decimal]'140870401.841575') 'large-QPC p95 keeps fractional interpolation without overflow/truncation'
$empty=Get-OwnedStabilityStatistics -Measurements @() -Phase formal -Configuration Release -Operation BottomResize -PlannedGestures 20
Check-OwnedStatistics ($empty.Status -ceq 'NOT_AVAILABLE' -and $empty.UsableGestures -eq 0 -and $null -eq $empty.Counters.SourceWrites.Total -and $null -eq $empty.MaxWritesPerQuantum -and $null -eq $empty.Resources.ReceiverExited) 'empty statistics do not manufacture zero counters/resources'
foreach($metric in @(Get-OwnedStatisticsMetrics)){Check-OwnedStatistics ($empty.TimingStatistics[$metric].Status -ceq 'NOT_AVAILABLE' -and $empty.TimingStatistics[$metric].Count -eq 0 -and $null -eq $empty.TimingStatistics[$metric].P50Ms -and $null -eq $empty.TimingStatistics[$metric].P95Ms) "empty timing percentiles unavailable $metric"}
$partial=Get-OwnedStabilityStatistics -Measurements @($ma) -Phase smoke -Configuration Debug -Operation Move -PlannedGestures 5
Check-OwnedStatistics ($partial.Status -ceq 'PARTIAL' -and $partial.UsableGestures -eq 1 -and $partial.TimingStatistics.NativeExitToWinEventEnd.Count -eq 1) 'partial group is explicitly partial, no placeholder repetitions'
$duplicate=Get-OwnedStabilityStatistics -Measurements @($ma,$ma) -Phase smoke -Configuration Debug -Operation Move -PlannedGestures 2
Check-OwnedStatistics ($duplicate.Status -ceq 'PARTIAL' -and $duplicate.UsableGestures -eq 1 -and $duplicate.UnavailableMeasurements[0].Reason -ceq 'DUPLICATE_RUN_NONCE') 'duplicate nonce excluded rather than counted twice'
$wrong=Get-OwnedStabilityStatistics -Measurements @($ma) -Phase formal -Configuration Debug -Operation Move -PlannedGestures 20
Check-OwnedStatistics ($wrong.Status -ceq 'NOT_AVAILABLE' -and $wrong.UnavailableGestures -eq 1) 'smoke is never mixed into formal samples'
$invalid=Get-OwnedStabilityStatistics -Measurements @($missing) -Phase smoke -Configuration Debug -Operation Move -PlannedGestures 5
Check-OwnedStatistics ($invalid.Status -ceq 'NOT_AVAILABLE' -and $invalid.UsableGestures -eq 0 -and $null -eq $invalid.Counters.SourceWrites.Total) 'unavailable measurement never enters success sample counters'
Check-OwnedStatistics ((Get-OwnedStatisticsQuantile @([decimal]-1,[decimal]1,[decimal]4) ([decimal]'0.5')) -eq [decimal]1) 'signed samples use same fixed interpolation'
Check-OwnedStatistics ($null -eq (Get-OwnedStatisticsQuantile @() ([decimal]'0.95'))) 'empty quantile null'
Write-Host "owned stability statistics synthetic_only=true checks=$checks PASS; no IO/native/environment/aggregate/Git/UAT"
