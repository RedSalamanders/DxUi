<# .SYNOPSIS Builds and runs Foundation, inherited controls, and supplied-device tests with per-suite receipts. #>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string] $Configuration = 'Debug',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [switch] $SkipBuild,
    [string[]] $Suites = @('Foundation','Embedded','Grid','Theme','Control','Menu','NewControls','TextField','NativeTextInput','MultilineText','ReadOnly','ComboBox','Tree','Tooltip','Rendering','Animation','Accessibility','WindowHost')
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$nativeArchitecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
if (($Platform -eq 'ARM64') -and ($nativeArchitecture -ne 'Arm64')) { throw 'ARM64 runtime tests require an ARM64 host; use build.ps1 for cross-compilation.' }
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform }
$reports = Join-Path $PSScriptRoot '.build/reports'
$logs = Join-Path $PSScriptRoot '.build/logs'
New-Item -ItemType Directory -Path $reports,$logs -Force | Out-Null
$failures = @()
Push-Location $PSScriptRoot
try {
    foreach ($suite in $Suites) {
        $name = switch ($suite) { Foundation {'DxUi.FoundationTests.exe'} Embedded {'DxUi.EmbeddedTests.exe'} default {'DxUi.ControlTests.exe'} }
        $executable = Join-Path $PSScriptRoot ".build/$Platform/$Configuration/$name"
        if (-not (Test-Path -LiteralPath $executable)) { throw "Test executable is missing: $executable" }
        $arguments = @(if ($suite -in @('Foundation','Embedded')) { @() } elseif ($suite -in @('Menu','NativeTextInput')) { @("--suite=$suite") } else { @("--suite=$suite",'--no-activate') })
        $log = Join-Path $logs "test-$suite-$Platform-$Configuration.log"
        Write-Host "Running $suite ($Platform $Configuration)"
        & $executable @arguments *> $log
        $testExit = $LASTEXITCODE
        $skips = @(Get-Content -LiteralPath $log | Where-Object { $_ -match '^SKIPPED:' })
        $receipt = [ordered]@{
            suite=$suite; configuration=$Configuration; platform=$Platform; nativeArchitecture=$nativeArchitecture
            completedUtc=[DateTime]::UtcNow.ToString('o'); executable=$executable
            sha256=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash; exitCode=$testExit; skips=$skips
        }
        $receipt | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $reports "$suite-$Platform-$Configuration.json") -Encoding utf8
        if ($testExit -ne 0) { $failures += $suite; Get-Content -LiteralPath $log -Tail 12 }
        else { Write-Host "PASS $suite ($($skips.Count) capability skips recorded)" }
    }
} finally { Pop-Location }
if ($failures.Count) { throw "DxUi failed suites: $($failures -join ', '). See .build/logs and .build/reports." }
Write-Host "All $($Suites.Count) requested suites passed."
