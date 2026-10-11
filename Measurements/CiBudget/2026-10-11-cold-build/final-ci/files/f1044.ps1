[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$external='C:/Users/eric/.codex/tmp/dpf-s0-s2-merge'
$collection=Join-Path $external 'ci-evidence/pr-73-run-38172114803-attempt-1-858f3ba12193400b84801b3a1fa75ac2'
$output=Join-Path $collection 'root-implementation-integration-decision-399.receipt.txt'
if(Test-Path -LiteralPath $output){throw 'The root decision already exists.'}
$inputs=[ordered]@{
    originalFullReview=Join-Path $collection 'review-20261011T214113Z-9736815beaff470294e019f3f1a05bd8.receipt.txt'
    rootFullFunctionalVisual=Join-Path $collection 'root-full-functional-visual-binding-399.receipt.txt'
    rootNativeBudgets=Join-Path $collection 'native-raw-budget-review-20261011T214157Z-953faca22e8f41f3959491dde41891ba.receipt.txt'
    rootRawPairBindings=Join-Path $collection 'root-paired-raw-bindings-82bb63ef2d394412ab274670b26b5b76.receipt.txt'
    rootFullNumericPairReplay=Join-Path $external 'paired-root-replay-run399-01/paired-replay.receipt.txt'
    independentFullNumericPairReplay=Join-Path $external 'paired-independent-run399-01/paired-replay.receipt.txt'
    rootArmBeforeSuiteInput=Join-Path $external 'early-ci-native-cold-budget/root-arm-before-suite-input-399.receipt.txt'
    rootSixProfileCrosscheck=Join-Path $external 'early-ci-native-cold-budget/root-native-agent-crosscheck-399.receipt.txt'
    rootFunctionsAndVisualReview=Join-Path $external 'early-ci-native-cold-budget/root-functional-visual-399-318fd6cdb9ac4db68bcf4a942c7321f0.receipt.txt'
    rootFreshAsanVisualReview=Join-Path $external 'early-ci-native-cold-budget/root-viewed-asan-gallery-399.receipt.txt'
}
$hashes=[ordered]@{}
$objects=@{}
foreach($key in $inputs.Keys){$hashes[$key]=[ordered]@{path=$inputs[$key];sha256=(Get-FileHash -LiteralPath $inputs[$key]).Hash};$objects[$key]=Get-Content -LiteralPath $inputs[$key] -Raw | ConvertFrom-Json -AsHashtable}
if($objects.originalFullReview.status -cne 'BLOCKED' -or $objects.originalFullReview.actualRunConclusion -cne 'failure'){throw 'Preserve the original full and CI failures.'}
foreach($key in @('rootFullNumericPairReplay','independentFullNumericPairReplay')){
    $r=$objects[$key]
    if($r.qualificationPassed -ne $false -or $r.ciGateSucceeded -ne $false -or $r.reports -ne 144 -or $r.rawRounds -ne 1440 -or $r.metricDecisionsPerJudge -ne 78 -or $r.sameBinaryControls -ne 72 -or $r.deterministicBudgets -ne 24 -or $r.confirmedOutcomes -ne 0 -or $r.changedLibraryHeldRegressions -ne 0){throw 'Incomplete or wrongly qualified numeric replay.'}
    foreach($s in $r.scenarios){
        if($s.originalVerdictAgreement -ne $false -or $s.actualVerdictAgreement -ne $false -or $s.differences.Count -ne 1){throw 'Unexpected migration disagreement inventory.'}
        $d=$s.differences[0]
        if($d.phase -cne 'clean' -or $d.metric -cne 'prepareP95Ms' -or $d.legacy.verdict -cne 'improved' -or $d.migrated.verdict -cne 'within-noise' -or @($s.legacyDecision.metrics | Where-Object verdict -CEQ 'regressed').Count -ne 0 -or @($s.migratedDecision.metrics | Where-Object verdict -CEQ 'regressed').Count -ne 0){throw 'A changed verdict includes a regression or an unreviewed metric.'}
    }
}
$repo='C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$head=(& git -C $repo rev-parse HEAD).Trim()
$tree=(& git -C $repo rev-parse 'HEAD^{tree}').Trim()
$dirty=@(& git -C $repo status --porcelain --untracked-files=normal)
if($LASTEXITCODE -ne 0 -or $head -cne '4bd3480170140090ab2d8ff883d05507281b52ee' -or $tree -cne 'e695cf74db0e2fc1a5c8c8d183cfb0bd136d639d' -or $dirty.Count -ne 0){throw 'The reviewed source is no longer frozen.'}
$r=$objects.rootFullNumericPairReplay
$receipt=[ordered]@{
    status='ROOT_EXPLICIT_REVIEW_COMPLETE_IMPLEMENTATION_ONLY_INTEGRATION_ELIGIBLE';
    qualificationPassed=$false; ciGateSucceeded=$false; aggregateRunConclusion='failure'; originalFullReviewStatus='BLOCKED';
    authority='The developer requested remote-main integration of completed S0-S2 with S3-S7 and unfinished qualification retained in WIP. Specs/Testing/Testing_Validation.md:743-750 permits normal implementation-only PR integration after reviewed current-source evidence, preserving unqualified policy findings and individually verified ARM foreground refusals before input. No other failed correctness, sanitizer, provenance or deterministic-resource check is accepted.';
    pullRequest=73; runId=38172114803; attempt=1; base='ec680e866a218b57000a1780018c00101a437f91'; head=$head; testedMerge='80d875a1de91d2b2c8602faa5614cf75eab6025b'; testedTree=$tree;
    reviewedInputs=$hashes; originalSealSha256=(Get-FileHash -LiteralPath (Join-Path $collection 'SHA256SUMS')).Hash;
    ordinaryChecks=[ordered]@{windowsTooling='success';linuxTooling='success';format='success';scope='success'};
    native=[ordered]@{profiles=6;receipts=128;passed=122;unlaunchedForegroundRefusals=6;capabilitySkips=0;x64FullJobs='all three success';arm64FullJobs='all three failure solely Menu/NativeTextInput refused before child input';s0S2Functions=38;functionExecutions=228;embeddedChecksPerProfile=4990;warmCompositesPerProfile=1000;warmCompositionAllocations=0;syntheticConsumers=8;syntheticConsumersPassed=8;asanExpectedUafProbes=2;actualHarnessImagesReviewed=18};
    performance=[ordered]@{reports=144;rounds=1440;fullMetricDecisionsPerJudge=78;controls=72;unstableControls=69;exactBudgets=24;rolesAndHarnessBound=$true;fullNumericDecisionsAndControlsReplayed=$true;regressionVerdicts=0;heldRegressionVerdicts=0;confirmedOutcomes=0;unqualifiedBasePolicy=$true;originalAgreementFlags=@($false,$false,$false);originalPublication='failed/inconclusive policy-review-required';currentUnchangedExactAndAbsoluteBudgets='clean/composition/hidden zero; Release dirty at most2160 of2560 per round; surface/replacement3686400 on both sides';migrationDifference='All three scenes: legacy clean/prepareP95Ms improved(-16.6667%), migrated within-noise. All remaining75 verdict labels agree. Complete numeric decisions and72 controls reproduce on identical144 raw buffers; no forged, missing or swapped role/source or deterministic resource rise found.';explicitReview='Treat these truthful method disagreements as unqualified performance-policy findings under the dated implementation-only decision. Preserve original BLOCKED review, agreement=false, publication failure and aggregate failure. Unstable controls prevent a speedup/non-regression/qualification claim. No production judge, policy, threshold, input or original evidence is changed.'};
    armRefusalReview='Each of the three native ARM profiles completes19ordinary suites and consumers inside the45-minute Full cap. Foreign WWAHost occludes the warning endpoint; guard refuses before both warning click and all suite input. Saved/exit cursor and foreground/focus/pointer restoration are unchanged. Shared diagnostic cannot identify the first/second guard or prove absence of transient external motion. Exactly six suite results remain unlaunched failures, with no capability skip and no timeout/cancellation exception.';
    currentHostedLimits=$r.limits;
    remaining='Performance and trusted-policy acceptance, producer role/absolute-budget hardening, native ARM64 foreground acceptance, physical device/input/IME/assistive technology and separately owned consumer pilots/pins remain indexed WIP. Historical FPS/timing flags stay retained and open. Main/release qualification is separate.';
    proposedAction='Ordinary GitHub PR merge with exact head matching after branch rules/base are rechecked. No admin override, bypass, force push, fake check, reset, consumer pin change or silent rebaseline.';
    rootReview='Root read the full independent replay driver, executed it separately, examined all three complete decision/control/budget summaries, bound all128 current native records/logs/performance bytes and18 harness image bytes to its previously reviewed function and visual proofs, and independently verified agent accounting. The full original 1021-file seal and frozen source/harness remain unchanged.';
    driver=$PSCommandPath;driverSha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash;completedUtc=[DateTimeOffset]::UtcNow
}
foreach($key in $inputs.Keys){if((Get-FileHash -LiteralPath $inputs[$key]).Hash -cne $hashes[$key].sha256){throw 'Input changed during decision assembly.'}}
$bytes=[Text.UTF8Encoding]::new($false).GetBytes(($receipt|ConvertTo-Json -Depth 30)+[char]10)
$stream=[IO.File]::Open($output,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
try{$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
[ordered]@{path=$output;sha256=(Get-FileHash -LiteralPath $output).Hash;status=$receipt.status;qualificationPassed=$false;ciGateSucceeded=$false}|ConvertTo-Json
