[CmdletBinding()]
param(
    [ValidateSet('AutomatedValidation', 'BindOnlyIndependentReview')]
    [string]$GuestMode = 'AutomatedValidation'
)

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$branch = (& git -C $repo branch --show-current).Trim()
if ($LASTEXITCODE -ne 0 -or $branch -cne 'codex/r1c4b-live-magnet') {
    throw 'Expected codex/r1c4b-live-magnet; no package was prepared.'
}
$dirty = @(& git -C $repo status --porcelain)
if ($LASTEXITCODE -ne 0 -or $dirty.Count -ne 0) {
    throw 'Worktree must be clean so every guest binary has one committed source identity.'
}
$head = (& git -C $repo rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -cnotmatch '^[0-9a-f]{40}$') {
    throw 'Could not identify the implementation commit.'
}

# Reuse only the already-reviewed nine owned runs when the ACTUAL shared
# implementation and build inputs are unchanged. This is a prior prerequisite,
# never a claim that the new package executed owned tests. Raw files remain in
# their original batch; the manifest binds their reviewed hashes without adding
# another writable mapping or expanding the deployment archive whitelist.
function Get-OwnedPrerequisite {
    $priorRunId = '050736d561fe4b3c941e9f194b3f51aa'
    $priorSource = '3ef009f5e72c5eebdbb64b3c3e0cd6c52c1b2d52'
    $scope = @('src/core', 'src/platform/windows/operations', 'CMakeLists.txt',
        'cmake', 'src/platform/windows/CMakeLists.txt',
        'src/platform/windows/observer.rc', 'src/platform/windows/observer.manifest')
    $changed = @(& git -C $repo diff --name-only $priorSource $head -- $scope)
    if ($LASTEXITCODE -ne 0) { throw 'Could not compare the reviewed owned implementation.' }
    if ($changed.Count -ne 0) {
        return [ordered]@{ result = 'RUN_CURRENT_PACKAGE'; reason = 'shared_source_changed';
            changed_paths = $changed }
    }
    $priorOutput = Join-Path $repo ('uat\r1c4b-mvp1-shield-sandbox\' + $priorRunId + '\output')
    $expected = [ordered]@{
        'debug-normal-repeat' = '6A85F9ED355D0CBB599134AADEA84BC056293F8826D5A5F1E7A155B40F8B5650'
        'debug-association-stop-before-call' = '4E9A623346C99A24684B070B902BA52172C1D56F8EA56AFD404B071755137476'
        'debug-capture-stop' = '3F41AF3D5962DC70AB7529D893F3DB250784F9BAB0BE98A412E8B1E8640152DC'
        'debug-writer-stall' = '19C415AC099E60D0F59FFE5A129ADFB23D5BE3226538A02517CEE59561C1C0D4'
        'debug-source-pause' = 'FEC238A0ADFEA4564FD61ED653CF8F835318E751EC22B0A640BAE9850887C2F4'
        'debug-capture-early-up' = 'DDE2E54C1C57452CE68BE38DBDFA2130D1B3509279712E739385E0B3AF5D3307'
        'debug-capture-fail' = '1BD287D36B452BC52DB845F6C9AD37A0A357451A9325B619435EF5EE0230A008'
        'debug-capture-lost' = '07225797DDC160669596EB2A8B4E12EBC592F451E4D23DA91E72A7698BC48306'
        'release-normal' = 'E21CA00DA769F578B07B342DE087A38FBEC437DEDB0705DBA6624D7B48C80948'
    }
    $summaryPath = Join-Path $priorOutput 'guest-owned-summary.json'
    $summaryHash = '23201480D2A815C58EF7105EE34A366385F759C026B02C6721F013A8AD7C5287'
    if ((Get-FileHash -LiteralPath $summaryPath -Algorithm SHA256).Hash -cne $summaryHash) {
        throw 'Reviewed owned summary changed; refusing automatic prerequisite reuse.'
    }
    $priorSummary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json -ErrorAction Stop
    if ($priorSummary.run_id -cne $priorRunId -or $priorSummary.source_commit -cne $priorSource -or
        @($priorSummary.owned_scenarios).Count -ne 9 -or
        @($priorSummary.owned_scenarios | Where-Object { $_.exit_code -ne 0 -or
            $_.nonempty_evidence -ne $true }).Count -ne 0) {
        throw 'Reviewed owned summary does not contain the exact nine successful runs.'
    }
    $records = @()
    foreach ($case in $expected.Keys) {
        $file = "$priorRunId-$case.jsonl"
        $path = Join-Path $priorOutput $file
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $expected[$case]) {
            throw "Reviewed owned evidence changed: $file"
        }
        $split = $case.Split('-', 2)
        $rows = @([IO.File]::ReadAllLines($path) | ForEach-Object {
            if ([string]::IsNullOrWhiteSpace($_)) { throw "Empty owned evidence line: $file" }
            $_ | ConvertFrom-Json -ErrorAction Stop
        })
        $sequence = 0
        foreach ($row in $rows) {
            $sequence++
            if ($row.schema -cne 'r1c4b-owned-product-shield/v1' -or $row.sequence -ne $sequence) {
                throw "Reviewed owned JSONL identity/sequence differs: $file"
            }
        }
        $startup = @($rows | Where-Object { $_.type -ceq 'startup' })
        $shutdown = @($rows | Where-Object { $_.type -ceq 'shutdown' })
        if ($startup.Count -ne 1 -or $startup[0].implementation_sha -cne $priorSource -or
            $startup[0].scenario -cne $split[1] -or $shutdown.Count -ne 1 -or
            $shutdown[0].accepted -ne $true -or $shutdown[0].shield_clean -ne $true -or
            $shutdown[0].association_cleanup_complete -ne $true -or
            $shutdown[0].capture_cleanup_complete -ne $true -or
            $shutdown[0].receiver_destroyed -ne $true -or $shutdown[0].writer_gone -ne $true) {
            throw "Reviewed owned prerequisite incomplete: $file"
        }
        $records += [ordered]@{ configuration = $split[0]; scenario = $split[1];
            file = $file; sha256 = $expected[$case]; rows = $rows.Count }
    }
    return [ordered]@{ result = 'REUSED_PRIOR_EVIDENCE'; run_id = $priorRunId;
        source_commit = $priorSource; summary_sha256 = $summaryHash;
        shared_source_unchanged = $true; compared_source_paths = $scope;
        current_package_owned_execution = 'NOT_RUN'; raw_evidence = $records }
}
$ownedPrerequisite = Get-OwnedPrerequisite

