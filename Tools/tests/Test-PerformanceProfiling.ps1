# Portable schema/runner fixtures only: no native rendering, build, tracing or qualification evidence.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$modulePath = Join-Path $repository 'Tools/PerformanceProfiling.psm1'
Import-Module $modulePath -Force
$shellPath = (Get-Process -Id $PID).Path

function Get-TestPngCrc([byte[]] $Bytes) {
    # Independent small-fixture reference, separate from the production BCL implementation.
    $crc=[uint32]4294967295
    foreach ($value in $Bytes) {
        $crc=[uint32]($crc -bxor [uint32]$value)
        for ($bit=0; $bit -lt 8; $bit++) { $crc=if ($crc -band 1) { [uint32](($crc -shr 1) -bxor [uint32]3988292384) } else { [uint32]($crc -shr 1) } }
    }
    return [uint32]($crc -bxor [uint32]4294967295)
}
function Get-TestPngUInt32([uint32] $Value) {
    $bytes=[BitConverter]::GetBytes($Value)
    if ([BitConverter]::IsLittleEndian) { [Array]::Reverse($bytes) }
    return ,$bytes
}
function New-TestPngChunk([string] $Kind, [byte[]] $Data) {
    $payload=[byte[]]([Text.Encoding]::ASCII.GetBytes($Kind) + $Data)
    return ,[byte[]]((Get-TestPngUInt32 $Data.Length) + $payload + (Get-TestPngUInt32 (Get-TestPngCrc $payload)))
}
function New-TestPng([int] $Width = 1280, [int] $Height = 720) {
    $ihdr=[byte[]]((Get-TestPngUInt32 $Width) + (Get-TestPngUInt32 $Height) + [byte[]]@(8,6,0,0,0))
    $memory=[IO.MemoryStream]::new()
    try {
        $compressor=[IO.Compression.ZLibStream]::new($memory,[IO.Compression.CompressionLevel]::Fastest,$true)
        try { $pixels=[byte[]]::new(($Width * 4 + 1) * $Height); $compressor.Write($pixels,0,$pixels.Length) } finally { $compressor.Dispose() }
        return ,[byte[]]([byte[]]@(137,80,78,71,13,10,26,10) + (New-TestPngChunk 'IHDR' $ihdr) + (New-TestPngChunk 'IDAT' $memory.ToArray()) + (New-TestPngChunk 'IEND' @()))
    } finally { $memory.Dispose() }
}
$syntheticPng=New-TestPng

function New-ProfilingCpu([int] $Value = 1000) {
    return [ordered]@{ timesAvailable=$true; timesError=0; cyclesAvailable=$true; cyclesError=0; kernel100ns=$Value; user100ns=$Value; cycles=$Value }
}
function New-ProfilingCounters([int] $Value = 1000) {
    return [ordered]@{ main=(New-ProfilingCpu $Value); process=(New-ProfilingCpu ($Value * 2)) }
}
function New-ProfilingReport([bool] $Heap = $false, [string] $Scenario = 'Default') {
    $windowNames = @('warm-1-20','warm-21-100','warm-101-200','clean-probe','dirty-probe')
    $counts = @(20,80,100,40,40); $first = @(0,20,100,200,200)
    $previousStage = @(2,3,4,6,7)
    $windows = @(for ($index = 0; $index -lt 5; $index++) {
        [ordered]@{ window=$windowNames[$index]; firstUpdate=$first[$index]; dirty=($index -ne 3); wallMs=($counts[$index] * 1.1);
            cpuBefore=(New-ProfilingCounters (1020 + $previousStage[$index] * 100)); cpuAfter=(New-ProfilingCounters (1040 + $previousStage[$index] * 100));
            frames=@(for ($frame = 0; $frame -lt $counts[$index]; $frame++) { [ordered]@{ completedMs=1.0; prepareMs=0.4; compositeMs=0.2; cppAllocations=0; compositeAllocations=0 } }) }
    })
    $stageNames = @('entry','device-and-target','scene','warm20','warm100','warm200','capture','clean-probe','dirty-probe','hidden','detached','released')
    $completed = @(0,0,0,20,100,200,200,240,280,280,280,280)
    $stages = @(for ($index = 0; $index -lt 12; $index++) {
        $sampler = [ordered]@{ enabled=$Heap; complete=$true; heapLimit=128; durationMs=$(if ($Heap) { 1.0 } else { 0.0 });
            inventoryError=0; heapCountHint=$(if ($Heap) { 1 } else { 0 }); jsonBytes=$(if ($Heap) { 128 } else { 0 }); jsonCapacityBytes=256 }
        if ($Heap) { $sampler['heaps'] = @([ordered]@{ heap=42; busyBytes=1024; freeBytes=128; entryOverheadBytes=16; regionCommittedBytes=4096; regionUncommittedBytes=0; error=0 }) }
        [ordered]@{ stage=$stageNames[$index]; completedFrames=$completed[$index]; elapsedMs=($index * 100.0); samplingMs=2.0;
            cpu=(New-ProfilingCounters (1000 + $index * 100)); process=[ordered]@{ privateBytes=1024; workingSetBytes=2048; peakWorkingSetBytes=3072; handles=10; handlesAvailable=$true; handlesError=0 };
            scheduling=[ordered]@{ priorityAvailable=$true; priorityError=0; priorityClass=32; affinityAvailable=$true; affinityError=0; groupsAvailable=$true; groupsError=0;
                processMask='0000000000000003'; systemMask='0000000000000003'; groups=@(0); threadLimit=256; threadScanLimit=65536; threadsScanned=10; threadError=0;
                threads=@([ordered]@{ id=17; creation100ns=12345; handleError=0; ownerAvailable=$true; ownerError=0; ownerProcessId=42;
                    creationAvailable=$true; creationError=0; priorityAvailable=$true; priorityError=0; affinityAvailable=$true; affinityError=0;
                    priority=0; group=0; mask='0000000000000003'; cpu=(New-ProfilingCpu (1001 + $index * 100)) }) };
            grid=[ordered]@{ available=($index -ge 2 -and $index -le 9); lookups=10; hits=5; layoutCreations=5; shapedUnits=20; retainedLayouts=5; capacity=256; displayLayouts=0; displayCapacity=64; textUnits=20; tableBytes=512 };
            view=[ordered]@{ available=($index -ge 2 -and $index -le 10); preparations=$(if ($index -ge 3 -and $index -le 9) { [Math]::Min($completed[$index],240) } else { 0 });
                composites=$(if ($index -ge 3 -and $index -le 9) { $completed[$index] } else { 0 }); surfaceAllocations=$(if ($index -ge 3 -and $index -le 9) { 1 } else { 0 });
                surfaceBytes=$(if ($index -ge 3 -and $index -le 8) { 3686400 } else { 0 }); replacementPeakBytes=0 };
            heapSampler=$sampler }
    })
    return [ordered]@{ schema=1; fixture='dxui-complex-ui-profiling-v1'; classification='diagnostic-only'; renderer='WARP'; scenario=$Scenario; heapSampling=$Heap;
        compiler=194435217; iteratorDebugLevel=0; pointerBits=64; processId=42; mainThreadId=17; width=1280; height=720; dpi=96; controls=83; modelRows=1000;
        completedFrames=280; hiddenPreparations=0; hiddenComposites=0; telemetryCompleteness='complete'; fixedDiagnosticStorageBytes=500000;
        timingScope='Synthetic window includes update/prepare/composite/completion; sampling occurs outside.';
        schedulingScope='Synthetic availability and allowed constraints only; no scheduler trace.';
        heapScope='Synthetic bounded inventory only; no allocation-owner proof.'; storageScope='Synthetic fixed storage; string capacity reported separately.';
        clock=[ordered]@{ implementation='QueryPerformanceCounter'; ticksPerSecond=10000000; tickNanoseconds=100.0 }; windows=$windows; stages=$stages }
}
function Assert-Report([object] $Report, [bool] $Heap = $false, [string] $Scenario = 'Default') {
    return Assert-ProfilingReport -Receipt $Report -Scenario $Scenario -Heap $Heap -Configuration Release -ProcessId 42
}
function Set-UnavailableCpu($Cpu, [string] $Kind = 'both') {
    if ($Kind -in @('times','both')) { $Cpu.timesAvailable=$false; $Cpu.timesError=6; $Cpu.kernel100ns=0; $Cpu.user100ns=0 }
    if ($Kind -in @('cycles','both')) { $Cpu.cyclesAvailable=$false; $Cpu.cyclesError=6; $Cpu.cycles=0 }
}

