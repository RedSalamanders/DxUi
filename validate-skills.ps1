<# .SYNOPSIS Validates every repository-local DxUi skill. #>
[CmdletBinding()] param()
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Tools/Validation.psm1') -Force
Complete-DxUiValidation (Test-DxUiSkills $PSScriptRoot) 'Skill'
