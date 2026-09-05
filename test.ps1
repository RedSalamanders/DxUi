<# .SYNOPSIS Builds and runs the extracted Foundation tests, with an execution receipt. #>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string] $Configuration = 'Debug',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [switch] $SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$nativeArchitecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
if (($Platform -eq 'ARM64') -and ($nativeArchitecture -ne 'Arm64')) {
    throw 'ARM64 runtime tests require an ARM64 host. Use build.ps1 for cross-compilation.'
}
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform }
$executable = Join-Path $PSScriptRoot ".build/$Platform/$Configuration/DxUi.FoundationTests.exe"
if (-not (Test-Path -LiteralPath $executable)) { throw "Test executable is missing: $executable" }
& $executable
$testExit = $LASTEXITCODE
$reports = Join-Path $PSScriptRoot '.build/reports'
New-Item -ItemType Directory -Path $reports -Force | Out-Null
$receipt = [ordered]@{
    suite = 'Foundation'; configuration = $Configuration; platform = $Platform; nativeArchitecture = $nativeArchitecture
    completedUtc = [DateTime]::UtcNow.ToString('o'); executable = $executable
    sha256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash; exitCode = $testExit
}
$receipt | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $reports "Foundation-$Platform-$Configuration.json") -Encoding utf8
if ($testExit -ne 0) { throw "Foundation tests failed: $testExit" }
