# Portable integration regression for the retained batch drivers; no native work or measurement.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Import-Module (Join-Path $repositoryRoot 'Tools/tests/TestSupport.psm1') -Force
$shellPath = (Get-Process -Id $PID).Path
$drivers = @('Measurements/Performance/2026-10-10-profiling/Invoke-Pilot.ps1',
    'Measurements/Performance/2026-10-10-profiling/Analyze.ps1',
    'Measurements/UiaSelection/2026-10-10/Invoke-Reproduction.ps1')
foreach ($driver in $drivers) {
    Invoke-FixtureCase "$driver keeps identity guards callable after real module imports" {
        param($root)
        $tokens = $null; $errors = $null
        $ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $repositoryRoot $driver), [ref]$tokens, [ref]$errors)
        Assert-Equal 0 $errors.Count 'driver parses'
        $imports = @($ast.FindAll({ param($node)
            $node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -ceq 'Import-Module'
        }, $true) | ForEach-Object { $_.Extent.Text })
        Assert-True ($imports.Count -gt 0) 'actual driver imports found'
        $body = @'
param([string] $RepositoryRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
{IMPORTS}
$before = [ordered]@{ sourceCommit='original'; sourceDirty=$false; profilingInputs=[ordered]@{ fixture='same'; flags=@('a','b') } }
$after = ConvertTo-Json $before -Depth 8 | ConvertFrom-Json -AsHashtable
Assert-PerformanceMeasurementState -Before $before -After $after
$after.sourceCommit = 'changed'
$rejected = $false
try { Assert-PerformanceMeasurementState -Before $before -After $after }
catch {
    if ($_.Exception.Message -notlike 'Benchmark sourceCommit changed*') { throw }
    $rejected = $true
}
if (-not $rejected) { throw 'Changed source identity was accepted.' }
Write-Output 'equal identity accepted; changed identity rejected'
'@
        Set-FixtureFile $root 'imports.ps1' ($body.Replace('{IMPORTS}', ($imports -join "`n")))
        $output = @(& $shellPath -NoProfile -File (Join-Path $root 'imports.ps1') $repositoryRoot 2>&1 | ForEach-Object { "$_" })
        Assert-Equal 0 $LASTEXITCODE ($output -join "`n")
        Assert-True ($output -contains 'equal identity accepted; changed identity rejected') 'both guard outcomes executed'
    }
}
Complete-TestRun 'ProfilingDriverImports'
