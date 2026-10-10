# Fixed diagnostic schedule. Never extends an inventory to replace a failure or outlier.
[CmdletBinding()]
param([Parameter(Mandatory)][string] $RepositoryRoot, [Parameter(Mandatory)][string] $OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Use a fresh pilot output directory; prior evidence must remain intact.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$schedule = @(for ($block = 0; $block -lt 3; $block++) {
    $order = if ($block % 2) { @(3,2,1,0) } else { @(0,1,2,3) }
    foreach ($slot in $order) {
        [ordered]@{ block=$block; scenario=$(if ($slot % 2) { 'MultilineGrid' } else { 'Default' }); heap=($slot -ge 2) }
    }
})
Import-Module (Join-Path $RepositoryRoot 'Tools/PerformancePolicy.psm1') -Force
Import-Module (Join-Path $RepositoryRoot 'Tools/PerformanceProfiling.psm1') -Force
$anchor = Get-ProfilingMeasurementState -Root $RepositoryRoot -Platform x64 -Configuration Release
$started = [DateTime]::UtcNow
$plan = [ordered]@{ purpose='bounded-diagnostic-only'; sourceCommit=$anchor.sourceCommit; executableSha256=$anchor.executableSha256; anchor=$anchor
    configuration='Release'; platform='x64'; reportBudget=12; overallBudgetSeconds=1200; ownedChildBudgetSeconds=120
    startedUtc=$started.ToString('o'); schedule=$schedule; acceptance='not-established'
    notes='Serial fresh processes; fixed inventory, forward/reverse/forward blocks with three reports per scenario/heap combination, no failure/outlier replacement. Every successful report must match the initial full v2 source/tool/dependency/artifact/environment and profiling-input anchor. The 20-minute limit stops starting reports; an in-flight guarded report may extend completion. No dedicated-host, actual-scheduler or policy qualification claim.' }
$encoding = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'pilot-plan.json'), ((ConvertTo-Json $plan -Depth 32) + "`n"), $encoding)
$results = [Collections.Generic.List[object]]::new()
for ($index = 0; $index -lt $schedule.Count; $index++) {
    if (([DateTime]::UtcNow - $started).TotalSeconds -ge 1200) { break }
    $item = $schedule[$index]
    $stem = 'profile-{0:d2}-{1}-{2}' -f ($index + 1), $item.scenario, $(if ($item.heap) { 'Heap' } else { 'NoHeap' })
    $output = Join-Path $OutputDirectory ($stem + '.json')
    Write-Host "Diagnostic pilot $($index + 1)/12: $($item.scenario), heap=$($item.heap)"
    $arguments = @('-NoLogo','-NoProfile','-File',(Join-Path $RepositoryRoot 'performance-profile.ps1'),'-Configuration','Release',
        '-Platform','x64','-Scenario',$item.scenario,'-SkipBuild','-OutputPath',$output)
    if ($item.heap) { $arguments += '-Heap' }
    $global:LASTEXITCODE = 0
    & pwsh @arguments *> (Join-Path $OutputDirectory ($stem + '.runner.log'))
    $childExit = $LASTEXITCODE
    $identityError = ''
    $identityMatched = $false
    if ($childExit -eq 0) {
        try {
            $report = Get-Content -LiteralPath $output -Raw | ConvertFrom-Json -AsHashtable -Depth 32
            $reportState = [ordered]@{}
            foreach ($key in $anchor.Keys) { $reportState[$key] = $report[$key] }
            Assert-PerformanceMeasurementState -Before $anchor -After $reportState
            $identityMatched = $true
        } catch { $identityError = $_.Exception.Message }
    }
    $results.Add([ordered]@{ index=$index + 1; scenario=$item.scenario; heap=$item.heap; exitCode=$childExit
        report=$stem + '.json'; identityMatched=$identityMatched; identityError=$identityError; completedUtc=[DateTime]::UtcNow.ToString('o') })
    if ($identityError) { break }
}
$completion = [ordered]@{ scheduled=12; attempted=$results.Count; wallSeconds=([DateTime]::UtcNow - $started).TotalSeconds
    results=$results.ToArray(); acceptance='not-established' }
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'pilot-completion.json'), ((ConvertTo-Json $completion -Depth 8) + "`n"), $encoding)
if ($results.Count -ne 12 -or @($results | Where-Object { $_.exitCode -ne 0 -or -not $_.identityMatched }).Count) { throw 'The fixed diagnostic inventory contains a failed, identity-mismatched or unattempted report. Retain every result; do not silently rerun.' }
Write-Host "Completed all twelve declared diagnostic reports in $([Math]::Round($completion.wallSeconds,1)) seconds."
