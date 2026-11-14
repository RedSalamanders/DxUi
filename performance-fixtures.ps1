<#
.SYNOPSIS Writes the counter and memory receipt of performance fixtures (plan W0.4) under Measurements.
.DESCRIPTION Specs/Testing/Testing_Validation.md, "Performance fixtures". For each requested fixture, variant and host, a
fresh test process runs the fixture once and writes its native receipt (every W0.1 counter per phase and step, the W0.3
memory fields, adapter and compiler). This script adds provenance - source commit and dirty flag, build profile, OS build,
WARP version, display refresh rates, the active power plan and a host-idle record - and the SHA-256 of the framework and
fixture sources, then writes <OutputRoot>/<FX>/<receipt>.json and regenerates that directory's README. Nothing on the
machine is changed: the power plan and refresh rate are recorded, and the idle record is an observation, never a gate.
Timing is "not measured" unless -TimingReceipt names a performance-timing.ps1 calibration or comparison receipt from a
hardware adapter for the same fixture. -CountDrawCalls installs the counting COM wrappers so frame.drawCalls is counted (embedded host;
the receipt name ends in -drawcalls). An existing receipt is never replaced without -Replace.
.EXAMPLE ./performance-fixtures.ps1 -Fixture FX5
.EXAMPLE ./performance-fixtures.ps1 -Fixture FX5 -Variant uhd-192dpi -Adapter Hardware -SkipBuild
#>
[CmdletBinding()]
param(
    [ValidatePattern('^(All|FX[0-9]+)$')][string[]] $Fixture = @('FX5'),
    [ValidatePattern('^[a-z0-9-]*$')][string] $Variant = '',
    [ValidateSet('Warp','Hardware')][string] $Adapter = 'Warp',
    [ValidateSet('All','Embedded','Window')][string] $FixtureHost = 'All',
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Release',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [string] $OutputRoot = 'Measurements/PerformanceAndMotion_2026-11-13',
    [string] $TimingReceipt = '',
    [ValidateRange(1,60)][int] $IdleSeconds = 3,
    [switch] $CountDrawCalls,
    [switch] $Replace,
    [switch] $SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Tools/PerformanceFixtures.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'Tools/FrameTiming.psm1') -Force

if (-not $SkipBuild) {
    $global:LASTEXITCODE = 0
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform -Projects 'DxUi.EmbeddedTests,DxUi.ControlTests'
    if ($LASTEXITCODE -ne 0) { throw "Performance fixture build failed with exit code $LASTEXITCODE." }
}
$binaries = Join-Path $PSScriptRoot ".build/$Platform/$Configuration"
$executables = @{ embedded = (Join-Path $binaries 'DxUi.EmbeddedTests.exe'); window = (Join-Path $binaries 'DxUi.ControlTests.exe') }
if (-not (Test-Path -LiteralPath $executables.embedded -PathType Leaf)) { throw "Missing $($executables.embedded); build first." }
$raw = Join-Path $PSScriptRoot ('.build/reports/performance-fixtures/' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ', [Globalization.CultureInfo]::InvariantCulture))
New-Item -ItemType Directory -Force -Path $raw | Out-Null

function Read-Json([string] $Path) { return (Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json -AsHashtable -Depth 40 -DateKind String) }
$global:LASTEXITCODE = 0
& $executables.embedded '--fixture-catalog' (Join-Path $raw 'catalog.json') | Out-Host
if ($LASTEXITCODE -ne 0) { throw "The fixture catalog could not be read (exit code $LASTEXITCODE)." }
$catalog = @((Read-Json (Join-Path $raw 'catalog.json'))['fixtures'])
$selected = if ($Fixture -contains 'All') { @($catalog | Where-Object { $_['built'] }) } else {
    @($Fixture | ForEach-Object { $id = $_; $entry = @($catalog | Where-Object { $_['id'] -ceq $id }); if (-not $entry.Count) { throw "Unknown fixture $id." }
        if (-not $entry[0]['built']) { throw "$id is declared by the plan but not built yet." }; $entry[0] })
}
$timing = $null
if ($TimingReceipt) { $timing = Read-Json $TimingReceipt }

$commit = @(& git -C $PSScriptRoot rev-parse HEAD)[0].Trim()
$dirty = [bool]@(& git -C $PSScriptRoot status --porcelain).Count
$warp = Join-Path $env:SystemRoot 'System32/d3d10warp.dll'

function Get-HostIdleRecord([string] $Path) {
    <# Watch-BenchmarkHost.ps1 for $IdleSeconds before the run: an observation, not an admission gate. #>
    $start = [Diagnostics.ProcessStartInfo]::new((Join-Path $PSHOME 'pwsh.exe'))
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    foreach ($argument in @('-NoProfile','-NonInteractive','-File',(Join-Path $PSScriptRoot 'Tools/Watch-BenchmarkHost.ps1'),'-OutputPath',$Path,'-IntervalMilliseconds','500')) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        while (-not (Test-Path -LiteralPath ($Path + '.ready'))) {
            if ($process.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw "The host observer did not start: $Path" }
            Start-Sleep -Milliseconds 100
        }
        Start-Sleep -Seconds $IdleSeconds
        [IO.File]::WriteAllText($Path + '.stop', [DateTime]::UtcNow.ToString('o'))
        # The observer is this script's own child; only it is ever stopped forcibly.
        if (-not $process.WaitForExit(30000)) { $process.Kill(); $process.WaitForExit() }
    } finally { $process.Dispose() }
    $samples = @(Get-Content -LiteralPath $Path | Where-Object { $_ } | ForEach-Object { $_ | ConvertFrom-Json -AsHashtable -Depth 6 })
    return [ordered]@{ seconds = $IdleSeconds; busyPercent = (Get-FrameTimingHostBusyPercent -Samples $samples); observer = 'Tools/Watch-BenchmarkHost.ps1'; required = $false }
}

