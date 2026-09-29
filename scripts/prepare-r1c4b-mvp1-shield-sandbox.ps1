[CmdletBinding()]
param()

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
& $cmake --build $build --config Release --target panebind-owned-shield-validation panebind-explorer-mvp1 panebind-test-input-environment --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Guest Release build failed.' }
& $cmake --build $build --config Debug --target panebind-owned-shield-validation --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Guest Debug owned build failed.' }

$bin = Join-Path $build 'src\platform\windows\Release'
$names = @(
    'panebind-owned-shield-validation.exe',
    'panebind-explorer-mvp1.exe',
    'panebind-test-input-environment.exe'
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
$debugIdentityText = (& $debugOwned --build-identity | Out-String)
$debugIdentityExit = $LASTEXITCODE
if ($debugIdentityExit -ne 0) { throw 'Read-only Debug owned build identity failed.' }
try { $debugIdentity = $debugIdentityText | ConvertFrom-Json -ErrorAction Stop }
catch { throw 'Debug owned build identity is not JSON.' }
if ($debugIdentity.implementation_sha -cne $head) {
    throw 'Debug owned build identity differs from the committed source.'
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
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'r1c4b-mvp1-shield-sandbox-guest.ps1') -Destination (Join-Path $inputDir 'guest-run.ps1') -ErrorAction Stop

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
    guest_script_sha256 = (Get-FileHash -LiteralPath (Join-Path $inputDir 'guest-run.ps1') -Algorithm SHA256).Hash
    input_marker_sha256 = (Get-FileHash -LiteralPath (Join-Path $inputDir 'run-id.txt') -Algorithm SHA256).Hash
    output_marker_sha256 = (Get-FileHash -LiteralPath (Join-Path $outputDir 'run-id.txt') -Algorithm SHA256).Hash
    imports = $imports
    debug_owned_imports = $debugImports
    owned_guest_run = 'NOT_RUN'
    explorer_guest_run = 'NOT_RUN'
}
$utf8 = New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $inputDir 'manifest.json'), ($manifest | ConvertTo-Json -Depth 6), $utf8)

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
    <Command>powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\PaneBindMVP1\Input\guest-run.ps1</Command>
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
Write-GuestConfig $desktopConfig (Join-Path $desktopRun 'input') (Join-Path $desktopRun 'output')

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
    GuestOwnedRun = 'NOT_RUN'
    GuestExplorerRun = 'NOT_RUN'
} | Format-List
