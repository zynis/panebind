# Shared identity checks for the existing V1 portable and guest packagers.
# This file does not build, launch the product, or create a package.
function Test-V1ProductToolOnlyPath([string]$Path) {
    # Audited against src/platform/windows/CMakeLists.txt: V1 now links the
    # shared GestureInputShield as well as core + operation_geometry. Changes
    # to gesture_input_shield.h/.cpp MUST invalidate product-binary reuse.
    # These remaining exact historical-MVP1 inputs belong only to separate
    # owned-validation/flow targets, not the V1 product EXE.
    # This is Product-only: the V1 fixture DOES link windows_explorer/shield,
    # so the separate Driver scope below is deliberately not widened. CMake,
    # real operation_geometry and all production includes remain protected.
    $separateMvp1Inputs=@(
        'src/platform/windows/operations/owned_gesture_shield_validation.cpp',
        'src/platform/windows/explorer/explorer_mvp_flow_tests.cpp',
        'scripts/prepare-r1c4b-mvp1-shield-sandbox.ps1',
        'scripts/r1c4b-mvp1-shield-sandbox-guest.ps1')
    # This existing file is a BUILD_TESTING-only executable, not a source of
    # panebind_windows_product or the product EXE. Keep the exemption exact;
    # nearby production guest_path_scope.h/window_catalog.cpp remain protected.
    return $separateMvp1Inputs -ccontains $Path -or
        $Path -ceq 'src/platform/windows/product/guest_path_scope_tests.cpp' -or
        $Path -cmatch '^(docs/|tests/|scripts/v1-[^/]+\.ps1$|src/platform/windows/product/v1_guest_[^/]+\.(cpp|h)$)'
}
function Get-V1IdentityChanges([string]$Repository,[string]$Source,[string]$Head) {
    if($Source -cnotmatch '^[0-9a-f]{40}$' -or $Head -cnotmatch '^[0-9a-f]{40}$'){throw 'Full source and packaging commit identities are required.'}
    & git -C $Repository cat-file -e ($Source+'^{commit}')
    if($LASTEXITCODE -ne 0){throw 'Source identity is not an available local commit.'}
    & git -C $Repository merge-base --is-ancestor $Source $Head
    if($LASTEXITCODE -ne 0){throw 'A binary source must be an ancestor of the actual packaging HEAD.'}
    # Include intervening edits, not merely the endpoint: edit/revert is not
    # accepted as proof that an old executable matches the current source.
    $history=@(& git -C $Repository log --format= --name-only --no-renames ($Source+'..'+$Head) --)
    if($LASTEXITCODE -ne 0){throw 'Cannot inspect binary-to-packaging history.'}
    $endpoint=@(& git -C $Repository diff --name-only --no-renames $Source $Head --)
    if($LASTEXITCODE -ne 0){throw 'Cannot compare binary and packaging trees.'}
    return @(@($history)+@($endpoint)|Where-Object{$_}|Sort-Object -Unique)
}
function Assert-V1SourceScope([string]$Repository,[string]$Source,[string]$Head,[ValidateSet('Product','Driver')][string]$Kind) {
    $changed=@(Get-V1IdentityChanges $Repository $Source $Head)
    $forbidden=@($changed|Where-Object {
        if($Kind -ceq 'Product'){-not (Test-V1ProductToolOnlyPath $_)}
        else{$_ -cnotmatch '^(docs/|tests/|scripts/v1-[^/]+\.ps1$)'}
    })
    if($forbidden.Count){throw ($Kind+' binary cannot be reused across changed implementation/build/license/governance inputs: '+($forbidden -join ', '))}
    return $changed
}
function Assert-PackagingSource([string]$Repository,[string]$Runtime,[string]$PackagingHead,[string]$DriverSource) {
    $changed=@(Assert-V1SourceScope $Repository $Runtime $PackagingHead Product)
    $driverChanges=@()
    if($DriverSource){$driverChanges=@(Assert-V1SourceScope $Repository $DriverSource $PackagingHead Driver)}
    $actualHead=(& git -C $Repository rev-parse HEAD).Trim()
    if($LASTEXITCODE -ne 0 -or $actualHead -cne $PackagingHead){throw 'Packaging HEAD changed during identity verification.'}
    $deliveryDocs=@('docs/V1_QUICK_START.md','docs/reports/V1_ACCEPTANCE.json','docs/reports/V1_DELIVERY_REPORT.md')
    $cleanScope=@('src','tests','scripts','cmake','CMakeLists.txt','LICENSE','AGENTS.md')+$deliveryDocs
    $dirty=@(& git -C $Repository diff --name-only HEAD -- @cleanScope)
    if($LASTEXITCODE -ne 0){throw 'Cannot inspect source/document worktree status.'}
    $untracked=@(& git -C $Repository ls-files --others --exclude-standard -- @cleanScope)
    if($LASTEXITCODE -ne 0 -or $dirty.Count -or $untracked.Count){throw 'Commit source, tools and delivery documents before packaging; unrelated plan input remains permitted.'}
    foreach($relative in @($deliveryDocs)+@('LICENSE','AGENTS.md')){
        & git -C $Repository ls-files --error-unmatch -- $relative | Out-Null
        if($LASTEXITCODE -ne 0){throw 'Delivery documents, LICENSE and AGENTS must belong to the committed packaging HEAD.'}
    }
    $inputs=@(foreach($relative in $deliveryDocs){
        $path=Join-Path $Repository $relative;$item=Get-Item -LiteralPath $path
        if($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Delivery input must be an ordinary file.'}
        [ordered]@{path=$relative;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    })
    return [ordered]@{source_commit=$Runtime;product_source_commit=$Runtime;driver_source_commit=$DriverSource;
        packaging_head=$PackagingHead;source_scope_equivalent=$true;
        product_unchanged_across_tool_changes=$changed;driver_unchanged_across_packaging_changes=$driverChanges;
        docs_only_changes=@($changed|Where-Object{$_ -cmatch '^docs/'});documentation_inputs=$inputs;
        interface_contract='panebind-v1-product-guest-interface/1'}
}
function Assert-V1BinaryInterface($Identity,[ValidateSet('Product','Driver')][string]$Kind,[string]$ExpectedSource) {
    if($Identity.source_commit -cne $ExpectedSource){throw 'Actual binary source_commit differs from its explicitly selected source identity.'}
    if($Kind -ceq 'Product'){
        if($Identity.product -cne 'PaneBind' -or $Identity.version -cne '1.0.0-dev' -or
           $Identity.subsystem -cne 'windows' -or $Identity.requires_elevation -ne $false){throw 'Product executable does not implement the reviewed V1 guest interface.'}
    }elseif($Identity.product -cne 'PaneBind V1 guest fixture' -or $Identity.host_synthetic_input -cne 'forbidden' -or
        $Identity.requires_medium_guest -ne $true -or $Identity.guest_contract -cne 'panebind-v1-guest-contract/2' -or
        $Identity.product_interface -cne 'panebind-v1-product-guest-interface/1'){
        throw 'Fixture executable lacks the exact split-identity guest contract; rebuild the driver, not the unchanged product.'
    }
}
function Assert-V1PortableDependencies([string[]]$Dependencies) {
    $systemDlls=@('kernel32.dll','user32.dll','gdi32.dll','advapi32.dll','ole32.dll','oleaut32.dll',
        'shell32.dll','shlwapi.dll','comctl32.dll','comdlg32.dll','dwmapi.dll','wtsapi32.dll',
        'ntdll.dll','rpcrt4.dll','shcore.dll','imm32.dll','version.dll','uxtheme.dll','winmm.dll')
    if(-not $Dependencies.Count -or @($Dependencies|Where-Object{$_ -notin $systemDlls -and $_ -cnotmatch '^api-ms-win-core-[a-z0-9-]+\.dll$'}).Count){
        throw ('Unexpected non-system/static-runtime dependency; do not silently omit DLLs: '+($Dependencies -join ', '))
    }
}
