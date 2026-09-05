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

function Test-DxUiInteractiveTerminal {
    try {
        $hasRawUi = $null -ne $Host -and $null -ne $Host.UI -and $null -ne $Host.UI.RawUI
        if (-not $hasRawUi) { return $false }

        try { $null = $Host.UI.RawUI.WindowTitle }
        catch { return $false }

        if (-not [Console]::IsOutputRedirected -and -not [Console]::IsErrorRedirected) { return $true }

        foreach ($marker in @('CODEX_SHELL','WT_SESSION','TERM_PROGRAM','VSCODE_PID','ConEmuPID','ANSICON')) {
            $value = [Environment]::GetEnvironmentVariable($marker)
            if (-not [string]::IsNullOrWhiteSpace($value)) { return $true }
        }
    }
    catch {
        Write-Verbose "Interactive terminal detection failed: $($_.Exception.Message)"
    }

    return $false
}

function Write-DxUiBuildBanner {
    param(
        [Parameter(Mandatory)]
        [string] $Configuration,

        [Parameter(Mandatory)]
        [string] $Platform,

        [Parameter(Mandatory)]
        [string] $Target
    )

    $contentWidth = 72
    $rows = @(
        @{ Text = ''; Color = 'DarkCyan' },
        @{ Text = '██████╗ ██╗  ██╗██╗   ██╗██╗'; Color = 'DarkCyan' },
        @{ Text = '██╔══██╗╚██╗██╔╝██║   ██║██║'; Color = 'Cyan' },
        @{ Text = '██║  ██║ ╚███╔╝ ██║   ██║██║'; Color = 'Green' },
        @{ Text = '██║  ██║ ██╔██╗ ██║   ██║██║'; Color = 'Yellow' },
        @{ Text = '██████╔╝██╔╝ ██╗╚██████╔╝██║'; Color = 'Magenta' },
        @{ Text = '╚═════╝ ╚═╝  ╚═╝ ╚═════╝ ╚═╝'; Color = 'DarkMagenta' },
        @{ Text = ''; Color = 'DarkMagenta' },
        @{ Text = '✦  PIXELS PRIMED  •  CONTROLS READY  •  COMPILE MODE: GO!  ✦'; Color = 'White' },
        @{ Text = "⚡  $($Configuration.ToUpperInvariant())  /  $($Platform.ToUpperInvariant())  /  $($Target.ToUpperInvariant())  ⚡"; Color = 'Yellow' },
        @{ Text = ''; Color = 'DarkCyan' }
    )

    $border = '─' * $contentWidth
    if ($script:UseInteractiveTerminal) { Write-Host "╭$border╮" -ForegroundColor DarkCyan }
    else { Write-Host "╭$border╮" }

    foreach ($row in $rows) {
        $padding = $contentWidth - $row.Text.Length
        $leftPadding = [Math]::Floor($padding / 2)
        $rightPadding = $padding - $leftPadding
        $line = '│' + (' ' * $leftPadding) + $row.Text + (' ' * $rightPadding) + '│'
        if ($script:UseInteractiveTerminal) { Write-Host $line -ForegroundColor $row.Color }
        else { Write-Host $line }
    }

    if ($script:UseInteractiveTerminal) { Write-Host "╰$border╯" -ForegroundColor DarkCyan }
    else { Write-Host "╰$border╯" }
    Write-Host ''
}

function Write-DxUiMSBuildLine {
    param(
        [AllowEmptyString()]
        [string] $Line
    )

    $color = if ($Line -match '(?i)(^|\s)(fatal\s+error|error\s+([A-Z]+)?\d+|MSBUILD\s*:\s*error)\b') {
        'Red'
    }
    elseif ($Line -match '(?i)(^|\s)warning\s+([A-Z]+)?\d+\b') {
        'Yellow'
    }
    elseif ($Line -match '(?i)\.(vcxproj|vcproj|sln)\s+->\s+') {
        'Green'
    }
    else {
        $null
    }

    if ($script:UseInteractiveTerminal -and $color) { Write-Host $Line -ForegroundColor $color }
    else { Write-Host $Line }
}

$script:UseInteractiveTerminal = Test-DxUiInteractiveTerminal
$target = if ($Rebuild) { 'Rebuild' } else { 'Build' }
Write-DxUiBuildBanner -Configuration $Configuration -Platform $Platform -Target $target
Write-Host 'Locating Visual Studio and MSBuild...' -ForegroundColor Yellow
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
$arguments = @((Join-Path $PSScriptRoot 'DxUi.sln'), '/m', '/nologo', '/verbosity:minimal', "/t:$target",
    "/p:Configuration=$Configuration", "/p:Platform=$Platform", "/p:DxUiOutputRoot=$output",
    "/flp:logfile=$log;verbosity=normal;encoding=UTF-8")
Write-Host 'Build configuration:' -ForegroundColor Cyan
Write-Host "  Configuration: $Configuration"
Write-Host "  Platform:      $Platform"
Write-Host "  Target:        $target"
Write-Host "  Toolchain:     $installation" -ForegroundColor DarkGray
Write-Host "  Output:        $output" -ForegroundColor DarkGray
Write-Host ''
Write-Host 'Compiling DxUi...' -ForegroundColor Yellow
$stopwatch = [Diagnostics.Stopwatch]::StartNew()
& $msbuild @arguments 2>&1 | ForEach-Object { Write-DxUiMSBuildLine -Line $_.ToString() }
$buildExitCode = $LASTEXITCODE
$stopwatch.Stop()
if ($buildExitCode -ne 0) {
    Write-Host ''
    Write-Host '========================================' -ForegroundColor Red
    Write-Host 'DxUi build failed' -ForegroundColor Red
    Write-Host "Exit code: $buildExitCode" -ForegroundColor Red
    Write-Host "Build time: $($stopwatch.Elapsed.ToString('mm\:ss'))" -ForegroundColor Red
    Write-Host "Log: $log" -ForegroundColor DarkGray
    Write-Host '========================================' -ForegroundColor Red
    throw "DxUi build failed. Log: $log"
}

Write-Host ''
Write-Host '========================================' -ForegroundColor Green
Write-Host "DxUi build passed in $($stopwatch.Elapsed.ToString('mm\:ss'))" -ForegroundColor Green
Write-Host "Output: $output" -ForegroundColor Cyan
Write-Host "Log: $log" -ForegroundColor DarkGray
Write-Host '========================================' -ForegroundColor Green
