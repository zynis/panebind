[CmdletBinding()]
param(
    [Parameter(ParameterSetName='Run')][string] $BuildDirectory = 'out/r1c3b-debug',
    [Parameter(ParameterSetName='Run')][ValidateSet('Debug')][string] $Configuration = 'Debug',
    [Parameter(ParameterSetName='Run')][ValidateRange(30,300)][int] $GlueTimeoutSeconds = 120,
    [Parameter(ParameterSetName='Run')][ValidateRange(120,1800)][int] $ObserveSeconds = 300,
    [Parameter(Mandatory=$true,ParameterSetName='ValidateEvidence')][string] $ValidateEvidencePrefix,
    [Parameter(Mandatory=$true,ParameterSetName='ValidateEvidence')][ValidateRange(0,255)][int] $ValidationHarnessExitCode,
    [Parameter(ParameterSetName='ValidateEvidence')][ValidateRange(0,255)][int] $ValidationObserverExitCode = 0,
    [switch] $VerboseOperations
)
$arguments = @{ EvidenceProfile = 'R1C3B3' }
foreach ($key in $PSBoundParameters.Keys) { $arguments[$key] = $PSBoundParameters[$key] }
if ($PSCmdlet.ParameterSetName -eq 'Run') {
    $arguments.BuildDirectory = $BuildDirectory; $arguments.Configuration = $Configuration
    $arguments.GlueTimeoutSeconds = $GlueTimeoutSeconds; $arguments.ObserveSeconds = $ObserveSeconds
}
& (Join-Path $PSScriptRoot 'run-r1c2b-explorer-glue-evidence.ps1') @arguments
exit $LASTEXITCODE
