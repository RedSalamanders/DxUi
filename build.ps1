<#
.SYNOPSIS Builds the currently supported DxUi static targets and tests.
.DESCRIPTION Prints bounded BEGIN/END/FAIL timing for six top-level build phases. UTC timestamps and monotonic elapsed
milliseconds diagnose build orchestration; they are not native workload measurements or performance acceptance.
.PARAMETER Configuration Debug, Release or ASan Debug.
.PARAMETER Platform x64 or ARM64.
.PARAMETER OutputRoot Optional isolated output root; defaults to this checkout's .build directory.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Debug',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [string] $OutputRoot = '',
    [switch] $Rebuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$nativeEntryElapsed = [Diagnostics.Stopwatch]::StartNew()
Import-Module (Join-Path $PSScriptRoot 'Build/BuildConsole.psm1') -ErrorAction Stop

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
        $displayLength = $row.Text.Length
        # Windows terminals render each ⚡ glyph as two columns, although PowerShell counts it as one character.
        if ($row.Text.Contains('⚡')) {
            $displayLength += 2
        }
        $padding = $contentWidth - $displayLength
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
Write-Host 'Locating Visual Studio and MSBuild...' -ForegroundColor Yellow
# The one discovery: vcpkg-install.ps1 pins vcpkg to this installation and its default toolset.
Import-Module (Join-Path $PSScriptRoot 'Tools/PerformancePolicy.psm1') -Force
$discovery = Invoke-DxUiBuildPhase -Phase VisualStudioDiscovery -Body {
    Import-Module (Join-Path $PSScriptRoot 'Tools/VisualStudio.psm1') -Force
    $installation = Get-DxUiVisualStudioInstallation
    $msbuild = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
    if (-not (Test-Path -LiteralPath $msbuild)) { throw "MSBuild is missing: $msbuild" }
    @{ installation=$installation; msbuild=$msbuild }
}
$installation = $discovery.installation
$msbuild = $discovery.msbuild
if (-not $OutputRoot) { $OutputRoot = Join-Path $PSScriptRoot '.build' }
$output = [IO.Path]::GetFullPath($OutputRoot).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
Invoke-DxUiBuildPhase -Phase OutputUseChecks -Body {
    $executables = @('DxUi.FoundationTests.exe','DxUi.ControlTests.exe','DxUi.EmbeddedTests.exe','DxUi.EmbeddedControls.exe','DxUi.InteractiveLease.exe')
    foreach ($name in $executables) {
        $expected = [IO.Path]::GetFullPath((Join-Path $output "$Platform/$Configuration/$name"))
        foreach ($process in (Get-CimInstance Win32_Process -Filter "Name = '$name'")) {
            if (-not $process.ExecutablePath) { throw "Cannot inspect running $name; no process was terminated." }
            if ([IO.Path]::GetFullPath($process.ExecutablePath) -eq $expected) {
                throw "Build output is running: PID $($process.ProcessId), $($process.ExecutablePath). No process was terminated."
            }
        }
    }
}
Invoke-DxUiBuildPhase -Phase DependencyAdmission -Body {
    $triplet = if ($Platform -eq 'ARM64') { 'arm64-windows' } else { 'x64-windows' }
    if (-not (Test-Path -LiteralPath (Join-Path $output "vcpkg_installed/$Platform/$triplet/include/wil/resource.h"))) {
        throw "Restore dependencies first: vcpkg-install.ps1 -Platform $Platform -OutputRoot `"$output`""
    }
}
$defaultOutput = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '.build')).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
$attestPerformanceBuild = [string]::Equals($output, $defaultOutput, [StringComparison]::OrdinalIgnoreCase)
if ($attestPerformanceBuild) {
    # Consumer-owned output roots retain their separate integration contract. Default builds also serve performance.ps1
    # and test.ps1 -SkipBuild, so bind compiled source/dependencies and the resolved toolchain before invoking MSBuild.
    $performanceBuildBefore = Invoke-DxUiBuildPhase -Phase PreBuildAttestation -Body {
        Start-PerformanceBuildAttestation -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration
    }
    if ($performanceBuildBefore.rebuildRequired) {
        $target = 'Rebuild'
        Write-Host 'Rebuilding because no current v2 attestation binds these exact inputs, tools and artifacts.' -ForegroundColor Yellow
    }
}
Write-DxUiBuildBanner -Configuration $Configuration -Platform $Platform -Target $target
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
Invoke-DxUiBuildPhase -Phase MSBuild -Body {
    if ($attestPerformanceBuild) { Push-Location -LiteralPath $PSScriptRoot }
    try {
        # Match the attested MSBuildStartupDirectory even when a paired harness calls this baseline from another checkout.
        # Consumer-owned output roots retain their caller's existing integration context.
        & $msbuild @arguments 2>&1 | ForEach-Object { Write-DxUiMSBuildLine -Line $_.ToString() }
        $buildExitCode = $LASTEXITCODE
    } finally { if ($attestPerformanceBuild) { Pop-Location } }
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
}
Assert-NativeBuildDiagnostics -LogPath $log
if ($attestPerformanceBuild) {
    Invoke-DxUiBuildPhase -Phase PostBuildAttestation -Body {
        Complete-PerformanceBuildAttestation -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration -Before $performanceBuildBefore
    }
}

$profileOutput=Join-Path $output "$Platform/$Configuration"
$library=Join-Path $profileOutput 'DxUi.lib'
$sample=Join-Path $profileOutput 'DxUi.EmbeddedControls.exe'
Write-NativeBuildResult -Project DxUi -Platform $Platform -Configuration $Configuration -LogPath $log -Artifacts @($library) -Executable $sample -Duration $nativeEntryElapsed.Elapsed