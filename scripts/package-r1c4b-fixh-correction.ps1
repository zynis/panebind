[CmdletBinding(DefaultParameterSetName='Package')]
param(
    [Parameter(Mandatory=$true,ParameterSetName='Package')][string]$ReplayArtifactPath,
    [Parameter(Mandatory=$true,ParameterSetName='Package')][string]$OutputDirectory,
    [Parameter(Mandatory=$true,ParameterSetName='SelfTest')][switch]$SelfTest
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression
$script:correctionRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$script:correctionRoot=[IO.Path]::GetFullPath((Join-Path $script:correctionRepo 'uat/r1c4b-fixh-correction'))
$script:originalRoot=[IO.Path]::GetFullPath((Join-Path $script:correctionRepo 'uat/r1c4b-fixh'))

function Assert-Correction([bool]$Pass,[string]$Reason){if(-not $Pass){throw [IO.InvalidDataException]::new("Fix H correction package: $Reason")}}
function Get-CorrectionField($Object,[string]$Name){$field=$Object.PSObject.Properties[$Name];Assert-Correction ($null -ne $field) "missing $Name";return $field.Value}
function Get-CorrectionText($Object,[string]$Name){$v=Get-CorrectionField $Object $Name;Assert-Correction ($v -is [string] -and -not [string]::IsNullOrWhiteSpace($v)) "typed $Name";return $v}
function Test-CorrectionBelow([string]$Path,[string]$Root){return $Path.StartsWith($Root.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)}
function Resolve-CorrectionPath([string]$Value,[string]$Root){
    Assert-Correction (-not [string]::IsNullOrWhiteSpace($Value)) 'empty path'
    $p=if([IO.Path]::IsPathRooted($Value)){[IO.Path]::GetFullPath($Value)}else{[IO.Path]::GetFullPath((Join-Path $script:correctionRepo $Value))}
    Assert-Correction (Test-CorrectionBelow $p $Root) 'path outside fixed ignored scope'
    $current=$p
    while($current -and -not $current.Equals($script:correctionRepo,[StringComparison]::OrdinalIgnoreCase)){
        if(Test-Path -LiteralPath $current){Assert-Correction (([IO.File]::GetAttributes($current) -band [IO.FileAttributes]::ReparsePoint) -eq 0) 'reparse-point path'}
        $current=[IO.Path]::GetDirectoryName($current)
    }
    Assert-Correction ($null -ne $current -and $current.Equals($script:correctionRepo,[StringComparison]::OrdinalIgnoreCase)) 'unresolved repository ancestry'
    return $p
}
function Resolve-CorrectionReference([string]$Value){
    Assert-Correction (-not [string]::IsNullOrWhiteSpace($Value)) 'empty original reference'
    $p=if([IO.Path]::IsPathRooted($Value)){[IO.Path]::GetFullPath($Value)}else{[IO.Path]::GetFullPath((Join-Path $script:correctionRepo $Value))}
    Assert-Correction (Test-CorrectionBelow $p $script:originalRoot) 'original reference outside fixed batch scope'
    # Only lexical validation: no stat/open/hash of historical evidence.
    return $p
}
function Get-CorrectionSha([string]$Path){return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash}
function Get-CorrectionUtf8([string]$Text){return [Text.UTF8Encoding]::new($false).GetBytes($Text)}
function Write-CorrectionNew([string]$Path,[byte[]]$Bytes){$stream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);try{$stream.Write($Bytes,0,$Bytes.Length)}finally{$stream.Dispose()}}

