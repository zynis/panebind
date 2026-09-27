Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# Fix H fixed plan and orchestration helpers. No input/window API or CLI here.
function Assert-OwnedStability([bool]$Value,[string]$Reason){if(-not $Value){throw [IO.InvalidDataException]::new('Fix H: '+$Reason)}}
function Get-OwnedStabilitySteps{
    $steps=@();$number=0;$group=0
    foreach($phase in @('smoke','formal')){
        $count=if($phase -ceq 'smoke'){5}else{20}
        foreach($configuration in @('Debug','Release')){foreach($operation in @('Move','BottomResize')){
            ++$group
            for($repetition=1;$repetition -le $count;++$repetition){
                ++$number
                $steps+=@([pscustomobject][ordered]@{Number=$number;GroupNumber=$group;Phase=$phase;Configuration=$configuration;Operation=$operation;Repetition=$repetition;Mode='normal';RunId=[Guid]::NewGuid().ToString('N')})
            }
        }}
    }
    return $steps
}
function Test-OwnedStabilityPlan($Steps){
    $expected=@(Get-OwnedStabilitySteps);$ids=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    Assert-OwnedStability (@($Steps).Count -eq 100) 'exactly 20 smoke and 80 formal steps required'
    for($i=0;$i -lt $expected.Count;++$i){
        foreach($name in @('Number','GroupNumber','Phase','Configuration','Operation','Repetition','Mode')){Assert-OwnedStability ($Steps[$i].$name -ceq $expected[$i].$name) ('fixed step mismatch '+$name)}
        Assert-OwnedStability ($Steps[$i].RunId -cmatch '^[a-f0-9]{32}$' -and $ids.Add($Steps[$i].RunId)) 'fresh unique preregistered RunId required'
    }
}
function Get-OwnedStabilityDefinitions{
    # Return only named definitions, not any old aggregate program.
    $sources=[ordered]@{
        'run-r1c4b-input-reliability.ps1'=@('Get-ReliabilityField','Test-ReliabilityTrue','Test-ReliabilityFalse','Get-ReliabilitySnapshot','Test-ReliabilitySameSnapshot','Test-ReliabilityEnvironmentContext','Get-ReliabilityButtonState','Get-ReliabilityButtonsObservation','Test-ReliabilityEnvironment','Get-ReliabilityExitCode','Write-ReliabilityNewJson')
        'run-r1c4b-input-reliability-gates.ps1'=@('Assert-ReliabilityGate','Test-ReliabilityBlockedVerdict','Get-ReliabilityAttemptButtonState','Read-ReliabilityRun')
    }
    foreach($file in $sources.Keys){
        $tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $file),[ref]$tokens,[ref]$errors)
        Assert-OwnedStability (@($errors).Count -eq 0) 'frozen helper source must parse'
        foreach($name in $sources[$file]){
            $definitions=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name},$true))
            Assert-OwnedStability ($definitions.Count -eq 1) ('unique frozen definition '+$name)
            [scriptblock]::Create($definitions[0].Extent.Text)
        }
    }
}
function Get-OwnedStabilityOuterIdentity([string]$Repo){
    $identity=[ordered]@{}
    foreach($file in @('scripts/run-r1c4b-owned-stability-gates.ps1','scripts/r1c4b-owned-stability-model.ps1','scripts/r1c4b-owned-stability-statistics.ps1','scripts/verify-r1c4b-owned-stability-summary.ps1','scripts/verify-r1c4b-fixh-prerequisites.ps1','scripts/package-r1c4b-owned-stability-evidence.ps1')){
        $identity[$file]=(Get-FileHash -LiteralPath (Join-Path $Repo $file) -Algorithm SHA256).Hash
    }
    return $identity
}
function Assert-OwnedStabilityCheckpoint([string]$Repo,$State,[string]$Configuration){
    $head=git -C $Repo rev-parse HEAD;$headCode=$LASTEXITCODE;$status=@(git -C $Repo status --porcelain)
    Assert-OwnedStability ($headCode -eq 0 -and $LASTEXITCODE -eq 0 -and $head -ceq $State.ExecutedHEAD -and $status.Count -eq 0) 'checkpoint changed; no next child'
    Assert-OwnedStability (Test-ReliabilitySameSnapshot $State.Identity[$Configuration] (Get-ReliabilitySnapshot $Repo $Configuration)) 'runtime/source/import/binary identity changed; no next child'
    Assert-OwnedStability (Test-ReliabilitySameSnapshot $State.OuterIdentity (Get-OwnedStabilityOuterIdentity $Repo)) 'aggregate/helper/statistics identity changed; no next child'
    Assert-OwnedStability ((Get-FileHash -LiteralPath $State.PlanPath -Algorithm SHA256).Hash -ceq $State.PlanSHA256) 'frozen plan changed; no next child'
}
function New-OwnedStabilityAttempt($Step){
    return [pscustomobject][ordered]@{
        Number=$Step.Number;GroupNumber=$Step.GroupNumber;Phase=$Step.Phase;Configuration=$Step.Configuration;Operation=$Step.Operation;Repetition=$Step.Repetition;Mode=$Step.Mode;RunId=$Step.RunId
        ChildInvoked=$false;ChildExitCode='NOT_AVAILABLE';ChildOutput=@();PreflightAttempted=$false;ProbeInvoked=$false;ProbeExitCode='NOT_AVAILABLE';TargetGestureEntered=$false
        Result='NOT_RUN';Error=$null;Reasons=@();RunDirectory=$null;MetadataPath=$null;MetadataSHA256=$null;Artifacts=@();IndependentVerdict=$null;Measurement=$null
        NativeBlockReason='NOT_AVAILABLE';ValidatorError=$null;PostObservationStatus='NOT_RUN';PostObservationError=$null;AfterIdentityStatus='NOT_RUN';AfterIdentityError=$null;ImplementationUnchanged='UNKNOWN'
        FirstFailureStage='NONE';FirstFailureRecordSequences=@();FinalButtonState='UNKNOWN';FinalButtonObservationSource='NOT_AVAILABLE'
    }
}
function Assert-OwnedStabilityNextStep($Steps,$Attempts,$Step){
    Assert-OwnedStability ($Step.Number -eq @($Attempts).Count+1 -and $Step.Number -le 100 -and $Step.Mode -ceq 'normal') 'out-of-order/non-normal/extra attempt'
    foreach($attempt in $Attempts){Assert-OwnedStability ($attempt.Result -ceq 'PASS') 'previous failure prohibits next attempt'}
    if($Step.Phase -ceq 'formal'){
        $smoke=@($Attempts|Where-Object Phase -ceq 'smoke')
        Assert-OwnedStability ($smoke.Count -eq 20 -and @($smoke|Where-Object Result -cne 'PASS').Count -eq 0) 'all four smoke groups must pass before formal'
    }
    $planned=$Steps[$Step.Number-1]
    foreach($name in @('Number','GroupNumber','Phase','Configuration','Operation','Repetition','Mode','RunId')){Assert-OwnedStability ($Step.$name -ceq $planned.$name) 'attempt differs from frozen plan'}
}
function Invoke-OwnedStabilityProgression($Steps,$State,[scriptblock]$Execute,[scriptblock]$RecordProgress){
    Test-OwnedStabilityPlan $Steps
    foreach($step in $Steps){
        Assert-OwnedStabilityNextStep $Steps $State.Attempts $step
        $attempt=New-OwnedStabilityAttempt $step;$State.Attempts+=@($attempt)
        # Preserve ordinal/RunId BEFORE calling anything that can start a child.
        try{
            & $RecordProgress 'attempt_started' $attempt
            & $Execute $attempt
        }catch{
            $attempt.Error=$_.Exception.Message
            if($attempt.Result -ceq 'NOT_RUN'){$attempt.Result='INVALID_EVIDENCE'}
            if($attempt.FirstFailureStage -ceq 'NONE'){$attempt.FirstFailureStage='ORCHESTRATION_OR_VERIFICATION'}
        }
        try{& $RecordProgress 'attempt_finished' $attempt}catch{$attempt.Error='progress write failed: '+$_.Exception.Message;$attempt.Result='INVALID_EVIDENCE';$attempt.FirstFailureStage='PROGRESS_WRITE'}
        $State.FinalButtonState=$attempt.FinalButtonState
        if($attempt.Result -cne 'PASS'){$State.FirstUnexpectedFailure=$attempt;$State.BatchResult=$attempt.Result;$State.LoopResult='STOPPED';return}
    }
    $State.LoopResult='ALL_PLANNED_PASS';$State.BatchResult='PENDING_INDEPENDENT_SUMMARY'
}
function Get-OwnedStabilityCounts($Steps,$Attempts){
    $groups=@()
    foreach($group in 1..8){
        $planned=@($Steps|Where-Object GroupNumber -eq $group);$actual=@($Attempts|Where-Object GroupNumber -eq $group)
        $groups+=@([pscustomobject][ordered]@{
            GroupNumber=$group;Phase=$planned[0].Phase;Configuration=$planned[0].Configuration;Operation=$planned[0].Operation;Planned=$planned.Count
            Attempts=$actual.Count;PreflightAttempts=@($actual|Where-Object PreflightAttempted -eq $true).Count
            ProbeInvocations=@($actual|Where-Object ProbeInvoked -eq $true).Count;ProbeInvocationsUnknown=@($actual|Where-Object ProbeInvoked -ceq 'UNKNOWN').Count
            TargetGesturesEntered=@($actual|Where-Object TargetGestureEntered -eq $true).Count;TargetGesturesUnknown=@($actual|Where-Object TargetGestureEntered -ceq 'UNKNOWN').Count
            Passed=@($actual|Where-Object Result -ceq 'PASS').Count;Blocked=@($actual|Where-Object Result -ceq 'BLOCKED').Count;Failed=@($actual|Where-Object Result -ceq 'FAIL').Count;InvalidEvidence=@($actual|Where-Object Result -ceq 'INVALID_EVIDENCE').Count
            NotRun=$planned.Count-@($actual|Where-Object ChildInvoked -eq $true).Count
        })
    }
    return $groups
}
function Write-OwnedStabilityProgress([string]$Path,[string]$Type,$Attempt,[string]$PlanHash){
    $record=[ordered]@{Schema='r1c4b-owned-stability-progress/v1';Type=$Type;Utc=[DateTime]::UtcNow.ToString('o');PlanSHA256=$PlanHash;Attempt=$Attempt}
    $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($record|ConvertTo-Json -Depth 45 -Compress)+[Environment]::NewLine)
    $stream=[IO.File]::Open($Path,[IO.FileMode]::Append,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    try{$stream.Write($bytes,0,$bytes.Length);$stream.Flush()}finally{$stream.Dispose()}
}
function Get-OwnedStabilityAttemptArtifacts([string]$Directory){
    $items=@()
    foreach($pair in @(@('Metadata','run.metadata.json'),@('Probe','probe.jsonl'),@('Pre','environment-pre.json'),@('Post','environment-post.json'))){
        $path=Join-Path $Directory $pair[1];$exists=Test-Path -LiteralPath $path -PathType Leaf
        $items+=@([pscustomobject]@{Kind=$pair[0];Path=$path;Exists=$exists;SHA256=$(if($exists){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}else{$null})})
    }
    return $items
}
function Assert-OwnedStabilityNormalVerdict($Validated,[string]$Operation){
    $v=Get-ReliabilityField $Validated 'IndependentVerdict'
    Assert-OwnedStability ((Get-ReliabilityField $Validated 'IndependentlyValidatedResult') -ceq 'PASS' -and (Get-ReliabilityField $Validated 'RunnerExitCode') -eq 0) 'independent normal verification did not pass'
    foreach($pair in @(@('Result','PASS'),@('TestMode','normal'),@('Operation',$Operation),@('FixtureResult','PASS'),@('GestureResult','PASS'),@('CleanupResult','NOT_NEEDED'),@('TakeoverAcceptance','PASS'))){Assert-OwnedStability ((Get-ReliabilityField $v $pair[0]) -ceq $pair[1]) 'only the full current normal verdict counts'}
    Assert-OwnedStability (Test-ReliabilityTrue (Get-ReliabilityField $v 'ContractVerified')) 'normal contract not verified'
    Assert-OwnedStability (Test-ReliabilityFalse (Get-ReliabilityField $v 'SyntheticFixture')) 'synthetic evidence never enters live counts'
    Assert-OwnedStability ((Get-ReliabilityField (Get-ReliabilityField $v 'NormalProof') 'Result') -ceq 'PASS') 'complete NormalProof missing'
}
function Get-OwnedStabilityStoppedResult($Attempt,$Plan){
    # An independent failure audit, not a cached-label classification. Throwing
    # means insufficient chain; the caller preserves raw reasons as INVALID.
    $m=Get-Content -LiteralPath $Attempt.MetadataPath -Raw -Encoding UTF8|ConvertFrom-Json
    Assert-OwnedStability ($m.Schema -ceq 'r1c4b-fixg-input-reliability-run/v2' -and $m.RunId -ceq $Attempt.RunId -and $m.Configuration -ceq $Attempt.Configuration -and $m.Operation -ceq $Attempt.Operation -and $m.TestMode -ceq 'normal' -and $m.ExecutedHEAD -ceq $Plan.ImplementationSHA -and $m.ExpectedHEAD -ceq $Plan.ImplementationSHA -and $m.AfterHEAD -ceq $Plan.ImplementationSHA -and $m.AfterIdentityStatus -ceq 'COLLECTED' -and (Test-ReliabilityTrue $m.ImplementationUnchanged)) 'stopped attempt identity is insufficient'
    $identity=Get-ReliabilityField $Plan.RuntimeIdentity $Attempt.Configuration
    Assert-OwnedStability (@($m.BeforeIdentity.PSObject.Properties).Count -eq 19 -and @($m.AfterIdentity.PSObject.Properties).Count -eq 19) 'stopped identity inventory incomplete'
    foreach($p in $identity.PSObject.Properties){Assert-OwnedStability ((Get-ReliabilityField $m.BeforeIdentity $p.Name) -ceq $p.Value -and (Get-ReliabilityField $m.AfterIdentity $p.Name) -ceq $p.Value) 'stopped runtime identity changed'}
    $directory=[IO.Path]::GetFullPath($Attempt.RunDirectory)
    if(Test-ReliabilityFalse $m.ProbeInvoked){
        Assert-OwnedStability ($m.NativeProcessState -ceq 'NOT_RUN' -and -not (Test-Path -LiteralPath (Join-Path $directory 'probe.jsonl'))) 'preflight block cannot conceal a native launch'
        $path=Join-Path $directory 'environment-pre.json'
        Assert-OwnedStability ([IO.Path]::GetFullPath($m.EnvironmentPrePath) -ceq $path -and (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ceq $m.EnvironmentPreSHA256 -and $m.EnvironmentPreExitCode -eq 2) 'stopped preflight reference unavailable'
        $pre=Get-Content -LiteralPath $path -Raw -Encoding UTF8|ConvertFrom-Json
        $buttons=Get-ReliabilityButtonsObservation $pre
        Assert-OwnedStability ($pre.schema -ceq 'r1c4b-readonly-input-environment/v2' -and $pre.startup_contract -ceq 'foreground_gui_readiness_v1' -and $buttons -cin @('OBSERVED_UP','OBSERVED_DOWN') -and -not (Test-ReliabilityEnvironment $pre)) 'preflight context/input proof unavailable'
        $gui=$pre.foreground_gui
        Assert-OwnedStability (Test-ReliabilityTrue $gui.query_attempted) 'valid foreground context must have an actual GUI query'
        foreach($name in @('query_tid','query_start_qpc','query_finish_qpc','error')){Assert-IsolationInt64 (Get-ReliabilityField $gui $name) ('preflight GUI '+$name)}
        Assert-OwnedStability ($gui.query_tid -eq $gui.foreground_before.tid -and $gui.query_start_qpc -ge $gui.foreground_before.finish_qpc -and $gui.query_finish_qpc -ge $gui.query_start_qpc -and $gui.foreground_after.start_qpc -ge $gui.query_finish_qpc) 'preflight actual queried TID/time mismatch'
        Assert-OwnedStability ($gui.query_succeeded -is [bool] -and $pre.mouse_buttons_swapped -is [bool]) 'typed query/mapping observations required'
        $expected=[ordered]@{QPC_VALID='PASS';BEFORE_CONTEXT_VALID='PASS';AFTER_CONTEXT_VALID='PASS';OBSERVATION_CONTEXT_STABLE='PASS';BUTTONS_OBSERVED_UP=$(if($buttons -ceq 'OBSERVED_UP'){'PASS'}else{'FAIL'});INPUT_MAPPING_SUPPORTED=$(if($pre.mouse_buttons_swapped){'FAIL'}else{'PASS'});FOREGROUND_GUI_QUERY=$(if($gui.query_succeeded){'PASS'}else{'FAIL'});FOREGROUND_GUI_TUPLE_STABLE='PASS'}
        foreach($pair in @(@('CAPTURE_CLEAR','capture_hwnd'),@('MENU_CLEAR','menu_owner_hwnd'),@('MOVE_SIZE_CLEAR','move_size_hwnd'),@('DISALLOWED_GUI_FLAGS_CLEAR','gui_flags'))){
            $value=Get-ReliabilityField $gui $pair[1]
            if($gui.query_succeeded){Assert-IsolationInt64 $value ('preflight '+$pair[1]);$clear=if($pair[1] -ceq 'gui_flags'){($value -band 30) -eq 0}else{$value -eq 0};$expected[$pair[0]]=if($clear){'PASS'}else{'FAIL'}}
            else{Assert-OwnedStability ($null -eq $value) 'failed GUI query must not invent handle/flag values';$expected[$pair[0]]='UNKNOWN'}
        }
        if($gui.query_succeeded){Assert-OwnedStability ($gui.error -eq 0) 'successful GUI query cannot retain an error'}
        Assert-OwnedStability ((@($pre.readiness_predicates.PSObject.Properties.Name) -join ',') -ceq (@($expected.Keys) -join ',')) 'complete ordered readiness predicates required'
        $first='NONE';$failed=@();foreach($name in $expected.Keys){Assert-OwnedStability ($pre.readiness_predicates.$name -ceq $expected[$name]) 'cached failed predicate differs from raw observation';if($expected[$name] -cne 'PASS' -and $first -ceq 'NONE'){$first=$name};if($expected[$name] -ceq 'FAIL'){$failed+=,$name}}
        Assert-OwnedStability ($failed.Count -gt 0 -and $pre.first_failed_predicate -ceq $first -and (@($pre.failed_predicates) -join ',') -ceq ($failed -join ',') -and $pre.result -ceq 'BLOCKED' -and $pre.startup_readiness -ceq 'BLOCKED' -and (Test-ReliabilityFalse $pre.startup_ready)) 'independent known block and its aliases must agree'
        return [pscustomobject]@{Result='BLOCKED';Stage='READ_ONLY_PREFLIGHT';EvidenceIntegrity='VALID';NativeFixtureStarted=$false;InputSent=$false;FinalButtons=$buttons;FirstFailedPredicate=$pre.first_failed_predicate;ObservationPath=$path}
    }
    Assert-OwnedStability (Test-ReliabilityTrue $m.ProbeInvoked) 'native invocation unknown'
    $log=Join-Path $directory 'probe.jsonl';Assert-OwnedStability ((Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash -ceq $m.LogSHA256) 'failed raw log hash differs'
    $fresh=Test-FixFInputReliabilityEvidence -Path $log
    Assert-OwnedStability ($fresh.Operation -ceq $Attempt.Operation -and $fresh.TestMode -ceq 'normal') 'failed native operation/mode mismatch'
    if($fresh.Result -ceq 'BLOCKED' -and ((Get-ReliabilityField $fresh 'EvidenceIntegrity') -ceq 'VALID' -or (Test-ReliabilityTrue (Get-ReliabilityField $fresh 'FailureDiagnosticVerified')))){return $fresh}
    if($fresh.Result -ceq 'FAIL'){return $fresh}
    throw 'failed native chain does not independently classify BLOCKED/FAIL'
}
