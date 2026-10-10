[CmdletBinding()] param([string] $PacketRoot = $PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-Median($Values) {
    $sorted = @($Values | Sort-Object)
    if (-not $sorted.Count) { throw 'An empty analytical series is invalid.' }
    $middle = [int][Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2) { return [double]$sorted[$middle] }
    return ([double]$sorted[$middle - 1] + [double]$sorted[$middle]) / 2
}

$manifestPath = Join-Path $PacketRoot 'SHA256SUMS'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'The retained packet SHA256SUMS is required for replay.' }
foreach ($line in Get-Content -LiteralPath $manifestPath) {
    if ($line -notmatch '^([A-Fa-f0-9]{64})  (.+)$') { throw "Malformed packet hash: $line" }
    $actual = (Get-FileHash -LiteralPath (Join-Path $PacketRoot $Matches[2]) -Algorithm SHA256).Hash
    if ($actual -cne $Matches[1].ToUpperInvariant()) { throw "Packet hash mismatch: $($Matches[2])" }
}

$plan = Get-Content -LiteralPath (Join-Path $PacketRoot 'baseline/pilot-plan.receipt.txt') -Raw | ConvertFrom-Json
if ($plan.reportBudget -ne 6 -or $plan.acceptance -cne 'descriptive-only') { throw 'Unexpected baseline plan.' }
$baselineFingerprint = $null
$baselineInputs = $null
$baseline = @(for ($index = 0; $index -lt 6; $index++) {
    $scenario = if ($index % 2) { 'MultilineGrid' } else { 'Default' }
    if ($plan.scenarios[$index] -cne $scenario) { throw 'Unexpected baseline schedule.' }
    $name = 'baseline-{0:d2}-{1}.json' -f ($index + 1), $scenario
    $r = Get-Content -LiteralPath (Join-Path $PacketRoot "baseline/$name") -Raw | ConvertFrom-Json
    if ($r.identityStatus -cne 'verifiable' -or $r.buildIdentityVersion -ne 2 -or
        $r.executableSha256 -cne $plan.executableSha256 -or $r.sourceCommit -cne $plan.sourceHead) {
        throw "Unexpected baseline provenance: $name"
    }
    $expectedFixture = if ($scenario -eq 'Default') { 'dxui-complex-ui-v2' } else { 'dxui-complex-ui-multiline-grid-v1' }
    if ($r.fixture -cne $expectedFixture -or $r.configuration -cne 'Release' -or $r.platform -cne 'x64' -or
        $r.identityScope -cne 'full' -or -not $r.sourceDirty -or $r.sourceFingerprint -cnotmatch '^[A-F0-9]{64}$') {
        throw "Unexpected baseline fixture/source state: $name"
    }
    $inputs = ConvertTo-Json $r.benchmarkInputs -Depth 8 -Compress
    if ($index -eq 0) { $baselineFingerprint = $r.sourceFingerprint; $baselineInputs = $inputs }
    elseif ($r.sourceFingerprint -cne $baselineFingerprint -or $inputs -cne $baselineInputs) { throw "Baseline source/input identity differs: $name" }
    if ($r.scenarios.Count -ne 2 -or $r.roundCount -ne 5 -or $r.framesPerRound -ne 40) { throw "Unexpected frame inventory: $name" }
    if (($r.scenarios.name -join ',') -cne 'clean,dirty' -or ($r.memoryPhases.name -join ',') -cne 'entry,device,scene,warm,capture,hidden') {
        throw "Unexpected baseline phase inventory: $name"
    }
    $phases = @(foreach ($phase in $r.scenarios) {
        if ($phase.rounds.Count -ne 5) { throw "Missing round: $name" }
        $fps = @($phase.rounds.fps)
        [ordered]@{ phase=$phase.name; fps=$fps; medianFps=(Get-Median $fps)
            minimumFps=($fps | Measure-Object -Minimum).Minimum; maximumFps=($fps | Measure-Object -Maximum).Maximum }
    })
    [ordered]@{ file=$name; scenario=$scenario; sourceFingerprint=$r.sourceFingerprint; sourceDirty=$r.sourceDirty; fixture=$r.fixture; phases=$phases; memoryPhases=$r.memoryPhases }
})
$analysis = [ordered]@{ purpose='descriptive-bounded-pilot'; qualification='not-established'; baseline=$baseline }
$diagnosticDirectory = Join-Path $PacketRoot 'diagnostic'
if (Test-Path -LiteralPath $diagnosticDirectory -PathType Container) {
    $repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
    $diagnosticPlan = Get-Content -LiteralPath (Join-Path $diagnosticDirectory 'pilot-plan.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
    foreach ($helper in @('Tools/PerformanceProfiling.psm1','Tools/PerformancePolicy.psm1','Tools/ScopedTesting.psm1')) {
        if (-not $diagnosticPlan.anchor.profilingInputs.Contains($helper) -or
            (Get-FileHash -LiteralPath (Join-Path $repositoryRoot $helper) -Algorithm SHA256).Hash -cne $diagnosticPlan.anchor.profilingInputs[$helper]) {
            throw "Replay helper bytes differ from the recorded profiling closure: $helper"
        }
    }
    Import-Module (Join-Path $repositoryRoot 'Tools/PerformanceProfiling.psm1') -Force
    Import-Module (Join-Path $repositoryRoot 'Tools/PerformancePolicy.psm1')
    $completion = Get-Content -LiteralPath (Join-Path $diagnosticDirectory 'pilot-completion.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
    if ($diagnosticPlan.reportBudget -ne 12 -or $diagnosticPlan.acceptance -cne 'not-established' -or
        $diagnosticPlan.schedule.Count -ne 12 -or $completion.scheduled -ne 12 -or $completion.attempted -ne 12 -or $completion.results.Count -ne 12) {
        throw 'The declared diagnostic inventory is incomplete.'
    }
    $reports = @(Get-ChildItem -LiteralPath $diagnosticDirectory -Filter '*.json' -File | Sort-Object Name)
    if ($reports.Count -ne 12) { throw 'Unexpected stamped diagnostic report inventory.' }
    $diagnostic = @(for ($index = 0; $index -lt 12; $index++) {
        $block = [int][Math]::Floor($index / 4)
        $slot = if ($block % 2) { 3 - ($index % 4) } else { $index % 4 }
        $scenario = if ($slot % 2) { 'MultilineGrid' } else { 'Default' }
        $heap = $slot -ge 2
        $name = 'profile-{0:d2}-{1}-{2}.json' -f ($index + 1), $scenario, $(if ($heap) { 'Heap' } else { 'NoHeap' })
        $item = $diagnosticPlan.schedule[$index]
        $result = $completion.results[$index]
        if ($item.block -ne $block -or $item.scenario -cne $scenario -or $item.heap -ne $heap -or
            $result.index -ne $index + 1 -or $result.scenario -cne $scenario -or $result.heap -ne $heap -or
            $result.report -cne $name -or $result.exitCode -ne 0 -or -not $result.identityMatched -or $result.identityError) {
            throw "Unexpected diagnostic schedule/completion: $name"
        }
        $r = Get-Content -LiteralPath (Join-Path $diagnosticDirectory $name) -Raw | ConvertFrom-Json -AsHashtable -Depth 32
        $state = [ordered]@{}
        foreach ($key in $diagnosticPlan.anchor.Keys) { $state[$key] = $r[$key] }
        Assert-PerformanceMeasurementState -Before $diagnosticPlan.anchor -After $state
        $companions = [ordered]@{ rawReportSha256=($name + '.raw.receipt.txt'); childMetadataSha256=($name + '.child.receipt.txt');
            logSha256=($name + '.log'); captureSha256=($name + '.png') }
        foreach ($key in $companions.Keys) {
            if ((Get-FileHash -LiteralPath (Join-Path $diagnosticDirectory $companions[$key]) -Algorithm SHA256).Hash -cne $r[$key]) {
                throw "Diagnostic companion hash mismatch: $name / $key"
            }
        }
        $child = Get-Content -LiteralPath (Join-Path $diagnosticDirectory $companions.childMetadataSha256) -Raw | ConvertFrom-Json -AsHashtable -Depth 8
        if ($child.exitCode -ne 0 -or $child.timedOut -or $child.terminationError -or $child.drainError -or $child.arguments.Count -ne 4 -or
            $child.arguments[0] -cne '--benchmark-complex-ui-profile' -or $child.arguments[2] -cne $scenario -or
            $child.arguments[3] -cne $(if ($heap) { 'On' } else { 'Off' })) { throw "Invalid child metadata: $name" }
        $raw = Get-Content -LiteralPath (Join-Path $diagnosticDirectory $companions.rawReportSha256) -Raw | ConvertFrom-Json -AsHashtable -Depth 32
        $completeness = Assert-ProfilingReport -Receipt $raw -Scenario $scenario -Heap $heap -Configuration Release -ProcessId $child.processId
        foreach ($key in $raw.Keys) {
            if ((ConvertTo-Json $raw[$key] -Depth 32 -Compress) -cne (ConvertTo-Json $r[$key] -Depth 32 -Compress)) {
                throw "Stamped diagnostic changed native data: $name / $key"
            }
        }
        $windows = @(foreach ($window in $r.windows) {
            $mainCpu = $null; $processCpu = $null; $mainCycles = $null; $processCycles = $null
            if ($window.cpuBefore.main.timesAvailable -and $window.cpuAfter.main.timesAvailable) {
                $mainCpu = ([double]$window.cpuAfter.main.kernel100ns + $window.cpuAfter.main.user100ns -
                    $window.cpuBefore.main.kernel100ns - $window.cpuBefore.main.user100ns) / 10000
            }
            if ($window.cpuBefore.process.timesAvailable -and $window.cpuAfter.process.timesAvailable) {
                $processCpu = ([double]$window.cpuAfter.process.kernel100ns + $window.cpuAfter.process.user100ns -
                    $window.cpuBefore.process.kernel100ns - $window.cpuBefore.process.user100ns) / 10000
            }
            if ($window.cpuBefore.main.cyclesAvailable -and $window.cpuAfter.main.cyclesAvailable) {
                $mainCycles = [decimal]$window.cpuAfter.main.cycles - [decimal]$window.cpuBefore.main.cycles
            }
            if ($window.cpuBefore.process.cyclesAvailable -and $window.cpuAfter.process.cyclesAvailable) {
                $processCycles = [decimal]$window.cpuAfter.process.cycles - [decimal]$window.cpuBefore.process.cycles
            }
            $frameTotal = ($window.frames.completedMs | Measure-Object -Sum).Sum
            [ordered]@{ window=$window.window; frames=$window.frames.Count; wallMs=$window.wallMs;
                completedFps=($window.frames.Count * 1000 / $frameTotal); medianCompletedMs=(Get-Median $window.frames.completedMs);
                minimumCompletedMs=($window.frames.completedMs | Measure-Object -Minimum).Minimum;
                maximumCompletedMs=($window.frames.completedMs | Measure-Object -Maximum).Maximum;
                medianPrepareMs=(Get-Median $window.frames.prepareMs); medianCompositeMs=(Get-Median $window.frames.compositeMs);
                mainCpuMs=$mainCpu; processCpuMs=$processCpu; mainCycleDelta=$mainCycles; processCycleDelta=$processCycles;
                processMinusMainReadOrderEstimateMs=$(if ($null -ne $mainCpu -and $null -ne $processCpu) { $processCpu - $mainCpu } else { $null }) }
        })
        $stages = @(foreach ($stage in $r.stages) {
            $heapSummary = $null
            if ($heap) {
                $heapSummary = [ordered]@{ complete=$stage.heapSampler.complete; count=$stage.heapSampler.heaps.Count;
                    busyBytes=($stage.heapSampler.heaps.busyBytes | Measure-Object -Sum).Sum;
                    freeBytes=($stage.heapSampler.heaps.freeBytes | Measure-Object -Sum).Sum;
                    regionCommittedBytes=($stage.heapSampler.heaps.regionCommittedBytes | Measure-Object -Sum).Sum;
                    errors=@($stage.heapSampler.heaps | Where-Object error -ne 0).Count }
            }
            [ordered]@{ stage=$stage.stage; completedFrames=$stage.completedFrames; elapsedMs=$stage.elapsedMs; samplingMs=$stage.samplingMs;
                heapSamplingMs=$stage.heapSampler.durationMs; process=$stage.process; cpu=$stage.cpu;
                observedThreads=$stage.scheduling.threads.Count; threadError=$stage.scheduling.threadError; grid=$stage.grid; view=$stage.view; heap=$heapSummary }
        })
        [ordered]@{ file=$name; scenario=$scenario; heap=$heap; telemetryCompleteness=$completeness;
            fixedDiagnosticStorageBytes=$r.fixedDiagnosticStorageBytes; windows=$windows; stages=$stages }
    })
    $analysis['diagnostic'] = $diagnostic
    $analysis['diagnosticWallSeconds'] = $completion.wallSeconds
    $analysis['limits'] = 'Instrumented and canonical workloads differ. Common sampling and heap-on can perturb later frames. CPU times are coarse sequential reads; raw cycle deltas are not durations or scheduling attribution. Heap accounting excludes non-heap virtual memory and component ownership. No acceptance, warm-up policy change or causation follows.'
}
$json = (ConvertTo-Json -InputObject $analysis -Depth 16) + "`n"
[IO.File]::WriteAllText((Join-Path $PacketRoot 'analysis.txt'), $json, [Text.UTF8Encoding]::new($false))
$diagnosticCount = if ($analysis.Contains('diagnostic')) { $analysis.diagnostic.Count } else { 0 }
Write-Host "Verified and analyzed $($baseline.Count) baseline and $diagnosticCount diagnostic reports. Diagnostic-only; no qualification verdict."