function Read-CorrectionReferences($Artifact,[string]$BatchId,[bool]$RequireComplete){
    $plan=Resolve-CorrectionReference (Get-CorrectionText $Artifact 'OriginalPlanPath')
    $planSha=Get-CorrectionText $Artifact 'OriginalPlanSHA256'
    Assert-Correction ($plan.IndexOf($BatchId,[StringComparison]::OrdinalIgnoreCase) -ge 0 -and $planSha -cmatch '^[0-9A-Fa-f]{64}$') 'unbound original plan reference'
    $controlField=$Artifact.PSObject.Properties['OriginalControlBindings'];$packageField=$Artifact.PSObject.Properties['OriginalPackageBindings']
    Assert-Correction ($null -ne $controlField -and $null -ne $packageField -and $null -ne $controlField.Value -and $null -ne $packageField.Value) 'missing original reference arrays'
    $controls=@($controlField.Value);$packages=@($packageField.Value)
    Assert-Correction ($controls.Count -le 7 -and $packages.Count -le 4) 'too many original references'
    if($RequireComplete){Assert-Correction ($controls.Count -eq 7 -and $packages.Count -eq 4) 'incomplete PASS original references'}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($item in $controls){
        $null=Get-CorrectionText $item 'Name';$p=Resolve-CorrectionReference (Get-CorrectionText $item 'Path');$h=Get-CorrectionText $item 'SHA256'
        Assert-Correction ($p.IndexOf($BatchId,[StringComparison]::OrdinalIgnoreCase) -ge 0 -and $h -cmatch '^[0-9A-Fa-f]{64}$' -and $seen.Add($p)) 'bad/duplicate original control reference'
    }
    $match=@($controls|Where-Object {(Resolve-CorrectionReference (Get-CorrectionText $_ 'Path')) -ieq $plan -and $_.SHA256 -ieq $planSha})
    if($RequireComplete -or $controls.Count -gt 0){Assert-Correction ($match.Count -eq 1) 'original plan SHA not bound to control list'}
    $zipCount=0;$manifestCount=0
    foreach($item in $packages){
        $null=Get-CorrectionText $item 'Name';$p=Resolve-CorrectionReference (Get-CorrectionText $item 'Path');$h=Get-CorrectionText $item 'SHA256';$bytes=Get-CorrectionField $item 'Bytes'
        Assert-Correction ($p.IndexOf($BatchId,[StringComparison]::OrdinalIgnoreCase) -ge 0 -and $h -cmatch '^[0-9A-Fa-f]{64}$' -and ($bytes -is [int] -or $bytes -is [long]) -and $bytes -gt 0 -and $seen.Add($p)) 'bad/duplicate old ZIP or manifest reference'
        if([IO.Path]::GetExtension($p) -ieq '.zip'){$zipCount++}elseif([IO.Path]::GetFileName($p) -ieq 'package-manifest.json'){$manifestCount++}else{throw [IO.InvalidDataException]::new('Fix H correction package: unexpected original package role')}
    }
    Assert-Correction ($zipCount -le 3 -and $manifestCount -le 1) 'unexpected old package references'
    if($RequireComplete){Assert-Correction ($zipCount -eq 3 -and $manifestCount -eq 1) 'not exactly three old ZIPs plus manifest'}
    # Old paths are references only. Never open prior ZIPs, logs or EXEs here.
    $completeness=if($controls.Count -eq 7 -and $packages.Count -eq 4 -and $zipCount -eq 3 -and $manifestCount -eq 1){'COMPLETE'}else{'PARTIAL'}
    return [pscustomobject]@{OriginalPlanPath=$plan;OriginalPlanSHA256=$planSha;OriginalControlBindings=$controls;OriginalPackageBindings=$packages;ReferenceCompleteness=$completeness}
}

