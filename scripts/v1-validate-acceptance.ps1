[CmdletBinding()]
param([switch]$SelfTest)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$report = Join-Path $repo 'docs\reports\V1_ACCEPTANCE.json'

function Value($Object, [string]$Name) {
    if ($null -eq $Object) { return $null }
    $property = $Object.PSObject.Properties[$Name]
    if ($property) { return $property.Value }
    return $null
}
function Test-Acceptance($Data) {
    $issues = New-Object 'Collections.Generic.List[string]'
    function Issue([string]$Message) { [void]$issues.Add($Message) }
    function Exact-Ids($Rows, [string[]]$Expected, [string]$Section) {
        $ids = @($Rows | ForEach-Object { [string](Value $_ 'id') })
        if ($ids.Count -ne $Expected.Count -or @($ids | Select-Object -Unique).Count -ne $Expected.Count -or
            @(Compare-Object -ReferenceObject $Expected -DifferenceObject $ids -CaseSensitive).Count) {
            Issue "$Section must contain exactly the frozen unique IDs."
        }
    }
    $modules = @(1..12 | ForEach-Object { 'M{0:D2}' -f $_ })
    $features = @($modules | ForEach-Object { $module=$_; 1..5 | ForEach-Object { "$module-F{0:D2}" -f $_ } })
    $mandatory = @(1..32 | ForEach-Object { 'B{0:D2}' -f $_ })
    $deferred = @(33..40 | ForEach-Object { 'B{0:D2}' -f $_ })
    $chains = @(1..12 | ForEach-Object { 'I{0:D2}' -f $_ })
    $environments = @(1..6 | ForEach-Object { 'E{0:D2}' -f $_ })
    Exact-Ids @(Value $Data 'features') $features 'features'
    Exact-Ids @(Value $Data 'modules') $modules 'modules'
    Exact-Ids @(Value $Data 'benchmarks') ($mandatory+$deferred) 'benchmarks'
    Exact-Ids @(Value $Data 'integration_chains') $chains 'integration_chains'
    Exact-Ids @(Value $Data 'environments') $environments 'environments'
    foreach ($module in @(Value $Data 'modules')) {
        $wanted = @(1..5 | ForEach-Object { "$(Value $module 'id')-F{0:D2}" -f $_ })
        $actual = @(Value $module 'feature_ids')
        if ($actual.Count -ne 5 -or @(Compare-Object $wanted $actual -CaseSensitive).Count) { Issue "$(Value $module 'id') has incorrect feature membership." }
    }

    function Pass-Binding($Row) {
        $id = [string](Value $Row 'id')
        $source = [string](Value $Row 'source_sha'); $binary = [string](Value $Row 'binary_sha256')
        $run = [string](Value $Row 'run_id'); $env = [string](Value $Row 'environment_id')
        $paths = @(Value $Row 'evidence_paths')
        $environment = @((Value $Data 'environments') | Where-Object { (Value $_ 'id') -ceq $env -or
            ($env -and (Value $_ 'environment_id') -ceq $env) })
        if ($source -cnotmatch '^[0-9a-fA-F]{40}$' -or $source -match '^0+$' -or
            $binary -cnotmatch '^[0-9a-fA-F]{64}$' -or $binary -match '^0+$' -or
            $run -cnotmatch '^[0-9a-fA-F]{32}$' -or -not $env -or $environment.Count -ne 1 -or
            -not $paths.Count -or $paths.Count -gt 32) {
            Issue "$id PASS has no complete source/binary/environment/run/evidence binding."
            return
        }
        $observed = New-Object Text.StringBuilder
        [long]$total = 0
        foreach ($path in $paths) {
            try {
                if (-not ($path -is [string]) -or [string]::IsNullOrWhiteSpace($path)) { throw 'empty evidence path' }
                $full = [IO.Path]::GetFullPath((Join-Path $repo $path))
                if (-not $full.StartsWith($repo+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'evidence outside repository' }
                $cursor=$full
                while ($cursor -and $cursor -cne $repo) {
                    if ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'reparse evidence path' }
                    $cursor=[IO.Path]::GetDirectoryName($cursor)
                }
                $item=Get-Item -LiteralPath $full -Force
                if ($item.PSIsContainer -or $item.Length -le 0) { throw 'missing or empty evidence file' }
                if ($item.Extension -in @('.json','.jsonl','.txt','.md')) {
                    $total+=$item.Length
                    if ($item.Length -gt 8MB -or $total -gt 32MB) { throw 'evidence text exceeds bounded read' }
                    [void]$observed.Append([IO.File]::ReadAllText($full))
                }
            } catch { Issue "$id invalid evidence '$path': $($_.Exception.Message)" }
        }
        # A screenshot or manually populated metadata alone cannot bind a run.
        # This checks recorded identities, not whether a native scenario passed.
        $text=$observed.ToString()
        foreach ($identity in @($source,$binary,$run)) {
            if ($text.IndexOf($identity,[StringComparison]::OrdinalIgnoreCase) -lt 0) { Issue "$id PASS identity is not present in its declared evidence files." }
        }
    }
    foreach ($section in @('features','modules','benchmarks','integration_chains','environments')) {
        foreach ($row in @(Value $Data $section)) {
            $id=[string](Value $row 'id'); $dev=[string](Value $row 'dev_status')
            if ($dev -cnotin @('PASS','FAIL','NOT_RUN','BLOCKED','DEFERRED')) { Issue "$id has invalid DEV status; IMPLEMENTED is not DEV PASS." }
            $passes=$dev -ceq 'PASS' -or (Value $row 'human_status') -ceq 'PASS' -or (Value $row 'status') -ceq 'PASS'
            if ($passes) { Pass-Binding $row }
            foreach ($case in @(Value $row 'cases')) {
                if ($null -ne $case -and (Value $case 'status') -ceq 'PASS') { Pass-Binding $row }
            }
            if ($section -eq 'environments' -and $passes) {
                foreach ($field in @('actual_os_build','actual_integrity','actual_session','display_configuration')) {
                    if ($null -eq (Value $row $field) -or [string](Value $row $field) -in @('','UNKNOWN','NOT_RUN')) { Issue "$id PASS lacks actual environment facts: $field." }
                }
                if ((Value $row 'available') -ne $true) { Issue "$id PASS environment is not recorded available." }
            }
        }
    }
    [int]$passed=0
    foreach ($row in @(Value $Data 'benchmarks')) {
        $id=[string](Value $row 'id'); $required=$mandatory -ccontains $id; $pass=(Value $row 'dev_status') -ceq 'PASS'
        if ((Value $row 'mandatory') -ne $required -or (Value $row 'weight') -ne 2.5) { Issue "$id changed mandatory flag or fixed 2.5 weight." }
        if (-not $required -and (Value $row 'dev_status') -cne 'DEFERRED') { Issue "$id must remain DEFERRED, zero points." }
        $points=0; if ($required -and $pass) { $points=2.5; ++$passed }
        if ((Value $row 'points_awarded') -ne $points) { Issue "$id points must come only from actual DEV PASS, never IMPLEMENTED." }
    }
    $score=Value $Data 'scorecard'
    foreach ($pair in @(@('denominator',40),@('mandatory_count',32),@('required_points',80),@('pass_count',$passed),
        @('points',($passed*2.5)),@('coverage_percent',($passed*2.5)),@('mandatory_all_pass',($passed -eq 32)))) {
        if ((Value $score $pair[0]) -ne $pair[1]) { Issue "scorecard.$($pair[0]) does not match the frozen benchmark results." }
    }
    if (@(Compare-Object $mandatory @(Value $score 'mandatory_ids') -CaseSensitive).Count -or
        @(Value $score 'mandatory_ids').Count -ne 32 -or
        @(Compare-Object $deferred @(Value $score 'deferred_ids') -CaseSensitive).Count -or
        @(Value $score 'deferred_ids').Count -ne 8) { Issue 'scorecard mandatory/deferred lists changed.' }
    if ((Value $Data 'dev_complete') -eq $true) {
        foreach ($section in @('features','modules','integration_chains')) {
            if (@((Value $Data $section) | Where-Object { (Value $_ 'dev_status') -cne 'PASS' }).Count) { Issue "DEV_COMPLETE requires every $section item PASS." }
        }
        if ($passed -ne 32 -or @((Value $Data 'environments') | Where-Object { (Value $_ 'id') -cne 'E06' -and (Value $_ 'dev_status') -cne 'PASS' }).Count) {
            Issue 'DEV_COMPLETE cannot replace mandatory benchmarks or E01-E05.'
        }
    }
    [PSCustomObject]@{valid=($issues.Count -eq 0);errors=@($issues.ToArray());benchmark_pass=$passed;coverage_percent=($passed*2.5)}
}

