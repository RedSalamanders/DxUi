# Reproduce descriptive native/form/Grid evidence. No native program is built or executed.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$packet = $PSScriptRoot

function Read-Packet([string] $Name) {
    return [IO.File]::ReadAllText((Join-Path $packet $Name)) | ConvertFrom-Json -AsHashtable -Depth 40
}
function Require([bool] $Condition, [string] $Message) {
    if (-not $Condition) { throw $Message }
}
function Median([double[]] $Values) {
    Require ($Values.Count -gt 0) 'A descriptive statistic requires observations.'
    $valuesInOrder = @($Values | Sort-Object)
    $middle = [int][Math]::Floor($valuesInOrder.Count / 2.0)
    if ($valuesInOrder.Count % 2) { return [double]$valuesInOrder[$middle] }
    return ([double]$valuesInOrder[$middle - 1] + [double]$valuesInOrder[$middle]) / 2.0
}
function Stats([double[]] $Values) {
    return [ordered]@{n=$Values.Count;median=(Median $Values);minimum=($Values | Measure-Object -Minimum).Minimum;
        maximum=($Values | Measure-Object -Maximum).Maximum}
}

$forms = Read-Packet 'forms.json'
$raw = Read-Packet 'forms.raw.receipt.txt'
Require ((Get-FileHash -LiteralPath (Join-Path $packet 'forms.raw.receipt.txt')).Hash -ceq $forms.rawReportSha256) 'Native form bytes do not match their stamp.'
foreach ($key in $raw.Keys) {
    if ($key -eq 'identityScope') { continue } # The stamp retains the original under nativeIdentityScope.
    Require ((ConvertTo-Json -InputObject $raw[$key] -Compress -Depth 40) -ceq
        (ConvertTo-Json -InputObject $forms[$key] -Compress -Depth 40)) "Native form payload changed: $key"
}
Require ($forms.nativeIdentityScope -ceq $raw.identityScope) 'Original form identity scope was not retained.'
Require ($raw.blocks -eq 12 -and $raw.framesPerPhase -eq 40 -and $raw.fields -eq 8) 'Unexpected form workload.'
Require ($raw.profiles.Count -eq 2) 'Both form profiles are required.'
$formRows = @()
$retirement = @()
$cycles = @()
$frameCount = 0
$noOpFrames = 0
foreach ($profile in $raw.profiles) {
    Require ($profile.runs.Count -eq 48) 'Each profile requires four fresh runs per block.'
    foreach ($part in @('leafHierarchy','metadata','bounds','pixels')) {
        Require ($profile.preTimingParity[$part] -eq $true) "Form parity did not pass: $part"
    }
    Require ($profile.preTimingParity.states -eq 5) 'Five pre-timing parity states are required.'
    foreach ($variant in @('manual','form')) {
        $runs = @($profile.runs | Where-Object { $_.variant -ceq $variant })
        Require ($runs.Count -eq 24) 'Each form variant requires two runs per block.'
        foreach ($phase in @('unchanged','width','validation')) {
            $metricRows = [ordered]@{profile=$profile.name;variant=$variant;phase=$phase;metrics=[ordered]@{}}
            foreach ($metric in @('layoutAndPrepareMs','completedFrameMs')) {
                $blockCenters = @(for ($block=0; $block -lt 12; ++$block) {
                    $runCenters = @(foreach ($run in $runs) {
                        if ($run.block -ne $block) { continue }
                        $phases = @($run.phases | Where-Object { $_.phase -ceq $phase })
                        Require ($phases.Count -eq 1 -and $phases[0].frames.Count -eq 40) 'Invalid form phase inventory.'
                        Median @($phases[0].frames | ForEach-Object { $_[$metric] })
                    })
                    Require ($runCenters.Count -eq 2) 'A form block is missing a variant run.'
                    ($runCenters[0]+$runCenters[1])/2.0
                })
                $metricRows.metrics[$metric] = Stats $blockCenters
            }
            $formRows += $metricRows
            foreach ($run in $runs) {
                $phases = @($run.phases | Where-Object { $_.phase -ceq $phase })
                foreach ($frame in $phases[0].frames) {
                    ++$frameCount
                    Require ($frame.compositeCppAllocations -eq 0 -and $frame.compositeCppAllocationBytes -eq 0) 'Composition allocated tracked C++ memory.'
                    if ($phase -ceq 'unchanged') {
                        ++$noOpFrames
                        foreach ($counter in @('measures','preparations','surfaceAllocations','cppAllocations','cppAllocationBytes')) {
                            Require ($frame[$counter] -eq 0) "Unchanged work: $counter"
                        }
                    }
                }
            }
        }
        foreach ($run in $runs) { Require ($run.afterRetirement.plainCppLiveDeltaBytes -eq 0) 'A form run retained tracked plain C++ memory after destruction.' }
        $retirement += [ordered]@{profile=$profile.name;variant=$variant;runs=$runs.Count;
            constructionAllocations=(Stats @($runs.constructionCppAllocations));
            constructionRequestedBytes=(Stats @($runs.constructionCppAllocationBytes));
            warmPlainLiveBytes=(Stats @(foreach ($run in $runs) { ($run.stages | Where-Object { $_.stage -ceq 'afterWarmup' }).plainCppLiveDeltaBytes }));
            retiredPlainLiveBytes=(Stats @($runs.afterRetirement.plainCppLiveDeltaBytes))}
    }
    foreach ($cycle in $profile.lifecycle) {
        Require ($cycle.stages.Count -eq 10) 'Ten lifecycle observations are required.'
        $stages = @(foreach ($stage in $cycle.stages) {
            if ($stage.stage -in @('hidden','hiddenGenerationReplaced')) {
                Require ($stage.statistics.surfaceBytes -eq 0 -and $stage.cppAllocations -eq 0) 'Hidden view retained a surface or allocated tracked C++ memory.'
            }
            if ($stage.stage -in @('detached','destroyed')) {
                foreach ($counter in @('preparations','composites','surfaceBytes','surfaceAllocations','replacementPeakBytes','cachedBrushes','cachedTextFormats')) {
                    Require ($stage.statistics[$counter] -eq 0) "Detached view retained statistics: $counter"
                }
            }
            if ($stage.stage -ceq 'destroyed') { Require ($stage.plainCppLiveDeltaBytes -eq 0) 'Lifecycle retained tracked plain C++ memory after destruction.' }
            [ordered]@{stage=$stage.stage;measures=$stage.measures;cppAllocations=$stage.cppAllocations;
                plainCppLiveDeltaBytes=$stage.plainCppLiveDeltaBytes;statistics=$stage.statistics}
        })
        $cycles += [ordered]@{profile=$profile.name;variant=$cycle.variant;stages=$stages}
    }
}
Require ($frameCount -eq 11520 -and $noOpFrames -eq 3840) 'Unexpected measured form frame count.'

