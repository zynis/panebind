[CmdletBinding()]
param([string]$ArtifactPath)
# Only immutable predecessor evidence is read. No native/environment CLI,
# historical runner program, GUI, input, Git command or evidence rewrite runs.
# Without ArtifactPath this script performs no filesystem writes.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Join-Path $repo 'uat/r1c4b-fixg'
$implementation='5fc0ccb575b82940b78e4a8e4cbac916e12a2b4c'
$inventoryRelative='uat/r1c4b-fixg/20260927T160611768Z-bounded-eight-8499fca50d464940a191f5f100ebcd5e.json'
$replayRelative='uat/r1c4b-fixg/20260927T160524985Z-fixf-original-prefix-frozen-a39cc6d4035d4dc997d6bf12cb63a224.json'
$fixFRun='uat/r1c4b-fixf/20260927T132106696Z-Debug-Move-controlled-abort-c2473a89d3db4c1da41ceb0ba51b1cb6'
$historicalBindings=[ordered]@{
    ($fixFRun+'/probe.jsonl')='09ECEDC632F91B4FAEB7CFA00682B7227827FE57AA8EDB79BB6701D68576B09D'
    ($fixFRun+'/run.metadata.json')='010322412D0E47E4BD4C52F230BD94325E530B3AA14E9E9F395E1076210F394F'
    ($fixFRun+'/environment-pre.json')='089CB6A07FCF1B6465861BE33B2DA4C8A2F1A35D52E010BCAFF02BE9231C5562'
    ($fixFRun+'/environment-post.json')='760621B0D8B4DBBF1940825DD9F35A4365C36F694C162C740BA9E83CE8D36458'
    'uat/r1c4b-fixf/20260927T132105318Z-bounded-eight-c7d46eec9ec2462e95df6b3badc8fe18.json'='5CBAF79AB38296A8800A7190EDF37EDCBF9D024E74C7CEEE10EA1179668F2253'
    'uat/r1c4b-input-isolation/20260926T201554647Z-Debug-BottomResize-3e6236754a004decb816079530b3861b.jsonl'='BB1E1F90D58A0B21C5A0B3F65F3A3DDB3994388C96FFE4C6A0F3ED5EF174B76F'
    'uat/r1c4b-input-isolation/20260926T201554647Z-Debug-BottomResize-3e6236754a004decb816079530b3861b.metadata.json'='D1B7C425CDB53A6356BC9B23C72376857BBE12996234F7032A5846B9B15BEB09'
    'uat/r1c4b-input-isolation/20260926T201554647Z-Debug-BottomResize-3e6236754a004decb816079530b3861b.revalidation.json'='27B68CB5923CC65B52CCB3EEE75F32095D6E320A00E127590660730159613A6F'
    'uat/r1c4b-input-isolation/20260927T095946432Z-Debug-Move-521d2098b1504bdfaa5366d029a56a9f.jsonl'='DACA83DC3AA9C5B43E7295B67E16E242526E240E5429C6C949A93C59C6CD03F8'
    'uat/r1c4b-input-isolation/20260927T095946432Z-Debug-Move-521d2098b1504bdfaa5366d029a56a9f.metadata.json'='3F733C50D15DAFADA70D538EFFA33899ECB8C6EC32F25BFC0D28F69B89D53D16'
    'uat/r1c4b-input-isolation/20260927T094931145Z-fixe-gates-c37c0342d5594a399685b57ceef982f2.json'='C97A4C6D02F9B091773E9744B2025DEC0274B3A7F148444B2D67C07209D6257C'
}
function Assert-FixHPrerequisite([bool]$Value,[string]$Reason){
    if(-not $Value){throw [IO.InvalidDataException]::new("Fix H prerequisite: $Reason")}
}
function Read-FixHPrerequisiteJson([string]$Path){return Get-Content -LiteralPath $Path -Raw -Encoding UTF8|ConvertFrom-Json -ErrorAction Stop}
function New-FixHPrerequisiteBinding([string]$Kind,[string]$Path,[string]$SHA256){
    $absolute=[IO.Path]::GetFullPath($Path)
    Assert-FixHPrerequisite ($absolute.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -and $SHA256 -cmatch '^[A-F0-9]{64}$') ("evidence path/hash must be exact and within this repository: kind=$Kind path=$absolute sha=$SHA256")
    Assert-FixHPrerequisite ((Get-FileHash -LiteralPath $absolute -Algorithm SHA256).Hash -ceq $SHA256) "original SHA256 mismatch: $absolute"
    $file=Get-Item -LiteralPath $absolute
    Assert-FixHPrerequisite (-not $file.PSIsContainer) 'evidence must be a regular file'
    return [pscustomobject]@{Kind=$Kind;Path=$absolute;RelativePath=$absolute.Substring($repo.Length+1).Replace('\','/');SHA256=$SHA256;Bytes=[long]$file.Length;Origin='FIXG_PREREQUISITE'}
}
function Test-FixHPrerequisiteBindings($Bindings){
    foreach($binding in $Bindings){
        $hash=(Get-FileHash -LiteralPath $binding.Path -Algorithm SHA256).Hash
        [long]$length=(Get-Item -LiteralPath $binding.Path).Length
        Assert-FixHPrerequisite ($hash -ceq $binding.SHA256 -and $length -eq $binding.Bytes) "evidence changed during read-only validation: $($binding.Path)"
        [pscustomobject]@{Kind=$binding.Kind;Path=$binding.Path;RelativePath=$binding.RelativePath;SHA256=$hash;Bytes=$length;Origin=$binding.Origin}
    }
}
function Read-FixHPrerequisiteDefinitions([string]$File,[string[]]$Names){
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $File),[ref]$tokens,[ref]$errors)
    Assert-FixHPrerequisite ($errors.Count -eq 0) "reviewed helper source syntax: $File"
    foreach($name in $Names){
        $nodes=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $name},$true))
        Assert-FixHPrerequisite ($nodes.Count -eq 1 -and $null -eq (Get-Command $name -ErrorAction SilentlyContinue)) "unique helper without override: $name"
        # Return definitions to this script scope. The program AST is never
        # evaluated, and none of these reviewed helpers invokes a child/CLI.
        $nodes[0].Extent.Text
    }
}
function Assert-FixHPrerequisiteVerdict($Actual,$Cached,[string]$Mode){
    foreach($field in @('Result','FixtureResult','GestureResult','CleanupResult','TakeoverAcceptance','ContractVerified','Operation','TestMode','FinalLeftDown','SyntheticFixture')){
        Assert-FixHPrerequisite ((Get-ReliabilityField $Actual $field) -ceq (Get-ReliabilityField $Cached $field)) "cached predecessor verdict differs: $field"
    }
    Assert-FixHPrerequisite ($Actual.Result -ceq 'PASS' -and (Test-ReliabilityTrue $Actual.ContractVerified) -and (Test-ReliabilityFalse $Actual.SyntheticFixture) -and (Test-ReliabilityFalse $Actual.FinalLeftDown)) 'complete empirical predecessor contract required'
    if($Mode -ceq 'controlled-abort'){
        Assert-FixHPrerequisite ($Actual.FixtureResult -ceq 'PASS_EXPECTED_ABORT' -and $Actual.GestureResult -ceq 'BLOCKED_BY_TEST_FAULT' -and $Actual.CleanupResult -ceq 'PASS' -and $Actual.TakeoverAcceptance -ceq 'NOT_RUN') 'abort cleanup cannot count as normal takeover'
    }else{
        Assert-FixHPrerequisite ($Actual.FixtureResult -ceq 'PASS' -and $Actual.GestureResult -ceq 'PASS' -and $Actual.CleanupResult -ceq 'NOT_NEEDED' -and $Actual.TakeoverAcceptance -ceq 'PASS' -and $Actual.NormalProof.Result -ceq 'PASS' -and $Actual.NormalProof.Cancel -ceq 'PASS_WITH_TERMINAL_SETTLEMENT' -and $Actual.NormalProof.Handoff -ceq 'PASS' -and $Actual.NormalProof.Takeover -ceq 'PASS' -and (Test-ReliabilityTrue $Actual.NormalProof.ContractVerified)) 'full unchanged normal proof required'
    }
}