Invoke-TestCase 'serialized native-like reports accept Int64 group/PID/counter values for both scenarios and heap modes' {
    foreach ($scenario in @('Default','MultilineGrid')) { foreach ($heap in @($false,$true)) {
        $report = New-ProfilingReport $heap $scenario | ConvertTo-Json -Depth 32 | ConvertFrom-Json -AsHashtable -Depth 32
        Assert-True ($report.stages[0].scheduling.groups[0] -is [long]) 'test reproduces actual JSON integer parsing'
        Assert-Equal 'complete' (Assert-Report $report $heap $scenario) 'parsed full report'
    } }
}
$invalid = @(
    @{Name='missing fixed phase'; Change={param($r) $r.stages=$r.stages[0..10]}},
    @{Name='wrong warm checkpoint'; Change={param($r) $r.stages[4].completedFrames=99}},
    @{Name='missing frame'; Change={param($r) $r.windows[1].frames=$r.windows[1].frames[0..78]}},
    @{Name='overflowing frame array'; Change={param($r) $r.windows[2].frames += $r.windows[2].frames[0]}},
    @{Name='nonfinite frame'; Change={param($r) $r.windows[0].frames[0].prepareMs=[double]::NaN}},
    @{Name='frame intervals disagree'; Change={param($r) $r.windows[0].frames[0].completedMs=0.1}},
    @{Name='window omits frame time'; Change={param($r) $r.windows[0].wallMs=0}},
    @{Name='clean frame allocates'; Change={param($r) $r.windows[3].frames[0].cppAllocations=1}},
    @{Name='composition allocates'; Change={param($r) $r.windows[4].frames[0].compositeAllocations=1}},
    @{Name='missing CPU availability'; Change={param($r) $r.stages[0].cpu.main.Remove('timesAvailable')}},
    @{Name='CPU availability/error disagreement'; Change={param($r) $r.stages[0].cpu.main.timesError=5}},
    @{Name='unavailable CPU fabricates observed value'; Change={param($r) Set-UnavailableCpu $r.stages[0].cpu.main 'cycles'; $r.stages[0].cpu.main.cycles=3; $r.telemetryCompleteness='partial'}},
    @{Name='decreasing window cumulative cycles'; Change={param($r) $r.windows[1].cpuAfter.process.cycles=1}},
    @{Name='decreasing counters across windows'; Change={param($r) $r.windows[1].cpuBefore.main.user100ns=1}},
    @{Name='warm window predates scene CPU'; Change={param($r) $r.windows[0].cpuBefore.main.cycles=1000}},
    @{Name='warm phase predates preceding window CPU'; Change={param($r) $r.stages[3].cpu.process.user100ns=2400}},
    @{Name='decreasing cumulative process CPU across phases'; Change={param($r) $r.stages[2].cpu.process.kernel100ns=1}},
    @{Name='decreasing live thread CPU for same creation identity'; Change={param($r) $r.stages[3].scheduling.threads[0].cpu.cycles=1}},
    @{Name='stage clock goes backward'; Change={param($r) $r.stages[3].elapsedMs=0}},
    @{Name='QPC units disagree'; Change={param($r) $r.clock.tickNanoseconds=1000}},
    @{Name='missing process scheduling query'; Change={param($r) $r.stages[0].scheduling.Remove('groupsError')}},
    @{Name='group outside valid range'; Change={param($r) $r.stages[0].scheduling.groups=@(64)}},
    @{Name='fractional group'; Change={param($r) $r.stages[0].scheduling.groups=@(0.5)}},
    @{Name='duplicate group'; Change={param($r) $r.stages[0].scheduling.groups=@(0,0)}},
    @{Name='undeclared missing thread inventory'; Change={param($r) $r.stages[0].scheduling.threads=@()}},
    @{Name='thread inventory exceeds storage'; Change={param($r) $r.stages[0].scheduling.threads=@(1..257 | ForEach-Object { Copy-JsonValue $r.stages[0].scheduling.threads[0] })}},
    @{Name='thread enumeration exceeds scan cap'; Change={param($r) $r.stages[0].scheduling.threadsScanned=65537}},
    @{Name='duplicate TID'; Change={param($r) $r.stages[0].scheduling.threads += $r.stages[0].scheduling.threads[0]}},
    @{Name='missing live-thread creation identity'; Change={param($r) $r.stages[0].scheduling.threads[0].creation100ns=0}},
    @{Name='missing opened thread owner query'; Change={param($r) $r.stages[0].scheduling.threads[0].Remove('ownerAvailable')}},
    @{Name='foreign thread falsely treated as owned'; Change={param($r) $r.stages[0].scheduling.threads[0].ownerProcessId=43}},
    @{Name='opened thread identity omitted'; Change={param($r) $r.stages[0].scheduling.threads[0].Remove('creationAvailable')}},
    @{Name='handle failure claims queried CPU'; Change={param($r) $r.stages[0].scheduling.threads[0].handleError=6; $r.telemetryCompleteness='partial'}},
    @{Name='partial query claims complete telemetry'; Change={param($r) Set-UnavailableCpu $r.stages[0].cpu.main}},
    @{Name='complete telemetry mislabeled partial'; Change={param($r) $r.telemetryCompleteness='partial'}},
    @{Name='missing observed view resource'; Change={param($r) $r.stages[4].view.Remove('surfaceAllocations')}},
    @{Name='hidden view retains surface'; Change={param($r) $r.stages[9].view.surfaceBytes=3686400}},
    @{Name='detached view retains surface'; Change={param($r) $r.stages[10].view.surfaceBytes=1}},
    @{Name='missing storage scope'; Change={param($r) $r.Remove('storageScope')}},
    @{Name='heap off invents traversal'; Change={param($r) $r.stages[0].heapSampler.durationMs=0.1}},
    @{Name='wrong process PID'; Change={param($r) $r.processId=43}},
    @{Name='heap output exceeds byte budget'; Change={param($r) $r.stages[0].heapSampler.jsonBytes=65537; $r.stages[0].heapSampler.jsonCapacityBytes=65537}}
    @{Name='CPU exceeds UInt64 domain'; Change={param($r) $r.stages[0].cpu.main.cycles=1e300}},
    @{Name='creation time exceeds UInt64 domain'; Change={param($r) $r.stages[0].scheduling.threads[0].creation100ns=1e300}},
    @{Name='rounded double at two-to-64 boundary'; Change={param($r) $r.stages[0].cpu.main.cycles=[Math]::Pow(2,64)}},
    @{Name='thread priority exceeds Int32 domain'; Change={param($r) $r.stages[0].scheduling.threads[0].priority=2147483648}}
)
foreach ($case in $invalid) {
    Invoke-TestCase ("reject $($case.Name)") {
        $report=New-ProfilingReport
        & $case.Change $report
        Assert-Throws { Assert-Report $report } $case.Name
    }
}
foreach ($kind in @('times','cycles')) {
    Invoke-TestCase ("retain explicitly unavailable $kind as partial, without comparing unobserved zeros") {
        $report=New-ProfilingReport; $report.telemetryCompleteness='partial'
        Set-UnavailableCpu $report.windows[1].cpuAfter.main $kind
        Set-UnavailableCpu $report.stages[4].cpu.process $kind
        Assert-Equal 'partial' (Assert-Report $report) 'honest API failure is retained'
    }
}
Invoke-TestCase 'retain disappearing thread with every unqueried field explicitly unavailable' {
    $report=New-ProfilingReport; $report.telemetryCompleteness='partial'
    $thread=$report.stages[4].scheduling.threads[0]
    $thread.handleError=87; $thread.creationAvailable=$false; $thread.creationError=6; $thread.creation100ns=0
    $thread.ownerAvailable=$false; $thread.ownerError=6; $thread.ownerProcessId=0
    $thread.priorityAvailable=$false; $thread.priorityError=6; $thread.priority=0
    $thread.affinityAvailable=$false; $thread.affinityError=6; $thread.group=0; $thread.mask='0000000000000000'
    Set-UnavailableCpu $thread.cpu
    Assert-Equal 'partial' (Assert-Report $report) 'exit race is explicit'
}
Invoke-TestCase 'foreign opened thread is retained as partial and every subsequent query stays unavailable' {
    $report=New-ProfilingReport; $report.telemetryCompleteness='partial'
    $worker=Copy-JsonValue $report.stages[4].scheduling.threads[0]
    $worker.id=18; $worker.ownerProcessId=43; $worker.handleError=1307
    $worker.creationAvailable=$false; $worker.creationError=6; $worker.creation100ns=0
    $worker.priorityAvailable=$false; $worker.priorityError=6; $worker.priority=0
    $worker.affinityAvailable=$false; $worker.affinityError=6; $worker.group=0; $worker.mask='0000000000000000'
    Set-UnavailableCpu $worker.cpu
    $report.stages[4].scheduling.threads += $worker
    Assert-Equal 'partial' (Assert-Report $report) 'foreign thread was not sampled as owned'
}
Invoke-TestCase 'retain failed priority/affinity/group/handle queries independently' {
    $report=New-ProfilingReport; $report.telemetryCompleteness='partial'
    $stage=$report.stages[4]; $stage.process.handlesAvailable=$false; $stage.process.handlesError=5; $stage.process.handles=0
    $stage.scheduling.priorityAvailable=$false; $stage.scheduling.priorityError=5; $stage.scheduling.priorityClass=0
    $stage.scheduling.affinityAvailable=$false; $stage.scheduling.affinityError=5; $stage.scheduling.processMask=$stage.scheduling.systemMask='0000000000000000'
    $stage.scheduling.groupsAvailable=$false; $stage.scheduling.groupsError=5; $stage.scheduling.groups=@()
    Assert-Equal 'partial' (Assert-Report $report) 'unavailable constraints are not invented zeros'
}
Invoke-TestCase 'declared thread scan overflow retains partial bounded evidence' {
    $report=New-ProfilingReport; $report.telemetryCompleteness='partial'
    $report.stages[4].scheduling.threadError=234; $report.stages[4].scheduling.threadsScanned=65536
    Assert-Equal 'partial' (Assert-Report $report) 'bounded overflow is declared'
}
Invoke-TestCase 'declared failed thread snapshot retains explicit empty inventory' {
    $report=New-ProfilingReport; $report.telemetryCompleteness='partial'
    $report.stages[4].scheduling.threadError=5; $report.stages[4].scheduling.threadsScanned=0; $report.stages[4].scheduling.threads=@()
    Assert-Equal 'partial' (Assert-Report $report) 'snapshot failure is retained'
}
Invoke-TestCase 'different creation identity permits reused worker TID counters to restart' {
    $report=New-ProfilingReport
    foreach ($stage in $report.stages) { $worker=Copy-JsonValue $stage.scheduling.threads[0]; $worker.id=18; $stage.scheduling.threads += $worker }
    $report.stages[4].scheduling.threads[1].creation100ns=54321; $report.stages[4].scheduling.threads[1].cpu=New-ProfilingCpu 1
    Assert-Equal 'complete' (Assert-Report $report) 'CPU identity includes creation time'
}
foreach ($case in @(
    @{Name='missing heap inventory'; Change={param($r) $r.stages[0].heapSampler.Remove('heaps')}},
    @{Name='overflowing heap inventory'; Change={param($r) $r.stages[0].heapSampler.heaps=@(1..129 | ForEach-Object { $r.stages[0].heapSampler.heaps[0] })}},
    @{Name='silently failed heap walk'; Change={param($r) $r.stages[0].heapSampler.heaps[0].error=5}},
    @{Name='silently overflowing heap hint'; Change={param($r) $r.stages[0].heapSampler.heapCountHint=129}},
    @{Name='duplicate heap identity'; Change={param($r) $r.stages[0].heapSampler.heaps += $r.stages[0].heapSampler.heaps[0]}},
    @{Name='heap interval exceeds stage'; Change={param($r) $r.stages[0].heapSampler.durationMs=3}},
    @{Name='heap JSON capacity smaller than size'; Change={param($r) $r.stages[0].heapSampler.jsonCapacityBytes=1}}
    @{Name='heap bytes exceed UInt64 domain'; Change={param($r) $r.stages[0].heapSampler.heaps[0].busyBytes=1e300}}
)) {
    Invoke-TestCase ("reject $($case.Name)") { $report=New-ProfilingReport $true; & $case.Change $report; Assert-Throws { Assert-Report $report $true } $case.Name }
}
Invoke-TestCase 'retain declared heap walk failure as partial with complete=false' {
    $report=New-ProfilingReport $true; $report.telemetryCompleteness='partial'
    $report.stages[0].heapSampler.heaps[0].error=5; $report.stages[0].heapSampler.complete=$false
    Assert-Equal 'partial' (Assert-Report $report $true) 'lock/walk failure is not labeled complete'
}
Invoke-TestCase 'retain bounded heap enumeration overflow without walking unknown handles' {
    $report=New-ProfilingReport $true; $report.telemetryCompleteness='partial'
    $sampler=$report.stages[0].heapSampler; $sampler.complete=$false; $sampler.inventoryError=234; $sampler.heapCountHint=129; $sampler.heaps=@()
    Assert-Equal 'partial' (Assert-Report $report $true) 'explicit overflow yields no sampled handles'
}
Invoke-TestCase 'UInt64 integer maximum remains an exact supported numeric boundary' {
    $report=New-ProfilingReport $true; $report.stages[0].heapSampler.heaps[0].busyBytes=[uint64]::MaxValue
    Assert-Equal 'complete' (Assert-Report $report $true) 'exact native UInt64 bound'
}

