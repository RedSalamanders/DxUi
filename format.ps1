<# .SYNOPSIS Formats all owned C++ source and tests, or checks them with -Check. #>
[CmdletBinding()] param([switch] $Check, [string] $FormatterPath = $env:DXUI_CLANG_FORMAT)
$ErrorActionPreference = 'Stop'
if (-not $FormatterPath) {
    $formatter = Get-Command clang-format.exe -ErrorAction SilentlyContinue
    if (-not $formatter) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        $installation = & $vswhere -latest -prerelease -products '*' -property installationPath
        $FormatterPath = Join-Path $installation 'VC/Tools/Llvm/x64/bin/clang-format.exe'
    } else { $FormatterPath = $formatter.Source }
}
if (-not (Test-Path -LiteralPath $FormatterPath -PathType Leaf)) { throw 'clang-format not found; see CONTRIBUTING.md for the pinned formatter.' }
$formatterVersion = & $FormatterPath --version
if ($LASTEXITCODE -ne 0 -or $formatterVersion -notmatch '^clang-format version 22\.1\.3(?:\s|$)') {
    throw "DxUI requires clang-format 22.1.3; found '$formatterVersion'. Use -FormatterPath or DXUI_CLANG_FORMAT; see CONTRIBUTING.md."
}
Write-Host "Formatter: $formatterVersion"
$files = @('src','include','Tests','Samples') | ForEach-Object {
    Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot $_) -Recurse -File | Where-Object Extension -In '.cpp','.h'
}
foreach ($file in $files) {
    if ($Check) { & $formatterPath --dry-run --Werror $file.FullName }
    else {
        # Preserve timestamps of already formatted files so a no-op formatting pass does not rebuild the library.
        & $formatterPath --dry-run --Werror $file.FullName 2>$null
        if ($LASTEXITCODE -eq 0) { continue }
        & $formatterPath -i $file.FullName
    }
    if ($LASTEXITCODE -ne 0) { throw "Formatting failed: $($file.FullName)" }
}
