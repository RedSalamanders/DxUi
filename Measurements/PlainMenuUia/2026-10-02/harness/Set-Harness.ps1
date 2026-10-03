<#
.SYNOPSIS Switches the retained MenuUiaCost evaluation harness on or off in a DxUi checkout.
.DESCRIPTION On copies DxUiTests.MenuUiaCost.h into Tests/Controls, includes it from DxUiTests.Menu.cpp after
DxUiTests.MenuResources.h and registers the fixture suite MenuUiaCost in DxUiTests.cpp. Off removes exactly those
insertions and moves the header back here, so commits, formatting and validation never see the harness.
.PARAMETER Repository The checkout to switch; the repository holding this script by default.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('On', 'Off')][string] $Mode,
    [string] $Repository = (Join-Path $PSScriptRoot '..\..\..\..')
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath $Repository).Path
$retained = Join-Path $PSScriptRoot 'DxUiTests.MenuUiaCost.h'
$header = Join-Path $repo 'Tests\Controls\DxUiTests.MenuUiaCost.h'
$menuTests = Join-Path $repo 'Tests\Controls\DxUiTests.Menu.cpp'
$runner = Join-Path $repo 'Tests\Controls\DxUiTests.cpp'
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Get-NewLine([string] $Text) { if ($Text.Contains("`r`n")) { "`r`n" } else { "`n" } }

# Each edit is a literal pair: Off turns the switched text back into the original, and either direction refuses a file
# that holds neither form, so a changed anchor fails loudly instead of leaving a half-switched tree.
function Switch-Text([string] $Path, [string[][]] $Pairs) {
    $text = [IO.File]::ReadAllText($Path)
    $nl = Get-NewLine $text
    foreach ($pair in $Pairs) {
        $original = $pair[0].Replace("`n", $nl)
        $switched = $pair[1].Replace("`n", $nl)
        if ($Mode -eq 'On') {
            if ($text.Contains($switched)) { continue }
            if (-not $text.Contains($original)) { throw "The anchor is missing from ${Path}: $($pair[0])" }
            $index = $text.IndexOf($original)
            $text = $text.Substring(0, $index) + $switched + $text.Substring($index + $original.Length)
        } else {
            if (-not $text.Contains($switched)) { continue }
            $index = $text.IndexOf($switched)
            $text = $text.Substring(0, $index) + $original + $text.Substring($index + $switched.Length)
        }
    }
    [IO.File]::WriteAllText($Path, $text, $utf8)
}

$include = '#include "DxUiTests.MenuResources.h"'
Switch-Text $menuTests @(, @($include, "$include`n#include `"DxUiTests.MenuUiaCost.h`""))
Switch-Text $runner @(
    @("void RunMenuTextLayoutResourceTests();`n", "void RunMenuTextLayoutResourceTests();`nvoid RunMenuUiaCostTests();`n"),
    @('constexpr std::array<const char*, 8> kFixtureSuites{"MenuTextLayoutResources",',
        'constexpr std::array<const char*, 9> kFixtureSuites{"MenuUiaCost", "MenuTextLayoutResources",'),
    @("    if (suiteFilter.has_value() && shouldRunSuite(`"MenuTextLayoutResources`"))",
        "    if (suiteFilter.has_value() && shouldRunSuite(`"MenuUiaCost`"))`n    {`n        runSuite(`"MenuUiaCost`", RunMenuUiaCostTests);`n        ranAnySuite = true;`n    }`n    if (suiteFilter.has_value() && shouldRunSuite(`"MenuTextLayoutResources`"))"))
if ($Mode -eq 'On') {
    Copy-Item -LiteralPath $retained -Destination $header -Force
} elseif (Test-Path -LiteralPath $header) {
    Move-Item -LiteralPath $header -Destination $retained -Force
}
git -C $repo status --short -- Tests/Controls
