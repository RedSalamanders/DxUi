[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$baseline = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'final-native-compiled-closure.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable
Import-Module (Join-Path $root 'Tools/ScopedTesting.psm1') -Force
Import-Module (Join-Path $root 'Tools/PerformancePolicy.psm1') -Force
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Preserve the previous archival comparison.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$currentClosure = Get-ScopedSourceIdentity -Root $root -CompiledOnly
if ($currentClosure -cne $baseline.compiledSourceIdentity) { throw 'Archival changed compiled source inputs.' }
$checks = [Collections.Generic.List[object]]::new()
foreach ($configuration in @('Debug','Release','ASan Debug')) {
    $tag = $configuration.Replace(' ','')
    $priorPath = Join-Path $PSScriptRoot "focus-integration-$tag-attempt3/post-focus.state.receipt.txt"
    $prior = Get-Content -LiteralPath $priorPath -Raw | ConvertFrom-Json -AsHashtable -Depth 32
    if ($prior.sourceCommit -cne $baseline.sourceCommit -or $prior.sourceDirty) { throw 'Require the clean final native-run identity.' }
    $current = Get-PerformanceMeasurementState -Root $root -Platform x64 -Configuration $configuration
    $currentPath = Join-Path $OutputDirectory "$tag.state.receipt.txt"
    [IO.File]::WriteAllText($currentPath,((ConvertTo-Json -InputObject $current -Depth 32)+[char]10),[Text.UTF8Encoding]::new($false))
    $comparedPrior = [ordered]@{}
    $comparedCurrent = [ordered]@{}
    foreach ($key in $prior.Keys) {
        if ($key -cin @('sourceCommit','sourceDirty')) { continue }
        if (-not $current.Contains($key)) { throw "Missing archival field: $key" }
        $comparedPrior[$key] = $prior[$key]
        $comparedCurrent[$key] = $current[$key]
    }
    if ($prior.Count -ne $current.Count) { throw 'Archival changed the actual state schema.' }
    Assert-PerformanceMeasurementState -Before $comparedPrior -After $comparedCurrent
    $checks.Add([ordered]@{configuration=$configuration; nativeSourceCommit=$prior.sourceCommit; archivalSourceCommit=$current.sourceCommit;
        archivalDirty=$current.sourceDirty; comparedActualFields=$comparedPrior.Count; allowedProseArchivalDifferences=@('sourceCommit','sourceDirty');
        originalStateSha256=(Get-FileHash -LiteralPath $priorPath -Algorithm SHA256).Hash;
        currentStateSha256=(Get-FileHash -LiteralPath $currentPath -Algorithm SHA256).Hash;
        compiledSourceIdentity=$currentClosure; actualCompilerSdkDependencyArtifactEnvironmentUnchanged=$true})
    Write-Output "PASS archival compiled inputs and actual $configuration toolchain/dependency/artifact/environment identities."
}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'comparison.receipt.txt'),((ConvertTo-Json -InputObject $checks.ToArray() -Depth 12)+[char]10),[Text.UTF8Encoding]::new($false))
