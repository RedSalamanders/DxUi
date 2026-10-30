# Historical NativeScope compatibility fixtures; current CI uses the pinned common classifier and callers.
# Which runs of the validation workflow need its six native jobs: the documentation rules (every kind of documentation, and
# everything else, this repository's tracked files included), the scope step on fixture merge commits and for pushes and
# manual runs, and the triggers and wiring of the workflows. Everything runs against fixture repositories; no build or runner
# is needed.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../BenchmarkGate.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../NativeScope.psm1') -Force
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

function Invoke-ScopeGit([string] $Root, [string[]] $Arguments) {
    & git -C $Root -c core.autocrlf=false -c user.name=fixture -c user.email=fixture@example.invalid -c commit.gpgsign=false @Arguments 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "git $($Arguments -join ' ') failed in $Root" }
}

function Add-ScopeCommit([string] $Root, [string] $Message, [hashtable] $Files) {
    foreach ($name in $Files.Keys) { Set-FixtureFile $Root $name $Files[$name] }
    Invoke-ScopeGit $Root @('add', '-A')
    Invoke-ScopeGit $Root @('commit', '-q', '-m', $Message)
}

function Invoke-ScopeScript([hashtable] $Arguments, [string] $Directory) {
    # The scope step's script, in this process, with the runner's files pointed at the fixture and no event of the run's own.
    # Returns its exit code, step output and job summary.
    $names = @('GITHUB_OUTPUT', 'GITHUB_STEP_SUMMARY', 'GITHUB_EVENT_NAME', 'GITHUB_BASE_REF')
    $previous = @{}
    foreach ($name in $names) { $previous[$name] = [Environment]::GetEnvironmentVariable($name) }
    try {
        New-Item -ItemType Directory -Path $Directory -Force | Out-Null
        Restore-TestEnvironmentVariable 'GITHUB_OUTPUT' (Join-Path $Directory 'step-output.txt')
        Restore-TestEnvironmentVariable 'GITHUB_STEP_SUMMARY' (Join-Path $Directory 'step-summary.md')
        Restore-TestEnvironmentVariable 'GITHUB_EVENT_NAME' $null
        Restore-TestEnvironmentVariable 'GITHUB_BASE_REF' $null
        $global:LASTEXITCODE = -1
        $output = & (Join-Path $repository 'Tools/Get-NativeScope.ps1') @Arguments 6>&1 2>&1
        $code = $LASTEXITCODE
    } finally { foreach ($name in $names) { Restore-TestEnvironmentVariable $name $previous[$name] } }
    $files = foreach ($name in @('step-output.txt', 'step-summary.md')) { $path = Join-Path $Directory $name; if (Test-Path -LiteralPath $path) { [IO.File]::ReadAllText($path) } else { '' } }
    return [pscustomobject]@{ Code = $code; Output = (@($output | ForEach-Object { "$_" }) -join "`n"); StepOutput = $files[0]; StepSummary = $files[1] }
}

Invoke-FixtureCase 'scope invocation restores an absent GitHub event variable' {
    param($root)
    $name = 'GITHUB_EVENT_NAME'
    $previous = [Environment]::GetEnvironmentVariable($name)
    try {
        Restore-TestEnvironmentVariable $name $null
        $result = Invoke-ScopeScript @{ EventName = 'push'; Repository = (Join-Path $root 'no-repository') } (Join-Path $root 'run')
        Assert-Equal 0 $result.Code 'the fixture scope decision completes'
        Assert-True (-not (Test-Path -LiteralPath "Env:$name")) 'the wrapper restores the event variable to absent, not empty'
    } finally { Restore-TestEnvironmentVariable $name $previous }
}

# --- Which pull requests build and test natively ---------------------------------------------------------------------------

