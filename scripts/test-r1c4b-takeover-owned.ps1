Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-takeover-owned-validation.ps1')

# All facts below are synthetic policy fixtures. This script cannot obtain
# cancellation authority, device events, or human acceptance.
$checks=0
function Check-Takeover([bool]$Value,[string]$Name){
    if(-not $Value){throw "takeover synthetic fixture failed: $Name"}
    $script:checks++
}
function Copy-TakeoverFacts($Value){
    return ($Value|ConvertTo-Json -Depth 12 -Compress|ConvertFrom-Json)
}

$raw=@{RegistrationObserved=$true;RegistrationSucceeded=$true;ReceiverIdentityValid=$true;
    BackgroundConfirmed=$true;MovementObserved=$true;UpObserved=$true;EvidenceHealthy=$true}
Check-Takeover ((Get-TakeoverRawBackgroundGate @raw) -ceq 'PASS') 'complete background raw evidence'
foreach($name in @($raw.Keys)){
    $bad=@{};foreach($key in $raw.Keys){$bad[$key]=$raw[$key]};$bad[$name]=$false
    Check-Takeover ((Get-TakeoverRawBackgroundGate @bad) -ceq 'BLOCKED') "missing raw proof $name"
}
Check-Takeover ((Get-TakeoverRawBackgroundGate) -ceq 'BLOCKED') 'no registration/input is not a device architecture rejection'

$cancel=@{CancelCalls=1;EnterObserved=$true;DragObserved=$true;ButtonHeldAtCancel=$true;
    AuthorityValid=$true;ApiCompleted=$true;ExitObserved=$true;CaptureChangeObserved=$true;
    CaptureReleased=$true;RemainingCursorSamples=18;LaterNativeDragCount=0;LaterGeometryChangeCount=0;
    ObservationComplete=$true;EvidenceHealthy=$true}
Check-Takeover ((Get-TakeoverMechanicalCancel @cancel) -ceq 'PASS') 'single cancel complete mechanical lifecycle'
Check-Takeover ((Get-TakeoverMechanicalCancel) -ceq 'UNKNOWN') 'no cancel data cannot fail'
foreach($name in @('EnterObserved','DragObserved','ButtonHeldAtCancel','AuthorityValid','ApiCompleted',
    'ExitObserved','CaptureChangeObserved','CaptureReleased','ObservationComplete','EvidenceHealthy')){
    $bad=@{};foreach($key in $cancel.Keys){$bad[$key]=$cancel[$key]};$bad[$name]=$false
    Check-Takeover ((Get-TakeoverMechanicalCancel @bad) -ceq 'UNKNOWN') "missing mechanical proof $name"
}
foreach($count in @(0,1,14)){
    $bad=@{};foreach($key in $cancel.Keys){$bad[$key]=$cancel[$key]};$bad.RemainingCursorSamples=$count
    Check-Takeover ((Get-TakeoverMechanicalCancel @bad) -ceq 'UNKNOWN') 'insufficient continued cursor path'
}
foreach($name in @('LaterNativeDragCount','LaterGeometryChangeCount')){
    $bad=@{};foreach($key in $cancel.Keys){$bad[$key]=$cancel[$key]};$bad[$name]=1
    Check-Takeover ((Get-TakeoverMechanicalCancel @bad) -ceq 'FAIL') "actual cancel counterexample $name"
    $bad.CancelCalls=0
    Check-Takeover ((Get-TakeoverMechanicalCancel @bad) -ceq 'UNKNOWN') 'counterexample before a cancellation is not a cancel failure'
    $bad.CancelCalls=1;$bad.AuthorityValid=$false
    Check-Takeover ((Get-TakeoverMechanicalCancel @bad) -ceq 'UNKNOWN') 'wrong source cannot reject architecture'
}
foreach($field in @('CancelCalls','RemainingCursorSamples','LaterNativeDragCount','LaterGeometryChangeCount')){
    $bad=@{};foreach($key in $cancel.Keys){$bad[$key]=$cancel[$key]};$bad[$field]=-1
    $caught=$false;try {$null=Get-TakeoverMechanicalCancel @bad}catch{$caught=$true}
    Check-Takeover $caught 'negative counter is invalid evidence'
}
$bad=@{};foreach($key in $cancel.Keys){$bad[$key]=$cancel[$key]};$bad.CancelCalls=2
$caught=$false;try {$null=Get-TakeoverMechanicalCancel @bad}catch{$caught=$true}
Check-Takeover $caught 'duplicate WM_CANCELMODE is invalid probe evidence'

$complete=@{Mechanical='PASS';RawBackground='PASS';RawContinuity=$true;RawUpAfterCancel=$true;EvidenceHealthy=$true}
Check-Takeover ((Get-TakeoverCompleteCancel @complete) -ceq 'PASS') 'full cancel requires mechanical and raw proof'
foreach($name in @('RawContinuity','RawUpAfterCancel','EvidenceHealthy')){
    $bad=@{};foreach($key in $complete.Keys){$bad[$key]=$complete[$key]};$bad[$name]=$false
    Check-Takeover ((Get-TakeoverCompleteCancel @bad) -ceq 'UNKNOWN') "missing complete cancel proof $name"
}
foreach($rawGate in @('BLOCKED','FAIL')){
    $bad=@{};foreach($key in $complete.Keys){$bad[$key]=$complete[$key]};$bad.RawBackground=$rawGate
    Check-Takeover ((Get-TakeoverCompleteCancel @bad) -ceq 'UNKNOWN') 'raw failure is not mechanical cancel rejection'
}
$bad=@{};foreach($key in $complete.Keys){$bad[$key]=$complete[$key]};$bad.Mechanical='FAIL';$bad.RawBackground='BLOCKED';$bad.RawContinuity=$false
Check-Takeover ((Get-TakeoverCompleteCancel @bad) -ceq 'FAIL') 'positive cancel counterexample survives unrelated raw absence'
$bad.Mechanical='UNKNOWN';$bad.RawBackground='PASS';$bad.RawContinuity=$true
Check-Takeover ((Get-TakeoverCompleteCancel @bad) -ceq 'UNKNOWN') 'complete raw cannot invent cancel evidence'

$envelope=@(
    [pscustomobject]@{schema='r1c4b-takeover-owned/v1';sequence=1;gesture=0;qpc=10;type='startup'},
    [pscustomobject]@{schema='r1c4b-takeover-owned/v1';sequence=2;gesture=0;qpc=11;type='shutdown'}
)
Test-TakeoverEnvelope $envelope;Check-Takeover $true 'valid envelope is only structural proof'
foreach($kind in @('schema','sequence','qpc','duplicate-startup','missing-shutdown','boolean-qpc')){
    $bad=Copy-TakeoverFacts $envelope
    switch($kind){
        'schema' {$bad[0].schema='r1c4b-auto-owned-modal/v1'}
        'sequence' {$bad[1].sequence=3}
        'qpc' {$bad[1].qpc=9}
        'duplicate-startup' {$bad[1].type='startup'}
        'missing-shutdown' {$bad[1].type='finished'}
        'boolean-qpc' {$bad[1].qpc=$true}
    }
    $caught=$false;try {Test-TakeoverEnvelope $bad}catch{$caught=$true}
    Check-Takeover $caught "invalid lifecycle envelope $kind"
}
$caught=$false;try {$null=Test-TakeoverOwnedRecords $envelope}catch{$caught=$true}
Check-Takeover $caught 'synthetic envelope cannot become empirical PASS'

