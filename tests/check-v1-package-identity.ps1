[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$repository=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $repository 'scripts/v1-package-identity.ps1')
$checks=0
function Check([bool]$Condition,[string]$Name){if(-not $Condition){throw ('FAIL: '+$Name)};$script:checks++;Write-Output ('PASS: '+$Name)}
function Reject([scriptblock]$Action,[string]$Name){$rejected=$false;try{& $Action|Out-Null}catch{$rejected=$true};Check $rejected $Name}
$before='b60e9c343c29f75007df7bf1833fd5d4ab8d3cf8'
$after='dd603a79aeba64c816e13ba49265db342c14e7e3'
$final='ab6bf9057797e7f85fc4aa234da7b105ce143a4f'
$changes=@(Assert-V1SourceScope $repository $before $after Product)
Check ($changes.Count -eq 7 -and $changes -ccontains 'src/platform/windows/product/v1_guest_fixture.cpp') 'actual b60..dd60 seven-file tool-only history preserves product scope'
Reject {Assert-V1SourceScope $repository $before $after Driver} 'old driver cannot be rebound across actual fixture implementation changes'
Check (@(Assert-V1SourceScope $repository $after $final Driver).Count -eq 3) 'docs-only delivery keeps the actual dd60 driver identity'
foreach($path in @('src/platform/windows/product/product_app.cpp','src/platform/windows/product/input_service.cpp','src/platform/windows/product/guest_path_scope.h','src/platform/windows/product/window_catalog.cpp','src/core/v1/gesture.h','CMakeLists.txt','LICENSE','AGENTS.md')){
    Check (-not (Test-V1ProductToolOnlyPath $path)) ('changed product/build/governance path must reject old product: '+$path)
}
foreach($path in @('docs/reports/V1_ACCEPTANCE.json','scripts/v1-sandbox-guest.ps1','tests/core/v1_input_route_tests.cpp','src/platform/windows/product/v1_guest_settings_cases.h','src/platform/windows/product/guest_path_scope_tests.cpp')){
    Check (Test-V1ProductToolOnlyPath $path) ('tool-only path remains outside product identity: '+$path)
}
# Exact separate-target exemptions, not an operations/Explorer directory rule.
# V1 now links GestureInputShield: only the unlinked owned/flow/test inputs
# remain exempt. The Driver scope is unchanged because it also links Explorer.
foreach($path in @(
    'src/platform/windows/operations/owned_gesture_shield_validation.cpp',
    'src/platform/windows/explorer/explorer_mvp_flow_tests.cpp',
    'scripts/prepare-r1c4b-mvp1-shield-sandbox.ps1',
    'scripts/r1c4b-mvp1-shield-sandbox-guest.ps1')){
    Check (Test-V1ProductToolOnlyPath $path) ('exact unlinked MVP1 input permits unchanged V1 product reuse: '+$path)
}
foreach($path in @(
    'src/platform/windows/operations/gesture_input_shield.h',
    'src/platform/windows/operations/gesture_input_shield.cpp',
    'src/platform/windows/operations/window_translation.cpp',
    'src/platform/windows/operations/window_translation.h',
    'src/platform/windows/operations/window_rect_adjustment.h',
    'src/core/geometry/geometry.h',
    'src/platform/windows/product/input_service.cpp',
    'src/platform/windows/product/guest_capture_scope.h',
    'src/platform/windows/CMakeLists.txt',
    'src/platform/windows/operations/owned_gesture_overlay_probe.cpp',
    'src/platform/windows/operations/gesture_input_shield_other.cpp',
    'scripts/prepare-r1c4b-mvp1-shield-sandbox-other.ps1',
    'scripts/r1c4b-mvp1-shield-sandbox-guest-other.ps1')){
    Check (-not (Test-V1ProductToolOnlyPath $path)) ('product dependency or adjacent unreviewed path still rejects reuse: '+$path)
}
# Static wiring only: these checks neither start an application nor establish
# that its client can receive input in a guest. The real driver must prove that
# native interaction before it starts the application's product geometry cases.
function Get-V1FixtureFunctionText([string]$Source,[string]$Name){
    $functionPrefix='(?m)^(?:\[\[maybe_unused\]\]\s*)?(?:void|bool|Gesture) '
    $start=[regex]::Match($Source,($functionPrefix+[regex]::Escape($Name)+'\s*\('))
    if(-not $start.Success){throw ('Fixture function missing: '+$Name)}
    $rest=$Source.Substring($start.Index+$start.Length)
    $next=[regex]::Match($rest,($functionPrefix+'\w+\s*\('))
    if($next.Success){return $Source.Substring($start.Index,$start.Length+$next.Index)}
    return $Source.Substring($start.Index)
}
function Assert-V1ClientUsesAdmittedSource([string]$Source){
    $client=Get-V1FixtureFunctionText $Source 'matrix_client_action'
    if($client -match '\bmatrix_sources\s*\(' -or
       -not $client.Contains('const auto& source=*identity;') -or
       -not $client.Contains('matrix_client_child(s,source,hit)')){
        throw 'Client interaction must use its admitted source, not requery the whole geometry group.'
    }
}
function Assert-V1NativeClientPreflightWiring([string]$Source){
    $flow=Get-V1FixtureFunctionText $Source 'application_matrix_flow'
    $preflight=[regex]::Match($flow,'matrix_paused_native_client_preflight\s*\(\s*s\s*,\s*kind\s*,\s*preflight_restored\s*\)')
    $product=[regex]::Match($flow,'matrix_five\s*\(\s*s\s*,\s*0\s*,\s*kind\s*\+\s*"\.Move"\s*\)')
    if(-not $preflight.Success -or -not $product.Success -or $preflight.Index -ge $product.Index){
        throw 'Native client preflight must precede application product cases.'
    }
    $productStarted=$flow.IndexOf('product_suite_started=true;',[StringComparison]::Ordinal)
    if($productStarted -le $preflight.Index -or
       -not $flow.Contains('const bool product_gesture_attempted=product_suite_started&&s.left_down_send_qpc!=product_down_before;') -or
       -not $flow.Contains('const std::string category_result=passed?"PASS":stage=="native_client_preflight"?"BLOCKED":target&&target->accepted?"FAIL":"BLOCKED";') -or
       -not $flow.Contains('quote(!product_gesture_attempted?"NOT_RUN":passed?"PASS":"NOT_CONFIRMED")')){
        throw 'A failed native preflight must not become an attempted or passed enhanced application case.'
    }
    $native=Get-V1FixtureFunctionText $Source 'matrix_paused_native_client_preflight'
    if(-not $native.Contains('matrix_client_action(s,label,"native_preflight")') -or
       -not $native.Contains('const bool passed=paused&&client_ready&&restored&&failure.empty()&&!s.left&&!s.control;') -or
       -not $native.Contains('if(!passed)throw std::runtime_error("application_native_client_preflight_failed:"+failure);')){
        throw 'Native client preflight must require the actual shared client action and restored product state.'
    }
    Assert-V1ClientUsesAdmittedSource $Source
    $client=Get-V1FixtureFunctionText $Source 'matrix_client_action'
    if($client -notmatch 'const bool actual\s*=\s*exact_child\s*&&\s*hit_read\s*&&\s*region\s*==\s*HTCLIENT\s*;' -or
       $client -notmatch '(?s)if\s*\(\s*!actual\s*\)\s*\{.*?throw std::runtime_error\("matrix_actual_same_process_client_hit_unavailable"\);'){
        throw 'Client input must reject a non-exact or non-HTCLIENT actual hit.'
    }
    if($client -notmatch '(?s)if\(!target\|\|!target->accepted\|\|!target->identity.*?application_fresh\(s,\*target,h.*?product::Catalog::same\(\*target->identity,\*identity\)' -or
       $client -notmatch '(?s)application_fresh\(s,\*target,h,&before_click,&identity_reason\).*?product::Catalog::same\(\*target->identity,\*before_click\).*?GetForegroundWindow\(\)!=h.*?WindowFromPoint\(point\)!=hit.*?matrix_client_fresh_exact_context_changed_before_click' -or
       -not $client.Contains('s.log.emit("application_client_identity_rejected"') -or
       -not $client.Contains('\"phase\":\"entry\",\"fresh_identity_reason\":"+quote(identity_reason)') -or
       -not $client.Contains('\"phase\":\"before_click\",\"fresh_identity_reason\":"+quote(identity_reason)')){
        throw 'Client input requires the exact authorized application and fresh actual hit before clicking.'
    }
}
function Assert-V1GroupBoundsScopeWiring([string]$Source,[string]$Layouts){
    $entry=Get-V1FixtureFunctionText $Source 'independent_entry_flow'
    $scope=[regex]::Match($entry,'(?s)if\(s\.suite=="GroupBoundsOnly"\)\{(?<body>.*?)return;\s*\}')
    if(-not $scope.Success -or
       [regex]::Matches($scope.Groups['body'].Value,'\bphase\s*\(').Count -ne 2 -or
       -not $scope.Value.Contains('phase("G07_four_grid",owned_four_grid_only);') -or
       -not $scope.Value.Contains('phase("eight_owned_group",owned_eight_group_only);') -or
       -not $scope.Value.Contains('cleanup(s);') -or
       -not $scope.Value.Contains('group_bounds_only_summary') -or
       $scope.Index -ge $entry.IndexOf('phase("settings_GUI",',[StringComparison]::Ordinal) -or
       $scope.Index -ge $entry.IndexOf('phase("owned_finite_I11_component",',[StringComparison]::Ordinal)){
        throw 'GroupBoundsOnly must return after its two real group cases and cleanup, before settings or I11.'
    }
    $drive=Get-V1FixtureFunctionText $Source 'drive'
    if($drive -notmatch '(?s)if\([^\r\n]*s\.suite=="GroupBoundsOnly"[^\r\n]*\)\{\s*independent_entry_flow\(s,'){
        throw 'GroupBoundsOnly must reach the actual independent entry flow.'
    }
    $full=Get-V1FixtureFunctionText $Layouts 'owned_layout_cases'
    $fourOnly=Get-V1FixtureFunctionText $Layouts 'owned_four_grid_only'
    $eightOnly=Get-V1FixtureFunctionText $Layouts 'owned_eight_group_only'
    $four=Get-V1FixtureFunctionText $Layouts 'layout_four_grid_case'
    $eight=Get-V1FixtureFunctionText $Layouts 'layout_eight_case'
    if(-not $full.Contains('g=layout_four_grid_case(set);if(!layout_eight_case(set,g))return;') -or
       -not $fourOnly.Contains('set.create();(void)layout_four_grid_case(set);') -or
       -not $fourOnly.Contains('set.close()') -or
       -not $four.Contains('layout_up(set,g,four);') -or
       -not $four.Contains('layout_up(set,g,{four[0],four[1],four[2],{580,365,940,535}});return g;') -or
       -not $eightOnly.Contains('layout_eight_case(set,std::nullopt)') -or
       -not $eight.Contains('previous?record(s,previous->offset,"gesture_end",previous->generation):std::string{}') -or
       -not $eight.Contains('const bool prior_retired=previous?previous_end.find("\"normal_up\":true")') -or
       $eight -notmatch '(?s)initial_idle=requested&&product::guest::matching_idle_status.*?GetGUIThreadInfo\(set.thread,&gui\)&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner' -or
       -not $eight.Contains('\"prior_generation\":null,\"prior_NormalUp_inferred\":false')){
        throw 'Full and scoped entries must reuse the actual group cases without inventing a prior NormalUp.'
    }
}
function Assert-V1NativeClientsWithoutProductWiring([string]$Source){
    $drive=Get-V1FixtureFunctionText $Source 'drive'
    $branch=[regex]::Match($drive,'(?s)if\(s\.suite=="NativeClientsOnly"\)\{(?<native>.*?)\}else\{')
    if(-not $branch.Success -or -not $branch.Value.Contains('native_clients_only_flow(s);') -or
       $branch.Index+$branch.Length -gt $drive.IndexOf('launch(s,',[StringComparison]::Ordinal) -or
       $branch.Groups['native'].Value -match '\b(?:launch|fixture_rect|read_product|record)\s*\(' -or
       -not $drive.Contains('if(s.suite!="NativeClientsOnly")cleanup(s);')){
        throw 'Native client baseline must bypass the product launch branch, including product stop on failure.'
    }
    $flow=Get-V1FixtureFunctionText $Source 'native_clients_only_flow'
    $ready=Get-V1FixtureFunctionText $Source 'native_clients_ready'
    $absent=Get-V1FixtureFunctionText $Source 'native_clients_product_absent'
    $client=Get-V1FixtureFunctionText $Source 'matrix_client_action'
    $forbidden='\b(?:launch|read_product|record|independent_entry_ready|matrix_input_idle|matrix_layout|matrix_five|down|phase|layout_phase|fixture_rect|SetWindowPos|DeferWindowPos)\s*\('
    if(($flow+$ready) -match $forbidden -or
       -not $flow.Contains('target=provision_application(s,kind);') -or
       -not $flow.Contains('matrix_client_action(s,kind,"without_product")') -or
       -not $absent.Contains('FindWindowW(L"PaneBind.V1.Application",L"PaneBind")') -or
       -not $absent.Contains('!s.process&&!s.product_pid&&!root&&complete&&!matching') -or
       -not $flow.Contains('\"product_function_result\":\"NOT_RUN\"')){
        throw 'Native client baseline requires real product absence and exact native application interaction, not product operations.'
    }
    $button=Get-V1FixtureFunctionText $Source 'button'
    if($client -match '\b(?:matrix_layout|matrix_five|down|phase|layout_phase|fixture_rect|SetWindowPos|DeferWindowPos)\s*\(' -or
       [regex]::Matches($client,'\bread_product\s*\(').Count -ne 1 -or
       -not $client.Contains('without_product?std::size_t{}:read_product(s).size()') -or
       [regex]::Matches($client,'\brecord\s*\(').Count -ne 2 -or
       -not $client.Contains('without_product?native_clients_product_absent(s,label+".client.after_click"):record(s,click_mark,"gesture_start").empty()') -or
       -not $client.Contains('without_product?native_clients_product_absent(s,label+".client.after_semantic"):record(s,click_mark,"gesture_start").empty()') -or
       -not $button.Contains('if(!pressed&&s.left&&s.suite!="NativeClientsOnly")')){
        throw 'Without-product client input must not read a nonexistent product log or use product release receipts.'
    }
}
function Assert-V1FocusedNativeReadinessWiring([string]$Source,[string]$Preflight){
    # Static reachability only. These tests do not run cmd, a browser or input.
    Assert-V1NativeClientsWithoutProductWiring $Source
    $flow=Get-V1FixtureFunctionText $Source 'native_clients_only_flow'
    $ready=Get-V1FixtureFunctionText $Preflight 'guest_native_application_ready'
    $shell=Get-V1FixtureFunctionText $Preflight 'guest_native_shell_control'
    if(-not $Source.Contains('#include "platform/windows/product/v1_guest_application_preflight.h"') -or
       -not $flow.Contains('std::array<std::string,2>{"Terminal","Edge"}') -or
       -not $flow.Contains('if(kind=="Terminal")guest_native_shell_control(s,*target);') -or
       -not $flow.Contains('if(!guest_native_application_ready(s,*target))throw std::runtime_error("native_initial_offline_document_not_ready");') -or
       $flow.IndexOf('guest_native_application_ready(s,*target)',[StringComparison]::Ordinal) -ge
           $flow.IndexOf('matrix_client_action(s,kind,"without_product")',[StringComparison]::Ordinal) -or
       -not $ready.Contains('native_clients_product_absent(s,target.kind+".initialization.before")') -or
       -not $ready.Contains('return same&&document_ready;') -or
       -not $shell.Contains('s.suite!="NativeClientsOnly"') -or
       -not $shell.Contains('!product::guest::same_exact_directory(target.directory,expected)') -or
       -not $shell.Contains('!native_clients_product_absent(s,"Terminal.direct_shell.before")') -or
       -not $shell.Contains('launched&&identity&&exited&&exit_known&&exit==0&&exact_receipt') -or
       ($ready+$shell) -match '\b(?:launch|SetForegroundWindow|SetFocus|SendInput|button|key|TerminateProcess)\s*\('){
        throw 'Focused native readiness must stay product-absent, exact guest scoped, and reject missing document initialization before client input.'
    }
}
function Assert-V1StretchModesScopeWiring([string]$Source,[string]$Chains){
    # Source wiring only. Native GUI settings, command results and restoration
    # still need the guarded guest run; this does not award that evidence.
    $entry=Get-V1FixtureFunctionText $Source 'independent_entry_flow'
    $scope=[regex]::Match($entry,'(?s)if\(s\.suite=="StretchModesOnly"\)\{(?<body>.*?)return;\s*\}')
    $drive=Get-V1FixtureFunctionText $Source 'drive'
    if(-not $scope.Success -or
       [regex]::Matches($scope.Groups['body'].Value,'\bphase\s*\(').Count -ne 1 -or
       -not $scope.Value.Contains('phase("smart_stretch_neighbour_modes",owned_stretch_ignore_neighbour_case);') -or
       -not $scope.Value.Contains('cleanup(s);') -or -not $scope.Value.Contains('stretch_modes_only_summary') -or
       $scope.Index -ge $entry.IndexOf('phase("settings_GUI",',[StringComparison]::Ordinal) -or
       $scope.Index -ge $entry.IndexOf('phase("owned_finite_I11_component",',[StringComparison]::Ordinal) -or
       $drive -notmatch '(?s)if\([^\r\n]*s\.suite=="StretchModesOnly"[^\r\n]*\)\{\s*independent_entry_flow\(s,'){
        throw 'StretchModesOnly must run only its actual component and cleanup, then return before unrelated suites.'
    }
    $component=Get-V1FixtureFunctionText $Chains 'owned_stretch_ignore_neighbour_case'
    if(-not $component.Contains('const Rect work=chain_work(set.frames[0].hwnd);') -or
       -not $component.Contains('config.stretch=true;config.smart_stretch=false;') -or
       -not $component.Contains('const auto applied=chain_gui_save(s,config);') -or
       -not $component.Contains('applied.stretch&&!applied.smart_stretch') -or
       -not $component.Contains('const Rect expected{work.l,source.t,source.r,source.b};') -or
       -not $component.Contains('chain_command(set,0,L"StretchLeft",expected);') -or
       -not $component.Contains('actual_source==expected&&actual_neighbour==neighbour&&expected.l!=neighbour.r') -or
       -not $component.Contains('chain_command(set,0,L"Undo",source);') -or
       -not $component.Contains('visible(set.frames[0].hwnd)==source&&visible(set.frames[1].hwnd)==neighbour') -or
       -not $component.Contains('const auto restored=chain_gui_save(s,original);') -or
       -not $component.Contains('restored==expected_config') -or -not $component.Contains('set.close()')){
        throw 'Disabled smart stretch must use actual GUI settings and the work-area oracle, preserve its neighbour, then Undo and restore configuration.'
    }
    $saved=$component.IndexOf('const auto applied=chain_gui_save(s,config);',[StringComparison]::Ordinal)
    $stretched=$component.IndexOf('chain_command(set,0,L"StretchLeft",expected);',[StringComparison]::Ordinal)
    $undone=$component.IndexOf('chain_command(set,0,L"Undo",source);',[StringComparison]::Ordinal)
    $restored=$component.IndexOf('const auto restored=chain_gui_save(s,original);',[StringComparison]::Ordinal)
    if($saved -ge $stretched -or $stretched -ge $undone -or $undone -ge $restored){
        throw 'Actual settings, stretch, Undo and configuration restoration must remain in that order.'
    }
}
function Assert-V1PauseRoutingScopeWiring([string]$Source,[string]$Chains){
    # Static reachability/oracle guards only. Actual Paused/UP/native delivery,
    # resource cleanup and resumed movement still require the guarded guest.
    $entry=Get-V1FixtureFunctionText $Source 'independent_entry_flow'
    $scope=[regex]::Match($entry,'(?s)if\(s\.suite=="PauseRoutingOnly"\)\{(?<body>.*?)return;\s*\}')
    $drive=Get-V1FixtureFunctionText $Source 'drive'
    if(-not $scope.Success -or
       [regex]::Matches($scope.Groups['body'].Value,'\bphase\s*\(').Count -ne 2 -or
       -not $scope.Value.Contains('phase("active_ctrl_pause_native_resume",owned_active_ctrl_pause_native_resume_case);') -or
       -not $scope.Value.Contains('phase("ctrl_shift_bypass_reselect",owned_ctrl_shift_bypass_reselect_case);') -or
       -not $scope.Value.Contains('cleanup(s);') -or -not $scope.Value.Contains('pause_routing_only_summary') -or
       $scope.Index -ge $entry.IndexOf('phase("settings_GUI",',[StringComparison]::Ordinal) -or
       $scope.Index -ge $entry.IndexOf('phase("owned_finite_I11_component",',[StringComparison]::Ordinal) -or
       $drive -notmatch '(?s)if\([^\r\n]*s\.suite=="PauseRoutingOnly"[^\r\n]*\)\{\s*independent_entry_flow\(s,'){
        throw 'PauseRoutingOnly must reach its two actual components and return after cleanup, before unrelated suites.'
    }
    $pause=Get-V1FixtureFunctionText $Chains 'owned_active_ctrl_pause_native_resume_case'
    $bypass=Get-V1FixtureFunctionText $Chains 'owned_ctrl_shift_bypass_reselect_case'
    $native=[regex]::Match($Chains,'(?ms)^std::vector<Rect> pause_routing_native_drag\(.*?(?=^std::vector<Rect> pause_routing_initial\()').Value
    if(-not $pause.Contains('const bool paused=launch(s,L"--pause",true)&&wait(') -or
       -not $pause.Contains('static_cast<unsigned>(v::End::Paused)') -or
       -not $pause.Contains('ended.find("\"pending_retired\":true")') -or
       -not $pause.Contains('abort_cleanup_up_receipt(s,g.offset,g.generation)') -or
       -not $pause.Contains('aborted.matched,aborted.json()') -or
       -not $pause.Contains('interruption_audit_calls(s,g,static_cast<std::uint64_t>(trigger.QuadPart),std::nullopt)') -or
       -not $pause.Contains('pause_routing_client_pair(set,"pause_routing_paused_real_client_DOWN_UP",1);') -or
       -not $pause.Contains('pause_routing_native_drag(set,0,-100,60,false,') -or
       -not $pause.Contains('const bool enabled=launch(s,L"--command Pause",true)&&wait(') -or
       -not $pause.Contains('layout_down(set,0,HTCAPTION,false)') -or
       -not $pause.Contains('layout_down(set,0,HTBOTTOM,false)') -or
       [regex]::Matches($pause,'\blayout_up\(set,next,actual\)').Count -ne 2 -or
       -not $pause.Contains('chain_gui_save(s,original)') -or -not $pause.Contains('set.close()')){
        throw 'Pause routing needs the real Pause/abort-UP audit, paused native input, actual Resume and new Move/Resize completions.'
    }
    if(-not $native.Contains('pause_routing_native_fresh_exact_context_changed_before_DOWN') -or
       -not $native.Contains('queue.exact_source_up_qpc(h,set.thread,s.left_up_send_qpc,down_stamp,destination,') -or
       -not $native.Contains('moved&&ended&&queue_stopped&&actual_up_qpc&&actual==expected&&idle&&!s.left&&!s.control&&') -or
       -not $native.Contains('!starts_claimed&&!placements&&!resources&&!cancels') -or
       $native -match '\b(?:layout_down|SetWindowPos|DeferWindowPos)\s*\(' -or
       -not $native.Contains('try{if(s.left&&!s.left_up_attempted)button(s,false);}') -or
       -not $native.Contains('try{if(shift_attempted&&!shift_release_attempted){shift_release_attempted=true;key(s,VK_SHIFT,false);}}')){
        throw 'Native pause/bypass input must preserve exact source UP evidence, no product operations, and independent bounded input cleanup.'
    }
    $raw=Get-V1FixtureFunctionText $Chains 'pause_routing_idle_raw_observation'
    $zero=[regex]::Match($raw,'(?s)zero_fields\[\]\s*\{(?<fields>.*?)\};').Groups['fields'].Value
    $falseFields=[regex]::Match($raw,'(?s)false_fields\[\]\s*\{(?<fields>.*?)\};').Groups['fields'].Value
    if(-not $raw.Contains('const auto value=evidence::unique_value(row,field);return value&&*value==expected;') -or
       -not $raw.Contains('!evidence::quoted(row,"\"fact\":","raw_mouse")') -or
       -not $raw.Contains('!pid||*pid!=product_pid') -or
       -not $raw.Contains('!evidence::quoted(row,"\"source_sha\":",source_sha)') -or
       -not $raw.Contains('for(const auto field:zero_fields)if(!exact(field,"0"))return false;') -or
       -not $raw.Contains('for(const auto field:false_fields)if(!exact(field,"false"))return false;') -or
       -not $native.Contains('if(pause_routing_idle_raw_observation(row,s.product_pid,s.product_source))++idle_raw_observations;') -or
       -not $native.Contains('else ++resources;')){
        throw 'Only complete known idle Raw observations may be counted separately; UNKNOWN resource fields must still block native routing.'
    }
    foreach($field in @('generation','source','source_pid','source_tid','overlay','native_message','legacy_up_dispatch_qpc',
        'SetCapture_previous_HWND','capture_actual_after','capture_failure','capture_owner_tid','capture_release_before_HWND',
        'capture_after_release','release_before_qpc','release_after_qpc','attach_before_qpc','attach_after_qpc',
        'detach_before_qpc','detach_after_qpc','setup_stage','native_failure','win32_error')){
        if(-not $zero.Contains(('\"'+$field+'\":'))){throw 'Nonzero Raw resource identity or API facts must not be ignored.'}
    }
    foreach($field in @('actual_legacy_up_accepted','capture_attempted','release_attempted','release_completed','release_succeeded',
        'capture_owner_observation_known','capture_release_after_is_copied_no_call','attach_attempted','attach_completed',
        'attach_succeeded','detach_attempted','detach_completed','detach_succeeded','overlay_destroyed','hotkey_unregistered')){
        if(-not $falseFields.Contains(('\"'+$field+'\":'))){throw 'Actual or unknown capture, association and cleanup facts must remain blocking resource evidence.'}
    }
    $nativeCall=$bypass.IndexOf('pause_routing_native_drag(set,0,-100,60,true,',[StringComparison]::Ordinal)
    $nextCtrl=$bypass.IndexOf('const auto next=layout_down(set,0,HTCAPTION,true);',[StringComparison]::Ordinal)
    if($nativeCall -lt 0 -or $nextCtrl -le $nativeCall -or
       $bypass.Substring($nativeCall,$nextCtrl-$nativeCall) -match '\bset\.setup\s*\(' -or
       -not $bypass.Contains('number(start,"\"group_members\":")==1') -or
       -not $bypass.Contains('source_key&&source_calls&&!other_calls&&') -or
       -not $bypass.Contains('visible(set.frames[1].hwnd)==initial[1]&&visible(set.frames[2].hwnd)==initial[2]') -or
       -not $bypass.Contains('chain_gui_save(s,original)') -or -not $bypass.Contains('set.close()')){
        throw 'The new Ctrl gesture must use the actual separated layout, a fresh one-member group and no old-member native calls.'
    }
}
function Assert-V1OwnedRuleTargetWiring([string]$Source,[string]$Layouts,[string]$Rules){
    # This verifies the real guest component's wiring, not native rule or GUI
    # behavior. The three command/Undo observations require actual guest runs.
    $entry=Get-V1FixtureFunctionText $Source 'independent_entry_flow'
    $scope=[regex]::Match($entry,'(?s)if\(s\.suite=="OwnedRulesOnly"\)\{(?<body>.*?)return;\s*\}')
    $drive=Get-V1FixtureFunctionText $Source 'drive'
    if(-not $scope.Success -or [regex]::Matches($scope.Groups['body'].Value,'\bphase\s*\(').Count -ne 1 -or
       -not $scope.Value.Contains('phase("owned_rule_target_stretch",owned_rule_target_stretch_case);') -or
       -not $scope.Value.Contains('cleanup(s);') -or -not $scope.Value.Contains('owned_rules_only_summary') -or
       $scope.Index -ge $entry.IndexOf('phase("settings_GUI",',[StringComparison]::Ordinal) -or
       $scope.Index -ge $entry.IndexOf('phase("owned_finite_I11_component",',[StringComparison]::Ordinal) -or
       $drive -notmatch '(?s)if\([^\r\n]*s\.suite=="OwnedRulesOnly"[^\r\n]*\)\{\s*independent_entry_flow\(s,' -or
       -not $Source.Contains('#include "platform/windows/product/v1_guest_rule_cases.h"')){
        throw 'OwnedRulesOnly must execute its actual rule target component and cleanup, then return before unrelated suites.'
    }
    $owns=Get-V1FixtureFunctionText $Layouts 'layout_ui_owns'
    $cleanup=Get-V1FixtureFunctionText $Layouts 'layout_ui_cleanup'
    if(-not $owns.Contains('index==1&&!storage.target1_alias.empty()?storage.target1_alias:storage.class_name') -or
       -not $owns.Contains('pid==GetCurrentProcessId()') -or
       -not $owns.Contains('GetWindowLongPtrW(frame.hwnd,GWLP_USERDATA)==reinterpret_cast<LONG_PTR>(&frame)') -or
       -not $Layouts.Contains('(state.suite!="OwnedRulesOnly"&&state.suite!="RemainingMoveContractsOnly")||storage->class_name!=L"PaneBind.V1.OwnedFixture."+state.run') -or
       -not $Layouts.Contains('storage->target1_alias=storage->class_name+L".StretchRuleTarget";') -or
       -not $Layouts.Contains('original.lpfnWndProc!=procedure||original.hInstance!=storage->instance') -or
       -not $Layouts.Contains('storage->alias_registered=RegisterClassExW(&original)!=0;') -or
       -not $cleanup.Contains('gone&&storage->alias_registered&&!storage->alias_unregister_attempted') -or
       -not $cleanup.Contains('UnregisterClassW(storage->target1_alias.c_str(),storage->instance)!=FALSE') -or
       -not $cleanup.Contains('storage->cleaned=exact&&gone&&alias_clean;')){
        throw 'The rule target alias must remain exact to member one and this Run, with actual registration and destruction/unregistration cleanup.'
    }
    $case=Get-V1FixtureFunctionText $Rules 'owned_rule_target_stretch_case'
    $baseline=$case.IndexOf('owned_rule_stretch_and_Undo(set,source,target,near_edge,"owned_rule_baseline_near_target_edge")',[StringComparison]::Ordinal)
    $add=$case.IndexOf('settings_guest_activate(s,gui,GetDlgItem(gui_page(s,gui),115));',[StringComparison]::Ordinal)
    $excluded=$case.IndexOf('owned_rule_stretch_and_Undo(set,source,target,work_edge,"owned_rule_excluded_visible_target_uses_work_edge")',[StringComparison]::Ordinal)
    $remove=$case.IndexOf('owned_rule_restore_rules_GUI(s,original);',[StringComparison]::Ordinal)
    $restored=$case.IndexOf('owned_rule_stretch_and_Undo(set,source,target,near_edge,"owned_rule_removed_target_returns_same_near_edge")',[StringComparison]::Ordinal)
    if($baseline -lt 0 -or $baseline -ge $add -or $add -ge $excluded -or $excluded -ge $remove -or $remove -ge $restored -or
       -not $case.Contains('set.create(true);') -or
       -not $case.Contains('gui_read(s,gui,class_filter,BM_GETCHECK)==BST_CHECKED') -or
       $case -notmatch 'settings_guest_acknowledge\(s,gui,L"[^"\r\n]+"\);gui_save_enter\(s,gui\);' -or
       -not $case.Contains('excluded.rules.back().excluded&&excluded.rules.back().window_class==utf8(alias)') -or
       -not $case.Contains('v::settings_detail::ascii_fold(excluded.rules.back().exe)==v::settings_detail::ascii_fold(actual_image)') -or
       -not $case.Contains('baseline_key&&excluded_key&&restored_key') -or
       $case.Substring($baseline,$restored-$baseline) -match '\bset\.setup\s*\(' -or
       -not $case.Contains('set.storage->alias_unregister_attempted&&set.storage->alias_unregistered') -or
       -not $Rules.Contains('chain_command(set,0,L"Undo",source);') -or
       -not $Rules.Contains('source_calls>=2&&!other_calls&&set.owns(0)&&set.owns(1)') -or
       $Rules -notmatch 'settings_guest_replace\(s,gui,gui_number\(s,gui,L"rule\.[^"\r\n]+"\),owned_rule_utf8\(fragment\)\);gui_save_enter\(s,gui\);' -or
       -not $Rules.Contains('MB_ERR_INVALID_CHARS')){
        throw 'The actual GUI must exclude only the exact target class between near-edge baseline and restored near-edge commands, preserving source identity and Undo.'
    }
}
$fixturePath='src/platform/windows/product/v1_guest_fixture.cpp'
$fixtureSource=[IO.File]::ReadAllText((Join-Path $repository $fixturePath),[Text.Encoding]::UTF8)
function Assert-V1RemainingMoveScope([string]$Source) {
    $route=[regex]::Match($Source,'(?s)if\(s\.suite=="RemainingMoveContractsOnly"\)\{(?<body>.*?)\n    \}')
    if(-not $route.Success){throw 'Remaining move-contract entry is missing.'}
    $body=$route.Groups['body'].Value
    if(-not $body.Contains('phase("remaining_group_membership",owned_remaining_group_membership_case);') -or
       -not $body.Contains('phase("remaining_magnet_policy",owned_remaining_magnet_policy_case);') -or
       -not $body.Contains('cleanup(s);') -or -not $body.Contains('return;') -or
       $body -match 'owned_finite_I11|owned_mouse_mapping|owned_settings|explorer_flow|applications_flow'){
        throw 'The remaining move-contract route must execute both real components and cleanup without unrelated suites.'
    }
}
Assert-V1RemainingMoveScope $fixtureSource
Check $true 'static remaining move entry calls both actual components, cleans up and returns without repeating old suites'
Reject {Assert-V1RemainingMoveScope ($fixtureSource.Replace('phase("remaining_group_membership",owned_remaining_group_membership_case);',''))} 'remaining move entry cannot silently omit dynamic-member functionality'
Assert-V1NativeClientPreflightWiring $fixtureSource
Check $true 'static driver wiring: every application native preflight precedes product cases and preserves exact HTCLIENT gates'
$layoutSource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_layout_cases.h'),[Text.Encoding]::UTF8)
$terminalMotionGuard='Sleep(60);refuse_observed_terminal_motion(s,g);'
Check ($fixtureSource.Contains($terminalMotionGuard) -and $layoutSource.Contains($terminalMotionGuard) -and
    $fixtureSource.Contains('const auto ended=record(s,gesture.offset,"gesture_end",gesture.generation);') -and
    $fixtureSource.Contains('throw std::runtime_error("test_motion_after_observed_termination_refused")')) 'static actual fixture and layout paths refuse a bound observed terminal after pacing and before cursor input'
