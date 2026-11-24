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
    Assert-True ($text.Contains('](../../../Specs/Testing/Testing_Validation.md#performance-fixtures)')) 'the default directory links three levels up'
    Assert-True (-not $text.Contains('`G13` line')) 'a receipt without budget lines has no G13 table'
}

Invoke-TestCase 'the generated README lists every G13 line with its status and closing item' {
    $native = New-NativeReceipt
    $native['budgetLines'] = [ordered]@{ G13 = @(
            [ordered]@{ line = 'per-control'; budget = '<= 912 B per control all-in on the FX12 10,000-control form'; counter = 'c'; limitBytes = 912; measuredBytes = 999; status = 'recorded exception'
                exception = [ordered]@{ closingItem = 'W9.6'; finding = 'f' } },
            [ordered]@{ line = 'minimized-video'; budget = '<= 8 MiB video memory when minimized'; counter = 'c'; limitBytes = 8388608; measuredBytes = $null
                status = 'not measured'; reason = 'software adapter' }) }
    $receipt = Add-PerformanceFixtureProvenance -Receipt $native -Provenance (New-Provenance) -Inputs @{}
    $text = New-PerformanceFixtureReadme -Receipts @($receipt) -FileNames @('fx5-default-embedded-warp-x64-release.json')
    foreach ($expected in @('| `per-control` | <= 912 B per control all-in on the FX12 10,000-control form | 999 | recorded exception (closed by W9.6) |',
            '| `minimized-video` | <= 8 MiB video memory when minimized | not measured | not measured: software adapter |')) {
        Assert-True ($text.Contains($expected)) "README contains: $expected"
    }
}
Invoke-TestCase 'an interactive-only fixture is skipped without -Interactive and runs alone with it' {
    $fx4 = [ordered]@{ id = 'FX4'; built = $true; interactiveOnly = $true }
    $fx5 = [ordered]@{ id = 'FX5'; built = $true; interactiveOnly = $false }
    $older = [ordered]@{ id = 'FX1'; built = $true } # a catalog written before the flag existed
    $ordinary = Get-PerformanceFixtureRunSelection -Selected @($fx4, $fx5, $older)
    Assert-Equal 'FX5,FX1' ((@($ordinary.Run) | ForEach-Object { $_['id'] }) -join ',') 'the ordinary run keeps every other fixture'
    Assert-Equal 'FX4: interactive, not run' (@($ordinary.Skipped) -join '|') 'the ordinary run says it skipped FX4 and does not fail'
    $interactive = Get-PerformanceFixtureRunSelection -Selected @($fx4) -Interactive
    Assert-Equal 'FX4' ((@($interactive.Run) | ForEach-Object { $_['id'] }) -join ',') 'the interactive run runs FX4'
    Assert-Throws { Get-PerformanceFixtureRunSelection -Selected @($fx4, $fx5) -Interactive } 'the lease never holds a fixture that does not need the desktop'
    Assert-Throws { Get-PerformanceFixtureRunSelection -Selected @() -Interactive } 'an interactive run without an interactive fixture'
}

Invoke-TestCase 'an interactive receipt refuses without the desktop lease and wherever test.ps1 -Interactive refuses' {
    $missing = Join-Path $root '.build/absent-for-test/DxUi.InteractiveLease.exe'
    $refusal = Get-PerformanceFixtureInteractiveRefusal -LeaseExecutable $missing -Environment @{} -UserInteractive $true
    Assert-True ($null -ne $refusal -and $refusal.Contains('lease is missing')) 'no lease executable, no interactive fixture'
    $present = Join-Path $PSScriptRoot 'Test-PerformanceFixtures.ps1' # any existing file stands in for the lease
    Assert-True ($null -eq (Get-PerformanceFixtureInteractiveRefusal -LeaseExecutable $present -Environment @{} -UserInteractive $true)) 'a lease and a desktop: no refusal'
    Assert-True ([string](Get-PerformanceFixtureInteractiveRefusal -LeaseExecutable $present -Environment @{ CI = 'true' } -UserInteractive $true)).Contains('CI job') 'a CI job refuses'
    Assert-True ([string](Get-PerformanceFixtureInteractiveRefusal -LeaseExecutable $present -Environment @{} -UserInteractive $false)).Contains('window station') 'no window station refuses'
    $script = [IO.File]::ReadAllText((Join-Path $root 'performance-fixtures.ps1'))
    foreach ($expected in @('[switch] $Interactive', 'Get-PerformanceFixtureInteractiveRefusal -LeaseExecutable', 'Invoke-DxUiInteractiveLease', "'--interactive'", 'Get-PerformanceFixtureRunSelection')) {
        Assert-True ($script.Contains($expected)) "performance-fixtures.ps1 uses $expected"
    }
}

Invoke-TestCase 'a regenerated README keeps the interactive run sheet' {
    $existing = "# FX4: Menus`r`n`r`nNot run yet.`r`n`r`n## Interactive run sheet`r`n`r`n1. Agree to the time.`r`n"
    $merged = Merge-PerformanceFixtureRunSheet -Generated "# FX4: Menus`n`n## Receipts`n" -Existing $existing
    Assert-Equal "# FX4: Menus`n`n## Receipts`n`n## Interactive run sheet`n`n1. Agree to the time.`n" $merged 'the run sheet follows the generated receipts'
    Assert-Equal "# FX5`n" (Merge-PerformanceFixtureRunSheet -Generated "# FX5`n" -Existing "# FX5`nold`n") 'without a run sheet the README is the generated one'
}


Invoke-TestCase 'a README under a nested -OutputRoot links to the repository from its own depth' {
    $receipt = Add-PerformanceFixtureProvenance -Receipt (New-NativeReceipt) -Provenance (New-Provenance) -Inputs @{}
    $text    = New-PerformanceFixtureReadme -Receipts @($receipt) -FileNames @('fx5-default-embedded-warp-x64-release.json') -RootPath '../../../..'
    Assert-True ($text.Contains('[performance fixtures](../../../../Specs/Testing/Testing_Validation.md#performance-fixtures)')) 'the framework link climbs four levels'
    Assert-True ($text.Contains('[performance and motion plan](../../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md)')) 'the plan link climbs four levels'
    Assert-True (-not $text.Contains('](../../../Specs/')) 'no link keeps the default depth'
    # performance-fixtures.ps1 derives the root from the directory it writes, so Measurements/<plan>/<item>/<FX> gets four levels.
    $repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
    $nested     = Join-Path $repository 'Measurements/PerformanceAndMotion_2026-11-13/W2.5/FX3'
    Assert-Equal '../../../..' ([IO.Path]::GetRelativePath($nested, $repository).Replace('\', '/')) 'a nested receipt directory is four levels below the root'
}
Complete-TestRun 'Performance fixture receipts'
