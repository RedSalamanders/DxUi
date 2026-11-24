<#
.SYNOPSIS
Restores the pinned DxUi build dependencies for one architecture.
.PARAMETER Platform
x64 or ARM64 target architecture.
#>
#Requires -Version 7.0
[CmdletBinding()]
param([ValidateSet('x64','ARM64')][string]$Platform='x64')

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$binarySources=$env:VCPKG_BINARY_SOURCES
try {
    $env:VCPKG_BINARY_SOURCES='clear'
    & (Join-Path $PSScriptRoot 'vcpkg-install.ps1') -Platform $Platform
} finally {
    $env:VCPKG_BINARY_SOURCES=$binarySources
}
