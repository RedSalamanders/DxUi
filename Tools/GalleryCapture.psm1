# Scaled gallery tile captures (gallery.ps1 -Scale/-Tiles/-Theme/-Warp) and their before/after parity.
# The native Gallery suite renders each theme's sheet at a DPI and crops the named tiles into
# <root>/<percent>/<theme>/<tile>.png beside its capture.json. This module builds those arguments, summarizes a capture as a
# receipt (receipt.json with provenance plus tiles.tsv with one line per tile: pixel size and SHA-256 of the decoded
# BGRA pixels) and compares two captures, or a capture with a retained receipt, tile by tile.
Set-StrictMode -Version Latest

$script:GalleryThemes = @('light', 'dark', 'rainbow-light', 'rainbow-dark', 'high-contrast')
$script:TilesHeader = "scale`ttheme`ttile`twidth`theight`tpixelSha256"

function Test-GalleryScalePercent([int] $Percent) {
    <# A whole-number DPI (96 x percent / 100) from 100% to 300%: the steps of 25 the harness accepts. #>
    return $Percent -ge 100 -and $Percent -le 300 -and (($Percent * 96) % 100) -eq 0
}

function Get-GalleryHarnessArguments {
    <# The DxUi.ControlTests.exe arguments of a tile capture. Every list is validated here, before a native run. #>
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $OutputDirectory, [int[]] $Scale = @(), [string[]] $Tiles = @(), [string[]] $Theme = @(), [switch] $Warp)
    $scales = @($Scale | Sort-Object -Unique)
    if (-not $scales.Count) { $scales = @(100) }
    foreach ($percent in $scales) {
        if (-not (Test-GalleryScalePercent $percent)) { throw "Unsupported gallery scale ${percent}%: use 100 to 300 in steps of 25." }
    }
    $tileList = @($Tiles | ForEach-Object { "$_".Trim() } | Where-Object { $_ })
    foreach ($tile in $tileList) {
        if ($tile.Contains(',') -or $tile.StartsWith('-')) { throw "Invalid gallery tile name: $tile" }
    }
    $themeList = @($Theme | ForEach-Object { "$_".Trim().ToLowerInvariant() } | Where-Object { $_ } | Select-Object -Unique)
    foreach ($name in $themeList) {
        if ($script:GalleryThemes -cnotcontains $name) { throw "Unknown gallery theme ${name}: use $($script:GalleryThemes -join ', ')." }
    }
    $arguments = [Collections.Generic.List[string]]::new()
    $arguments.AddRange([string[]]@('--suite=Gallery', '--no-activate', "--gallery-output-directory=$OutputDirectory", "--gallery-scale=$($scales -join ',')"))
    if ($tileList.Count) { $arguments.Add("--gallery-tiles=$($tileList -join ',')") }
    if ($themeList.Count) { $arguments.Add("--gallery-themes=$($themeList -join ',')") }
    if ($Warp) { $arguments.Add('--gallery-warp') }
    return , $arguments.ToArray()
}

function Clear-GalleryTileOutput([Parameter(Mandatory)][string] $Root) {
    <# Removes a previous capture's own outputs from a tile directory: its scale folders (digits only) and its three files.
       Anything else in the directory is left alone, and a drive or repository root is refused. #>
    $full = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    if (-not $full -or [IO.Path]::GetPathRoot($full).TrimEnd('\', '/') -eq $full -or (Test-Path -LiteralPath (Join-Path $full '.git'))) {
        throw "Refusing to clear gallery tiles in $full"
    }
    if (-not (Test-Path -LiteralPath $full -PathType Container)) { return }
    foreach ($item in Get-ChildItem -LiteralPath $full -Force) {
        if ($item.PSIsContainer -and $item.Name -match '^\d+$' -and -not $item.LinkType) { Remove-Item -LiteralPath $item.FullName -Recurse -Force }
        elseif (-not $item.PSIsContainer -and $item.Name -in @('capture.json', 'receipt.json', 'tiles.tsv')) { Remove-Item -LiteralPath $item.FullName -Force }
    }
}

function Get-GalleryPixelDigest([Parameter(Mandatory)][string] $Path) {
    <# A PNG's pixel size and the SHA-256 of its decoded 32-bit BGRA rows, top to bottom: equal digests mean equal pixels
       whatever the PNG encoder wrote around them. #>
    Add-Type -AssemblyName System.Drawing
    $image = [Drawing.Bitmap]::new($Path)
    try {
        $rectangle = [Drawing.Rectangle]::new(0, 0, $image.Width, $image.Height)
        $data = $image.LockBits($rectangle, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $rowBytes = $image.Width * 4
            $pixels = [byte[]]::new($rowBytes * $image.Height)
            for ($row = 0; $row -lt $image.Height; $row++) {
                [Runtime.InteropServices.Marshal]::Copy([IntPtr]::Add($data.Scan0, $row * $data.Stride), $pixels, $row * $rowBytes, $rowBytes)
            }
        } finally { $image.UnlockBits($data) }
        return [pscustomobject]@{ Width = $image.Width; Height = $image.Height; Sha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($pixels)).ToLowerInvariant() }
    } finally { $image.Dispose() }
}

