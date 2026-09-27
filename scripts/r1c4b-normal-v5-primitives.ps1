# Fix F normal-path composition only. The caller must first validate the v5
# envelope, additive record types, synthetic/empirical scope, fence diagnostics
# and button ledger. These records are consumed as-is: no v5 -> v4 projection,
# replacement schema, function override, or new abort-cleanup eligibility.
$fixFNormalFrozenValidator=Join-Path $PSScriptRoot 'r1c4b-input-isolation-validation.ps1'
if((Get-FileHash -LiteralPath $fixFNormalFrozenValidator -Algorithm SHA256).Hash -cne '9FFB838F9B530257774B1E53B61102E9C99166D0926D37300350DBF7B6A4719D'){
    throw [IO.InvalidDataException]::new('Fix F normal primitives require the unchanged Fix E validator')
}
. $fixFNormalFrozenValidator

function Test-FixFNormalOwnedRecords([object[]]$Rows){
    Assert-InputIsolation ($Rows.Count -ge 2 -and $Rows[0].type -ceq 'startup' -and $Rows[-1].type -ceq 'shutdown') 'Fix F normal lifecycle requires startup and shutdown'
    $s=$Rows[0];$last=$Rows[-1];$blocked=$false;$gesture=if($s.operation -ceq 'Move'){1}else{2}
    # Abort/cleanup has a separate validator and can never complete this path.
    foreach($r in $Rows){
        Assert-InputIsolation ($r.gesture -in @(0,$gesture)) 'wrong fresh normal gesture'
        Assert-InputIsolation ($r.type -cnotmatch '^(cleanup_|abort_)' -and $r.type -cnotin @('blocked','takeover_failure','test_fault')) 'abort or cleanup record cannot enter normal acceptance'
    }
    foreach($n in @('pid','ui_tid','qpc_frequency','takeover_geometry_writes','run_nonce','gesture_id')){Assert-AutoInteger (Get-AutoField $s $n) "startup $n"}
    foreach($n in @('human_input','real_explorer','sendinput_in_probe')){Assert-AutoBoolean (Get-AutoField $s $n) "startup $n"}
    Assert-InputIsolation ($s.operation -cin @('Move','BottomResize') -and $s.mode -ceq 'free_takeover' -and $s.foreground_contract -ceq 'verified_global_foreground_v2' -and $s.handoff_contract -ceq 'winevent_end_barrier_v1' -and $s.diagnostic_contract -ceq 'separated_authority_v1' -and $s.authority_contract -ceq 'product_gesture_v1' -and $s.input_isolation_contract -ceq 'post_end_shield_v1' -and $s.input_correlation -ceq 'actual_absolute_receipt_v1' -and $s.run_nonce -gt 0 -and $s.gesture_id -eq 1 -and $s.qpc_frequency -gt 0 -and $s.takeover_geometry_writes -eq 0 -and -not $s.human_input -and -not $s.real_explorer -and $s.sendinput_in_probe) 'Fix F normal path changed the original split-authority contracts'
    foreach($r in @(Get-IsolationRows $Rows 'source_work_post_skipped')){
        foreach($n in @('message','source_hwnd','source_pid','source_tid','actual_source_pid','actual_source_tid','actual_source_nonce','run_nonce')){Assert-AutoInteger (Get-AutoField $r $n) "skipped work $n"};foreach($n in @('own_identity','stop_requested','source_retired')){Assert-AutoBoolean (Get-AutoField $r $n) "skipped work $n"}
        $exact=$r.source_hwnd -gt 0 -and $r.actual_source_pid -eq $s.pid -and $r.actual_source_tid -eq $s.ui_tid -and $r.actual_source_nonce -eq $s.run_nonce
        Assert-InputIsolation ($blocked -and $r.run_nonce -eq $s.run_nonce -and $r.source_pid -eq $s.pid -and $r.source_tid -eq $s.ui_tid -and $r.own_identity -eq $exact -and ($r.stop_requested -or $r.source_retired -or -not $exact)) 'skipped source work cannot enter normal acceptance'
    }
    foreach($n in @('cursor_restored','owned_window_destroyed','guard_window_destroyed','receiver_stopped','external_windows_touched','input_shield_created','input_shield_destroyed','input_shield_activated')){Assert-AutoBoolean (Get-AutoField $last $n) "shutdown $n"}
    Assert-InputIsolation ($last.result -ceq 'CAPTURED_NOT_ACCEPTED' -and $last.owned_window_destroyed -and $last.guard_window_destroyed -and -not $last.external_windows_touched -and $last.run_nonce -eq $s.run_nonce -and -not $last.cursor_restored) 'unsafe resource cleanup or unauthorized cursor restore'
    $owned=@(Get-IsolationRows $Rows 'owned');$guard=@(Get-IsolationRows $Rows 'guard');$desktop=@(Get-IsolationRows $Rows 'desktop_gate');Assert-InputIsolation ($owned.Count -le 1 -and $guard.Count -le 1 -and $desktop.Count -le 1) 'duplicate initial identities'
    foreach($o in $owned){Assert-InputIsolation ($o.hwnd -gt 0 -and $o.pid -eq $s.pid -and $o.tid -eq $s.ui_tid -and $o.run_nonce -eq $s.run_nonce -and $o.actual_source_nonce -eq $s.run_nonce -and $o.dpi -gt 0 -and $o.monitor -gt 0) 'owned source generation/context';Assert-TakeoverGeometry $o;$null=Get-AutoRect $o.virtual_screen}
    foreach($g in $guard){Assert-InputIsolation ($g.hwnd -gt 0 -and $g.pid -eq $s.pid -and $g.tid -eq $s.ui_tid -and ($owned.Count -eq 0 -or $g.hwnd -ne $owned[0].hwnd)) 'guard exact PID/TID'}
    foreach($d in $desktop){Assert-InputIsolation ($d.active_unlocked -and $d.input_desktop_matches) 'desktop gate'}
    $bootstrap=Test-TakeoverGlobalForegroundV2 $Rows $owned $blocked
    $raw=Test-EndHandoffRawEvidence $Rows $s $owned $blocked
    $win=Test-IsolationWinEvents $Rows $owned $s $blocked
    $products=Test-IsolationProductPreflights $Rows $owned $s $win;$product=@($products|Where-Object Phase -ceq winevent_end)
    $isolation=Test-IsolationShieldLifecycle $Rows $owned $s $win $product
    $inputState=Test-IsolationInputs $Rows $owned $guard $s $bootstrap $win $isolation $blocked
    $native=Test-IsolationNativeLifecycle $Rows $owned $s $raw $bootstrap
    $writers=Test-IsolationSourceWriters $Rows $owned $s $win $product $isolation $raw.Packets
    $unowned=Test-IsolationGeometryReceipts $Rows $win $writers
    $acceptance=Test-IsolationAcceptance $Rows $owned $s $native $win $product $isolation $writers $inputState $raw.Packets
    # This unchanged helper proves the absence of cleanup input in a successful
    # normal path; all cleanup records were rejected above.
    $cleanup=Test-IsolationCleanup $Rows $owned $guard $s $win $isolation
    Test-EndDiagnosticAuxiliary $Rows $win $products
    foreach($p in @(Get-IsolationRows $Rows 'writer_input_isolation')){
        $proof=Test-IsolationPointProof $p $Rows $owned $s $isolation.Shield
        $b=@($Rows|Where-Object {$_.type -ceq 'writer_begin' -and $_.writer_input_isolation_sequence -eq $p.sequence})
        if($proof.Pass){Assert-InputIsolation ($b.Count -eq 1) 'passed writer test proof has no unique source operation'}else{$isolation.Failure=$true;Assert-InputIsolation ($b.Count -eq 0) 'source write followed failed owner-cursor test proof'}
    }
    $afterWin=if($null -ne $win.End){@(Get-IsolationRows $Rows 'DRAG'|Where-Object receipt_qpc -gt $win.End.callback_qpc).Count}else{0}
    $productPass=$product.Count -eq 1 -and $product[0].Pass
    $barrierCounterexample=$afterWin -gt 0 -or $unowned -gt 0 -or $writers.BarrierViolations -gt 0 -or ($product.Count -eq 1 -and (Get-EndDiagnosticDisposition $product[0].Checks ($null -ne $win.End)) -ceq 'FAIL')
    $architecture='UNRESOLVED';$result=if($raw.Gate -ceq 'FAIL'){'FAIL'}else{'BLOCKED'}
    if($barrierCounterexample){$result='FAIL';$architecture='REJECTED_AT_END_BARRIER'}
    elseif($writers.Failure -or $acceptance.Failure){$result='FAIL';$architecture='REJECTED_AT_TAKEOVER_WRITER'}
    elseif($isolation.Failure -or $inputState.Failure){$result='FAIL'}
    elseif($productPass -and $isolation.Pass -and $writers.Handoff -ceq 'PASS' -and $acceptance.Gate -ceq 'PASS' -and -not $blocked -and $win.Installed -and $win.Removed -and $raw.Gate -ceq 'PASS'){$result='PASS';$architecture='VALID_AT_OWNED_STAGE'}
    if($isolation.Shield -and $isolation.Destroy -and $isolation.Destroy.reason -ceq 'acceptance'){Assert-InputIsolation ($null -ne $acceptance.RawUp -and $isolation.Destroy.raw_up_receiver_qpc -eq $acceptance.RawUp.receiver_qpc) 'shield teardown does not bind actual accepted Raw UP'}
    Assert-InputIsolation ($last.takeover_geometry_writes -eq $writers.NativeCalls -and $last.input_shield_created -eq ($null -ne $isolation.Shield) -and $last.input_shield_destroyed -eq ($null -eq $isolation.Shield -or ($null -ne $isolation.Destroy -and $isolation.Destroy.destroy_success -and $isolation.Destroy.window_absent)) -and $last.input_shield_activated -eq (@(Get-IsolationRows $Rows 'input_shield_activation').Count -gt 0) -and $last.source_acceptance_sequence -eq $(if($null -ne $acceptance.Accepted){$acceptance.Accepted.sequence}else{0})) 'shutdown source/shield counters or acceptance references differ from actual records'
    $cancel=Get-EndHandoffBarrierGate -CancelCalls $native.CancelCalls -NativeEnter $native.RealEnter -NativeDrag $native.RealDrag -AuthorityValid ($owned.Count -eq 1 -and $bootstrap.result -ceq 'PASS') -CancelReturned $native.Returned -RealExit ($null -ne $native.Exit) -CaptureReleased $native.CaptureClear -LeftHeldAtExit $native.HeldExit -GuiClearAfterExit $productPass -RawHealthy $raw.Healthy -RawContinuation ($acceptance.Gate -ceq 'PASS' -and $writers.Remaining -ge 18) -NativeDragAfterReturn $native.AfterReturn -UnattributedGeometryAfterEnd $unowned -WritesBeforeEnd $writers.BarrierViolations
    $timings=[pscustomobject]@{NativeExitToWinEventEnd=@();WinEventEndToIsolationReady=@();IsolationReadyToHandoffWrite=@();WinEventEndToHandoffWrite=@();HandoffWriteDuration=@();RawReceiptToOwnerQuantum=@($writers.RawOwnerTicks);RawReceiptToNativeWrite=@($writers.RawWriteTicks)}
    if($null -ne $native.Exit -and $null -ne $win.End){$timings.NativeExitToWinEventEnd=@((Get-IsolationQpcDelta $win.End.callback_qpc $native.Exit.receipt_qpc 'native EXIT to WinEvent END'))}
    if($null -ne $isolation.Ready -and $null -ne $win.End){$timings.WinEventEndToIsolationReady=@((Get-IsolationQpcDelta $isolation.Ready.isolation_ready_qpc $win.End.callback_qpc 'WinEvent END to isolation ready'))}
    if($writers.HandoffStartQpc){$timings.IsolationReadyToHandoffWrite=@((Get-IsolationQpcDelta $writers.HandoffStartQpc $isolation.Ready.isolation_ready_qpc 'isolation ready to handoff write'));$timings.WinEventEndToHandoffWrite=@((Get-IsolationQpcDelta $writers.HandoffStartQpc $win.End.callback_qpc 'WinEvent END to handoff write'));$timings.HandoffWriteDuration=@([long]$writers.HandoffDuration)}
    $testGate=if($isolation.Failure -or $inputState.Failure){'FAIL'}elseif($isolation.Pass){'PASS'}else{'NOT_RUN'}
    Assert-InputIsolation ($result -ceq 'PASS' -and $architecture -ceq 'VALID_AT_OWNED_STAGE' -and $cancel -ceq 'PASS_WITH_TERMINAL_SETTLEMENT' -and $writers.Handoff -ceq 'PASS' -and $acceptance.Gate -ceq 'PASS' -and $cleanup.Status -ceq 'NOT_NEEDED' -and -not $inputState.PendingButton -and -not $acceptance.FinalLeftDown) 'Fix F normal path did not independently complete every original normal gate'
    return [pscustomobject]@{Result=$result;DiagnosedResult=$result;Operation=$s.operation;ForegroundContract=$s.foreground_contract;HandoffContract=$s.handoff_contract;DiagnosticContract=$s.diagnostic_contract;AuthorityContract=$s.authority_contract;IsolationContract=$s.input_isolation_contract;RunNonce=$s.run_nonce;ContractVerified=$true;ProductAuthoritySeparated='PASS';TestIsolationSeparated='PASS';ProductHandoffAuthority=$(if($productPass){'PASS'}else{'FAIL'});TestInputIsolation=$testGate;PostEndInputShield=$(if($isolation.Needed){'PASS'}else{'NOT_NEEDED'});WinEventWitness='PASS';PostEndGeometryStability='PASS';RawBackground=$raw.Gate;Cancel=$cancel;Handoff=$writers.Handoff;Takeover=$acceptance.Gate;PreReleaseControl='PASS';Architecture=$architecture;NativeWrites=$writers.NativeCalls;HandoffNativeCalls=$writers.HandoffCalls;ShieldNativeCalls=$isolation.ShieldNativeCalls;RawPackets=$raw.Packets.Count;RawMovementPackets=@($raw.Packets|Where-Object cursor_sampled).Count;RawUpPackets=@($raw.Packets|Where-Object {($_.button_flags -band 2) -ne 0}).Count;ContinuationQuanta=$writers.Remaining;NativeDragAfterWinEventEnd=$afterWin;UnownedGeometryAfterEnd=$unowned;TerminalSettlements=$native.Settlements;FullGestureTargets=$writers.FullTargets;CleanupInputRelease=$cleanup.Status;CleanupAcceptanceEligible=$false;PendingButton=$inputState.PendingButton;CleanupCurrentLeftDown=$cleanup.CurrentLeftDown;FinalLeftDown=$acceptance.FinalLeftDown;TimingTickSamples=$timings;QpcFrequency=$s.qpc_frequency;Reasons=@();SyntheticFixture=((Get-AutoField $s 'synthetic_fixture') -eq $true)}
}
