<#
.SYNOPSIS
Runs the retained Accessibility publication scheduling study against the named x64 Debug binaries.
.DESCRIPTION
This is a focused, explanatory study only. It does not build, restore, run other tests, or make a policy/qualification
decision. It executes one named Accessibility test serially in a seeded, balanced 12-block ABBA/BAAB schedule. The
test process writes directly to one raw log through cmd redirection (no PowerShell output pipe). Every invocation
must pass and emit exactly fourteen checked benchmark records before its block contributes to inference.
#>
[CmdletBinding()]
param(
    [string] $BaselineRoot = '',
    [string] $CandidateRoot = '',
    [string] $SetupManifest = '',
    [ValidateRange(12,12)][int] $Blocks = 12,
    [int] $Seed = -1,
    [string] $OutputRoot = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:TestName = 'TestAccessibilityPublicationMutationBatchRetainsFinalStateAndReportsCost'
$script:Scenarios = @('keystroke-batch', 'selection-batch')
$script:TimingMetrics = @(
    [ordered]@{ scenario='keystroke-batch'; field='microseconds'; name='keystroke-batch/operation' },
    [ordered]@{ scenario='keystroke-batch'; field='queryMicroseconds'; name='keystroke-batch/first-retained-provider-query' },
    [ordered]@{ scenario='keystroke-batch'; field='totalMicroseconds'; name='keystroke-batch/total' },
    [ordered]@{ scenario='selection-batch'; field='microseconds'; name='selection-batch/operation' },
    [ordered]@{ scenario='selection-batch'; field='queryMicroseconds'; name='selection-batch/first-retained-provider-query' },
    [ordered]@{ scenario='selection-batch'; field='totalMicroseconds'; name='selection-batch/total' }
)
$script:SnapshotCounters = @('operationSnapshotBuilds', 'snapshotBuilds')

$candidateDefault = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$studyFolder = [IO.Path]::GetFullPath($PSScriptRoot)
if (-not $SetupManifest) { $SetupManifest = Join-Path $studyFolder 'source-input-manifest.json' }
$SetupManifest = [IO.Path]::GetFullPath($SetupManifest)
if (-not (Test-Path -LiteralPath $SetupManifest -PathType Leaf)) { throw "The paired-study setup manifest is missing: $SetupManifest" }
$scriptHashAtStart = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
$manifestHashAtLoad = (Get-FileHash -LiteralPath $SetupManifest -Algorithm SHA256).Hash
$setup = Get-Content -LiteralPath $SetupManifest -Raw | ConvertFrom-Json -AsHashtable
if ((Get-FileHash -LiteralPath $SetupManifest -Algorithm SHA256).Hash -cne $manifestHashAtLoad) { throw 'Setup manifest changed while it was being loaded.' }
if ($setup.workloadOwner -cne 'DxUi' -or $setup.fixture -cne 'dxui-accessibility-publication-scheduling-v1') {
    throw 'The setup receipt does not identify the expected DxUi accessibility scheduling fixture.'
}
if ($setup.benchmark.benchmarkExecuted -ne $false -or $setup.benchmark.pairedTimingRun -ne $false) {
    throw 'The setup receipt must remain a build-only, not-yet-measured baseline.'
}
if (-not $BaselineRoot) { $BaselineRoot = if ($setup.baselineRoot) { [string]$setup.baselineRoot } else { 'C:\Users\eric\.codex\worktrees\accessibility-scheduling-baseline\DxUi' } }
if (-not $CandidateRoot) { $CandidateRoot = if ($setup.candidateRoot) { [string]$setup.candidateRoot } else { $candidateDefault } }
$CandidateRoot = [IO.Path]::GetFullPath($CandidateRoot)
$BaselineRoot = [IO.Path]::GetFullPath($BaselineRoot)
if ($setup.baselineRoot -and -not [string]::Equals([IO.Path]::GetFullPath($setup.baselineRoot), $BaselineRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "BaselineRoot differs from the named setup receipt: $BaselineRoot"
}
if ($setup.candidateRoot -and -not [string]::Equals([IO.Path]::GetFullPath($setup.candidateRoot), $CandidateRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "CandidateRoot differs from the named setup receipt: $CandidateRoot"
}

Import-Module (Join-Path $CandidateRoot 'Tools/PairedRun.psm1') -Force
Import-Module (Join-Path $CandidateRoot 'Tools/PerformanceComparison.psm1') -Force
Import-Module (Join-Path $CandidateRoot 'Tools/PerformancePolicy.psm1') -Force
Import-Module (Join-Path $CandidateRoot 'Tools/ScopedTesting.psm1') -Force

function Get-Sha256Text([string] $Text) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text))) }
    finally { $sha.Dispose() }
}

