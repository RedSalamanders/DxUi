<# .SYNOPSIS Formats all owned C++ source and tests, or checks them with -Check. #>
[CmdletBinding()] param([switch] $Check)
$ErrorActionPreference = 'Stop'
$formatter = Get-Command clang-format.exe -ErrorAction SilentlyContinue
if (-not $formatter) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $installation = & $vswhere -latest -prerelease -products '*' -property installationPath
    $candidate = Join-Path $installation 'VC/Tools/Llvm/x64/bin/clang-format.exe'
    if (-not (Test-Path -LiteralPath $candidate)) { throw 'clang-format not found; install Visual Studio C++ Clang tools.' }
    $formatterPath = $candidate
} else { $formatterPath = $formatter.Source }
$files = @('src','include','Tests','Samples') | ForEach-Object {
    Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot $_) -Recurse -File | Where-Object Extension -In '.cpp','.h'
}
foreach ($file in $files) {
    if ($Check) { & $formatterPath --dry-run --Werror $file.FullName }
    else { & $formatterPath -i $file.FullName }
    if ($LASTEXITCODE -ne 0) { throw "Formatting failed: $($file.FullName)" }
}
