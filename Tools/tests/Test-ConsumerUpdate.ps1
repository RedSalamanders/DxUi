# Deterministic advisory tests: no network, credentials, lock mutation or product checkout.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../ConsumerUpdate.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$fixture = New-FixtureRoot
$module = Get-Module ConsumerUpdate
try {
    $lock = Join-Path $fixture 'DxUi.lock.json'
    $old = 'a' * 40
    $new = 'b' * 40
    @{repository='https://github.com/RedSalamanders/DxUi';commit=$old;apiRevision=2;targets=@('DxUi')} | ConvertTo-Json | Set-Content $lock
    $before = (Get-FileHash $lock).Hash
    foreach ($case in @('same','green','pending','failed','absent','divergent','ahead','offline','wrong-run','malformed')) {
        $calls = [Collections.Generic.List[string]]::new()
        $request = {
            param($route)
            $calls.Add($route)
            if ($case -eq 'offline') { throw 'private transport error must not escape' }
            if ($route -eq 'commits/main') {
                return @{sha=$(if ($case -eq 'same') {$old} elseif ($case -eq 'malformed') {'bad'} else {$new})}
            }
            if ($route.StartsWith('compare/')) {
                return @{status=$(if ($case -eq 'divergent') {'diverged'} elseif ($case -eq 'ahead') {'behind'} else {'ahead'})}
            }
            if ($route -notlike 'actions/workflows/qualify.yml/runs*event=workflow_dispatch*') {
                throw 'The cheap push coordinator is not native qualification.'
            }
            if ($case -eq 'absent') { return @{workflow_runs=@()} }
            return @{workflow_runs=@(@{
                head_sha=$(if($case -eq 'wrong-run'){$old}else{$new})
                status=$(if($case -eq 'pending'){'in_progress'}else{'completed'})
                conclusion=$(if($case -eq 'failed'){'failure'}else{'success'})
            })}
        }.GetNewClosure()
        $notice = Get-DxUiUpdateNotice -LockFile $lock -Request $request
        $expected = switch ($case) {
            same {''} green {'update available'} divergent {'diverge'} ahead {'ahead of main'}
            offline {'unavailable'} malformed {'unavailable'} default {'no successful completed validation'}
        }
        if (($case -eq 'same' -and $null -ne $notice) -or ($case -ne 'same' -and $notice -notlike "*$expected*")) {
            throw "Wrong advisory for ${case}: $notice"
        }
        if (@($notice).Count -gt 1 -or $calls.Count -gt 3 -or ($case -eq 'same' -and $calls.Count -ne 1)) { throw "Repeated lookup/notice for $case" }
        if ((Get-FileHash $lock).Hash -ne $before) { throw 'Advisory modified the exact pin.' }
        Write-Host "PASS DxUi update advisory: $case"
    }

    Invoke-TestCase 'the owned credential helper is bounded, drained and only returns successful output' {
        $shell = (Get-Process -Id $PID).Path
        Set-FixtureFile $fixture 'helper-success.ps1' "[Console]::WriteLine('fixture-token')`n"
        Set-FixtureFile $fixture 'helper-failure.ps1' "[Console]::WriteLine('must-not-be-used')`nexit 7`n"
        Set-FixtureFile $fixture 'helper-timeout.ps1' @'
param([string] $IdentityPath)
$process = [Diagnostics.Process]::GetCurrentProcess()
[IO.File]::WriteAllText($IdentityPath, "$($process.Id)|$($process.StartTime.ToUniversalTime().Ticks)")
Start-Sleep -Seconds 30
'@

        $invoke = {
            param([string] $Script, [int] $TimeoutMilliseconds, [string] $HelperShell, [string] $HelperFixture, $HelperModule)
            $start = [Diagnostics.ProcessStartInfo]::new()
            $start.FileName = $HelperShell
            $identityPath = Join-Path $HelperFixture 'child-identity.txt'
            $start.Arguments = "-NoProfile -NonInteractive -File `"$Script`" `"$identityPath`""
            $start.UseShellExecute = $false
            $start.CreateNoWindow = $true
            $start.RedirectStandardOutput = $true
            $start.RedirectStandardError = $true
            return & $HelperModule { param($info, $budget) Invoke-DxUiOwnedProcess -StartInfo $info -TimeoutMilliseconds $budget } $start $TimeoutMilliseconds
        }

        $success = & $invoke (Join-Path $fixture 'helper-success.ps1') 2000 $shell $fixture $module
        Assert-Equal 0 $success.ExitCode 'successful helper exit'
        Assert-True (-not $success.TimedOut) 'successful helper is not timed out'
        Assert-Equal 'fixture-token' $success.StandardOutput.Trim() 'successful helper output is drained'

        $failure = & $invoke (Join-Path $fixture 'helper-failure.ps1') 2000 $shell $fixture $module
        Assert-Equal 7 $failure.ExitCode 'nonzero helper exit'
        Assert-Equal '' $failure.StandardOutput 'nonzero helper output is discarded'

        Set-FixtureFile $fixture 'helper-sentinel.ps1' "Start-Sleep -Seconds 30`n"
        $sentinel = [Diagnostics.Process]::new()
        $sentinelStart = [Diagnostics.ProcessStartInfo]::new()
        $sentinelStart.FileName = $shell
        $sentinelStart.Arguments = "-NoProfile -NonInteractive -File `"$(Join-Path $fixture 'helper-sentinel.ps1')`""
        $sentinelStart.UseShellExecute = $false
        $sentinelStart.CreateNoWindow = $true
        $sentinel.StartInfo = $sentinelStart
        $sentinelStarted = $false
        try {
            Assert-True $sentinel.Start() 'independent sentinel starts'
            $sentinelStarted = $true
            $sentinelId = $sentinel.Id
            $sentinelStartTicks = $sentinel.StartTime.ToUniversalTime().Ticks
            $timeout = & $invoke (Join-Path $fixture 'helper-timeout.ps1') 3000 $shell $fixture $module
            Assert-True $timeout.TimedOut 'slow helper reaches its timeout'
            $identity = [IO.File]::ReadAllText((Join-Path $fixture 'child-identity.txt')).Split('|')
            $child = Get-Process -Id ([int]$identity[0]) -ErrorAction SilentlyContinue
            try {
                Assert-True ($null -eq $child -or $child.StartTime.ToUniversalTime().Ticks -ne [long]$identity[1]) 'owned timed-out helper has exited'
            } finally { if ($child) { $child.Dispose() } }
            $sentinel.Refresh()
            Assert-True (-not $sentinel.HasExited -and $sentinel.Id -eq $sentinelId -and $sentinel.StartTime.ToUniversalTime().Ticks -eq $sentinelStartTicks) 'unrelated process remains untouched'
        } finally {
            if ($sentinelStarted -and -not $sentinel.HasExited) {
                $sentinel.Kill()
                Assert-True ($sentinel.WaitForExit(1000)) 'owned sentinel cleanup is bounded'
            }
            $sentinel.Dispose()
        }
        Assert-Equal $before (Get-FileHash $lock).Hash 'helper lifecycle leaves the exact pin unchanged'
    }.GetNewClosure()
} finally {
    Remove-FixtureRoot $fixture
}
Complete-TestRun 'DxUi update advisory'