$gridRows = @(foreach ($scenario in @('MultilineGridRetention','MultilineGridHeap','MultilineGridHeapPaced')) {
    $report = Read-Packet ($scenario+'.json')
    Require ($report.buildIdentityVersion -eq 2 -and $report.identityStatus -ceq 'verifiable') 'Grid report lacks v2 provenance.'
    Require ($report.hiddenPreparations -eq 0 -and $report.hiddenComposites -eq 0) 'Grid performed hidden work.'
    $start = $report.retention[0]
    $last = @($report.retention | Where-Object { $_.phase -ceq 'model-cleared' })
    $scroll = @($report.retention | Where-Object { $_.phase -ceq 'scroll' })
    Require ($report.retention.Count -eq 32 -and $scroll.Count -eq 30 -and $last.Count -eq 1) 'Grid retention inventory is incomplete.'
    $heapRows = @(foreach ($sample in @($start,$last[0],$report.detached)) {
        if (-not $sample.Contains('heaps')) { continue }
        Require ($sample.heaps.Count -gt 0) 'An explicitly requested heap snapshot is empty.'
        foreach ($heap in $sample.heaps) { Require ($heap.error -eq 0) 'A heap snapshot is partial.' }
        [ordered]@{phase=$(if ($sample.Contains('phase')) {$sample.phase} else {'detached'});count=$sample.heaps.Count;
            busyBytes=($sample.heaps.busyBytes | Measure-Object -Sum).Sum;
            committedBytes=($sample.heaps.regionCommittedBytes | Measure-Object -Sum).Sum;
            freeBytes=($sample.heaps.freeBytes | Measure-Object -Sum).Sum}
    })
    foreach ($sample in $report.retention) {
        Require ($sample.handles -eq $start.handles -and $sample.surfaceBytes -eq 3686400) 'Grid retention changed handles or surface bytes.'
        if ($sample.Contains('heaps')) { foreach ($heap in $sample.heaps) { Require ($heap.error -eq 0) 'A scrolling heap snapshot is partial.' } }
    }
    [ordered]@{scenario=$scenario;clock=$report.clock;sourceFingerprint=$report.sourceFingerprint;executableSha256=$report.executableSha256;
        scrollSamples=$scroll.Count;elapsedMs=$last[0].elapsedMs;start=$start;modelCleared=$last[0];detached=$report.detached;
        scrollingPrivateBytes=(Stats @($scroll.privateBytes));scrollingWorkingSetBytes=(Stats @($scroll.workingSetBytes));heapStages=$heapRows}
})

