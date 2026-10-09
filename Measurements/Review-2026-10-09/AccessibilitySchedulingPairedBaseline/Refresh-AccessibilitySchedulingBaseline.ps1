<#
.SYNOPSIS
Refreshes the isolated accessibility-scheduling baseline overlay and writes a versioned build-only setup receipt.
.DESCRIPTION
This helper is intended for an explicitly scheduled source-refresh/build phase. It copies compiled source inputs from the
candidate into the named baseline checkout, retaining only the measured pre-A1 Accessibility.cpp and the reviewed
baseline-only test adapter/project. It performs the canonical x64 Debug build in the baseline checkout, writes a
build-only scoped receipt, then emits a new setup manifest. It never runs a test suite or edits candidate source.
#>
[CmdletBinding()]
param(
    [string] $CandidateRoot = '',
    [string] $BaselineRoot = '',
    [string] $OriginalSetupManifest = '',
    [string] $VersionedSetupManifest = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$studyFolder = [IO.Path]::GetFullPath($PSScriptRoot)
if (-not $OriginalSetupManifest) { $OriginalSetupManifest = Join-Path $studyFolder 'source-input-manifest.json' }
$OriginalSetupManifest = [IO.Path]::GetFullPath($OriginalSetupManifest)
if (-not (Test-Path -LiteralPath $OriginalSetupManifest -PathType Leaf)) { throw "Original setup receipt is missing: $OriginalSetupManifest" }
$originalHash = (Get-FileHash -LiteralPath $OriginalSetupManifest -Algorithm SHA256).Hash
$original = Get-Content -LiteralPath $OriginalSetupManifest -Raw | ConvertFrom-Json -AsHashtable
if ($original.workloadOwner -cne 'DxUi' -or $original.fixture -cne 'dxui-accessibility-publication-scheduling-v1') {
    throw 'The original receipt is not the expected DxUi accessibility scheduling setup.'
}
if (-not $CandidateRoot) { $CandidateRoot = [string]$original.candidateRoot }
if (-not $BaselineRoot) { $BaselineRoot = [string]$original.baselineRoot }
$CandidateRoot = [IO.Path]::GetFullPath($CandidateRoot)
$BaselineRoot = [IO.Path]::GetFullPath($BaselineRoot)
if (-not [string]::Equals($CandidateRoot, [IO.Path]::GetFullPath($original.candidateRoot), [StringComparison]::OrdinalIgnoreCase)) {
    throw "Candidate root differs from the preserved setup receipt: $CandidateRoot"
}
if (-not [string]::Equals($BaselineRoot, [IO.Path]::GetFullPath($original.baselineRoot), [StringComparison]::OrdinalIgnoreCase)) {
    throw "Baseline root differs from the preserved setup receipt: $BaselineRoot"
}
if (-not $VersionedSetupManifest) { $VersionedSetupManifest = Join-Path $studyFolder 'source-input-manifest.refresh-2026-10-09-v2.json' }
$VersionedSetupManifest = [IO.Path]::GetFullPath($VersionedSetupManifest)
if (-not $VersionedSetupManifest.StartsWith($studyFolder + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Versioned setup manifest must be written inside the owned AccessibilitySchedulingPairedBaseline folder.'
}
if (Test-Path -LiteralPath $VersionedSetupManifest) { throw "Refusing to replace an existing versioned setup receipt: $VersionedSetupManifest" }

$baselineHead = @(& git -C $BaselineRoot rev-parse HEAD 2>$null)
$candidateHead = @(& git -C $CandidateRoot rev-parse HEAD 2>$null)
if ($LASTEXITCODE -ne 0 -or $baselineHead.Count -ne 1 -or $candidateHead.Count -ne 1) { throw 'Cannot establish candidate/baseline Git HEAD identities.' }
if ($baselineHead[0].Trim() -cne [string]$original.baselineHead -or $candidateHead[0].Trim() -cne [string]$original.candidateHead) {
    throw 'Candidate or baseline HEAD moved from the preserved setup receipt.'
}

$candidateModulePaths = @(
    'Tools/ScopedTesting.psm1', 'Tools/PerformancePolicy.psm1', 'Tools/VisualStudio.psm1'
)
foreach ($module in $candidateModulePaths) { Import-Module ([IO.Path]::GetFullPath((Join-Path $CandidateRoot $module))) -Force }

function Get-Sha256Text([string] $Text) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { [Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text))) }
    finally { $sha.Dispose() }
}