$pngModule=Get-Module PerformanceProfiling
Invoke-FixtureCase 'complete synthetic PNG structure and independent CRC reference are accepted' {
    param($root)
    $vector=[Text.Encoding]::ASCII.GetBytes('123456789')
    $actual=& $pngModule { param($bytes) Get-ProfilingPngCrc $bytes 0 $bytes.Length } $vector
    Assert-Equal ([uint32]3421780262) $actual 'standard IEEE CRC32 vector'
    Set-FixtureBytes $root 'valid.png' $syntheticPng
    $hash=& $pngModule { param($path) Assert-ProfilingPng $path } (Join-Path $root 'valid.png')
    Assert-Equal (Get-FileHash (Join-Path $root 'valid.png')).Hash $hash 'hash binds the validated byte buffer'
}
foreach ($mode in @('signature-only','wrong-width','truncated','no-IDAT','trailing','bad-CRC','unknown-critical','chunk-overflow')) {
    Invoke-FixtureCase ("reject PNG $mode without claiming decoded/visual proof") {
        param($root)
        $bytes=[byte[]]$syntheticPng.Clone()
        switch ($mode) {
            'signature-only' { $bytes=$bytes[0..7] }
            'wrong-width' { $bytes=New-TestPng 1281 720 }
            'truncated' { $bytes=$bytes[0..($bytes.Length-7)] }
            'no-IDAT' { $bytes=[byte[]]($bytes[0..32] + (New-TestPngChunk 'tEXt' ([Text.Encoding]::ASCII.GetBytes("test`0synthetic fixture"))) + (New-TestPngChunk 'IEND' @())) }
            'trailing' { $bytes=[byte[]]($bytes + [byte]0) }
            'bad-CRC' { $bytes[41]=$bytes[41] -bxor 1 }
            'unknown-critical' { $bytes=[byte[]]($bytes[0..32] + (New-TestPngChunk 'DxUI' @()) + $bytes[33..($bytes.Length-1)]) }
            'chunk-overflow' {
                $chunks=[byte[]]::new(0)
                for ($index=0; $index -lt 257; $index++) { $chunks += New-TestPngChunk 'tEXt' @() }
                $bytes=[byte[]]($bytes[0..32] + $chunks + $bytes[33..($bytes.Length-1)])
            }
        }
        Set-FixtureBytes $root 'invalid.png' $bytes
        Assert-Throws { & $pngModule { param($path) Assert-ProfilingPng $path } (Join-Path $root 'invalid.png') } $mode
    }
}