function Add-TakeoverFixtureRow([string]$Type,$Fields=@{}){
    $script:fixtureClock+=100
    $row=[ordered]@{schema='r1c4b-takeover-owned/v1';sequence=$script:fixtureRows.Count+1;gesture=$script:fixtureGesture;qpc=$script:fixtureClock;type=$Type}
    foreach($name in $Fields.Keys){$row[$name]=$Fields[$name]}
    $script:fixtureRows.Add([pscustomobject]$row)
}
function Fixture-Geometry($P){return @{positioning=@($P);visible=@(($P[0]+11),$P[1],($P[2]-11),($P[3]-11));positioning_error=0;visible_hresult=0}}
function Add-TakeoverFixtureRaw($Cursor,[bool]$Held,[int]$Buttons=0){
    $script:fixtureSerial++
    $fields=@{receiver_sequence=$script:fixtureSerial;receiver_qpc=$script:fixtureClock+99;receiver_hwnd=400;receiver_pid=200;receiver_tid=201;input_code=1;raw_flags=0;dx=0;dy=0;button_flags=$Buttons;cursor_sampled=($null -ne $Cursor);cursor_success=($null -ne $Cursor);cursor=$Cursor;left_down=$Held;foreground_hwnd=300;foreground_pid=100;device_handle_present=$false;test_tag_matches=$true}
    if($null -ne $Cursor){
        if($script:fixtureAbsolute){$fields.raw_flags=11;$fields.dx=$script:lastFixtureDx;$fields.dy=$script:lastFixtureDy}
        else{$fields.dx=1}
        if($script:fixtureLag){$fields.cursor=@($script:fixturePreviousCursor)}
    }
    if($script:fixtureLag -and ($Buttons -band 2)){$fields.left_down=$true}
    Add-TakeoverFixtureRow 'raw_input' $fields
    if($null -ne $Cursor -and $script:fixtureAbsolute){
        ++$script:fixtureMovementCount
        Add-TakeoverFixtureRow 'raw_wait_begin' @{expected_cursor=@($Cursor);input_start_qpc=$script:lastFixtureInputStart;timeout_ms=2000}
        Add-TakeoverFixtureRow 'raw_wait_result' @{wait_result=0;raw_packets=($script:fixtureSerial-1);movement_count=$script:fixtureMovementCount}
        Add-TakeoverFixtureRow 'raw_motion_correlation' @{receiver_sequence=$script:fixtureSerial;delivered=$true;snapshot_cursor_matches=(Test-AutoPoint $fields.cursor $Cursor)}
    }
}
function Add-TakeoverFixtureInput([int]$Flags,$Point,[bool]$Post=$false,[bool]$Priming=$false,[bool]$Restore=$false){
    $capture=if($script:fixtureGesture -gt 0 -and $script:fixtureLeft -and -not $Post){300}else{0}
    Add-TakeoverFixtureRow 'input_fence' @{cursor=@($script:fixtureCursor);expected_cursor=@($script:fixtureCursor);foreground=300;target=300;left_down=$script:fixtureLeft;cancel_pending=$script:fixturePending;post_cancel=$Post;priming=$Priming;gui_query_succeeded=$true;capture_hwnd=$capture;window_from_point_root=300;gui_flags=$(if($capture -or $script:fixturePending){2}else{0});menu_owner_hwnd=0;move_size_hwnd=$(if($capture -or $script:fixturePending){300}else{0})}
    if($Restore){Add-TakeoverFixtureRow 'cursor_restore_begin' @{point=@($Point);target=300}}
    elseif($Flags -eq 1 -and ($script:fixtureGesture -eq 0 -or $Post)){
        Add-TakeoverFixtureRow 'destination_fence' @{point=@($Point);root=300;target=300;foreground=300;capture_hwnd=0;menu_owner_hwnd=0;move_size_hwnd=$(if($script:fixturePending){300}else{0});gui_flags=$(if($script:fixturePending){2}else{0});cancel_pending=$script:fixturePending;left_down=$script:fixtureLeft}
    }
    $script:lastFixtureInputStart=$script:fixtureClock+10
    $inputFields=@{flags=$Flags;point=@($Point);sent=1;error=0;input_tag=0x50424D41;injection_start_qpc=$script:lastFixtureInputStart;injection_return_qpc=$script:fixtureClock+20;restoring_cursor=$Restore}
    if($script:fixtureAbsolute){
        $script:lastFixtureDx=if($Flags -eq 1){[long][Math]::Floor((2*[decimal]$Point[0]+1)*65536/(2*1920))}else{0}
        $script:lastFixtureDy=if($Flags -eq 1){[long][Math]::Floor((2*[decimal]$Point[1]+1)*65536/(2*1200))}else{0}
        $inputFields.normalized_dx=$script:lastFixtureDx;$inputFields.normalized_dy=$script:lastFixtureDy;$inputFields.actual_mouse_flags=if($Flags -eq 1){57345}else{$Flags};$inputFields.receiver_watermark=$script:fixtureSerial;$inputFields.virtual_screen=@(0,0,1920,1200)
    }
    Add-TakeoverFixtureRow 'input' $inputFields
    $script:fixturePreviousCursor=@($script:fixtureCursor)
    if($Flags -eq 1){$script:fixtureCursor=@($Point)}
    if($Flags -eq 2){$script:fixtureLeft=$true};if($Flags -eq 4){$script:fixtureLeft=$false}
}
function Add-TakeoverFixtureUpFence([bool]$Post=$false){
    Add-TakeoverFixtureRow 'input_fence' @{cursor=@($script:fixtureCursor);expected_cursor=@($script:fixtureCursor);foreground=300;target=300;left_down=$false;cancel_pending=$false;post_cancel=$Post;priming=$false;gui_query_succeeded=$true;capture_hwnd=0;window_from_point_root=300;gui_flags=0;menu_owner_hwnd=0;move_size_hwnd=0}
}
function New-TakeoverFixture([switch]$RawBlocked,[switch]$Absolute,[switch]$LaggedSnapshot,[switch]$Pending,[switch]$ExitTimeout){
    $script:fixtureRows=[Collections.Generic.List[object]]::new();$script:fixtureClock=1000;$script:fixtureGesture=0;$script:fixtureSerial=1;$script:fixtureLeft=$false;$script:fixtureCursor=@(10,10);$script:fixturePreviousCursor=@(10,10);$script:fixtureAbsolute=[bool]$Absolute;$script:fixtureLag=[bool]$LaggedSnapshot;$script:fixtureMovementCount=0;$script:fixturePending=$false;$script:fixtureAborted=$false
    $p=@(100,100,740,540)
    $startup=@{evidence_kind='automated_owned_cancel';human_input=$false;real_explorer=$false;sendinput_in_probe=$true;mode='cancel_only';takeover_geometry_writes=0;foreground_contract='verified_activation_v1';pid=100;ui_tid=101;qpc_frequency=10000000;synthetic_fixture=$true}
    if($Absolute){$startup.input_correlation='actual_absolute_receipt_v1'}
    Add-TakeoverFixtureRow 'startup' $startup
    Add-TakeoverFixtureRow 'desktop_gate' @{active_unlocked=$true;input_desktop_matches=$true}
    Add-TakeoverFixtureRow 'guard' @{hwnd=301;pid=100;tid=101}
    Add-TakeoverFixtureRow 'receiver' @{receiver_sequence=1;receiver_qpc=$script:fixtureClock+99;receiver_hwnd=400;receiver_pid=200;receiver_tid=201;registration_verified=$true;usage_page=1;usage=2;registration_flags=256;keyboard_registered=$false}
    Add-TakeoverFixtureRow 'foreground_attempt' @{target=300;foreground=300;set_foreground_success=$true;visible=$true}
    $owned=Fixture-Geometry $p;$owned.hwnd=300;$owned.pid=100;$owned.tid=101;$owned.saved_cursor=@(10,10);$owned.work_area=@(0,0,1920,1080);$owned.dpi=96
    if($Absolute){$owned.virtual_screen=@(0,0,1920,1200)}
    Add-TakeoverFixtureRow 'owned' $owned
    Add-TakeoverFixtureRow 'foreground_bootstrap' @{target=300;set_foreground_attempted=$true;set_foreground_success=$true;activation_click_required=$false;temporary_topmost=$false;activation_point=$null;window_from_point_root_matches=$false;hit_test=$null;foreign_capture_clear=$false;sendinput_move_success=$false;sendinput_down_success=$false;sendinput_up_success=$false;wm_activate_seen=$false;wm_setfocus_seen=$false;activation_event_seen=$false;foreground=300;topmost_now=$false;final_foreground_matches=$true;topmost_restored=$true;result='PASS';reason='none'}
    Add-TakeoverFixtureRow 'raw_preflight_begin' @{point=@(300,300)}
    Add-TakeoverFixtureInput 1 @(300,300)
    if($RawBlocked){
        Add-TakeoverFixtureRow 'raw_wait_begin' @{expected_cursor=@(300,300);input_start_qpc=$script:lastFixtureInputStart;timeout_ms=2000}
        Add-TakeoverFixtureRow 'raw_wait_result' @{wait_result=258;raw_packets=0;movement_count=0}
        Add-TakeoverFixtureRow 'LEGACY_MOVE' @{screen_point=@(300,300);coordinate_valid=$true}
        Add-TakeoverFixtureRow 'blocked' @{reason='BLOCKED_BY_RAW_INPUT_DELIVERY'}
    }else{
        Add-TakeoverFixtureRaw @(300,300) $false
        Add-TakeoverFixtureInput 2 @(0,0)
        Add-TakeoverFixtureRaw $null $true 1
        Add-TakeoverFixtureInput 1 @(312,300)
        Add-TakeoverFixtureRaw @(312,300) $true
        Add-TakeoverFixtureInput 4 @(0,0)
        Add-TakeoverFixtureRaw $null $false 2
        Add-TakeoverFixtureUpFence
        Add-TakeoverFixtureRow 'raw_preflight_complete' @{movement_count=2;up_count=1}
        for($g=1;$g -le 2;++$g){
            $script:fixtureGesture=$g;$start=@(300,$(if($g -eq 1){109}else{$p[3]-1}));$end=@(($start[0]+$(if($g -eq 1){180}else{0})),($start[1]+$(if($g -eq 2){120}else{0})));$initial=@($p)
            Add-TakeoverFixtureInput 1 $start
            $path=Fixture-Geometry $p;$path.hit_test=if($g -eq 1){2}else{15};$path.start=$start;$path.end=$end;$path.samples=20;$path.cancel_after_sample=2;$path.interval_ms=30;$path.foreground=300
            Add-TakeoverFixtureRow 'path' $path
            Add-TakeoverFixtureInput 2 @(0,0)
            Add-TakeoverFixtureRow 'native_button_down' @{target=300;hit_test=$path.hit_test}
            for($i=1;$i -le 20;++$i){
                $next=@(($start[0]+($end[0]-$start[0])*$i/20),($start[1]+($end[1]-$start[1])*$i/20))
                Add-TakeoverFixtureInput 1 $next ($i -gt 2) ($i -eq 1)
                if($i -eq 1){$ent=Fixture-Geometry $p;$ent.owner_capture=300;Add-TakeoverFixtureRow 'ENTER' $ent}
                Add-TakeoverFixtureRaw $next $true
                if($i -le 2){
                    if($g -eq 1){$p=@(($initial[0]+9*$i),$initial[1],($initial[2]+9*$i),$initial[3])}else{$p=@($initial[0],$initial[1],$initial[2],($initial[3]+6*$i))}
                    $drag=Fixture-Geometry $p;$drag.event=if($g -eq 1){'WM_MOVING'}else{'WM_SIZING'};$drag.edge=if($g -eq 1){9}else{6};$drag.cursor=$next;$drag.proposed=@($p);$drag.after_cancel=$false
                    Add-TakeoverFixtureRow 'DRAG' $drag
                }
                if($i -eq 3 -and $Pending){
                    $waitStart=$script:fixtureClock+10
                    if($ExitTimeout){
                        $waitFinish=$waitStart+20000000;$script:fixtureClock=$waitFinish-10
                        Add-TakeoverFixtureRow 'cancel_exit_wait' @{started_qpc=$waitStart;finished_qpc=$waitFinish;timeout_ms=2000;observation_wakeup_sample=3;wait_result=258;gui_query_succeeded=$true;target=300;source_tid=101;source_identity=$true;foreground=300;capture_hwnd=0;move_size_hwnd=300;gui_flags=2;left_down=$true}
                        Add-TakeoverFixtureRow 'blocked' @{reason='cancel_missing_EXIT'}
                        Add-TakeoverFixtureRow 'cleanup_fence' @{target=300;foreground=300;own_identity=$true;desktop_ready=$true;visible=$true;cursor=@($next);left_down=$true;modifiers_clear=$true;capture_hwnd=0;root=300;menu_owner_hwnd=0;move_size_hwnd=300;gui_flags=2}
                        Add-TakeoverFixtureRow 'cleanup_release' @{sent=1;error=0;input_tag=0x50424D41;injection_start_qpc=$script:fixtureClock+10;injection_return_qpc=$script:fixtureClock+20;target=300;cursor=@($next);capture_hwnd=0;root=300}
                        $script:fixtureLeft=$false;$script:fixtureAborted=$true;break
                    }
                    $exit=Fixture-Geometry $p;$exit.owner_capture=0;$exit.left_down=$true;Add-TakeoverFixtureRow 'EXIT' $exit
                    Add-TakeoverFixtureRow 'cancel_exit_wait' @{started_qpc=$waitStart;finished_qpc=$script:fixtureClock+10;timeout_ms=2000;observation_wakeup_sample=3;wait_result=0;gui_query_succeeded=$true;target=300;source_tid=101;source_identity=$true;foreground=300;capture_hwnd=0;move_size_hwnd=0;gui_flags=0;left_down=$true}
                    $script:fixturePending=$false
                    Add-TakeoverFixtureRow 'cancel_confirmed' (Fixture-Geometry $p)
                }
                $sample=Fixture-Geometry $p;$sample.index=$i;$sample.cursor=$next;$sample.left_down=$true;$sample.post_cancel=$i -gt 2
                Add-TakeoverFixtureRow 'sample' $sample
                if($i -eq 2){
                    $issued=$script:fixtureClock+10
                    Add-TakeoverFixtureRow 'cancel_begin' @{target=300;source_tid=101;capture_before=300;left_down=$true}
                    $message=Fixture-Geometry $p;$message.target=300;$message.owner_capture=300;$message.left_down=$true;Add-TakeoverFixtureRow 'cancel_message' $message
                    $change=Fixture-Geometry $p;$change.new_capture=0;$change.owner_capture=0;Add-TakeoverFixtureRow 'CAPTURE_CHANGED' $change
                    if(-not $Pending){$exit=Fixture-Geometry $p;$exit.owner_capture=0;$exit.left_down=$true;Add-TakeoverFixtureRow 'EXIT' $exit}
                    Add-TakeoverFixtureRow 'cancel_return' @{transport_success=$true;error=0;recipient_result=0;issued_qpc=$issued;returned_qpc=$script:fixtureClock+10;gui_query_succeeded=$true;capture_after=0;left_down=$true}
                    if($Pending){$script:fixturePending=$true}else{Add-TakeoverFixtureRow 'cancel_confirmed' (Fixture-Geometry $p)}
                }
            }
            if($script:fixtureAborted){break}
            Add-TakeoverFixtureInput 4 @(0,0) $true
            Add-TakeoverFixtureRaw $null $false 2
            Add-TakeoverFixtureUpFence $true
            Add-TakeoverFixtureRow 'path_complete' (Fixture-Geometry $p)
        }
        if(-not $script:fixtureAborted){Add-TakeoverFixtureInput 1 @(10,10) $true $false $true}
    }
    $script:fixtureSerial++
    Add-TakeoverFixtureRow 'receiver_shutdown' @{receiver_sequence=$script:fixtureSerial;receiver_qpc=$script:fixtureClock+99;receiver_hwnd=400;receiver_pid=200;receiver_tid=201;registration_removed=$true;window_destroyed=$true}
    Add-TakeoverFixtureRow 'shutdown' @{result=$(if($RawBlocked -or $script:fixtureAborted){'BLOCKED'}else{'CAPTURED_NOT_ACCEPTED'});cursor_restored=(-not ($RawBlocked -or $script:fixtureAborted));owned_window_destroyed=$true;guard_window_destroyed=$true;receiver_stopped=$true;external_windows_touched=$false}
    return ,@($script:fixtureRows.ToArray())
}
function Renumber-TakeoverFixture($Rows){
    $serial=0
    for($i=0;$i -lt $Rows.Count;++$i){
        $Rows[$i].sequence=$i+1
        if($Rows[$i].type -cin @('receiver','raw_input','receiver_shutdown')){$Rows[$i].receiver_sequence=++$serial}
    }
    return ,$Rows
}
$valid=New-TakeoverFixture
$result=Test-TakeoverOwnedRecords $valid -AllowSynthetic
Check-Takeover ($result.Result -ceq 'CAPTURED' -and $result.RawBackground -ceq 'PASS' -and $result.CancelMove -ceq 'PASS' -and $result.CancelResize -ceq 'PASS' -and $result.GeometryWrites -eq 0 -and $result.Architecture -ceq 'UNRESOLVED') 'complete synthetic cancel-only is not takeover architecture VALID'
$blocked=New-TakeoverFixture -RawBlocked
$result=Test-TakeoverOwnedRecords $blocked -AllowSynthetic
Check-Takeover ($result.Result -ceq 'BLOCKED' -and $result.RawPackets -eq 0 -and $result.RawBackground -ceq 'BLOCKED' -and $result.CancelMove -ceq 'UNKNOWN' -and $result.CancelResize -ceq 'UNKNOWN' -and $result.Architecture -ceq 'UNRESOLVED') 'legacy injected movement without raw blocks before cancellation'
$caught=$false;try {$null=Test-TakeoverOwnedRecords $valid}catch{$caught=$true};Check-Takeover $caught 'native validation rejects synthetic fixture by default'
foreach($kind in @('no-fence','foreign-destination','foreign-capture','wrong-input-tag','second-cancel','write-record','startup-write','startup-boolean','receiver-pid','receiver-sequence','receiver-cleanup','receiver-foreground','wrong-raw-kind','forged-sampling','restore-before-end','preflight-no-up')){
    $bad=Copy-TakeoverFacts $valid
    switch($kind){
        'no-fence' {($bad|Where-Object type -eq input_fence|Select-Object -First 1).type='LEGACY_MOVE'}
        'foreign-destination' {($bad|Where-Object type -eq destination_fence|Select-Object -First 1).root=999}
        'foreign-capture' {($bad|Where-Object {$_.type -eq 'input_fence' -and $_.post_cancel}|Select-Object -First 1).capture_hwnd=999}
        'wrong-input-tag' {($bad|Where-Object type -eq input|Select-Object -First 1).input_tag=0}
        'second-cancel' {($bad|Where-Object {$_.type -eq 'cancel_confirmed' -and $_.gesture -eq 1}).type='cancel_begin'}
        'write-record' {($bad|Where-Object type -eq sample|Select-Object -First 1).type='native_write'}
        'startup-write' {$bad[0].takeover_geometry_writes=1}
        'startup-boolean' {$bad[0].human_input='false'}
        'receiver-pid' {($bad|Where-Object type -eq raw_input|Select-Object -First 1).receiver_pid=100}
        'receiver-sequence' {($bad|Where-Object type -eq raw_input|Select-Object -First 1).receiver_sequence=99}
        'receiver-cleanup' {($bad|Where-Object type -eq receiver_shutdown).registration_removed=$false}
        'receiver-foreground' {($bad|Where-Object type -eq raw_input|Select-Object -First 1).foreground_pid=200}
        'wrong-raw-kind' {($bad|Where-Object type -eq raw_input|Select-Object -First 1).input_code=0}
        'forged-sampling' {($bad|Where-Object type -eq raw_input|Select-Object -First 1).cursor_sampled=$false}
        'restore-before-end' {($bad|Where-Object type -eq input|Select-Object -First 1).restoring_cursor=$true}
        'preflight-no-up' {($bad|Where-Object {$_.type -eq 'raw_input' -and $_.gesture -eq 0 -and ($_.button_flags -band 2)}).button_flags=0}
    }
    $caught=$false;try {$null=Test-TakeoverOwnedRecords $bad -AllowSynthetic}catch{$caught=$true}
    Check-Takeover $caught "reject mapped evidence $kind"
}
$bad=Copy-TakeoverFacts $valid
$bad=@($bad|Where-Object {-not ($_.type -eq 'raw_input' -and $_.gesture -eq 1 -and ($_.button_flags -band 2))})
$bad=Renumber-TakeoverFixture $bad
$result=Test-TakeoverOwnedRecords $bad -AllowSynthetic
Check-Takeover ($result.MechanicalMove -ceq 'PASS' -and $result.CancelMove -ceq 'UNKNOWN' -and $result.Architecture -ceq 'UNRESOLVED') 'missing raw UP cannot reject mechanically successful cancel or claim complete PASS'
$bad=Copy-TakeoverFacts $valid
($bad|Where-Object {$_.type -eq 'sample' -and $_.gesture -eq 1 -and $_.index -eq 3}).positioning[0]++
$result=Test-TakeoverOwnedRecords $bad -AllowSynthetic
Check-Takeover ($result.MechanicalMove -ceq 'FAIL' -and $result.CancelMove -ceq 'FAIL' -and $result.Architecture -ceq 'REJECTED_AT_CANCEL_STAGE') 'actual postcancel positioning change rejects cancellation'
$bad=Copy-TakeoverFacts $valid
($bad|Where-Object {$_.type -eq 'sample' -and $_.gesture -eq 1 -and $_.index -eq 3}).visible[0]++
$result=Test-TakeoverOwnedRecords $bad -AllowSynthetic
Check-Takeover ($result.MechanicalMove -ceq 'PASS' -and $result.CancelMove -ceq 'UNKNOWN' -and $result.Architecture -ceq 'UNRESOLVED') 'visible-only difference is not a native positioning reassertion'
$bad=Copy-TakeoverFacts $valid
$continued=$bad|Where-Object {$_.type -eq 'sample' -and $_.gesture -eq 1 -and $_.index -eq 3}
$continued.type='DRAG';$continued|Add-Member -NotePropertyName event -NotePropertyValue 'WM_MOVING';$continued|Add-Member -NotePropertyName edge -NotePropertyValue 9
$result=Test-TakeoverOwnedRecords $bad -AllowSynthetic
Check-Takeover ($result.MechanicalMove -ceq 'FAIL' -and $result.CancelMove -ceq 'FAIL' -and $result.Architecture -ceq 'REJECTED_AT_CANCEL_STAGE') 'actual native DRAG after confirmed cancellation rejects architecture'

