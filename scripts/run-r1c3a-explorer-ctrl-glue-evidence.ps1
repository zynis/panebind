[CmdletBinding()]
param(
    [Parameter(ParameterSetName = 'Run')]
    [ValidateNotNullOrEmpty()]
    [string] $BuildDirectory = 'out/r1c3a-debug',

    [Parameter(ParameterSetName = 'Run')]
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Debug',

    [Parameter(ParameterSetName = 'Run')]
    [ValidateRange(30, 300)]
    [int] $GlueTimeoutSeconds = 120,

    [Parameter(ParameterSetName = 'Run')]
    [ValidateRange(120, 1800)]
    [int] $ObserveSeconds = 300,

    [Parameter(Mandatory = $true, ParameterSetName = 'ValidateEvidence')]
    [ValidateNotNullOrEmpty()]
    [string] $ValidateEvidencePrefix,

    [Parameter(Mandatory = $true, ParameterSetName = 'ValidateEvidence')]
    [ValidateRange(0, 255)]
    [int] $ValidationHarnessExitCode,

    [Parameter(ParameterSetName = 'ValidateEvidence')]
    [ValidateRange(0, 255)]
    [int] $ValidationObserverExitCode = 0
)

# One strict shared validator; this entry point cannot select another profile.
$arguments = @{ EvidenceProfile = 'R1C3A' }
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
