# Changelog fragments. A change adds one file under Changes/ instead of editing CHANGELOG.md, so changes merge in any
# order: two branches that each prepend an entry to CHANGELOG.md conflict, two new files never do. Fold-Changelog.ps1
# moves the fragments into CHANGELOG.md, newest first, and validate-specs.ps1 checks their names and shape.
Set-StrictMode -Version Latest

$script:FragmentNamePattern = '^(?<date>\d{4}-\d{2}-\d{2})-[a-z0-9]+(?:-[a-z0-9]+)*\.md$'
$script:UnreleasedHeadingPattern = '(?m)^## Unreleased[ \t]*\r?\n(?:[ \t]*\r?\n)?'

function Get-ChangelogFragmentProblems([Parameter(Mandatory)][string] $Name, [AllowEmptyString()][string] $Text) {
    # A fragment is named <yyyy-mm-dd>-<topic>.md and holds one change: one top-level Markdown bullet, whose continuation
    # lines and sub-bullets are indented as in CHANGELOG.md.
    $problems = [Collections.Generic.List[string]]::new()
    $match = [regex]::Match($Name, $script:FragmentNamePattern)
    $parsed = [DateTime]::MinValue
    if (-not $match.Success) {
        $problems.Add("${Name}: name it <yyyy-mm-dd>-<topic>.md, the topic in lowercase letters, digits and hyphens")
    } elseif (-not [DateTime]::TryParseExact($match.Groups['date'].Value, 'yyyy-MM-dd', [Globalization.CultureInfo]::InvariantCulture,
            [Globalization.DateTimeStyles]::None, [ref]$parsed)) {
        $problems.Add("${Name}: $($match.Groups['date'].Value) is not a calendar date")
    }
    $lines = @($Text -split '\r?\n' | Where-Object { $_.Trim() })
    if ($lines.Count -eq 0 -or -not $lines[0].StartsWith('- ', [StringComparison]::Ordinal)) {
        $problems.Add("${Name}: start with a Markdown bullet ('- ')")
    }
    $entries = @($lines | Where-Object { $_.StartsWith('- ', [StringComparison]::Ordinal) })
    if ($entries.Count -gt 1) { $problems.Add("${Name}: holds $($entries.Count) entries; record one change and indent its sub-bullets") }
    if (@($lines | Select-Object -Skip 1 | Where-Object { -not $_.StartsWith(' ', [StringComparison]::Ordinal) -and -not $_.StartsWith('- ', [StringComparison]::Ordinal) }).Count) {
        $problems.Add("${Name}: indent every line after the bullet")
    }
    return , $problems.ToArray()
}

function Get-ChangelogFragments([Parameter(Mandatory)][string] $Root) {
    # The fragments under Changes/, newest first: their dated names sort that way, and fragments of one day by topic.
    $directory = Join-Path $Root 'Changes'
    if (-not [IO.Directory]::Exists($directory)) { return , @() }
    $paths = [string[]]@([IO.Directory]::EnumerateFiles($directory, '*.md') | Where-Object { [IO.Path]::GetFileName($_) -cne 'README.md' })
    [Array]::Sort($paths, [StringComparer]::Ordinal)
    [Array]::Reverse($paths)
    return , @($paths | ForEach-Object { [pscustomobject]@{ Name = [IO.Path]::GetFileName($_); Path = $_; Text = [IO.File]::ReadAllText($_) } })
}

function Merge-ChangelogFragments([Parameter(Mandatory)][string] $Changelog, [object[]] $Fragments) {
    # The changelog with the fragments' entries first under its Unreleased heading, in the order given, in the
    # changelog's own line endings.
    $heading = [regex]::Match($Changelog, $script:UnreleasedHeadingPattern)
    if (-not $heading.Success) { throw 'CHANGELOG.md has no "## Unreleased" heading to fold the fragments under' }
    if (-not $Fragments -or $Fragments.Count -eq 0) { return $Changelog }
    $newline = if ($Changelog.Contains("`r`n")) { "`r`n" } else { "`n" }
    $entries = foreach ($fragment in $Fragments) { (($fragment.Text.Trim() -split '\r?\n') -join $newline) + $newline }
    $at = $heading.Index + $heading.Length
    return $Changelog.Substring(0, $at) + ($entries -join '') + $Changelog.Substring($at)
}

Export-ModuleMember -Function Get-ChangelogFragmentProblems, Get-ChangelogFragments, Merge-ChangelogFragments
