# Shared identity checks for the existing V1 portable and guest packagers.
# This file does not build, launch the product, or create a package.
function Test-V1ProductToolOnlyPath([string]$Path) {
    return $Path -cmatch '^(docs/|tests/|scripts/v1-[^/]+\.ps1$|src/platform/windows/product/v1_guest_[^/]+\.(cpp|h)$)'
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
