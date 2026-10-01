# vcpkg builds with the Visual Studio installation and default MSVC toolset MSBuild compiles with: the discovery both share
# (Tools/VisualStudio.psm1) and the overlay triplet that pins vcpkg to them (Tools/VcpkgTriplet.psm1), on fixture installations
# and triplets. Needs no Visual Studio, vcpkg or network; a real restore is what vcpkg-install.ps1 itself proves.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../VisualStudio.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../VcpkgTriplet.psm1') -Force
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

# The stock triplets as the pinned vcpkg checkout ships them: arm64-windows with CRLF line endings, x64-windows with LF.
$stockArm64 = "set(VCPKG_TARGET_ARCHITECTURE arm64)`r`nset(VCPKG_CRT_LINKAGE dynamic)`r`nset(VCPKG_LIBRARY_LINKAGE dynamic)`r`n"
$stockX64 = "set(VCPKG_TARGET_ARCHITECTURE x64)`nset(VCPKG_CRT_LINKAGE dynamic)`nset(VCPKG_LIBRARY_LINKAGE dynamic)`nset(VCPKG_PROVIDED_FORTRAN ON)`n"
$installationPath = 'C:\Program Files\Microsoft Visual Studio\18\Enterprise'
$versionFile = 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt'

function New-FixtureInstallation([string] $Root, [AllowNull()] $VersionFileText, [string] $Name = 'Visual Studio 2026') {
    # A Visual Studio layout reduced to the one file the default toolset is read from; $null leaves the file out (the
    # parameter is untyped because a [string] parameter turns $null into '', an empty file).
    $installation = Join-Path $Root $Name
    New-Item -ItemType Directory -Path $installation -Force | Out-Null
    if ($null -ne $VersionFileText) { Set-FixtureFile $Root "$Name/$versionFile" $VersionFileText }
    return $installation
}

function Get-ThrownMessage([scriptblock] $Body) {
    try { & $Body } catch { return $_.Exception.Message }
    return $null
}

function Get-TextLines([string] $Text) {
    # Lines without their endings, whichever style the text uses; a final newline leaves one empty line last.
    return , @($Text -split '\r?\n')
}

Invoke-FixtureCase 'the default toolset is the version the installation names, whatever ends the file' {
    param($root)
    foreach ($content in @('14.51.36231', "14.51.36231`n", "14.51.36231`r`n", "  14.51.36231`r`n")) {
        $installation = New-FixtureInstallation $root $content
        $toolset = Get-DxUiDefaultToolset -Installation $installation
        Assert-Equal '14.51.36231' $toolset.Version "the version of <$($content.Replace("`r", '\r').Replace("`n", '\n'))>"
        Assert-Equal '14.51' $toolset.MajorMinor 'the major.minor that selects the toolset'
        Assert-Equal $installation $toolset.Installation 'the installation, as given, is what the pin names'
        Assert-True $toolset.VersionFile.EndsWith('Microsoft.VCToolsVersion.default.txt') 'the file it was read from'
    }
    Set-FixtureBytes $root "Visual Studio 2026/$versionFile" ([byte[]](0xEF, 0xBB, 0xBF) + [Text.Encoding]::ASCII.GetBytes('14.44.35207'))
    $toolset = Get-DxUiDefaultToolset -Installation (Join-Path $root 'Visual Studio 2026')
    Assert-Equal '14.44.35207' $toolset.Version 'a byte-order mark is not part of the version'
    Assert-Equal '14.44' $toolset.MajorMinor 'and its major.minor'
}

Invoke-FixtureCase 'a missing version file fails with the file and what to repair' {
    param($root)
    $installation = New-FixtureInstallation $root $null
    $message = Get-ThrownMessage { Get-DxUiDefaultToolset -Installation $installation }
    Assert-True $message 'the missing file fails'
    Assert-True $message.Contains($installation) 'the message names the installation'
    Assert-True $message.Contains('Microsoft.VCToolsVersion.default.txt') 'and the file'
    Assert-True ($message.Contains('does not exist') -and $message.Contains('Visual Studio Installer')) "and the repair: $message"
    Assert-True (Get-ThrownMessage { Get-DxUiDefaultToolset -Installation (Join-Path $root 'no such installation') }) 'an absent installation fails as well'
}

