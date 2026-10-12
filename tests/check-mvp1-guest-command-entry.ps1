$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
function Read-ActualAst([string]$Relative) {
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $repo $Relative),
        [ref]$tokens,[ref]$errors)
    if($errors.Count) { throw "actual_script_parse_error:$Relative/$($errors[0].Message)" }
    return $ast
}
function Import-ActualFunction($Ast,[string]$Name) {
    $node=$Ast.Find({param($item)
        $item -is [Management.Automation.Language.FunctionDefinitionAst] -and
        $item.Name -ceq $Name
    },$true)
    if(-not $node) { throw "actual_function_missing:$Name" }
    return $node.Extent.Text
}
$driver=Read-ActualAst 'scripts\r1c4b-mvp1-explorer-guest-driver.ps1'
$runner=Read-ActualAst 'scripts\r1c4b-mvp1-shield-sandbox-guest.ps1'
$prepare=Read-ActualAst 'scripts\prepare-r1c4b-mvp1-shield-sandbox.ps1'
. ([scriptblock]::Create((Import-ActualFunction $driver 'Get-GuestCommandBytes')))
. ([scriptblock]::Create((Import-ActualFunction $driver 'Test-GuestPipeStartup')))
. ([scriptblock]::Create((Import-ActualFunction $driver 'Test-DriverModeManifest')))
. ([scriptblock]::Create((Import-ActualFunction $runner 'Get-ExplorerConfigurations')))
. ([scriptblock]::Create((Import-ActualFunction $runner 'New-GuestDriverStartInfo')))
. ([scriptblock]::Create((Import-ActualFunction $runner 'Test-CommandChannelCleanup')))

foreach($line in @('','Y','S','Q')) {
    $bytes=Get-GuestCommandBytes $line
    if($bytes -isnot [byte[]] -or
        [Text.Encoding]::ASCII.GetString($bytes) -cne ($line+"`n")) {
        throw 'actual_guest_pipe_ascii_line_contract_wrong'
    }
}
foreach($line in @('YES','q',"Y`nQ",'外部命令')) {
    $rejected=$false
    try{$null=Get-GuestCommandBytes $line}catch{$rejected=$true}
    if(-not $rejected){throw 'actual_guest_pipe_invalid_command_accepted'}
}
$clientName='PaneBindMVP1-'+('a'*32)+'-debug'
$pipePath='\\.\pipe\'+$clientName
$startup=[pscustomobject]@{command_channel='guest_named_pipe';pipe_name=$pipePath;
    driver_pid=123;process_has_console=$false}
