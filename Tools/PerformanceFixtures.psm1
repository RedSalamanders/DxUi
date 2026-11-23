# Performance fixture receipts (Specs/Testing/Testing_Validation.md, "Performance fixtures"; plan item W0.4): provenance
# that the native receipt cannot see, and the generated README of a fixture's receipt directory. Read-only with respect to
# the machine: the power plan, refresh rate and host load are recorded, never changed.
Set-StrictMode -Version Latest
# The interactive desktop lease's refusals, reused for an interactive-only fixture's receipt (performance-fixtures.ps1 -Interactive).
Import-Module (Join-Path $PSScriptRoot 'InteractiveRun.psm1')

$script:FrameworkInputs = @(
    'Tests/Embedded/Embedded.Tests.PerformanceFixture.h',
    'Tests/Embedded/Embedded.Tests.PerformanceFixtureCatalog.h',
    'Tests/Embedded/Embedded.Tests.PerformanceFixtures.h',
    'Tests/Embedded/Embedded.Tests.BenchmarkInstrument.h',
    'Tests/Controls/Controls.Tests.PerformanceFixtures.cpp',
    'Samples/EmbeddedControls/GraphicsFixture.h',
    'performance-fixtures.ps1',
    'Tools/PerformanceFixtures.psm1')

function Get-PerformanceFixtureFrameworkInputs { return @($script:FrameworkInputs) }

function Get-PerformanceFixtureInputHashes {
    <# SHA-256 of the framework's files and the fixture's own sources, keyed by repository path. #>
    param([Parameter(Mandatory)][string] $Root, [string[]] $Sources = @())
    $hashes = [ordered]@{}
    foreach ($path in @($script:FrameworkInputs) + @($Sources)) {
        if ($hashes.Contains($path)) { continue }
        $full = Join-Path $Root $path
        if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { throw "Performance fixture input is missing: $path" }
        $hashes[$path] = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash
    }
    return $hashes
}

function Get-PerformanceFixtureReceiptName {
    <# The receipt file name: <fixture>-<variant>-<host>-<adapter>-<platform>-<configuration>.json, lowercase. #>
    param([Parameter(Mandatory)][string] $Fixture, [Parameter(Mandatory)][string] $Variant, [Parameter(Mandatory)][string] $HostName,
        [Parameter(Mandatory)][string] $Adapter, [Parameter(Mandatory)][string] $Platform, [Parameter(Mandatory)][string] $Configuration)
    return (("{0}-{1}-{2}-{3}-{4}-{5}.json" -f $Fixture, $Variant, $HostName, $Adapter, $Platform, ($Configuration -replace ' ', '-')).ToLowerInvariant())
}

function Get-PerformanceFixtureOsRecord {
    $version = Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
    $ubr = if ($version.PSObject.Properties['UBR']) { [int]$version.UBR } else { 0 }
    return [ordered]@{ description = [Runtime.InteropServices.RuntimeInformation]::OSDescription; build = [string]$version.CurrentBuildNumber; ubr = $ubr
        displayVersion = $(if ($version.PSObject.Properties['DisplayVersion']) { [string]$version.DisplayVersion } else { '' }) }
}

function Get-PerformanceFixtureDisplayRecord {
    <# Each display adapter's current refresh rate as Windows reports it; recorded, never changed. #>
    $controllers = @(Get-CimInstance -ClassName Win32_VideoController -ErrorAction SilentlyContinue)
    return @($controllers | ForEach-Object { [ordered]@{ name = [string]$_.Name; refreshRateHz = $_.CurrentRefreshRate; driverVersion = [string]$_.DriverVersion
        horizontalResolution = $_.CurrentHorizontalResolution; verticalResolution = $_.CurrentVerticalResolution } })
}