# Shims are deliberately synthetic unit evidence; real v2 receipt/build correctness is covered by
# PerformancePolicy tests and subsequent coordinated native runs, never established by this fixture.
function New-ProfilingRunnerFixture([string] $Root, [string] $Mode = 'valid', [bool] $Heap = $false) {
    foreach ($name in @('Tests/Embedded/Scope.Tests.ComplexUiProfiling.h','Tests/Embedded/Embedded.Tests.Embedded.cpp',
            'Tests/Embedded/Embedded.Tests.ComplexUiBenchmark.h','Tests/Support/Support.Tests.HeapDiagnostic.h',
            'Samples/ComplexUi/ComplexUiScene.h','Samples/EmbeddedControls/GraphicsFixture.h','Tools/ScopedTesting.psm1',
            'src/native.cpp','deps/header.h','.build/x64/Release/DxUi.EmbeddedTests.exe')) { Set-FixtureFile $Root $name "synthetic $name" }
    Set-FixtureFile $Root 'mode.txt' $Mode
    Set-FixtureFile $Root 'native.json' ((ConvertTo-Json -InputObject (New-ProfilingReport $Heap) -Depth 32 -Compress) + "`r`n  ")
    Set-FixtureBytes $Root 'capture.png' $syntheticPng
    Set-FixtureFile $Root 'performance-profile.ps1' ([IO.File]::ReadAllText((Join-Path $repository 'performance-profile.ps1')))
    Set-FixtureFile $Root 'build.ps1' 'param($Platform,$Configuration) Add-Content (Join-Path $PSScriptRoot "calls.txt") "build"; $global:LASTEXITCODE=0'
    Set-FixtureFile $Root 'Tools/PerformancePolicy.psm1' @'
Set-StrictMode -Version Latest
$script:root=Split-Path -Parent $PSScriptRoot
$script:calls=0
function Get-PerformanceMeasurementState {
    param($Root,$Platform,$Configuration)
    $script:calls++
    Add-Content (Join-Path $Root 'calls.txt') "state:$script:calls"
    if ($script:calls -eq 2) {
        $mode=[IO.File]::ReadAllText((Join-Path $Root 'mode.txt'))
        if ($mode -eq 'raw-drift') { Add-Content ([IO.File]::ReadAllText((Join-Path $Root 'output-path.txt'))) ' ' }
        if ($mode -eq 'capture-drift') { Add-Content (([IO.File]::ReadAllText((Join-Path $Root 'output-path.txt')))+'.png') ' ' }
    }
    return [ordered]@{ buildIdentityVersion=2; buildIdentityScope='full'; buildIdentityStatus='verifiable';
        sourceIdentity=(Get-FileHash (Join-Path $Root 'src/native.cpp')).Hash;
        dependencyIdentity=(Get-FileHash (Join-Path $Root 'deps/header.h')).Hash;
        artifactIdentity=(Get-FileHash (Join-Path $Root '.build/x64/Release/DxUi.EmbeddedTests.exe')).Hash }
}
function Assert-PerformanceBuildAttestation {
    param($Root,$Platform,$Configuration,$State)
    Add-Content (Join-Path $Root 'calls.txt') 'attest'
    $mode=[IO.File]::ReadAllText((Join-Path $Root 'mode.txt'))
    if ($mode -eq 'stale-build' -or ($mode -eq 'after-attestation-failure' -and $script:calls -eq 2)) { throw 'synthetic rejected stale build attestation' }
}
function Assert-PerformanceMeasurementState {
    param($Before,$After)
    if ((ConvertTo-Json $Before -Depth 32 -Compress) -cne (ConvertTo-Json $After -Depth 32 -Compress)) { throw 'synthetic changed measurement identity' }
}
Export-ModuleMember -Function Get-PerformanceMeasurementState,Assert-PerformanceBuildAttestation,Assert-PerformanceMeasurementState
'@
    $mock = @'
function Invoke-OwnedProfilingProcess {
    param($Executable,$Arguments,$WorkingDirectory)
    Add-Content (Join-Path $WorkingDirectory 'calls.txt') 'child'
    $mode=[IO.File]::ReadAllText((Join-Path $WorkingDirectory 'mode.txt'))
    [IO.File]::WriteAllText((Join-Path $WorkingDirectory 'output-path.txt'),$Arguments[1])
    if ($mode -ne 'missing-output') {
        [IO.File]::WriteAllBytes($Arguments[1],[IO.File]::ReadAllBytes((Join-Path $WorkingDirectory 'native.json')))
        if ($mode -ne 'missing-capture') { [IO.File]::WriteAllBytes(($Arguments[1]+'.png'),[IO.File]::ReadAllBytes((Join-Path $WorkingDirectory 'capture.png'))) }
        if ($mode -eq 'malformed-output') { [IO.File]::WriteAllText($Arguments[1],'{malformed') }
        if ($mode -eq 'partial-report' -or $mode -eq 'wrong-native-pid') {
            $report=[IO.File]::ReadAllText($Arguments[1]) | ConvertFrom-Json -AsHashtable -Depth 32
            if ($mode -eq 'partial-report') { $report.stages=$report.stages[0..10] } else { $report.processId=43 }
            [IO.File]::WriteAllText($Arguments[1],(ConvertTo-Json $report -Depth 32))
        }
    }
    $mutations=@{ 'source-drift'='src/native.cpp'; 'dependency-drift'='deps/header.h'; 'artifact-drift'='.build/x64/Release/DxUi.EmbeddedTests.exe';
        'fixture-drift'='Tests/Embedded/Scope.Tests.ComplexUiProfiling.h'; 'dispatcher-drift'='Tests/Embedded/Embedded.Tests.Embedded.cpp';
        'runner-drift'='performance-profile.ps1'; 'helper-drift'='Tests/Support/Support.Tests.HeapDiagnostic.h' }
    if ($mutations.ContainsKey($mode)) { Add-Content (Join-Path $WorkingDirectory $mutations[$mode]) 'mutated during child' }
    return [ordered]@{ processId=42; executable=$Executable; arguments=@($Arguments); workingDirectory=$WorkingDirectory;
        startedUtc='2026-10-10T00:00:00Z'; completedUtc='2026-10-10T00:00:01Z'; timeoutSeconds=120; drainTimeoutSeconds=5;
        exitCode=$(if ($mode -eq 'exit-failure') { 7 } else { 0 }); timedOut=($mode -eq 'timeout');
        terminationError=''; drainError=$(if ($mode -eq 'drain-failure') { 'synthetic held pipe' } else { '' }); stdout='synthetic stdout'; stderr='synthetic stderr' }
}
'@
    Set-FixtureFile $Root 'Tools/PerformanceProfiling.psm1' ([IO.File]::ReadAllText($modulePath) + "`n" + $mock)
    Import-Module (Join-Path $Root 'Tools/PerformanceProfiling.psm1') -Force
    return Join-Path $Root '.build/reports/profile.json'
}
Invoke-FixtureCase 'guarded valid run binds closure and retains exact raw bytes including whitespace' {
    param($root)
    $output=New-ProfilingRunnerFixture $root
    Invoke-ProfilingMeasurement -Root $root -Platform x64 -Configuration Release -Scenario Default -Heap $false -OutputPath $output
    $receipt=[IO.File]::ReadAllText($output) | ConvertFrom-Json -AsHashtable -Depth 32
    Assert-Equal 'diagnostic-only' $receipt.classification 'no acceptance verdict'
    Assert-Equal 'complete' $receipt.telemetryCompleteness 'complete unit telemetry'
    Assert-Equal 10 $receipt.profilingInputs.Count 'fixture/helper/dispatcher/runner closure'
    Assert-Equal ([Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $root 'native.json')))) ([Convert]::ToBase64String([IO.File]::ReadAllBytes(($output+'.raw.json')))) 'exact raw bytes'
    Assert-Equal (Get-FileHash ($output+'.raw.json')).Hash $receipt.rawReportSha256 'raw hash'
    Assert-Equal (Get-FileHash ($output+'.child.json')).Hash $receipt.childMetadataSha256 'owned child metadata hash'
    Assert-Equal 'state:1|attest|child|state:2|attest' ((Get-Content (Join-Path $root 'calls.txt')) -join '|') 'attestation brackets child'
}
Invoke-FixtureCase 'guarded partial telemetry remains diagnostic-only and provenance-bound' {
    param($root)
    $output=New-ProfilingRunnerFixture $root
    $report=New-ProfilingReport; $report.telemetryCompleteness='partial'; Set-UnavailableCpu $report.stages[4].cpu.main
    Set-FixtureJson $root 'native.json' $report
    Invoke-ProfilingMeasurement -Root $root -Platform x64 -Configuration Release -Scenario Default -Heap $false -OutputPath $output
    Assert-Equal 'partial' (Get-FixtureJson $root '.build/reports/profile.json').telemetryCompleteness 'explicit API unavailable receipt retained'
}
Invoke-FixtureCase 'direct stale-build refusal revokes report/raw and all previous sidecars before child launch' {
    param($root)
    $output=New-ProfilingRunnerFixture $root 'stale-build'
    foreach ($suffix in @('','.raw.json','.png','.log','.child.json')) { Set-FixtureFile $root ('.build/reports/profile.json'+$suffix) 'stale previous artifact' }
    Assert-Throws { Invoke-ProfilingMeasurement -Root $root -Platform x64 -Configuration Release -Scenario Default -Heap $false -OutputPath $output } 'stale build'
    foreach ($suffix in @('','.raw.json','.png','.log','.child.json')) { Assert-True (-not (Test-Path -LiteralPath ($output+$suffix))) 'no stale output retains its valid-looking name' }
    Assert-True (-not ((Get-Content (Join-Path $root 'calls.txt')) -contains 'child')) 'no child on stale build'
    Assert-Equal 5 @(Get-ChildItem (Split-Path $output) -Filter '*.stale').Count 'old artifacts preserved quarantined'
}
foreach ($mode in @('source-drift','dependency-drift','artifact-drift','fixture-drift','dispatcher-drift','runner-drift','helper-drift','raw-drift','capture-drift','after-attestation-failure','exit-failure','timeout','drain-failure','missing-output','missing-capture','malformed-output','partial-report','wrong-native-pid')) {
    Invoke-FixtureCase ("guard refuses $mode and retains child metadata/log while revoking native/raw/capture") {
        param($root)
        $output=New-ProfilingRunnerFixture $root $mode
        Assert-Throws { Invoke-ProfilingMeasurement -Root $root -Platform x64 -Configuration Release -Scenario Default -Heap $false -OutputPath $output } $mode
        foreach ($suffix in @('','.raw.json','.png')) { Assert-True (-not (Test-Path -LiteralPath ($output+$suffix))) 'no success artifact after failure' }
        Assert-True (Test-Path -LiteralPath ($output+'.child.json')) 'child PID/arguments/exit/timeout metadata retained'
        Assert-True ([IO.File]::ReadAllText(($output+'.log')).Contains('synthetic stderr')) 'child diagnostics retained'
        $metadata=[IO.File]::ReadAllText(($output+'.child.json')) | ConvertFrom-Json -AsHashtable
        Assert-Equal 42 ([int]$metadata.processId) 'owned child PID'
        Assert-Equal '--benchmark-complex-ui-profile' $metadata.arguments[0] 'exact diagnostic flag'
        if ($mode -eq 'exit-failure') { Assert-Equal 7 ([int]$metadata.exitCode) 'actual child exit retained' }
        if ($mode -eq 'timeout') { Assert-True $metadata.timedOut 'timeout retained' }
    }
}
Invoke-FixtureCase 'public SkipBuild runner still verifies attestation and heap mode' {
    param($root)
    $output=New-ProfilingRunnerFixture $root 'valid' $true
    & (Join-Path $root 'performance-profile.ps1') -Configuration Release -SkipBuild -Heap -OutputPath $output
    $receipt=[IO.File]::ReadAllText($output) | ConvertFrom-Json -AsHashtable -Depth 32
    Assert-True $receipt.heapSampling 'public -Heap maps to On'
    Assert-True (-not ((Get-Content (Join-Path $root 'calls.txt')) -contains 'build')) 'SkipBuild skips only compilation'
    $metadata=[IO.File]::ReadAllText(($output+'.child.json')) | ConvertFrom-Json -AsHashtable
    Assert-Equal 'On' $metadata.arguments[3] 'owned diagnostic child mode'
}
Invoke-FixtureCase 'direct runner refuses output overwriting an owned source path' {
    param($root)
    $null=New-ProfilingRunnerFixture $root
    Assert-Throws { Invoke-ProfilingMeasurement -Root $root -Platform x64 -Configuration Release -Scenario Default -Heap $false -OutputPath (Join-Path $root 'src/owned.json') } 'output boundary'
    Assert-True (-not (Test-Path (Join-Path $root 'calls.txt'))) 'refusal precedes identity/process work'
}