# This is a new per-run package. The historical owned-overlay-probe package,
# scripts and evidence remain untouched. Debug/Release /MT avoid a VC++ guest prerequisite.
$cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty Source -First 1
if (-not $cmake) {
    $cmake = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
if (-not (Test-Path -LiteralPath $cmake -PathType Leaf)) { throw 'cmake.exe is unavailable.' }
$build = Join-Path $repo 'out\r1c4b-mvp1-shield-guest-mt'
& $cmake -S $repo -B $build -G 'Visual Studio 18 2026' -A x64 -DBUILD_TESTING=ON -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
if ($LASTEXITCODE -ne 0) { throw 'Guest /MT configure failed.' }
& $cmake --build $build --config Release --target panebind-owned-shield-validation panebind-explorer-mvp1 panebind-test-input-environment panebind-shield-topmost-contrast --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Guest Release build failed.' }
& $cmake --build $build --config Debug --target panebind-owned-shield-validation panebind-explorer-mvp1 --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Guest Debug owned build failed.' }

$bin = Join-Path $build 'src\platform\windows\Release'
$names = @(
    'panebind-owned-shield-validation.exe',
    'panebind-explorer-mvp1.exe',
    'panebind-test-input-environment.exe',
    'panebind-shield-topmost-contrast.exe'
)
$binaries = @{}
foreach ($name in $names) {
    $path = Join-Path $bin $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing built executable: $path" }
    $binaries[$name] = $path
}
$debugOwned = Join-Path $build 'src\platform\windows\Debug\panebind-owned-shield-validation.exe'
if (-not (Test-Path -LiteralPath $debugOwned -PathType Leaf)) {
    throw "Missing Debug owned executable: $debugOwned"
}
$debugExplorer = Join-Path $build 'src\platform\windows\Debug\panebind-explorer-mvp1.exe'
if (-not (Test-Path -LiteralPath $debugExplorer -PathType Leaf)) {
    throw "Missing Debug Explorer executable: $debugExplorer"
}
foreach ($name in @('panebind-owned-shield-validation.exe', 'panebind-explorer-mvp1.exe')) {
    $identityText = (& $binaries[$name] --build-identity | Out-String)
    $identityExit = $LASTEXITCODE
    if ($identityExit -ne 0) { throw "Read-only build identity failed for $name" }
    try { $identity = $identityText | ConvertFrom-Json -ErrorAction Stop }
    catch { throw "Build identity is not JSON for $name" }
    if ($identity.implementation_sha -cne $head) {
        throw "Build identity differs from the committed source for $name"
    }
}
$contrastIdentityText = (& $binaries['panebind-shield-topmost-contrast.exe'] --build-identity | Out-String)
if ($LASTEXITCODE -ne 0) { throw 'Read-only contrast build identity failed.' }
try { $contrastIdentity = $contrastIdentityText | ConvertFrom-Json -ErrorAction Stop }
catch { throw 'Contrast build identity is not JSON.' }
if ($contrastIdentity.implementation_sha -cne $head) {
    throw 'Contrast build identity differs from the committed source.'
}
$debugIdentityText = (& $debugOwned --build-identity | Out-String)
$debugIdentityExit = $LASTEXITCODE
if ($debugIdentityExit -ne 0) { throw 'Read-only Debug owned build identity failed.' }
try { $debugIdentity = $debugIdentityText | ConvertFrom-Json -ErrorAction Stop }
catch { throw 'Debug owned build identity is not JSON.' }
if ($debugIdentity.implementation_sha -cne $head) {
    throw 'Debug owned build identity differs from the committed source.'
}
$debugExplorerIdentityText = (& $debugExplorer --build-identity | Out-String)
if ($LASTEXITCODE -ne 0) { throw 'Read-only Debug Explorer build identity failed.' }
try { $debugExplorerIdentity = $debugExplorerIdentityText | ConvertFrom-Json -ErrorAction Stop }
catch { throw 'Debug Explorer build identity is not JSON.' }
if ($debugExplorerIdentity.implementation_sha -cne $head) {
    throw 'Debug Explorer build identity differs from the committed source.'
}

$vsTools = 'D:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC'
$dumpbin = Get-ChildItem -LiteralPath $vsTools -Directory |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName 'bin\Hostx64\x64\dumpbin.exe' } |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1
if (-not $dumpbin) { throw 'dumpbin.exe is required to verify static guest runtime imports.' }
$imports = @{}
foreach ($name in $names) {
    $output = (& $dumpbin /dependents $binaries[$name] | Out-String)
    if ($LASTEXITCODE -ne 0) { throw "dumpbin failed for $name" }
    if ($output -match '(?im)^\s*(?:MSVCP|VCRUNTIME|ucrtbase|ucrtbased)[^\r\n]*\.dll\s*$') {
        throw "Guest executable still imports a dynamic VC++ runtime: $name"
    }
    $imports[$name] = @(
        [regex]::Matches($output, '(?im)^\s*([A-Za-z0-9_-]+\.dll)\s*$') |
            ForEach-Object { $_.Groups[1].Value }
    )
}
$debugImportsText = (& $dumpbin /dependents $debugOwned | Out-String)
if ($LASTEXITCODE -ne 0) { throw 'dumpbin failed for Debug owned executable.' }
if ($debugImportsText -match '(?im)^\s*(?:MSVCP|VCRUNTIME|ucrtbase|ucrtbased)[^\r\n]*\.dll\s*$') {
    throw 'Debug owned executable still imports a dynamic VC++ runtime.'
}
$debugImports = @(
    [regex]::Matches($debugImportsText, '(?im)^\s*([A-Za-z0-9_-]+\.dll)\s*$') |
        ForEach-Object { $_.Groups[1].Value }
)
$debugExplorerImportsText = (& $dumpbin /dependents $debugExplorer | Out-String)
if ($LASTEXITCODE -ne 0) { throw 'dumpbin failed for Debug Explorer executable.' }
if ($debugExplorerImportsText -match '(?im)^\s*(?:MSVCP|VCRUNTIME|ucrtbase|ucrtbased)[^\r\n]*\.dll\s*$') {
    throw 'Debug Explorer executable still imports a dynamic VC++ runtime.'
}
$debugExplorerImports = @(
    [regex]::Matches($debugExplorerImportsText, '(?im)^\s*([A-Za-z0-9_-]+\.dll)\s*$') |
        ForEach-Object { $_.Groups[1].Value }
)