Invoke-TestCase 'documentation alone leaves the native jobs out, whichever kind it is' {
    $paths = @('README.md', 'AGENTS.md', 'CHANGELOG.md', 'CONTRIBUTING.md', 'LICENSE', 'Changes/2026-10-04-ci-run-scope.md', 'docs/controls.md',
        'docs/gallery/theme-controls-light.png', 'docs/gallery/generation.json', 'Specs/Testing/Testing_Validation.md',
        'Specs/DesignSystem/components/Tree/preview.html', 'Specs/Done/SourceImport/source-origin.json', 'Specs/Plans/WIP/README.md',
        'Measurements/GridSelection/2026-10-01/summary.receipt.txt', 'Measurements/PlainMenuUia/2026-10-02/harness/DxUiTests.MenuUiaCost.h',
        '.agents/skills/build-dxui/SKILL.md', '.github/workflows/format.yml', '.github/workflows/gallery.yml',
        # Markdown is documentation wherever it is.
        'src/README.md', 'Tests/Controls/NOTES.md', 'Tools/README.md')
    $scope = Get-NativeScope -ChangedPaths $paths
    Assert-True (-not $scope.Native) "no native job: $($scope.NativePaths -join ', ')"
    Assert-Equal $paths.Count $scope.Total 'every path is counted'
    Assert-Equal $paths.Count $scope.Documentation 'as documentation'
    Assert-Equal 'Markdown' (Get-DocumentationReason 'Tools/README.md') 'Markdown says why'
    Assert-Equal 'retained measurement' (Get-DocumentationReason 'Measurements/x/run.json') 'and so does its directory'
}

Invoke-TestCase 'every other path needs the native jobs, a path the rules do not know included' {
    $paths = @('src/Controls/DxUi.Grid.cpp', 'include/DxUi/DxUi.h', 'Tests/Controls/DxUi.Tests.Grid.cpp', 'Tests/Support/Support.Tests.UiaTestClient.h',
        'Samples/ComplexUi/ComplexUiScene.h', 'Build/DxUi.Consumer.props', 'Directory.Build.props', 'DxUi.sln', 'vcpkg.json',
        'vcpkg-configuration.json', 'capabilities.json', 'test.ps1', 'test-consumer.ps1', 'build.ps1', 'gallery.ps1', 'vcpkg-install.ps1',
        'validate.ps1', 'Tools/SuiteFailure.psm1', 'Tools/NativeScope.psm1', 'Tools/Get-NativeScope.ps1', 'Tools/tests/Test-NativeScope.ps1',
        '.github/workflows/ci.yml', '.gitattributes', '.clang-format',
        # git is case-sensitive: a directory that merely looks like documentation is not, and neither is a new one.
        'specs/lowercase.json', 'Specs-old/x.json', 'docs2/x.txt', 'Measurements.json', 'LICENSE.txt', 'NewFolder/new.txt')
    foreach ($path in $paths) {
        $scope = Get-NativeScope -ChangedPaths @($path)
        Assert-True $scope.Native "$path needs the native jobs"
        Assert-Equal $path ($scope.NativePaths -join '|') "and is the path named for it"
        Assert-True ($null -eq (Get-DocumentationReason $path)) "$path is not documentation"
    }
}

Invoke-TestCase 'one such path among documentation is enough, and paths are normalized and counted once' {
    $scope = Get-NativeScope -ChangedPaths @('docs/controls.md', 'Specs\UI\UI_ControlsAndLayout.md', '.\src\Controls\DxUi.Tree.cpp', 'src/Controls/DxUi.Tree.cpp', 'Changes/2026-10-04-x.md')
    Assert-True $scope.Native 'a library file among documentation'
    Assert-Equal 4 $scope.Total 'a path given twice, in two spellings, counts once'
    Assert-Equal 3 $scope.Documentation 'the others are documentation'
    Assert-Equal 'src/Controls/DxUi.Tree.cpp' ($scope.NativePaths -join '|') 'in git''s spelling'
}

Invoke-TestCase 'a change whose paths could not be read is not known to be documentation' {
    Assert-True (Get-NativeScope -ChangedPaths @()).Native 'no path'
    Assert-True (Get-NativeScope -ChangedPaths @('', $null)).Native 'blank names'
}

Invoke-TestCase 'every tracked source, test, sample, build input and tool needs the native jobs, and every tracked document does not' {
    $tracked = @(& git -C $repository -c core.quotepath=off ls-files)
    Assert-Equal 0 $LASTEXITCODE 'git ls-files'
    Assert-True ($tracked.Count -gt 200) 'the repository lists its files'
    $built = @($tracked | Where-Object { $_ -cmatch '^(src|include|Tests|Samples|Build|Tools)/' -and -not $_.EndsWith('.md', [StringComparison]::Ordinal) })
    Assert-True ($built.Count -gt 100) 'the sources, tests, samples, build inputs and tools are tracked'
    foreach ($path in $built) { Assert-True (Get-NativeScope -ChangedPaths @($path)).Native "$path needs the native jobs" }
    foreach ($path in @('test.ps1', 'test-consumer.ps1', 'build.ps1', 'gallery.ps1', 'vcpkg-install.ps1', 'capabilities.json', 'vcpkg.json', 'DxUi.sln', '.github/workflows/ci.yml')) {
        Assert-True ($tracked -ccontains $path) "$path is tracked"
        Assert-True (Get-NativeScope -ChangedPaths @($path)).Native "$path, which a native job runs or reads, needs them"
    }
    $documents = @($tracked | Where-Object { $_ -cmatch '^(Specs|Changes|Measurements|docs|\.agents)/' })
    Assert-True ($documents.Count -gt 100) 'the documentation is tracked'
    Assert-True (-not (Get-NativeScope -ChangedPaths $documents).Native) 'all of it together is documentation'
}

