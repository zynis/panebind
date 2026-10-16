[CmdletBinding()]
param([Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{32}$')][string]$RunId,
      [ValidateSet('DebugAndRelease','Debug','Release')][string]$Configurations='DebugAndRelease',
      [ValidateSet('Full','IndependentEntry','SettingsDiagnostic','CoreAndApplications','ApplicationsOnly','EntryAndSettings','OwnedComponentsOnly')][string]$Suite='Full',
      [ValidateRange(30,1800)][int]$StageTimeoutSeconds=1800)
$ErrorActionPreference='Stop'
$inputDir='C:\PaneBindV1\Input'
$outputDir='C:\PaneBindV1\Output'
$utf8=New-Object Text.UTF8Encoding($false)
$summaryPath=$null
$summary=[ordered]@{schema='panebind-v1-guest/1';run_id=$RunId;status='NOT_READY';
    source_commit=$null;product_source_commit=$null;driver_source_commit=$null;packaging_head=$null;
    requested_configurations=$Configurations;requested_suite=$Suite;actual_suite=$null;
    automated_driver=$true;human_uat='NOT_RUN';stages=@();error=$null}
function New-Evidence([string]$Path,[string]$Text) {
    $stream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try{$bytes=$script:utf8.GetBytes($Text);$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
}
function Marker([string]$Path) {
    $value=[IO.File]::ReadAllText($Path,[Text.Encoding]::ASCII)
    if($value.EndsWith("`r`n")){$value=$value.Substring(0,$value.Length-2)}
    if($value -cnotmatch '^[0-9a-f]{32}$'){throw 'Invalid exact run marker.'}
    return $value
}
function Fixture-ExitReason([uint32]$Code) {
    # Safe meanings of the actual fixture exit status. These never infer a
    # platform cause, input delivery or product PASS from a numeric exit.
    $reasons=@{
        0=@('driver_exited_zero_evidence_required',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('cZqoUgCQ+lH2lgz/zU57mA1ZOGifUv2ADk4FbgZ0wYtuYw=='))))
        1=@('fixture_runtime_or_function_failed',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('ZgBpAHgAdAB1AHIAZQAgANCPTIgWYp9S/YDAaOVnMVkljQ=='))))
        64=@('fixture_command_line_unavailable',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('4GXVbPuL1lMgAGYAaQB4AHQAdQByAGUAIAB9VOROTIg='))))
        78=@('initial_guest_scope_not_authorized',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('HVLLWSAAZwB1AGUAcwB0ACAAJo03YgEwUgB1AG4ASQBkACAAFmLCU3BlA4P0Vg1OJns='))))
        80=@('input_RunId_marker_invalid',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('k49lUe52VV8gAFIAdQBuAEkAZAAgAAdosIsNTu9T+4sWYg1OOVNNkQ=='))))
        81=@('output_RunId_marker_invalid',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('k4/6Ue52VV8gAFIAdQBuAEkAZAAgAAdosIsNTu9T+4sWYg1OOVNNkQ=='))))
        82=@('output_lexical_path_not_exact',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('k4/6UcJTcGUNTi9mvnxueIhjQ2fvjYRf'))))
        83=@('product_lexical_path_not_exact',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('p07BVMJTcGUNTi9mLGcgAFIAdQBuAEkAZAAgAIR2vnxueCAARABlAGIAdQBnAC8AUgBlAGwAZQBhAHMAZQAgAO+NhF8='))))
        84=@('output_directory_not_plain',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('k4/6Ue52VV8NTu9T+4sBMA1OL2budlVfFmJ3UQlnzZHjiZBnXlwnYBv/KmeZUSAAZwB1AGEAcgBkACAA5WXXXw=='))))
        85=@('guard_evidence_creation_failed',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('4GXVbLBl+l4gAGcAdQBhAHIAZAAgAOVl118b//JdCWeHZfZODU4aT4aJ1nY='))))
        86=@('input_directory_not_plain',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('k49lUe52VV8NTu9T+4sBMA1OL2budlVfFmJ3UQlnzZHjiZBnXlwnYA=='))))
        87=@('own_token_not_verified_Medium_non_elevated',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('ZgBpAHgAdAB1AHIAZQAgAOqBq44gAHQAbwBrAGUAbgAgACpnOGieWzpOIABNAGUAZABpAHUAbQAgABROXpfQY0dT'))))
        88=@('own_interactive_session_unavailable',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('ZgBpAHgAdAB1AHIAZQAgAOqBq45el/aWpE6STiAAcwBlAHMAcwBpAG8AbgAgAA1O71M4aJ5b'))))
        89=@('own_Default_input_desktop_not_verified',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('ZgBpAHgAdAB1AHIAZQAgAOqBq44gAEQAZQBmAGEAdQBsAHQAIACTj2VRTGhilxZiIABVAE8ASQBfAEkATwAgACpnGpDHjw=='))))
        90=@('own_WTS_session_not_active',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('ZgBpAHgAdAB1AHIAZQAgAOqBq44gAFcAVABTACAAGk/diypnOGieWzpOIABBAGMAdABpAHYAZQA='))))
        91=@('output_canonical_path_not_exact',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('k4/6Ue52VV/EiQOD742EXw1O71P7ixZiDU5Je45OiGNDZ++NhF8='))))
        92=@('product_directory_not_plain',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('LGcgAFIAdQBuAEkAZAAgAKdOwVTudlVfDU7hbrONbmYakO52VV+BiUJs'))))
        93=@('Debug_input_directory_not_plain',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('RABlAGIAdQBnACAAk49lUVBb7nZVXw1O4W6zjW5mGpDudlVfgYlCbA=='))))
        94=@('actual_product_not_plain_file',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('8l0NWTZSp07BVA1OL2bvUzhonluEdm5mGpCHZfZO'))))
        95=@('input_product_not_plain_file',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('k49lUadOwVQNTi9m71M4aJ5bhHZuZhqQh2X2Tg=='))))
        96=@('product_canonical_path_not_exact',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('p07BVMSJA4PvjYRfDU7vU/uLFmINTkl7jk4sZyAAUgB1AG4ASQBkACAAvnxueO+NhF8='))))
        97=@('product_source_hash_mismatch_or_unreadable',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('p07BVA5Ok49lUTZSwVTIVAxeDU4ATvSBFmINTu9T+4vWUw=='))))
        98=@('portable_run_marker_missing',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('LGcgAFIAdQBuAEkAZAAgAKdOwVQ6fxFcv086ZBlQCZAHaLCL'))))
        99=@('preexisting_product_instance_present',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('8l0JZ6dOwVSeW4tPDP/SYt1+DVkodQ=='))))
        100=@('PMV2_context_not_available',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('4GXVbDhonlsgAFAATQB2ADIAIABEAFAASQAgAApOC06HZQ=='))))
        101=@('product_runtime_directory_is_not_fresh',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('LGcgAFIAdQBuAEkAZAAgAPJdCWenTsFU5WXXXxZivotufwz/0mLdfoaJ1nY='))))
        102=@('product_changed_after_guard',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('jFsQYiAAZwB1AGEAcgBkACAADlQ2UsFUyFQMXo1RIWvAaOVnMVkljQ=='))))
        103=@('guard_observation_or_evidence_exception',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('ZwB1AGEAcgBkACAAwonfWxZiwYtuY5lRZVECXzheG/8NTqhjrWV3UVNPr3ODWJ9T4FY='))))
        104=@('fixture_suite_manifest_mismatch',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('S23ViwmQeZgOTixnIABSAHUAbgBJAGQAIADqU/uLBW5VUw1OAE70gQ=='))))
        105=@('product_driver_split_identity_or_interface_rejected','Product and driver identities, interface or actual executable hashes disagree; no test input was sent.')
    }
    if($Code -le [int]::MaxValue -and $reasons.ContainsKey([int]$Code)){$reason=$reasons[[int]$Code]}
    else{$reason=@('unmapped_fixture_exit_status',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('KmcgZgRchHYgAGYAaQB4AHQAdQByAGUAIAAAkPpRtnIBYAz/AJfAaOVnn1PLWcGLbmM='))))}
    [ordered]@{code=$Code;reason_en=$reason[0];reason_zh=$reason[1];feature_pass_inferred=$false}
}
function Read-TestApplicationAvailability {
    # Guest-only, bounded known executable/package lookups. No directory
    # traversal, installation, launch, user documents or settings inspection.
    $known=@(
        @{kind='WindowsNotepad';path='C:\Windows\System32\notepad.exe'},
        @{kind='Edge';path='C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'},
        @{kind='Edge';path='C:\Program Files\Microsoft\Edge\Application\msedge.exe'},
        @{kind='VSCode';path='C:\Program Files\Microsoft VS Code\Code.exe'},
        @{kind='VSCode';path='C:\Users\WDAGUtilityAccount\AppData\Local\Programs\Microsoft VS Code\Code.exe'},
        @{kind='WindowsTerminalAlias';path='C:\Users\WDAGUtilityAccount\AppData\Local\Microsoft\WindowsApps\wt.exe'}
    )
    $paths=@(foreach($item in $known){
        $entry=[ordered]@{kind=$item.kind;path=$item.path;exists=$null;file_version=$null;query_error=$null}
        try{
            $entry.exists=Test-Path -LiteralPath $item.path -PathType Leaf -ErrorAction Stop
            if($entry.exists){$entry.file_version=[Diagnostics.FileVersionInfo]::GetVersionInfo($item.path).FileVersion}
        }catch{$entry.query_error=$_.FullyQualifiedErrorId}
        $entry
    })
    $packages=@(foreach($name in @('Microsoft.WindowsNotepad','Microsoft.WindowsTerminal','Microsoft.MicrosoftEdge.Stable','Microsoft.VisualStudioCode')){
        $entry=[ordered]@{requested_name=$name;found=$null;versions=@();query_error=$null}
        try{
            $found=@(Get-AppxPackage -Name $name -ErrorAction Stop)
            if($found.Count -gt 8){throw 'Exact known package result exceeds bounded inventory.'}
            $entry.found=$found.Count -gt 0
            $entry.versions=@($found|ForEach-Object{[ordered]@{name=$_.Name;version=$_.Version.ToString();architecture=$_.Architecture.ToString()}})
        }catch{$entry.query_error=$_.FullyQualifiedErrorId}
        $entry
    })
    [ordered]@{utc=[DateTime]::UtcNow.ToString('o');guest_only=$true;known_executables=$paths;exact_package_names=$packages;
        executable_or_package_presence_is_not_window_or_function_validation=$true;no_installation_or_process_start=$true}
}
function Prepare-PortableTestPackage($Spec,[string]$SourceCommit,[string]$ReleaseHash){
    $suffix=[Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('LQC/TzpkIAAtTodlIAB6ejxo'))
    $runRoot=Join-Path $outputDir ($RunId+$suffix)
    $payload=Join-Path $runRoot 'PaneBindV1'
    $ready=Join-Path $outputDir ($RunId+'-portable-ready.marker')
    if($Spec.kind -cne 'PaneBindReleasePortable' -or $Spec.relative_path -cne 'PaneBindV1-portable-test.zip' -or
       $Spec.package_id -cnotmatch '^[0-9a-f]{32}$' -or $Spec.source_commit -cne $SourceCommit -or
       $Spec.exe_sha256 -cne $ReleaseHash -or $Spec.zip_sha256 -cnotmatch '^[A-F0-9]{64}$' -or
       $Spec.manifest_sha256 -cnotmatch '^[A-F0-9]{64}$' -or [long]$Spec.zip_size -le 0 -or [long]$Spec.zip_size -gt 32MB -or
       [long]$Spec.expanded_bytes -le 0 -or [long]$Spec.expanded_bytes -gt 32MB){throw 'Unrecognized exact Release portable package contract.'}
    if((Test-Path -LiteralPath $runRoot) -or (Test-Path -LiteralPath $ready)){throw 'Portable guest target already exists; never reuse unknown state.'}
    $names=@('panebind.exe','portable.flag','LICENSE','manifest.json')+@('LU6HZetfH5BlUeiVLgBtAGQA','TZFuf/SLDmYuAG0AZAA=','8l3ld+6VmJguAG0AZAA=' | ForEach-Object {[Text.Encoding]::Unicode.GetString([Convert]::FromBase64String($_))})
    $files=@($Spec.files);$expected=@{};$expanded=[long]0
    if($files.Count -ne 7){throw 'Portable package must have exactly seven payload files.'}
    foreach($file in $files){
        if($file.relative_path -cnotin $names -or $expected.ContainsKey($file.relative_path) -or
           $file.sha256 -cnotmatch '^[A-F0-9]{64}$' -or [long]$file.size -le 0 -or [long]$file.size -gt 32MB){throw 'Unknown, duplicate or unbounded portable payload.'}
        $expected[$file.relative_path]=$file;$expanded+=[long]$file.size
    }
    if($expanded -ne [long]$Spec.expanded_bytes -or $expected['panebind.exe'].sha256 -cne $Spec.exe_sha256 -or
       $expected['manifest.json'].sha256 -cne $Spec.manifest_sha256){throw 'Portable expanded inventory/binary identity mismatch.'}
    $zip=Join-Path $inputDir $Spec.relative_path
    if((Get-Item -LiteralPath $zip).Length -ne [long]$Spec.zip_size -or
       (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -cne $Spec.zip_sha256){throw 'Portable input ZIP identity mismatch.'}
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive=[IO.Compression.ZipFile]::OpenRead($zip)
    try{
        if($archive.Entries.Count -gt 8 -or @($archive.Entries | Where-Object {$_.Name}).Count -ne 7){throw 'Portable ZIP has unknown or missing entries.'}
        $seen=@{}
        # Verify EVERY archived byte and exact flat path before creating the
        # new output directory; ExtractToDirectory is deliberately not used.
        foreach($entry in $archive.Entries){
            $normalized=$entry.FullName.Replace('\','/')
            if(-not $entry.Name){if($normalized -cne 'PaneBindV1/'){throw 'Unexpected portable ZIP directory.'};continue}
            if(-not $normalized.StartsWith('PaneBindV1/',[StringComparison]::Ordinal)){throw 'Portable ZIP root mismatch.'}
            $leaf=$normalized.Substring(11)
            if($leaf -cnotin $names -or $seen.ContainsKey($leaf) -or
               (($entry.ExternalAttributes -shr 16) -band 0xf000) -notin @(0,0x8000) -or ($entry.ExternalAttributes -band 0x400)){throw 'Portable ZIP path/collision/link rejected.'}
            if($entry.Length -ne [long]$expected[$leaf].size){throw 'Portable ZIP entry size mismatch.'}
            $stream=$entry.Open();$digest=[Security.Cryptography.SHA256]::Create()
            try{$actual=([BitConverter]::ToString($digest.ComputeHash($stream))).Replace('-','')}finally{$digest.Dispose();$stream.Dispose()}
            if($actual -cne $expected[$leaf].sha256){throw 'Portable ZIP entry hash mismatch.'};$seen[$leaf]=$true
        }
        [IO.Directory]::CreateDirectory($runRoot)|Out-Null
        [IO.Directory]::CreateDirectory($payload)|Out-Null
        foreach($entry in $archive.Entries){if($entry.Name){
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,(Join-Path $payload $entry.FullName.Replace('\','/').Substring(11)),$false)
        }}
    }finally{$archive.Dispose()}
    foreach($file in $files){$actual=Get-Item -LiteralPath (Join-Path $payload $file.relative_path)
        if($actual.PSIsContainer -or ($actual.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $actual.Length -ne [long]$file.size -or
           (Get-FileHash -LiteralPath $actual.FullName -Algorithm SHA256).Hash -cne $file.sha256){throw 'Portable extracted file identity mismatch.'}}
    $inner=Get-Content -LiteralPath (Join-Path $payload 'manifest.json') -Encoding UTF8 -Raw|ConvertFrom-Json
    if($inner.schema -cne 'panebind-v1-portable/1' -or $inner.package_id -cne $Spec.package_id -or
       $inner.source_commit -cne $SourceCommit -or $inner.configuration -cne 'Release' -or
       $inner.binary_identity.source_commit -cne $SourceCommit -or $inner.binary_identity.requires_elevation -ne $false -or
       $inner.binary_sha256 -cne $ReleaseHash -or @($inner.files).Count -ne 6){throw 'Portable inner manifest is not this ordinary-permission Release.'}
    $innerSeen=@{}
    foreach($file in @($inner.files)){
        if($file.relative_path -ceq 'manifest.json' -or -not $expected.ContainsKey($file.relative_path) -or $innerSeen.ContainsKey($file.relative_path) -or
           $file.sha256 -cne $expected[$file.relative_path].sha256 -or [long]$file.size -ne [long]$expected[$file.relative_path].size){throw 'Portable inner manifest inventory mismatch.'}
        $innerSeen[$file.relative_path]=$true
    }
    if([IO.File]::ReadAllText((Join-Path $payload 'portable.flag'),$utf8) -cne "PaneBind V1 portable candidate`n" -or
       (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -cne $Spec.zip_sha256){throw 'Portable flag or ZIP changed before readiness.'}
    New-Evidence $ready ((@($RunId,$Spec.package_id,$SourceCommit,$Spec.zip_sha256,$Spec.manifest_sha256,$ReleaseHash)-join "`n")+"`n")
    return [ordered]@{package_id=$Spec.package_id;source_commit=$SourceCommit;status='EXTRACTED_NOT_RUN';actual_directory=$payload;
        zip_sha256=$Spec.zip_sha256;exe_sha256=$ReleaseHash;marker=$ready;normal_GUI_started=$false;function_result='NOT_RUN'}
}
function Prepare-VSCodeTestAsset($Asset){
    # Called only after the exact WDAG/RunId/mapped-input guards and complete
    # payload hashes passed. Applications is guest-local, never a host mapping.
    $applicationRoot='C:\PaneBindV1\Applications\'+$RunId+'\VSCode'
    $record=[ordered]@{kind='VSCode';version=$Asset.version;commit=$Asset.commit;guest_directory=$applicationRoot;
        status='PREPARING';actual_exe=$null;actual_exe_sha256=$null;file_version=$null;marker=$null;
        application_started=$false;synthetic_input_sent=$false;function_result='NOT_RUN';error=$null}
    $evidence=Join-Path $outputDir ($RunId+'-vscode-asset.json')
    try{
        if($Asset.kind -cne 'VSCode' -or $Asset.version -cne '1.142.0' -or
           $Asset.commit -cne '6d62b1d5dace93a33b3a6d3eca1317bd5e15bcb1' -or
           $Asset.relative_path -cne 'VSCode-test.zip' -or $Asset.exe_relative_path -cne 'Code.exe' -or
           $Asset.zip_sha256 -cne 'ACE1DA7D1432D43C1AB4B2BBF643F6494F6D25DEC07494976755EAC522CD50D9' -or
           $Asset.exe_sha256 -cne '5CA701A1FACB91FEC1AC6BBAE11FC837821A1DA34DDEBB91A0BAD7E87AB88144' -or
           $Asset.zip_size -ne 351679077 -or $Asset.expected_entries -ne 3902 -or $Asset.max_entries -ne 10000 -or
           $Asset.expected_expanded_bytes -ne 1035323400 -or $Asset.max_expanded_bytes -ne 1610612736 -or
           $Asset.guest_directory -cne $applicationRoot){throw 'Unrecognized VS Code asset identity or extraction bounds.'}
        foreach($path in @('C:\PaneBindV1','C:\PaneBindV1\Applications')){
            if(Test-Path -LiteralPath $path){$item=Get-Item -LiteralPath $path;if(-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Guest application ancestors must be plain local directories.'}}
        }
        $runRoot='C:\PaneBindV1\Applications\'+$RunId
        if(Test-Path -LiteralPath $runRoot){throw 'Guest application RunId directory already exists; never reuse unknown application state.'}
        $zipPath=Join-Path $inputDir 'VSCode-test.zip'
        if((Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash -cne $Asset.zip_sha256){throw 'VS Code ZIP hash changed before extraction.'}
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $archive=[IO.Compression.ZipFile]::OpenRead($zipPath)
        try{
            if($archive.Entries.Count -ne $Asset.expected_entries -or $archive.Entries.Count -gt $Asset.max_entries){throw 'VS Code ZIP entry bound mismatch.'}
            $seen=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
            [long]$expanded=0
            foreach($entry in $archive.Entries){
                $name=$entry.FullName.Replace('\','/')
                $parts=@($name.Split('/') | Where-Object {$_ -ne ''})
                if(-not $name -or $name.StartsWith('/') -or $name -match '[:\x00-\x1f<>"|?*]' -or
                   @($parts | Where-Object {$_ -eq '..' -or $_ -eq '.' -or $_.EndsWith('.') -or $_.EndsWith(' ') -or $_ -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)'}).Count){throw 'Unsafe VS Code ZIP path.'}
                if((($entry.ExternalAttributes -shr 16) -band 0xf000) -eq 0xa000 -or ($entry.ExternalAttributes -band 0x400)){throw 'VS Code ZIP symlink/reparse entry rejected.'}
                $destination=[IO.Path]::GetFullPath((Join-Path $applicationRoot $name.Replace('/','\')))
                if(-not $destination.StartsWith($applicationRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'VS Code ZIP target escapes its new guest directory.'}
                if(-not $seen.Add($destination.TrimEnd('\'))){throw 'Colliding normalized VS Code ZIP path.'}
                if($entry.Length -gt 536870912){throw 'Oversized single VS Code ZIP entry.'}
                $expanded+=$entry.Length
                if($expanded -gt $Asset.max_expanded_bytes){throw 'VS Code ZIP expanded-size bound exceeded.'}
            }
            if($expanded -ne $Asset.expected_expanded_bytes){throw 'VS Code ZIP expanded-size identity mismatch.'}
            [IO.Directory]::CreateDirectory('C:\PaneBindV1\Applications')|Out-Null
            New-Item -ItemType Directory -Path $runRoot -ErrorAction Stop|Out-Null
            New-Item -ItemType Directory -Path $applicationRoot -ErrorAction Stop|Out-Null
            foreach($entry in $archive.Entries){
                $destination=Join-Path $applicationRoot $entry.FullName.Replace('/','\')
                if(-not $entry.Name){[IO.Directory]::CreateDirectory($destination)|Out-Null;continue}
                [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))|Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$destination,$false)
            }
            $record['archive_entries']=$archive.Entries.Count;$record['expanded_bytes']=$expanded
        }finally{$archive.Dispose()}
        $exe=Join-Path $applicationRoot 'Code.exe'
        $actual=Get-Item -LiteralPath $exe
        if($actual.PSIsContainer -or ($actual.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Prepared Code.exe is not an ordinary file.'}
        $record.actual_exe=$exe;$record.actual_exe_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
        $record.file_version=[Diagnostics.FileVersionInfo]::GetVersionInfo($exe).FileVersion
        if($record.actual_exe_sha256 -cne $Asset.exe_sha256 -or $record.file_version -cne $Asset.version){throw 'Prepared Code.exe version/hash mismatch.'}
        # No portable data folder: VS Code documents that it OVERRIDES the
        # explicit --user-data-dir / --extensions-dir used by the Medium
        # fixture. The fixture does not modify extracted program files and owns
        # fresh per-configuration profile directories instead.
        # No ACL/token change, inherited host profile, installation or launch.
        if(Test-Path -LiteralPath (Join-Path $applicationRoot 'data')){throw 'Unexpected VS Code portable data overrides explicit test profile.'}
        $record['profile_mode']='explicit_cli_directories_created_by_Medium_fixture_no_portable_data'
        $record.marker=Join-Path $applicationRoot 'panebind-v1-test-application.marker'
        New-Evidence $record.marker $RunId
        $record.status='PREPARED_NOT_LAUNCHED'
        return $record
    }catch{
        $record.status='FAILED';$record.error=$_.Exception.Message
        throw
    }finally{
        if(-not (Test-Path -LiteralPath $evidence)){New-Evidence $evidence ($record|ConvertTo-Json -Depth 7)}
    }
}
function Prepare-OfficialMediaApps($Spec){
    # Exact guest-only payload registration; no store/network, developer mode,
    # unsigned-package switch, host install or application activation here.
    # Activation/root identity remain the existing Medium fixture's real test.
    # https://learn.microsoft.com/powershell/module/appx/add-appxpackage
    $root='C:\PaneBindV1\OfficialApps\'+$RunId
    $receipt=[ordered]@{kind='ReviewedWindowsMediaApps';asset_id=$Spec.asset_id;run_id=$RunId;
        status='PREPARING';guest_directory=$root;registrations=@();activation='NOT_RUN';
        product_or_input_started=$false;function_result='NOT_RUN';error=$null}
    $evidence=Join-Path $outputDir ($RunId+'-official-apps.json')
    try{
        $pins=@{
            'Microsoft.VCLibs.140.00_14.0.30704.0_x64__8wekyb3d8bbwe'='DA48D70F7F15E6E5B107B7E1345EFA5E6F19595007656C4FB959A3D88BC4273B'
            'Microsoft.VCLibs.140.00.UWPDesktop_14.0.30704.0_x64__8wekyb3d8bbwe'='2C5FDF0360C91A98A38DD68ED3CEBF39638108F67F5198B938190DA3CA92DD71'
            'Microsoft.UI.Xaml.2.8_8.2310.30001.0_x64__8wekyb3d8bbwe'='1425B2B6710481AA35776CCCCD6B0425A39B086138A653859850598491ED4B2C'
            'Microsoft.WindowsNotepad_11.2312.18.0_x64__8wekyb3d8bbwe'='4E39B3F2FB2934F288161ED7369DB719976F8A5C0AC88A517A1CA31FAF9BB4BE'
            'Microsoft.WindowsNotepad_11.2312.18.0_neutral_split.language-zh-hans_8wekyb3d8bbwe'='06775630C3AF140B48F64D01407C6E20E9F9B07FF13697EF4FAF1584755F2E1B'
            'Microsoft.WindowsNotepad_11.2312.18.0_neutral_split.scale-100_8wekyb3d8bbwe'='85B25AB7C7531F4C8F49E0F82FC8AC0A1FA53FEFEF0D6BEB051B8E20F87AF7E6'
            'Microsoft.WindowsNotepad_11.2312.18.0_neutral_split.scale-125_8wekyb3d8bbwe'='AAB3795FF7EBDBF7AB6A7D798DAA2BA7CEDBDF02CA0448E74B8DBD6455995C93'
            'Microsoft.WindowsTerminal_1.18.10301.0_x64__8wekyb3d8bbwe'='3ED96A9A5F540F3A29CCB71F230B63464318096032ED3FB43CF48D3C724CE5AB'
            'Microsoft.WindowsNotepad_11.2312.18.0_neutral_~_8wekyb3d8bbwe'='F0C1AA97C5A0FAD3D478C6B40C08C91D26346D6B8B2AA05BF3795BFB523D2BD7'
            'Microsoft.WindowsTerminal_3001.18.10301.0_neutral_~_8wekyb3d8bbwe'='C5DED41D2E70AA71EC251D0FD837C93C5CB49B821541BBAD49A8B6CFC57AFA82'
        }
        if($Spec.kind -cne 'ReviewedWindowsMediaApps' -or $Spec.asset_id -cne 'f08fc54e44cf43eb811bc317c4249158' -or
           $Spec.relative_path -cne 'OfficialApps-test.zip' -or $Spec.guest_directory -cne $root -or
           $Spec.signed_blockmap_evidence_sha256 -cne '17B1687624C144DA874CD6D5975439C191F43D77945F15E1DA2873F52A8FDD58' -or
           $Spec.expected_entries -ne 584 -or $Spec.expanded_bytes -ne 63772921 -or @($Spec.packages).Count -ne 10 -or
           [long]$Spec.zip_size -le 0 -or [long]$Spec.zip_size -gt 80MB -or $Spec.zip_sha256 -cnotmatch '^[A-F0-9]{64}$'){
            throw 'Unrecognized exact official-media asset contract.'}
        $seenPackages=@{}
        foreach($package in @($Spec.packages)){
            if(-not $pins.ContainsKey($package.name) -or $seenPackages.ContainsKey($package.name) -or
               $package.inventory_sha256 -cne $pins[$package.name]){throw 'Unknown or changed official package inventory.'}
            $seenPackages[$package.name]=$true
        }
        foreach($path in @('C:\PaneBindV1','C:\PaneBindV1\OfficialApps')){
            if(Test-Path -LiteralPath $path){$item=Get-Item -LiteralPath $path
                if(-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Official application ancestors must be plain guest-local directories.'}}
        }
        if(Test-Path -LiteralPath $root){throw 'Official application RunId directory already exists; preserve it.'}
        $zipPath=Join-Path $inputDir $Spec.relative_path
        if((Get-Item -LiteralPath $zipPath).Length -ne [long]$Spec.zip_size -or
           (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash -cne $Spec.zip_sha256){throw 'Official asset archive hash/size changed.'}
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $archive=[IO.Compression.ZipFile]::OpenRead($zipPath)
        try{
            if($archive.Entries.Count -ne 584){throw 'Official archive entry count mismatch.'}
            $rows=@{};$seen=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
            [long]$expanded=0
            foreach($entry in $archive.Entries){
                $name=$entry.FullName
                $parts=@($name.Split('/'))
                if(-not $entry.Name -or $parts.Count -lt 2 -or -not $pins.ContainsKey($parts[0]) -or
                   $name -match '[\\:\x00-\x1f<>"|?*]' -or
                   @($parts|Where-Object {$_ -in @('','..','.') -or $_.EndsWith('.') -or $_.EndsWith(' ') -or $_ -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)'}).Count -or
                   -not $seen.Add($name) -or (($entry.ExternalAttributes -shr 16) -band 0xf000) -notin @(0,0x8000) -or
                   ($entry.ExternalAttributes -band 0x400) -or $entry.Length -gt 32MB){throw 'Unknown/unsafe official archive entry.'}
                $expanded+=$entry.Length;if($expanded -gt 63772921){throw 'Official expanded size exceeded.'}
                $stream=$entry.Open();$digest=[Security.Cryptography.SHA256]::Create()
                try{$hash=([BitConverter]::ToString($digest.ComputeHash($stream))).Replace('-','')}finally{$digest.Dispose();$stream.Dispose()}
                if(-not $rows.ContainsKey($parts[0])){$rows[$parts[0]]=@()}
                $rows[$parts[0]]+=,[pscustomobject]@{path=$name.Substring($parts[0].Length+1);size=$entry.Length;sha256=$hash}
            }
            foreach($package in @($Spec.packages)){
                $files=@($rows[$package.name]|Sort-Object path)
                $canonical=(@($files|ForEach-Object{$_.path+'|'+$_.size+'|'+$_.sha256})-join "`n")+"`n"
                $digest=[Security.Cryptography.SHA256]::Create()
                try{$hash=([BitConverter]::ToString($digest.ComputeHash($utf8.GetBytes($canonical)))).Replace('-','')}finally{$digest.Dispose()}
                if($files.Count -ne $package.files -or ($files|Measure-Object size -Sum).Sum -ne $package.bytes -or
                   $hash -cne $pins[$package.name]){throw 'Official package archive bytes do not match reviewed inventory.'}
            }
            if($expanded -ne 63772921){throw 'Official archive size is incomplete.'}
            [IO.Directory]::CreateDirectory('C:\PaneBindV1\OfficialApps')|Out-Null
            New-Item -ItemType Directory -Path $root -ErrorAction Stop|Out-Null
            foreach($entry in $archive.Entries){
                $target=Join-Path $root $entry.FullName.Replace('/','\')
                [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))|Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$false)
            }
        }finally{$archive.Dispose()}
        New-Evidence (Join-Path $root 'panebind-v1-official-apps.marker') $RunId
        foreach($package in @($Spec.packages)){
            $directory=Join-Path $root $package.name
            # Re-read all copied bytes before registration, including the
            # manifests and Microsoft signed blockmaps/signatures.
            foreach($file in @($rows[$package.name])){
                $actual=Get-Item -LiteralPath (Join-Path $directory $file.path.Replace('/','\'))
                if($actual.PSIsContainer -or ($actual.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $actual.Length -ne $file.size -or
                   (Get-FileHash -LiteralPath $actual.FullName -Algorithm SHA256).Hash -cne $file.sha256){throw 'Official extracted asset bytes changed.'}
            }
            $manifestPath=Join-Path $directory 'AppxManifest.xml'
            $record=[ordered]@{package=$package.name;manifest=$manifestPath;api='Add-AppxPackage -Register -DisableDevelopmentMode';
                attempted=$false;succeeded=$false;error=$null;hresult=$null;query=@();activation='NOT_RUN'}
            if(-not $package.register_manifest){$record.api='bundle_metadata_not_registration_manifest';$receipt.registrations+=,$record;continue}
            try{
                $record.attempted=$true
                Add-AppxPackage -Register $manifestPath -DisableDevelopmentMode -ErrorAction Stop
                $record.succeeded=$true
                [xml]$xml=Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8
                $record.query=@(Get-AppxPackage -Name ([string]$xml.Package.Identity.Name) -ErrorAction Stop|Select-Object -First 8|
                    ForEach-Object{[ordered]@{name=$_.Name;full_name=$_.PackageFullName;version=$_.Version.ToString();install_location=$_.InstallLocation;status=$_.Status.ToString()}})
            }catch{$record.error=$_.Exception.Message;$record.hresult=$_.Exception.HResult}
            $receipt.registrations+=,$record
        }
        $receipt.status=if(@($receipt.registrations|Where-Object {$_.attempted -and -not $_.succeeded}).Count){'REGISTRATION_FAILED_APPLICATION_ITEM_ONLY'}else{'REGISTERED_NOT_ACTIVATED_OR_FUNCTION_TESTED'}
    }catch{$receipt.status='ASSET_PREPARATION_FAILED_APPLICATION_ITEM_ONLY';$receipt.error=$_.Exception.Message}
    finally{if(-not(Test-Path -LiteralPath $evidence)){New-Evidence $evidence ($receipt|ConvertTo-Json -Depth 8)}}
    return $receipt
}
function Prepare-TerminalTestAsset($Spec){
    # Microsoft officially supports this unpackaged ZIP offline. .portable
    # confines Terminal settings to each new guest/configuration directory;
    # no Appx registration, default-terminal change or application launch.
    $root='C:\PaneBindV1\Terminal\'+$RunId
    $receipt=[ordered]@{kind='WindowsTerminalPortable';run_id=$RunId;status='PREPARING';
        guest_directory=$root;configurations=@();application_started=$false;function_result='NOT_RUN';error=$null}
    $evidence=Join-Path $outputDir ($RunId+'-terminal-asset.json')
    try{
        if($Spec.kind -cne 'WindowsTerminalPortable' -or $Spec.version -cne '1.25.2733.0' -or
           $Spec.source_commit -cne 'dc8ae365847d41f62d1ebca93815bb00959bc12f' -or
           $Spec.relative_path -cne 'Terminal-test.zip' -or $Spec.guest_directory -cne $root -or
           $Spec.archive_root -cne 'terminal-1.25.2733.0' -or $Spec.expected_entries -ne 63 -or $Spec.expanded_bytes -ne 34870043 -or
           $Spec.zip_size -ne 12127272 -or $Spec.zip_sha256 -cne 'BF3EF2012F6C44D8340A4C58125ACC9498D19B580F9890DC043CDF831852E796' -or
           $Spec.exe_sha256 -cne '1CBE52914F7437A4F064327383F1B4C45FE2EB8E92D4F8DAE0AD7EFCB13A20A1'){throw 'Unknown exact official Terminal test-asset contract.'}
        foreach($path in @('C:\PaneBindV1','C:\PaneBindV1\Terminal')){
            if(Test-Path -LiteralPath $path){$item=Get-Item -LiteralPath $path
                if(-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Terminal ancestors must be plain guest-local directories.'}}
        }
        if(Test-Path -LiteralPath $root){throw 'Terminal RunId directory already exists; preserve evidence.'}
        $zip=Join-Path $inputDir $Spec.relative_path
        if((Get-Item -LiteralPath $zip).Length -ne $Spec.zip_size -or
           (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -cne $Spec.zip_sha256){throw 'Terminal input ZIP identity mismatch.'}
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $archive=[IO.Compression.ZipFile]::OpenRead($zip)
        try{
            if($archive.Entries.Count -ne 63){throw 'Terminal ZIP entry count mismatch.'}
            [long]$expanded=0;$seen=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
            foreach($entry in $archive.Entries){
                $name=$entry.FullName;$parts=@($name.TrimEnd('/').Split('/'))
                if(-not $name.StartsWith('terminal-1.25.2733.0/',[StringComparison]::Ordinal) -or
                   $name -match '[\\:\x00-\x1f<>"|?*]' -or
                   @($parts|Where-Object{$_ -in @('','..','.') -or $_.EndsWith('.') -or $_.EndsWith(' ') -or $_ -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)'}).Count -or
                   -not $seen.Add($name.TrimEnd('/')) -or (($entry.ExternalAttributes -shr 16) -band 0xf000) -notin @(0,0x4000,0x8000) -or
                   ($entry.ExternalAttributes -band 0x400) -or $entry.Length -gt 32MB){throw 'Unsafe Terminal archive entry.'}
                $expanded+=$entry.Length;if($expanded -gt 34870043){throw 'Terminal expansion bound exceeded.'}
            }
            if($expanded -ne 34870043){throw 'Terminal expanded-size identity mismatch.'}
            [IO.Directory]::CreateDirectory('C:\PaneBindV1\Terminal')|Out-Null
            New-Item -ItemType Directory -Path $root -ErrorAction Stop|Out-Null
            $selected=if($Configurations -ceq 'DebugAndRelease'){@('Debug','Release')}else{@($Configurations)}
            foreach($configuration in $selected){
                $destination=Join-Path $root $configuration
                New-Item -ItemType Directory -Path $destination -ErrorAction Stop|Out-Null
                foreach($entry in $archive.Entries){
                    $target=Join-Path $destination $entry.FullName.Replace('/','\')
                    if(-not $entry.Name){[IO.Directory]::CreateDirectory($target)|Out-Null;continue}
                    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))|Out-Null
                    [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$false)
                }
                $program=Join-Path $destination 'terminal-1.25.2733.0'
                $exe=Join-Path $program 'WindowsTerminal.exe'
                $hash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
                if($hash -cne $Spec.exe_sha256){throw 'Prepared Terminal executable hash mismatch.'}
                New-Evidence (Join-Path $program '.portable') ''
                New-Evidence (Join-Path $program 'panebind-v1-test-application.marker') $RunId
                $receipt.configurations+=,[ordered]@{configuration=$configuration;actual_exe=$exe;exe_sha256=$hash;
                    file_version=[Diagnostics.FileVersionInfo]::GetVersionInfo($exe).FileVersion;
                    marker=(Join-Path $program 'panebind-v1-test-application.marker');portable_settings_guest_local=$true;
                    status='PREPARED_NOT_LAUNCHED';activation='NOT_RUN'}
            }
        }finally{$archive.Dispose()}
        $receipt.status='PREPARED_NOT_LAUNCHED'
    }catch{$receipt.status='PREPARATION_FAILED_APPLICATION_ITEM_ONLY';$receipt.error=$_.Exception.Message}
    finally{if(-not(Test-Path -LiteralPath $evidence)){New-Evidence $evidence ($receipt|ConvertTo-Json -Depth 7)}}
    return $receipt
}
try {
    # These checks precede compilation or any token/process creation API. This
    # entry is not a general launch tool and has no host override switch.
    if(-not [string]::Equals([IO.Path]::GetFullPath($PSScriptRoot),$inputDir,[StringComparison]::OrdinalIgnoreCase)){
        throw 'Guest script must execute from the exact mapped input directory.'}
    $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
    try{$account=$identity.Name}finally{$identity.Dispose()}
    if($account -notmatch '(?i)\\WDAGUtilityAccount$'){throw 'Only the disposable Windows Sandbox account is supported.'}
    if((Marker (Join-Path $inputDir 'panebind-v1-input.marker')) -cne $RunId -or
       (Marker (Join-Path $outputDir 'panebind-v1-output.marker')) -cne $RunId){throw 'Input/output RunId markers disagree.'}
    $summaryPath=Join-Path $outputDir ($RunId+'-guest-summary.json')
    if(Test-Path -LiteralPath $summaryPath){throw 'A summary already exists; preserve this batch and use a new RunId.'}
    $manifest=Get-Content -LiteralPath (Join-Path $inputDir 'manifest.json') -Encoding UTF8 -Raw | ConvertFrom-Json
    if($manifest.guest_configurations -cne $Configurations){throw 'Configuration selection must match this immutable package manifest.'}
    if($manifest.guest_suite -cne $Suite){throw 'Suite selection must match this immutable package manifest.'}
    if($manifest.schema -cne 'panebind-v1-sandbox/1' -or $manifest.run_id -cne $RunId -or
       $manifest.source_commit -cnotmatch '^[0-9a-f]{40}$' -or
       $manifest.product_source_commit -cne $manifest.source_commit -or $manifest.driver_source_commit -cnotmatch '^[0-9a-f]{40}$' -or
       $manifest.packaging_head -cnotmatch '^[0-9a-f]{40}$' -or $manifest.guest_contract -cne 'panebind-v1-guest-contract/2' -or
       $manifest.product_interface -cne 'panebind-v1-product-guest-interface/1' -or
       $manifest.guest_input -cne $inputDir -or $manifest.guest_output -cne $outputDir -or
       $manifest.host_root -cne ('I:\PaneBindMVP1Runs\'+$RunId)){
        throw 'Manifest identity or exact deployment paths disagree.'}
    $summary.source_commit=$manifest.source_commit
    $summary.product_source_commit=$manifest.product_source_commit
    $summary.driver_source_commit=$manifest.driver_source_commit
    $summary.packaging_head=$manifest.packaging_head
    $summary['guest_contract']=$manifest.guest_contract
    $summary['product_interface']=$manifest.product_interface
    $summary.actual_suite=$Suite
    $allowed=@('panebind.exe','panebind-v1-guest-fixture.exe','Debug\panebind.exe',
        'Debug\panebind-v1-guest-fixture.exe','v1-sandbox-guest.ps1','panebind-v1-input.marker')
    $probeMedium=$null -ne $manifest.medium_environment_probe -and $manifest.medium_environment_probe.enabled -eq $true
    if($probeMedium){
        if($manifest.medium_environment_probe.kind -cne 'ReadOnlyStandardGuestAcl'){throw 'Unknown guest environment probe.'}
        $allowed+='v1-medium-environment-probe-guest.ps1'
    }
    $probeDesktop=$null -ne $manifest.medium_desktop_probe -and $manifest.medium_desktop_probe.enabled -eq $true
    if($probeDesktop){
        if($manifest.medium_desktop_probe.kind -cne 'OneStandardGuestGuiPrecheck'){throw 'Unknown guest desktop probe.'}
        $allowed+='v1-medium-desktop-probe-guest.ps1'
    }
    $assets=@()
    if($null -ne $manifest.test_assets){$assets=@($manifest.test_assets)}
    if($assets.Count -gt 1 -or ($assets.Count -eq 1 -and $assets[0].kind -cne 'VSCode')){throw 'Only one reviewed optional VS Code test asset is permitted.'}
    if($assets.Count){$allowed+='VSCode-test.zip'}
    if($null -ne $manifest.terminal_test_asset){
        if($manifest.terminal_test_asset.kind -cne 'WindowsTerminalPortable' -or
           $manifest.terminal_test_asset.relative_path -cne 'Terminal-test.zip'){throw 'Unknown optional Terminal test asset.'}
        $allowed+='Terminal-test.zip'
    }
    if($null -ne $manifest.official_app_assets){
        if($manifest.official_app_assets.kind -cne 'ReviewedWindowsMediaApps' -or
           $manifest.official_app_assets.relative_path -cne 'OfficialApps-test.zip'){throw 'Unknown optional official application asset.'}
        $allowed+='OfficialApps-test.zip'
    }
    if($null -ne $manifest.portable_test){
        if($Configurations -ceq 'Debug'){throw 'Portable validation requires a Release stage.'}
        $allowed+='PaneBindV1-portable-test.zip'
    }
    $seen=@{}
    foreach($file in @($manifest.files)){
        if($file.relative_path -cnotin $allowed -or $seen.ContainsKey($file.relative_path)){throw 'Unknown or duplicate payload file.'}
        $seen[$file.relative_path]=$true
        $actual=Get-Item -LiteralPath (Join-Path $inputDir $file.relative_path)
        if($actual.PSIsContainer -or ($actual.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $actual.Length -ne [long]$file.size -or
           (Get-FileHash -LiteralPath $actual.FullName -Algorithm SHA256).Hash -cne $file.sha256){throw 'Payload size/hash mismatch.'}
    }
    if($seen.Count -ne $allowed.Count){throw 'Payload manifest is incomplete.'}
    if(@($manifest.configurations).Count -ne 2){throw 'Both exact binary identity pairs must be present.'}
    foreach($name in @('Debug','Release')){
        $pair=@($manifest.configurations|Where-Object name -ceq $name)
        if($pair.Count -ne 1){throw 'Duplicate or missing configuration identity.'};$pair=$pair[0]
        $prefix=if($name -ceq 'Debug'){'Debug\'}else{''}
        $productFile=@($manifest.files|Where-Object relative_path -ceq ($prefix+'panebind.exe'))
        $driverFile=@($manifest.files|Where-Object relative_path -ceq ($prefix+'panebind-v1-guest-fixture.exe'))
        $suffix=$name.ToLowerInvariant()
        if($productFile.Count -ne 1 -or $driverFile.Count -ne 1 -or
           $pair.product_identity.product -cne 'PaneBind' -or $pair.product_identity.version -cne '1.0.0-dev' -or
           $pair.product_identity.subsystem -cne 'windows' -or $pair.product_identity.requires_elevation -ne $false -or
           $pair.product_identity.source_commit -cne $manifest.product_source_commit -or
           $pair.product_identity.verified_binary_sha256 -cne $productFile[0].sha256 -or
           $manifest.('product_'+$suffix+'_sha256') -cne $productFile[0].sha256 -or
           $pair.fixture_identity.source_commit -cne $manifest.driver_source_commit -or
           $pair.fixture_identity.product -cne 'PaneBind V1 guest fixture' -or
           $pair.fixture_identity.guest_contract -cne $manifest.guest_contract -or
           $pair.fixture_identity.product_interface -cne $manifest.product_interface -or
           $pair.fixture_identity.host_synthetic_input -cne 'forbidden' -or $pair.fixture_identity.requires_medium_guest -ne $true -or
           $pair.fixture_identity.verified_binary_sha256 -cne $driverFile[0].sha256 -or
           $manifest.('driver_'+$suffix+'_sha256') -cne $driverFile[0].sha256){throw 'Product/driver identities, interface contract and actual payload hashes disagree.'}
    }
    if($probeMedium){
        # Isolated environment preparation only, before any product or input.
        # DENIED is a useful environment observation, not a product failure.
        $probeInfo=New-Object Diagnostics.ProcessStartInfo
        $probeInfo.FileName=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
        $probeInfo.Arguments='-NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\PaneBindV1\Input\v1-medium-environment-probe-guest.ps1 -RunId '+$RunId
        $probeInfo.UseShellExecute=$false;$probeInfo.CreateNoWindow=$true
        $probe=New-Object Diagnostics.Process;$probe.StartInfo=$probeInfo
        try{
            if(-not $probe.Start()){throw 'Guest environment probe did not start.'}
            if(-not $probe.WaitForExit(60000)){
                $probe.Kill();[void]$probe.WaitForExit(5000)
                throw 'Guest environment probe timed out; account cleanup UNKNOWN; no product/input is permitted.'
            }
            $probePath=Join-Path $outputDir ($RunId+'-medium-environment-probe.json')
            $probeFile=Get-Item -LiteralPath $probePath
            if($probeFile.PSIsContainer -or ($probeFile.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $probeFile.Length -gt 128KB){throw 'Guest environment receipt unavailable or not bounded.'}
            $probeRecord=[IO.File]::ReadAllText($probePath)|ConvertFrom-Json
            $probeEntry=@($manifest.files|Where-Object relative_path -ceq 'v1-medium-environment-probe-guest.ps1')
            if($probeRecord.schema -cne 'panebind-v1-medium-environment-probe/1' -or $probeRecord.run_id -cne $RunId -or
               $probeRecord.probe_source_commit -cne $manifest.packaging_head -or $probeRecord.driver_source_commit -cne $manifest.driver_source_commit -or
               $probeRecord.probe_sha256 -cne $probeEntry[0].sha256 -or $probeRecord.result.account_cleanup_confirmed -ne $true -or
               -not (($probe.ExitCode -eq 0 -and $probeRecord.result.status -ceq 'READ_ONLY_ACL_ALLOWED_PROCESS_NOT_RUN') -or
                      ($probe.ExitCode -eq 2 -and $probeRecord.result.status -ceq 'DENIED'))){throw 'Guest environment observation or exact-account cleanup UNKNOWN; product/input not started.'}
            $summary['medium_environment_probe']=$probeRecord
        }finally{$probe.Dispose()}
    }
    if($probeDesktop){
        # One official alternate-user GUI precheck in this disposable guest.
        # It grants no product/Shell authority and never sends input.
        $probeInfo=New-Object Diagnostics.ProcessStartInfo
        $probeInfo.FileName=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
        $probeInfo.Arguments='-NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\PaneBindV1\Input\v1-medium-desktop-probe-guest.ps1 -RunId '+$RunId
        $probeInfo.UseShellExecute=$false;$probeInfo.CreateNoWindow=$true
        $probe=New-Object Diagnostics.Process;$probe.StartInfo=$probeInfo
        try{
            if(-not $probe.Start()){throw 'Guest desktop probe did not start.'}
            if(-not $probe.WaitForExit(120000)){
                $probe.Kill();[void]$probe.WaitForExit(5000)
                throw 'Guest desktop precheck timed out; cleanup UNKNOWN; no product/input is permitted.'
            }
            $probePath=Join-Path $outputDir ($RunId+'-medium-desktop-probe.json')
            $probeFile=Get-Item -LiteralPath $probePath
            if($probeFile.PSIsContainer -or ($probeFile.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $probeFile.Length -gt 256KB){throw 'Guest desktop receipt unavailable or not bounded.'}
            $probeRecord=[IO.File]::ReadAllText($probePath)|ConvertFrom-Json
            $probeEntry=@($manifest.files|Where-Object relative_path -ceq 'v1-medium-desktop-probe-guest.ps1')
            if($probeRecord.schema -cne 'panebind-v1-medium-desktop-probe/1' -or $probeRecord.run_id -cne $RunId -or
               $probeRecord.probe_source_commit -cne $manifest.packaging_head -or $probeRecord.driver_source_commit -cne $manifest.driver_source_commit -or
               $probeRecord.probe_sha256 -cne $probeEntry[0].sha256 -or $probeRecord.result.account_cleanup_confirmed -ne $true -or
               $probeRecord.result.dacl_cleanup_confirmed -ne $true -or $probeRecord.result.child_process_exited -ne $true -or
               -not (($probe.ExitCode -eq 0 -and $probeRecord.result.status -ceq 'GUI_PRECHECK_OBSERVED_NOT_FULL_SHELL') -or
                     ($probe.ExitCode -eq 3 -and $probeRecord.result.status -ceq 'UNKNOWN'))){throw 'Guest desktop cleanup not confirmed; product/input not started.'}
            $summary['medium_desktop_probe']=$probeRecord
        }finally{$probe.Dispose()}
    }
    $summary['application_availability']=Read-TestApplicationAvailability
    $summary['test_assets']=@()
    foreach($asset in $assets){$summary.test_assets+=,(Prepare-VSCodeTestAsset $asset)}
    if($null -ne $manifest.terminal_test_asset){$summary['terminal_test_asset']=Prepare-TerminalTestAsset $manifest.terminal_test_asset}
    if($null -ne $manifest.official_app_assets){$summary['official_app_assets']=Prepare-OfficialMediaApps $manifest.official_app_assets}
    if($null -ne $manifest.portable_test){
        $release=@($manifest.files | Where-Object relative_path -ceq 'panebind.exe')
        if($release.Count -ne 1){throw 'Portable source Release identity missing.'}
        $summary['portable_test']=Prepare-PortableTestPackage $manifest.portable_test $manifest.source_commit $release[0].sha256
    }
    # Official API references inspected 2026-10-15. Original orchestration code:
    # /windows/win32/api/winnt/ne-winnt-token_information_class (TokenLinkedToken)
    # /windows/win32/api/securitybaseapi/nf-securitybaseapi-createrestrictedtoken
    # /windows/win32/api/securitybaseapi/nf-securitybaseapi-duplicatetokenex
    # /windows/win32/api/securitybaseapi/nf-securitybaseapi-settokeninformation
    # /windows/win32/api/winbase/nf-winbase-createprocesswithtokenw
    # /windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessasuserw
    # /windows/win32/api/winuser/nf-winuser-getuserobjectinformationw (UOI_IO=6)
    # No SetTokenInformation targets the original token. No credentials,
    # session-id/DACL/policy/UAC changes, SAFER_INERT or explicit privilege
    # enable. AsUser may temporarily enable its documented required privileges.
    # A restricted child sharing this disposable guest desktop is a test
    # environment, not a new security boundary against the guest's High apps.
    Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.Security.Cryptography;

public static class PaneBindV1MediumGuest {
    const uint TOKEN_RIGHTS=0x8b, QUERY=8, CREATE_SUSPENDED=4, NO_WINDOW=0x08000000;
    [StructLayout(LayoutKind.Sequential)] struct SidAttributes { public IntPtr Sid; public uint Attributes; }
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct Startup {
        public uint cb; public string reserved,desktop,title; public uint x,y,cx,cy,xchars,ychars,fill,flags;
        public ushort show,reserved2; public IntPtr reservedBytes,input,output,error;
    }
    [StructLayout(LayoutKind.Sequential)] struct ProcessInfo { public IntPtr process,thread;public uint pid,tid; }
    [StructLayout(LayoutKind.Sequential)] struct JobBasic {
        public long processTime,jobTime;public uint flags;public UIntPtr minWorking,maxWorking;
        public uint activeLimit;public UIntPtr affinity;public uint priority,scheduling;
    }
    [StructLayout(LayoutKind.Sequential)] struct IoCounters { public ulong a,b,c,d,e,f; }
    [StructLayout(LayoutKind.Sequential)] struct JobExtended {
        public JobBasic basic;public IoCounters io;public UIntPtr processMemory,jobMemory,peakProcess,peakJob;
    }
    [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
    [DllImport("kernel32.dll")] static extern uint GetCurrentProcessId();
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool ProcessIdToSessionId(uint pid,out uint session);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint ResumeThread(IntPtr h);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint WaitForSingleObject(IntPtr h,uint ms);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr h,out uint code);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool TerminateProcess(IntPtr h,uint code);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr CreateJobObjectW(IntPtr sa,string name);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool AssignProcessToJobObject(IntPtr job,IntPtr process);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetInformationJobObject(IntPtr job,int kind,ref JobExtended data,uint size);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool QueryInformationJobObject(IntPtr job,int kind,IntPtr data,uint size,out uint needed);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool OpenProcessToken(IntPtr p,uint rights,out IntPtr token);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool GetTokenInformation(IntPtr token,int kind,IntPtr data,int size,out int needed);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool DuplicateTokenEx(IntPtr source,uint rights,IntPtr sa,int impersonation,int type,out IntPtr token);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool CreateRestrictedToken(IntPtr source,uint flags,uint disable,IntPtr sids,uint delete,IntPtr privileges,uint restrict,IntPtr restricted,out IntPtr token);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool SetTokenInformation(IntPtr token,int kind,ref SidAttributes data,uint size);
    [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool ConvertStringSidToSidW(string value,out IntPtr sid);
    [DllImport("advapi32.dll")] static extern uint GetLengthSid(IntPtr sid);
    [DllImport("advapi32.dll")] static extern bool IsTokenRestricted(IntPtr token);
    [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr memory);
    [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcessWithTokenW(IntPtr token,uint flags,string exe,StringBuilder command,uint creation,IntPtr environment,string directory,ref Startup startup,out ProcessInfo info);
    [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcessAsUserW(IntPtr token,string exe,StringBuilder command,IntPtr processSa,IntPtr threadSa,bool inherit,uint creation,IntPtr environment,string directory,ref Startup startup,out ProcessInfo info);
    [DllImport("user32.dll")] static extern IntPtr GetThreadDesktop(uint thread);
    [DllImport("user32.dll")] static extern IntPtr GetProcessWindowStation();
    [DllImport("user32.dll",SetLastError=true)] static extern IntPtr OpenInputDesktop(uint flags,bool inherit,uint access);
    [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr desktop);
    [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool GetUserObjectInformationW(IntPtr h,int kind,StringBuilder data,uint size,out uint needed);
    [DllImport("user32.dll",EntryPoint="GetUserObjectInformationW",ExactSpelling=true,SetLastError=true)] static extern bool GetUserObjectInput(IntPtr h,int kind,out int receiving,uint size,out uint needed);
    [DllImport("wtsapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool WTSQuerySessionInformationW(IntPtr server,uint session,int kind,out IntPtr data,out uint bytes);
    [DllImport("wtsapi32.dll")] static extern void WTSFreeMemory(IntPtr data);
    sealed class TokenFacts {
        public string Sid;public int Integrity,Session,ElevationType;public bool Elevated,AdminEnabled,UIAccess,AppContainer,Restricted;
        public Dictionary<string,object> Public() {
            using(var sha=SHA256.Create()) {
                return new Dictionary<string,object>{{"sid_sha256",BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(Sid))).Replace("-","")},
                    {"integrity_rid",Integrity},{"session_id",Session},{"elevated",Elevated},{"elevation_type",ElevationType},
                    {"admin_sid_enabled",AdminEnabled},{"uiaccess",UIAccess},{"app_container",AppContainer},{"restricted",Restricted}};
            }
        }
    }
    static Exception Failure(string api) {return new Win32Exception(Marshal.GetLastWin32Error(),api);}
    static IntPtr Information(IntPtr token,int kind) {
        int needed;GetTokenInformation(token,kind,IntPtr.Zero,0,out needed);
        if(needed<=0||needed>1048576)throw Failure("GetTokenInformation size");
        IntPtr data=Marshal.AllocHGlobal(needed);
        if(!GetTokenInformation(token,kind,data,needed,out needed)){Marshal.FreeHGlobal(data);throw Failure("GetTokenInformation "+kind);}
        return data;
    }
    static int Number(IntPtr token,int kind) {IntPtr p=Information(token,kind);try{return Marshal.ReadInt32(p);}finally{Marshal.FreeHGlobal(p);}}
    static TokenFacts Facts(IntPtr token) {
        var result=new TokenFacts();IntPtr data=Information(token,1);
        try{result.Sid=new SecurityIdentifier(Marshal.ReadIntPtr(data)).Value;}finally{Marshal.FreeHGlobal(data);}
        data=Information(token,25);
        try {var sid=new SecurityIdentifier(Marshal.ReadIntPtr(data));var parts=sid.Value.Split('-');result.Integrity=int.Parse(parts[parts.Length-1]);}
        finally{Marshal.FreeHGlobal(data);}
        result.Session=Number(token,12);result.Elevated=Number(token,20)!=0;result.ElevationType=Number(token,18);
        result.UIAccess=Number(token,26)!=0;result.AppContainer=Number(token,29)!=0;result.Restricted=IsTokenRestricted(token);
        data=Information(token,2);
        try {
            int count=Marshal.ReadInt32(data),offset=IntPtr.Size==8?8:4,stride=Marshal.SizeOf(typeof(SidAttributes));
            if(count<0||count>1024)throw new InvalidOperationException("Unexpected token groups");
            for(int i=0;i<count;++i){var entry=(SidAttributes)Marshal.PtrToStructure(IntPtr.Add(data,offset+i*stride),typeof(SidAttributes));
                if(new SecurityIdentifier(entry.Sid).Value=="S-1-5-32-544"&&(entry.Attributes&4)!=0&&(entry.Attributes&16)==0)result.AdminEnabled=true;}
        }finally{Marshal.FreeHGlobal(data);}
        return result;
    }
    static bool Medium(TokenFacts candidate,TokenFacts original) {
        return candidate.Sid==original.Sid&&candidate.Session==original.Session&&candidate.Session>0&&
            candidate.Integrity==8192&&!candidate.Elevated&&!candidate.AdminEnabled&&!candidate.UIAccess&&!candidate.AppContainer;
    }
    static string ObjectName(IntPtr handle) {
        if(handle==IntPtr.Zero)throw Failure("Desktop/windowstation handle");
        uint needed;var name=new StringBuilder(256);
        if(!GetUserObjectInformationW(handle,2,name,512,out needed))throw Failure("GetUserObjectInformation name");
        return name.ToString();
    }
    static Dictionary<string,object> Desktop(int session) {
        uint actual;if(!ProcessIdToSessionId(GetCurrentProcessId(),out actual)||actual!=(uint)session||session<=0)throw new InvalidOperationException("Noninteractive or inconsistent session");
        IntPtr threadHandle=GetThreadDesktop(GetCurrentThreadId());
        var station=ObjectName(GetProcessWindowStation());var thread=ObjectName(threadHandle);
        var input=OpenInputDesktop(0,false,1);string inputName;int inputReceives,threadReceives;uint needed;
        try{
            inputName=ObjectName(input);
            if(!GetUserObjectInput(input,6,out inputReceives,4,out needed)||needed>4)throw Failure("GetUserObjectInformation input UOI_IO");
            if(!GetUserObjectInput(threadHandle,6,out threadReceives,4,out needed)||needed>4)throw Failure("GetUserObjectInformation thread UOI_IO");
        }finally{if(input!=IntPtr.Zero)CloseDesktop(input);}
        IntPtr data;uint bytes;if(!WTSQuerySessionInformationW(IntPtr.Zero,actual,8,out data,out bytes))throw Failure("WTS session state");
        int state;try{if(bytes<4)throw new InvalidOperationException("Incomplete WTS state");state=Marshal.ReadInt32(data);}finally{WTSFreeMemory(data);}
        if(station!="WinSta0"||thread!="Default"||inputName!="Default"||state!=0||inputReceives==0||threadReceives==0)
            throw new InvalidOperationException("Active WinSta0 Default desktop actually receiving input required");
        return new Dictionary<string,object>{{"window_station",station},{"thread_desktop",thread},{"input_desktop",inputName},
            {"input_uoi_io",inputReceives!=0},{"thread_uoi_io",threadReceives!=0},{"wts_state",state},{"session_id",session}};
    }
    static IntPtr MediumToken(IntPtr original,TokenFacts own,List<object> attempts,out string method) {
        IntPtr candidate=IntPtr.Zero,linked=IntPtr.Zero;method=null;
        if(Medium(own,own)) {
            if(!DuplicateTokenEx(original,TOKEN_RIGHTS,IntPtr.Zero,2,1,out candidate))throw Failure("Duplicate current Medium token");
            method="duplicate_current_medium";return candidate;
        }
        IntPtr linkInfo=IntPtr.Zero;
        try{
            linkInfo=Information(original,19);linked=Marshal.ReadIntPtr(linkInfo);
            if(!DuplicateTokenEx(linked,TOKEN_RIGHTS,IntPtr.Zero,2,1,out candidate))throw Failure("Duplicate linked primary token");
            var observed=Facts(candidate);attempts.Add(new Dictionary<string,object>{{"method","linked_standard_token"},{"facts",observed.Public()},{"accepted",Medium(observed,own)}});
            if(Medium(observed,own)){method="linked_standard_token";return candidate;}
            CloseHandle(candidate);candidate=IntPtr.Zero;
        }catch(Exception e){attempts.Add(new Dictionary<string,object>{{"method","linked_standard_token"},{"accepted",false},{"error",e.Message}});if(candidate!=IntPtr.Zero){CloseHandle(candidate);candidate=IntPtr.Zero;}}
        finally{if(linkInfo!=IntPtr.Zero)Marshal.FreeHGlobal(linkInfo);if(linked!=IntPtr.Zero)CloseHandle(linked);}
        // LUA_TOKEN and DISABLE_MAX_PRIVILEGE affect this NEW token only. No
        // SANDBOX_INERT flag, credentials, impersonation, or original adjustment.
        if(!CreateRestrictedToken(original,5,0,IntPtr.Zero,0,IntPtr.Zero,0,IntPtr.Zero,out candidate))throw Failure("CreateRestrictedToken LUA");
        IntPtr mediumSid=IntPtr.Zero;
        try {
            if(!ConvertStringSidToSidW("S-1-16-8192",out mediumSid))throw Failure("Create Medium integrity SID");
            var label=new SidAttributes{Sid=mediumSid,Attributes=32};
            if(!SetTokenInformation(candidate,25,ref label,(uint)Marshal.SizeOf(typeof(SidAttributes))+GetLengthSid(mediumSid)))throw Failure("Set NEW token Medium integrity");
            var observed=Facts(candidate);attempts.Add(new Dictionary<string,object>{{"method","restricted_lua_medium"},{"facts",observed.Public()},{"accepted",Medium(observed,own)}});
            if(!Medium(observed,own))throw new InvalidOperationException("Restricted token did not actually meet Medium non-elevated same-user/session contract");
            method="restricted_lua_medium";return candidate;
        }catch{CloseHandle(candidate);throw;}finally{if(mediumSid!=IntPtr.Zero)LocalFree(mediumSid);}
    }
    public static Dictionary<string,object> Run(string executable,string arguments,uint timeoutSeconds) {
        using(var identity=WindowsIdentity.GetCurrent())
            if(!identity.Name.EndsWith("\\WDAGUtilityAccount",StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("Disposable guest account required");
        bool exactFixture=string.Equals(executable,"C:\\PaneBindV1\\Input\\panebind-v1-guest-fixture.exe",StringComparison.OrdinalIgnoreCase)||
            string.Equals(executable,"C:\\PaneBindV1\\Input\\Debug\\panebind-v1-guest-fixture.exe",StringComparison.OrdinalIgnoreCase);
        if(timeoutSeconds<30||timeoutSeconds>1800||!exactFixture)throw new InvalidOperationException("Exact guest fixture scope required");
        var result=new Dictionary<string,object>{{"status","BLOCKED"},{"fixture_started",false},{"product_cleanup_pass",false},{"process_creation_attempts",new List<object>()},{"token_attempts",new List<object>()}};
        var tokenAttempts=(List<object>)result["token_attempts"];var processAttempts=(List<object>)result["process_creation_attempts"];
        IntPtr original=IntPtr.Zero,candidate=IntPtr.Zero,job=IntPtr.Zero;var info=new ProcessInfo();bool assigned=false;
        try {
            if(!OpenProcessToken(GetCurrentProcess(),TOKEN_RIGHTS,out original))throw Failure("Open own process token");
            var own=Facts(original);result["bootstrap_token"]=own.Public();result["bootstrap_desktop"]=Desktop(own.Session);
            string method;candidate=MediumToken(original,own,tokenAttempts,out method);result["token_method"]=method;
            var startup=new Startup();startup.cb=(uint)Marshal.SizeOf(typeof(Startup));startup.desktop="winsta0\\default";
            string command="\""+executable+"\" "+arguments;
            if(command.Length>1023)throw new InvalidOperationException("Bounded token launch command too long");
            result["requested_executable"]=executable;result["requested_desktop"]=startup.desktop;
            result["requested_directory"]="C:\\PaneBindV1\\Output";result["requested_creation_flags"]=NO_WINDOW|CREATE_SUSPENDED;
            var launchToken=Facts(candidate);result["launch_token"]=launchToken.Public();
            if(!Medium(launchToken,own))throw new InvalidOperationException("Launch token no longer satisfies exact Medium contract");
            bool created=CreateProcessWithTokenW(candidate,0,executable,new StringBuilder(command),NO_WINDOW|CREATE_SUSPENDED,IntPtr.Zero,"C:\\PaneBindV1\\Output",ref startup,out info);
            int error=created?0:Marshal.GetLastWin32Error();processAttempts.Add(new Dictionary<string,object>{{"api","CreateProcessWithTokenW"},{"succeeded",created},{"win32_error",error},{"same_candidate_token",true},{"executable",executable},{"desktop",startup.desktop}});
            // One bounded API contrast only. ERROR_ACCESS_DENIED is not proof
            // that the RO mapping or its ACL is the cause: AsUser has a
            // distinct documented token/privilege contract. Keep the token,
            // image, directory and desktop unchanged; do not adjust security.
            if(!created&&(error==1314||error==5)){
                result["as_user_fallback_trigger_win32_error"]=error;
                result["fallback_desktop"]=Desktop(own.Session);
                startup=new Startup();startup.cb=(uint)Marshal.SizeOf(typeof(Startup));startup.desktop="winsta0\\default";
                info=new ProcessInfo();
                created=CreateProcessAsUserW(candidate,executable,new StringBuilder(command),IntPtr.Zero,IntPtr.Zero,false,NO_WINDOW|CREATE_SUSPENDED,IntPtr.Zero,"C:\\PaneBindV1\\Output",ref startup,out info);
                error=created?0:Marshal.GetLastWin32Error();processAttempts.Add(new Dictionary<string,object>{{"api","CreateProcessAsUserW"},{"succeeded",created},{"win32_error",error},{"same_candidate_token",true},{"executable",executable},{"desktop",startup.desktop}});
            }
            if(!created)throw new Win32Exception(error,"Medium child creation failed; no security policy or ACL changes attempted");
            result["fixture_pid"]=info.pid;result["fixture_tid"]=info.tid;result["requested_desktop"]=startup.desktop;
            uint childSession;if(!ProcessIdToSessionId(info.pid,out childSession))throw Failure("Read suspended child process session");
            result["actual_process_session"]=childSession;
            if(childSession!=(uint)own.Session)throw new InvalidOperationException("Actual child process session changed");
            IntPtr childToken;if(!OpenProcessToken(info.process,QUERY,out childToken))throw Failure("Read suspended child token");
            try {var actual=Facts(childToken);result["actual_child_token"]=actual.Public();if(!Medium(actual,own))throw new InvalidOperationException("Actual child token is not exact Medium/non-elevated");}
            finally{CloseHandle(childToken);}
            job=CreateJobObjectW(IntPtr.Zero,null);if(job==IntPtr.Zero)throw Failure("Create fixture-only job");
            var limits=new JobExtended();limits.basic.flags=0x2000;
            if(!SetInformationJobObject(job,9,ref limits,(uint)Marshal.SizeOf(typeof(JobExtended))))throw Failure("Set fixture-only job close limit");
            if(!AssignProcessToJobObject(job,info.process))throw Failure("Assign exact suspended fixture to job");assigned=true;
            if(ResumeThread(info.thread)==uint.MaxValue)throw Failure("Resume exact Medium fixture");
            result["fixture_started"]=true;result["creation_flags"]="CREATE_SUSPENDED|CREATE_NO_WINDOW";
            uint waited=WaitForSingleObject(info.process,timeoutSeconds*1000);result["wait_result"]=waited;
            if(waited==258){result["status"]="TIMEOUT";result["forced_test_cleanup"]=true;return result;}
            if(waited!=0)throw Failure("Wait for exact fixture");
            uint exit;if(!GetExitCodeProcess(info.process,out exit))throw Failure("Read fixture exit");result["fixture_exit_code"]=exit;
            IntPtr accounting=Marshal.AllocHGlobal(48);
            try{uint needed;if(!QueryInformationJobObject(job,1,accounting,48,out needed))throw Failure("Query fixture descendants");
                uint active=(uint)Marshal.ReadInt32(accounting,40);result["descendants_alive_at_fixture_exit"]=active;result["forced_test_cleanup"]=active!=0;}
            finally{Marshal.FreeHGlobal(accounting);}
            result["status"]=exit==0?"FIXTURE_EXITED_ZERO_EVIDENCE_REQUIRED":"FIXTURE_FAILED";
            // Zero exit is never itself a feature or product-cleanup PASS.
            return result;
        }catch(Exception e){result["error"]=e.Message;return result;}
        finally {
            if(job!=IntPtr.Zero)CloseHandle(job);
            if(!assigned&&info.process!=IntPtr.Zero)TerminateProcess(info.process,78);
            if(info.thread!=IntPtr.Zero)CloseHandle(info.thread);if(info.process!=IntPtr.Zero)CloseHandle(info.process);
            if(candidate!=IntPtr.Zero)CloseHandle(candidate);if(original!=IntPtr.Zero)CloseHandle(original);
        }
    }
}
'@
    $anyStageFailed=$false
    $selectedConfigurations=if($Configurations -ceq 'DebugAndRelease'){@('Debug','Release')}else{@($Configurations)}
    foreach($configuration in $selectedConfigurations) {
        $sourceDir=if($configuration -ceq 'Release'){$inputDir}else{Join-Path $inputDir 'Debug'}
        $sourceProduct=Join-Path $sourceDir 'panebind.exe'
        $fixture=Join-Path $sourceDir 'panebind-v1-guest-fixture.exe'
        $actualDir=Join-Path $outputDir ($RunId+'-product-'+$configuration)
        if(Test-Path -LiteralPath $actualDir){throw 'Product output directory already exists; do not overwrite evidence.'}
        [IO.Directory]::CreateDirectory($actualDir)|Out-Null
        $actualProduct=Join-Path $actualDir 'panebind.exe'
        [IO.File]::Copy($sourceProduct,$actualProduct,$false)
        New-Evidence (Join-Path $actualDir 'portable.flag') "PaneBind V1 portable test candidate`n"
        $sourceHash=(Get-FileHash -LiteralPath $sourceProduct -Algorithm SHA256).Hash
        $actualHash=(Get-FileHash -LiteralPath $actualProduct -Algorithm SHA256).Hash
        if($sourceHash -cne $actualHash){throw 'Copied product hash mismatch.'}
        $arguments='--run-id '+$RunId+' --output "'+$outputDir+'" --product "'+$actualProduct+'" --suite '+$Suite
        $stage=[ordered]@{configuration=$configuration;source_input=$sourceProduct;actual_exe=$actualProduct;
            product_source_commit=$manifest.product_source_commit;driver_source_commit=$manifest.driver_source_commit;
            packaging_head=$manifest.packaging_head;guest_contract=$manifest.guest_contract;product_interface=$manifest.product_interface;
            source_input_sha256=$sourceHash;actual_exe_sha256=$actualHash;hash_equal=$true;
            fixture=$fixture;fixture_sha256=(Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash;
            arguments=$arguments;actual_suite=$Suite;actual_result=$null;feature_result='NOT_EVALUATED';human_uat='NOT_RUN'}
        $stage.actual_result=[PaneBindV1MediumGuest]::Run($fixture,$arguments,[uint32]$StageTimeoutSeconds)
        if($stage.actual_result.ContainsKey('fixture_exit_code')){
            $stage['fixture_exit_reason']=Fixture-ExitReason ([uint32]$stage.actual_result['fixture_exit_code'])
        }
        $stageFailed=$stage.actual_result.status -cne 'FIXTURE_EXITED_ZERO_EVIDENCE_REQUIRED' -or $stage.actual_result.forced_test_cleanup
        $stage['safe_to_continue_independent_release']=$false
        if($stageFailed -and $configuration -ceq 'Debug' -and
           $stage.actual_result.status -ceq 'FIXTURE_FAILED' -and
           -not $stage.actual_result.forced_test_cleanup -and
           $stage.actual_result.descendants_alive_at_fixture_exit -eq 0){
            # A local Debug function failure does not prohibit the authorized
            # independent Release contrast. Require actual normal product exit
            # and all its cleanup facts, not job/guest destruction or exit=0.
            $driverPath=Join-Path $outputDir ($RunId+'-debug-fixture.jsonl')
            $productLog=Join-Path $actualDir 'panebind.log.jsonl'
            if((Test-Path -LiteralPath $driverPath -PathType Leaf) -and (Test-Path -LiteralPath $productLog -PathType Leaf) -and
               (Get-Item -LiteralPath $driverPath).Length -le 8MB -and (Get-Item -LiteralPath $productLog).Length -le 1MB){
                $driverRows=@(Get-Content -LiteralPath $driverPath -Encoding UTF8 | ForEach-Object {$_|ConvertFrom-Json})
                $productRows=@(Get-Content -LiteralPath $productLog -Encoding UTF8 | ForEach-Object {$_|ConvertFrom-Json})
                $driverEnd=@($driverRows|Where-Object {$_.type -ceq 'driver_shutdown'}|Select-Object -Last 1)
                $stop=@($driverRows|Where-Object {$_.type -ceq 'product_stop'}|Select-Object -Last 1)
                $nativeStop=@($productRows|Where-Object {$_.type -ceq 'shutdown'}|Select-Object -Last 1)
                $externalCleanup=@($driverRows|Where-Object {
                    ($_.type -ceq 'exact_explorer_cleanup' -and $_.actual_root_gone -ne $true) -or
                    ($_.type -ceq 'application_cleanup' -and $_.root_confirmed -eq $true -and $_.actual_root_gone -ne $true)})
                $stage.safe_to_continue_independent_release=$driverEnd.Count -eq 1 -and $stop.Count -eq 1 -and $nativeStop.Count -eq 1 -and
                    $externalCleanup.Count -eq 0 -and
                    $driverEnd[0].owned_windows_destroyed -eq $true -and $driverEnd[0].product_process_closed -eq $true -and
                    $stop[0].process_exit_observed -eq $true -and $stop[0].product_exit_code -eq 0 -and
                    $nativeStop[0].own_hooks_stopped -eq $true -and $nativeStop[0].own_overlay_destroyed -eq $true -and
                    $nativeStop[0].native_worker_exited -eq $true -and $nativeStop[0].issued_native_completion_unknown -eq $false -and
                    $nativeStop[0].visual_recovery_stopped -eq $true -and $nativeStop[0].visual_restore_confirmed -eq $true
            }
        }
        $summary.stages+=,$stage
        New-Evidence (Join-Path $outputDir ($RunId+'-'+$configuration+'-launch.json')) ($stage|ConvertTo-Json -Depth 15)
        if($stageFailed){$anyStageFailed=$true
            if(-not $stage.safe_to_continue_independent_release){
                throw ('Actual Medium '+$configuration+' fixture failed or cleanup was not independently confirmed; inspect launch/raw evidence.')}
        }
    }
    $summary.status=if($anyStageFailed){'AUTOMATED_FIXTURES_COMPLETED_WITH_FAILURES'}else{'AUTOMATED_FIXTURES_EXITED_REVIEW_EVIDENCE'}
} catch {
    $summary.status='BLOCKED_OR_FAILED'
    $summary.error=$_.Exception.Message
} finally {
    if($summaryPath -and -not (Test-Path -LiteralPath $summaryPath)){
        New-Evidence $summaryPath ($summary|ConvertTo-Json -Depth 18)
    }
}
if($summary.status -cne 'AUTOMATED_FIXTURES_EXITED_REVIEW_EVIDENCE'){exit 78}
exit 0
