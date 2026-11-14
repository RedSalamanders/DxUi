<# .SYNOPSIS Scaled gallery tile captures: gallery.ps1's scale, tile, theme and WARP arguments, its refusals, the pixel digest,
the receipt and the before/after parity, on synthetic PNG fixtures. No native build is needed. #>
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../GalleryCapture.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../Validation.psm1') -Force
Add-Type -AssemblyName System.Drawing
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$gallery = Join-Path $repository 'gallery.ps1'

# A deterministic tile: every pixel's color follows from its position, one pixel optionally replaced.
function Set-FixturePng([string] $Root, [string] $Name, [int] $Width, [int] $Height, [int[]] $Seed = @(),
    [Drawing.Imaging.PixelFormat] $Format = [Drawing.Imaging.PixelFormat]::Format32bppArgb) {
    $path = Join-Path $Root $Name
    New-Item -ItemType Directory -Path (Split-Path $path) -Force | Out-Null
    $bitmap = [Drawing.Bitmap]::new($Width, $Height, $Format)
    try {
        for ($y = 0; $y -lt $Height; $y++) {
            for ($x = 0; $x -lt $Width; $x++) { $bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb(255, ($x * 37) % 256, ($y * 53) % 256, (($x + $y) * 11) % 256)) }
        }
        if (@($Seed).Count -eq 2) { $bitmap.SetPixel($Seed[0], $Seed[1], [Drawing.Color]::FromArgb(255, 1, 2, 3)) }
        $bitmap.Save($path, [Drawing.Imaging.ImageFormat]::Png)
    } finally { $bitmap.Dispose() }
}

# Two scales of two tiles in one theme, as the native harness lays out a capture.
function Set-FixtureCapture([string] $Root, [string] $Name, [hashtable] $Seeds = @{}) {
    foreach ($tile in @(@{ Path = '100/light/statusstrip-sections.png'; W = 8; H = 4 }, @{ Path = '100/light/textfield-multiline.png'; W = 6; H = 6 },
            @{ Path = '125/light/statusstrip-sections.png'; W = 10; H = 5 }, @{ Path = '125/light/textfield-multiline.png'; W = 8; H = 8 })) {
        $seed = if ($Seeds.ContainsKey($tile.Path)) { [int[]]$Seeds[$tile.Path] } else { @() }
        Set-FixturePng $Root "$Name/$($tile.Path)" $tile.W $tile.H $seed
    }
    Set-FixtureFile $Root "$Name/capture.json" '{ "schemaVersion": 1, "adapter": "WARP (forced)", "forceWarp": true, "scales": [{ "percent": 100, "dpi": 96 }, { "percent": 125, "dpi": 120 }], "themes": ["light"], "tileFilters": ["StatusStrip", "TextField / Multiline"], "tiles": [] }'
    return Join-Path $Root $Name
}

function Invoke-Parity([string] $Before, [string] $After, [string] $Output, [hashtable] $Extra = @{}) {
    $global:LASTEXITCODE = 0
    & $gallery -CompareBefore $Before -CompareAfter $After -OutputDirectory $Output @Extra 6>$null | Out-Null
    return $LASTEXITCODE
}

Invoke-TestCase 'capture scales are whole-number DPIs from 100% to 300%' {
    foreach ($percent in @(100, 125, 150, 175, 200, 300)) { Assert-True (Test-GalleryScalePercent $percent) "$percent% is supported" }
    foreach ($percent in @(0, 99, 110, 120, 325, 400)) { Assert-True (-not (Test-GalleryScalePercent $percent)) "$percent% is refused" }
}

Invoke-TestCase 'harness arguments carry the scales, tiles, themes and WARP' {
    $arguments = Get-GalleryHarnessArguments -OutputDirectory 'C:\out' -Scale 150, 100, 150 -Tiles 'StatusStrip', ' TextField / Multiline ' -Theme 'Light', 'dark' -Warp
    $expected = @('--suite=Gallery', '--no-activate', '--gallery-output-directory=C:\out', '--gallery-scale=100,150',
        '--gallery-tiles=StatusStrip,TextField / Multiline', '--gallery-themes=light,dark', '--gallery-warp')
    Assert-Equal ($expected -join '|') ($arguments -join '|') 'arguments'
    $default = Get-GalleryHarnessArguments -OutputDirectory 'C:\out' -Tiles 'Menu'
    Assert-Equal '--suite=Gallery|--no-activate|--gallery-output-directory=C:\out|--gallery-scale=100|--gallery-tiles=Menu' ($default -join '|') 'a tile filter alone captures at 100% on the default adapter'
}

