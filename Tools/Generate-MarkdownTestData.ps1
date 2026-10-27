# Original corpus preservation vectors, not a claim of HTML conformance.
[CmdletBinding()]
param(
    [string] $InputDirectory=(Join-Path $PSScriptRoot '../ThirdParty/Markdown'),
    [string] $OutputPath=(Join-Path $PSScriptRoot '../Tests/Controls/Controls.Tests.MarkdownCorpus.h'),
    [switch] $Check
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$pins=@{
    'corpora/CommonMark-0.31.2-spec.json'='D431B29D97B6F73E69D547109CF5081578FAC931E72AFE95639EBE766C1B2A20'
    'corpora/CommonMark-0.31.2-LICENSE.txt'='3E4806BA6F20073E8CE40DA5A0C4B59F7F44287965F538E195A4D734D833557B'
    'corpora/GFM-0.29.0.gfm.13-spec.txt'='7D8E5814BEFEC287AC116786D81FF14E0ADC9B13295B4494649E995408FD871C'
    'corpora/GFM-0.29.0.gfm.13-COPYING.txt'='C22E885F33B821BDDB24CF007145E5540655B6C0F403E49E6C76A93C28E6D9A9'
    'whatwg-2026-10-18/entities.json'='D741D877AC77C4194C4AD526B5B4A19AEF8DFE411AB840A466891CDBB9F362E6'
    'whatwg-2026-10-18/LICENSE.txt'='85DC6F5CCB57A6FE8C33D158F9FC8FC7EE5655A5D3DB2CDD131C6A3D0F48A864'
}
foreach($name in $pins.Keys){if((Get-FileHash -LiteralPath (Join-Path $InputDirectory $name)).Hash -cne $pins[$name]){throw "Pinned Markdown test input differs: $name"}}
function Literal([string]$Text){
    $value=[Text.StringBuilder]::new()
    foreach($unit in $Text.ToCharArray()){[void]$value.Append(('\x{0:X4}' -f [int]$unit))}
    return ('std::wstring_view(L"{0}", {1}u)' -f $value.ToString(),$Text.Length)
}
$common=@(Get-Content (Join-Path $InputDirectory 'corpora/CommonMark-0.31.2-spec.json') -Raw|ConvertFrom-Json)
$gfm=[Collections.Generic.List[string]]::new()
$gfmBlocks=[Collections.Generic.List[object]]::new();$section='';$html=[Text.StringBuilder]::new()
$state='outside';$delimiter='';$extensions='';$markdown=[Text.StringBuilder]::new()
foreach($line in [IO.File]::ReadAllLines((Join-Path $InputDirectory 'corpora/GFM-0.29.0.gfm.13-spec.txt'))){
    if($state -eq 'outside'){
        if($line -match '^#{1,6} (.+)$'){$section=$Matches[1]}
        if($line -match '^(`{32,}) example(.*)$'){$delimiter=$Matches[1];$extensions=$Matches[2].Trim();$state='markdown';[void]$markdown.Clear();[void]$html.Clear()}
    }elseif($state -eq 'markdown'){
        if($line -ceq '.'){$state='html'}else{[void]$markdown.Append($line.Replace([string][char]0x2192,"`t")).Append("`n")}
    }elseif($line -ceq $delimiter){$gfm.Add($markdown.ToString());$gfmBlocks.Add(@{section=$section;html=$html.ToString();number=$gfm.Count;extensions=$extensions});$state='outside'}
    else{[void]$html.Append($line.Replace([string][char]0x2192,"`t")).Append("`n")}
}
if($state -cne 'outside' -or $common.Count -ne 652 -or $gfm.Count -ne 672){throw 'Incomplete original Markdown corpus.'}
$output=[Text.StringBuilder]::new()
[void]$output.AppendLine(@'
#pragma once
#include <string_view>
// Generated original-source preservation vectors, not HTML conformance results.
// CommonMark/GFM CC-BY-SA notices and WHATWG source-data license: ThirdParty/Markdown.
namespace MarkdownCorpus
{
// clang-format off
inline constexpr std::wstring_view kCommonMark[] = {
'@)
foreach($example in $common){[void]$output.AppendLine(('    {0},' -f (Literal $example.markdown)))}
[void]$output.AppendLine('};')
[void]$output.AppendLine('struct BlockExample { std::size_t number; std::wstring_view section, html, extensions; };')
[void]$output.AppendLine('inline constexpr BlockExample kCommonMarkBlocks[] = {')
foreach($example in $common){[void]$output.AppendLine(('    {{{0}u, {1}, {2}, L""}},' -f $example.example,(Literal $example.section),(Literal $example.html)))}
[void]$output.AppendLine('};')
[void]$output.AppendLine('inline constexpr std::wstring_view kGfm[] = {')
foreach($example in $gfm){[void]$output.AppendLine(('    {0},' -f (Literal $example)))}
[void]$output.AppendLine('};')
[void]$output.AppendLine('inline constexpr BlockExample kGfmBlocks[] = {')
foreach($example in $gfmBlocks){[void]$output.AppendLine(('    {{{0}u, {1}, {2}, {3}}},' -f $example.number,(Literal $example.section),(Literal $example.html),(Literal $example.extensions)))}
[void]$output.AppendLine('};')
[void]$output.AppendLine('struct Entity { std::wstring_view source, expected; };')
[void]$output.AppendLine('inline constexpr Entity kEntities[] = {')
$entities=Get-Content (Join-Path $InputDirectory 'whatwg-2026-10-18/entities.json') -Raw|ConvertFrom-Json -AsHashtable
$names=[Collections.Generic.List[string]]::new()
foreach($name in $entities.Keys){if($name.EndsWith(';',[StringComparison]::Ordinal)){$names.Add($name)}}
$names.Sort([StringComparer]::Ordinal)
foreach($name in $names){[void]$output.AppendLine(('    {{{0}, {1}}},' -f (Literal $name),(Literal $entities[$name].characters)))}
[void]$output.AppendLine(@'
};
// clang-format on
} // namespace MarkdownCorpus
'@)
$expected=$output.ToString().Replace("`r`n","`n").Replace("`n","`r`n")
if($Check){if(-not(Test-Path -LiteralPath $OutputPath) -or [IO.File]::ReadAllText($OutputPath) -cne $expected){throw 'Markdown corpus output is missing or stale.'}}
else{[IO.File]::WriteAllText($OutputPath,$expected,[Text.UTF8Encoding]::new($false))}
Write-Host 'Markdown preservation vectors: 652 CommonMark, 672 GFM, 2125 independent entity values.'
