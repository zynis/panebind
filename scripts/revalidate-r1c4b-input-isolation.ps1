[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-input-isolation-replay.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$prefix=Join-Path $repo 'uat/r1c4b-input-isolation/20260926T201554647Z-Debug-BottomResize-3e6236754a004decb816079530b3861b'
$evidencePath=$prefix+'.jsonl';$metadataPath=$prefix+'.metadata.json';$artifactPath=$prefix+'.revalidation.json'
$expectedLog='BB1E1F90D58A0B21C5A0B3F65F3A3DDB3994388C96FFE4C6A0F3ED5EF174B76F'
$expectedMetadata='D1B7C425CDB53A6356BC9B23C72376857BBE12996234F7032A5846B9B15BEB09'
$validatorPath=Join-Path $PSScriptRoot 'r1c4b-input-isolation-validation.ps1'
$runtimePath=Join-Path $repo 'src/platform/windows/operations/magnet_takeover_probe.cpp'
$binaryPath=Join-Path $repo 'out/r1c4b-live-magnet-debug/src/platform/windows/Debug/panebind-magnet-takeover-probe.exe'
$sourceHash='CEBB558B403B90D3F1A55C521358884BB6C838624EA6C0BC8C5BA481192BC2FB'
$binaryHash='43289B87236C08E68EAC2A23BCA1AD9923454DDB21EBF4DE84447083933DEC6D'
Push-Location $repo
try{
    # Before any validation, independently preserve the exact original bytes.
    $logHash=(Get-FileHash -LiteralPath $evidencePath -Algorithm SHA256).Hash
    $metadataHash=(Get-FileHash -LiteralPath $metadataPath -Algorithm SHA256).Hash
    Assert-IsolationReplayData ($logHash -ceq $expectedLog) 'original JSONL hash mismatch: STOP'
    Assert-IsolationReplayData ($metadataHash -ceq $expectedMetadata) 'original metadata hash mismatch: STOP'
    Assert-IsolationReplayData (-not (Test-Path -LiteralPath $artifactPath)) 'revalidation artifact already exists: never overwrite'
    git check-ignore -- $artifactPath|Out-Null
    Assert-IsolationReplayData ($LASTEXITCODE -eq 0) 'artifact must remain ignored'
    $head=git rev-parse HEAD;Assert-IsolationReplayData ($LASTEXITCODE -eq 0) 'validator fix SHA unavailable'
    $status=@(git status --porcelain);Assert-IsolationReplayData ($LASTEXITCODE -eq 0 -and $status.Count -eq 0) 'require committed clean numeric repair'
    git diff --quiet a67d3049c1027e8b2b82f8e122041b71ff5043ca HEAD -- src
    Assert-IsolationReplayData ($LASTEXITCODE -eq 0) 'native/product runtime changed: STOP'
    Assert-IsolationReplayData ((Get-FileHash -LiteralPath $runtimePath -Algorithm SHA256).Hash -ceq $sourceHash -and (Get-FileHash -LiteralPath $binaryPath -Algorithm SHA256).Hash -ceq $binaryHash) 'current runtime source/binary differ from immutable evidence'
    $validatorHash=(Get-FileHash -LiteralPath $validatorPath -Algorithm SHA256).Hash
    $metadata=Get-Content -LiteralPath $metadataPath -Encoding UTF8|ConvertFrom-Json
    $verdict=$null;$assessment=$null;$exceptionDetails=$null
    try{
        $rows=@(Get-Content -LiteralPath $evidencePath -Encoding UTF8|ForEach-Object {$_|ConvertFrom-Json -ErrorAction Stop})
        # Full corrected computation from JSONL, not a report or saved verdict.
        $verdict=Test-InputIsolationOwnedRecords $rows
        $assessment=Get-IsolationReplayAssessment $metadata $verdict ([long]$rows.Count)
    }catch{
        $assessment=Get-IsolationReplayExceptionAssessment $_.Exception
        $exceptionDetails=[ordered]@{Message=$_.Exception.Message;Type=$_.Exception.GetType().FullName;ScriptStackTrace=$_.ScriptStackTrace}
    }
    $afterLog=(Get-FileHash -LiteralPath $evidencePath -Algorithm SHA256).Hash
    $afterMetadata=(Get-FileHash -LiteralPath $metadataPath -Algorithm SHA256).Hash
    Assert-IsolationReplayData ($afterLog -ceq $logHash -and $afterMetadata -ceq $metadataHash) 'original evidence mutated during replay'
    $afterHead=git rev-parse HEAD;Assert-IsolationReplayData ($LASTEXITCODE -eq 0 -and $afterHead -ceq $head) 'fix SHA changed during replay'
    $afterStatus=@(git status --porcelain);Assert-IsolationReplayData ($LASTEXITCODE -eq 0 -and $afterStatus.Count -eq 0) 'worktree changed during replay'
    Assert-IsolationReplayData ((Get-FileHash -LiteralPath $validatorPath -Algorithm SHA256).Hash -ceq $validatorHash) 'validator changed during replay'
    $afterSource=(Get-FileHash -LiteralPath $runtimePath -Algorithm SHA256).Hash
    $afterBinary=(Get-FileHash -LiteralPath $binaryPath -Algorithm SHA256).Hash
    Assert-IsolationReplayData ($afterSource -ceq $sourceHash -and $afterBinary -ceq $binaryHash) 'runtime source/binary changed during replay'
    $artifact=[ordered]@{
        schema='r1c4b-input-isolation-revalidation/v1'
        original_evidence_path=$evidencePath;original_evidence_sha256=$logHash
        original_metadata_path=$metadataPath;original_metadata_sha256=$metadataHash
        original_evidence_sha256_after=$afterLog;original_metadata_sha256_after=$afterMetadata
        evidence_implementation_sha='a67d3049c1027e8b2b82f8e122041b71ff5043ca'
        validator_fix_sha=$head;validator_script_sha256=$validatorHash;replay_time=[DateTime]::UtcNow.ToString('o')
        original_result='INVALID_EVIDENCE';replay_result=$assessment.ReplayResult
        first_semantic_failure=$assessment.FirstSemanticFailure;semantic_contract_version='post_end_shield_v1';numeric_fix_only=$true
        CorrectedValidatorVerified=$assessment.CorrectedValidatorVerified;ValidatorWorktreeClean=$true
        OriginalBinarySHA256=$binaryHash;RuntimeSourceSHA256=$sourceHash
        AfterBinarySHA256=$afterBinary;AfterRuntimeSourceSHA256=$afterSource
        replay_verdict=$verdict;exception=$exceptionDetails
    }
    # CreateNew is atomic and will never replace the original files or an earlier
    # replay artifact. This is the only evidence write in this program.
    $stream=[IO.File]::Open($artifactPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try{$bytes=[Text.UTF8Encoding]::new($false).GetBytes(($artifact|ConvertTo-Json -Depth 24));$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
    Write-Host "Fix E immutable replay artifact: $artifactPath"
    $artifact|ConvertTo-Json -Depth 24|Write-Host
    if($assessment.ReplayResult -cne 'PASS'){exit 2}
    exit 0
}finally{Pop-Location}
