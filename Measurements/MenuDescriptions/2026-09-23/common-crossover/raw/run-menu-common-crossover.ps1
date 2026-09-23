# Draft only. Root owns native execution after restoring the saved uninstrumented headers.
# One x64 Release rebuild per variant; exactly four ordinary benchmark invocations.
[CmdletBinding()]
param(
    [string] $Evidence = 'C:/RedSalamander.Perf/evidence/i26-ui/menu-common-crossover-20260923',
    [string] $BaselineRepo = 'Z:/src/DxUi-worktrees/i26-menu-intern-baseline',
    [string] $CandidateRepo = 'Z:/src/DxUi-worktrees/i26-menu-description',
    [string] $PythonExe = 'C:/Users/eric/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repos = @{ A = [IO.Path]::GetFullPath($BaselineRepo); B = [IO.Path]::GetFullPath($CandidateRepo) }
$inputPaths = @(
    'performance.ps1',
    'Tools/compare_performance.py',
    'Tools/Invoke-Python.ps1',
    'Tests/Embedded/EmbeddedTests.cpp',
    'Tests/Embedded/BenchmarkMain.h',
    'Tests/Embedded/ComplexUiBenchmark.h',
    'Samples/ComplexUi/ComplexUiScene.h',
    'Samples/EmbeddedControls/GraphicsFixture.h'
)
$restoredHashes = @{
    'Tests/Embedded/BenchmarkMain.h' = '7A418A55E91F5C61A1CFD59EDB04C88EF800EFC5BE4AF0B210D2C345250716C9'
    'Tests/Embedded/ComplexUiBenchmark.h' = '3E374D30FF3D475B3197C32DCFE2DD339F01FD8A8172ABB598C39D1450EBCA6C'
}

function Assert([bool] $Condition, [string] $Message) {
    if (-not $Condition) { throw $Message }
}
function Hash([string] $Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant() }
function Save-Json([string] $Path, $Value) {
    $Value | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $Path -Encoding utf8
}
function Read-Json([string] $Path) { Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json -AsHashtable }
function Input-Manifest([string] $Repo) {
    @($inputPaths | ForEach-Object {
        $path = Join-Path $Repo $_
        Assert (Test-Path -LiteralPath $path -PathType Leaf) "Missing fixture input: $path"
        [ordered]@{ path = $_; sha256 = Hash $path }
    })
}
function Library-Manifest([string] $Repo) {
    Push-Location -LiteralPath $Repo
    try {
        $files = @(& git ls-files --cached --others --exclude-standard -- src include Build Directory.Build.props Directory.Build.targets vcpkg.json vcpkg-tool.json)
        Assert ($LASTEXITCODE -eq 0) "Cannot enumerate library inputs in $Repo"
        @($files | Sort-Object -Unique | ForEach-Object { [ordered]@{ path = $_; sha256 = Hash (Join-Path $Repo $_) } })
    } finally { Pop-Location }
}
function Manifest-Fingerprint($Items) {
    $lines = @($Items | ForEach-Object { "$($_.path) $($_.sha256)" })
    $sha = [Security.Cryptography.SHA256]::Create()
    try { [Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($lines -join "`n")))) }
    finally { $sha.Dispose() }
}
function Git-Receipt([string] $Repo, [string] $Folder) {
    Push-Location -LiteralPath $Repo
    try {
        $head = (& git rev-parse HEAD).Trim()
        Assert ($LASTEXITCODE -eq 0) "Cannot read HEAD in $Repo"
        $head | Set-Content -LiteralPath (Join-Path $Folder 'head.txt') -Encoding utf8
        $status = @(& git status --porcelain=v1)
        Assert ($LASTEXITCODE -eq 0) "Cannot read status in $Repo"
        ($status -join "`n") | Set-Content -LiteralPath (Join-Path $Folder 'status.txt') -Encoding utf8
        $sourceDiff = @(& git diff --binary -- src include Build Directory.Build.props Directory.Build.targets vcpkg.json vcpkg-tool.json)
        Assert ($LASTEXITCODE -eq 0) "Cannot read library diff in $Repo"
        ($sourceDiff -join "`n") | Set-Content -LiteralPath (Join-Path $Folder 'library-diff.patch') -Encoding utf8
    } finally { Pop-Location }
}

