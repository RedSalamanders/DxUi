# Fixed follow-up to the Default paired study. Uses the existing guarded profiling runner;
# neither this driver nor its reports make a regression-acceptance decision.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $RepositoryRoot,
    [Parameter(Mandatory)][string] $BaselineRoot,
    [Parameter(Mandatory)][string] $OutputDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$started = [DateTime]::UtcNow
$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).ProviderPath
$BaselineRoot = (Resolve-Path -LiteralPath $BaselineRoot).ProviderPath
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Use a fresh output directory; preserve every earlier process.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
Import-Module (Join-Path $RepositoryRoot 'Tools/PerformanceProfiling.psm1') -Force
# Profiling force-loads Policy internally. Expose that same module to this caller after it.
Import-Module (Join-Path $RepositoryRoot 'Tools/PerformancePolicy.psm1')
$roots = @{ A=$BaselineRoot; B=$RepositoryRoot }
$anchors = [ordered]@{}
foreach ($role in @('A','B')) {
    $anchors[$role] = Get-ProfilingMeasurementState -Root $roots[$role] -Platform x64 -Configuration Release
    Assert-PerformanceBuildAttestation -Root $roots[$role] -Platform x64 -Configuration Release -State $anchors[$role]
}
if ($anchors.A.sourceCommit -cne '71059fb94c12c8966056e3afe626d1548ab1030b' -or
    $anchors.B.sourceCommit -cne '8c21a634c5abd55f14e384673e7d710de572bdcb') { throw 'This retained study requires its exact baseline and candidate commits.' }
foreach ($key in @('benchmarkSha256','toolchainIdentity','dependencyIdentity','environmentIdentity','harnessIdentity')) {
    if ($anchors.A[$key] -cne $anchors.B[$key]) { throw "Cross-side profile identity differs: $key" }
}
if ((ConvertTo-Json $anchors.A.profilingInputs -Compress) -cne (ConvertTo-Json $anchors.B.profilingInputs -Compress)) {
    throw 'The two diagnostic helper closures differ.'
}
$schedule = @(foreach ($role in @('A','B','B','A')) { [ordered]@{ role=$role; heap=$false } }
    foreach ($role in @('B','A','A','B')) { [ordered]@{ role=$role; heap=$true } })
$plan = [ordered]@{ classification='diagnostic-only'; anchors=$anchors; scenario='Default'; configuration='Release'; platform='x64'
    schedule=$schedule; startedUtc=$started.ToString('o'); reportBudget=8; overallBudgetSeconds=900; ownedNativeChildBudgetSeconds=120
    notes='Fixed heap-off ABBA then heap-on BAAB, two processes per side/profile; no replacement of failures or outliers. Existing extra warmup and shared sampling differ from canonical frames. No dedicated-host, scheduling-delay, heap-owner, policy or regression acceptance claim.' }
$plan | ConvertTo-Json -Depth 32 | Set-Content (Join-Path $OutputDirectory 'plan.json') -Encoding utf8
$results = [Collections.Generic.List[object]]::new()
try {
    for ($index=0; $index -lt $schedule.Count; $index++) {
        $remainingMs = [int][Math]::Floor((900 - ([DateTime]::UtcNow - $started).TotalSeconds) * 1000)
        if ($remainingMs -le 0) { break }
        $slot = $schedule[$index]
        $stem = 'profile-{0:d2}-{1}-{2}' -f ($index+1),$slot.role,$(if ($slot.heap) { 'Heap' } else { 'NoHeap' })
        $reportPath = Join-Path $OutputDirectory ($stem + '.json')
        Write-Host "Profile $($index+1)/8: $($slot.role), heap=$($slot.heap)"
        $start = [Diagnostics.ProcessStartInfo]::new((Get-Command pwsh).Source)
        $start.UseShellExecute=$false; $start.CreateNoWindow=$true
        $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
        $start.WorkingDirectory=$roots[$slot.role]
        foreach ($arg in @('-NoProfile','-NonInteractive','-File',(Join-Path $roots[$slot.role] 'performance-profile.ps1'),
            '-Configuration','Release','-Platform','x64','-Scenario','Default','-SkipBuild','-OutputPath',$reportPath)) { $start.ArgumentList.Add($arg) }
        if ($slot.heap) { $start.ArgumentList.Add('-Heap') }
        $process = [Diagnostics.Process]::Start($start)
        $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($remainingMs)
        if ($timedOut) { $process.Kill($true); [void]$process.WaitForExit(5000) }
        [IO.File]::WriteAllText((Join-Path $OutputDirectory ($stem + '.runner.log')),$stdout.GetAwaiter().GetResult())
        [IO.File]::WriteAllText((Join-Path $OutputDirectory ($stem + '.runner.err')),$stderr.GetAwaiter().GetResult())
        $exitCode=$process.ExitCode; $process.Dispose()
        $identityMatched=$false; $identityError=''
        if ($exitCode -eq 0 -and -not $timedOut) {
            try {
                $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json -AsHashtable -Depth 32
                $state = [ordered]@{}
                foreach ($key in $anchors[$slot.role].Keys) { $state[$key]=$report[$key] }
                Assert-PerformanceMeasurementState -Before $anchors[$slot.role] -After $state
                $identityMatched=$true
            } catch { $identityError=$_.Exception.Message }
        }
        $results.Add([ordered]@{ index=$index+1; role=$slot.role; heap=$slot.heap; report=$stem+'.json'; exitCode=$exitCode
            timedOut=$timedOut; identityMatched=$identityMatched; identityError=$identityError; completedUtc=[DateTime]::UtcNow.ToString('o') })
        if ($timedOut -or $identityError -or $exitCode -ne 0) { break }
    }
} finally {
    [ordered]@{ scheduled=8; attempted=$results.Count; elapsedSeconds=([DateTime]::UtcNow-$started).TotalSeconds
        results=$results.ToArray(); acceptance='not-established' } | ConvertTo-Json -Depth 8 |
        Set-Content (Join-Path $OutputDirectory 'completion.json') -Encoding utf8
}
if ($results.Count -ne 8 -or @($results | Where-Object { $_.exitCode -ne 0 -or $_.timedOut -or -not $_.identityMatched }).Count) {
    throw 'Incomplete diagnostic inventory; retain failures and unattempted slots without replacement.'
}
Write-Host 'All eight declared profiles completed; diagnostic only.'