Invoke-TestCase 'invalid scales, tiles and themes are refused before any native run' {
    Assert-Throws { Get-GalleryHarnessArguments -OutputDirectory 'C:\out' -Scale 110 } 'a scale without a whole-number DPI'
    Assert-Throws { Get-GalleryHarnessArguments -OutputDirectory 'C:\out' -Scale 325 } 'a scale beyond 300%'
    Assert-Throws { Get-GalleryHarnessArguments -OutputDirectory 'C:\out' -Tiles 'a,b' } 'a tile name with a comma'
    Assert-Throws { Get-GalleryHarnessArguments -OutputDirectory 'C:\out' -Tiles '--suite=Menu' } 'a tile name that reads as an option'
    Assert-Throws { Get-GalleryHarnessArguments -OutputDirectory 'C:\out' -Theme 'sepia' } 'an unknown theme'
}

Invoke-TestCase 'gallery.ps1 never publishes scaled tiles and parity needs both sides' {
    # Each refusal comes before the build, so no compiler or native test runs here.
    Assert-Throws { & $gallery -SkipBuild -PublishDocs -Scale 125 6>$null } 'publishing a scaled capture'
    Assert-Throws { & $gallery -SkipBuild -PublishDocs -Tiles 'StatusStrip' 6>$null } 'publishing a tile capture'
    Assert-Throws { & $gallery -CompareBefore 'C:\before' 6>$null } 'parity without an after capture'
    Assert-Throws { & $gallery -CompareBefore 'C:\before' -CompareAfter 'C:\after' -Warp 6>$null } 'parity does not render'
}

Invoke-FixtureCase 'the pixel digest reads decoded pixels, not PNG bytes' {
    param($root)
    Set-FixturePng $root 'argb.png' 7 5
    Set-FixturePng $root 'rgb.png' 7 5 -Format ([Drawing.Imaging.PixelFormat]::Format24bppRgb)
    Set-FixturePng $root 'seeded.png' 7 5 -Seed 3, 2
    $argb = Get-GalleryPixelDigest (Join-Path $root 'argb.png')
    $rgb = Get-GalleryPixelDigest (Join-Path $root 'rgb.png')
    Assert-True ((Get-FileHash (Join-Path $root 'argb.png')).Hash -cne (Get-FileHash (Join-Path $root 'rgb.png')).Hash) 'the two encodings differ in bytes'
    Assert-Equal 7 $argb.Width 'width'
    Assert-Equal 5 $argb.Height 'height'
    Assert-Equal $argb.Sha256 $rgb.Sha256 'equal pixels have one digest'
    Assert-True ($argb.Sha256 -cne (Get-GalleryPixelDigest (Join-Path $root 'seeded.png')).Sha256) 'one changed pixel changes the digest'
}

Invoke-FixtureCase 'identical captures pass parity at every scale' {
    param($root)
    $before = Set-FixtureCapture $root 'before'
    $after = Set-FixtureCapture $root 'after'
    Assert-Equal 0 (Invoke-Parity $before $after (Join-Path $root 'report')) 'exit code'
    $report = Get-FixtureJson $root 'report/parity.json'
    Assert-Equal 4 ([int]$report.compared) 'compared tiles'
    Assert-Equal 0 ([int]$report.differing) 'differing tiles'
    Assert-Equal '' ([IO.File]::ReadAllText((Join-Path $root 'report/differences.txt')).Trim()) 'no differing tile is named'
}

Invoke-FixtureCase 'a seeded one-pixel difference fails parity and names its tile and scale' {
    param($root)
    $before = Set-FixtureCapture $root 'before'
    $after = Set-FixtureCapture $root 'after' @{ '125/light/textfield-multiline.png' = @(4, 7) }
    Assert-Equal 1 (Invoke-Parity $before $after (Join-Path $root 'report')) 'exit code'
    $report = Get-FixtureJson $root 'report/parity.json'
    Assert-Equal 1 ([int]$report.differing) 'one tile differs'
    $tile = @($report.tiles | Where-Object { $_.status -ne 'Identical' })
    Assert-Equal '125/light/textfield-multiline/Different/pixels differ' "$($tile[0].scale)/$($tile[0].theme)/$($tile[0].tile)/$($tile[0].status)/$($tile[0].detail)" 'the differing tile'
    Assert-Equal "125/light/textfield-multiline`tDifferent`tpixels differ" ([IO.File]::ReadAllText((Join-Path $root 'report/differences.txt')).Trim()) 'differences.txt names it'
    Assert-Equal 0 (Invoke-Parity $before $after (Join-Path $root 'only100') @{ Scale = 100 }) 'the 100% tiles alone are identical'
}

