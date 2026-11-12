<#
.SYNOPSIS
Captures a built local Release dependency as an explicitly unpinned immutable snapshot.
.PARAMETER ProducerRoot
DxUi checkout containing an unchanged current Release library/PDB build receipt. Dirty source is allowed only when that exact dirty state was built. Defaults to this checkout.
.PARAMETER Platform
The built x64 or ARM64 profile; this command does not compile or claim native execution.
.PARAMETER StoreRoot
Optional marked SDK store beneath this tooling checkout's .build. Defaults to .build/local-sdk-snapshots and initializes it only when empty and unowned.
.OUTPUTS
Create-only local-unpinned descriptor and retained payload staging below .build/local-sdk-capture; immutable store bytes and input identity. This is not an SDK qualification, package or test result.
.NOTES
Requires byte-identified local producer tooling, its exact clean pinned engine, current canonical Release library/PDB provenance, restored dependencies and matching actual compiler/profile. Dirty tooling has a distinct local-only role refused by pinned source descriptors. Holds only the producer profile lease through source/profile/byte barriers. Never changes source, locks or ordinary consumer outputs. Failure retains staging. Hosted qualification/publication refuses this command. Consumer linking remains unavailable until its separate local-only adapter is implemented.
.EXAMPLE
./Tools/New-LocalSdkSnapshot.ps1 -ProducerRoot Z:/src/DxUi -Platform x64
#>
[CmdletBinding()]
param([string]$ProducerRoot=(Join-Path $PSScriptRoot '..'),[Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform,[string]$StoreRoot='')
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'SdkLocalCandidate.psm1') -Force
New-DxUiLocalSdkSnapshot -ProducerRoot $ProducerRoot -Platform $Platform -StoreRoot $StoreRoot