# --- The scope step ---------------------------------------------------------------------------------------------------------

Invoke-FixtureCase 'a push or a manual run always builds natively, without reading the repository' {
    param($root)
    foreach ($trigger in @('push', 'workflow_dispatch', '')) {
        $result = Invoke-ScopeScript @{ EventName = $trigger; Repository = (Join-Path $root 'no-repository') } (Join-Path $root "run-$trigger")
        Assert-Equal 0 $result.Code "the step succeeds for '$trigger'"
        Assert-True $result.StepOutput.Contains("native=true`n") "a '$trigger' run needs the native jobs"
        Assert-True $result.StepSummary.StartsWith('## Native jobs: run') 'and its summary says so'
    }
}

Invoke-FixtureCase 'a pull request builds natively unless it changes documentation alone' {
    param($root)
    $work = Join-Path $root 'work'
    New-Item -ItemType Directory -Path $work | Out-Null
    Invoke-ScopeGit $work @('init', '-q', '-b', 'main')
    Add-ScopeCommit $work 'initial' @{ 'README.md' = "one`n"; 'src/Base.cpp' = "int base;`n"; 'Specs/Plan.md' = "plan`n" }
    $cases = @(
        @{ Name = 'documentation'; Files = @{ 'docs/note.md' = "changed`n"; 'Specs/Plan.md' = "plan two`n"; 'Measurements/run/receipt.json' = "{}`n" }; Native = 'false'; Heading = '## Native jobs: skipped' }
        @{ Name = 'library'; Files = @{ 'src/Controls/Grid.cpp' = "changed`n"; 'docs/note.md' = "changed`n" }; Native = 'true'; Heading = '## Native jobs: run' }
    )
    foreach ($case in $cases) {
        $branch = "pr-$($case.Name)"
        Invoke-ScopeGit $work @('checkout', '-q', '--detach', 'main')
        Invoke-ScopeGit $work @('checkout', '-q', '-b', $branch)
        Add-ScopeCommit $work "the $($case.Name) pull request" $case.Files
        # The merge ref: the pull request merged into its base, as the runner checks it out.
        Invoke-ScopeGit $work @('checkout', '-q', '--detach', 'main')
        Invoke-ScopeGit $work @('merge', '-q', '--no-ff', '-m', "merge ref $($case.Name)", $branch)
        $result = Invoke-ScopeScript @{ EventName = 'pull_request'; BaseRef = 'main'; Repository = $work } (Join-Path $root $case.Name)
        Assert-Equal 0 $result.Code "the $($case.Name) step succeeds either way"
        Assert-True $result.StepOutput.Contains("native=$($case.Native)`n") "native is $($case.Native) for the $($case.Name) change"
        Assert-True $result.StepSummary.StartsWith($case.Heading) 'and the summary says what it decided'
        if ($case.Native -eq 'true') {
            Assert-True $result.StepSummary.Contains('- `src/Controls/Grid.cpp`') 'naming the path that decided'
            Assert-True (-not $result.StepSummary.Contains('docs/note.md')) 'and not the documentation beside it'
        }
    }
    Assert-Throws { & (Join-Path $repository 'Tools/Get-NativeScope.ps1') -EventName 'pull_request' -BaseRef '' -Repository $work 6>$null } 'a pull request without its base branch'
    # The last merge ref, against the commits the workflow names (Q12 of the 10 October review).
    $merge = (& git -C $work rev-parse HEAD).Trim()
    $base = (& git -C $work rev-parse 'HEAD^1').Trim()
    $named = Invoke-ScopeScript @{ EventName = 'pull_request'; BaseRef = 'main'; Repository = $work; ExpectedBaseSha = $base; ExpectedCandidateSha = $merge } (Join-Path $root 'named')
    Assert-Equal 0 $named.Code 'the named pair is scoped'
    foreach ($case in @(@{ Base = ('0' * 40); Merge = $merge }, @{ Base = $base; Merge = ('0' * 40) }, @{ Base = $base.Substring(0, 12); Merge = $merge })) {
        Assert-Throws { & (Join-Path $repository 'Tools/Get-NativeScope.ps1') -EventName 'pull_request' -BaseRef 'main' -Repository $work -ExpectedBaseSha $case.Base -ExpectedCandidateSha $case.Merge 6>$null } "a merge ref other than the named one ($($case.Base.Substring(0, 4))/$($case.Merge.Substring(0, 4))) fails the step"
    }
    # A repository without a commit (the fixtures live inside this checkout, so a plain directory would find it instead).
    $empty = Join-Path $root 'empty'
    New-Item -ItemType Directory -Path $empty | Out-Null
    Invoke-ScopeGit $empty @('init', '-q', '-b', 'main')
    Assert-Throws { & (Join-Path $repository 'Tools/Get-NativeScope.ps1') -EventName 'pull_request' -BaseRef 'main' -Repository $empty 6>$null } 'a pull request whose commits cannot be read fails the step'
}

