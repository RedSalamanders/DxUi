# Compare matched complex-UI receipts; regressions require advice, never automatic rebaselining.
Set-StrictMode -Version Latest

$script:Identity = @('fixture', 'renderer', 'width', 'height', 'dpi', 'controls', 'modelRows', 'framesPerRound', 'roundCount',
    'platform', 'configuration', 'nativeArchitecture', 'machine', 'cpu', 'os', 'compiler', 'warpVersion', 'powerPolicy')
$script:Evidence = @('sourceCommit', 'sourceFingerprint', 'executableSha256', 'benchmarkSha256')
# Direction and investigation band per metric. A band of integer 0 stays an integer in the written comparison.
$script:Metrics = [ordered]@{
    fps = @('higher', 0.05); frameP50Ms = @('lower', 0.05); frameP95Ms = @('lower', 0.05)
    prepareP95Ms = @('lower', 0.05); composeCpuP95Ms = @('lower', 0.05)
    privateBytes = @('lower', 0.02); privatePeakBytes = @('lower', 0.02)
    workingSetBytes = @('lower', 0.02); workingSetPeakBytes = @('lower', 0.02)
    surfaceBytes = @('lower', 0); replacementPeakBytes = @('lower', 0)
    cppAllocations = @('lower', 0); composeAllocations = @('lower', 0)
}
$script:Invariant = [Globalization.CultureInfo]::InvariantCulture
# The compiled inputs performance.ps1 hashes into a receipt's benchmarkSha256, in that hash's order. A paired run copies
# them, with the driver and this comparator, onto both trees.
$script:BenchmarkInputs = @('Tests/Embedded/BenchmarkMain.h', 'Tests/Embedded/ComplexUiBenchmark.h', 'Tests/Support/HeapDiagnostic.h',
    'Samples/ComplexUi/ComplexUiScene.h', 'Samples/EmbeddedControls/GraphicsFixture.h')
# The library inputs a receipt's sourceFingerprint covers.
$script:FingerprintPaths = @('src', 'include', 'Build', 'Directory.Build.props', 'Directory.Build.targets', 'vcpkg.json', 'vcpkg-tool.json')

function Get-PerformanceMetricNames { return @($script:Metrics.Keys) }

function Get-PerformanceIdentityKeys { return @($script:Identity) }

function Get-BenchmarkInputPaths { return @($script:BenchmarkInputs) }

function Get-SourceFingerprint {
    <# The receipt's sourceFingerprint of one tree: SHA-256 over the "path hash" lines of its tracked and unignored library
       inputs, one file hash per line. The lines are ordered by Sort-Object, as receipts have always been, so a fingerprint
       stays comparable with every stored one. #>
    param([Parameter(Mandatory)][string] $Root)
    $sources = @(& git -C $Root ls-files --cached --others --exclude-standard -- @($script:FingerprintPaths))
    if ($LASTEXITCODE -ne 0) { throw "Cannot enumerate library inputs under $Root" }
    $hashes = foreach ($source in ($sources | Sort-Object -Unique)) { "$source $((Get-FileHash -LiteralPath (Join-Path $Root $source) -Algorithm SHA256).Hash)" }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($hashes -join "`n")))) }
    finally { $sha.Dispose() }
}

function Test-JsonNumber([object] $Value) {
    return $Value -is [int] -or $Value -is [long] -or $Value -is [double] -or $Value -is [decimal] -or $Value -is [bigint]
}

function Test-SameJsonValue([object] $Left, [object] $Right) {
    # JSON equality without PowerShell's coercion: 5 and '5' differ, and strings compare case-sensitively.
    if ($null -eq $Left -or $null -eq $Right) { return $null -eq $Left -and $null -eq $Right }
    if ((Test-JsonNumber $Left) -and (Test-JsonNumber $Right)) { return [double]$Left -eq [double]$Right }
    if ($Left -is [string] -and $Right -is [string]) { return [string]::Equals($Left, $Right, [StringComparison]::Ordinal) }
    if ($Left -is [bool] -and $Right -is [bool]) { return $Left -eq $Right }
    return $false
}

function Get-ReceiptValue([System.Collections.IDictionary] $Receipt, [string] $Key) {
    if ($Receipt.Contains($Key)) { return $Receipt[$Key] }
    return $null
}

