<# .SYNOPSIS Validates source provenance, pins and active-source independence. #>
[CmdletBinding()] param()
& (Join-Path $PSScriptRoot 'Tools/Invoke-Python.ps1') -Script (Join-Path $PSScriptRoot 'Tools/validate_dependencies.py')
