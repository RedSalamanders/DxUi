<#!
.SYNOPSIS
Installs the DxUi vcpkg manifest dependencies for x64, ARM64, or both.

.DESCRIPTION
Uses the repository and commit pinned by vcpkg-tool.json. The managed vcpkg checkout, downloads, build trees,
packages, and installed trees all live beneath .build. Each platform receives a private install root so one
manifest install cannot purge the other platform's package metadata.
Background restores capture both native streams without creating a secondary console window.
An actual interactive console keeps ordinary native console inheritance and output.

vcpkg builds with the Visual Studio installation and MSVC toolset that build.ps1's MSBuild uses, not the newest
toolset it finds. The installation is the one Tools/VisualStudio.psm1 discovers for build.ps1 and the toolset is
that installation's default (VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt); a missing or malformed
version file fails before anything is cloned. Each platform gets an overlay triplet beneath the output root,
vcpkg-triplets\<platform>\<triplet>.cmake: the pinned vcpkg checkout's triplet plus the two pins (VCPKG_VISUAL_STUDIO_PATH
and VCPKG_PLATFORM_TOOLSET_VERSION), rewritten only when its contents change. Changing the triplet changes vcpkg's
package ABI hash, so the first restore after a change rebuilds the packages.

.PARAMETER Platform
Target platform: x64, ARM64, or All.

.PARAMETER OutputRoot
Directory that holds the managed vcpkg checkout, overlay triplets, downloads, build trees, packages, and installed
trees. Defaults to this checkout's .build.
#>
[CmdletBinding()]
param(
    [ValidateSet('x64', 'ARM64', 'All')]
    [string] $Platform = 'x64',
    [string] $OutputRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSCommandPath
Import-Module (Join-Path $repoRoot 'Tools/VisualStudio.psm1') -Force
Import-Module (Join-Path $repoRoot 'Tools/VcpkgTriplet.psm1') -Force
$consumerBuildModule = Import-Module (Join-Path $repoRoot 'Tools/ConsumerBuild.psm1') -PassThru -Scope Local -ErrorAction Stop
$interactiveConsole = & $consumerBuildModule { Test-DxUiConsumerInteractiveConsole }
$buildRoot = if ($OutputRoot) { [IO.Path]::GetFullPath($OutputRoot) } else { Join-Path $repoRoot '.build' }
$toolRoot = Join-Path $buildRoot 'vcpkg-tool'
$toolStampPath = Join-Path $buildRoot 'vcpkg-tool.commit'
$toolIdentityPath = Join-Path $repoRoot 'vcpkg-tool.json'
$manifestPath = Join-Path $repoRoot 'vcpkg.json'

$git = Get-Command git.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $git) {
    throw 'Git was not found. Install Git for Windows and ensure git.exe is on PATH.'
}
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "The vcpkg manifest was not found: $manifestPath"
}

$toolIdentity = Get-Content -Raw -LiteralPath $toolIdentityPath | ConvertFrom-Json
$repository = [string] $toolIdentity.repository
$commit = [string] $toolIdentity.commit
if ([string]::IsNullOrWhiteSpace($repository) -or $commit -notmatch '^[0-9a-fA-F]{40}$') {
    throw 'vcpkg-tool.json must contain a repository URL and a full 40-character commit hash.'
}

# vcpkg picks the newest MSVC toolset of the Visual Studio installation it prefers; build.ps1's MSBuild compiles with the
# installation's default toolset. Find both the way build.ps1 does, here, before anything is cloned or downloaded, so a
# broken installation fails at once and every triplet below is pinned to what MSBuild uses.
$installation = Get-DxUiVisualStudioInstallation
$toolset = Get-DxUiDefaultToolset -Installation $installation
[xml]$toolchainPolicy = Get-Content -LiteralPath (Join-Path $repoRoot 'Directory.Build.props') -Raw
$requiredTools = $toolchainPolicy.SelectSingleNode('/Project/PropertyGroup/RequiredMsvcToolsVersion').InnerText
if ($toolset.MajorMinor -cne $requiredTools) {
    throw "DxUi requires MSVC $requiredTools.x; the selected Visual Studio default is $($toolset.Version). Install and select the required C++ tools before restoring dependencies."
}
Write-Host "Visual Studio: $installation" -ForegroundColor Cyan
Write-Host "MSVC toolset:  $($toolset.Version), the default MSBuild compiles with; vcpkg is pinned to $($toolset.MajorMinor)" -ForegroundColor Cyan

New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
if (-not (Test-Path -LiteralPath (Join-Path $toolRoot '.git') -PathType Container)) {
    if (Test-Path -LiteralPath $toolRoot) {
        throw "The managed vcpkg path exists but is not a Git checkout: $toolRoot"
    }

    Write-Host "Cloning pinned vcpkg tooling into $toolRoot" -ForegroundColor Cyan
    # Set only this managed checkout: relocated consumers may exceed traditional Win32 path limits.
    if ($interactiveConsole) { & $git.Source clone -c core.longpaths=true --filter=blob:none $repository $toolRoot }
    else {
        $global:LASTEXITCODE = & $consumerBuildModule {
            param($executable, $url, $root)
            Invoke-DxUiConsumerNativeQuery -FilePath $executable -Arguments @('clone', '-c', 'core.longpaths=true', '--filter=blob:none', $url, $root) -Stream
        } $git.Source $repository $toolRoot
    }
    if ($LASTEXITCODE -ne 0) {
        throw "git clone failed with exit code $LASTEXITCODE."
    }
}

