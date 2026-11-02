<#
.SYNOPSIS Generates one release heading from the existing folded Unreleased notes.
.DESCRIPTION Requires an already fully folded changelog, preserves previous release history and atomically publishes the
exact version/date transition. The result is reviewed as a distinct release change; feature changes never run this command.
.PARAMETER Version Explicit semantic version, such as 1.2.3.
.PARAMETER Date Explicit release date in yyyy-MM-dd.
.PARAMETER Root Repository root; defaults to this checkout.
.OUTPUTS CHANGELOG.md only, unless WhatIf is specified. No staging, commit, network, build or desktop action.
.EXAMPLE
./Tools/Release-Changelog.ps1 -Version 1.2.3 -Date 2026-11-01 -WhatIf
#>
[CmdletBinding(SupportsShouldProcess,ConfirmImpact='Low')]
param([Parameter(Mandatory)][string]$Version,[Parameter(Mandatory)][string]$Date,
      [string]$Root=(Split-Path $PSScriptRoot -Parent))
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'Modules/Documentation/Changelog.psm1') -Force
$Root=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Root)
Invoke-ChangelogRelease -Root $Root -Version $Version -Date $Date -CheckOnly
if($PSCmdlet.ShouldProcess($Root,"Generate release $Version dated $Date")) {
    Invoke-ChangelogRelease -Root $Root -Version $Version -Date $Date
    Write-Host "Generated changelog release $Version ($Date)."
}
