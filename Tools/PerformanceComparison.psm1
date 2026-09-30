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

function Get-PerformanceMetricNames { return @($script:Metrics.Keys) }

function Get-PerformanceIdentityKeys { return @($script:Identity) }

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

function Compare-PerformanceReceipt {
    param([Parameter(Mandatory)][System.Collections.IDictionary] $Before, [Parameter(Mandatory)][System.Collections.IDictionary] $After)
    Assert-PerformanceReceipt $Before
    Assert-PerformanceReceipt $After
    foreach ($key in $script:Identity + @('benchmarkSha256')) {
        if (-not (Test-SameJsonValue $Before[$key] $After[$key])) { throw "Unmatched fixture: $key" }
    }
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

Export-ModuleMember -Function Get-PerformanceMetricNames, Get-PerformanceIdentityKeys, Read-PerformanceReceipt, Assert-PerformanceReceipt,
    Compare-PerformanceReceipt, ConvertTo-PerformanceComparisonJson, Invoke-PerformanceComparison