function Add-CorrectionZipText($Archive,[string]$Name,[string]$Text){
    $entry=$Archive.CreateEntry($Name,[IO.Compression.CompressionLevel]::Optimal)
    $stream=$entry.Open()
    try{$bytes=Get-CorrectionUtf8 $Text;$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
}

function New-FixHCorrectionPackage([string]$Replay,[string]$Destination,[bool]$SyntheticOnly=$false){
    $source=Resolve-CorrectionPath $Replay $script:correctionRoot
    $output=Resolve-CorrectionPath $Destination $script:correctionRoot
    Assert-Correction ([IO.Path]::GetExtension($source) -ieq '.json') 'replay artifact must be JSON'
    Assert-Correction ([IO.File]::Exists($source)) 'new replay artifact does not exist'
    Assert-Correction (-not [IO.Directory]::Exists($output) -and -not [IO.File]::Exists($output)) 'output directory already exists'
    Assert-Correction (-not (Test-CorrectionBelow $source $output)) 'source inside output directory'
    $artifact=Get-Content -LiteralPath $source -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-Correction ((Get-CorrectionText $artifact 'Schema') -ceq 'r1c4b-fixh-corrected-replay/v1') 'replay schema'
    $result=Get-CorrectionText $artifact 'Result'
    Assert-Correction ($result -cin @('PASS','INVALID_EVIDENCE')) 'replay Result is not an allowed auditor result'
    $batch=Get-CorrectionText $artifact 'OriginalBatchId'
    Assert-Correction ($batch -cmatch '^[0-9A-Fa-f]{32}$|^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$') 'OriginalBatchId format'
    $evidenceSha=Get-CorrectionText $artifact 'EvidenceImplementationSHA'
    $auditorSha=Get-CorrectionText $artifact 'AuditorImplementationSHA'
    Assert-Correction ($evidenceSha -ieq '01633ff31776b42be9fc2383ed301d9884e80019') 'wrong frozen evidence implementation'
    Assert-Correction ($auditorSha -cmatch '^[0-9A-Fa-f]{40}$' -or ($result -ceq 'INVALID_EVIDENCE' -and $auditorSha -ceq 'NOT_AVAILABLE')) 'auditor SHA format'
    $matchField=$artifact.PSObject.Properties['HistoricalCountersMatch'];$correctionField=$artifact.PSObject.Properties['CounterCorrections']
    Assert-Correction ($null -ne $matchField -and ($matchField.Value -is [bool] -or ($result -ceq 'INVALID_EVIDENCE' -and $null -eq $matchField.Value))) 'HistoricalCountersMatch type'
    Assert-Correction ($null -ne $correctionField -and $null -ne $correctionField.Value -and $correctionField.Value -is [array]) 'CounterCorrections type'
    $references=Read-CorrectionReferences $artifact $batch ($result -ceq 'PASS')
    $sourceLength=([IO.FileInfo]::new($source)).Length
    Assert-Correction ($sourceLength -gt 0) 'empty corrected replay artifact'
    $sourceSha=Get-CorrectionSha $source
    $archiveName='fixh-correction-evidence.zip'
    $archivePath=Join-Path $output $archiveName
    $manifestPath=Join-Path $output 'package-manifest.json'
    $sourceEntry='correction/corrected-replay.json'
    $internalManifest=[ordered]@{
        Schema='r1c4b-fixh-correction-archive-manifest/v1';PackagingScope='NEW_CORRECTED_REPLAY_ONLY';AcceptanceClaim='NONE'
        OriginalBatchId=$batch;AuditorReportedResult=$result;EvidenceImplementationSHA=$evidenceSha;AuditorImplementationSHA=$auditorSha
        Source=[ordered]@{OriginalPath=$source;ArchivePath=$sourceEntry;Bytes=$sourceLength;SHA256=$sourceSha}
        ReferenceCompleteness=$references.ReferenceCompleteness;OriginalPlanPath=$references.OriginalPlanPath;OriginalPlanSHA256=$references.OriginalPlanSHA256
        OriginalControlBindings=$references.OriginalControlBindings;OriginalPackageBindings=$references.OriginalPackageBindings
        OriginalReportBinding=$artifact.OriginalReportBinding
        HistoricalFilesIncluded=0;OldPackagesIncluded=0;ExecutablesIncluded=0;SyntheticOnly=$SyntheticOnly
    }
    $readme=@'
PaneBind Fix H correction audit evidence package

This archive contains only the new corrected-replay JSON artifact, byte-for-byte,
plus this README and a SHA256 manifest. The prior three ZIP packages and the
443 original evidence files are not copied, modified, or re-evaluated here.
Historical paths and hashes in the manifest are references, not packaged files.
Packaging proves byte preservation only. It is not acceptance, does not Seal
the batch, and does not upload or commit any evidence.
'@
    $created=$false
    try{
        $null=[IO.Directory]::CreateDirectory($output);$created=$true
        $file=[IO.File]::Open($archivePath,[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
        try{
            $zip=[IO.Compression.ZipArchive]::new($file,[IO.Compression.ZipArchiveMode]::Create,$true)
            try{
                $entry=$zip.CreateEntry($sourceEntry,[IO.Compression.CompressionLevel]::Optimal)
                $entryStream=$entry.Open()
                try{
                    $input=[IO.File]::Open($source,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
                    try{$input.CopyTo($entryStream)}finally{$input.Dispose()}
                }finally{$entryStream.Dispose()}
                Add-CorrectionZipText $zip 'SHA256-MANIFEST.json' (($internalManifest|ConvertTo-Json -Depth 12)+"`n")
                Add-CorrectionZipText $zip 'README.txt' $readme
            }finally{$zip.Dispose()}
        }finally{$file.Dispose()}
        Assert-Correction ((Get-CorrectionSha $source) -ieq $sourceSha -and ([IO.FileInfo]::new($source)).Length -eq $sourceLength) 'replay artifact changed during packaging'
        $file=[IO.File]::Open($archivePath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
        try{
            $zip=[IO.Compression.ZipArchive]::new($file,[IO.Compression.ZipArchiveMode]::Read,$true)
            try{
                Assert-Correction ($zip.Entries.Count -eq 3) 'archive entry count'
                $entry=$zip.GetEntry($sourceEntry)
                Assert-Correction ($null -ne $entry -and $entry.Length -eq $sourceLength -and $null -ne $zip.GetEntry('SHA256-MANIFEST.json') -and $null -ne $zip.GetEntry('README.txt')) 'archive contents'
                $entryStream=$entry.Open()
                try{$sha=[Security.Cryptography.SHA256]::Create();try{$entryHash=([BitConverter]::ToString($sha.ComputeHash($entryStream))).Replace('-','')}finally{$sha.Dispose()}}finally{$entryStream.Dispose()}
                Assert-Correction ($entryHash -ieq $sourceSha) 'archive source bytes differ'
            }finally{$zip.Dispose()}
        }finally{$file.Dispose()}
        $archiveLength=([IO.FileInfo]::new($archivePath)).Length
        $archiveSha=Get-CorrectionSha $archivePath
        $manifest=[ordered]@{
            Schema='r1c4b-fixh-correction-package/v1';PackagingResult='PASS';AcceptanceClaim='NONE';SyntheticOnly=$SyntheticOnly
            OriginalBatchId=$batch;AuditorReportedResult=$result;EvidenceImplementationSHA=$evidenceSha;AuditorImplementationSHA=$auditorSha
            OriginalBytesPreserved=$true;Source=[ordered]@{Role='FIXH_CORRECTED_REPLAY';OriginalPath=$source;ArchivePath=$sourceEntry;Bytes=$sourceLength;SHA256=$sourceSha}
            ReferenceCompleteness=$references.ReferenceCompleteness;OriginalPlanPath=$references.OriginalPlanPath;OriginalPlanSHA256=$references.OriginalPlanSHA256
            OriginalControlBindings=$references.OriginalControlBindings;OriginalPackageBindings=$references.OriginalPackageBindings
            OriginalReportBinding=$artifact.OriginalReportBinding
            Package=[ordered]@{Path=$archivePath;Bytes=$archiveLength;SHA256=$archiveSha;Archive=$archiveName}
            PriorPackageFilesCopied=0;OriginalLogsCopied=0;ExecutableFilesCopied=0;UploadPerformed=$false
        }
        Write-CorrectionNew $manifestPath (Get-CorrectionUtf8 (($manifest|ConvertTo-Json -Depth 12)+"`n"))
        return [pscustomobject]@{PackagingResult='PASS';AcceptanceClaim='NONE';SyntheticOnly=$SyntheticOnly;OriginalBatchId=$batch;AuditorReportedResult=$result;PackagePath=$archivePath;PackageSHA256=$archiveSha;PackageBytes=$archiveLength;ManifestPath=$manifestPath;SourceSHA256=$sourceSha;SourceBytes=$sourceLength}
    }catch{
        if($created){
            $errorPath=Join-Path $output 'package-error.json'
            try{
                $failure=[ordered]@{Schema='r1c4b-fixh-correction-package-error/v1';PackagingResult='FAILED';AcceptanceClaim='NONE';SourcePath=$source;OutputDirectory=$output;Error=$_.Exception.Message;PartialPackageExists=[IO.File]::Exists($archivePath)}
                Write-CorrectionNew $errorPath (Get-CorrectionUtf8 (($failure|ConvertTo-Json -Depth 5)+"`n"))
            }catch{Write-Warning "Could not write package-error.json: $($_.Exception.Message)"}
        }
        throw
    }
}

function Invoke-FixHCorrectionPackageSelfTest {
    $batch=[guid]::NewGuid().ToString('N')
    $fixture=Join-Path $script:correctionRoot ("package-selftest-$batch")
    Assert-Correction (-not (Test-Path -LiteralPath $fixture)) 'synthetic fixture collision'
    $null=[IO.Directory]::CreateDirectory($fixture)
    $old=Join-Path $script:originalRoot $batch
    $plan=Join-Path $old 'plan.json'
    $sha=('A'*64)
    $names=@('plan.json','progress.jsonl','inventory.json','statistics.json','summary.json','finalization.json','prerequisites.json')
    $controls=@(foreach($name in $names){[ordered]@{Name=$name;Path=(Join-Path $old $name);SHA256=$sha}})
    $packages=@(1..3|ForEach-Object{[ordered]@{Name="old-zip-$_";Path=(Join-Path $old "package-$_.zip");SHA256=$sha;Bytes=100L}})
    $packages+=@([ordered]@{Name='old-package-manifest';Path=(Join-Path $old 'package-manifest.json');SHA256=$sha;Bytes=100L})
    $artifact=[ordered]@{
        Schema='r1c4b-fixh-corrected-replay/v1';Result='INVALID_EVIDENCE';OriginalBatchId=$batch
        EvidenceImplementationSHA='01633ff31776b42be9fc2383ed301d9884e80019';AuditorImplementationSHA=('B'*40)
        OriginalPlanPath=$plan;OriginalPlanSHA256=$sha;OriginalControlBindings=$controls;OriginalPackageBindings=$packages
        OriginalReportBinding=$null
        HistoricalCountersMatch=$false;CounterCorrections=@(1..8|ForEach-Object{[ordered]@{GroupNumber=$_;Synthetic=$true}})
    }
    $source=Join-Path $fixture 'synthetic-corrected-replay.json'
    Write-CorrectionNew $source (Get-CorrectionUtf8 (($artifact|ConvertTo-Json -Depth 12)+"`n"))
    $output=Join-Path $fixture 'new-package'
    $result=New-FixHCorrectionPackage $source $output $true
    Assert-Correction ($result.PackagingResult -ceq 'PASS' -and $result.AcceptanceClaim -ceq 'NONE' -and $result.AuditorReportedResult -ceq 'INVALID_EVIDENCE') 'synthetic result promoted to acceptance'
    $manifest=Get-Content -LiteralPath $result.ManifestPath -Raw -Encoding UTF8|ConvertFrom-Json
    Assert-Correction ($manifest.Source.SHA256 -ieq (Get-CorrectionSha $source) -and $manifest.Source.Bytes -eq ([IO.FileInfo]::new($source)).Length) 'synthetic source byte mapping'
    Assert-Correction ($manifest.Package.SHA256 -ieq (Get-CorrectionSha $result.PackagePath) -and $manifest.Package.Bytes -eq ([IO.FileInfo]::new($result.PackagePath)).Length) 'synthetic ZIP byte mapping'
    Assert-Correction ($manifest.OriginalPackageBindings.Count -eq 4 -and $manifest.PriorPackageFilesCopied -eq 0 -and $manifest.OriginalLogsCopied -eq 0) 'historical files included'
    $reusedRejected=$false
    try{$null=New-FixHCorrectionPackage $source $output $true}catch{$reusedRejected=$true}
    Assert-Correction $reusedRejected 'output reuse not rejected'
    $outsideRejected=$false
    try{$null=New-FixHCorrectionPackage (Join-Path $script:correctionRepo 'README.md') (Join-Path $fixture 'outside-reject') $true}catch{$outsideRejected=$true}
    Assert-Correction $outsideRejected 'out-of-scope source not rejected'
    $artifact.Result='SEALED'
    $badSource=Join-Path $fixture 'bad-result.json'
    Write-CorrectionNew $badSource (Get-CorrectionUtf8 (($artifact|ConvertTo-Json -Depth 12)+"`n"))
    $resultRejected=$false
    try{$null=New-FixHCorrectionPackage $badSource (Join-Path $fixture 'bad-result-package') $true}catch{$resultRejected=$true}
    Assert-Correction $resultRejected 'unapproved result not rejected'
    $artifact.Result='PASS';$artifact.OriginalControlBindings=@()
    $badPassSource=Join-Path $fixture 'incomplete-pass.json'
    Write-CorrectionNew $badPassSource (Get-CorrectionUtf8 (($artifact|ConvertTo-Json -Depth 12)+"`n"))
    $passRejected=$false
    try{$null=New-FixHCorrectionPackage $badPassSource (Join-Path $fixture 'incomplete-pass-package') $true}catch{$passRejected=$true}
    Assert-Correction $passRejected 'PASS without full historical bindings not rejected'
    $artifact.Result='INVALID_EVIDENCE';$artifact.OriginalPackageBindings=@();$artifact.AuditorImplementationSHA='NOT_AVAILABLE';$artifact.HistoricalCountersMatch=$null;$artifact.CounterCorrections=@()
    $partialSource=Join-Path $fixture 'partial-invalid.json'
    Write-CorrectionNew $partialSource (Get-CorrectionUtf8 (($artifact|ConvertTo-Json -Depth 12)+"`n"))
    $partial=New-FixHCorrectionPackage $partialSource (Join-Path $fixture 'partial-invalid-package') $true
    $partialManifest=Get-Content -LiteralPath $partial.ManifestPath -Raw -Encoding UTF8|ConvertFrom-Json
    Assert-Correction ($partialManifest.ReferenceCompleteness -ceq 'PARTIAL' -and $partialManifest.AuditorReportedResult -ceq 'INVALID_EVIDENCE' -and $partialManifest.AcceptanceClaim -ceq 'NONE') 'partial invalid replay not preserved honestly'
    return [pscustomobject]@{SelfTest='PASS';SyntheticOnly=$true;Checks=9;FixtureDirectory=$fixture;PackagePath=$result.PackagePath;SourceSHA256=$result.SourceSHA256;PackageSHA256=$result.PackageSHA256;Note='Synthetic file/ZIP test only; no PaneBind UAT or acceptance.'}
}

try {
    $answer=if($SelfTest){Invoke-FixHCorrectionPackageSelfTest}else{New-FixHCorrectionPackage $ReplayArtifactPath $OutputDirectory $false}
    $answer|ConvertTo-Json -Depth 8
    exit 0
}catch {
    Write-Error $_
    exit 1
}
