# The gate around the hosted paired benchmark: which pull requests are measured and against what, how a set's verdict
# becomes the check's conclusion, the job summary and annotations, and the workflow's wiring. Everything runs against
# fixture repositories and synthetic summaries; no build, runner or benchmark is needed.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../PerformanceComparison.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../PairedRun.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../BenchmarkGate.psm1') -Force
Add-Type -AssemblyName System.IO.Compression.FileSystem
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

function Invoke-FixtureGit([string] $Root, [string[]] $Arguments) {
    & git -C $Root -c core.autocrlf=false -c user.name=fixture -c user.email=fixture@example.invalid -c commit.gpgsign=false @Arguments 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "git $($Arguments -join ' ') failed in $Root" }
}

function Get-FixtureHead([string] $Root, [string] $Revision = 'HEAD') {
    $output = & git -C $Root rev-parse $Revision 2>&1
    if ($LASTEXITCODE -ne 0) { throw "git rev-parse $Revision failed in $Root" }
    return ([string](@($output)[0])).Trim()
}

function Add-FixtureCommit([string] $Root, [string] $Message, [hashtable] $Files) {
    foreach ($name in $Files.Keys) { Set-FixtureFile $Root $name $Files[$name] }
    Invoke-FixtureGit $Root @('add', '-A')
    Invoke-FixtureGit $Root @('commit', '-q', '-m', $Message)
    return Get-FixtureHead $Root
}

# --- Which pull requests are measured ---------------------------------------------------------------------------------------

Invoke-TestCase 'every library, build, benchmark and measurement path is measured, with the reason that says why' {
    $cases = [ordered]@{
        'src/Controls/DxUi.Grid.cpp' = 'library input'; 'include/DxUi/DxUi.h' = 'library input'; 'Build/DxUi.Consumer.props' = 'library input'
        'Directory.Build.props' = 'library input'; 'Directory.Build.targets' = 'library input'; 'vcpkg.json' = 'library input'; 'vcpkg-tool.json' = 'library input'
        'performance.ps1' = 'benchmark harness'; 'Tools/Compare-Performance.ps1' = 'benchmark harness'; 'Tools/PerformanceComparison.psm1' = 'benchmark harness'
        'Tests/Embedded/Embedded.Tests.ComplexUiBenchmark.h' = 'benchmark harness'; 'Samples/ComplexUi/ComplexUiScene.h' = 'benchmark harness'
        'Tests/Embedded/Embedded.Tests.Embedded.cpp' = 'benchmark executable'; 'Tests/Embedded/DxUi.EmbeddedTests.vcxproj' = 'benchmark executable'; 'Tests/Support/Support.Tests.TestWatchdog.h' = 'benchmark executable'
        'Samples/EmbeddedControls/EmbeddedScene.h' = 'fixture or sample compiled into it'
        'DxUi.sln' = 'build or restore'; 'build.ps1' = 'build or restore'; 'vcpkg-install.ps1' = 'build or restore'; 'vcpkg-configuration.json' = 'build or restore'
        'Tools/VisualStudio.psm1' = 'build or restore'; 'Tools/VcpkgTriplet.psm1' = 'build or restore'
        'performance-paired.ps1' = 'paired measurement or gate'; 'Tools/PairedRun.psm1' = 'paired measurement or gate'; 'Tools/BenchmarkGate.psm1' = 'paired measurement or gate'
        'Tools/Get-BenchmarkScope.ps1' = 'paired measurement or gate'; 'Tools/Publish-BenchmarkVerdict.ps1' = 'paired measurement or gate'
        '.github/workflows/ci.yml' = 'hosted workflow'
    }
    foreach ($path in $cases.Keys) {
        $scope = Get-BenchmarkScope -ChangedPaths @($path)
        Assert-True $scope.Relevant "$path is measured"
        Assert-Equal $cases[$path] $scope.Matches[0].Reason "why $path is measured"
        Assert-Equal $path $scope.Matches[0].Path "the matched path of $path"
    }
}

Invoke-TestCase 'documentation, specifications, measurements, other tests and other tools are not measured' {
    $paths = @('README.md', 'AGENTS.md', 'CHANGELOG.md', 'Changes/2026-10-01-hosted-paired-benchmark.md', 'docs/performance.md', 'docs/gallery/light.png',
        'Specs/Core/Core_PerformanceAndResources.md', 'Specs/DesignSystem/components/Tree/preview.html', 'Specs/Plans/WIP/README.md', 'Measurements/GridSelection/2026-10-01/README.md',
        'Measurements/GridSelection/2026-10-01/summary.receipt.txt', '.agents/skills/performance-resources/SKILL.md', 'capabilities.json',
        'Tests/Controls/DxUi.Tests.Grid.cpp', 'Tests/Foundation/Foundation.Tests.Foundation.cpp', 'Tests/ConsumerModules/Consumer.cpp', 'Tools/tests/Test-PairedRun.ps1',
        'Tools/Validation.psm1', 'Tools/Changelog.psm1', 'Tools/Commit-Gallery.ps1', 'Tools/ConsumerUpdate.psm1', 'Tools/README.md', 'test.ps1', 'test-consumer.ps1',
        'validate.ps1', 'validate-specs.ps1', 'format.ps1', 'gallery.ps1', '.clang-format', '.github/workflows/format.yml', '.github/workflows/gallery.yml',
        # Markdown is never compiled, even inside a measured directory.
        'src/README.md', 'include/DxUi/NOTES.md', 'Samples/ComplexUi/README.md', 'Tests/Embedded/NOTES.md')
    $scope = Get-BenchmarkScope -ChangedPaths $paths
    Assert-True (-not $scope.Relevant) "nothing measured: $(@($scope.Matches | ForEach-Object { $_.Path }) -join ', ')"
    Assert-Equal $paths.Count $scope.Total 'every path is counted'
    Assert-Equal $paths.Count $scope.Ignored 'and ignored'
}

Invoke-TestCase 'one measured path among documentation is enough, and the paths are normalized and counted once' {
    $scope = Get-BenchmarkScope -ChangedPaths @('docs/performance.md', 'Specs\Core\x.md', '.\src\Controls\DxUi.Tree.cpp', 'src/Controls/DxUi.Tree.cpp', 'Changes/2026-10-01-x.md')
    Assert-True $scope.Relevant 'a library file among documentation'
    Assert-Equal 4 $scope.Total 'a path given twice, in two spellings, counts once'
    Assert-Equal 1 @($scope.Matches).Count 'one measured path'
    Assert-Equal 'src/Controls/DxUi.Tree.cpp' $scope.Matches[0].Path 'in git''s spelling'
    Assert-Equal 3 $scope.Ignored 'the others'
    Assert-True (-not (Get-BenchmarkScope -ChangedPaths @()).Relevant) 'an empty change measures nothing'
    Assert-True (-not (Get-BenchmarkScope -ChangedPaths @('', $null)).Relevant) 'blank names measure nothing'
    # git is case-sensitive, so a directory that merely looks like a measured one is not one.
    Assert-True (-not (Get-BenchmarkScope -ChangedPaths @('Src/Other.cpp', 'srcs/Other.cpp', 'src-notes/Other.cpp')).Relevant) 'a near miss is not a library path'
}

Invoke-TestCase 'the rules cover what a receipt hashes: the library inputs and the harness' {
    foreach ($path in @(Get-LibraryInputPaths) + @(Get-PairedHarness)) {
        $probe = if ($path -notmatch '\.[A-Za-z0-9]+$') { "$path/File.cpp" } else { $path }
        Assert-True (Get-BenchmarkScope -ChangedPaths @($probe)).Relevant "a change to $probe is measured"
    }
    foreach ($path in @(Get-BenchmarkInputPaths)) { Assert-True (Get-BenchmarkScope -ChangedPaths @($path)).Relevant "the benchmark input $path is measured" }
}

Invoke-TestCase 'every tracked library input, benchmark source and measurement script of this repository is measured' {
    $tracked = @(& git -C $repository ls-files -- src include Build Directory.Build.props Directory.Build.targets vcpkg.json vcpkg-tool.json Tests/Embedded Tests/Support Samples)
    Assert-Equal 0 $LASTEXITCODE 'git ls-files'
    Assert-True ($tracked.Count -gt 20) 'the repository lists its sources'
    foreach ($path in $tracked) {
        if ($path.EndsWith('.md', [StringComparison]::Ordinal)) { continue }
        Assert-True (Get-BenchmarkScope -ChangedPaths @($path)).Relevant "$path is measured"
    }
    foreach ($path in @('performance.ps1', 'performance-paired.ps1', 'build.ps1', 'vcpkg-install.ps1', '.github/workflows/ci.yml', 'Tools/BenchmarkGate.psm1', 'Tools/Get-BenchmarkScope.ps1', 'Tools/Publish-BenchmarkVerdict.ps1')) {
        Assert-True (Test-Path -LiteralPath (Join-Path $repository $path) -PathType Leaf) "$path exists"
        Assert-True (Get-BenchmarkScope -ChangedPaths @($path)).Relevant "$path is measured"
    }
}

