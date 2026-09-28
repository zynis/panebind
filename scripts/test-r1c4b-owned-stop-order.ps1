[CmdletBinding()]
param([string]$SourceRevision)

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$relative = 'src/platform/windows/operations/owned_gesture_overlay_probe.cpp'
if ($SourceRevision) {
    if ($SourceRevision -notmatch '^[0-9a-fA-F]{40}$') { throw 'SourceRevision must be an exact SHA.' }
    $spec = '{0}:{1}' -f $SourceRevision, $relative
    $source = (& git -C $repo show $spec) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw "Cannot read $spec" }
} else {
    $source = [IO.File]::ReadAllText((Join-Path $repo $relative), [Text.Encoding]::UTF8)
}

function Section([string]$start, [string]$end) {
    $first = $source.IndexOf($start, [StringComparison]::Ordinal)
    $last = $source.IndexOf($end, $first + $start.Length, [StringComparison]::Ordinal)
    if ($first -lt 0 -or $last -le $first) { throw "Missing source section: $start" }
    return $source.Substring($first, $last - $first)
}
function Ordered([string]$label, [string]$body, [string[]]$needles) {
    $at = 0
    foreach ($needle in $needles) {
        $found = $body.IndexOf($needle, $at, [StringComparison]::Ordinal)
        if ($found -lt 0) { throw "${label}: missing or out of order: $needle" }
        $at = $found + $needle.Length
    }
}

$owner = Section 'void overlay_owner() noexcept {' 'void overlay_watchdog() noexcept {'
$tail = $owner.LastIndexOf('const HWND overlay = state.overlay.load();', [StringComparison]::Ordinal)
if ($tail -lt 0) { throw 'Missing overlay destruction phase.' }
Ordered 'owner teardown' $owner.Substring($tail) @(
    'DestroyWindow(overlay) && !IsWindow(overlay)',
    'if (destroyed) SetEvent(state.overlay_gone);',
    'MsgWaitForMultipleObjectsEx(1, observation, remaining,',
    'DispatchMessageW(&message);',
    'log("receiver_observation_end"',
    'state.registration_removed = remove_raw_registration();',
    'state.receiver_destroyed =',
    'SetEvent(state.receiver_gone);'
)
$driver = Section 'int drive() noexcept {' '} // namespace'
Ordered 'stop driver' $driver @(
    'wait_event(state.overlay_gone, 2000, "keyboard_stop_overlay_gone")',
    'release_test_owned_left("keyboard_stop_after_overlay")'
)
$receiver = Section 'LRESULT CALLBACK receiver_procedure(' 'BOOL WINAPI console_control('
if (-not $receiver.Contains('SetEvent(state.raw_up);') -or
    ([regex]::Matches($source, 'SetEvent\(state\.raw_up\);')).Count -ne 1) {
    throw 'Raw UP must be signaled only by receiver_procedure.'
}
Ordered 'final teardown' $source @(
    'wait_event(state.receiver_gone, 7000, "final_receiver_gone")',
    'if (overlay_thread.joinable()) overlay_thread.join();',
    'const bool input_counts = state.raw_downs == 1 && state.raw_ups == 1'
)
Write-Output 'PASS: stop overlay/receiver order source contract; no Windows input was generated.'