$priorMotionSource=@(& git -C $repository show '5cb36bbd3e7707152dec1d5f5343b59e7d09af22:src/platform/windows/product/v1_guest_fixture.cpp') -join "`n"
Check ($LASTEXITCODE -eq 0 -and -not $priorMotionSource.Contains($terminalMotionGuard)) 'actual previously tested 5cb source lacks the observed-terminal motion guard; static check is not a native revalidation'
Assert-V1GroupBoundsScopeWiring $fixtureSource $layoutSource
Check $true 'static group scope: two shared actual cases return before unrelated suites; Full retains the shared cases and real prior evidence'
Reject {Assert-V1GroupBoundsScopeWiring ($fixtureSource.Replace('phase("eight_owned_group",owned_eight_group_only);','phase("owned_finite_I11_component",owned_finite_I11_chain);')) $layoutSource} 'static group scope rejects adding unrelated I11 work to the two-case entry'
Reject {Assert-V1GroupBoundsScopeWiring $fixtureSource ($layoutSource.Replace('layout_eight_case(set,std::nullopt)','layout_eight_case(set,Gesture{})'))} 'static group scope rejects a fabricated previous gesture for standalone eight-window validation'
Assert-V1NativeClientsWithoutProductWiring $fixtureSource
Check $true 'static native baseline: product launch is bypassed; exact client actions neither read product receipts nor run geometry cases'
Reject {Assert-V1NativeClientsWithoutProductWiring ($fixtureSource.Replace('without_product?std::size_t{}:read_product(s).size()','read_product(s).size()'))} 'static native baseline rejects reading product log before native input'
$nativePreflight=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_application_preflight.h'),[Text.Encoding]::UTF8)
Assert-V1FocusedNativeReadinessWiring $fixtureSource $nativePreflight
Check $true 'static focused native readiness checks only Terminal/Edge with actual product absence and document initialization before client input'
Reject {Assert-V1FocusedNativeReadinessWiring ($fixtureSource.Replace('if(!guest_native_application_ready(s,*target))throw std::runtime_error("native_initial_offline_document_not_ready");','')) $nativePreflight} 'static focused native readiness rejects removing the actual pre-input initialization gate'
Reject {Assert-V1FocusedNativeReadinessWiring $fixtureSource ($nativePreflight.Replace('!native_clients_product_absent(s,"Terminal.direct_shell.before")','false'))} 'static direct shell control rejects removing its exact product-absent guest gate'
$chainSource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_product_chains.h'),[Text.Encoding]::UTF8)
Assert-V1StretchModesScopeWiring $fixtureSource $chainSource
Check $true 'static stretch scope: actual disabled GUI setting, work-area oracle, unchanged neighbour, Undo and restoration are wired without unrelated suites'
Reject {Assert-V1StretchModesScopeWiring $fixtureSource ($chainSource.Replace('const Rect expected{work.l,source.t,source.r,source.b};','const Rect expected{neighbour.r,source.t,source.r,source.b};'))} 'static stretch scope rejects substituting the smart-enabled neighbour boundary oracle'
Assert-V1PauseRoutingScopeWiring $fixtureSource $chainSource
Check $true 'static pause routing: actual Pause/UP/native/Resume and bypass/new-relation components are wired without unrelated suites'
Reject {Assert-V1PauseRoutingScopeWiring $fixtureSource ($chainSource.Replace('!starts_claimed&&!placements&&!resources&&!cancels','!starts_claimed&&!resources&&!cancels'))} 'static pause routing rejects native bypass without the zero-product-placement gate'
Reject {Assert-V1PauseRoutingScopeWiring $fixtureSource ($chainSource.Replace('return value&&*value==expected;','return !value||*value==expected;'))} 'static idle Raw guard rejects treating missing or ambiguous resource fields as known zero/false'
Reject {Assert-V1PauseRoutingScopeWiring $fixtureSource ($chainSource.Replace('"\"source\":",',''))} 'static idle Raw guard rejects omitting the zero-source identity gate'
$ruleSource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_rule_cases.h'),[Text.Encoding]::UTF8)
Assert-V1OwnedRuleTargetWiring $fixtureSource $layoutSource $ruleSource
Check $true 'static owned rule scope: exact target alias, real GUI rule, baseline/exclusion/restoration commands and resource cleanup are wired'
Reject {Assert-V1OwnedRuleTargetWiring $fixtureSource $layoutSource ($ruleSource.Replace('excluded.rules.back().excluded&&excluded.rules.back().window_class==utf8(alias)','excluded.rules.back().excluded'))} 'static owned rule scope rejects accepting a rule without the exact target class'
function Assert-V1StretchInputWiring([string]$Fixture,[string]$Chain,[string]$Stretch){
    foreach($value in @('chain_double_click(set,item.region,item.shift,item.expected,item.label,true);',
        'const std::array<Direction,9> cases','chain_command(set,0,L"Undo",source);',
        'stretch_input_timeout_case(set,source);','stretch_input_movement_case(set,source);',
        'stretch_input_caption_button_case(set,source);','interval<=GetDoubleClickTime()',
        'actual_double_click_time_window_not_established')){
        if(-not $Stretch.Contains($value)){throw 'F04 input scope must retain strict UP, independent actions/Undo and explicit incomplete timing.'}
    }
    if(-not $Chain.Contains('bool require_actual_up=false') -or -not $Chain.Contains('actual_up_matched') -or
       -not $Fixture.Contains('scope_incomplete?"PARTIAL":')){throw 'F04 input observations must not be promoted from a missing UP or incomplete case.'}
}
$stretchInput=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_stretch_cases.h'),[Text.Encoding]::UTF8)
Assert-V1StretchInputWiring $fixtureSource $chainSource $stretchInput
Check $true 'static F04 input scope retains strict actual UP, named actions/Undo, timing negative and PARTIAL summary'
function Assert-V1StretchPreparationBeforeDown([string]$Chain){
    $mark=$Chain.IndexOf('const auto preparation_mark=require_actual_up?read_product(s).size():0;')
    if($mark -lt 0){throw 'F04 first DOWN requires the actual exact-point preparation receipt, not a fixed delay.'}
    $call=$Chain.IndexOf('chain_stretch_preparation(set,*point,native_region,*before,preparation_mark,3500,preparation)',$mark)
    $receipt=$Chain.IndexOf('F04_actual_product_preparation_before_first_DOWN',$mark)
    $down=$Chain.IndexOf('const DWORD first_down_stamp=GetTickCount();button(s,true);button(s,false);',$mark)
    if($call -le $mark -or $receipt -le $call -or $down -le $receipt){throw 'F04 first DOWN requires the actual exact-point preparation receipt, not a fixed delay.'}
    foreach($value in @('prepared_actual_source_and_hit','exact_number("\"config_generation\":",config_generation)',
        'exact_number("\"actual_hit_test\":",static_cast<unsigned>(native_region))',
        'exact_number("\"process_id\":",s.product_pid)','receipt_is_write_authority\":false',
        'preparation_publish_time_inferred\":false')){
        if(-not $Chain.Contains($value)){throw 'F04 preparation must match current product/source/configuration/hit and must not grant write authority.'}
    }
}
Assert-V1StretchPreparationBeforeDown $chainSource
Check $true 'static F04 strict path observes exact product preparation before its single first DOWN; no write authority inferred'
Reject {Assert-V1StretchPreparationBeforeDown ($chainSource.Replace('chain_stretch_preparation(set,*point,native_region,*before,preparation_mark,3500,preparation)','true'))} 'static F04 first DOWN rejects bypassing the shared actual preparation observer'
$oldF04Chain=@(& git -C $repository show 'a430607507f4980787296fa259a35ef4a97eb8f4:src/platform/windows/product/v1_guest_product_chains.h') -join "`n"
Check ($LASTEXITCODE -eq 0) 'static F04 regression loads actual e12 driver source'
Reject {Assert-V1StretchPreparationBeforeDown $oldF04Chain} 'actual e12 driver source fails missing preparation receipt guard (static regression, not native proof)'
Reject {Assert-V1StretchInputWiring $fixtureSource $chainSource ($stretchInput.Replace('chain_double_click(set,item.region,item.shift,item.expected,item.label,true);','chain_double_click(set,item.region,item.shift,item.expected,item.label,false);'))} 'static F04 scope rejects disabling actual UP receipts'
Reject {Assert-V1StretchInputWiring ($fixtureSource.Replace('scope_incomplete?"PARTIAL":','scope_incomplete?"PASS":')) $chainSource $stretchInput} 'static F04 scope rejects turning incomplete timing into PASS'
function Assert-V1StretchNativeBoundaryWiring([string]$Fixture,[string]$Stretch){
    foreach($value in @('message==WM_NCLBUTTONDOWN||message==WM_NCLBUTTONDBLCLK',
        'fact.time=GetMessageTime();fact.extra=GetMessageExtraInfo();','NativeClickDownBoundary native_click_down;')){
        if(-not $Fixture.Contains($value)){throw 'F04 movement boundary needs unchanged actual NC DOWN/DBLCLK observations.'}
    }
    foreach($value in @('chain_stretch_preparation(set,*point,HTRIGHT,source,preparation_mark,',
        'second_actual_exact_product_preparation_not_observed_within_time_window',
        'down2.matches(set,*point,click_downs+2,down2_sent,stamp2)',
        'second_loop?frame.native_end_qpc.load():0','second_preparation_checked_before_DOWN\":true')){
        if(-not $Stretch.Contains($value)){throw 'F04 second native press requires real preparation, actual DOWN and contextual UP; modal END is not invented.'}
    }
}
Assert-V1StretchNativeBoundaryWiring $fixtureSource $stretchInput
Check $true 'static F04 movement gate binds actual NC press, exact preparation and contextual UP without requiring a second modal loop'
Reject {Assert-V1StretchNativeBoundaryWiring $fixtureSource ($stretchInput.Replace('down2.matches(set,*point,click_downs+2,down2_sent,stamp2)','true'))} 'static F04 movement rejects missing actual second source press'
Reject {Assert-V1StretchNativeBoundaryWiring $fixtureSource ($stretchInput.Replace('chain_stretch_preparation(set,*point,HTRIGHT,source,preparation_mark,','true; /* removed actual preparation */ ('))} 'static F04 movement rejects missing second exact preparation'
function Assert-V1StretchBoundaryScope([string]$Fixture,[string]$Stretch){
    if(-not $Fixture.Contains('if(s.suite=="StretchInputOnly"||s.suite=="StretchBoundaryOnly")') -or
       -not $Fixture.Contains('phase("owned_stretch_input",owned_stretch_input_case);') -or
       -not $Stretch.Contains('if(!boundary_only){') -or
       -not $Stretch.Contains('"named_directions\":\"NOT_RUN\",\"timeout_negative\":\"NOT_RUN\"') -or
       -not $Stretch.Contains('set.setup({source},{limit});timing_established=stretch_input_movement_case(set,source);') -or
       -not $Stretch.Contains('set.setup({source},{limit});stretch_input_caption_button_case(set,source);')){
        throw 'Boundary scope must reuse real movement/caption cases and explicitly not rerun or rebind positive directions.'
    }
}
Assert-V1StretchBoundaryScope $fixtureSource $stretchInput
Check $true 'static boundary-only route reuses actual movement/caption checks and reports direction/timeout evidence not rerun'
Reject {Assert-V1StretchBoundaryScope $fixtureSource ($stretchInput.Replace('set.setup({source},{limit});stretch_input_caption_button_case(set,source);',''))} 'static boundary-only route rejects omitting the real caption-button negative'
function Assert-V1ExitRecoveryWiring([string]$Fixture,[string]$Exit){
    if(-not $Fixture.Contains('phase("owned_active_exit_and_controlled_kill_recovery",owned_exit_recovery_case);')){throw 'Exit recovery must call the real bounded guest component.'}
    foreach($value in @('for(const bool forced:std::array{false,true})','command=launch(s,L"--quit",true,&command_exit)',
        'GetProcessId(s.process)!=pid||process_birth(s.process)!=birth','killed=TerminateProcess(s.process,test_exit)',
        'exit_recovery_resources_after_exit(set,capture,helper,moved);','exit_recovery_cleanup_up_and_client(set,',
        'exit_recovery_restart(s,pid,birth,binary);','exit_recovery_restarted_same_EXE_actual_Move',
        'exit_recovery_restarted_same_EXE_actual_Resize','code==2','helper_exit==0',
        'source_read&&!source.hwndCapture','\"natural_crash\":\"NOT_RUN\"')){
        if(-not $Exit.Contains($value)){throw 'Exit recovery needs held exact identity, real exit/resource/visual/UP facts and same-EXE restart.'}
    }
    $measure=$Exit.IndexOf('const bool stopped=WaitForSingleObject(s.process,5000)==WAIT_OBJECT_0;')
    $cleanup=$Exit.IndexOf('exit_recovery_cleanup_up_and_client(set,',$measure)
    if($measure -lt 0 -or $cleanup -le $measure){throw 'Test cleanup UP must follow measured product exit.'}
}
$exitSource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_exit_cases.h'),[Text.Encoding]::UTF8)
Assert-V1ExitRecoveryWiring $fixtureSource $exitSource
Check $true 'static exit recovery calls actual guest-only active Quit and controlled termination, then cleanup UP and new Move/Resize'
Reject {Assert-V1ExitRecoveryWiring $fixtureSource ($exitSource.Replace('GetProcessId(s.process)!=pid||process_birth(s.process)!=birth','false'))} 'static exit recovery rejects removing held process PID/birth recheck'
Reject {Assert-V1ExitRecoveryWiring $fixtureSource ($exitSource.Replace('exit_recovery_resources_after_exit(set,capture,helper,moved);',''))} 'static exit recovery rejects exit-code-only cleanup inference'
function Assert-V1TrayReadinessWiring([string]$Fixture,[string]$Settings,[string]$Tray){
    # Reachability and evidence-binding only, never a native tray or Shell PASS.
    $drive=Get-V1FixtureFunctionText $Fixture 'drive'
    $branch=[regex]::Match($drive,'(?s)else if\(s\.suite=="TrayReadinessOnly"\)\{(?<body>.*?)\}else if\(s\.suite=="InterruptionOnly"\)')
    if(-not $Fixture.Contains('#include "platform/windows/product/v1_guest_tray_cases.h"') -or -not $branch.Success -or
       -not $branch.Value.Contains('independent_entry_ready(s,"tray_readiness_only.before",true);') -or
       -not $branch.Value.Contains('completed=owned_tray_readiness_case(s);blocked=!completed;') -or
       -not $branch.Value.Contains('if(s.process)cleanup(s);') -or
       -not $branch.Value.Contains('failed?"FAIL":completed?"PASS":"BLOCKED"')){
        throw 'Tray readiness must reach its guarded actual component and preserve BLOCKED/failure cleanup.'
    }
    $probe=Get-V1FixtureFunctionText $Tray 'tray_readiness_same_medium_probe'
    $flow=Get-V1FixtureFunctionText $Tray 'owned_tray_readiness_case'
    $add=$probe.IndexOf('probe.added=Shell_NotifyIconW(NIM_ADD,&data);')
    $lookup=$probe.IndexOf('probe.lookup=Shell_NotifyIconGetRect(&id,&probe.area);')
    $cleanup=$probe.IndexOf('const auto actual_window=probe.window;probe.cleanup();')
    if($add -lt 0 -or $lookup -le $add -or $cleanup -le $lookup -or
       -not $probe.Contains('if(!probe.added||!rectangle||!not_activated){') -or
       -not $probe.Contains('probe.delete_attempted&&probe.deleted&&probe.lookup_after_delete!=S_OK') -or
       -not $probe.Contains('probe.destroyed&&!IsWindow(actual_window)&&probe.unregister_attempted&&probe.unregistered') -or
       -not $Tray.Contains('if(window_identity()&&(!add_attempted||owns())){') -or
       -not $Tray.Contains('Shell_NotifyIconW(NIM_DELETE,&data)') -or
       -not $Tray.Contains('GetWindowLongPtrW(window,GWLP_USERDATA)==reinterpret_cast<LONG_PTR>(this)') -or
       -not $Tray.Contains('static constexpr UINT uid=0x5042') -or
       -not $probe.Contains('if(probe.added){') -or -not $probe.Contains('data.uVersion=NOTIFYICON_VERSION_4;') -or
       -not $probe.Contains('probe.versioned=Shell_NotifyIconW(NIM_SETVERSION,&data);') -or
       -not $flow.Contains('if(!tray_readiness_same_medium_probe(s))return false;')){
        throw 'Same-Medium probe must observe actual ADD/rectangle, remove only its exact icon/window/class before product menus, and fail closed.'
    }
    foreach($value in @('evidence::quoted(row,"\"schema\":","panebind-v1/1")',
        'evidence::quoted(row,"\"source_sha\":",s.product_source)',
        'evidence::quoted(row,"\"type\":",type)&&actual_pid&&*actual_pid==s.product_pid',
        'std::string registration;const bool registration_bound=sent&&wait([&]{',
        'registration=record(s,mark,"tray_registration")',
        'tray_guest_owner(s,owner)&&receipt_identity(registration,"tray_registration")&&actual_owner&&',
        '*actual_owner==reinterpret_cast<std::uintptr_t>(owner.app)&&actual_uid&&*actual_uid==1;',
        '},1500);','if(!sent||!registration_bound||!rectangle||!tray_guest_owner(s,owner)){',
        'owned_tray_entry_cases(s,false);','const auto exit_menu=tray_guest_open(s,owner);',
        'tray_guest_choose(s,owner,*exit_menu,505,L"',
        'receipt_identity(shutdown,"shutdown")','input_resource_stop_observed(shutdown)',
        'icon_after_exit!=S_OK','tray_actual_post_menu_Exit_client_DOWN_UP',
        's.frames[0].client_down[0].load()==downs+1','s.frames[0].client_up[0].load()==ups+1')){
        if(-not $flow.Contains($value)){throw ('Tray menus require a bounded exact current receipt, real menu Exit and actual post-exit client delivery: '+$value)}
    }
    $oldMenu=Get-V1FixtureFunctionText $Settings 'owned_tray_entry_cases'
    $choose=Get-V1FixtureFunctionText $Settings 'tray_guest_choose'
    if(-not $oldMenu.Contains('bool simulate_taskbar_recreation=true') -or
       -not $oldMenu.Contains('if(simulate_taskbar_recreation){') -or
       -not $choose.Contains('button(s,true);button(s,false);') -or
       [regex]::Matches($flow,'SendMessageTimeoutW\(owner\.app,taskbar,').Count -ne 1 -or
       $Tray -match '\b(?:SetForegroundWindow|SetFocus|SetCapture)\s*\(' -or
       $Tray -match '(?:SendMessage|PostMessage)\w*\([^;]*WM_COMMAND'){
        throw 'Tray control must retain old default behavior, suppress the second simulation and use actual menu input without Shell control.'
    }
}
$traySource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_tray_cases.h'),[Text.Encoding]::UTF8)
$traySettings=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_settings_cases.h'),[Text.Encoding]::UTF8)
Assert-V1TrayReadinessWiring $fixtureSource $traySettings $traySource
Check $true 'static tray readiness wires exact same-Medium probe cleanup, bounded receipt, one simulation and real menu Exit/client observations'
Reject {Assert-V1TrayReadinessWiring $fixtureSource $traySettings ($traySource.Replace('const auto actual_window=probe.window;probe.cleanup();','const auto actual_window=probe.window;'))} 'static tray readiness rejects deferring exact probe cleanup past its pre-menu observation'
Reject {Assert-V1TrayReadinessWiring $fixtureSource $traySettings ($traySource.Replace('receipt_identity(registration,"tray_registration")','true'))} 'static tray readiness rejects accepting an unbound registration receipt before menu input'
function Assert-V1GeometryHistoryWiring([string]$Fixture,[string]$History){
    if(-not $Fixture.Contains('phase("owned_geometry_noop_and_history_boundaries",owned_geometry_history_case);') -or
       -not $History.Contains('s.suite!="GeometryHistoryOnly"') -or
       -not $History.Contains('geometry_history_command(set,key,L"Center",center,true);') -or
       -not $History.Contains('geometry_history_command(set,key,L"StretchBottom",bottom,true);')){
        throw 'Geometry history must use its guarded real command route, including repeated targets.'
    }
    foreach($value in @('geometry_history_receipt(s,command,"command")',
        'geometry_history_receipt(s,placement,"placement")','*sequence>*placed',
        'geometry_history_status(s,*e::number(command,"\"sequence\":"))',
        'already_exact_no_native_call','e::number(placement,"\"call_id\":")==0',
        '*after_calls==*before_calls+(no_native?0:1)',
        'geometry_history_receipt(s,notice,"notice")&&sequence&&*sequence',
        'geometry_history_status(s,*e::number(notice,"\"sequence\":"))',
        'pause_routing_native_drag(set,0,60,40,true,','LayoutFixtures fresh{s};fresh.create();',
        'fresh_key!=key','actual_positioning==positioning')){
        if(-not $History.Contains($value)){throw ('Geometry history lost actual consumption, zero-write or identity evidence: '+$value)}
    }
}
$geometryHistory=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_geometry_history_cases.h'),[Text.Encoding]::UTF8)
Assert-V1GeometryHistoryWiring $fixtureSource $geometryHistory
Check $true 'static geometry history retains real command and notice barriers, no-op native counter and new-root identity checks'
Reject {Assert-V1GeometryHistoryWiring $fixtureSource ($geometryHistory.Replace('*after_calls==*before_calls+(no_native?0:1)','true'))} 'static geometry history rejects dropping the actual no-op native-call counter'
Reject {Assert-V1GeometryHistoryWiring $fixtureSource ($geometryHistory.Replace('geometry_history_receipt(s,notice,"notice")&&sequence&&*sequence','true'))} 'static geometry history rejects accepting an unbound Undo rejection receipt'
function Assert-V1VisualOwnershipWiring([string]$Fixture,[string]$Visual){
    if(-not $Fixture.Contains('owned_visual_ownership_case(s);') -or -not $Visual.Contains('s.suite!="VisualOwnershipOnly"')){
        throw 'Visual ownership must reach its guarded real product component.'
    }
    foreach($value in @('pid!=GetCurrentProcessId()||tid!=set.thread',
        'SetLayeredWindowAttributes(h,0,*alpha,LWA_ALPHA)',
        'e::unique_value(row,"\"alpha_native_attempted\":")== (restore_alpha?"true":"false")',
        'e::unique_value(row,"\"zorder_native_attempted\":")== (restore_top?"true":"false")',
        'visual_ownership_app_change(set,BYTE{201},std::nullopt,',
        'visual_ownership_app_change(set,std::nullopt,false,',
        'visual_ownership_shake_negatives(set);','layout_up(set,g,',
        'new_key!=key','helper_stopped&&helper_exit==0&&',
        'native_visual_is(fresh.frames[0].hwnd,141,true)',
        'pause_routing_client_pair(fresh,','fresh.close()')){
        if(-not $Visual.Contains($value)){throw ('Visual ownership lost actual attribute, identity or cleanup evidence: '+$value)}
    }
}
$visualOwnership=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_visual_ownership_cases.h'),[Text.Encoding]::UTF8)
Assert-V1VisualOwnershipWiring $fixtureSource $visualOwnership
Check $true 'static visual ownership uses same-app native changes, independent property masks, actual UP and helper exit with new-root preservation'
Reject {Assert-V1VisualOwnershipWiring $fixtureSource ($visualOwnership.Replace('e::unique_value(row,"\"alpha_native_attempted\":")== (restore_alpha?"true":"false")','true'))} 'static visual ownership rejects dropping the independent alpha restoration mask'
Reject {Assert-V1VisualOwnershipWiring $fixtureSource ($visualOwnership.Replace('helper_stopped&&helper_exit==0&&','true&&'))} 'static visual ownership rejects exit-code-only product cleanup without the actual held-helper exit'
function Assert-V1ResizeMagnetWiring([string]$Fixture,[string]$Resize){
    if(-not $Fixture.Contains('phase("owned_plain_Resize_magnet_active_edges_and_limit",owned_resize_magnet_case);') -or
       -not $Resize.Contains('s.suite!="ResizeMagnetOnly"')){throw 'Resize magnet must reach its guarded real product component.'}
    foreach($value in @('HTLEFT,HTRIGHT,HTTOP,HTBOTTOM,HTTOPLEFT,HTTOPRIGHT,HTBOTTOMLEFT,HTBOTTOMRIGHT',
        'geometry_history_positioning(set.frames[index].hwnd)==native[index]',
        'receipts&&native_calls==1&&set.owns(0)&&actual==expected[0]&&positioning==expected_native&&fixed&&candidates_unchanged',
        'e::unique_value(row,"\"native_attempted\":")=="true"',
        'const LayoutLimit bounded{120,100,450,work.height()}',
        'resize_magnet_quantum(set,g,HTRIGHT,30,0,initial,native,{pulled,neighbour}',
        'layout_up(set,g,{pulled,neighbour});',
        'constraint_calls==2&&set.owns(0)&&set.owns(1)',
        'move.generation>g.generation')){
        if(-not $Resize.Contains($value)){throw ('Resize magnet lost actual geometry, fixed-edge, limit, native or UP evidence: '+$value)}
    }
}
$resizeMagnet=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_resize_magnet_cases.h'),[Text.Encoding]::UTF8)
Assert-V1ResizeMagnetWiring $fixtureSource $resizeMagnet
Check $true 'static Resize magnet reaches eight actual edge/corner geometries, one native call, limits and original DOWN pullback'
Reject {Assert-V1ResizeMagnetWiring $fixtureSource ($resizeMagnet.Replace('native_calls==1','native_calls>=0'))} 'static Resize magnet rejects dropping the actual single-call quantum gate'
Reject {Assert-V1ResizeMagnetWiring $fixtureSource ($resizeMagnet.Replace('layout_up(set,g,{pulled,neighbour});',''))} 'static Resize magnet rejects omitting the actual matching UP before subsequent Move'
function Assert-V1FixedTerminalWiring([string]$Fixture,[string]$Resize,[string]$Burst){
    foreach($value in @('phase("owned_fixed_size_Resize_no_write_then_Move",owned_fixed_size_resize_case);',
        'phase("owned_active_Move_UP_burst_then_fresh_Move",owned_active_terminal_burst_case);')){
        if(-not $Fixture.Contains($value)){throw 'Fixed/terminal suite must call both actual guarded product test paths.'}
    }
    foreach($value in @('s.suite!="FixedAndTerminalOnly"','const LayoutLimit fixed{400,300,400,300}',
        'layout_up(set,g,initial);','geometry_history_status(s,*end_sequence)',
        '*prior_operations==*after_operations&&*prior_calls==*after_calls')){
        if(-not $Resize.Contains($value)){throw ('Fixed-size test lost actual constraints, UP or ordered zero-write evidence: '+$value)}
    }
    foreach($value in @('s.suite!="FixedAndTerminalOnly"','std::array<INPUT,2> packets{}',
        'packets[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;',
        'product::guest::matching_clean_captured_up(legacy,gone,up_identity)',
        'const auto barrier=std::min(up_qpc,*raw_qpc);',
        '*native_before<barrier','const Rect expected=claims?target:held;',
        'after_old==old_placements')){
        if(-not $Burst.Contains($value)){throw ('Enhanced terminal test lost exact guest, single UP, actual resource or native-order evidence: '+$value)}
    }
}
$activeTerminal=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_active_terminal_case.h'),[Text.Encoding]::UTF8)
Assert-V1FixedTerminalWiring $fixtureSource $resizeMagnet $activeTerminal
Check $true 'static fixed/terminal paths preserve real UP, ordered zero calls and UP-before-issue retirement semantics'
Reject {Assert-V1FixedTerminalWiring $fixtureSource ($resizeMagnet.Replace('layout_up(set,g,initial);','')) $activeTerminal} 'static fixed-size path cannot omit its actual matching UP'
Reject {Assert-V1FixedTerminalWiring $fixtureSource $resizeMagnet ($activeTerminal.Replace('*native_before<barrier','true'))} 'static active terminal path cannot accept a new native call after the actual UP barrier'
function Assert-V1StandardNativeWiring([string]$Fixture,[string]$Probe,[string]$Guest){
    if(-not $Fixture.Contains('!exact_standard_native_guest_account(username.data())') -or
       -not $Fixture.Contains('if(!found||actual_parent!=expected_pid)') -or
       -not $Fixture.Contains('s.suite!="NativeClientsOnly"||debug||') -or
       -not $Probe.Contains('return RunNativeParent(runId,scriptHash,driverSource,imageHash,imageSize,false,null,900);') -or
       -not $Probe.Contains('RunStandardNativeFixture(token,account,logon,session,bootstrapSid,bootstrapLogon,runId,productHash,nativeTimeout,') -or
       -not $Guest.Contains('if($standardNativeClients){') -or
       -not $Guest.Contains('if($completed){exit 0}else{exit 3}')){
        throw 'Standard native route must retain exact account/parent/suite guards, old CPWT and same original-token follow-up.'
    }
}
$standardProbe=[IO.File]::ReadAllText((Join-Path $repository 'scripts/v1-medium-desktop-probe-guest.ps1'),[Text.Encoding]::UTF8)
$standardGuest=[IO.File]::ReadAllText((Join-Path $repository 'scripts/v1-sandbox-guest.ps1'),[Text.Encoding]::UTF8)
Assert-V1StandardNativeWiring $fixtureSource $standardProbe $standardGuest
Check $true 'static standard native route keeps exact Run account/parent, old CPWT path and original credential-token follow-up'
Reject {Assert-V1StandardNativeWiring ($fixtureSource.Replace('!exact_standard_native_guest_account(username.data())','false')) $standardProbe $standardGuest} 'static standard route rejects removing the actual local account identity gate'
Reject {Assert-V1StandardNativeWiring ($fixtureSource.Replace('if(!found||actual_parent!=expected_pid)','if(found&&actual_parent!=expected_pid)')) $standardProbe $standardGuest} 'static standard route rejects unknown parent identity admission'
function Assert-V1MappingRemainderWiring([string]$Fixture,[string]$Mapping,[string]$Chains){
    # Reachability and evidence predicates only; no native delivery is inferred.
    $entry=Get-V1FixtureFunctionText $Fixture 'independent_entry_flow'
    $scope=[regex]::Match($entry,'(?s)if\(s\.suite=="MappingRemainderOnly"\)\{(?<body>.*?)return;\s*\}')
    if(-not $scope.Success -or [regex]::Matches($scope.Groups['body'].Value,'\bphase\s*\(').Count -ne 3 -or
       -not $scope.Value.Contains('phase("owned_custom_eight_region_mouse_mappings",owned_mapping_regions_case);') -or
       -not $scope.Value.Contains('phase("owned_custom_modifier_single_and_double_mapping",owned_mapping_modifier_double_case);') -or
       -not $scope.Value.Contains('phase("owned_hotkey_change_delete_and_duplicate_rejection",owned_mapping_hotkey_edits_case);') -or
       -not $scope.Value.Contains('cleanup(s);') -or -not $scope.Value.Contains('mapping_remainder_only_summary')){
        throw 'Mapping remainder must reach its three actual finite GUI/input components and cleanup.'
    }
    $hotkeyScope=[regex]::Match($entry,'(?s)if\(s\.suite=="HotkeyRemainderOnly"\)\{(?<body>.*?)return;\s*\}')
    $drive=Get-V1FixtureFunctionText $Fixture 'drive'
    $gateAt=$Fixture.IndexOf('void independent_entry_ready(State& s,std::string_view stage,bool restore_defaults){',[StringComparison]::Ordinal)
    if($gateAt -lt 0){throw 'Actual independent input-gate definition missing.'}
    $gate=Get-V1FixtureFunctionText $Fixture.Substring($gateAt) 'independent_entry_ready'
    if(-not $hotkeyScope.Success -or [regex]::Matches($hotkeyScope.Groups['body'].Value,'\bphase\s*\(').Count -ne 1 -or
       -not $hotkeyScope.Value.Contains('phase("owned_hotkey_change_delete_and_duplicate_rejection",owned_mapping_hotkey_edits_case);') -or
       -not $hotkeyScope.Value.Contains('cleanup(s);') -or -not $hotkeyScope.Value.Contains('hotkey_remainder_only_summary') -or
       $hotkeyScope.Value.Contains('owned_mapping_regions_case') -or $hotkeyScope.Value.Contains('owned_mapping_modifier_double_case') -or
       -not $drive.Contains('s.suite=="HotkeyRemainderOnly"') -or -not $gate.Contains('s.suite!="HotkeyRemainderOnly"') -or
       -not $gate.Contains('if(s.suite=="MappingRemainderOnly"||s.suite=="HotkeyRemainderOnly")for(const int code:std::array{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2,VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN})') -or
       -not $Mapping.Contains('s.suite!="MappingRemainderOnly"&&s.suite!="HotkeyRemainderOnly"')){
        throw 'Hotkey-only continuation must reach only its actual remaining phase with the same released-input guest gate and cleanup.'
    }
    $hotkeys=Get-V1FixtureFunctionText $Mapping 'owned_mapping_hotkey_edits_case'
    $draftStart=$hotkeys.IndexOf('const auto fragment=[](const v::Settings& value,bool mouse){',[StringComparison]::Ordinal)
    $draftEnd=$hotkeys.IndexOf('for(const bool mouse:{false,true}){',[StringComparison]::Ordinal)
    if($draftStart -lt 0 -or $draftEnd -le $draftStart){throw 'Bounded public invalid-draft construction missing.'}
    $draft=$hotkeys.Substring($draftStart,$draftEnd-$draftStart)
    if($draft.Contains('v::serialize(value)') -or $draft.Contains('chain_mouse_mapping_fragment(value)')){
        throw 'Deliberately duplicate GUI drafts cannot pass through the valid-Settings serializer.'
    }
    foreach($value in @('value.hotkeys.size()>v::settings_detail::max_hotkeys','value.mouse_bindings.size()>v::settings_detail::max_mouse',
        'out=L"mouse_count="','out=L"hotkey_count="','out.size()>16384')){
        if(-not $draft.Contains($value)){throw ('Invalid mapping draft lost its bounded public format: '+$value)}
    }
    if(-not $hotkeys.Contains('const auto serialized_before=v::serialize(before)') -or
       -not $hotkeys.Contains('!parsed_draft.value&&parsed_draft.error==expected_error') -or
       -not $hotkeys.Contains('GUI_submission_still_required')){
        throw 'The exact public duplicate draft must fail the real parser before its still-required actual GUI rejection.'
    }
    foreach($value in @('s.suite!="MappingRemainderOnly"','set.owns(0)&&visible(h)==geometry&&GetForegroundWindow()==h',
        'chain_stretch_preparation(set,*point,native_region,*geometry,preparation_mark,max_prepare,preparation)',
        'result.up=actual_up(s,mark,result.generation,3)',
        'e::number(result.up,"\"source_key\":")==key',
        'mapped&&released&&mapped_downs==1&&commands==1&&set.owns(0)',
        'chain_gui_save_mouse(s,bindings);','chain_gui_save_mouse(s,{{8,3,1,4,v::Action::ToggleTopmost},{8,3,2,4,v::Action::OpacityDown}})',
        'mapping_remaining_actual_resume_restores_original_enabled','launch(s,L"--command Pause",true)',
        'configured.hotkeys.back().action=v::Action::MoveDown','configured.hotkeys.pop_back()',
        'registered=RegisterHotKey(nullptr,registration,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,test_key)',
        'settings_guest_acknowledge(s,gui,mouse?','chain_settings(s)==before&&read(path)==saved&&hash(path)==saved_hash',
        'chain_check_product(s,pid,birth)')){
        if(-not $Mapping.Contains($value)){throw ('Mapping remainder lost an actual GUI, identity or paired-input gate: '+$value)}
    }
    if(-not $Chains.Contains('chain_type_fragment(s,gui,edit,fragment);gui_save_enter(s,gui);') -or
       -not $Chains.Contains('actual==wanted&&actual.generation>before.generation')){
        throw 'Custom mouse mappings must still be saved and read back through the actual public GUI.'
    }
}
$mappingRemaining=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_mapping_remaining_cases.h'),[Text.Encoding]::UTF8)
Assert-V1MappingRemainderWiring $fixtureSource $mappingRemaining $chainSource
Check $true 'static mapping remainder and narrow hotkey continuation retain actual GUI/input gates and bounded invalid public drafts'
Reject {Assert-V1MappingRemainderWiring ($fixtureSource.Replace('if(s.suite=="HotkeyRemainderOnly"){','if(s.suite=="HotkeyRemainderOnly_DISABLED"){')) $mappingRemaining $chainSource} 'static hotkey-only continuation rejects an unreachable remaining phase'
Reject {Assert-V1MappingRemainderWiring $fixtureSource ($mappingRemaining.Replace('const auto fragment=[](const v::Settings& value,bool mouse){','const auto fragment=[](const v::Settings& value,bool mouse){const auto invalid=v::serialize(value);')) $chainSource} 'static duplicate-draft guard rejects serializing an invalid Settings snapshot instead of its bounded public fragment'
function Assert-V1StandardLogonIdentityWiring([string]$Probe,[string]$Guest){
    $start=$Guest.IndexOf('    if($standardLogonIdentity){',[StringComparison]::Ordinal)
    $end=$Guest.IndexOf('    if($probeDesktop -and -not $standardNativeClients -and -not $standardLogonIdentity){',$start,[StringComparison]::Ordinal)
    if($start -lt 0 -or $end -le $start){throw 'Explicit logon identity terminal branch missing.'}
    $branch=$Guest.Substring($start,$end-$start)
    foreach($value in @('$record.run_id -cne $RunId','$record.actual_suite -cne ''StandardLogonNativeIdentity''',
        '$record.result.account_cleanup_confirmed -ne $true','$record.result.dacl_cleanup_confirmed -ne $true',
        '$record.result.all_probe_children_stopped -ne $true','$record.result.product_started -ne $false',
        '$record.result.synthetic_input_sent -ne $false','$record.result.standard_native_child.identity_observed -eq $true',
        'if($known){exit 0}else{exit 3}')){
        if(-not $branch.Contains($value)){throw ('Logon identity must terminate with exact identity/cleanup, not fall through: '+$value)}
    }
    if($end -ge $Guest.IndexOf('$summary[''application_availability'']=Read-TestApplicationAvailability',[StringComparison]::Ordinal) -or
       $branch -match '\b(?:Prepare-VSCodeTestAsset|Prepare-TerminalTestAsset|Prepare-OfficialMediaApps)\b'){
        throw 'Identity-only mode must finish before application/product/input preparation.'
    }
    foreach($value in @('$manifest.guest_standard_run_id -cne $RunId',
        '$manifest.guest_standard_account -cne (''PBV1D_''+$RunId.Substring(0,12))',
        '$manifest.guest_suite -cne ''NativeClientsOnly'' -or $manifest.guest_configurations -cne ''Release''',
        '$manifest.medium_desktop_probe.native_identity_only -isnot [bool]',
        '$manifest.medium_desktop_probe.standard_native_clients -isnot [bool]',
        'return RunNativeParent(runId,scriptHash,driverSource,imageHash,imageSize,true,null,30,true);',
        'CreateProcessWithLogonW(logonName,".",logonPassword,1,executable,new StringBuilder("\""+executable+"\" --build-identity")',
        '0x08000004,IntPtr.Zero,"C:\\PaneBindV1\\Input",ref startup,out child)',
        'WaitForSingleObject(child.process,20000)',
        'stdout.ContainsKey("text")&&(string)stdout["text"]==expected',
        'actual_child_identity_matches_created_account','actual_child_new_logon_sid_sha256',
        'if(asUserClients&&!logonIdentity&&(bool)child["identity_observed"]',
        'result["followup_native_clients_allowed"]=false',
        'if(account!=null&&actual==account&&(bool)result["all_probe_children_stopped"])')){
        if(-not $Probe.Contains($value)){throw ('Logon native identity lost its exact single-route or actual output/cleanup gate: '+$value)}
    }
    $baseline=$Probe.IndexOf('var baseline=RunNativeIdentity(',[StringComparison]::Ordinal)
    if($baseline -lt 0 -or $baseline -ge $Probe.IndexOf('uint added=NetUserAdd(null,1,ref info,out parameter)',[StringComparison]::Ordinal)){
        throw 'Actual WDAG native baseline must precede creation of the unique guest account.'
    }
}
Assert-V1StandardLogonIdentityWiring $standardProbe $standardGuest
Check $true 'static CPWL identity-only route binds exact Run/Release, actual native output, cleanup and terminal no-fallback scope'
Reject {Assert-V1StandardLogonIdentityWiring ($standardProbe.Replace('if(asUserClients&&!logonIdentity&&(bool)child["identity_observed"]','if(asUserClients&&(bool)child["identity_observed"]')) $standardGuest} 'static CPWL identity-only route rejects enabling native-client follow-up after identity'
Reject {Assert-V1StandardLogonIdentityWiring ($standardProbe.Replace('stdout.ContainsKey("text")&&(string)stdout["text"]==expected','true')) $standardGuest} 'static CPWL identity-only route rejects granting identity success without the exact actual stdout'
$frozenDriver='d2fbd1969417b56dcdbfbfb353611abc235e3065'
$frozenFixture=@(& git -C $repository show ($frozenDriver+':'+$fixturePath)) -join "`n"
Check ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($frozenFixture)) 'static regression loads the actual frozen d2 driver source'
$frozenReason='';try{Assert-V1NativeClientPreflightWiring $frozenFixture}catch{$frozenReason=$_.Exception.Message}
Check ($frozenReason -ceq 'Native client preflight must precede application product cases.') 'actual frozen d2 source fails the missing pre-product native client preflight guard'
$oldClientDriver='eec93e22d66d148f2611dfd59291208c66114aa1'
$oldClientFixture=@(& git -C $repository show ($oldClientDriver+':'+$fixturePath)) -join "`n"
Check ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($oldClientFixture)) 'static regression loads the actual eec client helper source'
$oldClientReason='';try{Assert-V1ClientUsesAdmittedSource $oldClientFixture}catch{$oldClientReason=$_.Exception.Message}
Check ($oldClientReason -ceq 'Client interaction must use its admitted source, not requery the whole geometry group.') 'actual eec helper fails the unnecessary all-sources requery guard'
Reject {Assert-V1NativeClientPreflightWiring ($fixtureSource.Replace('exact_child&&hit_read&&region==HTCLIENT','hit_read&&region==HTCLIENT'))} 'static guard rejects dropping exact-bound hit eligibility'
Reject {Assert-V1NativeClientPreflightWiring ($fixtureSource.Replace('exact_child&&hit_read&&region==HTCLIENT','exact_child&&hit_read'))} 'static guard rejects dropping actual HTCLIENT eligibility'
Reject {Assert-V1NativeClientPreflightWiring ($fixtureSource.Replace('quote(!product_gesture_attempted?"NOT_RUN":passed?"PASS":"NOT_CONFIRMED")','quote(!product_gesture_attempted?"PASS":passed?"PASS":"NOT_CONFIRMED")'))} 'static guard rejects enhanced PASS when no product gesture was attempted'
$product=[pscustomobject]@{product='PaneBind';version='1.0.0-dev';source_commit=$before;subsystem='windows';requires_elevation=$false}
$driver=[pscustomobject]@{product='PaneBind V1 guest fixture';source_commit=$after;host_synthetic_input='forbidden';requires_medium_guest=$true;
    guest_contract='panebind-v1-guest-contract/2';product_interface='panebind-v1-product-guest-interface/1'}