$scopes = [IO.File]::ReadAllLines((Join-Path $packet 'native-scopes.txt'))
$testNames = [IO.File]::ReadAllLines((Join-Path $packet 'new-tests.txt'))
Require ($scopes.Count -eq 19 -and $testNames.Count -eq 38) 'Native proof inventory changed.'
$nativeRows = @(foreach ($configuration in @('Debug','Release','ASanDebug')) {
    foreach ($scope in $scopes) {
        $receipt = Read-Packet ($scope+'-'+$configuration+'.receipt.txt')
        Require ($receipt.exitCode -eq 0 -and $receipt.skips.Count -eq 0) 'Native suite failed or skipped.'
        [ordered]@{scope=$scope;configuration=$configuration;sha256=$receipt.sha256;exitCode=$receipt.exitCode;skips=$receipt.skips.Count}
    }
    $testLog = [IO.File]::ReadAllText((Join-Path $packet ('Control-'+$configuration+'.log'))) +
        [IO.File]::ReadAllText((Join-Path $packet ('Accessibility-'+$configuration+'.log')))
    foreach ($name in $testNames) { Require ($testLog.Contains('[DONE] '+$name+' (')) "New test did not complete: $configuration $name" }
})
$analysis = [ordered]@{classification='descriptive-only';formTimingMethod='Median of twelve block centers; each center averages its two variant run frame medians. Milliseconds. No inferential verdict.';
    formRawSha256=$forms.rawReportSha256;formMeasuredFrames=$frameCount;unchangedZeroWorkFrames=$noOpFrames;
    formTimings=$formRows;formMemory=$retirement;formLifecycle=$cycles;grid=$gridRows;nativeSuites=$nativeRows;
    newTestsPerConfiguration=$testNames.Count;limits='No paired regression/policy/consumer/native-ARM64/hardware qualification. C++ counters exclude COM/driver heaps; plain-live deltas exclude aligned blocks. Process snapshots and retained pools are not leak attribution.'}
[IO.File]::WriteAllText((Join-Path $packet 'analysis.txt'), (ConvertTo-Json -InputObject $analysis -Depth 40)+[Environment]::NewLine, [Text.UTF8Encoding]::new($false))
Write-Output 'Verified 57 native suite receipts, 38 new tests per profile, 11520 form frames and all three Grid diagnostics.'
