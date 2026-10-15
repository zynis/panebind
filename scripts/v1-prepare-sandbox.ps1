[CmdletBinding()]
param(
    [ValidatePattern('^[0-9a-f]{32}$')][string]$RunId = ([Guid]::NewGuid().ToString('N')),
    [string]$BuildDirectory,
    [string]$VSCodeAssetDirectory,
    [string]$PortablePackageDirectory,
    [ValidatePattern('^[0-9a-f]{40}$')][string]$SourceCommit,
    [ValidateSet('DebugAndRelease','Debug','Release')][string]$GuestConfigurations='DebugAndRelease',
    # Retained for callers of the first draft. Fixture identity is now always
    # verified because the committed fixture has an identity-only entry.
    [switch]$FixtureBuildIdentity
)
$ErrorActionPreference = 'Stop'
function Assert-PackagingSource([string]$Repository,[string]$Runtime,[string]$PackagingHead) {
    if($Runtime -cnotmatch '^[0-9a-f]{40}$' -or $PackagingHead -cnotmatch '^[0-9a-f]{40}$'){throw 'Explicit full runtime and packaging commit identities are required.'}
    & git -C $Repository cat-file -e ($Runtime+'^{commit}')
    if($LASTEXITCODE -ne 0){throw 'Runtime SourceCommit is not an available local commit.'}
    & git -C $Repository merge-base --is-ancestor $Runtime $PackagingHead
    if($LASTEXITCODE -ne 0){throw 'Runtime SourceCommit must be an ancestor of the actual packaging HEAD.'}
    $actualHead=(& git -C $Repository rev-parse HEAD).Trim()
    if($LASTEXITCODE -ne 0 -or $actualHead -cne $PackagingHead){throw 'Packaging HEAD changed during identity verification.'}
    $deliveryDocs=@('docs/V1_QUICK_START.md','docs/reports/V1_ACCEPTANCE.json','docs/reports/V1_DELIVERY_REPORT.md')
    # Inspect every intervening commit as well as the endpoint diff: a source
    # edit followed by a revert is not a docs-only finalization.
    $rangeFiles=@(& git -C $Repository log --format= --name-only --no-renames ($Runtime+'..'+$PackagingHead) --)
    if($LASTEXITCODE -ne 0){throw 'Cannot inspect runtime-to-final commit changes.'}
    $endFiles=@(& git -C $Repository diff --name-only --no-renames $Runtime $PackagingHead --)
    if($LASTEXITCODE -ne 0){throw 'Cannot compare runtime and packaging trees.'}
    $changed=@(@($rangeFiles)+@($endFiles)|Where-Object{$_}|Sort-Object -Unique)
    if(@($changed|Where-Object{$_ -cnotin $deliveryDocs}).Count){throw 'Only the three delivery documents may differ after Runtime; source, scripts, LICENSE, AGENTS and other files are forbidden.'}
    $cleanScope=@('src','tests','scripts','cmake','CMakeLists.txt','LICENSE','AGENTS.md')+$deliveryDocs
    $dirty=@(& git -C $Repository diff --name-only HEAD -- @cleanScope)
    if($LASTEXITCODE -ne 0){throw 'Cannot inspect source and delivery-document worktree status.'}
    $untracked=@(& git -C $Repository ls-files --others --exclude-standard -- @cleanScope)
    if($LASTEXITCODE -ne 0 -or $dirty.Count -or $untracked.Count){throw 'Commit source and the three delivery documents before packaging; unrelated plan input remains permitted.'}
    foreach($relative in @($deliveryDocs)+@('LICENSE','AGENTS.md')){
        & git -C $Repository ls-files --error-unmatch -- $relative | Out-Null
        if($LASTEXITCODE -ne 0){throw 'Delivery documents, LICENSE and AGENTS must belong to the committed packaging HEAD.'}
    }
    $inputs=@(foreach($relative in $deliveryDocs){
        $path=Join-Path $Repository $relative
        $item=Get-Item -LiteralPath $path
        if($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Delivery input must be an ordinary file.'}
        [ordered]@{path=$relative;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    })
    return [ordered]@{source_commit=$Runtime;packaging_head=$PackagingHead;source_scope_equivalent=$true;
        docs_only_changes=$changed;documentation_inputs=$inputs}
}
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $repo 'out\v1-dev' }
$build = [IO.Path]::GetFullPath($BuildDirectory)
if (-not $build.StartsWith($repo + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'BuildDirectory must be an existing build under this repository.'
}
$branch = (& git -C $repo branch --show-current).Trim()
if ($LASTEXITCODE -ne 0 -or $branch -cne 'codex/r1c4b-live-magnet') { throw 'Unexpected development branch.' }
$packagingHead = (& git -C $repo rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $packagingHead -cnotmatch '^[0-9a-f]{40}$') { throw 'Cannot identify actual packaging HEAD.' }
if(-not $SourceCommit){$SourceCommit=$packagingHead}
$packagingIdentity=Assert-PackagingSource $repo $SourceCommit $packagingHead
$head=$SourceCommit
$sourceScope = @('src', 'tests', 'scripts', 'cmake', 'CMakeLists.txt')
$dirty = @(& git -C $repo diff --name-only HEAD -- @sourceScope)
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect tracked source status.' }
$untrackedSource = @(& git -C $repo ls-files --others --exclude-standard -- @sourceScope)
if ($LASTEXITCODE -ne 0 -or $dirty.Count -or $untrackedSource.Count) {
    throw 'Commit source/build/script changes before packaging; untracked plan input under docs is permitted.'
}
$trackedChanges = @(& git -C $repo diff --name-only HEAD --)
$packageRoot = Join-Path $repo ('uat\v1-sandbox\' + $RunId)
if (Test-Path -LiteralPath $packageRoot) { throw 'RunId directory already exists; preserve it and use a new RunId.' }
$inputDir = Join-Path $packageRoot 'input'
$outputDir = Join-Path $packageRoot 'output'
$remoteRoot = 'I:\PaneBindMVP1Runs\' + $RunId
$utf8 = New-Object Text.UTF8Encoding($false)
function Write-NewText([string]$Path,[string]$Text) {
    $bytes = $script:utf8.GetBytes($Text)
    $stream = [IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try { $stream.Write($bytes,0,$bytes.Length) } finally { $stream.Dispose() }
}
function Find-Binary([string]$Configuration,[string]$FileName) {
    $paths = @((Join-Path $script:build ('src\platform\windows\' + $Configuration + '\' + $FileName)),
        (Join-Path $script:build ($Configuration + '\' + $FileName)),
        (Join-Path $script:build ('bin\' + $Configuration + '\' + $FileName)))
    $found = @($paths | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
    if ($found.Count -ne 1) { throw "Expected one exact $Configuration/$FileName binary; candidates: $($paths -join ', ')" }
    if ((Get-Item -LiteralPath $found[0]).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Binary must not be a reparse point.' }
    return $found[0]
}
function Read-BuildIdentity([string]$Executable) {
    # Both documented identity-only entries return before UI/hook/input setup.
    $beforeHash=(Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $Executable
    $start.Arguments = '--build-identity'
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $start
    if (-not $process.Start()) { throw 'Identity process did not start.' }
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    try {
        if (-not $process.WaitForExit(10000)) {
            $process.Kill()
            throw 'Identity-only process exceeded ten seconds; no candidate was run.'
        }
        if ($process.ExitCode -ne 0) { throw 'Identity-only process failed.' }
        $value = $stdout.GetAwaiter().GetResult() | ConvertFrom-Json
        if ($value.source_commit -cne $script:head) { throw 'Actual binary source_commit does not equal the explicit runtime SourceCommit.' }
        if ((Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash -cne $beforeHash) {
            throw 'Binary changed while its build identity was read; wait for builds to finish.'
        }
        $value | Add-Member -NotePropertyName verified_binary_sha256 -NotePropertyValue $beforeHash
        return $value
    } finally { $process.Dispose() }
}
$testAssets=@()
$vsCodeZip=$null
if($VSCodeAssetDirectory){
    # One pinned, previously downloaded official test asset, not a general
    # application downloader or permission to package arbitrary executables.
    $assetRoot=[IO.Path]::GetFullPath($VSCodeAssetDirectory)
    $assetParent=Join-Path $repo 'uat\v1-assets'
    if(-not [string]::Equals([IO.Path]::GetDirectoryName($assetRoot),$assetParent,[StringComparison]::OrdinalIgnoreCase) -or
       [IO.Path]::GetFileName($assetRoot) -cnotmatch '^[0-9a-f]{32}$'){throw 'VS Code asset must be an exact new asset directory under this repository ignored uat/v1-assets.'}
    $cursor=$assetRoot
    while($cursor -and $cursor.StartsWith($repo,[StringComparison]::OrdinalIgnoreCase)){
        if((Get-Item -LiteralPath $cursor).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'VS Code asset ancestors must not be reparse points.'}
        $cursor=[IO.Path]::GetDirectoryName($cursor)
    }
    $receiptPath=Join-Path $assetRoot 'asset-receipt.json'
    & git -C $repo check-ignore -- $receiptPath | Out-Null
    if($LASTEXITCODE -ne 0){throw 'VS Code receipt must remain an ignored local test asset.'}
    foreach($name in @('asset-receipt.json','official-update-metadata.json','VSCode-win32-x64-1.142.0.zip')){
        $file=Get-Item -LiteralPath (Join-Path $assetRoot $name)
        if($file.PSIsContainer -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'VS Code asset files must be ordinary files.'}
    }
    $assetReceipt=Get-Content -LiteralPath $receiptPath -Encoding UTF8 -Raw | ConvertFrom-Json
    $official=Get-Content -LiteralPath (Join-Path $assetRoot 'official-update-metadata.json') -Encoding UTF8 -Raw | ConvertFrom-Json
    $assetHash='ACE1DA7D1432D43C1AB4B2BBF643F6494F6D25DEC07494976755EAC522CD50D9'
    $exeHash='5CA701A1FACB91FEC1AC6BBAE11FC837821A1DA34DDEBB91A0BAD7E87AB88144'
    $assetCommit='6d62b1d5dace93a33b3a6d3eca1317bd5e15bcb1'
    $assetUrl='https://vscode.download.prss.microsoft.com/dbazure/download/stable/'+$assetCommit+'/VSCode-win32-x64-1.142.0.zip'
    if($assetReceipt.schema -cne 'panebind-v1-internal-test-asset/1' -or $assetReceipt.version -cne '1.142.0' -or
       $assetReceipt.upstream_commit -cne $assetCommit -or $assetReceipt.zip.sha256_actual -cne $assetHash -or
       $assetReceipt.zip.sha256_from_official_metadata -cne $assetHash -or -not $assetReceipt.zip.metadata_hash_match -or
       $assetReceipt.inspected_binary.sha256 -cne $exeHash -or $assetReceipt.inspected_binary.authenticode_status -cne 'Valid' -or
       $assetReceipt.download.ssl_verify_result -ne 0 -or $assetReceipt.download.tls_peer_verification_disabled -ne $false -or
       $assetReceipt.download_url -cne $assetUrl -or $official.url -cne $assetUrl -or
       $official.version -cne $assetCommit -or $official.productVersion -cne '1.142.0' -or
       $official.sha256hash -cne $assetHash.ToLowerInvariant()){throw 'VS Code receipt and official metadata do not match the one reviewed official asset.'}
    $vsCodeZip=Join-Path $assetRoot 'VSCode-win32-x64-1.142.0.zip'
    if((Get-Item -LiteralPath $vsCodeZip).Length -ne 351679077 -or
       (Get-FileHash -LiteralPath $vsCodeZip -Algorithm SHA256).Hash -cne $assetHash){throw 'VS Code ZIP no longer matches the verified official bytes.'}
    $testAssets+=,[ordered]@{kind='VSCode';version='1.142.0';commit=$assetCommit;relative_path='VSCode-test.zip';
        zip_sha256=$assetHash;zip_size=351679077;exe_sha256=$exeHash;exe_relative_path='Code.exe';
        expected_entries=3902;max_entries=10000;expected_expanded_bytes=1035323400;max_expanded_bytes=1610612736;
        guest_directory=('C:\PaneBindV1\Applications\'+$RunId+'\VSCode');license_scope='internal_test_only_not_product_distribution'}
}
$binaries = @()
foreach ($configuration in @('Debug','Release')) {
    $product = Find-Binary $configuration 'panebind.exe'
    $fixture = Find-Binary $configuration 'panebind-v1-guest-fixture.exe'
    $binaries += [ordered]@{ configuration=$configuration; product=$product; fixture=$fixture;
        product_identity=(Read-BuildIdentity $product);
        fixture_identity=(Read-BuildIdentity $fixture);
        fixture_identity_kind='binary_self_reported' }
}
$portableTest=$null
$portableZip=$null
if($PortablePackageDirectory){
    if($GuestConfigurations -ceq 'Debug'){throw 'Portable ZIP validation is only available in a Release-containing guest run.'}
    $portableRoot=[IO.Path]::GetFullPath($PortablePackageDirectory)
    $portableParent=Join-Path $repo 'uat\v1-portable'
    $portableId=[IO.Path]::GetFileName($portableRoot)
    if(-not [string]::Equals([IO.Path]::GetDirectoryName($portableRoot),$portableParent,[StringComparison]::OrdinalIgnoreCase) -or
       $portableId -cnotmatch '^[0-9a-f]{32}$'){throw 'Portable input must be one exact ignored uat/v1-portable/PackageId directory.'}
    $cursor=$portableRoot
    while($cursor -and $cursor.StartsWith($repo,[StringComparison]::OrdinalIgnoreCase)){
        if((Get-Item -LiteralPath $cursor).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Portable input ancestors must not be reparse points.'}
        $cursor=[IO.Path]::GetDirectoryName($cursor)
    }
    $portableReceiptPath=Join-Path $portableRoot 'package-receipt.json'
    & git -C $repo check-ignore -- $portableReceiptPath | Out-Null
    if($LASTEXITCODE -ne 0){throw 'Portable package must remain an ignored local artifact.'}
    $portableReceipt=Get-Content -LiteralPath $portableReceiptPath -Encoding UTF8 -Raw | ConvertFrom-Json
    $portableZip=Join-Path $portableRoot ('PaneBindV1-Release-'+$portableId+'.zip')
    $portableManifestPath=Join-Path $portableRoot 'PaneBindV1\manifest.json'
    $portablePayload=Get-Item -LiteralPath (Join-Path $portableRoot 'PaneBindV1')
    if(-not $portablePayload.PSIsContainer -or ($portablePayload.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Portable payload directory must be plain.'}
    foreach($filePath in @($portableReceiptPath,$portableZip,$portableManifestPath)){
        $item=Get-Item -LiteralPath $filePath
        if($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Portable inputs must be ordinary files.'}
    }
    $portableManifest=Get-Content -LiteralPath $portableManifestPath -Encoding UTF8 -Raw | ConvertFrom-Json
    $releaseIdentity=@($binaries | Where-Object configuration -ceq 'Release')[0].product_identity
    $portablePackagingHead=if($portableReceipt.packaging_head){$portableReceipt.packaging_head}else{$portableReceipt.source_commit}
    $innerPackagingHead=if($portableManifest.packaging_head){$portableManifest.packaging_head}else{$portableManifest.source_commit}
    if($portableReceipt.schema -cne 'panebind-v1-portable-receipt/1' -or $portableReceipt.package_id -cne $portableId -or
       $portableReceipt.source_commit -cne $head -or $portableReceipt.binary_sha256 -cne $releaseIdentity.verified_binary_sha256 -or
       $portablePackagingHead -cne $packagingHead -or $innerPackagingHead -cne $packagingHead -or
       $portableManifest.schema -cne 'panebind-v1-portable/1' -or $portableManifest.source_commit -cne $head -or
       $portableManifest.package_id -cne $portableId -or $portableManifest.configuration -cne 'Release' -or
       $portableManifest.binary_identity.source_commit -cne $head -or $portableManifest.binary_sha256 -cne $portableReceipt.binary_sha256 -or
       $portableManifest.binary_identity.requires_elevation -ne $false -or
       (Get-FileHash -LiteralPath $portableZip -Algorithm SHA256).Hash -cne $portableReceipt.zip_sha256 -or
       (Get-FileHash -LiteralPath $portableManifestPath -Algorithm SHA256).Hash -cne $portableReceipt.manifest_sha256){throw 'Portable ZIP must bind this exact committed Release build, receipt and manifest.'}
    $portableNames=@('panebind.exe','portable.flag','LICENSE','manifest.json')+@('LU6HZetfH5BlUeiVLgBtAGQA','TZFuf/SLDmYuAG0AZAA=','8l3ld+6VmJguAG0AZAA=' | ForEach-Object {[Text.Encoding]::Unicode.GetString([Convert]::FromBase64String($_))})
    $portableFiles=@($portableReceipt.files);$seenPortable=@{};$expanded=[long]0
    if($portableFiles.Count -ne 7){throw 'Portable receipt must describe exactly seven user package files.'}
    foreach($file in $portableFiles){
        if($file.relative_path -cnotin $portableNames -or $seenPortable.ContainsKey($file.relative_path) -or
           $file.sha256 -cnotmatch '^[A-F0-9]{64}$' -or [long]$file.size -le 0 -or [long]$file.size -gt 32MB){throw 'Unknown, duplicate or unbounded portable file.'}
        $seenPortable[$file.relative_path]=$file;$expanded+=[long]$file.size
    }
    if($seenPortable['panebind.exe'].sha256 -cne $portableReceipt.binary_sha256 -or
       $seenPortable['manifest.json'].sha256 -cne $portableReceipt.manifest_sha256 -or @($portableManifest.files).Count -ne 6){throw 'Portable receipt payload identity differs from its binary/manifest identity.'}
    $innerSeen=@{}
    foreach($file in @($portableManifest.files)){
        if($file.relative_path -ceq 'manifest.json' -or -not $seenPortable.ContainsKey($file.relative_path) -or $innerSeen.ContainsKey($file.relative_path) -or
           $file.sha256 -cne $seenPortable[$file.relative_path].sha256 -or [long]$file.size -ne [long]$seenPortable[$file.relative_path].size){throw 'Portable inner manifest differs from the exact archive inventory.'}
        $innerSeen[$file.relative_path]=$true
    }
    $zipBytes=(Get-Item -LiteralPath $portableZip).Length
    if($expanded -gt 32MB -or $zipBytes -le 0 -or $zipBytes -gt 32MB){throw 'Portable test archive exceeds the small package bound.'}
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive=[IO.Compression.ZipFile]::OpenRead($portableZip)
    try{
        $entries=@($archive.Entries | Where-Object {$_.Name});$seenEntries=@{}
        if($archive.Entries.Count -gt 8 -or $entries.Count -ne 7){throw 'Portable archive must contain seven actual payload files.'}
        foreach($entry in $archive.Entries){
            $name=$entry.FullName.Replace('\','/')
            if(-not $entry.Name){if($name -cne 'PaneBindV1/'){throw 'Unknown portable ZIP directory.'};continue}
            if(-not $name.StartsWith('PaneBindV1/',[StringComparison]::Ordinal)){throw 'Portable ZIP root mismatch.'}
            $leaf=$name.Substring(11)
            if($leaf -cnotin $portableNames -or $seenEntries.ContainsKey($leaf) -or
               (($entry.ExternalAttributes -shr 16) -band 0xf000) -notin @(0,0x8000) -or ($entry.ExternalAttributes -band 0x400)){throw 'Portable ZIP path, duplicate or link rejected.'}
            $expected=$seenPortable[$leaf];if($entry.Length -ne [long]$expected.size){throw 'Portable ZIP file size mismatch.'}
            $stream=$entry.Open();$digest=[Security.Cryptography.SHA256]::Create()
            try{$actualHash=([BitConverter]::ToString($digest.ComputeHash($stream))).Replace('-','')}finally{$digest.Dispose();$stream.Dispose()}
            if($actualHash -cne $expected.sha256){throw 'Portable ZIP file hash mismatch.'};$seenEntries[$leaf]=$true
        }
    }finally{$archive.Dispose()}
    $portableTest=[ordered]@{kind='PaneBindReleasePortable';relative_path='PaneBindV1-portable-test.zip';package_id=$portableId;
        source_commit=$head;packaging_head=$portablePackagingHead;zip_sha256=$portableReceipt.zip_sha256;zip_size=$zipBytes;manifest_sha256=$portableReceipt.manifest_sha256;
        exe_sha256=$portableReceipt.binary_sha256;expanded_bytes=$expanded;files=$portableFiles;function_result='NOT_RUN'}
}
[IO.Directory]::CreateDirectory($inputDir) | Out-Null
[IO.Directory]::CreateDirectory($outputDir) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $inputDir 'Debug')) | Out-Null
foreach ($binary in $binaries) {
    $target = if ($binary.configuration -ceq 'Release') { $inputDir } else { Join-Path $inputDir 'Debug' }
    [IO.File]::Copy($binary.product,(Join-Path $target 'panebind.exe'),$false)
    [IO.File]::Copy($binary.fixture,(Join-Path $target 'panebind-v1-guest-fixture.exe'),$false)
    if((Get-FileHash -LiteralPath (Join-Path $target 'panebind.exe') -Algorithm SHA256).Hash -cne $binary.product_identity.verified_binary_sha256 -or
       (Get-FileHash -LiteralPath (Join-Path $target 'panebind-v1-guest-fixture.exe') -Algorithm SHA256).Hash -cne $binary.fixture_identity.verified_binary_sha256){
        throw 'Copied binary changed after the source-identity check; this incomplete RunId is preserved.'
    }
}
[IO.File]::Copy((Join-Path $PSScriptRoot 'v1-sandbox-guest.ps1'),(Join-Path $inputDir 'v1-sandbox-guest.ps1'),$false)
Write-NewText (Join-Path $inputDir 'panebind-v1-input.marker') $RunId
Write-NewText (Join-Path $outputDir 'panebind-v1-output.marker') $RunId
$payloadFiles = @('panebind.exe','panebind-v1-guest-fixture.exe','Debug\panebind.exe',
    'Debug\panebind-v1-guest-fixture.exe','v1-sandbox-guest.ps1','panebind-v1-input.marker')
if($vsCodeZip){
    $copied=Join-Path $inputDir 'VSCode-test.zip'
    [IO.File]::Copy($vsCodeZip,$copied,$false)
    if((Get-FileHash -LiteralPath $copied -Algorithm SHA256).Hash -cne $testAssets[0].zip_sha256 -or
       (Get-FileHash -LiteralPath $vsCodeZip -Algorithm SHA256).Hash -cne $testAssets[0].zip_sha256){throw 'VS Code ZIP changed during package copy; incomplete batch preserved.'}
    $payloadFiles+='VSCode-test.zip'
}
if($portableZip){
    $copied=Join-Path $inputDir 'PaneBindV1-portable-test.zip'
    [IO.File]::Copy($portableZip,$copied,$false)
    if((Get-FileHash -LiteralPath $copied -Algorithm SHA256).Hash -cne $portableTest.zip_sha256 -or
       (Get-FileHash -LiteralPath $portableZip -Algorithm SHA256).Hash -cne $portableTest.zip_sha256){throw 'Portable ZIP changed while copied; incomplete batch preserved.'}
    $payloadFiles+='PaneBindV1-portable-test.zip'
}
$files = @($payloadFiles | ForEach-Object {
    $file = Get-Item -LiteralPath (Join-Path $inputDir $_)
    [ordered]@{ relative_path=$_; size=$file.Length; sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
})
$manifest = [ordered]@{
    schema='panebind-v1-sandbox/1'; run_id=$RunId; source_commit=$head; packaging_head=$packagingHead; branch=$branch;
    source_scope_equivalent=$true;docs_only_changes=$packagingIdentity.docs_only_changes;documentation_inputs=$packagingIdentity.documentation_inputs;
    created_utc=[DateTime]::UtcNow.ToString('o'); files=$files;
    test_assets=$testAssets;
    portable_test=$portableTest;
    guest_configurations=$GuestConfigurations;
    configurations=@($binaries | ForEach-Object { [ordered]@{ name=$_.configuration; product_identity=$_.product_identity;
        fixture_identity=$_.fixture_identity; fixture_identity_kind=$_.fixture_identity_kind; manifest_expected_source=$head } });
    tracked_non_source_changes=$trackedChanges;
    automated_driver=$true; human_uat='NOT_RUN'; requested_integrity='Medium/non-elevated';
    guest_input='C:\PaneBindV1\Input'; guest_output='C:\PaneBindV1\Output';
    host_root=$remoteRoot; medium_result='NOT_RUN'; feature_result='NOT_RUN'
    requested_sandbox_resources=[ordered]@{vgpu='Disable';memory_mb=4096}
}
Write-NewText (Join-Path $inputDir 'manifest.json') ($manifest | ConvertTo-Json -Depth 12)
$xmlRemoteInput = [Security.SecurityElement]::Escape($remoteRoot + '\input')
$xmlRemoteOutput = [Security.SecurityElement]::Escape($remoteRoot + '\output')
$wsb = @"
<Configuration>
  <vGPU>Disable</vGPU>
  <MemoryInMB>4096</MemoryInMB>
  <Networking>Disable</Networking>
  <ClipboardRedirection>Disable</ClipboardRedirection>
  <AudioInput>Disable</AudioInput>
  <VideoInput>Disable</VideoInput>
  <PrinterRedirection>Disable</PrinterRedirection>
  <MappedFolders>
    <MappedFolder><HostFolder>$xmlRemoteInput</HostFolder><SandboxFolder>C:\PaneBindV1\Input</SandboxFolder><ReadOnly>true</ReadOnly></MappedFolder>
    <MappedFolder><HostFolder>$xmlRemoteOutput</HostFolder><SandboxFolder>C:\PaneBindV1\Output</SandboxFolder><ReadOnly>false</ReadOnly></MappedFolder>
  </MappedFolders>
  <LogonCommand><Command>powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\PaneBindV1\Input\v1-sandbox-guest.ps1 -RunId $RunId -Configurations $GuestConfigurations</Command></LogonCommand>
</Configuration>
"@
Write-NewText (Join-Path $packageRoot 'PaneBindV1.wsb') $wsb
$zipPath = Join-Path $packageRoot ($RunId + '.zip')
Compress-Archive -LiteralPath @($inputDir,$outputDir,(Join-Path $packageRoot 'PaneBindV1.wsb')) -DestinationPath $zipPath -CompressionLevel Optimal
$receipt = [ordered]@{ schema='panebind-v1-package-receipt/1'; run_id=$RunId; source_commit=$head; packaging_head=$packagingHead;
    source_scope_equivalent=$true;documentation_inputs=$packagingIdentity.documentation_inputs;
    package_root=$packageRoot; remote_root=$remoteRoot; zip=$zipPath;
    zip_sha256=(Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash;
    manifest_sha256=(Get-FileHash -LiteralPath (Join-Path $inputDir 'manifest.json') -Algorithm SHA256).Hash;
    wsb_sha256=(Get-FileHash -LiteralPath (Join-Path $packageRoot 'PaneBindV1.wsb') -Algorithm SHA256).Hash;
    deployed=$false; guest_started=$false; medium_result='NOT_RUN'; feature_result='NOT_RUN' }
Write-NewText (Join-Path $packageRoot 'package-receipt.json') ($receipt | ConvertTo-Json -Depth 6)
$receipt | ConvertTo-Json -Depth 6
