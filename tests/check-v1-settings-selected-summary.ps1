[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$guest=Join-Path (Split-Path -Parent $PSScriptRoot) 'scripts\v1-sandbox-guest.ps1'
$tokens=$null;$parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($guest,[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw 'Guest script does not parse.'}
# Extract the production converters, not the guest mainline or its guards.
foreach($name in @('Convert-SettingsEditorsPhaseEvidence','Read-SettingsEditorsPhaseEvidence')){
    $functions=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $name},$true))
    if($functions.Count -ne 1){throw ('Production converter not unique: '+$name)}
    Invoke-Expression $functions[0].Extent.Text
}
$script:checks=0
function Assert([bool]$Condition,[string]$Why){if(-not $Condition){throw $Why};$script:checks++}
function New-Row([string]$Type,[System.Collections.IDictionary]$Fields){
    $script:sequence++
    $record=[ordered]@{sequence=$script:sequence;type=$Type;source_sha=('c'*40);automated_guest_driver=$true}
    foreach($key in $Fields.Keys){$record[$key]=$Fields[$key]}
    [pscustomobject]$record
}
function New-SelectedRows([int]$FailAt=0,[switch]$NativeFailure,[switch]$UnsafeHex){
    $script:sequence=0
    $names=@('T01_six_original_size_pages','T01_narrow_real_controls','T02_structured_hotkey','T08_immediate_runtime_controls',
        'T09_ordinary_ZIP_and_UI_exit','T10_five_finite_actual_demonstrations','T07_RGB_picker_and_preview')
    $ids=@('T01','T01','T02','T08','T09','T10','T07')
    for($n=0;$n -lt 7;$n++){
        New-Row 'settings_ui_targeted_phase' @{name=$names[$n];phase_id=$ids[$n];order=$n+1;selected=$true;status='SELECTED'}
    }
    foreach($id in @('T03','T04','T05','T06')){
        New-Row 'settings_ui_targeted_phase' @{phase_id=$id;selected=$false;status='NOT_SELECTED_HISTORICAL_NOT_RERUN'}
    }
    for($n=0;$n -lt 7;$n++){
        $fields=@{name=$names[$n];phase_id=$ids[$n];order=$n+1;selected=$true}
        if($FailAt -and $n+1 -gt $FailAt){$fields.status='NOT_EXECUTED';New-Row 'settings_ui_targeted_phase' $fields;continue}
        $fields.status='STARTED';New-Row 'settings_ui_targeted_phase' $fields
        New-Row 'case' @{case=('actual_'+$ids[$n]);result=$(if($n+1 -eq $FailAt){'FAIL'}else{'PASS'})}
        if($n+1 -eq $FailAt){
            $fields.status='FAILED';$fields.reason='bounded_tool_exception';New-Row 'settings_ui_targeted_phase' $fields
            New-Row 'settings_editors_failure' @{reason='bounded_tool_exception';original_failure_preserved=$true};continue
        }
        if($n -eq 6){
            New-Row 'settings_ui_T07_route' @{route='system_picker';status='STARTED'}
            if($NativeFailure){
                New-Row 'case' @{case='native_picker_confirmation';result='FAIL'}
                New-Row 'settings_ui_T07_route' @{route='system_picker';status='FAILED';reason='native_tool_failure'}
                if(-not $UnsafeHex){New-Row 'case' @{case='settings_review_actual_safe_idle_before_continuation';stage='T07_failed_native_picker_before_HEX_route';result='PASS'}}
                New-Row 'settings_ui_T07_route' @{route='HEX_second_color';status='STARTED';native_failure_preserved=$true}
                New-Row 'case' @{case='second_color_actual_preview';result='PASS'}
                New-Row 'settings_ui_T07_route' @{route='HEX_second_color';status='RETURNED';actual_preview_pixel_checked=$true;return_not_combined_T07_PASS=$true}
            }else{
                New-Row 'settings_ui_T07_route' @{route='HEX_second_color';status='NOT_EXECUTED';reason='system_picker_confirmed_the_second_color'}
                New-Row 'settings_ui_T07_route' @{route='system_picker';status='RETURNED';actual_preview_pixel_checked=$true;return_not_combined_T07_PASS=$true}
            }
        }
        $fields.status='RETURNED';$fields.return_not_ui_PASS=$true;New-Row 'settings_ui_targeted_phase' $fields
        if($n -eq 6 -and $NativeFailure){
            $fields.status='FAILED';$fields.retained_subtest_failure=$true;$fields.function_returned_after_safe_recovery=$true
            New-Row 'settings_ui_targeted_phase' $fields
        }
    }
    New-Row 'settings_editors_summary' @{suite='SettingsEditorsOnly';result=$(if($FailAt -or $NativeFailure){'FAIL'}else{'PASS_SELECTED_SCOPE_NOT_ALL_T01_T10'});
        selected_ids=@('T01','T02','T08','T09','T10','T07');not_rerun_historical_ids=@('T03','T04','T05','T06');whole_contract_PASS=$false}
}
$normal=@(New-SelectedRows)
$result=Convert-SettingsEditorsPhaseEvidence $normal
Assert ($result.evidence_status -ceq 'RECORDED_SELECTED_EXECUTION_REQUIRES_REVIEW') 'Valid selected order was not retained.'
Assert (-not $result.all_T01_T10_PASS -and $result.visual_review -ceq 'NOT_RUN' -and $result.phases.Count -eq 7) 'A return incorrectly became all-ten/visual PASS.'
Assert (($result.selected_order -join ',') -ceq 'T01,T02,T08,T09,T10,T07' -and ($result.not_selected_historical -join ',') -ceq 'T03,T04,T05,T06') 'Historical excluded phases were rebound.'
$early=Convert-SettingsEditorsPhaseEvidence @(New-SelectedRows -FailAt 3)
Assert ($early.evidence_status -ceq 'RECORDED_WITH_FAILURES' -and $early.phases[2].execution -ceq 'FAILED' -and $early.tool_exceptions.Count -eq 1) 'Early tool failure was hidden.'
Assert ($early.phases[3].execution -ceq 'NOT_EXECUTED' -and -not $early.phases[3].executed -and $early.T07_routes[0].execution -ceq 'NOT_EXECUTED_PHASE_NOT_EXECUTED') 'Unrun later phases were granted completion.'
$native=Convert-SettingsEditorsPhaseEvidence @(New-SelectedRows -NativeFailure)
Assert ($native.evidence_status -ceq 'RECORDED_WITH_FAILURES' -and $native.T07_routes[0].execution -ceq 'FAILED' -and $native.T07_routes[1].execution -ceq 'RETURNED_REQUIRES_REVIEW') 'Native failure was erased by safe HEX return.'
Assert ($native.phases[3].returned -and $native.phases[4].returned -and $native.phases[5].returned -and $native.phases[6].failed) 'T07 failure revoked the earlier runtime/portable/demo facts.'
$unsafe=Convert-SettingsEditorsPhaseEvidence @(New-SelectedRows -NativeFailure -UnsafeHex)
Assert ($unsafe.evidence_status -ceq 'INCOMPLETE' -and $unsafe.evidence_errors -ccontains 'T07_HEX_safety_gate_unknown_or_failed') 'Unknown safety before HEX was treated as evidence.'
$wrong=@(New-SelectedRows)
@($wrong|Where-Object {$_.type -ceq 'settings_ui_targeted_phase' -and $_.status -ceq 'STARTED'})[0].order=2
Assert ((Convert-SettingsEditorsPhaseEvidence $wrong).evidence_status -ceq 'INCOMPLETE') 'Wrong production phase order was not detected.'
$missing=@(New-SelectedRows|Where-Object {-not ($_.type -ceq 'settings_ui_targeted_phase' -and $_.name -ceq 'T01_narrow_real_controls' -and $_.status -ceq 'SELECTED')})
Assert ((Convert-SettingsEditorsPhaseEvidence $missing).evidence_status -ceq 'INCOMPLETE') 'Missing selection was treated as complete.'
$gap=@(New-SelectedRows|Where-Object {-not ($_.type -ceq 'case' -and $_.case -ceq 'actual_T02')})
$gapResult=Convert-SettingsEditorsPhaseEvidence $gap
Assert ($gapResult.evidence_status -ceq 'INCOMPLETE' -and $gapResult.evidence_errors -ccontains 'driver_sequence_missing_or_not_contiguous_from_one') 'A missing middle assertion was hidden by forward-only sequence checking.'
$unknown=@(New-SelectedRows)
@($unknown|Where-Object {$_.type -ceq 'case'})[0].result='UNKNOWN'
$unknownResult=Convert-SettingsEditorsPhaseEvidence $unknown
Assert ($unknownResult.evidence_status -ceq 'INCOMPLETE' -and $unknownResult.phases[0].case_UNKNOWN -eq 1) 'An unknown actual assertion lost its count or was accepted.'
$wrongRoute=@(New-SelectedRows)
$firstCase=@($wrongRoute|Where-Object {$_.type -ceq 'case'})[0]
$firstCase.type='settings_ui_T07_route'
$firstCase|Add-Member NoteProperty route 'system_picker'
$firstCase|Add-Member NoteProperty status 'STARTED'
$wrongRouteResult=Convert-SettingsEditorsPhaseEvidence $wrongRoute
Assert ($wrongRouteResult.evidence_status -ceq 'INCOMPLETE' -and $wrongRouteResult.evidence_errors -ccontains 'T07_route_outside_active_phase') 'A different phase was allowed to supply T07 route evidence.'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\')
$scratch=Join-Path $root ('uat\v1-control\settings-summary-offline-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$raw=Join-Path $scratch 'synthetic-driver.jsonl'
try{
    [IO.File]::WriteAllLines($raw,@($normal|ForEach-Object{$_|ConvertTo-Json -Compress -Depth 10}),(New-Object Text.UTF8Encoding($false)))
    $read=Read-SettingsEditorsPhaseEvidence $raw ('c'*40)
    Assert ($read.evidence_status -ceq 'RECORDED_SELECTED_EXECUTION_REQUIRES_REVIEW' -and $read.raw_log_sha256 -ceq (Get-FileHash -LiteralPath $raw -Algorithm SHA256).Hash) 'The actual bounded production reader lost the raw hash.'
    Assert ((Read-SettingsEditorsPhaseEvidence $raw ('d'*40)).evidence_status -ceq 'INCOMPLETE') 'A different driver identity was accepted.'
    [IO.File]::WriteAllLines($raw,@($gap|ForEach-Object{$_|ConvertTo-Json -Compress -Depth 10}),(New-Object Text.UTF8Encoding($false)))
    Assert ((Read-SettingsEditorsPhaseEvidence $raw ('c'*40)).evidence_status -ceq 'INCOMPLETE') 'The actual reader accepted a JSONL with a missing middle assertion.'
    [IO.File]::AppendAllText($raw,"{invalid-json`n")
    Assert ((Read-SettingsEditorsPhaseEvidence $raw ('c'*40)).evidence_status -ceq 'INCOMPLETE') 'Malformed actual JSONL was hidden.'
}finally{
    # Only this invocation's exact synthetic file/directory, no recursion.
    if(-not $scratch.StartsWith($root+'\uat\v1-control\',[StringComparison]::OrdinalIgnoreCase)){throw 'Scratch escaped task root.'}
    if(Test-Path -LiteralPath $raw){Remove-Item -LiteralPath $raw -Force}
    [IO.Directory]::Delete($scratch,$false)
}
[pscustomobject]@{checks=$script:checks;result='PASS';scope='production selected-phase converter and bounded reader only';actual_UI='NOT_RUN'}|ConvertTo-Json -Compress