$absoluteFixture=New-TakeoverFixture -Absolute -LaggedSnapshot
$result=Test-TakeoverOwnedRecords $absoluteFixture -AllowSynthetic
Check-Takeover ($result.Result -ceq 'CAPTURED' -and $result.RawBackground -ceq 'PASS' -and $result.CancelMove -ceq 'PASS' -and $result.CancelResize -ceq 'PASS' -and $result.Architecture -ceq 'UNRESOLVED') 'actual absolute tagged delivery accepts diagnostic cursor lag and async UP lag, not takeover VALID'
foreach($kind in @('normalized-point','mouse-flags','raw-normalized','raw-tag','raw-flags','stale-watermark','missing-watermark','workarea-screen','no-later-up-fence','forged-snapshot','forged-delivery','wrong-wait-api')){
    $bad=Copy-TakeoverFacts $absoluteFixture
    switch($kind){
        'normalized-point' {($bad|Where-Object type -eq input|Select-Object -First 1).normalized_dx++}
        'mouse-flags' {($bad|Where-Object type -eq input|Select-Object -First 1).actual_mouse_flags=1}
        'raw-normalized' {($bad|Where-Object type -eq raw_input|Select-Object -First 1).dx++}
        'raw-tag' {($bad|Where-Object type -eq raw_input|Select-Object -First 1).test_tag_matches=$false}
        'raw-flags' {($bad|Where-Object type -eq raw_input|Select-Object -First 1).raw_flags=0}
        'stale-watermark' {($bad|Where-Object type -eq input|Select-Object -First 1).receiver_watermark=2}
        'missing-watermark' {($bad|Where-Object type -eq input|Select-Object -First 1).PSObject.Properties.Remove('receiver_watermark')}
        'workarea-screen' {($bad|Where-Object type -eq input|Select-Object -First 1).virtual_screen=@(0,0,1920,1080)}
        'no-later-up-fence' {$up=$bad|Where-Object {$_.type -eq 'input' -and $_.gesture -eq 0 -and $_.flags -eq 4};$done=$bad|Where-Object type -eq raw_preflight_complete;($bad|Where-Object {$_.type -eq 'input_fence' -and $_.sequence -gt $up.sequence -and $_.sequence -lt $done.sequence}).left_down=$true}
        'forged-snapshot' {($bad|Where-Object type -eq raw_motion_correlation|Select-Object -First 1).snapshot_cursor_matches=$true}
        'forged-delivery' {($bad|Where-Object type -eq raw_motion_correlation|Select-Object -First 1).delivered=$false}
        'wrong-wait-api' {($bad|Where-Object type -eq raw_wait_begin|Select-Object -First 1).input_start_qpc--}
    }
    $caught=$false;try {$null=Test-TakeoverOwnedRecords $bad -AllowSynthetic}catch{$caught=$true}
    Check-Takeover $caught "absolute receipt reject $kind"
}

