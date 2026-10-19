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
       -not $Layouts.Contains('state.suite!="OwnedRulesOnly"||storage->class_name!=L"PaneBind.V1.OwnedFixture."+state.run') -or
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
Assert-V1NativeClientPreflightWiring $fixtureSource
Check $true 'static driver wiring: every application native preflight precedes product cases and preserves exact HTCLIENT gates'
$layoutSource=[IO.File]::ReadAllText((Join-Path $repository 'src/platform/windows/product/v1_guest_layout_cases.h'),[Text.Encoding]::UTF8)
Assert-V1GroupBoundsScopeWiring $fixtureSource $layoutSource
Check $true 'static group scope: two shared actual cases return before unrelated suites; Full retains the shared cases and real prior evidence'
Reject {Assert-V1GroupBoundsScopeWiring ($fixtureSource.Replace('phase("eight_owned_group",owned_eight_group_only);','phase("owned_finite_I11_component",owned_finite_I11_chain);')) $layoutSource} 'static group scope rejects adding unrelated I11 work to the two-case entry'
Reject {Assert-V1GroupBoundsScopeWiring $fixtureSource ($layoutSource.Replace('layout_eight_case(set,std::nullopt)','layout_eight_case(set,Gesture{})'))} 'static group scope rejects a fabricated previous gesture for standalone eight-window validation'
Assert-V1NativeClientsWithoutProductWiring $fixtureSource
Check $true 'static native baseline: product launch is bypassed; exact client actions neither read product receipts nor run geometry cases'
Reject {Assert-V1NativeClientsWithoutProductWiring ($fixtureSource.Replace('without_product?std::size_t{}:read_product(s).size()','read_product(s).size()'))} 'static native baseline rejects reading product log before native input'
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
Reject {Assert-V1StretchInputWiring $fixtureSource $chainSource ($stretchInput.Replace('chain_double_click(set,item.region,item.shift,item.expected,item.label,true);','chain_double_click(set,item.region,item.shift,item.expected,item.label,false);'))} 'static F04 scope rejects disabling actual UP receipts'
Reject {Assert-V1StretchInputWiring ($fixtureSource.Replace('scope_incomplete?"PARTIAL":','scope_incomplete?"PASS":')) $chainSource $stretchInput} 'static F04 scope rejects turning incomplete timing into PASS'
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
$portableFunction=@($guestAst.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Prepare-PortableTestPackage'},$false))
Check ($portableFunction.Count -eq 1) 'test locates the actual guest portable validation function'
. ([scriptblock]::Create($portableFunction[0].Extent.Text))
$outputDir=Join-Path $repository 'uat\not-created-portable-negative-check';$RunId='00000000000000000000000000000000'
$missingSpec=[pscustomobject]@{kind='PaneBindReleasePortable';relative_path='PaneBindV1-portable-test.zip';package_id=$RunId;
    source_commit=$before;exe_sha256=('A'*64);zip_sha256=('B'*64);manifest_sha256=('C'*64);zip_size=128;expanded_bytes=128;
    files=@(1..6|ForEach-Object{[pscustomobject]@{relative_path=('file'+$_);size=1;sha256=('D'*64)}})}
$missingReason='';try{Prepare-PortableTestPackage $missingSpec $before ('A'*64)|Out-Null}catch{$missingReason=$_.Exception.Message}
Check ($missingReason -ceq 'Portable package must have exactly seven payload files.' -and -not(Test-Path -LiteralPath $outputDir)) 'actual guest rejects missing payload file before extraction; no directory or product was created'
foreach($relative in @('scripts/v1-package-identity.ps1','scripts/v1-prepare-portable.ps1','scripts/v1-prepare-sandbox.ps1','scripts/v1-sandbox-guest.ps1')){
    $tokens=$null;$errors=$null
    [Management.Automation.Language.Parser]::ParseFile((Join-Path $repository $relative),[ref]$tokens,[ref]$errors)|Out-Null
    Check ($errors.Count -eq 0) ('PowerShell parser: '+$relative)
}
Write-Output ('V1 packaging identity: '+$checks+' checks passed; no package, product UI or synthetic input was run.')
