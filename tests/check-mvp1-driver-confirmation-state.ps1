$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$source = Join-Path $repo 'scripts\r1c4b-mvp1-explorer-guest-driver.ps1'
$tokens = $null; $errors = $null
$current = [Management.Automation.Language.Parser]::ParseFile($source,[ref]$tokens,[ref]$errors)
if ($errors.Count) { throw 'driver_parse_error' }
# Preserve the real failing implementation as the counterexample. Only its
# collection statements are executed; guest startup, Win32 and input are not.
$oldText = (& git -C $repo show 'b29a587153ac08895391d7a699d880447ad32a53:scripts/r1c4b-mvp1-explorer-guest-driver.ps1' | Out-String)
if ($LASTEXITCODE -ne 0) { throw 'failing_driver_source_unavailable' }
$old = [Management.Automation.Language.Parser]::ParseInput($oldText,[ref]$tokens,[ref]$errors)
if ($errors.Count) { throw 'failing_driver_parse_error' }
function Assignment($Ast,[string]$Name,$Operator) {
    $matches = @($Ast.FindAll({param($node)
        $node -is [Management.Automation.Language.AssignmentStatementAst] -and
        $node.Left.Extent.Text -ceq $Name -and $node.Operator -eq $Operator
    },$true))
    if ($matches.Count -ne 1) { throw "actual_collection_assignment_missing:$Name/$Operator" }
    return $matches[0].Extent.Text
}
$equals = [Management.Automation.Language.TokenKind]::Equals
$plusEquals = [Management.Automation.Language.TokenKind]::PlusEquals
$oldInit = Assignment $old '$script:confirmed' $equals
$oldRead = Assignment $old '$confirmed' $equals
$oldAppend = Assignment $old '$script:confirmed' $plusEquals
$newInit = Assignment $current '$script:confirmationRows' $equals
$newRead = Assignment $current '$confirmed' $equals
$newAppend = Assignment $current '$script:confirmationRows' $plusEquals

# ConvertFrom-Json produces the same PSObject class as the actual product log
# reader, not a hashtable with different addition behavior.
$script:fixtureRows = @('[{"member":0,"confirmation_generation":4},' +
    '{"member":1,"confirmation_generation":12},' +
    '{"member":2,"confirmation_generation":20}]' | ConvertFrom-Json)
function Wait-ProductRow { return $script:fixtureRows[$member] }
$member = 0
. ([scriptblock]::Create($oldInit))
. ([scriptblock]::Create($oldRead))
$oldRejected = $false
try { . ([scriptblock]::Create($oldAppend)) }
catch {
    if ($_.Exception.Message -notmatch 'op_Addition') { throw }
    $oldRejected = $true
}
if (-not $oldRejected) { throw 'original_script_scope_collision_not_reproduced' }

. ([scriptblock]::Create($newInit))
for ($member = 0; $member -lt 3; $member++) {
    . ([scriptblock]::Create($newRead))
    . ([scriptblock]::Create($newAppend))
}
if (@($script:confirmationRows).Count -ne 3) { throw 'actual_confirmation_collection_incomplete' }
for ($member = 0; $member -lt 3; $member++) {
    if ($script:confirmationRows[$member].member -ne $member -or
        $script:confirmationRows[$member].confirmation_generation -ne
            $script:fixtureRows[$member].confirmation_generation) {
        throw 'actual_confirmation_collection_lost_identity_or_generation'
    }
}
# Check the other top-level script arrays for this exact scalar-name mistake.
$arrays = @($current.FindAll({param($node)
    $node -is [Management.Automation.Language.AssignmentStatementAst] -and
    $node.Left.Extent.Text.StartsWith('$script:') -and $node.Operator -eq $equals -and
    $node.Right.Extent.Text -ceq '@()'
},$true))
foreach ($array in $arrays) {
    $localName = '$' + $array.Left.Extent.Text.Substring('$script:'.Length)
    $collisions = @($current.FindAll({param($node)
        $node -is [Management.Automation.Language.AssignmentStatementAst] -and
        $node.Left.Extent.Text -ceq $localName
    },$true))
    if ($collisions.Count) { throw "script_array_scalar_collision:$localName" }
}
Write-Output 'actual_guest_driver_state: old PSObject op_Addition reproduced; new A/B/C collection and array-name isolation PASS (offline, no native/UI/input)'
