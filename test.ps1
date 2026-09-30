<#
.SYNOPSIS Builds and runs Foundation, inherited controls, and supplied-device tests with per-suite receipts.
.PARAMETER Tests Test function names (exact, case-sensitive) that limit each control suite to those tests, passed to
DxUi.ControlTests.exe as --test=. Every name must be a test of every requested control suite, so pair it with one -Suites value;
an unknown name fails the run. Foundation and Embedded ignore it. The receipt of a filtered run is written to a separate
*.filtered.json file, never over the receipt of the whole suite.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Debug',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [switch] $SkipBuild,
    [string] $PerformanceBaseline = '',
    [string[]] $Suites = @('Foundation','Embedded','Grid','Theme','Control','Menu','MenuExitLifetime','NewControls','EditorControls','TextField','NativeTextInput','MultilineText','ReadOnly','ComboBox','Tree','Tooltip','Rendering','Animation','Accessibility','WindowHost'),
    [string[]] $Tests = @()
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
# A comma-separated single value is accepted like an array, so -Tests 'A,B' and -Tests A,B are the same request.
$Tests = @($Tests | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
if ($Tests.Count -and -not @($Suites | Where-Object { $_ -notin @('Foundation','Embedded') }).Count) { throw '-Tests selects tests within DxUi.ControlTests.exe suites; none of the requested suites is one.' }
& (Join-Path $PSScriptRoot 'Tools/tests/Test-ConsumerUpdate.ps1')
& (Join-Path $PSScriptRoot 'Tools/tests/Test-AsanRuntime.ps1')
$nativeArchitecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
if (($Platform -eq 'ARM64') -and ($nativeArchitecture -ne 'Arm64')) { throw 'ARM64 runtime tests require an ARM64 host; use build.ps1 for cross-compilation.' }
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform }
# The runner's --test contract (single tests, unknown names) is checked against the executable this run is about to use.
if (@($Suites | Where-Object { $_ -notin @('Foundation','Embedded') }).Count) { & (Join-Path $PSScriptRoot 'Tools/tests/Test-TestFilter.ps1') -Configuration $Configuration -Platform $Platform }
$reports = Join-Path $PSScriptRoot '.build/reports'
$logs = Join-Path $PSScriptRoot '.build/logs'
New-Item -ItemType Directory -Path $reports,$logs -Force | Out-Null
# Each test invocation retains its own raw rounds, including noisy or failing comparisons.
$performanceReport = Join-Path $reports "Performance-$Platform-$Configuration-$([guid]::NewGuid().ToString('N')).json"
& (Join-Path $PSScriptRoot 'performance.ps1') -Configuration $Configuration -Platform $Platform -SkipBuild -OutputPath $performanceReport -Baseline $PerformanceBaseline
$performance = Get-Content -Raw -LiteralPath $performanceReport | ConvertFrom-Json
$performanceComparison = Get-Content -Raw -LiteralPath ($performanceReport + '.comparison.json') | ConvertFrom-Json
$failures = @()
if ($Configuration -eq 'ASan Debug') {
    $probe = Join-Path $PSScriptRoot ".build/$Platform/$Configuration/DxUi.FoundationTests.exe"
    $probeLog = Join-Path $logs "test-AddressSanitizer-$Platform.log"
    $previousOptions = $env:ASAN_OPTIONS
    try {
        $env:ASAN_OPTIONS = 'halt_on_error=1:abort_on_error=0:detect_leaks=0'
        & $probe --asan-probe *> $probeLog
        $probeExit = $LASTEXITCODE
    } finally { $env:ASAN_OPTIONS = $previousOptions }
    $detected = $probeExit -ne 0 -and [bool](Select-String -LiteralPath $probeLog -SimpleMatch 'AddressSanitizer: heap-use-after-free')
    [ordered]@{suite='AddressSanitizer';configuration=$Configuration;platform=$Platform;nativeArchitecture=$nativeArchitecture;
        completedUtc=[DateTime]::UtcNow.ToString('o');executable=$probe;sha256=(Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash;
        exitCode=$probeExit;detected=$detected;log=$probeLog} | ConvertTo-Json | Set-Content (Join-Path $reports "AddressSanitizer-$Platform.json")
    if (-not $detected) { throw "AddressSanitizer did not diagnose the deliberate isolated use-after-free. See $probeLog" }
    Write-Host 'PASS AddressSanitizer detection probe (isolated expected failure)'
}
Push-Location $PSScriptRoot
try {
    foreach ($suite in $Suites) {
        $name = switch ($suite) { Foundation {'DxUi.FoundationTests.exe'} Embedded {'DxUi.EmbeddedTests.exe'} default {'DxUi.ControlTests.exe'} }
        $executable = Join-Path $PSScriptRoot ".build/$Platform/$Configuration/$name"
        if (-not (Test-Path -LiteralPath $executable)) { throw "Test executable is missing: $executable" }
        $arguments = @(if ($suite -in @('Foundation','Embedded')) { @() } elseif ($suite -in @('Menu','NativeTextInput','MenuResources','MenuResourceScaling')) { @("--suite=$suite") } else { @("--suite=$suite",'--no-activate') })
        $filtered = $Tests.Count -and $suite -notin @('Foundation','Embedded')
        if ($filtered) { $arguments += "--test=$($Tests -join ',')" }
        # A filtered run is partial evidence: its log and receipt never replace those of the whole suite.
        $suffix = if ($filtered) { '.filtered' } else { '' }
        $log = Join-Path $logs "test-$suite-$Platform-$Configuration$suffix.log"
        Write-Host "Running $suite ($Platform $Configuration)$(if ($filtered) { ", tests: $($Tests -join ', ')" })"
        & $executable @arguments *> $log
        $testExit = $LASTEXITCODE
        $skips = @(Get-Content -LiteralPath $log | Where-Object { $_ -match '^SKIPPED:' })
        $receipt = [ordered]@{
            suite=$suite; configuration=$Configuration; platform=$Platform; nativeArchitecture=$nativeArchitecture
            completedUtc=[DateTime]::UtcNow.ToString('o'); executable=$executable
            sha256=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash; exitCode=$testExit; skips=$skips
            performanceReport=$performanceReport; performanceScenarios=$performance.scenarios
            performanceComparison=$performanceComparison.status; performanceExecutableSha256=$performance.executableSha256
        }
        if ($filtered) { $receipt['tests'] = @($Tests) }
        $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $reports "$suite-$Platform-$Configuration$suffix.json") -Encoding utf8
        if ($testExit -ne 0) { $failures += $suite; Get-Content -LiteralPath $log -Tail 12 }
        else { Write-Host "PASS $suite ($($skips.Count) capability skips recorded$(if ($filtered) { "; filtered to $($Tests -join ', ')" }))" }
    }
} finally { Pop-Location }
if ($failures.Count) { throw "DxUi failed suites: $($failures -join ', '). See .build/logs and .build/reports." }
Write-Host "All $($Suites.Count) requested suites passed."