function Add-PerformanceFixtureProvenance {
    <# Returns the native receipt with provenance, input hashes and the timing record added. The timing stays "not measured"
       unless a W0.2 calibration or comparison receipt from a hardware adapter is given for this fixture. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Receipt, [Parameter(Mandatory)][Collections.IDictionary] $Provenance,
        [Parameter(Mandatory)][Collections.IDictionary] $Inputs, [Collections.IDictionary] $Timing = $null, [string] $TimingPath = '')
    if ($Receipt['workloadOwner'] -cne 'DxUi' -or $Receipt['fixture'] -cne 'dxui-performance-fixture-v1') { throw 'Not a performance fixture receipt.' }
    $result = [ordered]@{}
    foreach ($key in $Receipt.Keys) { $result[$key] = $Receipt[$key] }
    $result['benchmarkInputs'] = $Inputs
    $result['provenance'] = $Provenance
    if ($null -ne $Timing) {
        if ($Timing['purpose'] -notin @('Calibration', 'Comparison')) { throw 'Only a calibration or comparison timing receipt fills the timing fields; a smoke run never does.' }
        if ($Timing['adapterMode'] -cne 'Hardware') { throw 'Timing fields come from a hardware adapter only.' }
        $timedFixture = ([string]$Timing['fixtureName']).Split('@')[0]
        if ($timedFixture -cne $Receipt['fixtureId']) { throw "The timing receipt measured $timedFixture, not $($Receipt['fixtureId'])." }
        $metrics = if ($Timing.Contains('calibration')) { $Timing['calibration']['metrics'] } else { $Timing['comparison']['metrics'] }
        $result['timing'] = [ordered]@{ status = 'measured'; source = $TimingPath; purpose = $Timing['purpose']; phase = $Timing['phase']; metrics = $metrics }
    }
    return $result
}

function Format-PerformanceFixtureNumber([object] $Value) {
    return ([double]$Value).ToString('N0', [Globalization.CultureInfo]::InvariantCulture)
}

