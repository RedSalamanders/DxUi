# Original Mermaid examples plus independent expectations for DxUi's subset.
[CmdletBinding()]
param(
    [string] $InputDirectory=(Join-Path $PSScriptRoot '../ThirdParty/Markdown/mermaid-11.4.1'),
    [string] $OutputPath=(Join-Path $PSScriptRoot '../Tests/Support/Support.Tests.NativeFlowchartCorpus.h'),
    [switch] $Check
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$pins=@{
    'collection.json'='157DEAF02EC36597EB5012AF8CE1EDF3CA710281CFC6E3E65493BD74A540799A'
    'syntax-inventory.json'='5CBAD5692330F513128C4D6CAF59A6ACE18A530073DEC9098208FC559FE48231'
    'dxui-flowchart-expectations.json'='EE44C515E59CB370150572A0B2871CE1DB3D7A7655A6A1E7A563FAAE60FB48CC'
    'dxui-family-matrix.json'='E4483443141EE85EDFD0FB9CEF1F6D9513D9A275FF1E15BB99FC3B7F6C470153'
}
foreach($name in $pins.Keys){
    if((Get-FileHash -LiteralPath (Join-Path $InputDirectory $name)).Hash -cne $pins[$name]){throw "Pinned native flowchart input differs: $name"}
}
$collection=@(Get-Content -LiteralPath (Join-Path $InputDirectory 'collection.json') -Raw | ConvertFrom-Json)
foreach($entry in $collection){
    if($entry.file -notmatch '^[a-zA-Z0-9_.]+\.txt$' -or
       (Get-FileHash -LiteralPath (Join-Path $InputDirectory $entry.file)).Hash -cne $entry.sha256){throw "Pinned Mermaid reference differs: $($entry.file)"}
}
$pattern='(?ms)^```mermaid(?:-example)?[^\r\n]*\r?\n(.*?)^```[ \t]*\r?$'
$examples=[regex]::Matches([IO.File]::ReadAllText((Join-Path $InputDirectory 'flowchart.md.txt')),$pattern)
$expectations=@(Get-Content -LiteralPath (Join-Path $InputDirectory 'dxui-flowchart-expectations.json') -Raw | ConvertFrom-Json)
$families=@(Get-Content -LiteralPath (Join-Path $InputDirectory 'dxui-family-matrix.json') -Raw | ConvertFrom-Json)
if($collection.Count -ne 24 -or $examples.Count -ne 105 -or $expectations.Count -ne 105 -or $families.Count -ne 22){throw 'Incomplete native Mermaid corpus inventory'}
function Literal([string]$Text){
    $value=[Text.StringBuilder]::new()
    foreach($unit in $Text.ToCharArray()){[void]$value.Append(('\x{0:X4}' -f [int]$unit))}
    return ('std::wstring_view(L"{0}", {1}u)' -f $value.ToString(),$Text.Length)
}
$output=[Text.StringBuilder]::new()
[void]$output.AppendLine(@'
#pragma once
#include "../../src/Controls/DxUi.NativeFlowchart.h"
// Generated offline from pinned Mermaid 11.4.1 MIT documentation. Original
// fence bodies use LF in vectors; the complete original bytes remain pinned.
// Subset expectations are reviewed DxUi data, never owned-parser observations.
namespace NativeFlowchartCorpus
{
// clang-format off
using namespace DxUi::RichText::Detail;
struct Example { std::size_t number; std::wstring_view source; FlowchartOutcome outcome; std::size_t nodes, edges; FlowchartDirection direction; };
inline constexpr Example flowcharts[] = {
'@)
for($i=0;$i -lt $examples.Count;$i++){
    $expected=$expectations[$i]
    if($expected.id -ne ($i+1) -or $expected.outcome -notin @('Prepared','Unsupported')){throw 'Invalid independent flowchart expectation'}
    $source=$examples[$i].Groups[1].Value.Replace("`r`n","`n")
    [void]$output.AppendLine(('    {{{0}u, {1}, FlowchartOutcome::{2}, {3}u, {4}u, FlowchartDirection::{5}}},' -f $expected.id,(Literal $source),$expected.outcome,$expected.nodes,$expected.edges,$expected.direction))
}
[void]$output.AppendLine('};')
[void]$output.AppendLine('struct Family { std::wstring_view document, source; };')
[void]$output.AppendLine('inline constexpr Family unsupportedFamilies[] = {')
foreach($family in $families){
    if($family.support -cne 'unsupported-family'){continue}
    $text=[IO.File]::ReadAllText((Join-Path $InputDirectory $family.file))
    $fences=[regex]::Matches($text,$pattern)
    if(!$fences.Count){throw "Declared diagram family has no original example: $($family.document)"}
    $source=$fences[0].Groups[1].Value.Replace("`r`n","`n")
    # Family dispatch is tested after an original diagram-local YAML header,
    # where present. All 105 flowchart vectors above retain that header intact.
    $source=[regex]::Replace($source,'(?ms)\A---\n.*?^---[ \t]*\n','')
    [void]$output.AppendLine(('    {{{0}, {1}}},' -f (Literal $family.document),(Literal $source)))
}
[void]$output.AppendLine(@'
};
// clang-format on
} // namespace NativeFlowchartCorpus
'@)
$expected=$output.ToString().Replace("`r`n","`n").Replace("`n","`r`n")
if($Check){
    if(-not(Test-Path -LiteralPath $OutputPath) -or [IO.File]::ReadAllText($OutputPath) -cne $expected){throw 'Native flowchart corpus output is missing or stale'}
}else{[IO.File]::WriteAllText($OutputPath,$expected,[Text.UTF8Encoding]::new($false))}
Write-Host 'Native flowchart vectors: 105 original examples, 13 admitted graphs, 92 unsupported, 20 other diagram families.'
