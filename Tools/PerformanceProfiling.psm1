# Separate diagnostic schema and owned-child runner. This module never compares acceptance metrics or policy.
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'PerformancePolicy.psm1') -Force

function Assert-ProfilingObject($Value, [string] $Context) {
    if ($Value -isnot [Collections.IDictionary]) { throw "Invalid profiling report: $Context must be an object." }
}
function Assert-ProfilingExact([Collections.IDictionary] $Value, [string] $Name, $Expected, [string] $Context) {
    if (-not $Value.Contains($Name) -or (ConvertTo-Json -InputObject $Value[$Name] -Compress) -cne (ConvertTo-Json -InputObject $Expected -Compress)) {
        throw "Invalid profiling report: $Context.$Name does not match the fixed fixture."
    }
}
function Assert-ProfilingNumber([Collections.IDictionary] $Value, [string] $Name, [string] $Context, [switch] $Integer, [switch] $Signed) {
    if (-not $Value.Contains($Name) -or $null -eq $Value[$Name] -or
        [Type]::GetTypeCode($Value[$Name].GetType()) -notin @('Byte','SByte','Int16','UInt16','Int32','UInt32','Int64','UInt64','Single','Double','Decimal')) {
        throw "Invalid profiling report: $Context.$Name must be numeric."
    }
    $number = [double]$Value[$Name]
    if (-not [double]::IsFinite($number) -or (-not $Signed -and $number -lt 0) -or ($Integer -and [Math]::Truncate($number) -ne $number)) {
        throw "Invalid profiling report: $Context.$Name is outside its numeric domain."
    }
    if ($Integer) {
        try { $exact = [decimal]$Value[$Name] } catch { throw "Invalid profiling report: $Context.$Name exceeds its native integer domain." }
        if (($Signed -and ($exact -lt [int]::MinValue -or $exact -gt [int]::MaxValue)) -or
            (-not $Signed -and ($exact -gt [decimal][uint64]::MaxValue -or
                ($Value[$Name] -is [double] -and $number -ge [Math]::Pow(2,64)) -or ($Value[$Name] -is [single] -and $number -ge [Math]::Pow(2,64))))) {
            throw "Invalid profiling report: $Context.$Name exceeds its native integer domain."
        }
    }
}
function Assert-ProfilingArray([Collections.IDictionary] $Value, [string] $Name, [int] $Count, [string] $Context) {
    if (-not $Value.Contains($Name) -or $Value[$Name] -isnot [array] -or $Value[$Name].Count -ne $Count) {
        throw "Invalid profiling report: $Context.$Name must contain exactly $Count entries."
    }
}
function Assert-ProfilingCpu($Value, [string] $Context) {
    Assert-ProfilingObject $Value $Context
    foreach ($key in @('kernel100ns','user100ns','cycles')) { Assert-ProfilingNumber $Value $key $Context -Integer }
    $times = Assert-ProfilingQuery $Value 'times' @('kernel100ns','user100ns') $Context
    $cycles = Assert-ProfilingQuery $Value 'cycles' @('cycles') $Context
    return $times -and $cycles
}
function Assert-ProfilingCounters($Value, [string] $Context) {
    Assert-ProfilingObject $Value $Context
    $complete = $true
    foreach ($kind in @('main','process')) { if (-not (Assert-ProfilingCpu $Value[$kind] "$Context.$kind")) { $complete = $false } }
    return $complete
}
function Assert-ProfilingQuery($Value, [string] $Prefix, [string[]] $Fields, [string] $Context) {
    $available = $Prefix + 'Available'; $error = $Prefix + 'Error'
    if (-not $Value.Contains($available) -or $Value[$available] -isnot [bool]) { throw "Invalid profiling report: $Context.$available must be explicit." }
    Assert-ProfilingNumber $Value $error $Context -Integer
    if ($Value[$error] -gt [uint32]::MaxValue -or $Value[$available] -ne ($Value[$error] -eq 0)) { throw "Invalid profiling report: $Context.$Prefix availability and error disagree." }
    if (-not $Value[$available]) {
        foreach ($key in $Fields) { Assert-ProfilingExact $Value $key 0 $Context }
    }
    return $Value[$available]
}
function Assert-ProfilingCpuHistory($Cpu, [Collections.IDictionary] $History, [string] $Identity, [string] $Context) {
    foreach ($key in @('kernel100ns','user100ns','cycles')) {
        $available = if ($key -eq 'cycles') { 'cyclesAvailable' } else { 'timesAvailable' }
        if (-not $Cpu[$available]) { continue }
        $historyKey = "$Identity/$key"
        if ($History.Contains($historyKey) -and $Cpu[$key] -lt $History[$historyKey]) { throw "Invalid profiling report: $Context.$key decreased for the same CPU identity." }
        $History[$historyKey] = $Cpu[$key]
    }
}
function Assert-ProfilingReport {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Receipt, [ValidateSet('Default','MultilineGrid')][string] $Scenario,
        [bool] $Heap, [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Release', [int] $ProcessId)
    Assert-ProfilingObject $Receipt 'report'
    foreach ($pair in @(@('schema',1),@('fixture','dxui-complex-ui-profiling-v1'),@('classification','diagnostic-only'),@('renderer','WARP'),
            @('scenario',$Scenario),@('heapSampling',$Heap),@('pointerBits',64),@('width',1280),@('height',720),@('dpi',96),
            @('controls',83),@('modelRows',1000),@('completedFrames',280),@('hiddenPreparations',0),@('hiddenComposites',0),
            @('iteratorDebugLevel',$(if ($Configuration -eq 'Release') { 0 } else { 2 })))) {
        Assert-ProfilingExact $Receipt $pair[0] $pair[1] 'report'
    }
    foreach ($key in @('compiler','processId','mainThreadId')) {
        Assert-ProfilingNumber $Receipt $key 'report' -Integer
        if ($Receipt[$key] -le 0) { throw "Invalid profiling report: report.$key must be positive." }
    }
    if ($ProcessId -gt 0) { Assert-ProfilingExact $Receipt 'processId' $ProcessId 'report' }
    foreach ($key in @('timingScope','schedulingScope','heapScope','storageScope')) {
        if ($Receipt[$key] -isnot [string] -or [string]::IsNullOrWhiteSpace($Receipt[$key])) { throw "Invalid profiling report: report.$key is absent." }
    }
    Assert-ProfilingNumber $Receipt 'fixedDiagnosticStorageBytes' 'report' -Integer
    if ($Receipt['fixedDiagnosticStorageBytes'] -le 0) { throw 'Invalid profiling report: fixed diagnostic storage is absent.' }
    $complete = $true
    Assert-ProfilingObject $Receipt['clock'] 'report.clock'
    Assert-ProfilingExact $Receipt['clock'] 'implementation' 'QueryPerformanceCounter' 'report.clock'
    foreach ($key in @('ticksPerSecond','tickNanoseconds')) {
        Assert-ProfilingNumber $Receipt['clock'] $key 'report.clock'
        if ($Receipt['clock'][$key] -le 0) { throw 'Invalid profiling report: QPC clock must be positive.' }
    }
    if ([Math]::Abs($Receipt['clock']['tickNanoseconds'] * $Receipt['clock']['ticksPerSecond'] - 1.0e9) -gt 0.01) { throw 'Invalid profiling report: QPC clock units differ.' }
    $windowNames = @('warm-1-20','warm-21-100','warm-101-200','clean-probe','dirty-probe')
    $frameCounts = @(20,80,100,40,40)
    $firstUpdates = @(0,20,100,200,200)
    Assert-ProfilingArray $Receipt 'windows' 5 'report'
    for ($index = 0; $index -lt 5; $index++) {
        $window = $Receipt['windows'][$index]; $context = "report.windows[$index]"
        Assert-ProfilingObject $window $context
        Assert-ProfilingExact $window 'window' $windowNames[$index] $context
        Assert-ProfilingExact $window 'firstUpdate' $firstUpdates[$index] $context
        Assert-ProfilingExact $window 'dirty' ($index -ne 3) $context
        Assert-ProfilingNumber $window 'wallMs' $context
        if (-not (Assert-ProfilingCounters $window['cpuBefore'] "$context.cpuBefore")) { $complete = $false }
        if (-not (Assert-ProfilingCounters $window['cpuAfter'] "$context.cpuAfter")) { $complete = $false }
        Assert-ProfilingArray $window 'frames' $frameCounts[$index] $context
        $total = 0.0
        foreach ($frame in $window['frames']) {
            Assert-ProfilingObject $frame "$context.frames"
            foreach ($key in @('completedMs','prepareMs','compositeMs')) { Assert-ProfilingNumber $frame $key "$context.frames" }
            foreach ($key in @('cppAllocations','compositeAllocations')) { Assert-ProfilingNumber $frame $key "$context.frames" -Integer }
            if ($frame['completedMs'] -le 0 -or $frame['completedMs'] -lt $frame['prepareMs'] + $frame['compositeMs']) {
                throw "Invalid profiling report: $context has inconsistent frame intervals."
            }
            Assert-ProfilingExact $frame 'compositeAllocations' 0 "$context.frames"
            if ($index -eq 3) { Assert-ProfilingExact $frame 'cppAllocations' 0 "$context.frames" }
            $total += [double]$frame['completedMs']
        }
        if ($window['wallMs'] + 1e-7 -lt $total) { throw "Invalid profiling report: $context wall interval excludes its frames." }
    }
    $stageNames = @('entry','device-and-target','scene','warm20','warm100','warm200','capture','clean-probe','dirty-probe','hidden','detached','released')
    $completed = @(0,0,0,20,100,200,200,240,280,280,280,280)
    Assert-ProfilingArray $Receipt 'stages' 12 'report'
    $previousElapsed = -1.0
    for ($index = 0; $index -lt 12; $index++) {
        $stage = $Receipt['stages'][$index]; $context = "report.stages[$index]"
        Assert-ProfilingObject $stage $context
        Assert-ProfilingExact $stage 'stage' $stageNames[$index] $context
        Assert-ProfilingExact $stage 'completedFrames' $completed[$index] $context
        foreach ($key in @('elapsedMs','samplingMs')) { Assert-ProfilingNumber $stage $key $context }
        if ($stage['elapsedMs'] -lt $previousElapsed) { throw "Invalid profiling report: $context stage time decreased." }
        $previousElapsed = [double]$stage['elapsedMs']
        if (-not (Assert-ProfilingCounters $stage['cpu'] "$context.cpu")) { $complete = $false }
        Assert-ProfilingObject $stage['process'] "$context.process"
        foreach ($key in @('privateBytes','workingSetBytes','peakWorkingSetBytes','handles')) { Assert-ProfilingNumber $stage['process'] $key "$context.process" -Integer }
        if (-not (Assert-ProfilingQuery $stage['process'] 'handles' @('handles') "$context.process")) { $complete = $false }
        $scheduling = $stage['scheduling']; $scheduleContext = "$context.scheduling"
        Assert-ProfilingObject $scheduling $scheduleContext
        Assert-ProfilingNumber $scheduling 'threadError' $scheduleContext -Integer
        if ($scheduling['threadError'] -gt [uint32]::MaxValue) { throw "Invalid profiling report: $scheduleContext.threadError is outside the API domain." }
        if ($scheduling['threadError'] -ne 0) { $complete = $false }
        Assert-ProfilingNumber $scheduling 'priorityClass' $scheduleContext -Integer
        $priorityAvailable = Assert-ProfilingQuery $scheduling 'priority' @('priorityClass') $scheduleContext
        $affinityAvailable = Assert-ProfilingQuery $scheduling 'affinity' @() $scheduleContext
        $groupsAvailable = Assert-ProfilingQuery $scheduling 'groups' @() $scheduleContext
        if (-not $priorityAvailable -or -not $affinityAvailable -or -not $groupsAvailable) { $complete = $false }
        if ($priorityAvailable -and $scheduling['priorityClass'] -le 0) { throw "Invalid profiling report: $scheduleContext priority class is absent." }
        foreach ($key in @('processMask','systemMask')) {
            if ($scheduling[$key] -isnot [string] -or $scheduling[$key] -cnotmatch '^[0-9A-F]{16}$') { throw "Invalid profiling report: $scheduleContext.$key is invalid." }
            if (-not $affinityAvailable) { Assert-ProfilingExact $scheduling $key '0000000000000000' $scheduleContext }
        }
        if ($scheduling['groups'] -isnot [array] -or $scheduling['groups'].Count -gt 64 -or
            ($groupsAvailable -and $scheduling['groups'].Count -lt 1) -or (-not $groupsAvailable -and $scheduling['groups'].Count -ne 0)) { throw "Invalid profiling report: $scheduleContext group inventory is malformed." }
        $groups = [Collections.Generic.HashSet[int]]::new()
        foreach ($group in $scheduling['groups']) {
            Assert-ProfilingNumber @{value=$group} 'value' "$scheduleContext.groups" -Integer
            if ($group -ge 64 -or -not $groups.Add([int]$group)) { throw "Invalid profiling report: $scheduleContext group inventory is invalid." }
        }
        Assert-ProfilingExact $scheduling 'threadLimit' 256 $scheduleContext
        Assert-ProfilingExact $scheduling 'threadScanLimit' 65536 $scheduleContext
        Assert-ProfilingNumber $scheduling 'threadsScanned' $scheduleContext -Integer
        if ($scheduling['threadsScanned'] -gt 65536 -or $scheduling['threads'] -isnot [array] -or $scheduling['threads'].Count -gt 256 -or
            $scheduling['threads'].Count -gt $scheduling['threadsScanned'] -or ($scheduling['threadError'] -eq 0 -and $scheduling['threads'].Count -lt 1)) { throw "Invalid profiling report: $scheduleContext thread inventory is malformed or silently overflowing." }
        $threads = [Collections.Generic.HashSet[int]]::new(); $hasMain = $false
        foreach ($thread in $scheduling['threads']) {
            Assert-ProfilingObject $thread "$scheduleContext.threads"
            foreach ($key in @('id','creation100ns','group')) { Assert-ProfilingNumber $thread $key "$scheduleContext.threads" -Integer }
            Assert-ProfilingNumber $thread 'priority' "$scheduleContext.threads" -Integer -Signed
            Assert-ProfilingNumber $thread 'handleError' "$scheduleContext.threads" -Integer
            Assert-ProfilingNumber $thread 'ownerProcessId' "$scheduleContext.threads" -Integer
            if ($thread['handleError'] -gt [uint32]::MaxValue) { throw "Invalid profiling report: $scheduleContext thread handle error is outside its API domain." }
            $ownerAvailable = Assert-ProfilingQuery $thread 'owner' @('ownerProcessId') "$scheduleContext.threads"
            if ($thread['ownerProcessId'] -gt [uint32]::MaxValue -or ($ownerAvailable -and $thread['ownerProcessId'] -eq 0) -or
                ($thread['handleError'] -eq 0 -and (-not $ownerAvailable -or $thread['ownerProcessId'] -ne $Receipt['processId'])) -or
                ($ownerAvailable -and $thread['ownerProcessId'] -eq $Receipt['processId'] -and $thread['handleError'] -ne 0)) { throw "Invalid profiling report: $scheduleContext thread ownership and handle state disagree." }
            $creationAvailable = Assert-ProfilingQuery $thread 'creation' @('creation100ns') "$scheduleContext.threads"
            $threadPriority = Assert-ProfilingQuery $thread 'priority' @('priority') "$scheduleContext.threads"
            $threadAffinity = Assert-ProfilingQuery $thread 'affinity' @('group') "$scheduleContext.threads"
            $threadCpu = Assert-ProfilingCpu $thread['cpu'] "$scheduleContext.threads.cpu"
            if (-not $creationAvailable -or -not $threadPriority -or -not $threadAffinity -or -not $threadCpu -or $thread['handleError'] -ne 0) { $complete = $false }
            if ($thread['handleError'] -ne 0 -and ($creationAvailable -or $threadPriority -or $threadAffinity -or $thread['cpu']['timesAvailable'] -or $thread['cpu']['cyclesAvailable'])) { throw "Invalid profiling report: $scheduleContext queried an unavailable thread handle." }
            if ($thread['id'] -le 0 -or $thread['id'] -gt [int]::MaxValue -or ($creationAvailable -and $thread['creation100ns'] -le 0) -or -not $threads.Add([int]$thread['id'])) { throw "Invalid profiling report: $scheduleContext duplicate or missing thread identity." }
            if ($thread['group'] -ge 64 -or ($threadAffinity -and $groupsAvailable -and -not $groups.Contains([int]$thread['group'])) -or $thread['mask'] -isnot [string] -or $thread['mask'] -cnotmatch '^[0-9A-F]{16}$') { throw "Invalid profiling report: $scheduleContext thread affinity is invalid." }
            if (-not $threadAffinity) { Assert-ProfilingExact $thread 'mask' '0000000000000000' "$scheduleContext.threads" }
            if ($thread['id'] -eq $Receipt['mainThreadId']) { $hasMain = $true }
        }
        if (-not $hasMain -and $scheduling['threadError'] -eq 0) { throw "Invalid profiling report: $scheduleContext silently omits the main thread." }
        $grid = $stage['grid']; Assert-ProfilingObject $grid "$context.grid"
        Assert-ProfilingExact $grid 'available' ($index -ge 2 -and $index -le 9) "$context.grid"
        foreach ($key in @('lookups','hits','layoutCreations','shapedUnits','retainedLayouts','capacity','displayLayouts','displayCapacity','textUnits','tableBytes')) {
            Assert-ProfilingNumber $grid $key "$context.grid" -Integer
        }
        if ($grid['hits'] -gt $grid['lookups'] -or $grid['retainedLayouts'] -gt $grid['capacity'] -or $grid['displayLayouts'] -gt $grid['displayCapacity']) { throw "Invalid profiling report: $context grid cache inventory is inconsistent." }
        $view = $stage['view']; Assert-ProfilingObject $view "$context.view"
        Assert-ProfilingExact $view 'available' ($index -ge 2 -and $index -le 10) "$context.view"
        foreach ($key in @('preparations','composites','surfaceAllocations','surfaceBytes','replacementPeakBytes')) {
            Assert-ProfilingNumber $view $key "$context.view" -Integer
            if (-not $view['available']) { Assert-ProfilingExact $view $key 0 "$context.view" }
        }
        if ($index -ge 9) { Assert-ProfilingExact $view 'surfaceBytes' 0 "$context.view" }
        $sampler = $stage['heapSampler']; Assert-ProfilingObject $sampler "$context.heapSampler"
        Assert-ProfilingExact $sampler 'enabled' $Heap "$context.heapSampler"
        if ($sampler['complete'] -isnot [bool]) { throw "Invalid profiling report: $context heap completeness is missing." }
        Assert-ProfilingExact $sampler 'heapLimit' 128 "$context.heapSampler"
        Assert-ProfilingNumber $sampler 'durationMs' "$context.heapSampler"
        foreach ($key in @('inventoryError','heapCountHint','jsonBytes','jsonCapacityBytes')) { Assert-ProfilingNumber $sampler $key "$context.heapSampler" -Integer }
        if ($sampler['inventoryError'] -gt [uint32]::MaxValue -or $sampler['heapCountHint'] -gt [uint32]::MaxValue -or $sampler['jsonBytes'] -gt 65536 -or $sampler['jsonBytes'] -gt $sampler['jsonCapacityBytes']) { throw "Invalid profiling report: $context heap metadata exceeds its domain." }
        if ($stage['samplingMs'] + 1e-7 -lt $sampler['durationMs']) { throw "Invalid profiling report: $context heap sampling exceeds its stage interval." }
        if ($Heap) {
            if ($sampler['heaps'] -isnot [array] -or $sampler['heaps'].Count -gt 128 -or ($sampler['inventoryError'] -eq 0 -and $sampler['heaps'].Count -lt 1)) { throw "Invalid profiling report: $context heap inventory is malformed or silently overflowing." }
            if (($sampler['heapCountHint'] -eq 0 -or $sampler['heapCountHint'] -gt 128) -and $sampler['inventoryError'] -eq 0) { throw "Invalid profiling report: $context heap count overflow/error was not declared." }
            $heapComplete = $sampler['inventoryError'] -eq 0
            $heapIds = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
            foreach ($heapSample in $sampler['heaps']) {
                Assert-ProfilingObject $heapSample "$context.heapSampler.heaps"
                foreach ($key in @('heap','busyBytes','freeBytes','entryOverheadBytes','regionCommittedBytes','regionUncommittedBytes')) { Assert-ProfilingNumber $heapSample $key "$context.heapSampler.heaps" -Integer }
                Assert-ProfilingNumber $heapSample 'error' "$context.heapSampler.heaps" -Integer
                if ($heapSample['error'] -gt [uint32]::MaxValue) { throw "Invalid profiling report: $context heap error is outside its API domain." }
                if ($heapSample['error'] -ne 0) { $heapComplete = $false }
                if ($heapSample['heap'] -le 0 -or -not $heapIds.Add([string]$heapSample['heap'])) { throw "Invalid profiling report: $context repeated or missing heap identity." }
            }
            Assert-ProfilingExact $sampler 'complete' $heapComplete "$context.heapSampler"
            if (-not $heapComplete) { $complete = $false }
        } elseif ($sampler.Contains('heaps') -or $sampler['durationMs'] -ne 0 -or $sampler['heapCountHint'] -ne 0 -or $sampler['inventoryError'] -ne 0 -or $sampler['jsonBytes'] -ne 0 -or -not $sampler['complete']) { throw "Invalid profiling report: $context heap-off sampled a heap." }
    }
    # Replay the actual fixed chronology once, rather than treating phases and windows as
    # independent streams. Main/process readings remain separate because they are sequential.
    $history = [ordered]@{}
    $windowBeforeStage = @{3=0; 4=1; 5=2; 7=3; 8=4}
    $mainCreation = $null
    for ($index = 0; $index -lt 12; $index++) {
        if ($windowBeforeStage.ContainsKey($index)) {
            $window = $Receipt['windows'][$windowBeforeStage[$index]]
            foreach ($edge in @('cpuBefore','cpuAfter')) { foreach ($kind in @('main','process')) {
                Assert-ProfilingCpuHistory $window[$edge][$kind] $history $kind "report.windows.$($window['window']).$edge.$kind"
            } }
        }
        $stage = $Receipt['stages'][$index]
        foreach ($kind in @('main','process')) { Assert-ProfilingCpuHistory $stage['cpu'][$kind] $history $kind "report.stages[$index].cpu.$kind" }
        foreach ($thread in $stage['scheduling']['threads']) {
            if (-not $thread['creationAvailable']) { continue }
            $identity = "thread/$($thread['id']):$($thread['creation100ns'])"
            if ($thread['id'] -eq $Receipt['mainThreadId']) {
                if ($null -ne $mainCreation -and $mainCreation -ne $thread['creation100ns']) { throw 'Invalid profiling report: main thread creation identity changed.' }
                $mainCreation = $thread['creation100ns']; $identity = 'main'
            }
            Assert-ProfilingCpuHistory $thread['cpu'] $history $identity "report.stages[$index].threads[$($thread['id'])].cpu"
        }
    }
    $completeness = if ($complete) { 'complete' } else { 'partial' }
    Assert-ProfilingExact $Receipt 'telemetryCompleteness' $completeness 'report'
    return $completeness
}

function Assert-BoundaryReport {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Receipt, [bool] $Timers,
        [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Release', [int] $ProcessId,
        [ValidateSet('','Default','CallingThread')][string] $WarpRasterMode = '',
        [ValidateSet('','Default','MultilineGrid','MultilineGridDistinct')][string] $BoundaryScenario = '',
        [ValidateSet('Clean','Dirty')][string] $BoundaryPhase = 'Clean')
    Assert-ProfilingObject $Receipt 'report'
    if ($WarpRasterMode) { $WarpRasterMode=if ($WarpRasterMode -ieq 'Default') { 'Default' } else { 'CallingThread' } }
    if ($BoundaryScenario) { $BoundaryScenario=@('Default','MultilineGrid','MultilineGridDistinct') | Where-Object { $_ -ieq $BoundaryScenario }; if (-not $WarpRasterMode) { $WarpRasterMode='Default' } }
    $schema=if ($BoundaryScenario) { 3 } elseif ($WarpRasterMode) { 2 } else { 1 }
    foreach ($pair in @(@('schema',$schema),@('fixture',"dxui-complex-ui-boundaries-v$schema"),@('classification','diagnostic-only'),@('renderer','WARP'),
            @('scenario',$(if ($BoundaryScenario) { $BoundaryScenario } else { 'Default' })),@('heapSampling',$false),@('boundaryTimers',$Timers),@('width',1280),@('height',720),@('dpi',96),
            @('controls',83),@('modelRows',1000),@('warmFrames',200),@('probeFrames',320),@('hiddenPreparations',0),@('hiddenComposites',0),
            @('hiddenSurfaceBytes',0),@('pointerBits',64),@('iteratorDebugLevel',$(if ($Configuration -eq 'Release') { 0 } else { 2 })))) {
        Assert-ProfilingExact $Receipt $pair[0] $pair[1] 'report'
    }
    if ($WarpRasterMode) {
        # Exact SDK creation flags: BGRA support, plus the opt-in WARP profiling
        # flag for calling-thread rasterization. Neither is a library default change.
        $expectedFlags=if ($WarpRasterMode -ceq 'CallingThread') { 40 } else { 32 }
        Assert-ProfilingExact $Receipt 'warpRasterMode' $WarpRasterMode 'report'
        Assert-ProfilingExact $Receipt 'requestedDeviceFlags' $expectedFlags 'report'
        Assert-ProfilingExact $Receipt 'deviceCreationFlags' $expectedFlags 'report'
    } elseif ($Receipt.Contains('warpRasterMode') -or $Receipt.Contains('requestedDeviceFlags') -or $Receipt.Contains('deviceCreationFlags')) {
        throw 'Invalid boundary report: undeclared WARP execution policy in a v1 report.'
    }
    foreach ($key in @('compiler','processId','mainThreadId','mainThreadCreation100ns','fixedDiagnosticStorageBytes')) {
        Assert-ProfilingNumber $Receipt $key 'report' -Integer
        if ($Receipt[$key] -le 0) { throw "Invalid boundary report: $key must be positive." }
    }
    if ($Receipt['fixedDiagnosticStorageBytes'] -gt 1MB) { throw 'Invalid boundary report: diagnostic storage exceeds its budget.' }
    if ($ProcessId -gt 0) { Assert-ProfilingExact $Receipt 'processId' $ProcessId 'report' }
    foreach ($key in @('timingScope','schedulingScope')) {
        if ($Receipt[$key] -isnot [string] -or [string]::IsNullOrWhiteSpace($Receipt[$key])) { throw "Invalid boundary report: $key is absent." }
    }
    if ($BoundaryScenario -and ($Receipt['ownerCpuScope'] -isnot [string] -or [string]::IsNullOrWhiteSpace($Receipt['ownerCpuScope']))) { throw 'Invalid boundary report: owner CPU scope is absent.' }
    if ($BoundaryScenario) {
        $BoundaryPhase=if ($BoundaryPhase -ieq 'Dirty') { 'Dirty' } else { 'Clean' }
        Assert-ProfilingExact $Receipt 'phase' $BoundaryPhase 'report'
        Assert-ProfilingExact $Receipt 'firstUpdate' $(if ($BoundaryPhase -ceq 'Dirty') { 200 } else { 0 }) 'report'
        Assert-ProfilingExact $Receipt 'dirtyAllocationCeilingPerFrame' $(if ($Configuration -eq 'Release') { 64 } else { 320 }) 'report'
    }
    $clock=$Receipt['clock']; Assert-ProfilingObject $clock 'report.clock'
    Assert-ProfilingExact $clock 'implementation' 'QueryPerformanceCounter' 'report.clock'
    Assert-ProfilingNumber $clock 'ticksPerSecond' 'report.clock' -Integer
    Assert-ProfilingNumber $clock 'tickNanoseconds' 'report.clock'
    if ($clock['ticksPerSecond'] -le 0 -or [Math]::Abs($clock['tickNanoseconds'] * $clock['ticksPerSecond'] - 1e9) -gt 0.01) {
        throw 'Invalid boundary report: QPC units differ.'
    }
    Assert-ProfilingArray $Receipt 'windows' 8 'report'
    $complete=$true; $history=@{}; $previous=[decimal]0
    for ($index=0; $index -lt 8; $index++) {
        $window=$Receipt['windows'][$index]; $context="report.windows[$index]"; Assert-ProfilingObject $window $context
        $kind=if ($index -lt 4) { $index } else { 7-$index }
        Assert-ProfilingExact $window 'clear' ($kind -in @(0,1)) $context
        Assert-ProfilingExact $window 'composite' ($kind -in @(0,2)) $context
        foreach ($key in @('startedQpc','endedQpc')) { Assert-ProfilingNumber $window $key $context -Integer }
        if ($window['startedQpc'] -lt $previous -or $window['endedQpc'] -le $window['startedQpc']) { throw 'Invalid boundary report: window QPC order differs.' }
        foreach ($edge in @('cpuBefore','cpuAfter')) {
            if (-not (Assert-ProfilingCounters $window[$edge] "$context.$edge")) { $complete=$false }
        }
        foreach ($kindCpu in @('main','process')) { Assert-ProfilingCpuHistory $window.cpuBefore[$kindCpu] $history $kindCpu "$context.cpuBefore.$kindCpu" }
        Assert-ProfilingArray $window 'frames' 40 $context
        $previous=[decimal]$window['startedQpc']
        foreach ($frame in $window['frames']) {
            Assert-ProfilingObject $frame "$context.frames"
            if ($BoundaryScenario -and $BoundaryPhase -ceq 'Dirty') {
                Assert-ProfilingNumber $frame 'cppAllocations' "$context.frames" -Integer
                if ($frame.cppAllocations -gt $Receipt.dirtyAllocationCeilingPerFrame) { throw 'Invalid boundary report: dirty frame exceeds its allocation ceiling.' }
            } else { Assert-ProfilingExact $frame 'cppAllocations' 0 "$context.frames" }
            if ($BoundaryScenario) { Assert-ProfilingExact $frame 'composeAllocations' 0 "$context.frames" }
            $priorFrameEnd=$previous
            Assert-ProfilingArray $frame 'qpc' 9 "$context.frames"
            for ($edge=0; $edge -lt 9; $edge++) {
                $value=@{ tick=$frame['qpc'][$edge] }; Assert-ProfilingNumber $value 'tick' "$context.frames.qpc" -Integer
                if (-not $Timers -and $edge -gt 0 -and $edge -lt 8) {
                    if ($value.tick -ne 0) { throw 'Invalid boundary report: disabled interior timer is nonzero.' }
                    continue
                }
                if ($value.tick -lt $previous -or $value.tick -gt $window['endedQpc']) { throw 'Invalid boundary report: frame QPC order or bounds differ.' }
                $previous=[decimal]$value.tick
            }
            if ($frame.qpc[8] -le $frame.qpc[0]) { throw 'Invalid boundary report: frame must have positive duration.' }
            if ($BoundaryScenario) {
                Assert-ProfilingNumber $frame 'updateEndQpc' "$context.frames" -Integer
                if (($Timers -and ($frame.updateEndQpc -lt $frame.qpc[0] -or $frame.updateEndQpc -gt $frame.qpc[1])) -or
                    (-not $Timers -and $frame.updateEndQpc -ne 0)) { throw 'Invalid boundary report: update boundary differs from timer mode or frame order.' }
                Assert-ProfilingArray $frame 'cpuProbeQpc' 4 "$context.frames"
                foreach ($tick in $frame.cpuProbeQpc) { Assert-ProfilingNumber @{tick=$tick} 'tick' "$context.frames.cpuProbeQpc" -Integer }
                if ($frame.cpuProbeQpc[0] -lt $priorFrameEnd -or $frame.cpuProbeQpc[0] -gt $frame.cpuProbeQpc[1] -or
                    $frame.cpuProbeQpc[1] -gt $frame.qpc[0] -or $frame.qpc[8] -gt $frame.cpuProbeQpc[2] -or
                    $frame.cpuProbeQpc[2] -gt $frame.cpuProbeQpc[3] -or $frame.cpuProbeQpc[3] -gt $window.endedQpc) { throw 'Invalid boundary report: owner CPU probe brackets do not enclose the frame.' }
                foreach ($edge in @('ownerCpuBefore','ownerCpuAfter')) {
                    if (-not (Assert-ProfilingCpu $frame[$edge] "$context.frames.$edge")) { $complete=$false }
                    Assert-ProfilingCpuHistory $frame[$edge] $history 'main' "$context.frames.$edge"
                }
                $previous=[decimal]$frame.cpuProbeQpc[3]
            } elseif ($frame.Contains('ownerCpuBefore') -or $frame.Contains('ownerCpuAfter') -or $frame.Contains('cpuProbeQpc')) { throw 'Invalid boundary report: owner CPU probes require v3.' }
        }
        foreach ($kindCpu in @('main','process')) { Assert-ProfilingCpuHistory $window.cpuAfter[$kindCpu] $history $kindCpu "$context.cpuAfter.$kindCpu" }
        $previous=[decimal]$window['endedQpc']
    }
    $completeness=if ($complete) { 'complete' } else { 'partial' }
    Assert-ProfilingExact $Receipt 'telemetryCompleteness' $completeness 'report'
    return $completeness
}

function Get-ProfilingMeasurementState {
    param([Parameter(Mandatory)][string] $Root, [string] $Platform, [string] $Configuration)
    $state = Get-PerformanceMeasurementState -Root $Root -Platform $Platform -Configuration $Configuration
    $inputs = [ordered]@{}
    foreach ($path in @('Tests/Embedded/Embedded.Tests.ComplexUiProfiling.h','Tests/Embedded/Embedded.Tests.ComplexUiBoundaries.h','Tests/Embedded/Embedded.Tests.Embedded.cpp',
            'Tests/Embedded/Embedded.Tests.BenchmarkInstrument.h','Tests/Embedded/Embedded.Tests.ComplexUiBenchmark.h','Tests/Support/Support.Tests.HeapDiagnostic.h',
            'Samples/ComplexUi/ComplexUiScene.h','Samples/EmbeddedControls/GraphicsFixture.h',
            'performance-profile.ps1','Tools/PerformanceProfiling.psm1','Tools/PerformancePolicy.psm1','Tools/ScopedTesting.psm1')) {
        $inputs[$path] = (Get-FileHash -LiteralPath (Join-Path $Root $path) -Algorithm SHA256).Hash
    }
    $state['profilingInputs'] = $inputs
    return $state
}

function Invoke-OwnedProfilingProcess {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Executable, [string[]] $Arguments, [string] $WorkingDirectory,
        [ValidateRange(0.1,120)][double] $TimeoutSeconds = 120, [ValidateRange(0.1,5)][double] $DrainTimeoutSeconds = 5,
        [ValidateRange(1024,1048576)][int] $PipeCharacterLimit = 1048576)
    $start = [Diagnostics.ProcessStartInfo]::new($Executable)
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    $start.WorkingDirectory = $WorkingDirectory
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $started = [DateTime]::UtcNow
    $process = [Diagnostics.Process]::Start($start)
    $clock = [Diagnostics.Stopwatch]::StartNew()
    try {
        $pipes = @(foreach ($reader in @($process.StandardOutput,$process.StandardError)) {
            $buffer = [char[]]::new(4096)
            [ordered]@{ reader=$reader; buffer=$buffer; task=$reader.ReadAsync($buffer,0,$buffer.Length);
                text=[Text.StringBuilder]::new(); closed=$false; truncated=$false; error='' }
        })
        $exitTask = $process.WaitForExitAsync()
        $timedOut = $false; $terminationError = ''; $drainError = ''; $drainDeadline = $null; $drainStartedElapsedMs = $null
        while ($true) {
            if ($null -eq $drainDeadline -and -not $process.HasExited -and $clock.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
                $timedOut = $true
                # Only this original retained process handle is owned. The attested native fixture
                # creates no descendants; do not follow parent IDs into independent applications.
                try { $process.Kill() } catch { if (-not $process.HasExited) { $terminationError = $_.Exception.Message } }
                if (-not $process.WaitForExit(5000)) { $terminationError = 'Owned profiling child did not terminate after timeout.' }
                $drainStartedElapsedMs = $clock.Elapsed.TotalMilliseconds
                $drainDeadline = $drainStartedElapsedMs + $DrainTimeoutSeconds * 1000
            }
            if ($null -eq $drainDeadline -and $process.HasExited) {
                $drainStartedElapsedMs = $clock.Elapsed.TotalMilliseconds
                $drainDeadline = $drainStartedElapsedMs + $DrainTimeoutSeconds * 1000
            }
            foreach ($pipe in $pipes) {
                if ($pipe.closed -or -not $pipe.task.IsCompleted) { continue }
                try {
                    $count = $pipe.task.GetAwaiter().GetResult()
                    if ($count -eq 0) { $pipe.closed = $true; continue }
                    $retained = [Math]::Min($count, $PipeCharacterLimit - $pipe.text.Length)
                    if ($retained -gt 0) { [void]$pipe.text.Append($pipe.buffer,0,$retained) }
                    if ($retained -lt $count) { $pipe.truncated = $true }
                    $pipe.task = $pipe.reader.ReadAsync($pipe.buffer,0,$pipe.buffer.Length)
                } catch { $pipe.error = $_.Exception.Message; $pipe.closed = $true }
            }
            if (($process.HasExited -or $timedOut) -and $pipes[0].closed -and $pipes[1].closed) { break }
            if ($null -ne $drainDeadline -and $clock.Elapsed.TotalMilliseconds -ge $drainDeadline) {
                $drainError = 'Owned profiling child output pipes did not close within the drain budget.'
                break
            }
            $tasks = [Collections.Generic.List[Threading.Tasks.Task]]::new()
            if (-not $process.HasExited -and -not $timedOut) { $tasks.Add($exitTask) }
            foreach ($pipe in $pipes) { if (-not $pipe.closed) { $tasks.Add($pipe.task) } }
            $deadline = if ($null -eq $drainDeadline) { $TimeoutSeconds * 1000 } else { $drainDeadline }
            $waitMs = [Math]::Max(1,[int][Math]::Ceiling($deadline - $clock.Elapsed.TotalMilliseconds))
            [void][Threading.Tasks.Task]::WaitAny($tasks.ToArray(),$waitMs)
        }
        foreach ($pipe in $pipes) {
            if ($pipe.error) { $drainError += " Pipe read error: $($pipe.error)" }
            if ($pipe.truncated) { $drainError += ' Pipe character budget exceeded; prefix retained.' }
        }
        return [ordered]@{ processId=$process.Id; executable=$Executable; arguments=@($Arguments); workingDirectory=$WorkingDirectory;
            startedUtc=$started.ToString('o'); completedUtc=[DateTime]::UtcNow.ToString('o'); timeoutSeconds=$TimeoutSeconds;
            drainTimeoutSeconds=$DrainTimeoutSeconds; exitCode=$(if ($process.HasExited) { $process.ExitCode } else { $null });
            timedOut=$timedOut; terminationError=$terminationError; drainError=$drainError;
            # Observed drain entry follows child-exit observation or termination handling; launch/setup is excluded.
            observedDrainElapsedMs=$(if ($null -ne $drainStartedElapsedMs) { [Math]::Max(0,[Math]::Round($clock.Elapsed.TotalMilliseconds-$drainStartedElapsedMs,3)) } else { $null });
            pipeCharacterLimit=$PipeCharacterLimit; stdoutTruncated=$pipes[0].truncated; stderrTruncated=$pipes[1].truncated;
            stdout=$pipes[0].text.ToString(); stderr=$pipes[1].text.ToString() }
    } catch {
        if (-not $process.HasExited) { $process.Kill(); [void]$process.WaitForExit(5000) }
        throw
    } finally {
        # Termination was already attempted and bounded above; preserve its failure metadata.
        $process.Dispose()
    }
}