$pendingFixture=New-TakeoverFixture -Absolute -LaggedSnapshot -Pending
$result=Test-TakeoverOwnedRecords $pendingFixture -AllowSynthetic
Check-Takeover ($result.Result -ceq 'CAPTURED' -and $result.CancelMove -ceq 'PASS' -and $result.CancelResize -ceq 'PASS' -and $result.Architecture -ceq 'UNRESOLVED') 'single planned third MOVE pending-mode proof accepts cancellation only'
foreach($kind in @('foreign-pending','pending-without-return','sample4-pending','pending-destination-mismatch')){
    $bad=Copy-TakeoverFacts $pendingFixture
    switch($kind){
        'foreign-pending' {($bad|Where-Object {$_.type -eq 'input_fence' -and $_.cancel_pending}|Select-Object -First 1).move_size_hwnd=999}
        'pending-without-return' {($bad|Where-Object {$_.type -eq 'cancel_return' -and $_.gesture -eq 1}).type='LEGACY_MOVE'}
        'sample4-pending' {$confirmed=$bad|Where-Object {$_.type -eq 'cancel_confirmed' -and $_.gesture -eq 1};($bad|Where-Object {$_.type -eq 'input_fence' -and $_.gesture -eq 1 -and $_.sequence -gt $confirmed.sequence}|Select-Object -First 1).cancel_pending=$true}
        'pending-destination-mismatch' {($bad|Where-Object {$_.type -eq 'destination_fence' -and $_.cancel_pending}|Select-Object -First 1).cancel_pending=$false}
    }
    $caught=$false;try {$null=Test-TakeoverOwnedRecords $bad -AllowSynthetic}catch{$caught=$true};Check-Takeover $caught "reject pending authority $kind"
}
$bad=Copy-TakeoverFacts $pendingFixture
$continued=$bad|Where-Object {$_.type -eq 'cancel_exit_wait' -and $_.gesture -eq 1}
$continued.type='DRAG';$continued|Add-Member -NotePropertyName event -NotePropertyValue 'WM_MOVING';$continued|Add-Member -NotePropertyName edge -NotePropertyValue 9
$sample3=$bad|Where-Object {$_.type -eq 'sample' -and $_.gesture -eq 1 -and $_.index -eq 3};$continued|Add-Member -NotePropertyName cursor -NotePropertyValue $sample3.cursor
$result=Test-TakeoverOwnedRecords $bad -AllowSynthetic
Check-Takeover ($result.CancelMove -ceq 'FAIL' -and $result.Architecture -ceq 'REJECTED_AT_CANCEL_STAGE') 'real planned-step DRAG after cancel return before confirmation is a cancellation counterexample'
$timeoutFixture=New-TakeoverFixture -Absolute -LaggedSnapshot -Pending -ExitTimeout
$result=Test-TakeoverOwnedRecords $timeoutFixture -AllowSynthetic
Check-Takeover ($result.Result -ceq 'BLOCKED' -and $result.RawBackground -ceq 'PASS' -and $result.CancelMove -ceq 'UNKNOWN' -and $result.CancelResize -ceq 'UNKNOWN' -and $result.Architecture -ceq 'UNRESOLVED' -and -not $result.PendingButton) 'timeout and own pending mode alone remain UNKNOWN; cleanup UP is not raw acceptance'

