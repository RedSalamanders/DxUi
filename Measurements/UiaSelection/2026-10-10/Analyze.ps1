[CmdletBinding()] param([string] $PacketRoot = (Join-Path $PSScriptRoot 'reproduction'))
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-Evidence([bool] $Condition, [string] $Reason, [string] $Subject) {
    if (-not $Condition) { throw "[$Reason] $Subject" }
}
function Assert-Fields($Object, [string[]] $Names, [string] $Subject) {
    foreach ($name in $Names) {
        Assert-Evidence ($null -ne $Object.PSObject.Properties[$name]) 'UIA_METADATA_FIELDS' "$Subject is missing $name"
    }
}
function Read-Receipt([string] $Path) {
    Assert-Evidence ((Get-Item -LiteralPath $Path).Length -le 1048576) 'UIA_RECEIPT_BOUND' $Path
    Get-Content -LiteralPath $Path -Encoding UTF8 -Raw | ConvertFrom-Json
}

$manifest = Join-Path $PacketRoot 'SHA256SUMS'
Assert-Evidence (Test-Path -LiteralPath $manifest -PathType Leaf) 'UIA_MANIFEST_MISSING' "$PacketRoot requires SHA256SUMS"
if (Test-Path -LiteralPath $manifest -PathType Leaf) {
    foreach ($line in Get-Content -LiteralPath $manifest) {
        if ($line -notmatch '^([A-Fa-f0-9]{64})  (.+)$') { throw "Malformed packet hash: $line" }
        if ((Get-FileHash -LiteralPath (Join-Path $PacketRoot $Matches[2]) -Algorithm SHA256).Hash -cne $Matches[1].ToUpperInvariant()) {
            throw "Packet hash mismatch: $($Matches[2])"
        }
    }
}
$plan = Read-Receipt (Join-Path $PacketRoot 'plan.receipt.txt')
$completion = Read-Receipt (Join-Path $PacketRoot 'completion.receipt.txt')
Assert-Fields $plan @('purpose','configuration','platform','reportBudget','isolatedRuns','precedingTreePrefixRuns',
    'executable','executableSha256','anchor','startedUtc','originalFailureCause') 'plan'
Assert-Fields $completion @('scheduled','attempted','results','identityMatched','identityError','wallSeconds',
    'originalFailureCause') 'completion'
Assert-Fields $plan.anchor @('identityStatus','buildIdentityVersion','sourceCommit','sourceDirty','sourceFingerprint') 'anchor'
Assert-Evidence ($plan.purpose -ceq 'bounded-uia-selection-investigation' -and $plan.configuration -ceq 'ASan Debug' -and
    $plan.platform -ceq 'x64' -and $plan.reportBudget -eq 11 -and $plan.isolatedRuns -eq 10 -and $plan.precedingTreePrefixRuns -eq 1 -and
    $plan.executableSha256 -cmatch '^[A-Fa-f0-9]{64}$' -and $plan.anchor.identityStatus -ceq 'verifiable' -and
    $plan.anchor.buildIdentityVersion -eq 2 -and $plan.anchor.sourceCommit -cmatch '^[A-Fa-f0-9]{40}$' -and
    $plan.anchor.sourceFingerprint -cmatch '^[A-Fa-f0-9]{64}$' -and $plan.anchor.sourceDirty -is [bool] -and
    $plan.originalFailureCause -ceq 'unproven') 'UIA_PLAN' 'unexpected plan or provenance'
Assert-Evidence ($completion.scheduled -eq 11 -and $completion.attempted -eq 11 -and $completion.results.Count -eq 11 -and
    $completion.identityMatched -is [bool] -and $completion.identityMatched -and
    $completion.identityError -is [string] -and $completion.identityError -ceq '' -and
    $completion.originalFailureCause -ceq 'unproven' -and $completion.wallSeconds -ge 0) 'UIA_COMPLETION' 'incomplete or changed inventory'
Assert-Evidence (@(Get-ChildItem -LiteralPath $PacketRoot -Filter 'uia-*.log' -File).Count -eq 11 -and
    @(Get-ChildItem -LiteralPath $PacketRoot -Filter 'uia-*.receipt.txt' -File).Count -eq 11) 'UIA_FILE_INVENTORY' 'unexpected child inventory'

$gridTest = 'TestSingleGridWindowRowAndCellEventsReachAClientSubscribedToTheWindow'
$prefix = @('TestSingleTreeWindowItemEventsReachAClientSubscribedToTheWindow',
    'TestTreeBesideAnotherControlItemEventsReachAClientSubscribedToTheWindow', $gridTest)
$expectedCounts = @(2,3,3,2,6,2,8,1,1,40,1,1,2,2,0,2)
$seenProcesses = [Collections.Generic.HashSet[long]]::new()
$previousCompleted = [DateTimeOffset]$plan.startedUtc

$runs = @(for ($index = 0; $index -lt 11; $index++) {
    $kind = if ($index -eq 10) { 'preceding-tree-prefix' } else { 'isolated' }
    $stem = 'uia-{0:d2}-{1}' -f ($index + 1), $kind
    $result = $completion.results[$index]
    Assert-Fields $result @('index','kind','receipt','log','exitCode','timedOut','terminationError','drainError') $stem
    Assert-Evidence ($result.index -eq $index + 1 -and $result.kind -ceq $kind -and
        $result.receipt -ceq "$stem.receipt.txt" -and $result.log -ceq "$stem.log") 'UIA_RUN_IDENTITY' $stem
    $metadata = Read-Receipt (Join-Path $PacketRoot "$stem.receipt.txt")
    Assert-Fields $metadata @('processId','executable','arguments','workingDirectory','startedUtc','completedUtc',
        'timeoutSeconds','drainTimeoutSeconds','exitCode','timedOut','terminationError','drainError',
        'pipeCharacterLimit','stdoutTruncated','stderrTruncated','logSha256','executableSha256','inventoryIndex','kind') $stem
    Assert-Evidence ($metadata.inventoryIndex -eq $index + 1 -and $metadata.kind -ceq $kind -and
        $metadata.executable -ceq $plan.executable -and $metadata.executableSha256 -ceq $plan.executableSha256) 'UIA_RUN_IDENTITY' $stem
    # The explicit ControlTests hash is distinct from the canonical EmbeddedTests hash in plan.anchor.
    foreach ($status in @($result, $metadata)) {
        Assert-Evidence ($status.exitCode -eq 0) 'UIA_CHILD_EXIT' $stem
        Assert-Evidence ($status.timedOut -is [bool] -and -not $status.timedOut) 'UIA_CHILD_TIMEOUT' $stem
        Assert-Evidence ($status.terminationError -is [string] -and $status.terminationError -ceq '') 'UIA_CHILD_TERMINATION' $stem
        Assert-Evidence ($status.drainError -is [string] -and $status.drainError -ceq '') 'UIA_CHILD_DRAIN' $stem
    }
    Assert-Evidence ($metadata.stdoutTruncated -is [bool] -and -not $metadata.stdoutTruncated -and
        $metadata.stderrTruncated -is [bool] -and -not $metadata.stderrTruncated) 'UIA_CHILD_TRUNCATION' $stem
    Assert-Evidence ($metadata.timeoutSeconds -eq 120 -and $metadata.drainTimeoutSeconds -eq 5 -and
        $metadata.pipeCharacterLimit -eq 1048576) 'UIA_CHILD_BOUNDS' $stem
    Assert-Evidence ($metadata.processId -gt 0 -and $seenProcesses.Add([long]$metadata.processId)) 'UIA_PROCESS_ID' $stem
    $started = [DateTimeOffset]$metadata.startedUtc
    $completed = [DateTimeOffset]$metadata.completedUtc
    Assert-Evidence ($started -ge $previousCompleted -and $completed -gt $started) 'UIA_CHILD_TIME' $stem
    $previousCompleted = $completed
    $tests = if ($index -eq 10) { $prefix } else { @($gridTest) }
    $arguments = @('--suite=Accessibility','--no-activate',"--test=$($tests -join ',')",'--test-timeout=60')
    Assert-Evidence (($metadata.arguments -join '|') -ceq ($arguments -join '|')) 'UIA_NATIVE_ARGUMENTS' $stem
    $logPath = Join-Path $PacketRoot "$stem.log"
    Assert-Evidence ((Get-FileHash -LiteralPath $logPath -Algorithm SHA256).Hash -ceq $metadata.logSha256) 'UIA_LOG_HASH' $stem
    # Two bounded UTF-16 pipes can occupy at most six MiB when encoded as UTF-8.
    Assert-Evidence ((Get-Item -LiteralPath $logPath).Length -le 6291456) 'UIA_LOG_BOUND' $stem
    $lines = @(Get-Content -LiteralPath $logPath -Encoding UTF8)
    $nativeStarts = @($lines | ForEach-Object { if ($_ -match '^  \[START\] (Test\w+)$') { $Matches[1] } })
    $nativeEnds = @($lines | ForEach-Object { if ($_ -match '^  \[DONE\] (Test\w+) \([0-9]+(?:\.[0-9]+)? s\)$') { $Matches[1] } })
    Assert-Evidence (($nativeStarts -join '|') -ceq ($tests -join '|') -and ($nativeEnds -join '|') -ceq ($tests -join '|') -and
        @($lines | Where-Object { $_ -ceq 'All DxUi tests passed.' }).Count -eq 1) 'UIA_NATIVE_INVENTORY' $stem

    $steps = [Collections.Generic.List[object]]::new()
    $begins = [Collections.Generic.List[object]]::new()
    $streams = [Collections.Generic.List[object]]::new()
    $snapshots = [Collections.Generic.List[object]]::new()
    $arrivals = [Collections.Generic.List[object]]::new()
    $providerWindows = [Collections.Generic.List[object]]::new()
    $attempts = [Collections.Generic.List[object]]::new()
    $tails = [Collections.Generic.List[object]]::new()
    $stepNames = [Collections.Generic.List[int]]::new()
    $capture = $null
    foreach ($line in $lines) {
        if ($line -match '^\s*\[UIA timeline\] step=(\d+) mark=(\d+) cutoff=(\d+) cutoffTick=(\d+) expected=(\d+) expectedArrived=(\d+) action=(\d+)\.\.(\d+)$') {
            $capture = [ordered]@{ step=[int]$Matches[1]; mark=[int]$Matches[2]; cutoff=[int]$Matches[3]; cutoffTick=[long]$Matches[4];
                expected=[int]$Matches[5]; expectedArrived=([int]$Matches[6] -eq 1); actionStarted=[long]$Matches[7]; actionCompleted=[long]$Matches[8] }
            $steps.Add($capture)
        } elseif ($line -match '^\s*\[UIA timeline\] begin step=(\d+) mark=(\d+) expected=(\d+)$') {
            $begins.Add([ordered]@{ step=[int]$Matches[1]; mark=[int]$Matches[2]; expected=[int]$Matches[3] })
        } elseif ($line -match '^\s*\[UIA timeline\] tail step=(\d+) mark=(\d+) cutoff=(\d+) cutoffTick=(\d+)$') {
            $capture = [ordered]@{ step=[int]$Matches[1]; mark=[int]$Matches[2]; cutoff=[int]$Matches[3]; cutoffTick=[long]$Matches[4] }
            $tails.Add($capture)
        } elseif ($line -match '^\s*\[UIA timeline\] step=(\d+) name=.+$') {
            $stepNames.Add([int]$Matches[1])
        } elseif ($line -match '^\s*\[UIA stream\] phase=(before|after|timeout) quiet=(\d+) ticks=(\d+)\.\.(\d+) lastArrival=(\d+) counts=(\d+)\.\.(\d+)$') {
            $streams.Add([ordered]@{ phase=$Matches[1]; quiet=[int]$Matches[2]; started=[long]$Matches[3]; completed=[long]$Matches[4];
                lastArrival=[long]$Matches[5]; first=[int]$Matches[6]; last=[int]$Matches[7] })
        } elseif ($line -match '^\s*\[UIA arrivals\] entries=(\d+) completed=(\d+) capacity=(\d+) overflow=(\d+) failed=(\d+)$') {
            Assert-Evidence ($null -ne $capture) 'UIA_SNAPSHOT_INVENTORY' $stem
            $snapshots.Add([ordered]@{ entries=[long]$Matches[1]; completed=[int]$Matches[2]; capacity=[int]$Matches[3]; overflow=[int]$Matches[4];
                failed=[int]$Matches[5]; cutoff=$capture.cutoff; cutoffTick=$capture.cutoffTick })
        } elseif ($line -match '^\s*\[UIA arrival\] seq=(\d+) index=(\d+) entered=(\d+) completed=(\d+) \[.*\]$') {
            Assert-Evidence ($snapshots.Count -gt 0) 'UIA_SNAPSHOT_INVENTORY' $stem
            $arrivals.Add([ordered]@{ sequence=[long]$Matches[1]; eventIndex=[int]$Matches[2]; entered=[long]$Matches[3]; completed=[long]$Matches[4];
                callbackMs=([long]$Matches[4]-[long]$Matches[3]); snapshot=$snapshots.Count-1 })
        } elseif ($line -match '^\s*\[UIA provider attempts\] step=(\d+) phase=(\w+) counts=(\d+)\.\.(\d+) stepFirst=(\d+) actionFirst=(\d+) capacity=(\d+) overflow=(\d+)$') {
            $providerWindows.Add([ordered]@{ step=[int]$Matches[1]; phase=$Matches[2]; first=[int]$Matches[3]; last=[int]$Matches[4]; stepFirst=[int]$Matches[5];
                actionFirst=[int]$Matches[6]; capacity=[int]$Matches[7]; overflow=[int]$Matches[8] })
        } elseif ($line -match '^\s*\[UIA provider attempt\] seq=(\d+) step=(\d+) tick=(\d+) phase=(before|action|after) thread=(\d+)$') {
            Assert-Evidence ($providerWindows.Count -gt 0) 'UIA_PROVIDER_SEQUENCE' $stem
            $attempts.Add([ordered]@{ sequence=[int]$Matches[1]; step=[int]$Matches[2]; tick=[long]$Matches[3]; phase=$Matches[4];
                thread=[long]$Matches[5]; window=$providerWindows.Count-1 })
        }
    }
    Assert-Evidence ($steps.Count -eq 16 -and $begins.Count -eq 16 -and $stepNames.Count -eq 16 -and
        ($stepNames -join ',') -ceq ((1..16) -join ',')) 'UIA_STEP_INVENTORY' $stem
    Assert-Evidence ($streams.Count -eq 32) 'UIA_STREAM_INVENTORY' $stem
    Assert-Evidence ($snapshots.Count -eq 17 -and $tails.Count -eq 1) 'UIA_SNAPSHOT_INVENTORY' $stem
    Assert-Evidence ($providerWindows.Count -eq 16) 'UIA_PROVIDER_INVENTORY' $stem
    foreach ($stream in $streams) {
        Assert-Evidence ($stream.quiet -eq 1 -and $stream.phase -cne 'timeout' -and $stream.started -le $stream.lastArrival -and
            $stream.lastArrival -le $stream.completed -and $stream.first -le $stream.last) 'UIA_QUIET_TIMEOUT' $stem
        Assert-Evidence ($stream.completed-$stream.started -le 10000) 'UIA_QUIET_DEADLINE' $stem
        Assert-Evidence ($stream.completed-$stream.lastArrival -ge 500) 'UIA_QUIET_DURATION' $stem
    }
    $betweenStepEvents = 0
    for ($s = 0; $s -lt 16; $s++) {
        $step = $steps[$s]; $begin = $begins[$s]; $before = $streams[$s*2]; $after = $streams[$s*2+1]
        $previousCutoff = if ($s) { $steps[$s-1].cutoff } else { 0 }
        Assert-Evidence ($step.step -eq $s+1 -and $step.expected -eq $expectedCounts[$s] -and $step.expectedArrived -and
            $begin.step -eq $step.step -and $begin.mark -eq $step.mark -and $begin.expected -eq $step.expected) 'UIA_STEP_EXPECTATION' $stem
        Assert-Evidence ($step.mark -ge $previousCutoff -and $step.cutoff -ge $step.mark -and $before.phase -ceq 'before' -and
            $after.phase -ceq 'after' -and $before.first -ge $previousCutoff -and $before.last -eq $step.mark -and
            $after.first -ge $step.mark -and $after.last -le $step.cutoff -and $before.completed -le $step.actionStarted -and
            $step.actionStarted -le $step.actionCompleted -and $step.actionCompleted -le $after.started -and
            $after.completed -le $step.cutoffTick -and $snapshots[$s].cutoff -eq $step.cutoff -and
            $snapshots[$s].cutoffTick -eq $step.cutoffTick) 'UIA_STEP_CUTOFF' $stem
        $betweenStepEvents += $step.mark-$previousCutoff
        $window = $providerWindows[$s]
        Assert-Evidence ($window.step -eq $s+1 -and $window.phase -ceq 'after' -and $window.capacity -eq 1024 -and
            $window.overflow -eq 0 -and $window.first -le $window.stepFirst -and $window.stepFirst -le $window.actionFirst -and
            $window.actionFirst -le $window.last -and $window.last -le 1024) 'UIA_PROVIDER_WINDOW' $stem
    }
    Assert-Evidence ($steps[14].mark -eq $steps[14].cutoff -and $providerWindows[14].last -eq $providerWindows[14].actionFirst) 'UIA_UNCHANGED_SELECTION' $stem
    $tail = $tails[0]
    Assert-Evidence ($tail.step -eq 16 -and $tail.mark -eq $steps[15].mark -and $tail.cutoff -ge $steps[15].cutoff -and
        $tail.cutoffTick -ge $steps[15].cutoffTick -and $snapshots[16].cutoff -eq $tail.cutoff -and
        $snapshots[16].cutoffTick -eq $tail.cutoffTick) 'UIA_TAIL_CUTOFF' $stem
    $sequences = [Collections.Generic.HashSet[long]]::new()
    $maximumInFlight = 0L
    $previousSnapshotEntries = 0L
    $previousSnapshotCompleted = 0
    $lastSnapshot = $snapshots[16]
    for ($s = 0; $s -lt $snapshots.Count; $s++) {
        $snapshot = $snapshots[$s]
        # Entries counts callbacks that started, including callbacks not yet stored. A transient excess can
        # precede the source's overflow flag; reject the promised bound without asserting that it lost an event.
        Assert-Evidence ($snapshot.entries -le 1024) 'UIA_DIAGNOSTIC_BOUND' $stem
        Assert-Evidence ($snapshot.entries -ge $previousSnapshotEntries) 'UIA_CALLBACK_SEQUENCE' "$stem cumulative callback entries decreased"
        Assert-Evidence ($snapshot.capacity -eq 1024 -and $snapshot.overflow -eq 0 -and $snapshot.failed -eq 0 -and
            $snapshot.entries -ge $snapshot.completed -and $snapshot.completed -eq $snapshot.cutoff -and
            $snapshot.completed -le 1024 -and $snapshot.completed -ge $previousSnapshotCompleted) 'UIA_DIAGNOSTIC_LOSS' $stem
        $maximumInFlight = [Math]::Max($maximumInFlight, $snapshot.entries-$snapshot.completed)
        $printed = @($arrivals | Where-Object { $_.snapshot -eq $s })
        Assert-Evidence ($printed.Count -eq $snapshot.completed-$previousSnapshotCompleted) 'UIA_CALLBACK_INDEX' $stem
        for ($j = 0; $j -lt $printed.Count; $j++) {
            $arrival = $printed[$j]
            Assert-Evidence ($arrival.eventIndex -eq $previousSnapshotCompleted+$j) 'UIA_CALLBACK_INDEX' $stem
            # Entry sequence is allocated before the receiver mutex: completion order may differ.
            Assert-Evidence ($arrival.sequence -gt 0 -and $arrival.sequence -le $snapshot.entries -and
                $sequences.Add($arrival.sequence)) 'UIA_CALLBACK_SEQUENCE' $stem
            # An entry may predate this action or its mark. Retain it; do not attribute it to the action.
            Assert-Evidence ($arrival.entered -le $arrival.completed -and $arrival.completed -le $snapshot.cutoffTick) 'UIA_CALLBACK_CUTOFF' $stem
        }
        $previousSnapshotEntries = $snapshot.entries
        $previousSnapshotCompleted = $snapshot.completed
    }
    Assert-Evidence ($arrivals.Count -gt 0 -and $arrivals.Count -eq $lastSnapshot.completed) 'UIA_CALLBACK_INDEX' $stem
    $previousProviderCount = 0
    for ($s = 0; $s -lt 16; $s++) {
        $window = $providerWindows[$s]
        $printed = @($attempts | Where-Object { $_.window -eq $s })
        Assert-Evidence ($window.first -eq $previousProviderCount -and $printed.Count -eq $window.last-$window.first) 'UIA_PROVIDER_SEQUENCE' $stem
        $currentAfterSeen = $false
        for ($j = 0; $j -lt $printed.Count; $j++) {
            $attempt = $printed[$j]
            Assert-Evidence ($attempt.sequence -eq $window.first+$j+1 -and $attempt.thread -gt 0) 'UIA_PROVIDER_SEQUENCE' $stem
            if ($attempt.sequence -le $window.stepFirst) {
                $previousPhase = if ($window.step -eq 1) { 'before' } else { 'after' }
                Assert-Evidence ($attempt.step -eq $window.step-1 -and $attempt.phase -ceq $previousPhase) 'UIA_PROVIDER_PHASE' $stem
            } elseif ($attempt.sequence -le $window.actionFirst) {
                Assert-Evidence ($attempt.step -eq $window.step -and $attempt.phase -ceq 'before') 'UIA_PROVIDER_PHASE' $stem
            } else {
                Assert-Evidence ($attempt.step -eq $window.step -and $attempt.phase -cin @('action','after') -and
                    -not ($currentAfterSeen -and $attempt.phase -ceq 'action')) 'UIA_PROVIDER_PHASE' $stem
                if ($attempt.phase -ceq 'after') { $currentAfterSeen = $true }
            }
        }
        $previousProviderCount = $window.last
    }
    $inFlight = $lastSnapshot.entries-$lastSnapshot.completed
    $timelineComplete = $inFlight -eq 0
    [ordered]@{ file=$result.log; kind=$result.kind; processId=$metadata.processId; steps=$steps.ToArray(); arrivals=$arrivals.Count;
        maximumRecordedCallbackMs=($arrivals.callbackMs | Measure-Object -Maximum).Maximum;
        unchangedClientEvents=0; unchangedProviderAttempts=0; diagnosticLoss=0;
        callbackEntries=$lastSnapshot.entries; completedCallbacks=$lastSnapshot.completed; inFlightCallbacks=$inFlight;
        maximumInFlightCallbacks=$maximumInFlight; callbackTimelineComplete=$timelineComplete;
        timelineCompleteness=$(if ($timelineComplete) { 'complete-retained-prefix' } else { 'in-flight-at-final-cutoff' });
        betweenStepClientEvents=$betweenStepEvents;
        betweenStepProviderAttempts=($providerWindows | ForEach-Object { $_.stepFirst-$_.first } | Measure-Object -Sum).Sum;
        beforeActionProviderAttempts=($providerWindows | ForEach-Object { $_.actionFirst-$_.stepFirst } | Measure-Object -Sum).Sum;
        unchangedProviderBeforeActionAttempts=$providerWindows[14].actionFirst-$providerWindows[14].stepFirst;
        providerAttempts=$attempts.Count; processWallSeconds=($completed-$started).TotalSeconds }
})
$analysis = [ordered]@{ purpose='bounded-uia-selection-investigation'; runs=$runs; wallSeconds=$completion.wallSeconds;
    originalFailureCause='unproven'; limits='The original archived failure is preserved. Successful repetitions establish these observations only. A quiet interval does not rule out later repeats; provider attempts do not prove delivery or event identity. Completeness describes the retained completed callback prefix, not all later callbacks. In-flight entries are reported separately; callback timings use coarse GetTickCount64 ticks.' }
[IO.File]::WriteAllText((Join-Path $PacketRoot 'analysis.txt'), ((ConvertTo-Json $analysis -Depth 12) + [char]10), [Text.UTF8Encoding]::new($false))
Write-Host 'Verified eleven fixed UIA runs and unchanged-selection assertions; original failure cause remains unproven.'