Assert-V1BinaryInterface $product Product $before;Assert-V1BinaryInterface $driver Driver $after
Check $true 'different true product and driver source identities are compatible under exact interface contract'
Reject {Assert-V1BinaryInterface $product Product $after} 'product identity is never rewritten as newer driver source'
$driver.guest_contract='panebind-v1-guest-contract/1'
Reject {Assert-V1BinaryInterface $driver Driver $after} 'old driver without split-identity guard cannot silently pass new package'
$product.requires_elevation=$true
Reject {Assert-V1BinaryInterface $product Product $before} 'elevated product does not match ordinary product interface'
Assert-V1PortableDependencies @('kernel32.dll','user32.dll','api-ms-win-core-synch-l1-2-0.dll')
Check $true 'actual portable packager accepts reviewed system-only imports'
Reject {Assert-V1PortableDependencies @('kernel32.dll','msvcp140.dll')} 'actual portable packager rejects an omitted external/versioned runtime dependency'
Assert-V1FixtureDependencies @('kernel32.dll','user32.dll','bcrypt.dll','propsys.dll','api-ms-win-core-synch-l1-2-0.dll')
Check $true 'actual fixture dependency validator permits its reviewed system-only crypto/property imports'
Reject {Assert-V1FixtureDependencies @('kernel32.dll','bcrypt.dll','propsys.dll','msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll')} 'actual fixture dependency validator rejects the MD runtime import list rather than relying on developer-installed DLLs'
# Load ONLY the real validation function definition, not the guest entry body.
# A missing-file spec is rejected before extraction, process creation or input.
$guestAst=[Management.Automation.Language.Parser]::ParseFile((Join-Path $repository 'scripts/v1-sandbox-guest.ps1'),[ref]$null,[ref]$null)
# Evaluate only the real source-array expression, never the guest function or
# WMI. PowerShell comma/addition precedence made actual Run 3927 produce one
# concatenated source instead of two; parser success alone cannot catch it.
function Assert-V1FullStartupSourceExpression([string]$Expression){
    $values=@(& ([scriptblock]::Create('param([string]$prefix) '+$Expression)) 'PaneBindV1.test.sample')
    if($values.Count -ne 2 -or $values[0] -isnot [string] -or $values[1] -isnot [string] -or
       $values[0] -cne 'PaneBindV1.test.sample.start' -or $values[1] -cne 'PaneBindV1.test.sample.stop'){
        throw 'Startup observation must construct two distinct exact source strings.'
    }
}
$startupFunction=@($guestAst.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Start-FullGuestStartupObservation'},$false))
if($startupFunction.Count -ne 1){throw 'Actual Full startup observation function is missing or ambiguous.'}
$sourceExpression=[regex]::Matches($startupFunction[0].Extent.Text,'\bsources\s*=\s*(?<expression>@\([^;\r\n]*\))\s*;owned=')
if($sourceExpression.Count -ne 1){throw 'Actual startup source expression is missing or ambiguous.'}
Assert-V1FullStartupSourceExpression $sourceExpression[0].Groups['expression'].Value
Check $true 'actual guest startup expression creates two exact event source strings without subscribing to WMI'
Reject {Assert-V1FullStartupSourceExpression '@($prefix+''.start'',$prefix+''.stop'')'} 'actual old Run 3927 source expression fails the two-source value check despite parsing successfully'
# Invoke the exact production row conversion/predicate without WMI. The
# retained 50f5 start is real; its rejected second row was not saved and must
# not be invented as a native StopTrace or attributed to a session-0 cause.
$traceFunction=@($guestAst.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Convert-FullStartupTraceRow'},$false))
$completeFunction=@($guestAst.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Complete-FullGuestStartupObservation'},$false))
if($traceFunction.Count -ne 1 -or $completeFunction.Count -ne 1 -or
   -not $completeFunction[0].Extent.Text.Contains('$row=Convert-FullStartupTraceRow $native') -or
   -not $completeFunction[0].Extent.Text.Contains('if($row.identity_error){$record.rejected_trace=$row;throw')){
    throw 'Actual startup handling must use the tested predicate and retain its rejected native row before throwing.'}
. ([scriptblock]::Create($traceFunction[0].Extent.Text))
$traceObserver=[ordered]@{pid=1234;session=1;managed_thread_id=9;session_source='test_parameter_not_native_observation'}
$retainedStart=[pscustomobject]@{ProcessID=[uint32]6960;ParentProcessID=[uint32]1160;SessionID=[uint32]1;
    ProcessName='rgnupdt.exe';TIME_CREATED=[uint64]134370283804911787;Sid=$null;ExitStatus=$null}
$traceRow=Convert-FullStartupTraceRow $retainedStart 'start' ([DateTime]'2026-10-21T03:53:00.4911787Z') $traceObserver
Check (-not $traceRow.identity_error -and $traceRow.pid -eq 6960 -and $traceRow.session -eq 1 -and
    $traceRow.raw_wmi_fields.event_time_created -eq [uint64]134370283804911787 -and
    $traceRow.observer_at_delivery.session -eq 1) 'actual startup predicate accepts the retained 50f5 start fields and records the separately supplied observer session'
$differentSession=$retainedStart.PSObject.Copy();$differentSession.SessionID=[uint32]0
$rejectedSession=Convert-FullStartupTraceRow $differentSession 'stop' ([DateTime]'2026-10-21T03:53:00.5Z') $traceObserver
$missingSession=[pscustomobject]@{ProcessID=[uint32]6960;ParentProcessID=[uint32]1160;ProcessName='rgnupdt.exe';TIME_CREATED=[uint64]134370283804911788;Sid=$null;ExitStatus=[uint32]0}
$rejectedMissing=Convert-FullStartupTraceRow $missingSession 'stop' ([DateTime]'2026-10-21T03:53:00.5Z') $traceObserver
Check ($rejectedSession.identity_error -ceq 'trace_session_mismatch' -and $rejectedSession.raw_wmi_fields.session -eq 0 -and
    $rejectedSession.observer_at_delivery.session -eq 1 -and $rejectedMissing.identity_error -ceq 'trace_session_missing' -and
    $null -eq $rejectedMissing.session -and $null -eq $rejectedMissing.raw_wmi_fields.session) 'actual startup predicate rejects a foreign session and distinguishes a missing raw session from numeric zero; neither is a claimed 50f5 native stop'
# The diagnostic verdict is not input authority. Execute ONLY the actual
# frontend if-branch and its real pure continuation predicate, not WMI/guest.
$continuation=@($guestAst.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Test-FullStartupFixtureContinuation'},$false))
$startupFrontend=@($guestAst.FindAll({param($node) $node -is [Management.Automation.Language.IfStatementAst] -and
    $node.Clauses[0].Item1.Extent.Text -ceq '-not (Test-FullStartupFixtureContinuation $summary.full_startup_observation)'},$false))
$finalQuery=@($completeFunction[0].Body.FindAll({param($node) $node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -ceq 'Get-CimInstance'},$false))
$completeTry=@($completeFunction[0].Body.EndBlock.Statements|Where-Object {$_ -is [Management.Automation.Language.TryStatementAst]})
if($continuation.Count -ne 1 -or $startupFrontend.Count -ne 1 -or $finalQuery.Count -ne 1 -or $completeTry.Count -ne 1 -or
   -not $completeTry[0].Finally.Extent.Text.Contains($finalQuery[0].Extent.Text)){
    throw 'Actual startup must make its one final query after success/rejection and gate the frontend only on current snapshot plus owned cleanup.'}
. ([scriptblock]::Create($continuation[0].Extent.Text))
$frontend=[scriptblock]::Create($startupFrontend[0].Extent.Text)
$diagnostic=[pscustomobject]@{status='ENVIRONMENT_READY_UNKNOWN';rejected_trace=[pscustomobject]@{session=[uint32]0};
    current_snapshot_collected=$true;current_processes=@();subscription_cleanup_confirmed=$true;event_cleanup_confirmed=$true}
$summary=[pscustomobject]@{full_startup_observation=$diagnostic}
& $frontend
Check ((Test-FullStartupFixtureContinuation $diagnostic) -and $diagnostic.status -ceq 'ENVIRONMENT_READY_UNKNOWN' -and
    $diagnostic.rejected_trace.session -eq 0) 'actual frontend may reach original fixture guards after one empty final snapshot and exact observer cleanup without converting an unknown/session-0 trace into PASS'
$frontendRejected=0
foreach($failure in @('missing_snapshot','current_process','subscriber_not_removed','events_not_removed')){
    $candidate=[pscustomobject]@{status='TARGET_LIFECYCLE_EXIT_OBSERVED';current_snapshot_collected=$true;current_processes=@();
        subscription_cleanup_confirmed=$true;event_cleanup_confirmed=$true}
    switch($failure){
        'missing_snapshot' {$candidate.current_snapshot_collected=$false}
        'current_process' {$candidate.current_processes=@([pscustomobject]@{pid=5652})}
        'subscriber_not_removed' {$candidate.subscription_cleanup_confirmed=$false}
        'events_not_removed' {$candidate.event_cleanup_confirmed=$false}
    }
    $summary=[pscustomobject]@{full_startup_observation=$candidate}
    try{& $frontend}catch{$frontendRejected++}
}
Check ($frontendRejected -eq 4) 'actual frontend rejects missing/nonempty current snapshot or either unconfirmed cleanup even if the diagnostic verdict claims an observed exit'
$portableFunction=@($guestAst.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Prepare-PortableTestPackage'},$false))
Check ($portableFunction.Count -eq 1) 'test locates the actual guest portable validation function'
. ([scriptblock]::Create($portableFunction[0].Extent.Text))
$outputDir=Join-Path $repository 'uat\not-created-portable-negative-check';$RunId='00000000000000000000000000000000'
$missingSpec=[pscustomobject]@{kind='PaneBindReleasePortable';relative_path='PaneBindV1-portable-test.zip';package_id=$RunId;
    source_commit=$before;exe_sha256=('A'*64);zip_sha256=('B'*64);manifest_sha256=('C'*64);zip_size=128;expanded_bytes=128;
    files=@(1..6|ForEach-Object{[pscustomobject]@{relative_path=('file'+$_);size=1;sha256=('D'*64)}})}
$missingReason='';try{Prepare-PortableTestPackage $missingSpec $before ('A'*64)|Out-Null}catch{$missingReason=$_.Exception.Message}
Check ($missingReason -ceq 'Portable package must have exactly seven payload files.' -and -not(Test-Path -LiteralPath $outputDir)) 'actual guest rejects missing payload file before extraction; no directory or product was created'
# Execute the actual official-assets archive branch in a fresh Windows
# PowerShell process: assemblies loaded by this test cannot mask a missing
# dependency in the packaging process. This only archives the repo license.
$preparePath=Join-Path $repository 'scripts/v1-prepare-sandbox.ps1'
$prepareAst=[Management.Automation.Language.Parser]::ParseFile($preparePath,[ref]$null,[ref]$null)
$archiveBranch=@($prepareAst.FindAll({param($node)
    $node -is [Management.Automation.Language.IfStatementAst] -and
    $node.Clauses[0].Item1.Extent.Text -ceq '$officialApps' -and
    $node.Extent.Text.Contains('$archivePath=Join-Path $inputDir $officialApps.relative_path')
},$false))
Check ($archiveBranch.Count -eq 1) 'locates the actual official asset archive branch'
$archiveTestDir=Join-Path $repository ('uat/v1-package-archive-check/'+[Guid]::NewGuid().ToString('N'))
$archiveSetup=@'
$ErrorActionPreference='Stop'
$inputDir=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('__DIRECTORY__'))
$license=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('__LICENSE__'))
[IO.Directory]::CreateDirectory($inputDir)|Out-Null
$officialApps=[pscustomobject]@{relative_path='official-test.zip';zip_sha256='';zip_size=0}
$officialFiles=@([pscustomobject]@{path='LICENSE';source=$license;sha256=(Get-FileHash -LiteralPath $license -Algorithm SHA256).Hash})
$payloadFiles=@()
__BRANCH__
$read=[IO.Compression.ZipFile]::OpenRead($archivePath)
try{if($read.Entries.Count -ne 1 -or $read.Entries[0].FullName -cne 'LICENSE' -or $read.Entries[0].Length -ne (Get-Item -LiteralPath $license).Length){throw 'Actual archive contents mismatch'}}finally{$read.Dispose()}
if($officialApps.zip_size -le 0 -or $officialApps.zip_sha256.Length -ne 64 -or $payloadFiles -cnotcontains 'official-test.zip'){throw 'Actual archive manifest fields missing'}
'actual_official_archive_pass'
'@
$archiveCommand=$archiveSetup.Replace('__DIRECTORY__',[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($archiveTestDir))).Replace('__LICENSE__',[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes((Join-Path $repository 'LICENSE')))).Replace('__BRANCH__',$archiveBranch[0].Extent.Text)
$archiveResult=@(& powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand ([Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($archiveCommand))))
Check ($LASTEXITCODE -eq 0 -and $archiveResult -ccontains 'actual_official_archive_pass') 'fresh PS5.1 executes the production archive branch and verifies its real ZIP and manifest fields'
foreach($relative in @('scripts/v1-package-identity.ps1','scripts/v1-prepare-portable.ps1','scripts/v1-prepare-sandbox.ps1','scripts/v1-sandbox-guest.ps1','scripts/v1-medium-desktop-probe-guest.ps1')){
    $tokens=$null;$errors=$null
    [Management.Automation.Language.Parser]::ParseFile((Join-Path $repository $relative),[ref]$tokens,[ref]$errors)|Out-Null
    Check ($errors.Count -eq 0) ('PowerShell parser: '+$relative)
}
Write-Output ('V1 packaging identity: '+$checks+' checks passed; no package, product UI or synthetic input was run.')