Invoke-FixtureCase 'a malformed version file fails with what it holds, never a guessed toolset' {
    param($root)
    foreach ($content in @('', '   ', "`r`n", 'garbage', '14', '14.51', 'v145', '14.51.x', '14.51.36231-preview', '14.51.36231 14.52.36725', "14.51.36231`n14.52.36725", '.51.36231', '14..36231')) {
        $installation = New-FixtureInstallation $root $content
        $label = $content.Replace("`r", '\r').Replace("`n", '\n')
        $message = Get-ThrownMessage { Get-DxUiDefaultToolset -Installation $installation }
        Assert-True $message "<$label> is rejected"
        Assert-True ($message.Contains('not a toolset version such as 14.51.36231') -or ($content.Trim() -eq '' -and $message.Contains('is empty'))) "<$label> says what is wrong: $message"
        Assert-True $message.Contains('Microsoft.VCToolsVersion.default.txt') "<$label> names the file"
    }
    Set-FixtureFile $root "Visual Studio 2026/$versionFile" ('9' * 200)
    $long = Get-ThrownMessage { Get-DxUiDefaultToolset -Installation (Join-Path $root 'Visual Studio 2026') }
    Assert-True ($long.Contains('999...') -and -not $long.Contains('9' * 100)) 'a long file is shown cut short'
}

Invoke-TestCase 'the overlay is the stock text unchanged, then the two pins, in the stock line endings' {
    foreach ($stock in @($stockArm64, $stockX64)) {
        $text = Get-DxUiVcpkgOverlayTripletText -StockText $stock -Installation $installationPath -ToolsetVersion '14.51'
        Assert-True $text.StartsWith($stock, [StringComparison]::Ordinal) 'the stock text is copied, not restated'
        $newline = if ($stock.Contains("`r`n")) { "`r`n" } else { "`n" }
        Assert-True $text.EndsWith($newline, [StringComparison]::Ordinal) 'the file ends with a newline'
        $withoutStyle = $text.Replace($newline, '')
        Assert-True (-not $withoutStyle.Contains("`r") -and -not $withoutStyle.Contains("`n")) 'one line-ending style throughout'
        $lines = Get-TextLines $text
        Assert-Equal 'set(VCPKG_VISUAL_STUDIO_PATH "C:\\Program Files\\Microsoft Visual Studio\\18\\Enterprise")' $lines[-3] 'the installation pin'
        Assert-Equal 'set(VCPKG_PLATFORM_TOOLSET_VERSION "14.51")' $lines[-2] 'the toolset pin'
        Assert-Equal '' $lines[-1] 'nothing after them'
        Assert-True $text.Substring($stock.Length).StartsWith($newline + '# ', [StringComparison]::Ordinal) 'a blank line and a comment say why the file differs from the stock one'
    }
    $pins = @((Get-TextLines (Get-DxUiVcpkgOverlayTripletText -StockText $stockArm64 -Installation $installationPath -ToolsetVersion '14.51')) | Where-Object { $_ -like 'set(VCPKG_VISUAL_STUDIO_PATH *' -or $_ -like 'set(VCPKG_PLATFORM_TOOLSET_VERSION *' })
    Assert-Equal 2 $pins.Count 'each pin appears once'
}

Invoke-TestCase 'a stock triplet without a final newline, or without text, still gives a valid overlay' {
    $text = Get-DxUiVcpkgOverlayTripletText -StockText 'set(VCPKG_CRT_LINKAGE dynamic)' -Installation $installationPath -ToolsetVersion '14.51'
    Assert-True $text.StartsWith("set(VCPKG_CRT_LINKAGE dynamic)`n`n# ", [StringComparison]::Ordinal) 'the stock line is kept whole, ends its line and is followed by a blank line and the comment'
    Assert-Equal 'set(VCPKG_PLATFORM_TOOLSET_VERSION "14.51")' (Get-TextLines $text)[-2] 'the pins follow'
    $empty = Get-DxUiVcpkgOverlayTripletText -StockText '' -Installation $installationPath -ToolsetVersion '14.51'
    Assert-True $empty.StartsWith('# ', [StringComparison]::Ordinal) 'no blank line leads an overlay of nothing'
    Assert-Equal 4 (Get-TextLines $empty).Count 'the comment, the two pins and the end'
}

