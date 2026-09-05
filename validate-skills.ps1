<# .SYNOPSIS Validates every repository-local DxUi skill. #>
[CmdletBinding()] param()
& (Join-Path $PSScriptRoot 'Tools/Invoke-Python.ps1') -Script (Join-Path $PSScriptRoot 'Tools/validate_skills.py')