function Read-PerformanceReceipt([Parameter(Mandatory)][string] $Path) {
    # UTF-8 with or without a byte-order mark, read as dictionaries so an absent key reads as null.
    $text = [IO.File]::ReadAllText([IO.Path]::GetFullPath($Path), [Text.UTF8Encoding]::new($false, $true))
    $receipt = $text | ConvertFrom-Json -AsHashtable
    if ($receipt -isnot [System.Collections.IDictionary]) { throw "A receipt must be a JSON object: $Path" }
    return $receipt
}

function Get-MedianValue([object[]] $Values) {
    # The middle element itself for an odd count (an integer stays an integer), the mean of the middle two otherwise.
    $sorted = @($Values | Sort-Object { [double]$_ })
    if ($sorted.Count -eq 0) { throw 'No rounds to take a median of' }
    $middle = [int][Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2) { return $sorted[$middle] }
    return ([double]$sorted[$middle - 1] + [double]$sorted[$middle]) / 2.0
}

function Assert-PerformanceReceipt([Parameter(Mandatory)][System.Collections.IDictionary] $Receipt) {
    foreach ($key in $script:Identity + $script:Evidence) {
        $value = Get-ReceiptValue $Receipt $key
        if ($null -eq $value -or ($value -is [string] -and $value.Length -eq 0)) { throw "Missing identity/evidence: $key" }
    }
    if (-not (Test-SameJsonValue $Receipt['roundCount'] 5) -or -not (Test-SameJsonValue $Receipt['framesPerRound'] 40)) {
        throw 'Expected five rounds of forty frames'
    }
    $scenarios = @(Get-ReceiptValue $Receipt 'scenarios' | Where-Object { $null -ne $_ })
    $names = @($scenarios | ForEach-Object { if ($_ -is [System.Collections.IDictionary]) { Get-ReceiptValue $_ 'name' } })
    $expected = @('clean', 'dirty')
    $distinct = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($name in $names) { if ($name -is [string]) { [void]$distinct.Add($name) } }
    if ($scenarios.Count -ne 2 -or $names.Count -ne 2 -or $distinct.Count -ne 2 -or -not $distinct.SetEquals([string[]]$expected)) {
        throw 'Exactly clean and dirty scenarios are required'
    }
    foreach ($scenario in $scenarios) {
        $rounds = @(Get-ReceiptValue $scenario 'rounds' | Where-Object { $null -ne $_ })
        if (-not (Test-SameJsonValue $rounds.Count $Receipt['roundCount'])) { throw 'Incomplete measurement rounds' }
        foreach ($record in $rounds) {
            if ($record -isnot [System.Collections.IDictionary]) { throw 'Incomplete measurement rounds' }
            foreach ($metric in $script:Metrics.Keys) {
                $value = Get-ReceiptValue $record $metric
                if (-not (Test-JsonNumber $value) -or [double]::IsNaN([double]$value) -or [double]::IsInfinity([double]$value) -or [double]$value -lt 0) {
                    throw "Invalid $($scenario['name'])/$metric"
                }
                if ($metric -in @('fps', 'frameP50Ms', 'frameP95Ms') -and [double]$value -le 0) { throw "Invalid zero $metric" }
            }
            if (-not (Test-SameJsonValue $record['composeAllocations'] 0)) { throw 'Composition allocated C++ heap memory' }
        }
    }
    if (-not (Test-SameJsonValue (Get-ReceiptValue $Receipt 'hiddenPreparations') 0) -or
        -not (Test-SameJsonValue (Get-ReceiptValue $Receipt 'hiddenComposites') 0)) {
        throw 'Hidden work must be zero'
    }
}

function Assert-MatchedFixture([System.Collections.IDictionary] $Before, [System.Collections.IDictionary] $After) {
    # Two receipts compare only when one machine, configuration and benchmark produced them.
    foreach ($key in $script:Identity + @('benchmarkSha256')) {
        if (-not (Test-SameJsonValue $Before[$key] $After[$key])) { throw "Unmatched fixture: $key" }
    }
}

