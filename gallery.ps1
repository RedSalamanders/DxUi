<# .SYNOPSIS Generate all controls and variants in five themes, plus the standalone embedded example. #>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string] $Configuration='Debug',
    [ValidateSet('x64','ARM64')][string] $Platform='x64',
    [string] $OutputDirectory='',
    [switch] $SkipBuild
)
$ErrorActionPreference='Stop'
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform }
if (-not $OutputDirectory) { $OutputDirectory=Join-Path $PSScriptRoot ".build/gallery/$Platform/$Configuration" }
$output=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $output -Force | Out-Null
Push-Location $PSScriptRoot
try {
    & ".\.build\$Platform\$Configuration\DxUi.ControlTests.exe" --suite=Gallery --no-activate "--gallery-output-directory=$output" *> (Join-Path $output 'generation.log')
    if ($LASTEXITCODE -ne 0) { Get-Content (Join-Path $output 'generation.log') -Tail 12; throw 'Control gallery generation failed.' }
    & ".\.build\$Platform\$Configuration\DxUi.EmbeddedControls.exe" --output (Join-Path $output 'embedded-controls.png')
    if ($LASTEXITCODE -ne 0) { throw 'Standalone embedded example failed.' }
    $images=@(Get-ChildItem -LiteralPath $output -Filter '*.png' | Sort-Object Name)
    if ($images.Count -ne 6) { throw "Expected five themes plus the embedded example; got $($images.Count)." }
    $body=($images | ForEach-Object { $name=[System.Net.WebUtility]::HtmlEncode($_.Name); "<section><h2>$name</h2><a href='$name'><img src='$name' alt='$name'></a></section>" }) -join "`n"
    "<!doctype html><html lang='en'><meta charset='utf-8'><title>DxUI control gallery</title><style>body{font:16px Segoe UI,sans-serif;background:#11151c;color:#edf2fa;margin:24px}img{max-width:100%;height:auto}section{margin:32px 0}a{color:inherit}</style><h1>DxUI control gallery</h1><p>All 26 public controls and interaction variants. Click an image for full resolution.</p>$body</html>" | Set-Content -LiteralPath (Join-Path $output 'index.html') -Encoding utf8
    Write-Host "Gallery: $(Join-Path $output 'index.html')"
} finally { Pop-Location }