function Get-GalleryTileInventory([Parameter(Mandatory)][string] $Path) {
    <# The tiles of a capture directory (decoded from <percent>/<theme>/<tile>.png) or of a retained receipt (its tiles.tsv,
       given as the receipt.json, the tiles.tsv or their directory), sorted by scale, theme and tile. #>
    $full = [IO.Path]::GetFullPath($Path)
    $tsv = $null
    if (Test-Path -LiteralPath $full -PathType Leaf) {
        $tsv = if ([IO.Path]::GetFileName($full) -ceq 'tiles.tsv') { $full } else { Join-Path (Split-Path $full) 'tiles.tsv' }
        if (-not (Test-Path -LiteralPath $tsv -PathType Leaf)) { throw "No tiles.tsv beside $full" }
    } elseif (-not (Test-Path -LiteralPath $full -PathType Container)) {
        throw "No gallery capture or receipt at $full"
    } elseif (-not @(Get-ChildItem -LiteralPath $full -Directory | Where-Object Name -match '^\d+$').Count -and (Test-Path -LiteralPath (Join-Path $full 'tiles.tsv'))) {
        $tsv = Join-Path $full 'tiles.tsv'
    }
    $tiles = [Collections.Generic.List[object]]::new()
    if ($tsv) {
        $lines = @([IO.File]::ReadAllLines($tsv) | Where-Object { $_ -and -not $_.StartsWith('#') })
        if (-not $lines.Count -or $lines[0] -cne $script:TilesHeader) { throw "Unexpected tiles.tsv header in $tsv" }
        foreach ($line in $lines | Select-Object -Skip 1) {
            $fields = $line.Split("`t")
            if ($fields.Count -ne 6 -or $fields[0] -notmatch '^\d+$' -or $fields[3] -notmatch '^\d+$' -or $fields[4] -notmatch '^\d+$' -or $fields[5] -notmatch '^[0-9a-f]{64}$') {
                throw "Malformed tiles.tsv line in ${tsv}: $line"
            }
            $tiles.Add([pscustomobject]@{ Scale = [int]$fields[0]; Theme = $fields[1]; Tile = $fields[2]; Width = [int]$fields[3]; Height = [int]$fields[4]; Sha256 = $fields[5] })
        }
    } else {
        foreach ($scaleDirectory in Get-ChildItem -LiteralPath $full -Directory | Where-Object Name -match '^\d+$') {
            foreach ($themeDirectory in Get-ChildItem -LiteralPath $scaleDirectory.FullName -Directory) {
                foreach ($png in Get-ChildItem -LiteralPath $themeDirectory.FullName -File -Filter '*.png') {
                    $digest = Get-GalleryPixelDigest $png.FullName
                    $tiles.Add([pscustomobject]@{ Scale = [int]$scaleDirectory.Name; Theme = $themeDirectory.Name; Tile = $png.BaseName; Width = $digest.Width; Height = $digest.Height; Sha256 = $digest.Sha256 })
                }
            }
        }
    }
    $keys = @{}
    foreach ($tile in $tiles) {
        $key = "$($tile.Scale)/$($tile.Theme)/$($tile.Tile)"
        if ($keys.ContainsKey($key)) { throw "Duplicate gallery tile $key in $full" }
        $keys[$key] = $true
    }
    return @($tiles | Sort-Object Scale, Theme, Tile)
}

