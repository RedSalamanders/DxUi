<# .SYNOPSIS Validates that every inherited test has a retained entry point or an explicit exclusion reason. #>
[CmdletBinding()] param()
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Tools/Validation.psm1') -Force
Complete-DxUiValidation (Test-DxUiTestPort $PSScriptRoot) 'Inherited-test'