function Compare-PerformanceReceipt {
    param([Parameter(Mandatory)][System.Collections.IDictionary] $Before, [Parameter(Mandatory)][System.Collections.IDictionary] $After)
    Assert-PerformanceReceipt $Before
    Assert-PerformanceReceipt $After
    Assert-MatchedFixture $Before $After
    $previous = @{}
    foreach ($scenario in $Before['scenarios']) { $previous[$scenario['name']] = $scenario }
    $changes = [Collections.Generic.List[object]]::new()
    foreach ($scenario in $After['scenarios']) {
        foreach ($metric in $script:Metrics.Keys) {
            $direction, $noise = $script:Metrics[$metric]
            $old = Get-MedianValue @($previous[$scenario['name']]['rounds'] | ForEach-Object { $_[$metric] })
            $new = Get-MedianValue @($scenario['rounds'] | ForEach-Object { $_[$metric] })
            $regressed = if ($direction -eq 'higher') { [double]$new -lt [double]$old * (1.0 - $noise) } else { [double]$new -gt [double]$old * (1.0 + $noise) }
            $changes.Add([ordered]@{
                    scenario = $scenario['name']; metric = $metric; before = $old; after = $new
                    changePercent = $(if ([double]$old -ne 0) { ([double]$new / [double]$old - 1.0) * 100.0 } else { $null })
                    noisePercent = $(if ($noise -is [int]) { [int]$noise * 100 } else { [double]$noise * 100.0 })
                    regressed = [bool]$regressed
                })
        }
    }
    $status = if (@($changes | Where-Object { $_['regressed'] }).Count) { 'advice-required' } else { 'within-noise-budget' }
    return [ordered]@{ status = $status; changes = $changes.ToArray() }
}

# --- Paired sets --------------------------------------------------------------------------------------------------------
# One A1/B1/B2/A2 pass gives each side two runs, too few to tell a change from a noisy machine. A set repeats the
# interleaved pass, then judges every phase and metric on all the runs at once: an exact two-sided Mann-Whitney U test of
# the baseline run medians against the candidate run medians, together with the metric's investigation band.

$script:Significance = 0.05
# Rank distributions already counted, by candidate size and the ranks in play; every metric without ties shares one.
$script:RankDistributions = @{}

function Get-DoubledMidranks([double[]] $Values) {
    # Twice each value's midrank (tied values share the mean of their ranks): an integer, so sums stay exact.
    $count = $Values.Length
    $keys = [double[]]$Values.Clone()
    $positions = [int[]](0..($count - 1))
    [Array]::Sort($keys, $positions)
    $ranks = [int[]]::new($count)
    $start = 0
    while ($start -lt $count) {
        $end = $start
        while ($end + 1 -lt $count -and $keys[$end + 1] -eq $keys[$start]) { $end++ }
        # Ranks start+1 through end+1 have the mean (start + end + 2) / 2.
        for ($index = $start; $index -le $end; $index++) { $ranks[$positions[$index]] = $start + $end + 2 }
        $start = $end + 1
    }
    return $ranks
}

function Get-SubsetSumCounts([int[]] $Values, [int] $Size) {
    # ways[s]: how many Size-element subsets of Values sum to s. Counted, not enumerated: a 0/1 knapsack over the values,
    # taking each value once and updating larger subset sizes first so it is not counted twice.
    $total = 0
    foreach ($value in $Values) { $total += $value }
    $ways = [long[][]]::new($Size + 1)
    for ($chosen = 0; $chosen -le $Size; $chosen++) { $ways[$chosen] = [long[]]::new($total + 1) }
    $ways[0][0] = 1
    $seen = 0
    foreach ($value in $Values) {
        $seen++
        for ($chosen = [Math]::Min($seen, $Size); $chosen -ge 1; $chosen--) {
            $from = $ways[$chosen - 1]
            $to = $ways[$chosen]
            for ($sum = $total; $sum -ge $value; $sum--) { $to[$sum] += $from[$sum - $value] }
        }
    }
    return , $ways[$Size]
}

