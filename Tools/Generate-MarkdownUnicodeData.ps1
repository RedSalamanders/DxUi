# Locale-independent Markdown flanking, first-strong bidi classes and full default label folding.
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
    'DerivedBidiClass.txt'='4867B4B7F0731ED1BFCD34CC6251211FF1542541FCE0734B6FBDA139EE80B3A4'
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
    $first=-1; $last=-1; $rangeFirst=-1; $rangeCategory=''; $rangeBidi=''
    foreach($line in [IO.File]::ReadLines((Join-Path $InputDirectory 'UnicodeData.txt'))) {
        $fields=$line.Split(';')
        $scalar=[Convert]::ToInt32($fields[0],16)
        if($fields[1].EndsWith(', First>',[StringComparison]::Ordinal)){$rangeFirst=$scalar;$rangeCategory=$fields[2];$rangeBidi=$fields[4];continue}
        $intervalFirst=$scalar
        if($fields[1].EndsWith(', Last>',[StringComparison]::Ordinal)){
            if($rangeFirst -lt 0 -or $rangeCategory -cne $fields[2] -or $rangeBidi -cne $fields[4] -or $scalar -lt $rangeFirst){throw 'Invalid UnicodeData range pair'}
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
# UCD @missing rules establish defaults in order; explicit class intervals then
# override them, including BN noncharacters/default-ignorables and ET currency.
# This is independent of assigned-character coverage in UnicodeData.txt.
$bidi=[byte[]]::new(0x110000)
$bidiLines=[IO.File]::ReadAllLines((Join-Path $InputDirectory 'DerivedBidiClass.txt'))
foreach($line in $bidiLines) {
    if($line -notmatch '^# @missing:\s*([0-9A-F]+)\.\.([0-9A-F]+);\s*([A-Za-z_]+)') { continue }
    $first=[Convert]::ToInt32($Matches[1],16); $last=[Convert]::ToInt32($Matches[2],16)
    $value=switch -CaseSensitive ($Matches[3]) {
        'Left_To_Right' { [byte]1 }
        'Right_To_Left' { [byte]2 }
        'Arabic_Letter' { [byte]2 }
        'European_Terminator' { [byte]0 }
        default { throw "Unexpected bidi default: $($Matches[3])" }
    }
    if($first -gt $last -or $last -ge $bidi.Length){throw 'Invalid bidi default range'}
    [Array]::Fill($bidi,$value,$first,$last-$first+1)
}
$explicit=[byte[]]::new($bidi.Length)
$knownBidi=@('L','R','AL','EN','ES','ET','AN','CS','NSM','BN','B','S','WS','ON','LRE','LRO','RLE','RLO','PDF','LRI','RLI','FSI','PDI')
foreach($line in $bidiLines) {
    if($line -notmatch '^([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s*;\s*([A-Z]+)\s*#') { continue }
    $first=[Convert]::ToInt32($Matches[1],16); $last=if($Matches[2]){[Convert]::ToInt32($Matches[2],16)}else{$first}; $property=$Matches[3]
    if($first -gt $last -or $last -ge $bidi.Length){throw 'Invalid explicit bidi range'}
    if($property -cnotin $knownBidi){throw "Unexpected explicit bidi class: $property"}
    $value=if($property -ceq 'L'){[byte]1}elseif($property -cin @('R','AL')){[byte]2}else{[byte]0}
    for($index=$first;$index -le $last;++$index){if($explicit[$index]){throw 'Overlapping explicit bidi ranges'};$explicit[$index]=1}
    [Array]::Fill($bidi,$value,$first,$last-$first+1)
}
foreach($class in @(@{name='BidiL';value=1},@{name='BidiR';value=2})) {
    [void]$builder.AppendLine("inline constexpr Range k$($class.name)`[] = {")
    $first=-1
    for($index=0;$index -le $bidi.Length;++$index) {
        $match=$index -lt $bidi.Length -and $bidi[$index] -eq $class.value
        if($match -and $first -lt 0){$first=$index}
        elseif(-not $match -and $first -ge 0){[void]$builder.AppendLine(('{{0x{0:X}u,0x{1:X}u}},' -f $first,($index-1)));$first=-1}
    }
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
