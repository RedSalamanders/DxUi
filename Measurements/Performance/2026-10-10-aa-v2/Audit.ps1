# Read-only audit of the retained v2 A/A. No DxUi executable, build, restore or measurement runs.
# The script writes only JSON to standard output. The shared version-1 receipt envelope is
# checked separately from the v2 build identity carried by every native report.
#
# Run from a checkout with the frozen harness bytes. Actual build products remain in the
# original run directory; -ArtifactRoot may point to a byte-preserved copy of its baseline tree.
[CmdletBinding()]
param(
    [string] $RepositoryRoot = ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))),
    [string] $RunRoot = 'C:/Users/eric/.codex/tmp/dpf-aa-final/20261010T174833Z-60e1cbff8f7a',
    [string] $ArtifactRoot = '',
    [string] $ReportsDirectory = $PSScriptRoot,
    [string] $SummaryFileName = 'summary.receipt.txt',
    [string] $BuildReceiptPath = '',
    [string] $IncrementalReceiptPath = '',
    [string] $BuildLogPath = '',
    [ValidatePattern('^[A-Fa-f0-9]{64}$')]
    [string] $ExpectedSummarySha256 = '9C6980663ACD92DC6AD217A6C98E1F5A9162683320C17DCADBECE14A337671D9'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$RunRoot = [IO.Path]::GetFullPath($RunRoot)
if (-not $ReportsDirectory) { $ReportsDirectory = Join-Path $RunRoot 'reports' }
if (-not $BuildReceiptPath) { $BuildReceiptPath = Join-Path $ReportsDirectory 'baseline-build.receipt.txt' }
if (-not $IncrementalReceiptPath) { $IncrementalReceiptPath = Join-Path $ReportsDirectory 'baseline-incremental.receipt.txt' }
if (-not $BuildLogPath) { $BuildLogPath = Join-Path $ReportsDirectory 'baseline-build.log' }
$ReportsDirectory = [IO.Path]::GetFullPath($ReportsDirectory)

function Assert-Audit([bool] $Condition, [string] $Message) {
    if (-not $Condition) { throw $Message }
}
function Read-AuditJson([string] $Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    $text = [Text.UTF8Encoding]::new($false, $true).GetString($bytes)
    if ($text.Length -gt 0 -and $text[0] -eq [char]0xfeff) { $text = $text.Substring(1) }
    $value = ConvertFrom-Json -InputObject $text -AsHashtable -Depth 50
    Assert-Audit ($value -is [System.Collections.IDictionary]) "Not a JSON object: $Path"
    return [ordered]@{bytes=$bytes;value=$value;sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes))}
}

$gate = Import-Module (Join-Path $RepositoryRoot 'Tools/BenchmarkGate.psm1') -Force -PassThru
Import-Module (Join-Path $RepositoryRoot 'Tools/PairedRun.psm1') -Force
Import-Module (Join-Path $RepositoryRoot 'Tools/PerformancePolicy.psm1') -Force
Import-Module (Join-Path $RepositoryRoot 'Tools/ScopedTesting.psm1') -Force

function Test-AuditEquivalent([object] $Left, [object] $Right) {
    return & $gate { param($Left,$Right) Test-EquivalentGateDecision $Left $Right } $Left $Right
}

$summaryPath = Join-Path $ReportsDirectory $SummaryFileName
$summaryPacket = Read-AuditJson $summaryPath
Assert-Audit ($summaryPacket.sha256 -ceq $ExpectedSummarySha256.ToUpperInvariant()) 'Summary bytes differ from the independently observed original hash.'
$summary = $summaryPacket.value
Assert-Audit ($summary['studyVersion'] -eq 3 -and $summary['studyPurpose'] -ceq 'aa-calibration') 'This audit requires the original current-protocol A/A purpose.'
Assert-Audit ($summary['baselineCommit'] -ceq '60e1cbff8f7a75757e9447073965d09101d9833c' -and
    $summary['candidateCommit'] -ceq $summary['baselineCommit']) 'The original A/A revisions differ.'
