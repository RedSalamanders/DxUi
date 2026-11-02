<# .SYNOPSIS Runs one tooling test with an isolated PowerShell session and propagates its final status. #>
[CmdletBinding()] param([Parameter(Mandatory)][string] $ScriptPath)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$global:LASTEXITCODE = 0
try {
    & $ScriptPath
    exit $global:LASTEXITCODE
} catch {
    [Console]::Error.WriteLine($_.ToString())
    exit 1
}
