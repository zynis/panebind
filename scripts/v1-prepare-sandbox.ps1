[CmdletBinding()]
param(
    [ValidatePattern('^[0-9a-f]{32}$')][string]$RunId = ([Guid]::NewGuid().ToString('N')),
    [string]$BuildDirectory,
    # Retained for callers of the first draft. Fixture identity is now always
    # verified because the committed fixture has an identity-only entry.
    [switch]$FixtureBuildIdentity
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $repo 'out\v1-dev' }
$build = [IO.Path]::GetFullPath($BuildDirectory)
if (-not $build.StartsWith($repo + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'BuildDirectory must be an existing build under this repository.'
}
$branch = (& git -C $repo branch --show-current).Trim()
if ($LASTEXITCODE -ne 0 -or $branch -cne 'codex/r1c4b-live-magnet') { throw 'Unexpected development branch.' }
$head = (& git -C $repo rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -cnotmatch '^[0-9a-f]{40}$') { throw 'Cannot identify committed build source.' }
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
        if ($value.source_commit -cne $script:head) { throw 'Binary source_commit does not equal current committed HEAD; rebuild first.' }
        if ((Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash -cne $beforeHash) {
            throw 'Binary changed while its build identity was read; wait for builds to finish.'
        }
        $value | Add-Member -NotePropertyName verified_binary_sha256 -NotePropertyValue $beforeHash
        return $value
    } finally { $process.Dispose() }
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
$files = @($payloadFiles | ForEach-Object {
    $file = Get-Item -LiteralPath (Join-Path $inputDir $_)
    [ordered]@{ relative_path=$_; size=$file.Length; sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
})
$manifest = [ordered]@{
    schema='panebind-v1-sandbox/1'; run_id=$RunId; source_commit=$head; branch=$branch;
    created_utc=[DateTime]::UtcNow.ToString('o'); files=$files;
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
  <LogonCommand><Command>powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\PaneBindV1\Input\v1-sandbox-guest.ps1 -RunId $RunId</Command></LogonCommand>
</Configuration>
"@
Write-NewText (Join-Path $packageRoot 'PaneBindV1.wsb') $wsb
$zipPath = Join-Path $packageRoot ($RunId + '.zip')
Compress-Archive -LiteralPath @($inputDir,$outputDir,(Join-Path $packageRoot 'PaneBindV1.wsb')) -DestinationPath $zipPath -CompressionLevel Optimal
$receipt = [ordered]@{ schema='panebind-v1-package-receipt/1'; run_id=$RunId; source_commit=$head;
    package_root=$packageRoot; remote_root=$remoteRoot; zip=$zipPath;
    zip_sha256=(Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash;
    manifest_sha256=(Get-FileHash -LiteralPath (Join-Path $inputDir 'manifest.json') -Algorithm SHA256).Hash;
    wsb_sha256=(Get-FileHash -LiteralPath (Join-Path $packageRoot 'PaneBindV1.wsb') -Algorithm SHA256).Hash;
    deployed=$false; guest_started=$false; medium_result='NOT_RUN'; feature_result='NOT_RUN' }
Write-NewText (Join-Path $packageRoot 'package-receipt.json') ($receipt | ConvertTo-Json -Depth 6)
$receipt | ConvertTo-Json -Depth 6