function Get-LocalIncludes([string] $Root, [string] $File) {
    # The quoted includes of one source file that name a file of this repository, as repository-relative paths.
    $directory = Split-Path -Parent (Join-Path $Root $File)
    foreach ($match in [regex]::Matches([IO.File]::ReadAllText((Join-Path $Root $File)), '(?m)^\s*#\s*include\s+"([^"]+)"')) {
        $full = [IO.Path]::GetFullPath((Join-Path $directory $match.Groups[1].Value))
        if (Test-Path -LiteralPath $full -PathType Leaf) { [IO.Path]::GetRelativePath($Root, $full).Replace('\', '/') }
    }
}

Invoke-TestCase 'everything the benchmark executable includes is measured, so a new header cannot escape the rules' {
    $pending = [Collections.Generic.Queue[string]]::new()
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $pending.Enqueue('Tests/Embedded/Embedded.Tests.Embedded.cpp')
    [void]$seen.Add('Tests/Embedded/Embedded.Tests.Embedded.cpp')
    while ($pending.Count) {
        $file = $pending.Dequeue()
        Assert-True (Get-BenchmarkScope -ChangedPaths @($file)).Relevant "$file is compiled into the benchmark executable and must be measured"
        foreach ($include in @(Get-LocalIncludes $repository $file)) { if ($seen.Add($include)) { $pending.Enqueue($include) } }
    }
    Assert-True ($seen.Count -gt 8) "the include walk found the executable's sources ($($seen.Count))"
    Assert-True $seen.Contains('Samples/ComplexUi/ComplexUiScene.h') 'it reaches the shared scene'
    Assert-True $seen.Contains('src/Support/PostedPayload.h') 'and a library header'
}

Invoke-TestCase 'every module the build and measurement scripts import is measured' {
    $pending = [Collections.Generic.Queue[string]]::new()
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($script in @('performance.ps1', 'performance-paired.ps1', 'build.ps1', 'vcpkg-install.ps1', 'Tools/Get-BenchmarkScope.ps1', 'Tools/Publish-BenchmarkVerdict.ps1')) { $pending.Enqueue($script); [void]$seen.Add($script) }
    while ($pending.Count) {
        $file = $pending.Dequeue()
        Assert-True (Get-BenchmarkScope -ChangedPaths @($file)).Relevant "$file runs in a paired benchmark and must be measured"
        $text = [IO.File]::ReadAllText((Join-Path $repository $file))
        foreach ($match in [regex]::Matches($text, 'Import-Module \(Join-Path \$\w+ ''([^'']+\.psm1)''\)')) {
            $name = $match.Groups[1].Value
            $beside = [IO.Path]::GetRelativePath($repository, [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent (Join-Path $repository $file)) $name))).Replace('\', '/')
            $import = if (Test-Path -LiteralPath (Join-Path $repository $beside) -PathType Leaf) { $beside } else { $name }
            Assert-True (Test-Path -LiteralPath (Join-Path $repository $import) -PathType Leaf) "$file imports $name"
            if ($seen.Add($import)) { $pending.Enqueue($import) }
        }
    }
    Assert-True ($seen.Contains('Tools/PerformanceComparison.psm1') -and $seen.Contains('Tools/PairedRun.psm1') -and $seen.Contains('Tools/VcpkgTriplet.psm1')) 'the walk reaches the comparator, the schedule and the restore'
}

Invoke-FixtureCase 'the baseline is the merge commit''s first parent, so only this pull request is compared' {
    param($root)
    Invoke-FixtureGit $root @('init', '-q', '-b', 'main')
    [void](Add-FixtureCommit $root 'initial' @{ 'README.md' = "one`n"; 'src/Base.cpp' = "int base;`n" })
    $fork = Add-FixtureCommit $root 'fork point' @{ 'docs/fork.md' = "fork`n" }
    Invoke-FixtureGit $root @('checkout', '-q', '-b', 'feature')
    [void](Add-FixtureCommit $root 'the pull request' @{ 'src/Controls/Feature.cpp' = "int feature;`n"; 'docs/feature.md' = "doc`n" })
    Invoke-FixtureGit $root @('checkout', '-q', 'main')
    # The base moved after the branch was cut, by a change of its own.
    $moved = Add-FixtureCommit $root 'main moves on' @{ 'src/Controls/Other.cpp' = "int other;`n" }
    # GitHub's merge ref: the base's tip merged with the pull request's head.
    Invoke-FixtureGit $root @('checkout', '-q', '--detach', 'main')
    Invoke-FixtureGit $root @('merge', '-q', '--no-ff', '-m', 'merge ref', 'feature')
    $merge = Get-FixtureHead $root
    $pair = Resolve-PullRequestPair -Repository $root -BaseRef main
    Assert-Equal $merge $pair.Candidate 'the candidate is the checkout, the merge ref'
    Assert-Equal $moved $pair.Baseline 'the baseline is the base as the merge ref was made, not the older fork point'
    Assert-True ($pair.Baseline -cne $fork) 'it is not the merge base with the pull request''s head'
    Assert-True $pair.Method.Contains('first parent') 'the method says so'
    $paths = @(Get-PullRequestChangedPaths -Repository $root -Baseline $pair.Baseline -Candidate $pair.Candidate)
    Assert-Equal 'docs/feature.md src/Controls/Feature.cpp' (($paths | Sort-Object) -join ' ') 'what the base gained meanwhile is not this pull request''s change'
    Assert-True (Get-BenchmarkScope -ChangedPaths $paths).Relevant 'and it changes a library file'
}

Invoke-FixtureCase 'a checkout that is not a merge commit falls back to the merge base with the base branch' {
    param($root)
    Invoke-FixtureGit $root @('init', '-q', '-b', 'main')
    [void](Add-FixtureCommit $root 'initial' @{ 'README.md' = "one`n" })
    $fork = Add-FixtureCommit $root 'fork point' @{ 'docs/fork.md' = "fork`n" }
    Invoke-FixtureGit $root @('checkout', '-q', '-b', 'feature')
    $head = Add-FixtureCommit $root 'the branch' @{ 'src/Feature.cpp' = "int feature;`n" }
    Invoke-FixtureGit $root @('checkout', '-q', 'main')
    $moved = Add-FixtureCommit $root 'main moves on' @{ 'docs/moved.md' = "moved`n" }
    Invoke-FixtureGit $root @('update-ref', 'refs/remotes/origin/main', $moved)
    Invoke-FixtureGit $root @('checkout', '-q', 'feature')
    $pair = Resolve-PullRequestPair -Repository $root -BaseRef main
    Assert-Equal $head $pair.Candidate 'the candidate is the checkout'
    Assert-Equal $fork $pair.Baseline 'the merge base with origin/main'
    Assert-True $pair.Method.Contains('merge base with origin/main') 'the method says so'
    Assert-Throws { Resolve-PullRequestPair -Repository $root -BaseRef nothing } 'a base branch that is not there'
    Assert-Throws { Resolve-PullRequestPair -Repository $root -Candidate 'no-such-revision' -BaseRef main } 'a candidate that is not there'
}

Invoke-FixtureCase 'a file moved out of a measured directory is still a measured change' {
    param($root)
    Invoke-FixtureGit $root @('init', '-q', '-b', 'main')
    $base = Add-FixtureCommit $root 'initial' @{ 'src/Controls/Moved.cpp' = "int moved = 1; // a body long enough for git to see the same file`nint more = 2;`nint again = 3;`n"; 'Specs/Keep.md' = "keep`n" }
    Invoke-FixtureGit $root @('mv', 'src/Controls/Moved.cpp', 'Specs/Moved.txt')
    Invoke-FixtureGit $root @('commit', '-q', '-m', 'move it out of the library')
    $paths = @(Get-PullRequestChangedPaths -Repository $root -Baseline $base -Candidate (Get-FixtureHead $root))
    Assert-Contains $paths 'src/Controls/Moved.cpp' 'the old name is listed, as a rename would hide it'
    Assert-Contains $paths 'Specs/Moved.txt' 'and the new one'
    Assert-True (Get-BenchmarkScope -ChangedPaths $paths).Relevant 'removing a library file is a library change'
}

Invoke-FixtureCase 'a path with a space is named as it is' {
    param($root)
    Invoke-FixtureGit $root @('init', '-q', '-b', 'main')
    $base = Add-FixtureCommit $root 'initial' @{ 'README.md' = "one`n" }
    [void](Add-FixtureCommit $root 'new files' @{ 'src/Controls/Two Words.cpp' = "int a;`n"; 'docs/a file.txt' = "b`n" })
    $paths = @(Get-PullRequestChangedPaths -Repository $root -Baseline $base -Candidate (Get-FixtureHead $root))
    Assert-Contains $paths 'src/Controls/Two Words.cpp' 'the library file'
    Assert-Contains $paths 'docs/a file.txt' 'and the other'
    Assert-True (Get-BenchmarkScope -ChangedPaths $paths).Relevant 'a library file with a space is a library file'
}

# --- Verdicts and controls --------------------------------------------------------------------------------------------------

function New-SetMetric {
    param([string] $Phase, [string] $Metric, [string] $Verdict = 'within-noise', [switch] $Exact, [double] $Noise = 5.0, [double] $Before = 100.0, [double] $After = 100.0, [double] $P = 0.0152)
    return [ordered]@{
        phase = $Phase; metric = $Metric; direction = 'lower'; exact = [bool]$Exact; baselineMedian = $Before; candidateMedian = $After
        changePercent = $(if ($Before -ne 0) { ($After / $Before - 1.0) * 100.0 } else { $null }); noisePercent = $(if ($Exact) { 0 } else { $Noise })
        pValue = $P; u = 1.0; rankDirection = 'higher'; baselineSpreadPercent = 3.0; candidateSpreadPercent = 4.0; verdict = $Verdict
        baselineRuns = @($Before); candidateRuns = @($After)
    }
}

function New-ControlChange([string] $Phase, [string] $Metric, [double] $Before, [double] $After, [double] $Noise = 5.0, [switch] $Exact) {
    return [ordered]@{
        scenario = $Phase; metric = $Metric; before = $Before; after = $After
        changePercent = $(if ($Before -ne 0) { ($After / $Before - 1.0) * 100.0 } else { $null }); noisePercent = $(if ($Exact) { 0 } else { $Noise }); regressed = $false
    }
}

function New-HeldControls {
    <# Four metrics a control leaves inside their bands: a timing, a memory, a rate and an exact count. #>
    return @(
        (New-ControlChange 'dirty' 'composeCpuP95Ms' 0.0160 0.0165), (New-ControlChange 'dirty' 'privateBytes' 26000000 26200000 2.0)
        (New-ControlChange 'clean' 'fps' 1100.0 1120.0), (New-ControlChange 'dirty' 'cppAllocations' 2160 2160 -Exact))
}

# One control comparison: its changes, or $null for one whose file is missing.
function New-Control([AllowNull()][object[]] $Changes) { return [pscustomobject]@{ Changes = $Changes } }

function Get-HeldControl { return New-Control (New-HeldControls) }

function New-Scenario {
    <# A scenario of a retained summary and its control comparison files under $Directory. #>
    param([string] $Name = 'Default', [Parameter(Mandatory)][string] $Directory, [Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Metrics, [Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Controls,
        [string] $Status = '', [double] $MinimumP = 0.0022, [int] $Runs = 6, [string] $Error = '')
    $comparisons = [Collections.Generic.List[object]]::new()
    $index = 0
    foreach ($control in $Controls) {
        $index++
        # Variable names are not case-sensitive: $name would be the parameter.
        $controlName = "$Name-C$index-control"
        $file = "$controlName.comparison.json"
        $unstable = $false
        if ($null -ne $control.Changes) {
            $unstable = @($control.Changes | Where-Object { if ($null -eq $_.changePercent) { $_.before -ne $_.after } else { [Math]::Abs($_.changePercent) -gt $_.noisePercent } }).Count -gt 0
            Set-FixtureFile $Directory $file (ConvertTo-Json -InputObject ([ordered]@{ status = 'within-noise-budget'; changes = @($control.Changes) }) -Depth 6)
        }
        $comparisons.Add([ordered]@{ name = $controlName; status = $(if ($unstable) { 'unstable-control' } else { 'stable-control' }); file = $file; drift = @() })
    }
    # The crossings are in a summary too; the gate reads only the controls.
    $comparisons.Add([ordered]@{ name = "$Name-B1-vs-A1"; status = 'advice-required'; file = "$Name-B1-vs-A1.comparison.json"; drift = @() })
    $regressed = @($Metrics | Where-Object { $_.verdict -eq 'regressed' }).Count
    if (-not $Status) { $Status = if ($regressed) { 'advice-required' } else { 'within-noise-budget' } }
    $set = if ($Status -eq 'invalid-evidence') { [ordered]@{ status = 'invalid-evidence'; error = $Error; metrics = @() } }
    else {
        [ordered]@{ status = $Status; significance = 0.05; baselineRuns = $Runs; candidateRuns = $Runs; minimumAttainableP = $MinimumP
            regressedMetrics = $regressed; improvedMetrics = @($Metrics | Where-Object { $_.verdict -eq 'improved' }).Count; metrics = @($Metrics) }
    }
    return [ordered]@{ scenario = $Name; runs = [ordered]@{}; comparisons = $comparisons.ToArray(); set = $set }
}

function New-Summary([object[]] $Scenarios, [string] $BaselineFingerprint = 'AAAAAAAAAAAAAAAAAAAA', [string] $CandidateFingerprint = 'BBBBBBBBBBBBBBBBBBBB') {
    return [ordered]@{
        command = 'performance-paired.ps1'
        baseline = [ordered]@{ kind = 'revision'; revision = '1111111111111111111111111111111111111111'; path = 'b'; commit = '1111111111111111111111111111111111111111'; sourceFingerprint = $BaselineFingerprint }
        candidate = [ordered]@{ kind = 'checkout'; revision = 'HEAD'; path = 'c'; commit = '2222222222222222222222222222222222222222'; sourceFingerprint = $CandidateFingerprint }
        configuration = 'Release'; platform = 'x64'; machine = 'runnervm-test'; repetitions = 3; order = 'A1, B1, B2, A2'; scenarios = @($Scenarios)
    }
}

$regressedComposeCpu = { New-SetMetric 'dirty' 'composeCpuP95Ms' 'regressed' -Before 0.016 -After 0.018 -P 0.024 }

function Get-Conclusion([object] $Summary, [string] $Directory, [switch] $Strict) {
    return Get-BenchmarkConclusion -Summary $Summary -ReportsDirectory $Directory -StrictControls:$Strict
}

Invoke-FixtureCase 'a regressed metric whose same-binary controls stayed inside its band is a confirmed degradation' {
    param($root)
    $scenario = New-Scenario -Directory $root -Controls @((Get-HeldControl), (Get-HeldControl), (Get-HeldControl), (Get-HeldControl)) -Metrics @((& $regressedComposeCpu), (New-SetMetric 'clean' 'fps'))
    $conclusion = Get-Conclusion (New-Summary @($scenario)) $root
    Assert-Equal 'degraded' $conclusion.Conclusion 'the run'
    $metric = $conclusion.Scenarios[0].Metrics | Where-Object { $_.Verdict -eq 'regressed' }
    Assert-Equal 'confirmed' $metric.Outcome 'the metric'
    Assert-True $metric.Held 'its controls held'
    Assert-True (-not $metric.ControlDrifted) 'none drifted'
    Assert-Equal 4 $conclusion.Scenarios[0].Controls.Total 'the controls found'
    Assert-Equal 0 $conclusion.Scenarios[0].Controls.Unstable 'none unstable'
}

Invoke-FixtureCase 'a regressed metric whose controls drifted beyond its band is inconclusive, listed and never dismissed' {
    param($root)
    # The case that prompted the gate: composeCpuP95Ms 0.016 to 0.018 (+12.5% here, +17% in the retained run) while its controls moved by as much.
    $drifted = @((New-ControlChange 'dirty' 'composeCpuP95Ms' 0.0160 0.0184), (New-ControlChange 'dirty' 'privateBytes' 26000000 26200000 2.0), (New-ControlChange 'clean' 'fps' 1100.0 1120.0), (New-ControlChange 'dirty' 'cppAllocations' 2160 2160 -Exact))
    $scenario = New-Scenario -Directory $root -Controls @((Get-HeldControl), (New-Control $drifted), (Get-HeldControl), (Get-HeldControl)) -Metrics @((& $regressedComposeCpu), (New-SetMetric 'clean' 'fps'))
    $conclusion = Get-Conclusion (New-Summary @($scenario)) $root
    Assert-Equal 'inconclusive' $conclusion.Conclusion 'the run'
    Assert-Equal 'inconclusive' $conclusion.Scenarios[0].Conclusion 'the scenario'
    $metric = $conclusion.Scenarios[0].Metrics | Where-Object { $_.Verdict -eq 'regressed' }
    Assert-Equal 'unconfirmed' $metric.Outcome 'the flagged metric stays flagged'
    Assert-True $metric.ControlDrifted 'its control drifted'
    Assert-True ($metric.ControlDriftPercent -gt 14.9 -and $metric.ControlDriftPercent -lt 15.1) "by the drift the controls saw ($($metric.ControlDriftPercent))"
    Assert-Equal 1 $conclusion.Scenarios[0].Controls.Unstable 'the unstable control is counted'
    Assert-True ($conclusion.Scenarios[0].Notes -join ' ').Contains('cannot be told from machine noise') 'the note says why'
}

Invoke-FixtureCase 'the controls of the flagged metric decide, not those of the twenty-six together' {
    param($root)
    # Working set and a p95 of a few microseconds drift in every control on a shared runner; the flagged fps held.
    $noisy = New-Control @((New-ControlChange 'dirty' 'composeCpuP95Ms' 0.0160 0.0165), (New-ControlChange 'dirty' 'privateBytes' 26000000 27500000 2.0), (New-ControlChange 'clean' 'fps' 1100.0 1120.0), (New-ControlChange 'dirty' 'cppAllocations' 2160 2160 -Exact))
    $quietTiming = & $regressedComposeCpu
    $quietTiming['verdict'] = 'within-noise'
    $metrics = @((New-SetMetric 'clean' 'fps' 'regressed' -Before 1100.0 -After 900.0), (New-SetMetric 'dirty' 'privateBytes' 'within-noise' -Noise 2.0), $quietTiming)
    $scenario = New-Scenario -Directory $root -Controls @($noisy, $noisy, $noisy, $noisy) -Metrics $metrics
    $conclusion = Get-Conclusion (New-Summary @($scenario)) $root
    Assert-Equal 4 $conclusion.Scenarios[0].Controls.Unstable 'every control is unstable by the scenario-wide reading'
    Assert-Equal 'degraded' $conclusion.Conclusion 'the held metric is a confirmed degradation all the same'
    Assert-Equal 2 $conclusion.Scenarios[0].Held 'two of three metrics are held by their controls'
    $strict = Get-Conclusion (New-Summary @($scenario)) $root -Strict
    Assert-Equal 'inconclusive' $strict.Conclusion 'the strict reading calls it inconclusive'
    Assert-True ($strict.Scenarios[0].Notes -join ' ').Contains('Strict controls') 'and says so'
}

Invoke-FixtureCase 'no regressed metric is a pass, with the metrics the controls could not hold counted, not hidden' {
    param($root)
    $noisy = New-Control @((New-ControlChange 'dirty' 'composeCpuP95Ms' 0.0160 0.0200), (New-ControlChange 'dirty' 'privateBytes' 26000000 26200000 2.0), (New-ControlChange 'clean' 'fps' 1100.0 1120.0), (New-ControlChange 'dirty' 'cppAllocations' 2160 2160 -Exact))
    $metrics = @((New-SetMetric 'clean' 'fps' 'improved' -Before 1000.0 -After 1100.0), (New-SetMetric 'dirty' 'privateBytes'), (New-SetMetric 'dirty' 'composeCpuP95Ms'), (New-SetMetric 'dirty' 'cppAllocations' -Exact))
    $summary = New-Summary @((New-Scenario -Directory $root -Controls @($noisy, (Get-HeldControl)) -Metrics $metrics))
    $conclusion = Get-Conclusion $summary $root
    Assert-Equal 'pass' $conclusion.Conclusion 'the run'
    Assert-Equal 3 $conclusion.Scenarios[0].Held 'three of four are held'
    Assert-Equal 4 $conclusion.Scenarios[0].Judged 'of four judged'
    Assert-Equal 1 $conclusion.Scenarios[0].Improved 'an improvement is counted'
    Assert-Equal 0 $conclusion.Scenarios[0].Regressed 'nothing regressed'
    Assert-Equal 'inconclusive' (Get-Conclusion $summary $root -Strict).Conclusion 'unstable controls do not read as a pass under the strict reading'
}

Invoke-FixtureCase 'an exact budget is confirmed only when its same-binary controls are exactly equal' {
    param($root)
    $exact = New-SetMetric 'dirty' 'cppAllocations' 'regressed' -Exact -Before 2160 -After 2200 -P 0.0022
    $held = New-Scenario -Directory $root -Controls @((Get-HeldControl), (Get-HeldControl)) -Metrics @($exact)
    Assert-Equal 'degraded' (Get-Conclusion (New-Summary @($held)) $root).Conclusion 'a deterministic budget that rose, in a quiet set'
    $moved = New-Control @((New-ControlChange 'dirty' 'composeCpuP95Ms' 0.0160 0.0165), (New-ControlChange 'dirty' 'cppAllocations' 2160 2161 -Exact))
    $other = New-Scenario -Directory $root -Controls @((Get-HeldControl), $moved) -Metrics @($exact)
    $conclusion = Get-Conclusion (New-Summary @($other)) $root
    Assert-Equal 'inconclusive' $conclusion.Conclusion 'the same binary counted differently, so the count is not deterministic here'
    Assert-True $conclusion.Scenarios[0].Metrics[0].ControlDrifted 'an exact control drifts at any change'
}

Invoke-FixtureCase 'a confirmed degradation is not softened by another metric whose controls drifted' {
    param($root)
    # Two regressed metrics in one scenario: fps held by its controls, composeCpuP95Ms not.
    $drifting = New-Control @((New-ControlChange 'dirty' 'composeCpuP95Ms' 0.0160 0.0200), (New-ControlChange 'clean' 'fps' 1100.0 1120.0))
    $metrics = @((& $regressedComposeCpu), (New-SetMetric 'clean' 'fps' 'regressed' -Before 1100.0 -After 900.0))
    $conclusion = Get-Conclusion (New-Summary @((New-Scenario -Directory $root -Controls @($drifting, (Get-HeldControl)) -Metrics $metrics))) $root
    Assert-Equal 'degraded' $conclusion.Conclusion 'the confirmed metric decides'
    Assert-Equal 'unconfirmed confirmed' (($conclusion.Scenarios[0].Metrics | ForEach-Object { $_.Outcome }) -join ' ') 'and the other is still listed as unconfirmed'
}

Invoke-FixtureCase 'a control that did not measure the flagged metric does not hold it' {
    param($root)
    $without = New-Control @((New-ControlChange 'clean' 'fps' 1100.0 1120.0))
    $scenario = New-Scenario -Directory $root -Controls @((Get-HeldControl), $without, (Get-HeldControl)) -Metrics @((& $regressedComposeCpu))
    $conclusion = Get-Conclusion (New-Summary @($scenario)) $root
    Assert-Equal 'inconclusive' $conclusion.Conclusion 'a metric seen by two of three controls is not held by them'
    Assert-True (-not $conclusion.Scenarios[0].Metrics[0].Held) 'not held'
    Assert-Equal 0 $conclusion.Scenarios[0].Controls.Missing 'though every control file was read'
}

Invoke-FixtureCase 'a budget is exact by its band of zero even when a summary does not say so' {
    param($root)
    $budget = New-SetMetric 'dirty' 'cppAllocations' 'regressed' -Exact -Before 2160 -After 2200 -P 0.0022
    $budget.Remove('exact')
    $scenario = New-Scenario -Directory $root -Controls @((Get-HeldControl), (Get-HeldControl)) -Metrics @($budget)
    $conclusion = Get-Conclusion (New-Summary @($scenario) -BaselineFingerprint 'CAFE' -CandidateFingerprint 'CAFE') $root
    Assert-Equal 'degraded' $conclusion.Conclusion 'a rise in it is not chance on identical library inputs'
    Assert-True $conclusion.Scenarios[0].Metrics[0].Exact 'read as exact'
}

Invoke-FixtureCase 'controls that cannot be read confirm nothing' {
    param($root)
    $flagged = @((& $regressedComposeCpu), (New-SetMetric 'clean' 'fps'))
    $missing = New-Scenario -Directory $root -Controls @((Get-HeldControl), (New-Control $null), (Get-HeldControl), (Get-HeldControl)) -Metrics $flagged
    $conclusion = Get-Conclusion (New-Summary @($missing)) $root
    Assert-Equal 'inconclusive' $conclusion.Conclusion 'one control file is missing'
    Assert-Equal 1 $conclusion.Scenarios[0].Controls.Missing 'it is counted'
    $none = New-Scenario -Directory $root -Controls @() -Metrics $flagged
    Assert-Equal 'inconclusive' (Get-Conclusion (New-Summary @($none)) $root).Conclusion 'a summary without controls confirms nothing'
    Set-FixtureFile $root 'Default-C1-control.comparison.json' '{ this is not json'
    $broken = New-Scenario -Directory $root -Controls @() -Metrics $flagged
    $broken['comparisons'] = @([ordered]@{ name = 'Default-C1-control'; status = 'stable-control'; file = 'Default-C1-control.comparison.json'; drift = @() })
    Assert-Equal 'inconclusive' (Get-Conclusion (New-Summary @($broken)) $root).Conclusion 'a control file that is not JSON is unreadable'
}

Invoke-FixtureCase 'on identical library inputs a timing or memory flag is chance and a deterministic budget still gates' {
    param($root)
    $noisy = New-Scenario -Directory $root -Controls @((Get-HeldControl), (Get-HeldControl)) -Metrics @((& $regressedComposeCpu), (New-SetMetric 'dirty' 'privateBytes' 'regressed' -Noise 2.0 -Before 26000000 -After 27000000))
    $same = Get-Conclusion (New-Summary @($noisy) -BaselineFingerprint 'CAFE' -CandidateFingerprint 'CAFE') $root
    Assert-Equal 'pass' $same.Conclusion 'the same library code measured twice cannot have regressed in time'
    Assert-True $same.LibraryUnchanged 'recorded'
    Assert-Equal 2 @($same.Scenarios[0].Metrics | Where-Object { $_.Outcome -eq 'noise' }).Count 'both flags stay listed as noise'
    Assert-Equal 2 $same.Scenarios[0].Regressed 'and still count as regressed in the set'
    Assert-True ($same.Scenarios[0].Notes -join ' ').Contains('identical') 'the note says why'
    $changed = Get-Conclusion (New-Summary @($noisy) -BaselineFingerprint 'CAFE' -CandidateFingerprint 'F00D') $root
    Assert-Equal 'degraded' $changed.Conclusion 'the same flags with changed library inputs gate'
    $budget = New-Scenario -Directory $root -Controls @((Get-HeldControl), (Get-HeldControl)) -Metrics @((New-SetMetric 'dirty' 'cppAllocations' 'regressed' -Exact -Before 2160 -After 2200 -P 0.0022))
    Assert-Equal 'degraded' (Get-Conclusion (New-Summary @($budget) -BaselineFingerprint 'CAFE' -CandidateFingerprint 'CAFE') $root).Conclusion 'a rise in a deterministic budget is judged even on identical library inputs'
    Assert-Equal 'degraded' (Get-Conclusion (New-Summary @($noisy) -BaselineFingerprint '' -CandidateFingerprint '') $root).Conclusion 'unknown fingerprints are not identical ones'
}

Invoke-FixtureCase 'a set too small to reach p below 0.05 cannot pass a timing it could not judge' {
    param($root)
    $few = New-Scenario -Directory $root -Controls @((Get-HeldControl), (Get-HeldControl)) -Metrics @((New-SetMetric 'clean' 'fps')) -MinimumP 0.3333 -Runs 2
    $conclusion = Get-Conclusion (New-Summary @($few)) $root
    Assert-Equal 'inconclusive' $conclusion.Conclusion 'two runs against two'
    Assert-True ($conclusion.Scenarios[0].Notes -join ' ').Contains('too few') 'the note says so'
    Assert-Equal 0.3333 $conclusion.Scenarios[0].MinimumAttainableP 'and the verdict file carries the smallest attainable p'
    $enough = New-Scenario -Directory $root -Controls @((Get-HeldControl), (Get-HeldControl)) -Metrics @((New-SetMetric 'clean' 'fps')) -MinimumP 0.0286 -Runs 4
    Assert-Equal 'pass' (Get-Conclusion (New-Summary @($enough)) $root).Conclusion 'four against four can reach it'
}

Invoke-FixtureCase 'invalid evidence is invalid, and the worst scenario decides the run' {
    param($root)
    $good = New-Scenario -Name 'Default' -Directory $root -Controls @((Get-HeldControl), (Get-HeldControl)) -Metrics @((New-SetMetric 'clean' 'fps'))
    $invalid = New-Scenario -Name 'MultilineGrid' -Directory $root -Controls @() -Metrics @() -Status 'invalid-evidence' -Error 'Unmatched fixture: benchmarkSha256'
    $conclusion = Get-Conclusion (New-Summary @($good, $invalid)) $root
    Assert-Equal 'invalid' $conclusion.Conclusion 'an invalid scenario'
    Assert-Equal 'pass invalid' (($conclusion.Scenarios | ForEach-Object { $_.Conclusion }) -join ' ') 'each scenario keeps its own'
    Assert-True ($conclusion.Scenarios[1].Notes -join ' ').Contains('Unmatched fixture') 'with the reason'
    $degraded = New-Scenario -Name 'MultilineGridDistinct' -Directory $root -Controls @((Get-HeldControl), (Get-HeldControl)) -Metrics @((& $regressedComposeCpu))
    $inconclusive = New-Scenario -Name 'Other' -Directory $root -Controls @() -Metrics @((& $regressedComposeCpu))
    Assert-Equal 'degraded' (Get-Conclusion (New-Summary @($good, $inconclusive, $degraded)) $root).Conclusion 'degraded outranks inconclusive and pass'
    Assert-Equal 'inconclusive' (Get-Conclusion (New-Summary @($good, $inconclusive)) $root).Conclusion 'inconclusive outranks pass'
    Assert-Equal 'pass' (Get-Conclusion (New-Summary @($good)) $root).Conclusion 'a pass is a pass'
    Assert-Equal 'invalid' (Get-Conclusion (New-Summary @($degraded, $invalid)) $root).Conclusion 'invalid outranks all'
    Assert-Equal 'invalid' (Get-Conclusion (New-Summary @()) $root).Conclusion 'a summary without scenarios proves nothing'
}

Invoke-FixtureCase 'a summary the gate cannot read is invalid, never an exception' {
    param($root)
    $empty = Get-Conclusion @{} $root
    Assert-Equal 'invalid' $empty.Conclusion 'an empty summary'
    Assert-Equal 0 @($empty.Scenarios).Count 'has no scenario to report'
    Assert-True ($empty.Notes -join ' ').Contains('lists no scenario') 'so the conclusion says why itself'
    Assert-True @(Get-BenchmarkAnnotations -Conclusion $empty)[0].StartsWith('::error ') 'an error annotation'
    Assert-True (ConvertTo-BenchmarkMarkdown -Conclusion $empty -Summary @{} -Gate).Contains('lists no scenario') 'and the job summary'
    # A scenario that throws while it is read is reported as invalid, with its reason, and does not end the run.
    $broken = @{ scenario = 'Broken'; set = @{ status = 'within-noise-budget'; metrics = @(@{ phase = 'clean'; metric = 'fps'; verdict = 'regressed'; noisePercent = 'not a number' }) } }
    $thrown = Get-Conclusion @{ scenarios = @($broken, @{ scenario = 'Next' }) } $root
    Assert-Equal 2 @($thrown.Scenarios).Count 'the scenario after the one that throws is read too'
    Assert-Equal 'Broken' $thrown.Scenarios[0].Scenario 'the broken scenario is named'
    Assert-Equal 'invalid' $thrown.Scenarios[0].Conclusion 'and invalid'
    Assert-True ($thrown.Scenarios[0].Notes -join ' ').Contains('could not be read') 'with the reason'
    $odd = Get-Conclusion @{ scenarios = @('text', 5, @{ scenario = 'Default' }, @{ scenario = 'Other'; set = 'not a set' }) } $root
    Assert-Equal 'invalid' $odd.Conclusion 'scenarios that are not scenarios, or have no set verdict'
    Assert-Equal 4 @($odd.Scenarios).Count 'every entry is reported'
    Assert-Equal 'invalid invalid invalid invalid' (($odd.Scenarios | ForEach-Object { $_.Conclusion }) -join ' ') 'each as invalid'
    Assert-True ($odd.Scenarios[2].Notes -join ' ').Contains('predates the paired sets') 'and says why'
    $markdown = ConvertTo-BenchmarkMarkdown -Conclusion $odd -Summary @{} -Gate
    Assert-True $markdown.StartsWith("## Paired benchmark: Invalid evidence`n") 'the summary says so'
    Assert-True (-not $markdown.Contains('repetitions of the interleaved pass')) 'and does not invent a method it has not got'
    # Every summary this repository retains, whatever its age or shape, gets a conclusion and a summary.
    $files = @(Get-ChildItem -LiteralPath (Join-Path $repository 'Measurements') -Recurse -File -Filter 'summary.receipt.txt')
    Assert-True ($files.Count -ge 1) "the repository retains summaries ($($files.Count)), so the loop below reads something"
    foreach ($file in $files) {
        $summary = Read-GateJson $file.FullName
        $conclusion = Get-Conclusion $summary $file.DirectoryName
        Assert-Contains @('pass', 'inconclusive', 'degraded', 'invalid') $conclusion.Conclusion "$($file.FullName) has a conclusion"
        Assert-True (ConvertTo-BenchmarkMarkdown -Conclusion $conclusion -Summary $summary -Gate).StartsWith('## Paired benchmark: ') "$($file.FullName) has a summary"
    }
}

Invoke-FixtureCase 'the conclusion reads the summaries performance-paired.ps1 retained' {
    param($root)
    # The retained set under Measurements is a real summary with its comparison files beside it.
    $reports = Join-Path $repository 'Measurements/MenuDescriptions/2026-09-30/paired-local'
    $summary = Read-GateJson (Join-Path $reports 'summary.receipt.txt')
    $conclusion = Get-Conclusion $summary $reports
    Assert-Equal 'pass' $conclusion.Conclusion 'a set the contract called within-noise-budget'
    Assert-Equal 6 $conclusion.Scenarios[0].Controls.Total 'three passes, two controls each'
    Assert-Equal 6 $conclusion.Scenarios[0].Controls.Unstable 'every control of that laptop drifted (the problem this gate exists for)'
    Assert-Equal 26 $conclusion.Scenarios[0].Judged 'two phases of thirteen metrics'
    Assert-True ($conclusion.Scenarios[0].Held -lt 26) 'some metrics were not held by their controls'
    Assert-True ($conclusion.Scenarios[0].Held -gt 0) 'and some were'
    Assert-Equal 'inconclusive' (Get-Conclusion $summary $reports -Strict).Conclusion 'under the strict reading that laptop''s set proves nothing'
}

function Expand-RetainedPacket([string] $Directory, [string] $Into) {
    # A retained packet is one archive of the files a run wrote, with its hash in SHA256SUMS. Returns the directory it extracts to.
    $sums = @([IO.File]::ReadAllLines((Join-Path $Directory 'SHA256SUMS')) | Where-Object { $_ })
    Assert-Equal 1 $sums.Count 'SHA256SUMS has one line, the archive''s'
    $hash, $name = $sums[0] -split '  ', 2
    Assert-Equal 'reports.zip' $name 'it names the archive'
    $archive = Join-Path $Directory $name
    Assert-Equal $hash (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() 'the archive is the one SHA256SUMS names'
    [IO.Compression.ZipFile]::ExtractToDirectory($archive, $Into)
    Assert-Equal 109 @(Get-ChildItem -LiteralPath $Into -File).Count 'the run''s 36 receipts and their comparator outputs, 36 comparisons, 4 passes of controls and the summary'
    Assert-True (Test-Path -LiteralPath (Join-Path $Into 'summary.receipt.txt') -PathType Leaf) 'with the summary'
    return $Into
}

Invoke-FixtureCase 'the retained hosted A/A set passes: identical library, nothing flagged, every control drifting, every exact budget held' {
    param($root)
    $reports = Expand-RetainedPacket (Join-Path $repository 'Measurements/HostedPairedGate/2026-10-01') (Join-Path $root 'reports')
    $summary = Read-GateJson (Join-Path $reports 'summary.receipt.txt')
    $conclusion = Get-Conclusion $summary $reports
    Assert-Equal 'pass' $conclusion.Conclusion 'the same code measured twice'
    Assert-True $conclusion.LibraryUnchanged 'both sides have one library fingerprint'
    Assert-Equal 'Default MultilineGrid MultilineGridDistinct' (($conclusion.Scenarios | ForEach-Object { $_.Scenario }) -join ' ') 'the three gating scenarios'
    Assert-Equal '10 10 17' (($conclusion.Scenarios | ForEach-Object { $_.Held }) -join ' ') 'the metrics every control held, as the README counts them'
    foreach ($scenario in $conclusion.Scenarios) {
        Assert-Equal 6 $scenario.Controls.Total "$($scenario.Scenario): three passes, two controls each"
        Assert-Equal 6 $scenario.Controls.Unstable "$($scenario.Scenario): every control of a hosted runner drifted"
        Assert-Equal 0 $scenario.Regressed "$($scenario.Scenario): nothing is flagged"
        $exact = @($scenario.Metrics | Where-Object { $_.Metric -in @('surfaceBytes', 'replacementPeakBytes', 'cppAllocations', 'composeAllocations') })
        Assert-Equal 8 $exact.Count "$($scenario.Scenario): the exact budgets of both phases"
        Assert-True (@($exact | Where-Object { -not $_.Held }).Count -eq 0) "$($scenario.Scenario): an exact budget never drifts, so a rise in one is always confirmed"
    }
    Assert-Equal 'inconclusive' (Get-Conclusion $summary $reports -Strict).Conclusion 'a gate that needed stable controls overall would never pass a hosted run'
}

Invoke-FixtureCase 'the second retained hosted A/A set: five flags on identical code are noise by the identical-inputs rule and inconclusive without it' {
    param($root)
    $reports = Expand-RetainedPacket (Join-Path $repository 'Measurements/HostedPairedGate/2026-10-01/aa-2') (Join-Path $root 'reports')
    $summary = Read-GateJson (Join-Path $reports 'summary.receipt.txt')
    $conclusion = Get-Conclusion $summary $reports
    Assert-Equal 'pass' $conclusion.Conclusion 'identical library inputs'
    Assert-Equal '4 1 0' (($conclusion.Scenarios | ForEach-Object { $_.Regressed }) -join ' ') 'the five flagged metrics'
    $flagged = @($conclusion.Scenarios | ForEach-Object { $_.Metrics } | Where-Object { $_.Verdict -eq 'regressed' })
    Assert-Equal 5 $flagged.Count 'five flagged metrics'
    Assert-Equal 0 @($flagged | Where-Object { $_.Outcome -ne 'noise' }).Count 'all listed as noise'
    Assert-Equal 0 @($flagged | Where-Object { -not $_.ControlDrifted }).Count 'every one with a drifted control'
    $summary['candidate']['sourceFingerprint'] = 'CHANGED'
    $changed = Get-Conclusion $summary $reports
    Assert-Equal 'inconclusive' $changed.Conclusion 'read as a change, the controls of every flagged metric drifted: nothing is confirmed, and nothing is dismissed'
    $outcomes = @($changed.Scenarios | ForEach-Object { $_.Metrics } | Where-Object { $_.Verdict -eq 'regressed' } | ForEach-Object { $_.Outcome })
    Assert-Equal 'unconfirmed unconfirmed unconfirmed unconfirmed unconfirmed' ($outcomes -join ' ') 'the five stay listed as unconfirmed'
}
# --- What a run says --------------------------------------------------------------------------------------------------------

function New-ReportScenarios([string] $Directory) {
    $held = New-Scenario -Name 'Default' -Directory $Directory -Controls @((Get-HeldControl), (Get-HeldControl)) -Metrics @((& $regressedComposeCpu), (New-SetMetric 'clean' 'fps' 'improved' -Before 1000.0 -After 1100.0), (New-SetMetric 'dirty' 'cppAllocations' -Exact -Before 2160 -After 2160))
    $drifted = New-Control @((New-ControlChange 'dirty' 'composeCpuP95Ms' 0.0160 0.0200), (New-ControlChange 'dirty' 'cppAllocations' 2160 2160 -Exact))
    $other = New-Scenario -Name 'MultilineGrid' -Directory $Directory -Controls @($drifted, $drifted) -Metrics @((& $regressedComposeCpu))
    return @($held, $other)
}

Invoke-FixtureCase 'the job summary says what the run is, what it asks and every flagged and judged metric' {
    param($root)
    $summary = New-Summary (New-ReportScenarios $root)
    $conclusion = Get-Conclusion $summary $root
    Assert-Equal 'degraded' $conclusion.Conclusion 'one scenario degraded, one inconclusive'
    $markdown = ConvertTo-BenchmarkMarkdown -Conclusion $conclusion -Summary $summary -Event 'pull request merge ref (refs/pull/46/merge)' -Pair '(first parent of the merge commit)' -Gate
    Assert-True $markdown.StartsWith("## Paired benchmark: Confirmed degradation`n") 'the headline is the conclusion'
    foreach ($text in @('Default', 'MultilineGrid', '### Scenarios', '### Flagged metrics', 'regressed, confirmed', 'regressed, controls drifted', 'improved', 'refs/pull/46/merge',
            '`111111111111`', '`222222222222`', 'runnervm-test', '3 repetitions', '6 runs per side', 'Mann-Whitney', 'summary.json', 'Re-run the job')) {
        Assert-True $markdown.Contains($text) "the summary names: $text"
    }
    Assert-True $markdown.Contains('| dirty | composeCpuP95Ms | 0.016 | 0.018 | +12.50% | 0.0240 | 5% |') 'the flagged metric with its medians, change, p and band'
    $flaggedStart = $markdown.IndexOf('### Flagged metrics')
    $flaggedTable = $markdown.Substring($flaggedStart, $markdown.IndexOf('<details>') - $flaggedStart)
    Assert-True $flaggedTable.Contains('| Default | dirty | composeCpuP95Ms | 0.016 | 0.018 |') 'a regressed metric is in the flagged table first, not only in the full one'
    Assert-True $flaggedTable.Contains('| Default | clean | fps | 1,000.0 | 1,100.0 |') 'and so is an improved one'
    Assert-True (-not $flaggedTable.Contains('cppAllocations')) 'a metric that moved nowhere is not'
    Assert-True $markdown.Contains('<details><summary>Default: all 3 metrics</summary>') 'each scenario folds its full table'
    Assert-Equal 2 ([regex]::Matches($markdown, '</details>')).Count 'one fold per scenario'
    Assert-True $markdown.Contains('| dirty | cppAllocations | 2,160 | 2,160 | 0.00% | 0.0152 | exact |') 'an exact budget says exact'
    Assert-True (-not $markdown.Contains("`r")) 'one newline style'
    Assert-True (-not $markdown.Contains('started by hand')) 'a pull request is not a manual run'
    $manual = ConvertTo-BenchmarkMarkdown -Conclusion $conclusion -Summary $summary
    Assert-True $manual.Contains('started by hand') 'a manual run says it stays green for a finding'
    Assert-True (-not $manual.Contains('hosted VM')) 'a summary of a run on this machine does not call it a hosted VM'
    Assert-True (ConvertTo-BenchmarkMarkdown -Conclusion $conclusion -Summary $summary -Hosted).Contains('a shared hosted VM, not a controlled quiet desktop') 'a hosted one says what that is worth'
}

Invoke-FixtureCase 'the summary is the same in every culture and keeps a pipe from breaking a table' {
    param($root)
    $summary = New-Summary (New-ReportScenarios $root)
    $conclusion = Get-Conclusion $summary $root
    $expected = ConvertTo-BenchmarkMarkdown -Conclusion $conclusion -Summary $summary
    $culture = try { [Globalization.CultureInfo]::GetCultureInfo('de-DE') } catch { $null }
    if ($culture) {
        $previous = [Globalization.CultureInfo]::CurrentCulture
        try {
            [Globalization.CultureInfo]::CurrentCulture = $culture
            $german = ConvertTo-BenchmarkMarkdown -Conclusion $conclusion -Summary $summary
        } finally { [Globalization.CultureInfo]::CurrentCulture = $previous }
        Assert-Equal $expected $german 'numbers use the invariant culture'
    }
    $summary['machine'] = 'a|b'
    Assert-True (ConvertTo-BenchmarkMarkdown -Conclusion $conclusion -Summary $summary).Contains('a\|b') 'a pipe is escaped'
}

Invoke-FixtureCase 'a run with no flagged metric says none, and an inconclusive one says what it does not establish' {
    param($root)
    $summary = New-Summary @((New-Scenario -Directory $root -Controls @((Get-HeldControl), (Get-HeldControl)) -Metrics @((New-SetMetric 'clean' 'fps'))))
    $pass = ConvertTo-BenchmarkMarkdown -Conclusion (Get-Conclusion $summary $root) -Summary $summary -Gate
    Assert-True $pass.StartsWith("## Paired benchmark: No regression established`n") 'a pass'
    Assert-True $pass.Contains('None: no metric regressed or improved.') 'no flagged metric'
    Assert-True $pass.Contains('not evidence that none exists') 'and what a pass does not mean'
    Assert-True (-not $pass.Contains('Resolution:')) 'every metric held by its controls, so there is nothing unresolved to say'
    $noisy = New-Control @((New-ControlChange 'clean' 'fps' 1100.0 1500.0), (New-ControlChange 'dirty' 'cppAllocations' 2160 2160 -Exact))
    $drifting = New-Summary @((New-Scenario -Directory $root -Controls @($noisy, (Get-HeldControl)) -Metrics @((New-SetMetric 'clean' 'fps'), (New-SetMetric 'dirty' 'cppAllocations' -Exact))))
    $caveat = ConvertTo-BenchmarkMarkdown -Conclusion (Get-Conclusion $drifting $root) -Summary $drifting -Gate
    Assert-True $caveat.Contains('Resolution: the metrics that held inside their band in every same-binary control were Default 1 of 2.') 'a pass says how many metrics its controls could not hold'
    Assert-True $caveat.StartsWith("## Paired benchmark: No regression established`n") 'and is still a pass'
    $few = New-Summary @((New-Scenario -Directory $root -Controls @((Get-HeldControl)) -Metrics @((New-SetMetric 'clean' 'fps')) -MinimumP 0.33 -Runs 2))
    $inconclusive = ConvertTo-BenchmarkMarkdown -Conclusion (Get-Conclusion $few $root) -Summary $few -Gate
    Assert-True $inconclusive.StartsWith("## Paired benchmark: Inconclusive`n") 'an inconclusive run is named so'
    Assert-True $inconclusive.Contains('It does not pass') 'and says it is not a pass'
}

Invoke-FixtureCase 'annotations match what each scenario means for the check' {
    param($root)
    $conclusion = Get-Conclusion (New-Summary (New-ReportScenarios $root)) $root
    $gate = @(Get-BenchmarkAnnotations -Conclusion $conclusion -Gate)
    Assert-Equal 2 $gate.Count 'one per scenario that is not a plain pass'
    Assert-True $gate[0].StartsWith('::error title=Paired benchmark Default%3A confirmed degradation::') 'a confirmed degradation fails the check'
    Assert-True $gate[0].Contains('dirty/composeCpuP95Ms +12.50%') 'and names the metric'
    Assert-True $gate[1].StartsWith('::error title=Paired benchmark MultilineGrid%3A inconclusive::') 'an inconclusive run is an error too, not a quiet pass'
    Assert-True $gate[1].Contains('Re-run the job') 'and says what to do'
    Assert-Equal 1 ([regex]::Matches($gate[1], 'Re-run the job')).Count 'once'
    Assert-True $gate[1].Contains('1 regressed metric has same-binary controls that drifted') 'one flagged metric is counted in the singular'
    $manual = @(Get-BenchmarkAnnotations -Conclusion $conclusion)
    Assert-True ($manual[0].StartsWith('::warning ') -and $manual[1].StartsWith('::warning ')) 'a manual run reports the same findings as warnings'
    $plain = New-Scenario -Directory $root -Controls @((Get-HeldControl), (Get-HeldControl)) -Metrics @((New-SetMetric 'clean' 'fps'))
    $pass = Get-Conclusion (New-Summary @($plain)) $root
    Assert-Equal 0 @(Get-BenchmarkAnnotations -Conclusion $pass -Gate).Count 'a plain pass is quiet'
    $noise = New-Scenario -Directory $root -Controls @((Get-HeldControl), (Get-HeldControl)) -Metrics @((& $regressedComposeCpu))
    $same = @(Get-BenchmarkAnnotations -Conclusion (Get-Conclusion (New-Summary @($noise) -BaselineFingerprint 'CAFE' -CandidateFingerprint 'CAFE') $root) -Gate)
    Assert-True ($same.Count -eq 1 -and $same[0].StartsWith('::notice ')) 'a flag on identical library inputs is a notice, not a hidden result'
    $invalid = Get-Conclusion (New-Summary @((New-Scenario -Directory $root -Controls @() -Metrics @() -Status 'invalid-evidence' -Error 'boom'))) $root
    Assert-True @(Get-BenchmarkAnnotations -Conclusion $invalid)[0].StartsWith('::error ') 'invalid evidence is an error even by hand'
}

Invoke-FixtureCase 'an annotation lists a few flagged metrics and points to the job summary for the rest' {
    param($root)
    $names = @('fps', 'frameP50Ms', 'frameP95Ms', 'prepareP95Ms', 'composeCpuP95Ms', 'privateBytes', 'privatePeakBytes', 'workingSetBytes')
    $metrics = @($names | ForEach-Object { New-SetMetric 'dirty' $_ 'regressed' -Before 100.0 -After 120.0 })
    $changes = @($names | ForEach-Object { New-ControlChange 'dirty' $_ 100.0 101.0 })
    $scenario = New-Scenario -Directory $root -Controls @((New-Control $changes), (New-Control $changes)) -Metrics $metrics
    $summary = New-Summary @($scenario)
    $conclusion = Get-Conclusion $summary $root
    Assert-Equal 'degraded' $conclusion.Conclusion 'eight confirmed metrics'
    $annotation = @(Get-BenchmarkAnnotations -Conclusion $conclusion -Gate)[0]
    Assert-True $annotation.Contains('dirty/fps') 'the first is named'
    Assert-True $annotation.Contains('dirty/privateBytes') 'the sixth is named'
    Assert-True (-not $annotation.Contains('dirty/privatePeakBytes')) 'the seventh is left to the summary'
    Assert-True (-not $annotation.Contains('dirty/workingSetBytes')) 'and so is the eighth'
    Assert-True $annotation.Contains('and 2 more (see the job summary)') 'and the rest are counted'
    Assert-True ($annotation.Length -lt 1500) "an annotation stays short: $($annotation.Length)"
    Assert-True (ConvertTo-BenchmarkMarkdown -Conclusion $conclusion -Summary $summary -Gate).Contains('| dirty | workingSetBytes |') 'while the summary lists all eight'
}

Invoke-TestCase 'workflow commands escape what the runner reads as syntax' {
    $line = Format-WorkflowCommand 'error' 'a: b, c%' "first`nsecond 100%`r"
    Assert-Equal '::error title=a%3A b%2C c%25::first%0Asecond 100%25%0D' $line 'the command'
    Assert-Throws { Format-WorkflowCommand 'info' 't' 'm' } 'only the levels the runner knows'
}

Invoke-FixtureCase 'step outputs and summaries go to the files the runner names, and only one line at a time' {
    param($root)
    $outputFile = Join-Path $root 'output.txt'
    $summaryFile = Join-Path $root 'summary.md'
    $previous = @($env:GITHUB_OUTPUT, $env:GITHUB_STEP_SUMMARY)
    try {
        $env:GITHUB_OUTPUT = $outputFile
        $env:GITHUB_STEP_SUMMARY = $summaryFile
        Add-WorkflowOutput 'relevant' 'true'
        Add-WorkflowOutput 'baseline' ''
        Add-WorkflowSummary "## One`n" 6>$null
        Add-WorkflowSummary "Two`n" 6>$null
        Assert-Throws { Add-WorkflowOutput 'method' "two`nlines" } 'a multi-line value would inject another output'
    } finally { $env:GITHUB_OUTPUT = $previous[0]; $env:GITHUB_STEP_SUMMARY = $previous[1] }
    Assert-Equal "relevant=true`nbaseline=`n" ([IO.File]::ReadAllText($outputFile)) 'outputs, one per line'
    Assert-Equal "## One`nTwo`n" ([IO.File]::ReadAllText($summaryFile)) 'summaries append'
    Assert-Equal 114 ([int][IO.File]::ReadAllBytes($outputFile)[0]) 'without a byte-order mark'
}

function Invoke-GateScript([string] $Script, [hashtable] $Arguments, [string] $Directory, [bool] $Actions = $true) {
    # A workflow step's script, in this process, with the runner's files pointed at the fixture (and, as on a runner,
    # GITHUB_ACTIONS set unless $Actions is false). Returns its exit code and output.
    $names = @('GITHUB_OUTPUT', 'GITHUB_STEP_SUMMARY', 'GITHUB_EVENT_NAME', 'GITHUB_REF', 'GITHUB_BASE_REF', 'GITHUB_ACTIONS', 'BENCHMARK_PAIR')
    $previous = @{}
    foreach ($name in $names) { $previous[$name] = [Environment]::GetEnvironmentVariable($name) }
    try {
        New-Item -ItemType Directory -Path $Directory -Force | Out-Null
        [Environment]::SetEnvironmentVariable('GITHUB_OUTPUT', (Join-Path $Directory 'step-output.txt'))
        [Environment]::SetEnvironmentVariable('GITHUB_STEP_SUMMARY', (Join-Path $Directory 'step-summary.md'))
        [Environment]::SetEnvironmentVariable('GITHUB_EVENT_NAME', 'pull_request')
        [Environment]::SetEnvironmentVariable('GITHUB_REF', 'refs/pull/46/merge')
        [Environment]::SetEnvironmentVariable('GITHUB_ACTIONS', $(if ($Actions) { 'true' } else { $null }))
        [Environment]::SetEnvironmentVariable('BENCHMARK_PAIR', '(first parent of the merge commit)')
        $global:LASTEXITCODE = -1
        $output = & (Join-Path $repository $Script) @Arguments 6>&1 2>&1
        $code = $LASTEXITCODE
    } finally { foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $previous[$name]) } }
    $files = foreach ($name in @('step-output.txt', 'step-summary.md')) { $path = Join-Path $Directory $name; if (Test-Path -LiteralPath $path) { [IO.File]::ReadAllText($path) } else { '' } }
    return [pscustomobject]@{ Code = $code; Output = (@($output | ForEach-Object { "$_" }) -join "`n"); StepOutput = $files[0]; StepSummary = $files[1] }
}

Invoke-FixtureCase 'the verdict step fails a pull request on a degradation or an inconclusive run and passes a pass' {
    param($root)
    $reports = Join-Path $root 'reports'
    Set-FixtureJson $reports 'summary.json' (New-Summary (New-ReportScenarios $reports))
    $result = Invoke-GateScript 'Tools/Publish-BenchmarkVerdict.ps1' @{ Reports = $reports; Gate = $true } $root
    Assert-Equal 1 $result.Code 'a confirmed degradation fails the step'
    Assert-True $result.StepOutput.Contains('conclusion=degraded') 'and sets the output'
    Assert-True $result.StepSummary.StartsWith('## Paired benchmark: Confirmed degradation') 'writes the job summary'
    Assert-True $result.StepSummary.Contains('a shared hosted VM, not a controlled quiet desktop') 'which says what a runner is worth'
    Assert-True ($result.Output -cmatch 'Job summary: \d+ bytes in the step summary file') 'and the log says how much the summary file holds'
    Assert-True (-not (Invoke-GateScript 'Tools/Publish-BenchmarkVerdict.ps1' @{ Reports = $reports; Gate = $true } (Join-Path $root 'local-out') $false).StepSummary.Contains('hosted VM')) 'and not on a developer machine'
    Assert-True $result.Output.Contains('::error title=') 'prints an annotation'
    Assert-True (Test-Path -LiteralPath (Join-Path $reports 'verdict.md')) 'keeps the report with the receipts'
    $verdict = Get-FixtureJson $reports 'verdict.json'
    Assert-Equal 'degraded' $verdict['Conclusion'] 'and its conclusion as data'
    $manual = Invoke-GateScript 'Tools/Publish-BenchmarkVerdict.ps1' @{ Reports = $reports } (Join-Path $root 'manual')
    Assert-Equal 0 $manual.Code 'a manual run stays green for the same finding'
    Assert-True $manual.Output.Contains('::warning title=') 'and reports it as a warning'

    $quiet = Join-Path $root 'quiet'
    Set-FixtureJson $quiet 'summary.json' (New-Summary @((New-Scenario -Directory $quiet -Controls @((Get-HeldControl), (Get-HeldControl)) -Metrics @((New-SetMetric 'clean' 'fps')))))
    $pass = Invoke-GateScript 'Tools/Publish-BenchmarkVerdict.ps1' @{ Reports = $quiet; Gate = $true } (Join-Path $root 'quiet-out')
    Assert-Equal 0 $pass.Code 'a pass passes'
    Assert-True $pass.StepOutput.Contains('conclusion=pass') 'with its output'

    $few = Join-Path $root 'few'
    Set-FixtureJson $few 'summary.json' (New-Summary @((New-Scenario -Directory $few -Controls @((Get-HeldControl)) -Metrics @((New-SetMetric 'clean' 'fps')) -MinimumP 0.33 -Runs 2)))
    $inconclusive = Invoke-GateScript 'Tools/Publish-BenchmarkVerdict.ps1' @{ Reports = $few; Gate = $true } (Join-Path $root 'few-out')
    Assert-Equal 1 $inconclusive.Code 'an inconclusive run fails a pull request rather than reading as a pass'
    Assert-True $inconclusive.StepOutput.Contains('conclusion=inconclusive') 'and says it is inconclusive'
    Assert-Equal 0 (Invoke-GateScript 'Tools/Publish-BenchmarkVerdict.ps1' @{ Reports = $few } (Join-Path $root 'few-manual')).Code 'a manual run stays green for it'

    $invalid = Join-Path $root 'invalid'
    Set-FixtureJson $invalid 'summary.json' (New-Summary @((New-Scenario -Directory $invalid -Controls @() -Metrics @() -Status 'invalid-evidence' -Error 'Unmatched fixture')))
    Assert-Equal 1 (Invoke-GateScript 'Tools/Publish-BenchmarkVerdict.ps1' @{ Reports = $invalid } (Join-Path $root 'invalid-out')).Code 'invalid evidence fails even a manual run'

    $empty = Join-Path $root 'empty'
    New-Item -ItemType Directory -Path $empty | Out-Null
    $none = Invoke-GateScript 'Tools/Publish-BenchmarkVerdict.ps1' @{ Reports = $empty } (Join-Path $root 'empty-out')
    Assert-Equal 1 $none.Code 'no summary is no verdict'
    Assert-True $none.StepOutput.Contains('conclusion=none') 'recorded'
    Assert-True $none.StepSummary.Contains('no verdict') 'and explained'

    $broken = Join-Path $root 'broken'
    Set-FixtureFile $broken 'summary.json' '{ this is not json'
    $unreadable = Invoke-GateScript 'Tools/Publish-BenchmarkVerdict.ps1' @{ Reports = $broken; Gate = $true } (Join-Path $root 'broken-out')
    Assert-Equal 1 $unreadable.Code 'an unreadable summary is no verdict either'
    Assert-True $unreadable.StepOutput.Contains('conclusion=none') 'recorded'
    Assert-True $unreadable.StepSummary.Contains('could not be read') 'and explained'
}

Invoke-FixtureCase 'the strict reading reaches the verdict step: drifted controls and nothing flagged pass, strictly they do not' {
    param($root)
    $noisy = New-Control @((New-ControlChange 'dirty' 'composeCpuP95Ms' 0.0160 0.0200), (New-ControlChange 'clean' 'fps' 1100.0 1120.0))
    $reports = Join-Path $root 'strict'
    Set-FixtureJson $reports 'summary.json' (New-Summary @((New-Scenario -Directory $reports -Controls @($noisy, (Get-HeldControl)) -Metrics @((New-SetMetric 'clean' 'fps')))))
    Assert-Equal 0 (Invoke-GateScript 'Tools/Publish-BenchmarkVerdict.ps1' @{ Reports = $reports; Gate = $true } (Join-Path $root 'plain-out')).Code 'a pass with a drifted control elsewhere'
    $strict = Invoke-GateScript 'Tools/Publish-BenchmarkVerdict.ps1' @{ Reports = $reports; Gate = $true; StrictControls = $true } (Join-Path $root 'strict-out')
    Assert-Equal 1 $strict.Code 'the strict reading fails it'
    Assert-True $strict.StepOutput.Contains('conclusion=inconclusive') 'as inconclusive'
    Assert-True $strict.StepSummary.Contains('Strict controls are on') 'and the summary says the reading is strict'
}

Invoke-FixtureCase 'the scope step measures a library change and skips a documentation one, with the commits to compare' {
    param($root)
    $work = Join-Path $root 'work'
    New-Item -ItemType Directory -Path $work | Out-Null
    Invoke-FixtureGit $work @('init', '-q', '-b', 'main')
    [void](Add-FixtureCommit $work 'initial' @{ 'README.md' = "one`n"; 'src/Base.cpp' = "int base;`n" })
    foreach ($case in @(@{ Name = 'library'; File = 'src/Controls/Grid.cpp'; Relevant = 'true' }, @{ Name = 'docs'; File = 'docs/note.md'; Relevant = 'false' })) {
        $branch = "pr-$($case.Name)"
        Invoke-FixtureGit $work @('checkout', '-q', '--detach', 'main')
        Invoke-FixtureGit $work @('checkout', '-q', '-b', $branch)
        [void](Add-FixtureCommit $work "the $($case.Name) pull request" @{ $case.File = "changed`n" })
        Invoke-FixtureGit $work @('checkout', '-q', '--detach', 'main')
        Invoke-FixtureGit $work @('merge', '-q', '--no-ff', '-m', "merge ref $($case.Name)", $branch)
        $result = Invoke-GateScript 'Tools/Get-BenchmarkScope.ps1' @{ Repository = $work; BaseRef = 'main' } (Join-Path $root $case.Name)
        Assert-Equal 0 $result.Code "the $($case.Name) scope step succeeds either way"
        Assert-True $result.StepOutput.Contains("relevant=$($case.Relevant)`n") "relevant is $($case.Relevant) for the $($case.Name) change"
        Assert-True $result.StepOutput.Contains("baseline=$(Get-FixtureHead $work 'main')`n") 'the baseline is the base the merge was made on'
        Assert-True $result.StepOutput.Contains("candidate=$(Get-FixtureHead $work)`n") 'the candidate is the merge ref'
        Assert-True $result.StepOutput.Contains('method=first parent of the merge commit') 'with the method that chose it'
        $heading = if ($case.Relevant -eq 'true') { '## Paired benchmark scope: measured' } else { '## Paired benchmark scope: skipped' }
        Assert-True $result.StepSummary.StartsWith($heading) 'and the summary says what it decided'
        if ($case.Relevant -eq 'true') { Assert-True $result.StepSummary.Contains('`src/Controls/Grid.cpp` | library input') 'naming the file that decided and why' }
    }
    Assert-Throws { & (Join-Path $repository 'Tools/Get-BenchmarkScope.ps1') -Repository $work -BaseRef '' 6>$null } 'a missing base branch'
}

# --- The workflow --------------------------------------------------------------------------------------------------------

$workflow = [IO.File]::ReadAllText((Join-Path $repository '.github/workflows/ci.yml'))

function Get-WorkflowJob([string] $Name) {
    # The text of one job: from its key at two spaces of indentation to the next job's.
    $match = [regex]::Match($workflow, "(?ms)^  ${Name}:\r?\n(?<body>.*?)(?=^  [A-Za-z0-9_-]+:\r?\n|\z)")
    if (-not $match.Success) { throw "ci.yml has no job $Name" }
    return $match.Groups['body'].Value
}

Invoke-TestCase 'the workflow runs the benchmark for measured pull requests to main and still on manual dispatch' {
    $events = [Collections.Generic.List[string]]::new()
    $inside = $false
    foreach ($line in ($workflow -split "`r?`n")) {
        if ($line -cmatch '^on:\s*$') { $inside = $true; continue }
        if ($inside -and $line -cmatch '^\S') { break }
        if ($inside -and $line -cmatch '^  (\S+):') { $events.Add($Matches[1]) }
    }
    Assert-Equal 'push pull_request workflow_dispatch' ($events -join ' ') 'the events are those it always had'
    Assert-True (-not $workflow.Contains('pull_request_target')) 'never pull_request_target: untrusted code gets no write token'
    Assert-True (-not $workflow.Contains('benchmark-scope')) 'no job before the benchmark decides its scope: a failed one would skip it'
    $job = Get-WorkflowJob 'paired-benchmark'
    Assert-True (-not ($job -cmatch '(?m)^    needs:')) 'the benchmark job needs no other job, so no other job can skip it'
    Assert-True $job.Contains("name: `${{ github.event_name == 'pull_request' && 'paired-benchmark (pull request)' || 'paired-benchmark' }}") 'a pull request''s check has a name of its own, so a required check names one check run'
    # A skipped job is reported under its unevaluated name expression, which a required check would never see: the job always
    # starts for a pull request to main, whatever the pull request changes, and ends early when nothing measured changed.
    Assert-True $job.Contains("if: `${{ (github.event_name == 'pull_request' && github.base_ref == 'main') || (github.event_name == 'workflow_dispatch' && inputs.benchmark_baseline != '') }}") 'it starts for every pull request to main, whatever it changes, and for a dispatch with a baseline'
    Assert-True (-not $workflow.Contains('continue-on-error')) 'no job or step turns a failing verdict green'
    Assert-True $job.Contains('runs-on: windows-2025-vs2026') 'on the hosted x64 runner'
    # Its steps, in order: the scope decides, and every step after it runs only for a dispatch or a measured scope.
    $blocks = @([regex]::Matches($job, '(?ms)^      - .*?(?=^      - |\z)') | ForEach-Object { $_.Value })
    $parts = [ordered]@{ checkout = 'actions/checkout@'; scope = './Tools/Get-BenchmarkScope.ps1'; restore = './vcpkg-install.ps1'; measure = './performance-paired.ps1'; verdict = './Tools/Publish-BenchmarkVerdict.ps1'; upload = 'actions/upload-artifact@' }
    Assert-Equal $parts.Count $blocks.Count 'checkout, scope, restore, measurement, verdict and upload'
    $step = @{}
    $position = 0
    foreach ($key in $parts.Keys) {
        $hit = @($blocks | Where-Object { $_.Contains($parts[$key]) })
        Assert-Equal 1 $hit.Count "one $key step"
        Assert-Equal $position ([Array]::IndexOf($blocks, $hit[0])) "the $key step comes in its place"
        $step[$key] = $hit[0]
        $position++
    }
    $go = "github.event_name == 'workflow_dispatch' || steps.scope.outputs.relevant == 'true'"
    Assert-True (-not $step['checkout'].Contains('if:')) 'the checkout always runs'
    Assert-True $step['checkout'].Contains('fetch-depth: 0') 'with the history the merge ref''s parents and a baseline revision need'
    Assert-True ($step['scope'] -cmatch "(?m)^      - id: scope\r?\n        if: \`$\{\{ github\.event_name == 'pull_request' \}\}\r?\n") 'the scope step decides for a pull request, and for a dispatch there is nothing to decide'
    Assert-True ($step['scope'] -cmatch '(?m)^        timeout-minutes: \d+\s*$') 'and cannot hang the job'
    Assert-True $step['restore'].Contains("if: `${{ $go }}") 'the restore waits for a measured scope'
    Assert-True $step['measure'].Contains("if: `${{ $go }}") 'so does the measurement'
    Assert-True $step['verdict'].Contains("if: `${{ !cancelled() && ($go) }}") 'and the verdict, which also follows a failed measurement but not a superseded run'
    Assert-True $step['upload'].Contains("if: `${{ always() && ($go) }}") 'and the upload, which keeps the receipts even when the run fails'
    Assert-True $step['measure'].Contains('BENCHMARK_BASELINE: ${{ inputs.benchmark_baseline || steps.scope.outputs.baseline }}') 'a dispatch names its baseline, a pull request gets the base of its merge ref'
    Assert-True $step['verdict'].Contains('BENCHMARK_PAIR: ${{ steps.scope.outputs.method }}') 'and the verdict says how it was chosen'
    Assert-True $step['measure'].Contains('BENCHMARK_CANDIDATE: ${{ inputs.benchmark_candidate }}') 'the candidate is the dispatch input, else this checkout: the merge ref'
    Assert-True $step['measure'].Contains('./performance-paired.ps1 -BaselineRevision $env:BENCHMARK_BASELINE -CandidateRevision $env:BENCHMARK_CANDIDATE') 'one paired run with this harness'
    Assert-True $step['verdict'].Contains('./Tools/Publish-BenchmarkVerdict.ps1 -Gate:($env:GITHUB_EVENT_NAME -eq ''pull_request'')') 'only a pull request is gated'
}

Invoke-TestCase 'the benchmark job runs the contract''s scenarios and repetitions, and a dispatch keeps its inputs' {
    $job = Get-WorkflowJob 'paired-benchmark'
    $scenarios = (Get-BenchmarkGateScenarios) -join ','
    Assert-Equal 'Default,MultilineGrid,MultilineGridDistinct' $scenarios 'the gating scenarios of the contract and docs/performance.md'
    Assert-True $job.Contains("BENCHMARK_SCENARIOS: `${{ inputs.benchmark_scenarios || '$scenarios' }}") 'a pull request measures the gating scenarios'
    Assert-True ($workflow -cmatch "(?s)benchmark_scenarios:.*?default: $([regex]::Escape($scenarios))\r?\n") 'and so does a dispatch by default'
    $repetitions = [regex]::Match($job, '-Repetitions (\d+)')
    Assert-True $repetitions.Success 'the repetitions are explicit'
    Assert-Equal (Get-BenchmarkGateRepetitions) ([int]$repetitions.Groups[1].Value) 'the repetitions of the gate'
    Assert-True ((Get-BenchmarkGateRepetitions) -ge 3) 'three give six runs a side, whose smallest attainable p is 0.0022'
    Assert-True ((Get-MinimumAttainableP (2 * (Get-BenchmarkGateRepetitions)) (2 * (Get-BenchmarkGateRepetitions))) -lt 0.05) 'a verdict can be reached'
    foreach ($name in @('benchmark_baseline', 'benchmark_candidate', 'benchmark_scenarios')) { Assert-True ($workflow -cmatch "(?m)^      ${name}:\r?\n") "the dispatch input $name is kept" }
    Assert-True ($workflow -cmatch "(?s)benchmark_baseline:.*?default: ''") 'an empty baseline still skips the benchmark'
    Assert-True $job.Contains('name: paired-benchmark-x64-Release') 'the artifact keeps its name'
    Assert-True ($job -cmatch '(?s)upload-artifact@\S+ # v4\s+if: \$\{\{ always\(\) && ') 'and is uploaded even when the run fails'
    Assert-True $job.Contains('.build/paired/*/reports/**') 'with every receipt, comparison, summary and verdict'
}

Invoke-TestCase 'the workflow cancels what a push supersedes, bounds its runtime and pins what it runs' {
    Assert-True ($workflow -cmatch '(?m)^concurrency:\r?\n(?:  #.*\r?\n)*  group: dxui-\$\{\{ github\.workflow \}\}-\$\{\{ github\.event_name == ''workflow_dispatch'' && github\.run_id \|\| github\.ref \}\}\r?\n  cancel-in-progress: true') 'a push cancels the older run of its ref, and so of its pull request (refs/pull/n/merge); a dispatch is its own group'
    # A pull request's limits (a run takes 10 to 15 minutes) and a manual run's (the 90 minutes it always had, for diagnostic scenarios).
    $limit = "\`$\{\{ github\.event_name == 'pull_request' && (\d+) \|\| (\d+) \}\}"
    $job = Get-WorkflowJob 'paired-benchmark'
    $jobLimit = [regex]::Match($job, "(?m)^    timeout-minutes: $limit\s*`$")
    Assert-True $jobLimit.Success 'the benchmark job has a timeout for each trigger'
    Assert-True ([int]$jobLimit.Groups[1].Value -le 45) "a pull request's run is bounded to 45 minutes: $($jobLimit.Groups[1].Value)"
    Assert-Equal 90 ([int]$jobLimit.Groups[2].Value) 'a manual run keeps its 90 minutes'
    $steps = @([regex]::Matches($job, "(?m)^        timeout-minutes: $limit\s*`$"))
    $fixed = @([regex]::Matches($job, '(?m)^        timeout-minutes: (\d+)\s*$') | ForEach-Object { [int]$_.Groups[1].Value })
    Assert-True ($steps.Count -ge 2) 'the restore and the measurement have step limits shorter than the job, so the verdict and artifacts still run'
    Assert-True ($fixed.Count -ge 1) 'and so has the scope step'
    foreach ($column in @(1, 2)) {
        $sum = ($steps | ForEach-Object { [int]$_.Groups[$column].Value } | Measure-Object -Sum).Sum + ($fixed | Measure-Object -Sum).Sum
        Assert-True ($sum -lt [int]$jobLimit.Groups[$column].Value) "the step limits together leave the job time to report (column $column)"
    }
    Assert-True ($workflow -cmatch '(?m)^permissions:\r?\n  contents: read\s*$') 'the token is read-only'
    Assert-True (-not ($workflow -cmatch '(?m)^\s+\w+:\s*write\s*$')) 'nothing in the workflow writes: the summary and the artifacts need no permission'
    $uses = @([regex]::Matches($workflow, '(?m)^\s*-?\s*uses:\s*(\S+)'))
    Assert-True ($uses.Count -ge 6) 'the workflow uses actions'
    foreach ($use in $uses) { Assert-True ($use.Groups[1].Value -cmatch '^[\w.-]+/[\w.-]+@[0-9a-f]{40}$') "pinned to a commit: $($use.Groups[1].Value)" }
}

Invoke-TestCase 'the gate scripts parse and are strict' {
    foreach ($name in @('Tools/Get-BenchmarkScope.ps1', 'Tools/Publish-BenchmarkVerdict.ps1', 'Tools/BenchmarkGate.psm1')) {
        $tokens = $null
        $errors = $null
        [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $repository $name), [ref]$tokens, [ref]$errors)
        Assert-Equal 0 @($errors).Count "$name has no syntax errors: $(@($errors | ForEach-Object { $_.Message }) -join '; ')"
        Assert-True ([IO.File]::ReadAllText((Join-Path $repository $name)) -cmatch '(?m)^Set-StrictMode -Version Latest\r?$') "$name is strict"
    }
}

Complete-TestRun 'BenchmarkGate'
