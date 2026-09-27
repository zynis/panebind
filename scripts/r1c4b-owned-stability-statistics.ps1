# Fix H pure statistics. The caller supplies a freshly validated complete v5
# normal verdict and its unmodified rows. This file performs no IO or input.
Set-StrictMode -Version Latest

function Get-OwnedStatisticsField($Value,[string]$Name){
    if($null -eq $Value){return $null}
    if($Value -is [Collections.IDictionary]){return ,($Value[$Name])}
    $property=$Value.PSObject.Properties[$Name];if($null -eq $property){return $null};return ,($property.Value)
}
function Assert-OwnedStatistics([bool]$Condition,[string]$Reason){if(-not $Condition){throw [IO.InvalidDataException]::new($Reason)}}
function Convert-OwnedStatisticsInt64($Value,[string]$Label,[switch]$Signed){
    $integer=$Value -is [sbyte] -or $Value -is [byte] -or $Value -is [int16] -or $Value -is [uint16] -or $Value -is [int32] -or $Value -is [uint32] -or $Value -is [int64] -or $Value -is [uint64]
    $minimum=if($Signed){[decimal][long]::MinValue}else{[decimal]0}
    Assert-OwnedStatistics ($integer -and [decimal]$Value -ge $minimum -and [decimal]$Value -le [decimal][long]::MaxValue) "$Label is not a supported signed Int64 integer"
    return [long]$Value
}
function Get-OwnedStatisticsMetrics{
    return @('NativeExitToWinEventEnd','WinEventEndToIsolationReady','IsolationReadyToHandoffWrite','WinEventEndToHandoffWrite','HandoffWriteDuration','RawReceiptToOwnerQuantum','RawReceiptToNativeWrite')
}
function Get-OwnedStatisticsRows($Rows,[string]$Type){return @($Rows|Where-Object {(Get-OwnedStatisticsField $_ 'type') -ceq $Type})}
function Get-OwnedStatisticsQuantile($Values,[decimal]$Fraction){
    Assert-OwnedStatistics ($Fraction -ge 0 -and $Fraction -le 1) 'Percentile fraction must be within [0,1]'
    $sorted=@($Values|Sort-Object)
    if($sorted.Count -eq 0){return $null}
    foreach($value in $sorted){Assert-OwnedStatistics ($value -is [decimal]) 'Percentile samples must already be decimal milliseconds'}
    [decimal]$index=([decimal]$sorted.Count-1)*$Fraction
    # Int32 is used only for bounded collection indices, never for QPC/ticks.
    $lower=[int][Math]::Floor($index);$upper=[int][Math]::Ceiling($index)
    return [decimal]$sorted[$lower]+([decimal]$sorted[$upper]-[decimal]$sorted[$lower])*($index-$lower)
}
function Get-OwnedStabilityMeasurement{
    param([object[]]$Rows,$Verdict,[ValidateSet('smoke','formal')][string]$Phase,[ValidateSet('Debug','Release')][string]$Configuration,[ValidateSet('Move','BottomResize')][string]$Operation)
    $measurement=[ordered]@{Schema='r1c4b-owned-stability-measurement/v1';Status='NOT_AVAILABLE';Reason=$null;Phase=$Phase;Configuration=$Configuration;Operation=$Operation;RunNonce=$null;QpcFrequency=$null;StartQpc=$null;EndQpc=$null;TimingTickSamples=$null;TimingMilliseconds=$null;TimingAvailability=$null;Counters=$null;Resources=$null;SequenceReferences=$null}
    try{
        $normal=Get-OwnedStatisticsField $Verdict 'NormalProof'
        foreach($pair in @(@('Result','PASS'),@('FixtureResult','PASS'),@('GestureResult','PASS'),@('CleanupResult','NOT_NEEDED'),@('TakeoverAcceptance','PASS'),@('Operation',$Operation),@('TestMode','normal'))){Assert-OwnedStatistics ((Get-OwnedStatisticsField $Verdict $pair[0]) -ceq $pair[1]) 'Statistics require a complete fresh normal verdict'}
        Assert-OwnedStatistics ((Get-OwnedStatisticsField $Verdict 'ContractVerified') -is [bool] -and $Verdict.ContractVerified -and $null -ne $normal -and (Get-OwnedStatisticsField $normal 'Result') -ceq 'PASS' -and (Get-OwnedStatisticsField $normal 'Operation') -ceq $Operation -and (Get-OwnedStatisticsField $normal 'ContractVerified') -is [bool] -and $normal.ContractVerified) 'Complete NormalProof is unavailable'
        Assert-OwnedStatistics ($Rows.Count -ge 2 -and (Get-OwnedStatisticsField $Rows[0] 'type') -ceq 'startup' -and (Get-OwnedStatisticsField $Rows[-1] 'type') -ceq 'shutdown') 'Complete actual rows are unavailable'
        $startup=$Rows[0];$shutdown=$Rows[-1]
        Assert-OwnedStatistics ($startup.schema -ceq 'r1c4b-takeover-owned/v5' -and $startup.test_mode -ceq 'normal' -and $startup.operation -ceq $Operation) 'Rows are not current v5 normal evidence'
        [long]$frequency=Convert-OwnedStatisticsInt64 (Get-OwnedStatisticsField $startup 'qpc_frequency') 'QPC frequency'
        Assert-OwnedStatistics ($frequency -gt 0 -and $frequency -eq (Convert-OwnedStatisticsInt64 (Get-OwnedStatisticsField $normal 'QpcFrequency') 'NormalProof frequency')) 'QPC frequency is missing or differs from NormalProof'
        [long]$nonce=Convert-OwnedStatisticsInt64 (Get-OwnedStatisticsField $startup 'run_nonce') 'Run nonce'
        Assert-OwnedStatistics ($nonce -gt 0 -and $nonce -eq (Convert-OwnedStatisticsInt64 (Get-OwnedStatisticsField $normal 'RunNonce') 'NormalProof nonce') -and $nonce -eq (Convert-OwnedStatisticsInt64 (Get-OwnedStatisticsField $shutdown 'run_nonce') 'Shutdown nonce')) 'NormalProof and rows have different generations'
        [long]$previous=0;[long]$serial=0
        foreach($row in $Rows){
            $serial++;[long]$qpc=Convert-OwnedStatisticsInt64 (Get-OwnedStatisticsField $row 'qpc') 'Row QPC'
            Assert-OwnedStatistics ($qpc -gt 0 -and $qpc -ge $previous -and (Convert-OwnedStatisticsInt64 (Get-OwnedStatisticsField $row 'sequence') 'Row sequence') -eq $serial) 'Clock reversal or noncontinuous sequence'
            $previous=$qpc
        }
        $owned=@(Get-OwnedStatisticsRows $Rows 'owned');$guard=@(Get-OwnedStatisticsRows $Rows 'guard');$receiver=@(Get-OwnedStatisticsRows $Rows 'receiver');$receiverEnd=@(Get-OwnedStatisticsRows $Rows 'receiver_shutdown')
        $hook=@(Get-OwnedStatisticsRows $Rows 'winevent_hook_installed');$hookEnd=@(Get-OwnedStatisticsRows $Rows 'winevent_hook_removed');$accepted=@(Get-OwnedStatisticsRows $Rows 'source_final_acceptance')
        foreach($set in @($owned,$guard,$receiver,$receiverEnd,$hook,$hookEnd,$accepted)){Assert-OwnedStatistics ($set.Count -eq 1) 'Resource lifecycle or final acceptance is incomplete'}
        foreach($flag in @('owned_window_destroyed','guard_window_destroyed','receiver_stopped','winevent_hook_removed')){Assert-OwnedStatistics ((Get-OwnedStatisticsField $shutdown $flag) -is [bool] -and $shutdown.$flag) 'Shutdown resource absence is not verified'}
        Assert-OwnedStatistics ($owned[0].pid -eq $startup.pid -and $owned[0].tid -eq $startup.ui_tid -and $owned[0].run_nonce -eq $nonce -and $guard[0].pid -eq $startup.pid -and $guard[0].tid -eq $startup.ui_tid) 'Source or guard generation mismatch'
        foreach($key in @('receiver_hwnd','receiver_pid','receiver_tid')){Assert-OwnedStatistics ((Get-OwnedStatisticsField $receiver[0] $key) -eq (Get-OwnedStatisticsField $receiverEnd[0] $key)) 'Receiver teardown identity differs'}
        foreach($flag in @('registration_removed','window_destroyed')){Assert-OwnedStatistics ((Get-OwnedStatisticsField $receiverEnd[0] $flag) -is [bool] -and $receiverEnd[0].$flag) 'Receiver was not removed and destroyed'}
        Assert-OwnedStatistics ($hook[0].install_success -is [bool] -and $hook[0].install_success -and $hookEnd[0].remove_success -is [bool] -and $hookEnd[0].remove_success -and $hookEnd[0].hook -eq $hook[0].hook -and $hookEnd[0].source_hwnd -eq $owned[0].hwnd -and $hookEnd[0].source_pid -eq $startup.pid -and $hookEnd[0].source_tid -eq $startup.ui_tid -and $hookEnd[0].sequence -gt $accepted[0].sequence -and $receiverEnd[0].sequence -gt $hookEnd[0].sequence -and $shutdown.sequence -gt $receiverEnd[0].sequence) 'Actual hook/receiver exit order is incomplete'
        $ends=@(Get-OwnedStatisticsRows $Rows 'winevent_callback'|Where-Object event -eq 11)
        Assert-OwnedStatistics ($ends.Count -eq 1) 'Unique WinEvent END is unavailable'
        $endMatches=@(Get-OwnedStatisticsRows $Rows 'winevent_match'|Where-Object callback_record_sequence -eq $ends[0].sequence)
        Assert-OwnedStatistics ($endMatches.Count -eq 1 -and $endMatches[0].accepted -is [bool] -and $endMatches[0].accepted) 'WinEvent END is not matched'
        [long]$endQpc=Convert-OwnedStatisticsInt64 $ends[0].callback_qpc 'WinEvent END QPC'
        [long]$afterDrag=0;foreach($drag in @(Get-OwnedStatisticsRows $Rows 'DRAG')){if((Convert-OwnedStatisticsInt64 $drag.receipt_qpc 'Native DRAG QPC') -gt $endQpc){$afterDrag++}}
        $begins=@(Get-OwnedStatisticsRows $Rows 'writer_begin');$results=@(Get-OwnedStatisticsRows $Rows 'writer_result');$quanta=@(Get-OwnedStatisticsRows $Rows 'raw_quantum')
        Assert-OwnedStatistics ($begins.Count -gt 0 -and $results.Count -eq $begins.Count) 'Source write lifecycle is incomplete'
        [long]$sourceWrites=0;[long]$handoffWrites=0;[long]$maxQuantum=0;$quantumCalls=@{};$operationIds=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach($begin in $begins){
            [long]$calls=Convert-OwnedStatisticsInt64 $begin.native_calls 'Source native call count'
            Assert-OwnedStatistics ($begin.actor -ceq 'source' -and $calls -le 1 -and $begin.kind -cin @('handoff','raw_movement') -and $operationIds.Add([string]$begin.operation_id)) 'Source writer actor/kind/call reference is invalid'
            $result=@($results|Where-Object operation_id -eq $begin.operation_id)
            Assert-OwnedStatistics ($result.Count -eq 1 -and $result[0].sequence -gt $begin.sequence -and $result[0].quantum_id -eq $begin.quantum_id -and (Convert-OwnedStatisticsInt64 $result[0].native_calls 'Writer result calls') -eq $calls) 'Source native-call result does not match its operation'
            $sourceWrites+=$calls
            if($begin.kind -ceq 'handoff'){$handoffWrites+=$calls}else{
                $quantum=@($quanta|Where-Object operation_id -eq $begin.operation_id)
                Assert-OwnedStatistics ($quantum.Count -eq 1 -and $quantum[0].quantum_id -eq $begin.quantum_id -and (Convert-OwnedStatisticsInt64 $quantum[0].native_calls 'Quantum native call count') -eq $calls) 'Owner quantum does not bind actual source writer calls'
                $id=[string](Convert-OwnedStatisticsInt64 $begin.quantum_id 'Owner quantum ID');Assert-OwnedStatistics ($id -cne '0' -and -not $quantumCalls.ContainsKey($id)) 'Owner quantum is duplicated or mislabelled'
                $quantumCalls[$id]=$calls;if($calls -gt $maxQuantum){$maxQuantum=$calls}
            }
        }
        Assert-OwnedStatistics ($quanta.Count -eq $quantumCalls.Count) 'Owner quantum has no actual source writer'
        $raw=@(Get-OwnedStatisticsRows $Rows 'raw_input');$placements=@(Get-OwnedStatisticsRows $Rows 'input_shield_position')
        $counters=[ordered]@{SourceWrites=$sourceWrites;MaxWritesPerQuantum=$maxQuantum;HandoffWrites=$handoffWrites;PostEndNativeDrag=$afterDrag;ShieldPlacements=[long]$placements.Count;RawPackets=[long]$raw.Count;ContinuationQuanta=[long]$quanta.Count}
        foreach($pair in @(@('NativeWrites','SourceWrites'),@('HandoffNativeCalls','HandoffWrites'),@('NativeDragAfterWinEventEnd','PostEndNativeDrag'),@('ShieldNativeCalls','ShieldPlacements'),@('RawPackets','RawPackets'),@('ContinuationQuanta','ContinuationQuanta'))){Assert-OwnedStatistics ((Convert-OwnedStatisticsInt64 (Get-OwnedStatisticsField $normal $pair[0]) ('NormalProof '+$pair[0])) -eq $counters[$pair[1]]) 'NormalProof counter differs from actual records'}
        Assert-OwnedStatistics ((Convert-OwnedStatisticsInt64 $shutdown.takeover_geometry_writes 'Shutdown source writes') -eq $sourceWrites) 'Shutdown source writes differ from actual source operations'
        $created=@(Get-OwnedStatisticsRows $Rows 'input_shield_created');$destroyed=@(Get-OwnedStatisticsRows $Rows 'input_shield_destroyed')
        $shield=[ordered]@{Status='NOT_NEEDED';CreatedCount=[long]$created.Count;DestroyedCount=[long]$destroyed.Count;Placements=[long]$placements.Count;HWND=$null;CreatedSequence=$null;DestroyedSequence=$null}
        if($created.Count){
            Assert-OwnedStatistics ($created.Count -eq 1 -and $destroyed.Count -eq 1 -and $destroyed[0].shield_hwnd -eq $created[0].shield_hwnd -and $destroyed[0].run_nonce -eq $nonce -and $destroyed[0].destroy_success -is [bool] -and $destroyed[0].destroy_success -and $destroyed[0].window_absent -is [bool] -and $destroyed[0].window_absent -and $destroyed[0].reason -ceq 'acceptance' -and $destroyed[0].source_acceptance_sequence -eq $accepted[0].sequence -and $destroyed[0].sequence -gt $accepted[0].sequence -and $destroyed[0].sequence -lt $hookEnd[0].sequence -and $normal.PostEndInputShield -ceq 'PASS') 'Actual shield exit is incomplete'
            $shield.Status='EXITED';$shield.HWND=$created[0].shield_hwnd;$shield.CreatedSequence=$created[0].sequence;$shield.DestroyedSequence=$destroyed[0].sequence
        }else{Assert-OwnedStatistics ($destroyed.Count -eq 0 -and $placements.Count -eq 0 -and $normal.PostEndInputShield -ceq 'NOT_NEEDED') 'Missing shield lifecycle was relabelled NOT_NEEDED'}
        $metrics=@(Get-OwnedStatisticsMetrics);$proofSamples=Get-OwnedStatisticsField $normal 'TimingTickSamples'
        Assert-OwnedStatistics ($null -ne $proofSamples -and @($proofSamples.PSObject.Properties).Count -eq $metrics.Count) 'Seven complete normal timing dimensions are required'
        $ticks=[ordered]@{};$milliseconds=[ordered]@{};$availability=[ordered]@{}
        foreach($metric in $metrics){
            $values=Get-OwnedStatisticsField $proofSamples $metric;Assert-OwnedStatistics ($null -ne $values) "Null or missing timing dimension: $metric"
            # Cross-stream EXIT/WinEvent receipt deltas are signed in the
            # frozen validator. Preserve accepted negatives; row-clock order
            # above is a different check and no latency is clamped to zero.
            $samples=@(foreach($value in @($values)){Convert-OwnedStatisticsInt64 $value ("Timing $metric") -Signed})
            $expected=if($metric -ceq 'RawReceiptToOwnerQuantum'){$quanta.Count}elseif($metric -ceq 'RawReceiptToNativeWrite'){@($begins|Where-Object {$_.kind -ceq 'raw_movement' -and $_.native_calls -eq 1}).Count}elseif($metric -cin @('IsolationReadyToHandoffWrite','WinEventEndToHandoffWrite','HandoffWriteDuration')){[long]$handoffWrites}else{1}
            Assert-OwnedStatistics ($samples.Count -eq $expected) "Partial or wrong-granularity timing dimension: $metric"
            $ticks[$metric]=$samples;$milliseconds[$metric]=@(foreach($sample in $samples){[decimal]$sample*[decimal]1000/[decimal]$frequency})
            $availability[$metric]=if($samples.Count){'AVAILABLE'}else{'NOT_AVAILABLE_NO_NATIVE_WRITE'}
        }
        $measurement.RunNonce=$nonce;$measurement.QpcFrequency=$frequency;$measurement.StartQpc=[long]$startup.qpc;$measurement.EndQpc=[long]$shutdown.qpc
        $measurement.TimingTickSamples=$ticks;$measurement.TimingMilliseconds=$milliseconds;$measurement.TimingAvailability=$availability;$measurement.Counters=$counters
        $measurement.Resources=[ordered]@{Source=[pscustomobject]@{Status='VERIFIED_ABSENT';HWND=$owned[0].hwnd;PID=$owned[0].pid;TID=$owned[0].tid;CreatedSequence=$owned[0].sequence;AbsenceSequence=$shutdown.sequence;Evidence='FROZEN_EMITTER_ABSENCE_AND_VALIDATED_LIFECYCLE'};Guard=[pscustomobject]@{Status='VERIFIED_ABSENT';HWND=$guard[0].hwnd;CreatedSequence=$guard[0].sequence;AbsenceSequence=$shutdown.sequence};Receiver=[pscustomobject]@{Status='EXITED';HWND=$receiver[0].receiver_hwnd;PID=$receiver[0].receiver_pid;TID=$receiver[0].receiver_tid;CreatedSequence=$receiver[0].sequence;DestroyedSequence=$receiverEnd[0].sequence};Hook=[pscustomobject]@{Status='REMOVED';Handle=$hook[0].hook;InstalledSequence=$hook[0].sequence;RemovedSequence=$hookEnd[0].sequence};Shield=[pscustomobject]$shield}
        $measurement.SequenceReferences=[pscustomobject]@{Startup=$startup.sequence;Shutdown=$shutdown.sequence;WinEventEnd=$ends[0].sequence;WinEventMatch=$endMatches[0].sequence;SourceFinalAcceptance=$accepted[0].sequence;WriterBegins=@($begins|ForEach-Object sequence);WriterResults=@($results|ForEach-Object sequence);RawQuanta=@($quanta|ForEach-Object sequence)}
        $measurement.Status='AVAILABLE'
    }catch{$measurement.Reason=$_.Exception.Message}
    return [pscustomobject]$measurement
}

