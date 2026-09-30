# Regression, missing evidence, fixture mismatch and hard-budget failures, plus parity with every stored comparison.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../PerformanceComparison.psm1') -Force
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

function New-Receipt {
    $receipt = [ordered]@{}
    foreach ($key in Get-PerformanceIdentityKeys) { $receipt[$key] = 'fixture' }
    $receipt.roundCount = 5
    $receipt.framesPerRound = 40
    $receipt.hiddenPreparations = 0
    $receipt.hiddenComposites = 0
    foreach ($key in @('sourceCommit', 'sourceFingerprint', 'executableSha256', 'benchmarkSha256')) { $receipt[$key] = 'a' }
    $receipt.scenarios = @(foreach ($name in @('clean', 'dirty')) {
            [ordered]@{ name = $name; rounds = @(foreach ($round in 1..5) {
                        $metrics = [ordered]@{}
                        foreach ($metric in Get-PerformanceMetricNames) { $metrics[$metric] = 100 }
                        $metrics.composeAllocations = 0
                        $metrics
                    }) }
        })
    return $receipt
}

function Set-DirtyMetric([Collections.IDictionary] $Receipt, [string] $Metric, [object] $Value) {
    foreach ($row in $Receipt['scenarios'][1]['rounds']) { $row[$Metric] = $Value }
}

function Get-Status([Collections.IDictionary] $Before, [Collections.IDictionary] $After) {
    return (Compare-PerformanceReceipt -Before $Before -After $After)['status']
}

$before = New-Receipt

Invoke-TestCase 'identical and new source are comparable' {
    $after = Copy-JsonValue $before
    $after.sourceCommit = 'b'
    $after.sourceFingerprint = 'b'
    Assert-Equal 'within-noise-budget' (Get-Status $before $after) 'status'
}

Invoke-TestCase 'an FPS regression requires advice' {
    $after = Copy-JsonValue $before
    Set-DirtyMetric $after 'fps' 80
    Assert-Equal 'advice-required' (Get-Status $before $after) 'status'
}

Invoke-TestCase 'a memory regression requires advice' {
    $after = Copy-JsonValue $before
    Set-DirtyMetric $after 'privateBytes' 104
    Assert-Equal 'advice-required' (Get-Status $before $after) 'status'
}

Invoke-TestCase 'surface growth has no tolerance' {
    $after = Copy-JsonValue $before
    Set-DirtyMetric $after 'surfaceBytes' 101
    Assert-Equal 'advice-required' (Get-Status $before $after) 'status'
}

Invoke-TestCase 'an improvement passes' {
    $after = Copy-JsonValue $before
    Set-DirtyMetric $after 'fps' 120
    Assert-Equal 'within-noise-budget' (Get-Status $before $after) 'status'
}

Invoke-TestCase 'a mismatched fixture is rejected' {
    foreach ($key in @('machine', 'configuration', 'compiler', 'benchmarkSha256')) {
        $candidate = Copy-JsonValue $before
        $candidate[$key] = 'different'
        Assert-Throws { Compare-PerformanceReceipt -Before $before -After $candidate } "a different $key"
    }
    $candidate = Copy-JsonValue $before
    $candidate.machine = 'FIXTURE'
    Assert-Throws { Compare-PerformanceReceipt -Before $before -After $candidate } 'identity compares case-sensitively'
}

Invoke-TestCase 'missing rounds are rejected' {
    $after = Copy-JsonValue $before
    $rounds = $after['scenarios'][0]['rounds']
    $after['scenarios'][0]['rounds'] = @($rounds[0..3])
    Assert-Throws { Compare-PerformanceReceipt -Before $before -After $after } 'four rounds'
}

Invoke-TestCase 'a missing or non-finite metric is rejected' {
    $after = Copy-JsonValue $before
    foreach ($value in @($null, [double]::NaN, [double]::PositiveInfinity, -1, $true, '100')) {
        Set-DirtyMetric $after 'fps' $value
        Assert-Throws { Compare-PerformanceReceipt -Before $before -After $after } "fps <$value>"
    }
}

