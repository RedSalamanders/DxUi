<#
.SYNOPSIS Summarizes interleaved MenuUiaCost runs (Invoke-MenuUiaPairs.ps1) and compares the candidate with the baseline.
.DESCRIPTION Every run is reduced to one value per metric, and the six or so values of each side are compared with the
paired benchmark's verdict (Tools/PerformanceComparison.psm1, Get-MetricVerdict): an exact two-sided Mann-Whitney test
and an investigation band, 5% for timings and 2% for memory, the contract's bands.
Memory, per variant: the median over the cycles after the first eight of what an open menu adds to the live heap
(rendered minus before), to private bytes, and to the library's count of menu accessibility records, and of what a
closed menu leaves (closed minus before).
Timing, per variant and operation: the median of every sample of the openings after the first, in wall microseconds and
in thousands of the thread's own CPU cycles.
.PARAMETER Directory The directory holding A*.jsonl and B*.jsonl.
.PARAMETER Json Where to write the comparison as JSON (optional).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $Directory,
    [string] $Json = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
Import-Module (Join-Path $repo 'Tools\PerformanceComparison.psm1') -Force

function Get-Median([double[]] $Values) {
    $sorted = @($Values | Sort-Object)
    if ($sorted.Count -eq 0) { return [double]::NaN }
    $middle = [int][Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2 -eq 1) { return [double]$sorted[$middle] }
    return ([double]$sorted[$middle - 1] + [double]$sorted[$middle]) / 2.0
}

# One value per run and metric: run label -> metric key -> value.
function Read-Run([string] $Path) {
    $records = @(Get-Content -LiteralPath $Path | Where-Object { $_.StartsWith('{"fixture":"dxui-menu-uia-cost-v1"') } | ForEach-Object { $_ | ConvertFrom-Json })
    $values = [ordered]@{}
    $environment = @($records | Where-Object { $_.part -eq 'environment' })
    $memory = @($records | Where-Object { $_.part -eq 'memory' -and $_.cycle -ge 8 })
    foreach ($group in ($memory | Group-Object variant)) {
        $byCycle = $group.Group | Group-Object cycle
        $open = @(); $private = @(); $recordDeltas = @(); $left = @()
        foreach ($cycle in $byCycle) {
            $phase = @{}
            foreach ($sample in $cycle.Group) { $phase[$sample.phase] = $sample }
            if (-not ($phase.ContainsKey('before') -and $phase.ContainsKey('rendered') -and $phase.ContainsKey('closed'))) { throw "Incomplete memory cycle in $Path" }
            $open += [double]($phase.rendered.liveBytes - $phase.before.liveBytes)
            $private += [double]($phase.rendered.privateBytes - $phase.before.privateBytes)
            $recordDeltas += [double]($phase.rendered.accessibilityRecords - $phase.before.accessibilityRecords)
            $left += [double]($phase.closed.liveBytes - $phase.before.liveBytes)
        }
        $values["memory/$($group.Name)/openLiveBytes"] = Get-Median $open
        $values["memory/$($group.Name)/openPrivateBytes"] = Get-Median $private
        $values["memory/$($group.Name)/openAccessibilityRecords"] = Get-Median $recordDeltas
        $values["memory/$($group.Name)/closedLiveBytes"] = Get-Median $left
    }
    $timing = @($records | Where-Object { $_.part -eq 'timing' -and $_.open -ge 1 })
    foreach ($group in ($timing | Group-Object variant, operation)) {
        $first = $group.Group[0]
        $values["timing/$($first.variant)/$($first.operation)/wallUs"] = Get-Median ([double[]]@($group.Group | ForEach-Object { $_.wallUs }))
        $values["timing/$($first.variant)/$($first.operation)/kiloCycles"] = Get-Median ([double[]]@($group.Group | ForEach-Object { $_.kiloCycles }))
    }
    return [ordered]@{ values = $values; uiaClientsListening = @($environment | ForEach-Object { $_.PSObject.Properties | Where-Object Name -like 'uiaClients*' | ForEach-Object { "$($_.Name)=$($_.Value)" } }) -join ' ' }
}

$runs = [ordered]@{}
foreach ($file in (Get-ChildItem -LiteralPath $Directory -Filter '*.jsonl' | Sort-Object Name)) {
    if ($file.BaseName -notmatch '^[AB]\d+$') { continue }
    $runs[$file.BaseName] = Read-Run $file.FullName
}
$baselineRuns = @($runs.Keys | Where-Object { $_ -like 'A*' })
$candidateRuns = @($runs.Keys | Where-Object { $_ -like 'B*' })
if (-not $baselineRuns.Count -or -not $candidateRuns.Count) { throw "Need A*.jsonl and B*.jsonl runs in $Directory" }
$keys = @($runs[$baselineRuns[0]].values.Keys)
$comparison = [ordered]@{ baselineRuns = $baselineRuns; candidateRuns = $candidateRuns; uiaClients = [ordered]@{}; metrics = [ordered]@{} }
foreach ($label in $runs.Keys) { $comparison.uiaClients[$label] = $runs[$label].uiaClientsListening }
foreach ($key in $keys) {
    $baseline = [double[]]@($baselineRuns | ForEach-Object { $runs[$_].values[$key] })
    $candidate = [double[]]@($candidateRuns | ForEach-Object { if ($runs[$_].values.Contains($key)) { $runs[$_].values[$key] } else { [double]::NaN } })
    $band = if ($key.StartsWith('timing/')) { 0.05 } else { 0.02 }
    $metric = [ordered]@{ baseline = $baseline; candidate = $candidate }
    if (@($baseline + $candidate | Where-Object { [double]::IsNaN($_) }).Count) {
        $metric.verdict = 'missing'
    } elseif (@($baseline + $candidate | Where-Object { $_ -ne 0 }).Count -eq 0) {
        $metric.verdict = 'zero'
    } else {
        foreach ($entry in (Get-MetricVerdict -Direction lower -Band $band -Baseline $baseline -Candidate $candidate).GetEnumerator()) { $metric[$entry.Key] = $entry.Value }
    }
    $comparison.metrics[$key] = $metric
}

'| Metric | Baseline median | Candidate median | Change | p | Verdict |'
'| --- | ---: | ---: | ---: | ---: | --- |'
foreach ($key in $comparison.metrics.Keys) {
    $metric = $comparison.metrics[$key]
    if (-not $metric.Contains('baselineMedian')) {
        '| {0} | {1} | {2} | | | {3} |' -f $key, (Get-Median $metric.baseline), (Get-Median $metric.candidate), $metric.verdict
        continue
    }
    $change = if ($null -ne $metric.changePercent) { '{0:+0.0;-0.0;0.0}%' -f $metric.changePercent } else { '' }
    '| {0} | {1:N1} | {2:N1} | {3} | {4:N4} | {5} |' -f $key, $metric.baselineMedian, $metric.candidateMedian, $change, $metric.pValue, $metric.verdict
}
''
foreach ($label in $comparison.uiaClients.Keys) { "$label $($comparison.uiaClients[$label])" }
if ($Json) { $comparison | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $Json -Encoding utf8 }
