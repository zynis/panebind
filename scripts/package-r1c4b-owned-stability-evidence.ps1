[CmdletBinding(DefaultParameterSetName='Package')]
param(
    [Parameter(Mandatory=$true,ParameterSetName='Package')][string]$PlanPath,
    [Parameter(Mandatory=$true,ParameterSetName='Package')][string]$ProgressPath,
    [Parameter(Mandatory=$true,ParameterSetName='Package')][string]$InventoryPath,
    [Parameter(Mandatory=$true,ParameterSetName='Package')][string]$StatisticsPath,
    [Parameter(Mandatory=$true,ParameterSetName='Package')][string]$SummaryPath,
    [Parameter(Mandatory=$true,ParameterSetName='Package')][string]$PrerequisiteArtifactPath,
    [Parameter(Mandatory=$true,ParameterSetName='Package')][string]$OutputDirectory,
    [Parameter(ParameterSetName='Package')][ValidateRange(1,25)][int]$MaxPartMiB=25,
    [Parameter(Mandatory=$true,ParameterSetName='SelfTest')][switch]$SelfTest
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression
$script:packageRepository=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))

# Fix H local delivery only. This is not a validator, UAT runner or generic
# archive tool: only explicit final H files, recorded H attempts, and the
# prerequisite artifact's bounded file references are eligible.
function Assert-OwnedPackage([bool]$Pass,[string]$Reason){if(-not $Pass){throw [IO.InvalidDataException]::new("Fix H package: $Reason")}}
function Get-OwnedPackageField($Value,[string]$Name){
    $p=$Value.PSObject.Properties[$Name]
    Assert-OwnedPackage ($null -ne $p) "missing $Name"
    return $p.Value
}
function Get-OwnedPackageText($Value,[string]$Name){$v=Get-OwnedPackageField $Value $Name;Assert-OwnedPackage ($v -is [string] -and -not [string]::IsNullOrWhiteSpace($v)) "typed $Name";return $v}
function Get-OwnedPackageHash([string]$Path){return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash}
function Test-OwnedPackageBelow([string]$Path,[string]$Root){return $Path.StartsWith($Root.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)}
function Resolve-OwnedPackagePath([string]$Value){
    Assert-OwnedPackage (-not [string]::IsNullOrWhiteSpace($Value)) 'empty path'
    $p=if([IO.Path]::IsPathRooted($Value)){[IO.Path]::GetFullPath($Value)}else{[IO.Path]::GetFullPath((Join-Path $script:packageRepository $Value))}
    Assert-OwnedPackage (Test-OwnedPackageBelow $p $script:packageRepository) 'path outside repository'
    $current=$p
    while($current -and -not $current.Equals($script:packageRepository,[StringComparison]::OrdinalIgnoreCase)){
        if(Test-Path -LiteralPath $current){Assert-OwnedPackage (([IO.File]::GetAttributes($current) -band [IO.FileAttributes]::ReparsePoint) -eq 0) 'reparse-point path is not eligible'}
        $current=[IO.Path]::GetDirectoryName($current)
    }
    Assert-OwnedPackage ($null -ne $current -and $current.Equals($script:packageRepository,[StringComparison]::OrdinalIgnoreCase)) 'unresolved repository ancestry'
    return $p
}
function Read-OwnedPackageJson([string]$Path){
    Assert-OwnedPackage ([IO.File]::Exists($Path)) 'required final artifact missing'
    return ([IO.File]::ReadAllText($Path,[Text.Encoding]::UTF8)|ConvertFrom-Json -ErrorAction Stop)
}
function Read-OwnedPackageOptionalJson([string]$Path,[string]$Role,$Problems){
    if(-not [IO.File]::Exists($Path)){$Problems.Add([pscustomobject]@{Role=$Role;OriginalPath=$Path;Status='MISSING';Error='RECORDED_FINAL_ARTIFACT_NOT_AVAILABLE'});return $null}
    try{return Read-OwnedPackageJson $Path}catch{$Problems.Add([pscustomobject]@{Role=$Role;OriginalPath=$Path;Status='INVALID_JSON';Error=$_.Exception.Message});return $null}
}
function Write-OwnedPackageNewBytes([string]$Path,[byte[]]$Bytes){
    $stream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try{$stream.Write($Bytes,0,$Bytes.Length)}finally{$stream.Dispose()}
}
function Get-OwnedPackageUtf8([string]$Text){return [Text.UTF8Encoding]::new($false).GetBytes($Text)}
function Add-OwnedPackageSource($Sources,$Groups,[string]$Group,[string]$Role,[string]$Path,[string]$ArchivePath,[string]$ExpectedHash){
    $p=Resolve-OwnedPackagePath $Path
    Assert-OwnedPackage ([IO.Path]::GetExtension($p).ToLowerInvariant() -in @('.json','.jsonl')) 'only original JSON/JSONL evidence is eligible'
    Assert-OwnedPackage ([IO.File]::Exists($p)) "missing required evidence: $p"
    Assert-OwnedPackage ($ArchivePath -notmatch '(^/|\\|(^|/)\.\.(/|$)|:)' -and -not [string]::IsNullOrWhiteSpace($ArchivePath)) 'unsafe archive entry name'
    Assert-OwnedPackage (@($Sources|Where-Object {$_.OriginalPath -ieq $p -or $_.ArchivePath -ceq $ArchivePath}).Count -eq 0) 'duplicate source/archive mapping'
    $hash=Get-OwnedPackageHash $p
    if($ExpectedHash){Assert-OwnedPackage ($ExpectedHash -cmatch '^[0-9A-Fa-f]{64}$' -and $hash -ieq $ExpectedHash) "recorded hash mismatch: $p"}
    $row=[pscustomobject]@{Role=$Role;OriginalPath=$p;ArchivePath=$ArchivePath;SHA256=$hash;Bytes=[long]([IO.FileInfo]::new($p).Length);Group=$Group;Part=$null}
    $Sources.Add($row)
    if(-not $Groups.Contains($Group)){$Groups[$Group]=[Collections.Generic.List[object]]::new()}
    $Groups[$Group].Add($row)
}
function Copy-OwnedPackageEntry($Archive,$Source){
    $entry=$Archive.CreateEntry($Source.ArchivePath,[IO.Compression.CompressionLevel]::Optimal)
    # FileShare.Read prevents writes/deletes while copying and hashing this
    # original. An earlier-to-open race is detected by the recorded SHA256.
    $sourceStream=[IO.File]::Open($Source.OriginalPath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    $output=$entry.Open();$hash=[Security.Cryptography.SHA256]::Create()
    try{
        Assert-OwnedPackage ($sourceStream.Length -eq $Source.Bytes) 'source size changed before copy'
        $sourceStream.CopyTo($output);$sourceStream.Position=0
        $actual=[BitConverter]::ToString($hash.ComputeHash($sourceStream)).Replace('-','')
        Assert-OwnedPackage ($actual -ceq $Source.SHA256) 'source bytes changed before/during copy'
    }finally{$hash.Dispose();$output.Dispose();$sourceStream.Dispose()}
}
function Add-OwnedPackageGeneratedEntry($Archive,[string]$Name,[byte[]]$Bytes){
    $entry=$Archive.CreateEntry($Name,[IO.Compression.CompressionLevel]::Optimal);$stream=$entry.Open()
    try{$stream.Write($Bytes,0,$Bytes.Length)}finally{$stream.Dispose()}
}
function Test-OwnedPackageZip([string]$Path,$Sources){
    $stream=[IO.File]::OpenRead($Path);$zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Read,$false)
    try{
        Assert-OwnedPackage ($zip.Entries.Count -eq $Sources.Count+2) 'archive entry inventory differs'
        foreach($source in $Sources){
            $entry=$zip.GetEntry($source.ArchivePath);Assert-OwnedPackage ($null -ne $entry -and $entry.Length -eq $source.Bytes) 'archive source mapping/size differs'
            $entryStream=$entry.Open();$hash=[Security.Cryptography.SHA256]::Create()
            try{$actual=[BitConverter]::ToString($hash.ComputeHash($entryStream)).Replace('-','');Assert-OwnedPackage ($actual -ceq $source.SHA256) 'archived bytes differ from original'}finally{$hash.Dispose();$entryStream.Dispose()}
        }
        Assert-OwnedPackage ($null -ne $zip.GetEntry('SHA256-MANIFEST.json') -and $null -ne $zip.GetEntry('README.txt')) 'archive review instructions/manifest missing'
    }finally{$zip.Dispose();$stream.Dispose()}
}
function New-OwnedStabilityEvidencePackage([string]$Plan,[string]$Progress,[string]$Inventory,[string]$Statistics,[string]$Summary,[string]$Prerequisite,[string]$Destination,[int]$PartMiB=25,[bool]$SyntheticOnly=$false){
    $hRoot=[IO.Path]::GetFullPath((Join-Path $script:packageRepository 'uat/r1c4b-fixh'))
    $gRoot=[IO.Path]::GetFullPath((Join-Path $script:packageRepository 'uat/r1c4b-fixg'))
    $fRoot=[IO.Path]::GetFullPath((Join-Path $script:packageRepository 'uat/r1c4b-fixf'))
    $out=Resolve-OwnedPackagePath $Destination
    Assert-OwnedPackage (Test-OwnedPackageBelow $out $hRoot) 'output must be a new ignored Fix H directory'
    Assert-OwnedPackage (-not (Test-Path -LiteralPath $out)) 'output already exists; never overwrite or reuse a package'
    $control=[ordered]@{Plan=(Resolve-OwnedPackagePath $Plan);Progress=(Resolve-OwnedPackagePath $Progress);Inventory=(Resolve-OwnedPackagePath $Inventory);Statistics=(Resolve-OwnedPackagePath $Statistics);Summary=(Resolve-OwnedPackagePath $Summary);Prerequisite=(Resolve-OwnedPackagePath $Prerequisite)}
    $receiptPath=Resolve-OwnedPackagePath (Join-Path ([IO.Path]::GetDirectoryName($control.Inventory)) 'finalization.json')
    $control.Finalization=$receiptPath
    foreach($p in $control.Values){Assert-OwnedPackage (Test-OwnedPackageBelow $p $hRoot) 'explicit final control artifact must be in Fix H ignored scope'}
    $problems=[Collections.Generic.List[object]]::new();$incomplete=[Collections.Generic.List[object]]::new()
    $planObject=Read-OwnedPackageJson $control.Plan;$prerequisiteObject=Read-OwnedPackageJson $control.Prerequisite
    $inventoryObject=Read-OwnedPackageOptionalJson $control.Inventory 'Inventory' $problems;$statisticsObject=Read-OwnedPackageOptionalJson $control.Statistics 'Statistics' $problems;$summaryObject=Read-OwnedPackageOptionalJson $control.Summary 'Summary' $problems
    $receipt=$null;if([IO.File]::Exists($receiptPath)){$receipt=Read-OwnedPackageOptionalJson $receiptPath 'Finalization' $problems}
    foreach($pair in @(@($planObject,'r1c4b-owned-stability-plan/v1'),@($prerequisiteObject,'r1c4b-fixh-prerequisites/v1'))){Assert-OwnedPackage ((Get-OwnedPackageText $pair[0] 'Schema') -ceq $pair[1]) 'unexpected required artifact schema'}
    $batch=Get-OwnedPackageText $planObject 'BatchId';$implementation=Get-OwnedPackageText $planObject 'ImplementationSHA'
    Assert-OwnedPackage ($implementation -cmatch '^[0-9a-fA-F]{40}$') 'implementation SHA format'
    foreach($pair in @(@($inventoryObject,'r1c4b-owned-stability-inventory/v1'),@($statisticsObject,'r1c4b-owned-stability-batch-statistics/v1'),@($summaryObject,'r1c4b-owned-stability-summary/v1'),@($receipt,'r1c4b-owned-stability-finalization/v1'))){if($null -ne $pair[0]){Assert-OwnedPackage ((Get-OwnedPackageText $pair[0] 'Schema') -ceq $pair[1] -and (Get-OwnedPackageText $pair[0] 'BatchId') -ceq $batch) 'present control artifact belongs to another schema/batch'}}
    $planHash=Get-OwnedPackageHash $control.Plan
    if($null -ne $inventoryObject){
        Assert-OwnedPackage ((Get-OwnedPackageText $inventoryObject 'ImplementationSHA') -ceq $implementation) 'plan/inventory implementation mismatch'
        Assert-OwnedPackage ((Resolve-OwnedPackagePath (Get-OwnedPackageText $inventoryObject 'PlanPath')) -ieq $control.Plan -and (Get-OwnedPackageText $inventoryObject 'PlanSHA256') -ieq $planHash -and (Resolve-OwnedPackagePath (Get-OwnedPackageText $inventoryObject 'ProgressPath')) -ieq $control.Progress) 'frozen plan/progress reference mismatch'
        Assert-OwnedPackage ((Get-OwnedPackageText $inventoryObject 'LoopResult') -cin @('ALL_PLANNED_PASS','STOPPED','NOT_RUN')) 'inventory is not terminal'
    }
    if($null -ne $receipt){
        Assert-OwnedPackage ((Get-OwnedPackageText $receipt 'ImplementationSHA') -ceq $implementation -and (Resolve-OwnedPackagePath (Get-OwnedPackageText $receipt 'InventoryPath')) -ieq $control.Inventory -and (Resolve-OwnedPackagePath (Get-OwnedPackageText $receipt 'StatisticsPath')) -ieq $control.Statistics -and (Resolve-OwnedPackagePath (Get-OwnedPackageText $receipt 'SummaryPath')) -ieq $control.Summary) 'finalization receipt bindings differ'
    }
    $batchResult='INVALID_EVIDENCE';$independent='NOT_AVAILABLE'
    if($null -ne $summaryObject){$batchResult=Get-OwnedPackageText $summaryObject 'BatchResult';$independent=Get-OwnedPackageText $summaryObject 'IndependentSummaryResult';Assert-OwnedPackage ($batchResult -cin @('PASS','BLOCKED','FAIL','INVALID_EVIDENCE') -and $independent -cin @('PASS','NOT_PASSED','INVALID_EVIDENCE')) 'independent summary is not final'}
    Assert-OwnedPackage ((Get-OwnedPackageText $prerequisiteObject 'Result') -ceq 'PASS') 'G prerequisites were not verified'
    $progressRecords=[Collections.Generic.List[object]]::new()
    if([IO.File]::Exists($control.Progress)){
        [long]$lineNumber=0
        foreach($line in [IO.File]::ReadAllLines($control.Progress,[Text.Encoding]::UTF8)){
            ++$lineNumber
            try{
                Assert-OwnedPackage (-not [string]::IsNullOrWhiteSpace($line)) 'blank progress record'
                $record=$line|ConvertFrom-Json -ErrorAction Stop
                Assert-OwnedPackage ((Get-OwnedPackageText $record 'Schema') -ceq 'r1c4b-owned-stability-progress/v1' -and (Get-OwnedPackageText $record 'Type') -cin @('plan_frozen','attempt_started','attempt_finished') -and (Get-OwnedPackageText $record 'PlanSHA256') -ieq $planHash) 'progress is not the fixed producer/frozen plan'
                $progressRecords.Add($record)
            }catch{$problems.Add([pscustomobject]@{Role='Progress';OriginalPath=$control.Progress;Status='INVALID_RECORD';Line=$lineNumber;Error=$_.Exception.Message})}
        }
    }else{$problems.Add([pscustomobject]@{Role='Progress';OriginalPath=$control.Progress;Status='MISSING';Error='RECORDED_FINAL_ARTIFACT_NOT_AVAILABLE'})}
    $inventorySource='FINAL_INVENTORY';$attempts=@()
    if($null -ne $inventoryObject){$attempts=@(Get-OwnedPackageField $inventoryObject 'Attempts')}
    else{
        # The only fallback is the explicit producer's finished records. No
        # directory search, guessed run path or fabricated replacement file.
        Assert-OwnedPackage ($null -ne $receipt -and @($progressRecords|Where-Object Type -ceq 'plan_frozen').Count -eq 1) 'missing inventory has no finalization/frozen producer evidence'
        $inventorySource='PROGRESS_FIXED_PRODUCER';$attempts=@($progressRecords|Where-Object Type -ceq 'attempt_finished'|ForEach-Object {Get-OwnedPackageField $_ 'Attempt'})
        foreach($start in @($progressRecords|Where-Object Type -ceq 'attempt_started')){
            $a=Get-OwnedPackageField $start 'Attempt';$id=Get-OwnedPackageText $a 'RunId'
            if(@($attempts|Where-Object RunId -ceq $id).Count -eq 0){$incomplete.Add([pscustomobject]@{Number=(Get-OwnedPackageField $a 'Number');RunId=$id;Reason='STARTED_WITHOUT_FINISHED_PRODUCER_RECORD';RecordedAttempt=$a;ArtifactsNotInferred=$true})}
        }
    }
    $sources=[Collections.Generic.List[object]]::new();$groups=[ordered]@{};$missing=[Collections.Generic.List[object]]::new();$runs=[Collections.Generic.List[object]]::new()
    foreach($name in $control.Keys){if([IO.File]::Exists($control[$name])){Add-OwnedPackageSource $sources $groups 'control-and-prerequisites' ('FIXH_'+$name.ToUpperInvariant()) $control[$name] ('fixh/'+$name.ToLowerInvariant()+'/'+[IO.Path]::GetFileName($control[$name])) ''}}
    $prerequisiteFiles=@(Get-OwnedPackageField $prerequisiteObject 'Files')
    Assert-OwnedPackage ($prerequisiteFiles.Count -ge 34 -and $prerequisiteFiles.Count -le 36) 'bounded G prerequisite file list'
    $prerequisiteKinds=@('FIXG_INVENTORY','FIXF_FROZEN_REPLAY','FIXG_METADATA','FIXG_PROBE','FIXG_PRE','FIXG_POST','FIXF_PREFIX_SUPPORT')
    foreach($pair in @(@('FIXG_INVENTORY',1),@('FIXF_FROZEN_REPLAY',1),@('FIXG_METADATA',8),@('FIXG_PROBE',8),@('FIXG_PRE',8),@('FIXG_POST',8))){Assert-OwnedPackage (@($prerequisiteFiles|Where-Object Kind -ceq $pair[0]).Count -eq $pair[1]) 'prerequisite file roles/counts differ from the G eight-run proof'}
    Assert-OwnedPackage (@($prerequisiteFiles|Where-Object Kind -ceq 'FIXF_PREFIX_SUPPORT').Count -in @(0,2)) 'incomplete optional F prefix support'
    foreach($file in $prerequisiteFiles){
        $kind=Get-OwnedPackageText $file 'Kind';$p=Resolve-OwnedPackagePath (Get-OwnedPackageText $file 'Path')
        $hash=Get-OwnedPackageText $file 'SHA256'
        Assert-OwnedPackage ($kind -cin $prerequisiteKinds) 'unapproved prerequisite file role'
        $group='control-and-prerequisites'
        if(Test-OwnedPackageBelow $p $gRoot){
            Assert-OwnedPackage ($kind -cne 'FIXF_PREFIX_SUPPORT') 'F support mislabeled as G'
            $relative=$p.Substring($gRoot.Length+1).Replace('\','/');$archive='prerequisites/fixg/'+$relative
            if($kind -cin @('FIXG_METADATA','FIXG_PROBE','FIXG_PRE','FIXG_POST')){
                $gNames=@{FIXG_METADATA='run.metadata.json';FIXG_PROBE='probe.jsonl';FIXG_PRE='environment-pre.json';FIXG_POST='environment-post.json'}
                Assert-OwnedPackage ([IO.Path]::GetFileName($p) -ceq $gNames[$kind]) 'G prerequisite is not an original runner artifact'
                $group='prerequisite-g-run-'+[IO.Path]::GetFileName([IO.Path]::GetDirectoryName($p))
            }
        }
        elseif(Test-OwnedPackageBelow $p $fRoot){
            Assert-OwnedPackage ($kind -ceq 'FIXF_PREFIX_SUPPORT') 'unexpected F prerequisite role'
            $relative=$p.Substring($fRoot.Length+1).Replace('\','/')
            Assert-OwnedPackage ($relative -cin @('20260927T132106696Z-Debug-Move-controlled-abort-c2473a89d3db4c1da41ceb0ba51b1cb6/probe.jsonl','20260927T132106696Z-Debug-Move-controlled-abort-c2473a89d3db4c1da41ceb0ba51b1cb6/environment-post.json')) 'unrelated F/history file is not eligible'
            $archive='prerequisites/fixf/'+$relative
            $group='prerequisite-f-prefix-support'
        }else{throw [IO.InvalidDataException]::new('Fix H package: prerequisite path outside bounded G/F scope')}
        $bytes=Get-OwnedPackageField $file 'Bytes';Assert-OwnedPackage (($bytes -is [int] -or $bytes -is [long]) -and $bytes -ge 0 -and $bytes -eq [IO.FileInfo]::new($p).Length) 'prerequisite original byte length differs'
        Add-OwnedPackageSource $sources $groups $group $kind $p $archive $hash
    }
    foreach($group in @($groups.Keys|Where-Object {$_ -clike 'prerequisite-g-run-*'})){Assert-OwnedPackage ($groups[$group].Count -eq 4) 'G original run may not be incomplete or split'}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase);[long]$number=0
    foreach($attempt in $attempts){
        ++$number;Assert-OwnedPackage ($attempt.Number -is [int] -or $attempt.Number -is [long]) 'typed attempt number'
        Assert-OwnedPackage ($attempt.Number -eq $number) 'attempts are not contiguous; packaging cannot discard a failure'
        $runId=Get-OwnedPackageText $attempt 'RunId';$guid=[guid]::Empty;Assert-OwnedPackage ([guid]::TryParse($runId,[ref]$guid) -and $seen.Add($runId)) 'missing/duplicate RunId'
        Assert-OwnedPackage ((Get-OwnedPackageText $attempt 'Mode') -ceq 'normal') 'non-normal H attempt'
        $artifacts=@(Get-OwnedPackageField $attempt 'Artifacts');$runGroup='attempt-'+$number.ToString('D3')+'-'+$guid.ToString('N')
        if($artifacts.Count -eq 0){
            Assert-OwnedPackage ((Get-OwnedPackageText $attempt 'Result') -cne 'PASS') 'successful attempt cannot have no artifact references'
            $missing.Add([pscustomobject]@{Attempt=$number;RunId=$runId;Kind='ALL_RUN_ARTIFACT_REFERENCES';OriginalPath=$null;Reason='ARTIFACT_REFERENCES_NOT_AVAILABLE';Included=$false;RecordedAttempt=$attempt;ArtifactsNotInferred=$true})
            $runs.Add([pscustomobject]@{Number=$number;RunId=$runId;Result=(Get-OwnedPackageText $attempt 'Result');SourceDirectory=$null;Group=$runGroup;RecordedAttempt=$attempt;ArtifactsNotInferred=$true})
            continue
        }
        Assert-OwnedPackage ($artifacts.Count -eq 4) 'known run requires four existing/missing artifact declarations'
        $parent=$null;$fileNames=@{Metadata='run.metadata.json';Probe='probe.jsonl';Pre='environment-pre.json';Post='environment-post.json'}
        foreach($kind in @('Metadata','Probe','Pre','Post')){
            $refs=@($artifacts|Where-Object Kind -ceq $kind);Assert-OwnedPackage ($refs.Count -eq 1) 'missing/duplicate artifact kind'
            $file=$refs[0];$p=Resolve-OwnedPackagePath (Get-OwnedPackageText $file 'Path')
            Assert-OwnedPackage ((Test-OwnedPackageBelow $p $gRoot) -and [IO.Path]::GetFileName($p) -ceq $fileNames[$kind]) 'attempt artifact outside exact runner file scope'
            $directory=[IO.Path]::GetDirectoryName($p)
            if($null -eq $parent){$parent=$directory}else{Assert-OwnedPackage ($parent -ieq $directory) 'one run is split across source directories'}
            $leaf=[IO.Path]::GetFileName($directory)
            Assert-OwnedPackage ($leaf.EndsWith($guid.ToString('N'),[StringComparison]::OrdinalIgnoreCase) -or $leaf.EndsWith($guid.ToString('D'),[StringComparison]::OrdinalIgnoreCase)) 'run directory does not bind RunId'
            $exists=Get-OwnedPackageField $file 'Exists';Assert-OwnedPackage ($exists -is [bool]) 'typed artifact Exists'
            Assert-OwnedPackage ($exists -eq [IO.File]::Exists($p)) 'recorded existence differs; preserve missing files instead of inventing them'
            if($exists){Add-OwnedPackageSource $sources $groups $runGroup ('FIXH_'+$kind.ToUpperInvariant()) $p ('fixh/attempts/'+$runGroup+'/'+$fileNames[$kind]) (Get-OwnedPackageText $file 'SHA256')}
            else{Assert-OwnedPackage ($null -eq (Get-OwnedPackageField $file 'SHA256')) 'missing artifact has a fabricated hash';$missing.Add([pscustomobject]@{Attempt=$number;RunId=$runId;Kind=$kind;OriginalPath=$p;Reason='RECORDED_MISSING';Included=$false})}
        }
        $runs.Add([pscustomobject]@{Number=$number;RunId=$runId;Result=(Get-OwnedPackageText $attempt 'Result');SourceDirectory=$parent;Group=$runGroup})
    }
    $target=[long]$PartMiB*1024*1024;$budget=$target-131072
    $parts=[Collections.Generic.List[object]]::new();$partFiles=[Collections.Generic.List[object]]::new();[long]$partBytes=0
    foreach($group in $groups.Keys){
        [long]$bytes=0;foreach($file in $groups[$group]){$bytes+=$file.Bytes}
        if($partFiles.Count -gt 0 -and $partBytes+$bytes -gt $budget){$parts.Add(@($partFiles.ToArray()));$partFiles=[Collections.Generic.List[object]]::new();$partBytes=0}
        foreach($file in $groups[$group]){$partFiles.Add($file)};$partBytes+=$bytes
    }
    if($partFiles.Count){$parts.Add(@($partFiles.ToArray()))}
    Assert-OwnedPackage ($parts.Count -gt 0) 'empty package'
    [IO.Directory]::CreateDirectory($out)|Out-Null;$null=Resolve-OwnedPackagePath $out
    $packages=[Collections.Generic.List[object]]::new();$created=[Collections.Generic.List[string]]::new()
    $description="PaneBind Fix H local evidence package. Original JSON/JSONL bytes are unchanged.`nBatch: $batch`nImplementation: $implementation`nFinal summary result (INVALID_EVIDENCE when unavailable): $batchResult`nAttempt reference source: $inventorySource`nSyntheticOnly: $SyntheticOnly`nPackaging does not validate or upgrade any UAT/architecture result.`nAll recorded H attempts, including failures, missing-file declarations and unfinished progress records, are represented. Missing final controls are not recreated.`nSource/binary identities remain references inside the original artifacts; EXE/source/screenshots/unrelated uat are not included.`nEach H run is indivisible. Partitioning uses conservative uncompressed sizes; an oversized indivisible group is explicitly reported.`nSHA256-MANIFEST.json in each ZIP maps absolute original paths to entry paths and hashes. The external package-manifest.json additionally lists every ZIP size/hash; it is external to avoid a self-hash cycle.`nNo upload or Git operation is performed. Absolute paths inside original records are not rewritten.`n"
    try{
        for($i=0;$i -lt $parts.Count;++$i){
            $name='fixh-evidence-'+([string]($i+1)).PadLeft(3,'0')+'.zip';$path=Join-Path $out $name;$files=@($parts[$i])
            foreach($file in $files){$file.Part=$name}
            $inside=[pscustomobject]@{Schema='r1c4b-owned-stability-archive-content/v1';BatchId=$batch;ImplementationSHA=$implementation;SyntheticOnly=$SyntheticOnly;Part=$name;OriginalBytesPreserved=$true;PackageInventorySource=$inventorySource;Files=$files;MissingArtifacts=@($missing.ToArray());ControlProblems=@($problems.ToArray());IncompleteAttempts=@($incomplete.ToArray());AcceptanceClaim='NONE'}
            $stream=[IO.File]::Open($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None);$created.Add($path)
            $zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create,$false)
            try{foreach($file in $files){Copy-OwnedPackageEntry $zip $file};Add-OwnedPackageGeneratedEntry $zip 'SHA256-MANIFEST.json' (Get-OwnedPackageUtf8 ($inside|ConvertTo-Json -Depth 12));Add-OwnedPackageGeneratedEntry $zip 'README.txt' (Get-OwnedPackageUtf8 $description)}finally{$zip.Dispose();$stream.Dispose()}
            Test-OwnedPackageZip $path $files
            $size=[long]([IO.FileInfo]::new($path).Length)
            $packages.Add([pscustomobject]@{Path=$path;Archive=$name;Bytes=$size;SHA256=(Get-OwnedPackageHash $path);TargetBytes=$target;OverTarget=($size -gt $target);SourceGroups=@($files|ForEach-Object Group|Select-Object -Unique);FileCount=$files.Count})
        }
        foreach($source in $sources){Assert-OwnedPackage ((Get-OwnedPackageHash $source.OriginalPath) -ceq $source.SHA256) 'original changed after packaging'}
        $manifest=[pscustomobject]@{Schema='r1c4b-owned-stability-evidence-package/v1';PackagingResult='PASS';DeliveryCompleteness=$(if($problems.Count -or $missing.Count -or $incomplete.Count){'PARTIAL'}else{'COMPLETE'});BatchId=$batch;ImplementationSHA=$implementation;BatchResult=$batchResult;IndependentSummaryResult=$independent;SyntheticOnly=$SyntheticOnly;OriginalBytesPreserved=$true;PackageInventorySource=$inventorySource;ControlProblems=@($problems.ToArray());IncompleteAttempts=@($incomplete.ToArray());CreatedUtc=[DateTime]::UtcNow.ToString('o');TargetPartBytes=$target;FinishedAttemptDeclarations=$runs.Count;Runs=@($runs.ToArray());MissingArtifacts=@($missing.ToArray());Files=@($sources.ToArray());Packages=@($packages.ToArray());AcceptanceClaim='NONE';UploadPerformed=$false}
        $manifestPath=Join-Path $out 'package-manifest.json';Write-OwnedPackageNewBytes $manifestPath (Get-OwnedPackageUtf8 ($manifest|ConvertTo-Json -Depth 12))
        Write-OwnedPackageNewBytes (Join-Path $out 'README.txt') (Get-OwnedPackageUtf8 $description)
        return [pscustomobject]@{PackagingResult='PASS';DeliveryCompleteness=$manifest.DeliveryCompleteness;PackageInventorySource=$inventorySource;BatchId=$batch;BatchResult=$batchResult;SyntheticOnly=$SyntheticOnly;OutputDirectory=$out;ManifestPath=$manifestPath;ManifestSHA256=(Get-OwnedPackageHash $manifestPath);Packages=@($packages.ToArray());OriginalFileCount=$sources.Count;MissingArtifacts=@($missing.ToArray());ControlProblems=@($problems.ToArray());IncompleteAttempts=@($incomplete.ToArray())}
    }catch{
        # Never delete partial delivery or overwrite originals. A package error
        # is not a reason to re-run any GUI gesture or relabel the batch.
        $failure=[pscustomobject]@{Schema='r1c4b-owned-stability-package-error/v1';PackagingResult='FAILED';BatchId=$batch;Error=$_.Exception.Message;PartialPaths=@($created.ToArray());SyntheticOnly=$SyntheticOnly;AcceptanceClaim='NONE'}
        try{Write-OwnedPackageNewBytes (Join-Path $out 'package-error.json') (Get-OwnedPackageUtf8 ($failure|ConvertTo-Json -Depth 8))}catch{}
        throw
    }
}
function Invoke-OwnedPackageSelfTest{
    # Only fresh synthetic files under ignored folders. No historical run or
    # unfinished H batch is read; these bytes are not empirical UAT evidence.
    $id=[guid]::NewGuid().ToString('N');$root=Join-Path $script:packageRepository ('uat/r1c4b-fixh/package-selftest-'+$id);[IO.Directory]::CreateDirectory($root)|Out-Null
    $plan=Join-Path $root 'plan.json';$progress=Join-Path $root 'progress.jsonl';$inventory=Join-Path $root 'inventory.json';$statistics=Join-Path $root 'statistics.json';$summary=Join-Path $root 'summary.json';$prerequisite=Join-Path $root 'prerequisites.json'
    $batch='synthetic-'+$id;$sha='1111111111111111111111111111111111111111'
    Write-OwnedPackageNewBytes $plan (Get-OwnedPackageUtf8 (@{Schema='r1c4b-owned-stability-plan/v1';BatchId=$batch;ImplementationSHA=$sha;SyntheticOnly=$true}|ConvertTo-Json))
    Write-OwnedPackageNewBytes $statistics (Get-OwnedPackageUtf8 (@{Schema='r1c4b-owned-stability-batch-statistics/v1';BatchId=$batch;SyntheticOnly=$true}|ConvertTo-Json))
    Write-OwnedPackageNewBytes $summary (Get-OwnedPackageUtf8 (@{Schema='r1c4b-owned-stability-summary/v1';BatchId=$batch;BatchResult='INVALID_EVIDENCE';IndependentSummaryResult='NOT_PASSED';SyntheticOnly=$true}|ConvertTo-Json))
    $g=Join-Path $script:packageRepository ('uat/r1c4b-fixg/package-selftest-prerequisites-'+$id);[IO.Directory]::CreateDirectory($g)|Out-Null;$refs=@()
    foreach($kind in @('FIXG_INVENTORY','FIXF_FROZEN_REPLAY')){$p=Join-Path $g ($kind+'.json');Write-OwnedPackageNewBytes $p (Get-OwnedPackageUtf8 '{"synthetic_only":true}');$refs+=[pscustomobject]@{Kind=$kind;Path=$p;SHA256=(Get-OwnedPackageHash $p);Bytes=[long]([IO.FileInfo]::new($p).Length)}}
    $gNames=@{FIXG_METADATA='run.metadata.json';FIXG_PROBE='probe.jsonl';FIXG_PRE='environment-pre.json';FIXG_POST='environment-post.json'}
    for($i=1;$i -le 8;++$i){$dir=Join-Path $g ('synthetic-run-'+$i);[IO.Directory]::CreateDirectory($dir)|Out-Null;foreach($kind in @('FIXG_METADATA','FIXG_PROBE','FIXG_PRE','FIXG_POST')){$p=Join-Path $dir $gNames[$kind];Write-OwnedPackageNewBytes $p (Get-OwnedPackageUtf8 '{"synthetic_only":true}');$refs+=[pscustomobject]@{Kind=$kind;Path=$p;SHA256=(Get-OwnedPackageHash $p);Bytes=[long]([IO.FileInfo]::new($p).Length)}}}
    Write-OwnedPackageNewBytes $prerequisite (Get-OwnedPackageUtf8 (@{Schema='r1c4b-fixh-prerequisites/v1';Result='PASS';SyntheticOnly=$true;Files=$refs}|ConvertTo-Json -Depth 6))
    $attempts=@();$names=@{Metadata='run.metadata.json';Probe='probe.jsonl';Pre='environment-pre.json';Post='environment-post.json'}
    for($n=1;$n -le 2;++$n){
        $run=[guid]::NewGuid().ToString('N');$dir=Join-Path $script:packageRepository ('uat/r1c4b-fixg/package-selftest-'+$id+'-'+$run);[IO.Directory]::CreateDirectory($dir)|Out-Null;$artifacts=@()
        foreach($kind in @('Metadata','Probe','Pre','Post')){$p=Join-Path $dir $names[$kind];$exists=-not ($n -eq 2 -and $kind -eq 'Post');if($exists){$data=if($kind -ceq 'Probe'){('{"synthetic_only":true,"payload":"'+('a'*600000)+'"}'+"`n")}else{'{"synthetic_only":true}'};Write-OwnedPackageNewBytes $p (Get-OwnedPackageUtf8 $data)};$artifacts+=[pscustomobject]@{Kind=$kind;Path=$p;Exists=$exists;SHA256=$(if($exists){Get-OwnedPackageHash $p}else{$null})}}
        $attempts+=[pscustomobject]@{Number=$n;RunId=$run;Mode='normal';Result=$(if($n -eq 1){'PASS'}else{'INVALID_EVIDENCE'});Artifacts=$artifacts}
    }
    $attempts+=[pscustomobject]@{Number=3;RunId=[guid]::NewGuid().ToString('N');Mode='normal';Result='INVALID_EVIDENCE';Artifacts=@();ChildInvoked=$false;ChildOutput=@('synthetic child was not invoked');Error='synthetic checkpoint failure'}
    $progressLines=@((@{Schema='r1c4b-owned-stability-progress/v1';Type='plan_frozen';PlanSHA256=(Get-OwnedPackageHash $plan);Attempt=$null;SyntheticOnly=$true}|ConvertTo-Json -Depth 8 -Compress))
    foreach($attempt in $attempts){foreach($type in @('attempt_started','attempt_finished')){$progressLines+=(@{Schema='r1c4b-owned-stability-progress/v1';Type=$type;PlanSHA256=(Get-OwnedPackageHash $plan);Attempt=$attempt;SyntheticOnly=$true}|ConvertTo-Json -Depth 8 -Compress)}}
    $progressLines+=(@{Schema='r1c4b-owned-stability-progress/v1';Type='attempt_started';PlanSHA256=(Get-OwnedPackageHash $plan);Attempt=@{Number=4;RunId=[guid]::NewGuid().ToString('N');Mode='normal';Result='NOT_RUN';Artifacts=@()};SyntheticOnly=$true}|ConvertTo-Json -Depth 8 -Compress)
    Write-OwnedPackageNewBytes $progress (Get-OwnedPackageUtf8 (($progressLines -join "`n")+"`n"))
    Write-OwnedPackageNewBytes $inventory (Get-OwnedPackageUtf8 (@{Schema='r1c4b-owned-stability-inventory/v1';BatchId=$batch;ImplementationSHA=$sha;PlanPath=$plan;PlanSHA256=(Get-OwnedPackageHash $plan);ProgressPath=$progress;LoopResult='STOPPED';BatchResult='INVALID_EVIDENCE';Attempts=$attempts;SyntheticOnly=$true}|ConvertTo-Json -Depth 8))
    Write-OwnedPackageNewBytes (Join-Path $root 'finalization.json') (Get-OwnedPackageUtf8 (@{Schema='r1c4b-owned-stability-finalization/v1';BatchId=$batch;ImplementationSHA=$sha;InventoryPath=$inventory;StatisticsPath=$statistics;SummaryPath=$summary;InventoryWriteStatus='PASS';InventoryWriteError=$null;StatisticsError=$null;SummaryError=$null;SummaryExitCode=2;SummaryExists=$true;AggregateExitCode=2;SyntheticOnly=$true}|ConvertTo-Json))
    $output=Join-Path $root 'package';$result=New-OwnedStabilityEvidencePackage $plan $progress $inventory $statistics $summary $prerequisite $output 1 $true
    Assert-OwnedPackage ($result.SyntheticOnly -and $result.Packages.Count -ge 2 -and $result.MissingArtifacts.Count -eq 2 -and $result.BatchResult -ceq 'INVALID_EVIDENCE') 'synthetic failure/missing/split round trip'
    $manifest=Read-OwnedPackageJson $result.ManifestPath
    foreach($run in $manifest.Runs){$partCount=@($manifest.Files|Where-Object Group -ceq $run.Group|ForEach-Object Part|Select-Object -Unique).Count;Assert-OwnedPackage ($partCount -eq $(if($null -eq $run.SourceDirectory){0}else{1})) 'run split across archives or unrecorded file was inferred'}
    $zero=@($manifest.MissingArtifacts|Where-Object Reason -ceq 'ARTIFACT_REFERENCES_NOT_AVAILABLE');Assert-OwnedPackage ($zero.Count -eq 1 -and $zero[0].RecordedAttempt.ChildOutput[0] -ceq 'synthetic child was not invoked' -and $zero[0].RecordedAttempt.Error -ceq 'synthetic checkpoint failure') 'zero-reference failed attempt/output/error lost'
    $rejected=$false;try{$null=New-OwnedStabilityEvidencePackage $plan $progress $inventory $statistics $summary $prerequisite $output 1 $true}catch{$rejected=$true};Assert-OwnedPackage $rejected 'existing output accepted'
    $rejected=$false;try{$null=Resolve-OwnedPackagePath (Join-Path $script:packageRepository '../foreign.json')}catch{$rejected=$true};Assert-OwnedPackage $rejected 'outside path accepted'
    $rejected=$false;try{$s=[Collections.Generic.List[object]]::new();$gr=[ordered]@{};Add-OwnedPackageSource $s $gr 'bad' 'BAD' $plan '../escape.json' ''}catch{$rejected=$true};Assert-OwnedPackage $rejected 'archive traversal accepted'
    $rejected=$false;try{$s=[Collections.Generic.List[object]]::new();$gr=[ordered]@{};Add-OwnedPackageSource $s $gr 'bad' 'BAD' $plan 'safe.json' ('0'*64)}catch{$rejected=$true};Assert-OwnedPackage $rejected 'hash mismatch accepted'
    $partialDirectory=Join-Path $root 'partial-finalization';[IO.Directory]::CreateDirectory($partialDirectory)|Out-Null
    $missingInventory=Join-Path $partialDirectory 'inventory.json';$missingStatistics=Join-Path $partialDirectory 'statistics.json';$missingSummary=Join-Path $partialDirectory 'summary.json'
    Write-OwnedPackageNewBytes (Join-Path $partialDirectory 'finalization.json') (Get-OwnedPackageUtf8 (@{Schema='r1c4b-owned-stability-finalization/v1';BatchId=$batch;ImplementationSHA=$sha;InventoryPath=$missingInventory;StatisticsPath=$missingStatistics;SummaryPath=$missingSummary;InventoryWriteStatus='FAILED';InventoryWriteError='synthetic write failure';StatisticsError='synthetic write failure';SummaryError='synthetic write failure';SummaryExitCode=2;SummaryExists=$false;AggregateExitCode=2;SyntheticOnly=$true}|ConvertTo-Json))
    $partial=New-OwnedStabilityEvidencePackage $plan $progress $missingInventory $missingStatistics $missingSummary $prerequisite (Join-Path $partialDirectory 'package') 1 $true
    Assert-OwnedPackage ($partial.PackageInventorySource -ceq 'PROGRESS_FIXED_PRODUCER' -and $partial.DeliveryCompleteness -ceq 'PARTIAL' -and $partial.ControlProblems.Count -eq 3 -and $partial.IncompleteAttempts.Count -eq 1 -and $partial.BatchResult -ceq 'INVALID_EVIDENCE') 'partial controls/unfinished producer attempt were lost or promoted'
    $partialManifest=Read-OwnedPackageJson $partial.ManifestPath
    Assert-OwnedPackage ($partialManifest.Runs.Count -eq 3 -and @($partialManifest.Files|Where-Object Role -ceq 'FIXH_FINALIZATION').Count -eq 1 -and -not [IO.File]::Exists($missingInventory) -and -not [IO.File]::Exists($missingStatistics) -and -not [IO.File]::Exists($missingSummary)) 'partial package fabricated final controls or discarded finished evidence'
    return [pscustomobject]@{SyntheticOnly=$true;Checks=9;Result='PASS';FixtureDirectory=$root;PackageResult=$result;PartialPackageResult=$partial;Note='Fresh synthetic file/ZIP checks only; not empirical UAT. Originals are retained under ignored paths.'}
}
try{
    if($SelfTest){$result=Invoke-OwnedPackageSelfTest}else{$result=New-OwnedStabilityEvidencePackage $PlanPath $ProgressPath $InventoryPath $StatisticsPath $SummaryPath $PrerequisiteArtifactPath $OutputDirectory $MaxPartMiB}
    $result|ConvertTo-Json -Depth 12
    exit 0
}catch{Write-Error $_;exit 1}