# Predeclare the complete run and pair set before creating evidence or launching native work.
$runOrder = @('A1', 'B1', 'B2', 'A2')
$comparisonSet = [ordered]@{
    'pair1-A1-B1' = @('A1', 'B1')
    'pair2-A2-B2' = @('A2', 'B2')
    'control-A1-A2' = @('A1', 'A2')
    'control-B1-B2' = @('B1', 'B2')
}
Assert (-not (Test-Path -LiteralPath $Evidence)) "Evidence already exists: $Evidence"
Assert (Test-Path -LiteralPath $PythonExe -PathType Leaf) "Missing Python runtime: $PythonExe"
Assert ($repos.A -ne $repos.B) 'Baseline and candidate repositories must differ.'

$manifests = @{}
$libraryManifests = @{}
foreach ($variant in @('A','B')) {
    $repo = $repos[$variant]
    $manifests[$variant] = Input-Manifest $repo
    $libraryManifests[$variant] = Library-Manifest $repo
    foreach ($item in $restoredHashes.GetEnumerator()) {
        $found = Hash (Join-Path $repo $item.Key)
        Assert ($found -eq $item.Value) "Restore original $($item.Key) before running ($variant actual $found)."
    }
}
for ($index = 0; $index -lt $inputPaths.Count; ++$index) {
    Assert ($manifests.A[$index].path -eq $manifests.B[$index].path) "Input name mismatch at $index"
    Assert ($manifests.A[$index].sha256 -eq $manifests.B[$index].sha256) "Fixture input differs: $($inputPaths[$index])"
}

New-Item -ItemType Directory -Path $Evidence -ErrorAction Stop | Out-Null
$driverCopy = Join-Path $Evidence 'run-menu-common-crossover.ps1'
Copy-Item -LiteralPath $PSCommandPath -Destination $driverCopy
Save-Json (Join-Path $Evidence 'protocol.json') ([ordered]@{
    fixture = 'ordinary-complex-ui-x64-release-crossover-v1'
    declaredRunOrder = $runOrder
    declaredComparisons = $comparisonSet
    recordedUtc = [DateTime]::UtcNow.ToString('o')
    baselineRepo = $repos.A
    candidateRepo = $repos.B
    pythonExe = $PythonExe
    pythonSha256 = Hash $PythonExe
    driverSha256 = Hash $driverCopy
    note = 'One ordinary performance.ps1 -SkipBuild invocation per run; compare saved reports only after all four complete.'
})
Save-Json (Join-Path $Evidence 'benchmark-inputs-A.json') $manifests.A
Save-Json (Join-Path $Evidence 'benchmark-inputs-B.json') $manifests.B

