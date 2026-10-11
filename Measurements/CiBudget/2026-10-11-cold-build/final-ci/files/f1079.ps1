<# Pure evidence review: reads the frozen repository and downloaded receipts/logs, replays judges, writes one external report.
Does not build, execute native tests, rerun CI, change policy or merge. Exit 0 means no new blocker found; never ci-gate success. #>
[CmdletBinding()] param([Parameter(Mandatory)][string] $CollectionRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$CollectionRoot = [IO.Path]::GetFullPath($CollectionRoot)
$approvedExternalRoot = [IO.Path]::GetFullPath('C:/Users/eric/.codex/tmp/dpf-s0-s2-merge')
if (-not $CollectionRoot.StartsWith($approvedExternalRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Review output must stay inside the approved external evidence directory.' }
$utf8 = [Text.UTF8Encoding]::new($false)
$collection = Get-Content -Raw -LiteralPath (Join-Path $CollectionRoot 'collection.receipt.txt') | ConvertFrom-Json -AsHashtable
$RepositoryRoot = [string]$collection['repositoryRoot']
$jobs = @(Get-Content -Raw -LiteralPath (Join-Path $CollectionRoot 'jobs.receipt.txt') | ConvertFrom-Json -AsHashtable)
$manifest = Get-Content -Raw -LiteralPath (Join-Path $CollectionRoot 'source/test-scopes.receipt.txt') | ConvertFrom-Json -AsHashtable
$blockers = [Collections.Generic.List[object]]::new()
$nativeEvidence = [Collections.Generic.List[object]]::new()
$consumerEvidence = [Collections.Generic.List[object]]::new()
$probeEvidence = [Collections.Generic.List[object]]::new()
$armRefusalCandidates = [Collections.Generic.List[object]]::new()
$flaggedMetrics = [Collections.Generic.List[object]]::new()
$galleryEvidence = [Collections.Generic.List[object]]::new()
$attestationEvidence = [Collections.Generic.List[object]]::new()
$nativeJobBudgets = [Collections.Generic.List[object]]::new()
$pairedEvidence = $null

function Require([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
function Digest([byte[]] $Bytes) { return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant() }
function Receipt-Instant([object] $Value) {
    if ($Value -is [DateTimeOffset]) { return $Value }
    if ($Value -is [DateTime]) { return [DateTimeOffset]::new($Value.ToUniversalTime()) }
    return [DateTimeOffset]::Parse([string]$Value, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::RoundtripKind)
}
function Review-Check([string] $Label, [scriptblock] $Action) {
    try { & $Action } catch { $blockers.Add([ordered]@{ check = $Label; reason = $_.Exception.Message }) }
}
function Assert-Freeze {
    $head = (& git -C $RepositoryRoot rev-parse HEAD).Trim()
    Require ($LASTEXITCODE -eq 0 -and $head -ceq $collection['expectedHead']) 'Local HEAD changed since source-bound collection.'
    $status = @(& git -C $RepositoryRoot status --porcelain --untracked-files=normal)
    Require ($LASTEXITCODE -eq 0 -and $status.Count -eq 0) 'Frozen repository is dirty; do not replay its current modules as the tested source.'
    $tree = (& git -C $RepositoryRoot rev-parse 'HEAD^{tree}').Trim()
    Require ($LASTEXITCODE -eq 0 -and $tree -ceq $collection['testedTree']) 'Frozen source tree differs from the actual tested PR merge tree.'
}
function Assert-Seal {
    foreach ($line in [IO.File]::ReadAllLines((Join-Path $CollectionRoot 'SHA256SUMS'))) {
        Require ($line -cmatch '^(?<hash>[a-f0-9]{64})  (?<path>.+)$') 'Malformed evidence seal entry.'
        $expected = $Matches['hash']; $relative = $Matches['path']
        $path = [IO.Path]::GetFullPath((Join-Path $CollectionRoot $relative))
        Require ($path.StartsWith($CollectionRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) 'Seal path escapes its collection.'
        Require ((Digest ([IO.File]::ReadAllBytes($path))) -ceq $expected) "Collected evidence bytes changed: $relative"
    }
}
function Artifact-Root([string] $Name) {
    $selectedRecords = @($collection['artifacts'] | Where-Object { $_['name'] -ceq $Name })
    Require ($selectedRecords.Count -eq 1) "Missing or duplicate artifact binding: $Name"
    return Join-Path $CollectionRoot $selectedRecords[0]['directory']
}
function Find-File([string] $Directory, [string] $Leaf) {
    $selectedRecords = @(Get-ChildItem -LiteralPath $Directory -File -Recurse | Where-Object { $_.Name -ceq $Leaf })
    Require ($selectedRecords.Count -eq 1) "Expected one $Leaf in $Directory, found $($selectedRecords.Count)."
    return $selectedRecords[0].FullName
}
function Receipt([string] $Directory, [string] $Suite, [string] $Platform, [string] $Configuration, [Nullable[bool]] $Disabled = $null) {
    $selectedRecords = [Collections.Generic.List[object]]::new()
    foreach ($file in (Get-ChildItem -LiteralPath $Directory -Filter '*.json' -File -Recurse)) {
        $value = Get-Content -Raw -LiteralPath $file.FullName | ConvertFrom-Json -AsHashtable
        if ($value -is [System.Collections.IDictionary] -and $value['suite'] -ceq $Suite -and $value['platform'] -ceq $Platform -and
            $value['configuration'] -ceq $Configuration -and -not $value.Contains('tests') -and
            ($null -eq $Disabled -or ($value['disableStlAnnotations'] -is [bool] -and $value['disableStlAnnotations'] -eq $Disabled))) {
            $selectedRecords.Add([ordered]@{ file = $file.FullName; value = $value; sha256 = (Get-FileHash -LiteralPath $file.FullName).Hash })
        }
    }
    Require ($selectedRecords.Count -eq 1) "Expected one unfiltered $Platform/$Configuration/$Suite receipt, found $($selectedRecords.Count)."
    return $selectedRecords[0]
}
function Profile-Job([string] $Platform, [string] $Configuration) {
    $runner = if ($Platform -ceq 'ARM64') { 'windows-11-vs2026-arm' } else { 'windows-2025-vs2026' }
    $budget = if ($Platform -ceq 'ARM64' -and $Configuration -ceq 'ASan Debug') { 60 } else { 40 }
    $pattern = '^native \(' + [regex]::Escape($Platform) + ', ' + [regex]::Escape($runner) + ', ' + [regex]::Escape($Configuration) + '(?:, (?<budget>40|60))?\)$'
    $selectedRecords = @($jobs | Where-Object { $_['name'] -cmatch $pattern })
    Require ($selectedRecords.Count -eq 1) "Missing or duplicate native job $Platform/$Configuration."
    $display = [regex]::Match($selectedRecords[0]['name'], $pattern)
    Require (-not $display.Groups['budget'].Success -or [int]$display.Groups['budget'].Value -eq $budget) 'Expanded native display names another selected job budget.'
    $nativeJobBudgets.Add([ordered]@{ profile="$Platform/$Configuration"; job=$selectedRecords[0]['id']; name=$selectedRecords[0]['name']; runner=$runner; declaredBudgetMinutes=$budget; displayedBudgetMinutes=$(if ($display.Groups['budget'].Success) { [int]$display.Groups['budget'].Value } else { $null }); workflowDigest=$collection['workflowDigest'] })
    return $selectedRecords[0]
}
function Assert-CompletedInJob([System.Collections.IDictionary] $Value, [System.Collections.IDictionary] $Job) {
    $time = Receipt-Instant $Value['completedUtc']
    Require ($time -ge (Receipt-Instant $Job['started_at']) -and $time -le (Receipt-Instant $Job['completed_at'])) 'Receipt completion is outside the collected job attempt.'
}
function Assert-Sha([object] $Value, [string] $Name) { Require ([string]$Value -cmatch '^[A-Fa-f0-9]{64}$') "Invalid or missing $Name." }
function Read-ExactExitCode([System.Collections.IDictionary] $Value) {
    Require ($Value.Contains('exitCode') -and ($Value['exitCode'] -is [int] -or $Value['exitCode'] -is [long])) 'Missing or nonintegral exitCode; null, booleans and strings are not native exit results.'
    Require ($Value['exitCode'] -ge [int]::MinValue -and $Value['exitCode'] -le [int]::MaxValue) 'Exit code is outside the signed native process range.'
    return [int]$Value['exitCode']
}

function Assert-PairedRawBudgetAndFixture([System.Collections.IDictionary] $Value, [string] $Scenario) {
    $fixtures = @{ Default='dxui-complex-ui-v2'; MultilineGrid='dxui-complex-ui-multiline-grid-v1'; MultilineGridDistinct='dxui-complex-ui-multiline-grid-distinct-v1' }
    Require ($fixtures.ContainsKey($Scenario)) 'Unexpected raw paired scenario.'
    $expected = [ordered]@{ fixture=$fixtures[$Scenario]; renderer='WARP'; width=1280; height=720; dpi=96; controls=83; modelRows=1000; framesPerRound=40; roundCount=5; platform='x64'; configuration='Release'; dirtyAllocationCeilingPerFrame=64 }
    foreach ($key in $expected.Keys) {
        Require ($Value.Contains($key) -and (ConvertTo-Json -InputObject $Value[$key] -Compress) -ceq (ConvertTo-Json -InputObject $expected[$key] -Compress)) "Raw paired fixture/profile differs from the current native contract: $Scenario/$key."
    }
    Require ($Value.Contains('nativeArchitecture') -and [string]::Equals([string]$Value['nativeArchitecture'],'x64',[StringComparison]::OrdinalIgnoreCase)) 'Raw paired architecture is not native x64.'
    $phases = @($Value['scenarios'])
    Require ($phases.Count -eq 2 -and (@($phases | ForEach-Object { $_['name'] } | Sort-Object) -join ',') -ceq 'clean,dirty') 'Raw paired clean/dirty inventory differs from the native contract.'
    foreach ($phase in $phases) {
        Require (@($phase['rounds']).Count -eq 5) 'Raw paired phase does not retain all five rounds.'
        foreach ($round in $phase['rounds']) {
            $allocations = $round['cppAllocations']
            Require ($allocations -is [ValueType] -and [double]::IsFinite([double]$allocations) -and [double]$allocations -ge 0 -and [double]$allocations -eq [Math]::Floor([double]$allocations)) 'Raw paired C++ allocation count is not a finite nonnegative integer.'
            if ($phase['name'] -ceq 'clean') { Require ($allocations -eq 0) 'Raw clean whole-frame C++ allocation budget is not zero.' }
            else { Require ($allocations -le 2560) 'Raw Release dirty whole-frame C++ allocation budget exceeds 64 per frame / 2560 per round.' }
        }
    }
}

function Bind-PairedRoleReceipt([System.Collections.IDictionary] $Summary, [System.Collections.IDictionary] $Scenario,
    [string] $Run, [System.Collections.IDictionary] $Value, [System.Collections.IDictionary] $SideStates, [string[]] $HarnessPaths) {
    $steps = @($Summary['steps'] | Where-Object { $_['Name'] -ceq $Run })
    Require ($steps.Count -eq 1 -and $steps[0]['Side'] -cin @('baseline','candidate')) 'Raw paired receipt does not identify one scheduled role.'
    $role = [string]$steps[0]['Side']; $side = $Summary[$role]
    Require ($side -is [System.Collections.IDictionary]) 'Raw paired role has no summary side.'
    Require ($Value['sourceCommit'] -ceq $side['commit']) "Raw paired commit does not match its scheduled $role side: $Run."
    Assert-Sha $side['sourceFingerprint'] "$role summary sourceFingerprint"
    Require ($Value['sourceFingerprint'] -ceq $side['sourceFingerprint']) "Raw paired fingerprint does not match its scheduled $role side: $Run."
    Require ($Value.Contains('sourceDirty') -and $Value['sourceDirty'] -is [bool]) 'Raw paired dirty provenance is missing or malformed.'
    # Current harness overlays legitimately dirty an older measured checkout. Retain that literal fact; it is not clean-source evidence.
    Require ($side.Contains('harnessOverlay')) 'Raw paired summary side lacks explicit harness-overlay provenance.'
    $overlays = @($side['harnessOverlay']); $seenPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($overlay in $overlays) {
        Require ([string]$overlay -cmatch '^(created|replaced) (.+)$') 'Unsupported paired harness-overlay record.'
        $path = $Matches[2]
        Require ($path -cin $HarnessPaths -and $seenPaths.Add($path)) 'Paired harness-overlay path is unsupported or repeated.'
    }
    if ($Value['sourceDirty']) { Require ($overlays.Count -gt 0) 'Dirty paired source has no supported harness-overlay explanation.' }
    $fields = @('sourceCommit','sourceFingerprint','sourceDirty','executableSha256','benchmarkSha256','harnessIdentity','toolchainIdentity','dependencyIdentity','environmentIdentity',
        'resolvedBuildIdentity','buildInputIdentity','buildArtifactIdentity','buildAttestationIdentity','buildIdentityVersion','identityScope','identityStatus',
        'platform','configuration','nativeArchitecture','machine','cpu','os','compiler','warpVersion','warpSha256','powerPolicy')
    foreach ($key in $fields) {
        Require ($Value.Contains($key)) "Raw paired role is missing $key."
        # Current summaries put only commit/fingerprint on a side, but bind any additional exact fields when available.
        if ($side.Contains($key)) { Require ((ConvertTo-Json -InputObject $side[$key] -Compress) -ceq (ConvertTo-Json -InputObject $Value[$key] -Compress)) "Raw paired side field differs: $role/$key." }
    }
    $runs = $Scenario['runs']
    Require ($runs -is [System.Collections.IDictionary] -and $runs.Contains($Run)) 'Raw paired report lacks its retained run identity.'
    $record = $runs[$Run]
    Require ($record -is [System.Collections.IDictionary]) 'Raw paired retained run identity is malformed.'
    foreach ($key in @('sourceCommit','sourceDirty','sourceFingerprint','executableSha256','benchmarkSha256')) {
        Require ($record.Contains($key) -and (ConvertTo-Json -InputObject $record[$key] -Compress) -ceq (ConvertTo-Json -InputObject $Value[$key] -Compress)) "Retained paired run identity differs from its raw report: $Run/$key."
    }
    foreach ($key in $fields) {
        if ($record.Contains($key)) { Require ((ConvertTo-Json -InputObject $record[$key] -Compress) -ceq (ConvertTo-Json -InputObject $Value[$key] -Compress)) "Retained paired run field differs: $Run/$key." }
    }
    if (-not $SideStates.Contains($role)) {
        $state = [ordered]@{}
        foreach ($key in $fields) { $state[$key] = $Value[$key] }
        $SideStates[$role] = $state
    } else {
        foreach ($key in $fields) { Require ((ConvertTo-Json -InputObject $SideStates[$role][$key] -Compress) -ceq (ConvertTo-Json -InputObject $Value[$key] -Compress)) "Raw paired identity changed within the scheduled $role role: $key." }
    }
}

function Bind-UploadedBuildAttestation([string] $Artifact, [System.Collections.IDictionary] $Performance, [System.Collections.IDictionary] $Job) {
    $profile = [string]$Performance['platform'] + '-' + ([string]$Performance['configuration'] -replace ' ','')
    $pattern = '[/\\]performance-build[/\\]' + [regex]::Escape($profile) + '[/\\]build\.json$'
    $files = @(Get-ChildItem -LiteralPath $Artifact -Filter 'build.json' -File -Recurse | Where-Object { $_.FullName -cmatch $pattern })
    if ($files.Count -eq 0) {
        return [ordered]@{ profile=$profile; status='NOT_UPLOADED_BY_CURRENT_WORKFLOW'; verified=$false; buildAttestationIdentity=$Performance['buildAttestationIdentity']; limitation='No uploaded sidecar bytes; the valid recorded v2 identity and exact-source runner checks do not independently reproduce absent compiler/dependency/artifact inventories.' }
    }
    $bound = [Collections.Generic.List[object]]::new()
    foreach ($file in $files) {
        $sidecar = Get-Content -Raw -LiteralPath $file.FullName | ConvertFrom-Json -AsHashtable
        if ($sidecar['identity'] -ceq $Performance['buildAttestationIdentity']) { $bound.Add([ordered]@{ file=$file.FullName; value=$sidecar }) }
    }
    Require ($bound.Count -eq 1) 'Uploaded sidecars do not uniquely bind this performance report to its aggregate v2 build attestation.'
    $record = $bound[0]; $sidecar = $record['value']
    Require ($sidecar['version'] -eq 1 -and $sidecar['outcome'] -ceq 'PASSED') 'Uploaded scoped build sidecar is not a successful receipt.'
    $completed = if ($sidecar['completedUtc'] -is [DateTime]) { $sidecar['completedUtc'].ToUniversalTime().ToString('o') } else { [string]$sidecar['completedUtc'] }
    Require ((Digest ($utf8.GetBytes($sidecar['identity'] + "`n" + $completed + "`nPASSED"))) -ceq $sidecar['digest']) 'Uploaded build sidecar digest does not reproduce from its exact identity/completion/outcome fields.'
    Require ((Get-PerformanceBuildAttestationIdentity -State $Performance -ArtifactIdentity $Performance['buildArtifactIdentity']) -ceq $sidecar['identity']) 'Uploaded build sidecar does not bind the report resolved/input/toolchain/artifact fields under the v2 domain.'
    Assert-CompletedInJob $sidecar $Job
    Require ((Receipt-Instant $completed) -le (Receipt-Instant $Performance['completedUtc'])) 'Build sidecar was published after its performance measurement.'
    return [ordered]@{ profile=$profile; status='UPLOADED_SCOPED_AGGREGATE_VERIFIED'; verified=$true; file=$record['file']; sha256=(Get-FileHash -LiteralPath $record['file']).Hash; buildAttestationIdentity=$sidecar['identity']; resolvedBuildIdentity=$Performance['resolvedBuildIdentity']; buildInputIdentity=$Performance['buildInputIdentity']; toolchainIdentity=$Performance['toolchainIdentity']; buildArtifactIdentity=$Performance['buildArtifactIdentity']; limitation='The scoped sidecar binds the aggregate identity; it contains no compiler/dependency/artifact inventory bytes to reproduce the individual input digests.' }
}

# These are readers/analysis functions. The immutable base comes from git show in this unchanged repository;
# the current candidate module tree was bound to the actual tested merge before importing it.
Assert-Freeze
Assert-Seal
Import-Module (Join-Path $RepositoryRoot 'Tools/PerformanceComparison.psm1') -Force
Import-Module (Join-Path $RepositoryRoot 'Tools/PerformancePolicy.psm1') -Force
Import-Module (Join-Path $RepositoryRoot 'Tools/PairedRun.psm1') -Force
Import-Module (Join-Path $RepositoryRoot 'Tools/BenchmarkGate.psm1') -Force

foreach ($name in @('validation', 'windows-tooling', 'format', 'native-scope')) {
    Review-Check $name {
        $job = @($jobs | Where-Object { $_['name'] -ceq $name })
        Require ($job.Count -eq 1 -and $job[0]['status'] -ceq 'completed' -and $job[0]['conclusion'] -ceq 'success') "Required ordinary job $name did not succeed."
    }
}

foreach ($profile in $manifest['prCoverage']) {
    $platform = [string]$profile['platform']; $configuration = [string]$profile['configuration']
    $profileName = "$platform/$configuration"
    $artifact = Artifact-Root "native-$platform-$configuration"
    $job = Profile-Job $platform $configuration
    $jobLog = Find-File (Join-Path $CollectionRoot 'logs') "job-$($job['id']).log"
    $jobText = [IO.File]::ReadAllText($jobLog)
    $profileRefusals = [Collections.Generic.List[object]]::new()
    $expectedSuites = @($profile['scopes'] | Where-Object { $_ -cne 'Tooling' }) + @('Menu', 'NativeTextInput')
    if ($platform -ceq 'x64' -and $configuration -ceq 'Release') { $expectedSuites += @('MenuResourceScaling', 'MenuTextLayoutResources') }
    foreach ($suite in $expectedSuites) {
        Review-Check "$profileName/$suite" {
            $record = Receipt $artifact $suite $platform $configuration
            $value = $record['value']
            Assert-CompletedInJob $value $job
            Require ([string]::Equals([string]$value['nativeArchitecture'], $platform, [StringComparison]::OrdinalIgnoreCase)) 'Native architecture mismatch; a cross-build is not runtime qualification.'
            Assert-Sha $value['sha256'] 'native executable SHA256'
            Require (@($value['skips']).Count -eq 0) 'Capability skips are incomplete qualification and are not a dated safe-refusal exception.'
            $performanceLeaf = ([string]$value['performanceReport'] -split '[/\\]')[-1]
            $performancePath = Find-File $artifact $performanceLeaf
            $performance = Read-PerformanceReceipt $performancePath
            Assert-PerformanceReceipt $performance
            $identity = Test-PerformanceReceiptIdentity -Receipt $performance
            Require $identity['verifiable'] "Native performance provenance cannot be verified: $($identity['missing'] -join ', ')"
            Require ($performance['sourceCommit'] -ceq $collection['testedMerge'] -and $performance['sourceDirty'] -eq $false) 'Native receipt is not the exact clean tested merge commit.'
            Require ($performance['platform'] -ceq $platform -and $performance['configuration'] -ceq $configuration) 'Performance receipt profile differs from suite profile.'
            Require ([string]$value['performanceExecutableSha256'] -ceq [string]$performance['executableSha256']) 'Suite and attached performance executable hashes differ.'
            Require ($performance['hiddenPreparations'] -eq 0 -and $performance['hiddenComposites'] -eq 0) 'Hidden work violates the exact resource gate.'
            Assert-CompletedInJob $performance $job
            $attestationBinding = Bind-UploadedBuildAttestation $artifact $performance $job
            $attestationBinding['performanceReport'] = $performancePath
            $attestationEvidence.Add($attestationBinding)
            $exit = Read-ExactExitCode $value
            $possibleRefusal = $platform -ceq 'ARM64' -and $suite -cin @('Menu', 'NativeTextInput') -and $exit -eq 24 -and
                $value['lease'] -is [System.Collections.IDictionary] -and $value['lease']['hosted'] -eq $true -and
                $value['lease']['state'] -cin @('refused', 'unwarned') -and $null -eq $value['lease']['seconds'] -and
                $value['lease']['runMovedSomething'] -eq $false -and $value['lease']['foreground'] -ceq 'unchanged' -and
                $value['lease']['focus'] -ceq 'unchanged' -and $value['lease']['cursor'] -ceq 'unchanged'
            if ($possibleRefusal) {
                $suiteLog = Find-File $artifact "test-$suite-$platform-$configuration.hosted.log"
                $suiteText = [IO.File]::ReadAllText($suiteLog)
                Require ($suiteText.Trim() -ceq "[LEASE] $suite did not run under the foreground lease") 'Refusal log does not prove that this suite never launched.'
                Require ($jobText.Contains("FAIL $suite did not run under the foreground lease") -and $jobText.Contains('Foreground lease (')) 'Foreground refusal/restoration evidence is absent from the exact job log.'
                $candidate = [ordered]@{ profile = $profileName; suite = $suite; exitCode = $exit; receipt = $record['file']; receiptSha256 = $record['sha256']; job = $job['id']; jobLog = $jobLog; lease = $value['lease']; classification = 'Human must verify the precise warning/desktop refusal occurred before suite input; candidate only, not PASSED.' }
                $profileRefusals.Add($candidate); $armRefusalCandidates.Add($candidate)
            } else { Require ($exit -eq 0) "Non-exempt native failure: exit $exit." }
            $nativeEvidence.Add([ordered]@{ profile = $profileName; suite = $suite; exitCode = $exit; receipt = $record['file']; receiptSha256 = $record['sha256']; executableSha256 = $value['sha256']; performance = $performancePath; buildInputIdentity = $performance['buildInputIdentity']; buildArtifactIdentity = $performance['buildArtifactIdentity']; attestation = $performance['buildAttestationIdentity']; uploadedAttestationBinding=$attestationBinding; possibleDatedArmRefusal = [bool]$possibleRefusal })
        }
    }
    $annotations = @($false)
    if ($configuration -ceq 'ASan Debug') { $annotations += $true }
    $fingerprints = [Collections.Generic.List[string]]::new()
    foreach ($disabled in $annotations) {
        Review-Check "$profileName/consumer/annotationsDisabled=$disabled" {
            $record = Receipt $artifact 'ExternalConsumer' $platform $configuration $disabled
            $value = $record['value']; $identity = $value['buildIdentity']['Identity']
            $exit = Read-ExactExitCode $value
            Require ($exit -eq 0 -and $value['commit'] -ceq $collection['testedMerge'] -and
                $value['negativeChecks'] -eq 10 -and $value['nativeModuleChecks'] -eq 12) 'Synthetic consumer is incomplete, failed or not the exact tested merge pin.'
            Require ([string]::Equals([string]$value['nativeArchitecture'], $platform, [StringComparison]::OrdinalIgnoreCase)) 'Consumer native architecture differs from its profile.'
            Assert-CompletedInJob $value $job
            Assert-Sha $value['executableSha256'] 'consumer executable SHA256'; Assert-Sha $value['librarySha256'] 'consumer library SHA256'
            Require ($identity['commit'] -ceq $collection['testedMerge'] -and $identity['platform'] -ceq $platform -and $identity['disableStlAnnotations'] -eq $disabled) 'Consumer build identity differs from its exact pin/profile/annotation policy.'
            $orderedIdentity = [ordered]@{}
            foreach ($key in @('commit','apiRevision','platform','toolset','vcToolsVersion','windowsSdkVersion','runtimeFamily','preferredToolArchitecture','disableStlAnnotations','msbuildSha256','compilerSha256','linkerSha256','windowsHeaderSha256','kernel32Sha256','ucrtSha256')) {
                Require ($identity.Contains($key) -and $null -ne $identity[$key]) "Missing consumer build identity field: $key"
                $orderedIdentity[$key] = $identity[$key]
                if ($key.EndsWith('Sha256')) { Assert-Sha $identity[$key] $key }
            }
            $fingerprint = Digest ($utf8.GetBytes(($orderedIdentity | ConvertTo-Json -Compress)))
            Require ($fingerprint -ceq $value['buildIdentity']['Fingerprint']) 'Consumer fingerprint does not reproduce from the retained toolchain/SDK identity.'
            $fingerprints.Add($fingerprint)
            $consumerEvidence.Add([ordered]@{ profile = $profileName; annotationsDisabled = $disabled; commit = $value['commit']; exitCode = $value['exitCode']; negativeChecks = $value['negativeChecks']; nativeModuleChecks = $value['nativeModuleChecks']; receipt = $record['file']; receiptSha256 = $record['sha256']; executableSha256 = $value['executableSha256']; librarySha256 = $value['librarySha256']; buildIdentity = $value['buildIdentity'] })
        }
    }
    Review-Check "$profileName/consumer-step-results" {
        $regular = @($job['steps'] | Where-Object { $_['name'] -ceq 'Qualify the relocated public consumer' })
        Require ($regular.Count -eq 1 -and $regular[0]['conclusion'] -ceq 'success') 'Regular consumer step did not succeed; a native refusal cannot excuse it.'
        if ($configuration -ceq 'ASan Debug') {
            $variant = @($job['steps'] | Where-Object { $_['name'] -ceq 'Qualify the consumer with STL annotations disabled' })
            Require ($variant.Count -eq 1 -and $variant[0]['conclusion'] -ceq 'success') 'Annotations-disabled consumer step did not succeed.'
            Require ($fingerprints.Count -eq 2 -and $fingerprints[0] -cne $fingerprints[1]) 'ASan annotation variants are not independently completed and fingerprinted.'
        }
        Require ([regex]::Matches($jobText, 'PASS relocated exact-pin consumer, rendering and 10 rejected pin/build mismatches').Count -eq $annotations.Count) 'Full job log does not establish every expected consumer completion.'
        Require ([regex]::Matches($jobText, 'Native module fixture: 0 failed probes across EXE \+ two DLLs').Count -eq $annotations.Count) 'Full job log does not establish each consumer module fixture completion.'
    }
    if ($configuration -ceq 'ASan Debug') {
        Review-Check "$profileName/ASan-detection" {
            $record = Receipt $artifact 'AddressSanitizer' $platform $configuration
            $value = $record['value']; Assert-CompletedInJob $value $job
            $exit = Read-ExactExitCode $value
            Require ($value['detected'] -is [bool] -and $value['detected'] -eq $true -and $exit -ne 0) 'Deliberate ASan negative probe was not detected.'
            $probeLog = Find-File $artifact "test-AddressSanitizer-$platform.log"
            Require ([IO.File]::ReadAllText($probeLog).Contains('AddressSanitizer: heap-use-after-free')) 'ASan probe log lacks the intentional diagnosis.'
            $foundation = Receipt $artifact 'Foundation' $platform $configuration
            Require ($value['sha256'] -ceq $foundation['value']['sha256']) 'ASan probe and ordinary Foundation suite did not use the same executable bytes.'
            $probeEvidence.Add([ordered]@{ profile = $profileName; exitCode = $value['exitCode']; detected = $value['detected']; receipt = $record['file']; executableSha256 = $value['sha256']; log = $probeLog; expectedNegative = $true })
        }
    }
    Review-Check "$profileName/job-outcome" {
        Require ($job['status'] -ceq 'completed' -and $job['conclusion'] -cin @('success', 'failure')) 'Native job cancelled, timed out, skipped or otherwise incomplete.'
        $failedSteps = @($job['steps'] | Where-Object { $_['conclusion'] -cin @('failure','cancelled','timed_out','action_required') })
        if ($job['conclusion'] -ceq 'failure') {
            Require ($profileRefusals.Count -gt 0 -and $failedSteps.Count -eq 1 -and $failedSteps[0]['name'].Contains('./test.ps1 -Full -SkipTooling')) 'Native job has a failure beyond the candidate ARM64 foreground refusal.'
        } else { Require ($failedSteps.Count -eq 0 -and $profileRefusals.Count -eq 0) 'A green native job hides a failed step or refusal.' }
        if ($platform -ceq 'x64' -and $configuration -ceq 'Release') {
            $resource = @($job['steps'] | Where-Object { $_['name'] -ceq 'Characterize menu resource scaling without capture' })
            Require ($resource.Count -eq 1 -and $resource[0]['conclusion'] -ceq 'success') 'Required x64 Release menu resource characterization did not complete.'
        }
    }
    if ($job['conclusion'] -ceq 'success') {
        Review-Check "$profileName/fresh-gallery-images" {
            $galleryStep = @($job['steps'] | Where-Object { $_['name'].Contains('gallery.ps1') })
            Require ($galleryStep.Count -eq 1 -and $galleryStep[0]['conclusion'] -ceq 'success') 'Gallery generator did not succeed on this native profile.'
            $galleryFiles = @(Get-ChildItem -LiteralPath $artifact -Filter '*.png' -File -Recurse | Where-Object { $_.FullName -cmatch '[/\\]gallery[/\\]' })
            $expectedImages = @('embedded-controls.png','theme-controls-dark.png','theme-controls-high-contrast.png','theme-controls-light.png','theme-controls-rainbow-dark.png','theme-controls-rainbow-light.png')
            Require ($galleryFiles.Count -eq 6) 'Fresh native gallery must contain the five theme sheets and embedded example.'
            foreach ($name in $expectedImages) {
                $image = @($galleryFiles | Where-Object { $_.Name -ceq $name })
                Require ($image.Count -eq 1) "Missing or duplicate fresh gallery image: $name"
                $bytes = [IO.File]::ReadAllBytes($image[0].FullName)
                Require ($bytes.Length -gt 24 -and ([BitConverter]::ToString($bytes[0..7])) -ceq '89-50-4E-47-0D-0A-1A-0A') 'Gallery image lacks a valid PNG signature/header.'
                $width = ([uint64]$bytes[16] * 16777216) + ([uint64]$bytes[17] * 65536) + ([uint64]$bytes[18] * 256) + $bytes[19]
                $height = ([uint64]$bytes[20] * 16777216) + ([uint64]$bytes[21] * 65536) + ([uint64]$bytes[22] * 256) + $bytes[23]
                Require ($width -gt 0 -and $height -gt 0) 'Gallery image has empty dimensions.'
                $galleryEvidence.Add([ordered]@{ profile = $profileName; file = $image[0].FullName; sha256 = (Get-FileHash -LiteralPath $image[0].FullName).Hash; width = $width; height = $height; sourceCommit = $collection['testedMerge']; humanVisualReviewRequired = $true })
            }
        }
    }
}

Review-Check 'paired-replay-and-budget-proof' {
    $pairedRoot = Artifact-Root 'paired-benchmark-x64-Release'
    $summaryPath = Find-File $pairedRoot 'summary.json'
    $reports = [IO.Path]::GetDirectoryName($summaryPath)
    $summary = Read-GateJson $summaryPath
    $pairJob = @($jobs | Where-Object { $_['name'] -clike 'paired-benchmark*' })
    Require ($pairJob.Count -eq 1 -and $pairJob[0]['status'] -ceq 'completed' -and $pairJob[0]['conclusion'] -cin @('success','failure')) 'Paired job did not complete.'
    $pairJob = $pairJob[0]
    Require ($summary['baseline']['commit'] -ceq $collection['expectedBase'] -and $summary['candidate']['commit'] -ceq $collection['testedMerge'] -and
        $summary['harnessCommit'] -ceq $collection['testedMerge'] -and $summary['harnessDirty'] -eq $false) 'Paired baseline/candidate/harness commits differ from the exact PR base/merge.'
    Require ($summary['studyVersion'] -eq 3 -and $summary['studyPurpose'] -ceq 'regression-qualification' -and
        $summary['blocks'] -eq 12 -and $summary['platform'] -ceq 'x64' -and $summary['configuration'] -ceq 'Release') 'Paired study is not the ordinary twelve-block PR qualification protocol.'
    $expectedScenarios = @('Default','MultilineGrid','MultilineGridDistinct')
    $actualScenarios = @($summary['scenarios'] | ForEach-Object { $_['scenario'] } | Sort-Object) -join ','
    $requiredScenarios = @($expectedScenarios | Sort-Object) -join ','
    Require (@($summary['scenarios']).Count -eq 3 -and $actualScenarios -ceq $requiredScenarios) 'Paired scenarios are missing, repeated or outside the required set.'
    $expectedHarness = @(Get-PairedHarness) + @('build.ps1','Tools/ScopedTesting.psm1','Tools/Validation.psm1','Tools/Changelog.psm1')
    Require ($summary['harness'].Count -eq $expectedHarness.Count) 'Paired harness inventory differs from the measured current-source contract.'
    foreach ($path in $expectedHarness) {
        Require ($summary['harness'].Contains($path)) "Missing source-bound harness input: $path"
        Require ((Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $path)).Hash -ceq $summary['harness'][$path]) "Frozen candidate bytes do not reproduce the Windows-checkout harness hash: $path"
    }
    $basePolicy = Get-Content -Raw -LiteralPath (Join-Path $CollectionRoot 'source/base-policy.receipt.txt') | ConvertFrom-Json -AsHashtable
    # qualified=false remains HOLD. Its reviewed hash/version targets are used only to reproduce diagnostic decisions.
    $dual = Test-DualJudgeReceiptEvidence -Summary $summary -ReportsDirectory $reports -ApprovedJudgeSha256 $basePolicy['approvedJudgeSha256'] -ApprovedJudgeVersion $basePolicy['judgeVersion']
    Require $dual['valid'] "Raw receipt, seeded schedule, dual-judge or complete control replay failed: $($dual['reason'])"
    $receiptCount = 0; $roundCount = 0; $decisionCount = 0; $controlCount = 0; $exact = [Collections.Generic.List[object]]::new()
    $sideStates = @{}
    $overlayPaths = $expectedHarness + @('Tests/Embedded/BenchmarkMain.h','Tests/Embedded/ComplexUiBenchmark.h','Tests/Support/HeapDiagnostic.h')
    foreach ($scenario in $summary['scenarios']) {
        $controls = $dual['controlsByScenario'][$scenario['scenario']]
        Require ($controls['Total'] -eq 24 -and $controls['Missing'] -eq 0) 'A scenario does not have all twenty-four replayed same-binary controls.'
        $controlCount += [int]$controls['Total']
        Require (@($scenario['set']['metrics']).Count -eq 26) 'A scenario does not have all twenty-six predeclared metric decisions.'
        $decisionCount += @($scenario['set']['metrics']).Count
        $runNames = @($summary['steps'] | ForEach-Object { [string]$_['Name'] } | Sort-Object) -join ','
        Require ($scenario['runs'] -is [System.Collections.IDictionary] -and $scenario['runs'].Count -eq 48 -and (@($scenario['runs'].Keys | Sort-Object) -join ',') -ceq $runNames) 'Retained paired run identity inventory differs from the exact forty-eight scheduled reports.'
        foreach ($hashRecord in $scenario['judgeComparison']['sameReceiptsSha256']) {
            $path = Join-Path $reports "$($scenario['scenario'])-$($hashRecord['run']).json"
            $value = Read-PerformanceReceipt $path
            $receiptCount++
            Assert-CompletedInJob $value $pairJob
            $attestationBinding = Bind-UploadedBuildAttestation $pairedRoot $value $pairJob
            $attestationBinding['performanceReport'] = $path
            $attestationEvidence.Add($attestationBinding)
            Require ($value['sourceCommit'] -cin @($collection['expectedBase'], $collection['testedMerge'])) 'Raw paired report comes from another source commit.'
            Bind-PairedRoleReceipt $summary $scenario ([string]$hashRecord['run']) $value $sideStates $overlayPaths
            Assert-PairedRawBudgetAndFixture $value ([string]$scenario['scenario'])
            Require ($value['hiddenPreparations'] -eq 0 -and $value['hiddenComposites'] -eq 0) 'Raw paired report violates hidden-work budgets.'
            foreach ($phase in $value['scenarios']) {
                $roundCount += @($phase['rounds']).Count
                foreach ($round in $phase['rounds']) { Require ($round['composeAllocations'] -eq 0) 'Raw composition allocation budget is not zero.' }
            }
        }
        foreach ($metric in $scenario['set']['metrics']) {
            if ($metric['exact']) {
                $exact.Add([ordered]@{ scenario = $scenario['scenario']; phase = $metric['phase']; metric = $metric['metric']; baselineMedian = $metric['baselineMedian']; candidateMedian = $metric['candidateMedian']; baselineMaximum = $metric['baselineMaximum']; candidateMaximum = $metric['candidateMaximum']; medianBudgetVerdict = $metric['medianBudgetVerdict']; peakBudgetVerdict = $metric['peakBudgetVerdict']; verdict = $metric['verdict'] })
                Require ($metric['verdict'] -cne 'regressed') "Deterministic resource regression: $($scenario['scenario'])/$($metric['phase'])/$($metric['metric'])"
            }
        }
    }
    Require ($receiptCount -eq 144 -and $roundCount -eq 1440 -and $decisionCount -eq 78 -and $controlCount -eq 72 -and $exact.Count -eq 24) 'Paired reports, raw rounds, decisions, controls or exact-budget inventory are incomplete.'
    Require ($sideStates.Count -eq 2 -and $sideStates.Contains('baseline') -and $sideStates.Contains('candidate')) 'Raw paired identities do not retain both scheduled roles.'
    $measuredLibraryChanged = $sideStates['baseline']['sourceFingerprint'] -cne $sideStates['candidate']['sourceFingerprint']
    $diagnostic = Get-BenchmarkConclusion -Summary $summary -ReportsDirectory $reports
    $strictDiagnostic = Get-BenchmarkConclusion -Summary $summary -ReportsDirectory $reports -StrictControls
    # An untrusted policy overwrites the aggregate with inconclusive after scenario judgment. Inspect every
    # replayed scenario/metric first: HOLD cannot mask a confirmed degradation or invalid evidence.
    Require (@($diagnostic['Scenarios']).Count -eq 3) 'Diagnostic scenario inventory is incomplete.'
    foreach ($scenario in $diagnostic['Scenarios']) {
        Require ($scenario['Conclusion'] -cin @('pass','inconclusive')) "Non-exempt diagnostic finding in $($scenario['Scenario']): $($scenario['Conclusion'])."
        Require (@($scenario['Metrics']).Count -eq 26) 'Diagnostic metric inventory is incomplete or invalid.'
        $controls = $dual['controlsByScenario'][$scenario['Scenario']]
        foreach ($metric in $scenario['Metrics']) {
            $key = "$($metric['Phase'])/$($metric['Metric'])"
            $seen = $controls['Metrics'][$key]
            Require ($null -ne $seen -and $seen['Observed'] -eq $controls['Total']) "Replayed control observations are incomplete for $key."
            $expectedHeld = -not [bool]$seen['Drifted']
            Require ($metric['Held'] -eq $expectedHeld -and $metric['ControlDrifted'] -eq [bool]$seen['Drifted']) 'Diagnostic stability does not match the canonical raw-control replay.'
            Require (-not ($measuredLibraryChanged -and $metric['Verdict'] -ceq 'regressed' -and $expectedHeld)) "Confirmed measured degradation from changed library inputs in $($scenario['Scenario'])/$key cannot be covered by a projected or policy HOLD."
            Require ($metric['Outcome'] -cne 'confirmed') "Confirmed measured degradation in $($scenario['Scenario'])/$key cannot be covered by the policy HOLD."
            if ($metric['Verdict'] -ceq 'regressed') { $flaggedMetrics.Add([ordered]@{ scenario=$scenario['Scenario']; metric=$metric; replayedControls=$seen; classification='Unconfirmed measurement flag remains HOLD evidence requiring human investigation; not dismissed as noise.' }) }
        }
    }
    Require ($diagnostic['Conclusion'] -cin @('pass','inconclusive') -and $strictDiagnostic['Conclusion'] -cin @('pass','inconclusive')) 'Diagnostic aggregate is invalid or degraded; policy/strict-control inconclusive findings remain HOLD.'
    $gate = Get-BenchmarkConclusion -Summary $summary -ReportsDirectory $reports -RequireQualifiedPolicy
    $recorded = Read-GateJson (Find-File $reports 'verdict.json')
    Require ($recorded['Conclusion'] -ceq $gate['Conclusion'] -and $recorded['PolicyStatus'] -ceq $gate['PolicyStatus']) 'Recorded gate verdict differs from exact-source replay.'
    $failed = @($pairJob['steps'] | Where-Object { $_['conclusion'] -cin @('failure','cancelled','timed_out','action_required') })
    if ($gate['Conclusion'] -ceq 'inconclusive' -and $gate['PolicyStatus'] -ceq 'policy-review-required') {
        Require ($basePolicy['qualified'] -eq $false -and $pairJob['conclusion'] -ceq 'failure' -and $failed.Count -eq 1 -and
            $failed[0]['name'].Contains('Publish-BenchmarkVerdict.ps1')) 'Paired failure is beyond the existing unqualified-policy verdict.'
    } else { Require ($gate['Conclusion'] -ceq 'pass' -and $pairJob['conclusion'] -ceq 'success' -and $failed.Count -eq 0) 'Fresh performance failure is not a dated policy HOLD.' }
    $script:pairedEvidence = [ordered]@{ summary = $summaryPath; rawReports = $receiptCount; rawRounds = $roundCount; metricDecisions = $decisionCount; sameBinaryControls = $controlCount; harnessFiles = $expectedHarness.Count; dualJudgeReplay = $dual; scheduledRoleIdentities=$sideStates; measuredLibraryChanged=$measuredLibraryChanged; absoluteCppBudgets=[ordered]@{cleanPerRound=0;dirtyPerFrame=64;dirtyPerRound=2560}; deterministicBudgets = $exact.ToArray(); diagnosticVerdict = $diagnostic; strictDiagnosticVerdict=$strictDiagnostic; gateVerdict = $gate; policyQualified = $basePolicy['qualified']; policyHashIsReviewTargetNotApproval = ($basePolicy['qualified'] -eq $false) }
}

Review-Check 'coverage-and-aggregate-accounting' {
    Require ($nativeEvidence.Count -eq 128 -and $consumerEvidence.Count -eq 8 -and $probeEvidence.Count -eq 2 -and $null -ne $pairedEvidence) 'Native/consumer/probe/paired accounting is incomplete.'
    Require (@($galleryEvidence | Where-Object { $_['profile'] -ceq 'x64/Release' }).Count -eq 6) 'Fresh x64 Release gallery images are missing.'
    $aggregate = @($jobs | Where-Object { $_['name'] -ceq 'ci-gate' })
    Require ($aggregate.Count -eq 1 -and $aggregate[0]['status'] -ceq 'completed' -and $aggregate[0]['conclusion'] -cin @('success','failure')) 'Aggregate job did not finish.'
    $holds = $armRefusalCandidates.Count -gt 0 -or $pairedEvidence['gateVerdict']['Conclusion'] -cne 'pass'
    Require ($aggregate[0]['conclusion'] -ceq $(if ($holds) { 'failure' } else { 'success' })) 'Actual aggregate outcome does not match the reviewed producer outcomes.'
    Require ($collection['runConclusion'] -ceq $(if ($holds) { 'failure' } else { 'success' })) 'Workflow run outcome does not match the complete producer accounting.'
}
Review-Check 'post-review-source-and-receipt-immutability' { Assert-Freeze; Assert-Seal }
$report = [ordered]@{
    version = 1; runId = $collection['runId']; attempt = $collection['attempt']; pullRequest = $collection['pullRequest']
    head = $collection['expectedHead']; base = $collection['expectedBase']; testedMerge = $collection['testedMerge']; testedTree = $collection['testedTree']; workflowDigest = $collection['workflowDigest']
    status = $(if ($blockers.Count) { 'BLOCKED' } else { 'REVIEW_READY_NOT_CI_GATE_SUCCESS' }); actualRunConclusion = $collection['runConclusion']
    blockers = $blockers.ToArray(); nativeJobBudgets=$nativeJobBudgets.ToArray(); native = $nativeEvidence.ToArray(); consumers = $consumerEvidence.ToArray(); asanNegativeProbes = $probeEvidence.ToArray(); paired = $pairedEvidence; uploadedAttestationBindings=$attestationEvidence.ToArray(); freshGalleryImagesRequiringHumanVisualReview = $galleryEvidence.ToArray()
    armForegroundExceptionCandidatesRequiringHumanReview = $armRefusalCandidates.ToArray(); unresolvedFlaggedMetricsRequiringHumanReview = $flaggedMetrics.ToArray()
    remainingQualification = @('Unqualified performance-policy HOLD remains a failed/incomplete gate.', 'Native ARM64 foreground runtime remains incomplete where a verified safe refusal occurred.', 'Local x64 Debug/Release/ASan four-suite foreground receipts and their exact-source/artifact/restoration proof remain separate required evidence.', 'Physical device/input and actual consumer adoption remain in the separate WIP.', 'Main-push/release/adoption acceptance is separate from implementation-only PR integration.')
    limitations = $collection['limitations']; reviewedUtc = [DateTime]::UtcNow.ToString('o'); driverSha256 = (Get-FileHash -LiteralPath $PSCommandPath).Hash
}
$output = Join-Path $CollectionRoot ("review-$([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))-$([guid]::NewGuid().ToString('N')).receipt.txt")
[IO.File]::WriteAllText($output, (($report | ConvertTo-Json -Depth 60) + "`n"), $utf8)
Write-Host "$($report.status): $output"
Write-Host "Native $($nativeEvidence.Count)/128; consumers $($consumerEvidence.Count)/8; ASan probes $($probeEvidence.Count)/2; blockers $($blockers.Count)."
if ($blockers.Count) { foreach ($entry in $blockers) { Write-Host "BLOCKER $($entry['check']): $($entry['reason'])" }; exit 1 }
Write-Host 'Human review must confirm each ARM before-input refusal and every unresolved metric before ordinary implementation-only integration; this script never marks failed CI green.'
exit 0
