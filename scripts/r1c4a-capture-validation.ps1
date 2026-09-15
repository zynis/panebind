function Assert-C4ACaptureFields {
    param($Value,[string[]]$Fields)
    Assert-C4A ($null -ne $Value) 'structured capture object missing'
    Assert-C4A (@($Value.PSObject.Properties.Name).Count -eq $Fields.Count) 'unexpected/missing capture diagnostic fields'
    foreach($field in $Fields){Assert-C4A ($field -cin $Value.PSObject.Properties.Name) "missing capture diagnostic $field"}
}
function Assert-C4ACaptureUInt {
    param($Value)
    Assert-C4A (($Value -is [int] -or $Value -is [long] -or $Value -is [decimal]) -and
        [decimal]$Value -ge 0 -and [decimal]$Value -le [uint64]::MaxValue -and [decimal]$Value -eq [decimal]::Truncate([decimal]$Value)) 'invalid capture diagnostic integer'
}
function Assert-C4AStructuredCapture {
    param($Preview)
    $c=$Preview.capture
    Assert-C4ACaptureFields $c @('succeeded','recoverable','fatal','failed_member_index','failure_stage','eligibility_reason','eligibility_code','diagnostic','glue_validation_invalidation','reason','members')
    foreach($field in @('succeeded','recoverable','fatal')){Assert-C4A ($c.$field -is [bool]) 'invalid capture disposition boolean'}
    Assert-C4A (([int]$c.succeeded+[int]$c.recoverable+[int]$c.fatal) -eq 1) 'conflicting capture dispositions'
    foreach($label in @($c.reason,$c.glue_validation_invalidation)){Assert-C4A ($label -cmatch '^[A-Za-z0-9_]{1,80}$') 'capture diagnostics contain free text/path'}
    Assert-C4A ($c.failure_stage -cin @('None','Binding','NativeValidation','ReceiptHealth','Context')) 'unknown capture stage'
    if($null -ne $c.failed_member_index){Assert-C4A ($c.failed_member_index -is [int] -and $c.failed_member_index -in 0,1,2) 'invalid capture failure member'}
    if($null -ne $c.eligibility_reason){Assert-C4A ($c.eligibility_reason -cmatch '^[A-Za-z][A-Za-z0-9]{0,79}$') 'invalid eligibility label';Assert-C4ACaptureUInt $c.eligibility_code}
    else {Assert-C4A ($null -eq $c.eligibility_code) 'eligibility code without reason'}
    if($null -ne $c.diagnostic){Assert-C4ACaptureFields $c.diagnostic @('domain','code');Assert-C4A ($c.diagnostic.domain -in 0,1,2,3) 'invalid diagnostic domain';Assert-C4ACaptureUInt $c.diagnostic.code}
    if($c.succeeded){
        Assert-C4A ($Preview.valid -eq $true -and $c.failure_stage -ceq 'None' -and $null -eq $c.failed_member_index -and $null -eq $c.eligibility_reason -and $null -eq $c.diagnostic -and $c.reason -ceq 'none' -and $c.glue_validation_invalidation -ceq 'none') 'capture success contradicts diagnostics'
    } else {
        Assert-C4A ($Preview.valid -eq $false -and $Preview.ready -eq $false -and $c.failure_stage -cne 'None' -and $c.reason -cne 'none') 'failed capture claimed valid/ready'
        Assert-C4A ($null -ne $c.failed_member_index -or $c.failure_stage -ceq 'Context') 'capture failure omitted member'
        if($c.recoverable){Assert-C4A ($c.failure_stage -ceq 'NativeValidation' -and $c.eligibility_reason -cin @('MonitorChanged','DpiChanged') -and $c.glue_validation_invalidation -ceq 'none' -and $null -eq $Preview.snapshots) 'unsafe recoverable capture'}
    }
    Assert-C4A (@($c.members).Count -eq 3) 'capture requires bounded three-member diagnostics'
    for($i=0;$i -lt 3;++$i){
        $o=$c.members[$i]
        Assert-C4ACaptureFields $o @('member','browser_observed','canonical_identity_matches','anchor_hwnd_matches','location_exact','navigation_epoch','browser_stream_reason','browser')
        Assert-C4A ($o.member -eq $i -and $o.browser_observed -is [bool]) 'capture diagnostic member order'
        foreach($field in @('canonical_identity_matches','anchor_hwnd_matches','location_exact')){Assert-C4A ($null -eq $o.$field -or $o.$field -is [bool]) 'invalid optional anchor fact'}
        Assert-C4A ($o.browser_stream_reason -cmatch '^[A-Za-z0-9_]{1,80}$') 'browser reason contains free text'
        if($o.browser_observed){
            Assert-C4ACaptureUInt $o.navigation_epoch
            $b=$o.browser
            $counts=@('callback_sequence','latest_sequence','navigate_complete_count','matching_navigate_complete_count','unrelated_navigate_complete_count','identity_query_failure_count','quit_count','malformed_count','overflow_count','wrong_thread_count','post_retirement_count','geometry_event_count')
            $flags=@('latest_activity_was_quit','accepting','subscribed','unadvised')
            Assert-C4ACaptureFields $b ($counts+$flags+@('last_geometry_dispid','last_malformed_dispid','subscription_diagnostic'))
            foreach($field in $counts){Assert-C4ACaptureUInt $b.$field}
            foreach($field in $flags){Assert-C4A ($b.$field -is [bool]) 'invalid browser boolean'}
            foreach($field in @('last_geometry_dispid','last_malformed_dispid','subscription_diagnostic')){Assert-C4A (($b.$field -is [int] -or $b.$field -is [long]) -and $b.$field -ge [int]::MinValue -and $b.$field -le [int]::MaxValue) 'invalid bounded browser code'}
            Assert-C4A (($b.geometry_event_count -eq 0 -and $b.last_geometry_dispid -eq 0) -or ($b.geometry_event_count -gt 0 -and $b.last_geometry_dispid -in 264,265,266,267)) 'geometry diagnostic DISPID mismatch'
        } else {Assert-C4A ($null -eq $o.browser -and $null -eq $o.navigation_epoch) 'unobserved browser has invented facts'}
        if($c.succeeded -or $c.recoverable){
            Assert-C4A ($o.browser_observed -and $o.canonical_identity_matches -eq $true -and $o.anchor_hwnd_matches -eq $true -and $o.location_exact -eq $true -and $o.browser_stream_reason -ceq 'none') 'capture authority/anchor not proven'
            $b=$o.browser
            Assert-C4A ($b.callback_sequence -eq $b.latest_sequence -and $b.matching_navigate_complete_count -eq $o.navigation_epoch -and $b.subscribed -and $b.accepting -and -not $b.unadvised -and -not $b.latest_activity_was_quit) 'capture browser lifecycle/epoch invalid'
            foreach($field in @('unrelated_navigate_complete_count','identity_query_failure_count','quit_count','malformed_count','overflow_count','wrong_thread_count','post_retirement_count')){Assert-C4A ($b.$field -eq 0) 'unsafe stream treated as capture success/recoverable'}
        }
    }
}
function Test-C4ACaptureRecords {
    param([object[]]$Records)
    $marked='capture_contract' -in $Records[0].PSObject.Properties.Name
    $previews=@($Records|Where-Object {$_.type -in @('readiness_preview','readiness_accepted')})
    if(-not $marked){
        Assert-C4A ($Records[-1].result -ceq 'BLOCKED') 'Fix 5 PASS requires structured capture contract'
        return [pscustomobject]@{Resolution=$(if($previews.Count){'INSUFFICIENT'}else{'NOT_REACHED'});FailedMemberIndex=$null;FailureStage=$null;EligibilityReason=$null;GlueValidationInvalidation=$null;BrowserAnchorFacts=$null}
    }
    Assert-C4A ($Records[0].capture_contract -ceq 'structured_preaccept_v1') 'unknown capture contract'
    foreach($p in $previews){Assert-C4AStructuredCapture $p}
    $failed=@($previews|Where-Object {$_.capture.succeeded -eq $false}|Select-Object -Last 1)
    if($failed.Count){$c=$failed[0].capture;return [pscustomobject]@{Resolution='COMPLETE';FailedMemberIndex=$c.failed_member_index;FailureStage=$c.failure_stage;EligibilityReason=$c.eligibility_reason;Diagnostic=$c.diagnostic;GlueValidationInvalidation=$c.glue_validation_invalidation;UnderlyingReason=$c.reason;Recoverable=$c.recoverable;BrowserAnchorFacts=$c.members}}
    return [pscustomobject]@{Resolution=$(if($previews.Count){'COMPLETE'}else{'NOT_REACHED'});FailedMemberIndex=$null;FailureStage=$null;EligibilityReason=$null}
}
