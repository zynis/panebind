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