function Get-FileSha256([string] $Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Required study input is missing: $Path" }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

function Get-ExpectedOverlayHash([System.Collections.IDictionary] $InputRecord, [string] $Role) {
    if ($Role -eq 'baseline') { return [string]$InputRecord.baselineSha256 }
    if ($InputRecord.path -eq 'Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp') { return $null }
    if ($InputRecord.path -eq 'Tests/Controls/DxUi.ControlTests.vcxproj') { return [string]$InputRecord.candidateSha256 }
    if ($InputRecord.path -eq 'src/Controls/DxUi.Accessibility.cpp') { return [string]$InputRecord.candidateCurrentSha256 }
    return [string]$InputRecord.candidateSha256
}

function Get-ImportedRuntimeResolutionInventory([string] $ExecutableDirectory, [string[]] $Names, [string] $VisualStudio) {
    $directories = [Collections.Generic.List[string]]::new()
    foreach ($directory in @($ExecutableDirectory, [Environment]::SystemDirectory, $env:SystemRoot, (Join-Path $env:SystemRoot 'System32/downlevel'))) {
        if ($directory -and (Test-Path -LiteralPath $directory -PathType Container -ErrorAction SilentlyContinue) -and -not $directories.Contains($directory)) { $directories.Add($directory) }
    }
    foreach ($directory in ($env:PATH -split ';')) {
        # PATH can contain an inaccessible service-profile directory. It cannot resolve this process's import;
        # omit it from this explanatory search inventory. Every required non-API-set DLL must still resolve below.
        if ($directory -and (Test-Path -LiteralPath $directory -PathType Container -ErrorAction SilentlyContinue) -and -not $directories.Contains($directory)) { $directories.Add($directory) }
    }
    $redistRoot = Join-Path $VisualStudio 'VC/Redist/MSVC'
    if (Test-Path -LiteralPath $redistRoot -PathType Container) {
        foreach ($versionDirectory in Get-ChildItem -LiteralPath $redistRoot -Directory -ErrorAction SilentlyContinue) {
            foreach ($pattern in @('debug_nonredist/x64/Microsoft.VC*.DebugCRT', 'x64/Microsoft.VC*.CRT')) {
                foreach ($runtimeDirectory in Get-ChildItem -Path (Join-Path $versionDirectory.FullName $pattern) -Directory -ErrorAction SilentlyContinue) {
                    if (-not $directories.Contains($runtimeDirectory.FullName)) { $directories.Add($runtimeDirectory.FullName) }
                }
            }
        }
    }

    $resolved = [Collections.Generic.List[object]]::new()
    foreach ($name in ($Names | Sort-Object -Unique)) {
        $found = $null
        foreach ($directory in $directories) {
            $candidate = Join-Path $directory $name
            if (Test-Path -LiteralPath $candidate -PathType Leaf -ErrorAction SilentlyContinue) { $found = $candidate; break }
        }
        if (-not $found -and (Test-Path -LiteralPath $redistRoot -PathType Container)) {
            $fallback = @(Get-ChildItem -LiteralPath $redistRoot -Filter $name -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1)
            if ($fallback.Count -eq 1) { $found = $fallback[0].FullName }
        }
        if (-not $found -and $name -like 'api-ms-win-*') {
            $resolved.Add([ordered]@{ name=$name; resolution='Windows API-set contract'; sha256=$null; fileVersion=$null })
            continue
        }
        if (-not $found) { throw "Cannot resolve imported runtime dependency '$name' before measurement." }
        $version = (Get-Item -LiteralPath $found).VersionInfo.FileVersion
        $resolved.Add([ordered]@{ name=$name; resolution=[IO.Path]::GetFullPath($found); sha256=(Get-FileSha256 $found); fileVersion=$version })
    }
    return $resolved.ToArray()
}

function Get-TreeIdentity([string] $Role, [string] $Root, [string] $ExePath, [string] $LibraryPath) {
    $head = @(& git -C $Root rev-parse HEAD 2>$null)
    if ($LASTEXITCODE -ne 0 -or $head.Count -ne 1) { throw "Cannot read $Role HEAD at $Root" }
    $expectedHead = if ($Role -eq 'baseline') { [string]$setup.baselineHead } else { [string]$setup.candidateHead }
    if ($head[0].Trim() -cne $expectedHead) { throw "$Role HEAD changed from the reviewed setup identity." }

    $inputHashes = [ordered]@{}
    foreach ($record in $setup.overlayInputs) {
        $expected = Get-ExpectedOverlayHash $record $Role
        if ($null -eq $expected -or $expected -eq '') { continue }
        $path = Join-Path $Root ([string]$record.path)
        $actual = Get-FileSha256 $path
        if ($actual -cne $expected) { throw "$Role source/build input changed: $($record.path)" }
        $inputHashes[[string]$record.path] = $actual
    }
    $sourceLines = foreach ($key in ($inputHashes.Keys | Sort-Object -CaseSensitive)) { "$key=$($inputHashes[$key])" }
    $sourceFingerprint = Get-Sha256Text ($sourceLines -join "`n")

    $statusLines = @(& git -C $Root status --porcelain --untracked-files=no 2>$null)
    if ($LASTEXITCODE -ne 0) { throw "Cannot read tracked status for $Role." }
    $statusHash = Get-Sha256Text ($statusLines -join "`n")

    $dependencyStatus = Join-Path $Root '.build/vcpkg_installed/x64/vcpkg/status'
    $dependencyHash = Get-FileSha256 $dependencyStatus
    if ($dependencyHash -cne [string]$setup.benchmarkInputs.'vcpkg-installed-status') { throw "$Role restored dependency status differs from the frozen setup receipt." }
    $toolFiles = foreach ($tool in $setup.toolchain.toolFiles) {
        $actual = Get-FileSha256 ([string]$tool.path)
        if ($actual -cne [string]$tool.sha256) { throw "Toolchain binary changed: $($tool.path)" }
        [ordered]@{ path=$tool.path; sha256=$actual; version=$tool.version }
    }
    $runtimeImportInventory = Get-ImportedRuntimeResolutionInventory -ExecutableDirectory (Split-Path $ExePath -Parent) -Names ([string[]]$setup.runtimeDependencies) -VisualStudio ([string]$setup.toolchain.visualStudio)

    $buildLog = $null
    if ($Role -eq 'baseline') {
        $baselineBuildLog = Join-Path $Root ([string]$setup.build.compile.log)
        $baselineBuildLogHash = Get-FileSha256 $baselineBuildLog
        if ($baselineBuildLogHash -cne [string]$setup.build.compile.logSha256) { throw 'Baseline build log differs from the frozen setup receipt.' }
        if ((Get-FileSha256 $LibraryPath) -cne [string]$setup.build.compile.dxuiLibSha256 -or
            (Get-FileSha256 $ExePath) -cne [string]$setup.build.compile.controlTestsExeSha256) {
            throw 'Baseline x64 Debug binaries differ from the frozen setup receipt.'
        }
        $restoreLog = Join-Path $Root ([string]$setup.build.restore.log)
        $restoreLogHash = Get-FileSha256 $restoreLog
        if ($restoreLogHash -cne [string]$setup.build.restore.logSha256) { throw 'Baseline restore log differs from the frozen setup receipt.' }
        $buildLog = [ordered]@{
            compile=[ordered]@{ path=$setup.build.compile.log; sha256=$baselineBuildLogHash; result=$setup.build.compile.result; exitCode=$setup.build.compile.exitCode }
            restore=[ordered]@{ path=$setup.build.restore.log; sha256=$restoreLogHash; result=$setup.build.restore.result; exitCode=$setup.build.restore.exitCode; packages=$setup.build.restore.packages }
        }
    } else {
        $latest = Get-ChildItem -LiteralPath (Join-Path $Root '.build/logs') -Filter 'build-x64-Debug-*.log' -File | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
        if (-not $latest) { throw 'Candidate x64 Debug build log is missing.' }
        $buildLog = [ordered]@{
            compile=[ordered]@{ path=[IO.Path]::GetRelativePath($Root, $latest.FullName); sha256=(Get-FileSha256 $latest.FullName) }
            configuration='Debug'; platform='x64'; result='latest-x64-Debug-build-log-recorded'
        }
    }
    $os = [Environment]::OSVersion.Version.ToString()
    $envIdentity = [ordered]@{
        osVersion=$os
        osArchitecture=[Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
        processArchitecture=[Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
        processorIdentifier=[string]$env:PROCESSOR_IDENTIFIER
        processorCount=[Environment]::ProcessorCount
        pathSha256=(Get-Sha256Text ([string]$env:PATH))
    }
    $identity = [ordered]@{
        role=$Role; root=$Root; head=$head[0].Trim(); trackedStatusSha256=$statusHash
        sourceInputCount=$inputHashes.Count; sourceInputSha256=$sourceFingerprint; sourceInputs=$inputHashes
        fixtureSha256=(Get-FileSha256 (Join-Path $Root 'Tests/Controls/DxUi.Tests.Accessibility.cpp'))
        librarySha256=(Get-FileSha256 $LibraryPath); executableSha256=(Get-FileSha256 $ExePath)
        dependencyStatusSha256=$dependencyHash; dependencyPackages=$setup.build.restore.packages
        toolchain=$toolFiles; importedRuntimeResolutionInventory=$runtimeImportInventory; environment=$envIdentity; build=$buildLog
    }
    return $identity
}

function Get-IdentitySha256([System.Collections.IDictionary] $Identity) {
    return Get-Sha256Text (ConvertTo-Json -InputObject $Identity -Depth 12 -Compress)
}

function Get-CompiledClosure([string] $Root) {
    $paths = @(& git -c core.quotepath=false -C $Root ls-files --cached --others --exclude-standard 2>$null)
    if ($LASTEXITCODE -ne 0) { throw "Cannot enumerate the compiled source closure at $Root." }
    $closure = [ordered]@{}
    foreach ($path in ($paths | Where-Object { $_ } | Sort-Object -Unique)) {
        if ($path -match '^\.build/' -or $path -match '^(Measurements|docs|Changes|legacy|Specs/(Plans|Done|TestRuns|Reviews|Mockups))/' -or $path -match '\.md$') { continue }
        $file = Join-Path $Root $path
        if (Test-Path -LiteralPath $file -PathType Leaf) { $closure[$path] = Get-FileSha256 $file }
    }
    return $closure
}

function Assert-ExpectedOverlayHash([string] $Root, [string] $Path, [string] $Expected, [string] $Role) {
    $actual = Get-FileSha256 (Join-Path $Root $Path)
    if ($actual -cne $Expected) { throw "$Role overlay input differs from the setup receipt: $Path" }
    return $actual
}

function Assert-SharedCompiledClosure([string] $BaselineRoot, [string] $CandidateRoot) {
    $baselineFiles = Get-CompiledClosure $BaselineRoot
    $candidateFiles = Get-CompiledClosure $CandidateRoot
    $allowedDifferences = @(
        'src/Controls/DxUi.Accessibility.cpp',
        'Tests/Controls/DxUi.ControlTests.vcxproj',
        'Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp'
    )
    foreach ($path in $allowedDifferences) {
        if ($path -ne 'Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp' -and
            (-not $baselineFiles.Contains($path) -or -not $candidateFiles.Contains($path))) {
            throw "A declared baseline/candidate compiled-input exception is missing: $path"
        }
    }
    if (-not $baselineFiles.Contains('Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp') -or
        $candidateFiles.Contains('Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp')) {
        throw 'The baseline-only diagnostic adapter is missing from baseline or present in candidate.'
    }
    $expectedInputs = @{}
    foreach ($record in $setup.overlayInputs) { $expectedInputs[[string]$record.path] = $record }
    $accessibility = $expectedInputs['src/Controls/DxUi.Accessibility.cpp']
    $project = $expectedInputs['Tests/Controls/DxUi.ControlTests.vcxproj']
    $adapter = $expectedInputs['Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp']
    if (-not $accessibility -or -not $project -or -not $adapter) { throw 'The setup receipt omits an allowed baseline/candidate input record.' }
    $baselineExceptions = [ordered]@{
        'src/Controls/DxUi.Accessibility.cpp' = [string]$accessibility.baselineSha256
        'Tests/Controls/DxUi.ControlTests.vcxproj' = [string]$project.baselineSha256
        'Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp' = [string]$adapter.baselineSha256
    }
    $candidateExceptions = [ordered]@{
        'src/Controls/DxUi.Accessibility.cpp' = [string]$accessibility.candidateCurrentSha256
        'Tests/Controls/DxUi.ControlTests.vcxproj' = [string]$project.candidateSha256
    }
    foreach ($path in $allowedDifferences) {
        if ($path -eq 'Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp') {
            [void](Assert-ExpectedOverlayHash $BaselineRoot $path $baselineExceptions[$path] 'baseline')
            continue
        }
        [void](Assert-ExpectedOverlayHash $BaselineRoot $path $baselineExceptions[$path] 'baseline')
        [void](Assert-ExpectedOverlayHash $CandidateRoot $path $candidateExceptions[$path] 'candidate')
    }
    $commonBaseline = [ordered]@{}
    foreach ($path in $baselineFiles.Keys) { if ($path -notin $allowedDifferences) { $commonBaseline[$path] = $baselineFiles[$path] } }
    $commonCandidate = [ordered]@{}
    foreach ($path in $candidateFiles.Keys) { if ($path -notin $allowedDifferences) { $commonCandidate[$path] = $candidateFiles[$path] } }
    $baselineJson = ConvertTo-Json -InputObject $commonBaseline -Depth 4 -Compress
    $candidateJson = ConvertTo-Json -InputObject $commonCandidate -Depth 4 -Compress
    if ($baselineJson -cne $candidateJson) {
        $missing = @($commonBaseline.Keys | Where-Object { -not $commonCandidate.Contains($_) })
        $extra = @($commonCandidate.Keys | Where-Object { -not $commonBaseline.Contains($_) })
        $changed = @($commonBaseline.Keys | Where-Object { $commonCandidate.Contains($_) -and $commonBaseline[$_] -cne $commonCandidate[$_] })
        throw "Compiled source closure differs beyond the three declared inputs. Missing=[$($missing -join ',')]; extra=[$($extra -join ',')]; changed=[$($changed -join ',')]"
    }
    return [ordered]@{
        sharedCompiledInputCount=$commonBaseline.Count
        sharedCompiledInputSha256=(Get-Sha256Text $baselineJson)
        permittedDifferences=$allowedDifferences
        baselineOnlyAdapterSha256=$baselineExceptions['Tests/Controls/Controls.Tests.BaselineSchedulingAdapters.cpp']
        baselineAccessibilitySha256=$baselineExceptions['src/Controls/DxUi.Accessibility.cpp']
        candidateAccessibilitySha256=$candidateExceptions['src/Controls/DxUi.Accessibility.cpp']
        baselineTestProjectSha256=$baselineExceptions['Tests/Controls/DxUi.ControlTests.vcxproj']
        candidateTestProjectSha256=$candidateExceptions['Tests/Controls/DxUi.ControlTests.vcxproj']
    }
}

function Get-ScopedBuildAttestation([string] $Role, [string] $Root) {
    $buildInputIdentity = Get-ScopedBuildInputIdentity -Root $Root -Platform x64
    $artifactIdentity = Get-ScopedArtifactIdentity -Root $Root -Platform x64 -Configuration Debug
    $receiptPath = Join-Path $Root '.build/reports/scoped-tests/x64-Debug/build.json'
    $expectedReceiptIdentity = Get-ScopedDigest ($buildInputIdentity + "`n" + $artifactIdentity)
    if (-not (Test-ScopedReceipt -Path $receiptPath -Identity $expectedReceiptIdentity)) {
        throw "$Role has no valid scoped x64 Debug build attestation for its exact compiled source, installed dependency, and artifact closure."
    }
    return [ordered]@{
        buildInputIdentity=$buildInputIdentity; artifactIdentity=$artifactIdentity; attestationIdentity=$expectedReceiptIdentity
        receiptPath=[IO.Path]::GetRelativePath($Root, $receiptPath); receiptSha256=(Get-FileSha256 $receiptPath)
    }
}

function Get-VerifiedPerformanceToolIdentity([string] $Role, [string] $Root) {
    $identity = Get-PerformanceToolIdentities -Root $Root -Platform x64 -Configuration Debug
    if ($identity.identityStatus -cne 'verifiable') { throw "$Role performance/toolchain identity is unverifiable: $($identity.identityError)" }
    $identity['role'] = $Role
    return $identity
}

function Assert-EqualPerformanceToolIdentities([System.Collections.IDictionary] $Baseline, [System.Collections.IDictionary] $Candidate) {
    foreach ($field in @('harnessIdentity','toolchainIdentity','dependencyIdentity','environmentIdentity')) {
        if ([string]$Baseline[$field] -cne [string]$Candidate[$field]) { throw "Baseline/candidate performance identity differs for $field." }
    }
}

function Get-Median([double[]] $Values) {
    if ($Values.Length -eq 0) { throw 'Cannot calculate a median from no benchmark values.' }
    $sorted = @($Values | Sort-Object)
    $middle = [int][Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2) { return [double]$sorted[$middle] }
    return [double](($sorted[$middle - 1] + $sorted[$middle]) / 2.0)
}

function ConvertTo-CheckedProcessResult([System.Collections.IDictionary] $Step, [string] $LogPath, [int] $ExitCode) {
    $text = Get-Content -LiteralPath $LogPath -Raw
    if ($ExitCode -ne 0) { throw "Runner $($Step.Name) exited $ExitCode; see $LogPath" }
    $startPattern = [regex]::Escape("[START] $script:TestName")
    $donePattern = [regex]::Escape("[DONE] $script:TestName")
    if ([regex]::Matches($text, $startPattern).Count -ne 1 -or [regex]::Matches($text, $donePattern).Count -ne 1) {
        throw "Runner $($Step.Name) did not execute and complete exactly the required test; see $LogPath"
    }
    if ($text -match '(?m)^\s*(FAILED:|SKIPPED:|TIMEOUT:)') { throw "Runner $($Step.Name) reported failure, skip, or timeout; see $LogPath" }
    $matches = [regex]::Matches($text, 'UIA_PUBLICATION_BENCHMARK\s+(\{[^\r\n]*\})')
    if ($matches.Count -ne 14) { throw "Runner $($Step.Name) emitted $($matches.Count) benchmark records, expected 14; see $LogPath" }
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($match in $matches) {
        $row = $match.Groups[1].Value | ConvertFrom-Json -AsHashtable
        if ([string]$row.scenario -notin $script:Scenarios) { throw "Unexpected benchmark scenario '$($row.scenario)' in $LogPath" }
        if ([int]$row.operations -ne 128) { throw "Unexpected operation count in $LogPath" }
        if ([int]$row.repetition -lt 0 -or [int]$row.repetition -gt 6) { throw "Unexpected repetition number in $LogPath" }
        foreach ($field in @('microseconds','queryMicroseconds','totalMicroseconds')) {
            $value=[double]$row[$field]
            if ([double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -le 0) { throw "Invalid $field value in $LogPath" }
        }
        if ($row.uiaListening -isnot [bool]) { throw "Missing or invalid UIA listening state in $LogPath" }
        $script:uiaListeningObservations.Add([ordered]@{ run=$Step.Name; scenario=[string]$row.scenario; repetition=[int]$row.repetition; value=[bool]$row.uiaListening })
        foreach ($field in $script:SnapshotCounters) { if ([double]$row[$field] -lt 0) { throw "Invalid $field counter in $LogPath" } }
        $rows.Add($row)
    }
    $medians = [ordered]@{}
    foreach ($scenario in $script:Scenarios) {
        $scenarioRows = @($rows | Where-Object { $_.scenario -ceq $scenario })
        if ($scenarioRows.Count -ne 7 -or (@($scenarioRows.repetition | Sort-Object -Unique) -join ',') -cne '0,1,2,3,4,5,6') {
            throw "Scenario $scenario does not have exactly repetitions 0 through 6 in $LogPath"
        }
        $scenarioMedians = [ordered]@{}
        foreach ($field in @('microseconds','queryMicroseconds','totalMicroseconds') + $script:SnapshotCounters) {
            $values = [double[]]@($scenarioRows | ForEach-Object { [double]($_[$field]) })
            $scenarioMedians[$field] = Get-Median $values
        }
        $medians[$scenario] = $scenarioMedians
    }
    return [ordered]@{ name=$Step.Name; side=$Step.Side; block=$Step.Block; position=$Step.Position; blockOrder=$Step.Order; exitCode=$ExitCode;
        log=(Split-Path $LogPath -Leaf); logSha256=(Get-FileSha256 $LogPath); emittedRecords=$rows.Count; testStarted=$true; testCompleted=$true;
        firstRetainedProviderQueryAssertions='passed-by-completed-test'; processMedians=$medians }
}

function Invoke-LoggedRunner([string] $Executable, [string] $WorkingDirectory, [string] $LogPath) {
    foreach ($path in @($Executable, $LogPath)) {
        if ($path -match '[&|<>^()%!\r\n"]') { throw "Runner or log path has unsupported cmd.exe metacharacters: $path" }
    }
    $command = '"' + $Executable + '" --suite=Accessibility --test=' + $script:TestName + ' --no-activate --test-timeout=300 1> "' + $LogPath + '" 2>&1'
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $env:ComSpec
    $startInfo.Arguments = '/d /s /c "' + $command + '"'
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $process = [Diagnostics.Process]::Start($startInfo)
    try {
        if (-not $process.WaitForExit(360000)) {
            $process.Kill($true)
            $process.WaitForExit()
            throw 'The exact benchmark test exceeded the six-minute outer process bound.'
        }
        return [int]$process.ExitCode
    } finally { $process.Dispose() }
}

function Get-TimingAnalysis([object[]] $ScheduleBlocks, [System.Collections.IDictionary] $ProcessResults) {
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($definition in $script:TimingMetrics) {
        $effects = [Collections.Generic.List[double]]::new()
        $blockRows = [Collections.Generic.List[object]]::new()
        foreach ($block in $ScheduleBlocks) {
            $baselineRuns = @($block.BaselineRuns | ForEach-Object { $ProcessResults[$_] })
            $candidateRuns = @($block.CandidateRuns | ForEach-Object { $ProcessResults[$_] })
            if ($baselineRuns.Count -ne 2 -or $candidateRuns.Count -ne 2) { throw "Block $($block.Name) does not contain two runs per side." }
            $baselineProcessMedians = [double[]]@($baselineRuns | ForEach-Object { [double]($_['processMedians'][$definition.scenario][$definition.field]) })
            $candidateProcessMedians = [double[]]@($candidateRuns | ForEach-Object { [double]($_['processMedians'][$definition.scenario][$definition.field]) })
            $baselineCenter = Get-Median $baselineProcessMedians
            $candidateCenter = Get-Median $candidateProcessMedians
            $effect = [Math]::Log($candidateCenter / $baselineCenter)
            $effects.Add($effect)
            $blockRows.Add([ordered]@{ block=$block.Name; order=$block.Order; baselineRuns=@($block.BaselineRuns); candidateRuns=@($block.CandidateRuns);
                baselineProcessMedians=$baselineProcessMedians; candidateProcessMedians=$candidateProcessMedians; baselineCenter=$baselineCenter; candidateCenter=$candidateCenter; candidateOverBaselineLogEffect=$effect })
        }
        $pValue = Get-ExactSignFlipPValue -Effects $effects.ToArray()
        $meanEffect = [double](($effects | Measure-Object -Average).Average)
        $rows.Add([ordered]@{ name=$definition.name; scenario=$definition.scenario; field=$definition.field; direction='higher-is-slower';
            independentBlocks=$effects.Count; candidateOverBaselineGeometricRatio=[Math]::Exp($meanEffect); geometricChangePercent=(([Math]::Exp($meanEffect)-1.0)*100.0);
            meanBlockLogEffect=$meanEffect; blockEffects=$blockRows.ToArray(); exactTwoSidedSignFlipPValue=$pValue })
    }
    $adjusted = Get-HolmAdjustedPValues -PValues ([double[]]@($rows | ForEach-Object { [double]$_.exactTwoSidedSignFlipPValue }))
    for ($index=0; $index -lt $rows.Count; $index++) { $rows[$index].holmAdjustedPValue=$adjusted[$index] }
    return [ordered]@{ method='12 independent block log effects; exact two-sided sign-flip; Holm family-wise correction'; familySize=6;
        significanceLevel=0.05; firstHolmThreshold=(0.05/6.0); minimumAttainableTwoSidedP=(2.0/[Math]::Pow(2.0,12));
        minimumPBelowFirstHolmThreshold=((2.0/[Math]::Pow(2.0,12)) -lt (0.05/6.0)); timings=$rows.ToArray(); interpretation='explanatory only; not a performance policy or qualification verdict' }
}

function Get-CounterSummary([object[]] $RunRecords) {
    $summaries = [Collections.Generic.List[object]]::new()
    foreach ($scenario in $script:Scenarios) {
        foreach ($counter in $script:SnapshotCounters) {
            $sideSummaries = [ordered]@{}
            foreach ($side in @('baseline','candidate')) {
                $values = [double[]]@($RunRecords | Where-Object side -eq $side | ForEach-Object { [double]($_['processMedians'][$scenario][$counter]) })
                $sideSummaries[$side] = [ordered]@{ processCount=$values.Count; medianOfProcessMedians=(Get-Median $values); minimumProcessMedian=($values | Measure-Object -Minimum).Minimum; maximumProcessMedian=($values | Measure-Object -Maximum).Maximum }
            }
            $summaries.Add([ordered]@{ scenario=$scenario; counter=$counter; baseline=$sideSummaries.baseline; candidate=$sideSummaries.candidate; inference='none; deterministic snapshot counters reported separately from timing p-values' })
        }
    }
    return $summaries.ToArray()
}

function Write-StudyRunReadme([string] $Path, [int] $RunSeed) {
    $content = @'
# Accessibility scheduling paired study run

This directory contains one exploratory paired timing run for `TestAccessibilityPublicationMutationBatchRetainsFinalStateAndReportsCost`. It executes only the `Accessibility` suite with `--no-activate`, the exact named test, and a 300-second test watchdog. Each of 12 independent randomized ABBA/BAAB blocks has two baseline and two candidate processes. Each process must emit seven records for each of the two benchmark scenarios; process medians form the block-level log effects.

The schedule seed and exact order are in `study-result.json`; 48 serial runner invocations are retained as raw `.log` files and individually SHA-256 hashed there. Timing analysis covers the operation, first retained-provider query, and total microseconds for both scenarios, with exact two-sided sign-flip p-values and Holm correction over six timing outcomes. Snapshot-build counters are reported separately without statistical inference.

This is descriptive/explanatory evidence only. It is not a policy acceptance, pass/fail gate, rebaseline, or platform/consumer qualification. A failed test, invalid record, missing identity, or changed pre/post identity leaves the study incomplete or inconclusive.
'@
    [IO.File]::WriteAllText($Path, $content, [Text.UTF8Encoding]::new($false))
}

if (-not $Seed -or $Seed -lt 0) { $Seed = [Security.Cryptography.RandomNumberGenerator]::GetInt32(1, [int]::MaxValue) }
$schedule = Get-RandomizedPairedBlockSchedule -Blocks $Blocks -Seed $Seed
if (@($schedule.Blocks | Where-Object Order -eq 'ABBA').Count -ne 6 -or @($schedule.Blocks | Where-Object Order -eq 'BAAB').Count -ne 6 -or @($schedule.Steps).Count -ne 48) {
    throw 'The shared schedule generator did not produce twelve balanced four-run blocks.'
}
$baselineExe = Join-Path $BaselineRoot '.build/x64/Debug/DxUi.ControlTests.exe'
$candidateExe = Join-Path $CandidateRoot '.build/x64/Debug/DxUi.ControlTests.exe'
$baselineLib = Join-Path $BaselineRoot '.build/x64/Debug/DxUi.lib'
$candidateLib = Join-Path $CandidateRoot '.build/x64/Debug/DxUi.lib'
foreach ($path in @($baselineExe,$candidateExe,$baselineLib,$candidateLib)) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Named x64 Debug study artifact is missing: $path" } }
if (-not [string]::Equals((Get-FileSha256 (Join-Path $BaselineRoot 'Tests/Controls/DxUi.Tests.Accessibility.cpp')), [string]$setup.benchmark.fixtureSha256, [StringComparison]::Ordinal)) {
    throw 'The baseline benchmark fixture no longer matches the frozen setup receipt.'
}

if (-not $OutputRoot) { $OutputRoot = Join-Path $studyFolder 'Runs' }
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
if (-not $OutputRoot.StartsWith($studyFolder + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Study outputs must remain under the owned AccessibilitySchedulingPairedBaseline folder.'
}
$runId = '{0}-seed-{1}' -f [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'), $Seed
$runRoot = Join-Path $OutputRoot $runId
if (Test-Path -LiteralPath $runRoot) { throw "Study output directory already exists: $runRoot" }

$baselinePre = Get-TreeIdentity -Role baseline -Root $BaselineRoot -ExePath $baselineExe -LibraryPath $baselineLib
$candidatePre = Get-TreeIdentity -Role candidate -Root $CandidateRoot -ExePath $candidateExe -LibraryPath $candidateLib
if ($baselinePre.fixtureSha256 -cne $candidatePre.fixtureSha256 -or $baselinePre.fixtureSha256 -cne [string]$setup.benchmark.fixtureSha256) {
    throw 'Baseline and candidate must use the identical frozen benchmark fixture.'
}
$compiledClosurePre = Assert-SharedCompiledClosure -BaselineRoot $BaselineRoot -CandidateRoot $CandidateRoot
$baselineBuildAttestationPre = Get-ScopedBuildAttestation -Role baseline -Root $BaselineRoot
$candidateBuildAttestationPre = Get-ScopedBuildAttestation -Role candidate -Root $CandidateRoot
$baselinePerformanceIdentityPre = Get-VerifiedPerformanceToolIdentity -Role baseline -Root $BaselineRoot
$candidatePerformanceIdentityPre = Get-VerifiedPerformanceToolIdentity -Role candidate -Root $CandidateRoot
Assert-EqualPerformanceToolIdentities $baselinePerformanceIdentityPre $candidatePerformanceIdentityPre
$activeInstrumentationPre = @(Get-ScopedInstrumentation)
if ($activeInstrumentationPre.Count) { throw "Instrumentation flags are active; ordinary study runner is required: $($activeInstrumentationPre -join ', ')" }
$scriptHashBefore = $scriptHashAtStart
$manifestHashBefore = $manifestHashAtLoad
if ((Get-FileSha256 $PSCommandPath) -cne $scriptHashBefore -or (Get-FileSha256 $SetupManifest) -cne $manifestHashBefore) {
    throw 'Study script or setup metadata changed during preflight; no benchmark was started.'
}
$script:uiaListeningObservations = [Collections.Generic.List[object]]::new()

New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
Write-StudyRunReadme -Path (Join-Path $runRoot 'README.md') -RunSeed $Seed
$processResults = [ordered]@{}
$runRecords = [Collections.Generic.List[object]]::new()
$failure = $null
try {
    foreach ($step in $schedule.Steps) {
        $exe = if ($step.Side -eq 'baseline') { $baselineExe } else { $candidateExe }
        $logName = '{0:D2}-{1}-{2}.log' -f [int]$step.Position, [string]$step.Name, ([string]$step.Block)
        $logPath = Join-Path $runRoot $logName
        Write-Host ("Study {0}/48: {1} {2} ({3})" -f $step.Position, $step.Name, $step.Side, $step.Block)
        $workingDirectory = if ($step.Side -eq 'baseline') { $BaselineRoot } else { $CandidateRoot }
        $exitCode = Invoke-LoggedRunner -Executable $exe -WorkingDirectory $workingDirectory -LogPath $logPath
        $result = ConvertTo-CheckedProcessResult -Step $step -LogPath $logPath -ExitCode $exitCode
        $processResults[[string]$step.Name] = $result
        $runRecords.Add($result)
    }
} catch {
    $failure = $_.Exception.Message
}

$baselinePost = $null
$candidatePost = $null
$compiledClosurePost = $null
$baselineBuildAttestationPost = $null
$candidateBuildAttestationPost = $null
$baselinePerformanceIdentityPost = $null
$candidatePerformanceIdentityPost = $null
$activeInstrumentationPost = @()
$postIdentityError = $null
try {
    $baselinePost = Get-TreeIdentity -Role baseline -Root $BaselineRoot -ExePath $baselineExe -LibraryPath $baselineLib
    $candidatePost = Get-TreeIdentity -Role candidate -Root $CandidateRoot -ExePath $candidateExe -LibraryPath $candidateLib
    $compiledClosurePost = Assert-SharedCompiledClosure -BaselineRoot $BaselineRoot -CandidateRoot $CandidateRoot
    $baselineBuildAttestationPost = Get-ScopedBuildAttestation -Role baseline -Root $BaselineRoot
    $candidateBuildAttestationPost = Get-ScopedBuildAttestation -Role candidate -Root $CandidateRoot
    $baselinePerformanceIdentityPost = Get-VerifiedPerformanceToolIdentity -Role baseline -Root $BaselineRoot
    $candidatePerformanceIdentityPost = Get-VerifiedPerformanceToolIdentity -Role candidate -Root $CandidateRoot
    Assert-EqualPerformanceToolIdentities $baselinePerformanceIdentityPost $candidatePerformanceIdentityPost
    $activeInstrumentationPost = @(Get-ScopedInstrumentation)
    if ($activeInstrumentationPost.Count) { throw "Instrumentation flags became active during the study: $($activeInstrumentationPost -join ', ')" }
} catch { $postIdentityError = $_.Exception.Message }
$scriptHashAfter = Get-FileSha256 $PSCommandPath
$manifestHashAfter = Get-FileSha256 $SetupManifest
$identityStable = $null -eq $postIdentityError -and $null -ne $baselinePost -and $null -ne $candidatePost -and
    (Get-IdentitySha256 $baselinePre) -ceq (Get-IdentitySha256 $baselinePost) -and
    (Get-IdentitySha256 $candidatePre) -ceq (Get-IdentitySha256 $candidatePost) -and
    (Get-IdentitySha256 $compiledClosurePre) -ceq (Get-IdentitySha256 $compiledClosurePost) -and
    (Get-IdentitySha256 $baselineBuildAttestationPre) -ceq (Get-IdentitySha256 $baselineBuildAttestationPost) -and
    (Get-IdentitySha256 $candidateBuildAttestationPre) -ceq (Get-IdentitySha256 $candidateBuildAttestationPost) -and
    (Get-IdentitySha256 $baselinePerformanceIdentityPre) -ceq (Get-IdentitySha256 $baselinePerformanceIdentityPost) -and
    (Get-IdentitySha256 $candidatePerformanceIdentityPre) -ceq (Get-IdentitySha256 $candidatePerformanceIdentityPost) -and
    $scriptHashBefore -ceq $scriptHashAfter -and $manifestHashBefore -ceq $manifestHashAfter

$timingAnalysis = $null
$counterSummary = @()
if ($script:uiaListeningObservations.Count -ne 672 -or @($script:uiaListeningObservations | ForEach-Object { $_['value'] } | Sort-Object -Unique).Count -ne 1) {
    $uiaListeningStable = $false
} else { $uiaListeningStable = $true }
if ($null -eq $failure -and $runRecords.Count -eq 48 -and $identityStable -and $uiaListeningStable) {
    try {
        $timingAnalysis = Get-TimingAnalysis -ScheduleBlocks $schedule.Blocks -ProcessResults $processResults
        $counterSummary = Get-CounterSummary -RunRecords $runRecords.ToArray()
    } catch { $failure = $_.Exception.Message }
}
$status = if ($failure) { 'incomplete-no-conclusion' } elseif (-not $identityStable) { 'identity-drift-inconclusive' } elseif (-not $uiaListeningStable) { 'uia-listening-state-varied-inconclusive' } elseif ($runRecords.Count -ne 48) { 'incomplete-no-conclusion' } else { 'complete-explanatory-only' }

$allLogs = @(
    foreach ($step in $schedule.Steps) {
        $logName = '{0:D2}-{1}-{2}.log' -f [int]$step.Position, [string]$step.Name, ([string]$step.Block)
        $logPath = Join-Path $runRoot $logName
        if (Test-Path -LiteralPath $logPath -PathType Leaf) {
            [ordered]@{ path=$logName; sha256=(Get-FileSha256 $logPath); side=$step.Side; block=$step.Block; run=$step.Name }
        }
    }
)
$report = [ordered]@{
    schemaVersion=1; artifactType='accessibility-scheduling-explanatory-study'; workloadOwner='DxUi'; fixture=[string]$setup.fixture
    benchmarkInputs=$setup.benchmarkInputs; runId=$runId; status=$status; qualificationVerdict='none'; policyQualification=$false
    exactInvocation=[ordered]@{ suite='Accessibility'; test=$script:TestName; noActivate=$true; testTimeoutSeconds=300; processesAreSerial=$true }
    runtimeResolutionSemantics='Imported PE names are searched on disk and are not proof of modules loaded by the process; PerformanceToolIdentity records the resolved build and dependency closure.'
    baselineOnlyAdapterInputs=@($setup.overlayInputs | Where-Object { $_.origin -like 'baseline-only*' })
    schedule=$schedule; rawLogCount=$allLogs.Count; rawLogs=$allLogs; processRuns=$runRecords.ToArray()
    baselinePre=$baselinePre; baselinePost=$baselinePost; candidatePre=$candidatePre; candidatePost=$candidatePost
    compiledClosurePre=$compiledClosurePre; compiledClosurePost=$compiledClosurePost
    baselineBuildAttestationPre=$baselineBuildAttestationPre; baselineBuildAttestationPost=$baselineBuildAttestationPost
    candidateBuildAttestationPre=$candidateBuildAttestationPre; candidateBuildAttestationPost=$candidateBuildAttestationPost
    baselinePerformanceToolIdentityPre=$baselinePerformanceIdentityPre; baselinePerformanceToolIdentityPost=$baselinePerformanceIdentityPost
    candidatePerformanceToolIdentityPre=$candidatePerformanceIdentityPre; candidatePerformanceToolIdentityPost=$candidatePerformanceIdentityPost
    prePostIdentityStable=[bool]$identityStable; postIdentityError=$postIdentityError
    activeInstrumentationPre=$activeInstrumentationPre; activeInstrumentationPost=$activeInstrumentationPost
    uiaListeningStable=[bool]$uiaListeningStable; uiaListeningObservations=$script:uiaListeningObservations.ToArray()
    studyScriptSha256Before=$scriptHashBefore; studyScriptSha256After=$scriptHashAfter
    setupManifestPath=$SetupManifest; setupManifestSha256Before=$manifestHashBefore; setupManifestSha256After=$manifestHashAfter
    timingAnalysis=$timingAnalysis; deterministicSnapshotCounters=$counterSummary
    errors=$(if ($failure) { @($failure) } else { @() }); conclusion='Descriptive paired study only; it does not qualify a release, alter policy, or authorize a rebaseline.'
}
$reportPath = Join-Path $runRoot 'study-result.json'
$json = ConvertTo-Json -InputObject $report -Depth 16
[IO.File]::WriteAllText($reportPath, $json + "`n", [Text.UTF8Encoding]::new($false))
Write-Host "Study result: $reportPath"
Write-Host "Study status: $status; raw logs: $($allLogs.Count); seed: $Seed"
if ($failure) { throw "The paired study stopped without a conclusion: $failure. Partial raw evidence is retained in $runRoot" }
if (-not $identityStable) { throw "The paired study identities changed or became unavailable. No conclusion was produced. See $reportPath" }
if (-not $uiaListeningStable) { throw "UIA listening state varied or was not observed consistently. No conclusion was produced. See $reportPath" }
