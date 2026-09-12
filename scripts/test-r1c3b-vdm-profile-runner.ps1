[CmdletBinding()]
param([switch] $DefinitionsOnly)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$vdmDefinitionsOnly=$DefinitionsOnly
. (Join-Path $PSScriptRoot 'test-r1c3b-frame-profile-runner.ps1') -DefinitionsOnly
. (Join-Path $PSScriptRoot 'r1c3b-phase3-validation.ps1')
$runner=Join-Path $PSScriptRoot 'run-r1c3b-phase3-vdm-profile.ps1'
$fixtureRoot=Join-Path $tempBase ('panebind-r1c3b-vdm-runner-'+[Guid]::NewGuid().ToString('N'))
$script:vdmFixtureCount=0
function New-VdmProfileRecords {
    param([bool] $Ctrl=$true,[switch] $Delayed)
    $r=@(New-FrameProfileRecords -Ctrl:$Ctrl -Delayed:$Delayed)
    $spans=[Collections.Generic.List[object]]::new()
    foreach($s in @($r|Where-Object record_kind -eq profile_span)){$spans.Add($s)}
    foreach($s in @($spans|Where-Object stage -eq virtual_desktop)) {
        $spans.Add([pscustomobject]@{record_kind='profile_span';span_id=$spans.Count+1;parent_span_id=$s.span_id
            stage='virtual_desktop_query';quantum_id=$s.quantum_id;operation_generation=$s.operation_generation
            source_receipt=$s.source_receipt;role=$s.role;begin_qpc=$s.begin_qpc+1;end_qpc=$s.end_qpc-1
            begin_receipt_watermark=(Get-FixtureWatermark ($s.begin_qpc+1));end_receipt_watermark=(Get-FixtureWatermark ($s.end_qpc-1))})
    }
    $ordered=@($spans|Sort-Object begin_qpc,@{Expression='end_qpc';Descending=$true},span_id)
    $ids=@{}; for($i=0;$i -lt $ordered.Count;++$i){$ids[[uint64]$ordered[$i].span_id]=$i+1}
    foreach($s in $ordered){$s.span_id=$ids[[uint64]$s.span_id];if($s.parent_span_id){$s.parent_span_id=$ids[[uint64]$s.parent_span_id]}}
    $validations=@($r|Where-Object record_kind -eq validation)
    foreach($v in $validations){
        if($v.span_id){$v.span_id=$ids[[uint64]$v.span_id]}
        $retained=$v.span_id -gt 0 -or $v.validation_phase -ne 'setup'
        Add-Fields $v @{retained_vdm=$retained;manager_create_calls=$(if($retained){0}else{1});virtual_desktop_query_calls=1}
    }
    $status=$r|Where-Object record_kind -eq validation_status
    Add-Fields $status @{retained_manager_create_calls=1;retained_manager_release_calls=1}
    foreach($phase in @('setup','start','active','end','restore','invalidation')) {
        $creates=0; $queries=0
        foreach($v in @($validations|Where-Object validation_phase -eq $phase)){$creates+=$v.manager_create_calls;$queries+=$v.virtual_desktop_query_calls}
        $release=$creates+$(if($phase -eq 'restore'){1}else{0})
        Add-Fields $status @{('manager_create_calls_'+$phase)=($creates+$(if($phase -eq 'setup'){1}else{0}));('virtual_desktop_query_calls_'+$phase)=$queries;('manager_release_calls_'+$phase)=$release}
    }
    $profile=$r|Where-Object record_kind -eq profile_status
    $profile.profile_qpc_reads+=2*($ordered.Count-$profile.span_count);$profile.span_count=$ordered.Count
    Add-Fields ($r|Where-Object record_kind -eq startup) @{vdm_reuse_enabled=$true;virtual_desktop_result_cache=$false}
    $before=@($r|Where-Object {$_.record_kind -notin @('profile_span','summary','shutdown')})
    $after=@($r|Where-Object {$_.record_kind -in @('summary','shutdown')})
    $all=@(Set-FixtureSequence ($before+$ordered+$after))
    foreach($record in $all){Add-Fields $record @{schema_version=1;schema_name='panebind.r1c3b3.explorer_glue_profile'}}
    return $all
}
function Test-VdmDirect {
    param([string] $Name,[object[]] $Records,[bool] $Pass=$true)
    $actual=$false;$reason=''
    try {
        $out=@(Assert-StageProfileEvidence $Records ($Records|Where-Object record_kind -eq startup) ($Records|Where-Object record_kind -eq summary) ($Records|Where-Object record_kind -eq facts) -Phase2 -Phase3)
        $out+=@(Assert-ConsentBoundProfileEvidence $Records)
        $out+=@(Assert-VdmProfileEvidence $Records)
        $actual=$true
    }catch{$reason=$_.Exception.Message}
    if($actual -ne $Pass){throw "$Name unexpected result: $reason"}
    ++$script:vdmFixtureCount;Write-Output "$Name VDM fixture: PASS"
}
if($vdmDefinitionsOnly){return}
[void](New-Item -ItemType Directory -Path $fixtureRoot)
try {
    foreach($delayed in @($false,$true)){
        $r=@(New-VdmProfileRecords -Delayed:$delayed)
        Test-VdmDirect "ValidDelayed=$delayed" $r
        Test-CtrlFixture "VDM_VALID_$delayed" $r; ++$script:vdmFixtureCount
    }
    $mutations=[ordered]@{
        ResultCache={param($r)($r|Where-Object record_kind -eq startup).virtual_desktop_result_cache=$true}
        ReuseOff={param($r)($r|Where-Object record_kind -eq startup).vdm_reuse_enabled=$false}
        MissingCreate={param($r)($r|Where-Object record_kind -eq validation_status).retained_manager_create_calls=0}
        Recreate={param($r)($r|Where-Object record_kind -eq validation_status).retained_manager_create_calls=2}
        NoRelease={param($r)($r|Where-Object record_kind -eq validation_status).retained_manager_release_calls=0}
        DoubleRelease={param($r)($r|Where-Object record_kind -eq validation_status).retained_manager_release_calls=2}
        ActiveCreate={param($r)($r|Where-Object record_kind -eq validation_status).manager_create_calls_active=1}
        QueryTotal={param($r)($r|Where-Object record_kind -eq validation_status).virtual_desktop_query_calls_active=0}
        MissingQuery={param($r)@($r|Where-Object {$_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active'})[0].virtual_desktop_query_calls=0}
        QueryRetry={param($r)@($r|Where-Object {$_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active'})[0].virtual_desktop_query_calls=2}
        RecreatedValidation={param($r)@($r|Where-Object {$_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active'})[0].manager_create_calls=1}
        EphemeralActive={param($r)@($r|Where-Object {$_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active'})[0].retained_vdm=$false}
        CreateInsteadOfQuery={param($r)@($r|Where-Object {$_.record_kind -eq 'profile_span' -and $_.stage -eq 'virtual_desktop_query'})[0].stage='virtual_desktop_manager_acquire'}
        NoQuerySpan={param($r)@($r|Where-Object {$_.record_kind -eq 'profile_span' -and $_.stage -eq 'virtual_desktop_query'})[0].stage='receipt_finalize'}
        WrongQueryParent={param($r)@($r|Where-Object {$_.record_kind -eq 'profile_span' -and $_.stage -eq 'virtual_desktop_query'})[0].parent_span_id=1}
        QueryFailure={param($r)@($r|Where-Object {$_.record_kind -eq 'validation' -and $_.validation_phase -eq 'active'})[0].succeeded=$false}
        EarlyRelease={param($r)($r|Where-Object record_kind -eq validation_status).manager_release_calls_start=1}
    }
    foreach($name in $mutations.Keys){$r=@(New-VdmProfileRecords);& $mutations[$name] $r;Test-VdmDirect $name $r $false}
    $r=@(New-VdmProfileRecords -Ctrl:$false)
    Test-VdmDirect 'NoCtrl' $r
    $observer=@(New-ObserverRecords -ActivePairCount 1|Where-Object {$_.record_kind -ne 'event' -or $_.native_window_id -ne '0x0000000000000020'})
    for($i=0;$i -lt $observer.Count;++$i){$observer[$i].observer_sequence=$i+1}
    Test-CtrlFixture 'VDM_NO_CTRL' $r 2 2 'SAFE_BLOCKED / CTRL_NOT_DOWN_AT_START' $observer; ++$script:vdmFixtureCount
    ($r|Where-Object record_kind -eq activation_attempt).ctrl_down_at_owner_processing=$true
    Test-CtrlFixture 'VDM_LATE_CTRL' $r 2 2 'SAFE_BLOCKED / CTRL_NOT_DOWN_AT_START' $observer; ++$script:vdmFixtureCount
    $r=@(New-VdmProfileRecords);($r|Where-Object record_kind -eq validation_status).virtual_desktop_query_calls_active=0
    Test-CtrlFixture 'VDM_NO_QUERY_E2E' $r 0 1 'INVALID_EVIDENCE';++$script:vdmFixtureCount
    $before=Get-VdmFamilyShare (New-FrameProfileRecords);$after=Get-VdmFamilyShare (New-VdmProfileRecords)
    if([Math]::Abs([double]($before-$after)) -gt 0.000000001){throw 'Stage splitting manufactured a family share improvement'}
    ++$script:vdmFixtureCount
    $prefix=Write-Fixture 'vdm-output' (New-VdmProfileRecords) (New-ObserverRecords -ActivePairCount 1)
    foreach($verboseMode in @($false,$true)){
        $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$runner,'-ValidateEvidencePrefix',$prefix,'-ValidationHarnessExitCode','0')
        if($verboseMode){$arguments+='-VerboseOperations'}
        $text=@(& powershell.exe @arguments)|Out-String
        if($LASTEXITCODE -ne 0 -or $text -notmatch 'PHASE1 DEBUG vs PHASE2 DEBUG' -or $text -notmatch 'VDM_LIFETIME_FRESHNESS_GATE: PASS' -or
            (($text -match 'OP_PROFILE generation=') -ne $verboseMode) -or (($text -match 'OP_VALIDATION generation=') -ne $verboseMode)) {throw 'Compact/verbose output contract failed'}
        ++$script:vdmFixtureCount
    }
    Write-Output "R1-C3B Phase3 VDM runner fixtures: PASS; count=$script:vdmFixtureCount"
}finally{
    $resolved=[IO.Path]::GetFullPath($fixtureRoot)
    if(-not $resolved.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase) -or $resolved -eq $tempBase){throw 'Unsafe fixture cleanup'}
    if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