function Write-GalleryTileReceipt {
    <# Writes receipt.json (provenance) and tiles.tsv (one line per tile) into $Destination for the tiles of $Root. #>
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string] $Destination, [Parameter(Mandatory)][Collections.IDictionary] $Provenance)
    $tiles = @(Get-GalleryTileInventory $Root)
    if (-not $tiles.Count) { throw "No gallery tiles under $Root" }
    $capture = Join-Path $Root 'capture.json'
    $native = if (Test-Path -LiteralPath $capture) { Get-Content -LiteralPath $capture -Raw | ConvertFrom-Json } else { $null }
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    $lines = @($script:TilesHeader) + @($tiles | ForEach-Object { "$($_.Scale)`t$($_.Theme)`t$($_.Tile)`t$($_.Width)`t$($_.Height)`t$($_.Sha256)" })
    [IO.File]::WriteAllText((Join-Path $Destination 'tiles.tsv'), (($lines -join "`n") + "`n"), [Text.UTF8Encoding]::new($false))
    $receipt = [ordered]@{
        schemaVersion = 1
        workloadOwner = 'DxUi'
        fixture       = 'dxui-gallery-tiles'
    }
    foreach ($key in $Provenance.Keys) { $receipt[$key] = $Provenance[$key] }
    if ($native) {
        $receipt.adapter = $native.adapter
        $receipt.forceWarp = $native.forceWarp
        $receipt.scales = @($native.scales)
        $receipt.themes = @($native.themes)
        $receipt.tileFilters = @($native.tileFilters)
    }
    $receipt.tileCount = $tiles.Count
    $receipt.tilesFile = 'tiles.tsv'
    $receipt.pixelDigest = 'SHA-256 of the PNG decoded to 32-bit BGRA rows, top to bottom (Tools/GalleryCapture.psm1, Get-GalleryPixelDigest)'
    [IO.File]::WriteAllText((Join-Path $Destination 'receipt.json'), ((ConvertTo-Json -InputObject $receipt -Depth 6) + "`n"), [Text.UTF8Encoding]::new($false))
    return $tiles.Count
}

function Compare-GalleryCaptures {
    <# Compares two captures (or receipts) tile by tile at each scale: Identical, Different (size or pixels),
       MissingBefore or MissingAfter. With -Scale, -Theme or -Tiles only those tiles are compared; a tile filter is a
       tile file name or its control prefix (textfield matches textfield-multiline). #>
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Before, [Parameter(Mandatory)][string] $After, [int[]] $Scale = @(), [string[]] $Theme = @(), [string[]] $Tiles = @())
    $filters = @($Tiles | ForEach-Object { ("$_".ToLowerInvariant() -replace '[^a-z0-9]+', '-').Trim('-') } | Where-Object { $_ })
    $select = {
        param($tile)
        ((-not $Scale.Count) -or $Scale -contains $tile.Scale) -and ((-not $Theme.Count) -or $Theme -contains $tile.Theme) -and
        ((-not $filters.Count) -or @($filters | Where-Object { $tile.Tile -ceq $_ -or $tile.Tile.StartsWith("$_-", [StringComparison]::Ordinal) }).Count)
    }
    $beforeTiles = @{}
    $beforeInventory = @(Get-GalleryTileInventory $Before)
    foreach ($tile in $beforeInventory) { if (& $select $tile) { $beforeTiles["$($tile.Scale)/$($tile.Theme)/$($tile.Tile)"] = $tile } }
    $afterTiles = @{}
    $afterInventory = @(Get-GalleryTileInventory $After)
    foreach ($tile in $afterInventory) { if (& $select $tile) { $afterTiles["$($tile.Scale)/$($tile.Theme)/$($tile.Tile)"] = $tile } }
    $results = foreach ($key in @(@($beforeTiles.Keys) + @($afterTiles.Keys) | Sort-Object -Unique)) {
        $old = $beforeTiles[$key]
        $new = $afterTiles[$key]
        $reference = if ($old) { $old } else { $new }
        $status = if (-not $old) { 'MissingBefore' } elseif (-not $new) { 'MissingAfter' }
                  elseif ($old.Width -eq $new.Width -and $old.Height -eq $new.Height -and $old.Sha256 -ceq $new.Sha256) { 'Identical' } else { 'Different' }
        $detail = if ($status -eq 'Different' -and ($old.Width -ne $new.Width -or $old.Height -ne $new.Height)) { "size $($old.Width)x$($old.Height) -> $($new.Width)x$($new.Height)" }
                  elseif ($status -eq 'Different') { 'pixels differ' } else { '' }
        [pscustomobject]@{ Scale = $reference.Scale; Theme = $reference.Theme; Tile = $reference.Tile; Status = $status; Detail = $detail }
    }
    return @($results | Sort-Object Scale, Theme, Tile)
}

Export-ModuleMember -Function Test-GalleryScalePercent, Get-GalleryHarnessArguments, Clear-GalleryTileOutput, Get-GalleryPixelDigest,
    Get-GalleryTileInventory, Write-GalleryTileReceipt, Compare-GalleryCaptures