Invoke-FixtureCase 'a changed size or a missing tile fails parity' {
    param($root)
    $before = Set-FixtureCapture $root 'before'
    $after = Set-FixtureCapture $root 'after'
    Set-FixturePng $root 'after/100/light/statusstrip-sections.png' 9 4
    Remove-Item -LiteralPath (Join-Path $after '125/light/statusstrip-sections.png')
    $results = Compare-GalleryCaptures -Before $before -After $after
    $statuses = @($results | ForEach-Object { "$($_.Scale)/$($_.Tile)=$($_.Status)" }) -join ' '
    Assert-Equal '100/statusstrip-sections=Different 100/textfield-multiline=Identical 125/statusstrip-sections=MissingAfter 125/textfield-multiline=Identical' $statuses 'statuses'
    Assert-Equal 'size 8x4 -> 9x4' (@($results)[0].Detail) 'a size change is reported as such'
    Assert-Equal 1 (Invoke-Parity $before $after (Join-Path $root 'report')) 'exit code'
}

Invoke-FixtureCase 'a retained receipt is compared with a new capture and filtered to the named tiles' {
    param($root)
    $before = Set-FixtureCapture $root 'before'
    $after = Set-FixtureCapture $root 'after' @{ '100/light/statusstrip-sections.png' = @(0, 0) }
    $receiptDirectory = Join-Path $root 'Measurements/Plan/Item'
    $count = Write-GalleryTileReceipt -Root $before -Destination $receiptDirectory -Provenance ([ordered]@{ sourceCommit = 'a' * 40; configuration = 'Release'; platform = 'x64'; benchmarkInputs = [ordered]@{ 'gallery.ps1' = 'b' * 64 } })
    Assert-Equal 4 $count 'receipt rows'
    $lines = [IO.File]::ReadAllLines((Join-Path $receiptDirectory 'tiles.tsv'))
    Assert-Equal "scale`ttheme`ttile`twidth`theight`tpixelSha256" $lines[0] 'tiles.tsv header'
    Assert-True ($lines[1] -cmatch "^100`tlight`tstatusstrip-sections`t8`t4`t[0-9a-f]{64}$") "first row: $($lines[1])"
    $receipt = Get-FixtureJson $root 'Measurements/Plan/Item/receipt.json'
    Assert-Equal 'WARP (forced)' $receipt.adapter 'the adapter comes from the native capture.json'
    Assert-Equal 'dxui-gallery-tiles' $receipt.fixture 'fixture'
    Assert-Equal ('a' * 40) $receipt.sourceCommit 'provenance'
    Set-FixtureFile $root 'Measurements/Plan/Item/README.md' 'Gallery tiles of a fixture capture.'
    Assert-Equal 0 (Test-DxUiMeasurements $root).Failures.Count 'the receipt is a valid library measurement'
    $statuses = @(Compare-GalleryCaptures -Before (Join-Path $receiptDirectory 'receipt.json') -After $after | ForEach-Object { $_.Status }) -join ' '
    Assert-Equal 'Different Identical Identical Identical' $statuses 'the receipt side compares by size and digest'
    $only = @(Compare-GalleryCaptures -Before $receiptDirectory -After $after -Tiles 'TextField')
    Assert-Equal 2 $only.Count 'a control filter keeps its tiles at every scale'
    Assert-True (@($only | Where-Object Status -ne 'Identical').Count -eq 0) 'and they are identical'
}

Invoke-FixtureCase 'clearing a tile capture removes only that capture''s outputs' {
    param($root)
    $capture = Set-FixtureCapture $root 'tiles'
    Set-FixtureFile $root 'tiles/receipt.json' '{}'
    Set-FixtureFile $root 'tiles/tiles.tsv' ''
    Set-FixtureFile $root 'tiles/notes.txt' 'kept'
    Set-FixtureFile $root 'tiles/review/keep.png' 'kept'
    Clear-GalleryTileOutput $capture
    Assert-Equal 'notes.txt review' ((Get-ChildItem -LiteralPath $capture | Sort-Object Name | ForEach-Object Name) -join ' ') 'what remains'
    Assert-Throws { Clear-GalleryTileOutput ([IO.Path]::GetPathRoot($root)) } 'a drive root is refused'
    Assert-Throws { Clear-GalleryTileOutput $repository } 'the repository root is refused'
}

Complete-TestRun 'GalleryCapture'