function Get-FileSha256([string] $Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Required input is missing: $Path" }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

function Get-IdentitySha256([System.Collections.IDictionary] $Identity) {
    return Get-Sha256Text (ConvertTo-Json -InputObject $Identity -Depth 20 -Compress)
}

function Get-CompiledClosure([string] $Root) {
    $paths = @(& git -c core.quotepath=false -C $Root ls-files --cached --others --exclude-standard 2>$null)
    if ($LASTEXITCODE -ne 0) { throw "Cannot enumerate compiled source closure at $Root." }
    $closure = [ordered]@{}
    foreach ($path in ($paths | Where-Object { $_ } | Sort-Object -Unique)) {
        if ($path -match '^\.build/' -or $path -match '^(Measurements|docs|Changes|legacy|Specs/(Plans|Done|TestRuns|Reviews|Mockups))/' -or $path -match '\.md$') { continue }
        $file = Join-Path $Root $path
        if (Test-Path -LiteralPath $file -PathType Leaf) { $closure[$path] = Get-FileSha256 $file }
    }
    return $closure
}

function Assert-CandidateBuildReceipt([string] $Root) {
    $buildInput = Get-ScopedBuildInputIdentity -Root $Root -Platform x64
    $artifact = Get-ScopedArtifactIdentity -Root $Root -Platform x64 -Configuration Debug
    $path = Join-Path $Root '.build/reports/scoped-tests/x64-Debug/build.json'
    $identity = Get-ScopedDigest ($buildInput + "`n" + $artifact)
    if (-not (Test-ScopedReceipt -Path $path -Identity $identity)) {
        throw 'Candidate requires a valid scoped x64 Debug build receipt for its current compiled-source, installed-dependency, and artifact identities.'
    }
    return [ordered]@{ buildInputIdentity=$buildInput; artifactIdentity=$artifact; attestationIdentity=$identity; receiptPath=$path; receiptSha256=(Get-FileSha256 $path) }
}

function Get-PerformanceIdentity([string] $Root, [string] $Role, [ValidateSet('Full','PlatformOnly')][string] $IdentityScope = 'Full') {
    $identity = Get-PerformanceToolIdentities -Root $Root -Platform x64 -Configuration Debug -IdentityScope $IdentityScope
    $expectedStatus = if ($IdentityScope -eq 'Full') { 'verifiable' } else { 'platform-only' }
    if ($identity.identityStatus -cne $expectedStatus) { throw "$Role $IdentityScope performance/toolchain identity is unavailable: $($identity.identityError)" }
    $identity['role'] = $Role
    return $identity
}

function Assert-MatchingToolIdentities([System.Collections.IDictionary] $Baseline, [System.Collections.IDictionary] $Candidate) {
    foreach ($field in @('harnessIdentity','toolchainIdentity','dependencyIdentity','environmentIdentity')) {
        if ([string]$Baseline[$field] -cne [string]$Candidate[$field]) { throw "Baseline and candidate differ in resolved $field." }
    }
}

function Assert-MatchingPlatformIdentities([System.Collections.IDictionary] $Baseline, [System.Collections.IDictionary] $Candidate) {
    foreach ($field in @('toolchainIdentity','dependencyIdentity','environmentIdentity')) {
        if ([string]$Baseline[$field] -cne [string]$Candidate[$field]) { throw "Baseline and candidate differ in pre-overlay $field." }
    }
}

function Assert-AllowedSourceDifferences([System.Collections.IDictionary] $Baseline, [System.Collections.IDictionary] $Candidate, [switch] $RequireEqual) {
    $allowed = @('src/Controls/DxUi.Accessibility.cpp','Tests/Controls/DxUi.ControlTests.vcxproj',
        'Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp')
    foreach ($path in @($Baseline.Keys | Where-Object { $_ -notin $allowed })) {
        if (-not $Candidate.Contains($path)) { throw "Baseline has a compiled input absent from candidate; helper will not delete it: $path" }
        if ($RequireEqual -and $Baseline[$path] -cne $Candidate[$path]) { throw "Compiled input still differs after refresh: $path" }
    }
    foreach ($path in @($Candidate.Keys | Where-Object { $_ -notin $allowed })) {
        if ($RequireEqual -and -not $Baseline.Contains($path)) { throw "Candidate compiled input is absent from baseline after refresh: $path" }
    }
    if (-not $Baseline.Contains('src/Controls/DxUi.Accessibility.cpp') -or -not $Candidate.Contains('src/Controls/DxUi.Accessibility.cpp') -or
        -not $Baseline.Contains('Tests/Controls/DxUi.ControlTests.vcxproj') -or -not $Candidate.Contains('Tests/Controls/DxUi.ControlTests.vcxproj') -or
        -not $Baseline.Contains('Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp') -or
        $Candidate.Contains('Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp')) {
        throw 'The retained implementation, baseline project exception, or baseline-only adapter has an unexpected presence.'
    }
    return $allowed
}

function Get-ResolvedToolRecord([string] $Installation, [string] $Root) {
    $msbuild = Join-Path $Installation 'MSBuild/Current/Bin/MSBuild.exe'
    $propertyNames = 'VCToolsInstallDir,VCToolsVersion,WindowsSdkDir,WindowsTargetPlatformVersion,PreferredToolArchitecture'
    $output = & $msbuild (Join-Path $Root 'src/DxUi.vcxproj') /nologo '/p:Configuration=Debug' '/p:Platform=x64' "-getProperty:$propertyNames"
    if ($LASTEXITCODE -ne 0) { throw 'MSBuild could not resolve the x64 Debug toolchain properties.' }
    $properties = ($output -join "`n" | ConvertFrom-Json).Properties
    foreach ($name in $propertyNames.Split(',')) { if ([string]::IsNullOrWhiteSpace([string]$properties.$name)) { throw "MSBuild returned no $name." } }
    $hostTool = "Host$($properties.PreferredToolArchitecture)"
    $compiler = Join-Path $properties.VCToolsInstallDir "bin/$hostTool/x64/cl.exe"
    $linker = Join-Path $properties.VCToolsInstallDir "bin/$hostTool/x64/link.exe"
    $toolsetPath = Join-Path $Installation 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt'
    $paths = @($compiler,$linker,$msbuild,$toolsetPath)
    $items = foreach ($path in $paths) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Resolved tool file is missing: $path" }
        $version = (Get-Item -LiteralPath $path).VersionInfo.FileVersion
        [ordered]@{ path=$path; sha256=(Get-FileSha256 $path); version=$version }
    }
    return [ordered]@{ windowsSdkVersion=[string]$properties.WindowsTargetPlatformVersion; visualStudio=$Installation
        defaultMsvcToolset=(Get-Content -LiteralPath $toolsetPath -Raw).Trim()
        preferredToolArchitecture=[string]$properties.PreferredToolArchitecture; toolFiles=$items }
}

