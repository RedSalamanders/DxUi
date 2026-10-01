<# .SYNOPSIS Folds the changelog fragments under Changes/ into CHANGELOG.md, newest first, and removes them.
.DESCRIPTION
Run it in a change of its own after a batch of merges. A malformed fragment stops the fold before anything changes.
.PARAMETER Root
Repository root; defaults to this one. #>
[CmdletBinding()] param([string] $Root = (Split-Path $PSScriptRoot -Parent))
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Changelog.psm1') -Force

$Root = [IO.Path]::GetFullPath($Root)
$fragments = Get-ChangelogFragments $Root
$problems = [Collections.Generic.List[string]]::new()
foreach ($fragment in $fragments) {
    $found = Get-ChangelogFragmentProblems $fragment.Name $fragment.Text
    $problems.AddRange([string[]]$found)
}
if ($problems.Count) { throw "Fix the changelog fragments first:`n$($problems -join "`n")" }
if ($fragments.Count -eq 0) {
    Write-Host 'No changelog fragments to fold.'
    exit 0
}

$changelog = Join-Path $Root 'CHANGELOG.md'
$bytes = [IO.File]::ReadAllBytes($changelog)
$bom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
$merged = Merge-ChangelogFragments ([IO.File]::ReadAllText($changelog)) $fragments
[IO.File]::WriteAllText($changelog, $merged, [Text.UTF8Encoding]::new($bom))
$directory = [IO.Path]::GetFullPath((Join-Path $Root 'Changes'))
foreach ($fragment in $fragments) {
    # Only a fragment file directly under Changes/ is removed.
    if (-not [string]::Equals([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($fragment.Path)), $directory, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove a path outside Changes/: $($fragment.Path)"
    }
    Remove-Item -LiteralPath $fragment.Path
}
Write-Host "Folded $($fragments.Count) changelog fragments into CHANGELOG.md, newest first: $(@($fragments | ForEach-Object Name) -join ', ')"