$runId = [Guid]::NewGuid().ToString('N').ToLowerInvariant()
$run = Join-Path $repo (Join-Path 'uat\r1c4b-mvp1-shield-sandbox' $runId)
if (Test-Path -LiteralPath $run) { throw 'New RunId directory already exists; refusing to overwrite it.' }
$inputDir = Join-Path $run 'input'
$outputDir = Join-Path $run 'output'
New-Item -ItemType Directory -Path $inputDir, $outputDir -ErrorAction Stop | Out-Null
$debugInputDir = Join-Path $inputDir 'Debug'
New-Item -ItemType Directory -Path $debugInputDir -ErrorAction Stop | Out-Null

$runBytes = [Text.Encoding]::ASCII.GetBytes($runId)
if ($runBytes.Length -ne 32) { throw 'Run ID encoding is invalid.' }
[IO.File]::WriteAllBytes((Join-Path $inputDir 'run-id.txt'), $runBytes)
[IO.File]::WriteAllBytes((Join-Path $outputDir 'run-id.txt'), $runBytes)
foreach ($name in $names) {
    Copy-Item -LiteralPath $binaries[$name] -Destination (Join-Path $inputDir $name) -ErrorAction Stop
}
Copy-Item -LiteralPath $debugOwned -Destination (Join-Path $debugInputDir 'panebind-owned-shield-validation.exe') -ErrorAction Stop
Copy-Item -LiteralPath $debugExplorer -Destination (Join-Path $debugInputDir 'panebind-explorer-mvp1.exe') -ErrorAction Stop
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'r1c4b-mvp1-shield-sandbox-guest.ps1') -Destination (Join-Path $inputDir 'guest-run.ps1') -ErrorAction Stop
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'r1c4b-mvp1-explorer-guest-driver.ps1') -Destination (Join-Path $inputDir 'explorer-driver.ps1') -ErrorAction Stop