function Get-LatestBuildLog([string] $Root, [string[]] $NamesBefore) {
    $newLogs = @(Get-ChildItem -LiteralPath (Join-Path $Root '.build/logs') -Filter 'build-x64-Debug-*.log' -File |
        Where-Object { $_.Name -notin $NamesBefore } | Sort-Object LastWriteTimeUtc -Descending)
    if ($newLogs.Count -ne 1) { throw "Expected exactly one newly generated canonical baseline build log; found $($newLogs.Count)." }
    return $newLogs[0]
}

# The candidate must already have successful scoped build evidence; the helper never builds or edits it.
$candidateBuildPre = Assert-CandidateBuildReceipt $CandidateRoot
$baselineBefore = Get-CompiledClosure $BaselineRoot
$candidateBefore = Get-CompiledClosure $CandidateRoot
$allowedPaths = Assert-AllowedSourceDifferences $baselineBefore $candidateBefore
$expectedRecords = @{}
foreach ($record in $original.overlayInputs) { $expectedRecords[[string]$record.path] = $record }
$retainedRecord = $expectedRecords['src/Controls/DxUi.Accessibility.cpp']
$projectRecord = $expectedRecords['Tests/Controls/DxUi.ControlTests.vcxproj']
$adapterRecord = $expectedRecords['Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp']
if (-not $retainedRecord -or -not $projectRecord -or -not $adapterRecord) { throw 'Preserved receipt is missing a retained-source/project/adapter identity.' }
$retainedPath = Join-Path $CandidateRoot 'Measurements/Review-2026-10-09/AccessibilitySchedulingBefore/Accessibility.cpp.txt'
$retainedHash = Get-FileSha256 $retainedPath
if ($retainedHash -cne [string]$original.retainedImplementation.sha256) {
    throw 'The immutable retained pre-A1 Accessibility.cpp source does not match its original identity.'
}
$retainedCompatibilityText = [IO.File]::ReadAllText($retainedPath)
$retainedApiBridges = @(
    [ordered]@{ before='TakeMessagePayload<AccessibilityProviderCreationPayload>(lp, hwnd)'; after='TakeMessagePayload<AccessibilityProviderCreationPayload>(hwnd, msg, lp)' },
    [ordered]@{ before='TakeMessagePayload<AccessibilityUiActionPayload>(lp, hwnd)'; after='TakeMessagePayload<AccessibilityUiActionPayload>(hwnd, msg, lp)' }
)
foreach ($bridge in $retainedApiBridges) {
    if ([regex]::Matches($retainedCompatibilityText,[regex]::Escape([string]$bridge.before)).Count -ne 1) {
        throw "The retained source does not contain exactly one reviewed API bridge: $($bridge.before)"
    }
    $retainedCompatibilityText = $retainedCompatibilityText.Replace([string]$bridge.before,[string]$bridge.after)
}
$retainedCompatibilityHash = Get-Sha256Text $retainedCompatibilityText
$baselineRetainedPath = Join-Path $BaselineRoot 'src/Controls/DxUi.Accessibility.cpp'
if ((Get-FileSha256 $baselineRetainedPath) -cnotin @($retainedHash,$retainedCompatibilityHash)) {
    throw 'The baseline implementation differs from both the immutable original and its two reviewed payload-call bridges.'
}
if ($baselineBefore['Tests/Controls/DxUi.ControlTests.vcxproj'] -cne [string]$projectRecord.baselineSha256 -or
    $baselineBefore['Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp'] -cne [string]$adapterRecord.baselineSha256) {
    throw 'The existing baseline-only project or diagnostic adapter changed from its disclosed identity.'
}
if ($candidateBefore.Contains('Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp')) { throw 'Candidate unexpectedly contains the baseline-only adapter.' }