$written = [Collections.Generic.List[string]]::new()
foreach ($entry in $selected) {
    $id = [string]$entry['id']
    $directory = Join-Path (Join-Path $PSScriptRoot $OutputRoot) $id
    $variants = @($entry['variants'] | Where-Object { if ($Variant) { $_['name'] -ceq $Variant } else { $Adapter -eq 'Hardware' -or -not $_['hardwareOnly'] } })
    if (-not $variants.Count) { throw "$id has no variant '$Variant' for the $Adapter adapter." }
    foreach ($hostName in @($entry['hosts'])) {
        if ($FixtureHost -ne 'All' -and $hostName -ne $FixtureHost.ToLowerInvariant()) { continue }
        if ($hostName -eq 'window' -and $Adapter -ne 'Warp') { throw 'The window host renders on WARP only.' }
        foreach ($variantEntry in $variants) {
            $name = Get-PerformanceFixtureReceiptName -Fixture $id -Variant $variantEntry['name'] -HostName $hostName -Adapter $Adapter -Platform $Platform -Configuration $Configuration
            if ($CountDrawCalls) { if ($hostName -ne 'embedded') { throw 'Draw calls are counted on the embedded host only.' }; $name = $name.Replace('.json', '-drawcalls.json') }
            $target = Join-Path $directory $name
            if ((Test-Path -LiteralPath $target) -and -not $Replace) { throw "Preserve the existing receipt $target (use -Replace to measure it again deliberately)." }
            $selection = "$id@$($variantEntry['name'])"
            $idle = Get-HostIdleRecord (Join-Path $raw "$name.host.jsonl")
            $nativePath = Join-Path $raw $name
            $arguments = if ($hostName -eq 'embedded') { @('--fixture-receipt', $selection, $Adapter, $nativePath) + @(if ($CountDrawCalls) { 'DrawCalls' }) } else { @('--fixture-receipt', $selection, $nativePath) }
            $global:LASTEXITCODE = 0
            & $executables[$hostName] @arguments | Out-Host
            if ($LASTEXITCODE -ne 0) { throw "The $hostName receipt run of $selection failed with exit code $LASTEXITCODE." }
            $native = Read-Json $nativePath
            $provenance = [ordered]@{
                sourceCommit = $commit; sourceDirty = $dirty; platform = $Platform; configuration = $Configuration
                command = "performance-fixtures.ps1 -Fixture $id -Variant $($variantEntry['name']) -Adapter $Adapter -FixtureHost $hostName -Configuration '$Configuration' -Platform $Platform"
                executableSha256 = (Get-FileHash -LiteralPath $executables[$hostName] -Algorithm SHA256).Hash
                os = (Get-PerformanceFixtureOsRecord); warpVersion = $(if ($native['adapter']['software']) { (Get-Item -LiteralPath $warp).VersionInfo.FileVersion } else { '' })
                displays = @(Get-PerformanceFixtureDisplayRecord); powerPlan = (Get-FrameTimingActiveScheme); hostIdle = $idle
                cpu = (Get-ItemProperty -LiteralPath 'HKLM:\HARDWARE\DESCRIPTION\System\CentralProcessor\0' -Name ProcessorNameString).ProcessorNameString.Trim()
                recordedUtc = [DateTime]::UtcNow.ToString('o')
            }
            $inputs = Get-PerformanceFixtureInputHashes -Root $PSScriptRoot -Sources @($entry['sources'])
            $receipt = Add-PerformanceFixtureProvenance -Receipt $native -Provenance $provenance -Inputs $inputs -Timing $timing -TimingPath $TimingReceipt
            New-Item -ItemType Directory -Force -Path $directory | Out-Null
            [IO.File]::WriteAllText($target, (ConvertTo-Json -InputObject $receipt -Depth 40) + "`n", [Text.UTF8Encoding]::new($false))
            $written.Add($target)
        }
    }
    if (Test-Path -LiteralPath $directory) {
        $files = @(Get-ChildItem -LiteralPath $directory -Filter '*.json' | Sort-Object Name)
        $receipts = @($files | ForEach-Object { Read-Json $_.FullName })
        [IO.File]::WriteAllText((Join-Path $directory 'README.md'), (New-PerformanceFixtureReadme -Receipts $receipts -FileNames @($files | ForEach-Object Name)), [Text.UTF8Encoding]::new($false))
    }
}
foreach ($path in $written) { Write-Host "Performance fixture receipt: $path" }
