function Assert-C4AConsoleWaitEvidence {
    param([object[]]$Records,[switch]$AllowHistoricalBlock)
    $startup=$Records[0]
    $waits=@($Records|Where-Object type -eq console_wait)
    $marked='console_wait_contract' -in $startup.PSObject.Properties.Name
    if(-not $marked -and $AllowHistoricalBlock -and $waits.Count -eq 0){return}
    $historicalV1=$AllowHistoricalBlock -and $marked -and $startup.console_wait_contract -ceq 'sta_message_pump_v1'
    Assert-C4A ($marked -and ($historicalV1 -or ($startup.console_wait_contract -ceq 'sta_message_pump_v2' -and $startup.console_mode_contract -ceq 'preserve_host_mode_v1')) -and $startup.console_input_contract -ceq 'readconsoleinputex_nowait_v1' -and $startup.owner_sta_thread -gt 0) 'STA console/host-mode contract missing/wrong'
    $kinds=@('member_a_confirmation','member_b_confirmation','member_c_confirmation','group_consent','readiness_recheck','subjective_grade','rigid_body_feel')
    foreach($wait in $waits) {
        Assert-C4A ($wait.input_wait_kind -cin $kinds -and $wait.wait_result -ceq 'complete' -and $wait.error -eq 0) 'console wait failed'
        if($historicalV1){Assert-C4A ($wait.console_mode_restored -eq $true) 'historical mode restore failed'}
        else {
            Assert-C4A ($wait.modes_observed -eq $true -and $wait.mode_changed -eq $false -and $wait.input_mode_before -eq $wait.input_mode_after) 'host mode not preserved'
            foreach($field in @('input_mode_before','input_mode_after')) {
                Assert-C4A (($wait.$field -is [int] -or $wait.$field -is [long]) -and $wait.$field -ge 0 -and $wait.$field -le [uint32]::MaxValue) 'invalid observed console mode'
            }
        }
        Assert-C4A ($wait.owner_thread -eq $startup.owner_sta_thread -and $wait.wait_call_count -gt 0 -and $wait.pump_call_count -gt 0 -and $wait.pump_call_count -le $wait.wait_call_count -and $wait.console_input_event_count -gt 0 -and $wait.message_dispatch_count -ge 0 -and $wait.message_dispatch_count -le 64*$wait.pump_call_count) 'invalid wait/dispatch counters'
        foreach($field in @('characters','text','line','key_code','scan_code','message_contents','clipboard_contents','copied_path')) {
            Assert-C4A ($field -notin $wait.PSObject.Properties.Name) 'console wait contains input/message contents'
        }
    }
    for($i=0;$i -lt 3;++$i) {
        $kind=$kinds[$i];$match=@($waits|Where-Object input_wait_kind -eq $kind)
        $prompt=@($Records|Where-Object {$_.type -eq 'target_prompt' -and $_.member -eq $i})
        $confirmation=@($Records|Where-Object {$_.type -eq 'target_confirmed' -and $_.member -eq $i})
        Assert-C4A ($match.Count -eq 1 -and $prompt.Count -eq 1 -and $confirmation.Count -eq 1 -and $match[0].sequence -gt $prompt[0].sequence -and $match[0].sequence -lt $confirmation[0].sequence) 'member confirmation lacks STA-safe wait'
    }
    $groupWait=@($waits|Where-Object input_wait_kind -eq group_consent)
    $consent=@($Records|Where-Object type -eq group_consent)
    $confirmed=@($Records|Where-Object type -eq target_confirmed)
    Assert-C4A ($groupWait.Count -eq 1 -and $consent.Count -eq 1 -and $groupWait[0].sequence -gt ($confirmed.sequence|Measure-Object -Maximum).Maximum -and $groupWait[0].sequence -lt $consent[0].sequence) 'group consent lacks STA-safe wait'
    $previews=@($Records|Where-Object type -eq readiness_preview)
    $rechecks=@($waits|Where-Object input_wait_kind -eq readiness_recheck)
    $accepted=@($Records|Where-Object type -eq readiness_accepted)
    foreach($wait in $rechecks) {
        $previous=@($previews|Where-Object sequence -lt $wait.sequence|Select-Object -Last 1)
        Assert-C4A ($previous.Count -eq 1 -and $previous[0].ready -eq $false -and ($accepted.Count -eq 0 -or $wait.sequence -lt $accepted[0].sequence)) 'readiness wait outside pre-setup adjustment'
    }
    for($i=1;$i -lt $previews.Count;++$i) {
        if(-not $previews[$i].setup_check) {
            $between=@($rechecks|Where-Object {$_.sequence -gt $previews[$i-1].sequence -and $_.sequence -lt $previews[$i].sequence})
            Assert-C4A ($between.Count -gt 0) 're-preview lacks pumped human wait'
        }
    }
    if($Records[-1].result -ceq 'PASS') {
        $grade=@($waits|Where-Object input_wait_kind -eq subjective_grade)
        $feel=@($waits|Where-Object input_wait_kind -eq rigid_body_feel)
        $summary=@($Records|Where-Object type -eq summary)
        $subjective=@($Records|Where-Object type -eq subjective)
        Assert-C4A ($grade.Count -eq 1 -and $feel.Count -eq 1 -and $summary.Count -eq 1 -and $subjective.Count -eq 1 -and $grade[0].sequence -gt $summary[0].sequence -and $feel[0].sequence -gt $grade[0].sequence -and $feel[0].sequence -lt $subjective[0].sequence) 'subjective waits not STA-safe/outside completed runtime'
    } elseif($Records[-1].reason -ceq 'readiness_cancelled') {
        Assert-C4A ($rechecks.Count -gt 0 -and $rechecks[-1].sequence -gt $previews[-1].sequence) 'readiness Q cancellation lacks pumped wait'
    }
}