$baselinePerformanceBefore = Get-PerformanceIdentity $BaselineRoot 'baseline' -IdentityScope PlatformOnly
$candidatePerformanceBefore = Get-PerformanceIdentity $CandidateRoot 'candidate' -IdentityScope PlatformOnly
# Before overlay, only the host/toolchain and installed dependency environment must match. The harness source
# identity is expected to differ until the candidate's current shared compiled source closure has been copied.
Assert-MatchingPlatformIdentities $baselinePerformanceBefore $candidatePerformanceBefore
$visualStudio = [string](Get-DxUiVisualStudioInstallation)
$toolchainBefore = Get-ResolvedToolRecord -Installation $visualStudio -Root $CandidateRoot
foreach ($oldTool in $original.toolchain.toolFiles) {
    # Preserve the historical inventory, but do not relabel its Hostx64 files as the compiler used by MSBuild.
    if ((Get-FileSha256 ([string]$oldTool.path)) -cne [string]$oldTool.sha256) { throw "Inventoried tool changed from the preserved receipt: $($oldTool.path)" }
}
$originalCompileLogPath = Join-Path $BaselineRoot ([string]$original.build.compile.log)
$originalCompileLogHash = Get-FileSha256 $originalCompileLogPath
if ($originalCompileLogHash -cne [string]$original.build.compile.logSha256) { throw 'The preserved baseline compilation log changed.' }
$originalCompileLogText = [IO.File]::ReadAllText($originalCompileLogPath)
$resolvedCompilerAndLinker = @($toolchainBefore.toolFiles | Where-Object { [IO.Path]::GetFileName([string]$_.path) -in @('cl.exe','link.exe') })
if ($resolvedCompilerAndLinker.Count -ne 2) { throw 'The current tool record does not contain exactly one compiler and linker.' }
foreach ($tool in $resolvedCompilerAndLinker) {
    if ($originalCompileLogText.IndexOf([string]$tool.path,[StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw "Preserved MSBuild log does not establish the currently resolved tool path: $($tool.path)"
    }
}
$toolHostErratum = [ordered]@{
    originalManifestSha256=$originalHash
    originalReportedToolFiles=@($original.toolchain.toolFiles)
    preservedBuildLog=[string]$original.build.compile.log; preservedBuildLogSha256=$originalCompileLogHash
    actualToolHostInPreservedLog=[string]$toolchainBefore.preferredToolArchitecture
    observedCompilerAndLinkerPaths=@($resolvedCompilerAndLinker | ForEach-Object { [string]$_.path })
    explanation='The original inventory recorded Hostx64 compiler/linker hashes; its immutable MSBuild log shows Hostx86 tools were invoked. Historical Hostx86 binary hashes were not recorded and are not inferred. The refreshed baseline is fully rebuilt, and both current roots attest their actual resolved tools before and after that rebuild.'
}
$toolchainDefaultVersion = ([string]$toolchainBefore.defaultMsvcToolset)
if ($toolchainBefore.windowsSdkVersion -cne [string]$original.toolchain.windowsSdkVersion -or
    $toolchainDefaultVersion -cne [string]$original.toolchain.defaultMsvcToolset) {
    throw 'Resolved Windows SDK or default MSVC toolset differs from the preserved setup receipt.'
}
$candidateFixturePath = Join-Path $CandidateRoot 'Tests/Controls/DxUi.Tests.Accessibility.cpp'
$candidateFixtureHash = Get-FileSha256 $candidateFixturePath
$fixtureText = [IO.File]::ReadAllText($candidateFixturePath)
foreach ($required in @('TestAccessibilityPublicationMutationBatchRetainsFinalStateAndReportsCost','UIA_PUBLICATION_BENCHMARK',
        'first retained-provider query after the mutation batch','keystroke-batch','selection-batch')) {
    if (-not $fixtureText.Contains($required)) { throw "Candidate benchmark fixture lacks the expected scenario assertion or record: $required" }
}
if ($fixtureText -notmatch 'constexpr\s+size_t\s+iterations\s*=\s*128u') { throw 'Candidate benchmark fixture is not frozen at exactly 128 operations.' }

# Refresh only the compiled source closure. The retained library implementation, baseline test project, and adapter stay intact.
# The immutable archived implementation stays unchanged. Its compiled copy receives only two argument-order/message
# bridges for the hardened shared payload API; no scheduling, snapshot, provider or mutation logic is changed.
[IO.File]::WriteAllText($baselineRetainedPath,$retainedCompatibilityText,[Text.UTF8Encoding]::new($false))
foreach ($path in ($candidateBefore.Keys | Sort-Object -CaseSensitive)) {
    if ($path -in $allowedPaths) { continue }
    $source = Join-Path $CandidateRoot $path
    $destination = Join-Path $BaselineRoot $path
    if ((Test-Path -LiteralPath $destination -PathType Leaf) -and
        (Get-FileSha256 $destination) -ceq [string]$candidateBefore[$path]) { continue }
    $parent = Split-Path -Parent $destination
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) { [void](New-Item -ItemType Directory -Path $parent -Force) }
    Copy-Item -LiteralPath $source -Destination $destination -Force
    # A copied source can carry an older timestamp than an existing object file.
    # Changed bytes must trigger the canonical incremental build.
    (Get-Item -LiteralPath $destination).LastWriteTimeUtc = [DateTime]::UtcNow
}
$baselineAfterOverlay = Get-CompiledClosure $BaselineRoot
$candidateAfterOverlay = Get-CompiledClosure $CandidateRoot
$null = Assert-AllowedSourceDifferences $baselineAfterOverlay $candidateAfterOverlay -RequireEqual
if ((Get-FileSha256 $baselineRetainedPath) -cne $retainedCompatibilityHash) { throw 'Overlay changed the reviewed retained-source API bridges.' }
if ((Get-FileSha256 (Join-Path $BaselineRoot 'Tests/Controls/DxUi.Tests.Accessibility.cpp')) -cne $candidateFixtureHash) { throw 'Refreshed baseline fixture differs from candidate.' }
if ((Get-FileSha256 (Join-Path $BaselineRoot 'Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp')) -cne [string]$adapterRecord.baselineSha256) {
    throw 'Overlay changed the baseline-only diagnostic adapters.'
}
$baselineBuildInputBeforeBuild = Get-ScopedBuildInputIdentity -Root $BaselineRoot -Platform x64
$baselineArtifactBeforeBuild = Get-ScopedArtifactIdentity -Root $BaselineRoot -Platform x64 -Configuration Debug
$candidateBuildInputBeforeBuild = Get-ScopedBuildInputIdentity -Root $CandidateRoot -Platform x64
$candidateArtifactBeforeBuild = Get-ScopedArtifactIdentity -Root $CandidateRoot -Platform x64 -Configuration Debug
if ($candidateBuildInputBeforeBuild -cne $candidateBuildPre.buildInputIdentity -or $candidateArtifactBeforeBuild -cne $candidateBuildPre.artifactIdentity) {
    throw 'Candidate compiled source, installed dependencies, or artifacts changed after validating its scoped build receipt.'
}
$baselinePerformancePreBuild = Get-PerformanceIdentity $BaselineRoot 'baseline'
$candidatePerformancePreBuild = Get-PerformanceIdentity $CandidateRoot 'candidate'
Assert-MatchingToolIdentities $baselinePerformancePreBuild $candidatePerformancePreBuild
if ((Get-FileSha256 $OriginalSetupManifest) -cne $originalHash) { throw 'Preserved original setup receipt changed during overlay refresh.' }

$buildLogDirectory = Join-Path $BaselineRoot '.build/logs'
$oldBuildLogNames = @(Get-ChildItem -LiteralPath $buildLogDirectory -Filter 'build-x64-Debug-*.log' -File -ErrorAction SilentlyContinue | ForEach-Object Name)
& (Join-Path $BaselineRoot 'build.ps1') -Platform x64 -Configuration Debug -Rebuild
if ($LASTEXITCODE -ne 0) { throw "Canonical baseline x64 Debug build failed with exit code $LASTEXITCODE." }
$baselineBuildLog = Get-LatestBuildLog -Root $BaselineRoot -NamesBefore $oldBuildLogNames
$baselineBuildLogHash = Get-FileSha256 $baselineBuildLog.FullName
$baselineBuildInputAfterBuild = Get-ScopedBuildInputIdentity -Root $BaselineRoot -Platform x64
if ($baselineBuildInputAfterBuild -cne $baselineBuildInputBeforeBuild) { throw 'Baseline compiled source or installed dependencies changed during canonical build.' }
$baselineArtifactAfterBuild = Get-ScopedArtifactIdentity -Root $BaselineRoot -Platform x64 -Configuration Debug
$candidateBuildInputAfterBuild = Get-ScopedBuildInputIdentity -Root $CandidateRoot -Platform x64
$candidateArtifactAfterBuild = Get-ScopedArtifactIdentity -Root $CandidateRoot -Platform x64 -Configuration Debug
if ($candidateBuildInputAfterBuild -cne $candidateBuildInputBeforeBuild -or $candidateArtifactAfterBuild -cne $candidateArtifactBeforeBuild) {
    throw 'Candidate compiled source, installed dependencies, or artifacts changed while the baseline was built.'
}
$baselinePerformancePostBuild = Get-PerformanceIdentity $BaselineRoot 'baseline'
$candidatePerformancePostBuild = Get-PerformanceIdentity $CandidateRoot 'candidate'
Assert-MatchingToolIdentities $baselinePerformancePostBuild $candidatePerformancePostBuild
if ((Get-IdentitySha256 $baselinePerformancePreBuild) -cne (Get-IdentitySha256 $baselinePerformancePostBuild) -or
    (Get-IdentitySha256 $candidatePerformancePreBuild) -cne (Get-IdentitySha256 $candidatePerformancePostBuild)) {
    throw 'Resolved performance/toolchain/dependency identities changed across the baseline build.'
}
if ((Get-FileSha256 $OriginalSetupManifest) -cne $originalHash) { throw 'Preserved original setup receipt changed during baseline build.' }

$baselineBuildReceiptPath = Join-Path $BaselineRoot '.build/reports/scoped-tests/x64-Debug/build.json'
$baselineBuildReceiptIdentity = Get-ScopedDigest ($baselineBuildInputAfterBuild + "`n" + $baselineArtifactAfterBuild)
Write-ScopedReceipt -Path $baselineBuildReceiptPath -Identity $baselineBuildReceiptIdentity
if (-not (Test-ScopedReceipt -Path $baselineBuildReceiptPath -Identity $baselineBuildReceiptIdentity)) { throw 'Baseline build-only scoped receipt did not validate.' }
$candidateBuildPost = Assert-CandidateBuildReceipt $CandidateRoot
if ($candidateBuildPost.attestationIdentity -cne $candidateBuildPre.attestationIdentity -or $candidateBuildPost.receiptSha256 -cne $candidateBuildPre.receiptSha256) {
    throw 'Candidate build receipt changed during baseline refresh.'
}

$adapterPath = Join-Path $BaselineRoot 'Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp'
$projectPath = Join-Path $BaselineRoot 'Tests/Controls/DxUi.ControlTests.vcxproj'
$candidateProjectPath = Join-Path $CandidateRoot 'Tests/Controls/DxUi.ControlTests.vcxproj'
$toolchain = Get-ResolvedToolRecord -Installation $visualStudio -Root $CandidateRoot
foreach ($tool in $toolchain.toolFiles) {
    if ((Get-FileSha256 ([string]$tool.path)) -cne [string]$tool.sha256) { throw "Resolved tool changed while composing setup: $($tool.path)" }
}
if ((Get-Sha256Text (ConvertTo-Json -InputObject $toolchainBefore -Depth 8 -Compress)) -cne
    (Get-Sha256Text (ConvertTo-Json -InputObject $toolchain -Depth 8 -Compress))) {
    throw 'Resolved MSBuild properties or compiler/linker inputs changed across the baseline build.'
}
$baselineStatusPath = Join-Path $BaselineRoot '.build/vcpkg_installed/x64/vcpkg/status'
$candidateStatusPath = Join-Path $CandidateRoot '.build/vcpkg_installed/x64/vcpkg/status'
$baselineStatusHash = Get-FileSha256 $baselineStatusPath
$candidateStatusHash = Get-FileSha256 $candidateStatusPath
if ($baselineStatusHash -cne $candidateStatusHash) { throw 'Baseline and candidate restored dependency status files differ.' }
$originalRestoreLog = Join-Path $BaselineRoot ([string]$original.build.restore.log)
$originalRestoreLogHash = Get-FileSha256 $originalRestoreLog
if ($originalRestoreLogHash -cne [string]$original.build.restore.logSha256) { throw 'Previously recorded baseline restore log no longer matches the preserved setup receipt.' }
$candidateBuildLog = Get-ChildItem -LiteralPath (Join-Path $CandidateRoot '.build/logs') -Filter 'build-x64-Debug-*.log' -File |
    Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if (-not $candidateBuildLog) { throw 'Candidate x64 Debug build log is unavailable.' }

$sourceInputs = [ordered]@{}
foreach ($path in $candidateAfterOverlay.Keys) { $sourceInputs[$path] = [string]$candidateAfterOverlay[$path] }
$sourceInputs['vcpkg-installed-status'] = $baselineStatusHash
foreach ($tool in $toolchain.toolFiles) { $sourceInputs[[string]$tool.path] = [string]$tool.sha256 }
$overlayInputs = [Collections.Generic.List[object]]::new()
foreach ($path in $candidateAfterOverlay.Keys) {
    $record = [ordered]@{ path=$path; origin='refreshed shared compiled source input'; candidateSha256=[string]$candidateAfterOverlay[$path]; baselineSha256=[string]$baselineAfterOverlay[$path] }
    if ($path -eq 'src/Controls/DxUi.Accessibility.cpp') {
        $record.origin='retained pre-A1 baseline implementation with two disclosed posted-payload API bridges'; $record.candidateCurrentSha256=[string]$candidateAfterOverlay[$path]
    } elseif ($path -eq 'Tests/Controls/DxUi.ControlTests.vcxproj') {
        $record.origin='baseline-only adapter registration'; $record.candidateSha256=Get-FileSha256 $candidateProjectPath
        $record.baselineSha256=Get-FileSha256 $projectPath
    }
    $overlayInputs.Add($record)
}
$overlayInputs.Add([ordered]@{ path='Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp'; origin='baseline-only diagnostic adapter no-ops/legacy forward'; baselineSha256=(Get-FileSha256 $adapterPath) })
$sourceInputs['src/Controls/DxUi.Accessibility.cpp'] = Get-FileSha256 (Join-Path $CandidateRoot 'src/Controls/DxUi.Accessibility.cpp')
$sourceInputs['Tests/Controls/DxUi.ControlTests.vcxproj'] = Get-FileSha256 $candidateProjectPath
$sourceInputs['Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp'] = Get-FileSha256 $adapterPath

$attestationPath = Join-Path $BaselineRoot '.build/reports/scoped-tests/x64-Debug/build-only-attestation.json'
$attestation = [ordered]@{
    artifactType='dxui-scoped-build-only-attestation'; workloadOwner='DxUi'; configuration='Debug'; platform='x64'
    outcome='BUILD_PASSED'; testsExecuted=$false; testSuitePassClaim=$false
    scopedBuildInputIdentityBeforeBuild=$baselineBuildInputBeforeBuild; scopedBuildInputIdentityAfterBuild=$baselineBuildInputAfterBuild
    scopedArtifactIdentityBeforeBuild=$baselineArtifactBeforeBuild; scopedArtifactIdentityAfterBuild=$baselineArtifactAfterBuild
    scopedBuildReceiptIdentity=$baselineBuildReceiptIdentity
    scopedBuildReceiptSha256=(Get-FileSha256 $baselineBuildReceiptPath); buildLog=[IO.Path]::GetRelativePath($BaselineRoot,$baselineBuildLog.FullName)
    buildLogSha256=$baselineBuildLogHash; baselinePerformanceToolIdentity=$baselinePerformancePostBuild
}
[void](New-Item -ItemType Directory -Path (Split-Path $attestationPath) -Force)
[IO.File]::WriteAllText($attestationPath,(ConvertTo-Json -InputObject $attestation -Depth 12) + "`n",[Text.UTF8Encoding]::new($false))
$buildOnlyAttestationHash = Get-FileSha256 $attestationPath

$manifest = [ordered]@{
    schemaVersion=1
    createdUtc=[DateTime]::UtcNow.ToString('o')
    artifactType='accessibility-scheduling-paired-baseline-build-manifest-v2'
    workloadOwner='DxUi'
    fixture='dxui-accessibility-publication-scheduling-v1'
    candidateRoot=$CandidateRoot
    candidateHead=$candidateHead[0].Trim()
    baselineRoot=$BaselineRoot
    baselineHead=$baselineHead[0].Trim()
    overlayMethod='Refreshed the full compiled source closure from the candidate; retained pre-A1 Accessibility.cpp scheduling/provider logic with two disclosed payload-call API bridges, plus the two disclosed baseline test-adapter files.'
    retainedImplementation=[ordered]@{ target='src/Controls/DxUi.Accessibility.cpp'; source='Measurements/Review-2026-10-09/AccessibilitySchedulingBefore/Accessibility.cpp.txt'; sha256=$retainedHash; compiledSha256=$retainedCompatibilityHash; apiBridges=$retainedApiBridges; interpretation='immutable pre-A1 source retained; compiled copy changes only two posted-payload call signatures for shared API compatibility' }
    benchmark=[ordered]@{ test='TestAccessibilityPublicationMutationBatchRetainsFinalStateAndReportsCost'; fixturePath='Tests/Controls/DxUi.Tests.Accessibility.cpp'; fixtureSha256=$candidateFixtureHash; includedScenarios=@('keystroke-batch','selection-batch'); sampleFields=@('microseconds','queryMicroseconds','totalMicroseconds','operationSnapshotBuilds','snapshotBuilds','uiaListening'); operationsPerRepetition=128; repetitionsPerScenario=7; benchmarkExecuted=$false; pairedTimingRun=$false; conclusion='not-measured' }
    adapters=[ordered]@{ path='Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp'; sha256=(Get-FileSha256 $adapterPath); purpose='baseline-only diagnostic no-ops and legacy forwarders for test-link compatibility'; candidateContainsAdapter=$false; testSuiteExecuted=$false }
    provenanceErrata=[ordered]@{ originalToolHostInventory=$toolHostErratum }
    build=[ordered]@{
        configuration='Debug'; platform='x64'; rebuild=$true
        compile=[ordered]@{ log=[IO.Path]::GetRelativePath($BaselineRoot,$baselineBuildLog.FullName); logSha256=$baselineBuildLogHash; exitCode=0; result='passed'; dxuiLibSha256=(Get-FileSha256 (Join-Path $BaselineRoot '.build/x64/Debug/DxUi.lib')); controlTestsExeSha256=(Get-FileSha256 (Join-Path $BaselineRoot '.build/x64/Debug/DxUi.ControlTests.exe')); scopedBuildReceipt=[IO.Path]::GetRelativePath($BaselineRoot,$baselineBuildReceiptPath); scopedBuildReceiptSha256=(Get-FileSha256 $baselineBuildReceiptPath); testsExecuted=$false }
        restore=[ordered]@{ result='pre-existing dependencies verified; this helper did not restore'; exitCode=0; log=$original.build.restore.log; logSha256=$originalRestoreLogHash; installedStatusSha256=$baselineStatusHash; packages=$original.build.restore.packages }
        candidateBuildReceipt=[ordered]@{ path=[IO.Path]::GetRelativePath($CandidateRoot,$candidateBuildPost.receiptPath); sha256=$candidateBuildPost.receiptSha256; buildInputIdentity=$candidateBuildPost.buildInputIdentity; artifactIdentity=$candidateBuildPost.artifactIdentity }
        baselineBuildOnlyAttestation=[ordered]@{ path=[IO.Path]::GetRelativePath($BaselineRoot,$attestationPath); sha256=$buildOnlyAttestationHash; testsExecuted=$false; testSuitePassClaim=$false; receiptIdentity=$baselineBuildReceiptIdentity; buildInputIdentityBeforeBuild=$baselineBuildInputBeforeBuild; buildInputIdentityAfterBuild=$baselineBuildInputAfterBuild; artifactIdentityBeforeBuild=$baselineArtifactBeforeBuild; artifactIdentityAfterBuild=$baselineArtifactAfterBuild }
    }
    overlayInputs=$overlayInputs.ToArray()
    overlayVerification=[ordered]@{ sharedCompiledInputCount=@($candidateAfterOverlay.Keys | Where-Object { $_ -notin $allowedPaths }).Count; allowedDifferences=$allowedPaths; candidateBuildReceiptVerifiedBeforeAndAfter=$true; baselineBuildSourceStable=$true; postOverlayBuildMutationWasOnlyBuildProductsAndScopedReceipt=$true; sourceClosure='candidate hashes equal baseline for every compiled input except the retained implementation, baseline project registration, and baseline-only adapter' }
    toolchain=$toolchain
    toolIdentities=[ordered]@{ baselinePreBuild=$baselinePerformancePreBuild; candidatePreBuild=$candidatePerformancePreBuild; baselinePostBuild=$baselinePerformancePostBuild; candidatePostBuild=$candidatePerformancePostBuild }
    runtimeDependencies=@($original.runtimeDependencies)
    runtimeDependencyDump=$original.runtimeDependencyDump
    benchmarkInputs=$sourceInputs
    sourceDescription='DxUi library-owned accessibility publication mutation batch fixture. The baseline compiles pre-A1 Accessibility.cpp with exactly two disclosed payload-call API bridges while preserving its scheduling, snapshot, provider and mutation logic. It compiles the same current benchmark fixture and shared source inputs as candidate. Two disclosed test-adapter files bridge diagnostic link/API drift. This is explanatory comparison, not qualification of historical accessibility behavior.'
    excluded=@('test suite execution','paired timing execution','all other test executables','Release, ARM64, and ASan profiles','consumer builds or tests','interactive desktop qualification')
}
if ((Get-FileSha256 $OriginalSetupManifest) -cne $originalHash) { throw 'Preserved original setup receipt changed before versioned manifest publication.' }
if (Test-Path -LiteralPath $VersionedSetupManifest) { throw "Refusing to replace an existing versioned setup receipt: $VersionedSetupManifest" }
$json = ConvertTo-Json -InputObject $manifest -Depth 16
[IO.File]::WriteAllText($VersionedSetupManifest,$json + "`n",[Text.UTF8Encoding]::new($false))
Write-Host "Build-only baseline attestation: $attestationPath"
Write-Host "Versioned setup manifest: $VersionedSetupManifest"
Write-Host 'No test suite or benchmark was executed; preserved original setup receipt remains unchanged.'
