Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# Pure model/AST and memory seams only. No live runner program, environment
# executable, native probe, Git process, UAT read or evidence write is invoked.
. (Join-Path $PSScriptRoot 'r1c4b-owned-stability-model.ps1')
# Get-FileHash is a module-exported function. Load its metadata before installing
# the memory function, so module autoload cannot replace that seam on first use.
$null=Get-Command Get-FileHash
$script:boundaryChecks=0
function Check-OwnedBoundary([bool]$Pass,[string]$Reason){if(-not $Pass){throw "owned boundary fixture: $Reason"};++$script:boundaryChecks}
function Reject-OwnedBoundary([scriptblock]$Action,[string]$Reason){$rejected=$false;try{$null=& $Action}catch{$rejected=$true};Check-OwnedBoundary $rejected $Reason}
function Copy-OwnedBoundary($Value){return $Value|ConvertTo-Json -Depth 40 -Compress|ConvertFrom-Json}
function Read-OwnedBoundaryAst([string]$File){
    $tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $File),[ref]$tokens,[ref]$errors)
    Check-OwnedBoundary ($errors.Count -eq 0) "parse actual source $File";return $ast
}
function One-OwnedBoundaryNode($Ast,[scriptblock]$Predicate,[string]$Name){$nodes=@($Ast.FindAll($Predicate,$true));Check-OwnedBoundary ($nodes.Count -eq 1) "unique $Name";return $nodes[0]}
$singleAst=Read-OwnedBoundaryAst 'run-r1c4b-input-reliability.ps1'
$numericAst=Read-OwnedBoundaryAst 'r1c4b-input-isolation-validation.ps1'
$fixtureAst=Read-OwnedBoundaryAst 'test-r1c4b-input-reliability-runner.ps1'
$liveAst=Read-OwnedBoundaryAst 'run-r1c4b-owned-stability-gates.ps1'
$summaryAst=Read-OwnedBoundaryAst 'verify-r1c4b-owned-stability-summary.ps1'
$definitions=[ordered]@{
    single=@('Get-ReliabilityField','Test-ReliabilityTrue','Test-ReliabilityFalse','Get-ReliabilitySnapshot','Test-ReliabilitySameSnapshot','Test-ReliabilityEnvironmentContext','Get-ReliabilityButtonsObservation','Test-ReliabilityEnvironment')
    numeric=@('Assert-InputIsolation','Assert-IsolationInt64')
    fixture=@('New-ReliabilityContext','New-ReliabilityEnvironment')
}
foreach($source in $definitions.Keys){
    $ast=switch($source){single {$singleAst};numeric {$numericAst};fixture {$fixtureAst}}
    foreach($name in $definitions[$source]){
        $node=One-OwnedBoundaryNode $ast {param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name} $name
        . ([scriptblock]::Create($node.Extent.Text))
    }
}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$script:boundaryMemory=@{}
$script:boundaryHashFailure=$null
$script:boundaryHead='a'*40
$script:boundaryHeadExit=0
$script:boundaryStatus=@()
$script:boundaryStatusExit=0
$script:boundaryGitCalls=0
$script:boundaryChildCalls=0
function Owned-BoundaryPath([string]$Path){return [IO.Path]::GetFullPath($Path)}
function Set-OwnedBoundaryMemory([string]$Path,[string]$Text,[string]$Hash){$script:boundaryMemory[(Owned-BoundaryPath $Path)]=[pscustomobject]@{Text=$Text;Hash=$Hash}}
function Get-FileHash([string]$LiteralPath,[string]$Algorithm){
    $path=Owned-BoundaryPath $LiteralPath
    if($path -ceq $script:boundaryHashFailure){throw 'Synthetic boundary hash read failure'}
    if($Algorithm -cne 'SHA256' -or -not $script:boundaryMemory.ContainsKey($path)){throw "Unapproved memory hash path: $path"}
    return [pscustomobject]@{Hash=$script:boundaryMemory[$path].Hash}
}
function Get-Content([string]$LiteralPath,[switch]$Raw,[string]$Encoding){
    $path=Owned-BoundaryPath $LiteralPath
    if(-not $script:boundaryMemory.ContainsKey($path)){throw "Unapproved memory content path: $path"}
    if($Raw){return $script:boundaryMemory[$path].Text};return $script:boundaryMemory[$path].Text -split "\r?\n"|Where-Object {$_}
}
function Test-Path([string]$LiteralPath,[string]$PathType){return $script:boundaryMemory.ContainsKey((Owned-BoundaryPath $LiteralPath))}
function git{
    ++$script:boundaryGitCalls
    if($args.Count -ne 4 -or $args[0] -cne '-C' -or $args[1] -cne $repo){throw 'Unapproved synthetic Git arguments'}
    if($args[2] -ceq 'rev-parse' -and $args[3] -ceq 'HEAD'){$script:LASTEXITCODE=$script:boundaryHeadExit;return $script:boundaryHead}
    if($args[2] -ceq 'status' -and $args[3] -ceq '--porcelain'){$script:LASTEXITCODE=$script:boundaryStatusExit;return $script:boundaryStatus}
    throw 'Synthetic Git supports exactly the two checkpoint reads'
}
$filesNode=One-OwnedBoundaryNode $singleAst {param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$files'} 'actual runtime identity manifest'
$modelAst=Read-OwnedBoundaryAst 'r1c4b-owned-stability-model.ps1'
$outerNode=One-OwnedBoundaryNode $modelAst {param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq 'Get-OwnedStabilityOuterIdentity'} 'actual outer identity helper'
$outerManifest=One-OwnedBoundaryNode $outerNode {param($n) $n -is [Management.Automation.Language.ForEachStatementAst]} 'actual six-file outer manifest'
foreach($Configuration in @('Debug','Release')){foreach($relative in @(& ([scriptblock]::Create($filesNode.Right.Extent.Text)))){Set-OwnedBoundaryMemory (Join-Path $repo $relative) '' ('C'*64)}}
$outerFiles=@(& ([scriptblock]::Create($outerManifest.Condition.Extent.Text)))
Check-OwnedBoundary ($outerFiles.Count -eq 6) 'all six actual aggregate/helper/statistics/package files are frozen'
foreach($relative in $outerFiles){Set-OwnedBoundaryMemory (Join-Path $repo $relative) '' ('D'*64)}
$memoryPlan=Join-Path $repo 'uat/r1c4b-fixh/offline-memory-plan.json'
Set-OwnedBoundaryMemory $memoryPlan '' ('E'*64)
$checkpoint=[ordered]@{ExecutedHEAD=('a'*40);Identity=@{};OuterIdentity=(Get-OwnedStabilityOuterIdentity $repo);PlanPath=$memoryPlan;PlanSHA256=('E'*64)}
foreach($configuration in @('Debug','Release')){$checkpoint.Identity[$configuration]=Get-ReliabilitySnapshot $repo $configuration;Check-OwnedBoundary ($checkpoint.Identity[$configuration].Count -eq 19) "frozen actual runtime $configuration inventory"}
function Invoke-OwnedBoundaryNextChild{Assert-OwnedStabilityCheckpoint $repo $checkpoint Debug;++$script:boundaryChildCalls}
Invoke-OwnedBoundaryNextChild
Check-OwnedBoundary ($script:boundaryChildCalls -eq 1 -and $script:boundaryGitCalls -eq 2) 'actual checkpoint helper permits one fake child only on all unchanged identities'
function Reset-OwnedBoundaryCheckpoint{
    $script:boundaryHead='a'*40;$script:boundaryHeadExit=0;$script:boundaryStatus=@();$script:boundaryStatusExit=0;$script:boundaryHashFailure=$null;$script:boundaryChildCalls=0
    foreach($key in @($checkpoint.Identity.Debug.Keys)){$script:boundaryMemory[(Owned-BoundaryPath (Join-Path $repo $key))].Hash='C'*64}
    foreach($key in $outerFiles){$script:boundaryMemory[(Owned-BoundaryPath (Join-Path $repo $key))].Hash='D'*64}
    $script:boundaryMemory[(Owned-BoundaryPath $memoryPlan)].Hash='E'*64
}
foreach($failure in @('head_changed','head_failed','dirty_status','status_failed','runtime_source','runtime_binary','plan_changed','hash_read_failed')){
    Reset-OwnedBoundaryCheckpoint
    switch($failure){
        head_changed {$script:boundaryHead='b'*40}
        head_failed {$script:boundaryHeadExit=1}
        dirty_status {$script:boundaryStatus=@(' M scripts/r1c4b-owned-stability-model.ps1')}
        status_failed {$script:boundaryStatusExit=1}
        runtime_source {$script:boundaryMemory[(Owned-BoundaryPath (Join-Path $repo @($checkpoint.Identity.Debug.Keys)[0]))].Hash='F'*64}
        runtime_binary {$script:boundaryMemory[(Owned-BoundaryPath (Join-Path $repo @($checkpoint.Identity.Debug.Keys)[-1]))].Hash='F'*64}
        plan_changed {$script:boundaryMemory[(Owned-BoundaryPath $memoryPlan)].Hash='F'*64}
        hash_read_failed {$script:boundaryHashFailure=Owned-BoundaryPath $memoryPlan}
    }
    Reject-OwnedBoundary {Invoke-OwnedBoundaryNextChild} "actual checkpoint rejects $failure"
    Check-OwnedBoundary ($script:boundaryChildCalls -eq 0) "$failure invokes no next fake child"
}
foreach($relative in $outerFiles){
    Reset-OwnedBoundaryCheckpoint;$script:boundaryMemory[(Owned-BoundaryPath (Join-Path $repo $relative))].Hash='F'*64
    Reject-OwnedBoundary {Invoke-OwnedBoundaryNextChild} "actual extra identity rejects $relative"
    Check-OwnedBoundary ($script:boundaryChildCalls -eq 0) 'changed extra helper never starts the callback child'
}
Reset-OwnedBoundaryCheckpoint

$memoryDirectory=Join-Path $repo 'uat/r1c4b-fixg/offline-boundary-11111111111111111111111111111111'
$memoryMetadata=Join-Path $memoryDirectory 'run.metadata.json';$memoryPre=Join-Path $memoryDirectory 'environment-pre.json';$memoryProbe=Join-Path $memoryDirectory 'probe.jsonl'
$stoppedPlan=[pscustomobject]@{ImplementationSHA=('a'*40);RuntimeIdentity=[pscustomobject]@{Debug=[pscustomobject]$checkpoint.Identity.Debug}}
$stoppedAttempt=[pscustomobject]@{RunId=('1'*32);Configuration='Debug';Operation='Move';RunDirectory=$memoryDirectory;MetadataPath=$memoryMetadata}
function New-OwnedBoundaryPreflight{
    $env=New-ReliabilityEnvironment
    $env.foreground_gui.capture_hwnd=4444;$env.startup_ready=$false;$env.startup_readiness='BLOCKED';$env.result='BLOCKED';$env.first_failed_predicate='CAPTURE_CLEAR';$env.failed_predicates=@('CAPTURE_CLEAR');$env.readiness_predicates.CAPTURE_CLEAR='FAIL'
    return $env
}
function New-OwnedBoundaryStoppedMetadata{
    return [pscustomobject]@{Schema='r1c4b-fixg-input-reliability-run/v2';RunId=('1'*32);Configuration='Debug';Operation='Move';TestMode='normal';ExecutedHEAD=('a'*40);ExpectedHEAD=('a'*40);AfterHEAD=('a'*40);AfterIdentityStatus='COLLECTED';ImplementationUnchanged=$true;BeforeIdentity=[pscustomobject]$checkpoint.Identity.Debug;AfterIdentity=[pscustomobject]$checkpoint.Identity.Debug;ProbeInvoked=$false;NativeProcessState='NOT_RUN';EnvironmentPrePath=$memoryPre;EnvironmentPreSHA256=('A'*64);EnvironmentPreExitCode=2}
}
function Set-OwnedBoundaryStopped($Environment,$Metadata=(New-OwnedBoundaryStoppedMetadata)){
    $null=$script:boundaryMemory.Remove((Owned-BoundaryPath $memoryProbe))
    Set-OwnedBoundaryMemory $memoryMetadata ($Metadata|ConvertTo-Json -Depth 40 -Compress) ('B'*64)
    Set-OwnedBoundaryMemory $memoryPre ($Environment|ConvertTo-Json -Depth 40 -Compress) ('A'*64)
}
Set-OwnedBoundaryStopped (New-OwnedBoundaryPreflight)
$blocked=Get-OwnedStabilityStoppedResult $stoppedAttempt $stoppedPlan
Check-OwnedBoundary ($blocked.Result -ceq 'BLOCKED' -and $blocked.EvidenceIntegrity -ceq 'VALID' -and $blocked.Stage -ceq 'READ_ONLY_PREFLIGHT' -and -not $blocked.NativeFixtureStarted -and -not $blocked.InputSent -and $blocked.FinalButtons -ceq 'OBSERVED_UP') 'actual raw v2 nonzero capture is a valid no-input preflight block'
foreach($failure in @('query_not_attempted','string_capture','string_flags','string_query_success','missing_query_qpc','reversed_query_qpc','wrong_query_tid','tuple_changed','query_failure_invents_capture','missing_predicate','unordered_predicates','forged_capture_predicate','wrong_first_failure','wrong_failed_list','ready_alias','unknown_mapping','unknown_context','down_without_predicate')){
    $env=New-OwnedBoundaryPreflight
    switch($failure){
        query_not_attempted {$env.foreground_gui.query_attempted=$false}
        string_capture {$env.foreground_gui.capture_hwnd='4444'}
        string_flags {$env.foreground_gui.gui_flags='0'}
        string_query_success {$env.foreground_gui.query_succeeded='true'}
        missing_query_qpc {$env.foreground_gui.PSObject.Properties.Remove('query_start_qpc')}
        reversed_query_qpc {$env.foreground_gui.query_finish_qpc=$env.foreground_gui.query_start_qpc-1}
        wrong_query_tid {$env.foreground_gui.query_tid++}
        tuple_changed {$env.foreground_gui.foreground_after.pid++}
        query_failure_invents_capture {$env.foreground_gui.query_succeeded=$false;$env.foreground_gui.error=5}
        missing_predicate {$env.readiness_predicates.PSObject.Properties.Remove('MENU_CLEAR')}
        unordered_predicates {$value=$env.readiness_predicates.CAPTURE_CLEAR;$env.readiness_predicates.PSObject.Properties.Remove('CAPTURE_CLEAR');$env.readiness_predicates|Add-Member -NotePropertyName CAPTURE_CLEAR -NotePropertyValue $value}
        forged_capture_predicate {$env.readiness_predicates.CAPTURE_CLEAR='PASS'}
        wrong_first_failure {$env.first_failed_predicate='NONE'}
        wrong_failed_list {$env.failed_predicates=@('CAPTURE_CLEAR','MENU_CLEAR')}
        ready_alias {$env.startup_ready=$true}
        unknown_mapping {$env.mouse_buttons_swapped=$null}
        unknown_context {$env.context_reliable=$false}
        down_without_predicate {$env.keys.left.state='DOWN';$env.keys.left.high_bit_down=$true}
    }
    Set-OwnedBoundaryStopped $env
    Reject-OwnedBoundary {Get-OwnedStabilityStoppedResult $stoppedAttempt $stoppedPlan} "actual preflight classifier rejects $failure"
}
$env=New-OwnedBoundaryPreflight;$env.foreground_gui.query_succeeded=$false;$env.foreground_gui.error=5
foreach($name in @('capture_hwnd','menu_owner_hwnd','move_size_hwnd','gui_flags')){$env.foreground_gui.$name=$null}
$env.readiness_predicates.FOREGROUND_GUI_QUERY='FAIL'
foreach($name in @('CAPTURE_CLEAR','MENU_CLEAR','MOVE_SIZE_CLEAR','DISALLOWED_GUI_FLAGS_CLEAR')){$env.readiness_predicates.$name='UNKNOWN'}
$env.first_failed_predicate='FOREGROUND_GUI_QUERY';$env.failed_predicates=@('FOREGROUND_GUI_QUERY')
Set-OwnedBoundaryStopped $env;$blocked=Get-OwnedStabilityStoppedResult $stoppedAttempt $stoppedPlan
Check-OwnedBoundary ($blocked.Result -ceq 'BLOCKED' -and $blocked.FirstFailedPredicate -ceq 'FOREGROUND_GUI_QUERY') 'actual failed query stays a query blocker with UNKNOWN capture/mode fields'
$env=New-OwnedBoundaryPreflight;$env.foreground_gui.capture_hwnd=0;$env.keys.left.state='DOWN';$env.keys.left.high_bit_down=$true;$env.buttons_observation='OBSERVED_DOWN';$env.buttons_observed_up=$false;$env.all_required_inputs_up=$false;$env.readiness_predicates.CAPTURE_CLEAR='PASS';$env.readiness_predicates.BUTTONS_OBSERVED_UP='FAIL';$env.first_failed_predicate='BUTTONS_OBSERVED_UP';$env.failed_predicates=@('BUTTONS_OBSERVED_UP')
Set-OwnedBoundaryStopped $env;$blocked=Get-OwnedStabilityStoppedResult $stoppedAttempt $stoppedPlan
Check-OwnedBoundary ($blocked.FinalButtons -ceq 'OBSERVED_DOWN' -and $blocked.FirstFailedPredicate -ceq 'BUTTONS_OBSERVED_UP') 'actual reliable DOWN is a specific input blocker, not UNKNOWN or cleanup failure'
foreach($failure in @('wrong_head','missing_after_identity','changed_identity','wrong_run_id','bad_pre_hash','native_log_exists')){
    $metadata=New-OwnedBoundaryStoppedMetadata
    switch($failure){wrong_head {$metadata.AfterHEAD='b'*40};missing_after_identity {$metadata.AfterIdentityStatus='NOT_RUN'};changed_identity {$metadata.BeforeIdentity=Copy-OwnedBoundary $metadata.BeforeIdentity;$metadata.BeforeIdentity.PSObject.Properties[@($metadata.BeforeIdentity.PSObject.Properties.Name)[0]].Value='F'*64};wrong_run_id {$metadata.RunId='2'*32};bad_pre_hash {$metadata.EnvironmentPreSHA256='F'*64}}
    Set-OwnedBoundaryStopped (New-OwnedBoundaryPreflight) $metadata
    if($failure -ceq 'native_log_exists'){Set-OwnedBoundaryMemory $memoryProbe '{}' ('F'*64)}
    Reject-OwnedBoundary {Get-OwnedStabilityStoppedResult $stoppedAttempt $stoppedPlan} "stopped identity/artifact refuses $failure"
}

# Evaluate only the real guarded catch/write statements with memory seams;
# never evaluate either live or summary program's top-level execution chain.
$script:boundaryWriteErrors=[Collections.Generic.List[string]]::new()
function Write-Error([string]$Message,[string]$ErrorAction){$script:boundaryWriteErrors.Add($Message)}
function Write-ReliabilityNewJson{throw 'Synthetic boundary evidence write failure'}
$outerSummaryCatch=One-OwnedBoundaryNode $summaryAst {param($n) $n -is [Management.Automation.Language.CatchClauseAst] -and $n.Body.Extent.Text.Contains("`$summary.IndependentSummaryResult='INVALID_EVIDENCE'")} 'actual summary software-error catch'
$summary=[ordered]@{Error=$null;BatchResult='PASS';IndependentSummaryResult='PASS'};$exitCode=0
. ([scriptblock]::Create("try{throw 'Synthetic summary verification exception'}"+$outerSummaryCatch.Extent.Text))
Check-OwnedBoundary ($exitCode -eq 2 -and $summary.BatchResult -ceq 'INVALID_EVIDENCE' -and $summary.IndependentSummaryResult -ceq 'INVALID_EVIDENCE' -and $summary.Error -ceq 'Synthetic summary verification exception') 'actual independent summary exception cannot become PASS'
$summaryWrite=One-OwnedBoundaryNode $summaryAst {param($n) $n -is [Management.Automation.Language.TryStatementAst] -and $n.Body.Extent.Text -match '^\{Write-ReliabilityNewJson \$ArtifactPath \$summary\}$'} 'actual summary artifact writer guard'
$ArtifactPath='MEMORY_ONLY';$exitCode=0
. ([scriptblock]::Create($summaryWrite.Extent.Text))
Check-OwnedBoundary ($exitCode -eq 2 -and $script:boundaryWriteErrors[-1].Contains('summary evidence write failed')) 'actual summary write failure forces nonzero without writing a file'
$finalizationFence=One-OwnedBoundaryNode $liveAst {param($n) $n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text -ceq "`$state.MetadataWriteStatus -cne 'WRITTEN' -or `$state.StatisticsError -or `$state.SummaryError"} 'actual aggregate finalization exit fence'
foreach($failure in @('inventory','statistics','summary')){
    $state=[ordered]@{MetadataWriteStatus='WRITTEN';StatisticsError=$null;SummaryError=$null};$summaryExit=0
    switch($failure){inventory {$state.MetadataWriteStatus='ERROR'};statistics {$state.StatisticsError='Synthetic statistics exception'};summary {$state.SummaryError='Synthetic summary invocation exception'}}
    . ([scriptblock]::Create($finalizationFence.Extent.Text))
    Check-OwnedBoundary ($summaryExit -eq 2) "actual $failure finalization failure forces aggregate nonzero"
}
$receiptWrite=One-OwnedBoundaryNode $liveAst {param($n) $n -is [Management.Automation.Language.TryStatementAst] -and $n.Body.Extent.Text.Contains("'r1c4b-owned-stability-finalization/v1'")} 'actual finalization receipt writer guard'
$state=[ordered]@{BatchId=('3'*32);ExecutedHEAD=('a'*40);MetadataWriteStatus='WRITTEN';MetadataWriteError=$null;StatisticsError=$null;SummaryError=$null}
$inventoryPath='MEMORY_ONLY_INVENTORY';$statisticsPath='MEMORY_ONLY_STATISTICS';$summaryPath='MEMORY_ONLY_SUMMARY';$directory=$memoryDirectory;$summaryExit=0
. ([scriptblock]::Create($receiptWrite.Extent.Text))
Check-OwnedBoundary ($summaryExit -eq 2 -and $script:boundaryWriteErrors[-1].Contains('finalization receipt write failed')) 'actual receipt write failure forces nonzero without replacing historical evidence'
Write-Host "Fix H actual-helper memory boundary checks=$script:boundaryChecks PASS; no GUI/input/native/environment/Git/artifact"
