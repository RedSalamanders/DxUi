<# .SYNOPSIS Validates source provenance, pins and active-source independence. #>
[CmdletBinding()] param()
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Tools/Validation.psm1') -Force
Complete-DxUiValidation (Test-DxUiDependencies $PSScriptRoot) 'Dependency'
