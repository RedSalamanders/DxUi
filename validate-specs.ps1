<# .SYNOPSIS Validates specification authority, indexes and local references. #>
[CmdletBinding()] param()
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Tools/Validation.psm1') -Force
Complete-DxUiValidation (Test-DxUiSpecs $PSScriptRoot) 'Specification'
