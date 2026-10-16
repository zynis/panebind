[CmdletBinding()]
param([ValidatePattern('^[0-9a-f]{32}$')][string]$RunId=([Guid]::NewGuid().ToString('N')))
$ErrorActionPreference='Stop'
# One fixed, reviewed bootstrap asset; no application/URL/installer selection,
# host installation, remote transport, product launch or input driver exists.
# Official release: https://github.com/microsoft/winget-cli/releases/tag/v1.29.380
# Guest deployment/download contract: https://learn.microsoft.com/windows/package-manager/winget/
# Mappings/network: https://learn.microsoft.com/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-configure-using-wsb-file
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$assetId='b91fac4c0b9c4d63afdc05bc04ba9813'
$asset=Join-Path $repo ('uat\v1-assets\'+$assetId+'\inspected-x64')
$guestScript=Join-Path $PSScriptRoot 'v1-notepad-prepare-guest.ps1'
$branch=(& git -C $repo branch --show-current).Trim()
if($LASTEXITCODE -ne 0 -or $branch -cne 'codex/r1c4b-live-magnet'){throw 'Unexpected development branch.'}
$source=(& git -C $repo rev-parse HEAD).Trim()
if($LASTEXITCODE -ne 0 -or $source -cnotmatch '^[0-9a-f]{40}$'){throw 'Actual full packaging source HEAD required.'}
$scope=@('scripts/v1-notepad-prepare-guest.ps1','scripts/v1-prepare-notepad-sandbox.ps1','AGENTS.md')
$dirty=@(& git -C $repo diff --name-only HEAD -- @scope)
if($LASTEXITCODE -ne 0){throw 'Cannot inspect preparation source status.'}
$untracked=@(& git -C $repo ls-files --others --exclude-standard -- @scope)
if($LASTEXITCODE -ne 0 -or $dirty.Count -or $untracked.Count){throw 'Commit the exact preparation scripts/governance before packaging; unrelated product and plan input are preserved.'}
foreach($relative in $scope){& git -C $repo ls-files --error-unmatch -- $relative|Out-Null;if($LASTEXITCODE -ne 0){throw 'Preparation source must be tracked in actual HEAD.'}}
function Assert-OrdinaryRepoPath([string]$Path,[bool]$Directory){
    $full=[IO.Path]::GetFullPath($Path)
    if(-not $full.StartsWith($script:repo+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Preparation paths must stay inside this repository.'}
    $cursor=$full
    while($cursor -and $cursor.StartsWith($script:repo,[StringComparison]::OrdinalIgnoreCase)){
        if((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Preparation path/ancestor is a reparse point.'}
        $cursor=[IO.Path]::GetDirectoryName($cursor)
    }
    if(Test-Path -LiteralPath $full){if((Get-Item -LiteralPath $full).PSIsContainer -ne $Directory){throw 'Unexpected preparation file/directory type.'}}
}
function Write-NewText([string]$Path,[string]$Text){
    $bytes=(New-Object Text.UTF8Encoding($false)).GetBytes($Text)
    $stream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try{$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
}
$expected=[ordered]@{
    'AppInstaller_x64.msix'=@(62556620,'2E05D50B493DB869D98643533A0141C2C75FC15F122BF5723AB4A1B93C48D267')
    'Microsoft.VCLibs.140.00_14.0.33519.0_x64.appx'=@(896581,'9C17B521F9D690A1F504DA5108ED6EEC5669EB3A8FD1331EEF43E40D84E74283')
    'Microsoft.VCLibs.140.00.UWPDesktop_14.0.33728.0_x64.appx'=@(6757465,'077A3D1A5D0622BD3004DCA85F5E192D6E98EC79B83D4AA06766759EA6C09C3D')
    'Microsoft.WindowsAppRuntime.1.8_8000.616.304.0_x64.appx'=@(25431545,'A31595CC4B5AEBC18466EC24E8D4B566FE0FCAFB52D833B6D139B8691D0E5177')
}
Assert-OrdinaryRepoPath $asset $true;Assert-OrdinaryRepoPath $guestScript $false
foreach($name in $expected.Keys){
    $file=Join-Path $asset $name;Assert-OrdinaryRepoPath $file $false;$item=Get-Item -LiteralPath $file
    if($item.Length -ne $expected[$name][0] -or (Get-FileHash -LiteralPath $file).Hash -cne $expected[$name][1]){throw 'Fixed official preparation package size/hash mismatch.'}
    $signature=Get-AuthenticodeSignature -LiteralPath $file
    if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -cne 'CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US'){throw 'Fixed preparation package Microsoft signature is not valid.'}
}
$scriptHash=(Get-FileHash -LiteralPath $guestScript).Hash
$root=Join-Path $repo ('uat\v1-sandbox\'+$RunId)
Assert-OrdinaryRepoPath $root $true
if(Test-Path -LiteralPath $root){throw 'RunId already exists; preserve the original and use a new RunId.'}
$input=Join-Path $root 'input';$output=Join-Path $root 'output';$remote='I:\PaneBindMVP1Runs\'+$RunId
[IO.Directory]::CreateDirectory((Split-Path -Parent $root))|Out-Null
New-Item -ItemType Directory -Path $root -ErrorAction Stop|Out-Null
New-Item -ItemType Directory -Path $input -ErrorAction Stop|Out-Null
New-Item -ItemType Directory -Path $output -ErrorAction Stop|Out-Null
foreach($name in $expected.Keys){
    $from=Join-Path $asset $name;$to=Join-Path $input $name;[IO.File]::Copy($from,$to,$false)
    if((Get-FileHash -LiteralPath $to).Hash -cne $expected[$name][1] -or (Get-FileHash -LiteralPath $from).Hash -cne $expected[$name][1]){throw 'Fixed package changed during copy; incomplete new RunId retained.'}
}
[IO.File]::Copy($guestScript,(Join-Path $input 'v1-notepad-prepare-guest.ps1'),$false)
if((Get-FileHash -LiteralPath $guestScript).Hash -cne $scriptHash -or (Get-FileHash -LiteralPath (Join-Path $input 'v1-notepad-prepare-guest.ps1')).Hash -cne $scriptHash){throw 'Preparation script changed during copy.'}
Write-NewText (Join-Path $input 'panebind-v1-input.marker') $RunId
Write-NewText (Join-Path $output 'panebind-v1-output.marker') $RunId
$names=@($expected.Keys)+@('v1-notepad-prepare-guest.ps1','panebind-v1-input.marker')
$files=@(foreach($name in $names){$file=Get-Item -LiteralPath (Join-Path $input $name);[ordered]@{relative_path=$name;size=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash}})
$manifest=[ordered]@{schema='panebind-v1-notepad-preparation/1';run_id=$RunId;asset_id=$assetId;source_commit=$source;branch=$branch;
    purpose='official_Notepad_download_only_no_product_or_input';networking='Enable';host_root=$remote;
    guest_input='C:\PaneBindV1\Input';guest_output='C:\PaneBindV1\Output';files=$files;created_utc=[DateTime]::UtcNow.ToString('o');
    bootstrap_source='Microsoft/winget-cli v1.29.380 official release';bootstrap_source_bundle_sha256='65DEA9C01CE08EE7B763366B27C0E651F97DB857C11CA9B9C301826C10092F2E';
    bootstrap_dependencies_zip_sha256='BA875AFE9D190F61218985AC0292A99D1DB710BF93E13C68944CA9D89F0D82D1';
    official_notepad_product_id='9MSMLRH6LZF3';product_started=$false;synthetic_input_sent=$false;host_installation='NONE';
    preparation_result='NOT_RUN';application_function_result='NOT_RUN';formal_test_guest_networking='Disable'}
$manifest['preparation_location']=[ordered]@{geo_id=244;source='read_only_test_host_configuration';physical_location_asserted=$false;
    allowed_initial_geo_id=39070;restore_original=$true}
Write-NewText (Join-Path $input 'manifest.json') ($manifest|ConvertTo-Json -Depth 10)
$xmlInput=[Security.SecurityElement]::Escape($remote+'\input');$xmlOutput=[Security.SecurityElement]::Escape($remote+'\output')
$wsb=@"
<Configuration>
  <vGPU>Disable</vGPU>
  <MemoryInMB>4096</MemoryInMB>
  <Networking>Enable</Networking>
  <ClipboardRedirection>Disable</ClipboardRedirection>
  <AudioInput>Disable</AudioInput>
  <VideoInput>Disable</VideoInput>
  <PrinterRedirection>Disable</PrinterRedirection>
  <MappedFolders>
    <MappedFolder><HostFolder>$xmlInput</HostFolder><SandboxFolder>C:\PaneBindV1\Input</SandboxFolder><ReadOnly>true</ReadOnly></MappedFolder>
    <MappedFolder><HostFolder>$xmlOutput</HostFolder><SandboxFolder>C:\PaneBindV1\Output</SandboxFolder><ReadOnly>false</ReadOnly></MappedFolder>
  </MappedFolders>
  <LogonCommand><Command>powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\PaneBindV1\Input\v1-notepad-prepare-guest.ps1 -RunId $RunId</Command></LogonCommand>
</Configuration>
"@
Write-NewText (Join-Path $root 'PaneBindV1.wsb') $wsb
$zip=Join-Path $root ($RunId+'.zip')
Compress-Archive -LiteralPath @($input,$output,(Join-Path $root 'PaneBindV1.wsb')) -DestinationPath $zip -CompressionLevel Optimal
$archiveFiles=@(@($names|ForEach-Object{'input/'+$_})+@('input/manifest.json','output/panebind-v1-output.marker','PaneBindV1.wsb')|ForEach-Object{
    $file=Get-Item -LiteralPath (Join-Path $root $_.Replace('/','\'));[ordered]@{relative_path=$_;size=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash}})
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead($zip)
try{
    $entries=@($archive.Entries|Where-Object{$_.Name});$seen=@{}
    if($entries.Count -ne $archiveFiles.Count){throw 'Preparation ZIP file inventory mismatch.'}
    foreach($entry in $entries){
        $name=$entry.FullName.Replace('\','/');$expectedFile=@($archiveFiles|Where-Object relative_path -ceq $name)
        if($seen.ContainsKey($name) -or $expectedFile.Count -ne 1 -or $entry.Length -ne $expectedFile[0].size){throw 'Preparation ZIP path/size mismatch.'};$seen[$name]=$true
        $stream=$entry.Open();$digest=[Security.Cryptography.SHA256]::Create()
        try{$hash=([BitConverter]::ToString($digest.ComputeHash($stream))).Replace('-','')}finally{$digest.Dispose();$stream.Dispose()}
        if($hash -cne $expectedFile[0].sha256){throw 'Preparation ZIP content hash mismatch.'}
    }
}finally{$archive.Dispose()}
if((& git -C $repo rev-parse HEAD).Trim() -cne $source -or $LASTEXITCODE -ne 0){throw 'Actual packaging HEAD changed; incomplete new RunId retained for diagnosis.'}
$receipt=[ordered]@{schema='panebind-v1-notepad-package-receipt/1';run_id=$RunId;asset_id=$assetId;source_commit=$source;branch=$branch;
    package_root=$root;remote_root=$remote;zip=$zip;zip_sha256=(Get-FileHash -LiteralPath $zip).Hash;
    manifest_sha256=(Get-FileHash -LiteralPath (Join-Path $input 'manifest.json')).Hash;wsb_sha256=(Get-FileHash -LiteralPath (Join-Path $root 'PaneBindV1.wsb')).Hash;
    guest_script_sha256=$scriptHash;files=$archiveFiles;networking='Enable';deployed=$false;guest_started=$false;
    preparation_result='NOT_RUN';application_function_result='NOT_RUN';product_started=$false;synthetic_input_sent=$false;host_installation='NONE'}
$receipt['preparation_location']=$manifest.preparation_location
Write-NewText (Join-Path $root 'package-receipt.json') ($receipt|ConvertTo-Json -Depth 10)
$receipt|ConvertTo-Json -Depth 10
