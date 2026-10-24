<# .SYNOPSIS The repository's CODEOWNERS: one file at .github/CODEOWNERS, rules GitHub reads as written, owners from the
reviewed set, every literal pattern a tracked path, and every protected tracked path owned. Needs git, no network. #>
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

# The owners a rule may name, and the tracked paths that must have one of them: CI, every script that decides what CI
# runs, measures or passes, and the agent instructions (Q12 of the 10 October code review).
$allowedOwners = @('@eric-jesover')
$protectedPath = '^(?:\.github/.+|Tools/.+|performance[^/]*\.ps1|\.agents/.+|AGENTS\.md)$'

# A rule's pattern as a regular expression over repository-relative paths. Only the forms this repository uses are
# accepted: a path anchored at the root with a leading slash, a trailing slash for a directory and everything under it,
# and * (within one path segment) or ** (across segments). Anything else is refused, so a new form extends this test.
function ConvertTo-OwnerPattern([string] $Pattern) {
    if (-not $Pattern.StartsWith('/') -or $Pattern -match '[\\!\[\]?]' -or $Pattern -match '//') {
        throw "Unsupported CODEOWNERS pattern: $Pattern"
    }
    $body = $Pattern.Substring(1)
    $directory = $body.EndsWith('/')
    $body = $body.TrimEnd('/')
    if (-not $body) { throw "A rule for the whole repository hides every other owner: $Pattern" }
    $regex = [Text.RegularExpressions.Regex]::Escape($body).Replace('\*\*', '.*').Replace('\*', '[^/]*')
    # A file pattern also names a directory of that name and everything under it, as in .gitignore.
    $suffix = if ($directory) { '/.+' } else { '(?:/.+)?' }
    return "^$regex$suffix$"
}

function Read-OwnerRules([string] $Text) {
    $rules = [Collections.Generic.List[object]]::new()
    $number = 0
    foreach ($line in ($Text -split "`r?`n")) {
        ++$number
        $trimmed = $line.Trim()
        if (-not $trimmed -or $trimmed.StartsWith('#')) { continue }
        $fields = @($trimmed -split '\s+')
        if ($fields.Count -lt 2) { throw "Line $number names no owner: $trimmed" }
        $rules.Add([pscustomobject]@{ Line = $number; Pattern = $fields[0]; Regex = ConvertTo-OwnerPattern $fields[0]; Owners = @($fields | Select-Object -Skip 1) })
    }
    return $rules.ToArray()
}

# The last rule that matches a path, as GitHub applies them; $null when none does.
function Resolve-PathOwners([object[]] $Rules, [string] $Path) {
    $match = $null
    foreach ($rule in $Rules) { if ($Path -cmatch $rule.Regex) { $match = $rule } }
    return $match
}

# The tracked paths as UTF-8, whatever the console's code page.
function Get-TrackedPaths([string] $Repository) {
    $start = [Diagnostics.ProcessStartInfo]::new('git')
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.StandardOutputEncoding = [Text.UTF8Encoding]::new($false)
    $start.StandardErrorEncoding = [Text.UTF8Encoding]::new($false)
    foreach ($argument in @('-C', $Repository, 'ls-files', '-z')) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    try {
        $errors = $process.StandardError.ReadToEndAsync()
        $text = $process.StandardOutput.ReadToEnd()
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw "git ls-files failed in ${Repository}: $($errors.GetAwaiter().GetResult())" }
        return @($text.Split([char]0, [StringSplitOptions]::RemoveEmptyEntries))
    } finally { $process.Dispose() }
}

$codeOwnersPath = Join-Path $root '.github/CODEOWNERS'
$tracked = Get-TrackedPaths $root

Invoke-TestCase 'the matcher reads patterns as GitHub does' {
    $rules = Read-OwnerRules "# comment`n/Tools/ @a`n/performance.ps1 @b`n/docs/*.md @c`n/src/**/x.h @d`n/Tools/tests/ @e"
    Assert-Equal '@e' (Resolve-PathOwners $rules 'Tools/tests/Test-X.ps1').Owners[0] 'the last matching rule wins'
    Assert-Equal '@a' (Resolve-PathOwners $rules 'Tools/BenchmarkGate.psm1').Owners[0] 'a directory rule owns what is under it'
    Assert-True ($null -eq (Resolve-PathOwners $rules 'src/Tools/x.ps1')) 'an anchored rule matches only at the root'
    Assert-True ($null -eq (Resolve-PathOwners $rules 'tools/x.ps1')) 'paths match case-sensitively, as on GitHub'
    Assert-Equal '@b' (Resolve-PathOwners $rules 'performance.ps1').Owners[0] 'a file rule owns its file'
    Assert-True ($null -eq (Resolve-PathOwners $rules 'performance.ps1.bak')) 'a file rule owns no longer name'
    Assert-Equal '@c' (Resolve-PathOwners $rules 'docs/hosting.md').Owners[0] '* matches within a segment'
    Assert-True ($null -eq (Resolve-PathOwners $rules 'docs/sub/hosting.md')) '* does not cross a segment'
    Assert-Equal '@d' (Resolve-PathOwners $rules 'src/a/b/x.h').Owners[0] '** crosses segments'
    foreach ($bad in @('Tools/', '/Tools\x', '!/Tools/', '/[Tt]ools/', '/', '/a//b')) {
        Assert-Throws { ConvertTo-OwnerPattern $bad } "an unsupported pattern is refused: $bad"
    }
    Assert-Throws { Read-OwnerRules '/Tools/' } 'a rule without an owner is refused'
}

Invoke-TestCase 'CODEOWNERS lives only at .github/CODEOWNERS' {
    Assert-True ([IO.File]::Exists($codeOwnersPath)) 'the file exists'
    Assert-Contains $tracked '.github/CODEOWNERS' 'the file is tracked'
    foreach ($other in @('CODEOWNERS', 'docs/CODEOWNERS')) {
        Assert-True (-not ($tracked -ccontains $other)) "GitHub reads only the first of .github/, the root and docs/; $other would confuse"
    }
}

$rules = Read-OwnerRules ([IO.File]::ReadAllText($codeOwnersPath))

Invoke-TestCase 'every rule names reviewed owners and a tracked path' {
    Assert-True ($rules.Count -gt 0) 'the file has rules'
    foreach ($rule in $rules) {
        foreach ($owner in $rule.Owners) { Assert-Contains $allowedOwners $owner "line $($rule.Line) names a reviewed owner" }
        $matched = @($tracked | Where-Object { $_ -cmatch $rule.Regex })
        Assert-True ($matched.Count -gt 0) "line $($rule.Line) ($($rule.Pattern)) names a tracked path; a renamed path loses its owner"
    }
}

Invoke-TestCase 'every protected tracked path has an owner' {
    $protected = @($tracked | Where-Object { $_ -cmatch $protectedPath })
    Assert-True ($protected -ccontains '.github/workflows/ci.yml' -and $protected -ccontains 'performance-paired.ps1' -and $protected -ccontains 'AGENTS.md') 'the protected set covers CI, the root performance scripts and the agent instructions'
    foreach ($path in $protected) {
        $rule = Resolve-PathOwners $rules $path
        Assert-True ($null -ne $rule) "$path has an owner"
    }
}

Complete-TestRun 'Code owners'
