# Uses the repository's unchanged paired runner. Setup must already have rebuilt both
# Release outputs with TEMP/TMP outside both trees and passed full identity preflight.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $RepositoryRoot,
    [Parameter(Mandatory)][string] $BaselineRoot,
    [Parameter(Mandatory)][string] $OutputDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$RepositoryRoot=(Resolve-Path -LiteralPath $RepositoryRoot).ProviderPath
$BaselineRoot=(Resolve-Path -LiteralPath $BaselineRoot).ProviderPath
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'The study requires a fresh output directory.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
Import-Module (Join-Path $RepositoryRoot 'Tools/PairedRun.psm1') -Force
$seed=[Security.Cryptography.RandomNumberGenerator]::GetInt32(0,[int]::MaxValue)
$schedule=Get-RandomizedPairedBlockSchedule -Blocks 12 -Seed $seed
$metadata=[ordered]@{ classification='focused-Default-diagnostic'; baselineRoot=$BaselineRoot; candidateRoot=$RepositoryRoot
    startedUtc=[DateTime]::UtcNow.ToString('o'); budgetSeconds=2400; seed=$seed; schedule=$schedule
    runtimeTemp=$env:TEMP; runtimeTmp=$env:TMP; timedOut=$false; acceptance='not-established' }
$metadata | ConvertTo-Json -Depth 12 | Set-Content (Join-Path $OutputDirectory 'runner.json') -Encoding utf8
$start=[Diagnostics.ProcessStartInfo]::new((Get-Command pwsh).Source)
$start.UseShellExecute=$false; $start.CreateNoWindow=$true
$start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
$start.WorkingDirectory=$RepositoryRoot
foreach ($argument in @('-NoProfile','-NonInteractive','-File',(Join-Path $RepositoryRoot 'performance-paired.ps1'),
    '-BaselinePath',$BaselineRoot,'-Configuration','Release','-Platform','x64','-Scenario','Default','-Blocks','12',
    '-Seed',[string]$seed,'-SkipBuild','-OutputDirectory',$OutputDirectory)) { $start.ArgumentList.Add($argument) }
$stdoutFile=[IO.File]::Create((Join-Path $OutputDirectory 'runner.log'))
$stderrFile=[IO.File]::Create((Join-Path $OutputDirectory 'runner.err'))
$process=$null
try {
    $process=[Diagnostics.Process]::Start($start)
    $metadata['processId']=$process.Id
    $metadata | ConvertTo-Json -Depth 12 | Set-Content (Join-Path $OutputDirectory 'runner.json') -Encoding utf8
    $stdoutTask=$process.StandardOutput.BaseStream.CopyToAsync($stdoutFile)
    $stderrTask=$process.StandardError.BaseStream.CopyToAsync($stderrFile)
    $metadata.timedOut=-not $process.WaitForExit(2400000)
    if ($metadata.timedOut) { $process.Kill($true); [void]$process.WaitForExit(5000) }
    $stdoutTask.GetAwaiter().GetResult(); $stderrTask.GetAwaiter().GetResult()
    $metadata['exitCode']=$process.ExitCode
} finally {
    $metadata['completedUtc']=[DateTime]::UtcNow.ToString('o')
    $metadata | ConvertTo-Json -Depth 12 | Set-Content (Join-Path $OutputDirectory 'runner.json') -Encoding utf8
    $stdoutFile.Dispose(); $stderrFile.Dispose()
    if ($process) { $process.Dispose() }
}
if ($metadata.timedOut -or $metadata.exitCode -ne 0) { throw 'The fixed owned study failed or exceeded its budget; preserve every partial report.' }
$summaryPath=@(Get-ChildItem -LiteralPath $OutputDirectory -Recurse -Filter summary.json -File)
if ($summaryPath.Count -ne 1) { throw 'Expected one completed study summary.' }
$summary=Get-Content $summaryPath[0].FullName -Raw | ConvertFrom-Json -AsHashtable -Depth 32
if ($summary.seed -ne $seed -or $summary.order -cne $schedule.Order) { throw 'Actual runner assignment differs from its predeclared schedule.' }
Write-Host "Completed the fixed Default study: $($summaryPath[0].FullName)"
