<# .SYNOPSIS Generate all controls and variants in five themes, plus the standalone embedded example.
.DESCRIPTION Without tile options it writes the five theme sheets at 100% and the embedded example (and publishes them with
-PublishDocs). -Scale, -Tiles, -Theme or -Warp instead capture individual tiles as review evidence under
.build/gallery/<Platform>/<Configuration>/tiles: <percent>/<theme>/<tile>.png plus receipt.json and tiles.tsv (pixel size
and SHA-256 per tile). -CompareBefore/-CompareAfter compares two such captures, or a capture with a retained receipt, and
exits 1 when any compared tile differs.
.PARAMETER Scale Display scales in percent, 100 to 300 in steps of 25 (for example 100,125,150,200).
.PARAMETER Tiles Tiles to write: a control (TextField), a caption (TextField / Multiline) or a tile file name (textfield-multiline).
.PARAMETER Theme Themes to render: light, dark, rainbow-light, rainbow-dark, high-contrast. All five when omitted.
.PARAMETER Warp Render every host on WARP instead of the default adapter.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration='Debug',
    [ValidateSet('x64','ARM64')][string] $Platform='x64',
    [string] $OutputDirectory='',
    [switch] $SkipBuild,
    [switch] $PublishDocs,
    [int[]] $Scale = @(),
    [string[]] $Tiles = @(),
    [ValidateSet('light','dark','rainbow-light','rainbow-dark','high-contrast')][string[]] $Theme = @(),
    [switch] $Warp,
    [string] $CompareBefore = '',
    [string] $CompareAfter = ''
)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'Tools/GalleryCapture.psm1') -Force
$Tiles = @($Tiles | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })

