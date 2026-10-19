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
    $start=[regex]::Match($Source,('(?m)^void '+[regex]::Escape($Name)+'\s*\('))
    if(-not $start.Success){throw ('Fixture function missing: '+$Name)}
    $rest=$Source.Substring($start.Index+$start.Length)
    $next=[regex]::Match($rest,'(?m)^void \w+\s*\(')
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
$fixturePath='src/platform/windows/product/v1_guest_fixture.cpp'
$fixtureSource=[IO.File]::ReadAllText((Join-Path $repository $fixturePath),[Text.Encoding]::UTF8)
Assert-V1NativeClientPreflightWiring $fixtureSource
Check $true 'static driver wiring: every application native preflight precedes product cases and preserves exact HTCLIENT gates'
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