Invoke-TestCase 'an allocating composition is rejected' {
    $after = Copy-JsonValue $before
    Set-DirtyMetric $after 'composeAllocations' 1
    Assert-Throws { Compare-PerformanceReceipt -Before $before -After $after } 'composition allocations'
}

Invoke-TestCase 'hidden work is rejected' {
    $after = Copy-JsonValue $before
    $after.hiddenPreparations = 1
    Assert-Throws { Compare-PerformanceReceipt -Before $before -After $after } 'hidden preparations'
}

Invoke-FixtureCase 'the command writes the status and returns its exit code' {
    param($root)
    Set-FixtureJson $root 'a.json' $before
    $regressed = Copy-JsonValue $before
    Set-DirtyMetric $regressed 'fps' 80
    Set-FixtureJson $root 'b.json' $regressed
    $broken = Copy-JsonValue $before
    $broken.Remove('machine')
    Set-FixtureJson $root 'c.json' $broken
    $output = Join-Path $root 'out.json'
    $cases = @(
        @{ Candidate = 'a.json'; Baseline = ''; Status = 'unpaired'; Exit = 0 },
        @{ Candidate = 'a.json'; Baseline = 'a.json'; Status = 'within-noise-budget'; Exit = 0 },
        @{ Candidate = 'b.json'; Baseline = 'a.json'; Status = 'advice-required'; Exit = 1 },
        @{ Candidate = 'c.json'; Baseline = ''; Status = 'invalid-evidence'; Exit = 1 })
    foreach ($case in $cases) {
        $baseline = if ($case.Baseline) { Join-Path $root $case.Baseline } else { '' }
        & (Join-Path $PSScriptRoot '../Compare-Performance.ps1') -Candidate (Join-Path $root $case.Candidate) -Baseline $baseline -Output $output 6>$null
        Assert-Equal $case.Exit $LASTEXITCODE "exit code for $($case.Status)"
        Assert-Equal $case.Status (Get-FixtureJson $root 'out.json')['status'] 'written status'
    }
    Assert-Equal 'Missing identity/evidence: machine' (Get-FixtureJson $root 'out.json')['error'] 'invalid evidence names the key'
}

Invoke-TestCase 'every stored paired comparison is reproduced exactly' {
    # The comparisons under Measurements were written by the Python comparator this module replaces.
    $count = 0
    foreach ($file in Get-ChildItem -Recurse -File -Path (Join-Path $repository 'Measurements') -Filter '*-vs-*.comparison.json') {
        if ($file.Name -notmatch '^(?<scene>.+)-(?<new>[^-]+)-vs-(?<old>[^-.]+)(?:-control)?\.comparison\.json$') { throw "Unrecognized comparison name: $($file.FullName)" }
        $candidate = Read-PerformanceReceipt (Join-Path $file.DirectoryName "$($Matches.scene)-$($Matches.new).json")
        $baseline = Read-PerformanceReceipt (Join-Path $file.DirectoryName "$($Matches.scene)-$($Matches.old).json")
        $actual = ConvertTo-PerformanceComparisonJson (Compare-PerformanceReceipt -Before $baseline -After $candidate) -NewLine "`n"
        $expected = [IO.File]::ReadAllText($file.FullName).Replace("`r`n", "`n")
        Assert-True ([string]::Equals($actual, $expected, [StringComparison]::Ordinal)) "stored comparison differs: $($file.FullName)"
        $count++
    }
    Assert-True ($count -ge 52) "stored paired comparisons checked: $count"
}

