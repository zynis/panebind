Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# Pure selection/AST fixtures only. No CTest, executable, HWND, input or Git.
$checks=0
function Check-OfflineSelection([bool]$Ok,[string]$Reason){if(-not $Ok){throw "offline selection fixture: $Reason"};$script:checks++}
function Reject-OfflineSelection([scriptblock]$Action,[string]$Reason){
    $rejected=$false;try{$null=& $Action}catch{$rejected=$true};Check-OfflineSelection $rejected $Reason
}
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'run-r1c4b-offline-tests.ps1'),[ref]$tokens,[ref]$errors)
Check-OfflineSelection (@($errors).Count -eq 0) 'entry parses'
foreach($name in @('Assert-OfflineTestGate','Get-OfflineTestAudit','Select-OfflineTestInventory','Get-OfflineTestRegex')){
    $nodes=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name},$true))
    Check-OfflineSelection ($nodes.Count -eq 1) "one actual helper $name"
    $allowed=@('Assert-OfflineTestGate','Get-OfflineTestAudit','Where-Object','Sort-Object','ForEach-Object')
    Check-OfflineSelection (@($nodes[0].Body.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -cnotin $allowed},$true)).Count -eq 0) "$name cannot execute a CLI or perform IO"
    . ([scriptblock]::Create($nodes[0].Extent.Text))
}
$build=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../out/synthetic-offline-selection'))
$audit=Get-OfflineTestAudit
function Reset-OfflineSelection{
    $script:inventory=[pscustomobject]@{kind='ctestInfo';version=[pscustomobject]@{major=1;minor=0};tests=@(
        foreach($name in $script:audit.Keys){
            $entry=$script:audit[$name]
            $relative=if($entry.Windows){"src/platform/windows/Debug/$($entry.Target).exe"}else{"Debug/$($entry.Target).exe"}
            [pscustomobject]@{name=$name;command=@([IO.Path]::GetFullPath([IO.Path]::Combine($script:build,$relative)));properties=@([pscustomobject]@{name='LABELS';value=@($entry.Phase)})}
        }
    )}
}
Reset-OfflineSelection
$names=@(Select-OfflineTestInventory $inventory $build Debug)
Check-OfflineSelection ($names.Count -eq @($audit.Keys|Where-Object {$audit[$_].Phase -ceq 'offline'}).Count -and $names.Count -gt 0) 'positive subset includes every audited offline command'
Check-OfflineSelection ('windows-magnet-owned-probe' -cnotin $names -and $audit['windows-magnet-owned-probe'].Phase -ceq 'interactive') 'owned geometry HWND probe is explicitly interactive'
foreach($name in @('windows-explorer-glue-activation-unit','windows-explorer-vdm-unit','windows-explorer-glue-event-source-unit','explorer-group-batch','explorer-group-event','sta-console-pump')){
    Check-OfflineSelection ($audit[$name].Phase -ceq 'interactive' -and $name -cnotin $names) "$name is excluded even if its name says unit"
}
$regex=Get-OfflineTestRegex $names
foreach($name in $audit.Keys){Check-OfflineSelection ([regex]::IsMatch($name,$regex) -eq ($audit[$name].Phase -ceq 'offline')) "exact positive regex $name"}
Reject-OfflineSelection {Get-OfflineTestRegex @()} 'empty regex cannot fall through to all tests'
$escaped=Get-OfflineTestRegex @('literal.+(x)')
Check-OfflineSelection ([regex]::IsMatch('literal.+(x)',$escaped) -and -not [regex]::IsMatch('literalZxx',$escaped)) 'regex metacharacters are escaped'
Reset-OfflineSelection;$inventory.tests=@();Reject-OfflineSelection {Select-OfflineTestInventory $inventory $build Debug} 'zero inventory rejected'
Reset-OfflineSelection;$inventory.version.major=2;Reject-OfflineSelection {Select-OfflineTestInventory $inventory $build Debug} 'unsupported inventory rejected'
Reset-OfflineSelection;$inventory.tests+=@($inventory.tests[0]);Reject-OfflineSelection {Select-OfflineTestInventory $inventory $build Debug} 'duplicate registration rejected'
Reset-OfflineSelection;$inventory.tests=@($inventory.tests|Where-Object name -CNE 'windows-magnet-owned-probe');Reject-OfflineSelection {Select-OfflineTestInventory $inventory $build Debug} 'omitted interactive registration is not silently ignored'
Reset-OfflineSelection;$inventory.tests+=@([pscustomobject]@{name='unreviewed-unit';command=@('unreviewed.exe');properties=@([pscustomobject]@{name='LABELS';value=@('offline')})});Reject-OfflineSelection {Select-OfflineTestInventory $inventory $build Debug} 'unclassified unit name cannot acquire authority from an offline label'
foreach($fault in @('missing','mixed','unknown','wrong')){
    Reset-OfflineSelection
    switch($fault){'missing'{$inventory.tests[0].properties=@()};'mixed'{$inventory.tests[0].properties[0].value=@('offline','interactive')};'unknown'{$inventory.tests[0].properties[0].value=@('unclassified')};'wrong'{$inventory.tests[0].properties[0].value=@('interactive')}}
    Reject-OfflineSelection {Select-OfflineTestInventory $inventory $build Debug} "$fault offline phase label rejected"
}
Reset-OfflineSelection;$unsafe=@($inventory.tests|Where-Object name -CEQ 'windows-magnet-owned-probe')[0];$unsafe.properties[0].value=@('offline');Reject-OfflineSelection {Select-OfflineTestInventory $inventory $build Debug} 'interactive command relabeled offline rejected'
Reset-OfflineSelection;$inventory.tests[0].command+=@('--new-argument');Reject-OfflineSelection {Select-OfflineTestInventory $inventory $build Debug} 'unreviewed arguments rejected'
Reset-OfflineSelection;$inventory.tests[0].command=@('C:\Windows\notepad.exe');Reject-OfflineSelection {Select-OfflineTestInventory $inventory $build Debug} 'unreviewed/outside executable rejected'
foreach($property in @('FIXTURES_REQUIRED','FIXTURES_SETUP','FIXTURES_CLEANUP','DEPENDS')){
    Reset-OfflineSelection;$inventory.tests[0].properties+=@([pscustomobject]@{name=$property;value='interactive-child'})
    Reject-OfflineSelection {Select-OfflineTestInventory $inventory $build Debug} "$property hidden execution expansion rejected"
}
Reset-OfflineSelection
$preview=[pscustomobject]@{kind=$inventory.kind;version=$inventory.version;tests=@($inventory.tests|Where-Object {$audit[$_.name].Phase -ceq 'offline'})}
$selected=@(Select-OfflineTestInventory $preview $build Debug $true)
Check-OfflineSelection (($selected -join [char]0) -ceq ($names -join [char]0)) 'actual filtered preview equals the audited positive set'
$preview.tests+=@(@($inventory.tests|Where-Object name -CEQ 'windows-magnet-owned-probe')[0]);Reject-OfflineSelection {Select-OfflineTestInventory $preview $build Debug $true} 'interactive preview contamination is refused before execution'
$execution=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.CommandElements[0].Extent.Text -ceq '$ctest' -and $n.Extent.Text -ceq '& $ctest @runArguments'},$true))
Check-OfflineSelection ($execution.Count -eq 1) 'only one actual execution call exists'
$arguments=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$runArguments'},$true))
Check-OfflineSelection ($arguments.Count -eq 1 -and $arguments[0].Right.Extent.Text.Contains('$selectionArguments+') -and $arguments[0].Right.Extent.Text.Contains('--stop-on-failure')) 'execution reuses positive preview arguments and first-failure stop'
$selection=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$selectionArguments'},$true))
Check-OfflineSelection ($selection.Count -eq 1 -and $selection[0].Right.Extent.Text.Contains("'-R'") -and $selection[0].Right.Extent.Text.Contains("'^offline$'") -and $selection[0].Right.Extent.Text.Contains("'--no-tests=error'")) 'name regex, anchored positive label and no-tests error stay explicit'
$equalGate=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -ceq 'Assert-OfflineTestGate' -and $n.Extent.Text.Contains('Actual selected commands differ')},$true))
Check-OfflineSelection ($equalGate.Count -eq 1 -and $equalGate[0].Extent.EndOffset -lt $execution[0].Extent.StartOffset) 'preview equality check precedes every execution'
$selected=@('missing-one');$names=@('expected-one')
$expression=[scriptblock]::Create($equalGate[0].CommandElements[1].Extent.Text)
Check-OfflineSelection (-not (& $expression)) 'missing/changed selected names fail the actual equality predicate'
$list=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text -ceq '$ListOnly'},$true))
Check-OfflineSelection ($list.Count -eq 1 -and $list[0].Extent.EndOffset -lt $execution[0].Extent.StartOffset -and $list[0].Extent.Text.Contains('exit 0')) 'list-only branch cannot run tests'
Write-Host "offline-selection synthetic_only=true checks=$checks PASS; no CTest/childCLI/GUI/input/Git"
