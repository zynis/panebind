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
function Test-C4ALegacyReadinessRecords {
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

# C4 architecture correction: independently recompute exact R1-A adjacency.
# Edge numbers follow Core Edge {Left=0, Top=1, Right=2, Bottom=3}.
function Get-C4ATopology {
    param([object[]]$Snapshots)
    $pairs=@();$neighbors=@(@(0),@(1),@(2));$count=0
    for($i=0;$i -lt 3;++$i){
        for($j=$i+1;$j -lt 3;++$j){
            $a=$Snapshots[$i].visible;$b=$Snapshots[$j].visible
            $xo=[Math]::Max(0,([Math]::Min([decimal]$a[2],[decimal]$b[2])-[Math]::Max([decimal]$a[0],[decimal]$b[0])))
            $yo=[Math]::Max(0,([Math]::Min([decimal]$a[3],[decimal]$b[3])-[Math]::Max([decimal]$a[1],[decimal]$b[1])))
            $choices=@(
                [pscustomobject]@{first_edge=2;second_edge=0;signed_gap=([decimal]$b[0]-[decimal]$a[2]);orthogonal_overlap=$yo;order=0},
                [pscustomobject]@{first_edge=0;second_edge=2;signed_gap=([decimal]$a[0]-[decimal]$b[2]);orthogonal_overlap=$yo;order=1},
                [pscustomobject]@{first_edge=3;second_edge=1;signed_gap=([decimal]$b[1]-[decimal]$a[3]);orthogonal_overlap=$xo;order=2},
                [pscustomobject]@{first_edge=1;second_edge=3;signed_gap=([decimal]$a[1]-[decimal]$b[3]);orthogonal_overlap=$xo;order=3})
            $relations=@($choices|Where-Object {$_.signed_gap -eq 0 -and $_.orthogonal_overlap -gt 0})
            $relation=$relations.Count -eq 1
            if($relation){$c=$relations[0];++$count;$neighbors[$i]+=$j;$neighbors[$j]+=$i}
            else {$c=@($choices|Sort-Object @{Expression={[Math]::Abs($_.signed_gap)}},@{Expression={$_.orthogonal_overlap};Descending=$true},order)[0]}
            $pairs+=[pscustomobject]@{first=$i;second=$j;relation=$relation;first_edge=$c.first_edge;second_edge=$c.second_edge;signed_gap=$c.signed_gap;orthogonal_overlap=$c.orthogonal_overlap}
        }
    }
    $components=@()
    for($i=0;$i -lt 3;++$i){$component=@($i);for($k=0;$k -lt $component.Count;++$k){foreach($n in $neighbors[$component[$k]]){if($n -notin $component){$component+=$n}}};$components+=,@($component|Sort-Object)}
    return [pscustomobject]@{Pairs=$pairs;Components=$components;RelationCount=$count;Ready=($components[0].Count -eq 3)}
}
function Assert-C4ATopologyPreview {
    param($Preview,[object[]]$Bindings,[object[]]$BindingSnapshots)
    Assert-C4A ($Preview.valid -eq $true -and $Preview.owner_thread -eq $true -and $Preview.group_generation -eq $Bindings[0].group -and $Preview.gesture_generation -eq 0) 'preview validation/authority'
    Assert-C4A ($Preview.native_apply_count -eq 0 -and $Preview.hdwp_begin_count -eq 0 -and $Preview.pending_count -eq 0 -and $Preview.leader_present -eq $false -and $Preview.event_source_running -eq $false) 'preview has native/gesture/hook side effects'
    Assert-C4A ($Preview.capture_qpc -gt 0) 'fresh capture timestamp missing'
    Assert-C4AReadinessIdentity $Preview.snapshots $Bindings
    Assert-C4A ((Get-C4AReadinessSnapshotKey $Preview.snapshots -ContextOnly) -ceq (Get-C4AReadinessSnapshotKey $BindingSnapshots -ContextOnly)) 'preview changed nongeometry context'
    Assert-C4A (@($Preview.member_sizes).Count -eq 3 -and (Get-C4ARectKey $Preview.work_area) -ceq (Get-C4ARectKey $Preview.snapshots[0].work_area)) 'displayed topology dimensions'
    for($i=0;$i -lt 3;++$i){$r=$Preview.snapshots[$i].visible;Assert-C4A (($Preview.member_sizes[$i] -join ',') -ceq (@(([decimal]$r[2]-$r[0]),([decimal]$r[3]-$r[1])) -join ',')) 'member dimensions differ from fresh capture'}
    $t=Get-C4ATopology $Preview.snapshots
    Assert-C4A ($Preview.relation_count -eq $t.RelationCount -and $Preview.ready -eq $t.Ready -and @($Preview.pairs).Count -eq 3 -and @($Preview.components).Count -eq 3) 'incorrect topology readiness'
    Assert-C4A ($Preview.reason -ceq $(if($t.Ready){'ready'}else{'disconnected_topology'})) 'incorrect topology reason'
    for($i=0;$i -lt 3;++$i){
        foreach($field in @('first','second','relation','first_edge','second_edge','signed_gap','orthogonal_overlap')){Assert-C4A ($Preview.pairs[$i].$field -eq $t.Pairs[$i].$field) "topology pair $field"}
        Assert-C4A (($Preview.components[$i] -join ',') -ceq ($t.Components[$i] -join ',')) 'topology component mismatch'
    }
}
function Test-C4AReadinessRecords {
    param([object[]]$Records,[object[]]$Bindings)
    $contract=if('readiness_contract' -in $Records[0].PSObject.Properties.Name){$Records[0].readiness_contract}else{''}
    if($contract -cne 'topology_neutral_accepted_baseline_v2') {
        Assert-C4A ($Records[-1].result -ceq 'BLOCKED') 'historical fixed-L evidence cannot pass topology-neutral contract'
        return Test-C4ALegacyReadinessRecords $Records $Bindings
    }
    $binding=@($Records|Where-Object type -eq binding_snapshot);$previews=@($Records|Where-Object type -eq readiness_preview)
    $accepted=@($Records|Where-Object type -eq readiness_accepted);$consents=@($Records|Where-Object type -eq topology_consent)
    Assert-C4A ($binding.Count -eq 1 -and $previews.Count -gt 0) 'topology snapshot/preview missing'
    Assert-C4AReadinessIdentity $binding[0].snapshots $Bindings
    Assert-C4A ($binding[0].sequence -gt ($Bindings.sequence|Measure-Object -Maximum).Maximum) 'binding snapshot chronology'
    $previousQpc=0;$usedConsents=0
    for($i=0;$i -lt $previews.Count;++$i) {
        $p=$previews[$i]
        Assert-C4A ($p.attempt -eq $i+1 -and $p.sequence -gt $binding[0].sequence -and $p.capture_qpc -gt $previousQpc) 'preview attempt/capture chronology'
        Assert-C4ATopologyPreview $p $Bindings $binding[0].snapshots
        if($p.setup_check){
            Assert-C4A ($i -gt 0 -and $previews[$i-1].ready -eq $true -and $previews[$i-1].setup_check -eq $false) 'acceptance needs preceding READY preview'
            $human=@($consents|Where-Object {$_.sequence -gt $previews[$i-1].sequence -and $_.sequence -lt $p.sequence})
            Assert-C4A ($human.Count -eq 1 -and $human[0].confirmed -eq $true -and $human[0].input_source -ceq 'interactive_console' -and $human[0].preview_attempt -eq $previews[$i-1].attempt) 'fresh acceptance lacks human topology consent'
            Assert-C4A (@($Records|Where-Object {$_.type -eq 'console_wait' -and $_.input_wait_kind -eq 'readiness_recheck' -and $_.sequence -gt $previews[$i-1].sequence -and $_.sequence -lt $human[0].sequence}).Count -gt 0) 'topology consent preceded its human wait'
            ++$usedConsents
        }
        $previousQpc=$p.capture_qpc
    }
    Assert-C4A ($usedConsents -eq $consents.Count) 'unmatched topology consent'
    Assert-C4A (@($Records|Where-Object {$_.type -eq 'batch' -and $_.phase -eq 'setup'}).Count -eq 0) 'synthetic setup is prohibited'
    if($accepted.Count -eq 0 -and $Records[-1].result -ceq 'BLOCKED') {
        Assert-C4A (@($Records|Where-Object {$_.type -in @('batch','receipt','gesture','quantum','feedback','summary','subjective')}).Count -eq 0) 'runtime before acceptance'
        return [pscustomobject]@{Result='BLOCKED_DURING_TOPOLOGY_PREVIEW';Reason=$Records[-1].reason;Runtime='NOT_STARTED'}
    }
    Assert-C4A ($accepted.Count -eq 1) 'exactly one accepted topology required'
    $a=$accepted[0];$last=$previews[-1]
    Assert-C4ATopologyPreview $a $Bindings $binding[0].snapshots
    Assert-C4A ($a.ready -eq $true -and $a.setup_check -eq $true -and $last.ready -eq $true -and $last.setup_check -eq $true -and $a.attempt -eq $last.attempt -and $a.capture_qpc -eq $last.capture_qpc -and $a.sequence -gt $last.sequence) 'accepted baseline is not fresh acceptance check'
    Assert-C4A ((Get-C4AReadinessSnapshotKey $a.snapshots) -ceq (Get-C4AReadinessSnapshotKey $last.snapshots)) 'accepted baseline differs from fresh capture'
    foreach($op in @($Records|Where-Object type -eq batch)){Assert-C4A ($op.native_start_qpc -gt $a.capture_qpc -and $op.sequence -gt $a.sequence) 'native operation before acceptance'}
    foreach($r in @($Records|Where-Object type -eq receipt)){Assert-C4A ($r.callback_qpc -gt $a.capture_qpc -and $r.sequence -gt $a.sequence) 'event source active before acceptance'}
    return [pscustomobject]@{Result='PASS';AcceptedSnapshots=$a.snapshots;PreviewAttempts=$previews.Count}
}