Invoke-TestCase 'every stored unpaired receipt still validates' {
    $count = 0
    $long = 0
    foreach ($file in Get-ChildItem -Recurse -File -Path (Join-Path $repository 'Measurements') -Filter '*.json.comparison.json') {
        $stored = [IO.File]::ReadAllText($file.FullName)
        if (-not $stored.Contains('"status": "unpaired"')) { continue }
        $receipt = Read-PerformanceReceipt $file.FullName.Substring(0, $file.FullName.Length - '.comparison.json'.Length)
        if ($receipt['framesPerRound'] -ne 40) {
            # The 2026-09-13 long fixture (600-frame rounds) was compared with a temporarily patched comparator, as its
            # README records; the standard five-rounds-of-forty rule rejects it, as the Python comparator did.
            Assert-Throws { Assert-PerformanceReceipt $receipt } "a long-fixture receipt fails the standard rule: $($file.FullName)"
            $long++
            continue
        }
        Assert-PerformanceReceipt $receipt
        $actual = ConvertTo-PerformanceComparisonJson ([ordered]@{ status = 'unpaired'; changes = @() }) -NewLine "`n"
        Assert-True ([string]::Equals($actual, $stored.Replace("`r`n", "`n"), [StringComparison]::Ordinal)) "stored unpaired output differs: $($file.FullName)"
        $count++
    }
    Assert-True ($count -ge 233) "stored unpaired receipts checked: $count"
    Assert-Equal 16 $long 'long-fixture receipts'
}

# --- Paired sets: the exact rank test, and a verdict per metric from repeated interleaved runs -------------------------

function Assert-Near([double] $Expected, [double] $Actual, [double] $Tolerance, [string] $Message) {
    if ([Math]::Abs($Expected - $Actual) -gt $Tolerance) { throw "${Message}: expected <$Expected>, got <$Actual>" }
}

function New-Jitter([int] $Seed, [int] $Count, [double] $Spread) {
    # A fixed linear congruential sequence scaled to [-Spread, Spread], so the tests are deterministic.
    $state = [long]$Seed
    foreach ($index in 1..$Count) {
        $state = ($state * 1103515245 + 12345) % 2147483648
        (($state / 2147483648.0) * 2.0 - 1.0) * $Spread
    }
}

function New-Run([hashtable] $Values = @{}, [string] $Executable = 'a') {
    # One run's receipt: every metric 100 unless named as 'phase/metric'. All five rounds alike, so the run median is the value.
    $receipt = New-Receipt
    $receipt.executableSha256 = $Executable
    foreach ($key in $Values.Keys) {
        $phase, $metric = $key -split '/'
        foreach ($scenario in $receipt['scenarios']) {
            if ($scenario['name'] -eq $phase) { foreach ($row in $scenario['rounds']) { $row[$metric] = $Values[$key] } }
        }
    }
    return $receipt
}

function Get-SetMetric([Collections.IDictionary] $Set, [string] $Phase, [string] $Metric) {
    return @($Set['metrics'] | Where-Object { $_['phase'] -eq $Phase -and $_['metric'] -eq $Metric })[0]
}

function Test-OneMetricSet([string] $Key, [double[]] $Baseline, [double[]] $Candidate) {
    # A set whose only varying metric is $Key, judged by Compare-PerformanceSet.
    $set = Compare-PerformanceSet -Baseline @($Baseline | ForEach-Object { New-Run @{ $Key = $_ } }) -Candidate @($Candidate | ForEach-Object { New-Run @{ $Key = $_ } 'b' })
    $phase, $metric = $Key -split '/'
    return [ordered]@{ Set = $set; Metric = (Get-SetMetric $set $phase $metric) }
}

function Get-BruteForceP([double[]] $Baseline, [double[]] $Candidate) {
    # The two-sided exact p-value by enumerating every way to pick the candidate's runs; ranks by counting, not sorting.
    $all = @($Baseline) + @($Candidate)
    $count = $all.Count
    $picked = $Candidate.Count
    $doubled = foreach ($value in $all) { 2 * @($all | Where-Object { $_ -lt $value }).Count + @($all | Where-Object { $_ -eq $value }).Count + 1 }
    $observed = 0
    foreach ($index in $Baseline.Count..($count - 1)) { $observed += $doubled[$index] }
    $mean = $picked * ($count + 1)
    $extreme = 0
    $total = 0
    for ($mask = 0; $mask -lt (1 -shl $count); $mask++) {
        $sum = 0
        $size = 0
        for ($index = 0; $index -lt $count; $index++) { if ($mask -band (1 -shl $index)) { $size++; $sum += $doubled[$index] } }
        if ($size -ne $picked) { continue }
        $total++
        if ([Math]::Abs($sum - $mean) -ge [Math]::Abs($observed - $mean)) { $extreme++ }
    }
    return $extreme / $total
}

