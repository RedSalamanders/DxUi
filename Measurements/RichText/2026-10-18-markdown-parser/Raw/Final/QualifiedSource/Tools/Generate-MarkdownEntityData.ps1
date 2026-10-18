# Derive a sorted C++ data table from the exact WHATWG character-reference data.
[CmdletBinding()]
param(
    [string] $InputDirectory=(Join-Path $PSScriptRoot '../ThirdParty/Markdown/whatwg-2026-10-18'),
    [string] $OutputPath=(Join-Path $PSScriptRoot '../src/Controls/DxUi.MarkdownEntityData.h'),
    [switch] $Check
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$pins=@{
    'entities.json'='D741D877AC77C4194C4AD526B5B4A19AEF8DFE411AB840A466891CDBB9F362E6'
    'LICENSE.txt'='85DC6F5CCB57A6FE8C33D158F9FC8FC7EE5655A5D3DB2CDD131C6A3D0F48A864'
}
foreach($name in $pins.Keys){
    if((Get-FileHash -LiteralPath (Join-Path $InputDirectory $name)).Hash -cne $pins[$name]){throw "Pinned Markdown entity input differs: $name"}
}
$data=Get-Content -LiteralPath (Join-Path $InputDirectory 'entities.json') -Raw | ConvertFrom-Json -AsHashtable
$names=[Collections.Generic.List[string]]::new()
foreach($name in $data.Keys){if($name.EndsWith(';',[StringComparison]::Ordinal)){$names.Add($name)}}
$names.Sort([StringComparer]::Ordinal)
if($names.Count -ne 2125){throw 'Unexpected semicolon-terminated entity inventory.'}
$output=[Text.StringBuilder]::new()
[void]$output.AppendLine(@'
#pragma once
#include <cstdint>
#include <string_view>
// Generated data, Copyright WHATWG (Apple, Google, Mozilla, Microsoft).
// BSD-3-Clause source-code terms: ThirdParty/Markdown/whatwg-2026-10-18/LICENSE.txt.
// Regenerate with Tools/Generate-MarkdownEntityData.ps1; no parser implementation is imported.
namespace DxUi::RichText::MarkdownEntityData
{
struct Entry
{
    std::wstring_view name;
    std::uint32_t first, second;
};
// clang-format off
inline constexpr Entry kEntities[] = {
'@)
foreach($name in $names){
    if($name -cnotmatch '^&[A-Za-z][A-Za-z0-9]*;$' -or $name.Length -gt 33){throw 'Invalid bounded entity name.'}
    $points=@($data[$name].codepoints)
    if($points.Count -lt 1 -or $points.Count -gt 2){throw 'Unsupported entity expansion.'}
    $expected=[Text.StringBuilder]::new()
    foreach($point in $points){
        if($point -le 0 -or $point -gt 0x10ffff -or ($point -ge 0xd800 -and $point -le 0xdfff)){throw 'Invalid entity scalar.'}
        [void]$expected.Append([char]::ConvertFromUtf32($point))
    }
    if($expected.ToString() -cne $data[$name].characters){throw 'Entity scalar/string disagreement.'}
    $second=if($points.Count -eq 2){$points[1]}else{0}
    [void]$output.AppendLine(('    {{L"{0}", 0x{1:X}u, 0x{2:X}u}},' -f $name,$points[0],$second))
}
[void]$output.AppendLine(@'
};
// clang-format on
} // namespace DxUi::RichText::MarkdownEntityData
'@)
$text=$output.ToString().Replace("`r`n","`n").Replace("`n","`r`n")
if($Check){
    if(-not(Test-Path -LiteralPath $OutputPath) -or [IO.File]::ReadAllText($OutputPath) -cne $text){throw 'Generated Markdown entity table is stale.'}
}else{[IO.File]::WriteAllText($OutputPath,$text,[Text.UTF8Encoding]::new($false))}
Write-Host "Markdown entity data: $($names.Count) independently pinned records verified."
