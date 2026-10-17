[CmdletBinding()] param([string] $Root, [string] $BaselineRoot, [string] $OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $Root 'Tools/PairedRun.psm1') -Force
$paths=@((Get-PairedHarness) + @('Tests/Embedded/Scope.Tests.ComplexUiProfiling.h','Tests/Embedded/Scope.Tests.ComplexUiBoundaries.h',
    'Tests/Embedded/Embedded.Tests.Embedded.cpp','performance-profile.ps1','Tools/PerformanceProfiling.psm1','Tools/ScopedTesting.psm1') | Sort-Object -Unique)
$backup=Join-Path $OutputDirectory 'overlay-backup'
if (Test-Path $backup) { throw 'Setup backup already exists; retain and restore it before a new setup.' }
$records=@(Copy-HarnessOverlay -Source $Root -Target $BaselineRoot -Paths $paths -BackupDirectory $backup)
$records | ConvertTo-Json -Depth 12 | Set-Content (Join-Path $OutputDirectory 'overlay-records.json') -Encoding utf8
Assert-HarnessOverlay -Target $BaselineRoot -Records $records
& (Join-Path $BaselineRoot 'build.ps1') -Configuration Release -Platform x64
if ($LASTEXITCODE) { throw "Baseline diagnostic build failed: $LASTEXITCODE" }
Assert-HarnessOverlay -Target $BaselineRoot -Records $records