Assert-Audit ($summary['baseline']['path'] -ceq $summary['candidate']['path']) 'A/A role labels did not declare one physical tree.'
Assert-Audit ($summary['baseline']['sourceFingerprint'] -ceq $summary['candidate']['sourceFingerprint']) 'A/A source fingerprints differ.'
Assert-Audit ($summary['harnessCommit'] -ceq 'e0a57ece2965c848d71c4f94cfef4fae9a196aa9' -and $summary['harnessDirty'] -eq $false) 'The declared original harness was not clean e0a57ec.'
Assert-Audit ($summary['blocks'] -eq 12 -and $summary['seed'] -eq 20261010) 'The original block count or seed differs.'
Assert-Audit ($summary['platform'] -ceq 'x64' -and $summary['configuration'] -ceq 'Release') 'The original build profile differs.'

$expectedScenarios = [Collections.Generic.HashSet[string]]::new([string[]]@(Get-BenchmarkGateScenarios), [StringComparer]::Ordinal)
$actualScenarios = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($scenario in $summary['scenarios']) {
    Assert-Audit ($scenario -is [System.Collections.IDictionary] -and $actualScenarios.Add([string]$scenario['scenario'])) 'Repeated or malformed scenario.'
}
Assert-Audit ($summary['scenarios'].Count -eq 3 -and $actualScenarios.SetEquals($expectedScenarios)) 'The complete three-scenario contract inventory is required.'

$harnessRows = @(foreach ($path in $summary['harness'].Keys) {
    $actual = (Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $path) -Algorithm SHA256).Hash
    Assert-Audit ($actual -ceq $summary['harness'][$path]) "Current frozen harness bytes differ: $path"
    [ordered]@{path=$path;sha256=$actual}
})
Assert-Audit ($harnessRows.Count -eq 17) 'The original seventeen-path harness inventory differs.'

