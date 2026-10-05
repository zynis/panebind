$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
function Read-ActualAst([string]$RelativePath) {
    $tokens = $null; $errors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $repo $RelativePath),
        [ref]$tokens,[ref]$errors)
    if ($errors.Count) { throw "actual_script_parse_error:$RelativePath" }
    return $ast
}
function Import-ActualFunction($Ast,[string]$Name) {
    $function = $Ast.Find({param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -ceq $Name
    },$true)
    if (-not $function) { throw "actual_entry_function_missing:$Name" }
    return $function.Extent.Text
}
$driver = Read-ActualAst 'scripts\r1c4b-mvp1-explorer-guest-driver.ps1'
$runner = Read-ActualAst 'scripts\r1c4b-mvp1-shield-sandbox-guest.ps1'
. ([scriptblock]::Create((Import-ActualFunction $driver 'Test-DriverModeManifest')))
. ([scriptblock]::Create((Import-ActualFunction $runner 'Get-ExplorerConfigurations')))
foreach ($case in @(
    @{mode='AutomatedValidation'; configuration='debug'},
    @{mode='AutomatedValidation'; configuration='release'},
    @{mode='BindOnlyIndependentReview'; configuration='release'}
)) {
    $Mode = $case.mode; $Configuration = $case.configuration
    Test-DriverModeManifest ([pscustomobject]@{
        explorer_guest_mode=$Mode; independent_review_configuration='release'
    })
}
$rejected = 0
foreach ($case in @(
    @{mode='BindOnlyIndependentReview'; configuration='debug'; manifest='BindOnlyIndependentReview'},
    @{mode='BindOnlyIndependentReview'; configuration='release'; manifest='AutomatedValidation'},
    @{mode='AutomatedValidation'; configuration='release'; manifest=$null}
)) {
    $Mode=$case.mode; $Configuration=$case.configuration
    try { Test-DriverModeManifest ([pscustomobject]@{
        explorer_guest_mode=$case.manifest; independent_review_configuration='release'
    }) } catch { $rejected++ }
}
if ($rejected -ne 3) { throw 'actual_review_mode_guard_not_strict' }
if ((@(Get-ExplorerConfigurations 'AutomatedValidation') -join ',') -cne 'debug,release' -or
    (@(Get-ExplorerConfigurations 'BindOnlyIndependentReview') -join ',') -cne 'release') {
    throw 'actual_runner_configuration_route_changed'
}
# Inspect the ACTUAL review branch: it ends before the subsequent gesture
# calls and contains no input helper/native start. This is an offline branch
# contract check, not evidence of a live usable product or manual UAT.
$review = @($driver.FindAll({param($node)
    $node -is [Management.Automation.Language.IfStatementAst] -and
    $node.Extent.Text.Contains("Record 'review_bound'")
},$true))
if ($review.Count -ne 1 -or
    -not $review[0].Extent.Text.Contains("`$Mode -ceq 'BindOnlyIndependentReview'")) {
    throw 'actual_bind_only_branch_missing'
}
$commands = @($review[0].FindAll({param($node)
    $node -is [Management.Automation.Language.CommandAst]
},$true) | ForEach-Object { $_.GetCommandName() })
if (@($commands | Where-Object { $_ -in @('Invoke-Drag','Send-MouseMove','Console-Line') }).Count -ne 0 -or
    @($review[0].FindAll({param($node) $node -is [Management.Automation.Language.ExitStatementAst]},$true)).Count -ne 1) {
    throw 'actual_bind_only_branch_would_enter_automatic_gestures'
}
Write-Output 'actual_guest_bind_only_entry: strict manifest/config guard; Release-only route; exit before automated gestures PASS (offline, no native/UI/input)'
