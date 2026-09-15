# BLOCKED is a stage/reason diagnosis, never successful runtime evidence.
function Get-C4ABlockedClassification {
    param([object[]]$Records)
    $shutdown=$Records[-1]
    Assert-C4A ('reason' -in $shutdown.PSObject.Properties.Name -and -not [string]::IsNullOrWhiteSpace($shutdown.reason)) 'BLOCKED shutdown lacks original reason'
    $prompts=@($Records|Where-Object type -eq target_prompt)
    $confirmed=@($Records|Where-Object type -eq target_confirmed)
    $consent=@($Records|Where-Object type -eq group_consent)
    $bindings=@($Records|Where-Object type -eq binding)
    $runtime=@($Records|Where-Object {$_.type -in @('batch','receipt','quantum','gesture','feedback','summary')})
    Assert-C4A (@($Records|Where-Object {$_.type -notin @('startup','shutdown','target_prompt','target_confirmed','console_wait','group_consent','binding','binding_snapshot','readiness','readiness_preview','readiness_accepted','topology_consent','batch','receipt','quantum','gesture','feedback','summary','subjective')}).Count -eq 0) 'unknown records in BLOCKED evidence'
    Assert-C4A ($prompts.Count -le 3 -and $confirmed.Count -le $prompts.Count -and $consent.Count -le 1 -and $bindings.Count -le 3) 'invalid partial authorization cardinality'
    for($i=0;$i -lt $prompts.Count;++$i){
        $p=$prompts[$i];Assert-C4A ($p.member -eq $i -and $p.baseline_generation -gt 0 -and $p.prompt_generation -gt $p.baseline_generation) 'partial prompt progression'
        if($i){Assert-C4A ($confirmed.Count -ge $i -and $confirmed[$i-1].sequence -lt $p.sequence) 'provisioning order'}
    }
    for($i=0;$i -lt $confirmed.Count;++$i){
        $c=$confirmed[$i];$p=$prompts[$i]
        Assert-C4A ($c.member -eq $i -and $c.sequence -gt $p.sequence -and $c.target_confirmation_generation -gt $p.prompt_generation -and $c.eligibility_generation -gt $c.target_confirmation_generation -and $c.token_generation -gt $c.eligibility_generation) 'partial confirmation chronology'
        Assert-C4A ($c.baseline_exclusion_complete -eq $true -and $c.unique_new_target -eq $true -and $c.exact_location -eq $true -and $c.token_issued -eq $true -and $c.input_source -ceq 'interactive_console') 'partial confirmation authority'
    }
    if($consent.Count){Assert-C4A ($confirmed.Count -eq 3 -and $consent[0].confirmed -eq $true -and $consent[0].input_source -ceq 'interactive_console' -and $consent[0].sequence -gt $confirmed[-1].sequence) 'partial group consent'}
    if($bindings.Count){Assert-C4A ($consent.Count -eq 1 -and $bindings[0].sequence -gt $consent[0].sequence) 'binding without consent'}
    $waits=@($Records|Where-Object type -eq console_wait)
    foreach($wait in $waits){
        # Failures/cancellation are precisely why this is BLOCKED. Preserve their
        # statuses, while refusing fabricated input-content or foreign-owner facts.
        Assert-C4A ($wait.owner_thread -eq $Records[0].owner_sta_thread) 'blocked wait owner'
        foreach($field in @('characters','text','line','key_code','scan_code','clipboard_contents','copied_path','message_contents')){Assert-C4A ($field -notin $wait.PSObject.Properties.Name) 'blocked log contains input contents'}
    }
    if($runtime.Count){
        Assert-C4A ($bindings.Count -eq 3 -and $consent.Count -eq 1 -and $confirmed.Count -eq 3) 'runtime requires exact authorized set'
        Assert-C4A (@($Records|Where-Object type -eq readiness_accepted).Count -eq 1) 'runtime before accepted topology'
        $stage='GESTURE'
        if(@($Records|Where-Object {$_.type -eq 'batch' -and $_.phase -eq 'restore'}).Count){$stage='RESTORE'}
        $gestures=@($Records|Where-Object type -eq gesture|Sort-Object gesture)
        if($gestures.Count -eq 3 -and @($gestures|Where-Object exact -ne $true).Count -eq 0 -and
           ($gestures.gesture -join ',') -ceq '1,2,3' -and ($gestures.source_member -join ',') -ceq '0,1,2') {
            # Restore capture can fail before creating any native batch record;
            # a final gesture failure can also occur here. Use the explicit new
            # harness stage rather than guessing from completed gestures alone.
            if('stage' -in $shutdown.PSObject.Properties.Name -and $shutdown.stage -ceq 'RESTORE'){$stage='RESTORE'}
            elseif($stage -cne 'RESTORE' -and 'stage' -notin $shutdown.PSObject.Properties.Name){$stage='POST_GESTURE_PRE_RESTORE'}
        }
        if(@($waits|Where-Object {$_.input_wait_kind -in @('subjective_grade','rigid_body_feel')}).Count){$stage='SUBJECTIVE'}
    } elseif(@($Records|Where-Object {$_.type -in @('binding_snapshot','readiness','readiness_preview','readiness_accepted')}).Count){$stage='TOPOLOGY_PREVIEW'}
    elseif($consent.Count){$stage='GROUP_BIND'}
    elseif($confirmed.Count -eq 3){$stage='GROUP_CONSENT'}
    else {$stage=@('MEMBER_A_PROVISIONING','MEMBER_B_PROVISIONING','MEMBER_C_PROVISIONING')[$confirmed.Count]}
    if('stage' -in $shutdown.PSObject.Properties.Name){Assert-C4A ($shutdown.stage -ceq $stage) 'reported shutdown stage contradicts observed progression'}
    return [pscustomobject]@{Result="BLOCKED_DURING_$stage";Reason=$shutdown.reason;Runtime=$(if($runtime.Count){'ENTERED_NOT_PASSED'}else{'NOT_STARTED'});TargetPromptCount=$prompts.Count;TargetConfirmedCount=$confirmed.Count;BindingCount=$bindings.Count;GroupConsentPresent=($consent.Count -eq 1);ConsoleWaitResults=@($waits|Select-Object input_wait_kind,wait_result,error)}
}
