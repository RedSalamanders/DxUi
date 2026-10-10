<# .SYNOPSIS Validates that every live native project and solution maps the six configurations.
.PARAMETER Root Repository to audit; defaults to this one, and may name a consumer checkout.
Build definitions do not establish instrumentation or native runtime qualification. #>
[CmdletBinding()] param([string] $Root = $PSScriptRoot)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Tools/Validation.psm1') -Force
if (-not [IO.Path]::IsPathRooted($Root)) { $Root = [IO.Path]::Combine((Get-Location).ProviderPath, $Root) }
$Root = [IO.Path]::GetFullPath($Root)
Complete-DxUiValidation (Test-DxUiBuildMatrix $Root) 'Build-matrix'