function Get-OwnedStabilityStatistics{
    param([object[]]$Measurements=@(),[ValidateSet('smoke','formal')][string]$Phase,[ValidateSet('Debug','Release')][string]$Configuration,[ValidateSet('Move','BottomResize')][string]$Operation,[long]$PlannedGestures)
    Assert-OwnedStatistics ($PlannedGestures -gt 0) 'Planned gestures must be positive'
    $usable=@();$unavailable=@();$nonces=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach($measurement in $Measurements){
        $same=(Get-OwnedStatisticsField $measurement 'Schema') -ceq 'r1c4b-owned-stability-measurement/v1' -and (Get-OwnedStatisticsField $measurement 'Phase') -ceq $Phase -and (Get-OwnedStatisticsField $measurement 'Configuration') -ceq $Configuration -and (Get-OwnedStatisticsField $measurement 'Operation') -ceq $Operation
        if($same -and (Get-OwnedStatisticsField $measurement 'Status') -ceq 'AVAILABLE' -and $nonces.Add([string]$measurement.RunNonce)){$usable+=@($measurement)}else{$unavailable+=@([pscustomobject]@{RunNonce=(Get-OwnedStatisticsField $measurement 'RunNonce');Reason=$(if(-not $same){'GROUP_OR_SCHEMA_MISMATCH'}elseif((Get-OwnedStatisticsField $measurement 'Status') -ceq 'AVAILABLE'){'DUPLICATE_RUN_NONCE'}else{Get-OwnedStatisticsField $measurement 'Reason'})})}
    }
    $timings=[ordered]@{};$frequencies=[ordered]@{}
    foreach($measurement in $usable){$key=[string]$measurement.QpcFrequency;if(-not $frequencies.Contains($key)){$frequencies[$key]=[long]0};$frequencies[$key]++}
    foreach($metric in @(Get-OwnedStatisticsMetrics)){
        $values=@();foreach($measurement in $usable){$metricValues=Get-OwnedStatisticsField $measurement.TimingMilliseconds $metric;$values+=@($metricValues)}
        $granularity=if($metric -ceq 'RawReceiptToOwnerQuantum'){'raw_owner_quantum'}elseif($metric -ceq 'RawReceiptToNativeWrite'){'raw_source_writer'}else{'gesture'}
        $timings[$metric]=[pscustomobject]@{Status=$(if($values.Count){'AVAILABLE'}else{'NOT_AVAILABLE'});Count=[long]$values.Count;Unit='ms';Granularity=$granularity;GestureCount=[long]$usable.Count;P50Ms=(Get-OwnedStatisticsQuantile $values ([decimal]0.50));P95Ms=(Get-OwnedStatisticsQuantile $values ([decimal]0.95));QuantileMethod='linear_interpolation_in_decimal_millisecond_domain';NoSLA=$true}
    }
    $counters=[ordered]@{}
    foreach($counter in @('SourceWrites','HandoffWrites','PostEndNativeDrag','ShieldPlacements','RawPackets','ContinuationQuanta')){
        $total=$null;$maximum=$null
        if($usable.Count){[decimal]$sum=0;[long]$max=0;foreach($measurement in $usable){[long]$count=Convert-OwnedStatisticsInt64 (Get-OwnedStatisticsField $measurement.Counters $counter) $counter;$sum+=[decimal]$count;if($count -gt $max){$max=$count}};Assert-OwnedStatistics ($sum -le [decimal][long]::MaxValue) 'Counter total exceeds signed Int64';$total=[long]$sum;$maximum=$max}
        $counters[$counter]=[pscustomobject]@{Status=$(if($usable.Count){'AVAILABLE'}else{'NOT_AVAILABLE'});Total=$total;MaxPerGesture=$maximum}
    }
    $maxQuantum=$null;if($usable.Count){[long]$maxQuantum=0;foreach($measurement in $usable){[long]$count=Convert-OwnedStatisticsInt64 $measurement.Counters.MaxWritesPerQuantum 'Maximum writes per owner quantum';if($count -gt $maxQuantum){$maxQuantum=$count}}}
    $resources=[ordered]@{SourceVerifiedAbsent=$null;GuardVerifiedAbsent=$null;ReceiverExited=$null;HookRemoved=$null;ShieldExited=$null;ShieldNotNeeded=$null}
    if($usable.Count){foreach($key in @($resources.Keys)){$resources[$key]=[long]0};foreach($measurement in $usable){$resources.SourceVerifiedAbsent++;$resources.GuardVerifiedAbsent++;$resources.ReceiverExited++;$resources.HookRemoved++;if($measurement.Resources.Shield.Status -ceq 'EXITED'){$resources.ShieldExited++}else{$resources.ShieldNotNeeded++}}}
    return [pscustomobject]@{Schema='r1c4b-owned-stability-statistics/v1';Phase=$Phase;Configuration=$Configuration;Operation=$Operation;Status=$(if(-not $usable.Count){'NOT_AVAILABLE'}elseif($unavailable.Count -or $Measurements.Count -ne $PlannedGestures){'PARTIAL'}else{'COMPLETE'});PlannedGestures=$PlannedGestures;ReceivedMeasurements=[long]$Measurements.Count;UsableGestures=[long]$usable.Count;UnavailableGestures=[long]$unavailable.Count;UnavailableMeasurements=$unavailable;QpcFrequencies=$frequencies;TimingStatistics=$timings;Counters=$counters;MaxWritesPerQuantum=$maxQuantum;Resources=$resources;Scope='fresh_complete_v5_normal_passes_only';QuantileMethod='linear_interpolation_in_decimal_millisecond_domain';RetryCountSource='BATCH_INVENTORY_NOT_STATISTICS'}
}
