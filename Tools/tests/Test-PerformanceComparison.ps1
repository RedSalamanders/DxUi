# Regression, missing evidence, fixture mismatch and hard-budget failures, plus parity with every stored comparison.
[CmdletBinding()] param()
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

Complete-TestRun 'PerformanceComparison'