Invoke-TestCase 'the installation path is quoted so CMake reads back exactly what vswhere reported' {
    $text = Get-DxUiVcpkgOverlayTripletText -StockText $stockX64 -Installation 'C:\VS "x" ${HOME} $ENV{PATH}\2026' -ToolsetVersion '14.51'
    Assert-True $text.Contains('set(VCPKG_VISUAL_STUDIO_PATH "C:\\VS \"x\" \${HOME} \$ENV{PATH}\\2026")') "backslash, quote and dollar sign are escaped: $text"
    $forward = Get-DxUiVcpkgOverlayTripletText -StockText $stockX64 -Installation '/opt/vs/2026' -ToolsetVersion '14.51'
    Assert-True $forward.Contains('set(VCPKG_VISUAL_STUDIO_PATH "/opt/vs/2026")') 'a path with nothing special is unchanged'
    Assert-Throws { Get-DxUiVcpkgOverlayTripletText -StockText $stockX64 -Installation $installationPath -ToolsetVersion '14.51.36231' } 'the pin is major.minor, never a patch version'
    Assert-Throws { Get-DxUiVcpkgOverlayTripletText -StockText $stockX64 -Installation '' -ToolsetVersion '14.51' } 'an empty installation is not pinned'
}

$cmake = Get-Command cmake -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
Invoke-FixtureCase 'CMake, which vcpkg evaluates the triplet with, reads the stock settings and both pins back' {
    param($root)
    if (-not $cmake) { Write-Host '     skipped: cmake is not on PATH (a restore evaluates the triplet with it)'; return }
    foreach ($path in @($installationPath, 'C:\VS "x" ${HOME} $ENV{PATH}\2026')) {
        Set-FixtureFile $root 'stock/arm64-windows.cmake' $stockArm64
        $toolset = [pscustomobject]@{ Installation = $path; MajorMinor = '14.51' }
        $overlay = Update-DxUiVcpkgOverlayTriplet -StockTripletPath (Join-Path $root 'stock/arm64-windows.cmake') -Toolset $toolset -OutputDirectory (Join-Path $root 'overlay')
        $include = $overlay.Path.Replace('\', '/')
        Set-FixtureFile $root 'read.cmake' "include(`"$include`")`nmessage(`"ARCH=[`${VCPKG_TARGET_ARCHITECTURE}]`")`nmessage(`"PATH=[`${VCPKG_VISUAL_STUDIO_PATH}]`")`nmessage(`"TOOLSET=[`${VCPKG_PLATFORM_TOOLSET_VERSION}]`")`n"
        $output = @(& $cmake.Source -P (Join-Path $root 'read.cmake') 2>&1 | ForEach-Object { "$_" })
        Assert-Contains $output 'ARCH=[arm64]' 'the stock setting survives'
        Assert-Contains $output "PATH=[$path]" 'the installation pin reads back exactly'
        Assert-Contains $output 'TOOLSET=[14.51]' 'the toolset pin reads back'
    }
}

Invoke-FixtureCase 'the overlay is written under the stock name, and only when its contents change' {
    param($root)
    Set-FixtureFile $root 'vcpkg/triplets/arm64-windows.cmake' $stockArm64
    $stock = Join-Path $root 'vcpkg/triplets/arm64-windows.cmake'
    $output = Join-Path $root 'vcpkg-triplets/ARM64'
    $toolset = Get-DxUiDefaultToolset -Installation (New-FixtureInstallation $root '14.51.36231')
    $first = Update-DxUiVcpkgOverlayTriplet -StockTripletPath $stock -Toolset $toolset -OutputDirectory $output
    Assert-True $first.Changed 'the first run writes'
    Assert-Equal 'arm64-windows' $first.Triplet 'the triplet it overlays'
    Assert-Equal $output $first.Directory 'the directory --overlay-triplets takes'
    Assert-Equal (Join-Path $output 'arm64-windows.cmake') $first.Path 'under the stock name, so vcpkg takes it in place of the stock triplet'
    $expected = Get-DxUiVcpkgOverlayTripletText -StockText $stockArm64 -Installation $toolset.Installation -ToolsetVersion '14.51'
    Assert-Equal $expected ([Text.UTF8Encoding]::new($false, $true).GetString([IO.File]::ReadAllBytes($first.Path))) 'the file is the text, UTF-8 without a byte-order mark'
    Assert-Equal 'arm64-windows.cmake' (@(Get-ChildItem -LiteralPath $output | ForEach-Object Name) -join ',') 'and nothing else is in the overlay directory'

    # An old timestamp makes a rewrite visible: a rewrite stamps the file with the current time.
    $old = [DateTime]::new(2020, 1, 1, 0, 0, 0, [DateTimeKind]::Utc)
    [IO.File]::SetLastWriteTimeUtc($first.Path, $old)
    $second = Update-DxUiVcpkgOverlayTriplet -StockTripletPath $stock -Toolset $toolset -OutputDirectory $output
    Assert-True (-not $second.Changed) 'an unchanged second run reports no change'
    Assert-Equal $old.Ticks ([IO.File]::GetLastWriteTimeUtc($first.Path)).Ticks 'and leaves the file, its timestamp included, alone'

    $patched = Get-DxUiDefaultToolset -Installation (New-FixtureInstallation $root '14.51.36300')
    Assert-True (-not (Update-DxUiVcpkgOverlayTriplet -StockTripletPath $stock -Toolset $patched -OutputDirectory $output).Changed) 'a patch update of the same major.minor changes nothing'
    Assert-Equal $old.Ticks ([IO.File]::GetLastWriteTimeUtc($first.Path)).Ticks 'so it never reaches vcpkg as a new triplet'

    $newer = Get-DxUiDefaultToolset -Installation (New-FixtureInstallation $root '14.52.36725')
    $third = Update-DxUiVcpkgOverlayTriplet -StockTripletPath $stock -Toolset $newer -OutputDirectory $output
    Assert-True $third.Changed 'a different default toolset rewrites'
    Assert-True (([IO.File]::ReadAllText($third.Path)).Contains('set(VCPKG_PLATFORM_TOOLSET_VERSION "14.52")')) 'with the new pin'
    Assert-True ([IO.File]::GetLastWriteTimeUtc($third.Path) -gt $old) 'and a new timestamp'

    [IO.File]::SetLastWriteTimeUtc($third.Path, $old)
    $moved = Get-DxUiDefaultToolset -Installation (New-FixtureInstallation $root '14.52.36725' 'Another installation')
    Assert-True (Update-DxUiVcpkgOverlayTriplet -StockTripletPath $stock -Toolset $moved -OutputDirectory $output).Changed 'a different installation rewrites'
    Assert-True (([IO.File]::ReadAllText($third.Path)).Contains('Another installation')) 'with the new path'

    Set-FixtureFile $root 'vcpkg/triplets/arm64-windows.cmake' ($stockArm64 + "set(VCPKG_BUILD_TYPE release)`r`n")
    Assert-True (Update-DxUiVcpkgOverlayTriplet -StockTripletPath $stock -Toolset $moved -OutputDirectory $output).Changed 'a changed stock triplet rewrites'
    Assert-True (([IO.File]::ReadAllText($third.Path)).Contains('set(VCPKG_BUILD_TYPE release)')) 'and is copied whole'
}

Invoke-FixtureCase 'a stock triplet the checkout lacks fails with its path and writes nothing' {
    param($root)
    $toolset = Get-DxUiDefaultToolset -Installation (New-FixtureInstallation $root '14.51.36231')
    $missing = Join-Path $root 'vcpkg/triplets/arm64-windows.cmake'
    $message = Get-ThrownMessage { Update-DxUiVcpkgOverlayTriplet -StockTripletPath $missing -Toolset $toolset -OutputDirectory (Join-Path $root 'overlay') }
    Assert-True ($message -and $message.Contains($missing)) "the message names the triplet: $message"
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $root 'overlay'))) 'nothing was written'
}