$allBindings=[Collections.Generic.List[object]]::new()
$packageBindings=[Collections.Generic.List[object]]::new()
$inventoryFile=New-FixHPrerequisiteBinding 'FIXG_INVENTORY' (Join-Path $repo $inventoryRelative) '9FAA3A049D027079F9026C77B34213ECEBC8AC5C9D53C75708AC6C54A1F01A37'
$replayFile=New-FixHPrerequisiteBinding 'FIXF_FROZEN_REPLAY' (Join-Path $repo $replayRelative) '4444A4AEA058828DCFE8B42C31CF8F47AE20E4EF0BFBABED35A2A3BB5A3A47F6'
foreach($binding in @($inventoryFile,$replayFile)){$allBindings.Add($binding);$packageBindings.Add($binding)}
$historyBefore=@(foreach($relative in $historicalBindings.Keys){
    $binding=New-FixHPrerequisiteBinding 'HISTORICAL_DE_F_HASH_REFERENCE' (Join-Path $repo $relative) $historicalBindings[$relative]
    $allBindings.Add($binding);$binding
})
$inventory=Read-FixHPrerequisiteJson $inventoryFile.Path
$replay=Read-FixHPrerequisiteJson $replayFile.Path
Assert-FixHPrerequisite ($inventory.Schema -ceq 'r1c4b-fixg-input-reliability-eight/v2' -and $inventory.ExecutedHEAD -ceq $implementation -and $inventory.Result -ceq 'PASS' -and $inventory.AggregateExitCode -eq 0 -and $inventory.ExpectedAbortPassed -eq 4 -and $inventory.NormalPassed -eq 4 -and $inventory.ProbeInvocations -eq 8 -and @($inventory.Runs).Count -eq 8 -and $null -eq $inventory.FirstUnexpectedFailure -and $inventory.MetadataWriteStatus -ceq 'WRITTEN' -and $null -eq $inventory.MetadataWriteError) 'fixed G eight-run inventory is not complete'
Assert-FixHPrerequisite ($replay.Schema -ceq 'r1c4b-fixg-bootstrap-revalidation/v1' -and $replay.EvidenceImplementationSHA -ceq '2f06924e88b5bef81b35e1d2414aab9b4c6d4f0f' -and $replay.ClassifierHEAD -ceq $implementation -and @($replay.ClassifierWorktree).Count -eq 0 -and $replay.ClassifierSHAContainsCurrentFiles -eq $true -and $replay.OriginalEvidenceHashesUnchanged -eq $true -and $replay.ReplayResult -ceq 'VERIFIED_BLOCKED_BEFORE_INPUT' -and $null -eq $replay.HistoricalMetadata.Result -and $null -eq $replay.HistoricalMetadata.AfterHEAD -and $null -eq $replay.HistoricalMetadata.AfterIdentity -and $replay.HistoricalMetadata.AfterIdentityReconstructed -eq $false) 'frozen F replay must preserve the blocked run and missing historical fields'
Assert-FixHPrerequisite (@($replay.OriginalFilesBefore.PSObject.Properties).Count -eq 11 -and @($replay.OriginalFilesAfter.PSObject.Properties).Count -eq 11) 'frozen replay historical binding count'
foreach($binding in $historyBefore){
    foreach($side in @('OriginalFilesBefore','OriginalFilesAfter')){
        $entry=$replay.$side.PSObject.Properties[$binding.RelativePath].Value
        Assert-FixHPrerequisite ([IO.Path]::GetFullPath($entry.Path) -ceq $binding.Path -and $entry.SHA256 -ceq $binding.SHA256) 'frozen replay predecessor path/hash mismatch'
    }
}
$frozenValidators=[ordered]@{'scripts/r1c4b-input-isolation-validation.ps1'='9FFB838F9B530257774B1E53B61102E9C99166D0926D37300350DBF7B6A4719D';'scripts/r1c4b-normal-v5-primitives.ps1'='064B9C1EDAC92C7A7F011FDF7F8B01D15D339D0214CF21436F753E8876483D6E'}
foreach($relative in $frozenValidators.Keys){Assert-FixHPrerequisite ((Get-FileHash -LiteralPath (Join-Path $repo $relative) -Algorithm SHA256).Hash -ceq $frozenValidators[$relative]) "frozen validator changed: $relative"}
foreach($property in $replay.ClassifierFilesSHA256.PSObject.Properties){Assert-FixHPrerequisite ((Get-FileHash -LiteralPath (Join-Path $repo $property.Name) -Algorithm SHA256).Hash -ceq $property.Value) "frozen classifier dependency changed: $($property.Name)"}
. (Join-Path $PSScriptRoot 'r1c4b-native-abort-validation.ps1')
$singleHelpers=@('Get-ReliabilityField','Test-ReliabilityTrue','Test-ReliabilityFalse','Get-ReliabilitySnapshot','Test-ReliabilitySameSnapshot','Test-ReliabilityEnvironmentContext','Get-ReliabilityButtonsObservation','Test-ReliabilityEnvironment','Get-ReliabilityExitCode')
foreach($definition in @(Read-FixHPrerequisiteDefinitions 'run-r1c4b-input-reliability.ps1' $singleHelpers)){. ([scriptblock]::Create($definition))}
foreach($definition in @(Read-FixHPrerequisiteDefinitions 'run-r1c4b-input-reliability-gates.ps1' @('Get-ReliabilityPlan','Assert-ReliabilityGate','Test-ReliabilityBlockedVerdict','Read-ReliabilityRun'))){. ([scriptblock]::Create($definition))}
# Historical HEAD is the actual G implementation, never today's HEAD or the
# G report commit. The original nineteen identities do not absorb H helpers.
$state=[ordered]@{ExecutedHEAD=$implementation;Identity=@{}}
$implementationBefore=[ordered]@{}
foreach($configuration in @('Debug','Release')){
    $identity=Get-ReliabilitySnapshot $repo $configuration
    $recorded=Get-ReliabilityField $inventory.Identity $configuration
    Assert-FixHPrerequisite ($identity.Count -eq 19 -and @($recorded.PSObject.Properties).Count -eq 19) 'original G nineteen-file identity inventory'
    foreach($name in $identity.Keys){Assert-FixHPrerequisite ($identity[$name] -ceq (Get-ReliabilityField $recorded $name)) "frozen G implementation identity: $configuration $name"}
    $state.Identity[$configuration]=$identity;$implementationBefore[$configuration]=$identity
}
$nonces=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$runIds=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$verifiedRuns=[Collections.Generic.List[object]]::new()
$plan=@(Get-ReliabilityPlan)
for($i=0;$i -lt $plan.Count;++$i){
    $step=$plan[$i];$record=$inventory.Runs[$i]
    Assert-FixHPrerequisite ($record.Number -eq $step.Number -and $record.Configuration -ceq $step.Configuration -and $record.Operation -ceq $step.Operation -and $record.Mode -ceq $step.Mode -and $record.RunId -cmatch '^[a-f0-9]{32}$' -and $runIds.Add($record.RunId) -and $record.Passed -eq $true -and $record.ProbeInvoked -eq $true -and $record.ChildExitCode -eq 0 -and $record.RunnerExitCode -eq 0 -and $record.IndependentlyValidatedResult -ceq 'PASS') 'original eight-run plan, identity or layered exit mismatch'
    $metadataPath=[IO.Path]::GetFullPath($record.MetadataPath)
    Assert-FixHPrerequisite ($metadataPath.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'G run evidence escaped its original root'
    $metadataFile=New-FixHPrerequisiteBinding 'FIXG_METADATA' $metadataPath $record.MetadataSHA256
    $allBindings.Add($metadataFile);$packageBindings.Add($metadataFile)
    $metadata=Read-FixHPrerequisiteJson $metadataPath
    $directory=Split-Path -Parent $metadataPath
    $files=[ordered]@{Metadata=$metadataFile}
    $files.Probe=New-FixHPrerequisiteBinding 'FIXG_PROBE' (Join-Path $directory 'probe.jsonl') $record.LogSHA256
    foreach($part in @('Pre','Post')){
        $environmentPath=Join-Path $directory ('environment-'+$part.ToLowerInvariant()+'.json')
        Assert-FixHPrerequisite ([IO.Path]::GetFullPath((Get-ReliabilityField $metadata ('Environment'+$part+'Path'))) -ceq $environmentPath) 'historical environment path is not the actual run file'
        $files[$part]=New-FixHPrerequisiteBinding ('FIXG_'+$part.ToUpperInvariant()) $environmentPath (Get-ReliabilityField $metadata ('Environment'+$part+'SHA256'))
    }
    foreach($part in @('Probe','Pre','Post')){$allBindings.Add($files[$part]);$packageBindings.Add($files[$part])}
    $validated=Read-ReliabilityRun $metadataPath $step $record.RunId
    $verdict=$validated.IndependentVerdict
    Assert-FixHPrerequisite ($validated.IndependentlyValidatedResult -ceq 'PASS' -and $validated.RunnerExitCode -eq 0 -and $record.ProbeExitCode -eq $validated.ProbeExitCode -and $validated.EnvironmentPreReady -eq $true -and $validated.EnvironmentPostReady -eq $true -and $validated.MetadataWriteStatus -ceq 'WRITTEN' -and $null -eq $validated.MetadataWriteError) 'historical full v5 and readonly v2 pre/post acceptance'
    Assert-FixHPrerequisiteVerdict $verdict $validated.Result $step.Mode
    Assert-FixHPrerequisiteVerdict $verdict $record.IndependentVerdict $step.Mode
    foreach($field in @('FixtureResult','GestureResult','CleanupResult','TakeoverAcceptance')){Assert-FixHPrerequisite ($record.$field -ceq $verdict.$field) 'historical inventory layered verdict mismatch'}
    $rows=@(Get-Content -LiteralPath $files.Probe.Path -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json -ErrorAction Stop})
    $verifiedRuns.Add([pscustomobject]@{Number=$step.Number;Configuration=$step.Configuration;Operation=$step.Operation;Mode=$step.Mode;RunId=$record.RunId;HistoricalImplementationSHA=$implementation;RunNonce=$validated.ValidatedNonce;Files=[pscustomobject]$files;ProbeExitCode=$validated.ProbeExitCode;RunnerExitCode=$validated.RunnerExitCode;IndependentVerdict=$verdict;LifecycleRecordSequences=[pscustomobject]@{Owned=@($rows|Where-Object type -ceq owned|ForEach-Object sequence);Receiver=@($rows|Where-Object type -ceq receiver|ForEach-Object sequence);Hook=@($rows|Where-Object type -ceq winevent_hook_installed|ForEach-Object sequence);HookRemoved=@($rows|Where-Object type -ceq winevent_hook_removed|ForEach-Object sequence);ReceiverShutdown=@($rows|Where-Object type -ceq receiver_shutdown|ForEach-Object sequence);Shutdown=$rows[-1].sequence}})
    Write-Verbose "Fix G predecessor $($step.Number)/8 independently verified as $($verdict.FixtureResult)"
}
$fixFLog=Join-Path $repo ($fixFRun+'/probe.jsonl')
$prefix=Test-FixFInputReliabilityEvidence -Path $fixFLog
Assert-FixHPrerequisite (Test-ReliabilityBlockedVerdict $prefix 2) 'original F fresh prefix remains verified BLOCKED, never fixture PASS'
foreach($field in @('Result','EvidenceIntegrity','PrefixValidation','ExecutionResult','PrefixKind','BlockPhase','NativeBlockReason','FixtureResult','GestureResult','CleanupResult','TakeoverAcceptance','InputAttempted','TestDownPending','ContractVerified','FinalLeftDown')){Assert-FixHPrerequisite ((Get-ReliabilityField $prefix $field) -ceq (Get-ReliabilityField $replay.PrefixVerdict $field)) 'frozen F prefix differs from independently validated original'}
$fixFMetadata=Read-FixHPrerequisiteJson (Join-Path $repo ($fixFRun+'/run.metadata.json'))
Assert-FixHPrerequisite ($fixFMetadata.ExecutedHEAD -ceq '2f06924e88b5bef81b35e1d2414aab9b4c6d4f0f' -and $null -eq $fixFMetadata.Result -and $null -eq $fixFMetadata.AfterHEAD -and $null -eq $fixFMetadata.AfterIdentity -and $fixFMetadata.ProbeExitCode -eq 2) 'original F null/blocked facts are not reconstructed'
$fixFPost=Read-FixHPrerequisiteJson (Join-Path $repo ($fixFRun+'/environment-post.json'))
Assert-FixHPrerequisite ($fixFPost.schema -ceq 'r1c4b-readonly-input-environment/v1' -and (Get-ReliabilityButtonsObservation $fixFPost) -ceq 'OBSERVED_UP' -and $replay.PostButtonObservation.ButtonsState -ceq 'OBSERVED_UP') 'F original v1 button observation remains historical and separate from v2 readiness'
foreach($relative in @(($fixFRun+'/probe.jsonl'),($fixFRun+'/environment-post.json'))){
    $packageBindings.Add((New-FixHPrerequisiteBinding 'FIXF_PREFIX_SUPPORT' (Join-Path $repo $relative) $historicalBindings[$relative]))
}
$dVerdict=Test-InputIsolationOwnedEvidence -Path (Join-Path $repo 'uat/r1c4b-input-isolation/20260926T201554647Z-Debug-BottomResize-3e6236754a004decb816079530b3861b.jsonl')
$eVerdict=Test-InputIsolationOwnedEvidence -Path (Join-Path $repo 'uat/r1c4b-input-isolation/20260927T095946432Z-Debug-Move-521d2098b1504bdfaa5366d029a56a9f.jsonl')
Assert-FixHPrerequisite ($dVerdict.Result -ceq 'PASS' -and $dVerdict.Takeover -ceq 'PASS' -and $eVerdict.Result -ceq 'BLOCKED' -and $eVerdict.CleanupInputRelease -ceq 'SKIPPED_NO_AUTHORITY' -and $eVerdict.CleanupCurrentLeftDown -eq $true) 'historical D/E full replay verdicts must remain unchanged'
$afterBindings=@(Test-FixHPrerequisiteBindings @($allBindings.ToArray()))
$historyAfter=@($afterBindings|Where-Object Kind -ceq 'HISTORICAL_DE_F_HASH_REFERENCE')
$implementationAfter=[ordered]@{}
foreach($configuration in @('Debug','Release')){$identity=Get-ReliabilitySnapshot $repo $configuration;Assert-FixHPrerequisite (Test-ReliabilitySameSnapshot $implementationBefore[$configuration] $identity) 'implementation changed during predecessor validation';$implementationAfter[$configuration]=$identity}
foreach($property in $replay.ClassifierFilesSHA256.PSObject.Properties){Assert-FixHPrerequisite ((Get-FileHash -LiteralPath (Join-Path $repo $property.Name) -Algorithm SHA256).Hash -ceq $property.Value) 'frozen classifier dependency changed during validation'}
$result=[ordered]@{
    Schema='r1c4b-fixh-prerequisites/v1';VerifiedUtc=[DateTime]::UtcNow.ToString('o');Result='PASS';ReadOnlyOriginals=$true;NewGuiRuns=0;NewInputCalls=0
    FixGImplementationSHA=$implementation;FixGInventory=$inventoryFile;FrozenFixFReplay=$replayFile;FrozenValidatorsSHA256=$frozenValidators
    Runs=@($verifiedRuns.ToArray());ExpectedAbortVerified=4;NormalVerified=4;FixFPrefixVerdict=$prefix;FixFPostButtonsObserved='OBSERVED_UP';CurrentButtonStateClaim='NONE'
    HistoricalDReplayResult=$dVerdict.Result;HistoricalEBlockedResult=$eVerdict.Result;HistoricalECleanupResult=$eVerdict.CleanupInputRelease
    EvidenceFilesBefore=@($allBindings.ToArray());EvidenceFilesAfter=$afterBindings;HistoricalFilesBefore=$historyBefore;HistoricalFilesAfter=$historyAfter;OriginalEvidenceHashesUnchanged=$true
    ImplementationIdentityBefore=$implementationBefore;ImplementationIdentityAfter=$implementationAfter;VerifierFileSHA256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
    Files=@($packageBindings.ToArray());HistoricalEvidenceCountsTowardFixH=$false;FIXE_HISTORICAL_FORMAL='13_PASS_THEN_BLOCKED_AT_14';FIXF_HISTORICAL_GUI='BLOCKED_BEFORE_INPUT';FIXG_HISTORICAL_BOUNDED='4_ABORT_PASS_AND_4_NORMAL_PASS'
}
if($ArtifactPath){
    $target=[IO.Path]::GetFullPath($ArtifactPath)
    $artifactRoot=[IO.Path]::GetFullPath((Join-Path $repo 'uat/r1c4b-fixh'))+[IO.Path]::DirectorySeparatorChar
    Assert-FixHPrerequisite ($target.StartsWith($artifactRoot,[StringComparison]::OrdinalIgnoreCase) -and -not (Test-Path -LiteralPath $target)) 'new output must be unused and under ignored Fix H directory'
    $parent=[IO.Path]::GetDirectoryName($target)
    if(-not (Test-Path -LiteralPath $parent)){$null=New-Item -ItemType Directory -Path $parent}
    $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($result|ConvertTo-Json -Depth 40)+[Environment]::NewLine)
    $stream=[IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    try{$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
    Write-Host ('PrerequisiteArtifact = '+$target)
    Write-Host ('PrerequisiteArtifactSHA256 = '+(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash)
}else{$result|ConvertTo-Json -Depth 40}