function New-PerformanceFixtureReadme {
    <# The README of one fixture's receipt directory, generated from every receipt in it. RootPath is the repository root
       relative to that directory, from which its links are written: ../../.. for Measurements/<plan>/<FX>, one more level
       for each directory an -OutputRoot nests deeper. #>
    param([Parameter(Mandatory)][object[]] $Receipts, [Parameter(Mandatory)][string[]] $FileNames, [string] $RootPath = '../../..')
    $first = $Receipts[0]
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add("# $($first['fixtureId']): $($first['title'])")
    $lines.Add('')
    $lines.Add('Generated by `performance-fixtures.ps1` from the receipts in this directory; do not edit by hand. Fixture framework:')
    $lines.Add("[performance fixtures]($RootPath/Specs/Testing/Testing_Validation.md#performance-fixtures); plan item ``W0.4`` of the")
    $lines.Add("[performance and motion plan]($RootPath/Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md).")
    $lines.Add('')
    $lines.Add("- **Scenario**: $($first['scenario'])")
    $budgets = @($first['budgets'])
    $tick = [char]0x60
    $budgetText = if ($budgets.Count) { ($budgets | ForEach-Object { $tick + $_ + $tick }) -join ', ' } else { 'none directly (gates of later items)' }
    $lines.Add('- **Budgets proved**: ' + $budgetText)
    $lines.Add('- **Phases**: ' + ((@($first['phases']) | ForEach-Object { "``$($_['name'])`` ($($_['steps']) steps, $($_['kind'])): $($_['description'])" }) -join '; '))
    $lines.Add('- **Timing**: filled only by `performance-timing.ps1` on a hardware adapter in a quiet window (W0.2); "not measured" otherwise.')
    $lines.Add('')
    $lines.Add('## Receipts')
    $lines.Add('')
    for ($index = 0; $index -lt $Receipts.Count; $index++) {
        $receipt = $Receipts[$index]
        $p = $receipt['provenance']
        $variant = $receipt['variant']
        $adapter = $receipt['adapter']
        $displays = @($p['displays'] | ForEach-Object { "$($_['name']) $($_['refreshRateHz']) Hz" }) -join ', '
        $lines.Add("### [$($FileNames[$index])]($($FileNames[$index]))")
        $lines.Add('')
        $lines.Add("- Host ``$($receipt['host'])``, variant ``$($variant['name'])`` ($($variant['width'])x$($variant['height']) at $($variant['dpi']) DPI), renderer $($receipt['renderer']), $($p['platform']) $($receipt['configuration']) (compiler $($receipt['compiler']))")
        $lines.Add("- Source ``$($p['sourceCommit'])``, uncommitted changes: $($p['sourceDirty']); command: ``$($p['command'])``")
        $lines.Add("- Adapter: $($adapter['description']), driver $($adapter['driverVersion'])$(if ($p['warpVersion']) { "; WARP $($p['warpVersion'])" })")
        $lines.Add("- OS $($p['os']['description']) (build $($p['os']['build']).$($p['os']['ubr'])); displays: $displays; power plan: $($p['powerPlan']['text'])")
        $idle = $p['hostIdle']
        $lines.Add("- Host idle record: $([string]::Format([Globalization.CultureInfo]::InvariantCulture, '{0:0.0}', $idle['busyPercent']))% busy over $($idle['seconds']) s before the run (recorded, not required); recorded $($p['recordedUtc'])")
        $lines.Add("- Timing: $($receipt['timing']['status'])")
        $lines.Add('')
        $lines.Add('| Phase | Steps | `Paint` executions per step | Preparations | Resources created | Churn | C++ allocations per step |')
        $lines.Add('| --- | --- | --- | --- | --- | --- | --- |')
        foreach ($phase in @($receipt['phases'])) {
            $s = $phase['summary']
            $range = { param($name) if ($s.Contains($name)) { $v = $s[$name]; if ($v['min'] -eq $v['max']) { Format-PerformanceFixtureNumber $v['min'] } else { "$(Format-PerformanceFixtureNumber $v['min'])-$(Format-PerformanceFixtureNumber $v['max'])" } } else { 'not counted' } }
            $lines.Add("| ``$($phase['name'])`` ($($phase['kind'])) | $($phase['steps']) | $(& $range 'frame.paintExecutions') | $(Format-PerformanceFixtureNumber $s['frame.preparations']['total']) total | $(Format-PerformanceFixtureNumber $s['created']['total']) total | $(Format-PerformanceFixtureNumber $s['churn']['total']) total | $(& $range 'cppAllocations') |")
        }
        $lines.Add('')
        $memory = $receipt['memory']
        $tracked = @($memory['samples'] | Where-Object { $_.Contains('trackedLiveBytes') } | ForEach-Object { "$($_['at'])$(if ($_.Contains('phase')) { " $($_['phase'])" }) $(Format-PerformanceFixtureNumber $_['trackedLiveBytes'])" })
        if ($tracked.Count) { $lines.Add("Tracked live C++ bytes: $($tracked -join '; ').") }
        $lines.Add("Video memory: $(if ($memory['videoMemory']['applicable']) { 'reported per sample' } else { $memory['videoMemory']['reason'] }). Every counter per step is in the receipt's ``series``.")
        $lines.Add('')
        if ($receipt.Contains('budgetLines') -and $receipt['budgetLines'].Contains('G13')) {
            # W9.1: each G13 line as its counter beside the budget; a line over budget names the item that closes it.
            $lines.Add('| `G13` line | Budget | Measured bytes | Status |')
            $lines.Add('| --- | --- | --- | --- |')
            foreach ($line in @($receipt['budgetLines']['G13'])) {
                $measured = if ($null -eq $line['measuredBytes']) { 'not measured' } else { Format-PerformanceFixtureNumber $line['measuredBytes'] }
                $status = $line['status']
                if ($line.Contains('exception')) { $status += " (closed by $($line['exception']['closingItem']))" }
                if ($line.Contains('reason')) { $status += ": $($line['reason'])" }
                $lines.Add("| ``$($line['line'])`` | $($line['budget']) | $measured | $status |")
            }
            $lines.Add('')
        }
    }
    return ($lines -join "`n").TrimEnd() + "`n"
}

