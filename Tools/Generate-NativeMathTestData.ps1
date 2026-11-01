<#
.SYNOPSIS Generates parser safety vectors from every pinned KaTeX support-table row.
.DESCRIPTION Uses the original rendered math sample when supplied, otherwise the row's command label.
No TeX, HTML or JavaScript is evaluated. These vectors establish exhaustive parser coverage, not conformance.
.PARAMETER InputDirectory Directory containing the pinned KaTeX collection, license and support table.
.PARAMETER OutputPath Generated C++ test header.
.PARAMETER Check Compares exact output without writing.
.OUTPUTS One generated header unless Check is selected. No network or temporary files.
.EXAMPLE
./Tools/Generate-NativeMathTestData.ps1 -Check
#>
[CmdletBinding()]
param(
    [string] $InputDirectory=(Join-Path $PSScriptRoot '../ThirdParty/Markdown/katex-0.16.11'),
    [string] $OutputPath=(Join-Path $PSScriptRoot '../Tests/Support/Support.Tests.NativeMathSupportRows.h'),
    [switch] $Check
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$pins=@{
    'collection.json'='FDF24EAAED88F55A551D63B3361F68CF5CEB95F1BF39E3761FECD1AFA5A1E7A9'
    'LICENSE.txt'='766CCC1F306C885AA45542A9846BBD0A505B27A0374F146778171C2254CE18E3'
    'support_table.md.txt'='05128B0C4FC12996251F2184BA0F4E4CE29F3B2878919CC37169140FDA339061'
}
foreach($name in $pins.Keys){
    if((Get-FileHash -LiteralPath (Join-Path $InputDirectory $name)).Hash -cne $pins[$name]){throw "Pinned KaTeX support input differs: $name"}
}
function Literal([string]$Text){
    $value=[Text.StringBuilder]::new()
    foreach($unit in $Text.ToCharArray()){[void]$value.Append(('\x{0:X4}' -f [int]$unit))}
    return ('std::wstring_view(L"{0}", {1}u)' -f $value.ToString(),$Text.Length)
}
function CommandLabel([string]$Text){
    $value=[Text.StringBuilder]::new()
    $punctuation='!"#$%&''()*+,-./:;<=>?@[\]^_`{|}~'
    for($index=0;$index -lt $Text.Length;$index++){
        if($Text[$index] -eq '\' -and $index+1 -lt $Text.Length -and $punctuation.Contains($Text[$index+1])){$index++}
        [void]$value.Append($Text[$index])
    }
    return [Net.WebUtility]::HtmlDecode($value.ToString().Trim())
}
$output=[Text.StringBuilder]::new()
[void]$output.AppendLine(@'
#pragma once
#include <cstddef>
#include <string_view>
// Generated offline from all pinned KaTeX 0.16.11 support-table rows.
// Math samples are original source, never evaluated upstream output. Command-only
// rows can lack required arguments and are not independent support expectations.
namespace NativeMathSupportRows
{
// clang-format off
struct Row { std::size_t line; std::wstring_view label, source; bool originalSample; };
inline constexpr Row rows[] = {
'@)
$pattern='^\|((?:\\.|[^|])*)\|((?:\\.|[^|])*)\|(.*)$'
$rowCount=0; $sampleCount=0; $lineNumber=0
foreach($line in [IO.File]::ReadLines((Join-Path $InputDirectory 'support_table.md.txt'))){
    $lineNumber++
    if(-not $line.StartsWith('|') -or $line.StartsWith('|Symbol/Function') -or $line.StartsWith('|:')){continue}
    if($line -cnotmatch $pattern){throw "Unparsed pinned support-table row at line $lineNumber"}
    $label=$Matches[1]; $rendered=$Matches[2].Trim()
    $sample=$rendered.StartsWith('$')
    if($sample){
        if($rendered.Length -lt 2 -or -not $rendered.EndsWith('$')){throw "Unclosed math sample at line $lineNumber"}
        $source=$rendered.Substring(1,$rendered.Length-2)
        $sampleCount++
    }else{$source=CommandLabel $label}
    if(-not $source -or -not $label){throw "Empty pinned support-table row at line $lineNumber"}
    $sampleText=if($sample){'true'}else{'false'}
    [void]$output.AppendLine(('    {{{0}u, {1}, {2}, {3}}},' -f $lineNumber,(Literal $label),(Literal $source),$sampleText))
    $rowCount++
}
if($rowCount -ne 1135 -or $sampleCount -ne 995){throw 'Incomplete pinned KaTeX support-table inventory'}
[void]$output.AppendLine(@'
};
// clang-format on
} // namespace NativeMathSupportRows
'@)
$expected=$output.ToString().Replace("`r`n","`n").Replace("`n","`r`n")
if($Check){
    if(-not(Test-Path -LiteralPath $OutputPath) -or [IO.File]::ReadAllText($OutputPath) -cne $expected){throw 'Native math support-row output is missing or stale'}
}else{[IO.File]::WriteAllText($OutputPath,$expected,[Text.UTF8Encoding]::new($false))}
Write-Host "Native math support rows: $rowCount total, $sampleCount original samples, $($rowCount-$sampleCount) command-only probes."