if ($CompareBefore -or $CompareAfter) {
    if (-not $CompareBefore -or -not $CompareAfter) { throw 'Parity needs both -CompareBefore and -CompareAfter.' }
    if ($PublishDocs -or $Warp) { throw 'Parity compares existing captures; it neither renders nor publishes.' }
    if (-not $OutputDirectory) { $OutputDirectory = Join-Path $PSScriptRoot '.build/gallery/parity' }
    $output = [IO.Path]::GetFullPath($OutputDirectory)
    New-Item -ItemType Directory -Path $output -Force | Out-Null
    $results = @(Compare-GalleryCaptures -Before $CompareBefore -After $CompareAfter -Scale $Scale -Theme $Theme -Tiles $Tiles)
    if (-not $results.Count) { throw 'Parity found no tiles to compare.' }
    $differing = @($results | Where-Object Status -ne 'Identical')
    $report = [ordered]@{
        before = [IO.Path]::GetFullPath($CompareBefore); after = [IO.Path]::GetFullPath($CompareAfter)
        compared = $results.Count; identical = $results.Count - $differing.Count; differing = $differing.Count
        tiles = @($results | ForEach-Object { [ordered]@{ scale = $_.Scale; theme = $_.Theme; tile = $_.Tile; status = $_.Status; detail = $_.Detail } })
    }
    $report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'parity.json') -Encoding utf8
    $names = @($differing | ForEach-Object { "$($_.Scale)/$($_.Theme)/$($_.Tile)`t$($_.Status)$(if ($_.Detail) { "`t$($_.Detail)" })" })
    Set-Content -LiteralPath (Join-Path $output 'differences.txt') -Value $names -Encoding utf8
    foreach ($group in $results | Group-Object Scale) {
        $changed = @($group.Group | Where-Object Status -ne 'Identical').Count
        Write-Host ("{0,4}%: {1} of {2} tiles identical" -f $group.Name, ($group.Count - $changed), $group.Count)
    }
    foreach ($line in $names) { Write-Host "DIFFERS $line" }
    Write-Host "Parity report: $(Join-Path $output 'parity.json')"
    exit $(if ($differing.Count) { 1 } else { 0 })
}

$tileMode = [bool]($Scale.Count -or $Tiles.Count -or $Theme.Count -or $Warp)
if ($tileMode -and $PublishDocs) { throw 'The published gallery stays at one scale; scaled tiles are review evidence under .build/gallery.' }
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform }
if (-not $OutputDirectory) { $OutputDirectory=Join-Path $PSScriptRoot ".build/gallery/$Platform/$Configuration$(if ($tileMode) { '/tiles' })" }
$output=[IO.Path]::GetFullPath($OutputDirectory)
if ($tileMode) {
    $arguments = Get-GalleryHarnessArguments -OutputDirectory $output -Scale $Scale -Tiles $Tiles -Theme $Theme -Warp:$Warp
    Clear-GalleryTileOutput $output
}
New-Item -ItemType Directory -Path $output -Force | Out-Null
$catalogSource = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'src/Controls/ControlCatalog.cpp') -Raw
$catalogCount = [regex]::Matches($catalogSource, 'ControlDescriptor\s*\{\s*ControlKind::').Count
if ($catalogCount -eq 0) { throw 'Could not resolve the canonical public control catalog count.' }
Push-Location $PSScriptRoot
try {
    if ($tileMode) {
        & ".\.build\$Platform\$Configuration\DxUi.ControlTests.exe" @arguments *> (Join-Path $output 'generation.log')
        if ($LASTEXITCODE -ne 0) { Get-Content (Join-Path $output 'generation.log') -Tail 12; throw 'Gallery tile capture failed.' }
        $relative = [IO.Path]::GetRelativePath($PSScriptRoot, $output)
        $command = "gallery.ps1 -Configuration '$Configuration' -Platform $Platform" + $(if ($Scale.Count) { " -Scale $($Scale -join ',')" }) +
            $(if ($Tiles.Count) { " -Tiles '$($Tiles -join "','")'" }) + $(if ($Theme.Count) { " -Theme $($Theme -join ',')" }) + $(if ($Warp) { ' -Warp' })
        $inputs = [ordered]@{}
        foreach ($file in @('gallery.ps1', 'Tools/GalleryCapture.psm1', 'Tests/Controls/DxUi.Tests.Gallery.cpp', 'Tests/Controls/DxUi.Tests.Runner.cpp')) {
            $inputs[$file] = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $file) -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        $provenance = [ordered]@{
            command = $command; sourceCommit = (& git rev-parse HEAD).Trim(); sourceDirty = [bool](& git status --porcelain)
            configuration = $Configuration; platform = $Platform; nativeArchitecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
            benchmarkInputs = $inputs
        }
        $count = Write-GalleryTileReceipt -Root $output -Destination $output -Provenance $provenance
        Write-Host "Gallery tiles: $count under $relative (receipt.json, tiles.tsv)"
        return
    }
    & ".\.build\$Platform\$Configuration\DxUi.ControlTests.exe" --suite=Gallery --no-activate "--gallery-output-directory=$output" *> (Join-Path $output 'generation.log')
    if ($LASTEXITCODE -ne 0) { Get-Content (Join-Path $output 'generation.log') -Tail 12; throw 'Control gallery generation failed.' }
    & ".\.build\$Platform\$Configuration\DxUi.EmbeddedControls.exe" --output (Join-Path $output 'embedded-controls.png')
    if ($LASTEXITCODE -ne 0) { throw 'Standalone embedded example failed.' }
    $images=@(Get-ChildItem -LiteralPath $output -Filter '*.png' | Sort-Object Name)
    if ($images.Count -ne 6) { throw "Expected five themes plus the embedded example; got $($images.Count)." }
    $body=($images | ForEach-Object { $name=[System.Net.WebUtility]::HtmlEncode($_.Name); "<section><h2>$name</h2><a href='$name'><img src='$name' alt='$name'></a></section>" }) -join "`n"
    "<!doctype html><html lang='en'><meta charset='utf-8'><title>DxUi control gallery</title><style>body{font:16px Segoe UI,sans-serif;background:#11151c;color:#edf2fa;margin:24px}img{max-width:100%;height:auto}section{margin:32px 0}a{color:inherit}</style><h1>DxUi control gallery</h1><p>All $catalogCount public controls and interaction variants. Click an image for full resolution.</p>$body</html>" | Set-Content -LiteralPath (Join-Path $output 'index.html') -Encoding utf8
    Write-Host "Gallery: $(Join-Path $output 'index.html')"
    if ($PublishDocs) {
        # Source state excludes the published gallery, which this command replaces.
        $sourceDirty = [bool](& git status --porcelain -- . ':(exclude)docs/gallery')
        $destination = Join-Path $PSScriptRoot 'docs/gallery'
        New-Item -ItemType Directory -Path $destination -Force | Out-Null
        foreach ($file in @($images.FullName) + @((Join-Path $output 'index.html'))) {
            Copy-Item -LiteralPath $file -Destination $destination -Force
        }
        $sections = ($images | ForEach-Object { "## $($_.BaseName)`n`n![$($_.BaseName) control gallery]($($_.Name))" }) -join "`n`n"
        $scaledTilesSection = @'
## Scaled tile captures

These sheets stay at 100%. Review evidence at other display scales comes from the same harness and capture path:
`gallery.ps1 -Scale 100,125,150 -Tiles StatusStrip,TextField` renders the sheets at each scale, non-activating, and writes
only the named tiles (a control, a caption or a tile file name) to `.build/gallery/<Platform>/<Configuration>/tiles`
as `<percent>/<theme>/<tile>.png`, with `receipt.json` (source commit, build profile, adapter, scales) and `tiles.tsv`
(pixel size and SHA-256 of the pixels of every tile). `-Theme light,dark,high-contrast` limits the themes and `-Warp`
renders on WARP. `gallery.ps1 -CompareBefore <capture or receipt> -CompareAfter <capture>` reports per tile and scale
whether the pixels are identical, writes the differing tiles to `differences.txt` and exits 1 on any difference. Scaled
tiles are never published here: a change retains the receipt and the tiles its gate names under `Measurements/`, as the
[first receipt](../../Measurements/PerformanceAndMotion_2026-11-13/G1/README.md) does.
'@.Replace("`r`n", "`n").TrimEnd()
        "# Generated control gallery`n`n[Usage documentation](../README.md) | [Control guide](../controls.md) | [HTML gallery](index.html)`n`nGenerated from compiled DxUi controls using the explicit local gallery.ps1 -PublishDocs command. Generated documentation is reviewed and committed through an ordinary pull request; DxUi has no hosted publication workflow. The five theme sheets cover all $catalogCount public controls and populated interaction variants; the sixth image is the supplied-device example. Click a sheet to inspect it at full resolution. Rendering may vary with Windows fonts/DPI. These are documentation snapshots, not replacements for the original test baselines.`n`n$scaledTilesSection`n`n$sections" | Set-Content -LiteralPath (Join-Path $destination 'README.md') -Encoding utf8
        $receipt = [ordered]@{
            command='gallery.ps1 -PublishDocs'; sourceCommit=(& git rev-parse HEAD).Trim()
            sourceDirty=$sourceDirty
            configuration=$Configuration; platform=$Platform; nativeArchitecture=[System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
            controlCount=$catalogCount; images=@($images | ForEach-Object { @{ file=$_.Name; sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash } })
        }
        $receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $destination 'generation.json') -Encoding utf8
        Write-Host "Published documentation gallery: $destination"
    }
} finally { Pop-Location }
