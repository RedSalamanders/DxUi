[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../ParallelSuites.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../SuiteFailure.psm1') -Force
$pwsh = (Get-Process -Id $PID).Path

Invoke-FixtureCase 'bounded children overlap, retain isolated streams and propagate failures' {
    param($root)
    $child = Join-Path $root 'child.ps1'
    @'
param([string]$Root,[string]$Name,[string]$Mode)
[IO.File]::WriteAllText((Join-Path $Root "$Name.ready"), [DateTime]::UtcNow.Ticks.ToString())
if ($Mode -ne 'later') {
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    while (-not ((Test-Path -LiteralPath (Join-Path $Root 'first.ready')) -and (Test-Path -LiteralPath (Join-Path $Root 'second.ready')))) {
        if ([DateTime]::UtcNow -gt $deadline) { [Console]::Error.WriteLine('overlap barrier failed'); exit 91 }
        Start-Sleep -Milliseconds 25
    }
    Start-Sleep -Milliseconds 300
    [IO.File]::WriteAllText((Join-Path $Root "$Name.done"), 'done')
} elseif (-not ((Test-Path -LiteralPath (Join-Path $Root 'first.done')) -or (Test-Path -LiteralPath (Join-Path $Root 'second.done')))) {
    [Console]::Error.WriteLine('concurrency limit exceeded'); exit 92
}
[Console]::Out.Write("stdout $Name")
if ($Mode -eq 'failure') { [Console]::Error.WriteLine('TIMEOUT: TestHeld after 3 s'); exit 124 }
[Console]::Out.WriteLine()
if ($Mode -eq 'later') { [Console]::Out.WriteLine('SKIPPED: optional fixture') }
exit 0
'@ | Set-Content -LiteralPath $child -Encoding utf8NoBOM
    $runs = @(foreach ($item in @(@('first','failure'),@('second','success'),@('third','later'))) {
        [pscustomobject]@{ Name=$item[0]; Executable=$pwsh; Arguments=@('-NoProfile','-File',$child,$root,$item[0],$item[1]); Log=(Join-Path $root "$($item[0]).log") }
    })
    $results = @(Invoke-DxUiParallelSuites -Runs $runs -Limit 2 -WorkingDirectory $root)
    Assert-Equal 3 $results.Count 'every requested child reports one result'
    Assert-Equal 124 (@($results | Where-Object Name -eq 'first')[0].ExitCode) 'the exact failing exit code is retained'
    Assert-Equal 0 (@($results | Where-Object Name -eq 'second')[0].ExitCode) 'the peer succeeds after real overlap'
    Assert-Equal 0 (@($results | Where-Object Name -eq 'third')[0].ExitCode) 'queued work runs after a failure'
    $failure = Get-SuiteFailureReport -Suite first -ExitCode 124 -LogPath $runs[0].Log
    Assert-Equal 'TIMEOUT: TestHeld after 3 s' $failure.Timeout 'stderr reaches the ordinary watchdog failure packet'
    Assert-True ((Get-Content -Raw -LiteralPath $runs[2].Log).Contains('SKIPPED: optional fixture')) 'capability skip text stays in the suite log'
    $skips = Get-SuiteSkips -LogPath $runs[2].Log
    Assert-Contains $skips '(before any test): optional fixture' 'the standard skip parser sees the retained skip'
    foreach ($run in $runs) {
        Assert-True (Test-Path -LiteralPath $run.Log -PathType Leaf) "final log exists for $($run.Name)"
        Assert-True (Test-Path -LiteralPath ($run.Log + '.stdout') -PathType Leaf) 'stdout provenance is retained'
        Assert-True (Test-Path -LiteralPath ($run.Log + '.stderr') -PathType Leaf) 'stderr provenance is retained'
    }
}
Invoke-FixtureCase 'failed process creation fails closed' {
    param($root)
    $runs = @([pscustomobject]@{Name='missing';Executable=(Join-Path $root 'missing-executable');Arguments=@();Log=(Join-Path $root 'missing.log')})
    Assert-Throws { Invoke-DxUiParallelSuites -Runs $runs -Limit 2 -WorkingDirectory $root } 'unlaunchable children do not produce passing records'
}
Invoke-FixtureCase 'arguments remain literal across paths and child processes' {
    param($root)
    $directory = Join-Path $root 'with spaces'
    New-Item -ItemType Directory -Path $directory | Out-Null
    $child = Join-Path $directory 'literal args.ps1'
    @'
param([string]$Value)
[IO.File]::WriteAllText('argument.txt', $Value, [Text.UTF8Encoding]::new($false))
[Console]::Out.WriteLine('argument retained')
'@ | Set-Content -LiteralPath $child -Encoding utf8NoBOM
    $value = 'unicode β and $(literal)'
    $run = [pscustomobject]@{Name='literal';Executable=$pwsh;Arguments=@('-NoProfile','-File',$child,$value);Log=(Join-Path $directory 'literal.log')}
    $result = @(Invoke-DxUiParallelSuites -Runs @($run) -Limit 2 -WorkingDirectory $directory)
    Assert-Equal 0 $result[0].ExitCode 'literal argument child succeeds'
    Assert-Equal $value (Get-Content -Raw -LiteralPath (Join-Path $directory 'argument.txt')) 'spaces, Unicode and shell metacharacters remain one literal argument'
}
Invoke-TestCase 'canonical runner keeps parallelism optional and outside the foreground lease' {
    $runner = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot '../../test.ps1')
    Assert-True ($runner.Contains('[ValidateRange(1,8)][int] $ParallelSuites = 1')) 'serial is the bounded default'
    Assert-True ($runner.Contains('if ($suite -notin $leaseSuites)')) 'the concurrent batch excludes every foreground suite'
    Assert-True ($runner.Contains('Invoke-DxUiParallelSuites -Runs $ordinaryRuns')) 'canonical runner uses the tested owned-child scheduler'
    Assert-True ($runner.Contains('Start-TestRunEvidence -Repository $PSScriptRoot -CaptureNativeArtifacts')) 'artifact ownership and before/after capture remain in force'
    Assert-True ($runner.Contains('$parallelResults[$result.Name] = $result')) 'each completion remains associated with its own suite receipt'
}
Complete-TestRun 'Parallel suite execution'