$binaryHashes = [ordered]@{}
foreach ($name in $names) {
    $binaryHashes[$name] = (Get-FileHash -LiteralPath (Join-Path $inputDir $name) -Algorithm SHA256).Hash
}
$manifest = [ordered]@{
    schema = 'r1c4b-mvp1-shield-sandbox-package/v2'
    run_id = $runId
    source_commit = $head
    configuration = 'Release-and-Debug-MT-x64'
    binaries_sha256 = $binaryHashes
    debug_owned_sha256 = (Get-FileHash -LiteralPath (Join-Path $debugInputDir 'panebind-owned-shield-validation.exe') -Algorithm SHA256).Hash
    debug_explorer_sha256 = (Get-FileHash -LiteralPath (Join-Path $debugInputDir 'panebind-explorer-mvp1.exe') -Algorithm SHA256).Hash
    guest_script_sha256 = (Get-FileHash -LiteralPath (Join-Path $inputDir 'guest-run.ps1') -Algorithm SHA256).Hash
    explorer_driver_sha256 = (Get-FileHash -LiteralPath (Join-Path $inputDir 'explorer-driver.ps1') -Algorithm SHA256).Hash
    input_marker_sha256 = (Get-FileHash -LiteralPath (Join-Path $inputDir 'run-id.txt') -Algorithm SHA256).Hash
    output_marker_sha256 = (Get-FileHash -LiteralPath (Join-Path $outputDir 'run-id.txt') -Algorithm SHA256).Hash
    imports = $imports
    debug_owned_imports = $debugImports
    debug_explorer_imports = $debugExplorerImports
    explorer_test_exception = 'exact_sandbox_same_sid_session_high'
    explorer_guest_mode = $GuestMode
    independent_review_configuration = 'release'
    owned_prerequisite = $ownedPrerequisite
    owned_guest_run = 'NOT_RUN'
    explorer_guest_run = 'NOT_RUN'
}
$utf8 = New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $inputDir 'manifest.json'), ($manifest | ConvertTo-Json -Depth 8), $utf8)

