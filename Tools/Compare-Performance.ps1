<# .SYNOPSIS Compare a complex-UI receipt with an optional matched baseline and write the comparison JSON.
Exits 1 when a regression needs developer advice or the evidence is invalid; the written status is the result. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string] $Candidate, [string] $Baseline = '', [Parameter(Mandatory)][string] $Output)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'PerformanceComparison.psm1') -Force
exit (Invoke-PerformanceComparison -Candidate $Candidate -Baseline $Baseline -Output $Output)
