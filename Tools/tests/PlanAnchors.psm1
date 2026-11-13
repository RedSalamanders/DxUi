<#
  Evidence anchors in plan Markdown, checked against the checked-out tree. The rule is normative in
  Specs/Testing/Testing_Validation.md ("Plan evidence anchors"); Tools/tests/Test-PlanAnchors.ps1 tests it and runs it
  over Specs/Plans/WIP/*.md.

  Anchor forms, each written in backticks:
    `path:12`, `path:12-30`, `path:12, 40-44`      a path containing '/' is relative to the repository root; a file
                                                   name without a directory names a file under src/Controls/
    [File.cpp](../../src/Controls/File.cpp) `:12`  a Markdown link to a file followed directly by a backticked line list
    `:12-30`                                       continues the nearest file named earlier in the same item, paragraph or
                                                   table row; with none it is reported as unresolved
    `path:12`, `40-44`                             a bare line list chained by a comma (or ", and") after an anchor
                                                   continues that anchor's file
  Anchors inside a ticked item (a top-level `- [x]` list item) describe code before that item landed and are skipped.
#>
Set-StrictMode -Version Latest

$script:AnchorPattern = '^(?<path>[A-Za-z0-9_][A-Za-z0-9_.\-/]*\.[A-Za-z][A-Za-z0-9]*):(?<lines>\d+(?:-\d+)?(?:,[ ]*\d+(?:-\d+)?)*)$'
$script:ContinuationPattern = '^:(?<lines>\d+(?:-\d+)?(?:,[ ]*\d+(?:-\d+)?)*)$'
$script:ChainedPattern = '^(?<lines>\d+(?:-\d+)?)$'
$script:AdjacentGapPattern = '^[ \t\r\n]*\(?[ \t\r\n]*$'
$script:ChainGapPattern = '^[ \t\r\n]*,[ \t\r\n]*(?:and[ \t\r\n]+)?$'
$script:SymbolPattern = '^(?<name>[A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)*)(?:\(\))?$'
$script:ItemIdPattern = '^[A-Z]{1,3}\d+(?:\.\d+)*$'
$script:TokenPattern = '\[(?<text>[^\]\r\n]*)\]\((?<target>[^)\s]+)\)|`(?<code>[^`]*)`'

function ConvertTo-DxUiPlanAnchorSymbol([AllowEmptyString()][string] $Code) {
    <# The symbol rule: a backticked span is a code symbol when it is an identifier, optionally qualified with `::` and
       optionally followed by `()`, that starts with an upper-case letter or an underscore or is qualified. Not symbols:
       names starting lower-case (prose words, Win32 parameter names such as `lParam`), all-capital names (Win32
       messages and constants such as `WM_MOUSEMOVE`, which name an event rather than the cited code), numbers, units,
       paths, file names, expressions and plan ids (`W2.1`, `FX5`, `G14`, `A6`). Returns the name searched for, the last
       `::` component, or $null. #>
    $match = [regex]::Match($Code, $script:SymbolPattern)
    if (-not $match.Success) { return $null }
    $name = $match.Groups['name'].Value
    if ($name -cmatch $script:ItemIdPattern) { return $null }
    if ($name -cmatch '^[A-Z0-9_]+$') { return $null }
    if (-not ($name -cmatch '^[A-Z_]' -or $name.Contains('::'))) { return $null }
    return ($name -split '::')[-1]
}

function ConvertTo-LineRanges([string] $Text) {
    foreach ($part in $Text.Split(',')) {
        $bounds = $part.Trim().Split('-')
        $first = [int]$bounds[0]
        $last = if ($bounds.Count -gt 1) { [int]$bounds[1] } else { $first }
        [pscustomobject]@{ First = $first; Last = $last; Text = $part.Trim() }
    }
}

function Get-PlanBlocks([string[]] $Lines) {
    <# A block is a top-level list item with every indented or blank line under it, or a run of unindented paragraph or
       table lines. Headings and fenced code end a block and are not read. #>
    $blocks = [Collections.Generic.List[object]]::new()
    $current = $null
    $fence = $false
    for ($index = 0; $index -lt $Lines.Count; ++$index) {
        $line = $Lines[$index]
        if ($line -match '^\s*(```|~~~)') { $fence = -not $fence; $current = $null; continue }
        if ($fence) { continue }
        if ($line -match '^#') { $current = $null; continue }
        if (-not $line.Trim()) {
            if ($null -ne $current -and -not $current.Item) { $current = $null }
            continue
        }
        $item = $line -match '^(?:[-*+]|\d+\.)\s'
        if ($item -or $null -eq $current -or ($line -notmatch '^\s' -and $current.Item)) {
            $current = [pscustomobject]@{
                Item = $item
                Ticked = $item -and $line -match '^[-*+]\s+\[[xX]\]\s'
                Lines = [Collections.Generic.List[string]]::new()
                Numbers = [Collections.Generic.List[int]]::new()
            }
            $blocks.Add($current)
        }
        $current.Lines.Add($line)
        $current.Numbers.Add($index + 1)
    }
    return $blocks
}

function Get-BlockTokens([object] $Block) {
    <# Links and code spans (a code span may wrap a line) in order, separated by gaps of prose. A gap ends a sentence or
       table cell (Break) at a sentence stop before anything but a lower-case letter, at a table cell boundary, at a new
       list item or table row, and after a table row; it ends a clause (Clause) also at a semicolon. #>
    $text = [string]::Join("`n", $Block.Lines)
    $starts = [int[]]::new($Block.Lines.Count)
    $offset = 0
    for ($i = 0; $i -lt $Block.Lines.Count; ++$i) { $starts[$i] = $offset; $offset += $Block.Lines[$i].Length + 1 }
    $lineAt = {
        param([int] $Position)
        $at = 0
        for ($i = 0; $i -lt $starts.Count; ++$i) { if ($starts[$i] -le $Position) { $at = $i } }
        $Block.Numbers[$at]
    }
    $tokens = [Collections.Generic.List[object]]::new()
    $addGap = {
        param([int] $From, [int] $To, [string] $Next)
        $gap = $text.Substring($From, $To - $From)
        $row = $gap.Contains('|')
        $break = $row
        foreach ($newline in [regex]::Matches($gap, '\n')) {
            $at = $From + $newline.Index
            $lineStart = $text.LastIndexOf("`n", [Math]::Max(0, $at - 1)) + 1
            $rest = $text.Substring($at + 1)
            if ($text.Substring($lineStart, $at - $lineStart) -match '^\s*\|' -or $rest -match '^\s*\|') { $row = $true }
            if ($row -or $rest -match '^\s*(?:[-*+]\s|\d+\.\s)') { $break = $true }
        }
        foreach ($stop in [regex]::Matches($gap, '[.!?](?=\s|$)')) {
            $after = $gap.Substring($stop.Index + 1).TrimStart()
            $first = if ($after) { $after[0] } elseif ($Next) { $Next[0] } else { [char]'A' }
            if (-not [char]::IsLower($first)) { $break = $true }
        }
        $tokens.Add([pscustomobject]@{ Kind = 'Gap'; Text = $gap; Break = $break; Row = $row; Clause = ($break -or $gap.Contains(';')); Line = (& $lineAt $From) })
    }
    $position = 0
    foreach ($match in [regex]::Matches($text, $script:TokenPattern)) {
        & $addGap $position $match.Index $match.Value
        $kind = if ($match.Groups['code'].Success) { 'Code' } else { 'Link' }
        $value = if ($kind -eq 'Code') { $match.Groups['code'].Value } else { $match.Groups['target'].Value }
        $tokens.Add([pscustomobject]@{ Kind = $kind; Text = $value; Line = (& $lineAt $match.Index) })
        $position = $match.Index + $match.Length
    }
    & $addGap $position $text.Length ''
    return $tokens
}

function Resolve-AnchorPath([string] $Root, [string] $PlanDirectory, [string] $Path, [switch] $Link) {
    if ($Link) {
        $target = [Uri]::UnescapeDataString($Path.Split('#', 2)[0])
        if (-not $target -or $target -match '^[A-Za-z][A-Za-z0-9+.-]*:') { return $null }
        $full = [IO.Path]::GetFullPath((Join-Path $PlanDirectory $target))
    } elseif ($Path.Contains('/')) {
        $full = [IO.Path]::GetFullPath((Join-Path $Root $Path))
    } else {
        $full = [IO.Path]::GetFullPath((Join-Path $Root "src/Controls/$Path"))
    }
    $prefix = $Root.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { return $null }
    return $full.Substring($prefix.Length).Replace('\', '/')
}

function Get-DxUiPlanAnchorReport {
    <# Returns, per plan, the anchors checked and skipped, every anchor, and every problem: MissingFile, OutOfRange,
       Unresolved (a `:lines` continuation with no file before it in its item, paragraph or table row) and Drifted (the symbol written
       with an anchor group occurs in none of its files). #>
    [CmdletBinding()] param([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string[]] $PlanPath)
    $Root = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $files = @{}
    $readFile = {
        param([string] $Relative)
        if (-not $files.ContainsKey($Relative)) {
            $full = Join-Path $Root $Relative
            $files[$Relative] = if ([IO.File]::Exists($full)) {
                $content = [IO.File]::ReadAllText($full)
                $count = [regex]::Matches($content, '\n').Count
                if ($content.Length -and -not $content.EndsWith("`n")) { ++$count }
                [pscustomobject]@{ Text = $content; LineCount = $count }
            } else { $null }
        }
        $files[$Relative]
    }
    $anchors = [Collections.Generic.List[object]]::new()
    $problems = [Collections.Generic.List[object]]::new()
    $plans = [Collections.Generic.List[object]]::new()
    $addProblem = {
        param([string] $Kind, [string] $Plan, [int] $Line, [string] $Path, [string] $Symbol, [string] $Detail)
        $problems.Add([pscustomobject]@{ Kind = $Kind; Plan = $Plan; PlanLine = $Line; Path = $Path; Symbol = $Symbol
            Message = "${Plan}:${Line}: $($Kind.ToLowerInvariant()): $Detail" })
    }
    foreach ($plan in $PlanPath) {
        $planFull = [IO.Path]::GetFullPath($plan)
        $planName = if ($planFull.StartsWith($Root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            $planFull.Substring($Root.Length + 1).Replace('\', '/')
        } else { $planFull }
        $planDirectory = [IO.Path]::GetDirectoryName($planFull)
        $checked = 0
        $skipped = 0
        foreach ($block in Get-PlanBlocks ([IO.File]::ReadAllLines($planFull))) {
            $tokens = @(Get-BlockTokens $block)
            $lastFile = $null   # nearest file named earlier in the item, paragraph or table row
            $pending = $null    # symbol written directly before the next anchor (only whitespace or "(" between)
            $group = [Collections.Generic.List[object]]::new()
            $groupSymbol = $null
            $closeGroup = {
                # A symbol binds to the anchor group that follows it; it must occur in one of the group's files.
                if ($null -ne $groupSymbol) { foreach ($member in $group) { $member.Symbol = $groupSymbol.Text } }
                if ($group.Count -and $null -ne $groupSymbol -and -not $block.Ticked) {
                    $present = @($group | Where-Object { $null -ne $_.File })
                    $pattern = '(?<![A-Za-z0-9_])' + [regex]::Escape($groupSymbol.Name) + '(?![A-Za-z0-9_])'
                    if ($present.Count -and -not @($present | Where-Object { [regex]::IsMatch($_.File.Text, $pattern) }).Count) {
                        $cited = @($present | ForEach-Object { "$($_.Path):$($_.Lines)" }) -join '; '
                        & $addProblem 'Drifted' $planName $groupSymbol.Line $cited $groupSymbol.Text "symbol ``$($groupSymbol.Text)`` occurs in none of $cited"
                    }
                }
                $group.Clear()
                $groupSymbol = $null
            }
            for ($index = 0; $index -lt $tokens.Count; ++$index) {
                $token = $tokens[$index]
                if ($token.Kind -eq 'Gap') {
                    if ($group.Count -and ($token.Clause -or $token.Text -notmatch $script:ChainGapPattern)) { . $closeGroup; $pending = $null }
                    if ($token.Text -notmatch $script:AdjacentGapPattern) { $pending = $null }
                    if ($token.Row) { $lastFile = $null }
                    continue
                }
                $path = $null
                $lines = $null
                $form = 'Continuation'
                $line = $token.Line
                $display = $token.Text
                if ($token.Kind -eq 'Link') {
                    $relative = Resolve-AnchorPath $Root $planDirectory $token.Text -Link
                    $gap = $tokens[$index + 1]
                    $next = if ($index + 2 -lt $tokens.Count) { $tokens[$index + 2] } else { $null }
                    if ($null -ne $relative -and $null -ne $next -and $next.Kind -eq 'Code' -and $gap.Text -match '^\s*$' -and $next.Text -match $script:ContinuationPattern) {
                        $path = $relative
                        $lines = $Matches['lines']
                        $display = "$($token.Text) $($next.Text)"
                        $form = 'Link'
                        $index += 2
                    } else {
                        if ($group.Count) { . $closeGroup; $pending = $null }
                        if ($null -ne $relative -and [IO.File]::Exists((Join-Path $Root $relative))) { $lastFile = $relative }
                        continue
                    }
                } elseif ($token.Text -match $script:AnchorPattern) {
                    $path = Resolve-AnchorPath $Root $planDirectory $Matches['path']
                    $lines = $Matches['lines']
                    $form = 'Path'
                } elseif ($token.Text -match $script:ContinuationPattern) {
                    $lines = $Matches['lines']
                    $path = $lastFile
                    if ($null -eq $path) {
                        if (-not $block.Ticked) { & $addProblem 'Unresolved' $planName $line $null $null "``$($token.Text)`` names no file earlier in its item, paragraph or table row" }
                        continue
                    }
                } elseif ($group.Count -and $token.Text -match $script:ChainedPattern) {
                    $path = $group[$group.Count - 1].Path
                    $lines = $Matches['lines']
                    $form = 'Chained'
                } else {
                    if ($group.Count) { . $closeGroup; $pending = $null }
                    $symbol = ConvertTo-DxUiPlanAnchorSymbol $token.Text
                    $pending = if ($null -ne $symbol) { [pscustomobject]@{ Name = $symbol; Text = $token.Text; Line = $line } } else { $null }
                    continue
                }
                if (-not $group.Count) { $groupSymbol = $pending; $pending = $null }
                if ($null -eq $path) {
                    if (-not $block.Ticked) { & $addProblem 'MissingFile' $planName $line $display $null "``$display`` lies outside the repository" }
                    continue
                }
                $anchor = [pscustomobject]@{ Plan = $planName; PlanLine = $line; Path = $path; Lines = $lines; Form = $form; Symbol = $null; Skipped = [bool]$block.Ticked; File = $null }
                $anchors.Add($anchor)
                $group.Add($anchor)
                $lastFile = $path
                if ($block.Ticked) { ++$skipped; continue }
                ++$checked
                $file = & $readFile $path
                if ($null -eq $file) {
                    & $addProblem 'MissingFile' $planName $line $path $null "$path does not exist"
                    continue
                }
                $anchor.File = $file
                foreach ($range in ConvertTo-LineRanges $lines) {
                    if ($range.First -lt 1 -or $range.Last -lt $range.First -or $range.Last -gt $file.LineCount) {
                        & $addProblem 'OutOfRange' $planName $line $path $null "${path}:$($range.Text) is outside the file's $($file.LineCount) lines"
                    }
                }
            }
            . $closeGroup
        }
        $plans.Add([pscustomobject]@{ Plan = $planName; Checked = $checked; Skipped = $skipped })
    }
    return [pscustomobject]@{ Plans = $plans.ToArray(); Anchors = $anchors.ToArray(); Problems = $problems.ToArray() }
}

Export-ModuleMember -Function ConvertTo-DxUiPlanAnchorSymbol, Get-DxUiPlanAnchorReport
