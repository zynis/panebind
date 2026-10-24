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
       -not $drive.Contains('if(s.suite!="NativeClientsOnly"&&s.suite!="MediumDesktopEnvironmentOnly"&&s.suite!="MediumDesktopUiOnly"&&s.suite!="MediumDesktopNativeOnly"&&s.suite!="MediumDesktopNativeClientOnly")cleanup(s);')){
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
       -not $flow.Contains('native_client_completion({bound,client_attempted,passed,source_idle,closed,cleanup_error.empty(),input_clear})') -or
       -not $flow.Contains('quote(product::guest::native_client_verdict_name(completion.overall))') -or
       -not $flow.Contains('quote(product::guest::native_client_verdict_name(completion.client))') -or
       -not $flow.Contains('quote(product::guest::native_client_verdict_name(completion.normal_close))') -or
       -not $shell.Contains('"Terminal_internal_shell_working_directory_verified\":false') -or
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
Reject {Assert-V1FocusedNativeReadinessWiring ($fixtureSource.Replace('source_idle,closed,cleanup_error.empty(),input_clear','source_idle,true,cleanup_error.empty(),input_clear')) $nativePreflight} 'static native completion rejects replacing real normal-close confirmation with true'
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
function Assert-V1LifecycleHideWiring([string]$Fixture,[string]$Lifecycle,[string]$Prepare,[string]$Guest){
    if(-not $Fixture.Contains('#include "platform/windows/product/v1_guest_lifecycle_remaining_case.h"') -or
       -not $Fixture.Contains('phase("observed_hide_show_new_admission_one_fresh_Move",owned_lifecycle_remaining_case);') -or
       [regex]::Matches($Fixture,'LifecycleRemainderOnly').Count -ne 6 -or
       -not $Prepare.Contains("'LifecycleRemainderOnly'") -or -not $Guest.Contains("'LifecycleRemainderOnly'")){
        throw 'Hide/show remainder requires its exact independent, parser, manifest and two-script suite wiring.'
    }
    foreach($value in @('settings_guest_scope(s);','s.suite!="LifecycleRemainderOnly"',
        'set.run(LayoutUiOperation::Configure,{})','const auto hidden_status=geometry_history_status(s);',
        '*hidden_eligible+1==*eligible','new_key&&new_key!=old_key','placements(mark).empty()',
        'e::number(start,"\"key\":")==new_key','layout_up(set,g,{expected});')){
        if(-not $Lifecycle.Contains($value)){throw ('Hide/show lost actual admission, public observation or UP: '+$value)}
    }
    if([regex]::Matches($Lifecycle,'\blayout_down\s*\(').Count -ne 1 -or
       $Lifecycle.Contains('geometry_history_rejected_undo(') -or $Lifecycle.Contains('DestroyOne') -or
       $Lifecycle.Contains('fresh.create(')){
        throw 'Hide/show must not replay the independently observed close/recreate/history sequence.'
    }
}
$lifecycleHide=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_lifecycle_remaining_case.h'),[Text.Encoding]::UTF8)
$lifecyclePrepare=[IO.File]::ReadAllText((Join-Path $repository 'scripts/v1-prepare-sandbox.ps1'),[Text.Encoding]::UTF8)
$lifecycleGuest=[IO.File]::ReadAllText((Join-Path $repository 'scripts/v1-sandbox-guest.ps1'),[Text.Encoding]::UTF8)
Assert-V1LifecycleHideWiring $fixtureSource $lifecycleHide $lifecyclePrepare $lifecycleGuest
Check $true 'static hide/show route retains exact guest guards, public observed ineligibility, fresh key and one real Move/UP'
Reject {Assert-V1LifecycleHideWiring $fixtureSource ($lifecycleHide.Replace('const auto hidden_status=geometry_history_status(s);','const auto hidden_status=before_status;')) $lifecyclePrepare $lifecycleGuest} 'static hide/show rejects substituting old status for actual product observation'
Reject {Assert-V1LifecycleHideWiring $fixtureSource $lifecycleHide ($lifecyclePrepare.Replace("'LifecycleRemainderOnly'","'DisabledLifecycle'")) $lifecycleGuest} 'static hide/show rejects a missing exact packager suite'
function Assert-V1HighContrastWiring([string]$Fixture,[string]$Cases,[string]$Prepare,[string]$Guest){
    if(-not $Fixture.Contains('#include "platform/windows/product/v1_guest_high_contrast_case.h"') -or
       -not $Fixture.Contains('phase("actual_guest_HC_mode_and_keyboard_subset",owned_high_contrast_case);') -or
       -not $Fixture.Contains('REUSED_34F1_ORDINARY_NOTICE_NOT_RUN_HERE') -or
       $Fixture.Contains('phase("ordinary_notice_before_HC_transition",owned_idle_notice_before_HC_case);') -or
       [regex]::Matches($Fixture,'HighContrastOnly').Count -ne 6 -or
       -not $Prepare.Contains("'HighContrastOnly'") -or -not $Guest.Contains("'HighContrastOnly'")){
        throw 'High contrast needs exact independent/parser/manifest and guest-only script dispatch.'
    }
    foreach($value in @('settings_guest_scope(s);','s.suite!="HighContrastOnly"',
        'high_contrast_SET_requires_actual_idle_product','GuestHighContrastLease lease{s};',
        'SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(actual),&actual,0)',
        'LocalFree(entry.pointer)','original.flags|HCF_HIGHCONTRASTON',
        'live_string->data()==string_base&&live_string->size()==string_length&&candidate==actual_parameter',
        'out.scheme_retained=!out.scheme_borrowed&&buffers.retain(actual.lpszDefaultScheme,out.retained_pointer_reused);',
        'std::array<Entry,8> entries_',
        'if(entries_[i].pointer==pointer){reused=true;return true;}',
        'if(!buffers.may_read()){','GET_not_attempted_retained_buffer_bound',
        'const bool all_released=buffers_.release_all();',
        'actual.scheme_freed=actual.GET_attempted&&!actual.scheme_borrowed&&buffers_.pointer_freed(actual.returned_scheme_pointer);',
        'actual.scheme_free_attempted=buffers_.pointer_release_attempted(actual.returned_scheme_pointer);',
        'all_released&&actual.scheme_freed&&!actual.scheme_borrowed',
        'observe_SET_enter(request,"enable");','observe_SET_enter(request,"restore");',
        'active=lease.read(trace?&s:nullptr);','if(active.scheme_borrowed){',
        'blocked("GET_exact_alias_live_SET_caller_buffer_not_system_allocation");s.log.failed=true;',
        'return exact&&ownership_complete;',
        'HIGHCONTRASTW request{sizeof(request),original.flags,',
        'actual.flags==original.flags&&actual.scheme==original.scheme&&actual.colors==original.colors',
        'high_contrast_environment_blocked','profile_update_requested\":false',
        'gui_press(s,gui,VK_TAB)','GetGUIThreadInfo(gui.tid,&info)',
        'info.hwndFocus==wanted','WM_QUERYUISTATE','UISF_HIDEFOCUS',
        'info.hwndCaret==wanted','guest_capture_settings_page(s,gui,0);',
        'guest_capture_settings_page(s,gui,3);','gui_save_enter(s,gui);','gui_press(s,gui,VK_ESCAPE)',
        'live==expected','read(s.product.parent_path()/L"settings.ini")==before_cancel',
        'foreground_before==source->frames[0].hwnd&&foreground_after==foreground_before',
        'record(s,mark,"placement").empty()','chain_check_product(s,pid,birth);',
        'high_contrast_original_system_state_restored','lease.restore()',
        'whole_M10_F05_PASS\":false','screen_BMP_visual_review\":\"NOT_RUN',
        '200_percent_DPI\":\"NOT_RUN','specific_notice_visible_exit\":\"ACTUAL_WINDOW_PIXELS_AND_HIDE_REQUIRE_VISUAL_REVIEW',
        'guest_actual_product_notice_strip(*source,0,actual,','high_contrast_notice_real_one_shot_expiry_hides',
        'high_contrast_new_gesture_retires_actual_notice_timer')){
        if(-not $Cases.Contains($value)){throw ('High contrast lost actual GET/SET, keyboard, restoration or scope fact: '+$value)}
    }
    if($Cases.IndexOf('settings_guest_scope(s);') -gt $Cases.IndexOf('GuestHighContrastLease lease{s};') -or
       [regex]::Matches($Cases,'SystemParametersInfoW\(SPI_SETHIGHCONTRAST,sizeof\(request\),&request,SPIF_SENDCHANGE\)').Count -ne 2 -or
       $Cases -match 'SPIF_UPDATEINIFILE|SetSysColors|SetTheme|RegSet|ChangeDisplaySettings|SetProcessDpi|SetThreadDpi|PostMessage|SendMessage\(' -or
       -not $Fixture.Contains('quote(blocked?"BLOCKED":s.log.failed?"FAIL":"AUTOMATED_PASS_VISUAL_REVIEW_NOT_RUN")')){
        throw 'Only an exact idle guest may GET/SET/restore HC, without persistent theme/DPI edits or an inferred full PASS.'
    }
}
$highContrast=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_high_contrast_case.h'),[Text.Encoding]::UTF8)
Assert-V1HighContrastWiring $fixtureSource $highContrast $lifecyclePrepare $lifecycleGuest
Check $true 'static high contrast is exact-guest-only, actual bounded GET/SET/restore, real Tab focus and separately unreviewed BMP evidence'
Reject {Assert-V1HighContrastWiring $fixtureSource ($highContrast.Replace('settings_guest_scope(s);','')) $lifecyclePrepare $lifecycleGuest} 'static high contrast rejects system setup without the exact disposable guest gate'
Reject {Assert-V1HighContrastWiring $fixtureSource ($highContrast.Replace('actual.colors==original.colors','true')) $lifecyclePrepare $lifecycleGuest} 'static high contrast rejects restoring only a flag while leaving the actual original colors unchecked'
Reject {Assert-V1HighContrastWiring $fixtureSource ($highContrast.Replace('out.scheme_retained=!out.scheme_borrowed&&buffers.retain(actual.lpszDefaultScheme,out.retained_pointer_reused);','out.scheme_retained=buffers.retain(actual.lpszDefaultScheme,out.retained_pointer_reused);')) $lifecyclePrepare $lifecycleGuest} 'static high contrast rejects ownership of an exact live caller-buffer alias'
Reject {Assert-V1HighContrastWiring $fixtureSource ($highContrast.Replace('actual.GET_attempted&&!actual.scheme_borrowed&&buffers_.pointer_freed(actual.returned_scheme_pointer)','true')) $lifecyclePrepare $lifecycleGuest} 'static high contrast rejects reporting pending, borrowed or unobserved storage as actually freed'
Reject {Assert-V1HighContrastWiring $fixtureSource ($highContrast.Replace('if(entries_[i].pointer==pointer){reused=true;return true;}','if(false){reused=true;return true;}')) $lifecyclePrepare $lifecycleGuest} 'static high contrast rejects registering the same still-retained pointer for duplicate release'
Reject {Assert-V1HighContrastWiring $fixtureSource ($highContrast.Replace('return exact&&ownership_complete;','return exact;')) $lifecyclePrepare $lifecycleGuest} 'static high contrast rejects turning state restoration into allocation-ownership or HC PASS'
function Assert-V1NativeTopDropWiring([string]$Fixture,[string]$Cases,[string]$Prepare,[string]$Guest){
    if(-not $Fixture.Contains('#include "platform/windows/product/v1_guest_native_top_drop_case.h"') -or
       -not $Fixture.Contains('phase("default_top_preview_single_real_UP_actual_Maximize",owned_native_top_drop_case);') -or
       [regex]::Matches($Fixture,'NativeTopDropOnly').Count -ne 6 -or
       -not $Prepare.Contains("'NativeTopDropOnly'") -or -not $Guest.Contains("'NativeTopDropOnly'")){
        throw 'Default Top drop needs exact guest/parser/manifest and independent single-gesture dispatch.'
    }
    foreach($value in @('settings_guest_scope(s);','s.suite!="NativeTopDropOnly"',
        'original.dock_actions[2]!=v::Action::Maximize','config.preview=true;',
        'layout_phase(set,0,g,dx,dy,{raw},','chain_preview_exact(s,source,work);',
        'rows(g,"normal_up_maximize").empty()','actual_up(s,g.offset,g.generation).empty()',
        'layout_up(set,g,{});','IsZoomed(source)&&!IsIconic(source)&&GetWindowPlacement(source,&maximized)',
        'maximized.showCmd==SW_SHOWMAXIMIZED&&visible(source)==work',
        'remaining_group_receipt(s,maximize,"normal_up_maximize")',
        'e::unique_value(maximize,"\"native_attempted\":")=="true"',
        'e::unique_value(maximize,"\"request_started\":")=="true"',
        'rows(g,"normal_up_maximize").size()==1','dock_remaining_commit_receipts(s,g).empty()',
        'RI_MOUSE_LEFT_BUTTON_UP','\"actual_legacy_up_accepted\"','\"release_succeeded\"','\"detach_succeeded\"',
        'placements_before==rows(g,"placement")','chain_check_product(s,pid,birth);',
        'hash(s.product)==binary','const bool closed=set.close();')){
        if(-not $Cases.Contains($value)){throw ('Top drop lost actual preview, release, native state or cleanup: '+$value)}
    }
    if([regex]::Matches($Cases,'\blayout_down\s*\(').Count -ne 1 -or
       [regex]::Matches($Cases,'\blayout_up\s*\(').Count -ne 1 -or
       $Cases -match 'chain_command\(|geometry_history_command\(|ShowWindow\(|ShowWindowAsync\(|SetWindowPos\(|PostMessage|SendMessage\('){
        throw 'Default Top must observe its one real release, not force Maximize/placement or replay restore/Full.'
    }
}
$nativeTopDrop=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_native_top_drop_case.h'),[Text.Encoding]::UTF8)
Assert-V1NativeTopDropWiring $fixtureSource $nativeTopDrop $lifecyclePrepare $lifecycleGuest
Check $true 'static default Top uses real preview, one UP, actual maximized state and independent resource/late-write evidence'
Reject {Assert-V1NativeTopDropWiring $fixtureSource ($nativeTopDrop.Replace('chain_preview_exact(s,source,work);','')) $lifecyclePrepare $lifecycleGuest} 'static Top drop rejects substituting command Maximize for actual preview'
Reject {Assert-V1NativeTopDropWiring $fixtureSource ($nativeTopDrop.Replace('maximized.showCmd==SW_SHOWMAXIMIZED&&visible(source)==work','true')) $lifecyclePrepare $lifecycleGuest} 'static Top drop rejects request-return as actual native state completion'
function Assert-V1SessionInterruptionWiring([string]$Fixture,[string]$Cases,[string]$Prepare,[string]$Guest,[string]$Remote){
    if(-not $Fixture.Contains('#include "platform/windows/product/v1_guest_session_interruption_case.h"') -or
       -not $Fixture.Contains('phase("TEST_session_LOCK_abort_and_UNLOCK_same_PID_Move_Resize",owned_session_interruption_case);') -or
       [regex]::Matches($Fixture,'SessionInterruptionOnly').Count -ne 6 -or
       -not $Prepare.Contains("'SessionInterruptionOnly'") -or -not $Guest.Contains("'SessionInterruptionOnly'") -or
       -not $Remote.Contains("'SessionInterruptionOnly'")){
        throw 'Session interruption requires exact guest, owner, parser, manifest and finite independent dispatch.'
    }
    foreach($value in @('settings_guest_scope(s);','s.suite!="SessionInterruptionOnly"',
        'tray_guest_owner(s,owner)','GetAncestor(owner.app,GA_ROOT)==owner.app',
        'current_session==session','product_session==session','session_interruption_desktop(owner.tid)==desktop_name',
        'wrong.birth^=1','session^1u','session_interruption_exact_active_source_capture_and_product_owner',
        'SendMessageTimeoutW(owner.app,WM_WTSSESSION_CHANGE,','locked?WTS_SESSION_LOCK:WTS_SESSION_UNLOCK',
        'static_cast<LPARAM>(session)','SMTO_ABORTIFHUNG|SMTO_ERRORONEXIT,1000,&result)',
        'interruption_end(s,g,v::End::SessionLost,false,source)',
        'e::number(ended,"\"end\":")==static_cast<unsigned>(v::End::SessionLost)',
        'e::unique_value(ended,"\"normal_up\":")=="false"','!IsWindow(overlay)',
        'transparency_resource(s,g,"thread_detached")','e::unique_value(gone,"\"detach_succeeded\":")=="true"',
        's.left&&!s.left_up_attempted&&actual_up(s,g.offset,g.generation).empty()',
        'GuestUpHookObservation observer;','e::number(head,"\"actual_left_UP_callbacks\":")==1',
        'e::number(head,"\"last_extra_information\":")==0x50425631',
        'set.frames[0].up.load()==client_ups+1','cleanup.json()',
        'inject(false,"released_TEST_UNLOCK_before_fresh_gestures")',
        'interruption_recovery(s,set,g.generation)','chain_check_product(s,pid,birth);',
        'if(*qpc>=trigger)++late','actual_OS_lock_unlock\":\"NOT_RUN',
        'old_generation_NormalUp\":false','hash(s.product)==binary','saved==expected')){
        if(-not $Cases.Contains($value)){throw ('Session injection lost actual scope, retirement, unique UP or recovery: '+$value)}
    }
    if([regex]::Matches($Cases,'\blayout_down\s*\(').Count -ne 1 -or
       [regex]::Matches($Cases,'\bbutton\(s,false\)').Count -ne 2 -or
       $Cases -match '\b(LockWorkStation|ExitWindowsEx|WTSDisconnectSession|WTSLogoffSession|SetCapture|ReleaseCapture|SetForegroundWindow|SetFocus|PostMessageW?)\s*\('){
        throw 'Session TEST may inject two exact owner notifications and one original UP, not lock Windows, forge UP or reclaim context.'
    }
    $down=$Cases.IndexOf('const auto g=layout_down(set,0,HTCAPTION,false);')
    $lock=$Cases.IndexOf('const auto trigger=inject(true,"held_Move_TEST_LOCK");')
    $gone=$Cases.IndexOf('session_interruption_real_End5_nonNormal_Gone_before_cleanup_UP')
    $up=$Cases.IndexOf('button(s,false);const auto cleanup=abort_cleanup_up_receipt')
    $unlock=$Cases.IndexOf('inject(false,"released_TEST_UNLOCK_before_fresh_gestures")')
    $recovery=$Cases.IndexOf('interruption_recovery(s,set,g.generation)')
    if($down -lt 0 -or $down -ge $lock -or $lock -ge $gone -or $gone -ge $up -or $up -ge $unlock -or $unlock -ge $recovery){
        throw 'Session TEST must preserve held Move, actual abort/Gone, original cleanup UP, unlock notification and fresh recovery order.'
    }
}
$sessionInterruption=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_session_interruption_case.h'),[Text.Encoding]::UTF8)
$sessionRemote=[IO.File]::ReadAllText((Join-Path $repository 'uat/v1-control/remote-control.ps1'),[Text.Encoding]::UTF8)
Assert-V1SessionInterruptionWiring $fixtureSource $sessionInterruption $lifecyclePrepare $lifecycleGuest $sessionRemote
Check $true 'static session TEST preserves exact owner/session, real non-NormalUp/Gone, one tagged cleanup UP and same-process Move/Resize recovery'
Reject {Assert-V1SessionInterruptionWiring $fixtureSource ($sessionInterruption.Replace('product_session==session','true')) $lifecyclePrepare $lifecycleGuest $sessionRemote} 'static session TEST rejects loss of exact product session identity'
Reject {Assert-V1SessionInterruptionWiring $fixtureSource ($sessionInterruption.Replace('interruption_end(s,g,v::End::SessionLost,false,source)','record(s,g.offset,"gesture_end",g.generation)')) $lifecyclePrepare $lifecycleGuest $sessionRemote} 'static session TEST rejects message completion substituted for actual SessionLost termination'
function Assert-V1QueueFailureWiring([string]$Fixture,[string]$Cases,[string]$Service,[string]$Header,[string]$Prepare,[string]$Guest,[string]$Remote){
    if(-not $Fixture.Contains('#include "platform/windows/product/v1_guest_queue_failure_case.h"') -or
       -not $Fixture.Contains('phase("one_shot_injected_production_queue_failure_and_public_recovery",owned_queue_failure_case);') -or
       [regex]::Matches($Fixture,'QueueFailureOnly').Count -ne 6 -or
       -not $Prepare.Contains("'QueueFailureOnly'") -or -not $Guest.Contains("'QueueFailureOnly'") -or
       -not $Remote.Contains("'QueueFailureOnly'")){throw 'Queue failure requires an exact guarded standalone suite through preparation and deployment.'}
    foreach($value in @('guest::detect_capture_scope(PANEBIND_BUILD_SHA)','scope.run_id!=run_id',
        'guest::capture_manifest_suite(manifest,"QueueFailureOnly")','pair->ready_authorized','context->matching_up_observed',
        'capture_source_current(identity)','L"PaneBind.V1.OwnedFixture."+wrun','SECURITY_MANDATORY_MEDIUM_RID',
        'EqualSid(','session!=own_session','driver_debug_sha256','driver_release_sha256',
        'callback_owner_stop?guest_callback_stop_state_:guest_queue_failure_state_',
        'state.store(2,std::memory_order_release)',
        'guest_queue_failure_observation_ready_.store(true,std::memory_order_release)',
        'origin\":\"guest_injected_failure','natural_queue_full_proven\":false',
        'const bool actual_full=next==r;','if(actual_full||injected)',
        'request_capture_stop(Removal::ContextLost);','PostMessageW(owner_,catalog_message,1,0)',
        'input_owner_start_serial_.fetch_add(1,std::memory_order_release)')){
        if(-not $Service.Contains($value)){throw ('Production one-shot lost exact proof, actual failure branch or fact: '+$value)}
    }
    $push=[regex]::Match($Service,'(?s)bool InputService::push\(.*?(?=bool InputService::pop\()').Value
    foreach($value in @('guest_queue_failure_matches(','guest_queue_failure_state_.compare_exchange_strong(armed,3)',
        'authorized_->compare_exchange_strong(authority,0)','injected=!pair->up_observed&&!pair->aborted&&!context->matching_up_observed')){
        if(-not $push.Contains($value)){throw ('Actual push must atomically consume the immutable authorized generation: '+$value)}
    }
    if($push -match 'capture_hash|detect_capture_scope|OpenProcess|GetTokenInformation|std::mutex|lock_guard|Sleep\(|WaitForSingleObject'){
        throw 'No guest proof IO, new lock, wait or polling belongs in push/hook execution.'
    }
    foreach($value in @('event.generation==permit.generation&&authority==permit.generation',
        'active_pair&&capture_ready&&!up_or_abort','same(permit.source,event.prepared->source)')){
        if(-not $Header.Contains($value)){throw ('Common actual fault predicate lost identity or UP rejection: '+$value)}
    }
    foreach($value in @('settings_guest_scope(s);','s.suite!="QueueFailureOnly"',
        'queue_failure_actual_exact_Move_before_arm','L"--guest-queue-failure "',
        'guest_queue_failure_arm','input_queue_failure','v::End::QueueFailure',
        'queue_failure_actual_independent_input_resources_gone_before_UP','interruption_audit_calls(s,g,trigger,std::nullopt)',
        'product::guest::matching_queue_failure_status(',
        'queue_failure_status(s,true,false,','queue_failure_status(s,false,false,','queue_failure_status(s,true,true,',
        'GuestUpHookObservation head;','button(s,false);','launch(s,L"--pause",true)',
        'launch(s,L"--command Pause",true)','actual_input_owner_start_serial',
        'queue_failure_recovery_actual_Move_exact','queue_failure_recovery_actual_Resize_exact',
        'layout_up(set,next,{moved});','layout_up(set,resize,{resized});',
        'one_process_lifetime_permit_already_used_or_armed','OS_silent_hook_removal\":\"NOT_RUN')){
        if(-not $Cases.Contains($value)){throw ('Queue scene lost actual first Move, failure evidence or public recovery: '+$value)}
    }
    if($Cases.Contains('PostMessageW(') -or $Cases.Contains('SendMessageW(') -or
       $Cases.Contains('geometry_history_status(s)') -or
       $Cases.Contains('SetCapture(') -or $Cases.Contains('ReleaseCapture(') -or
       [regex]::Matches($Cases,'\bbutton\(s,false\)').Count -ne 1){
        throw 'The queue scene may request one guarded failure and send only the original cleanup UP, never fake an abort or capture receipt.'
    }
}
$queueFailure=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_queue_failure_case.h'),[Text.Encoding]::UTF8)
$queueFailureService=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/input_service.cpp'),[Text.Encoding]::UTF8)
$queueFailureHeader=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/input_service.h'),[Text.Encoding]::UTF8)
$queueFailureRemote=[IO.File]::ReadAllText((Join-Path $repository 'uat/v1-control/remote-control.ps1'),[Text.Encoding]::UTF8)
Assert-V1QueueFailureWiring $fixtureSource $queueFailure $queueFailureService $queueFailureHeader $lifecyclePrepare $lifecycleGuest $queueFailureRemote
Check $true 'static exact guest one-shot uses the actual push failure branch, original UP and same-process public recovery'
Reject {Assert-V1QueueFailureWiring $fixtureSource $queueFailure $queueFailureService ($queueFailureHeader.Replace('event.generation==permit.generation&&authority==permit.generation','true')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static injected failure rejects unbound or UP-revoked generation'
Reject {Assert-V1QueueFailureWiring $fixtureSource $queueFailure ($queueFailureService.Replace('guest::detect_capture_scope(PANEBIND_BUILD_SHA)','guest::CaptureScope{true}')) $queueFailureHeader $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static injected failure rejects substituting an unverified guest proof'
function Assert-V1CallbackOwnerStopWiring([string]$Fixture,[string]$Cases,[string]$Service,[string]$Header,[string]$App,[string]$Prepare,[string]$Guest,[string]$Remote){
    if(-not $Fixture.Contains('#include "platform/windows/product/v1_guest_callback_owner_stop_case.h"') -or
       -not $Fixture.Contains('phase("actual_callback_owner_exit_and_same_process_public_recovery",owned_callback_owner_stop_case);') -or
       [regex]::Matches($Fixture,'CallbackOwnerStopOnly').Count -ne 6 -or
       -not $Prepare.Contains("'CallbackOwnerStopOnly'") -or -not $Guest.Contains("'CallbackOwnerStopOnly'") -or
       -not $Remote.Contains("'CallbackOwnerStopOnly'")){throw 'Callback-owner stop needs exact guarded preparation/parser/manifest and standalone dispatch.'}
    foreach($value in @('guest::capture_manifest_suite(manifest,"CallbackOwnerStopOnly")',
        'arm_guest_input_failure(run_id,generation,source,true,why)',
        'RegisterWaitForSingleObject(','WT_EXECUTEONLYONCE','WaitForSingleObject(guest_callback_wait_thread_,0)',
        'GetExitCodeThread(guest_callback_wait_thread_,&code)',
        'PostThreadMessageW(thread_id_.load(),guest_callback_stop_message,',
        'guest_callback_owner_stop_matches(','guest_callback_stop_state_.compare_exchange_strong(armed,3)',
        'authorized_->compare_exchange_strong(authority,0)','core::v1::End::InputUnavailable',
        'UnhookWindowsHookEx(hook_)','UnregisterHotKey(nullptr,910)','UnhookWinEvent(events_[i])',
        'RegisterRawInputDevices(&key,1,sizeof(key))','DestroyWindow(receiver_)',
        'stop_facts.shield_stop_clean=capture_stop_facts_.clean()',
        'thread_object_signaled','thread_exit_observed','callback_stop_cleanup_confirmed')){
        if(-not $Service.Contains($value)){throw ('Actual callback-owner stop lost native proof or explicit lifecycle step: '+$value)}
    }
    foreach($value in @('permit.exact_guest_verified&&permit.generation',
        'generation==permit.generation&&authority==permit.generation',
        'healthy&&available&&!paused&&active_pair&&capture_ready&&!up_or_abort',
        'actual->hwnd==permit.source.hwnd','actual->key==permit.source.key',
        'actual->pid==permit.source.pid','actual->tid==permit.source.tid','actual->born==permit.source.born')){
        if(-not $Header.Contains($value)){throw ('Callback stop predicate lost actual identity or UP/stop rejection: '+$value)}
    }
    foreach($value in @('L"--guest-callback-owner-stop"','if(!scope.enabled)return 78;',
        'a->input.request_guest_callback_owner_stop(','"guest_callback_owner_stop_arm"',
        '"input_callback_owner_stop"','"input_callback_owner_exit"',
        'guest_callback_stop_dispatch(','input.revoke_generation(generation,v::End::InputUnavailable)',
        'if(dispatch.finish_gui_gesture){finish(v::End::InputUnavailable)',
        'a->callback_owner_stop_fact(false)','a->callback_owner_stop_fact(true)')){
        if(-not $App.Contains($value)){throw ('Actual app stop dispatch lost public guarded request or observed non-NormalUp receipt: '+$value)}
    }
    foreach($value in @('settings_guest_scope(s);','s.suite!="CallbackOwnerStopOnly"',
        'callback_owner_stop_actual_exact_Move_before_request','L"--guest-callback-owner-stop "',
        'exact_guest_callback_owner_stop_posted','guest_callback_owner_stop_arm','input_callback_owner_stop','input_callback_owner_exit',
        'thread_object_signaled','thread_exit_observed','exit_code&&*exit_code!=STILL_ACTIVE',
        'mouse_unhook_attempted','mouse_unhook_return','event_unhooks(exited)',
        'hotkey_unregister_return','raw_unregister_return','receiver_destroy_return','shield_stop_clean',
        'v::End::InputUnavailable','callback_owner_stop_actual_independent_shield_gone_before_original_UP',
        'interruption_audit_calls(s,g,trigger_qpc,std::nullopt)',
        'queue_failure_status(s,true,false,','queue_failure_status(s,false,false,','queue_failure_status(s,true,true,',
        'GuestUpHookObservation head;','button(s,false);','client_ups+1',
        'launch(s,L"--pause",true)','launch(s,L"--command Pause",true)',
        'one_process_lifetime_permit_already_used_or_armed','*initial_serial+1',
        'callback_owner_stop_recovery_actual_Move_exact','callback_owner_stop_recovery_actual_Resize_exact',
        'layout_up(set,next,{moved});','layout_up(set,resize,{resized});',
        'product_LL_UP_required_after_actual_owner_exit\":false','OS_silent_hook_removal\":\"NOT_RUN')){
        if(-not $Cases.Contains($value)){throw ('Callback TEST lost actual native exit, one original UP or public recovery: '+$value)}
    }
    if($Cases -match 'PostMessageW\(|SendMessageW\(|SetCapture\(|ReleaseCapture\(|TerminateThread\(|SuspendThread\(' -or
       $Cases.Contains('geometry_history_status(s)') -or
       [regex]::Matches($Cases,'\bbutton\(s,false\)').Count -ne 1){
        throw 'Callback TEST must use public fault requests, one original UP and actual API receipts, not private aborts, capture changes or fake native events.'
    }
}
$callbackOwnerStop=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_callback_owner_stop_case.h'),[Text.Encoding]::UTF8)
$callbackOwnerStopApp=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/product_app.cpp'),[Text.Encoding]::UTF8)
Assert-V1CallbackOwnerStopWiring $fixtureSource $callbackOwnerStop $queueFailureService $queueFailureHeader $callbackOwnerStopApp $lifecyclePrepare $lifecycleGuest $queueFailureRemote
Check $true 'static explicit callback-owner stop requires actual thread HANDLE exit, native API teardown, independent shield cleanup and same-process public recovery'
Reject {Assert-V1CallbackOwnerStopWiring $fixtureSource ($callbackOwnerStop.Replace('exit_code&&*exit_code!=STILL_ACTIVE','true')) $queueFailureService $queueFailureHeader $callbackOwnerStopApp $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static callback-owner stop rejects a mere posted message or healthy=false instead of actual exited thread'
Reject {Assert-V1CallbackOwnerStopWiring $fixtureSource $callbackOwnerStop $queueFailureService ($queueFailureHeader.Replace('healthy&&available&&!paused&&active_pair&&capture_ready&&!up_or_abort','true')) $callbackOwnerStopApp $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static callback-owner stop rejects stale, UP-observed or non-authorized generation'
Reject {Assert-V1CallbackOwnerStopWiring $fixtureSource $callbackOwnerStop $queueFailureService $queueFailureHeader $callbackOwnerStopApp ($lifecyclePrepare.Replace("'CallbackOwnerStopOnly'","'DisabledCallbackOwnerStop'")) $lifecycleGuest $queueFailureRemote} 'static callback-owner stop rejects missing exact suite protection'
function Assert-V1ArbitrationRecoveryWiring([string]$Fixture,[string]$Cases,[string]$Prepare,[string]$Guest){
    if([regex]::Matches($Fixture,'ArbitrationRecoveryOnly').Count -ne 7 -or
       -not $Fixture.Contains('phase("single_nonstop_command_abort_and_fresh_recovery",owned_arbitration_remaining_case);') -or
       -not $Prepare.Contains("'ArbitrationRecoveryOnly'") -or -not $Guest.Contains("'ArbitrationRecoveryOnly'")){
        throw 'Arbitration recovery requires exact independent/parser/manifest/script dispatch, not a force switch.'
    }
    foreach($value in @('settings_guest_scope(s);','const bool recovery_only=s.suite=="ArbitrationRecoveryOnly";',
        'if(!recovery_only){','plain_generation=g.generation;','REUSED_ORIGINAL_IDENTITY_NOT_RUN_THIS_SUITE',
        'const bool abort_gone=wait([&]{return gone_valid(actual_gone(g));},500);',
        'GetForegroundWindow()==source','GetClientRect(source,&client)',
        'cursor(s,destination);','GetCursorPos(&actual_cursor)','WindowFromPoint(actual_cursor)',
        'hit==source&&root==source','hit_pid==source_pid&&hit_tid==set.thread&&source_tid==set.thread',
        'static_cast<int>(region)==HTCLIENT','!after_gui.hwndCapture&&!after_gui.hwndMoveSize&&!after_gui.hwndMenuOwner',
        'GuestUpHookObservation cleanup_head;','head_stopped&&s.left_up_attempted&&s.left_up_send_qpc',
        'guest_hook_is_not_product_or_window_UP_receipt','old_generation_NormalUp_inferred',
        'const auto following=layout_down(set,0,HTCAPTION,false);')){
        if(-not $Cases.Contains($value)){throw ('Arbitration recovery lost exact original-UP routing or following operation: '+$value)}
    }
    if([regex]::Matches($Cases,'\bbutton\(s,false\)').Count -ne 1 -or
       $Cases.Contains('SetForegroundWindow(') -or $Cases.Contains('SetCapture(') -or
       $Cases.Contains('ReleaseCapture(') -or $Cases.Contains('PostMessage(')){
        throw 'Recovery may route the one original TEST UP to its already-foreground exact client, never refocus/capture/replay.'
    }
}
$arbitrationRecovery=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_arbitration_remaining_case.h'),[Text.Encoding]::UTF8)
Assert-V1ArbitrationRecoveryWiring $fixtureSource $arbitrationRecovery $lifecyclePrepare $lifecycleGuest
Check $true 'static arbitration recovery has one exact-client cleanup UP, guest-only scope and same-instance following operation'
Reject {Assert-V1ArbitrationRecoveryWiring $fixtureSource ($arbitrationRecovery.Replace('hit==source&&root==source','true')) $lifecyclePrepare $lifecycleGuest} 'static arbitration recovery rejects removal of the actual client root check'
Reject {Assert-V1ArbitrationRecoveryWiring $fixtureSource $arbitrationRecovery ($lifecyclePrepare.Replace("'ArbitrationRecoveryOnly'","'DisabledRecovery'")) $lifecycleGuest} 'static arbitration recovery rejects missing packager scope'
function Assert-V1TransparencyRemainderWiring([string]$Fixture,[string]$Cases,[string]$Layout,[string]$Prepare,[string]$Guest){
    foreach($value in @('#include "platform/windows/product/v1_guest_transparency_remaining_case.h"',
        'phase("temporary_alpha_real_FocusLost_and_successor",owned_transparency_focus_remaining_case);',
        'NOT_RUN_HERE_REUSED_B6C8_ORIGINAL_IDENTITY','b6c832c8651e4c24a729b5a868b67d40',
        'bbc5b1e539bc62ae5a760d3c2f2d139efb4890a6','c69620feb0ceaecb37f1900e69621e9fc47c815c',
        'current_run_scope\":\"FocusLost_and_same_instance_successor_only',
        'new_product_per_pixel_PASS_inferred\":false',
        's.suite!="TransparencyRemainderOnly"','s.suite=="TransparencyRemainderOnly"',
        'else if(value==L"TransparencyRemainderOnly")s.suite="TransparencyRemainderOnly";')){
        if(-not $Fixture.Contains($value)){throw ('Transparency remainder suite lost actual wiring: '+$value)}
    }
    if(-not $Prepare.Contains("'TransparencyRemainderOnly'") -or -not $Guest.Contains("'TransparencyRemainderOnly'")){
        throw 'Transparency remainder requires both exact packager and guest suite declarations.'
    }
    if($Fixture.Contains('phase("temporary_alpha_actual_per_pixel_limitation",owned_transparency_unsupported_remaining_case);')){
        throw 'The focused rerun must not silently rerun the unchanged per-pixel phase or relabel it as new-product evidence.'
    }
    foreach($value in @('settings_guest_scope(s);','s.suite!="TransparencyRemainderOnly"','config.drag_alpha=190;',
        'transparency_owned_visual_setup(set,0,173,true);','SetForegroundWindow(other)',
        'interruption_end(s,g,v::End::FocusLost,false,source)','transparency_gone(s,set,g);',
        'abort_cleanup_up_receipt(s,g.offset,g.generation)','next.generation>g.generation',
        'transparency_owned_per_pixel_setup(set,0);','GetClassLongPtrW(h,GCL_STYLE)',
        'transparency_actual_unreadable_alpha_not_overwritten','!visual_requests',
        'NOT_PROVEN_BY_NOTICE_LOG','transparency_actual_limitation_hint(set,g,moved[0]);',
        'transparency_real_UP_hides_limitation_hint','visible_limitation_feedback\":\"ACTUAL_WINDOW_AND_SCREEN_PIXELS_REQUIRE_VISUAL_REVIEW')){
        if(-not $Cases.Contains($value)){throw ('Transparency remainder lost actual observation/limit: '+$value)}
    }
    foreach($value in @('state.suite!="TransparencyRemainderOnly"','original.style|=CS_OWNDC;',
        'L".NoLayeredOwnDC"','storage->alias_unregistered','GetClassLongPtrW(frames[1].hwnd,GCL_STYLE)&CS_OWNDC')){
        if(-not $Layout.Contains($value)){throw ('Transparency CS_OWNDC alias lost exact ownership: '+$value)}
    }
    $visualSetup=[regex]::Match($Cases,'(?s)void transparency_owned_visual_setup\(.*?(?=void transparency_owned_visual_exact\()').Value
    $pixelSetup=[regex]::Match($Cases,'(?s)Rect transparency_owned_per_pixel_setup\(.*?(?=void transparency_restore\()').Value
    if([string]::IsNullOrWhiteSpace($visualSetup) -or [string]::IsNullOrWhiteSpace($pixelSetup)){
        throw 'Actual fixture visual setup must have narrow named boundaries, not an unbounded source mutation exemption.'
    }
    foreach($value in @('if(!set.owns(index))throw','if(set.owns(index)){style_attempted=true',
        'if(style_attempted&&!style_error&&set.owns(index))','if(alpha_return&&set.owns(index))')){
        if(-not $visualSetup.Contains($value)){throw ('Owned visual setup lost an exact identity recheck: '+$value)}
    }
    foreach($value in @('if(!set.owns(index))throw','class_error||(class_style&(CS_OWNDC|CS_CLASSDC))',
        'width>2048||height>2048','if(set.owns(index)){style_attempted=true',
        'if(style_attempted&&!style_error&&set.owns(index))','0xff80a0c0u',
        'UpdateLayeredWindow(h,screen,&destination,&size,memory,&origin,0,&blend,ULW_ALPHA)',
        'update_attempted&&updated&&opaque_hit&&actual_baseline&&layered&&!read_alpha&&foreground_exact',
        'EqualRect(&before,&after)','visible(h)==visible_after;',
        'return *visible_after;',
        '~OwnGdi(){close();}','gdi.close();','SelectObject(memory,previous)',
        'DeleteDC(memory)','DeleteObject(bitmap)','ReleaseDC(nullptr,screen)')){
        if(-not $pixelSetup.Contains($value)){throw ('Per-pixel precondition lost real ownership, API/readback or GDI cleanup: '+$value)}
    }
    if(-not $Cases.Contains('initial[0]=transparency_owned_per_pixel_setup(set,0);') -or
       $pixelSetup.Contains('visible(h)==visible_before')){
        throw 'Per-pixel TEST must freeze its actual post-layering geometry before DOWN, not assume pre-layered DWM margins.'
    }
    $productCases=$Cases.Replace($visualSetup,'').Replace($pixelSetup,'')
    if($Cases.Contains('PostMessageW(') -or $productCases.Contains('SetWindowLongPtrW(') -or
       $productCases.Contains('SetLayeredWindowAttributes(') -or $productCases.Contains('UpdateLayeredWindow(') -or
       $productCases.Contains('SetWindowPos(')){
        throw 'Only two exact owned TEST setup functions may set fixture visual state; real product cases cannot pre-mutate or fake messages.'
    }
}
$transparencyRemainder=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_transparency_remaining_case.h'),[Text.Encoding]::UTF8)
$transparencyLayout=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_layout_cases.h'),[Text.Encoding]::UTF8)
Assert-V1TransparencyRemainderWiring $fixtureSource $transparencyRemainder $transparencyLayout $lifecyclePrepare $lifecycleGuest
Check $true 'static focused transparency rerun retains real FocusLost successor and separately reuses original b6c8 per-pixel identity'
Reject {Assert-V1TransparencyRemainderWiring $fixtureSource ($transparencyRemainder.Replace('next.generation>g.generation','true')) $transparencyLayout $lifecyclePrepare $lifecycleGuest} 'static transparency rejects losing the fresh successor generation'
Reject {Assert-V1TransparencyRemainderWiring $fixtureSource ($transparencyRemainder.Replace('if(!set.owns(index))throw','if(false)throw')) $transparencyLayout $lifecyclePrepare $lifecycleGuest} 'static transparency rejects visual setup without exact fixture ownership'
Reject {Assert-V1TransparencyRemainderWiring ($fixtureSource.Replace('NOT_RUN_HERE_REUSED_B6C8_ORIGINAL_IDENTITY','PASS_CURRENT_PRODUCT')) $transparencyRemainder $transparencyLayout $lifecyclePrepare $lifecycleGuest} 'static focused transparency rejects rebinding the historical per-pixel result to this product'
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
function Assert-V1FixedSizeOnlyWiring([string]$Fixture,[string]$Resize,[string]$Prepare,[string]$Guest){
    $start=$Fixture.IndexOf('    if(s.suite=="FixedSizeResizeOnly"){',[StringComparison]::Ordinal)
    $end=$Fixture.IndexOf('    if(s.suite=="FixedAndTerminalOnly"){',[StringComparison]::Ordinal)
    if($start -lt 0 -or $end -le $start){throw 'Fixed-size-only branch must return before the original fixed/terminal branch.'}
    $branch=$Fixture.Substring($start,$end-$start)
    foreach($value in @('phase("owned_fixed_size_Resize_no_write_then_Move",owned_fixed_size_resize_case);',
        'cleanup(s);','fixed_size_resize_only_summary','return;')){
        if(-not $branch.Contains($value)){throw ('Fixed-size-only path lost actual component or cleanup: '+$value)}
    }
    if([regex]::Matches($branch,'phase\(').Count -ne 1 -or
       $branch.Contains('owned_active_terminal_burst_case') -or $branch.Contains('owned_resize_magnet_case')){
        throw 'Fixed-size-only must not repeat the terminal burst or eight-direction component.'
    }
    if(-not $Fixture.Contains('else if(value==L"FixedSizeResizeOnly")s.suite="FixedSizeResizeOnly";') -or
       [regex]::Matches($Fixture,'s\.suite!="FixedSizeResizeOnly"').Count -ne 2 -or
       -not $Fixture.Contains('||s.suite=="FixedSizeResizeOnly"||') -or
       -not $Resize.Contains('(s.suite!="FixedAndTerminalOnly"&&s.suite!="FixedSizeResizeOnly")||!original.enabled') -or
       -not $Prepare.Contains("'FixedAndTerminalOnly','FixedSizeResizeOnly'") -or
       -not $Guest.Contains("'FixedAndTerminalOnly','FixedSizeResizeOnly'")){
        throw 'Fixed-size-only must retain exact parser, manifest/entry, reusable case and both package suite guards.'
    }
}
Assert-V1FixedSizeOnlyWiring $fixtureSource $resizeMagnet $lifecyclePrepare $lifecycleGuest
Check $true 'static fixed-size-only reuses one actual fixed Resize/UP/zero-call/following Move path without D29 or eight-direction replay'
Reject {Assert-V1FixedSizeOnlyWiring ($fixtureSource.Replace('if(s.suite=="FixedSizeResizeOnly"){','if(s.suite=="FixedSizeResizeOnly"){phase("unexpected_terminal",owned_active_terminal_burst_case);')) $resizeMagnet $lifecyclePrepare $lifecycleGuest} 'static fixed-size-only rejects adding the closed terminal burst to its narrow branch'
Reject {Assert-V1FixedSizeOnlyWiring $fixtureSource ($resizeMagnet.Replace('&&s.suite!="FixedSizeResizeOnly"','')) $lifecyclePrepare $lifecycleGuest} 'static fixed-size-only rejects a disconnected reusable-case guard'
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
function Assert-V1SystemShakeWiring([string]$Fixture,[string]$Cases,[string]$Prepare,[string]$Guest,[string]$Remote){
    foreach($value in @('#include "platform/windows/product/v1_guest_system_shake_case.h"',
        'else if(s.suite=="SystemShakeCoexistenceOnly"){','system_shake_coexistence_flow(s);',
        'else if(value==L"SystemShakeCoexistenceOnly")s.suite="SystemShakeCoexistenceOnly";',
        's.suite!="TransparencyRemainderOnly"&&s.suite!="SystemShakeCoexistenceOnly"')){
        if(-not $Fixture.Contains($value)){throw ('System Shake lost guarded standalone production-independent wiring: '+$value)}
    }
    foreach($script in @($Prepare,$Guest,$Remote)){
        if(-not $script.Contains("'SystemShakeCoexistenceOnly'")){throw 'System Shake requires exact preparation, guest and remote manifest suite whitelists.'}
    }
    foreach($value in @('s.suite!="SystemShakeCoexistenceOnly"','!medium(GetCurrentProcess())',
        'marker(s.output/L"panebind-v1-output.marker",s.run)',
        'else if(s.process||s.product_pid||FindWindowW(L"PaneBind.V1.Application",L"PaneBind"))',
        'complete&&matches==0','this_flow_product_create_requests',
        'NoWindowMinimizingShortcuts','DisallowShaking','RRF_RT_REG_DWORD',
        'missing_value_implies_enabled','raw_preference_is_not_Settings_UI_proof',
        'BLOCKED_DISABLED','BLOCKED_NOT_OBSERVED_UNKNOWN','AVAILABLE_ACTUAL_NATIVE_EFFECT',
        'actual_minimization=actual_minimization||(iconic[0]&&iconic[1]);',
        's.frames[0].starts.load()==starts+1','gui.hwndMoveSize==h&&gui.hwndCapture==h',
        'GuestUpHookObservation observer;','number(hook,"\"last_extra_information\":")==0x50425631',
        's.frames[0].native_end_qpc.load()>=s.left_up_send_qpc',
        'phase(s,0,g,offsets[n],0,translated(*start,offsets[n],0)',
        'visual_exact(s,h,alpha,top','up(s,0,g,translated(*start,offsets.back(),0));',
        'system_shake_no_native_minimizing_side_effect','system_shake_no_extra_visual_mutation_after_NormalUp',
        'visual_gui_save(s,original_settings);','owned_system_shake_coexistence_case(s,native_configuration);cleanup(s);',
        'positive_shake_offsets(h)','settings_before==native_configuration')){
        if(-not $Cases.Contains($value)){throw ('System Shake lost actual native control, unknown/config or product UP facts: '+$value)}
    }
    $native=$Cases.IndexOf('const bool available=system_shake_native_control(s,native_configuration);')
    $start=$Cases.IndexOf('launch(s,L"--diagnostic",false)')
    if($native -lt 0 -or $start -le $native -or $Cases -match '\b(RegSetValue\w*|RegDelete\w*|SystemParametersInfo\w*|SetCapture|ReleaseCapture|AttachThreadInput|PostMessage\w*|SendMessage\w*)\s*\('){
        throw 'System Shake cannot replace genuine product-absent control with a paused/hooked product or write settings/input authority.'
    }
}
$systemShake=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_system_shake_case.h'),[Text.Encoding]::UTF8)
Assert-V1SystemShakeWiring $fixtureSource $systemShake $lifecyclePrepare $lifecycleGuest $queueFailureRemote
Check $true 'static system Shake keeps actual product-absent control, explicit unknown policy state and real product gesture cleanup'
Reject {Assert-V1SystemShakeWiring $fixtureSource ($systemShake.Replace('actual_minimization=actual_minimization||(iconic[0]&&iconic[1]);','actual_minimization=true;')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static system Shake rejects fabricated native capability'
Reject {Assert-V1SystemShakeWiring $fixtureSource ($systemShake.Replace('else if(s.process||s.product_pid||FindWindowW(L"PaneBind.V1.Application",L"PaneBind"))','else if(false)')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static system Shake rejects native control with a product still installed or paused'
function Assert-V1RulesPathWiring([string]$Fixture,[string]$Cases,[string]$Prepare,[string]$Guest,[string]$Remote){
    if(-not $Fixture.Contains('#include "platform/windows/product/v1_guest_rules_path_case.h"') -or
       -not $Fixture.Contains('phase("actual_public_rules_same_basename_distinct_image_paths",owned_rules_path_case);') -or
       [regex]::Matches($Fixture,'RulesPathOnly').Count -ne 7){throw 'Path rules must be an exact independent suite through parser, admission and production dispatch.'}
    foreach($script in @($Prepare,$Guest,$Remote)){
        if(-not $script.Contains("'RulesPathOnly'")){throw 'Path rules require exact preparation, guest and deployment suite whitelists.'}
    }
    $child=$Fixture.IndexOf('if(count>=2&&std::wstring_view(args[1])==L"--rule-test-window")')
    if($child -lt 0 -or $child -ge $Fixture.IndexOf('if(count==2&&std::wstring_view(args[1])==L"--build-identity")') -or
       -not $Fixture.Contains('int result=78;try{result=rules_path_child(count,args);}catch(...){}LocalFree(args);return result;')){
        throw 'Copied TEST target must reject the ordinary host before normal driver/window/identity entry.'
    }
    foreach($value in @('if(count!=7)return 78;','!equal(user.data(),L"WDAGUtilityAccount")||!medium(GetCurrentProcess())',
        'input_actor_parent_pid()!=pid','GetProcessIdOfThread(parent_thread.value)!=pid','process_birth(parent.value)!=birth',
        '!medium(parent.value)||!input_actor_same_user(parent.value)||!input_actor_desktop(tid,pid)',
        'marker(input/L"panebind-v1-input.marker",run)','marker(output/L"panebind-v1-output.marker",run)',
        'portable_field(package,"guest_suite")!="RulesPathOnly"','portable_field(identity,"driver_source_commit")!=PANEBIND_BUILD_SHA',
        'image_hash!=hash(parent_name.data())','debug?"driver_debug_sha256":"driver_release_sha256"',
        'base/L"RuleTargets"/slot','rules_path_plain_image(self)','MsgWaitForMultipleObjectsEx(1,handles',
        'GetTickCount64()+180000','WM_CLOSE','process_handle_signaled','forced_termination')){
        if(-not $Cases.Contains($value)){throw ('Path rule child lost exact immutable identity or bounded lifecycle: '+$value)}
    }
    $create=$Cases.IndexOf('const HWND window=CreateWindowExW(')
    $proof=$Cases.IndexOf('image_hash!=portable_field(identity,debug?"driver_debug_sha256":"driver_release_sha256")')
    if($proof -lt 0 -or $create -le $proof -or $Cases.Contains('TerminateProcess(') -or $Cases.Contains('std::ofstream') -or
       $Cases -match '\b(SendInput|WriteFile|SetWindowPos|SetWindowLongPtr\w*|SetLayeredWindowAttributes)\s*\('){
        throw 'Path rule TEST cannot mutate settings directly, inject child input or replace exact ordinary application identity.'
    }
    foreach($value in @('settings_guest_scope(s);','s.suite!="RulesPathOnly"','a.image.filename()==b.image.filename()',
        '!equal(a.image.wstring(),b.image.wstring())','settings_guest_activate(s,gui,GetDlgItem(gui_page(s,gui),115))',
        'settings_guest_acknowledge(s,gui,','gui_save_enter(s,gui);',
        'excluded==expected','e::number(placement,"\"key\":")==source_key','e::number(receipt,"\"key\":")==source_key',
        '*after_calls==*before_calls+(allowed?1:0)','visible(other.hwnd)==other_before',
        'geometry_history_positioning(other.hwnd)==other_position','!claims&&!placements&&actual==before&&after_position==before_position',
        'rules_path_same_name_B_other_path_still_actual_Move','rules_path_basename_rule_A_real_rejection',
        'rules_path_basename_rule_B_real_rejection','rules_path_per_app_command_mask_A_zero_calls',
        'rules_path_A_final_public_rule_removal_recovery','a.close(),b_closed=b.close()','rules_path_original_configuration_restored')){
        if(-not $Cases.Contains($value)){throw ('Path rules lost actual GUI, independent target, rejection or recovery facts: '+$value)}
    }
    foreach($value in @("if(`$Suite -ceq 'RulesPathOnly')",'[IO.File]::Copy($binary.fixture,$copy,$false)',
        '$ruleTargetPayloads+=$relative','$payloadFiles+=$ruleTargetPayloads')){
        if(-not $Prepare.Contains($value)){throw ('Path rules require exact current D/R driver copies in the readonly package: '+$value)}
    }
    foreach($value in @('RuleTargets\A\panebind-rule-app.exe','RuleTargets\B\panebind-rule-app.exe',
        'Debug\RuleTargets\A\panebind-rule-app.exe','Debug\RuleTargets\B\panebind-rule-app.exe',
        '$ruleImage[0].sha256 -cne $driverFile[0].sha256','[long]$ruleImage[0].size -ne [long]$driverFile[0].size')){
        if(-not $Guest.Contains($value)){throw ('Path rules require four exact readonly files bound to corresponding driver bytes: '+$value)}
    }
}
$rulesPath=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_rules_path_case.h'),[Text.Encoding]::UTF8)
Assert-V1RulesPathWiring $fixtureSource $rulesPath $lifecyclePrepare $lifecycleGuest $queueFailureRemote
Check $true 'static path rules retain exact guest child images, public GUI edits and same-name different-path actual command contrasts'
Reject {Assert-V1RulesPathWiring $fixtureSource ($rulesPath.Replace('image_hash!=hash(parent_name.data())','false')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static path rules reject mismatched controller and child image bytes'
Reject {Assert-V1RulesPathWiring $fixtureSource ($rulesPath.Replace('visible(other.hwnd)==other_before','true')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static path rules reject ignoring operations on the nonselected sibling'
Reject {Assert-V1RulesPathWiring $fixtureSource ($rulesPath.Replace('e::number(placement,"\"key\":")==source_key','true')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static path rules reject unbound positive placement identity'
function Assert-V1MediumDesktopEnvironment([string]$Fixture,[string]$Cases,[string]$Prepare,[string]$Guest,[string]$Remote){
    foreach($value in @('#include "platform/windows/product/v1_guest_medium_desktop_environment_case.h"',
        'else if(s.suite=="MediumDesktopEnvironmentOnly"){','medium_desktop_environment_flow(s);',
        'else if(value==L"MediumDesktopEnvironmentOnly")s.suite="MediumDesktopEnvironmentOnly";',
        's.suite!="SystemShakeCoexistenceOnly"&&s.suite!="MediumDesktopEnvironmentOnly"',
        's.suite!="NativeClientsOnly"&&s.suite!="MediumDesktopEnvironmentOnly"')){
        if(-not $Fixture.Contains($value)){throw ('Medium desktop lost standalone, no-product guarded wiring: '+$value)}
    }
    foreach($script in @($Prepare,$Guest,$Remote)){
        if(-not $script.Contains("'MediumDesktopEnvironmentOnly'")){throw 'Medium desktop requires exact preparation, guest and deployment suite admission.'}
    }
    foreach($value in @('(s.suite!="MediumDesktopEnvironmentOnly"&&!ui_only&&!native_only)||!desktop()',
        '!equal(user.data(),L"WDAGUtilityAccount")||!medium(GetCurrentProcess())||s.process||s.product_pid',
        'marker(fs::path(L"C:\\PaneBindV1\\Input")/L"panebind-v1-input.marker",s.run)',
        'matching_existing_names','open_error!=ERROR_FILE_NOT_FOUND','CreateDesktopW(name.c_str(),nullptr,nullptr,0,access,nullptr)',
        'SetThreadDesktop(created)','startup.lpDesktop=requested_desktop.data()',
        'CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,0,',
        'input_actor_same_user(child)','session&&session==own_session','equal(image,executable.wstring())',
        'WaitForInputIdle(child,idle_budget)','EnumDesktopWindows(target,','first.windows==second.windows',
        'GetThreadDesktop(out.tid)','GetUserObjectInformationW(out.thread_desktop,UOI_NAME,',
        'GetThreadDesktop_attempted','GetThreadDesktop_error','GetUserObjectInformation_name_error',
        'out.rect_return=GetWindowRect(h,&out.rectangle)!=FALSE',
        'GetWindowRect_attempted','GetWindowRect_return','GetWindowRect_error',
        'out.known=h&&out.pid&&out.tid&&class_known&&out.rect_return&&',
        'out.desktop_conflict=out.name_query_return&&!out.desktop_name.empty()&&!equal(out.desktop_name,desktop_name)',
        'WaitForSingleObject(process,0)==WAIT_TIMEOUT&&!out.desktop_conflict',
        'rectangle.left<rectangle.right&&rectangle.top<rectangle.bottom',
        '(rect_return?rect({rectangle.left,rectangle.top,rectangle.right,rectangle.bottom}):"null")',
        'actual_created_object_name','!equal(created_name,name)',
        'unknown_zero_baseline=!baseline.complete&&baseline.error==ERROR_SUCCESS&&baseline.windows.empty()',
        'input_handle_comparison=ui_request||(native_request&&native_request->client_input)',
        'access=observer_access|(input_handle_comparison?MAXIMUM_ALLOWED:0)',
        'effective_granted_mask',
        '!baseline.windows.empty()||(!baseline.complete&&!unknown_zero_baseline)',
        'event_hook=SetWinEventHook(EVENT_OBJECT_CREATE,EVENT_OBJECT_SHOW,nullptr,',
        'medium_desktop_window_event,0,0,WINEVENT_OUTOFCONTEXT)',
        'context->facts.size()','pid!=context->child_pid',
        'observation_deadline=observation_begin+10000','observation_deadline-before_idle',
        'observe("initial")','observe("child_window_event")','observe("deadline_final")',
        'MsgWaitForMultipleObjectsEx(1,&child,static_cast<DWORD>(observation_deadline-now),QS_ALLINPUT,MWMO_INPUTAVAILABLE)',
        'event_samples<16','buffered_emit_tick_is_not_native_call_tick',
        'out.event_hook_stopped=UnhookWinEvent(event_hook)!=FALSE','medium_desktop_window_events=nullptr',
        'restore_attempted=out.event_hook_stopped',
        'observation.enumerated_ready&&observation.event_hook_stopped&&observation.thread_restored',
        'held_desktop_exact=created&&equal(enumerated_desktop_before,name)&&equal(enumerated_desktop_after,name)',
        'bool stable=first.complete&&second.complete&&held_desktop_exact&&shell_before&&shell_before==shell_after',
        'const bool in_second=std::find(second.windows.begin(),second.windows.end(),facts[n].hwnd)!=second.windows.end()',
        'readonly_root=first.complete&&second.complete&&held_desktop_exact&&in_second&&facts[n].readonly_same(fresh)',
        'if(readonly_root&&fresh.hwnd==shell_after)',
        'if(readonly_root&&fresh.type==L"Shell_TrayWnd")',
        'current_session==own_session','exact_tray==1&&shell_root!=tray_root',
        'exact_shell==1&&exact_tray==1','SetThreadDesktop(original)','CloseDesktop(created)',
        'before.same(after)&&desktop()','s.result=accepted?0:78;',
        'accepted=ready&&(!ui_only||observation.ui_completed)',
        'READY_ENUMERATED_ONLY','ordinary_full_desktop_acceptance','NOT_PROVEN','process_cleanup_PASS_inferred')){
        if(-not $Cases.Contains($value)){throw ('Medium desktop lost actual bounded, exact process/window or cleanup boundary: '+$value)}
    }
    if($Cases -match '\b(SendInput|SwitchDesktop|SetProcessWindowStation|SetUserObjectSecurity|SetSecurityInfo|SetTokenInformation|CreateRestrictedToken|SetForegroundWindow|SetFocus|TerminateProcess|PostMessage\w*|SendMessage\w*|Sleep)\s*\(' -or
       $Cases.Contains('launch(s,') -or [regex]::Matches($Cases,'WaitForInputIdle\(child,idle_budget\)').Count -ne 1){
        throw 'Medium desktop is one no-input environment observation, not a new input route, altered security or forced Shell takeover.'
    }
}
$mediumDesktopEnvironment=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_medium_desktop_environment_case.h'),[Text.Encoding]::UTF8)
Assert-V1MediumDesktopEnvironment $fixtureSource $mediumDesktopEnvironment $lifecyclePrepare $lifecycleGuest $queueFailureRemote
Check $true 'static Medium desktop keeps exact original guest guards, one inherited-security object/child and independent actual Shell/Tray observation'
Reject {Assert-V1MediumDesktopEnvironment $fixtureSource ($mediumDesktopEnvironment.Replace('session&&session==own_session','true')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static Medium desktop rejects adopting a process in an unverified session'
Reject {Assert-V1MediumDesktopEnvironment $fixtureSource ($mediumDesktopEnvironment.Replace('exact_shell==1&&exact_tray==1','true')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static Medium desktop rejects interpreting process creation as actual Shell/Tray readiness'
Reject {Assert-V1MediumDesktopEnvironment $fixtureSource ($mediumDesktopEnvironment.Replace('observation_deadline=observation_begin+10000','observation_deadline=UINT64_MAX')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static Medium desktop rejects unbounded event readiness observation'
Reject {Assert-V1MediumDesktopEnvironment $fixtureSource ($mediumDesktopEnvironment.Replace('restore_attempted=out.event_hook_stopped','restore_attempted=true')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static Medium desktop rejects restoring the thread before confirmed own hook unload'
Reject {Assert-V1MediumDesktopEnvironment $fixtureSource ($mediumDesktopEnvironment.Replace('out.rect_return=GetWindowRect(h,&out.rectangle)!=FALSE','out.rect_return=!out.desktop_name.empty()&&GetWindowRect(h,&out.rectangle)!=FALSE')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static actual window reader rejects reintroducing foreign-desktop UNKNOWN short-circuit before native geometry read'
Reject {Assert-V1MediumDesktopEnvironment $fixtureSource ($mediumDesktopEnvironment.Replace('if(readonly_root&&fresh.hwnd==shell_after)','if(readonly_root&&!fresh.desktop_name.empty()&&fresh.hwnd==shell_after)')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static actual Shell counter rejects converting UNKNOWN foreign-thread desktop query into zero readonly Shell roots'
Reject {Assert-V1MediumDesktopEnvironment $fixtureSource ($mediumDesktopEnvironment.Replace('input_handle_comparison=ui_request||(native_request&&native_request->client_input)','input_handle_comparison=true')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static input-handle comparison cannot silently broaden the original no-input desktop observation'
Reject {Assert-V1MediumDesktopEnvironment $fixtureSource ($mediumDesktopEnvironment.Replace('readonly_root=first.complete&&second.complete&&held_desktop_exact&&in_second&&facts[n].readonly_same(fresh)','readonly_root=facts[n].readonly_same(fresh)')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static readonly Shell roots still require complete membership in the same held created desktop twice'
foreach($script in @($lifecyclePrepare,$lifecycleGuest,$queueFailureRemote)){
    Check ($script.Contains("'CurrentCoreChainOnly'")) 'static current core chain retains exact preparation, guest and deployment admission'
}
Check ($fixtureSource.Contains('owned_finite_I11_chain(s,true);') -and
    $fixtureSource.Contains('CurrentCoreChainOnly_requires_Release') -and
    $fixtureSource.Contains('independent_entry_ready(s,"current_core_chain_only.after",false)') -and
    $fixtureSource.Contains('I11_900_seconds') -and $fixtureSource.Contains('whole_version_acceptance')) 'static current core chain is explicit Release two-round reuse, not an independent 900-second or whole-version PASS'
function Assert-V1SettingsControlsRemaining([string]$Fixture,[string]$Cases,[string]$Prepare,[string]$Guest,[string]$Remote){
    foreach($value in @('#include "platform/windows/product/v1_guest_settings_controls_remaining_case.h"',
        's.suite!="SettingsValidationOnly"&&s.suite!="SettingsControlsRemainderOnly"',
        'else if(value==L"SettingsControlsRemainderOnly")s.suite="SettingsControlsRemainderOnly";',
        'phase("changed_preview_color_alpha_and_four_numeric_restart",owned_settings_preview_controls_case);',
        'phase("changed_adjacency_controls_actual_Ctrl_membership",owned_settings_adjacency_control_case);',
        'phase("changed_step_control_actual_public_Move_and_Resize",owned_settings_step_control_case);')){
        if(-not $Fixture.Contains($value)){throw ('Remaining settings controls lost exact standalone admission or independent actual phases: '+$value)}
    }
    foreach($script in @($Prepare,$Guest,$Remote)){
        if(-not $script.Contains("'SettingsControlsRemainderOnly'")){throw 'Remaining settings controls require exact package, guest and deployment suite admission.'}
    }
    foreach($value in @('s.suite!="SettingsControlsRemainderOnly"||s.left||s.control',
        'settings_invalid_number_input(s,gui,target,std::to_wstring(value))','gui_save_enter(s,gui);',
        'saved==wanted&&saved.generation>before.generation','cleanup(s);if(s.process)',
        'hash(s.product)==binary&&chain_settings(s)==wanted','settings_controls_restart_and_readback(s,saved);',
        'constexpr COLORREF fill=RGB(32,96,160);constexpr int alpha=255;',
        'chain_preview_exact(s,set.frames[0].hwnd,half)','GetLayeredWindowAttributes(overlay,&ignored_key,&actual_alpha,&flags)',
        'flags==LWA_ALPHA&&!GetUpdateRect(overlay,nullptr,FALSE)','GetPixel(screen,sample.x,sample.y)',
        'released&&pixel==fill','visible(source)==raw&&GetForegroundWindow()==source',
        'layout_up(set,g,{away});','for(const int requested:{4,0})',
        'gap=MulDiv(2,static_cast<int>(dpi),96)+1,tolerance=MulDiv(4,static_cast<int>(dpi),96)',
        'e::number(started,"\"group_members\":")==(group?2U:1U)',
        'group?translated(b,24,17):b','layout_up(set,g,after);',
        'step=MulDiv(13,static_cast<int>(dpi),96)','chain_command(set,0,L"MoveRight",moved)',
        'chain_command(set,0,L"HeightMore",taller)','visible(set.frames[1].hwnd)==b',
        'settings_controls_restore(s,original)','pause_routing_failed_case_input(s)')){
        if(-not $Cases.Contains($value)){throw ('Remaining settings controls lost actual GUI, independent numeric/native/pixel oracle or cleanup: '+$value)}
    }
    if($Cases -match '\b(SetWindowPos|SetWindowLongPtr\w*|SetLayeredWindowAttributes|PostMessage\w*|SendMessage\w*|AnimateWindow|WriteFile)\s*\(' -or
       $Cases.Contains('std::ofstream') -or $Cases.Contains('SPI_SET')){
        throw 'Remaining settings controls cannot mutate the product/source/config directly or manufacture animation API evidence.'
    }
}
$settingsControlsRemaining=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_settings_controls_remaining_case.h'),[Text.Encoding]::UTF8)
Assert-V1SettingsControlsRemaining $fixtureSource $settingsControlsRemaining $lifecyclePrepare $lifecycleGuest $queueFailureRemote
Check $true 'static remaining settings controls retain real public edits, restart, opaque screen color, Ctrl geometry and public command step'
Reject {Assert-V1SettingsControlsRemaining $fixtureSource ($settingsControlsRemaining.Replace('released&&pixel==fill','released')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static remaining settings controls reject using native alpha without actual independent fill pixels'
Reject {Assert-V1SettingsControlsRemaining $fixtureSource ($settingsControlsRemaining.Replace('group?translated(b,24,17):b','translated(b,24,17)')) $lifecyclePrepare $lifecycleGuest $queueFailureRemote} 'static remaining settings controls reject moving the neighbour when zero tolerance excludes it'
# Keep this PS5.1 script ASCII; production text is read as UTF-8 below.
$settingsMetadataSave=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('6K6+572u5LuF5Zyo4oCc5L+d5a2Y5bm25YWz6Zet4oCd5oiQ5Yqf5ZCO55Sf5pWI44CC'))
$settingsMetadataCancel=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('5Y+W5raI5oiW5YWz6Zet6K6+572u56qX5Y+j5LiN5Lya5pu05pS55q2j5Zyo5L2/55So55qE6YCJ6aG544CC'))
$settingsMetadataGeneration=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('5b2T5YmN5Luj6ZmF'))
$settingsMetadataInternal=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('5YaF6YOo54mI5pys5a2X5q61'))
function Assert-V1UserSettingsMetadata([string]$Source){
    $load=[regex]::Match($Source,'(?s)    void load\(\) \{(?<body>.*?)\r?\n    \}\r?\n    std::optional')
    if(-not $load.Success){throw 'Settings user metadata requires the actual production load scope.'}
    $body=$load.Groups['body'].Value
    foreach($value in @($settingsMetadataSave,$settingsMetadataCancel,
        'SetWindowTextW(metadata,version.c_str());')){
        if(-not $body.Contains($value)){throw 'Settings metadata must explain actual Save and Cancel semantics in the displayed production control.'}
    }
    foreach($value in @('std::to_wstring(draft.schema)','std::to_wstring(draft.generation)',$settingsMetadataGeneration,$settingsMetadataInternal,'RunId','token')){
        if($body.Contains($value)){throw 'User-facing settings metadata must not expose internal schema/generation or authorization terminology.'}
    }
}
$userSettingsSource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/settings_window.cpp'),[Text.Encoding]::UTF8)
Assert-V1UserSettingsMetadata $userSettingsSource
Check $true 'static user settings metadata explains Save/Cancel without internal generation terminology'
$publicSettingsMetadata='const std::wstring version=L"'+$settingsMetadataSave+'\r\n'+$settingsMetadataCancel+'";'
$oldSettingsMetadata=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('Y29uc3QgYXV0byB2ZXJzaW9uPUwi6YWN572u54mI5pys77yaIitzdGQ6OnRvX3dzdHJpbmcoZHJhZnQuc2NoZW1hKStMIu+8m+W9k+WJjeS7o+mZhe+8miIrc3RkOjp0b193c3RyaW5nKGRyYWZ0LmdlbmVyYXRpb24pK0wiXHJcbuWGhemDqOeJiOacrOWtl+auteWPquivu++8m+S/neWtmOWQjueUseS6p+WTgee7n+S4gOaOqOi/m+mFjee9ruS7o+mZheOAgiI7'))
Reject {Assert-V1UserSettingsMetadata ($userSettingsSource.Replace($publicSettingsMetadata,$oldSettingsMetadata))} 'static settings metadata rejects the original schema/generation UI regression'
Reject {Assert-V1UserSettingsMetadata ($userSettingsSource.Replace($settingsMetadataCancel,'Cancel'))} 'static settings metadata rejects losing the Cancel/no-runtime-change explanation'
function Assert-V1MediumDesktopActivation([string]$Environment,[string]$Activation,[string]$Fixture){
    foreach($value in @('if(out.enumerated_ready){',
        'medium_desktop_activate_and_return(created,name,[&]{',
        'observe("activation_fresh_readonly_roots");return out.enumerated_ready;',
        '},ui_request,native_request);',
        'observation.activation_complete&&observation.handle_closed&&original_unchanged',
        'activation.cleanup_known&&', '!activation.deadline_or_completion_unknown')){
        if(-not $Environment.Contains($value)){throw 'Desktop activation must be gated by actual readonly roots and verified independent return, never product authority.'}
    }
    if(-not $Environment.Contains('#include "platform/windows/product/v1_guest_medium_desktop_activation_case.h"')){
        throw 'Desktop activation requires actual guarded fixture wiring.'
    }
    foreach($value in @('if(!guarded||!still_exact)',
        'DESKTOP_READOBJECTS|DESKTOP_SWITCHDESKTOP','OpenDesktopW(exact_run_name.c_str(),0,FALSE,access)',
        'OpenDesktopW(L"Default",0,FALSE,access)', 'original_input.exact(L"Default")',
        'state->entry_gate.compare_exchange_strong(pending,1)',
        'state->entry_gate.compare_exchange_strong(pending,2)',
        'owner_before_entry==WAIT_TIMEOUT&&!state->entry_wait_unknown.load()',
        'ready&&independent_owner_live&&input_before.exact(L"Default")',
        'WaitForSingleObject(state->return_request,budget)', 'WaitForSingleObject(state->entry_done,left)',
        'medium_desktop_activation_return_once(state,state->owner_records,"independent_return_owner")',
        'if(!before.exact(state->target_name))', 'state->return_attempt.compare_exchange_strong(unused,1)',
        'actual.exact(exact_run_name)', 'out.return_owner_exited=exited==WAIT_OBJECT_0',
        'if(out.return_owner_exited)out.records.insert', 'out.cleanup_known=out.opened_handles_closed&&out.default_return_verified&&out.return_owner_exited&&!out.deadline_or_completion_unknown')){
        if(-not $Activation.Contains($value)){throw 'Desktop activation lost exact legal handles, actual API facts, bounded independent return or lifetime gates.'}
    }
    if([regex]::Matches($Activation,'\bSwitchDesktop\(').Count -ne 2 -or $Activation.Contains('INFINITE') -or
       $Activation -match '\b(SendInput|Sleep|SetForegroundWindow|SetFocus|SetProcessWindowStation|SetUserObjectSecurity|SetSecurityInfo|SetTokenInformation|CreateRestrictedToken|TerminateProcess)\s*\('){
        throw 'Desktop activation is only one own entry and one atomically claimed Default return, without input, polling or security changes.'
    }
}
$mediumActivationSource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_medium_desktop_activation_case.h'),[Text.Encoding]::UTF8)
Assert-V1MediumDesktopActivation $mediumDesktopEnvironment $mediumActivationSource $fixtureSource
Check $true 'static exact guest desktop activation uses independent bounded Default return, not product or input authority'
Reject {Assert-V1MediumDesktopActivation ($mediumDesktopEnvironment.Replace('if(out.enumerated_ready){','if(true){')) $mediumActivationSource $fixtureSource} 'static desktop activation rejects starting before exact readonly Shell readiness'
Reject {Assert-V1MediumDesktopActivation $mediumDesktopEnvironment ($mediumActivationSource.Replace('WaitForSingleObject(state->entry_done,left)','WAIT_OBJECT_0')) $fixtureSource} 'static desktop activation rejects assuming an in-flight entry completed'
Reject {Assert-V1MediumDesktopActivation $mediumDesktopEnvironment ($mediumActivationSource.Replace('if(!before.exact(state->target_name))','if(false)')) $fixtureSource} 'static Default return rejects displacing an unknown input desktop'
function Assert-V1MediumDesktopUI([string]$Fixture,[string]$Environment,[string]$Activation,[string]$UI){
    foreach($source in @($lifecyclePrepare,$lifecycleGuest,$queueFailureRemote)){
        if(-not $source.Contains("'MediumDesktopUiOnly'")){throw 'Exact UI-only suite must retain independent package and runner admission.'}
    }
    foreach($value in @('s.suite=="MediumDesktopUiOnly"','medium_desktop_environment_flow(s);',
        '#include "platform/windows/product/v1_guest_medium_desktop_ui_case.h"',
        's.suite=="MediumDesktopUiOnly"?s.medium_ui_product_closed:!s.process')){
        if(-not $Fixture.Contains($value)){throw 'UI-only suite must use actual product start/exit observations, never null shared HANDLE as proof.'}
    }
    foreach($value in @('accepted=ready&&(!ui_only||observation.ui_completed)',
        'ui_only?&ui_request:nullptr','s.medium_ui_product_started=observation.ui_product_started')){
        if(-not $Environment.Contains($value)){throw 'UI-only success requires the real case, not activation alone.'}
    }
    foreach($value in @('state->ui_input->retire_and_wait(1000)','if(!quiescent){state->entry_wait_unknown.store(true);return;}',
        'if(ui_request){state->deadline_ms=30000;state->outer_ms=35000;',
        'ui_request&&out.active_verified&&out.post_activation_ready&&out.tray_probe_cleanup_known',
        'out.ui_cleanup_known=false','out.ui_started=ui.product_started',
        'out.tray_probe_cleanup_known&&out.ui_cleanup_known')){
        if(-not $Activation.Contains($value)){throw 'UI-only input must retire and quiesce before independent return; unknown cleanup is not PASS.'}
    }
    foreach($value in @('flags.compare_exchange_strong(idle,in_flight','flags.fetch_or(retired_bit',
        'WaitForSingleObject(input_done,budget_ms)','!incomplete_pair.load',
        'GetUserObjectInformationW(input,UOI_IO','hash(product_)!=sha_',
        'marker(output/L"panebind-v1-output.marker",run_)','process_birth(process_)!=birth_',
        'QueryFullProcessImageNameW(process_','EnumDesktopWindows(desktop_',
        'mutex_error!=ERROR_FILE_NOT_FOUND','startup.lpDesktop=desktop.data()',
        'input_context()||!target()||!lease_->begin()',
        'SendInput(count,packets,sizeof(INPUT))','API_insert_is_not_message_delivery_proof',
        'Shell_NotifyIconGetRect(&icon,&area)','if(!nonempty)throw',
        'cursor(point,target);','GetCursorPos(&actual)!=FALSE','after_hit==hit&&after_hit_tid==hit_tid&&after_hit_pid==hit_pid',
        'shell_gui.idle()&&hit_gui.idle()&&root_gui.idle()&&foreground_gui.idle()&&buttons_allowed',
        'actual_pair_context(false)','actual_pair_context(true)',
        'if(pair&&inserted==1&&input_context(true)&&(cleanup_target?cleanup_target():target()))',
        'send(pair,2,actual_pair_target,"exact_product_icon_RIGHTDOWN_RIGHTUP",true,own_rightup_cleanup)',
        'EqualRect(&fresh,&area)&&hit&&GetWindowThreadProcessId(hit,&hit_pid)&&hit_pid==shell_pid_&&member(root)',
        'choose(menu,501,L"','choose(exit,505,L"','out_.product_closed',
        'whole_M01_B27_B28_PASS_inferred')){
        if(-not $UI.Contains($value)){throw ('UI-only must retain exact guest/native evidence boundary: '+$value)}
    }
    if($UI.Contains('Sleep(') -or $UI.Contains('INFINITE') -or
       $UI -match '\b(SetForegroundWindow|SetFocus|SetWindowPos|PostMessage\w*|SendMessage\w*|TerminateProcess|SetUserObjectSecurity|SetTokenInformation)\s*\('){
        throw 'UI-only cannot force focus, inject product commands, change security or hide native failures.'
    }
}
$mediumUiSource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_medium_desktop_ui_case.h'),[Text.Encoding]::UTF8)
Assert-V1MediumDesktopUI $fixtureSource $mediumDesktopEnvironment $mediumActivationSource $mediumUiSource
Check $true 'static actual named-desktop UI uses frozen ordinary product, paired guarded input and independent retirement before return'
Reject {Assert-V1MediumDesktopUI $fixtureSource ($mediumDesktopEnvironment.Replace('accepted=ready&&(!ui_only||observation.ui_completed)','accepted=ready')) $mediumActivationSource $mediumUiSource} 'static UI-only cannot convert desktop activation alone into product UI success'
Reject {Assert-V1MediumDesktopUI $fixtureSource $mediumDesktopEnvironment ($mediumActivationSource.Replace('state->ui_input->retire_and_wait(1000)','true')) $mediumUiSource} 'static UI-only cannot return through unretired or in-flight input'
Reject {Assert-V1MediumDesktopUI $fixtureSource $mediumDesktopEnvironment $mediumActivationSource ($mediumUiSource.Replace('flags.compare_exchange_strong(idle,in_flight','flags.exchange(in_flight'))} 'static UI-only rejects reacquiring input after independent retirement'
Reject {Assert-V1MediumDesktopUI $fixtureSource $mediumDesktopEnvironment $mediumActivationSource ($mediumUiSource.Replace('if(!nonempty)throw','if(false)throw'))} 'static ordinary GUI launch cannot bypass actual product UID1 geometry before tray input'
function Assert-V1MediumDesktopNative([string]$Fixture,[string]$Environment,[string]$Activation,[string]$Native){
    foreach($source in @($lifecyclePrepare,$lifecycleGuest,$queueFailureRemote)){
        if(-not $source.Contains("'MediumDesktopNativeOnly'")){throw 'Exact native application suite requires package and runner admission.'}
    }
    foreach($value in @('s.suite=="MediumDesktopNativeOnly"','s.suite!="MediumDesktopNativeOnly"','medium_desktop_environment_flow(s);')){
        if(-not $Fixture.Contains($value)){throw 'Native application run must bypass product launch and Default cleanup paths.'}
    }
    foreach($value in @('native_only?&native_request:nullptr','(!native_only||observation.native_completed)',
        '#include "platform/windows/product/v1_guest_medium_desktop_native_case.h"')){
        if(-not $Environment.Contains($value)){throw 'Native application result must be the actual bounded case, not activation alone.'}
    }
    foreach($value in @('out.active_verified&&out.post_activation_ready&&native_request',
        'medium_terminal_native_case(run,created,*native_request,still_exact,state->began+state->deadline_ms,state->ui_input)',
        'state->deadline_ms=native_request->client_input?30000:15000;',
        'state->outer_ms=native_request->client_input?35000:20000;',
        'out.native_cleanup_known=false')){
        if(-not $Activation.Contains($value)){throw 'Native application preflight requires actual activated roots and a separate bounded return.'}
    }
    foreach($value in @('MediumTerminalNativeResources','request.manifest_sha256','hash(input/L"Terminal-test.zip")==zip_pin',
        'hash(image)==exe_pin','marker(directory/L"panebind-v1-test-application.marker",run)',
        'if(!prepared||!absent("before_launch"))','input_actor_same_user(process)',
        'std::find(first.windows.begin(),first.windows.end(),candidate)',
        'std::find(second.windows.begin(),second.windows.end(),candidate)',
        'CASCADIA_HOSTING_WINDOW_CLASS','begin+10000','WM_NULL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,100',
        'PostMessageW(window,WM_CLOSE,0,0)','resources.finish();','normal_root_close_is_not_all_process_cleanup')){
        if(-not $Native.Contains($value)){throw ('Native application lost exact asset, real new process/window or normal close fact: '+$value)}
    }
    if($Native -match '\b(SendInput|SetWindowPos|SetForegroundWindow|SetFocus|TerminateProcess|Sleep|SetUserObjectSecurity|SetTokenInformation)\s*\(' -or $Native.Contains('INFINITE')){
        throw 'Native application precondition cannot force focus, inject input or alter security.'
    }
}
$mediumNativeSource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_medium_desktop_native_case.h'),[Text.Encoding]::UTF8)
Assert-V1MediumDesktopNative $fixtureSource $mediumDesktopEnvironment $mediumActivationSource $mediumNativeSource
Check $true 'static real Terminal precondition uses exact reviewed assets, a product-absent active Medium Run and normal close'
Reject {Assert-V1MediumDesktopNative $fixtureSource $mediumDesktopEnvironment $mediumActivationSource ($mediumNativeSource.Replace('if(!prepared||!absent("before_launch"))','if(!prepared)'))} 'static native application cannot use a paused product as product-absent control'
Reject {Assert-V1MediumDesktopNative $fixtureSource $mediumDesktopEnvironment $mediumActivationSource ($mediumNativeSource.Replace('hash(image)==exe_pin','true'))} 'static native application cannot substitute an unverified executable'
function Assert-V1MediumTerminalClient([string]$Fixture,[string]$Environment,[string]$Activation,[string]$Native,[string]$Client){
    foreach($source in @($lifecyclePrepare,$lifecycleGuest,$queueFailureRemote)){
        if(-not $source.Contains("'MediumDesktopNativeClientOnly'")){throw 'Client input requires its distinct exact package suite.'}
    }
    foreach($value in @('s.suite=="MediumDesktopNativeClientOnly"','s.suite!="MediumDesktopNativeClientOnly"')){
        if(-not $Fixture.Contains($value)){throw 'Client input must preserve the exact standalone no-product guest path.'}
    }
    foreach($value in @('native_only=s.suite=="MediumDesktopNativeOnly"||native_client_only',
        'hash(fs::path(L"C:\\PaneBindV1\\Input\\manifest.json")),native_client_only')){
        if(-not $Environment.Contains($value)){throw 'Client permission cannot arise from the old no-input suite.'}
    }
    if(-not $Activation.Contains('if(native_request->client_input)state->ui_input=std::make_shared<MediumDesktopUiInputLease>();') -or
       -not $Native.Contains('if(request.client_input&&responded&&active()&&exact_window())') -or
       -not $Native.Contains('(!request.client_input||out.client_completed)')){
        throw 'Client path requires actual responsive native root and a distinct retiring input lease.'
    }
    foreach($value in @('!lease||!lease->valid()||lease->retired()','if(!active||!exact_window||!active())','if(!exact_window())',
        'input_actor_same_user(held_process)','GetProcessId(held_process)!=pid','process_birth(held_process)!=birth',
        'GetUserObjectInformationW(input,UOI_IO','if(mutex||error!=ERROR_FILE_NOT_FOUND||!mutex_closed||FindWindowW',
        'GetGUIThreadInfo(requested,&gui)','GetThreadDesktop(tid)',
        'desktops->IsWindowOnCurrentVirtualDesktop(source,&current)','queried==S_OK?',
        'source_position_read','EqualRect(&actual_position,&source_position)',
        'gui.hwndFocus==focused&&actual_tid==focus_tid&&actual_pid==pid',
        'focus_position_read;',
        'EqualRect(&actual_focus,&focus_position)',
        'identity&&geometry&&mapped_back&&dpi&&stable&&context()&&no_buttons()',
        'send(text.data(),static_cast<UINT>(text.size()),fresh_keyboard_focus',
        'send(enter,2,fresh_keyboard_focus','!lease->begin()',
        'SendInput(count,packets,sizeof(INPUT))','inserted%2','incomplete_pair.store(true',
        'ReadDirectoryChangesW(directory,buffer.data()','CancelIoEx(directory,&operation)',
        'if(file_watch->pending)file_watch.release()','completions>=4',
        'bytes==utf8(nonce)','out.completed=out.exact_nonce_observed&&context()',
        'API_success_alone_is_not_client_semantic_PASS','medium_terminal_client_wait',
        'read(shell_ready,256)==ready_value+"\r\n"','if(exact)out.shell_ready_observed=true',
        'out.shell_ready_phase_accepted=ready_wait_accepted&&GetTickCount64()<shell_ready_deadline_tick',
        'if(!out.shell_ready_phase_accepted)throw','"shell_bootstrap_ready"',
        'actual_initial_budget_ms','last_wait_return','last_context_result',
        'batch_emit_tick_is_not_observation_time')){
        if(-not $Client.Contains($value)){throw ('Client path lost exact input, completion or actual nonce oracle: '+$value)}
    }
    if($Client -match '\b(SetWindowPos|SetForegroundWindow|SetFocus|ReleaseCapture|SetCapture|TerminateProcess|Sleep|SetTokenInformation|SetUserObjectSecurity)\s*\(' -or $Client.Contains('INFINITE')){
        throw 'Client preflight cannot force foreground/capture, change permissions or poll.'
    }
    if($Client.Contains('MOUSEEVENTF_LEFTDOWN') -or $Client.Contains('MOUSEEVENTF_ABSOLUTE')){
        throw 'The keyboard-only Terminal nonce subcase cannot quietly claim or insert mouse-client input.'
    }
}
$mediumClientSource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_medium_desktop_native_client_case.h'),[Text.Encoding]::UTF8)
Assert-V1MediumTerminalClient $fixtureSource $mediumDesktopEnvironment $mediumActivationSource $mediumNativeSource $mediumClientSource
Check $true 'static exact Terminal keyboard nonce uses fresh existing focus, paired input and actual nonce; root hit remains observation and mouse is not run'
Reject {Assert-V1MediumTerminalClient $fixtureSource $mediumDesktopEnvironment $mediumActivationSource $mediumNativeSource ($mediumClientSource.Replace('bytes==utf8(nonce)','true'))} 'static Terminal client rejects SendInput success substituted for actual command receipt'
Reject {Assert-V1MediumTerminalClient $fixtureSource $mediumDesktopEnvironment $mediumActivationSource $mediumNativeSource ($mediumClientSource.Replace('identity&&geometry&&mapped_back&&dpi&&stable&&context()&&no_buttons()','context()'))} 'static Terminal keyboard nonce cannot bypass fresh focus and geometry checks'
Reject {Assert-V1MediumTerminalClient $fixtureSource $mediumDesktopEnvironment $mediumActivationSource $mediumNativeSource ($mediumClientSource.Replace('focus_position_read;','true;'))} 'static zero-area keyboard focus is not permission to ignore a failed actual rectangle query'
function Assert-V1MediumProductGeometry([string]$Environment,[string]$UI,[string]$Geometry){
    if(-not $Environment.Contains('#include "platform/windows/product/v1_guest_medium_desktop_product_geometry_case.h"') -or
       -not $UI.Contains('const auto geometry=medium_desktop_product_geometry_case(run_,desktop_,product_,process_,pid_,birth_,')){
        throw 'The real-app geometry case must be connected to the actual ordinary product and held Run desktop.'
    }
    foreach($value in @('manifest_string(manifest,"guest_suite")!="MediumDesktopUiOnly"','terminal_test_asset',
        'process_birth(product_process_)!=product_birth_','process_birth(target_process_)!=target_birth_',
        'input_actor_same_user(target_process_)','a.complete','b.complete','first.hwnd==second.hwnd',
        'first.pid==second.pid','first.tid==second.tid',
        'manager->IsWindowOnCurrentVirtualDesktop(target_,&current)','queried!=S_OK',
        'lease_->begin()','if(!allowed()){','(cleanup||!retired())',
        'native_threshold_step_&&!continuation_ready_&&!up&&flags==MOUSEEVENTF_MOVE',
        'if(threshold&&skip_unissued_threshold("second_fresh_guard_changed",true))return false;',
        '"geometry_input_revoked_after_inflight_claim"','if(!guard_changed&&!progressed)return false;',
        '"hotkey_stop"','"thread_association_failure"','actual.find("\"type\":\"native_move_handoff\"")',
        'SendInput(count,packets.data(),sizeof(INPUT))','incomplete_pair.store(true',
        'matching_clean_captured_up','up_qpc_','legacy_left_up','isolation_gone',
        'ProductLogEvidence','evidence_.begin(product_pid_,product_birth_,launch_tick_,source_)',
        'PostMessageW(target_,WM_CLOSE,0,0)','five_applications_or_whole_version_PASS')){
        if(-not $Geometry.Contains($value)){throw ('Real-app geometry lost exact scope, actual receipt or cleanup guard: '+$value)}
    }
    if($Geometry -match '\b(SetWindowPos|DeferWindowPos|SetForegroundWindow|SetFocus|AttachThreadInput|SetCapture|ReleaseCapture|Sleep|TerminateProcess|SetTokenInformation|SetUserObjectSecurity)\s*\(' -or
       $Geometry.Contains('INFINITE')){throw 'The geometry driver must not become an alternative writer, activation or permission path.'}
}
$mediumGeometrySource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_medium_desktop_product_geometry_case.h'),[Text.Encoding]::UTF8)
Assert-V1MediumProductGeometry $mediumDesktopEnvironment $mediumUiSource $mediumGeometrySource
Check $true 'static actual product/Terminal geometry case is wired without Default input or a private geometry writer'
Reject {Assert-V1MediumProductGeometry $mediumDesktopEnvironment $mediumUiSource ($mediumGeometrySource.Replace('process_birth(target_process_)!=target_birth_','false'))} 'static geometry input cannot inherit a reused target PID'
Reject {Assert-V1MediumProductGeometry $mediumDesktopEnvironment $mediumUiSource ($mediumGeometrySource.Replace('queried!=S_OK','false'))} 'static geometry input cannot treat unknown Task View membership as current'
Reject {Assert-V1MediumProductGeometry $mediumDesktopEnvironment $mediumUiSource ($mediumGeometrySource.Replace('(cleanup||!retired())','true'))} 'static normal serial UP cannot send a final MOVE after an observed terminal fact'
$mediumExplorerSource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_medium_desktop_explorer_target.h'),[Text.Encoding]::UTF8)
foreach($value in @('startup.lpDesktop=desktop.data()','native_baseline_','shell_baseline_',
    'entry->shell_entry_count!=1','shell::same_filesystem_location','shell::bind_explorer_consent_target',
    'facts.callback_sequence==callback_at_binding_','observation_->receipt_facts()',
    '(!launched_||handed_off_)',
    'native_new(candidate,pid,process.value,born)','if(bind_attempted_)throw')){
    if(-not $mediumExplorerSource.Contains($value)){throw ('Exact Explorer target lost its source/location/epoch boundary: '+$value)}
}
if($mediumExplorerSource -match '\b(SendInput|SetWindowPos|DeferWindowPos|SetForegroundWindow|SetFocus|AttachThreadInput|SetCapture|ReleaseCapture|Navigate|Navigate2|TerminateProcess|SetTokenInformation|SetUserObjectSecurity|Sleep)\s*\('){
    throw 'The exact Explorer adapter may only prepare and observe its own target, not become another input/writer/navigation path.'
}
foreach($value in @('explorer_case_=manifest_explorer(manifest)','geometry_manifest_Explorer_and_Terminal_mutually_exclusive',
    'hit_point(target_,region,nullptr,!explorer_case_&&region==HTCAPTION)',
    'explorer_->take_bound_process()','!explorer_->guard()',
    'MediumDesktopUiInputLease::in_flight)&&explorer_->refresh()')){
    if(-not $mediumGeometrySource.Contains($value)){throw ('Common geometry chain lost the bounded Explorer adapter connection: '+$value)}
}
function Assert-V1TraySemanticPoint([string]$UI,[string]$Semantic){
    foreach($value in @('point=medium_tray_semantic_point(area,shell_pid_',
        'input_context()&&target()&&no_buttons();','click(true,point,target);')){
        if(-not $UI.Contains($value)){throw 'Tray click must use the semantic point and preserve the original fresh guards.'}
    }
    foreach($value in @('put_ConnectionTimeout(150)','put_TransactionTimeout(150)','put_AutoSetFocus(FALSE)',
        'ElementFromPoint(center','static_cast<DWORD>(pid)==shell_pid','own_name&&',
        'control==UIA_ButtonControlTypeId','PtInRect(&icon,point)&&PtInRect(&actual,point)&&active()',
        'element.value->GetClickablePoint(&point,&got)','GetForegroundWindow()==foreground',
        'if(!exact)throw','nonmatching_name_redacted')){
        if(-not $Semantic.Contains($value)){throw 'Semantic tray observation must bind the exact Shell button, point and bounded context.'}
    }
    if($Semantic -match '(?:->|\b)(?:Invoke|ShowContextMenu|SetFocus|SetForegroundWindow|PostMessage\w*|SendMessage\w*|SendInput|FindAll|FindFirst|GetParentElement)\s*\(' -or
        $Semantic.Contains('INFINITE') -or $Semantic.Contains('Sleep(')){
        throw 'Semantic point observation must remain read-only with no tree or action fallback.'
    }
}
$semanticTray=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_tray_semantic_point.h'),[Text.Encoding]::UTF8)
Assert-V1TraySemanticPoint $mediumUiSource $semanticTray
Check $true 'static tray contrast requires the actual named Shell button and its physical clickable point without UIA actions'
Reject {Assert-V1TraySemanticPoint $mediumUiSource ($semanticTray.Replace('own_name&&','true&&'))} 'static tray contrast rejects replacing the exact icon name with permission'
Reject {Assert-V1TraySemanticPoint $mediumUiSource ($semanticTray.Replace('PtInRect(&icon,point)&&PtInRect(&actual,point)&&active()','true'))} 'static tray contrast rejects a point outside the original icon or retired context'
foreach($value in @('medium_observe_held_process_exit(process,pid,birth,caller_deadline_tick,quit_seen',
    'if(!out.budget_ms||quit_seen)','WaitForSingleObject(process,0)',
    'while(batch--&&!quit_seen&&GetTickCount64()<end&&out.dispatched_messages<1024&&',
    'out.completed=!quit_seen&&responded','all_descendants_exit_inferred')){
    Check ($mediumNativeSource.Contains($value)) ('static bounded process close preserves the actual deadline and signal boundary: '+$value)
}
Check ($mediumGeometrySource.Contains('out_.cleanup_known=out_.cleanup_known&&!quit_seen_&&target_closed_')) 'static close observation QUIT blocks further product UI input through cleanup status'
$readyFailureSource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_medium_desktop_terminal_failure_observation.h'),[Text.Encoding]::UTF8)
foreach($value in @('first.complete&&second.complete','one.readonly_same(two)&&current()&&GetForegroundWindow()==source',
    'entries.size()>=4096','tree.nodes.size()>=32','queries>=64','CREATE_NEW',
    'foreground_or_rect_changed_after_BitBlt_pixels_not_saved','empty_does_not_prove_cmd_never_started')){
    Check ($readyFailureSource.Contains($value)) ('static single ready-failure observation retains its exact scope: '+$value)
}
Check ($mediumClientSource.Contains('if(!out.shell_ready_phase_accepted){') -and
    $mediumClientSource.Contains('medium_terminal_observe_ready_failure(out,run,configuration,created,source,pid,tid,held_process,birth,')) 'static actual client ready failure is wired to its one read-only observation before the preserved rejection'
if($readyFailureSource -match '\b(SendInput|SetForegroundWindow|SetFocus|PrintWindow|SetWindowPos|TerminateProcess|Sleep|SetTokenInformation|SetUserObjectSecurity)\s*\('){throw 'Read-only ready-failure evidence cannot become input, control or a different authority route.'}
function Assert-V1DirectCmdContrast([string]$Native,[string]$Probe){
    if(-not $Native.Contains('const bool earlier_client_cleanup_known=out.client_cleanup_known;out.client_cleanup_known=false;') -or
       -not $Native.Contains('out.client_cleanup_known=earlier_client_cleanup_known&&direct.cleanup_known;') -or
       -not $Native.Contains('if(!direct.may_continue)throw')){throw 'Direct command observation must not lose an unknown child cleanup or authorize Terminal input.'}
    foreach($value in @('run+L"-direct-cmd-bootstrap.cmd"','run+L"-direct-cmd-probe.txt"',
        'portable_field(manifest,"guest_suite")!="MediumDesktopNativeClientOnly"','CREATE_NEW',
        'FALSE,CREATE_NO_WINDOW,nullptr,','owned.process,pid,birth,caller_deadline,quit_seen,1500',
        '(!out.created||exit_observation.signalled)','Terminal_ShellReady_satisfied','client_nonce_satisfied')){
        if(-not $Probe.Contains($value)){throw 'Direct command contrast lost its separate output, fixed scope or bounded exit observation.'}
    }
    if($Probe -match '\b(SendInput|SetForegroundWindow|SetFocus|AttachThreadInput|SetTokenInformation|CreateProcessAsUser\w*|TerminateProcess|Sleep)\s*\(' -or
       $Probe.Contains('INFINITE')){throw 'The finite no-console contrast must not become an input, alternative token or forced-cleanup route.'}
}
$directCmdProbe=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_medium_desktop_direct_cmd_probe.h'),[Text.Encoding]::UTF8)
Assert-V1DirectCmdContrast $mediumNativeSource $directCmdProbe
Check $true 'static direct cmd contrast is a separately guarded finite child, not Terminal ready or client authority'
Reject {Assert-V1DirectCmdContrast $mediumNativeSource ($directCmdProbe.Replace('run+L"-direct-cmd-probe.txt"','run+L"-terminal-shell-ready.txt"'))} 'static direct cmd contrast cannot write the Terminal ready oracle'
Reject {Assert-V1DirectCmdContrast ($mediumNativeSource.Replace('if(!direct.may_continue)throw','if(false)throw')) $directCmdProbe} 'static direct cmd cleanup UNKNOWN cannot continue the existing Terminal case'
function Assert-V1FairPumpQuit([string]$Geometry){
    $peek=$Geometry.IndexOf('if(!PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE))break;',[StringComparison]::Ordinal)
    if($peek -lt 0){throw 'Fair wait must observe queued QUIT before its final phase predicate.'}
    $quit=$Geometry.IndexOf('if(message.message==WM_QUIT){quit_seen_=true;return rejected("WM_QUIT");}',$peek,[StringComparison]::Ordinal)
    $deadline=$Geometry.IndexOf('if(GetTickCount64()>=end)break;',$peek,[StringComparison]::Ordinal)
    if($quit -lt $peek -or $deadline -lt $peek -or $quit -gt $deadline){throw 'Already observed QUIT cannot be skipped by the phase deadline.'}
}
Assert-V1FairPumpQuit $mediumGeometrySource
Check $true 'static actual fair wait records an observed QUIT before testing its phase deadline'
$withoutFirstPeekQuit=[regex]::Replace($mediumGeometrySource,
    '(PM_NOREMOVE\)\)break;\s*)if\(message\.message==WM_QUIT\)\{quit_seen_=true;return rejected\("WM_QUIT"\);\}', '$1')
if($withoutFirstPeekQuit -ceq $mediumGeometrySource){throw 'The first-Peek QUIT mutation did not reach the actual wait implementation.'}
Reject {Assert-V1FairPumpQuit $withoutFirstPeekQuit} 'static fair wait rejects losing an observed QUIT at the final predicate boundary'
function Assert-V1PlacementPostLimitPolicy([string]$Writer){
    foreach($value in @('r.post_track_limits_query_requested=placement_post_needs_track_limits(r,job->native);',
        'r.after=catalog_.get(r.before.hwnd,settings,nullptr,false,*r.post_track_limits_query_requested);',
        'retain_placement_preflight_limits(r);',
        'result.post_track_limits_query_requested=placement_post_needs_track_limits(result,native);',
        'result.after = catalog_.get(before->hwnd, settings, &capture_reason,false,*result.post_track_limits_query_requested);',
        'retain_placement_preflight_limits(result);',
        'const auto before = catalog_.get(expected.hwnd, settings, &capture_reason);',
        'const auto current = catalog_.get(expected.hwnd, settings, &reason);',
        'r.after->declared_track_limits_observed=false;', 'r.after->application_grid_known=false;')){
        if(-not $Writer.Contains($value)){throw 'Actual immediate/delayed post limits policy must retain fresh pre-call checks and distinguish carried facts.'}
    }
}
$placementWriterSource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/window_writer.cpp'),[Text.Encoding]::UTF8)
Assert-V1PlacementPostLimitPolicy $placementWriterSource
Check $true 'static actual immediate and delayed captures use the tested post-limit policy without removing fresh admissions'
Reject {Assert-V1PlacementPostLimitPolicy ($placementWriterSource.Replace('retain_placement_preflight_limits(result);',''))} 'static post-limit bypass cannot silently replace real preflight bounds with default limits'
Reject {Assert-V1PlacementPostLimitPolicy ($placementWriterSource.Replace('catalog_.get(expected.hwnd, settings, &capture_reason);','catalog_.get(expected.hwnd, settings, &capture_reason,false,false);'))} 'static post-limit exception cannot disable the next actual fresh placement admission'
function Assert-V1GeometryBottomRegion([string]$Geometry){
    if(-not $Geometry.Contains('case HTBOTTOM:expected_region=4;break;') -or
       -not $Geometry.Contains('number(pending,"\"actual_DOWN_region\":")!=expected_region')){
        throw 'The real bottom-edge test DOWN must match domain region4, not the old right-edge region2.'
    }
}
Assert-V1GeometryBottomRegion $mediumGeometrySource
Check $true 'static actual minimum-resize DOWN branch checks the observed bottom region4'
Reject {Assert-V1GeometryBottomRegion ($mediumGeometrySource.Replace('case HTBOTTOM:expected_region=4;break;','case HTBOTTOM:expected_region=2;break;'))} 'static minimum test rejects its old deterministic right-versus-bottom mismatch'
function Assert-V1SingleSizingProposal([string]$Geometry){
    $begin=$Geometry.IndexOf('std::optional<TerminalSizingProposal> terminal_sizing_proposal()', [StringComparison]::Ordinal)
    $end=$Geometry.IndexOf('// Read current-launch diagnostics', $begin, [StringComparison]::Ordinal)
    if($begin -lt 0 -or $end -le $begin){throw 'The one-point proposal must be in the actual geometry adapter.'}
    $probe=$Geometry.Substring($begin,$end-$begin)
    if([regex]::Matches($probe,'SendMessageTimeoutW\(').Count -ne 1 -or
       $probe -match '\b(for|while|wait_for|SetWindowPos|SetForegroundWindow|SetCapture|ReleaseCapture)\s*\('){
        throw 'The proposal must not scan, retry, place, activate or change capture.'
    }
    foreach($value in @('matching_clean_captured_up(', 'target_tid_!=GetCurrentThreadId()',
        'lease_->flags.load(std::memory_order_acquire)==0', 'GetForegroundWindow()==target_',
        'SendMessageTimeoutW(target_,WM_SIZING,WMSZ_BOTTOM',
        'SMTO_ABORTIFHUNG|SMTO_BLOCK|SMTO_ERRORONEXIT,30,&message_result)',
        'eligible=api!=FALSE&&message_result==TRUE&&context&&shape&&bounds;',
        'if(!eligible)return {};')){
        if(-not $probe.Contains($value)){throw ('Single proposal lost a real fact or UNKNOWN gate: '+$value)}
    }
    foreach($value in @('if(const auto proposal=terminal_sizing_proposal())',
        'one_gesture(HTBOTTOM,0,1,"SingleSizingProposalResize",false,false,false,&*proposal);',
        'one_gesture(HTCAPTION,24,18,"AfterSingleSizingProposalMove");',
        'placement=phase_placement(placement_mark,*observed,source_key_);',
        'geometry_single_sizing_proposal_stale_before_real_DOWN')){
        if(-not $Geometry.Contains($value)){throw 'The proposal must feed the guarded real gesture, not an idealized helper.'}
    }
    $up=$Geometry.IndexOf('if(!constraint_declined)out_.emit("medium_product_geometry_NormalUp"', [StringComparison]::Ordinal)
    $comparison=$Geometry.IndexOf('out_.emit("medium_product_geometry_Terminal_WM_SIZING_comparison"', [StringComparison]::Ordinal)
    if($up -lt 0 -or $comparison -le $up){throw 'Interpret the preference only after the actual NormalUp and cleanup checks.'}
}
Assert-V1SingleSizingProposal $mediumGeometrySource
Check $true 'static one-point app proposal feeds the actual guarded Resize with real UP before preference interpretation'
Reject {Assert-V1SingleSizingProposal ($mediumGeometrySource.Replace('eligible=api!=FALSE&&message_result==TRUE&&context&&shape&&bounds;','eligible=true;'))} 'static sizing proposal cannot invent eligibility from delivery alone'
Reject {Assert-V1SingleSizingProposal ($mediumGeometrySource.Replace('if(!eligible)return {};',''))} 'static UNKNOWN proposal cannot proceed to real synthetic input'
Reject {Assert-V1SingleSizingProposal ($mediumGeometrySource.Replace('one_gesture(HTBOTTOM,0,1,"SingleSizingProposalResize",false,false,false,&*proposal);',''))} 'static proposal cannot substitute a message result for an actual guarded Resize'
function Assert-V1LiveResizeCallback([string]$Writer,[string]$Application,[string]$Geometry){
    if($Application.Contains('return rectangle({r.left,r.top,r.right,r.bottom});') -or
       $Geometry.Contains('return rect({r.left,r.top,r.right,r.bottom});')){
        throw 'Rejected malformed raw callback RECTs must not be normalized by the domain Rect logger.'
    }
    foreach($value in @('resize&&!resize_request_scope_allowed(*resize,placements.size())',
        'if(placement.visible.size()==current->visible.size())continue;',
        'facts.api_return=SendMessageTimeoutW(before->hwnd,WM_SIZING,edge,',
        'prepare_resize_query(*current,*before,placement.visible,member,facts)',
        'if(request.coupled&&changed)',
        'if(!resize_guard(reason)){reject_resize(index,reason);return result;}',
        'if(!current||!consistent(expected,*current,reason))',
        'for(auto& query:queries)if(query)query->context_revalidated=true;',
        'const auto final_placements=resize?std::span<const v::Placement>{adjusted}:placements;',
        'place_with_destination(*expected,placement.visible,settings,guard,capture,&destinations[index])')){
        if(-not $Writer.Contains($value)){throw ('Live resize lost its actual callback/whole-batch/UP boundary: '+$value)}
    }
    $callback=$Writer.IndexOf('facts.api_return=SendMessageTimeoutW(before->hwnd,WM_SIZING,edge,',[StringComparison]::Ordinal)
    $post=$Writer.IndexOf('for(auto& query:queries)if(query)query->context_revalidated=true;',[StringComparison]::Ordinal)
    $placement=$Writer.IndexOf('const auto final_placements=resize?',[StringComparison]::Ordinal)
    if($callback -lt 0 -or $post -le $callback -or $placement -le $post){throw 'All callbacks and revalidation must precede the existing single writer.'}
    foreach($value in @('std::optional<WindowWriter::ResizeRequest> resize=std::nullopt',
        'capture,resize);','const auto resize_request=is_resize(state.edges)?',
        'WindowWriter::ResizeRequest{state.edges,state.ctrl||plan.placements.size()>1}',
        'e.hook_observed_qpc,resize_request)',
        'state.x.reset();adopted_x.reset();','state.y.reset();adopted_y.reset();',
        'log.event("application_resize_proposal"')){
        if(-not $Application.Contains($value)){throw ('Actual product dispatcher is not wired to the guarded resize callback: '+$value)}
    }
    foreach($value in @('std::string application_right_placement(',
        'placement=application_right_placement(placement_mark,step.expected,*original,positioning,*observed,application_callback);',
        'snapshot.find(placement,snapshot.find(line)+line.size())',
        'actual_application_callback_not_product_solver')){
        if(-not $Geometry.Contains($value)){throw 'Affected real RightResize must pair actual callback and exact independent geometry, not wait for the superseded raw R.'}
    }
}
$liveResizeApplication=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/product_app.cpp'),[Text.Encoding]::UTF8)
Assert-V1LiveResizeCallback $placementWriterSource $liveResizeApplication $mediumGeometrySource
Check $true 'static live Resize alone reaches the application callback and fresh whole-batch gates before the existing writer'
Reject {Assert-V1LiveResizeCallback ($placementWriterSource.Replace('if(request.coupled&&changed)','if(false)')) $liveResizeApplication $mediumGeometrySource} 'static coupled callback cannot independently correct members and break their shared layout'
Reject {Assert-V1LiveResizeCallback ($placementWriterSource.Replace('if(!resize_guard(reason)){reject_resize(index,reason);return result;}','')) $liveResizeApplication $mediumGeometrySource} 'static callback return cannot bypass UP revocation before placement'
Reject {Assert-V1LiveResizeCallback $placementWriterSource ($liveResizeApplication.Replace('const auto resize_request=is_resize(state.edges)?','const auto resize_request=true?')) $mediumGeometrySource} 'static Move and discrete operations cannot silently acquire a resize callback'
Reject {Assert-V1LiveResizeCallback $placementWriterSource ($liveResizeApplication+' return rectangle({r.left,r.top,r.right,r.bottom});') $mediumGeometrySource} 'static malformed raw callback evidence cannot silently normalize reversed native edges'
function Assert-V1CoupledCallbackTest([string]$Geometry,[string]$Blank){
    foreach($value in @('one_gesture(HTRIGHT,1,0,"CtrlSharedEdgeResizeCallback",false,true,false,nullptr,true);',
        'magnet_.reset();minimum_chain_active_=true;',
        'one_gesture(HTCAPTION,24,18,"AfterCtrlSharedResizeCallbackMove");',
        'shared_right_callback(placement_mark,*original,',
        'geometry_coupled_rejection_resources_not_retired_before_TEST_abort_UP',
        'matching_abort_cleanup_up(last_status_,ended,gone,identity,released)',
        'const bool constraint_declined=minimum_declined||coupled_declined;',
        'if(!constraint_declined)out_.emit("medium_product_geometry_NormalUp"',
        'const unsigned wanted=coupled_declined?0u:2u;',
        'magnet_->finish_shared_right_resize(!coupled_declined)')){
        if(-not $Geometry.Contains($value)){throw ('Actual coupled test lost its narrow route or outcome: '+$value)}
    }
    foreach($value in @('*frame==coupled->before&&native==coupled->before_native',
        '*frame==coupled->after&&native==coupled->after_native',
        'coupled_=CoupledGeometry{wanted,wanted,native,native};')){
        if(-not $Blank.Contains($value)){throw 'The exact TEST neighbour may not accept an arbitrary new geometry baseline.'}
    }
}
$coupledBlankSource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_medium_desktop_magnet_target.h'),[Text.Encoding]::UTF8)
Assert-V1CoupledCallbackTest $mediumGeometrySource $coupledBlankSource
Check $true 'static actual Ctrl shared-edge callback test separates two exact placements from zero-native rejection and abort UP'
Reject {Assert-V1CoupledCallbackTest ($mediumGeometrySource.Replace('const bool constraint_declined=minimum_declined||coupled_declined;','const bool constraint_declined=minimum_declined;')) $coupledBlankSource} 'static coupled rejection cannot borrow the NormalUp branch'
Reject {Assert-V1CoupledCallbackTest $mediumGeometrySource ($coupledBlankSource.Replace('*frame==coupled->after&&native==coupled->after_native','true'))} 'static coupled TEST target cannot silently accept arbitrary source changes'
Write-Output ('V1 packaging identity: '+$checks+' checks passed; no package, product UI or synthetic input was run.')
