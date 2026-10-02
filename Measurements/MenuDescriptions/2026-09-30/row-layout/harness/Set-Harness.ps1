<#
.SYNOPSIS Switches the retained evaluation harness (MenuRowProbe / MenuRowLiveBytes suites) on or off in a checkout.
.DESCRIPTION Off restores Tests/Controls/DxUiTests.cpp, removes the harness include from DxUiTests.Menu.cpp and moves the
harness header out of the tree, so commits, format and validation never see it. On puts all three back.
#>
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('On', 'Off')][string] $Mode)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..\..')).Path
$scratch = $PSScriptRoot
$header = Join-Path $repo 'Tests\Controls\DxUiTests.RowProbe.h'
$menuTests = Join-Path $repo 'Tests\Controls\DxUiTests.Menu.cpp'
$runner = Join-Path $repo 'Tests\Controls\DxUiTests.cpp'
$includeLine = '#include "DxUiTests.RowProbe.h"'
$crlf = "`r`n"
$utf8 = New-Object System.Text.UTF8Encoding($false)
Push-Location $repo
try {
    $text = [IO.File]::ReadAllText($menuTests)
    if ($Mode -eq 'Off') {
        $text = $text.Replace($crlf + $includeLine, '')
        [IO.File]::WriteAllText($menuTests, $text, $utf8)
        git checkout -- Tests/Controls/DxUiTests.cpp
        if (Test-Path -LiteralPath $header) { Move-Item -LiteralPath $header -Destination (Join-Path $scratch 'DxUiTests.RowProbe.h') -Force }
    } else {
        Copy-Item -LiteralPath (Join-Path $scratch 'DxUiTests.RowProbe.h') -Destination $header -Force
        if (-not $text.Contains($includeLine)) {
            $anchor = '#include "DxUiTests.MenuResources.h"'
            if (-not $text.Contains($anchor)) { throw 'anchor include missing in DxUiTests.Menu.cpp' }
            $text = $text.Replace($anchor, $anchor + $crlf + $includeLine)
            [IO.File]::WriteAllText($menuTests, $text, $utf8)
        }
        $r = [IO.File]::ReadAllText($runner)
        if (-not $r.Contains('RunMenuRowProbeTests')) {
            $r = [regex]::Replace($r, 'void RunMenuTextLayoutResourceTests\(\);(\r?\n)', 'void RunMenuTextLayoutResourceTests();$1void RunMenuRowProbeTests();$1void RunMenuRowLiveBytesTests();$1', 1)
            $r = $r.Replace('constexpr std::array<const char*, 8> kFixtureSuites{"MenuTextLayoutResources",', 'constexpr std::array<const char*, 10> kFixtureSuites{"MenuRowProbe", "MenuRowLiveBytes", "MenuTextLayoutResources",')
            $r = $r.Replace('    if (suiteFilter.has_value() && shouldRunSuite("MenuTextLayoutResources"))', '    if (suiteFilter.has_value() && shouldRunSuite("MenuRowProbe"))' + $crlf + '    {' + $crlf + '        runSuite("MenuRowProbe", RunMenuRowProbeTests);' + $crlf + '        ranAnySuite = true;' + $crlf + '    }' + $crlf + '    if (suiteFilter.has_value() && shouldRunSuite("MenuRowLiveBytes"))' + $crlf + '    {' + $crlf + '        runSuite("MenuRowLiveBytes", RunMenuRowLiveBytesTests);' + $crlf + '        ranAnySuite = true;' + $crlf + '    }' + $crlf + '    if (suiteFilter.has_value() && shouldRunSuite("MenuTextLayoutResources"))')
            [IO.File]::WriteAllText($runner, $r, $utf8)
        }
    }
    git status --short
} finally { Pop-Location }

