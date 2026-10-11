<# Pure helper slices against synthetic metadata and an already downloaded historical artifact.
No repository tests, builds, native execution, Git mutations or network requests. #>
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$utf8 = [Text.UTF8Encoding]::new($false)
$helper = Join-Path $PSScriptRoot 'Review-PrCiEvidence.ps1'
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($helper,[ref]$tokens,[ref]$errors)
if (@($errors).Count) { throw 'Helper parser errors.' }
foreach ($name in @('Require','Digest','Receipt-Instant','Profile-Job','Find-File','Receipt','Assert-CompletedInJob','Assert-Sha','Read-ExactExitCode','Bind-UploadedBuildAttestation','Assert-PairedRawBudgetAndFixture','Bind-PairedRoleReceipt')) {
    $definition = @($ast.FindAll({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $name },$true))
    if ($definition.Count -ne 1) { throw "Cannot isolate function $name." }
    . ([scriptblock]::Create($definition[0].Extent.Text))
}
$repo = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
foreach ($source in @(@{path='Tools/ScopedTesting.psm1';name='Get-ScopedDigest'},@{path='Tools/PerformancePolicy.psm1';name='Get-PerformanceBuildAttestationIdentity'})) {
    $sourceAst = [Management.Automation.Language.Parser]::ParseFile((Join-Path $repo $source.path),[ref]$tokens,[ref]$errors)
    $definition = @($sourceAst.FindAll({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $source.name },$true))
    if ($definition.Count -ne 1) { throw 'Missing pure digest function.' }
    . ([scriptblock]::Create($definition[0].Extent.Text))
}
$passed = [Collections.Generic.List[string]]::new()
function Expect-Rejection([string]$Label,[scriptblock]$Action,[string]$Reason) {
    try { & $Action; throw 'Expected rejection did not occur.' }
    catch { if ($_.Exception.Message -notmatch $Reason) { throw }; $passed.Add($Label) }
}
$collection = @{workflowDigest='7d4a1afd4c0a74ae8b93185f5ca4f78eda76d0eda4eff3e368bffd8c207730c8'}
$nativeJobBudgets = [Collections.Generic.List[object]]::new()
foreach ($sample in @(@{exitCode=0},@{exitCode=[long]24},@{exitCode=-1073741819})) {
    Require ((Read-ExactExitCode $sample) -eq $sample['exitCode']) 'An exact signed native exit result was rejected.'
}
$passed.Add('exact zero, refusal and signed sanitizer exit results remain readable')
foreach ($sample in @(@{},@{exitCode=$null},@{exitCode=$false},@{exitCode=$true},@{exitCode='0'},@{exitCode=[double]0},@{exitCode=[double]1.5})) {
    Expect-Rejection 'missing or malformed native/consumer/ASan exit rejected' { [void](Read-ExactExitCode $sample) } 'Missing or nonintegral exitCode'
}
foreach ($sample in @(@{exitCode=[long]2147483648},@{exitCode=[long]-2147483649})) {
    Expect-Rejection 'out-of-range exit rejected' { [void](Read-ExactExitCode $sample) } 'signed native process range'
}
foreach ($platform in @('x64','ARM64')) {
    foreach ($configuration in @('Debug','Release','ASan Debug')) {
        $runner = if ($platform -ceq 'ARM64') { 'windows-11-vs2026-arm' } else { 'windows-2025-vs2026' }
        $budget = if ($platform -ceq 'ARM64' -and $configuration -ceq 'ASan Debug') {60} else {40}
        foreach ($displayBudget in @('',", $budget")) {
            $jobs = @(@{id=1;name="native ($platform, $runner, $configuration$displayBudget)"})
            Require ((Profile-Job $platform $configuration)['id'] -eq 1) 'Valid matrix display did not select the profile.'
        }
    }
}
$passed.Add('all six profiles, three/four-value matrix displays')
$jobs = @(@{id=1;name='native (ARM64, windows-11-vs2026-arm, ASan Debug, 40)'})
Expect-Rejection 'ARM64 ASan 40-minute display rejected' { [void](Profile-Job ARM64 'ASan Debug') } 'another selected job budget'
$jobs = @(@{id=1;name='native (x64, windows-2025-vs2026, ASan Debug, 60)'})
Expect-Rejection 'x64 ASan 60-minute display rejected' { [void](Profile-Job x64 'ASan Debug') } 'another selected job budget'
$jobs = @(@{id=1;name='native (ARM64, windows-2025-vs2026, Release, 40)'})
Expect-Rejection 'wrong runner rejected' { [void](Profile-Job ARM64 Release) } 'Missing or duplicate'
$historical = 'C:/Users/eric/.codex/tmp/dpf-s0-s2-merge/native-attestation-inventory-38055762177-44f6c29a8460420c975990fae3881409'
foreach ($suite in @('Menu','NativeTextInput')) {
    $receipt = Receipt $historical $suite ARM64 'ASan Debug'
    Require ($receipt['value']['exitCode'] -eq 24) 'Historical refusal receipt was not selected.'
    $log = Find-File $historical "test-$suite-ARM64-ASan Debug.hosted.log"
    Require ([IO.File]::ReadAllText($log).Trim() -ceq "[LEASE] $suite did not run under the foreground lease") 'Historical refusal log binding differs.'
}
$passed.Add('historical hosted Menu/NativeTextInput receipt and exact log names')
$fixture = Join-Path $PSScriptRoot ('slice-audit-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$performance = @{platform='x64';configuration='Release';buildIdentityVersion=2;resolvedBuildIdentity=('1'*64);buildInputIdentity=('2'*64);toolchainIdentity=('3'*64);buildArtifactIdentity=('4'*64);completedUtc='2026-10-11T06:01:00.0000000Z'}
$performance['buildAttestationIdentity'] = Get-PerformanceBuildAttestationIdentity -State $performance -ArtifactIdentity $performance['buildArtifactIdentity']
$job = @{started_at='2026-10-11T05:59:00Z';completed_at='2026-10-11T06:02:00Z'}
$absent = Bind-UploadedBuildAttestation $fixture $performance $job
Require ($absent['verified'] -eq $false -and $absent['status'] -ceq 'NOT_UPLOADED_BY_CURRENT_WORKFLOW') 'Absent sidecar was incorrectly verified.'
$passed.Add('absent sidecar remains an explicit limitation')
$sidecar = @{version=1;outcome='PASSED';identity=$performance['buildAttestationIdentity'];completedUtc='2026-10-11T06:00:00.0000000Z'}
$sidecar['digest'] = Digest ($utf8.GetBytes($sidecar['identity'] + "`n" + $sidecar['completedUtc'] + "`nPASSED"))
$sidecarPath = Join-Path $fixture 'reports/performance-build/x64-Release/build.json'
[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($sidecarPath))
[IO.File]::WriteAllText($sidecarPath,($sidecar | ConvertTo-Json),$utf8)
Require ((Bind-UploadedBuildAttestation $fixture $performance $job)['verified'] -eq $true) 'Valid scoped v2 aggregate binding was rejected.'
$passed.Add('uploaded scoped sidecar bytes/digest bind the v2 field combination')
$performance['buildInputIdentity'] = '9'*64
Expect-Rejection 'rebound input identity rejected' { [void](Bind-UploadedBuildAttestation $fixture $performance $job) } 'v2 domain'
$performance['buildInputIdentity'] = '2'*64
$sidecar['digest'] = '0'*64
[IO.File]::WriteAllText($sidecarPath,($sidecar | ConvertTo-Json),$utf8)
Expect-Rejection 'tampered scoped sidecar digest rejected' { [void](Bind-UploadedBuildAttestation $fixture $performance $job) } 'digest does not reproduce'

function Copy-ProbeValue([object] $Value) { return ConvertFrom-Json -InputObject (ConvertTo-Json -InputObject $Value -Depth 40 -Compress) -AsHashtable }
function New-PairedProbeReceipt([string] $Role, [string] $FixtureName='dxui-complex-ui-v2') {
    $baseline = $Role -ceq 'baseline'
    $value = [ordered]@{ fixture=$FixtureName; renderer='WARP'; width=1280; height=720; dpi=96; controls=83; modelRows=1000; framesPerRound=40; roundCount=5;
        platform='x64'; configuration='Release'; nativeArchitecture='X64'; dirtyAllocationCeilingPerFrame=64; hiddenPreparations=0; hiddenComposites=0;
        sourceCommit=$(if($baseline){'a'*40}else{'b'*40}); sourceFingerprint=$(if($baseline){'A'*64}else{'B'*64}); sourceDirty=$baseline;
        executableSha256=$(if($baseline){'C'*64}else{'D'*64}); benchmarkSha256=('E'*64); harnessIdentity=('8'*64); toolchainIdentity=('7'*64); dependencyIdentity=('9'*64); environmentIdentity=('F'*64);
        resolvedBuildIdentity=$(if($baseline){'3'*64}else{'4'*64}); buildInputIdentity=$(if($baseline){'1'*64}else{'2'*64}); buildArtifactIdentity=$(if($baseline){'5'*64}else{'6'*64});
        buildIdentityVersion=2; identityScope='full'; identityStatus='verifiable'; machine='synthetic';cpu='synthetic';os='synthetic';compiler=1944;warpVersion='synthetic';warpSha256=('0'*64);powerPolicy='synthetic';
        scenarios=@(foreach($phase in @('clean','dirty')){[ordered]@{name=$phase;rounds=@(foreach($round in 1..5){[ordered]@{
            fps=1000;frameP50Ms=1;frameP95Ms=1;prepareP95Ms=0.2;composeCpuP95Ms=0.1;privateBytes=100000;privatePeakBytes=100000;workingSetBytes=100000;workingSetPeakBytes=100000;
            surfaceBytes=3686400;replacementPeakBytes=3686400;cppAllocations=$(if($phase -ceq 'clean'){0}else{2160});composeAllocations=0
        }})}})
    }
    $value['buildAttestationIdentity'] = Get-PerformanceBuildAttestationIdentity $value $value['buildArtifactIdentity']
    return $value
}
$baselineRaw = New-PairedProbeReceipt baseline; $candidateRaw = New-PairedProbeReceipt candidate
$roleSummary = @{ baseline=@{commit=$baselineRaw.sourceCommit;sourceFingerprint=$baselineRaw.sourceFingerprint;harnessOverlay=@('replaced build.ps1')};
    candidate=@{commit=$candidateRaw.sourceCommit;sourceFingerprint=$candidateRaw.sourceFingerprint;harnessOverlay=@()};
    steps=@(@{Name='A01';Side='baseline'},@{Name='B01';Side='candidate'}) }
$roleScenario = @{scenario='Default';runs=@{A01=(Copy-ProbeValue $baselineRaw);B01=(Copy-ProbeValue $candidateRaw)}}
$roleStates = @{}
Bind-PairedRoleReceipt $roleSummary $roleScenario A01 $baselineRaw $roleStates @('build.ps1')
Bind-PairedRoleReceipt $roleSummary $roleScenario B01 $candidateRaw $roleStates @('build.ps1')
Require ($roleStates.Count -eq 2 -and $roleStates.baseline.sourceDirty -eq $true -and $roleStates.candidate.sourceDirty -eq $false) 'Supported dirty baseline overlay provenance was not retained literally.'
$passed.Add('scheduled roles bind distinct exact raw inputs while retaining the legitimate dirty baseline overlay')
$tamperedSummary = Copy-ProbeValue $roleSummary; $tamperedSummary.candidate.sourceFingerprint = $tamperedSummary.baseline.sourceFingerprint
Expect-Rejection 'equalized summary fingerprint cannot hide changed raw library inputs' { Bind-PairedRoleReceipt $tamperedSummary $roleScenario B01 $candidateRaw @{} @('build.ps1') } 'fingerprint does not match'
Expect-Rejection 'scheduled A/B raw receipt swap rejected' { Bind-PairedRoleReceipt $roleSummary $roleScenario A01 $candidateRaw @{} @('build.ps1') } 'commit does not match'
$tamperedSummary = Copy-ProbeValue $roleSummary; $tamperedSummary.baseline.sourceFingerprint=$candidateRaw.sourceFingerprint; $tamperedSummary.candidate.sourceFingerprint=$baselineRaw.sourceFingerprint
Expect-Rejection 'swapped summary fingerprints rejected without relying on judge outcomes' { Bind-PairedRoleReceipt $tamperedSummary $roleScenario A01 $baselineRaw @{} @('build.ps1') } 'fingerprint does not match'
$tamperedSummary = Copy-ProbeValue $roleSummary; $tamperedSummary.candidate.buildArtifactIdentity='1'*64
Expect-Rejection 'available exact summary build field must match raw receipt' { Bind-PairedRoleReceipt $tamperedSummary $roleScenario B01 $candidateRaw @{} @('build.ps1') } 'side field differs'
$tamperedScenario = Copy-ProbeValue $roleScenario; $tamperedScenario.runs.B01.executableSha256='1'*64
Expect-Rejection 'retained per-run executable identity must match raw bytes' { Bind-PairedRoleReceipt $roleSummary $tamperedScenario B01 $candidateRaw @{} @('build.ps1') } 'run identity differs'
$tamperedSummary = Copy-ProbeValue $roleSummary; $tamperedSummary.baseline.harnessOverlay=@()
Expect-Rejection 'dirty raw source without an explicit supported overlay rejected' { Bind-PairedRoleReceipt $tamperedSummary $roleScenario A01 $baselineRaw @{} @('build.ps1') } 'no supported harness-overlay explanation'
$tamperedSummary = Copy-ProbeValue $roleSummary; $tamperedSummary.baseline.harnessOverlay=@('replaced src/Controls/DxUi.Forms.cpp')
Expect-Rejection 'library-source path cannot be relabeled as a permitted harness overlay' { Bind-PairedRoleReceipt $tamperedSummary $roleScenario A01 $baselineRaw @{} @('build.ps1') } 'overlay path is unsupported'
$tamperedRaw = Copy-ProbeValue $baselineRaw; $tamperedRaw.sourceDirty='false'
Expect-Rejection 'string dirty provenance rejected' { Bind-PairedRoleReceipt $roleSummary $roleScenario A01 $tamperedRaw @{} @('build.ps1') } 'dirty provenance is missing or malformed'
$tamperedRaw = Copy-ProbeValue $candidateRaw; $tamperedRaw.buildInputIdentity='1'*64; $tamperedRaw.buildAttestationIdentity=Get-PerformanceBuildAttestationIdentity $tamperedRaw $tamperedRaw.buildArtifactIdentity
$secondRunSummary = Copy-ProbeValue $roleSummary; $secondRunSummary.steps += @{Name='B02';Side='candidate'}
$secondRunScenario = Copy-ProbeValue $roleScenario; $secondRunScenario.runs.B02=Copy-ProbeValue $tamperedRaw
Expect-Rejection 'rebound v2 input identity within one scheduled role rejected' { Bind-PairedRoleReceipt $secondRunSummary $secondRunScenario B02 $tamperedRaw $roleStates @('build.ps1') } 'identity changed within'

foreach ($binding in @(@{name='Default';fixture='dxui-complex-ui-v2'},@{name='MultilineGrid';fixture='dxui-complex-ui-multiline-grid-v1'},@{name='MultilineGridDistinct';fixture='dxui-complex-ui-multiline-grid-distinct-v1'})) {
    $raw = New-PairedProbeReceipt baseline $binding.fixture
    Assert-PairedRawBudgetAndFixture $raw $binding.name
}
$boundary = Copy-ProbeValue $baselineRaw; foreach($round in $boundary.scenarios[1].rounds){$round.cppAllocations=2560}
Assert-PairedRawBudgetAndFixture $boundary Default
$passed.Add('all three native fixture IDs and the literal Release dirty allocation boundary pass')
foreach ($field in @('fixture','renderer','width','height','dpi','controls','modelRows','framesPerRound','roundCount','platform','configuration','dirtyAllocationCeilingPerFrame','nativeArchitecture')) {
    $changed = Copy-ProbeValue $baselineRaw
    if ($changed[$field] -is [string]) { $changed[$field] = 'another-fixture-or-profile' } else { $changed[$field] = [int]$changed[$field]+1 }
    Expect-Rejection "raw $field cannot drift away from the current native fixture/profile" { Assert-PairedRawBudgetAndFixture $changed Default } 'native contract|native x64'
}
$missingCeiling = Copy-ProbeValue $baselineRaw; $missingCeiling.Remove('dirtyAllocationCeilingPerFrame')
Expect-Rejection 'missing literal dirty allocation ceiling rejected' { Assert-PairedRawBudgetAndFixture $missingCeiling Default } 'native contract'

# Equal absolute violations deliberately produce no relative regression, proving that replay cannot replace these native hard bounds.
Import-Module (Join-Path $repo 'Tools/PerformanceComparison.psm1') -Force
foreach ($violation in @(@{phase=0;value=1;name='clean';reason='Raw clean whole-frame'},@{phase=1;value=2561;name='dirty';reason='Raw Release dirty whole-frame'})) {
    $a = Copy-ProbeValue $baselineRaw; $b = Copy-ProbeValue $candidateRaw
    foreach($receipt in @($a,$b)){foreach($round in $receipt.scenarios[$violation.phase].rounds){$round.cppAllocations=$violation.value}}
    $blocks = @(foreach($index in 1..12){@{name="block$index";order='ABBA';Baseline=@($a,$a);Candidate=@($b,$b)}})
    $relative = Compare-PairedBlockSet -Blocks $blocks -MinimumBlocks 12 -AssignmentProtocol (Get-PairedAssignmentProtocol)
    Require ($relative.status -ceq 'within-noise-budget' -and $relative.regressedMetrics -eq 0) 'Equal hard-budget violations unexpectedly relied on a relative regression for rejection.'
    foreach($receipt in @($a,$b)){ Expect-Rejection "equal A/B $($violation.name) absolute violation rejected for $($receipt.sourceCommit[0]) role" { Assert-PairedRawBudgetAndFixture $receipt Default } $violation.reason }
}
$singleOverflow = Copy-ProbeValue $candidateRaw; $singleOverflow.scenarios[1].rounds[4].cppAllocations=2561
Expect-Rejection 'single raw dirty round overflow rejected even when the run median stays within budget' { Assert-PairedRawBudgetAndFixture $singleOverflow Default } 'Raw Release dirty whole-frame'
$diagnosticLoop = @($ast.FindAll({param($node) $node -is [Management.Automation.Language.ForEachStatementAst] -and $node.Condition.Extent.Text -ceq '$diagnostic[''Scenarios'']'},$true))
Require ($diagnosticLoop.Count -eq 1) 'Cannot isolate diagnostic metric review.'
$diagnosticAction = [scriptblock]::Create($diagnosticLoop[0].Extent.Text)
$diagnostic = @{Conclusion='inconclusive';Scenarios=@()}; $dual = @{controlsByScenario=@{}}
$measuredLibraryChanged = $true
$flaggedMetrics = [Collections.Generic.List[object]]::new()
foreach ($scenarioName in @('Default','MultilineGrid','MultilineGridDistinct')) {
    $scenario = @{Scenario=$scenarioName;Conclusion='inconclusive';Metrics=@()}
    $controls = @{Total=24;Metrics=@{}}
    for ($index=0;$index -lt 26;$index++) {
        $scenario['Metrics'] += @{Phase='compose';Metric="m$index";Verdict='regressed';Outcome='unconfirmed';Held=$false;ControlDrifted=$true}
        $controls['Metrics']["compose/m$index"] = @{Observed=24;Drifted=$true;MaxDriftPercent=8}
    }
    $diagnostic['Scenarios'] += $scenario; $dual['controlsByScenario'][$scenarioName] = $controls
}
. $diagnosticAction
Require ($flaggedMetrics.Count -eq 78) 'Policy/strict-control inconclusive discarded unconfirmed measurement flags.'
$passed.Add('inconclusive HOLD retains all 78 synthetic unconfirmed flags')
$diagnostic['Scenarios'][0]['Conclusion'] = 'degraded'
Expect-Rejection 'degraded scenario hidden by inconclusive aggregate rejected' { . $diagnosticAction } 'Non-exempt diagnostic finding'
$diagnostic['Scenarios'][0]['Conclusion'] = 'inconclusive'
$diagnostic['Scenarios'][0]['Metrics'][0]['Outcome'] = 'confirmed'
Expect-Rejection 'confirmed metric hidden by inconclusive scenario rejected' { . $diagnosticAction } 'Confirmed measured degradation'
$diagnostic['Scenarios'][0]['Metrics'][0]['Outcome'] = 'unconfirmed'
$diagnostic['Scenarios'][0]['Metrics'][0]['Held'] = $true
$diagnostic['Scenarios'][0]['Metrics'][0]['ControlDrifted'] = $false
$dual['controlsByScenario']['Default']['Metrics']['compose/m0']['Drifted'] = $false
Expect-Rejection 'regressed plus held changed-library metric rejected even with unconfirmed projected outcome' { . $diagnosticAction } 'changed library inputs'
$result = [ordered]@{helperSha256=(Get-FileHash -LiteralPath $helper).Hash;passed=$passed.ToArray();checks=$passed.Count;scope='Pure external helper slices only; no candidate build/test or PR qualification.';completedUtc=[DateTime]::UtcNow.ToString('o')}
[IO.File]::WriteAllText((Join-Path $fixture 'audit.receipt.txt'),($result | ConvertTo-Json -Depth 5),$utf8)
Write-Host "HELPER_SLICE_AUDIT: $($passed.Count) checks passed; $fixture"