Invoke-TestCase 'the exact rank test gives the closed-form p-value of complete separation' {
    foreach ($size in 1..6) {
        $combinations = 1.0
        foreach ($index in 1..$size) { $combinations = $combinations * ($size + $index) / $index }
        $test = Get-MannWhitneyTest ([double[]](1..$size)) ([double[]](($size + 1)..(2 * $size)))
        Assert-Near (2.0 / $combinations) $test['pValue'] 1e-15 "p-value of $size against $size"
        Assert-Near ($size * $size) $test['u'] 0 "U of $size against $size"
        Assert-Equal 'higher' $test['direction'] "direction of $size against $size"
    }
}

Invoke-TestCase 'the exact rank test reproduces the documented reference example' {
    # Hollander and Wolfe's permeability data, as in R's wilcox.test help: W = 35 and a one-sided p of 0.1272 for the larger group.
    $larger = [double[]](0.80, 0.83, 1.89, 1.04, 1.45, 1.38, 1.91, 1.64, 0.73, 1.46)
    $smaller = [double[]](1.15, 0.88, 0.90, 0.74, 1.21)
    $test = Get-MannWhitneyTest $larger $smaller
    Assert-Near 15 $test['u'] 0 'U of the smaller group (35 for the larger)'
    Assert-Near 0.2544 $test['pValue'] 0.0001 'two-sided p-value'
    Assert-Equal 'lower' $test['direction'] 'the smaller group ranks lower'
    $mirrored = Get-MannWhitneyTest $smaller $larger
    Assert-Near $test['pValue'] $mirrored['pValue'] 1e-15 'swapping the sides keeps the p-value'
    Assert-Equal 'higher' $mirrored['direction'] 'and flips the direction'
}

Invoke-TestCase 'the exact rank test counts ties as an enumeration of every rank assignment does' {
    $cases = @(
        @{ A = @(1.1, 2.3, 3.7, 4.1); B = @(2.9, 5.0, 6.2, 7.7) }
        @{ A = @(1, 2, 2, 3, 5); B = @(2, 3, 3, 4, 6) }
        @{ A = @(5, 5, 5); B = @(5, 5, 6, 6, 7, 7, 7) }
        @{ A = @(10, 10, 10, 10); B = @(10, 10, 10, 10) }
        @{ A = @(1, 1, 2, 2, 3, 3); B = @(1, 2, 2, 3, 3, 3) }
        @{ A = @(3, 1); B = @(2, 4, 1, 5, 5, 0) })
    foreach ($case in $cases) {
        $expected = Get-BruteForceP ([double[]]$case.A) ([double[]]$case.B)
        $actual = (Get-MannWhitneyTest ([double[]]$case.A) ([double[]]$case.B))['pValue']
        Assert-Near $expected $actual 1e-12 "p-value of [$($case.A -join ' ')] against [$($case.B -join ' ')]"
    }
    Assert-Near 1 (Get-MannWhitneyTest ([double[]](7, 7, 7)) ([double[]](7, 7, 7)))['pValue'] 0 'equal values are never significant'
    $none = Get-MannWhitneyTest ([double[]](1, 2, 3)) ([double[]](1, 2, 3))
    Assert-Equal 'none' $none['direction'] 'identical samples have no direction'
}