$judges = Get-VersionedPerformanceJudge -RepositoryRoot $RepositoryRoot -BaselineCommit $summary['baselineCommit'] `
    -CandidateModulePath (Join-Path $RepositoryRoot 'Tools/PerformanceComparison.psm1')
Assert-Audit ($judges['status'] -ceq 'available') 'The immutable measured-base and current frozen candidate judges are unavailable.'
foreach ($key in @('baseJudgeSha256','candidateJudgeSha256','baseJudgeVersion','candidateJudgeVersion')) {
    Assert-Audit ($judges[$key] -ceq $summary['judgeProvenance'][$key]) "Declared judge source/version differs: $key"
}
Assert-Audit ($summary['judgeProvenance']['baselineCommit'] -ceq $summary['baselineCommit']) 'Judge provenance names a different base.'
Assert-Audit ($judges['baseJudgeSha256'] -ceq 'EB1B3047ED8B031383AEBA4D9A72A37D3719B1320A46381EFF042897CCB72E33' -and
    $judges['candidateJudgeSha256'] -ceq $judges['baseJudgeSha256']) 'Frozen judge source hashes differ from the reviewed original.'

$schedule = Get-RandomizedPairedBlockSchedule -Blocks $summary['blocks'] -Seed $summary['seed']
Assert-Audit ($summary['allocation'] -ceq $schedule['Allocation']) 'Independent assignment protocol differs.'
Assert-Audit ($summary['order'] -ceq $schedule['Order']) 'Literal retained run order differs from seed replay.'
Assert-Audit (Test-AuditEquivalent $summary['blockSchedule'] $schedule['Blocks']) 'Retained blocks differ from seed replay.'
Assert-Audit (Test-AuditEquivalent $summary['steps'] $schedule['Steps']) 'Retained steps differ from seed replay.'
$expectedRuns = [string[]]@($schedule['Steps'] | ForEach-Object Name)
Assert-Audit ($expectedRuns.Count -eq 48 -and @($expectedRuns | Select-Object -Unique).Count -eq 48) 'The seeded run inventory is not forty-eight unique runs.'

$expectedControlSlots = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$metricNames = & $judges['candidateModule'] { Get-PerformanceMetricNames }
foreach ($phase in @('clean','dirty')) { foreach ($metric in $metricNames) { [void]$expectedControlSlots.Add("$phase/$metric") } }
Assert-Audit ($expectedControlSlots.Count -eq 26) 'The frozen phase/metric family is not twenty-six slots.'

$allReceipts = [Collections.Generic.List[object]]::new()
$rawHashes = [Collections.Generic.List[object]]::new()
$scenarioRows = @()
foreach ($scenario in $summary['scenarios']) {
    $scenarioName = [string]$scenario['scenario']
    $comparison = $scenario['judgeComparison']
    Assert-Audit ($comparison['reportBytesStable'] -eq $true -and $comparison['verdictAgreement'] -eq $true) "Recorded dual-judge stability/agreement failed: $scenarioName"
    Assert-Audit ($comparison['baseJudgeCommit'] -ceq $summary['baselineCommit'] -and
        $comparison['baseJudgeSha256'] -ceq $judges['baseJudgeSha256'] -and
        $comparison['candidateJudgeSha256'] -ceq $judges['candidateJudgeSha256']) "Per-scenario judge provenance differs: $scenarioName"
    $hashRecords = @($comparison['sameReceiptsSha256'])
    Assert-Audit ($hashRecords.Count -eq 48 -and $scenario['runs'].Count -eq 48) "Incomplete raw report inventory: $scenarioName"
    $byRun = @{}
    foreach ($record in $hashRecords) {
        $runName = [string]$record['run']
        Assert-Audit ($runName -cin $expectedRuns -and -not $byRun.ContainsKey($runName)) "Repeated or unrecognized raw run: $scenarioName/$runName"
        $path = Join-Path $ReportsDirectory "$scenarioName-$runName.json"
        $packet = Read-AuditJson $path
        Assert-Audit ($packet.sha256 -ceq $record['sha256']) "Raw bytes changed after judgment: $path"
        $receipt = $packet.value
        $identity = Test-PerformanceReceiptIdentity -Receipt $receipt
        Assert-Audit ($identity['verifiable']) "Missing full v2 identity: $path ($($identity['missing'] -join ', '))"
        Assert-Audit ($receipt['sourceCommit'] -ceq $summary['baselineCommit'] -and
            $receipt['sourceFingerprint'] -ceq $summary['baseline']['sourceFingerprint']) "Raw source provenance differs: $path"
        Assert-Audit ($receipt['platform'] -ceq 'x64' -and $receipt['configuration'] -ceq 'Release' -and $receipt['nativeArchitecture'] -ceq 'X64') "Raw profile differs: $path"
        foreach ($inputPath in $receipt['benchmarkInputs'].Keys) {
            Assert-Audit ($summary['harness'].Contains($inputPath) -and $receipt['benchmarkInputs'][$inputPath] -ceq $summary['harness'][$inputPath]) "Raw benchmark closure differs: $path/$inputPath"
        }
        $benchmarkLines = ($receipt['benchmarkInputs'].GetEnumerator() | ForEach-Object { "$($_.Key) $($_.Value)" }) -join "`n"
        Assert-Audit ((Get-ScopedDigest $benchmarkLines).ToUpperInvariant() -ceq $receipt['benchmarkSha256']) "Raw benchmark closure digest differs: $path"
        $byRun[$runName] = $receipt
        $allReceipts.Add($receipt)
        $rawHashes.Add([ordered]@{scenario=$scenarioName;run=$runName;path=$path;sha256=$packet.sha256})
    }

    # Both decisions consume the objects decoded from those exact hashed bytes.
    $receiptBlocks = @($summary['blockSchedule'] | ForEach-Object { [ordered]@{
        name=$_['Name'];order=$_['Order']
        Baseline=@($_['BaselineRuns'] | ForEach-Object { $byRun[[string]$_] })
        Candidate=@($_['CandidateRuns'] | ForEach-Object { $byRun[[string]$_] })
    } })
    $flatBaseline = @($receiptBlocks | ForEach-Object { $_['Baseline'] })
    $flatCandidate = @($receiptBlocks | ForEach-Object { $_['Candidate'] })
    $legacyReplay = & $judges['module'] { param($Baseline,$Candidate)
        Compare-PerformanceSet -Baseline $Baseline -Candidate $Candidate
    } $flatBaseline $flatCandidate
    $migratedReplay = & $judges['candidateModule'] { param($Blocks,$Protocol)
        Compare-PairedBlockSet -Blocks $Blocks -MinimumBlocks 12 -AssignmentProtocol $Protocol -CalibrationAA
    } $receiptBlocks $summary['allocation']
    Assert-Audit (Test-AuditEquivalent $comparison['legacyJudge'] $legacyReplay) "Recorded base-judge decision differs from raw replay: $scenarioName"
    Assert-Audit (Test-AuditEquivalent $comparison['migratedJudge'] $migratedReplay) "Recorded candidate-judge decision differs from raw replay: $scenarioName"
    Assert-Audit (Test-AuditEquivalent $scenario['set'] $migratedReplay) "Consumed set differs from raw replay: $scenarioName"
    $verdictsAgree = & $gate { param($Legacy,$Migrated)
        Test-EquivalentPerformanceVerdicts -Legacy $Legacy -Migrated $Migrated
    } $legacyReplay $migratedReplay
    Assert-Audit ($verdictsAgree -and $migratedReplay['calibrationOnly'] -eq $true) "Replayed verdict agreement or A/A designation failed: $scenarioName"

    $controls = @($scenario['comparisons'] | Where-Object { $_['name'].EndsWith('-control', [StringComparison]::Ordinal) })
    Assert-Audit ($controls.Count -eq 24) "Incomplete same-side control inventory: $scenarioName"
    foreach ($control in $controls) {
        $controlPacket = Read-AuditJson (Join-Path $ReportsDirectory $control['file'])
        $slots = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach ($change in $controlPacket.value['changes']) {
            $slot = '{0}/{1}' -f $change['scenario'],$change['metric']
            Assert-Audit ($expectedControlSlots.Contains($slot) -and $slots.Add($slot)) "Repeated or unexpected control metric: $($control['name'])/$slot"
        }
        Assert-Audit ($controlPacket.value['changes'].Count -eq 26 -and $slots.SetEquals($expectedControlSlots)) "Incomplete twenty-six-slot control: $($control['name'])"
    }
    # This production helper checks exact scheduled control names/pairs, full decision equality,
    # and absolute 5% timing / 2% memory / zero exact-resource bands. Its canonical drift map is consumed below.
    $controlEvidence = & $gate { param($Scenario,$Blocks,$Receipts,$Judge,$Directory)
        Test-ReplayedControlEvidence -Scenario $Scenario -BlockSchedule $Blocks -ReceiptsByRun $Receipts -JudgeModule $Judge -ReportsDirectory $Directory
    } $scenario $summary['blockSchedule'] $byRun $judges['candidateModule'] $ReportsDirectory
    Assert-Audit ($controlEvidence['valid']) "Scheduled raw control replay failed: $($controlEvidence['reason'])"
    $metricMap = $controlEvidence['drift']['Metrics']
    Assert-Audit ($metricMap.Count -eq 26 -and @($metricMap.Values | Where-Object { $_['Observed'] -ne 24 }).Count -eq 0) "Control metric observations are incomplete: $scenarioName"
    $strictScenario = & $gate { param($Scenario,$Directory,$Controls)
        Get-ScenarioConclusion -Entry $Scenario -ReportsDirectory $Directory -Unchanged $true -Strict $true -ReplayedControls $Controls
    } $scenario $ReportsDirectory $controlEvidence['drift']
    $scenarioRows += [ordered]@{
        scenario=$scenarioName;rawReceipts=$byRun.Count;bothFullJudgeDecisionsMatch=$true;consumedSetMatches=$true;verdictsAgree=$verdictsAgree
        calibrationOnly=$migratedReplay['calibrationOnly'];setStatus=$migratedReplay['status'];metricSlots=$migratedReplay['metrics'].Count
        regressed=@($migratedReplay['metrics'] | Where-Object { $_['verdict'] -ceq 'regressed' }).Count
        controls=$controlEvidence['drift']['Total'];unstable=$controlEvidence['drift']['Unstable'];controlMetricComparisons=$controls.Count*26
        driftedMetricCells=@($metricMap.Values | Where-Object { $_['Drifted'] }).Count;strictConclusion=$strictScenario['Conclusion'];strictNotes=$strictScenario['Notes']
        drift=@($metricMap.Keys | Sort-Object | ForEach-Object { [ordered]@{slot=$_;observed=$metricMap[$_]['Observed'];drifted=$metricMap[$_]['Drifted'];maxAbsoluteChangePercent=$metricMap[$_]['MaxDriftPercent']} })
    }
}
Assert-Audit ($allReceipts.Count -eq 144) 'The full raw receipt inventory is not one hundred forty-four.'