function Get-MannWhitneyTest {
    <# Exact two-sided Mann-Whitney U test of Candidate against Baseline. The null distribution is the rank sum of every way
       to pick the candidate's runs out of all the runs, counted over the observed (tie-averaged) ranks, so ties and small
       samples need no approximation. The p-value is the share of those choices whose rank sum lies at least as far from its
       mean as the observed one. u counts the baseline/candidate pairs in which the candidate is higher (a tie counts half);
       direction says which side's ranks are higher overall. #>
    param([Parameter(Mandatory)][double[]] $Baseline, [Parameter(Mandatory)][double[]] $Candidate)
    if ($Baseline.Length -lt 1 -or $Candidate.Length -lt 1) { throw 'A rank test needs at least one run on each side' }
    foreach ($value in @($Baseline) + @($Candidate)) {
        if ([double]::IsNaN($value) -or [double]::IsInfinity($value)) { throw 'A rank test needs finite values' }
    }
    $baselineRuns = $Baseline.Length
    $candidateRuns = $Candidate.Length
    $count = $baselineRuns + $candidateRuns
    $ranks = @(Get-DoubledMidranks ([double[]]($Baseline + $Candidate)))
    $observed = 0
    for ($index = $baselineRuns; $index -lt $count; $index++) { $observed += $ranks[$index] }
    $sorted = [int[]]@($ranks | Sort-Object)
    $key = "$candidateRuns|$($sorted -join ',')"
    if (-not $script:RankDistributions.ContainsKey($key)) { $script:RankDistributions[$key] = Get-SubsetSumCounts $sorted $candidateRuns }
    $ways = $script:RankDistributions[$key]
    # Twice the mean rank sum is candidateRuns * (count + 1), whatever the ties.
    $mean = $candidateRuns * ($count + 1)
    $distance = [Math]::Abs($observed - $mean)
    $all = [long]0
    $extreme = [long]0
    for ($sum = 0; $sum -lt $ways.Length; $sum++) {
        $all += $ways[$sum]
        if ([Math]::Abs($sum - $mean) -ge $distance) { $extreme += $ways[$sum] }
    }
    return [ordered]@{
        baselineRuns = $baselineRuns; candidateRuns = $candidateRuns
        u = ($observed - $candidateRuns * ($candidateRuns + 1)) / 2.0
        pValue = [double]$extreme / [double]$all
        direction = $(if ($observed -gt $mean) { 'higher' } elseif ($observed -lt $mean) { 'lower' } else { 'none' })
    }
}

function Get-MinimumAttainableP {
    <# The smallest two-sided p-value a set of these sizes can reach: every candidate run beyond every baseline run, with no
       ties. Below 0.05 a set can establish a shift; above it, it cannot, however large the shift. #>
    param([Parameter(Mandatory)][int] $BaselineRuns, [Parameter(Mandatory)][int] $CandidateRuns)
    $baseline = [double[]](1..$BaselineRuns)
    $candidate = [double[]](($BaselineRuns + 1)..($BaselineRuns + $CandidateRuns))
    return (Get-MannWhitneyTest $baseline $candidate)['pValue']
}

function Get-MetricVerdict {
    <# One metric's verdict from the run medians of both sides. A shift is regressed (improved) only when the rank test is
       significant (p < 0.05) and the candidate's median is beyond the metric's investigation band, on the worse (better)
       side of the baseline's; anything else is within-noise. An exact budget (band 0) never tolerates growth: any candidate
       run above the baseline's median is regressed. The spread of each side's own runs is reported for context and never
       vetoes a verdict. #>
    param([Parameter(Mandatory)][ValidateSet('higher', 'lower')][string] $Direction, [Parameter(Mandatory)][double] $Band,
        [Parameter(Mandatory)][double[]] $Baseline, [Parameter(Mandatory)][double[]] $Candidate)
    $before = [double](Get-MedianValue $Baseline)
    $after = [double](Get-MedianValue $Candidate)
    $test = Get-MannWhitneyTest $Baseline $Candidate
    $worse = if ($Direction -eq 'higher') { $after -lt $before * (1.0 - $Band) } else { $after -gt $before * (1.0 + $Band) }
    $better = if ($Direction -eq 'higher') { $after -gt $before * (1.0 + $Band) } else { $after -lt $before * (1.0 - $Band) }
    $exact = $Band -eq 0
    $anyWorse = @($Candidate | Where-Object { if ($Direction -eq 'higher') { $_ -lt $before } else { $_ -gt $before } }).Count -gt 0
    $significant = $test['pValue'] -lt $script:Significance
    $verdict = if (($exact -and $anyWorse) -or ($significant -and $worse)) { 'regressed' } elseif ($significant -and $better) { 'improved' } else { 'within-noise' }
    $spread = { param([double[]] $Runs, [double] $Median)
        if ($Median -eq 0) { return $null }
        $range = $Runs | Measure-Object -Minimum -Maximum
        return ($range.Maximum - $range.Minimum) / $Median * 100.0 }
    return [ordered]@{
        exact = $exact; baselineMedian = $before; candidateMedian = $after
        changePercent = $(if ($before -ne 0) { ($after / $before - 1.0) * 100.0 } else { $null })
        noisePercent = $(if ($exact) { 0 } else { $Band * 100.0 })
        pValue = $test['pValue']; u = $test['u']; rankDirection = $test['direction']
        baselineSpreadPercent = (& $spread $Baseline $before); candidateSpreadPercent = (& $spread $Candidate $after)
        verdict = $verdict
    }
}