Invoke-TestCase 'six runs against six reach p < 0.05 for exactly the nominal share of rank patterns' {
    # Every one of the 924 equally likely ways to interleave twelve distinct values; the exact test is a level-4.1% test.
    $significant = 0
    $patterns = 0
    for ($mask = 0; $mask -lt 4096; $mask++) {
        $size = 0
        foreach ($index in 0..11) { if ($mask -band (1 -shl $index)) { $size++ } }
        if ($size -ne 6) { continue }
        $baseline = [Collections.Generic.List[double]]::new()
        $candidate = [Collections.Generic.List[double]]::new()
        foreach ($index in 0..11) { if ($mask -band (1 -shl $index)) { $candidate.Add($index + 1) } else { $baseline.Add($index + 1) } }
        $patterns++
        if ((Get-MannWhitneyTest $baseline.ToArray() $candidate.ToArray())['pValue'] -lt 0.05) { $significant++ }
    }
    Assert-Equal 924 $patterns 'rank patterns'
    Assert-Equal 38 $significant 'patterns with p < 0.05 (2 x 19 of 924, as the tables give)'
}

Invoke-TestCase 'the p-value a set can reach depends on its runs per side' {
    Assert-Near (2.0 / 924) (Get-MinimumAttainableP 6 6) 1e-15 'three repetitions give six runs per side'
    Assert-Near (2.0 / 70) (Get-MinimumAttainableP 4 4) 1e-15 'two repetitions'
    Assert-Near (2.0 / 6) (Get-MinimumAttainableP 2 2) 1e-15 'one repetition'
    Assert-True ((Get-MinimumAttainableP 4 4) -lt 0.05) 'four runs per side can reach 0.05'
    Assert-True ((Get-MinimumAttainableP 2 2) -ge 0.05) 'two runs per side cannot'
}

Invoke-TestCase 'identical distributions are within noise' {
    # Twelve jittered runs of one distribution, dealt alternately to the two sides so their ranks interleave.
    $values = @(New-Jitter 11 12 0.03 | Sort-Object | ForEach-Object { 100 * (1 + $_) })
    $baseline = @(0, 2, 4, 6, 8, 10 | ForEach-Object { $values[$_] })
    $candidate = @(1, 3, 5, 7, 9, 11 | ForEach-Object { $values[$_] })
    $result = Test-OneMetricSet 'dirty/fps' $baseline $candidate
    Assert-Equal 'within-noise' $result.Metric['verdict'] 'verdict'
    Assert-True ($result.Metric['pValue'] -gt 0.5) "interleaved ranks are far from significant: $($result.Metric['pValue'])"
    Assert-Equal 'within-noise-budget' $result.Set['status'] 'set status'
    Assert-Equal 0 $result.Set['regressedMetrics'] 'no regressed metric'
    Assert-Equal 26 @($result.Set['metrics']).Count 'every phase and metric is judged'
    # The runs of a side may also drift the same way on both: the same values on both sides are simply the same distribution.
    $same = Test-OneMetricSet 'dirty/fps' $baseline $baseline
    Assert-Equal 'within-noise' $same.Metric['verdict'] 'the same runs'
    Assert-Near 1 $same.Metric['pValue'] 1e-12 'p-value of the same runs'
}

Invoke-TestCase 'a 20% FPS drop with 3% jitter is regressed with p < 0.05' {
    $baseline = @(New-Jitter 3 6 0.03 | ForEach-Object { 100 * (1 + $_) })
    $candidate = @(New-Jitter 5 6 0.03 | ForEach-Object { 80 * (1 + $_) })
    $result = Test-OneMetricSet 'dirty/fps' $baseline $candidate
    $metric = $result.Metric
    Assert-Equal 'regressed' $metric['verdict'] 'verdict'
    Assert-Near (2.0 / 924) $metric['pValue'] 1e-15 'the sides separate completely'
    Assert-True ($metric['changePercent'] -lt -15 -and $metric['changePercent'] -gt -25) "the median shift is about -20%: $($metric['changePercent'])"
    Assert-True ($metric['baselineSpreadPercent'] -gt 1 -and $metric['baselineSpreadPercent'] -lt 8) "the same-binary spread is reported: $($metric['baselineSpreadPercent'])"
    Assert-Equal 'advice-required' $result.Set['status'] 'set status'
    Assert-Equal 1 $result.Set['regressedMetrics'] 'one regressed metric'
    Assert-Equal 'within-noise' (Get-SetMetric $result.Set 'clean' 'fps')['verdict'] 'the clean phase did not move'
}