$identityRows = @(foreach ($key in @('sourceCommit','sourceFingerprint','executableSha256','buildIdentityVersion','resolvedBuildIdentity',
    'buildInputIdentity','buildArtifactIdentity','buildAttestationIdentity','benchmarkSha256','harnessIdentity','toolchainIdentity',
    'dependencyIdentity','environmentIdentity','identityScope','identityStatus','sourceDirty')) {
    $values = @($allReceipts | ForEach-Object { $_[$key] } | Select-Object -Unique)
    Assert-Audit ($values.Count -eq 1) "A/A source/build/environment identity differs across raw receipts: $key"
    [ordered]@{field=$key;value=$values[0]}
})

$baselineRoot = if ($ArtifactRoot) { [IO.Path]::GetFullPath($ArtifactRoot) } else { Join-Path $RunRoot 'baseline' }
$executablePath = Join-Path $baselineRoot '.build/x64/Release/DxUi.EmbeddedTests.exe'
$firstReceipt = $allReceipts[0]
$actualExeHash = (Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash
Assert-Audit ($actualExeHash -ceq $firstReceipt['executableSha256']) 'Actual retained executable differs from every raw report.'
$actualArtifactIdentity = Get-ScopedArtifactIdentity -Root $baselineRoot -Platform x64 -Configuration Release
Assert-Audit ($actualArtifactIdentity -ceq $firstReceipt['buildArtifactIdentity']) 'Actual retained executable/library/symbol inventory differs from every raw report.'
$actualAttestation = Get-PerformanceBuildAttestationIdentity -State $firstReceipt -ArtifactIdentity $actualArtifactIdentity
Assert-Audit ($actualAttestation -ceq $firstReceipt['buildAttestationIdentity']) 'Actual-artifact-bound v2 build attestation differs from every raw report.'
$buildPacket = Read-AuditJson $BuildReceiptPath
$incrementalPacket = Read-AuditJson $IncrementalReceiptPath
Assert-Audit (Test-ScopedReceipt -Path $BuildReceiptPath -Identity $actualAttestation) 'Captured build envelope/digest does not bind the actual-artifact v2 attestation.'
Assert-Audit (Test-ScopedReceipt -Path $IncrementalReceiptPath -Identity $incrementalPacket.value['identity']) 'Captured incremental receipt envelope/digest is invalid.'
$buildLog = [IO.File]::ReadAllText($BuildLogPath)
Assert-Audit ($buildLog.Contains('Build succeeded.') -and $buildLog.Contains('0 Error(s)')) 'Captured actual build log lacks its successful completion.'

# Keep the actual A/A purpose. Required-policy qualification intentionally refuses it.
$strict = Get-BenchmarkConclusion -Summary $summary -ReportsDirectory $ReportsDirectory -StrictControls
$required = Get-BenchmarkConclusion -Summary $summary -ReportsDirectory $ReportsDirectory -StrictControls -RequireQualifiedPolicy
Assert-Audit ($strict['Conclusion'] -ceq 'inconclusive' -and @($scenarioRows | Where-Object { $_['strictConclusion'] -cne 'inconclusive' }).Count -eq 0) 'Unstable controls unexpectedly cleared strict qualification.'
Assert-Audit ($required['Conclusion'] -ceq 'inconclusive') 'A/A unexpectedly cleared required-policy qualification.'
Assert-Audit ($summary['studyPurpose'] -ceq 'aa-calibration') 'The audit changed the retained purpose.'
foreach ($record in $rawHashes) {
    Assert-Audit ((Get-FileHash -LiteralPath $record['path'] -Algorithm SHA256).Hash -ceq $record['sha256']) "Raw bytes changed during replay: $($record['path'])"
}
Assert-Audit ((Get-FileHash -LiteralPath $summaryPath -Algorithm SHA256).Hash -ceq $summaryPacket.sha256) 'Summary bytes changed during replay.'

$result = [ordered]@{
    classification='read-only-v2-aa-audit';summaryPath=$summaryPath;summarySha256=$summaryPacket.sha256;studyPurpose=$summary['studyPurpose']
    baselineCommit=$summary['baselineCommit'];harnessCommit=$summary['harnessCommit'];declaredHarnessDirty=$summary['harnessDirty'];harnessPathsVerified=$harnessRows.Count
    samePhysicalTree=$true;rawReceipts=$allReceipts.Count;controlComparisons=($scenarioRows.controls | Measure-Object -Sum).Sum
    controlMetricComparisons=($scenarioRows.controlMetricComparisons | Measure-Object -Sum).Sum
    unstableControls=($scenarioRows.unstable | Measure-Object -Sum).Sum;driftedMetricCells=($scenarioRows.driftedMetricCells | Measure-Object -Sum).Sum
    judgeProvenance=$summary['judgeProvenance'];schedule=[ordered]@{seed=$summary['seed'];blocks=$summary['blocks'];steps=$expectedRuns.Count;literalOrderMatches=$true;blockRecordsMatch=$true;stepRecordsMatch=$true;abba=@($schedule['Blocks'] | Where-Object Order -eq 'ABBA').Count;baab=@($schedule['Blocks'] | Where-Object Order -eq 'BAAB').Count}
    identity=$identityRows;scenarios=$scenarioRows
    artifacts=[ordered]@{executable=$executablePath;executableBytes=(Get-Item -LiteralPath $executablePath).Length;executableSha256=$actualExeHash;artifactIdentity=$actualArtifactIdentity;v2BuildAttestationIdentity=$actualAttestation;buildEnvelopeVersion=$buildPacket.value['version'];buildEnvelopeIdentity=$buildPacket.value['identity'];buildEnvelopeValid=$true;buildReceiptSha256=$buildPacket.sha256;incrementalEnvelopeVersion=$incrementalPacket.value['version'];incrementalEnvelopeIdentity=$incrementalPacket.value['identity'];incrementalEnvelopeValid=$true;incrementalReceiptSha256=$incrementalPacket.sha256;buildLogSha256=(Get-FileHash -LiteralPath $BuildLogPath -Algorithm SHA256).Hash}
    strictControls=[ordered]@{conclusion=$strict['Conclusion'];policyStatus=$strict['PolicyStatus'];notes=$strict['Notes']}
    requireQualifiedPolicy=[ordered]@{conclusion=$required['Conclusion'];policyStatus=$required['PolicyStatus'];notes=$required['Notes']}
    limits=@('Calibration evidence cannot qualify an ordinary candidate or required CI; aggregate no-flags does not establish held controls.',
        'Captured incremental envelope/digest checked; the full intermediate-file inventory is not recomputed.',
        'Resolved compiler/build-input identities are verified as retained v2 attestation fields; no new MSBuild evaluation is performed after harness-overlay restoration.',
        'No dedicated-host, hardware, consumer, ARM64, or new performance-measurement qualification is asserted.')
}
ConvertTo-Json -InputObject $result -Depth 10