function Write-GuestConfig([string]$configPath, [string]$hostInput, [string]$hostOutput) {
    $inputXml = [Security.SecurityElement]::Escape($hostInput)
    $outputXml = [Security.SecurityElement]::Escape($hostOutput)
    $wsb = @"
<Configuration>
  <vGPU>Disable</vGPU>
  <Networking>Disable</Networking>
  <AudioInput>Disable</AudioInput>
  <VideoInput>Disable</VideoInput>
  <PrinterRedirection>Disable</PrinterRedirection>
  <ClipboardRedirection>Disable</ClipboardRedirection>
  <MappedFolders>
    <MappedFolder>
      <HostFolder>$inputXml</HostFolder>
      <SandboxFolder>C:\PaneBindMVP1\Input</SandboxFolder>
      <ReadOnly>true</ReadOnly>
    </MappedFolder>
    <MappedFolder>
      <HostFolder>$outputXml</HostFolder>
      <SandboxFolder>C:\PaneBindMVP1\Output</SandboxFolder>
      <ReadOnly>false</ReadOnly>
    </MappedFolder>
  </MappedFolders>
  <LogonCommand>
    <Command>powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File C:\PaneBindMVP1\Input\guest-run.ps1</Command>
  </LogonCommand>
</Configuration>
"@
    [IO.File]::WriteAllText($configPath, $wsb, $utf8)
    [xml]$null = Get-Content -LiteralPath $configPath -Raw
}

$localConfig = Join-Path $run 'owned-shield-local.wsb'
$desktopConfig = Join-Path $run 'owned-shield-I.wsb'
Write-GuestConfig $localConfig $inputDir $outputDir
$desktopRun = 'I:\PaneBindMVP1Runs\' + $runId
# The desktop drive exists on ZS-Workstation, not necessarily on the build host.
# Construct its literal mapped paths without asking local path providers to resolve I:.
Write-GuestConfig $desktopConfig ($desktopRun + '\input') ($desktopRun + '\output')

# The archive is for a new, exact RunId directory on ZS-Workstation. It does
# not contact the host or start the guest. Do not use the local .wsb there.
$archive = Join-Path $run 'desktop-deploy.zip'
Compress-Archive -LiteralPath $inputDir, $outputDir, $desktopConfig -DestinationPath $archive -CompressionLevel Optimal -ErrorAction Stop

[pscustomobject]@{
    RunId = $runId
    SourceCommit = $head
    Package = $run
    DesktopRun = $desktopRun
    DesktopConfig = $desktopConfig
    DesktopConfigSha256 = (Get-FileHash -LiteralPath $desktopConfig -Algorithm SHA256).Hash
    Archive = $archive
    ArchiveSha256 = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    ManifestSha256 = (Get-FileHash -LiteralPath (Join-Path $inputDir 'manifest.json') -Algorithm SHA256).Hash
    BinarySha256 = $binaryHashes
    DebugOwnedSha256 = $manifest.debug_owned_sha256
    DebugExplorerSha256 = $manifest.debug_explorer_sha256
    GuestOwnedRun = 'NOT_RUN'
    OwnedPrerequisite = $ownedPrerequisite.result
    GuestExplorerRun = 'NOT_RUN'
    GuestMode = $GuestMode
    IndependentReview = 'NOT_RUN'
} | Format-List