function Get-RunMedian([System.Collections.IDictionary] $Receipt, [string] $Phase, [string] $Metric) {
    foreach ($scenario in $Receipt['scenarios']) {
        if ($scenario['name'] -ceq $Phase) { return [double](Get-MedianValue @($scenario['rounds'] | ForEach-Object { $_[$Metric] })) }
    }
    throw "A receipt has no $Phase scenario"
}

function Compare-PerformanceSet {
    <# Judges a paired set: the receipts of every baseline run against those of every candidate run, for each phase
       (clean, dirty) and metric, by Get-MetricVerdict on the median of each run's rounds. The set is advice-required when
       any metric regressed, otherwise within-noise-budget; the latter says no change was established, not that none
       exists. Runs of one side must be of one binary, and both sides of one fixture, or the evidence is invalid. #>
    param([Parameter(Mandatory)][System.Collections.IDictionary[]] $Baseline, [Parameter(Mandatory)][System.Collections.IDictionary[]] $Candidate)
    foreach ($receipt in @($Baseline) + @($Candidate)) {
        Assert-PerformanceReceipt $receipt
        Assert-MatchedFixture $Baseline[0] $receipt
    }
    foreach ($side in @(@('baseline', $Baseline), @('candidate', $Candidate))) {
        foreach ($key in @('executableSha256', 'sourceFingerprint')) {
            foreach ($receipt in $side[1]) {
                if (-not (Test-SameJsonValue $side[1][0][$key] $receipt[$key])) { throw "The $($side[0]) runs differ in ${key}: a set compares runs of one unchanged binary" }
            }
        }
    }
    $metrics = [Collections.Generic.List[object]]::new()
    foreach ($phase in @($Baseline[0]['scenarios'] | ForEach-Object { $_['name'] })) {
        foreach ($metric in $script:Metrics.Keys) {
            $direction, $band = $script:Metrics[$metric]
            $baselineRuns = [double[]]@($Baseline | ForEach-Object { Get-RunMedian $_ $phase $metric })
            $candidateRuns = [double[]]@($Candidate | ForEach-Object { Get-RunMedian $_ $phase $metric })
            $record = [ordered]@{ phase = $phase; metric = $metric; direction = $direction }
            $verdict = Get-MetricVerdict -Direction $direction -Band ([double]$band) -Baseline $baselineRuns -Candidate $candidateRuns
            foreach ($key in $verdict.Keys) { $record[$key] = $verdict[$key] }
            $record['baselineRuns'] = $baselineRuns
            $record['candidateRuns'] = $candidateRuns
            $metrics.Add($record)
        }
    }
    $regressed = @($metrics | Where-Object { $_['verdict'] -ceq 'regressed' }).Count
    return [ordered]@{
        status = $(if ($regressed) { 'advice-required' } else { 'within-noise-budget' })
        significance = $script:Significance
        baselineRuns = $Baseline.Count; candidateRuns = $Candidate.Count
        minimumAttainableP = (Get-MinimumAttainableP $Baseline.Count $Candidate.Count)
        regressedMetrics = $regressed; improvedMetrics = @($metrics | Where-Object { $_['verdict'] -ceq 'improved' }).Count
        metrics = $metrics.ToArray()
    }
}