function New-TakeoverLocalResetFixture {
    $rows=Copy-TakeoverFacts (New-TakeoverFixture -Absolute -LaggedSnapshot -Pending)
    $rows[0]|Add-Member -NotePropertyName local_activation_policy -NotePropertyValue 'own_background_reset_v1'
    $attempt=$rows|Where-Object type -eq foreground_attempt;$attempt.set_foreground_success=$false;$attempt.foreground=500
    $owned=$rows|Where-Object type -eq owned;$boot=$rows|Where-Object type -eq foreground_bootstrap
    $point=@(320,300)
    $parts=[Collections.Generic.List[object]]::new()
    $clock=$owned.qpc+1
    $prep=@{type='local_activation_preparation';target=300;guard=301;source_pid=100;source_tid=101;attempted=$true;authorized=$true;before_active=300;before_focus=300;after_active=0;after_focus=0;foreground_before=500;foreground_after=500;gui_query_succeeded=$true;gui_flags=0;capture_hwnd=0;menu_owner_hwnd=0;move_size_hwnd=0;buttons_modifiers_clear=$true;global_unchanged=$true;local_cleared=$true;success=$true;own_identity_before=$true;desktop_ready=$true;own_identity_after=$true;focus_return=300;focus_error=0;active_called=$true;active_return=300;active_error=0}
    $items=@($prep,@{type='activation_visibility';target=300;enabled=$true;success=$true;flags=83;topmost_style=$true;visible=$true})
    foreach($phase in @('move','down','up')){
        $f=@{type='activation_fence';phase=$phase;target=300;foreground=$(if($phase -eq 'up'){300}else{500});cursor=$(if($phase -eq 'move'){@(10,10)}else{$point});activation_point=$point;own_identity=$true;desktop_ready=$true;same_integrity=$true;visible=$true;temporary_topmost=$true;window_from_point_root=300;window_from_point_root_matches=$true;hit_test=1;gui_query_succeeded=$true;foreground_snapshot_stable=$true;gui_flags=0;capture_hwnd=0;menu_owner_hwnd=0;move_size_hwnd=0;foreign_capture_clear=$true;modifiers_clear=$true;button_state_matches=$true;left_down=($phase -eq 'up');input_tag=0x50424D41}
        $items+=,$f
        $items+=,@{type='activation_input';phase=$phase;flags=$(if($phase -eq 'move'){1}elseif($phase -eq 'down'){2}else{4});point=$point;sent=1;error=0;input_tag=0x50424D41}
        if($phase -eq 'down'){
            $items+=,@{type='activation_event';message='WM_ACTIVATE';activation_code=2;activation_epoch=1;target=300;foreground_matches=$true}
            $items+=,@{type='activation_button';message='WM_LBUTTONDOWN';target=300}
        }
        if($phase -eq 'up'){$items+=,@{type='activation_button';message='WM_LBUTTONUP';target=300}}
    }
    $items+=,@{type='activation_visibility';target=300;enabled=$false;success=$true;flags=19;topmost_style=$false;visible=$true}
    foreach($item in $items){
        $clock+=3;$record=[ordered]@{schema='r1c4b-takeover-owned/v1';sequence=0;gesture=0;qpc=$clock}
        foreach($name in $item.Keys){$record[$name]=$item[$name]}
        if($item.type -eq 'activation_input'){$record.injection_start_qpc=$clock-1;$record.injection_return_qpc=$clock}
        $parts.Add([pscustomobject]$record)
    }
    $boot.set_foreground_success=$false;$boot.activation_click_required=$true;$boot.temporary_topmost=$true;$boot.activation_point=$point;$boot.window_from_point_root_matches=$true;$boot.hit_test=1;$boot.foreign_capture_clear=$true;$boot.sendinput_move_success=$true;$boot.sendinput_down_success=$true;$boot.sendinput_up_success=$true;$boot.wm_activate_seen=$true;$boot.wm_setfocus_seen=$false;$boot.activation_event_seen=$true
    $first=$rows|Where-Object type -eq input_fence|Select-Object -First 1;$first.cursor=$point;$first.expected_cursor=$point
    $show=[pscustomobject]@{schema='r1c4b-takeover-owned/v1';sequence=0;gesture=0;qpc=$attempt.qpc-1;type='show_window';requested_show=4;visible=$true;startup_flags=0;startup_show=0}
    $combined=[Collections.Generic.List[object]]::new()
    foreach($row in $rows){if($row.type -eq 'foreground_attempt'){$combined.Add($show)};if($row.type -eq 'foreground_bootstrap'){foreach($part in $parts){$combined.Add($part)}};$combined.Add($row)}
    return ,(Renumber-TakeoverFixture @($combined.ToArray()))
}
$localFixture=New-TakeoverLocalResetFixture
$result=Test-TakeoverOwnedRecords $localFixture -AllowSynthetic
Check-Takeover ($result.Result -ceq 'CAPTURED' -and $result.Architecture -ceq 'UNRESOLVED') 'own UI local reset then genuine fresh activation event preserves all gates'
foreach($kind in @('foreign-local-hwnd','wrong-ui-thread','global-changed','partial-local-clear','missing-identity','missing-active-call','activating-show','no-new-activation-event')){
    $bad=Copy-TakeoverFacts $localFixture;$prep=$bad|Where-Object type -eq local_activation_preparation
    switch($kind){
        'foreign-local-hwnd' {$prep.before_focus=999}
        'wrong-ui-thread' {$prep.source_tid=999}
        'global-changed' {$prep.foreground_after=501}
        'partial-local-clear' {$prep.after_active=300}
        'missing-identity' {$prep.own_identity_after=$false}
        'missing-active-call' {$prep.active_called=$false}
        'activating-show' {($bad|Where-Object type -eq show_window).requested_show=5}
        'no-new-activation-event' {($bad|Where-Object type -eq activation_event).activation_epoch=0}
    }
    $caught=$false;try {$null=Test-TakeoverOwnedRecords $bad -AllowSynthetic}catch{$caught=$true};Check-Takeover $caught "reject local bootstrap proof $kind"
}
$bad=Copy-TakeoverFacts $localFixture;$prep=$bad|Where-Object type -eq local_activation_preparation
$prep.before_active=0;$prep.before_focus=0;$prep.attempted=$false;$prep.active_called=$false;$prep.focus_return=0;$prep.active_return=0
$result=Test-TakeoverOwnedRecords $bad -AllowSynthetic
Check-Takeover ($result.Result -ceq 'CAPTURED') 'already-clear local state requires no native clear calls, still fresh click event'