function Get-ProfilingPngUInt32([byte[]] $Bytes, [int] $Offset) {
    return [uint32]([uint64]$Bytes[$Offset] * 16777216 + [uint64]$Bytes[$Offset+1] * 65536 + [uint64]$Bytes[$Offset+2] * 256 + $Bytes[$Offset+3])
}
function Get-ProfilingPngCrc([byte[]] $Bytes, [int] $Offset, [int] $Count) {
    # GZIP and PNG use the same IEEE CRC32. Let the BCL compute it in a bounded memory
    # stream, without a per-byte PowerShell loop, external tool or additional assembly.
    $memory = [IO.MemoryStream]::new()
    try {
        $gzip = [IO.Compression.GZipStream]::new($memory,[IO.Compression.CompressionLevel]::NoCompression,$true)
        try { $gzip.Write($Bytes,$Offset,$Count) } finally { $gzip.Dispose() }
        $memory.Position = $memory.Length - 8
        $crc = [byte[]]::new(4); $memory.ReadExactly($crc,0,4)
        if (-not [BitConverter]::IsLittleEndian) { [Array]::Reverse($crc) }
        return [BitConverter]::ToUInt32($crc,0)
    } finally { $memory.Dispose() }
}
function Assert-ProfilingPng([string] $Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw 'Profiling child wrote no capture.' }
    $length = (Get-Item -LiteralPath $Path).Length
    if ($length -lt 57 -or $length -gt 16MB) { throw 'Profiling capture is outside its byte budget or truncated.' }
    $bytes = [IO.File]::ReadAllBytes($Path)
    for ($index=0; $index -lt 8; $index++) { if ($bytes[$index] -ne @(137,80,78,71,13,10,26,10)[$index]) { throw 'Profiling capture has no PNG signature.' } }
    $position=8; $chunks=0; $hasData=$false; $dataEnded=$false; $dataBytes=0
    while ($position -lt $bytes.Length) {
        if (++$chunks -gt 256 -or $bytes.Length - $position -lt 12) { throw 'Profiling capture chunk inventory exceeds its budget or is truncated.' }
        $count=Get-ProfilingPngUInt32 $bytes $position
        if ($count -gt $bytes.Length - $position - 12) { throw 'Profiling capture chunk payload is truncated.' }
        $kind=[Text.Encoding]::ASCII.GetString($bytes,$position+4,4)
        if ($kind -cnotmatch '^[A-Za-z]{4}$' -or $kind[2] -cnotmatch '[A-Z]') { throw 'Profiling capture chunk type is invalid.' }
        $crc=Get-ProfilingPngUInt32 $bytes ($position+8+$count)
        if ($crc -ne (Get-ProfilingPngCrc $bytes ($position+4) ([int]$count+4))) { throw 'Profiling capture chunk CRC differs.' }
        if ($chunks -eq 1) {
            if ($kind -cne 'IHDR' -or $count -ne 13 -or (Get-ProfilingPngUInt32 $bytes ($position+8)) -ne 1280 -or
                (Get-ProfilingPngUInt32 $bytes ($position+12)) -ne 720) { throw 'Profiling capture IHDR or dimensions differ.' }
            if ($bytes[$position+16] -ne 8 -or $bytes[$position+17] -notin @(2,6) -or $bytes[$position+18] -ne 0 -or
                $bytes[$position+19] -ne 0 -or $bytes[$position+20] -ne 0) { throw 'Profiling capture uses an unsupported PNG profile.' }
        } elseif ($kind -ceq 'IHDR') { throw 'Profiling capture repeats IHDR.' }
        if ($kind -ceq 'IDAT') {
            if ($dataEnded) { throw 'Profiling capture IDAT chunks are not contiguous.' }
            $hasData=$true; $dataBytes += $count
        } elseif ($hasData) { $dataEnded=$true }
        $position += [int]$count+12
        if ($kind -ceq 'IEND') {
            if ($count -ne 0 -or -not $hasData -or $dataBytes -eq 0 -or $position -ne $bytes.Length) { throw 'Profiling capture IEND or image data inventory is incomplete.' }
            return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes))
        }
        if ($kind[0] -cmatch '[A-Z]' -and $kind -cnotin @('IHDR','IDAT','PLTE')) { throw 'Profiling capture contains an unsupported critical chunk.' }
    }
    throw 'Profiling capture has no final IEND.'
}