try {
    $data=Get-Content -LiteralPath $report -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($SelfTest) {
        # Isolated in-memory counterexamples; never edit the actual report.
        $seed=$data | ConvertTo-Json -Depth 25 | ConvertFrom-Json
        foreach ($section in @('features','modules','benchmarks','integration_chains','environments')) {
            foreach ($row in $seed.$section) {
                $row.dev_status='NOT_RUN'
                if ($row.PSObject.Properties['human_status']) { $row.human_status='NOT_RUN' }
                # Existing cases may be shared-ID strings or explicit result
                # objects. Only objects actually carrying status are mutated;
                # references are not result claims and have no writable status.
                foreach ($case in @(Value $row 'cases')) {
                    if ($null -ne $case -and $case -isnot [string] -and $case.PSObject.Properties['status']) {
                        $case.status='NOT_RUN'
                    }
                }
            }
        }
        foreach ($row in $seed.benchmarks) { $row.points_awarded=0; if (-not $row.mandatory) { $row.dev_status='DEFERRED' } }
        $seed.dev_complete=$false; $seed.scorecard.pass_count=0; $seed.scorecard.points=0; $seed.scorecard.coverage_percent=0; $seed.scorecard.mandatory_all_pass=$false
        $failures=New-Object 'Collections.Generic.List[string]'
        function Expect([string]$Name,$Candidate,[bool]$Expected) {
            $script:selfChecks++
            if ((Test-Acceptance $Candidate).valid -ne $Expected) { [void]$failures.Add($Name) }
        }
        $script:selfChecks=0
        Expect 'unclaimed baseline is valid but awards zero' $seed $true
        $bad=$seed | ConvertTo-Json -Depth 25 | ConvertFrom-Json; $bad.features[1].id=$bad.features[0].id
        Expect 'duplicate feature ID must fail' $bad $false
        $bad=$seed | ConvertTo-Json -Depth 25 | ConvertFrom-Json; $bad.scorecard.pass_count=3; $bad.scorecard.points=7.5; $bad.scorecard.coverage_percent=7.5
        Expect 'invented benchmark score must fail' $bad $false
        $bad=$seed | ConvertTo-Json -Depth 25 | ConvertFrom-Json; $bad.features[0].dev_status='PASS'; $bad.features[0].source_sha=$null; $bad.features[0].evidence_paths=@()
        Expect 'PASS without evidence must fail' $bad $false
        $bad=$seed | ConvertTo-Json -Depth 25 | ConvertFrom-Json; $bad.benchmarks[32].dev_status='PASS'; $bad.benchmarks[32].points_awarded=2.5
        Expect 'deferred benchmark cannot earn points' $bad $false
        [PSCustomObject]@{schema='panebind-v1-acceptance-validator-selftest/1';checks=$script:selfChecks;failures=@($failures.ToArray());passed=($failures.Count -eq 0);actual_report_modified=$false} | ConvertTo-Json -Depth 4
        if ($failures.Count) { exit 1 }; exit 0
    }
    $result=Test-Acceptance $data
    [PSCustomObject]@{schema='panebind-v1-acceptance-consistency/1';report=$report;valid=$result.valid;errors=$result.errors;
        benchmark_pass=$result.benchmark_pass;coverage_percent=$result.coverage_percent;native_cases_rerun=$false;product_DEV_PASS_granted_by_validator=$false} | ConvertTo-Json -Depth 5
    if (-not $result.valid) { exit 1 }; exit 0
} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }
