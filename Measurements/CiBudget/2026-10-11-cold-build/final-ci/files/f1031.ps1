<# Independent read-only replay of retained PR73 run399 measurements.
The only writes are new derived files under OutputDirectory, outside the original collection.
Successful replay is not qualification, CI success, approval, or a new measurement. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $CollectionRoot,
    [Parameter(Mandatory)][string] $OutputDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$CollectionRoot = [IO.Path]::GetFullPath($CollectionRoot)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$external = [IO.Path]::GetFullPath('C:/Users/eric/.codex/tmp/dpf-s0-s2-merge')
if (-not $OutputDirectory.StartsWith($external + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
    $OutputDirectory.StartsWith($CollectionRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
    [string]::Equals($OutputDirectory, $CollectionRoot, [StringComparison]::OrdinalIgnoreCase) -or
    (Test-Path -LiteralPath $OutputDirectory)) { throw 'Use a new external output directory outside the sealed collection.' }
$utf8 = [Text.UTF8Encoding]::new($false, $true)
function Require([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
function Digest([byte[]] $Bytes) { [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)) }
function Read-Object([string] $Path) { $utf8.GetString([IO.File]::ReadAllBytes($Path)) | ConvertFrom-Json -AsHashtable }
function Json([object] $Value) { ConvertTo-Json -InputObject $Value -Depth 60 -Compress }
function UInt([object] $Value, [string] $Label) {
    Require ($Value -is [int] -or $Value -is [long] -or $Value -is [uint32] -or $Value -is [uint64] -or $Value -is [bigint]) "$Label must be a typed integer."
    Require ([bigint]$Value -ge 0 -and [bigint]$Value -le [bigint][uint64]::MaxValue) "$Label is outside UInt64."
}
function Int64([object] $Value, [string] $Label) {
    Require ($Value -is [int] -or $Value -is [long] -or $Value -is [bigint]) "$Label must be a typed integer."
    Require ([bigint]$Value -ge [bigint][long]::MinValue -and [bigint]$Value -le [bigint][long]::MaxValue) "$Label is outside Int64."
}
function Instant([object] $Value) {
    if ($Value -is [DateTime]) { return [DateTimeOffset]::new($Value.ToUniversalTime()) }
    if ($Value -is [DateTimeOffset]) { return $Value }
    [DateTimeOffset]::Parse([string]$Value, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::RoundtripKind)
}
function Verify-Seal {
    $entries = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
    foreach ($line in [IO.File]::ReadAllLines((Join-Path $CollectionRoot 'SHA256SUMS'))) {
        Require ($line -cmatch '^(?<hash>[a-f0-9]{64})  (?<path>.+)$') 'Malformed original seal.'
        $hash = $Matches.hash; $relative = $Matches.path
        $path = [IO.Path]::GetFullPath((Join-Path $CollectionRoot $relative))
        Require ($path.StartsWith($CollectionRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) 'Original seal path escapes the packet.'
        Require (-not $entries.ContainsKey($relative)) 'Repeated original seal entry.'
        Require ((Digest ([IO.File]::ReadAllBytes($path))).ToLowerInvariant() -ceq $hash) "Original sealed bytes changed: $relative"
        $entries.Add($relative, $hash)
    }
    return ,$entries
}
$started = [DateTimeOffset]::UtcNow
$sealPath = Join-Path $CollectionRoot 'SHA256SUMS'
$sealHash = Digest ([IO.File]::ReadAllBytes($sealPath))
$seal = Verify-Seal
$collection = Read-Object (Join-Path $CollectionRoot 'collection.receipt.txt')
$repo = [IO.Path]::GetFullPath([string]$collection.repositoryRoot)
function Verify-Source {
    $head = (& git -C $repo rev-parse HEAD).Trim()
    Require ($LASTEXITCODE -eq 0 -and $head -ceq $collection.expectedHead) 'Local source HEAD differs from the collected head.'
    $tree = (& git -C $repo rev-parse 'HEAD^{tree}').Trim()
    Require ($LASTEXITCODE -eq 0 -and $tree -ceq $collection.testedTree) 'Local source tree differs from the tested merge tree.'
    $status = @(& git -C $repo status --porcelain --untracked-files=normal)
    Require ($LASTEXITCODE -eq 0 -and $status.Count -eq 0) 'Local source is not frozen and clean.'
    [ordered]@{ head=$head; tree=$tree; clean=$true }
}
$sourceBefore = Verify-Source
foreach ($key in @('runId','attempt')) { UInt $collection[$key] "collection/$key" }
Require ($collection.runId -eq 38172114803 -and $collection.attempt -eq 1 -and
    $collection.expectedHead -ceq '4bd3480170140090ab2d8ff883d05507281b52ee' -and
    $collection.expectedBase -ceq 'ec680e866a218b57000a1780018c00101a437f91' -and
    $collection.testedMerge -ceq '80d875a1de91d2b2c8602faa5614cf75eab6025b' -and
    $collection.testedTree -ceq 'e695cf74db0e2fc1a5c8c8d183cfb0bd136d639d') 'This driver is scoped to the exact run399 collection.'
$merge = Read-Object (Join-Path $CollectionRoot 'merge-commit.receipt.txt')
Require ($merge.sha -ceq $collection.testedMerge -and $merge.tree.sha -ceq $collection.testedTree -and
    @($merge.parents).Count -eq 2 -and $merge.parents[0].sha -ceq $collection.expectedBase -and
    $merge.parents[1].sha -ceq $collection.expectedHead) 'Actual merge object does not bind the ordered PR parents/tree.'
$jobs = Read-Object (Join-Path $CollectionRoot 'jobs.receipt.txt')
$job = @($jobs | Where-Object { $_.id -eq 114566698380 })
Require ($job.Count -eq 1) 'Paired job inventory is not unique.'
$job = $job[0]
foreach ($key in @('id','run_id','run_attempt')) { UInt $job[$key] "job/$key" }
Require ($job.run_id -eq $collection.runId -and $job.run_attempt -eq 1 -and
    $job.status -ceq 'completed' -and $job.conclusion -ceq 'failure') 'Original paired job state differs.'
$failures = @($job.steps | Where-Object { $_.conclusion -cin @('failure','cancelled','timed_out','action_required') })
Require ($failures.Count -eq 1 -and $failures[0].name.Contains('Publish-BenchmarkVerdict.ps1')) 'The paired job has a failure beyond verdict publication.'
$measure = @($job.steps | Where-Object { $_.name -ceq 'Run $pairedArgs = @{' })
Require ($measure.Count -eq 1 -and $measure[0].conclusion -ceq 'success') 'Original measurement step did not succeed.'
$artifactRoot = Join-Path $CollectionRoot 'artifacts/paired-benchmark-x64-Release'
$reports = Join-Path $artifactRoot 'paired/20261011T202547Z-ec680e866a21/reports'
$summaryPath = Join-Path $reports 'summary.json'
$summaryBytes = [IO.File]::ReadAllBytes($summaryPath)
$summary = $utf8.GetString($summaryBytes) | ConvertFrom-Json -AsHashtable
$recordedVerdict = Read-Object (Join-Path $reports 'verdict.json')
Require ($recordedVerdict.Conclusion -ceq 'inconclusive' -and $recordedVerdict.PolicyStatus -ceq 'policy-review-required') 'Original failed qualification verdict differs.'
Require ($summary.studyVersion -eq 3 -and $summary.studyPurpose -ceq 'regression-qualification' -and
    $summary.blocks -eq 12 -and $summary.platform -ceq 'x64' -and $summary.configuration -ceq 'Release') 'Study/profile protocol differs.'
UInt $summary.blocks 'summary/blocks'; UInt $summary.seed 'summary/seed'
Require ($summary.seed -le [int]::MaxValue) 'Seed exceeds the native generator input range.'
Require ($summary.baseline.commit -ceq $collection.expectedBase -and $summary.candidate.commit -ceq $collection.testedMerge -and
    $summary.harnessCommit -ceq $collection.testedMerge -and $summary.harnessDirty -is [bool] -and $summary.harnessDirty -eq $false) 'Study role or harness source binding differs.'
$moduleFiles = @('Tools/PerformanceComparison.psm1','Tools/PerformancePolicy.psm1','Tools/PairedRun.psm1','Tools/BenchmarkGate.psm1',
    'Tools/ScopedTesting.psm1','Specs/Core/Core_PerformanceAndResources.md','Specs/Testing/Testing_Validation.md','docs/performance.md')
$moduleHashes = [ordered]@{}
foreach ($path in $moduleFiles) { $moduleHashes[$path] = (Get-FileHash -LiteralPath (Join-Path $repo $path)).Hash }
Import-Module (Join-Path $repo 'Tools/PerformanceComparison.psm1') -Force
Import-Module (Join-Path $repo 'Tools/PerformancePolicy.psm1') -Force
Import-Module (Join-Path $repo 'Tools/PairedRun.psm1') -Force
$gate = Import-Module (Join-Path $repo 'Tools/BenchmarkGate.psm1') -Force -PassThru
function Same([object] $Left, [object] $Right) { & $gate { param($l,$r) Test-EquivalentGateDecision $l $r } $Left $Right }
$policy = Read-Object (Join-Path $CollectionRoot 'source/base-policy.receipt.txt')
$candidatePolicy = Read-Object (Join-Path $CollectionRoot 'source/candidate-policy.receipt.txt')
Require ($policy.qualified -is [bool] -and $policy.qualified -eq $false -and (Same $policy $candidatePolicy)) 'Original unqualified policy changed.'
$immutablePolicyLines = @(& git -C $repo show "$($collection.expectedBase):Tools/PerformanceAcceptancePolicy.v1.json")
Require ($LASTEXITCODE -eq 0) 'Immutable base policy is unavailable.'
$immutablePolicy = ($immutablePolicyLines -join [char]10) | ConvertFrom-Json -AsHashtable
Require (Same $policy $immutablePolicy) 'Collected policy differs from immutable base policy.'
$judges = Get-VersionedPerformanceJudge -RepositoryRoot $repo -BaselineCommit $summary.baseline.commit -CandidateModulePath (Join-Path $repo 'Tools/PerformanceComparison.psm1')
Require ($judges.status -ceq 'available' -and $judges.baseJudgeSha256 -ceq $summary.judgeProvenance.baseJudgeSha256 -and
    $judges.candidateJudgeSha256 -ceq $summary.judgeProvenance.candidateJudgeSha256 -and
    $judges.candidateJudgeSha256 -ceq $policy.approvedJudgeSha256 -and $judges.candidateJudgeVersion -ceq $policy.judgeVersion) 'Frozen judge source/version attestation differs.'
foreach ($side in @('base','candidate')) {
    $text = [IO.File]::ReadAllText((Join-Path $CollectionRoot "source/$side-judge.psm1")) -replace '\r\n', "`n"
    $expected = if ($side -ceq 'base') { $judges.baseJudgeSha256 } else { $judges.candidateJudgeSha256 }
    Require ((Digest ($utf8.GetBytes($text))) -ceq $expected) 'Collected judge text differs from its immutable source.'
}
$plan = Get-RandomizedPairedBlockSchedule -Blocks ([int]$summary.blocks) -Seed ([int]$summary.seed)
Require ($summary.allocation -ceq $plan.Allocation -and $summary.order -ceq $plan.Order -and
    (Same $summary.blockSchedule $plan.Blocks) -and (Same $summary.steps $plan.Steps)) 'Independent seeded schedule replay differs.'
$harness = @(Get-PairedHarness) + @('build.ps1','Tools/ScopedTesting.psm1','Tools/Validation.psm1','Tools/Changelog.psm1')
Require ($summary.harness.Count -eq $harness.Count) 'Shared harness input count differs.'
foreach ($path in $harness) { Require ($summary.harness.Contains($path) -and
    (Get-FileHash -LiteralPath (Join-Path $repo $path)).Hash -ceq $summary.harness[$path]) "Shared harness bytes differ: $path" }
$supportedOverlays = $harness + @('Tests/Embedded/BenchmarkMain.h','Tests/Embedded/ComplexUiBenchmark.h','Tests/Support/HeapDiagnostic.h')
foreach ($role in @('baseline','candidate')) {
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($overlay in $summary[$role].harnessOverlay) {
        Require ([string]$overlay -cmatch '^(created|replaced) (.+)$') 'Unsupported harness-overlay record.'
        $path = $Matches[2]
        Require ($path -cin $supportedOverlays -and $seen.Add($path)) 'Repeated or unsupported overlaid path.'
    }
}
$fields = @('sourceCommit','sourceFingerprint','sourceDirty','executableSha256','benchmarkSha256','harnessIdentity','toolchainIdentity',
    'dependencyIdentity','environmentIdentity','resolvedBuildIdentity','buildInputIdentity','buildArtifactIdentity','buildAttestationIdentity',
    'buildIdentityVersion','identityScope','identityStatus','platform','configuration','nativeArchitecture','machine','cpu','os','compiler',
    'warpVersion','warpSha256','powerPolicy','clock','benchmarkInputs')
$fixtures = @{ Default='dxui-complex-ui-v2'; MultilineGrid='dxui-complex-ui-multiline-grid-v1'; MultilineGridDistinct='dxui-complex-ui-multiline-grid-distinct-v1' }
$expectedFixture = @{ renderer='WARP'; width=1280; height=720; dpi=96; controls=83; modelRows=1000; framesPerRound=40; roundCount=5;
    platform='x64'; configuration='Release'; dirtyAllocationCeilingPerFrame=64 }
Require (@($summary.scenarios).Count -eq 3 -and (@($summary.scenarios | ForEach-Object { $_.scenario } | Sort-Object) -join ',') -ceq
    'Default,MultilineGrid,MultilineGridDistinct') 'Required scenario inventory differs.'
$sideStates = @{}; $results = [Collections.Generic.List[object]]::new(); $times = [Collections.Generic.List[object]]::new()
$receiptCount=0; $roundCount=0; $decisionCount=0; $controlCount=0; $exactCount=0; $v2Count=0; $driftCells=0
$previousUtc = [DateTimeOffset]::MinValue
foreach ($scenario in $summary.scenarios) {
    $name = [string]$scenario.scenario
    $byRun = @{}; $hashes = @{}; $rounds = [Collections.Generic.List[object]]::new()
    Require ($scenario.judgeComparison.reportBytesStable -is [bool] -and $scenario.judgeComparison.reportBytesStable) 'Original reports were not recorded stable.'
    Require ($scenario.judgeComparison.verdictAgreement -is [bool]) 'Agreement is not a typed original boolean.'
    foreach ($record in $scenario.judgeComparison.sameReceiptsSha256) {
        Require (-not $hashes.ContainsKey($record.run)) 'Repeated judged report.'
        $hashes[$record.run] = $record.sha256
    }
    Require ($hashes.Count -eq 48 -and $scenario.runs.Count -eq 48) 'Scheduled receipt or retained identity inventory differs.'
    foreach ($step in $summary.steps) {
        $run = [string]$step.Name; $role = [string]$step.Side; $path = Join-Path $reports "$name-$run.json"
        Require ($hashes.Contains($run) -and $scenario.runs.Contains($run)) 'A scheduled raw receipt is missing.'
        $bytes = [IO.File]::ReadAllBytes($path)
        Require ((Digest $bytes) -ceq $hashes[$run]) "Judged raw hash differs: $name-$run"
        # Decode exactly this hashed buffer once; all judges/control comparisons below share this object.
        $receipt = $utf8.GetString($bytes) | ConvertFrom-Json -AsHashtable
        Assert-PerformanceReceipt -Receipt $receipt
        $identity = Test-PerformanceReceiptIdentity -Receipt $receipt
        Require $identity.verifiable "Missing full v2 receipt identity: $name-$run ($($identity.missing -join ', '))"
        $v2Count++
        Require ($receipt.sourceCommit -ceq $summary[$role].commit -and $receipt.sourceFingerprint -ceq $summary[$role].sourceFingerprint) 'Scheduled role differs from raw source identity.'
        Require ($receipt.sourceDirty -is [bool] -and $receipt.buildSkipped -is [bool]) 'Dirty/build-skipped metadata is not typed.'
        if ($receipt.sourceDirty) { Require (@($summary[$role].harnessOverlay).Count -gt 0) 'Raw dirty source lacks a supported overlay explanation.' }
        foreach ($key in $fields) { Require ($receipt.Contains($key)) "Missing raw identity field: $key" }
        if (-not $sideStates.Contains($role)) {
            $state = [ordered]@{}; foreach ($key in $fields) { $state[$key] = $receipt[$key] }; $sideStates[$role]=$state
        } else { foreach ($key in $fields) { Require (Same $sideStates[$role][$key] $receipt[$key]) "Raw role identity changed: $role/$key" } }
        foreach ($key in @('sourceCommit','sourceDirty','sourceFingerprint','executableSha256','benchmarkSha256')) {
            Require ($scenario.runs[$run].Contains($key) -and (Same $scenario.runs[$run][$key] $receipt[$key])) 'Retained run identity does not bind the exact raw report.'
        }
        foreach ($key in $fields) {
            if ($scenario.runs[$run].Contains($key)) { Require (Same $scenario.runs[$run][$key] $receipt[$key]) 'Optional retained run field differs.' }
            if ($summary[$role].Contains($key)) { Require (Same $summary[$role][$key] $receipt[$key]) 'Optional retained side field differs.' }
        }
        Require ($receipt.fixture -ceq $fixtures[$name] -and [string]::Equals([string]$receipt.nativeArchitecture,'x64',[StringComparison]::OrdinalIgnoreCase)) 'Native fixture or architecture differs.'
        foreach ($key in $expectedFixture.Keys) { Require (Same $receipt[$key] $expectedFixture[$key]) "Literal raw fixture/profile differs: $key" }
        foreach ($key in @('width','height','dpi','controls','modelRows','framesPerRound','roundCount','dirtyAllocationCeilingPerFrame','compiler','buildIdentityVersion','hiddenPreparations','hiddenComposites')) { UInt $receipt[$key] "raw/$key" }
        Require ($receipt.hiddenPreparations -eq 0 -and $receipt.hiddenComposites -eq 0) 'Absolute hidden-work budget differs.'
        $utc = Instant $receipt.completedUtc
        Require ($utc -ge (Instant $job.started_at) -and $utc -le (Instant $job.completed_at)) 'Raw receipt completed outside its original job.'
        Require ($utc -gt $previousUtc) 'Raw completion order differs from the native driver scenario-outer/step-inner order.'
        $previousUtc=$utc; $times.Add([ordered]@{ scenario=$name; run=$run; position=$step.Position; completedUtc=$utc })
        foreach ($phase in $receipt.scenarios) {
            Require (@($phase.rounds).Count -eq 5) 'Raw five-round inventory differs.'
            foreach ($round in $phase.rounds) {
                foreach ($key in @('cppAllocations','composeAllocations','surfaceBytes','replacementPeakBytes','privateBytes','privatePeakBytes','workingSetBytes','workingSetPeakBytes')) { UInt $round[$key] "round/$key" }
                # The native fixture serializes signed end-minus-start private memory, including releases.
                Int64 $round.privateGrowthBytes 'round/privateGrowthBytes'
                Require ($round.composeAllocations -eq 0) 'Absolute composition allocation budget differs.'
                if ($phase.name -ceq 'clean') { Require ($round.cppAllocations -eq 0) 'Absolute clean allocation budget differs.' }
                else { Require ($phase.name -ceq 'dirty' -and $round.cppAllocations -le 2560) 'Absolute Release dirty allocation budget exceeds 64/frame and 2560/round.' }
                $rounds.Add([ordered]@{ role=$role; phase=$phase.name; run=$run; value=$round }); $roundCount++
            }
        }
        $byRun[$run]=$receipt; $receiptCount++
    }
    $blocks = @($summary.blockSchedule | ForEach-Object { [ordered]@{ name=$_.Name; order=$_.Order;
        Baseline=@($_.BaselineRuns | ForEach-Object { $byRun[$_] }); Candidate=@($_.CandidateRuns | ForEach-Object { $byRun[$_] }) } })
    $a=@($blocks | ForEach-Object { $_.Baseline }); $b=@($blocks | ForEach-Object { $_.Candidate })
    $legacy = & $judges.module { param($a,$b) Compare-PerformanceSet -Baseline $a -Candidate $b } $a $b
    $migrated = & $judges.candidateModule { param($b,$p) Compare-PairedBlockSet -Blocks $b -MinimumBlocks 12 -AssignmentProtocol $p } $blocks $summary.allocation
    Require (Same $legacy $scenario.judgeComparison.legacyJudge) 'Complete legacy decision differs from raw replay.'
    Require (Same $migrated $scenario.judgeComparison.migratedJudge) 'Complete migrated decision differs from raw replay.'
    Require (Same $migrated $scenario.set) 'Consumed complete set differs from raw replay.'
    $agrees = & $gate { param($l,$r) Test-EquivalentPerformanceVerdicts $l $r } $legacy $migrated
    Require ($agrees -eq $scenario.judgeComparison.verdictAgreement) 'Original agreement flag does not describe the replayed decisions.'
    $differences=[Collections.Generic.List[object]]::new()
    foreach ($metric in $migrated.metrics) {
        $old=@($legacy.metrics | Where-Object { $_.phase -ceq $metric.phase -and $_.metric -ceq $metric.metric })
        Require ($old.Count -eq 1) 'Legacy metric inventory differs.'
        if ($old[0].verdict -cne $metric.verdict) { $differences.Add([ordered]@{ phase=$metric.phase; metric=$metric.metric; legacy=$old[0]; migrated=$metric }) }
    }
    $expectedControls=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($block in $summary.blockSchedule) { foreach ($role in @('baseline','candidate')) { [void]$expectedControls.Add("$name-$($block.Name)-$role-control") } }
    $retainedControls=@($scenario.comparisons | Where-Object { ([string]$_.name).EndsWith('-control',[StringComparison]::Ordinal) })
    Require ($retainedControls.Count -eq 24) 'Control record count differs.'
    $controlNames=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $byControl=@{}
    foreach ($control in $retainedControls) {
        Require ($expectedControls.Contains($control.name) -and $controlNames.Add($control.name) -and $control.file -ceq "$($control.name).comparison.json") 'Scheduled control identity differs.'
        $byControl[$control.name]=$control
    }
    $driftMetrics=@{}; $unstable=0; $controls=[Collections.Generic.List[object]]::new(); $driftedObservationCount=0
    foreach ($block in $summary.blockSchedule) {
        foreach ($role in @('Baseline','Candidate')) {
            $controlName="$name-$($block.Name)-$($role.ToLowerInvariant())-control"; $control=$byControl[$controlName]
            $pair=$block["${role}Runs"]; $controlPath=Join-Path $reports $control.file
            $controlBytes=[IO.File]::ReadAllBytes($controlPath)
            $retained=$utf8.GetString($controlBytes) | ConvertFrom-Json -AsHashtable
            $replayed=& $judges.candidateModule { param($a,$b) Compare-PerformanceReceipt -Before $a -After $b } $byRun[$pair[0]] $byRun[$pair[1]]
            Require (Same $retained $replayed) "Complete control comparison differs: $controlName"
            $drifted=& $gate { param($m,$c) Add-ControlMetricChanges -Metrics $m -Changes $c } $driftMetrics $replayed.changes
            $actualStatus=if($drifted){'unstable-control'}else{'stable-control'}
            Require ($control.status -ceq $actualStatus) "Retained control stability differs: $controlName"
            if($drifted){$unstable++}
            $outside=@($replayed.changes | Where-Object { if($null -eq $_.changePercent){[double]$_.before -ne [double]$_.after}else{[Math]::Abs([double]$_.changePercent) -gt [double]$_.noisePercent} })
            $driftedObservationCount += $outside.Count
            $controls.Add([ordered]@{ name=$controlName; beforeRun=$pair[0]; afterRun=$pair[1]; retainedSha256=(Digest $controlBytes);
                status=$actualStatus; outsideBand=$outside; fullReplayedComparison=$replayed })
            Require ((Digest ([IO.File]::ReadAllBytes($controlPath))) -ceq (Digest $controlBytes)) 'Control bytes changed during replay.'
        }
    }
    Require ($driftMetrics.Count -eq 26) 'Control metric inventory differs.'
    foreach ($key in $driftMetrics.Keys) { Require ($driftMetrics[$key].Observed -eq 24) 'A control did not measure every declared slot.' }
    $drift=[ordered]@{ Total=24; Unstable=$unstable; Missing=0; Metrics=$driftMetrics }
    $projection=& $gate { param($entry,$dir,$drift) Get-ScenarioConclusion -Entry $entry -ReportsDirectory $dir -Unchanged $false -Strict $false -ReplayedControls $drift } $scenario $reports $drift
    $strictProjection=& $gate { param($entry,$dir,$drift) Get-ScenarioConclusion -Entry $entry -ReportsDirectory $dir -Unchanged $false -Strict $true -ReplayedControls $drift } $scenario $reports $drift
    $exact=@($migrated.metrics | Where-Object exact)
    Require (@($migrated.metrics).Count -eq 26 -and $exact.Count -eq 8) 'Declared metric/exact-budget inventory differs.'
    foreach ($metric in $exact) { Require (-not $metric.deterministicRise -and $metric.verdict -cne 'regressed') 'Deterministic median or peak budget grew.' }
    $budgets=[Collections.Generic.List[object]]::new()
    foreach ($role in @('baseline','candidate')) {
        foreach ($phase in @('clean','dirty')) {
            $selected=@($rounds | Where-Object { $_.role -ceq $role -and $_.phase -ceq $phase })
            $budgets.Add([ordered]@{ role=$role; phase=$phase; roundCount=$selected.Count;
                cppMinimum=($selected.value.cppAllocations | Measure-Object -Minimum).Minimum; cppMaximum=($selected.value.cppAllocations | Measure-Object -Maximum).Maximum;
                composeMaximum=($selected.value.composeAllocations | Measure-Object -Maximum).Maximum; surfaceMinimum=($selected.value.surfaceBytes | Measure-Object -Minimum).Minimum;
                surfaceMaximum=($selected.value.surfaceBytes | Measure-Object -Maximum).Maximum; replacementMaximum=($selected.value.replacementPeakBytes | Measure-Object -Maximum).Maximum })
        }
    }
    $rawFlags=@($migrated.metrics | Where-Object { $_.rawRegressed -and -not $_.exact })
    $heldRegressions=@($projection.Metrics | Where-Object { $_.Verdict -ceq 'regressed' -and $_.Held })
    $confirmed=@($projection.Metrics | Where-Object { $_.Outcome -ceq 'confirmed' })
    $results.Add([ordered]@{ scenario=$name; fullLegacyReproduced=$true; fullMigratedReproduced=$true; consumedSetReproduced=$true;
        originalVerdictAgreement=$scenario.judgeComparison.verdictAgreement; actualVerdictAgreement=$agrees; differences=$differences.ToArray();
        legacyDecision=$legacy; migratedDecision=$migrated; controls=$controls.ToArray(); controlSummary=$drift; driftedControlCells=$driftedObservationCount;
        diagnosticProjection=$projection; strictDiagnosticProjection=$strictProjection; heldRegressedMetrics=$heldRegressions; confirmedMetrics=$confirmed;
        rawBeyondWorseBand=$rawFlags; exactBudgets=$exact; absoluteRawBudgetRanges=$budgets.ToArray() })
    $decisionCount+=@($migrated.metrics).Count; $controlCount+=24; $exactCount+=$exact.Count; $driftCells+=$driftedObservationCount
    foreach ($record in $scenario.judgeComparison.sameReceiptsSha256) {
        Require ((Digest ([IO.File]::ReadAllBytes((Join-Path $reports "$name-$($record.run).json")))) -ceq $record.sha256) 'Raw receipt bytes changed during replay.'
    }
}
Require ($sideStates.Count -eq 2 -and $sideStates.Contains('baseline') -and $sideStates.Contains('candidate')) 'Raw side inventory differs.'
foreach ($key in @('benchmarkSha256','harnessIdentity','toolchainIdentity','dependencyIdentity','environmentIdentity','machine','cpu','os','compiler','warpVersion','warpSha256','powerPolicy','clock','benchmarkInputs')) {
    Require (Same $sideStates.baseline[$key] $sideStates.candidate[$key]) "Shared comparison identity differs across roles: $key"
}
$candidateFingerprint=Get-SourceFingerprint -Root $repo
Require ($candidateFingerprint -ceq $sideStates.candidate.sourceFingerprint) 'Current compiled-library input fingerprint does not reproduce candidate raw identity.'
Require ($receiptCount -eq 144 -and $roundCount -eq 1440 -and $decisionCount -eq 78 -and $controlCount -eq 72 -and $exactCount -eq 24 -and $v2Count -eq 144) 'Incomplete final replay inventory.'
$sourceAfter=Verify-Source
Require (Same $sourceBefore $sourceAfter) 'Source identity changed during replay.'
foreach ($path in $moduleFiles) { Require ((Get-FileHash -LiteralPath (Join-Path $repo $path)).Hash -ceq $moduleHashes[$path]) 'An imported source/contract changed during replay.' }
foreach ($path in $harness) { Require ((Get-FileHash -LiteralPath (Join-Path $repo $path)).Hash -ceq $summary.harness[$path]) 'A shared harness file changed during replay.' }
$sealAfter=Verify-Seal
Require ($sealAfter.Count -eq $seal.Count -and (Digest ([IO.File]::ReadAllBytes($sealPath))) -ceq $sealHash) 'Original evidence seal changed during replay.'
$disagreementCount=@($results | Where-Object { -not $_.actualVerdictAgreement }).Count
$heldCount=($results | ForEach-Object { @($_.heldRegressedMetrics).Count } | Measure-Object -Sum).Sum
$confirmedCount=($results | ForEach-Object { @($_.confirmedMetrics).Count } | Measure-Object -Sum).Sum
$report=[ordered]@{
    evidenceReplayStatus='REPRODUCED_ORIGINAL_BYTES_WITH_CHANGED_VERDICTS'; qualificationPassed=$false; ciGateSucceeded=$false;
    interpretation='Read-only reproduction. Preserve every original agreement=false and failed/inconclusive qualification. Changed verdicts require explicit review; no approval or judge/policy relaxation is supplied here.';
    startedUtc=$started; completedUtc=[DateTimeOffset]::UtcNow; collection=$CollectionRoot; driver=$PSCommandPath; driverSha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash;
    originalSealSha256=$sealHash; originalSealedFiles=$seal.Count; originalSummarySha256=(Digest $summaryBytes); sourceBefore=$sourceBefore; sourceAfter=$sourceAfter;
    importedSourcesAndContracts=$moduleHashes; expectedBase=$collection.expectedBase; expectedHead=$collection.expectedHead; testedMerge=$collection.testedMerge; testedTree=$collection.testedTree;
    originalJob=$job; originalQualificationVerdict=$recordedVerdict; basePolicyQualified=$policy.qualified; judgeSourceProvenance=$summary.judgeProvenance;
    seed=$summary.seed; allocation=$summary.allocation; schedule=$summary.blockSchedule; scheduleReplayMatches=$true; actualTimestampOrderMatchesNativeDriver=$true;
    firstReportUtc=$times[0].completedUtc; lastReportUtc=$times[-1].completedUtc; reports=$receiptCount; rawRounds=$roundCount; frameSamples=($roundCount*40);
    metricDecisionsPerJudge=$decisionCount; sameBinaryControls=$controlCount; controlMetricObservations=($controlCount*26); driftedControlMetricObservations=$driftCells;
    deterministicBudgets=$exactCount; fullV2Receipts=$v2Count; harness=$summary.harness; roleIdentities=$sideStates; overlayRecords=[ordered]@{baseline=$summary.baseline.harnessOverlay;candidate=$summary.candidate.harnessOverlay};
    measuredLibraryChanged=($sideStates.baseline.sourceFingerprint -cne $sideStates.candidate.sourceFingerprint); disagreeingScenarios=$disagreementCount;
    changedLibraryHeldRegressions=$heldCount; confirmedOutcomes=$confirmedCount; absoluteCleanCppBudget=0; absoluteDirtyCppBudget=2560; absoluteCompositionCppBudget=0; absoluteHiddenWorkBudget=0;
    uploadedBuildSidecars=@(Get-ChildItem -LiteralPath $artifactRoot -Recurse -File -Filter 'build.json').Count;
    limits=@('Recorded full-v2 identities bind declared aggregate fields; compiler, SDK, dependency and executable byte inventories are not uploaded and cannot be independently rehashed.',
        'Baseline library fingerprint is bound to every scheduled raw A receipt and the summary; this audit does not reconstruct Windows-checkout filtered baseline file bytes from Git objects.',
        'Timing and memory shifts remain descriptive investigation evidence. Method disagreement is not attributed to chance and supplies no speedup or non-regression claim.',
        'The ordinary hosted measurement is not independent policy calibration, consumer adoption, physical hardware, or long-run qualification.');
    scenarios=$results.ToArray()
}
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$output=Join-Path $OutputDirectory 'paired-replay.receipt.txt'
[IO.File]::WriteAllText($output, (($report | ConvertTo-Json -Depth 60) + [char]10), $utf8)
[ordered]@{ output=$output; outputSha256=(Get-FileHash -LiteralPath $output).Hash; replayStatus=$report.evidenceReplayStatus; qualificationPassed=$false;
    reports=$receiptCount; rounds=$roundCount; metricDecisions=$decisionCount; controls=$controlCount; exactBudgets=$exactCount; disagreeingScenarios=$disagreementCount;
    heldRegressions=$heldCount; confirmedOutcomes=$confirmedCount; originalSealUnchanged=$true } | ConvertTo-Json -Depth 5
