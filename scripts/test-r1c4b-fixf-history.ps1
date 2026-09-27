[CmdletBinding()]
param([string]$ArtifactPath)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4b-input-isolation-validation.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$evidence=Join-Path $repo 'uat/r1c4b-input-isolation'
$d='20260926T201554647Z-Debug-BottomResize-3e6236754a004decb816079530b3861b'
$e='20260927T095946432Z-Debug-Move-521d2098b1504bdfaa5366d029a56a9f'
$bindings=[ordered]@{
    ($d+'.jsonl')='BB1E1F90D58A0B21C5A0B3F65F3A3DDB3994388C96FFE4C6A0F3ED5EF174B76F'
    ($d+'.metadata.json')='D1B7C425CDB53A6356BC9B23C72376857BBE12996234F7032A5846B9B15BEB09'
    ($d+'.revalidation.json')='27B68CB5923CC65B52CCB3EEE75F32095D6E320A00E127590660730159613A6F'
    ($e+'.jsonl')='DACA83DC3AA9C5B43E7295B67E16E242526E240E5429C6C949A93C59C6CD03F8'
    ($e+'.metadata.json')='3F733C50D15DAFADA70D538EFFA33899ECB8C6EC32F25BFC0D28F69B89D53D16'
    '20260927T094931145Z-fixe-gates-c37c0342d5594a399685b57ceef982f2.json'='C97A4C6D02F9B091773E9744B2025DEC0274B3A7F148444B2D67C07209D6257C'
}
function Assert-History([bool]$Value,[string]$Reason){if(-not $Value){throw "Fix F historical compatibility: $Reason"}}
function Assert-HistoryHashes {
    foreach($name in $bindings.Keys){Assert-History ((Get-FileHash -LiteralPath (Join-Path $evidence $name) -Algorithm SHA256).Hash -ceq $bindings[$name]) "immutable $name"}
    Assert-History ((Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'r1c4b-input-isolation-validation.ps1') -Algorithm SHA256).Hash -ceq '9FFB838F9B530257774B1E53B61102E9C99166D0926D37300350DBF7B6A4719D') 'unchanged frozen validator'
}
Assert-HistoryHashes
$dVerdict=Test-InputIsolationOwnedEvidence -Path (Join-Path $evidence ($d+'.jsonl'))
$eVerdict=Test-InputIsolationOwnedEvidence -Path (Join-Path $evidence ($e+'.jsonl'))
Assert-History ($dVerdict.Result -ceq 'PASS' -and $dVerdict.Takeover -ceq 'PASS') 'full old corrected D replay still PASS'
Assert-History ($eVerdict.Result -ceq 'BLOCKED' -and $eVerdict.CleanupInputRelease -ceq 'SKIPPED_NO_AUTHORITY' -and $eVerdict.CleanupCurrentLeftDown -eq $true) 'old blocked cleanup must not become new cleanup PASS'
Assert-HistoryHashes
$result=[ordered]@{Schema='r1c4b-fixf-history-compatibility/v1';ReadOnlyOriginals=$true;NewGuiRuns=0;HistoricalHashes=$bindings;FrozenValidatorSHA256='9FFB838F9B530257774B1E53B61102E9C99166D0926D37300350DBF7B6A4719D';DReplayResult=$dVerdict.Result;EBlockedResult=$eVerdict.Result;EOldCleanupResult=$eVerdict.CleanupInputRelease;HistoricalCurrentStateClaim='NONE';HistoricalFormal='13_PASS_THEN_BLOCKED_AT_14';Result='PASS'}
if($ArtifactPath){
    $root=[IO.Path]::GetFullPath((Join-Path $repo 'uat/r1c4b-fixf'))+[IO.Path]::DirectorySeparatorChar
    $target=[IO.Path]::GetFullPath($ArtifactPath)
    Assert-History ($target.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)) 'new artifact outside Fix F ignored directory'
    $bytes=[Text.Encoding]::UTF8.GetBytes(($result|ConvertTo-Json -Depth 16)+[Environment]::NewLine)
    $stream=[IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    try{$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
}
$result|ConvertTo-Json -Depth 16