# --- Current shared workflow contract (legacy scope command tests above remain compatibility coverage) --------------------
$engine = Join-Path $repository '.ci-engine'
Import-Module (Join-Path $engine 'Tools/Modules/Ci.Policy.psm1')
Import-Module (Join-Path $engine 'Tools/Modules/Ci.Project.psm1')
Invoke-TestCase 'the current caller is pinned and PR delegation has no native profiles' {
    $project=Read-CiProject $repository
    Test-CiProjectConfiguration @{root=$repository;engineRoot=$engine;project=$project;development=$false}
    Assert-Equal 0 @((Get-Content (Join-Path $repository 'Tests/test-scopes.json') -Raw|ConvertFrom-Json).prCoverage).Count 'native PR delegation is empty'
    $pr=[IO.File]::ReadAllText((Join-Path $engine '.github/workflows/pr.yml'))
    Assert-True ($pr.Contains("CI_NATIVE_BUILDS_DISABLED: 'true'")) 'PR tooling cannot enter native builds'
    Assert-True (-not ($pr -match 'setup-msvc|build.ps1|test-consumer.ps1')) 'PRs do not execute native producer or compiler fixtures'
}
Invoke-TestCase 'main native qualification keeps both native architectures and the global production barrier' {
    $native=[IO.File]::ReadAllText((Join-Path $engine '.github/workflows/native.yml'))
    Assert-Equal 2 ([regex]::Matches($native,'platform: ARM64').Count) 'native ARM64 in both roles'
    Assert-Equal 2 ([regex]::Matches($native,'platform: x64').Count) 'native x64 in both roles'
    Assert-True ($native.Contains('needs: [admission, test-gate]')) 'neither production branch can precede the global gate'
    Assert-True ($native.Contains('timeout-minutes: 360')) 'the regression job retains a finite cold-build/test allowance'
    Assert-True ($native.Contains('retention-days: 90')) 'source-bound regression and production receipts remain available'
    Assert-True (-not $native.Contains('continue-on-error: true')) 'native qualification cannot ignore a failed phase'
    $queue=[IO.File]::ReadAllText((Join-Path $repository '.github/workflows/qualify.yml'))
    Assert-True ($queue.Contains('cancel-in-progress: false') -and $queue.Contains('queue: max')) 'later merges cannot cancel admitted work'
}
Invoke-TestCase 'the project adapter keeps Full native suites, consumer qualification and gallery coverage' {
    $adapter=[IO.File]::ReadAllText((Join-Path $repository 'Tools/Invoke-ProjectCi.ps1'))
    foreach($command in @("Run 'test.ps1'","'-Full','-SkipTooling','-SkipBuild'","Run 'test-consumer.ps1'","Run 'gallery.ps1'","'-DisableStlAnnotations'","MenuResourceScaling,MenuTextLayoutResources","Get-CiSuiteEvidence","Invoke-CiAssertionProbe")){
        Assert-True ($adapter.Contains($command)) "the adapter retains $command"
    }
    $test=[IO.File]::ReadAllText((Join-Path $repository 'test.ps1'))
    Assert-True ($test.Contains('normal-suites.json')) 'normal native execution writes its own unfiltered suite summary'
}
Invoke-TestCase 'historical scope commands remain parseable compatibility entry points' {
    foreach($name in @('Tools/NativeScope.psm1','Tools/Get-NativeScope.ps1')){
        $tokens=$null;$errors=$null
        [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $repository $name),[ref]$tokens,[ref]$errors)
        Assert-Equal 0 @($errors).Count "$name syntax"
    }
}
Complete-TestRun 'NativeScope'
