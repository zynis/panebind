[CmdletBinding()]
param(
    [Parameter(ParameterSetName='Run')]
    [string] $BuildDirectory = 'out/r1c3b-debug',
    [Parameter(ParameterSetName='Run')]
    [ValidateSet('Debug','Release')]
    [string] $Configuration = 'Debug',
    [Parameter(ParameterSetName='Run')]
    [ValidateRange(30,300)]
    [int] $GlueTimeoutSeconds = 120,
    [Parameter(ParameterSetName='Run')]
    [ValidateRange(120,1800)]
    [int] $ObserveSeconds = 300,
    [Parameter(ParameterSetName='Run')]
    [bool] $ExternalObserverEnabled = $true,
    [Parameter(Mandatory=$true,ParameterSetName='ValidateEvidence')]
    [string] $ValidateEvidencePrefix,
    [Parameter(Mandatory=$true,ParameterSetName='ValidateEvidence')]
    [ValidateRange(0,255)]
    [int] $ValidationHarnessExitCode,
    [Parameter(ParameterSetName='ValidateEvidence')]
    [ValidateRange(0,255)]
    [int] $ValidationObserverExitCode = 0
)
# Reserved future comparison switch; first Phase 1 human profile must retain
# independent Observer evidence. No internal-only acceptance path is enabled.
if ($PSCmdlet.ParameterSetName -eq 'Run' -and -not $ExternalObserverEnabled) {
    throw 'ExternalObserverEnabled=false is reserved for a future controlled comparison; Phase 1 UAT requires ON.'
}
$arguments = @{ EvidenceProfile = 'R1C3B' }
if ($PSCmdlet.ParameterSetName -eq 'Run') {
    $arguments.BuildDirectory = $BuildDirectory
    $arguments.Configuration = $Configuration
    $arguments.GlueTimeoutSeconds = $GlueTimeoutSeconds
    $arguments.ObserveSeconds = $ObserveSeconds
} else {
    $arguments.ValidateEvidencePrefix = $ValidateEvidencePrefix
    $arguments.ValidationHarnessExitCode = $ValidationHarnessExitCode
    $arguments.ValidationObserverExitCode = $ValidationObserverExitCode
}
& (Join-Path $PSScriptRoot 'run-r1c2b-explorer-glue-evidence.ps1') @arguments
exit $LASTEXITCODE
