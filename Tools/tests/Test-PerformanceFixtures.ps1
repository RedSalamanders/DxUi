<# .SYNOPSIS Proves the receipt side of the performance fixtures (Tools/PerformanceFixtures.psm1): input hashes cover the
framework and the fixture, provenance is added without dropping a native field, timing is filled only from a hardware
calibration or comparison of the same fixture, and the generated README lists every receipt. No native build is needed. #>
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $root 'Tools/PerformanceFixtures.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force

function New-NativeReceipt {
    $summary = [ordered]@{ min = 83; max = 83; total = 3320 }
    $phase = [ordered]@{ name = 'dirty'; kind = 'measured'; description = 'values change'; steps = 40
        series = [ordered]@{ 'frame.paintExecutions' = @(83, 83) }
        summary = [ordered]@{ 'frame.paintExecutions' = $summary; 'frame.preparations' = [ordered]@{ min = 1; max = 1; total = 40 }
            created = [ordered]@{ min = 56; max = 56; total = 2240 }; churn = [ordered]@{ min = 56; max = 56; total = 2240 }
            cppAllocations = [ordered]@{ min = 54; max = 54; total = 2160 } } }
    return [ordered]@{ schema = 1; workloadOwner = 'DxUi'; fixture = 'dxui-performance-fixture-v1'; fixtureId = 'FX5'; title = 'ComplexUi'
        scenario = 'The scene.'; budgets = @('G2', 'G3'); host = 'embedded'; variant = [ordered]@{ name = 'default'; width = 1280; height = 720; dpi = 96 }
        renderer = 'WARP'; configuration = 'Release'; compiler = 195237025
        adapter = [ordered]@{ description = 'Microsoft Basic Render Driver'; software = $true; driverVersion = '10.0.26300.1' }
        phases = @($phase); memory = [ordered]@{ videoMemory = [ordered]@{ applicable = $false; reason = 'software adapter' }
            samples = @([ordered]@{ at = 'built'; trackedLiveBytes = 53125 }) }
        timing = [ordered]@{ status = 'not measured'; reason = 'WARP' } }
}
function New-Provenance {
    return [ordered]@{ sourceCommit = ('a' * 40); sourceDirty = $false; platform = 'x64'; configuration = 'Release'; command = 'performance-fixtures.ps1 -Fixture FX5'
        os = [ordered]@{ description = 'Windows'; build = '26300'; ubr = 1 }; warpVersion = '10.0'; displays = @([ordered]@{ name = 'Display'; refreshRateHz = 120 })
        powerPlan = [ordered]@{ guid = 'g'; text = 'Balanced' }; hostIdle = [ordered]@{ seconds = 3; busyPercent = 4.25 }; recordedUtc = '2026-11-14T00:00:00Z' }
}

Invoke-TestCase 'input hashes cover the framework and the fixture sources, once each' {
    $hashes = Get-PerformanceFixtureInputHashes -Root $root -Sources @('Samples/ComplexUi/ComplexUiScene.h', 'Tests/Embedded/Embedded.Tests.PerformanceFixture.h')
    foreach ($path in (Get-PerformanceFixtureFrameworkInputs) + @('Samples/ComplexUi/ComplexUiScene.h')) {
        Assert-True ($hashes.Contains($path) -and $hashes[$path] -match '^[0-9A-F]{64}$') "hash of $path"
    }
    Assert-Equal (@(Get-PerformanceFixtureFrameworkInputs).Count + 1) $hashes.Count 'a source named twice is hashed once'
    Assert-Throws { Get-PerformanceFixtureInputHashes -Root $root -Sources @('Tests/Embedded/Absent.Tests.Nothing.h') } 'a missing source'
}

Invoke-TestCase 'receipt names are stable and lowercase' {
    Assert-Equal 'fx5-uhd-192dpi-embedded-hardware-x64-asan-debug.json' (Get-PerformanceFixtureReceiptName -Fixture FX5 -Variant uhd-192dpi -HostName embedded -Adapter Hardware -Platform x64 -Configuration 'ASan Debug') 'receipt name'
}

Invoke-TestCase 'provenance is added and timing stays not measured without a hardware calibration' {
    $receipt = Add-PerformanceFixtureProvenance -Receipt (New-NativeReceipt) -Provenance (New-Provenance) -Inputs ([ordered]@{ 'a.h' = ('0' * 64) })
    Assert-Equal 'not measured' $receipt['timing']['status'] 'timing without a timing receipt'
    Assert-Equal ('a' * 40) $receipt['provenance']['sourceCommit'] 'provenance carried'
    Assert-Equal 83 $receipt['phases'][0]['summary']['frame.paintExecutions']['max'] 'native fields kept'
    $smoke = [ordered]@{ purpose = 'Smoke'; adapterMode = 'Hardware'; fixtureName = 'FX5' }
    Assert-Throws { Add-PerformanceFixtureProvenance -Receipt (New-NativeReceipt) -Provenance (New-Provenance) -Inputs @{} -Timing $smoke } 'a smoke run never fills timing'
    $warp = [ordered]@{ purpose = 'Calibration'; adapterMode = 'Warp'; fixtureName = 'FX5' }
    Assert-Throws { Add-PerformanceFixtureProvenance -Receipt (New-NativeReceipt) -Provenance (New-Provenance) -Inputs @{} -Timing $warp } 'WARP never fills timing'
    $other = [ordered]@{ purpose = 'Calibration'; adapterMode = 'Hardware'; fixtureName = 'FX3' }
    Assert-Throws { Add-PerformanceFixtureProvenance -Receipt (New-NativeReceipt) -Provenance (New-Provenance) -Inputs @{} -Timing $other } 'another fixture never fills timing'
    $calibration = [ordered]@{ purpose = 'Calibration'; adapterMode = 'Hardware'; fixtureName = 'FX5@uhd-192dpi'; phase = 'dirty'; calibration = [ordered]@{ metrics = @([ordered]@{ metric = 'prepareCpuMs' }) } }
    $timed = Add-PerformanceFixtureProvenance -Receipt (New-NativeReceipt) -Provenance (New-Provenance) -Inputs @{} -Timing $calibration -TimingPath 'timing.json'
    Assert-Equal 'measured' $timed['timing']['status'] 'a hardware calibration fills timing'
    $foreign = New-NativeReceipt
    $foreign['fixture'] = 'dxui-complex-ui-v2'
    Assert-Throws { Add-PerformanceFixtureProvenance -Receipt $foreign -Provenance (New-Provenance) -Inputs @{} } 'not a fixture receipt'
}

Invoke-TestCase 'the generated README names the scenario, every receipt and its provenance' {
    $receipt = Add-PerformanceFixtureProvenance -Receipt (New-NativeReceipt) -Provenance (New-Provenance) -Inputs @{}
    $text = New-PerformanceFixtureReadme -Receipts @($receipt) -FileNames @('fx5-default-embedded-warp-x64-release.json')
    foreach ($expected in @('# FX5: ComplexUi', '[fx5-default-embedded-warp-x64-release.json](fx5-default-embedded-warp-x64-release.json)', '`G2`, `G3`',
            '| `dirty` (measured) | 40 | 83 | 40 total | 2,240 total | 2,240 total | 54 |', '120 Hz', 'Balanced', '4.3% busy', 'Timing: not measured', 'built 53,125')) {
        Assert-True ($text.Contains($expected)) "README contains: $expected"
    }
    Assert-True ($text.EndsWith("`n") -and -not $text.Contains("`r")) 'README uses LF and ends with a newline'
}
Complete-TestRun 'Performance fixture receipts'
