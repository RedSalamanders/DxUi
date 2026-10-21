# Deterministic, byte-pinned Unicode 17 source/property and conformance data.
[CmdletBinding()]
param(
    [string] $InputDirectory = (Join-Path $PSScriptRoot '../ThirdParty/Unicode/17.0.0'),
    [string] $OutputDirectory = '',
    [switch] $Check
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$pins=[ordered]@{
    'GraphemeBreakProperty.txt'='D6B51D1D2AE5C33B451B7ED994B48F1F4DC62B2272A5831E7FD418514A6BAE89'
    'GraphemeBreakTest.txt'='E2D134D2C52919BACE503EBB6A551C1855FE1A1FAEC18478C78FFF254A1793EC'
    'DerivedCoreProperties.txt'='24C7FED1195C482FAAEFD5C1E7EB821C5EE1FB6DE07ECDBAA64B56A99DA22C08'
    'emoji-data.txt'='2CB2BB9455CDA83E8481541ECF5B6DFDA66A3BB89EFA3FA7C5297ECCF607B72B'
    'LICENSE.txt'='E7A93B009565CFCE55919A381437AC4DB883E9DA2126FA28B91D12732BC53D96'
}
foreach($name in $pins.Keys){
    if((Get-FileHash -LiteralPath (Join-Path $InputDirectory $name) -Algorithm SHA256).Hash -cne $pins[$name]){throw "Pinned Unicode 17 input differs: $name"}
}
function Parse-Hex([string] $Value){return [int]::Parse($Value,[Globalization.NumberStyles]::HexNumber,[Globalization.CultureInfo]::InvariantCulture)}
function Read-Ranges([string] $Name,[string] $Category,[hashtable] $Values){
    $rows=[Collections.Generic.List[object]]::new()
    foreach($line in [IO.File]::ReadAllLines((Join-Path $InputDirectory $Name))){
        $data=$line.Split('#')[0].Trim(); if(-not $data){continue}
        if($data -notmatch '^([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s*;\s*([A-Za-z_]+)(?:\s*;\s*([A-Za-z_]+))?\s*$'){throw "Malformed Unicode property row: $Name"}
        $property=$Matches[3]; $detail=if($Matches.ContainsKey(4)){$Matches[4]}else{''}
        if($Category -and $property -cne $Category){continue}
        $key=if($Category -eq 'InCB'){$detail}else{$property}
        if(-not $Values.ContainsKey($key)){continue}
        $first=Parse-Hex $Matches[1]; $last=if($Matches.ContainsKey(2)){Parse-Hex $Matches[2]}else{$first}
        if($last -lt $first -or $last -gt 0x10ffff){throw 'Unicode scalar range is invalid.'}
        $rows.Add(@{first=$first;last=$last;value=[int]$Values[$key]})
    }
    $ordered=@($rows | Sort-Object {$_.first}); $previous=-1
    foreach($row in $ordered){if($row.first -le $previous){throw "Overlapping Unicode $Category ranges"};$previous=$row.last}
    return $ordered
}
function Find-Value([object[]] $Ranges,[int] $CodePoint){
    $first=0; $last=$Ranges.Length
    while($first -lt $last){$middle=$first+[int][Math]::Floor(($last-$first)/2);if($Ranges[$middle].last -lt $CodePoint){$first=$middle+1}else{$last=$middle}}
    if($first -lt $Ranges.Length -and $Ranges[$first].first -le $CodePoint){return $Ranges[$first].value};return 0
}
$gcb=@(Read-Ranges 'GraphemeBreakProperty.txt' '' @{
    CR=1;LF=2;Control=3;Extend=4;ZWJ=5;Regional_Indicator=6;Prepend=7;SpacingMark=8;L=9;V=10;T=11
}) # Hangul LV/LVT are classified arithmetically, without hundreds of alternating rows.
$pictographic=@(Read-Ranges 'emoji-data.txt' 'Extended_Pictographic' @{Extended_Pictographic=16})
$indic=@(Read-Ranges 'DerivedCoreProperties.txt' 'InCB' @{Consonant=32;Extend=64;Linker=96})
$points=[Collections.Generic.SortedSet[int]]::new();[void]$points.Add(0);[void]$points.Add(0x110000)
foreach($range in @($gcb)+@($pictographic)+@($indic)){[void]$points.Add($range.first);[void]$points.Add($range.last+1)}
$edges=@($points);$merged=[Collections.Generic.List[object]]::new()
for($index=0;$index -lt $edges.Length-1;$index++){
    $first=$edges[$index];$last=$edges[$index+1]-1
    $flags=(Find-Value $gcb $first) -bor (Find-Value $pictographic $first) -bor (Find-Value $indic $first)
    if(-not $flags){continue}
    if($merged.Count -and $merged[$merged.Count-1].last+1 -eq $first -and $merged[$merged.Count-1].flags -eq $flags){$merged[$merged.Count-1].last=$last}
    else{$merged.Add(@{first=$first;last=$last;flags=$flags})}
}
$header=[Text.StringBuilder]::new()
[void]$header.AppendLine(@'
#pragma once
#include <cstdint>

// Generated from pinned Unicode 17.0.0 data. See ThirdParty/Unicode/17.0.0/LICENSE.txt.
// Regenerate with the owning PowerShell generator; never hand edit property rows.
namespace DxUi::UnicodeGraphemeData
{
struct PropertyRange
{
    std::uint32_t first;
    std::uint32_t last;
    std::uint8_t properties;
};
inline constexpr char kUnicodeVersion[] = "17.0.0";
// clang-format off
inline constexpr PropertyRange kProperties[] = {
'@)
foreach($range in $merged){[void]$header.AppendLine(('    {{0x{0:X6}u, 0x{1:X6}u, {2}u}},' -f $range.first,$range.last,$range.flags))}
[void]$header.AppendLine(@'
};
// clang-format on
} // namespace DxUi::UnicodeGraphemeData
'@)
$cases=[Collections.Generic.List[object]]::new()
foreach($line in [IO.File]::ReadAllLines((Join-Path $InputDirectory 'GraphemeBreakTest.txt'))){
    $data=$line.Split('#')[0].Trim();if(-not $data){continue}
    $literal=[Text.StringBuilder]::new();$units=0;$boundaries=[Collections.Generic.List[int]]::new()
    foreach($token in $data.Split(' ',[StringSplitOptions]::RemoveEmptyEntries)){
        if($token -ceq '÷'){$boundaries.Add($units);continue};if($token -ceq '×'){continue}
        $cp=Parse-Hex $token
        if($cp -gt 0x10ffff -or ($cp -ge 0xd800 -and $cp -le 0xdfff)){throw 'Conformance input contains a non-scalar code point.'}
        if($cp -ge 0x10000){$cp-=0x10000;[void]$literal.Append(('\x{0:X4}\x{1:X4}' -f (0xd800+($cp -shr 10)),(0xdc00+($cp -band 1023))));$units+=2}
        else{[void]$literal.Append(('\x{0:X4}' -f $cp));$units++}
    }
    if(-not $boundaries.Count -or $boundaries[0] -ne 0 -or $boundaries[$boundaries.Count-1] -ne $units -or $units -gt 65535){throw 'Conformance boundary inventory is invalid.'}
    $cases.Add(@{text=$literal.ToString();units=$units;boundaries=$boundaries.ToArray()})
}
if($cases.Count -ne 766){throw 'Pinned Unicode 17 conformance count differs.'}
$tests=[Text.StringBuilder]::new()
[void]$tests.AppendLine(@'
#pragma once
#include <cstdint>
#include <span>
#include <string_view>

// Unicode 17.0.0 GraphemeBreakTest.txt, all 766 original vectors, without exclusions.
// See ThirdParty/Unicode/17.0.0/LICENSE.txt. Generated; never hand edit vectors.
namespace UnicodeGraphemeCorpus
{
struct Case
{
    std::wstring_view text;
    std::span<const std::uint16_t> boundaries;
};
// clang-format off
'@)
for($index=0;$index -lt $cases.Count;$index++){
    [void]$tests.AppendLine(('inline constexpr std::uint16_t kEnds{0}[] = {{{1}}};' -f $index,(($cases[$index].boundaries | ForEach-Object {"$($_)u"}) -join ', ')))
}
[void]$tests.AppendLine('inline constexpr Case kCases[] = {')
for($index=0;$index -lt $cases.Count;$index++){
    [void]$tests.AppendLine(('    {{std::wstring_view(L"{0}", {1}u), kEnds{2}}},' -f $cases[$index].text,$cases[$index].units,$index))
}
[void]$tests.AppendLine(@'
};
// clang-format on
} // namespace UnicodeGraphemeCorpus
'@)
$propertyPath=if($OutputDirectory){Join-Path $OutputDirectory 'DxUi.TextElementsData.h'}else{Join-Path $PSScriptRoot '../src/Controls/DxUi.TextElementsData.h'}
$corpusPath=if($OutputDirectory){Join-Path $OutputDirectory 'Controls.Tests.UnicodeGraphemeCorpus.h'}else{Join-Path $PSScriptRoot '../Tests/Controls/Controls.Tests.UnicodeGraphemeCorpus.h'}
foreach($item in @(@{path=$propertyPath;text=$header.ToString()},@{path=$corpusPath;text=$tests.ToString()})){
    $expected=$item.text.Replace("`r`n","`n")
    if($Check){
        if(-not (Test-Path -LiteralPath $item.path -PathType Leaf) -or [IO.File]::ReadAllText($item.path).Replace("`r`n","`n") -cne $expected){throw "Generated Unicode data differs: $($item.path)"}
    }else{
        New-Item -ItemType Directory -Path (Split-Path $item.path) -Force | Out-Null
        [IO.File]::WriteAllText($item.path,$expected.Replace("`n","`r`n"),[Text.UTF8Encoding]::new($false))
    }
}
Write-Host "Generated $($merged.Count) disjoint property ranges and all $($cases.Count) conformance vectors."
