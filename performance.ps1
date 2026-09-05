<# .SYNOPSIS Measure completed complex-UI WARP frames and resources, optionally comparing a matched baseline. #>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string] $Configuration = 'Release',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [string] $OutputPath = '',
    [string] $Baseline = '',
    [switch] $SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$native = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
if (($Platform -eq 'ARM64' -and $native -ne 'Arm64') -or ($Platform -eq 'x64' -and $native -ne 'X64')) {
    throw 'Performance evidence requires native execution matching the requested architecture.'
}
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform }
if (-not $OutputPath) { $OutputPath = Join-Path $PSScriptRoot ".build/reports/Performance-$Platform-$Configuration.json" }
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
if ($Baseline) {
    $Baseline = [IO.Path]::GetFullPath($Baseline)
    if (-not (Test-Path -LiteralPath $Baseline -PathType Leaf)) { throw "Missing performance baseline: $Baseline" }
    if ($Baseline -eq $OutputPath) { throw 'Baseline and candidate paths must differ.' }
}
New-Item -ItemType Directory -Path (Split-Path $OutputPath) -Force | Out-Null
$exe = Join-Path $PSScriptRoot ".build/$Platform/$Configuration/DxUi.EmbeddedTests.exe"
Push-Location $PSScriptRoot
try {
    & $exe --benchmark $OutputPath
    if ($LASTEXITCODE -ne 0) { throw 'Complex-UI benchmark failed.' }
    $receipt = Get-Content -Raw -LiteralPath $OutputPath | ConvertFrom-Json -AsHashtable
    $receipt.platform = $Platform
    $receipt.configuration = $Configuration
    $receipt.nativeArchitecture = $native
    $receipt.machine = [Environment]::MachineName
    $receipt.cpu = (Get-ItemProperty -LiteralPath 'HKLM:\HARDWARE\DESCRIPTION\System\CentralProcessor\0' -Name ProcessorNameString).ProcessorNameString.Trim()
    $receipt.os = [System.Runtime.InteropServices.RuntimeInformation]::OSDescription
    $receipt.warpVersion = (Get-Item -LiteralPath (Join-Path $env:SystemRoot 'System32/d3d10warp.dll')).VersionInfo.FileVersion
    $receipt.powerPolicy = ((& powercfg.exe /getactivescheme) -join ' ').Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the active power policy.' }
    $receipt.buildSkipped = [bool]$SkipBuild
    $receipt.completedUtc = [DateTime]::UtcNow.ToString('o')
    $receipt.sourceCommit = (& git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Cannot identify source commit.' }
    $receipt.sourceDirty = [bool](& git status --porcelain)
    $sources = @(& git ls-files --cached --others --exclude-standard -- src include Build Directory.Build.props Directory.Build.targets vcpkg.json vcpkg-tool.json)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot enumerate library inputs.' }
    $hashes = foreach ($source in ($sources | Sort-Object -Unique)) { "$source $((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash)" }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $receipt.sourceFingerprint = [Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($hashes -join "`n")))) }
    finally { $sha.Dispose() }
    $receipt.executableSha256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    $receipt.benchmarkSha256 = (Get-FileHash -LiteralPath Tests/Embedded/ComplexUiBenchmark.h -Algorithm SHA256).Hash
    $receipt.measurement = 'Completed offscreen WARP frames, including clear and blocking one-pixel readback; no swap-chain presentation or vsync.'
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputPath -Encoding utf8
    $comparison = $OutputPath + '.comparison.json'
    $arguments = @($OutputPath, '--output', $comparison)
    if ($Baseline) { $arguments += @('--baseline', [IO.Path]::GetFullPath($Baseline)) }
    & (Join-Path $PSScriptRoot 'Tools/Invoke-Python.ps1') -Script (Join-Path $PSScriptRoot 'Tools/compare_performance.py') -Arguments $arguments
    Write-Host "Performance receipt: $OutputPath"
} finally { Pop-Location }
