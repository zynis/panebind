[CmdletBinding()]
param(
    [ValidatePattern('^[0-9a-f]{32}$')][string]$RunId = ([Guid]::NewGuid().ToString('N')),
    [string]$BuildDirectory,
    # Driver-only changes do not require rebuilding or relabelling the product.
    [string]$ProductBuildDirectory,
    [string]$VSCodeAssetDirectory,
    [string]$TerminalAssetDirectory,
    [string]$OfficialAppAssetDirectory,
    [string]$PortablePackageDirectory,
    [switch]$MediumEnvironmentProbe,
    [switch]$MediumDesktopProbe,
    [switch]$MediumNativeIdentityOnly,
    [switch]$StandardGuestNativeClients,
    [switch]$StandardLogonNativeIdentity,
    [ValidatePattern('^[0-9a-f]{40}$')][string]$SourceCommit,
    [ValidatePattern('^[0-9a-f]{40}$')][string]$DriverSourceCommit,
    [ValidateSet('DebugAndRelease','Debug','Release')][string]$GuestConfigurations='DebugAndRelease',
    [ValidateSet('Full','IndependentEntry','SettingsDiagnostic','CoreAndApplications','ApplicationsOnly','EntryAndSettings','OwnedComponentsOnly','InterruptionOnly','FiniteSessionOnly','CurrentCoreChainOnly','GroupBoundsOnly','NativeClientsOnly','StretchModesOnly','PauseRoutingOnly','OwnedRulesOnly','StretchInputOnly','StretchBoundaryOnly','ExitRecoveryOnly','TrayReadinessOnly','GeometryHistoryOnly','VisualOwnershipOnly','ResizeMagnetOnly','FixedAndTerminalOnly','MappingRemainderOnly','HotkeyRemainderOnly','MotionGuardOnly','CornerInputOnly','PortableEntryOnly','RemainingMoveContractsOnly','RecoveryContractsOnly','RemainingNativeContractsOnly','ModifierLockOnly','SettingsValidationOnly','SettingsControlsRemainderOnly','FeedbackRemainderOnly','GroupRemainderOnly','GroupHeightOnly','LCornerOnly','DockRemainderOnly','RestoreRemainderOnly','CommandsRemainderOnly','ArbitrationRemainderOnly','ArbitrationRecoveryOnly','CommandsGeometryOnly','LowerFactsOnly','LifecycleRemainderOnly','HighContrastOnly','NativeTopDropOnly','SessionInterruptionOnly','QueueFailureOnly','CallbackOwnerStopOnly','TransparencyRemainderOnly','SystemShakeCoexistenceOnly','RulesPathOnly','MediumDesktopEnvironmentOnly','MediumDesktopUiOnly','MediumDesktopNativeOnly','MediumDesktopNativeClientOnly')][string]$Suite='Full',
    # Retained for callers of the first draft. Fixture identity is now always
    # verified because the committed fixture has an identity-only entry.
    [switch]$FixtureBuildIdentity
)
$ErrorActionPreference = 'Stop'
if($StandardLogonNativeIdentity){
    if($Suite -cne 'NativeClientsOnly' -or $GuestConfigurations -cne 'Release' -or
       $StandardGuestNativeClients -or $MediumNativeIdentityOnly -or $MediumEnvironmentProbe -or
       $VSCodeAssetDirectory -or $TerminalAssetDirectory -or $OfficialAppAssetDirectory -or $PortablePackageDirectory){
        throw 'Standard logon identity requires Release/NativeClientsOnly without assets, other probes or product input.'
    }
    $MediumDesktopProbe=$true
}
if($StandardGuestNativeClients){
    if($Suite -cne 'NativeClientsOnly' -or $GuestConfigurations -cne 'Release' -or
       $MediumNativeIdentityOnly -or $StandardLogonNativeIdentity -or $MediumEnvironmentProbe -or $PortablePackageDirectory){
        throw 'Standard guest native clients require Release/NativeClientsOnly without another probe or portable suite.'
    }
    $MediumDesktopProbe=$true
}
if($MediumNativeIdentityOnly){
    if($Suite -cne 'NativeClientsOnly' -or $GuestConfigurations -cne 'Release' -or $MediumEnvironmentProbe -or
       $VSCodeAssetDirectory -or $TerminalAssetDirectory -or $OfficialAppAssetDirectory -or $PortablePackageDirectory){
        throw 'Native identity contrast requires Release/NativeClientsOnly without application assets or other probes.'
    }
    $MediumDesktopProbe=$true
}
. (Join-Path $PSScriptRoot 'v1-package-identity.ps1')
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $repo 'out\v1-dev' }
$build = [IO.Path]::GetFullPath($BuildDirectory)
if (-not $build.StartsWith($repo + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'BuildDirectory must be an existing build under this repository.'
}
$productBuild=if($ProductBuildDirectory){[IO.Path]::GetFullPath($ProductBuildDirectory)}else{$build}
if(-not $productBuild.StartsWith($repo+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'ProductBuildDirectory must remain inside this repository.'}
foreach($directory in @($build,$productBuild)){
    $cursor=$directory
    while($cursor -and $cursor.StartsWith($repo,[StringComparison]::OrdinalIgnoreCase)){
        if((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor).Attributes -band [IO.FileAttributes]::ReparsePoint)){
            throw 'Product/driver build-directory ancestors must not be reparse points.'}
        $cursor=[IO.Path]::GetDirectoryName($cursor)
    }
}
$branch = (& git -C $repo branch --show-current).Trim()
if ($LASTEXITCODE -ne 0 -or $branch -cne 'codex/r1c4b-live-magnet') { throw 'Unexpected development branch.' }
$packagingHead = (& git -C $repo rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $packagingHead -cnotmatch '^[0-9a-f]{40}$') { throw 'Cannot identify actual packaging HEAD.' }
if(-not $SourceCommit){$SourceCommit=$packagingHead}
if(-not $DriverSourceCommit){$DriverSourceCommit=$packagingHead}
$packagingIdentity=Assert-PackagingSource $repo $SourceCommit $packagingHead $DriverSourceCommit
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
function Find-Binary([string]$Configuration,[string]$FileName,[string]$Directory) {
    $paths = @((Join-Path $Directory ('src\platform\windows\' + $Configuration + '\' + $FileName)),
        (Join-Path $Directory ($Configuration + '\' + $FileName)),
        (Join-Path $Directory ('bin\' + $Configuration + '\' + $FileName)))
    $found = @($paths | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
    if ($found.Count -ne 1) { throw "Expected one exact $Configuration/$FileName binary; candidates: $($paths -join ', ')" }
    if ((Get-Item -LiteralPath $found[0]).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Binary must not be a reparse point.' }
    return $found[0]
}
function Read-BuildIdentity([string]$Executable,[string]$Kind,[string]$ExpectedSource) {
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
        Assert-V1BinaryInterface $value $Kind $ExpectedSource
        if ((Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash -cne $beforeHash) {
            throw 'Binary changed while its build identity was read; wait for builds to finish.'
        }
        $value | Add-Member -NotePropertyName verified_binary_sha256 -NotePropertyValue $beforeHash
        return $value
    } finally { $process.Dispose() }
}
$testAssets=@()
$vsCodeZip=$null
$terminalZip=$null
$terminalAsset=$null
if($TerminalAssetDirectory){
    $assetRoot=[IO.Path]::GetFullPath($TerminalAssetDirectory)
    if($assetRoot -cne (Join-Path $repo 'uat\v1-assets\e6a9aed8fa9d460f8c0117ac8690324e')){throw 'Only the reviewed exact official Terminal asset directory is accepted.'}
    $cursor=$assetRoot
    while($cursor -and $cursor.StartsWith($repo,[StringComparison]::OrdinalIgnoreCase)){
        if((Get-Item -LiteralPath $cursor).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Terminal asset ancestors must be plain.'}
        $cursor=[IO.Path]::GetDirectoryName($cursor)
    }
    foreach($name in @('asset-receipt.json','official-release.json','source-identity.json','upstream-LICENSE.txt','Microsoft.WindowsTerminal_1.25.2733.0_x64.zip')){
        $file=Get-Item -LiteralPath (Join-Path $assetRoot $name)
        if($file.PSIsContainer -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Terminal asset inputs must be ordinary files.'}
    }
    $receipt=Get-Content -LiteralPath (Join-Path $assetRoot 'asset-receipt.json') -Raw -Encoding UTF8|ConvertFrom-Json
    $official=Get-Content -LiteralPath (Join-Path $assetRoot 'official-release.json') -Raw -Encoding UTF8|ConvertFrom-Json
    $source=Get-Content -LiteralPath (Join-Path $assetRoot 'source-identity.json') -Raw -Encoding UTF8|ConvertFrom-Json
    $zipHash='BF3EF2012F6C44D8340A4C58125ACC9498D19B580F9890DC043CDF831852E796'
    $exeHash='1CBE52914F7437A4F064327383F1B4C45FE2EB8E92D4F8DAE0AD7EFCB13A20A1'
    $zipName='Microsoft.WindowsTerminal_1.25.2733.0_x64.zip'
    $url='https://github.com/microsoft/terminal/releases/download/v1.25.2733.0/'+$zipName
    $entry=@($official.assets|Where-Object name -ceq $zipName)
    if($receipt.schema -cne 'panebind-v1-terminal-test-asset/1' -or $receipt.asset_id -cne 'e6a9aed8fa9d460f8c0117ac8690324e' -or
       $receipt.version -cne '1.25.2733.0' -or $receipt.download_url -cne $url -or $receipt.tls_verification_disabled -ne $false -or
       $receipt.zip.sha256_actual -cne $zipHash -or $receipt.zip.entries -ne 63 -or $receipt.zip.expanded_bytes -ne 34870043 -or
       $receipt.inspected_binary.sha256 -cne $exeHash -or $receipt.inspected_binary.authenticode_status -cne 'Valid' -or
       $receipt.inspected_binary.signer -notmatch 'O=Microsoft Corporation' -or $receipt.license -cne 'MIT' -or
       $official.tag_name -cne 'v1.25.2733.0' -or $official.prerelease -ne $false -or $entry.Count -ne 1 -or
       $entry[0].digest -cne ('sha256:'+$zipHash.ToLowerInvariant()) -or $entry[0].browser_download_url -cne $url -or
       $source.source_commit -cne 'dc8ae365847d41f62d1ebca93815bb00959bc12f' -or
       (Get-FileHash -LiteralPath (Join-Path $assetRoot 'upstream-LICENSE.txt') -Algorithm SHA256).Hash -cne 'AD0CF28F3381CA9BB0BF101D127402D44C17BFA0991E1A00BFF7AE6679E9DADA'){
        throw 'Terminal receipt, official metadata, actual signature/license or source identity differs from reviewed asset.'}
    $terminalZip=Join-Path $assetRoot $zipName
    if((Get-Item -LiteralPath $terminalZip).Length -ne 12127272 -or
       (Get-FileHash -LiteralPath $terminalZip -Algorithm SHA256).Hash -cne $zipHash){throw 'Official Terminal ZIP bytes changed.'}
    $terminalAsset=[ordered]@{kind='WindowsTerminalPortable';version='1.25.2733.0';source_commit=$source.source_commit;
        relative_path='Terminal-test.zip';zip_sha256=$zipHash;zip_size=12127272;exe_sha256=$exeHash;
        expected_entries=63;expanded_bytes=34870043;archive_root='terminal-1.25.2733.0';
        guest_directory=('C:\PaneBindV1\Terminal\'+$RunId);license='MIT';asset_scope='internal_guest_test_only';
        preparation='NOT_RUN';activation='NOT_RUN'}
}
$officialApps=$null
$officialFiles=@()
if($OfficialAppAssetDirectory){
    # Pinned user-provided Windows media payloads, verified with the official
    # IAppxFactory signed-blockmap API. Internal guest assets, not a product
    # redistribution or a generic Appx importer. No media executable is run.
    $assetRoot=[IO.Path]::GetFullPath($OfficialAppAssetDirectory)
    if($assetRoot -cne (Join-Path $repo 'uat\v1-assets\f08fc54e44cf43eb811bc317c4249158')){throw 'Only the reviewed exact official-media asset directory is accepted.'}
    $cursor=$assetRoot
    while($cursor -and $cursor.StartsWith($repo,[StringComparison]::OrdinalIgnoreCase)){
        if((Get-Item -LiteralPath $cursor).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Official asset ancestors must not be reparse points.'}
        $cursor=[IO.Path]::GetDirectoryName($cursor)
    }
    $pins=@(
        @('official-dependencies','Microsoft.VCLibs.140.00_14.0.30704.0_x64__8wekyb3d8bbwe',16,2112335,'DA48D70F7F15E6E5B107B7E1345EFA5E6F19595007656C4FB959A3D88BC4273B'),
        @('official-dependencies','Microsoft.VCLibs.140.00.UWPDesktop_14.0.30704.0_x64__8wekyb3d8bbwe',28,14114229,'2C5FDF0360C91A98A38DD68ED3CEBF39638108F67F5198B938190DA3CA92DD71'),
        @('official-dependencies','Microsoft.UI.Xaml.2.8_8.2310.30001.0_x64__8wekyb3d8bbwe',9,17113065,'1425B2B6710481AA35776CCCCD6B0425A39B086138A653859850598491ED4B2C'),
        @('official-apps','Microsoft.WindowsNotepad_11.2312.18.0_x64__8wekyb3d8bbwe',232,11850868,'4E39B3F2FB2934F288161ED7369DB719976F8A5C0AC88A517A1CA31FAF9BB4BE'),
        @('official-apps','Microsoft.WindowsNotepad_11.2312.18.0_neutral_split.language-zh-hans_8wekyb3d8bbwe',4,22911,'06775630C3AF140B48F64D01407C6E20E9F9B07FF13697EF4FAF1584755F2E1B'),
        @('official-apps','Microsoft.WindowsNotepad_11.2312.18.0_neutral_split.scale-100_8wekyb3d8bbwe',22,33245,'85B25AB7C7531F4C8F49E0F82FC8AC0A1FA53FEFEF0D6BEB051B8E20F87AF7E6'),
        @('official-apps','Microsoft.WindowsNotepad_11.2312.18.0_neutral_split.scale-125_8wekyb3d8bbwe',22,38984,'AAB3795FF7EBDBF7AB6A7D798DAA2BA7CEDBDF02CA0448E74B8DBD6455995C93'),
        @('official-apps','Microsoft.WindowsTerminal_1.18.10301.0_x64__8wekyb3d8bbwe',245,18416538,'3ED96A9A5F540F3A29CCB71F230B63464318096032ED3FB43CF48D3C724CE5AB'),
        @('official-apps','Microsoft.WindowsNotepad_11.2312.18.0_neutral_~_8wekyb3d8bbwe',3,47446,'F0C1AA97C5A0FAD3D478C6B40C08C91D26346D6B8B2AA05BF3795BFB523D2BD7'),
        @('official-apps','Microsoft.WindowsTerminal_3001.18.10301.0_neutral_~_8wekyb3d8bbwe',3,23300,'C5DED41D2E70AA71EC251D0FD837C93C5CB49B821541BBAD49A8B6CFC57AFA82'))
    $auditHashes=@{'signed-blockmap-validation.json'='17B1687624C144DA874CD6D5975439C191F43D77945F15E1DA2873F52A8FDD58';'official-apps-inspection.json'='49DF1B2698788EB77ECB4F737A2DE92C0DCC4244273AAF8C61B7617C6F087C71'}
    foreach($name in $auditHashes.Keys){
        $item=Get-Item -LiteralPath (Join-Path $assetRoot $name)
        if($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
           (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash -cne $auditHashes[$name]){throw 'Reviewed official asset evidence identity changed.'}
    }
    $audit=Get-Content -LiteralPath (Join-Path $assetRoot 'signed-blockmap-validation.json') -Raw -Encoding UTF8|ConvertFrom-Json
    if($audit.schema -cne 'panebind-iso-signed-blockmap-validation/1' -or @($audit.packages).Count -ne 10 -or
       @($audit.packages|Where-Object {$_.native_hresult -cne '0x00000000' -or $_.validated_signed_blockmap -ne $true}).Count){throw 'All ten exact signed blockmaps must have actual successful validation.'}
    $packages=@()
    foreach($pin in $pins){
        $root=Join-Path $assetRoot ($pin[0]+'\Program Files\WindowsApps\'+$pin[1])
        $pending=New-Object 'System.Collections.Generic.Queue[string]';$pending.Enqueue($root);$rows=@()
        while($pending.Count){
            $directory=$pending.Dequeue();$actual=Get-Item -LiteralPath $directory
            if(-not $actual.PSIsContainer -or ($actual.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Official package directories must be plain.'}
            foreach($item in @(Get-ChildItem -LiteralPath $directory -Force)){
                if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Official package link rejected.'}
                if($item.PSIsContainer){$pending.Enqueue($item.FullName);continue}
                if($rows.Count -ge 600 -or $item.Length -gt 32MB){throw 'Official package bounds exceeded.'}
                $relative=$item.FullName.Substring($root.Length+1).Replace('\','/')
                if($relative -match '[|\r\n:]'){throw 'Official inventory path rejected.'}
                $rows+=,[pscustomobject]@{path=$relative;size=$item.Length;sha256=(Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash;source=$item.FullName}
            }
        }
        $rows=@($rows|Sort-Object path)
        $canonical=(@($rows|ForEach-Object{$_.path+'|'+$_.size+'|'+$_.sha256})-join "`n")+"`n"
        $digest=[Security.Cryptography.SHA256]::Create()
        try{$inventory=([BitConverter]::ToString($digest.ComputeHash($utf8.GetBytes($canonical)))).Replace('-','')}finally{$digest.Dispose()}
        if($rows.Count -ne $pin[2] -or ($rows|Measure-Object size -Sum).Sum -ne $pin[3] -or $inventory -cne $pin[4]){throw ('Reviewed package inventory changed: '+$pin[1])}
        $packages+=,[ordered]@{name=$pin[1];files=$pin[2];bytes=$pin[3];inventory_sha256=$pin[4];register_manifest=(Test-Path -LiteralPath (Join-Path $root 'AppxManifest.xml'))}
        foreach($row in $rows){$officialFiles+=,[pscustomobject]@{path=($pin[1]+'/'+$row.path);source=$row.source;size=$row.size;sha256=$row.sha256}}
    }
    $officialApps=[ordered]@{kind='ReviewedWindowsMediaApps';relative_path='OfficialApps-test.zip';asset_id='f08fc54e44cf43eb811bc317c4249158';
        guest_directory=('C:\PaneBindV1\OfficialApps\'+$RunId);packages=$packages;expected_entries=$officialFiles.Count;
        expanded_bytes=($officialFiles|Measure-Object size -Sum).Sum;zip_sha256=$null;zip_size=$null;
        signed_blockmap_evidence_sha256=$auditHashes['signed-blockmap-validation.json'];
        source_iso_whole_hash='NOT_VERIFIED';wim_warning='PRESERVED_SOME_FILES_HAVE_INCORRECT_REFERENCE_COUNT';
        license_scope='user_provided_official_media_internal_guest_test_only_not_product_distribution';registration='NOT_RUN';activation='NOT_RUN'}
}
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
    $product = Find-Binary $configuration 'panebind.exe' $productBuild
    $fixture = Find-Binary $configuration 'panebind-v1-guest-fixture.exe' $build
    $productDependencies=Get-V1SandboxBinaryDependencies $productBuild $product Product
    $fixtureDependencies=Get-V1SandboxBinaryDependencies $build $fixture Driver
    $productIdentity=Read-BuildIdentity $product Product $SourceCommit
    $fixtureIdentity=Read-BuildIdentity $fixture Driver $DriverSourceCommit
    if($productDependencies.binary_sha256 -cne $productIdentity.verified_binary_sha256 -or
       $fixtureDependencies.binary_sha256 -cne $fixtureIdentity.verified_binary_sha256){throw 'Binary changed between dependency and identity checks.'}
    $binaries += [ordered]@{ configuration=$configuration; product=$product; fixture=$fixture;
        product_identity=$productIdentity;fixture_identity=$fixtureIdentity;
        product_dependencies=$productDependencies;fixture_dependencies=$fixtureDependencies;
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
       $portablePackagingHead -cne $innerPackagingHead -or
       $portableManifest.schema -cne 'panebind-v1-portable/1' -or $portableManifest.source_commit -cne $head -or
       $portableManifest.package_id -cne $portableId -or $portableManifest.configuration -cne 'Release' -or
       $portableManifest.binary_identity.source_commit -cne $head -or $portableManifest.binary_sha256 -cne $portableReceipt.binary_sha256 -or
       $portableManifest.binary_identity.requires_elevation -ne $false -or
       (Get-FileHash -LiteralPath $portableZip -Algorithm SHA256).Hash -cne $portableReceipt.zip_sha256 -or
       (Get-FileHash -LiteralPath $portableManifestPath -Algorithm SHA256).Hash -cne $portableReceipt.manifest_sha256){throw 'Portable ZIP must bind this exact committed Release build, receipt and manifest.'}
    # Preserve the tested ZIP and its original packaging identity. A new driver
    # package can reference it only while the actual product scope is unchanged.
    $null=Assert-V1SourceScope $repo $head $portablePackagingHead Product
    $null=Assert-V1SourceScope $repo $portablePackagingHead $packagingHead Product
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
$ruleTargetPayloads=@()
foreach ($binary in $binaries) {
    $target = if ($binary.configuration -ceq 'Release') { $inputDir } else { Join-Path $inputDir 'Debug' }
    [IO.File]::Copy($binary.product,(Join-Path $target 'panebind.exe'),$false)
    [IO.File]::Copy($binary.fixture,(Join-Path $target 'panebind-v1-guest-fixture.exe'),$false)
    if((Get-FileHash -LiteralPath (Join-Path $target 'panebind.exe') -Algorithm SHA256).Hash -cne $binary.product_identity.verified_binary_sha256 -or
       (Get-FileHash -LiteralPath (Join-Path $target 'panebind-v1-guest-fixture.exe') -Algorithm SHA256).Hash -cne $binary.fixture_identity.verified_binary_sha256){
        throw 'Copied binary changed after the source-identity check; this incomplete RunId is preserved.'
    }
    if($Suite -ceq 'RulesPathOnly'){
        foreach($slot in @('A','B')){
            $rulePrefix=if($binary.configuration -ceq 'Debug'){'Debug\'}else{''}
            $relative=$rulePrefix+'RuleTargets\'+$slot+'\panebind-rule-app.exe'
            $copy=Join-Path $inputDir $relative
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($copy)) | Out-Null
            [IO.File]::Copy($binary.fixture,$copy,$false)
            if((Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash -cne $binary.fixture_identity.verified_binary_sha256){
                throw 'Exact A/B rule target must retain the current driver bytes; incomplete RunId preserved.'
            }
            $ruleTargetPayloads+=$relative
        }
    }
}
[IO.File]::Copy((Join-Path $PSScriptRoot 'v1-sandbox-guest.ps1'),(Join-Path $inputDir 'v1-sandbox-guest.ps1'),$false)
if($MediumEnvironmentProbe){[IO.File]::Copy((Join-Path $PSScriptRoot 'v1-medium-environment-probe-guest.ps1'),(Join-Path $inputDir 'v1-medium-environment-probe-guest.ps1'),$false)}
if($MediumDesktopProbe){[IO.File]::Copy((Join-Path $PSScriptRoot 'v1-medium-desktop-probe-guest.ps1'),(Join-Path $inputDir 'v1-medium-desktop-probe-guest.ps1'),$false)}
Write-NewText (Join-Path $inputDir 'panebind-v1-input.marker') $RunId
Write-NewText (Join-Path $outputDir 'panebind-v1-output.marker') $RunId
$payloadFiles = @('panebind.exe','panebind-v1-guest-fixture.exe','Debug\panebind.exe',
    'Debug\panebind-v1-guest-fixture.exe','v1-sandbox-guest.ps1','panebind-v1-input.marker')
$payloadFiles+=$ruleTargetPayloads
if($MediumEnvironmentProbe){$payloadFiles+='v1-medium-environment-probe-guest.ps1'}
if($MediumDesktopProbe){$payloadFiles+='v1-medium-desktop-probe-guest.ps1'}
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
if($officialApps){
    # Windows PowerShell 5.1 does not load the enum's assembly merely by
    # loading ZipFile's FileSystem assembly in a fresh packaging process.
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archivePath=Join-Path $inputDir $officialApps.relative_path
    $archive=[IO.Compression.ZipFile]::Open($archivePath,[IO.Compression.ZipArchiveMode]::Create)
    try{foreach($file in $officialFiles){
        $entry=$archive.CreateEntry($file.path,[IO.Compression.CompressionLevel]::Optimal)
        $source=[IO.File]::OpenRead($file.source);$target=$entry.Open()
        try{$source.CopyTo($target)}finally{$target.Dispose();$source.Dispose()}
        if((Get-FileHash -LiteralPath $file.source -Algorithm SHA256).Hash -cne $file.sha256){throw 'Official asset changed during archive creation; preserve incomplete batch.'}
    }}finally{$archive.Dispose()}
    $officialApps.zip_sha256=(Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
    $officialApps.zip_size=(Get-Item -LiteralPath $archivePath).Length
    $payloadFiles+=$officialApps.relative_path
}
if($terminalZip){
    $target=Join-Path $inputDir $terminalAsset.relative_path
    [IO.File]::Copy($terminalZip,$target,$false)
    if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -cne $terminalAsset.zip_sha256){throw 'Terminal ZIP changed during package copy.'}
    $payloadFiles+=$terminalAsset.relative_path
}
$files = @($payloadFiles | ForEach-Object {
    $file = Get-Item -LiteralPath (Join-Path $inputDir $_)
    [ordered]@{ relative_path=$_; size=$file.Length; sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
})
$manifest = [ordered]@{
    schema='panebind-v1-sandbox/1'; run_id=$RunId; source_commit=$head; packaging_head=$packagingHead; branch=$branch;
    product_source_commit=$head;driver_source_commit=$DriverSourceCommit;guest_contract='panebind-v1-guest-contract/2';
    product_interface=$packagingIdentity.interface_contract;
    product_debug_sha256=$binaries[0].product_identity.verified_binary_sha256;
    driver_debug_sha256=$binaries[0].fixture_identity.verified_binary_sha256;
    product_release_sha256=$binaries[1].product_identity.verified_binary_sha256;
    driver_release_sha256=$binaries[1].fixture_identity.verified_binary_sha256;
    product_unchanged_across_tool_changes=$packagingIdentity.product_unchanged_across_tool_changes;
    driver_unchanged_across_packaging_changes=$packagingIdentity.driver_unchanged_across_packaging_changes;
    source_scope_equivalent=$true;docs_only_changes=$packagingIdentity.docs_only_changes;documentation_inputs=$packagingIdentity.documentation_inputs;
    guest_account_mode=$(if($StandardLogonNativeIdentity){'RunBoundStandardLogonNativeIdentity'}elseif($StandardGuestNativeClients){'RunBoundStandardNativeClients'}else{'WDAGDefault'});
    guest_standard_account=$(if($StandardGuestNativeClients -or $StandardLogonNativeIdentity){'PBV1D_'+$RunId.Substring(0,12)}else{$null});
    guest_standard_run_id=$(if($StandardGuestNativeClients -or $StandardLogonNativeIdentity){$RunId}else{$null});
    created_utc=[DateTime]::UtcNow.ToString('o'); files=$files;
    test_assets=$testAssets;
    terminal_test_asset=$terminalAsset;
    official_app_assets=$officialApps;
    medium_environment_probe=[ordered]@{enabled=[bool]$MediumEnvironmentProbe;kind='ReadOnlyStandardGuestAcl'};
    # The legacy kind is the existing controller envelope, not a GUI result.
    medium_desktop_probe=[ordered]@{enabled=[bool]$MediumDesktopProbe;kind='OneStandardGuestGuiPrecheck';
        native_identity_only=[bool]$MediumNativeIdentityOnly;stop_after_probe=[bool]($MediumNativeIdentityOnly -or $StandardGuestNativeClients -or $StandardLogonNativeIdentity);
        standard_native_clients=[bool]$StandardGuestNativeClients;
        standard_logon_identity=[bool]$StandardLogonNativeIdentity;
        standard_native_api=$(if($StandardLogonNativeIdentity){'CreateProcessWithLogonW'}elseif($StandardGuestNativeClients){'CreateProcessAsUserW'}else{$null});
        followup_suite=$(if($StandardGuestNativeClients){'NativeClientsOnly'}else{$null});
        actual_suite=$(if($StandardLogonNativeIdentity){'StandardLogonNativeIdentity'}elseif($MediumNativeIdentityOnly){'MediumNativeIdentityOnly'}elseif($StandardGuestNativeClients){'StandardGuestNativeClients'}else{$null});
        native_identity_image=$(if($MediumNativeIdentityOnly -or $StandardGuestNativeClients -or $StandardLogonNativeIdentity){'panebind-v1-guest-fixture.exe'}else{$null})};
    portable_test=$portableTest;
    guest_configurations=$GuestConfigurations;
    guest_suite=$Suite;
    configurations=@($binaries | ForEach-Object { [ordered]@{ name=$_.configuration; product_identity=$_.product_identity;
        product_dependencies=$_.product_dependencies;fixture_dependencies=$_.fixture_dependencies;
        fixture_identity=$_.fixture_identity; fixture_identity_kind=$_.fixture_identity_kind;
        manifest_expected_source=$head;manifest_expected_driver_source=$DriverSourceCommit } });
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
  <LogonCommand><Command>powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\PaneBindV1\Input\v1-sandbox-guest.ps1 -RunId $RunId -Configurations $GuestConfigurations -Suite $Suite</Command></LogonCommand>
</Configuration>
"@
Write-NewText (Join-Path $packageRoot 'PaneBindV1.wsb') $wsb
$zipPath = Join-Path $packageRoot ($RunId + '.zip')
Compress-Archive -LiteralPath @($inputDir,$outputDir,(Join-Path $packageRoot 'PaneBindV1.wsb')) -DestinationPath $zipPath -CompressionLevel Optimal
$receipt = [ordered]@{ schema='panebind-v1-package-receipt/1'; run_id=$RunId; source_commit=$head; packaging_head=$packagingHead;
    product_source_commit=$head;driver_source_commit=$DriverSourceCommit;guest_contract='panebind-v1-guest-contract/2';
    product_interface=$packagingIdentity.interface_contract;configurations=$manifest.configurations;
    source_scope_equivalent=$true;documentation_inputs=$packagingIdentity.documentation_inputs;
    package_root=$packageRoot; remote_root=$remoteRoot; zip=$zipPath; guest_suite=$Suite;
    zip_sha256=(Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash;
    manifest_sha256=(Get-FileHash -LiteralPath (Join-Path $inputDir 'manifest.json') -Algorithm SHA256).Hash;
    wsb_sha256=(Get-FileHash -LiteralPath (Join-Path $packageRoot 'PaneBindV1.wsb') -Algorithm SHA256).Hash;
    deployed=$false; guest_started=$false; medium_result='NOT_RUN'; feature_result='NOT_RUN' }
Write-NewText (Join-Path $packageRoot 'package-receipt.json') ($receipt | ConvertTo-Json -Depth 6)
$receipt | ConvertTo-Json -Depth 6