$oldPython = $env:DXUI_PYTHON
try {
    $env:DXUI_PYTHON = $PythonExe
    foreach ($variant in @('A','B')) {
        $repo = $repos[$variant]
        $folder = Join-Path $Evidence $variant
        New-Item -ItemType Directory -Path $folder | Out-Null
        Git-Receipt $repo $folder
        Save-Json (Join-Path $folder 'library-source-hashes.json') $libraryManifests[$variant]
        foreach ($item in $manifests[$variant]) {
            $copy = Join-Path (Join-Path $folder 'inputs') $item.path
            New-Item -ItemType Directory -Path (Split-Path $copy) -Force | Out-Null
            Copy-Item -LiteralPath (Join-Path $repo $item.path) -Destination $copy
            Assert ((Hash $copy) -eq $item.sha256) "Copied input changed for $variant/$($item.path)"
        }
        $logsFolder = Join-Path $repo '.build/logs'
        $beforeNames = if (Test-Path -LiteralPath $logsFolder -PathType Container) {
            @(Get-ChildItem -LiteralPath $logsFolder -Filter 'build-x64-Release-*.log' -File | ForEach-Object Name)
        } else { @() }
        $buildStarted = [DateTime]::UtcNow
        $buildConsole = Join-Path $folder 'build-console.log'
        $buildExit = 1
        try {
            & (Join-Path $repo 'build.ps1') -Configuration Release -Platform x64 -Rebuild *> $buildConsole
            Assert $? "Build script failed for $variant"
            $buildExit = 0
        } catch {
            ($_ | Out-String) | Add-Content -LiteralPath $buildConsole
            '1' | Set-Content -LiteralPath (Join-Path $folder 'build-exit.txt')
            throw
        }
        $buildExit.ToString() | Set-Content -LiteralPath (Join-Path $folder 'build-exit.txt')
        $newLogs = @(Get-ChildItem -LiteralPath $logsFolder -Filter 'build-x64-Release-*.log' -File | Where-Object { $_.Name -notin $beforeNames })
        Assert ($newLogs.Count -eq 1) "Expected one fresh MSBuild log for $variant, found $($newLogs.Count)"
        $msbuildLog = Join-Path $folder 'build-msbuild.log'
        Copy-Item -LiteralPath $newLogs[0].FullName -Destination $msbuildLog
        $msbuildText = Get-Content -Raw -LiteralPath $msbuildLog
        Assert ($msbuildText -match '(?m)CL\.exe[^\r\n]*EmbeddedTests\.cpp') "Rebuild did not prove EmbeddedTests.cpp compilation for $variant"
        $exe = Join-Path $repo '.build/x64/Release/DxUi.EmbeddedTests.exe'
        $lib = Join-Path $repo '.build/x64/Release/DxUi.lib'
        Assert ((Test-Path -LiteralPath $exe -PathType Leaf) -and (Test-Path -LiteralPath $lib -PathType Leaf)) "Missing Release outputs for $variant"
        $buildReceipt = [ordered]@{
            variant = $variant
            startedUtc = $buildStarted.ToString('o')
            completedUtc = [DateTime]::UtcNow.ToString('o')
            rebuiltEmbeddedTestsTranslationUnit = $true
            buildConsoleSha256 = Hash $buildConsole
            msbuildLogOriginalName = $newLogs[0].Name
            msbuildLogSha256 = Hash $msbuildLog
            executableSha256 = Hash $exe
            librarySha256 = Hash $lib
            librarySourceFingerprint = Manifest-Fingerprint $libraryManifests[$variant]
        }
        Save-Json (Join-Path $folder 'build-receipt.json') $buildReceipt
        foreach ($item in $manifests[$variant]) {
            Assert ((Hash (Join-Path $repo $item.path)) -eq $item.sha256) "Input changed during $variant rebuild: $($item.path)"
        }
    }

    foreach ($run in $runOrder) {
        $variant = $run.Substring(0, 1)
        $repo = $repos[$variant]
        $folder = Join-Path $Evidence $variant
        $exe = Join-Path $repo '.build/x64/Release/DxUi.EmbeddedTests.exe'
        $lib = Join-Path $repo '.build/x64/Release/DxUi.lib'
        $built = Read-Json (Join-Path $folder 'build-receipt.json')
        $beforeExe = Hash $exe
        $beforeLib = Hash $lib
        Assert (($beforeExe -eq $built.executableSha256) -and ($beforeLib -eq $built.librarySha256)) "Binary changed before $run"
        foreach ($item in $manifests[$variant]) {
            Assert ((Hash (Join-Path $repo $item.path)) -eq $item.sha256) "Input changed before ${run}: $($item.path)"
        }
        $reportPath = Join-Path $Evidence "$run.json"
        $logPath = Join-Path $Evidence "$run.log"
        $started = [DateTime]::UtcNow
        try {
            & (Join-Path $repo 'performance.ps1') -Configuration Release -Platform x64 -SkipBuild -OutputPath $reportPath *> $logPath
            Assert $? "performance.ps1 failed for $run"
            Assert (Test-Path -LiteralPath $reportPath -PathType Leaf) "Missing $run report"
            $receipt = Read-Json $reportPath
            $unpairedPath = "$reportPath.comparison.json"
            Assert (Test-Path -LiteralPath $unpairedPath -PathType Leaf) "Missing $run unpaired comparison"
            Assert ((Read-Json $unpairedPath).status -eq 'unpaired') "Unexpected $run comparison status"
            $afterExe = Hash $exe
            $afterLib = Hash $lib
            Assert (($beforeExe -eq $afterExe) -and ($beforeLib -eq $afterLib)) "Binary changed during $run"
            Assert ($receipt.executableSha256 -eq $beforeExe) "Receipt executable hash mismatch for $run"
            Assert ($receipt.sourceFingerprint -eq $built.librarySourceFingerprint) "Compiled source fingerprint changed for $run"
            Assert (($receipt.configuration -eq 'Release') -and ($receipt.platform -eq 'x64') -and $receipt.buildSkipped) "Wrong $run execution mode"
            Assert ($receipt.sourceCommit -eq (Get-Content -Raw -LiteralPath (Join-Path $folder 'head.txt')).Trim()) "Source HEAD changed for $run"
            foreach ($item in $manifests[$variant]) {
                if ($receipt.benchmarkInputs.ContainsKey($item.path)) {
                    Assert ($receipt.benchmarkInputs[$item.path] -eq $item.sha256) "Receipt input mismatch for $run/$($item.path)"
                }
            }
            Save-Json (Join-Path $Evidence "$run-invocation.json") ([ordered]@{
                run = $run; variant = $variant; startedUtc = $started.ToString('o'); completedUtc = [DateTime]::UtcNow.ToString('o')
                executableSha256Before = $beforeExe; executableSha256After = $afterExe
                librarySha256Before = $beforeLib; librarySha256After = $afterLib
                reportSha256 = Hash $reportPath; unpairedComparisonSha256 = Hash $unpairedPath
                receiptCompletedUtc = $receipt.completedUtc; sourceCommit = $receipt.sourceCommit
                sourceFingerprint = $receipt.sourceFingerprint; benchmarkSha256 = $receipt.benchmarkSha256
            })
            '0' | Set-Content -LiteralPath (Join-Path $Evidence "$run-exit.txt")
        } catch {
            ($_ | Out-String) | Add-Content -LiteralPath $logPath
            '1' | Set-Content -LiteralPath (Join-Path $Evidence "$run-exit.txt")
            throw
        }
    }

    $comparisons = [ordered]@{}
    foreach ($name in $comparisonSet.Keys) {
        $pair = $comparisonSet[$name]
        $before = Join-Path $Evidence "$($pair[0]).json"
        $after = Join-Path $Evidence "$($pair[1]).json"
        $output = Join-Path $Evidence "$name.json"
        $log = Join-Path $Evidence "$name.log"
        & $PythonExe (Join-Path $repos.B 'Tools/compare_performance.py') $after --baseline $before --output $output *> $log
        $comparisonExit = $LASTEXITCODE
        $comparisonExit.ToString() | Set-Content -LiteralPath (Join-Path $Evidence "$name-exit.txt")
        Assert (Test-Path -LiteralPath $output -PathType Leaf) "Missing comparison: $name"
        $comparison = Read-Json $output
        Assert ($comparison.status -in @('within-noise-budget','advice-required')) "Invalid comparison evidence: $name/$($comparison.status)"
        Assert (($comparisonExit -eq 0) -eq ($comparison.status -eq 'within-noise-budget')) "Comparison exit/status mismatch: $name"
        $comparisons[$name] = $comparison
    }

    # Emit observations, not an automatic acceptance decision. Improvement-sized
    # A/A or B/B changes count as control variation even if comparator status is green.
    $observations = @()
    $p1 = $comparisons['pair1-A1-B1'].changes
    foreach ($change in $p1) {
        $key = "$($change.scenario)/$($change.metric)"
        $p2 = @($comparisons['pair2-A2-B2'].changes | Where-Object { "$($_.scenario)/$($_.metric)" -eq $key })[0]
        $ca = @($comparisons['control-A1-A2'].changes | Where-Object { "$($_.scenario)/$($_.metric)" -eq $key })[0]
        $cb = @($comparisons['control-B1-B2'].changes | Where-Object { "$($_.scenario)/$($_.metric)" -eq $key })[0]
        $controlAOutsideBand = if ($null -eq $ca.changePercent) { $ca.before -ne $ca.after } else { [Math]::Abs($ca.changePercent) -gt $ca.noisePercent }
        $controlBOutsideBand = if ($null -eq $cb.changePercent) { $cb.before -ne $cb.after } else { [Math]::Abs($cb.changePercent) -gt $cb.noisePercent }
        $observations += [ordered]@{
            key = $key; noisePercent = $change.noisePercent
            pair1 = $change; pair2 = $p2
            baselineControl = $ca; candidateControl = $cb
            baselineControlOutsideBand = $controlAOutsideBand
            candidateControlOutsideBand = $controlBOutsideBand
            repeatedPairFlag = [bool]($change.regressed -and $p2.regressed)
            interpretation = if ($change.regressed -and $p2.regressed) {
                if ($controlAOutsideBand -or $controlBOutsideBand) { 'repeated-pair-flag-with-control-variation' }
                else { 'repeated-pair-flag-needs-causal-review' }
            } elseif ($change.regressed -or $p2.regressed) { 'single-pair-flag-unresolved' }
            else { 'no-threshold-flag-in-this-crossover' }
        }
    }
    Save-Json (Join-Path $Evidence 'interpretation.json') ([ordered]@{
        rule = 'Preserve every comparison. Investigation bands are not acceptance budgets. No automatic waiver or rebaseline.'
        runOrder = $runOrder; comparisons = $comparisonSet; observations = $observations
    })
    $files = @(Get-ChildItem -LiteralPath $Evidence -File -Recurse | Sort-Object FullName | ForEach-Object {
        [ordered]@{ path = $_.FullName.Substring($Evidence.Length + 1).Replace('\','/'); size = $_.Length; sha256 = Hash $_.FullName }
    })
    Save-Json (Join-Path $Evidence 'file-hashes.json') $files
    Write-Output "Completed exactly four ordinary runs: $($runOrder -join ', '). Evidence: $Evidence"
} finally {
    $env:DXUI_PYTHON = $oldPython
}