# Real bounded PowerShell children exercise only process control; they are not native performance runs.
Import-Module $modulePath -Force
Invoke-FixtureCase 'owned child timeout terminates that child and preserves its exit/PID' {
    param($root)
    Set-FixtureFile $root 'wait.ps1' 'Start-Sleep -Seconds 10'
    $clock=[Diagnostics.Stopwatch]::StartNew()
    $child=Invoke-OwnedProfilingProcess -Executable $shellPath -Arguments @('-NoProfile','-File',(Join-Path $root 'wait.ps1')) -WorkingDirectory $root -TimeoutSeconds 0.2 -DrainTimeoutSeconds 0.2
    Assert-True $child.timedOut 'owned timeout observed'
    Assert-True ($child.processId -gt 0 -and $child.exitCode -ne 0) 'actual owned exit and PID retained'
    Assert-True ($clock.Elapsed.TotalSeconds -lt 8) 'wait and drain remain bounded'
    Assert-True (-not (Get-Process -Id $child.processId -ErrorAction SilentlyContinue)) 'owned child terminated'
}
Invoke-FixtureCase 'direct child exit with inherited pipe still has bounded drain and explicit error' {
    param($root)
    Set-FixtureFile $root 'holder.ps1' 'Start-Sleep -Seconds 3'
    Set-FixtureFile $root 'parent.ps1' @'
param($Shell,$Root)
$start=[Diagnostics.ProcessStartInfo]::new($Shell)
$start.UseShellExecute=$false; $start.CreateNoWindow=$true
foreach ($arg in @('-NoProfile','-File',(Join-Path $Root 'holder.ps1'))) { $start.ArgumentList.Add($arg) }
$child=[Diagnostics.Process]::Start($start)
[IO.File]::WriteAllText((Join-Path $Root 'holder.identity.json'),(ConvertTo-Json @{id=$child.Id; creationTicks=$child.StartTime.ToUniversalTime().Ticks}))
$stdout='emitted stdout prefix'; $stderr='emitted stderr prefix'
[Console]::Out.WriteLine($stdout); [Console]::Out.Flush()
[Console]::Error.WriteLine($stderr); [Console]::Error.Flush()
$child.Dispose()
'@
    try {
        $clock=[Diagnostics.Stopwatch]::StartNew()
        $child=Invoke-OwnedProfilingProcess -Executable $shellPath -Arguments @('-NoProfile','-File',(Join-Path $root 'parent.ps1'),$shellPath,$root) -WorkingDirectory $root -TimeoutSeconds 10 -DrainTimeoutSeconds 0.2
        Assert-True (-not $child.timedOut -and $child.exitCode -eq 0) 'direct child ended normally'
        Assert-True (-not [string]::IsNullOrEmpty($child.drainError)) 'retained descendant pipe declared incomplete'
        Assert-True $child.stdout.Contains('emitted stdout prefix') 'stdout prefix retained before pipe EOF'
        Assert-True $child.stderr.Contains('emitted stderr prefix') 'stderr prefix retained before pipe EOF'
        Assert-True ($clock.Elapsed.TotalSeconds -lt 3) 'drain does not wait for the descendant'
    } finally {
        if (Test-Path (Join-Path $root 'holder.identity.json')) {
            $identity=[IO.File]::ReadAllText((Join-Path $root 'holder.identity.json')) | ConvertFrom-Json -AsHashtable
            $holder=Get-Process -Id ([int]$identity.id) -ErrorAction SilentlyContinue
            if ($holder) {
                try {
                    $null=$holder.Handle # Retain the exact handle whose creation identity is checked.
                    if ($holder.StartTime.ToUniversalTime().Ticks -eq $identity.creationTicks) { $holder.Kill(); [void]$holder.WaitForExit(5000) }
                } finally { $holder.Dispose() }
            }
        }
    }
}
Invoke-FixtureCase 'timeout kills the original handle while a test-owned descendant survives and drains remain bounded' {
    param($root)
    Set-FixtureFile $root 'holder.ps1' 'Start-Sleep -Seconds 10'
    Set-FixtureFile $root 'parent.ps1' @'
param($Shell,$Root)
$start=[Diagnostics.ProcessStartInfo]::new($Shell)
$start.UseShellExecute=$false; $start.CreateNoWindow=$true
foreach ($arg in @('-NoProfile','-File',(Join-Path $Root 'holder.ps1'))) { $start.ArgumentList.Add($arg) }
$child=[Diagnostics.Process]::Start($start)
[IO.File]::WriteAllText((Join-Path $Root 'holder.identity.json'),(ConvertTo-Json @{id=$child.Id; creationTicks=$child.StartTime.ToUniversalTime().Ticks}))
$child.Dispose()
Start-Sleep -Seconds 10
'@
    $holder=$null
    try {
        $child=Invoke-OwnedProfilingProcess -Executable $shellPath -Arguments @('-NoProfile','-File',(Join-Path $root 'parent.ps1'),$shellPath,$root) -WorkingDirectory $root -TimeoutSeconds 2 -DrainTimeoutSeconds 0.2
        Assert-True $child.timedOut 'original child timeout observed'
        $identity=[IO.File]::ReadAllText((Join-Path $root 'holder.identity.json')) | ConvertFrom-Json -AsHashtable
        $holder=Get-Process -Id ([int]$identity.id) -ErrorAction SilentlyContinue
        Assert-True ($null -ne $holder) 'timeout never follows descendant parent IDs'
        $null=$holder.Handle
        Assert-Equal ([long]$identity.creationTicks) $holder.StartTime.ToUniversalTime().Ticks 'surviving helper identity'
        Assert-True (-not [string]::IsNullOrEmpty($child.drainError)) 'descendant-held pipe is an explicit bounded failure'
    } finally {
        if ($holder) {
            try { if (-not $holder.HasExited) { $holder.Kill(); [void]$holder.WaitForExit(5000) } } finally { $holder.Dispose() }
        }
    }
}
Invoke-FixtureCase 'incremental pipe storage retains a bounded prefix and declares overflow' {
    param($root)
    Set-FixtureFile $root 'output.ps1' "[Console]::Out.Write(('A' * 10000)); [Console]::Out.Flush()"
    $child=Invoke-OwnedProfilingProcess -Executable $shellPath -Arguments @('-NoProfile','-File',(Join-Path $root 'output.ps1')) -WorkingDirectory $root -TimeoutSeconds 5 -PipeCharacterLimit 1024
    Assert-Equal 1024 $child.stdout.Length 'bounded prefix length'
    Assert-True $child.stdoutTruncated 'overflow explicit'
    Assert-True $child.drainError.Contains('budget exceeded') 'overflow is non-success diagnostic metadata'
}
Complete-TestRun 'PerformanceProfiling'