function Format-PerformanceSetVerdict {
    <# The lines a runner prints for a set: its status and counts, then every metric that regressed or improved. #>
    param([Parameter(Mandatory)][System.Collections.IDictionary] $Set, [string] $Label = 'Set')
    $format = { param([string] $Text, [object[]] $Values) [string]::Format($script:Invariant, $Text, $Values) }
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add((& $format '{0}: {1}; {2} runs per side, smallest attainable p {3:0.####}, {4} regressed, {5} improved' @($Label, $Set['status'],
                $Set['baselineRuns'], $Set['minimumAttainableP'], $Set['regressedMetrics'], $Set['improvedMetrics'])))
    if ($Set['minimumAttainableP'] -ge $script:Significance) {
        $lines.Add('  Too few runs per side to reach p < 0.05: only a rise in an exact budget can be established. Repeat more.')
    }
    foreach ($metric in $Set['metrics']) {
        if ($metric['verdict'] -ceq 'within-noise') { continue }
        $change = if ($null -eq $metric['changePercent']) { 'from zero' } else { & $format '{0:+0.00;-0.00}%' @([double]$metric['changePercent']) }
        $lines.Add((& $format '  {0} {1}/{2}: {3:#,0.###} -> {4:#,0.###} ({5}, p {6:0.####}, band {7}%, spread {8:0.0}% / {9:0.0}%)' @($metric['verdict'], $metric['phase'],
                    $metric['metric'], $metric['baselineMedian'], $metric['candidateMedian'], $change, $metric['pValue'], $metric['noisePercent'],
                    $(if ($null -eq $metric['baselineSpreadPercent']) { [double]::NaN } else { $metric['baselineSpreadPercent'] }),
                    $(if ($null -eq $metric['candidateSpreadPercent']) { [double]::NaN } else { $metric['candidateSpreadPercent'] }))))
    }
    return $lines.ToArray()
}

function ConvertTo-PerformanceComparisonJson([Parameter(Mandatory)][System.Collections.IDictionary] $Result, [string] $NewLine = [Environment]::NewLine) {
    # The layout compare_performance.py wrote: two-space indentation, ASCII escapes and a final newline.
    $json = ConvertTo-Json -InputObject $Result -Depth 6 -EscapeHandling EscapeNonAscii
    return (($json -replace "`r?`n", $NewLine) + $NewLine)
}

function Invoke-PerformanceComparison {
    <# Writes the comparison and returns the exit code: 1 when advice is required or the evidence is invalid. #>
    param([Parameter(Mandatory)][string] $Candidate, [string] $Baseline = '', [Parameter(Mandatory)][string] $Output)
    try {
        $after = Read-PerformanceReceipt $Candidate
        Assert-PerformanceReceipt $after
        $result = if ($Baseline) { Compare-PerformanceReceipt -Before (Read-PerformanceReceipt $Baseline) -After $after }
        else { [ordered]@{ status = 'unpaired'; changes = @() } }
    } catch {
        $result = [ordered]@{ status = 'invalid-evidence'; error = $_.Exception.Message; changes = @() }
    }
    [IO.File]::WriteAllText([IO.Path]::GetFullPath($Output), (ConvertTo-PerformanceComparisonJson $result), [Text.UTF8Encoding]::new($false))
    Write-Host "Performance comparison: $($result['status'])"
    foreach ($change in $result['changes']) {
        if ($change['regressed']) {
            Write-Host ([string]::Format($script:Invariant, 'REGRESSION {0}/{1}: {2:F3} -> {3:F3}', $change['scenario'], $change['metric'], [double]$change['before'], [double]$change['after']))
        }
    }
    if ($result['status'] -eq 'advice-required') {
        Write-Host ('Repeat on the same quiet fixture to confirm. Ask the developer for advice before accepting a confirmed regression. ' +
            'Options: optimize the affected work/cache, reduce optional scope, or defer/revert the change. Do not replace the baseline to pass.')
    }
    return [int]($result['status'] -in @('advice-required', 'invalid-evidence'))
}

Export-ModuleMember -Function Get-PerformanceMetricNames, Get-PerformanceIdentityKeys, Get-BenchmarkInputPaths, Get-SourceFingerprint,
    Read-PerformanceReceipt, Assert-PerformanceReceipt, Compare-PerformanceReceipt, ConvertTo-PerformanceComparisonJson,
    Invoke-PerformanceComparison, Get-MannWhitneyTest, Get-MinimumAttainableP, Get-MetricVerdict, Compare-PerformanceSet,
    Format-PerformanceSetVerdict
