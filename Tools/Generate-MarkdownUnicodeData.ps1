# Locale-independent Markdown flanking classes and full default label folding.
[CmdletBinding()]
param(
    [string]$InputDirectory=(Join-Path $PSScriptRoot '../ThirdParty/Unicode/17.0.0'),
    [string]$OutputPath=(Join-Path $PSScriptRoot '../src/Controls/DxUi.MarkdownUnicodeData.h'),
    [switch]$Check
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$pins=@{
    'CaseFolding.txt'='FF8D8FEFBF123574205085D6714C36149EB946D717A0C585C27F0F4EF58C4183'
    'UnicodeData.txt'='2E1EFC1DCB59C575EEDF5CCAE60F95229F706EE6D031835247D843C11D96470C'
    'LICENSE.txt'='E7A93B009565CFCE55919A381437AC4DB883E9DA2126FA28B91D12732BC53D96'
}
# The existing shared Unicode license remains owned by the grapheme generator.
foreach($name in $pins.Keys) {
    if((Get-FileHash -LiteralPath (Join-Path $InputDirectory $name) -Algorithm SHA256).Hash -cne $pins[$name]){throw "Pinned Markdown Unicode input differs: $name"}
}
$builder=[Text.StringBuilder]::new()
[void]$builder.AppendLine('// Generated from pinned Unicode 17.0.0; see ThirdParty/Unicode/17.0.0/LICENSE.txt.')
[void]$builder.AppendLine('// clang-format off')
[void]$builder.AppendLine('#pragma once')
[void]$builder.AppendLine('#include <cstdint>')
[void]$builder.AppendLine('namespace DxUi::RichText::Detail::MarkdownUnicodeData {')
[void]$builder.AppendLine('struct Range { std::uint32_t first, last; };')
foreach($class in @('Punctuation','Symbol','Space','Alphanumeric')) {
    [void]$builder.AppendLine("inline constexpr Range k$class`[] = {")
    $first=-1; $last=-1; $rangeFirst=-1; $rangeCategory=''
    foreach($line in [IO.File]::ReadLines((Join-Path $InputDirectory 'UnicodeData.txt'))) {
        $fields=$line.Split(';')
        $scalar=[Convert]::ToInt32($fields[0],16)
        if($fields[1].EndsWith(', First>',[StringComparison]::Ordinal)){$rangeFirst=$scalar;$rangeCategory=$fields[2];continue}
        $intervalFirst=$scalar
        if($fields[1].EndsWith(', Last>',[StringComparison]::Ordinal)){
            if($rangeFirst -lt 0 -or $rangeCategory -cne $fields[2] -or $scalar -lt $rangeFirst){throw 'Invalid UnicodeData range pair'}
            $intervalFirst=$rangeFirst;$rangeFirst=-1
        }
        $match=if($class -eq 'Punctuation'){$fields[2] -cmatch '^[PS]'}elseif($class -eq 'Symbol'){$fields[2] -cmatch '^S'}elseif($class -eq 'Alphanumeric'){$fields[2] -cmatch '^[LN]'}else{$fields[2] -ceq 'Zs'}
        if(-not $match){continue}
        if($first -ge 0 -and $intervalFirst -eq $last+1){$last=$scalar;continue}
        if($first -ge 0){[void]$builder.AppendLine(('{{0x{0:X}u,0x{1:X}u}},' -f $first,$last))}
        $first=$intervalFirst;$last=$scalar
    }
    if($first -ge 0){[void]$builder.AppendLine(('{{0x{0:X}u,0x{1:X}u}},' -f $first,$last))}
    if($rangeFirst -ge 0){throw 'Unterminated UnicodeData range'}
    [void]$builder.AppendLine('};')
}
[void]$builder.AppendLine('struct Fold { std::uint32_t scalar; std::uint32_t mapped[3]; unsigned count; };')
[void]$builder.AppendLine('inline constexpr Fold kFold[] = {')
foreach($line in [IO.File]::ReadLines((Join-Path $InputDirectory 'CaseFolding.txt'))) {
    if($line -notmatch '^([0-9A-F]+); ([CF]); ([0-9A-F ]+);'){continue}
    $scalar=[Convert]::ToInt32($Matches[1],16)
    $mapped=@($Matches[3].Trim().Split(' ') | ForEach-Object {[Convert]::ToInt32($_,16)})
    if($mapped.Count -gt 3){throw 'Unexpected full fold length'}
    $values=@($mapped | ForEach-Object {'0x{0:X}u' -f $_})
    while($values.Count -lt 3){$values+= '0u'}
    [void]$builder.AppendLine(('{{0x{0:X}u,{{{1}}},{2}u}},' -f $scalar,($values -join ','),$mapped.Count))
}
[void]$builder.AppendLine('};')
[void]$builder.AppendLine('} // namespace DxUi::RichText::Detail::MarkdownUnicodeData')
[void]$builder.AppendLine('// clang-format on')
$expected=$builder.ToString().Replace("`r`n","`n").Replace("`n","`r`n")
if($Check) {
    if(-not (Test-Path -LiteralPath $OutputPath) -or [IO.File]::ReadAllText($OutputPath) -cne $expected){throw 'Generated Markdown Unicode data differs'}
} else {[IO.File]::WriteAllText($OutputPath,$expected,[Text.UTF8Encoding]::new($false))}