if(-not(Test-GuestPipeStartup $startup 123 $pipePath)) {
    throw 'actual_pipe_startup_rejects_full_native_path'
}
foreach($name in @($clientName,'\\.\pipe\different')) {
    $startup.pipe_name=$name
    if(Test-GuestPipeStartup $startup 123 $pipePath) {
        throw 'actual_pipe_startup_accepts_bare_or_wrong_native_path'
    }
}
$startup.pipe_name=$pipePath
foreach($value in @($true,$null,'false')) {
    $startup.process_has_console=$value
    if(Test-GuestPipeStartup $startup 123 $pipePath) {
        throw 'actual_pipe_startup_accepts_unproven_console_absence'
    }
}
$startup.process_has_console=$false
if(Test-GuestPipeStartup $startup 124 $pipePath) {
    throw 'actual_pipe_startup_accepts_wrong_driver_pid'
}
foreach($mode in @('CommandStopValidation','CommandDisconnectValidation')) {
    $Mode=$mode;$Configuration='debug'
    Test-DriverModeManifest ([pscustomobject]@{
        explorer_guest_mode=$mode;independent_review_configuration='release'
    })
    if((@(Get-ExplorerConfigurations $mode)-join ',') -cne 'debug') {
        throw 'actual_command_validation_configuration_wrong'
    }
    $start=New-GuestDriverStartInfo 'C:\PaneBindMVP1\Input\explorer-driver.ps1' 'debug' `
        ('a'*32) $mode
    if(-not $start.CreateNoWindow -or $start.UseShellExecute -or
        -not $start.RedirectStandardOutput -or -not $start.RedirectStandardError -or
        $start.Arguments -notmatch '-NonInteractive' -or
        $start.Arguments -notmatch ('-Mode '+$mode) -or
        $start.Arguments -match 'WindowStyle') {
        throw 'actual_driver_start_still_depends_on_console'
    }
}
# Compile the ACTUAL embedded test driver, but invoke only pure STARTUPINFO
# selection. This is not platform input or a live console/Explorer test.
$text=[IO.File]::ReadAllText((Join-Path $repo 'scripts\r1c4b-mvp1-explorer-guest-driver.ps1'))
$match=[regex]::Match($text,"(?s)Add-Type -TypeDefinition @'\r?\n(.*?)\r?\n'@")
if(-not $match.Success){throw 'actual_driver_csharp_missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
$pipeStartup=[PaneBindMvpGuestInput]::ProductStartup($true)
$consoleStartup=[PaneBindMvpGuestInput]::ProductStartup($false)
if($pipeStartup.flags -ne 0 -or
    [PaneBindMvpGuestInput]::ProductCreationFlags($true) -ne 0x08000000 -or
    $consoleStartup.flags -ne 1 -or $consoleStartup.showWindow -ne 5 -or
    [PaneBindMvpGuestInput]::ProductCreationFlags($false) -ne 0x00000010) {
    throw 'actual_product_creation_not_no_console_or_console_regressed'
}
$base=@(
    '{"type":"status","sequence":10}',
    '{"type":"status","sequence":11}',
    '{"type":"gesture_event","event":"resource","reason":"stop_cleanup","sequence":12,"succeeded":true,"overlay_destroyed":true,"hotkey_unregistered":true,"receiver_destroyed":true,"raw_registration_removed":true,"winevent_unhooked":true,"classes_unregistered":true,"association_detached":true}',
    '{"type":"shutdown","sequence":13,"result":"STOPPED","source":"guest_pipe_command","resources_stopped":true,"gesture_events_recorded":true}'
)
$rows=@($base|ForEach-Object{$_|ConvertFrom-Json})
if(-not(Test-CommandChannelCleanup $rows 'CommandStopValidation')) {
    throw 'actual_command_stop_predicate_rejects_complete_receipts'
}
foreach($reason in @('guest_command_disconnected','guest_command_driver_exited')) {
    $disconnected=@($base|ForEach-Object{$_|ConvertFrom-Json})
    $disconnected[3].result='BLOCKED'
    $disconnected[3]|Add-Member -NotePropertyName reason -NotePropertyValue $reason
    if(-not(Test-CommandChannelCleanup $disconnected 'CommandDisconnectValidation')) {
        throw 'actual_command_disconnect_predicate_rejects_complete_abort_cleanup'
    }
    if(Test-CommandChannelCleanup $disconnected 'CommandStopValidation') {
        throw 'actual_command_stop_accepts_aborted_terminal'
    }
}
foreach($field in @('succeeded','overlay_destroyed','hotkey_unregistered',
    'receiver_destroyed','raw_registration_removed','winevent_unhooked',
    'classes_unregistered','association_detached')) {
    foreach($value in @($false,$null,'true')) {
        $bad=@($base|ForEach-Object{$_|ConvertFrom-Json});$bad[2].$field=$value
        if(Test-CommandChannelCleanup $bad 'CommandStopValidation') {
            throw "actual_command_cleanup_accepts_unproven:$field"
        }
    }
}
foreach($field in @('resources_stopped','gesture_events_recorded')) {
    $bad=@($base|ForEach-Object{$_|ConvertFrom-Json});$bad[3].$field=$false
    if(Test-CommandChannelCleanup $bad 'CommandStopValidation') {
        throw "actual_stop_terminal_accepts_failed:$field"
    }
    $bad[3].result='BLOCKED'
    $bad[3]|Add-Member -NotePropertyName reason -NotePropertyValue 'guest_command_driver_exited'
    if(Test-CommandChannelCleanup $bad 'CommandDisconnectValidation') {
        throw "actual_disconnect_terminal_accepts_failed:$field"
    }
}
$bad=@($base|ForEach-Object{$_|ConvertFrom-Json});$bad[3].result='BLOCKED'
$bad[3]|Add-Member -NotePropertyName reason -NotePropertyValue 'unrelated_product_failure'
if(Test-CommandChannelCleanup $bad 'CommandDisconnectValidation') {
    throw 'actual_disconnect_terminal_hides_unrelated_product_failure'
}
$bad=@($base|ForEach-Object{$_|ConvertFrom-Json})
$bad+=('{"type":"gesture_event","event":"writer","sequence":8}'|ConvertFrom-Json)
if((Test-CommandChannelCleanup $bad 'CommandStopValidation') -or
    (Test-CommandChannelCleanup @() 'CommandStopValidation') -or
    (Test-CommandChannelCleanup @($rows[0],$rows[2],$rows[3]) 'CommandStopValidation') -or
    (Test-CommandChannelCleanup ($rows+@($rows[3])) 'CommandStopValidation')) {
    throw 'actual_command_cleanup_accepts_gesture_missing_or_duplicate_receipts'
}
$flow=@($driver.FindAll({param($node)
    $node -is [Management.Automation.Language.CommandAst] -and
    $node.GetCommandName() -ceq 'Invoke-Drag'
},$true)|ForEach-Object{$_.Extent.Text})
$expected=@("Invoke-Drag 0 'plain' -48 -48 'plain_move_candidate'",
    "Invoke-Drag 0 'ctrl' 34 0 'ctrl_move'",
    "Invoke-Drag 1 'plain' 48 -48 'plain_move_candidate'",
    "Invoke-Drag 2 'plain' -48 48 'plain_move_candidate'",
    "Invoke-Drag 1 'resize' 0 28 'native_resize'",
    "Invoke-Drag 0 'plain' -48 -48 'plain_move_candidate'",
    "Invoke-Drag 2 'ctrl' 22 18 'ctrl_move'")
if(($flow-join "`n") -cne ($expected-join "`n")) {
    throw 'actual_post_resize_a_preconditions_or_ctrl_c_flow_changed'
}
if(-not $text.Contains("Assert-ProductGeometry 'after_native_resize' `$true") -or
    -not $text.Contains("Assert-ProductGeometry 'before_guest_commands' `$true") -or
    -not $text.Contains('no_console_driver_creation_not_observed') -or
    -not $text.Contains('actual_product_pipe_entry_not_observed') -or
    -not $prepare.Extent.Text.Contains('explorer_command_channel')) {
    throw 'actual_command_or_resize_state_gate_missing'
}
Write-Output 'actual_guest_command_entry: ASCII command route; no-console creation selectors; actual stop/disconnect cleanup predicate; complete real Resize/A/Ctrl C flow preserved PASS (offline, no native/UI/input)'
