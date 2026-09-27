[CmdletBinding()]
param([Parameter(Mandatory)][string]$ArtifactPath)
# Offline read-only historical revalidation. Original JSONL/metadata/inventory
# are immutable; the only output is an independently named CreateNew artifact.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-native-abort-validation.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$runRelative='uat/r1c4b-fixf/20260927T132106696Z-Debug-Move-controlled-abort-c2473a89d3db4c1da41ceb0ba51b1cb6'
$implementation='2f06924e88b5bef81b35e1d2414aab9b4c6d4f0f'
$originalBindings=[ordered]@{
    ($runRelative+'/probe.jsonl')='09ECEDC632F91B4FAEB7CFA00682B7227827FE57AA8EDB79BB6701D68576B09D'
    ($runRelative+'/run.metadata.json')='010322412D0E47E4BD4C52F230BD94325E530B3AA14E9E9F395E1076210F394F'
    ($runRelative+'/environment-pre.json')='089CB6A07FCF1B6465861BE33B2DA4C8A2F1A35D52E010BCAFF02BE9231C5562'
    ($runRelative+'/environment-post.json')='760621B0D8B4DBBF1940825DD9F35A4365C36F694C162C740BA9E83CE8D36458'
    'uat/r1c4b-fixf/20260927T132105318Z-bounded-eight-c7d46eec9ec2462e95df6b3badc8fe18.json'='5CBAF79AB38296A8800A7190EDF37EDCBF9D024E74C7CEEE10EA1179668F2253'
    'uat/r1c4b-input-isolation/20260926T201554647Z-Debug-BottomResize-3e6236754a004decb816079530b3861b.jsonl'='BB1E1F90D58A0B21C5A0B3F65F3A3DDB3994388C96FFE4C6A0F3ED5EF174B76F'
    'uat/r1c4b-input-isolation/20260926T201554647Z-Debug-BottomResize-3e6236754a004decb816079530b3861b.metadata.json'='D1B7C425CDB53A6356BC9B23C72376857BBE12996234F7032A5846B9B15BEB09'
    'uat/r1c4b-input-isolation/20260926T201554647Z-Debug-BottomResize-3e6236754a004decb816079530b3861b.revalidation.json'='27B68CB5923CC65B52CCB3EEE75F32095D6E320A00E127590660730159613A6F'
    'uat/r1c4b-input-isolation/20260927T095946432Z-Debug-Move-521d2098b1504bdfaa5366d029a56a9f.jsonl'='DACA83DC3AA9C5B43E7295B67E16E242526E240E5429C6C949A93C59C6CD03F8'
    'uat/r1c4b-input-isolation/20260927T095946432Z-Debug-Move-521d2098b1504bdfaa5366d029a56a9f.metadata.json'='3F733C50D15DAFADA70D538EFFA33899ECB8C6EC32F25BFC0D28F69B89D53D16'
    'uat/r1c4b-input-isolation/20260927T094931145Z-fixe-gates-c37c0342d5594a399685b57ceef982f2.json'='C97A4C6D02F9B091773E9744B2025DEC0274B3A7F148444B2D67C07209D6257C'
}
function Assert-FixGReplay([bool]$Value,[string]$Reason){if(-not $Value){throw [IO.InvalidDataException]::new("Fix G historical prefix: $Reason")}}
function Get-FixGOriginalHashes {
    $hashes=[ordered]@{}
    foreach($relative in $originalBindings.Keys){$path=Join-Path $repo $relative;$hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash;Assert-FixGReplay ($hash -ceq $originalBindings[$relative]) "immutable SHA256: $relative";$hashes[$relative]=[pscustomobject]@{Path=[IO.Path]::GetFullPath($path);SHA256=$hash}}
    return $hashes
}
$beforeHashes=Get-FixGOriginalHashes
$target=[IO.Path]::GetFullPath($ArtifactPath)
$artifactRoot=[IO.Path]::GetFullPath((Join-Path $repo 'uat/r1c4b-fixg'))+[IO.Path]::DirectorySeparatorChar
Assert-FixGReplay ($target.StartsWith($artifactRoot,[StringComparison]::OrdinalIgnoreCase) -and -not (Test-Path -LiteralPath $target)) 'new artifact must be unused and under ignored Fix G directory'
$metadata=Get-Content -LiteralPath (Join-Path $repo ($runRelative+'/run.metadata.json')) -Raw -Encoding UTF8|ConvertFrom-Json
$inventory=Get-Content -LiteralPath (Join-Path $repo 'uat/r1c4b-fixf/20260927T132105318Z-bounded-eight-c7d46eec9ec2462e95df6b3badc8fe18.json') -Raw -Encoding UTF8|ConvertFrom-Json
Assert-FixGReplay ($metadata.Schema -ceq 'r1c4b-input-reliability-run/v1' -and $metadata.ExecutedHEAD -ceq $implementation -and $metadata.ExpectedHEAD -ceq $implementation -and $metadata.ProbeInvoked -eq $true -and $metadata.ProbeExitCode -eq 2 -and $metadata.RunId -ceq 'c2473a89d3db4c1da41ceb0ba51b1cb6') 'original run identity and native exit'
Assert-FixGReplay ($null -eq $metadata.Result -and $null -eq $metadata.AfterHEAD -and $null -eq $metadata.AfterIdentity -and $metadata.LogSHA256 -ceq $originalBindings[$runRelative+'/probe.jsonl'] -and $metadata.EnvironmentPostSHA256 -ceq $originalBindings[$runRelative+'/environment-post.json'] -and $metadata.EnvironmentPostExitCode -eq 0) 'historical missing fields stay null; immutable post/log references'
Assert-FixGReplay ($inventory.ExecutedHEAD -ceq $implementation -and $inventory.Result -ceq 'STOPPED' -and @($inventory.Runs).Count -eq 1 -and $inventory.Runs[0].RunId -ceq $metadata.RunId -and $inventory.Runs[0].MetadataSHA256 -ceq $originalBindings[$runRelative+'/run.metadata.json'] -and $inventory.Runs[0].LogSHA256 -ceq $metadata.LogSHA256 -and $inventory.Runs[0].ChildExitCode -eq 2 -and $inventory.Runs[0].ProbeExitCode -eq 2 -and $inventory.Runs[0].Passed -eq $false) 'original bounded plan stayed stopped after its sole native launch'
foreach($property in $metadata.BeforeIdentity.PSObject.Properties){Assert-FixGReplay ($inventory.Identity.Debug.PSObject.Properties[$property.Name].Value -ceq $property.Value) 'original metadata/inventory implementation identities agree'}
$path=Join-Path $repo ($runRelative+'/probe.jsonl')
$rawRows=@(Get-Content -LiteralPath $path -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json -ErrorAction Stop})
Assert-FixGReplay ($rawRows.Count -eq 28 -and $rawRows[8].hwnd -eq 13571864 -and $rawRows[8].pid -eq 56980 -and $rawRows[8].tid -eq 46328 -and $rawRows[11].foreground -eq 854960 -and $rawRows[11].capture_hwnd -eq 71435280) 'actual original 28-record identity/capture facts'
$verdict=Test-FixFInputReliabilityEvidence -Path $path
Assert-FixGReplay ($verdict.EvidenceIntegrity -ceq 'VALID' -and $verdict.PrefixValidation -ceq 'VERIFIED_BLOCKED_BEFORE_INPUT' -and $verdict.Result -ceq 'BLOCKED' -and $verdict.FixtureResult -ceq 'BLOCKED' -and $verdict.GestureResult -ceq 'NOT_RUN' -and $verdict.CleanupResult -ceq 'NOT_RUN' -and $verdict.TakeoverAcceptance -ceq 'NOT_RUN' -and -not $verdict.ContractVerified -and -not $verdict.InputAttempted -and -not $verdict.TestDownPending) 'independent prefix verdict cannot become complete fixture acceptance'
# Import only the reviewed pure observation definitions from the actual runner;
# no runner top-level branch, CLI, environment probe or function override runs.
$runnerPath=Join-Path $PSScriptRoot 'run-r1c4b-input-reliability.ps1'
$tokens=$null;$parseErrors=$null;$runnerAst=[Management.Automation.Language.Parser]::ParseFile($runnerPath,[ref]$tokens,[ref]$parseErrors)
Assert-FixGReplay ($parseErrors.Count -eq 0) 'observation helper syntax'
foreach($name in @('Get-ReliabilityField','Test-ReliabilityTrue','Test-ReliabilityFalse','Test-ReliabilityEnvironmentContext','Get-ReliabilityButtonsObservation')){
    Assert-FixGReplay ($null -eq (Get-Command $name -ErrorAction SilentlyContinue)) "no helper override: $name"
    $definitions=@($runnerAst.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $name},$true));Assert-FixGReplay ($definitions.Count -eq 1) "unique pure observation helper: $name"
    . ([scriptblock]::Create($definitions[0].Extent.Text))
}
$postPath=Join-Path $repo ($runRelative+'/environment-post.json');$post=Get-Content -LiteralPath $postPath -Raw -Encoding UTF8|ConvertFrom-Json
$buttons=Get-ReliabilityButtonsObservation $post
Assert-FixGReplay ($post.schema -ceq 'r1c4b-readonly-input-environment/v1' -and $buttons -ceq 'OBSERVED_UP' -and $post.before.start_qpc -gt $rawRows[-1].qpc -and $post.qpc_frequency -eq $rawRows[0].qpc_frequency) 'independent original post context and all eleven actual UP observations'
$sourcePath='src/platform/windows/operations/magnet_takeover_probe.cpp'
$source=@(git -C $repo show ($implementation+':'+$sourcePath));Assert-FixGReplay ($LASTEXITCODE -eq 0) 'historical source available from local Git objects'
$sourceText=$source -join "`n"
Assert-FixGReplay ($sourceText.Contains('const HWND foreground=GetForegroundWindow();const DWORD tid=GetWindowThreadProcessId(foreground,nullptr);') -and $sourceText.Contains('const bool query=tid&&GetGUIThreadInfo(tid,&gui)!=FALSE;') -and $sourceText.Contains('const bool stable=foreground&&GetForegroundWindow()==foreground;')) 'pinned source queries the observed foreground TID and separately checks foreground stability'
$classifierHead=git -C $repo rev-parse HEAD;Assert-FixGReplay ($LASTEXITCODE -eq 0 -and $classifierHead -cmatch '^[a-f0-9]{40}$') 'current classifier HEAD available'
$classifierStatus=@(git -C $repo status --porcelain -- scripts/r1c4b-native-abort-validation.ps1 scripts/revalidate-r1c4b-fixg-bootstrap.ps1 scripts/run-r1c4b-input-reliability.ps1);Assert-FixGReplay ($LASTEXITCODE -eq 0) 'classifier worktree state available'
$implementationHashes=[ordered]@{}
foreach($file in @('r1c4b-native-abort-validation.ps1','r1c4b-normal-v5-primitives.ps1','r1c4b-input-isolation-validation.ps1','r1c4b-end-diagnostics-validation.ps1','r1c4b-end-handoff-validation.ps1','r1c4b-takeover-owned-validation.ps1','r1c4b-auto-owned-modal-validation.ps1','run-r1c4b-input-reliability.ps1','revalidate-r1c4b-fixg-bootstrap.ps1')){$implementationHashes['scripts/'+$file]=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $file) -Algorithm SHA256).Hash}
$afterHashes=Get-FixGOriginalHashes
$artifact=[ordered]@{
    Schema='r1c4b-fixg-bootstrap-revalidation/v1';CreatedUtc=[DateTime]::UtcNow.ToString('o');ReadOnlyOriginals=$true;NewGuiRuns=0;NewInputCalls=0
    EvidenceImplementationSHA=$implementation;OriginalFilesBefore=$beforeHashes;OriginalFilesAfter=$afterHashes;OriginalEvidenceHashesUnchanged=$true
    ClassifierHEAD=$classifierHead;ClassifierWorktree=$classifierStatus;ClassifierFilesSHA256=$implementationHashes;ClassifierSHAContainsCurrentFiles=($classifierStatus.Count -eq 0)
    HistoricalMetadata=[pscustomobject]@{Path=(Join-Path $repo ($runRelative+'/run.metadata.json'));Result=$metadata.Result;AfterHEAD=$metadata.AfterHEAD;AfterIdentity=$metadata.AfterIdentity;ImplementationUnchanged=$metadata.ImplementationUnchanged;OriginalValidatorError=$metadata.Reason;ProbeExitCode=$metadata.ProbeExitCode;RunnerExitCode=$metadata.RunnerExitCode;AfterIdentityReconstructed=$false}
    PrefixVerdict=$verdict;OriginalRecordCount=$rawRows.Count
    LegacyActivationQuery=[pscustomobject]@{RecordSequence=12;GUIQuerySucceeded=$rawRows[11].gui_query_succeeded;ForegroundSnapshotStable=$rawRows[11].foreground_snapshot_stable;ForegroundHWND=$rawRows[11].foreground;CaptureHWND=$rawRows[11].capture_hwnd;MenuOwnerHWND=$rawRows[11].menu_owner_hwnd;MoveSizeHWND=$rawRows[11].move_size_hwnd;GUIFlags=$rawRows[11].gui_flags;QueryTID='NOT_AVAILABLE';GUIError='NOT_AVAILABLE';GUIQueryStartQPC='NOT_AVAILABLE';GUIQueryFinishQPC='NOT_AVAILABLE';ForegroundPID='NOT_AVAILABLE';ForegroundTID='NOT_AVAILABLE';EvidenceBoundary='Original activation record lacks these additive fields; none reconstructed from post, cleanup or current state'}
    HistoricalQueryImplementation=[pscustomobject]@{SHA=$implementation;Path=$sourcePath;OriginalRuntimeSourceSHA256=$metadata.BeforeIdentity.PSObject.Properties[$sourcePath].Value;Inspection='LOCAL_PINNED_SOURCE';QueryMethod='GetForegroundWindow -> GetWindowThreadProcessId(observed foreground) -> GetGUIThreadInfo(observed foreground TID) -> independent GetForegroundWindow stability check';PerRecordQueriedTID='NOT_AVAILABLE'}
    PostButtonObservation=[pscustomobject]@{Path=$postPath;SHA256=$originalBindings[$runRelative+'/environment-post.json'];Schema=$post.schema;ButtonsState=$buttons;BeforeStartQPC=$post.before.start_qpc;BeforeFinishQPC=$post.before.finish_qpc;AfterStartQPC=$post.after.start_qpc;AfterFinishQPC=$post.after.finish_qpc;QPCFrequency=$post.qpc_frequency;Keys=$post.keys;ObservationWallClockUtc='NOT_AVAILABLE';FileLastWriteUtc=(Get-Item -LiteralPath $postPath).LastWriteTimeUtc.ToString('o');CurrentButtonStateClaim='NONE';CaptureReadinessProof='NOT_AVAILABLE_IN_V1'}
    FullCleanupAcceptance='NOT_RUN';FullTakeoverAcceptance='NOT_RUN';ExternalInputSource='UNKNOWN';CaptureHolderCause='UNKNOWN';ReplayResult='VERIFIED_BLOCKED_BEFORE_INPUT'
}
$parent=[IO.Path]::GetDirectoryName($target)
if(-not (Test-Path -LiteralPath $parent)){$null=New-Item -ItemType Directory -Path $parent}
$bytes=[Text.UTF8Encoding]::new($false).GetBytes(($artifact|ConvertTo-Json -Depth 40)+[Environment]::NewLine)
$stream=[IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
try{$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
Write-Host ('FIXF_ORIGINAL_BOOTSTRAP_PREFIX = '+$verdict.PrefixValidation)
Write-Host 'FIXF_ORIGINAL_EVIDENCE_HASHES_UNCHANGED = true'
Write-Host ('Artifact = '+$target)
Write-Host ('ArtifactSHA256 = '+(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash)
exit 0
