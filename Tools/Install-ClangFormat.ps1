<# .SYNOPSIS Installs the pinned clang-format 22.1.3 for Windows x64 under .build/format, where format.ps1 finds it.
.DESCRIPTION
Fetches the checksum-pinned clang-format wheel from the Python Package Index (its JSON index lists each file with its
SHA-256), verifies the download against the pin and extracts only clang-format.exe. A wheel is a zip archive: nothing
is installed from it and no Python is needed. A working install of the pinned version is kept as is.
.PARAMETER WheelPath A local copy of the wheel to use instead of downloading it; the pinned SHA-256 still applies.
#>
[CmdletBinding()]
param([string] $WheelPath = '')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem

# Keep format.ps1's version check and the build contract aligned with this pin.
$version = '22.1.3'
$wheelSha256 = '426453233bea583775542da9b859de6e088bf32b4f393527fc5b05bbc7d33ae3'
$root = Join-Path $PSScriptRoot '../.build/format'
$executable = [IO.Path]::GetFullPath((Join-Path $root 'clang_format/data/bin/clang-format.exe'))
$entryName = 'clang_format/data/bin/clang-format.exe'

function Test-PinnedFormatter {
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { return $false }
    $reported = & $executable --version
    return $LASTEXITCODE -eq 0 -and $reported -match "^clang-format version $([regex]::Escape($version))(?:\s|$)"
}

if (Test-PinnedFormatter) {
    Write-Host "clang-format $version is already installed: $executable"
    exit 0
}
New-Item -ItemType Directory -Path $root -Force | Out-Null
$downloaded = $false
if (-not $WheelPath) {
    $index = Invoke-RestMethod -Uri 'https://pypi.org/simple/clang-format/' -Headers @{ Accept = 'application/vnd.pypi.simple.v1+json' }
    $candidates = @($index.files | Where-Object { $_.hashes.sha256 -eq $wheelSha256 })
    if ($candidates.Count -ne 1) { throw "The package index lists $($candidates.Count) files with the pinned clang-format SHA-256." }
    $WheelPath = Join-Path $root $candidates[0].filename
    Invoke-WebRequest -Uri $candidates[0].url -OutFile $WheelPath
    $downloaded = $true
}
try {
    $actual = (Get-FileHash -LiteralPath $WheelPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $wheelSha256) { throw "The clang-format wheel does not match its pinned SHA-256 (found $actual): $WheelPath" }
    $archive = [IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($WheelPath))
    try {
        $entry = $archive.GetEntry($entryName)
        if (-not $entry) { throw "The clang-format wheel has no $entryName" }
        New-Item -ItemType Directory -Path (Split-Path $executable) -Force | Out-Null
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $executable, $true)
    } finally { $archive.Dispose() }
} finally {
    if ($downloaded -and (Test-Path -LiteralPath $WheelPath)) { Remove-Item -LiteralPath $WheelPath }
}
if (-not (Test-PinnedFormatter)) { throw "The extracted formatter does not report clang-format ${version}: $executable" }
Write-Host "Installed clang-format ${version}: $executable"
exit 0
