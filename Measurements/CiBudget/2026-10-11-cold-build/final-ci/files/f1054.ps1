<# Pure helper slices against synthetic metadata and an already downloaded historical artifact.
No repository tests, builds, native execution, Git mutations or network requests. #>
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$utf8 = [Text.UTF8Encoding]::new($false)
$helper = Join-Path $PSScriptRoot 'Review-PrCiEvidence.ps1'
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($helper,[ref]$tokens,[ref]$errors)
if (@($errors).Count) { throw 'Helper parser errors.' }
foreach ($name in @('Require','Digest','Receipt-Instant','Profile-Job','Find-File','Receipt','Assert-CompletedInJob','Assert-Sha','Read-ExactExitCode','Read-ExactInteger','Assert-HostedLease','Assert-NativeSuiteLog','Assert-ConsumerMetadata','Assert-AsanProbeMetadata','Assert-NativeProducerStep','Bind-UploadedBuildAttestation','Assert-PairedRawBudgetAndFixture','Bind-PairedRoleReceipt')) {
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
function New-HostedProbeReceipt([string] $Suite='Menu', [string] $Platform='ARM64', [string] $Configuration='ASan Debug') {
    return @{suite=$Suite;platform=$Platform;configuration=$Configuration;exitCode=0;skips=@();interactive=$true;
        lease=@{mode='verified-github-hosted';hosted=$true;state='completed';exitCode=0;confirmation='started';autoStart=$false;requestedAutoStart=$false;
            confirmationSeconds=120;requestedConfirmationSeconds=120;seconds=0.3;runMovedSomething=$true;foreground='restored';focus='unchanged';cursor='restored';
            sha256=('1'*64);cursorSaved='10,20';cursorAtExit='30,40'}}
}
$nativeLogRoot = Join-Path $fixture 'native-success-logs'; [void][IO.Directory]::CreateDirectory($nativeLogRoot)
foreach ($binding in @(@{suite='Menu';platform='ARM64';configuration='ASan Debug'},@{suite='NativeTextInput';platform='ARM64';configuration='Debug'},
        @{suite='MenuResourceScaling';platform='x64';configuration='Release'},@{suite='MenuResources';platform='x64';configuration='Release'})) {
    $record = New-HostedProbeReceipt $binding.suite $binding.platform $binding.configuration
    Assert-HostedLease $record $false
    $log = Join-Path $nativeLogRoot "test-$($binding.suite)-$($binding.platform)-$($binding.configuration).hosted.log"
    [IO.File]::WriteAllText($log,"native fixture completed`n",$utf8)
    Require ((Assert-NativeSuiteLog $nativeLogRoot $record $binding.suite $binding.platform $binding.configuration) -ceq $log) 'Fresh hosted success log did not bind.'
}
$passed.Add('fresh hosted success preserves four foreground suite leases and exact raw log names')
$ordinaryResource=@{suite='MenuTextLayoutResources';platform='x64';configuration='Release';exitCode=0}
$ordinaryResourceLog=Join-Path $nativeLogRoot 'test-MenuTextLayoutResources-x64-Release.log'
[IO.File]::WriteAllText($ordinaryResourceLog,"[DONE] MenuTextLayoutResources (0.333 s)`nAll DxUi tests passed.`n",$utf8)
Require ((Assert-NativeSuiteLog $nativeLogRoot $ordinaryResource MenuTextLayoutResources x64 Release) -ceq $ordinaryResourceLog) 'The noninteractive DirectWrite layout fixture lost its ordinary producer log.'
$passed.Add('noninteractive MenuTextLayoutResources binds its ordinary log without invented foreground evidence')
$hosted = New-HostedProbeReceipt; $unchanged = Copy-ProbeValue $hosted
$unchanged.lease.foreground='unchanged'; $unchanged.lease.cursor='unchanged'; $unchanged.lease.runMovedSomething=$false; $unchanged.lease.seconds=0
Assert-HostedLease $unchanged $false
$passed.Add('successful unchanged desktop and rounded zero elapsed time remain valid')
foreach ($change in @(@{target='receipt';field='interactive';remove=$true;reason='hosted lease'},@{target='lease';field='mode';value='local-confirmed';reason='hosted mode'},
        @{target='lease';field='hosted';value='true';reason='hosted mode'},@{target='lease';field='state';value='launchfailed';reason='completed successful'},
        @{target='lease';field='exitCode';value=25;reason='completed successful'},@{target='lease';field='confirmation';value='none';reason='completed successful'},
        @{target='lease';field='autoStart';value=$true;reason='autoStart policy'},@{target='lease';field='requestedAutoStart';remove=$true;reason='requestedAutoStart policy'},
        @{target='lease';field='confirmationSeconds';value=30;reason='confirmation metadata'},@{target='lease';field='requestedConfirmationSeconds';value='120';reason='nonintegral'},
        @{target='lease';field='seconds';remove=$true;reason='elapsed time'},@{target='lease';field='seconds';value=[double]::NaN;reason='elapsed time'},
        @{target='lease';field='seconds';value=-0.1;reason='elapsed time'},@{target='lease';field='runMovedSomething';value='false';reason='movement metadata'},
        @{target='lease';field='foreground';value='none';reason='foreground restoration'},@{target='lease';field='focus';value='failed';reason='focus restoration'},
        @{target='lease';field='cursor';value='failed';reason='cursor restoration'},@{target='lease';field='sha256';value='unverified';reason='SHA256'})) {
    $changed = Copy-ProbeValue $hosted; $target = if($change['target'] -ceq 'receipt'){$changed}else{$changed['lease']}
    if($change['remove']){[void]$target.Remove($change['field'])}else{$target[$change['field']]=$change['value']}
    Expect-Rejection "hosted $($change['field']) mismatch cannot be accepted as success" { Assert-HostedLease $changed $false } $change['reason']
}
$refusal = Copy-ProbeValue $hosted; $refusal.exitCode=24; $refusal.lease.state='unwarned'; $refusal.lease.exitCode=23; $refusal.lease.seconds=$null
$refusal.lease.runMovedSomething=$false; foreach($part in @('foreground','focus','cursor')){$refusal.lease[$part]='unchanged'}
Assert-HostedLease $refusal $true
$desktopRefusal=Copy-ProbeValue $refusal; $desktopRefusal.lease.state='refused'; $desktopRefusal.lease.exitCode=20; $desktopRefusal.lease.confirmation='none'
Assert-HostedLease $desktopRefusal $true
$passed.Add('current exact hosted warning/desktop refusals remain candidates with suite failure 24')
Expect-Rejection 'refusal cannot be relabeled a passed native suite' { [void](Assert-NativeSuiteLog $nativeLogRoot $refusal Menu ARM64 'ASan Debug') } 'nonzero exit'
foreach($change in @(@{field='exitCode';value=25;reason='state, exit'},@{field='seconds';value=0;reason='elapsed child time'},
        @{field='runMovedSomething';value=$true;reason='movement'},@{field='autoStart';remove=$true;reason='autoStart policy'})) {
    $changed=Copy-ProbeValue $refusal; if($change['remove']){[void]$changed.lease.Remove($change['field'])}else{$changed.lease[$change['field']]=$change['value']}
    Expect-Rejection "refusal $($change['field']) contradiction blocks exception accounting" { Assert-HostedLease $changed $true } $change['reason']
}
$nativeLog=Join-Path $nativeLogRoot 'test-Menu-ARM64-ASan Debug.hosted.log'
foreach($line in @('SKIPPED: capability unavailable','TIMEOUT: case after 300 s','FAIL: native fixture','FAILED: native assertion','')) {
    [IO.File]::WriteAllText($nativeLog,$line,$utf8)
    Expect-Rejection 'zero-exit receipt cannot hide failed, skipped, timed-out or empty raw suite log' { [void](Assert-NativeSuiteLog $nativeLogRoot $hosted Menu ARM64 'ASan Debug') } 'suite log'
}
[IO.File]::WriteAllText($nativeLog,'[LEASE] Menu did not run under the foreground lease',$utf8)
Expect-Rejection 'unlaunched raw child cannot become passed evidence' { [void](Assert-NativeSuiteLog $nativeLogRoot $hosted Menu ARM64 'ASan Debug') } 'unlaunched child'
[IO.File]::WriteAllText($nativeLog,'completed without failures',$utf8)
$timed=Copy-ProbeValue $hosted; $timed.timeout=@{test='case';seconds=300}
Expect-Rejection 'timeout metadata blocks an exit-zero native receipt' { [void](Assert-NativeSuiteLog $nativeLogRoot $timed Menu ARM64 'ASan Debug') } 'timeout metadata'
$missingLogs=Join-Path $fixture 'missing-suite-logs'; [void][IO.Directory]::CreateDirectory($missingLogs)
Expect-Rejection 'absent native raw suite log blocks completion evidence' { [void](Assert-NativeSuiteLog $missingLogs $hosted Menu ARM64 'ASan Debug') } 'Expected one'
$duplicate=Join-Path $nativeLogRoot 'duplicate'; [void][IO.Directory]::CreateDirectory($duplicate)
[IO.File]::WriteAllText((Join-Path $duplicate ([IO.Path]::GetFileName($nativeLog))),'completed',$utf8)
Expect-Rejection 'duplicate canonical native raw log blocks binding' { [void](Assert-NativeSuiteLog $nativeLogRoot $hosted Menu ARM64 'ASan Debug') } 'Expected one'

$apiRevision=Read-ExactInteger (Get-Content -Raw -LiteralPath (Join-Path $repo 'capabilities.json') | ConvertFrom-Json -AsHashtable) 'apiRevision'
function Set-ProbeConsumerFingerprint([System.Collections.IDictionary] $Value) {
    $ordered=[ordered]@{}; foreach($key in @('commit','apiRevision','platform','toolset','vcToolsVersion','windowsSdkVersion','runtimeFamily','preferredToolArchitecture','disableStlAnnotations','msbuildSha256','compilerSha256','linkerSha256','windowsHeaderSha256','kernel32Sha256','ucrtSha256')){$ordered[$key]=$Value['buildIdentity']['Identity'][$key]}
    $Value['buildIdentity']['Fingerprint']=Digest ($utf8.GetBytes(($ordered | ConvertTo-Json -Compress)))
}
function New-ConsumerProbe([bool] $Disabled) {
    $value=@{suite='ExternalConsumer';commit=('b'*40);platform='ARM64';configuration='ASan Debug';disableStlAnnotations=$Disabled;exitCode=0;negativeChecks=10;nativeModuleChecks=12;
        buildIdentity=@{Identity=[ordered]@{commit=('b'*40);apiRevision=$apiRevision;platform='ARM64';toolset='v145';vcToolsVersion='14.50.fixture';windowsSdkVersion='10.0.fixture';runtimeFamily='MD';preferredToolArchitecture='arm64';disableStlAnnotations=$Disabled;
            msbuildSha256=('1'*64);compilerSha256=('2'*64);linkerSha256=('3'*64);windowsHeaderSha256=('4'*64);kernel32Sha256=('5'*64);ucrtSha256=('6'*64)}}}
    Set-ProbeConsumerFingerprint $value; return $value
}
$consumer=New-ConsumerProbe $false; $disabledConsumer=New-ConsumerProbe $true
$regularFingerprint=Assert-ConsumerMetadata $consumer ('b'*40) ARM64 $false $apiRevision
$disabledFingerprint=Assert-ConsumerMetadata $disabledConsumer ('b'*40) ARM64 $true $apiRevision
Require ($regularFingerprint -cne $disabledFingerprint) 'Valid annotation variants share a fingerprint.'
$passed.Add('both typed consumer variants bind the dynamic frozen API and distinct exact fingerprints')
foreach($change in @(@{target='identity';field='apiRevision';value=($apiRevision+1);reason='API revision'},@{target='identity';field='apiRevision';value="$apiRevision";reason='nonintegral'},
        @{target='identity';field='runtimeFamily';value='MT';reason='runtime family'},@{target='identity';field='preferredToolArchitecture';value='foreign';reason='compiler host'},
        @{target='identity';field='toolset';value=' ';reason='blank'},@{target='identity';field='vcToolsVersion';remove=$true;reason='blank'},
        @{target='identity';field='windowsSdkVersion';value=123;reason='blank'},@{target='identity';field='disableStlAnnotations';value=0;reason='annotation policy'},
        @{target='receipt';field='disableStlAnnotations';value='false';reason='annotation policy'},@{target='receipt';field='negativeChecks';value='10';reason='nonintegral'},
        @{target='receipt';field='nativeModuleChecks';value=[double]12;reason='nonintegral'},@{target='receipt';field='negativeChecks';remove=$true;reason='nonintegral'})) {
    $changed=Copy-ProbeValue $consumer; $target=if($change['target'] -ceq 'identity'){$changed['buildIdentity']['Identity']}else{$changed}
    if($change['remove']){[void]$target.Remove($change['field'])}else{$target[$change['field']]=$change['value']}
    Set-ProbeConsumerFingerprint $changed
    Expect-Rejection "self-consistent consumer $($change['field']) mismatch remains rejected" { [void](Assert-ConsumerMetadata $changed ('b'*40) ARM64 $false $apiRevision) } $change['reason']
}
Expect-Rejection 'invalid frozen API scalar cannot authorize consumer metadata' { [void](Assert-ConsumerMetadata $consumer ('b'*40) ARM64 $false $true) } 'API revision is not an integer'

$foundation=@{suite='Foundation';platform='ARM64';configuration='ASan Debug';nativeArchitecture='Arm64';exitCode=0;executable='C:/fixture/DxUi.FoundationTests.exe';sha256=('a'*64)}
$asan=@{suite='AddressSanitizer';platform='ARM64';configuration='ASan Debug';nativeArchitecture='Arm64';exitCode=-1073741819;detected=$true;executable=$foundation.executable;sha256=$foundation.sha256}
$probeText='AddressSanitizer: heap-use-after-free'; $probeJobText='PASS AddressSanitizer detection probe (isolated expected failure)'
Assert-AsanProbeMetadata $asan $foundation ARM64 'ASan Debug' $probeText $probeJobText
$passed.Add('intentional signed ASan negative binds successful ordinary Foundation bytes/path and one completed probe')
foreach($change in @(@{field='nativeArchitecture';remove=$true;reason='architecture'},@{field='nativeArchitecture';value='X64';reason='architecture'},
        @{field='configuration';value='Debug';reason='profiles differ'},@{field='sha256';value=('b'*64);reason='bytes/path'},
        @{field='executable';value='C:/other/DxUi.FoundationTests.exe';reason='bytes/path'},@{field='detected';value='true';reason='not detected'},@{field='exitCode';value=0;reason='not detected'})) {
    $changed=Copy-ProbeValue $asan; if($change['remove']){[void]$changed.Remove($change['field'])}else{$changed[$change['field']]=$change['value']}
    Expect-Rejection "ASan $($change['field']) mismatch cannot satisfy detection" { Assert-AsanProbeMetadata $changed $foundation ARM64 'ASan Debug' $probeText $probeJobText } $change['reason']
}
Expect-Rejection 'missing intentional ASan diagnosis rejected' { Assert-AsanProbeMetadata $asan $foundation ARM64 'ASan Debug' 'ordinary crash' $probeJobText } 'intentional diagnosis'
Expect-Rejection 'missing ASan final completion rejected' { Assert-AsanProbeMetadata $asan $foundation ARM64 'ASan Debug' $probeText '' } 'one completed'
Expect-Rejection 'duplicate ASan final completion rejected' { Assert-AsanProbeMetadata $asan $foundation ARM64 'ASan Debug' $probeText ($probeJobText+"`n"+$probeJobText) } 'one completed'
$badFoundation=Copy-ProbeValue $foundation; $badFoundation.exitCode=1
Expect-Rejection 'ordinary Foundation failure is not an intentional ASan negative' { Assert-AsanProbeMetadata $asan $badFoundation ARM64 'ASan Debug' $probeText $probeJobText } 'ordinary Foundation failed'

$completedJob=@{status='completed';conclusion='success';steps=@(@{name='Run ./test.ps1 -Full -SkipTooling -Configuration "ASan Debug" -Platform ARM64';status='completed';conclusion='success'})}
Assert-NativeProducerStep $completedJob 0
$failedJob=Copy-ProbeValue $completedJob; $failedJob.conclusion='failure'; $failedJob.steps[0].conclusion='failure'
Assert-NativeProducerStep $failedJob 2
$passed.Add('one completed Full step succeeds or retains the sole verified foreground-refusal failure')
foreach($change in @(@{field='steps';value=@();reason='missing'},@{field='steps';value=@($completedJob.steps[0],$completedJob.steps[0]);reason='duplicated'},
        @{field='status';value='cancelled';reason='incomplete'})) {
    $changed=Copy-ProbeValue $completedJob; $changed[$change['field']]=$change['value']
    Expect-Rejection "native $($change['field']) noncompletion rejected" { Assert-NativeProducerStep $changed 0 } $change['reason']
}
foreach($outcome in @('skipped','cancelled','failure')) {
    $changed=Copy-ProbeValue $completedJob; $changed.steps[0].conclusion=$outcome
    Expect-Rejection "green job cannot accept $outcome Full step" { Assert-NativeProducerStep $changed 0 } 'successful Full step'
}
Expect-Rejection 'failed Full step without verified refusal cannot be accepted' { Assert-NativeProducerStep $failedJob 0 } 'beyond the candidate'
$extraFailure=Copy-ProbeValue $failedJob; $extraFailure.steps += @{name='Qualify the relocated public consumer';status='completed';conclusion='failure'}
Expect-Rejection 'foreground refusal cannot excuse a failed consumer stage' { Assert-NativeProducerStep $extraFailure 2 } 'beyond the candidate'
Expect-Rejection 'green native job cannot conceal a foreground refusal' { Assert-NativeProducerStep $completedJob 1 } 'hides a failed'

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
