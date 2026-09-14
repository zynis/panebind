# Fix 1 fixture contract only. Runtime movement/feedback/timing gates stay in
# r1c4a-evidence-validation.ps1 and are not weakened by readiness classification.
function Get-C4AReadinessSnapshotKey {
    param([object[]]$Snapshots,[switch]$ContextOnly)
    Assert-C4A ($Snapshots.Count -eq 3) 'readiness requires three snapshots'
    $keys=@()
    for($i=0;$i -lt 3;++$i) {
        $s=$Snapshots[$i]
        $fields=@('member','visible','positioning','pid','tid','dpi','monitor','work_area','image','class','current_desktop','exact_location','minimized','maximized')
        foreach($field in $fields){Assert-C4A ($field -in $s.PSObject.Properties.Name) "missing snapshot $field"}
        Assert-C4A ($s.member -eq $i) 'readiness snapshot member order'
        $null=Get-C4ARectKey $s.visible;$null=Get-C4ARectKey $s.positioning;$null=Get-C4ARectKey $s.work_area
        if($ContextOnly){$fields=@($fields|Where-Object {$_ -notin @('visible','positioning')})}
        $keys+=($s|Select-Object -Property $fields|ConvertTo-Json -Depth 5 -Compress)
    }
    return ($keys -join '|')
}
function Assert-C4AReadinessIdentity {
    param([object[]]$Snapshots,[object[]]$Bindings)
    $null=Get-C4AReadinessSnapshotKey $Snapshots
    for($i=0;$i -lt 3;++$i) {
        $s=$Snapshots[$i]
        Assert-C4A ($s.pid -eq $Bindings[$i].pid -and $s.tid -eq $Bindings[$i].tid -and $s.dpi -gt 0 -and $s.dpi -eq $Snapshots[0].dpi -and $s.monitor -ceq $Snapshots[0].monitor -and (Get-C4ARectKey $s.work_area) -ceq (Get-C4ARectKey $Snapshots[0].work_area)) 'readiness identity/monitor/DPI'
        Assert-C4A ([IO.Path]::GetFileName($s.image) -ieq 'explorer.exe' -and $s.class -cin @('CabinetWClass','ExploreWClass') -and $s.current_desktop -eq $true -and $s.exact_location -eq $true -and $s.minimized -eq $false -and $s.maximized -eq $false) 'readiness member eligibility'
    }
}
function Get-C4AReadinessDimensions {
    param([object[]]$Snapshots)
    $sizes=@();foreach($s in $Snapshots){$sizes+=,@(([decimal]$s.visible[2]-[decimal]$s.visible[0]),([decimal]$s.visible[3]-[decimal]$s.visible[1]))}
    $width=[Math]::Max(($sizes[0][0]+$sizes[1][0]),$sizes[2][0])
    $height=[Math]::Max(($sizes[0][1]+$sizes[2][1]),$sizes[1][1])
    $area=$Snapshots[0].work_area
    return [pscustomobject]@{Sizes=$sizes;Width=$width;Height=$height;WidthDeficit=[Math]::Max(0,($width-([decimal]$area[2]-[decimal]$area[0])));HeightDeficit=[Math]::Max(0,($height-([decimal]$area[3]-[decimal]$area[1])))}
}
function Assert-C4ALivePreview {
    param($Preview,[object[]]$Bindings,[object[]]$BindingSnapshots)
    Assert-C4A ($Preview.valid -eq $true -and $Preview.owner_thread -eq $true -and $Preview.group_generation -eq $Bindings[0].group -and $Preview.gesture_generation -eq 0) 'preview validation/authority'
    Assert-C4A ($Preview.native_apply_count -eq 0 -and $Preview.hdwp_begin_count -eq 0 -and $Preview.pending_count -eq 0 -and $Preview.leader_present -eq $false -and $Preview.event_source_running -eq $false) 'preview has native/gesture/hook side effects'
    Assert-C4A ($Preview.capture_qpc -gt 0) 'fresh capture timestamp missing'
    Assert-C4AReadinessIdentity $Preview.snapshots $Bindings
    Assert-C4A ((Get-C4AReadinessSnapshotKey $Preview.snapshots -ContextOnly) -ceq (Get-C4AReadinessSnapshotKey $BindingSnapshots -ContextOnly)) 'preview changed nongeometry context'
    $dimensions=Get-C4AReadinessDimensions $Preview.snapshots
    Assert-C4A (@($Preview.member_sizes).Count -eq 3 -and $Preview.required_width -eq $dimensions.Width -and $Preview.required_height -eq $dimensions.Height -and (Get-C4ARectKey $Preview.work_area) -ceq (Get-C4ARectKey $Preview.snapshots[0].work_area)) 'readiness displayed dimensions'
    for($i=0;$i -lt 3;++$i){Assert-C4A (@($Preview.member_sizes[$i]).Count -eq 2 -and ($Preview.member_sizes[$i] -join ',') -ceq ($dimensions.Sizes[$i] -join ',')) 'member dimensions differ from fresh capture'}
    if($Preview.ready -eq $true -or $Preview.reason -ceq 'work_area_deficit') {
        Assert-C4A ($Preview.width_deficit -eq $dimensions.WidthDeficit -and $Preview.height_deficit -eq $dimensions.HeightDeficit) 'incorrect readiness deficit'
    }
    if($Preview.ready -eq $true){Assert-C4A ($Preview.reason -ceq 'ready' -and $dimensions.WidthDeficit -eq 0 -and $dimensions.HeightDeficit -eq 0) 'FIT contradicts work-area capacity'}
    else {Assert-C4A ($Preview.reason -cin @('work_area_deficit','B_C_overlap_for_l_shape','required_edges_mismatch')) 'invalid NOT FIT reason'}
}
function Test-C4AReadinessRecords {
    param([object[]]$Records,[object[]]$Bindings)
    $legacy=@($Records|Where-Object type -eq readiness)
    $previews=@($Records|Where-Object type -eq readiness_preview)
    $accepted=@($Records|Where-Object type -eq readiness_accepted)
    $binding=@($Records|Where-Object type -eq binding_snapshot)
    $blocked=$Records[-1].result -ceq 'BLOCKED'
    if($legacy.Count) {
        # Compatibility is deliberately BLOCKED-only; old PASS has no accepted
        # live baseline proof and cannot be promoted to a Fix 1 PASS.
        Assert-C4A ($blocked -and $legacy.Count -eq 1 -and $previews.Count -eq 0 -and $accepted.Count -eq 0 -and $binding.Count -eq 0) 'legacy readiness cannot pass Fix 1'
        Assert-C4A (@($Records|Where-Object {$_.type -notin @('startup','target_prompt','target_confirmed','group_consent','binding','readiness','shutdown')}).Count -eq 0) 'runtime activity contradicts readiness block'
        $r=$legacy[0]
        Assert-C4A ($r.ready -eq $false -and $r.reason -ceq 'work_area_deficit' -and $Records[-1].reason -ceq $r.reason -and $r.sequence -gt ($Bindings.sequence|Measure-Object -Maximum).Maximum) 'legacy layout block reason/order'
        Assert-C4AReadinessIdentity $r.original $Bindings
        $dimensions=Get-C4AReadinessDimensions $r.original
        Assert-C4A ($r.width_deficit -eq $dimensions.WidthDeficit -and $r.height_deficit -eq $dimensions.HeightDeficit -and ($r.width_deficit -gt 0 -or $r.height_deficit -gt 0)) 'legacy deficit proof'
        return [pscustomobject]@{Result='BLOCKED_BY_LAYOUT_READINESS';Runtime='NOT_STARTED';WidthDeficit=$r.width_deficit;HeightDeficit=$r.height_deficit;LegacyEvidence=$true}
    }
    Assert-C4A ($Records[0].readiness_contract -ceq 'live_preview_accepted_baseline_v1' -and $binding.Count -eq 1 -and $previews.Count -ge 1) 'live readiness contract missing'
    Assert-C4AReadinessIdentity $binding[0].snapshots $Bindings
    Assert-C4A ($binding[0].sequence -gt ($Bindings.sequence|Measure-Object -Maximum).Maximum) 'binding snapshot chronology'
    $previousQpc=0
    for($i=0;$i -lt $previews.Count;++$i) {
        $p=$previews[$i]
        Assert-C4A ($p.attempt -eq $i+1 -and $p.sequence -gt $binding[0].sequence -and $p.capture_qpc -gt $previousQpc) 'preview attempt/capture chronology'
        Assert-C4ALivePreview $p $Bindings $binding[0].snapshots
        if($p.setup_check){Assert-C4A ($i -gt 0 -and $previews[$i-1].ready -eq $true -and $previews[$i-1].setup_check -eq $false) 'setup recheck lacks preceding FIT preview'}
        $previousQpc=$p.capture_qpc
    }
    if($blocked) {
        Assert-C4A ($accepted.Count -eq 0 -and $previews[-1].ready -eq $false -and $Records[-1].reason -ceq 'readiness_cancelled') 'not a layout-only cancellation'
        Assert-C4A (@($Records|Where-Object {$_.type -notin @('startup','target_prompt','target_confirmed','group_consent','binding','binding_snapshot','readiness_preview','console_wait','shutdown')}).Count -eq 0) 'native/runtime activity before readiness acceptance'
        return [pscustomobject]@{Result='BLOCKED_BY_LAYOUT_READINESS';Runtime='NOT_STARTED';WidthDeficit=$previews[-1].width_deficit;HeightDeficit=$previews[-1].height_deficit;LegacyEvidence=$false}
    }
    Assert-C4A ($accepted.Count -eq 1 -and $Records[-1].result -ceq 'PASS') 'exactly one accepted readiness required'
    $a=$accepted[0];$last=$previews[-1]
    Assert-C4ALivePreview $a $Bindings $binding[0].snapshots
    Assert-C4A ($a.ready -eq $true -and $a.setup_check -eq $true -and $last.ready -eq $true -and $last.setup_check -eq $true -and $a.attempt -eq $last.attempt -and $a.capture_qpc -eq $last.capture_qpc -and $a.sequence -gt $last.sequence) 'accepted baseline is not the fresh setup check'
    Assert-C4A ((Get-C4AReadinessSnapshotKey $a.snapshots) -ceq (Get-C4AReadinessSnapshotKey $last.snapshots)) 'accepted baseline differs from setup capture'
    $setup=@($Records|Where-Object {$_.type -eq 'batch' -and $_.phase -eq 'setup'})
    Assert-C4A ($setup.Count -eq 1 -and $setup[0].native_start_qpc -gt $a.capture_qpc -and $setup[0].sequence -gt $a.sequence) 'setup occurred before accepted fresh capture'
    Assert-C4A ((Get-C4AReadinessSnapshotKey $setup[0].before) -ceq (Get-C4AReadinessSnapshotKey $a.snapshots)) 'setup baseline is not accepted baseline'
    foreach($operation in @($Records|Where-Object type -eq batch)) {
        Assert-C4A ($operation.native_start_qpc -gt $a.capture_qpc) 'native operation before accepted baseline'
    }
    foreach($receipt in @($Records|Where-Object type -eq receipt)) {
        Assert-C4A ($receipt.callback_qpc -gt $a.capture_qpc) 'event source active before accepted baseline'
    }
    return [pscustomobject]@{Result='PASS';AcceptedSnapshots=$a.snapshots;PreviewAttempts=$previews.Count}
}
