[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [string]$BuildDirectory,
    [switch]$ListOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
function Assert-OfflineTestGate([bool]$Condition,[string]$Reason){if(-not $Condition){throw $Reason}}
function Get-OfflineTestAudit{
    # Each entry was inspected from CMake's real command and source call path.
    # New/renamed tests stay blocked until this small positive audit is updated.
    $audit=[ordered]@{}
    foreach($entry in @(
        @('move-handoff-gate','panebind-move-handoff-gate-tests','offline',$false),
        @('cursor-move-magnet-intent','panebind-cursor-move-magnet-intent-tests','offline',$false),
        @('magnet-gesture','panebind-magnet-gesture-tests','offline',$false),
        @('magnet-constraint-solver','panebind-magnet-tests','offline',$false),
        @('glue-group-move','panebind-glue-group-move-tests','offline',$false),
        @('geometry','panebind-geometry-tests','offline',$false),
        @('core-model','panebind-core-model-tests','offline',$false),
        @('topology','panebind-topology-tests','offline',$false),
        @('translation','panebind-translation-tests','offline',$false),
        @('glue-move-coordinator','panebind-glue-move-coordinator-tests','offline',$false),
        @('test-foreground-bootstrap-model','panebind-test-foreground-model-tests','offline',$true),
        @('windows-magnet-postverify','panebind-magnet-postverify-tests','offline',$true),
        @('test-handoff-preflight-diagnostic','panebind-handoff-preflight-tests','offline',$true),
        @('test-product-input-isolation','panebind-input-isolation-tests','offline',$true),
        @('test-owned-native-abort-diagnostic','panebind-owned-abort-diagnostic-tests','offline',$true),
        @('windows-rect-adjustment','panebind-rect-adjustment-tests','offline',$true),
        @('windows-magnet-owned-probe','panebind-magnet-native-probe','interactive',$true),
        @('explorer-browser-sink','panebind-browser-sink-tests','offline',$true),
        @('explorer-group-mutable-capture','panebind-group-capture-tests','offline',$true),
        @('windows-explorer-glue-activation-unit','panebind-windows-explorer-glue-activation-tests','interactive',$true),
        @('windows-explorer-glue-profile-unit','panebind-windows-explorer-glue-profile-tests','offline',$true),
        @('windows-explorer-vdm-unit','panebind-windows-explorer-vdm-tests','interactive',$true),
        @('windows-text-encoding','panebind-windows-text-encoding-tests','offline',$true),
        @('windows-owned-operations-unit','panebind-windows-owned-operations-tests','offline',$true),
        @('windows-companion-unit','panebind-windows-companion-tests','offline',$true),
        @('windows-explorer-unit','panebind-windows-explorer-tests','offline',$true),
        @('windows-explorer-glue-event-source-unit','panebind-windows-explorer-glue-event-source-tests','interactive',$true),
        @('windows-explorer-glue-session-unit','panebind-windows-explorer-glue-session-tests','offline',$true),
        @('explorer-group-batch','panebind-explorer-group-batch-tests','interactive',$true),
        @('explorer-group-event','panebind-explorer-group-event-tests','interactive',$true),
        @('explorer-group-layout','panebind-explorer-group-layout-tests','offline',$true),
        @('explorer-group-readiness','panebind-explorer-group-readiness-tests','offline',$true),
        @('sta-console-pump','panebind-sta-console-tests','interactive',$true)
    )){
        $audit[$entry[0]]=[pscustomobject]@{Target=$entry[1];Phase=$entry[2];Windows=$entry[3]}
    }
    return $audit
}
function Select-OfflineTestInventory($Inventory,[string]$Build,[string]$Configuration,[bool]$Preview=$false){
    Assert-OfflineTestGate ($Inventory.kind -ceq 'ctestInfo' -and $Inventory.version.major -eq 1) 'Unsupported CTest JSON inventory'
    $audit=Get-OfflineTestAudit
    $tests=@($Inventory.tests)
    Assert-OfflineTestGate ($tests.Count -gt 0) 'Offline selection is empty; never fall back to full CTest'
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $offline=@()
    foreach($test in $tests){
        Assert-OfflineTestGate ($seen.Add([string]$test.name)) 'Duplicate CTest name'
        Assert-OfflineTestGate ($audit.Contains([string]$test.name)) "UNCLASSIFIED test: $($test.name)"
        $known=$audit[[string]$test.name]
        $labelProperties=@($test.properties|Where-Object name -CEQ 'LABELS')
        Assert-OfflineTestGate ($labelProperties.Count -eq 1) "Missing/duplicate phase label: $($test.name)"
        $labels=@($labelProperties[0].value)
        Assert-OfflineTestGate ($labels.Count -eq 1 -and $labels[0] -ceq $known.Phase) "Unknown/mixed/wrong phase label: $($test.name)"
        Assert-OfflineTestGate (@($test.command).Count -eq 1) "Unreviewed command arguments: $($test.name)"
        $relative=if($known.Windows){"src/platform/windows/$Configuration/$($known.Target).exe"}else{"$Configuration/$($known.Target).exe"}
        $expected=[IO.Path]::GetFullPath([IO.Path]::Combine($Build,$relative))
        Assert-OfflineTestGate ([IO.Path]::GetFullPath([string]$test.command[0]).Equals($expected,[StringComparison]::OrdinalIgnoreCase)) "Unreviewed executable: $($test.name)"
        # Fixture/dependency expansion can silently add another command. None
        # exists in this audited set; refuse additions rather than guessing.
        Assert-OfflineTestGate (@($test.properties|Where-Object {$_.name -cin @('FIXTURES_REQUIRED','FIXTURES_SETUP','FIXTURES_CLEANUP','DEPENDS')}).Count -eq 0) "Unreviewed fixture/dependency: $($test.name)"
        if($Preview){Assert-OfflineTestGate ($known.Phase -ceq 'offline') "Interactive test entered offline preview: $($test.name)"}
        if($known.Phase -ceq 'offline'){$offline+=@([string]$test.name)}
    }
    if(-not $Preview){
        Assert-OfflineTestGate ($seen.Count -eq $audit.Count) 'Audited registration missing from full inventory'
        foreach($name in $audit.Keys){Assert-OfflineTestGate ($seen.Contains($name)) "Audited test omitted: $name"}
    }
    Assert-OfflineTestGate ($offline.Count -gt 0) 'Offline selection is empty; never fall back to full CTest'
    return @($offline|Sort-Object)
}
function Get-OfflineTestRegex([string[]]$Names){
    Assert-OfflineTestGate ($Names.Count -gt 0) 'Cannot construct an empty positive selection'
    return '^('+(@($Names|ForEach-Object {[regex]::Escape($_)}) -join '|')+')$'
}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $BuildDirectory){$BuildDirectory='out/r1c4b-live-magnet-'+$Configuration.ToLowerInvariant()}
$build=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $repo $BuildDirectory}))
try{
    $cache=Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt') -Encoding UTF8
    $ctestEntries=@($cache|Where-Object {$_ -cmatch '^CMAKE_CTEST_COMMAND:INTERNAL='})
    $sourceEntries=@($cache|Where-Object {$_ -cmatch '^CMAKE_HOME_DIRECTORY:INTERNAL='})
    Assert-OfflineTestGate ($ctestEntries.Count -eq 1 -and $sourceEntries.Count -eq 1) 'Configured CMake/CTest identity missing'
    Assert-OfflineTestGate ([IO.Path]::GetFullPath($sourceEntries[0].Substring($sourceEntries[0].IndexOf('=')+1)).Equals($repo,[StringComparison]::OrdinalIgnoreCase)) 'Build belongs to another repository'
    $ctest=$ctestEntries[0].Substring($ctestEntries[0].IndexOf('=')+1)
    Assert-OfflineTestGate (Test-Path -LiteralPath $ctest -PathType Leaf) 'Configured CTest executable missing'
    $version=@(& $ctest --version);Assert-OfflineTestGate ($LASTEXITCODE -eq 0) 'CTest version query failed'
    $help=@(& $ctest --help);Assert-OfflineTestGate ($LASTEXITCODE -eq 0 -and ($help -join ' ') -match 'json-v1' -and ($help -join ' ') -match '--no-tests=') 'Local CTest lacks required fail-closed selection options'
    $baseArguments=@('--test-dir',$build,'-C',$Configuration)
    $allJson=@(& $ctest @baseArguments --show-only=json-v1)
    Assert-OfflineTestGate ($LASTEXITCODE -eq 0) 'CTest inventory query failed'
    $inventory=($allJson -join [Environment]::NewLine)|ConvertFrom-Json
    $names=@(Select-OfflineTestInventory $inventory $build $Configuration)
    $regex=Get-OfflineTestRegex $names
    $selectionArguments=$baseArguments+@('-R',$regex,'-L','^offline$','--no-tests=error')
    $selectedJson=@(& $ctest @selectionArguments --show-only=json-v1)
    Assert-OfflineTestGate ($LASTEXITCODE -eq 0) 'CTest selected preview query failed'
    $preview=($selectedJson -join [Environment]::NewLine)|ConvertFrom-Json
    $selected=@(Select-OfflineTestInventory $preview $build $Configuration $true)
    Assert-OfflineTestGate (($selected -join [char]0) -ceq ($names -join [char]0)) 'Actual selected commands differ from audited positive set'
    Write-Host "$($version[0]); configuration=$Configuration; offline_count=$($names.Count)"
    foreach($name in $names){Write-Host "offline: $name"}
    if($ListOnly){exit 0}
    # This is the ONLY execution branch: same exact positive name/label filter
    # already inspected above. No unfiltered CTest, retries or fallback.
    $runArguments=$selectionArguments+@('--output-on-failure','--stop-on-failure')
    & $ctest @runArguments
    exit $LASTEXITCODE
}catch{
    Write-Error ("Offline selection refused: "+$_.Exception.Message) -ErrorAction Continue
    exit 2
}
