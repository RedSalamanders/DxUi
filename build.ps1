<#
.SYNOPSIS Builds the currently supported DxUi static targets and tests.
.PARAMETER Configuration Debug or Release.
.PARAMETER Platform x64 or ARM64.
.PARAMETER OutputRoot Optional isolated output root; defaults to this checkout's .build directory.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string] $Configuration = 'Debug',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [string] $OutputRoot = '',
    [switch] $Rebuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$vswhereCandidates = @(
    (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'),
    (Join-Path $env:ProgramFiles 'Microsoft Visual Studio/Installer/vswhere.exe')
)
$vswhere = $vswhereCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $vswhere) { throw 'Visual Studio Installer/vswhere.exe was not found.' }
$installation = & $vswhere -latest -prerelease -products '*' -requires Microsoft.Component.MSBuild -property installationPath
if (-not $installation) { throw 'Visual Studio with MSBuild was not found; install VS 2026 C++ tools.' }
$msbuild = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild)) { throw "MSBuild is missing: $msbuild" }
if (-not $OutputRoot) { $OutputRoot = Join-Path $PSScriptRoot '.build' }
$output = [IO.Path]::GetFullPath($OutputRoot).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
$executables = @('DxUi.FoundationTests.exe','DxUi.ControlTests.exe','DxUi.EmbeddedTests.exe','DxUi.EmbeddedControls.exe')
foreach ($name in $executables) {
    $expected = [IO.Path]::GetFullPath((Join-Path $output "$Platform/$Configuration/$name"))
    foreach ($process in (Get-CimInstance Win32_Process -Filter "Name = '$name'")) {
        if (-not $process.ExecutablePath) { throw "Cannot inspect running $name; no process was terminated." }
        if ([IO.Path]::GetFullPath($process.ExecutablePath) -eq $expected) {
            throw "Build output is running: PID $($process.ProcessId), $($process.ExecutablePath). No process was terminated."
        }
    }
}
$triplet = if ($Platform -eq 'ARM64') { 'arm64-windows' } else { 'x64-windows' }
if (-not (Test-Path -LiteralPath (Join-Path $output "vcpkg_installed/$Platform/$triplet/include/wil/resource.h"))) {
    throw "Restore dependencies first: vcpkg-install.ps1 -Platform $Platform -OutputRoot `"$output`""
}
$logDirectory = Join-Path $output 'logs'
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
$log = Join-Path $logDirectory ("build-$Platform-$Configuration-" + [guid]::NewGuid().ToString('N') + '.log')
$target = if ($Rebuild) { 'Rebuild' } else { 'Build' }
$arguments = @((Join-Path $PSScriptRoot 'DxUi.sln'), '/m', '/nologo', '/verbosity:minimal', "/t:$target",
    "/p:Configuration=$Configuration", "/p:Platform=$Platform", "/p:DxUiOutputRoot=$output",
    "/flp:logfile=$log;verbosity=normal;encoding=UTF-8")
Write-Host "DxUi $Configuration $Platform | $installation"
& $msbuild @arguments
if ($LASTEXITCODE -ne 0) { throw "DxUi build failed. Log: $log" }
Write-Host "Build passed. Log: $log"