function Get-PerformanceFixtureRunSelection {
    <# Which of the selected catalog entries a run executes. An interactive-only fixture (Definition::interactiveOnly, the
       catalog's "interactiveOnly") takes the foreground: without -Interactive it is skipped with "<id>: interactive, not run"
       and never fails the run; with -Interactive only interactive-only fixtures run (under the desktop lease), and any other
       is refused by name so the lease holds only what needs the desktop. #>
    param([Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Selected, [switch] $Interactive)
    $run = [Collections.Generic.List[object]]::new()
    $skipped = [Collections.Generic.List[string]]::new()
    $refused = [Collections.Generic.List[string]]::new()
    foreach ($entry in $Selected) {
        $interactiveOnly = $entry.Contains('interactiveOnly') -and [bool]$entry['interactiveOnly']
        if ($Interactive -and -not $interactiveOnly) { $refused.Add([string]$entry['id']) }
        elseif (-not $Interactive -and $interactiveOnly) { $skipped.Add("$($entry['id']): interactive, not run") }
        else { $run.Add($entry) }
    }
    if ($refused.Count) { throw "-Interactive runs only interactive-only fixtures; not interactive: $($refused -join ', '). Run those without -Interactive." }
    if ($Interactive -and -not $run.Count) { throw '-Interactive was given no interactive-only fixture.' }
    return [pscustomobject]@{ Run = @($run); Skipped = @($skipped) }
}

function Get-PerformanceFixtureInteractiveRefusal {
    <# Why an interactive-only fixture's receipt cannot run here, or $null: the desktop lease (DxUi.InteractiveLease.exe) is
       missing, or the environment refuses exactly as test.ps1 -Interactive does (Get-DxUiInteractiveRefusal: a CI job, no
       interactive window station). Every input can be given, so the tooling test needs no desktop. #>
    param([Parameter(Mandatory)][string] $LeaseExecutable, [hashtable] $Environment = $null, [Nullable[bool]] $UserInteractive = $null)
    $refusal = Get-DxUiInteractiveRefusal -Environment $Environment -UserInteractive $UserInteractive
    if ($refusal) { return $refusal }
    if (-not (Test-Path -LiteralPath $LeaseExecutable -PathType Leaf)) {
        return "the interactive desktop lease is missing: $LeaseExecutable (build DxUi.InteractiveLease with build.ps1); an interactive-only fixture never runs without it"
    }
    return $null
}

function Merge-PerformanceFixtureRunSheet {
    <# A regenerated README keeps the hand-written "## Interactive run sheet" section of the one it replaces (that heading to
       the end of the file), so an interactive fixture's first receipt does not erase how to take the next. LF line endings. #>
    param([Parameter(Mandatory)][string] $Generated, [AllowEmptyString()][string] $Existing = '')
    $existingText = $Existing.Replace("`r`n", "`n")
    $marker = "`n## Interactive run sheet"
    $at = $existingText.IndexOf($marker, [StringComparison]::Ordinal)
    if ($at -lt 0) { return $Generated }
    return $Generated.Replace("`r`n", "`n").TrimEnd() + "`n" + $existingText.Substring($at).TrimEnd() + "`n"
}

Export-ModuleMember -Function Get-PerformanceFixtureFrameworkInputs, Get-PerformanceFixtureInputHashes, Get-PerformanceFixtureReceiptName,
    Get-PerformanceFixtureOsRecord, Get-PerformanceFixtureDisplayRecord, Add-PerformanceFixtureProvenance, New-PerformanceFixtureReadme,
    Get-PerformanceFixtureRunSelection, Get-PerformanceFixtureInteractiveRefusal, Merge-PerformanceFixtureRunSheet