$boundaryFixture=Copy-TakeoverFacts $pendingFixture
foreach($g in 1,2){
    $boundary=$boundaryFixture|Where-Object {$_.type -eq 'cancel_return' -and $_.gesture -eq $g}
    $captured=$boundaryFixture|Where-Object {$_.type -eq 'cancel_confirmed' -and $_.gesture -eq $g}
    foreach($name in @('positioning','visible','positioning_error','visible_hresult')){$boundary|Add-Member -NotePropertyName $name -NotePropertyValue $captured.$name}
}
$result=Test-TakeoverOwnedRecords $boundaryFixture -AllowSynthetic
Check-Takeover ($result.CancelMove -ceq 'PASS' -and $result.CancelResize -ceq 'PASS') 'pre-stimulus cancel-return geometry remains stable through all later samples'
$bad=Copy-TakeoverFacts $boundaryFixture
($bad|Where-Object {$_.type -eq 'cancel_confirmed' -and $_.gesture -eq 1}).positioning[0]++
$result=Test-TakeoverOwnedRecords $bad -AllowSynthetic
Check-Takeover ($result.CancelMove -ceq 'FAIL' -and $result.Architecture -ceq 'REJECTED_AT_CANCEL_STAGE') 'confirmation cannot silently reset a changed cancel-return positioning baseline'
$bad=Copy-TakeoverFacts $boundaryFixture
$changed=$bad|Where-Object {$_.type -eq 'cancel_exit_wait' -and $_.gesture -eq 1}
$changed.type='POSITION_CHANGED';$baseline=$bad|Where-Object {$_.type -eq 'cancel_return' -and $_.gesture -eq 1}
foreach($name in @('positioning','visible','positioning_error','visible_hresult')){$changed|Add-Member -NotePropertyName $name -NotePropertyValue $baseline.$name}
$changed.positioning=@(($baseline.positioning[0]+1),$baseline.positioning[1],($baseline.positioning[2]+1),$baseline.positioning[3])
$result=Test-TakeoverOwnedRecords $bad -AllowSynthetic
Check-Takeover ($result.CancelMove -ceq 'FAIL' -and $result.Architecture -ceq 'REJECTED_AT_CANCEL_STAGE') 'actual new-stimulus positioning change before confirmation rejects cancellation'

