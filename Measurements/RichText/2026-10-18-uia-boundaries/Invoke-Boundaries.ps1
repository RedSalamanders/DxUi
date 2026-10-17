# Fixed diagnostic only. No judge, policy, baseline or original receipt is changed.
[CmdletBinding()] param([string] $Root,[string] $BaselineRoot,[string] $OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$started=[DateTime]::UtcNow
if (Test-Path $OutputDirectory) { throw 'Use a fresh output directory; preserve previous attempts.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
Import-Module (Join-Path $Root 'Tools/PerformanceProfiling.psm1') -Force
Import-Module (Join-Path $Root 'Tools/PerformancePolicy.psm1')
$roots=@{A=$BaselineRoot;B=$Root}; $anchors=[ordered]@{}
foreach ($role in @('A','B')) {
    $anchors[$role]=Get-ProfilingMeasurementState -Root $roots[$role] -Platform x64 -Configuration Release
    Assert-PerformanceBuildAttestation -Root $roots[$role] -Platform x64 -Configuration Release -State $anchors[$role]
}
foreach ($key in @('benchmarkSha256','harnessIdentity','toolchainIdentity','dependencyIdentity','environmentIdentity')) {
    if ($anchors.A[$key] -cne $anchors.B[$key]) { throw "Cross-side identity differs: $key" }
}
if ((ConvertTo-Json $anchors.A.profilingInputs -Compress) -cne (ConvertTo-Json $anchors.B.profilingInputs -Compress)) { throw 'Diagnostic closures differ.' }
$schedule=@(foreach ($role in @('A','B','B','A')) { @{role=$role;timers='Off'} }; foreach ($role in @('B','A','A','B')) { @{role=$role;timers='On'} })
@{classification='diagnostic-only';startedUtc=$started.ToString('o');anchors=$anchors;schedule=$schedule;overallBudgetSeconds=900;
    ownedNativeChildBudgetSeconds=120;reportBudget=8;acceptance='not-established';
    scope='Default full/clear/composite/readback controls forward/reverse. Off ABBA then On BAAB; no trace, scheduler-delay, GPU-duration or dedicated-host claim.'} |
    ConvertTo-Json -Depth 32 | Set-Content (Join-Path $OutputDirectory 'plan.json') -Encoding utf8
$results=[Collections.Generic.List[object]]::new()
try {
    for ($index=0; $index -lt $schedule.Count; $index++) {
        $remaining=[int][Math]::Floor((900-([DateTime]::UtcNow-$started).TotalSeconds)*1000)
        if ($remaining -le 0) { break }
        $slot=$schedule[$index]; $stem='boundary-{0:d2}-{1}-{2}' -f ($index+1),$slot.role,$slot.timers
        $reportPath=Join-Path $OutputDirectory ($stem+'.json')
        $start=[Diagnostics.ProcessStartInfo]::new((Get-Command pwsh).Source)
        $start.UseShellExecute=$false; $start.CreateNoWindow=$true; $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
        $start.WorkingDirectory=$roots[$slot.role]
        foreach ($arg in @('-NoProfile','-NonInteractive','-File',(Join-Path $roots[$slot.role] 'performance-profile.ps1'),
            '-Configuration','Release','-Platform','x64','-SkipBuild','-BoundaryTimers',$slot.timers,'-OutputPath',$reportPath)) { $start.ArgumentList.Add($arg) }
        Write-Host "Boundary $($index+1)/8: $($slot.role), timers=$($slot.timers)"
        $process=[Diagnostics.Process]::Start($start)
        try {
            $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
            $timedOut=-not $process.WaitForExit($remaining)
            if ($timedOut) { $process.Kill($true); [void]$process.WaitForExit(5000) }
            [IO.File]::WriteAllText((Join-Path $OutputDirectory ($stem+'.runner.log')),$stdout.GetAwaiter().GetResult())
            [IO.File]::WriteAllText((Join-Path $OutputDirectory ($stem+'.runner.err')),$stderr.GetAwaiter().GetResult())
            $exitCode=$process.ExitCode
        } finally { $process.Dispose() }
        $identityMatched=$false; $identityError=''
        if ($exitCode -eq 0 -and -not $timedOut) {
            try {
                $report=Get-Content $reportPath -Raw | ConvertFrom-Json -AsHashtable -Depth 32
                $state=[ordered]@{}; foreach ($key in $anchors[$slot.role].Keys) { $state[$key]=$report[$key] }
                Assert-PerformanceMeasurementState -Before $anchors[$slot.role] -After $state
                $identityMatched=$true
            } catch { $identityError=$_.Exception.Message }
        }
        $results.Add(@{index=$index+1;role=$slot.role;timers=$slot.timers;report=$stem+'.json';exitCode=$exitCode;timedOut=$timedOut;
            identityMatched=$identityMatched;identityError=$identityError;completedUtc=[DateTime]::UtcNow.ToString('o')})
        if ($timedOut -or $exitCode -ne 0 -or $identityError) { break }
    }
} finally {
    @{scheduled=8;attempted=$results.Count;elapsedSeconds=([DateTime]::UtcNow-$started).TotalSeconds;results=$results.ToArray();acceptance='not-established'} |
        ConvertTo-Json -Depth 12 | Set-Content (Join-Path $OutputDirectory 'completion.json') -Encoding utf8
}
if ($results.Count -ne 8 -or @($results | Where-Object {$_.timedOut -or $_.exitCode -ne 0 -or -not $_.identityMatched}).Count) { throw 'Incomplete pilot; preserve failures and unattempted slots.' }
Write-Host 'All eight declared diagnostic processes completed.'
