[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$branch = (& git -C $repo branch --show-current).Trim()
if ($LASTEXITCODE -ne 0 -or $branch -ne 'codex/r1c4b-live-magnet') {
    throw 'Expected codex/r1c4b-live-magnet; no package was prepared.'
}
$dirty = @(& git -C $repo status --porcelain)
if ($LASTEXITCODE -ne 0 -or $dirty.Count -ne 0) {
    throw 'Worktree must be clean so the guest binary can be attributed to one commit.'
}
$head = (& git -C $repo rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -notmatch '^[0-9a-f]{40}$') {
    throw 'Could not identify the source commit.'
}

# The two existing /MD executables depend on VC++ DLLs that a fresh guest may
# not contain. This one-purpose Release build uses the static MSVC runtime.
$cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1
if (-not $cmake) {
    $cmake = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
if (-not (Test-Path -LiteralPath $cmake -PathType Leaf)) { throw 'cmake.exe is unavailable.' }
$build = Join-Path $repo 'out\r1c4b-mvp1-guest-release'
& $cmake -S $repo -B $build -G 'Visual Studio 18 2026' -A x64 -DBUILD_TESTING=ON -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
if ($LASTEXITCODE -ne 0) { throw 'Guest Release configure failed.' }
& $cmake --build $build --config Release --target panebind-owned-overlay-probe panebind-test-input-environment --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Guest Release build failed.' }

$bin = Join-Path $build 'src\platform\windows\Release'
$probe = Join-Path $bin 'panebind-owned-overlay-probe.exe'
$preflight = Join-Path $bin 'panebind-test-input-environment.exe'
foreach ($path in @($probe, $preflight)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing built executable: $path" }
}

$vsTools = 'D:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC'
$dumpbin = Get-ChildItem -LiteralPath $vsTools -Directory |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName 'bin\Hostx64\x64\dumpbin.exe' } |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1
if (-not $dumpbin) { throw 'dumpbin.exe is required to verify static guest runtime imports.' }
$imports = @{}
foreach ($path in @($probe, $preflight)) {
    $output = (& $dumpbin /dependents $path | Out-String)
    if ($LASTEXITCODE -ne 0) { throw "dumpbin failed for $path" }
    if ($output -match '(?im)^\s*(?:MSVCP|VCRUNTIME|ucrtbased)[^\r\n]*\.dll\s*$') {
        throw "Guest executable still imports a dynamic VC++ runtime: $path"
    }
    $imports[[IO.Path]::GetFileName($path)] = @(
        [regex]::Matches($output, '(?im)^\s*([A-Za-z0-9_-]+\.dll)\s*$') |
            ForEach-Object { $_.Groups[1].Value }
    )
}

$runId = [Guid]::NewGuid().ToString('N').ToLowerInvariant()
$run = Join-Path $repo (Join-Path 'uat\r1c4b-mvp1-sandbox' $runId)
if (Test-Path -LiteralPath $run) { throw 'Run directory already exists; refusing to overwrite it.' }
$inputDir = Join-Path $run 'input'
$outputDir = Join-Path $run 'output'
New-Item -ItemType Directory -Path $inputDir, $outputDir -ErrorAction Stop | Out-Null

$runBytes = [Text.Encoding]::ASCII.GetBytes($runId)
if ($runBytes.Length -ne 32) { throw 'Run ID encoding is invalid.' }
[IO.File]::WriteAllBytes((Join-Path $inputDir 'run-id.txt'), $runBytes)
[IO.File]::WriteAllBytes((Join-Path $outputDir 'run-id.txt'), $runBytes)
Copy-Item -LiteralPath $probe -Destination (Join-Path $inputDir 'panebind-owned-overlay-probe.exe') -ErrorAction Stop
Copy-Item -LiteralPath $preflight -Destination (Join-Path $inputDir 'panebind-test-input-environment.exe') -ErrorAction Stop
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'r1c4b-mvp1-sandbox-guest.ps1') -Destination (Join-Path $inputDir 'guest-run.ps1') -ErrorAction Stop

$manifest = [ordered]@{
    schema = 'r1c4b-mvp1-sandbox-package/v1'
    run_id = $runId
    source_commit = $head
    configuration = 'Release-MT-x64'
    probe_sha256 = (Get-FileHash -LiteralPath (Join-Path $inputDir 'panebind-owned-overlay-probe.exe') -Algorithm SHA256).Hash
    preflight_sha256 = (Get-FileHash -LiteralPath (Join-Path $inputDir 'panebind-test-input-environment.exe') -Algorithm SHA256).Hash
    guest_script_sha256 = (Get-FileHash -LiteralPath (Join-Path $inputDir 'guest-run.ps1') -Algorithm SHA256).Hash
    imports = $imports
    guest_debug_run = 'NOT_RUN'
}
$utf8 = New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $inputDir 'manifest.json'), ($manifest | ConvertTo-Json -Depth 5), $utf8)

# Windows Sandbox maps only these two per-run folders. The host source tree,
# user profile, and build directory are not exposed to the guest.
$inputXml = [Security.SecurityElement]::Escape($inputDir)
$outputXml = [Security.SecurityElement]::Escape($outputDir)
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
$configPath = Join-Path $run 'owned-mvp1.wsb'
[IO.File]::WriteAllText($configPath, $wsb, $utf8)
[xml]$null = Get-Content -LiteralPath $configPath -Raw

[pscustomobject]@{
    RunId = $runId
    SourceCommit = $head
    Package = $run
    Config = $configPath
    InputReadOnly = $inputDir
    OutputWritable = $outputDir
    ProbeSha256 = $manifest.probe_sha256
    GuestDebugRun = 'NOT_RUN'
    GuestStarted = $false
} | Format-List
