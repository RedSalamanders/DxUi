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
    'dxui-flowchart-expectations.json'='2BB253E39A1F4EE201A04D95BB02971A1D3DD4226AEFB076ACB522AA651FFE85'
    'dxui-family-matrix.json'='5A49B46D69193C2D0D911DEC1A0BC79210164F4F75C175E9F89589823475C3B5'
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
$familyExamples=@($families | Where-Object { $_.support -in @('unsupported-family','limited-state','limited-class') })
if($familyExamples.Count -ne 20 -or @($families | Where-Object support -eq 'unsupported-family').Count -ne 18 -or
   @($families | Where-Object support -eq 'limited-state').Count -ne 1 -or @($families | Where-Object support -eq 'limited-class').Count -ne 1){
    throw 'Invalid native Mermaid family support matrix'
}
foreach($family in $families){
    if($family.support -notin @('unsupported-family','limited-flowchart','limited-state','limited-class','example-collection')){throw "Unknown Mermaid support value: $($family.support)"}
    if($family.support -in @('unsupported-family','limited-state','limited-class')){
        $expected=$family.firstExample
        if(!$expected -or $expected.outcome -notin @('Prepared','Unsupported') -or
           $expected.issue -notin @('None','UnsupportedFamily','UnsupportedStatement','UnsupportedEdge') -or
           $expected.direction -notin @('TopBottom','BottomTop','LeftRight','RightLeft') -or
           !$expected.reason -or $expected.nodes -lt 0 -or $expected.edges -lt 0 -or
           ($expected.outcome -eq 'Prepared' -and $expected.issue -cne 'None') -or
           ($expected.outcome -eq 'Unsupported' -and ($expected.nodes -ne 0 -or $expected.edges -ne 0))){throw "Invalid independent first-example expectation: $($family.document)"}
    }elseif($family.PSObject.Properties.Name -contains 'firstExample'){
        throw "Unexpected first-example expectation for non-family document: $($family.document)"
    }
}
function Literal([string]$Text){
    $value=[Text.StringBuilder]::new()
    foreach($unit in $Text.ToCharArray()){[void]$value.Append(('\x{0:X4}' -f [int]$unit))}
    return ('std::wstring_view(L"{0}", {1}u)' -f $value.ToString(),$Text.Length)
}
$output=[Text.StringBuilder]::new()
[void]$output.AppendLine(@'
#pragma once
#include "Controls/DxUi.NativeFlowchart.h"
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
[void]$output.AppendLine('struct Family { std::wstring_view document, source; FlowchartOutcome outcome; FlowchartIssue issue; std::size_t nodes, edges; FlowchartDirection direction; std::wstring_view reason; };')
[void]$output.AppendLine('inline constexpr Family familyExamples[] = {')
foreach($family in $familyExamples){
    $text=[IO.File]::ReadAllText((Join-Path $InputDirectory $family.file))
    $fences=[regex]::Matches($text,$pattern)
    if(!$fences.Count){throw "Declared diagram family has no original example: $($family.document)"}
    $source=$fences[0].Groups[1].Value.Replace("`r`n","`n")
    # Family dispatch is tested after an original diagram-local YAML header,
    # where present. All 105 flowchart vectors above retain that header intact.
    $source=[regex]::Replace($source,'(?ms)\A---\n.*?^---[ \t]*\n','')
    $expected=$family.firstExample
    [void]$output.AppendLine(('    {{{0}, {1}, FlowchartOutcome::{2}, FlowchartIssue::{3}, {4}u, {5}u, FlowchartDirection::{6}, {7}}},' -f `
        (Literal $family.document),(Literal $source),$expected.outcome,$expected.issue,$expected.nodes,$expected.edges,$expected.direction,(Literal $expected.reason)))
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
Write-Host 'Native flowchart vectors: 105 original examples, 16 admitted graphs, 89 unsupported; 20 original family examples (2 limited profiles, 18 unsupported families).'