function Invoke-ProfilingMeasurement {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Root, [string] $Platform, [string] $Configuration,
        [ValidateSet('Default','MultilineGrid')][string] $Scenario, [bool] $Heap, [Parameter(Mandatory)][string] $OutputPath,
        [ValidateSet('','Off','On')][string] $BoundaryTimers = '',
        [ValidateSet('','Default','CallingThread')][string] $WarpRasterMode = '',
        [ValidateSet('','Default','MultilineGrid','MultilineGridDistinct')][string] $BoundaryScenario = '',
        [ValidateSet('Clean','Dirty')][string] $BoundaryPhase = 'Clean')
    # PowerShell ValidateSet accepts alternate casing; native arguments have an
    # exact spelling. Normalize before constructing commands or validating output.
    $Scenario=if ($Scenario -ieq 'Default') { 'Default' } else { 'MultilineGrid' }
    if ($BoundaryTimers) { $BoundaryTimers=if ($BoundaryTimers -ieq 'On') { 'On' } else { 'Off' } }
    if ($WarpRasterMode) { $WarpRasterMode=if ($WarpRasterMode -ieq 'Default') { 'Default' } else { 'CallingThread' } }
    if ($BoundaryTimers -and ($Heap -or $Scenario -ne 'Default')) { throw 'Boundary diagnostics require Default without heap sampling.' }
    if ($WarpRasterMode -and -not $BoundaryTimers) { throw 'WARP raster mode requires boundary diagnostics.' }
    if ($BoundaryScenario -and -not $BoundaryTimers) { throw 'Boundary scenario requires boundary diagnostics.' }
    if ($BoundaryPhase -ne 'Clean' -and -not $BoundaryScenario) { throw 'Dirty boundary phase requires an explicit boundary scenario.' }
    $BoundaryPhase=if ($BoundaryPhase -ieq 'Dirty') { 'Dirty' } else { 'Clean' }
    if ($BoundaryScenario) { $BoundaryScenario=@('Default','MultilineGrid','MultilineGridDistinct') | Where-Object { $_ -ieq $BoundaryScenario }; if (-not $WarpRasterMode) { $WarpRasterMode='Default' } }
    $OutputPath = [IO.Path]::GetFullPath($OutputPath)
    if ([IO.Path]::GetExtension($OutputPath) -ine '.json') { throw 'Profiling output must name a JSON report.' }
    $rootPrefix = [IO.Path]::GetFullPath($Root) + [IO.Path]::DirectorySeparatorChar
    $reportsPrefix = [IO.Path]::GetFullPath((Join-Path $Root '.build/reports')) + [IO.Path]::DirectorySeparatorChar
    if ($OutputPath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase) -and -not $OutputPath.StartsWith($reportsPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Profiling output inside this repository must stay under .build/reports.' }
    New-Item -ItemType Directory -Path (Split-Path -Parent $OutputPath) -Force | Out-Null
    $rawPath = $OutputPath + '.raw.json'
    foreach ($path in @($OutputPath,$rawPath,($OutputPath + '.png'),($OutputPath + '.log'),($OutputPath + '.child.json'))) {
        if (Test-Path -LiteralPath $path -PathType Leaf) { Move-Item -LiteralPath $path -Destination ($path + '.' + [guid]::NewGuid().ToString('N') + '.stale') }
    }
    $before = Get-ProfilingMeasurementState -Root $Root -Platform $Platform -Configuration $Configuration
    Assert-PerformanceBuildAttestation -Root $Root -Platform $Platform -Configuration $Configuration -State $before
    $exe = Join-Path $Root ".build/$Platform/$Configuration/DxUi.EmbeddedTests.exe"
    $temporary = $OutputPath + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
    $rawTemporary = $rawPath + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
    try {
        $arguments = if ($BoundaryTimers) { @('--benchmark-complex-ui-boundaries',$OutputPath,$BoundaryTimers) }
            else { @('--benchmark-complex-ui-profile',$OutputPath,$Scenario,$(if ($Heap) { 'On' } else { 'Off' })) }
        if ($WarpRasterMode) { $arguments += $WarpRasterMode }
        if ($BoundaryScenario) { $arguments[0]='--benchmark-complex-ui-boundaries-v3'; $arguments += @($BoundaryScenario,$BoundaryPhase) }
        $child = Invoke-OwnedProfilingProcess -Executable $exe -WorkingDirectory $Root -Arguments $arguments
        [IO.File]::WriteAllText(($OutputPath + '.log'), ($child.stdout + $child.stderr), [Text.UTF8Encoding]::new($false))
        $metadata = [ordered]@{}
        foreach ($key in $child.Keys) { if ($key -notin @('stdout','stderr')) { $metadata[$key] = $child[$key] } }
        [IO.File]::WriteAllText(($OutputPath + '.child.json'), ((ConvertTo-Json -InputObject $metadata -Depth 8) + "`n"), [Text.UTF8Encoding]::new($false))
        if ($child.timedOut) { throw 'Profiling child exceeded the fixed 120-second budget.' }
        if ($child.terminationError) { throw "Profiling child termination failed: $($child.terminationError)" }
        if ($child.drainError) { throw "Profiling child output drain failed: $($child.drainError)" }
        if ($child.exitCode -ne 0) { throw "Profiling child failed with exit code $($child.exitCode)." }
        if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) { throw 'Profiling child wrote no report.' }
        if ((Get-Item -LiteralPath $OutputPath).Length -gt 8MB) { throw 'Profiling report exceeded its 8 MiB byte budget.' }
        $capturePath = $OutputPath + '.png'
        $captureHash = Assert-ProfilingPng $capturePath
        $bytes = [IO.File]::ReadAllBytes($OutputPath)
        $rawHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes))
        $after = Get-ProfilingMeasurementState -Root $Root -Platform $Platform -Configuration $Configuration
        Assert-PerformanceMeasurementState -Before $before -After $after
        Assert-PerformanceBuildAttestation -Root $Root -Platform $Platform -Configuration $Configuration -State $after
        $receipt = [Text.UTF8Encoding]::new($false, $true).GetString($bytes) | ConvertFrom-Json -AsHashtable -Depth 32
        $completeness = if ($BoundaryTimers) { Assert-BoundaryReport -Receipt $receipt -Timers ($BoundaryTimers -eq 'On') -Configuration $Configuration -ProcessId $child.processId -WarpRasterMode $WarpRasterMode -BoundaryScenario $BoundaryScenario -BoundaryPhase $BoundaryPhase }
            else { Assert-ProfilingReport -Receipt $receipt -Scenario $Scenario -Heap $Heap -Configuration $Configuration -ProcessId $child.processId }
        if ((Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash -cne $rawHash) { throw 'Profiling native bytes changed during validation.' }
        if ((Get-FileHash -LiteralPath $capturePath -Algorithm SHA256).Hash -cne $captureHash) { throw 'Profiling capture bytes changed during validation.' }
        foreach ($key in $before.Keys) { $receipt[$key] = $before[$key] }
        $receipt['completedUtc'] = [DateTime]::UtcNow.ToString('o')
        $receipt['workloadOwner'] = 'DxUi'
        $receipt['childTimeoutSeconds'] = 120
        $receipt['telemetryCompleteness'] = $completeness
        $receipt['childMetadataPath'] = $OutputPath + '.child.json'
        $receipt['childMetadataSha256'] = (Get-FileHash -LiteralPath $receipt['childMetadataPath'] -Algorithm SHA256).Hash
        $receipt['logSha256'] = (Get-FileHash -LiteralPath ($OutputPath + '.log') -Algorithm SHA256).Hash
        $receipt['capturePath'] = $capturePath; $receipt['captureSha256'] = $captureHash
        $receipt['captureValidationScope'] = 'PNG structure, CRC, bounded chunks, IHDR 1280x720, IDAT and final IEND only; decoding and populated-scene visual proof belong to the native capture gate.'
        $receipt['rawReportPath'] = $rawPath; $receipt['rawReportSha256'] = $rawHash
        $receipt['benchmarkInputsScope'] = 'benchmarkInputs/benchmarkSha256 retain the canonical ComplexUi closure; profilingInputs binds this diagnostic fixture, shared helpers, dispatcher and runner.'
        $receipt['measurement'] = 'Diagnostic only. Fixed extra warmup and sampling are not canonical acceptance; no regression, trusted-policy, actual scheduling, heap-owner or dedicated-host verdict.'
        [IO.File]::WriteAllBytes($rawTemporary, $bytes)
        [IO.File]::WriteAllText($temporary, ((ConvertTo-Json -InputObject $receipt -Depth 32) + "`n"), [Text.UTF8Encoding]::new($false))
        Move-Item -LiteralPath $rawTemporary -Destination $rawPath
        Move-Item -LiteralPath $temporary -Destination $OutputPath -Force
    } catch {
        foreach ($path in @($OutputPath,$rawPath,($OutputPath + '.png'))) {
            if (Test-Path -LiteralPath $path -PathType Leaf) { Move-Item -LiteralPath $path -Destination ($path + '.' + [guid]::NewGuid().ToString('N') + '.invalid.json') }
        }
        throw
    } finally {
        foreach ($path in @($temporary,$rawTemporary)) { if (Test-Path -LiteralPath $path -PathType Leaf) { Remove-Item -LiteralPath $path } }
    }
}

Export-ModuleMember -Function Assert-ProfilingReport, Assert-BoundaryReport, Get-ProfilingMeasurementState, Invoke-OwnedProfilingProcess, Invoke-ProfilingMeasurement