Invoke-TestCase 'a significant shift inside the band is within noise' {
    $baseline = @(New-Jitter 7 6 0.003 | ForEach-Object { 100 * (1 + $_) })
    $slower = @(New-Jitter 9 6 0.003 | ForEach-Object { 103 * (1 + $_) })
    $timing = (Test-OneMetricSet 'dirty/frameP95Ms' $baseline $slower).Metric
    Assert-True ($timing['pValue'] -lt 0.05) 'the shift is statistically significant'
    Assert-Equal 'within-noise' $timing['verdict'] 'but 3% is inside the 5% timing band'
    $memory = (Test-OneMetricSet 'dirty/privateBytes' $baseline @($slower | ForEach-Object { $_ / 103 * 101.5 })).Metric
    Assert-True ($memory['pValue'] -lt 0.05) 'a 1.5% memory rise is significant'
    Assert-Equal 'within-noise' $memory['verdict'] 'and inside the 2% memory band'
    $beyond = (Test-OneMetricSet 'dirty/privateBytes' $baseline @($slower | ForEach-Object { $_ / 103 * 102.5 })).Metric
    Assert-Equal 'regressed' $beyond['verdict'] 'a 2.5% memory rise is beyond its band'
}

Invoke-TestCase 'a shift beyond the band that the runs cannot separate from noise is within noise' {
    $result = Test-OneMetricSet 'dirty/frameP95Ms' @(90, 95, 100, 105, 110, 115) @(96, 101, 106, 111, 116, 121)
    Assert-True ($result.Metric['changePercent'] -gt 5) "the medians are more than 5% apart: $($result.Metric['changePercent'])"
    Assert-True ($result.Metric['pValue'] -gt 0.05) "but the runs overlap: p $($result.Metric['pValue'])"
    Assert-Equal 'within-noise' $result.Metric['verdict'] 'verdict'
}

Invoke-TestCase 'an increase in an exact budget in any single candidate run is regressed' {
    $flat = @(2160, 2160, 2160, 2160, 2160, 2160)
    foreach ($case in @(@('dirty/cppAllocations', 2160), @('dirty/surfaceBytes', 3686400), @('clean/replacementPeakBytes', 7372800))) {
        $key, $value = $case
        $baseline = @(1..6 | ForEach-Object { $value })
        $oneHigher = @(1..5 | ForEach-Object { $value }) + @($value + 1)
        $result = Test-OneMetricSet $key $baseline $oneHigher
        Assert-Equal 'regressed' $result.Metric['verdict'] "one run of $key one higher"
        Assert-True ($result.Metric['pValue'] -ge 0.05) 'not a statistical matter: exact budgets stay exact'
        Assert-Equal 'advice-required' $result.Set['status'] "set status for $key"
        $oneLower = @(1..5 | ForEach-Object { $value }) + @($value - 1)
        Assert-Equal 'within-noise' (Test-OneMetricSet $key $baseline $oneLower).Metric['verdict'] "one run of $key lower"
        $allLower = @(1..6 | ForEach-Object { $value - 1 })
        $lower = Test-OneMetricSet $key $baseline $allLower
        Assert-Equal 'improved' $lower.Metric['verdict'] "every run of $key lower"
        Assert-Equal 'within-noise-budget' $lower.Set['status'] 'an improvement needs no advice'
    }
    # Runs of the baseline are not required to be identical: the reference is their median.
    $noisy = Test-OneMetricSet 'dirty/cppAllocations' @(2160, 2160, 2160, 2160, 2160, 2170) @(2160, 2160, 2160, 2160, 2160, 2160)
    Assert-Equal 'within-noise' $noisy.Metric['verdict'] 'one high baseline run does not move the reference'
}