function Convert-TakeoverFixtureToGlobalV2($Rows){
    $rows=Copy-TakeoverFacts $Rows
    $rows[0].foreground_contract='verified_global_foreground_v2'
    $rows[0].PSObject.Properties.Remove('local_activation_policy')
    $rows=@($rows|Where-Object {$_.type -cne 'local_activation_preparation' -and $_.type -cne 'activation_event'})
    $attempt=$rows|Where-Object type -eq foreground_attempt
    # Non-null local queue state is deliberately not an authority condition.
    $attempt|Add-Member -NotePropertyName source_thread_local_active -NotePropertyValue 300 -Force
    $attempt|Add-Member -NotePropertyName source_thread_local_focus -NotePropertyValue 301 -Force
    $boot=$rows|Where-Object type -eq foreground_bootstrap
    $boot.wm_activate_seen=$false;$boot.wm_setfocus_seen=$false;$boot.activation_event_seen=$false
    $ready=[pscustomobject]@{schema='r1c4b-takeover-owned/v1';sequence=0;gesture=0;qpc=$boot.qpc-1;type='foreground_ready';target=300;source_pid=100;source_tid=101;own_identity=$true;desktop_ready=$true;visible=$true;foreground=300;foreground_pid=100;foreground_tid=101;foreground_snapshot_stable=$true;gui_query_succeeded=$true;gui_flags=0;capture_hwnd=0;menu_owner_hwnd=0;move_size_hwnd=0;buttons_modifiers_clear=$true;topmost_now=$false;source_thread_local_active=300;source_thread_local_focus=301;success=$true}
    $complete=@($rows|Where-Object type -eq raw_preflight_complete);$preOutcome=$null
    if($complete.Count){
        $preUp=$rows|Where-Object {$_.type -eq 'input' -and $_.gesture -eq 0 -and $_.flags -eq 4}
        $preOutcome=[pscustomobject]@{schema='r1c4b-takeover-owned/v1';sequence=0;gesture=0;qpc=$complete[0].qpc-1;type='raw_preflight_outcome';attempted=$true;actual_move_verified=$true;actual_held_move_verified=$true;actual_up_verified=$true;input_delivery_verified=$true;observation_completed=$true;receiver_healthy=$true;up_wait_result=0;up_wait_started_qpc=$preUp.injection_return_qpc;up_wait_finished_qpc=$preUp.injection_return_qpc+1;up_timeout_ms=2000;raw_up_observed=$true}
    }
    foreach($wait in @($rows|Where-Object type -eq cancel_exit_wait)){
        $path=$rows|Where-Object {$_.type -eq 'path' -and $_.gesture -eq $wait.gesture}
        $third=@(($path.start[0]+($path.end[0]-$path.start[0])*3/20),($path.start[1]+($path.end[1]-$path.start[1])*3/20))
        foreach($name in @('desktop_ready','visible','buttons_modifiers_clear','cursor_success','cursor_matches_expected','receiver_healthy')){$wait|Add-Member -NotePropertyName $name -NotePropertyValue $true -Force}
        $wait|Add-Member -NotePropertyName menu_owner_hwnd -NotePropertyValue 0 -Force
        $wait|Add-Member -NotePropertyName cursor -NotePropertyValue $third -Force
        $wait|Add-Member -NotePropertyName expected_cursor -NotePropertyValue $third -Force
    }
    $combined=[Collections.Generic.List[object]]::new()
    foreach($row in $rows){if($row.type -eq 'foreground_bootstrap'){$combined.Add($ready)};if($row.type -eq 'raw_preflight_complete'){$combined.Add($preOutcome)};$combined.Add($row)}
    return ,(Renumber-TakeoverFixture @($combined.ToArray()))
}
$v2Direct=Convert-TakeoverFixtureToGlobalV2 (New-TakeoverFixture -Absolute -LaggedSnapshot -Pending)
$result=Test-TakeoverOwnedRecords $v2Direct -AllowSynthetic
Check-Takeover ($result.RawBackground -ceq 'PASS' -and $result.CancelMove -ceq 'PASS' -and $result.CancelResize -ceq 'PASS' -and $result.Architecture -ceq 'UNRESOLVED') 'v2 local active/focus non-null plus background INPUTSINK is PASS, not takeover acceptance'
$v2Incomplete=Convert-TakeoverFixtureToGlobalV2 (New-TakeoverFixture -RawBlocked -Absolute)
$result=Test-TakeoverOwnedRecords $v2Incomplete -AllowSynthetic
Check-Takeover ($result.Result -ceq 'BLOCKED' -and $result.RawBackground -ceq 'BLOCKED' -and $result.CancelMove -ceq 'UNKNOWN' -and $result.Architecture -ceq 'UNRESOLVED') 'v2 early raw observation block before a complete four-stimulus preflight is not Raw FAIL or cancel evidence'
$v2Click=Convert-TakeoverFixtureToGlobalV2 (New-TakeoverLocalResetFixture)
$result=Test-TakeoverOwnedRecords $v2Click -AllowSynthetic
Check-Takeover ($result.RawBackground -ceq 'PASS' -and $result.CancelMove -ceq 'PASS' -and $result.Result -ceq 'CAPTURED') 'v2 exact intended down/up receipt and fresh global proof succeed without activation callbacks or local reset'
foreach($kind in @('wrong-global','wrong-foreground-pid','wrong-foreground-tid','stale-global','foreign-capture','foreign-menu','foreign-movesize','desktop','visible','dirty-input','topmost','diagnostic-type','diagnostic-negative','missing-ready','removed-local-policy','missing-down-receipt','missing-up-receipt','failed-restore')){
    $bad=Copy-TakeoverFacts $v2Click;$ready=$bad|Where-Object type -eq foreground_ready
    switch($kind){
        'wrong-global' {$ready.foreground=999}
        'wrong-foreground-pid' {$ready.foreground_pid=200}
        'wrong-foreground-tid' {$ready.foreground_tid=999}
        'stale-global' {$ready.foreground_snapshot_stable=$false}
        'foreign-capture' {$ready.capture_hwnd=999}
        'foreign-menu' {$ready.menu_owner_hwnd=999}
        'foreign-movesize' {$ready.move_size_hwnd=999}
        'desktop' {$ready.desktop_ready=$false}
        'visible' {$ready.visible=$false}
        'dirty-input' {$ready.buttons_modifiers_clear=$false}
        'topmost' {$ready.topmost_now=$true}
        'diagnostic-type' {$ready.source_thread_local_focus='300'}
        'diagnostic-negative' {$ready.source_thread_local_active=-1}
        'missing-ready' {$bad=@($bad|Where-Object type -ne foreground_ready);$bad=Renumber-TakeoverFixture $bad}
        'removed-local-policy' {$bad[0]|Add-Member -NotePropertyName local_activation_policy -NotePropertyValue 'own_background_reset_v1'}
        'missing-down-receipt' {($bad|Where-Object {$_.type -eq 'activation_button' -and $_.message -eq 'WM_LBUTTONDOWN'}).message='WM_LBUTTONUP'}
        'missing-up-receipt' {($bad|Where-Object {$_.type -eq 'activation_button' -and $_.message -eq 'WM_LBUTTONUP'}).message='WM_LBUTTONDOWN'}
        'failed-restore' {($bad|Where-Object {$_.type -eq 'activation_visibility' -and -not $_.enabled}).success=$false}
    }
    $caught=$false;try{$null=Test-TakeoverOwnedRecords $bad -AllowSynthetic}catch{$caught=$true}
    Check-Takeover $caught "v2 strict global/click guard rejects $kind"
}
$bad=Copy-TakeoverFacts $v2Direct
($bad|Where-Object type -eq foreground_ready).source_thread_local_active=999
($bad|Where-Object type -eq foreground_ready).source_thread_local_focus=0
$result=Test-TakeoverOwnedRecords $bad -AllowSynthetic
Check-Takeover ($result.RawBackground -ceq 'PASS') 'bounded arbitrary local diagnostic HWND/null does not confer or deny global authority'
foreach($kind in @('receiver-equals-foreground','foreground-RIM-INPUT')){
    $bad=Copy-TakeoverFacts $v2Direct
    if($kind -eq 'receiver-equals-foreground'){($bad|Where-Object type -eq raw_input|Select-Object -First 1).foreground_pid=200}
    else{($bad|Where-Object type -eq raw_input|Select-Object -First 1).input_code=0}
    $caught=$false;try{$null=Test-TakeoverOwnedRecords $bad -AllowSynthetic}catch{$caught=$true}
    Check-Takeover $caught "v2 no background proof $kind"
}
function New-TakeoverV2RawNegativeFixture([switch]$NoMovement){
    $rows=Copy-TakeoverFacts $v2Direct;$out=$rows|Where-Object type -eq raw_preflight_outcome
    $rows=@($rows|Where-Object {$_.sequence -le $out.sequence -or $_.type -cin @('receiver_shutdown','shutdown')})
    if($NoMovement){$rows=@($rows|Where-Object {$_.type -cnotin @('raw_wait_begin','raw_wait_result','raw_motion_correlation') -and -not ($_.type -eq 'raw_input' -and $_.cursor_sampled)})}
    else{
        $rows=@($rows|Where-Object {-not ($_.type -eq 'raw_input' -and ($_.button_flags -band 2))})
        $out.raw_up_observed=$false;$out.up_wait_result=258;$out.up_wait_finished_qpc=$out.up_wait_started_qpc+20000000
        foreach($row in $rows){if($row.sequence -gt ($rows|Where-Object {$_.type -eq 'input' -and $_.flags -eq 4}).sequence){$row.qpc+=20000000;if($row.type -eq 'receiver_shutdown'){$row.receiver_qpc+=20000000}}}
    }
    $end=$rows[-1];$end.result='BLOCKED';$end.cursor_restored=$false
    $why=[pscustomobject]@{schema='r1c4b-takeover-owned/v1';sequence=0;gesture=0;qpc=$out.qpc+1;type='blocked';reason=$(if($NoMovement){'RAW_PREFLIGHT_MISSING_MOVEMENT'}else{'RAW_PREFLIGHT_MISSING_UP'})}
    $combined=[Collections.Generic.List[object]]::new()
    foreach($row in $rows){$combined.Add($row);if($row.type -eq 'raw_preflight_outcome'){$combined.Add($why)}}
    $rows=Renumber-TakeoverFixture @($combined.ToArray())
    $serial=0
    foreach($row in $rows){if($row.type -cin @('receiver','raw_input')){$serial=$row.receiver_sequence};if($row.type -eq 'input'){$row.receiver_watermark=$serial}}
    return ,$rows
}
foreach($noMovement in @($false,$true)){
    $negative=New-TakeoverV2RawNegativeFixture -NoMovement:$noMovement
    $result=Test-TakeoverOwnedRecords $negative -AllowSynthetic
    Check-Takeover ($result.Result -ceq 'FAIL' -and $result.RawBackground -ceq 'FAIL' -and $result.CancelMove -ceq 'UNKNOWN' -and $result.CancelResize -ceq 'UNKNOWN' -and $result.Architecture -ceq 'UNRESOLVED' -and $result.GeometryWrites -eq 0) 'complete observed raw stimulus missing movement/up is Raw FAIL, not empirical cancel failure'
}
$v2Timeout=Convert-TakeoverFixtureToGlobalV2 (New-TakeoverFixture -Absolute -LaggedSnapshot -Pending -ExitTimeout)
$result=Test-TakeoverOwnedRecords $v2Timeout -AllowSynthetic
Check-Takeover ($result.Result -ceq 'FAIL' -and $result.RawBackground -ceq 'PASS' -and $result.CancelMove -ceq 'FAIL' -and $result.CancelResize -ceq 'UNKNOWN' -and $result.Architecture -ceq 'REJECTED_AT_CANCEL_STAGE' -and $result.Facts[0].VerifiedCancellationDeadlineFailure) 'Fix A complete deadline with actual third MOVE/raw receipt and exact still-native source is one reliable negative'
foreach($field in @('desktop_ready','visible','buttons_modifiers_clear','receiver_healthy','source_identity','left_down','gui_query_succeeded')){
    $bad=Copy-TakeoverFacts $v2Timeout;($bad|Where-Object type -eq cancel_exit_wait).$field=$false
    $result=Test-TakeoverOwnedRecords $bad -AllowSynthetic
    Check-Takeover ($result.CancelMove -ceq 'UNKNOWN' -and $result.Architecture -ceq 'UNRESOLVED') "deadline missing context $field is not an architecture counterexample"
}
$bad=Copy-TakeoverFacts $v2Timeout
$wait=$bad|Where-Object type -eq cancel_exit_wait;$wait.gui_flags=0;$wait.move_size_hwnd=0
$result=Test-TakeoverOwnedRecords $bad -AllowSynthetic
Check-Takeover ($result.CancelMove -ceq 'UNKNOWN' -and $result.Architecture -ceq 'UNRESOLVED') 'bare timeout without positive retained native mode remains unknown'

Write-Host "takeover-owned synthetic_only=true checks=$checks PASS"
