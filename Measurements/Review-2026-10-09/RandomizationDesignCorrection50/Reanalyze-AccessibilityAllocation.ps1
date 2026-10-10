# Reanalyse only the historical six-ABBA/six-BAAB assignment space. No build, benchmark or policy approval occurs here.
[CmdletBinding()]
param(
    [string] $StudyResult = (Join-Path $PSScriptRoot '../AccessibilitySchedulingPairedBaseline/Runs/20261009T201426Z-seed-20261009/study-result.json'),
    [string] $OutputFile = (Join-Path $PSScriptRoot 'accessibility-correction.json')
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$inputPath = [IO.Path]::GetFullPath($StudyResult)
$study = Get-Content -LiteralPath $inputPath -Raw | ConvertFrom-Json -AsHashtable
$schedule = @($study.schedule.Blocks)
if ($study.schedule.Allocation -cne 'balanced-ABBA-BAAB' -or $schedule.Count -ne 12 -or
    @($schedule | Where-Object Order -ceq 'ABBA').Count -ne 6 -or
    @($schedule | Where-Object Order -ceq 'BAAB').Count -ne 6 -or @($study.processRuns).Count -ne 48 -or
    @($study.rawLogs).Count -ne 48 -or @($study.timingAnalysis.timings).Count -ne 6 -or
    @($study.timingAnalysis.timings.name | Select-Object -Unique).Count -ne 6) {
    throw 'This correction is specific to the retained twelve-block globally balanced study.'
}
foreach ($record in $study.rawLogs) {
    $path = Join-Path ([IO.Path]::GetDirectoryName($inputPath)) $record.path
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $record.sha256) { throw "Original raw log changed: $($record.path)" }
}
$byRun = @{}
foreach ($run in $study.processRuns) { $byRun[$run.name] = $run }
$rows = @(foreach ($outcome in $study.timingAnalysis.timings) {
    $effects = @(foreach ($block in $schedule) {
        $a = @($block.BaselineRuns | ForEach-Object { [double]$byRun[$_].processMedians[$outcome.scenario][$outcome.field] })
        $b = @($block.CandidateRuns | ForEach-Object { [double]$byRun[$_].processMedians[$outcome.scenario][$outcome.field] })
        [Math]::Log((($b[0] + $b[1]) / 2.0) / (($a[0] + $a[1]) / 2.0))
    })
    for ($i = 0; $i -lt 12; $i++) {
        if ([Math]::Abs($effects[$i] - [double]$outcome.blockEffects[$i].candidateOverBaselineLogEffect) -gt 1e-12) {
            throw "Retained effect differs from its process medians: $($outcome.name) block $i"
        }
    }
    $observed = [Math]::Abs([double](($effects | Measure-Object -Sum).Sum))
    $assignments = 0; $extreme = 0
    for ($mask = 0; $mask -lt 4096; $mask++) {
        $abba = 0
        for ($i = 0; $i -lt 12; $i++) { if ($mask -band (1 -shl $i)) { $abba++ } }
        if ($abba -ne 6) { continue }
        $assignments++; $sum = 0.0
        for ($i = 0; $i -lt 12; $i++) {
            $alternativeAbba = [bool]($mask -band (1 -shl $i))
            $observedAbba = $schedule[$i].Order -ceq 'ABBA'
            $sum += $(if ($alternativeAbba -eq $observedAbba) { $effects[$i] } else { -$effects[$i] })
        }
        if ([Math]::Abs($sum) -ge ($observed - 1e-12)) { $extreme++ }
    }
    if ($assignments -ne 924) { throw 'The historical allocation space was not enumerated completely.' }
    [ordered]@{ name=$outcome.name; blockEffects=$effects; observedAbsoluteSum=$observed;
        assignmentCount=$assignments; extremeAssignments=$extreme; exactTwoSidedRandomizationP=([double]$extreme / $assignments);
        originalIndependentSignFlipP=$outcome.exactTwoSidedSignFlipPValue; originalHolmAdjustedP=$outcome.holmAdjustedPValue }
})
# Holm across the original six declared outcomes, independently of the production judge.
$sorted = @(0..($rows.Count - 1) | Sort-Object { $rows[$_].exactTwoSidedRandomizationP })
$previous = 0.0
for ($rank = 0; $rank -lt $sorted.Count; $rank++) {
    $index = $sorted[$rank]
    $previous = [Math]::Max($previous, [Math]::Min(1.0, ($sorted.Count - $rank) * $rows[$index].exactTwoSidedRandomizationP))
    $rows[$index].holmAdjustedPValue = $previous
}
$normalizedScript = [IO.File]::ReadAllText($PSCommandPath) -replace "`r`n", "`n"
$scriptHasher = [Security.Cryptography.SHA256]::Create()
try { $scriptHash = [Convert]::ToHexString($scriptHasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($normalizedScript))) }
finally { $scriptHasher.Dispose() }
$result = [ordered]@{ artifactType='historical-assignment-statistical-correction'; workloadOwner=$study.workloadOwner;
    fixture=$study.fixture; benchmarkInputs=$study.benchmarkInputs;
    originalStudySha256=(Get-FileHash -LiteralPath $inputPath -Algorithm SHA256).Hash;
    correctionScriptNormalizedSha256=$scriptHash; scriptNormalization='UTF-8 text with CRLF normalized to LF';
    originalRawHashesChecked=@($study.rawLogs).Count; inputProjection='Retained process medians checked against recorded block effects; original raw log hashes verified';
    allocation='six ABBA and six BAAB orders, shuffled without replacement';
    method='Enumerate all C(12,6) equally probable order assignments; flip each block effect exactly when its alternative order differs from its observed order';
    assignmentCount=924; minimumTwoSidedP=(2.0/924.0); familySize=6; significanceLevel=0.05; timings=$rows;
    interpretation='Corrects inference only. Original observations, cost deltas and accepted first-query tradeoff are unchanged; this is not policy qualification.' }
$result | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $OutputFile -Encoding utf8
Write-Host "Historical assignment correction written: $OutputFile"