$originResult = & $consumerBuildModule { param($executable, $root) Invoke-DxUiConsumerNativeQuery -FilePath $executable -Arguments @('-C', $root, 'remote', 'get-url', 'origin') } $git.Source $toolRoot
$origin = $originResult.StandardOutput.TrimEnd([char]13, [char]10)
$global:LASTEXITCODE = $originResult.ExitCode
if ($LASTEXITCODE -ne 0 -or $origin.TrimEnd('/') -ne $repository.TrimEnd('/')) {
    throw "The managed vcpkg checkout does not use the pinned repository: $repository"
}

$dirtyResult = & $consumerBuildModule { param($executable, $root) Invoke-DxUiConsumerNativeQuery -FilePath $executable -Arguments @('-C', $root, 'status', '--porcelain') } $git.Source $toolRoot
$dirty = $dirtyResult.StandardOutput
$global:LASTEXITCODE = $dirtyResult.ExitCode
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to inspect the managed vcpkg checkout.'
}
if ($dirty) {
    throw "The managed vcpkg checkout contains local changes: $toolRoot"
}

$headResult = & $consumerBuildModule { param($executable, $root) Invoke-DxUiConsumerNativeQuery -FilePath $executable -Arguments @('-C', $root, 'rev-parse', 'HEAD') } $git.Source $toolRoot
$head = $headResult.StandardOutput.TrimEnd([char]13, [char]10)
$global:LASTEXITCODE = $headResult.ExitCode
if ($LASTEXITCODE -ne 0 -or $head -ne $commit) {
    Write-Host "Selecting pinned vcpkg commit $commit" -ForegroundColor Cyan
    if ($interactiveConsole) { & $git.Source -C $toolRoot fetch --depth 1 origin $commit }
    else {
        $global:LASTEXITCODE = & $consumerBuildModule {
            param($executable, $root, $revision)
            Invoke-DxUiConsumerNativeQuery -FilePath $executable -Arguments @('-C', $root, 'fetch', '--depth', '1', 'origin', $revision) -Stream
        } $git.Source $toolRoot $commit
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to fetch pinned vcpkg commit $commit."
    }
    if ($interactiveConsole) { & $git.Source -C $toolRoot checkout --detach $commit }
    else {
        $global:LASTEXITCODE = & $consumerBuildModule {
            param($executable, $root, $revision)
            Invoke-DxUiConsumerNativeQuery -FilePath $executable -Arguments @('-C', $root, 'checkout', '--detach', $revision) -Stream
        } $git.Source $toolRoot $commit
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to check out pinned vcpkg commit $commit."
    }
}

$vcpkg = Join-Path $toolRoot 'vcpkg.exe'
$stampedCommit = if (Test-Path -LiteralPath $toolStampPath -PathType Leaf) {
    (Get-Content -Raw -LiteralPath $toolStampPath).Trim()
} else {
    ''
}
if (-not (Test-Path -LiteralPath $vcpkg -PathType Leaf) -or $stampedCommit -ne $commit) {
    Write-Host 'Bootstrapping vcpkg...' -ForegroundColor Cyan
    if ($interactiveConsole) { & (Join-Path $toolRoot 'bootstrap-vcpkg.bat') -disableMetrics }
    else {
        $global:LASTEXITCODE = & $consumerBuildModule {
            param($root)
            Invoke-DxUiConsumerNativeQuery -FilePath (Join-Path $env:SystemRoot 'System32/cmd.exe') -WorkingDirectory $root -BootstrapBatch -Stream
        } $toolRoot
    }
    if ($LASTEXITCODE -ne 0) {
        throw "vcpkg bootstrap failed with exit code $LASTEXITCODE."
    }
    Set-Content -LiteralPath $toolStampPath -Value $commit -Encoding ascii
}

$platforms = if ($Platform -eq 'All') { @('x64', 'ARM64') } else { @($Platform) }
foreach ($targetPlatform in $platforms) {
    $platformScope = $targetPlatform
    $triplet = if ($targetPlatform -eq 'ARM64') { 'arm64-windows' } else { 'x64-windows' }
    $installRoot = Join-Path $buildRoot "vcpkg_installed\$platformScope"
    $buildTreesRoot = Join-Path $buildRoot "vcpkg_buildtrees\$platformScope"
    $packagesRoot = Join-Path $buildRoot "vcpkg_packages\$platformScope"
    $downloadsRoot = Join-Path $buildRoot 'vcpkg_downloads'
    # The pinned checkout's own triplet, copied and pinned; vcpkg takes an overlay in place of the triplet of the same name.
    $overlay = Update-DxUiVcpkgOverlayTriplet -StockTripletPath (Join-Path $toolRoot "triplets/$triplet.cmake") -Toolset $toolset `
        -OutputDirectory (Join-Path $buildRoot "vcpkg-triplets\$platformScope")
    Write-Host "Overlay triplet $($overlay.Path) $(if ($overlay.Changed) { 'written' } else { 'unchanged' })" -ForegroundColor Cyan

    Write-Host "Installing DxUi dependencies ($triplet)" -ForegroundColor Cyan
    $arguments = @(
        'install',
        "--triplet=$triplet",
        "--x-manifest-root=$repoRoot",
        "--x-install-root=$installRoot",
        "--x-buildtrees-root=$buildTreesRoot",
        "--x-packages-root=$packagesRoot",
        "--downloads-root=$downloadsRoot",
        "--overlay-triplets=$($overlay.Directory)"
    )
    if ($interactiveConsole) { & $vcpkg @arguments }
    else {
        $global:LASTEXITCODE = & $consumerBuildModule {
            param($executable, $values)
            Invoke-DxUiConsumerNativeQuery -FilePath $executable -Arguments $values -Stream
        } $vcpkg $arguments
    }
    if ($LASTEXITCODE -ne 0) {
        throw "vcpkg install failed for $triplet with exit code $LASTEXITCODE."
    }
}

Write-Host 'vcpkg dependencies are ready.' -ForegroundColor Green
