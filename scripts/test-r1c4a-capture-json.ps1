[CmdletBinding()]
param(
    [string]$BuildDirectory='out/r1c4-topology-debug',
    [ValidateSet('Debug','Release')][string]$Configuration='Debug'
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'r1c4a-evidence-validation.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$build=if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $repo $BuildDirectory}
$exe=Join-Path $build "src/platform/windows/$Configuration/panebind-group-capture-tests.exe"
if(-not(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'Synthetic capture test binary missing'}
$samples=@(& $exe --diagnostic-samples)
if($LASTEXITCODE -ne 0 -or $samples.Count -ne 4){throw 'Unexpected serializer fixture output'}
$stages=@('Binding','NativeValidation','ReceiptHealth','Context')
for($i=0;$i -lt $samples.Count;++$i){
    $capture=$samples[$i]|ConvertFrom-Json -ErrorAction Stop
    Assert-C4A ($capture.failure_stage -ceq $stages[$i]) 'serializer stage order'
    $preview=[pscustomobject]@{capture=$capture;valid=$capture.succeeded;ready=$false;snapshots=$null}
    Assert-C4AStructuredCapture $preview
}
Write-Host "R1C4A_CAPTURE_SERIALIZER = PASS ($Configuration; 4 synthetic stages)"