Invoke-TestCase 'a clear improvement is reported and needs no advice' {
    $baseline = @(New-Jitter 3 6 0.03 | ForEach-Object { 100 * (1 + $_) })
    $candidate = @(New-Jitter 5 6 0.03 | ForEach-Object { 120 * (1 + $_) })
    $result = Test-OneMetricSet 'dirty/fps' $baseline $candidate
    Assert-Equal 'improved' $result.Metric['verdict'] 'verdict'
    Assert-Equal 'within-noise-budget' $result.Set['status'] 'set status'
    Assert-Equal 1 $result.Set['improvedMetrics'] 'one improved metric'
    # Lower is better for a time: the same numbers on a time metric are a regression.
    Assert-Equal 'regressed' (Test-OneMetricSet 'dirty/frameP95Ms' $baseline $candidate).Metric['verdict'] 'a rise in a time'
}

Invoke-TestCase 'every metric keeps its own band and direction' {
    # Worse by a bit more than its band is regressed, by a bit less is not, and better by more than the band is improved.
    $bands = [ordered]@{ fps = 0.05; frameP50Ms = 0.05; frameP95Ms = 0.05; prepareP95Ms = 0.05; composeCpuP95Ms = 0.05
        privateBytes = 0.02; privatePeakBytes = 0.02; workingSetBytes = 0.02; workingSetPeakBytes = 0.02 }
    $baseline = @(New-Jitter 21 6 0.001 | ForEach-Object { 100 * (1 + $_) })
    foreach ($metric in $bands.Keys) {
        $band = $bands[$metric]
        $sign = if ($metric -eq 'fps') { -1 } else { 1 }
        $worse = { param($shift) @(New-Jitter 23 6 0.001 | ForEach-Object { 100 * (1 + $sign * $shift) * (1 + $_) }) }
        Assert-Equal 'regressed' (Test-OneMetricSet "dirty/$metric" $baseline (& $worse ($band + 0.01))).Metric['verdict'] "$metric worse than its band"
        Assert-Equal 'within-noise' (Test-OneMetricSet "dirty/$metric" $baseline (& $worse ($band - 0.01))).Metric['verdict'] "$metric worse inside its band"
        Assert-Equal 'improved' (Test-OneMetricSet "dirty/$metric" $baseline (& $worse (-($band + 0.01)))).Metric['verdict'] "$metric better than its band"
    }
    $exact = @(Get-PerformanceMetricNames | Where-Object { $_ -notin $bands.Keys -and $_ -ne 'composeAllocations' })
    Assert-Equal 3 $exact.Count "the exact budgets: $($exact -join ', ')"
}

Invoke-TestCase 'a run is judged by the median of its rounds' {
    $runs = 1..6 | ForEach-Object { New-Run @{} }
    foreach ($run in $runs) { $run['scenarios'][1]['rounds'][4]['fps'] = 900; $run['scenarios'][1]['rounds'][0]['fps'] = 1 }
    $candidate = 1..6 | ForEach-Object { New-Run @{} 'b' }
    foreach ($run in $candidate) { $run['scenarios'][1]['rounds'][4]['fps'] = 5; $run['scenarios'][1]['rounds'][0]['fps'] = 1000 }
    $set = Compare-PerformanceSet -Baseline @($runs) -Candidate @($candidate)
    $fps = Get-SetMetric $set 'dirty' 'fps'
    Assert-Near 100 $fps['baselineMedian'] 0 'outlier rounds do not move a baseline run'
    Assert-Equal 'within-noise' $fps['verdict'] 'nor a candidate run'
}

Invoke-TestCase 'a same-binary spread is reported and never a veto' {
    # Each side swings 30% between its own runs, yet every candidate run is far below every baseline run.
    $baseline = 130, 100, 115, 105, 125, 110
    $candidate = 60, 50, 55, 52, 58, 62
    $result = Test-OneMetricSet 'dirty/fps' $baseline $candidate
    Assert-True ($result.Metric['baselineSpreadPercent'] -gt 25) "the spread is reported: $($result.Metric['baselineSpreadPercent'])"
    Assert-Equal 'regressed' $result.Metric['verdict'] 'a wide spread does not veto a complete separation'
    $lines = Format-PerformanceSetVerdict $result.Set
    Assert-True (@($lines | Where-Object { $_.Contains('regressed dirty/fps') -and $_.Contains('spread') }).Count -eq 1) 'the report line carries the spread'
}

