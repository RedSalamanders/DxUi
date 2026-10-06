# Exercise the control-test runner's --test filter against the built DxUi.ControlTests.exe (test.ps1 runs it after the build).
[CmdletBinding()] param(
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Debug',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $repo ".build/$Platform/$Configuration/DxUi.ControlTests.exe"
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Test executable is missing: $exe" }

# Runs the executable from the repository root and returns its exit code, its output and the tests it started.
function Invoke-Runner([string[]] $Arguments) {
    Push-Location $repo
    try { $output = @(& $exe @Arguments 2>&1 | ForEach-Object { "$_" }); $exit = $LASTEXITCODE } finally { Pop-Location }
    $started = @($output | ForEach-Object { if ($_ -match '^  \[START\] (\w+)$') { $Matches[1] } })
    return [pscustomobject]@{ Exit = $exit; Output = $output; Started = $started }
}
$grid = @('--suite=Grid', '--no-activate')

Invoke-TestCase 'the filter runs only the named tests, in suite order' {
    $run = Invoke-Runner ($grid + '--test=TestVisibleSpan,TestSortCycle')
    Assert-Equal 0 $run.Exit 'a filtered run of known tests passes'
    Assert-Equal 'TestSortCycle,TestVisibleSpan' ($run.Started -join ',') 'exactly the named tests start, as the runner orders them'
}
Invoke-TestCase 'an unknown name fails the run instead of passing with nothing run' {
    $run = Invoke-Runner ($grid + '--test=NoSuchTest')
    Assert-Equal 2 $run.Exit 'an unknown test name exits with the usage code'
    Assert-Equal 0 $run.Started.Count 'an unknown name runs no test'
    Assert-Contains $run.Output 'Unknown test name for the selected suites: NoSuchTest' 'the run names the unknown test'
}
Invoke-TestCase 'one unknown name fails a run that also names a known test' {
    $run = Invoke-Runner ($grid + '--test=TestSortCycle,NoSuchTest')
    Assert-Equal 2 $run.Exit 'a partly unknown list fails'
    Assert-Contains $run.Output 'Unknown test name for the selected suites: NoSuchTest' 'the run names only the unknown test'
}
Invoke-TestCase 'names are exact and case-sensitive' {
    Assert-Equal 2 (Invoke-Runner ($grid + '--test=testsortcycle')).Exit 'a name with other case is unknown'
    Assert-Equal 2 (Invoke-Runner ($grid + '--test=TestSortCycle ')).Exit 'a name with a trailing space is malformed'
}
Invoke-TestCase 'an empty or malformed list is rejected' {
    foreach ($list in @('--test=', '--test=TestSortCycle,', '--test=,TestSortCycle', '--test=Test-SortCycle')) {
        $run = Invoke-Runner ($grid + $list)
        Assert-Equal 2 $run.Exit "$list exits with the usage code"
        Assert-Equal 0 $run.Started.Count "$list runs no test"
    }
}
Invoke-TestCase 'a fixture suite without named tests rejects the filter before it runs' {
    foreach ($suite in @('MenuResources', 'MenuResourceScaling', 'MenuTextLayoutResources', 'MenuExitLifetime', 'Gallery', 'ButtonContrast')) {
        $run = Invoke-Runner @("--suite=$suite", '--test=TestSortCycle')
        $suitesStarted = @($run.Output | Where-Object { $_ -match '^\[START\]' })
        Assert-Equal 2 $run.Exit "$suite rejects --test"
        Assert-Equal 0 $suitesStarted.Count "$suite starts nothing"
    }
}
Invoke-TestCase 'no runner calls a test directly, so the filter can select every test' {
    $direct = foreach ($file in Get-ChildItem -Path (Join-Path $repo 'Tests/Controls/DxUiTests*') -File) {
        $source = [IO.File]::ReadAllText($file.FullName)
        foreach ($runner in [regex]::Matches($source, '(?ms)^void (Run\w+)\(\)\r?\n\{(.*?)^\}')) {
            foreach ($call in [regex]::Matches($runner.Groups[2].Value, '(?m)^\s+(Test\w+)\(\);')) { "$($file.Name): $($runner.Groups[1].Value) calls $($call.Groups[1].Value)" }
        }
    }
    Assert-Equal 0 @($direct).Count "runners register tests as DXUI_RUN_TEST(TestName); ($(@($direct) -join '; '))"
}
Invoke-TestCase 'without the filter a suite runs every test its runner registers' {
    $run = Invoke-Runner $grid
    Assert-Equal 0 $run.Exit 'the unfiltered Grid suite passes'
    $source = [IO.File]::ReadAllText((Join-Path $repo 'Tests/Controls/DxUi.Tests.Grid.cpp'))
    $registered = @([regex]::Matches($source, '(?m)^    DXUI_RUN_TEST\((\w+)\);\r?$') | ForEach-Object { $_.Groups[1].Value })
    Assert-True ($registered.Count -gt 0) 'the Grid runner registers tests with DXUI_RUN_TEST'
    Assert-Equal ($registered -join ',') ($run.Started -join ',') 'every registered Grid test starts, in order'
}
Complete-TestRun 'Test filter'
