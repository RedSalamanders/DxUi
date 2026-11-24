<#
.SYNOPSIS Builds the currently supported DxUi static targets and tests.
.DESCRIPTION Prints bounded BEGIN/END/FAIL timing for six top-level build phases. UTC timestamps and monotonic elapsed
milliseconds diagnose build orchestration; they are not native workload measurements or performance acceptance.
.PARAMETER Configuration Debug, Release or ASan Debug.
.PARAMETER Platform x64 or ARM64.
.PARAMETER OutputRoot Optional isolated output root; defaults to this checkout's .build directory.
.PARAMETER Clean Clean first-party artifacts for the selected profile without requiring dependency restore.
.PARAMETER Rebuild Clean and compile the selected profile.
.PARAMETER MaxCpuCount Bound MSBuild's worker processes. Zero uses the existing tool default.
.PARAMETER Analyze Runs MSVC code analysis on the checkout's solution under .build/analysis.
.PARAMETER ProductionOnly Build only the Release library target; excludes test executables from the production role.
.PARAMETER Projects Optional union of owned native project names. Builds their library dependency too and publishes only selected-target evidence. Omit for a full solution build.
.PARAMETER SourceRoot Optional separate clean immutable DxUi source checkout for a production-only Release SDK input build. Tooling remains owned by this checkout; output stays in the source checkout's default build tree.
.PARAMETER LocalUnpinnedSource Explicit local-only dirty SourceRoot library build. Requires Release and ProductionOnly; records library inputs for local snapshot capture only and never writes a pinned-source build descriptor.
.PARAMETER ToolingEngineRoot Explicit current harness .ci-engine reader for paired diagnostics of older pins. Both actual engines are verified and bound; unavailable to production or external-source SDK builds.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Debug',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [string] $OutputRoot = '',
    [switch] $Clean,
    [switch] $Rebuild,
    [ValidateRange(0,256)][int] $MaxCpuCount = 0,
    [switch] $ProductionOnly,
    [string[]] $Projects = @(),
    [string] $SourceRoot = '',
    [switch] $LocalUnpinnedSource,
    [string] $ToolingEngineRoot = '',
    [switch] $Analyze
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Projects=@($Projects | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($env:CI_NATIVE_BUILDS_DISABLED -eq 'true') { throw 'Native builds are forbidden in compiler-free PR checks.' }
if ($Clean -and ($Rebuild -or $Analyze -or $OutputRoot -or $SourceRoot -or $ToolingEngineRoot)) {
    throw '-Clean uses a standalone local profile with the default output.'
}
if ($Analyze -and ($OutputRoot -or $ProductionOnly -or $Projects.Count -or $SourceRoot -or $ToolingEngineRoot)) {
    throw '-Analyze uses this checkout, the full solution and its dedicated output directory.'
}
if($LocalUnpinnedSource -and (-not $SourceRoot -or $env:GITHUB_ACTIONS -ceq 'true')){throw 'LocalUnpinnedSource requires an explicit dirty local SourceRoot; hosted execution refuses before discovery.'}
if ($ProductionOnly -and $Configuration -ne 'Release') { throw 'ProductionOnly requires Release.' }
if ($ProductionOnly -and $Projects.Count) { throw 'Projects and ProductionOnly are separate build roles.' }
if ($ToolingEngineRoot -and ($ProductionOnly -or $SourceRoot)) { throw 'ToolingEngineRoot is only for ordinary paired diagnostic builds, not production or SDK input roles.' }
$engineArguments=@{};if($ToolingEngineRoot){$engineArguments.ToolingEngineRoot=$ToolingEngineRoot}
$buildRoot=$PSScriptRoot
if($SourceRoot){
    if(-not $ProductionOnly -or $Configuration -cne 'Release' -or $OutputRoot){throw 'SourceRoot supports only production-only Release with the source checkout default output.'}
    $buildRoot=[IO.Path]::GetFullPath($SourceRoot).TrimEnd('\','/')
    if(-not(Test-Path -LiteralPath (Join-Path $buildRoot '.git')) -or -not(Test-Path -LiteralPath (Join-Path $buildRoot 'DxUi.sln'))){throw 'SourceRoot requires a complete immutable DxUi Git checkout.'}
    $status=@(& git -C $buildRoot status --porcelain --untracked-files=all 2>&1)
    if($LASTEXITCODE -ne 0){throw 'SourceRoot Git state could not be read.'}
    $sourceDirty=[bool](($status -join '').Trim())
    if($sourceDirty -and -not $LocalUnpinnedSource){throw 'SourceRoot requires clean committed source.'}
    if($LocalUnpinnedSource -and -not $sourceDirty){throw 'LocalUnpinnedSource requires actual dirty source; use the ordinary clean SourceRoot route otherwise.'}
}
if ($Analyze) {
    Import-Module (Join-Path $buildRoot '.ci-engine/Tools/Modules/Ci.Policy.psm1') -ErrorAction Stop
    $OutputRoot = Assert-CiContainedPath -Root $buildRoot -Path (Join-Path $buildRoot '.build/analysis')
}
$buildEvidencePath = Join-Path $PSScriptRoot 'Tools/DxUiBuildEvidence.psm1'
if ($ToolingEngineRoot) {
    $readerRoot = [IO.Path]::GetFullPath($ToolingEngineRoot).TrimEnd('\','/')
    Import-Module (Join-Path $readerRoot 'Tools/Modules/Ci.EngineBinding.psm1') -Scope Local -ErrorAction Stop
    $scopedPath = Resolve-CiPinnedEngineModule -ProjectRoot (Split-Path $readerRoot -Parent) -Name 'ScopedTesting.psm1'
} elseif (Test-Path -LiteralPath (Join-Path $PSScriptRoot '.ci/project.json')) {
    $scopedPath = Join-Path $PSScriptRoot 'Tools/ScopedTesting.psm1'
} else {
    # A paired overlay on a pre-CI source tree retains the standalone profile lease.
    $scopedPath = $buildEvidencePath
}
$profileScope = Import-Module $scopedPath -Scope Local -PassThru -ErrorAction Stop
Import-Module $buildEvidencePath -Scope Local -ErrorAction Stop
$defaultBuildOutput=-not $OutputRoot -or [string]::Equals([IO.Path]::GetFullPath($OutputRoot).TrimEnd('\','/'),[IO.Path]::GetFullPath((Join-Path $buildRoot '.build')).TrimEnd('\','/'),[StringComparison]::OrdinalIgnoreCase)
$profileLease = & $profileScope {
    param($Root, $Architecture, $Profile, $Output)
    Enter-ScopedProfileLease -Root $Root -Platform $Architecture -Configuration $Profile -OutputRoot $Output
} $buildRoot $Platform $Configuration $OutputRoot
$buildCompleted=$false
$sdkModuleLoaded=$false
try {
if ($defaultBuildOutput) {
    $scopedReports = Join-Path $buildRoot ".build/reports/scoped-tests/$Platform-$($Configuration -replace ' ','')"
    Remove-ScopedIteration -Root $buildRoot -Path (Join-Path $scopedReports 'iteration.json')
    Remove-ScopedIteration -Root $buildRoot -Path (Join-Path $scopedReports 'build.json')
}
$defaultRelease=$defaultBuildOutput -and $Configuration -eq 'Release'
$attestSdkLibrary=$defaultRelease -and -not $Clean
if($defaultRelease){
    Import-Module (Join-Path $PSScriptRoot 'Tools/SdkLibraryInputs.psm1') -Force
    $sdkModuleLoaded=$true
    Remove-DxUiSdkLibraryReceipt -Root $buildRoot -Platform $Platform
}
if($SourceRoot){
    Import-Module (Join-Path $PSScriptRoot 'Tools/SdkSourceProfiles.psm1')
    $producerToolingBefore=Get-DxUiSdkToolingProvenance -Root $PSScriptRoot -LocalCandidate:$LocalUnpinnedSource
}
Import-Module (Join-Path $PSScriptRoot 'Tools/Validation.psm1') -Force
$mappingErrors=@(Test-DxUiSolutionConfigurations (Join-Path $buildRoot 'DxUi.sln'))
if ($mappingErrors.Count) { throw "Invalid native solution coverage: $($mappingErrors -join '; ')" }
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
$target = if ($Clean) { 'Clean' } elseif ($Rebuild) { 'Rebuild' } else { 'Build' }
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
if (-not $OutputRoot) { $OutputRoot = Join-Path $buildRoot '.build' }
$output = [IO.Path]::GetFullPath($OutputRoot).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
Import-Module (Join-Path $PSScriptRoot 'Tools/ProcessInspection.psm1') -Force
Invoke-DxUiBuildPhase -Phase OutputUseChecks -Body {
    $executables = @('DxUi.FoundationTests.exe','DxUi.ControlTests.exe','DxUi.EmbeddedTests.exe','DxUi.EmbeddedControls.exe','DxUi.InteractiveLease.exe')
    foreach ($name in $executables) {
        $expected = [IO.Path]::GetFullPath((Join-Path $output "$Platform/$Configuration/$name"))
        foreach ($process in (Get-CimInstance Win32_Process -Filter "Name = '$name'")) {
            $image=Resolve-DxUiObservedProcessPath -Observation $process -DisplayName $name
            if (-not $image) { continue }
            if ([IO.Path]::GetFullPath($image) -eq $expected) {
                throw "Build output is running: PID $($process.ProcessId), $image. No process was terminated."
            }
        }
    }
}
if ($Clean) {
    Import-Module (Join-Path $buildRoot '.ci-engine/Tools/Modules/Ci.Policy.psm1') -ErrorAction Stop
    $null = Assert-CiContainedPath -Root $buildRoot -Path $output
    $null = Assert-CiContainedPath -Root $buildRoot -Path (Join-Path $output "reports/performance-build/$Platform-$($Configuration -replace ' ','')")
    Remove-PerformanceBuildAttestation -Root $buildRoot -Platform $Platform -Configuration $Configuration
} else {
    Invoke-DxUiBuildPhase -Phase DependencyAdmission -Body {
        $triplet = if ($Platform -eq 'ARM64') { 'arm64-windows' } else { 'x64-windows' }
        $header = Join-Path $output "vcpkg_installed/$Platform/$triplet/include/wil/resource.h"
        if ($Analyze -and -not (Test-Path -LiteralPath $header -PathType Leaf)) {
            $binarySources = $env:VCPKG_BINARY_SOURCES
            try {
                $env:VCPKG_BINARY_SOURCES = 'clear'
                & (Join-Path $buildRoot 'vcpkg-install.ps1') -Platform $Platform -OutputRoot $output
            } finally {
                $env:VCPKG_BINARY_SOURCES = $binarySources
            }
        }
        if (-not (Test-Path -LiteralPath $header -PathType Leaf)) {
            throw "Restore dependencies first: vcpkg-install.ps1 -Platform $Platform -OutputRoot `"$output`""
        }
    }
}
$defaultOutput = [IO.Path]::GetFullPath((Join-Path $buildRoot '.build')).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
$selectedProjects=@()
$partialBuild=$false
if ($Projects.Count) {
    $selectedProjects=@(Resolve-ScopedBuildProjects $Projects)
    $partialBuild=$selectedProjects.Count -lt (Get-ScopedBuildProjectCatalog).Count
    Assert-ScopedSelectedBuildGraph -Root $buildRoot -MSBuildPath $msbuild -Platform $Platform -Configuration $Configuration -OutputRoot $output -Projects $selectedProjects
}
$attestPerformanceBuild = (-not ($Clean -or $ProductionOnly)) -and [string]::Equals($output, $defaultOutput, [StringComparison]::OrdinalIgnoreCase)
if ($partialBuild -and 'DxUi.EmbeddedTests' -cnotin $selectedProjects) { $attestPerformanceBuild=$false }
if (-not ($Clean -or $attestPerformanceBuild)) { $target = 'Rebuild' }
if ($attestPerformanceBuild) {
    # Consumer-owned output roots retain their separate integration contract. Default builds also serve performance.ps1
    # and test.ps1 -SkipBuild, so bind compiled source/dependencies and the resolved toolchain before invoking MSBuild.
    $performanceBuildBefore = Invoke-DxUiBuildPhase -Phase PreBuildAttestation -Body {
        Start-PerformanceBuildAttestation -Root $buildRoot -Platform $Platform -Configuration $Configuration @engineArguments
    }
    $scopedBuildBefore = Get-ScopedBuildInputIdentity $buildRoot $Platform @engineArguments
    $scopedBuildReceipt = Get-DxUiNativeBuildReceiptPath $buildRoot $Platform $Configuration
    $scopedReuse = $false
    try {
        $scopedArtifact = Get-ScopedArtifactIdentity $buildRoot $Platform $Configuration
        $scopedReuse = if ($partialBuild) {
            Test-ScopedBuildCoverage $buildRoot $Platform $Configuration $selectedProjects $scopedBuildBefore $scopedArtifact
        } else { Test-ScopedReceipt $scopedBuildReceipt (Get-ScopedDigest ($scopedBuildBefore+"`n"+$scopedArtifact)) }
    } catch [System.IO.IOException] { $scopedReuse = $false }
      catch [System.Management.Automation.RuntimeException] { $scopedReuse = $false }
    if (-not $scopedReuse) {
        $target = 'Rebuild'
        Write-Host 'Rebuilding because the complete native test-source receipt is missing or stale.' -ForegroundColor Yellow
    }
    if (Test-Path -LiteralPath $scopedBuildReceipt) { Remove-Item -LiteralPath $scopedBuildReceipt }
    if ($performanceBuildBefore.rebuildRequired) {
        $target = 'Rebuild'
        Write-Host 'Rebuilding because no current v2 attestation binds these exact inputs, tools and artifacts.' -ForegroundColor Yellow
    }
}
if ($partialBuild) {
    # Never leave a full-solution pass behind a selected or failed build attempt.
    $fullReceipt=Get-DxUiNativeBuildReceiptPath $buildRoot $Platform $Configuration
    if (Test-Path -LiteralPath $fullReceipt) { Remove-Item -LiteralPath $fullReceipt }
}
if ([string]::Equals($output,$defaultOutput,[StringComparison]::OrdinalIgnoreCase)) { Remove-ScopedBuildReceipts $buildRoot $Platform $Configuration }
if($attestSdkLibrary){
    $sdkLibraryBefore=if($attestPerformanceBuild){
        Get-DxUiSdkLibraryInputState -Root $buildRoot -Platform $Platform -BuildState $performanceBuildBefore
    }else{Get-DxUiSdkLibraryInputState -Root $buildRoot -Platform $Platform}
}
Write-DxUiBuildBanner -Configuration $Configuration -Platform $Platform -Target $target
$logDirectory = Join-Path $output 'logs'
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
$log = Join-Path $logDirectory ("build-$Platform-$Configuration-" + [guid]::NewGuid().ToString('N') + '.log')
$workerArgument = if ($MaxCpuCount) { "/m:$MaxCpuCount" } else { '/m' }
$arguments = @((Join-Path $buildRoot 'DxUi.sln'), $workerArgument, '/nologo', '/verbosity:minimal', "/t:$target",
    "/p:Configuration=$Configuration", "/p:Platform=$Platform", "/p:DxUiOutputRoot=$output",
    "/flp:logfile=$log;verbosity=normal;encoding=UTF-8")
if ($ProductionOnly) {
    if ($Configuration -ne 'Release') { throw 'ProductionOnly requires Release.' }
    $arguments[4] = if ($target -eq 'Build') { '/t:DxUi' } else { "/t:DxUi:$target" }
}
if ($partialBuild) {
    $arguments[4]='/t:' + (($selectedProjects | ForEach-Object { $_.Replace('.','_')+':'+$target }) -join ';')
    Write-Host "Selected native target union: $($selectedProjects -join ', ')"
}
if ($Analyze) {
    $arguments += '/p:DxUiAnalyze=true'
    Write-Host 'MSVC code analysis' -ForegroundColor Cyan
}
if ($Clean) { $arguments += '/p:DxUiGuardClean=true' }
Write-Host 'Build configuration:' -ForegroundColor Cyan
Write-Host "  Configuration: $Configuration"
Write-Host "  Platform:      $Platform"
Write-Host "  Target:        $target"
Write-Host "  Toolchain:     $installation" -ForegroundColor DarkGray
Write-Host "  Output:        $output" -ForegroundColor DarkGray
Write-Host ''
if ($Clean) { Write-Host 'Cleaning DxUi...' -ForegroundColor Yellow }
else { Write-Host 'Compiling DxUi...' -ForegroundColor Yellow }
$stopwatch = [Diagnostics.Stopwatch]::StartNew()
Invoke-DxUiBuildPhase -Phase MSBuild -Body {
    if ($attestPerformanceBuild -or $attestSdkLibrary) { Push-Location -LiteralPath $buildRoot }
    try {
        # Match the attested MSBuildStartupDirectory even when a paired harness calls this baseline from another checkout.
        # Consumer-owned output roots retain their caller's existing integration context.
        & $msbuild @arguments 2>&1 | ForEach-Object { Write-DxUiMSBuildLine -Line $_.ToString() }
        $buildExitCode = $LASTEXITCODE
    } finally { if ($attestPerformanceBuild -or $attestSdkLibrary) { Pop-Location } }
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
if ($Clean) {
    Write-Host "Cleaned DxUi first-party outputs for $Platform $Configuration. Log: $log"
    $buildCompleted=$true
    return
}
if ($attestPerformanceBuild) {
    $performanceBuildAfter=Invoke-DxUiBuildPhase -Phase PostBuildAttestation -Body {
        Complete-PerformanceBuildAttestation -Root $buildRoot -Platform $Platform -Configuration $Configuration -Before $performanceBuildBefore @engineArguments -PassThru
    }
    if ((Get-ScopedBuildInputIdentity $buildRoot $Platform @engineArguments) -cne $scopedBuildBefore) {
        throw 'Compiled source or installed dependencies changed during the build; no native test receipt was published.'
    }
    $scopedArtifact = Get-ScopedArtifactIdentity $buildRoot $Platform $Configuration
    if ($partialBuild) {
        Write-ScopedTargetBuildReceipt $buildRoot $Platform $Configuration $selectedProjects $scopedBuildBefore $scopedArtifact
    } else {
        Assert-ScopedBuildOutputs $buildRoot $Platform $Configuration @((Get-ScopedBuildProjectCatalog).Keys)
        Write-ScopedReceipt $scopedBuildReceipt (Get-ScopedDigest ($scopedBuildBefore+"`n"+$scopedArtifact))
    }
}
if($attestSdkLibrary){
    $sdkArguments=@{Root=$buildRoot;Platform=$Platform;Before=$sdkLibraryBefore}
    if($attestPerformanceBuild){$sdkArguments.BuildState=$performanceBuildAfter}
    $sdkLibraryRecord=Complete-DxUiSdkLibraryBuild @sdkArguments
    if($SourceRoot){
        $producerToolingAfter=Get-DxUiSdkToolingProvenance -Root $PSScriptRoot -LocalCandidate:$LocalUnpinnedSource
        if(($producerToolingBefore|ConvertTo-Json -Compress) -cne ($producerToolingAfter|ConvertTo-Json -Compress)){throw 'SDK producer tooling changed during source compilation.'}
        if(-not $LocalUnpinnedSource){
            Complete-DxUiSdkSourceBuildRecord -Root $buildRoot -Platform $Platform -Tooling $producerToolingAfter `
                -LibraryRecord $sdkLibraryRecord -Arguments $arguments -LogPath $log|Out-Null
        }elseif(-not $sdkLibraryRecord.inputState.sourceDirty){throw 'Local source build lost its explicit dirty input role.'}
    }
}

$profileOutput=Join-Path $output "$Platform/$Configuration"
$library=Join-Path $profileOutput 'DxUi.lib'
$sample=Join-Path $profileOutput 'DxUi.EmbeddedControls.exe'
if ($ProductionOnly) { $sample = '' }
if ($partialBuild -and 'DxUi.EmbeddedControls' -cnotin $selectedProjects) { $sample='' }
Write-NativeBuildResult -Project DxUi -Platform $Platform -Configuration $Configuration -LogPath $log -Artifacts @($library) -Executable $sample -Duration $nativeEntryElapsed.Elapsed
$buildCompleted=$true
} finally {
    try {
        if ($defaultBuildOutput -and -not $buildCompleted) { Remove-ScopedBuildReceipts $buildRoot $Platform $Configuration }
        if($sdkModuleLoaded -and -not $buildCompleted){Remove-DxUiSdkLibraryReceipt -Root $buildRoot -Platform $Platform}
    } finally {
        & $profileScope { param($Lease) Exit-ScopedProfileLease $Lease } $profileLease
    }
}