Invoke-TestCase 'vcpkg-install.ps1 keeps its interface, fails early and passes the overlay to vcpkg' {
    $path = Join-Path $repository 'vcpkg-install.ps1'
    $tokens = $null
    $errors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors)
    Assert-Equal 0 @($errors).Count "vcpkg-install.ps1 has no syntax errors: $(@($errors | ForEach-Object { $_.Message }) -join '; ')"
    # Consumers run the pinned checkout's script during their restore, so a new parameter would break them.
    $parameters = @($ast.ParamBlock.Parameters)
    Assert-Equal 'Platform,OutputRoot' (($parameters | ForEach-Object { $_.Name.VariablePath.UserPath }) -join ',') 'the parameters consumers pass'
    Assert-Equal "'x64'" $parameters[0].DefaultValue.Extent.Text 'the default platform'
    Assert-Equal "''" $parameters[1].DefaultValue.Extent.Text 'the default output root'
    Assert-True $parameters[0].Extent.Text.Contains("'x64', 'ARM64', 'All'") 'the platforms'
    $script = [IO.File]::ReadAllText($path)
    $toolsetAt = $script.IndexOf('Get-DxUiDefaultToolset', [StringComparison]::Ordinal)
    Assert-True ($toolsetAt -ge 0) 'the default toolset is read'
    foreach ($later in @('git.exe clone', 'bootstrap-vcpkg.bat', '& $vcpkg @arguments')) {
        $at = $script.IndexOf($later, [StringComparison]::Ordinal)
        Assert-True ($at -gt $toolsetAt) "the toolset is read before '$later', so a broken installation fails before anything is cloned or run"
    }
    Assert-True $script.Contains('"--overlay-triplets=$($overlay.Directory)"') 'vcpkg takes the overlay directory'
    Assert-True $script.Contains('-StockTripletPath (Join-Path $toolRoot "triplets/$triplet.cmake")') 'copied from the pinned checkout, not restated'
    Assert-True $script.Contains('"vcpkg-triplets\$platformScope"') 'under the build output, one directory per platform'
}