Invoke-TestCase 'two runs per side establish no timing shift but still catch a budget rise' {
    $timing = Test-OneMetricSet 'dirty/fps' @(100, 101) @(50, 51)
    Assert-Near (2.0 / 6) $timing.Metric['pValue'] 1e-15 'complete separation of two against two'
    Assert-Equal 'within-noise' $timing.Metric['verdict'] 'half the FPS is not established by two runs'
    Assert-Near (2.0 / 6) $timing.Set['minimumAttainableP'] 1e-15 'the set records what it could establish'
    Assert-True (@(Format-PerformanceSetVerdict $timing.Set | Where-Object { $_.Contains('Too few runs') }).Count -eq 1) 'and says so'
    Assert-Equal 'regressed' (Test-OneMetricSet 'dirty/cppAllocations' @(2160, 2160) @(2160, 2161)).Metric['verdict'] 'an allocation rise'
}

Invoke-TestCase 'a set of one binary per side on one fixture is required' {
    $good = @(1..4 | ForEach-Object { New-Run @{} 'a' })
    $other = @(1..4 | ForEach-Object { New-Run @{} 'b' })
    $mixed = @(1..3 | ForEach-Object { New-Run @{} 'a' }) + @(New-Run @{} 'c')
    Assert-Throws { Compare-PerformanceSet -Baseline $mixed -Candidate $other } 'a baseline of two binaries'
    Assert-Throws { Compare-PerformanceSet -Baseline $good -Candidate $mixed } 'a candidate of two binaries'
    $sources = @(1..3 | ForEach-Object { New-Run @{} 'a' }) + @(New-Run @{} 'a')
    $sources[3]['sourceFingerprint'] = 'changed during the run'
    Assert-Throws { Compare-PerformanceSet -Baseline $sources -Candidate $other } 'a baseline whose sources changed between runs'
    $elsewhere = @(1..4 | ForEach-Object { $run = New-Run @{} 'b'; $run['machine'] = 'another'; $run })
    Assert-Throws { Compare-PerformanceSet -Baseline $good -Candidate $elsewhere } 'a candidate from another machine'
    $invalid = @(1..4 | ForEach-Object { New-Run @{ 'dirty/fps' = -1 } 'b' })
    Assert-Throws { Compare-PerformanceSet -Baseline $good -Candidate $invalid } 'an invalid receipt'
    Assert-Equal 'within-noise-budget' (Compare-PerformanceSet -Baseline $good -Candidate $other)['status'] 'identical receipts under different binaries'
}

Invoke-TestCase 'the set verdict prints its status, the smallest attainable p and each finding' {
    $baseline = @(New-Jitter 3 6 0.03 | ForEach-Object { 100 * (1 + $_) })
    $candidate = @(New-Jitter 5 6 0.03 | ForEach-Object { 80 * (1 + $_) })
    $result = Test-OneMetricSet 'dirty/fps' $baseline $candidate
    $lines = @(Format-PerformanceSetVerdict $result.Set -Label 'Default set')
    Assert-True $lines[0].StartsWith('Default set: advice-required; 6 runs per side, smallest attainable p 0.0022, 1 regressed, 0 improved') "header: $($lines[0])"
    Assert-Equal 2 $lines.Count 'a header and the one regressed metric'
    Assert-True ($lines[1].Contains('regressed dirty/fps') -and $lines[1].Contains('p 0.0022') -and $lines[1].Contains('band 5%')) "finding: $($lines[1])"
    $json = $result.Set | ConvertTo-Json -Depth 8
    Assert-True ($json.Contains('"verdict": "regressed"')) 'the verdict serializes into summary.json'
}

Complete-TestRun 'PerformanceComparison'
