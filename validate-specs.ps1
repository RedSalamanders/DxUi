<# .SYNOPSIS Validates specification authority, indexes and local references. #>
[CmdletBinding()] param()
& (Join-Path $PSScriptRoot 'Tools/Invoke-Python.ps1') -Script (Join-Path $PSScriptRoot 'Tools/validate_specs.py')