Invoke-TestCase 'build.ps1 and vcpkg-install.ps1 share one Visual Studio discovery' {
    foreach ($name in @('build.ps1', 'vcpkg-install.ps1', 'test-consumer.ps1', 'Tools/tests/Test-AsanRuntime.ps1')) {
        $script = [IO.File]::ReadAllText((Join-Path $repository $name))
        Assert-True $script.Contains('Get-DxUiVisualStudioInstallation') "$name asks the shared discovery"
        Assert-True (-not $script.Contains('vswhere')) "$name has no discovery of its own"
    }
}

Invoke-TestCase 'the installer and its modules stay ASCII and parse in Windows PowerShell 5.1, which RedSalamander runs the pinned installer with' {
    $paths = @('vcpkg-install.ps1', 'Tools/VisualStudio.psm1', 'Tools/VcpkgTriplet.psm1') | ForEach-Object { Join-Path $repository $_ }
    foreach ($path in $paths) {
        # 5.1 reads a file without a byte-order mark as ANSI, so anything beyond ASCII would be read differently there.
        Assert-Equal 0 @([IO.File]::ReadAllBytes($path) | Where-Object { $_ -gt 127 }).Count "$([IO.Path]::GetFileName($path)) is ASCII"
    }
    $windowsPowerShell = Get-Command powershell.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $windowsPowerShell) { Write-Host '     skipped the 5.1 parse: powershell.exe is not here'; return }
    foreach ($path in $paths) {
        # Syntax only PowerShell 7 has (a ternary, && and ||, ??) does not parse in 5.1, so the parse runs there.
        $parse = "`$errors = `$null; [void][Management.Automation.Language.Parser]::ParseFile('$($path.Replace("'", "''"))', [ref]`$null, [ref]`$errors); if (`$errors) { `$errors | ForEach-Object { `$_.Message }; exit 1 }"
        $output = @(& $windowsPowerShell.Source -NoProfile -NonInteractive -Command $parse)
        Assert-Equal 0 $LASTEXITCODE "Windows PowerShell 5.1 parses $([IO.Path]::GetFileName($path)): $($output -join '; ')"
    }
}

Complete-TestRun 'VcpkgTriplet'
