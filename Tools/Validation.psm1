# Repository validators: specifications and docs, skills, source ownership, inherited tests and the build matrix.
# Each Test-DxUi* function returns Failures and Messages instead of printing, so the tooling tests can drive it
# against fixture trees; the root validate-*.ps1 scripts print the result and fail on any finding.
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'Changelog.psm1')

$script:PathComparison = if ($IsWindows) { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }

function New-ValidationResult([Collections.Generic.List[string]] $Failures, [string[]] $Messages) {
    return [pscustomobject]@{ Failures = [string[]]$Failures.ToArray(); Messages = [string[]]@($Messages | Where-Object { $_ }) }
}

function Read-TextFile([Parameter(Mandatory)][string] $Path, [switch] $StripBom) {
    # Strict UTF-8 with universal newlines. A byte-order mark stays in the text unless -StripBom is given.
    $text = [Text.UTF8Encoding]::new($false, $true).GetString([IO.File]::ReadAllBytes($Path))
    if ($StripBom -and $text.Length -and $text[0] -eq [char]0xFEFF) { $text = $text.Substring(1) }
    return $text.Replace("`r`n", "`n").Replace("`r", "`n")
}

function Read-JsonFile([Parameter(Mandatory)][string] $Path, [switch] $StripBom) {
    return (Read-TextFile $Path -StripBom:$StripBom) | ConvertFrom-Json -AsHashtable -NoEnumerate
}

function Test-PyTruthy([object] $Value) {
    if ($null -eq $Value) { return $false }
    if ($Value -is [bool]) { return $Value }
    if ($Value -is [string]) { return $Value.Length -gt 0 }
    if ($Value -is [int] -or $Value -is [long] -or $Value -is [double] -or $Value -is [decimal] -or $Value -is [bigint]) { return $Value -ne 0 }
    if ($Value -is [Collections.ICollection]) { return $Value.Count -gt 0 }
    return $true
}

function Test-SameValue([object] $Left, [object] $Right) {
    # JSON equality without PowerShell coercion: 2 and '2' differ, and strings compare case-sensitively.
    if ($null -eq $Left -or $null -eq $Right) { return $null -eq $Left -and $null -eq $Right }
    $numeric = { param($v) $v -is [int] -or $v -is [long] -or $v -is [double] -or $v -is [decimal] -or $v -is [bigint] }
    if ((& $numeric $Left) -and (& $numeric $Right)) { return [double]$Left -eq [double]$Right }
    if ($Left -is [string] -and $Right -is [string]) { return [string]::Equals($Left, $Right, [StringComparison]::Ordinal) }
    if ($Left -is [bool] -and $Right -is [bool]) { return $Left -eq $Right }
    return $false
}

function Get-TypeName([object] $Value) {
    if ($null -eq $Value) { return 'null' }
    return $Value.GetType().Name
}

function Get-JsonValue([object] $Object, [string] $Key, [object] $Default = $null) {
    # A missing key reads as the default (Python's dict.get); a value that is not an object cannot be read.
    if ($Object -isnot [Collections.IDictionary]) { throw "expected a JSON object with '$Key', found $(Get-TypeName $Object)" }
    if ($Object.Contains($Key)) { return , $Object[$Key] }
    return , $Default
}

function Get-RequiredJsonValue([object] $Object, [string] $Key) {
    if ($Object -isnot [Collections.IDictionary] -or -not $Object.Contains($Key)) { throw "Missing key: $Key" }
    return , $Object[$Key]
}

function Test-FullMatch([object] $Value, [string] $Pattern) {
    # re.fullmatch on strings only; any other value is a type error, as in Python.
    if ($Value -isnot [string]) { throw "expected a string, found $(Get-TypeName $Value)" }
    return [regex]::IsMatch($Value, "^(?:$Pattern)\z")
}

function Get-FullPath([string] $Path) { return [IO.Path]::GetFullPath($Path) }

function Test-UnderPath([string] $Path, [string] $Root) {
    # pathlib's is_relative_to on normalized absolute paths, with this platform's case rules.
    $relative = [IO.Path]::GetRelativePath((Get-FullPath $Root), (Get-FullPath $Path))
    if ([IO.Path]::IsPathRooted($relative)) { return $false }
    return -not ($relative -eq '..' -or $relative.StartsWith('..' + [IO.Path]::DirectorySeparatorChar) -or $relative.StartsWith('../'))
}

function Get-RelativeText([string] $Path, [string] $Root, [switch] $Posix) {
    $relative = [IO.Path]::GetRelativePath((Get-FullPath $Root), (Get-FullPath $Path))
    if ($Posix) { return $relative.Replace('\', '/') }
    return $relative
}

function Test-PathExists([string] $Path) { return [IO.File]::Exists($Path) -or [IO.Directory]::Exists($Path) }

function Get-PySuffix([string] $Name) {
    # pathlib's suffix: '.user' for 'a.user', nothing for '.user' or 'a.'.
    $index = $Name.LastIndexOf('.')
    if ($index -le 0 -or $index -eq $Name.Length - 1) { return '' }
    return $Name.Substring($index)
}

function Get-TreeItems([string] $Root, [string] $Pattern = '*', [string[]] $PruneNames = @(), [switch] $IncludeDirectories) {
    # pathlib's rglob, including hidden entries. Directories named in PruneNames are not entered, and neither is one that
    # holds a .git entry, a file or a directory: that is another checkout (a nested repository, or a linked worktree such
    # as .claude/worktrees/<name> under the main checkout), whose files this tree does not own. The walk's own root is
    # entered whether or not it has a .git entry.
    $items = [Collections.Generic.List[string]]::new()
    if (-not [IO.Directory]::Exists($Root)) { return }
    $fileOptions = [IO.EnumerationOptions]::new()
    $fileOptions.MatchType = [IO.MatchType]::Simple
    $fileOptions.AttributesToSkip = [IO.FileAttributes]0
    $directoryOptions = [IO.EnumerationOptions]::new()
    $directoryOptions.MatchType = [IO.MatchType]::Simple
    $directoryOptions.AttributesToSkip = [IO.FileAttributes]::ReparsePoint
    $pending = [Collections.Generic.Stack[string]]::new()
    $pending.Push((Get-FullPath $Root))
    while ($pending.Count) {
        $directory = $pending.Pop()
        foreach ($file in [IO.Directory]::EnumerateFiles($directory, $Pattern, $fileOptions)) { $items.Add($file) }
        foreach ($child in [IO.Directory]::EnumerateDirectories($directory, '*', $directoryOptions)) {
            if (Test-PathExists ([IO.Path]::Combine($child, '.git'))) { continue }
            if ($IncludeDirectories -and [IO.Enumeration.FileSystemName]::MatchesSimpleExpression($Pattern, [IO.Path]::GetFileName($child), [bool]$IsWindows)) { $items.Add($child) }
            if ([IO.Path]::GetFileName($child) -cnotin $PruneNames) { $pending.Push($child) }
        }
    }
    return $items
}

function Join-DataPath([string] $Parent, [string] $Child) {
    # pathlib's '/': a rooted child replaces the parent. Join-Path would append it and resolve provider drives.
    return [IO.Path]::Combine($Parent, $Child)
}

function Get-GitPathList([string] $Root, [string[]] $Arguments) {
    # NUL-separated git output decoded as UTF-8, independent of the console code page.
    $start = [Diagnostics.ProcessStartInfo]::new('git')
    foreach ($argument in @('-C', $Root) + $Arguments) { $start.ArgumentList.Add($argument) }
    $start.RedirectStandardOutput = $true
    $start.StandardOutputEncoding = [Text.UTF8Encoding]::new($false)
    $start.UseShellExecute = $false
    $process = [Diagnostics.Process]::Start($start)
    try {
        $output = $process.StandardOutput.ReadToEnd()
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw "git $($Arguments -join ' ') failed with exit code $($process.ExitCode) under $Root" }
    } finally { $process.Dispose() }
    return [string[]]@($output.Split([char]0) | Where-Object { $_ })
}

function Sort-Ordinal([string[]] $Values) {
    $copy = [string[]]@($Values)
    [Array]::Sort($copy, [StringComparer]::Ordinal)
    return $copy
}

function Format-PyList([string[]] $Values) {
    return '[' + ((Sort-Ordinal $Values | ForEach-Object { "'$_'" }) -join ', ') + ']'
}

# --- Specifications, documentation, design system and measurements --------------------------------------------

function Get-MarkdownProse([string] $Content) {
    # C++ lambdas such as [](bool checked) in fenced code are code, not Markdown links.
    return [regex]::Replace($Content, '(?ms)^(`{3,}|~{3,})[^\n]*\n.*?^\1[ \t]*$', '')
}

function Get-CatalogControls([string] $Root) {
    $catalog = Read-TextFile (Join-Path $Root 'include/DxUi/ControlCatalog.h')
    $match = [regex]::Match($catalog, 'enum class ControlKind[^\{]*\{([^}]+)')
    if (-not $match.Success) { throw 'include/DxUi/ControlCatalog.h declares no ControlKind enum' }
    return , [string[]]@($match.Groups[1].Value.Split(',') | ForEach-Object { $_.Trim() } | Where-Object { $_ })
}

function Test-DxUiMeasurements([Parameter(Mandatory)][string] $Root) {
    $failures = [Collections.Generic.List[string]]::new()
    if (@(Get-TreeItems (Join-Path $Root 'docs/measurements') '*.json').Count) {
        $failures.Add('Raw measurements belong in Measurements or their consumer repository, not docs')
    }
    foreach ($path in Get-TreeItems (Join-Path $Root 'Measurements') '*.json') {
        if ($path.EndsWith('.comparison.json', [StringComparison]::Ordinal)) { continue }
        $relative = Get-RelativeText $path $Root
        try {
            $receipt = Read-JsonFile $path -StripBom
            # Evaluated in Python's short-circuit order, so a malformed receipt reports what the original reported.
            $nonLibrary = -not (Test-SameValue (Get-JsonValue $receipt 'workloadOwner') 'DxUi')
            if (-not $nonLibrary) {
                $fixture = Get-JsonValue $receipt 'fixture' ''
                if ($fixture -isnot [string]) { throw "fixture is not a string ($(Get-TypeName $fixture))" }
                $nonLibrary = -not $fixture.StartsWith('dxui-', [StringComparison]::Ordinal)
            }
            if ($nonLibrary) { $failures.Add("Non-library measurement: $relative") }
            $inputs = Get-JsonValue $receipt 'benchmarkInputs' @{}
            $invalid = -not (Test-PyTruthy $inputs)
            if (-not $invalid) {
                if ($inputs -isnot [Collections.IDictionary]) { throw "benchmarkInputs is not a JSON object ($(Get-TypeName $inputs))" }
                foreach ($value in $inputs.Values) { if (-not (Test-FullMatch $value '[0-9a-fA-F]{64}')) { $invalid = $true; break } }
            }
            if ($invalid) { $failures.Add("Missing fixture input hashes: $relative") }
            if (-not [IO.File]::Exists((Join-Path (Split-Path $path) 'README.md'))) { $failures.Add("Missing measurement explanation: $relative") }
        } catch {
            $failures.Add("Invalid measurement ${relative}: $($_.Exception.Message)")
        }
    }
    return New-ValidationResult $failures @()
}

function Test-DxUiDesignSystem([Parameter(Mandatory)][string] $Root) {
    # Every catalog control has design-system guidelines and a preview; no stale component remains.
    $failures = [Collections.Generic.List[string]]::new()
    $system = Join-Path $Root 'Specs/DesignSystem'
    try {
        $controls = Get-CatalogControls $Root
        $index = Read-JsonFile (Join-Path $system 'design-system.json') -StripBom
        $tokens = Read-JsonFile (Join-Path $system 'tokens.json') -StripBom
        if (-not (Test-SameValue (Get-JsonValue $index 'layout') 'files') -or -not (Test-PyTruthy (Get-JsonValue $index 'title'))) {
            $failures.Add('Invalid design-system index')
        }
        $color = Get-RequiredJsonValue $tokens 'color'
        if (-not (Test-PyTruthy (Get-RequiredJsonValue $color 'themes')) -or -not (Test-PyTruthy (Get-RequiredJsonValue $color 'tokens'))) {
            $failures.Add('Design-system tokens need themes and colors')
        }
        if (-not (Read-TextFile (Join-Path $system 'README.md')).Trim()) { $failures.Add('Empty design-system README') }
        $components = Join-Path $system 'components'
        foreach ($name in $controls) {
            $guide = Join-DataPath (Join-DataPath $components $name) 'README.md'
            $preview = Join-DataPath (Join-DataPath $components $name) 'preview.html'
            if (-not [IO.File]::Exists($guide) -or -not [IO.File]::Exists($preview)) { $failures.Add("Missing design-system component: $name") }
            elseif (-not (Read-TextFile $preview).StartsWith('<!-- @dsCard ', [StringComparison]::Ordinal)) {
                $failures.Add("Design-system preview lacks its card marker: $name")
            }
        }
        $known = [Collections.Generic.HashSet[string]]::new([string[]]($controls + @('Cover')), [StringComparer]::Ordinal)
        foreach ($folder in [IO.Directory]::EnumerateDirectories($components)) {
            $name = [IO.Path]::GetFileName($folder)
            if (-not $known.Contains($name)) { $failures.Add("Design-system component is not in the catalog: $name") }
        }
    } catch {
        $failures.Add("Invalid design system: $($_.Exception.Message)")
    }
    return New-ValidationResult $failures @()
}

function Test-DxUiDocs([Parameter(Mandatory)][string] $Root) {
    $failures = [Collections.Generic.List[string]]::new()
    try {
        if (-not (Read-TextFile (Join-Path $Root 'README.md') -StripBom).Contains('](docs/README.md)')) {
            $failures.Add('Root README must link docs/README.md')
        }
        $controls = Get-CatalogControls $Root
        $guide = Read-TextFile (Join-Path $Root 'docs/controls.md') -StripBom
        foreach ($name in $controls) {
            if (-not [regex]::IsMatch($guide, '(?m)^\| ' + [regex]::Escape($name) + ' \|')) { $failures.Add("Missing control usage: $name") }
        }
        $gallery = Get-FullPath (Join-Path $Root 'docs/gallery')
        $receipt = Read-JsonFile (Join-Path $gallery 'generation.json') -StripBom
        if (-not (Test-SameValue (Get-RequiredJsonValue $receipt 'controlCount') $controls.Count)) {
            $failures.Add('Gallery/catalog control count mismatch')
        }
        $images = Get-RequiredJsonValue $receipt 'images'
        $entries = @($images)
        $files = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach ($entry in $entries) { [void]$files.Add([string](Get-RequiredJsonValue $entry 'file')) }
        if ($entries.Count -ne 6 -or $files.Count -ne 6) { $failures.Add('Gallery requires six distinct images') }
        foreach ($entry in $entries) {
            $file = [string](Get-RequiredJsonValue $entry 'file')
            $image = Get-FullPath (Join-DataPath $gallery $file)
            if (-not [string]::Equals([IO.Path]::GetDirectoryName($image), $gallery, $script:PathComparison) -or (Get-PySuffix ([IO.Path]::GetFileName($image))) -cne '.png') {
                $failures.Add('Invalid gallery image path')
                continue
            }
            $data = [IO.File]::ReadAllBytes($image)
            $signature = [byte[]](0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A)
            $isPng = $data.Length -ge 8 -and [Linq.Enumerable]::SequenceEqual([byte[]]$data[0..7], $signature)
            $hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($data)).ToLowerInvariant()
            $expected = Get-RequiredJsonValue $entry 'sha256'
            if ($expected -isnot [string]) { throw "the sha256 of $file is not a string ($(Get-TypeName $expected))" }
            if (-not $isPng -or $hash -cne $expected.ToLowerInvariant()) { $failures.Add("Gallery hash/format mismatch: $file") }
        }
    } catch {
        $failures.Add("Invalid documentation/gallery: $($_.Exception.Message)")
    }
    return New-ValidationResult $failures @()
}

function Test-DxUiSpecs([Parameter(Mandatory)][string] $Root) {
    $failures = [Collections.Generic.List[string]]::new()
    $required = @(
        'AGENTS.md', 'Specs/README.md', 'Specs/Core/Core_Architecture.md',
        'Specs/Core/Core_PerformanceAndResources.md', 'Specs/Build/Build_ToolchainAndConsumption.md',
        'Specs/UI/UI_ControlsAndLayout.md', 'Specs/UI/UI_ThemeAndTypography.md',
        'Specs/UI/UI_InputAndAccessibility.md', 'Specs/Rendering/Rendering_EmbeddedD3D11.md',
        'Specs/Rendering/Rendering_Win32Host.md', 'Specs/Testing/Testing_Validation.md',
        'Specs/Core/Core_Documentation.md', 'Specs/UI/UI_DesignSystem.md', 'Specs/DesignSystem/README.md', 'docs/README.md',
        'docs/controls.md', 'docs/getting-started.md', 'docs/hosting.md', 'docs/performance.md', 'docs/gallery/README.md',
        'docs/samples.md', 'Measurements/README.md')
    foreach ($name in $required) {
        $path = Join-Path $Root $name
        if (-not [IO.File]::Exists($path) -or -not (Read-TextFile $path).Trim()) { $failures.Add("Missing or empty $name") }
    }
    $files = @(Get-TreeItems $Root '*.md' -PruneNames @('.build', '.git'))
    foreach ($path in $files) {
        $content = Get-MarkdownProse (Read-TextFile $path)
        foreach ($match in [regex]::Matches($content, '\]\(([^)]+)\)')) {
            $target = $match.Groups[1].Value
            if ([regex]::IsMatch($target, '^[a-z]+://|^#')) { continue }
            $target = [Uri]::UnescapeDataString($target.Split('#', 2)[0].Trim('<', '>'))
            if (-not (Test-PathExists (Join-DataPath ([IO.Path]::GetDirectoryName($path)) $target))) {
                $failures.Add("$(Get-RelativeText $path $Root): broken reference $target")
            }
        }
    }
    $active = Join-Path $Root 'Specs/Plans/WIP'
    $index = Read-TextFile (Join-Path $active 'README.md')
    $indexed = @([regex]::Matches($index, '\]\(([^)]+\.md)\)') | ForEach-Object { $_.Groups[1].Value })
    $expected = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($plan in [IO.Directory]::EnumerateFiles($active, '*.md')) {
        $name = [IO.Path]::GetFileName($plan)
        if ($name -cne 'README.md') { [void]$expected.Add($name) }
    }
    $indexedSet = [Collections.Generic.HashSet[string]]::new([string[]]$indexed, [StringComparer]::Ordinal)
    if (-not $indexedSet.SetEquals($expected) -or $indexed.Count -ne $indexedSet.Count) {
        $failures.Add('Every direct WIP plan must appear exactly once in its index')
    }
    foreach ($part in @((Test-DxUiDocs $Root), (Test-DxUiDesignSystem $Root), (Test-DxUiMeasurements $Root), (Test-DxUiChangelog $Root))) {
        $failures.AddRange($part.Failures)
    }
    return New-ValidationResult $failures @("Validated $($files.Count) Markdown files, $($required.Count) authority files and $($expected.Count) active plans")
}

function Test-DxUiChangelog([Parameter(Mandatory)][string] $Root) {
    # A change is recorded as a fragment under Changes/, which its README explains, until Fold-Changelog.ps1 moves it under
    # CHANGELOG.md's Unreleased heading. Every fragment is one dated bullet.
    $failures = [Collections.Generic.List[string]]::new()
    if (-not [IO.File]::Exists((Join-Path $Root 'Changes/README.md'))) { $failures.Add('Changes/README.md must explain changelog fragments') }
    $changelog = Join-Path $Root 'CHANGELOG.md'
    if (-not [IO.File]::Exists($changelog) -or -not [regex]::IsMatch((Read-TextFile $changelog -StripBom), '(?m)^## Unreleased[ \t]*\r?$')) {
        $failures.Add('CHANGELOG.md must keep its "## Unreleased" heading, under which fragments are folded')
    }
    $fragments = Get-ChangelogFragments $Root
    foreach ($fragment in $fragments) {
        $problems = Get-ChangelogFragmentProblems $fragment.Name $fragment.Text
        foreach ($problem in $problems) { $failures.Add("Changelog fragment $problem") }
    }
    return New-ValidationResult $failures @()
}

# --- Skills -------------------------------------------------------------------------------------------------------

function ConvertFrom-SkillFrontMatter([string] $Text) {
    <# A strict subset of YAML: one 'key: value' per line, with plain, 'single' or "double" quoted scalars and # comment
       lines. Anything else is rejected, so every accepted value reads as the same string in any YAML parser. A plain
       scalar YAML 1.1 would type (null, booleans, numbers, dates) is returned as a non-string. #>
    $values = [ordered]@{}
    $lineNumber = 0
    foreach ($line in $Text.Split("`n")) {
        $lineNumber++
        if (-not $line.Trim() -or $line.TrimStart().StartsWith('#')) { continue }
        $match = [regex]::Match($line, '^(?<key>[A-Za-z_][A-Za-z0-9_-]*):(?:[ ]+(?<value>.*?))?[ ]*$')
        if (-not $match.Success) { throw "unsupported front matter at line ${lineNumber}: use one 'key: value' per line" }
        $key = $match.Groups['key'].Value
        if ($values.Contains($key)) { throw "duplicate front matter key '$key' at line $lineNumber" }
        $raw = $match.Groups['value'].Value
        $values[$key] = if (-not $raw) { $null }
        elseif ($raw[0] -eq "'") {
            $quoted = [regex]::Match($raw, "^'(?<text>(?:[^']|'')*)'(?:[ ]+#.*)?$")
            if (-not $quoted.Success) { throw "unterminated single-quoted value at line $lineNumber" }
            $quoted.Groups['text'].Value.Replace("''", "'")
        } elseif ($raw[0] -eq '"') {
            $quoted = [regex]::Match($raw, '^"(?<text>(?:[^"\\]|\\["\\])*)"(?:[ ]+#.*)?$')
            if (-not $quoted.Success) { throw "unsupported double-quoted value at line ${lineNumber}: only \"" and \\ escapes are accepted" }
            [regex]::Replace($quoted.Groups['text'].Value, '\\(["\\])', '$1')
        } else {
            if ($raw -match '^[-?:,\[\]{}#&*!|>''"%@`]' -or $raw.Contains(': ') -or $raw.Contains(' #') -or $raw.EndsWith(':') -or $raw -match '[\x00-\x1f]') {
                throw "unsupported plain value at line ${lineNumber}: quote it"
            }
            $typed = $raw -cmatch '^(?:~|null|Null|NULL|yes|Yes|YES|no|No|NO|true|True|TRUE|false|False|FALSE|on|On|ON|off|Off|OFF|=|<<)$' -or
                $raw -cmatch '^[-+]?(?:[0-9][0-9_:]*(?:\.[0-9_]*)?(?:[eE][-+]?[0-9]+)?|\.[0-9_]+(?:[eE][-+]?[0-9]+)?|0b[01_]+|0x[0-9a-fA-F_]+|\.(?:inf|Inf|INF|nan|NaN|NAN))$' -or
                $raw -cmatch '^[0-9]{4}-[0-9]{1,2}-[0-9]{1,2}'
            if ($typed) { [pscustomobject]@{ YamlScalar = $raw } } else { $raw }
        }
    }
    if ($values.Count -eq 0) { return $null }
    return $values
}

function Test-DxUiSkills([Parameter(Mandatory)][string] $Root) {
    $failures = [Collections.Generic.List[string]]::new()
    $messages = [Collections.Generic.List[string]]::new()
    $folder = Join-Path $Root '.agents/skills'
    $entries = @(Sort-Ordinal @([IO.Directory]::EnumerateFileSystemEntries($folder) | ForEach-Object { [string]$_ }))
    foreach ($entry in $entries) {
        if (-not [IO.Directory]::Exists($entry)) { continue }
        $name = [IO.Path]::GetFileName($entry)
        $path = Join-Path $entry 'SKILL.md'
        if (-not [IO.File]::Exists($path)) { $failures.Add("${name}: missing SKILL.md"); continue }
        $parts = (Read-TextFile $path).Split([string[]]@('---'), 3, [StringSplitOptions]::None)
        try {
            $metadata = if ($parts.Count -eq 3 -and -not $parts[0].Trim()) { ConvertFrom-SkillFrontMatter $parts[1] } else { $null }
        } catch {
            $failures.Add("${name}: $($_.Exception.Message)")
            continue
        }
        if ($metadata -isnot [Collections.IDictionary]) { $failures.Add("${name}: missing YAML front matter"); continue }
        $skillName = if ($metadata.Contains('name')) { $metadata['name'] } else { '' }
        $description = if ($metadata.Contains('description')) { $metadata['description'] } else { '' }
        if ($skillName -isnot [string] -or $skillName -cne $name -or -not (Test-FullMatch $skillName '[a-z0-9]+(?:-[a-z0-9]+)*') -or $skillName.Length -gt 64) {
            $failures.Add("${name}: invalid name")
        }
        if ($description -isnot [string] -or -not $description.Trim() -or $description.Length -gt 1024) { $failures.Add("${name}: invalid description") }
        if (-not $parts[2].Trim()) { $failures.Add("${name}: empty instructions") }
        $messages.Add("Validated $name")
    }
    if ($entries.Count -eq 0) { $failures.Add('No skills found') }
    return New-ValidationResult $failures $messages
}

# --- Source ownership and dependency boundaries -----------------------------------------------------------------

function Resolve-OwnedPath([string] $Root, [object] $Relative) {
    # An owned path is a relative, forward-slash path under src, include, Tests or Samples (or .clang-format).
    if ($Relative -isnot [string] -or -not $Relative -or $Relative.Contains('\') -or $Relative.StartsWith('/')) { return $null }
    $parts = @($Relative.Split('/') | Where-Object { $_ -and $_ -ne '.' })
    if ($parts.Count -eq 0 -or $parts -contains '..') { return $null }
    if (($Relative -cne '.clang-format' -and $parts[0] -cnotin @('src', 'include', 'Tests', 'Samples')) -or
        (Get-PySuffix $parts[-1]).ToLowerInvariant() -in @('.user', '.suo')) { return $null }
    $resolved = Get-FullPath (Join-Path $Root $Relative)
    if (Test-UnderPath $resolved $Root) { return $resolved }
    return $null
}

# A consumer restores its pin under <root>\.build\dependencies\DxUi\source\<commit>\, 74 characters after its root, and
# Git without long paths cannot create a full path over 259 characters. This budget leaves roots of up to 35 characters.
$script:ConsumerPathBudget = 150

function Get-CommittablePaths([string] $Root) {
    # What a commit of this tree would hold: tracked files and untracked ones git does not ignore, so ignored build
    # output is not a finding. A tree that is not a checkout (a fixture) is walked instead.
    if (Test-PathExists (Join-Path $Root '.git')) { return Get-GitPathList $Root @('ls-files', '-z', '--cached', '--others', '--exclude-standard') }
    return [string[]]@(Get-TreeItems $Root | ForEach-Object { Get-RelativeText $_ $Root -Posix })
}

function Test-DxUiDependencies([Parameter(Mandatory)][string] $Root) {
    $Root = Get-FullPath $Root
    $failures = [Collections.Generic.List[string]]::new()
    $manifest = Read-JsonFile (Join-Path $Root 'vcpkg.json')
    $tool = Read-JsonFile (Join-Path $Root 'vcpkg-tool.json')
    if (-not (Test-SameValue (Get-RequiredJsonValue $manifest 'builtin-baseline') (Get-RequiredJsonValue $tool 'commit')) -or
        -not (Test-FullMatch $tool['commit'] '[0-9a-f]{40}')) {
        $failures.Add('vcpkg baseline/tool revision mismatch')
    }
    $provenance = Read-JsonFile (Join-Path $Root 'Specs/Done/SourceImport/source-origin.json')
    if (-not (Test-SameValue (Get-JsonValue $provenance 'schemaVersion') 2) -or -not (Test-FullMatch (Get-RequiredJsonValue $provenance 'commit') '[0-9a-f]{40}')) {
        $failures.Add('Source origin needs schema 2 and an exact historical commit')
    }
    $origins = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $destinations = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $originFiles = Get-RequiredJsonValue $provenance 'files'
    foreach ($entry in @($originFiles)) {
        $original = [string](Get-RequiredJsonValue $entry 'source')
        if ($origins.Contains($original) -or -not (Test-FullMatch (Get-JsonValue $entry 'originalSha256' '') '[0-9a-f]{64}')) {
            $failures.Add("Invalid or duplicate source origin: $original")
        }
        [void]$origins.Add($original)
        $bytes = Get-JsonValue $entry 'originalBytes'
        if (-not ($bytes -is [int] -or $bytes -is [long] -or $bytes -is [bigint] -or $bytes -is [bool]) -or $bytes -lt 0) {
            $failures.Add("Invalid historical size: $original")
        }
        if (Test-SameValue (Get-JsonValue $entry 'disposition') 'retired') {
            if (-not (Test-PyTruthy (Get-JsonValue $entry 'reason')) -or $entry.Contains('currentPath')) {
                $failures.Add("Retired source requires a reason and no current path: $original")
            }
            continue
        }
        $relative = Get-JsonValue $entry 'currentPath' ''
        $path = Resolve-OwnedPath $Root $relative
        $disposition = Get-JsonValue $entry 'disposition'
        if (-not ((Test-SameValue $disposition 'owned') -or (Test-SameValue $disposition 'consolidated')) -or $null -eq $path -or -not [IO.File]::Exists($path)) {
            $failures.Add("Missing or invalid owned source: $relative")
        }
        if ($destinations.Contains([string]$relative)) { $failures.Add("Duplicate current source path: $relative") }
        [void]$destinations.Add([string]$relative)
    }
    if (Test-PathExists (Join-Path $Root 'upstream')) { $failures.Add('DxUi owns its source; remove the duplicate upstream tree') }

    # No roots are pending any longer; an inventory entry is always invalid, and supported source may not rely on one.
    $debt = Read-JsonFile (Join-Path $Root 'Specs/Done/SourceImport/pending-dependencies.json')
    $allowed = [ordered]@{}
    if (-not (Test-SameValue (Get-JsonValue $debt 'schemaVersion') 1)) { $failures.Add('Pending dependency inventory needs schema 1') }
    $debtFiles = Get-RequiredJsonValue $debt 'files'
    foreach ($entry in @($debtFiles)) {
        $relative = [string](Get-RequiredJsonValue $entry 'path')
        $includeList = Get-RequiredJsonValue $entry 'includes'
        $includes = [string[]]@($includeList)
        $failures.Add("Invalid or duplicate pending dependency path: $relative")
        $sortedUnique = Sort-Ordinal @([Collections.Generic.HashSet[string]]::new($includes, [StringComparer]::Ordinal))
        if ($includes.Count -eq 0 -or (($sortedUnique -join "`0") -cne ($includes -join "`0"))) {
            $failures.Add("Pending dependencies must be sorted and unique: $relative")
        }
        $allowed[$relative] = [Collections.Generic.HashSet[string]]::new($includes, [StringComparer]::Ordinal)
    }
    $observed = [ordered]@{}
    foreach ($folder in @('src', 'include', 'Tests', 'Samples')) {
        foreach ($path in Get-TreeItems (Join-Path $Root $folder) -IncludeDirectories) {
            $leaf = [IO.Path]::GetFileName($path)
            $suffix = Get-PySuffix $leaf
            if ($suffix.ToLowerInvariant() -in @('.user', '.suo')) { $failures.Add("Developer-local settings in owned source: $(Get-RelativeText $path $Root)") }
            if ($suffix -cnotin @('.h', '.cpp') -or -not [IO.File]::Exists($path)) { continue }
            $text = Read-TextFile $path -StripBom
            $relative = Get-RelativeText $path $Root -Posix
            if ($text.Contains('RedSalamander::DxUi')) { $failures.Add("Use the owned DxUi namespace: $relative") }
            $missing = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
            foreach ($include in [regex]::Matches($text, '#include\s*([<"])([^">]+)[">]')) {
                $opener = $include.Groups[1].Value
                $name = $include.Groups[2].Value
                if ($opener -eq '"') {
                    $candidates = @((Get-FullPath (Join-DataPath ([IO.Path]::GetDirectoryName($path)) $name)), (Get-FullPath (Join-DataPath (Join-Path $Root 'include') $name)))
                    $resolved = $candidates | Where-Object { [IO.File]::Exists($_) } | Select-Object -First 1
                    if ($null -eq $resolved) { [void]$missing.Add($name) }
                    elseif (-not (Test-UnderPath $resolved $Root)) { $failures.Add("Include escapes this repository: ${relative}: $name") }
                }
                $lower = $name.ToLowerInvariant()
                if ($name -ceq 'Helpers.h' -or @('upstream', 'redsalamander', 'redxe', 'pluginterfaces/viewer' | Where-Object { $lower.Contains($_) }).Count) {
                    $failures.Add("Application include in supported source: ${relative}: $name")
                }
            }
            if ($missing.Count) { $observed[$relative] = $missing }
        }
    }
    $paths = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($key in @($observed.Keys) + @($allowed.Keys)) { [void]$paths.Add($key) }
    foreach ($key in Sort-Ordinal @($paths)) {
        $actual = if ($observed.Contains($key)) { [string[]]@($observed[$key]) } else { [string[]]@() }
        $recorded = if ($allowed.Contains($key)) { [string[]]@($allowed[$key]) } else { [string[]]@() }
        if (((Sort-Ordinal $actual) -join "`0") -cne ((Sort-Ordinal $recorded) -join "`0")) {
            $failures.Add("Unresolved include inventory mismatch: ${key}: actual=$(Format-PyList $actual); recorded=$(Format-PyList $recorded)")
        }
    }
    # Compilable controls are normal source. Reject missing/escaping build inputs and split static libraries.
    $libraries = 0
    foreach ($folder in @('src', 'Tests', 'Samples')) {
        foreach ($project in Get-TreeItems (Join-Path $Root $folder) '*.vcxproj') {
            $document = [Xml.XmlDocument]::new()
            $document.Load($project)
            $elements = @($document.SelectNodes('//*'))
            if (@($elements | Where-Object { $_.LocalName -ceq 'ConfigurationType' -and $_.InnerText -ceq 'StaticLibrary' }).Count) { $libraries++ }
            foreach ($item in $elements) {
                if ($item.LocalName -cne 'ClCompile' -or -not $item.HasAttribute('Include')) { continue }
                $include = $item.GetAttribute('Include')
                $source = Get-FullPath (Join-DataPath ([IO.Path]::GetDirectoryName($project)) $include.Replace('\', '/'))
                if (-not (Test-UnderPath $source $Root) -or -not [IO.File]::Exists($source)) {
                    $failures.Add("Invalid build source: $(Get-RelativeText $project $Root): $include")
                }
            }
        }
    }
    if ($libraries -gt 1) { $failures.Add('DxUi ships one static library; split targets are not supported') }
    $committable = Get-CommittablePaths $Root
    foreach ($path in $committable) {
        if ($path.Length -gt $script:ConsumerPathBudget) {
            $failures.Add("Path of $($path.Length) characters, over the $($script:ConsumerPathBudget) a consumer's pinned restore can check out: $path")
        }
    }
    $interface = Test-DxUiConsumerInterface $Root
    foreach ($failure in $interface.Failures) { $failures.Add($failure) }
    return New-ValidationResult $failures @("Validated $($origins.Count) historical origins, $($destinations.Count) owned paths, $($allowed.Count) pending dependency records, supported-source independence, $($committable.Count) paths within $($script:ConsumerPathBudget) characters and $($interface.Entries) consumer interface entries at API revision $($interface.Revision)")
}

# --- Consumer interface -----------------------------------------------------------------------------------------

function Resolve-ConsumerInterfacePath([string] $Root, [object] $Relative) {
    # A relative, forward-slash path inside this repository, or nothing.
    if ($Relative -isnot [string] -or -not $Relative -or $Relative.Contains('\') -or $Relative.StartsWith('/')) { return $null }
    if (@($Relative.Split('/') | Where-Object { $_ -eq '..' }).Count) { return $null }
    $resolved = Get-FullPath (Join-Path $Root $Relative)
    if (Test-UnderPath $resolved $Root) { return $resolved }
    return $null
}

function Get-ParameterNames([object] $Parameters) {
    return , [string[]]@($Parameters | Where-Object { $_ } | ForEach-Object { $_.Name.VariablePath.UserPath })
}

function Get-MandatoryParameterNames([object[]] $ParameterAsts) {
    $mandatory = [Collections.Generic.List[string]]::new()
    foreach ($parameter in $ParameterAsts) {
        foreach ($attribute in $parameter.Attributes) {
            if ($attribute.TypeName.FullName -notmatch '(^|\.)Parameter(Attribute)?$') { continue }
            foreach ($argument in $attribute.NamedArguments) {
                if ($argument.ArgumentName -ine 'Mandatory') { continue }
                if ($null -eq $argument.Argument -or [bool]$argument.Argument.SafeGetValue()) { $mandatory.Add($parameter.Name.VariablePath.UserPath); break }
            }
        }
    }
    return , [string[]]@($mandatory | Sort-Object -Unique)
}

function Get-ConsumerMandatoryParameters([object] $Interface, [string] $Kind, [string] $Relative, [string] $Function = '') {
    # Walk optional JSON objects explicitly. Chained hashtable indexing throws when an older capabilities file omits
    # this additive contract, and PowerShell's array subexpression can turn a missing leaf into a null index target.
    $mandatory = Get-JsonValue $Interface 'mandatoryParameters' @{}
    $entries = Get-JsonValue $mandatory $Kind @{}
    if ($entries -isnot [Collections.IDictionary] -or -not $entries.Contains($Relative)) { return , [string[]]@() }
    $entry = $entries[$Relative]
    if ($Function) {
        if ($entry -isnot [Collections.IDictionary] -or -not $entry.Contains($Function)) { return , [string[]]@() }
        $entry = $entry[$Function]
    }
    return , [string[]]@($entry | Where-Object { $_ } | ForEach-Object { [string]$_ })
}

function Get-ScriptParameterNames([string] $Path) {
    # Read from the parse tree, so nothing runs.
    $ast = [Management.Automation.Language.Parser]::ParseFile($Path, [ref]$null, [ref]$null)
    if (-not $ast.ParamBlock) { return [pscustomobject]@{ Names=@(); Mandatory=@() } }
    return [pscustomobject]@{ Names=(Get-ParameterNames $ast.ParamBlock.Parameters); Mandatory=(Get-MandatoryParameterNames $ast.ParamBlock.Parameters) }
}

function Get-ModuleFunctionParameters([string] $Path) {
    # The functions a module exports, each with its parameter names, read from the parse tree, so nothing is imported or run.
    # Without Export-ModuleMember a module exports every function it defines.
    $ast = [Management.Automation.Language.Parser]::ParseFile($Path, [ref]$null, [ref]$null)
    $defined = @{}
    foreach ($function in $ast.FindAll({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] }, $false)) {
        $parameters = if ($function.Body.ParamBlock) { $function.Body.ParamBlock.Parameters } else { $function.Parameters }
        $defined[$function.Name] = [pscustomobject]@{ Names=(Get-ParameterNames $parameters); Mandatory=(Get-MandatoryParameterNames $parameters) }
    }
    $exports = @($ast.FindAll({ param($node) $node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -eq 'Export-ModuleMember' }, $true))
    if ($exports.Count -eq 0) { return $defined }
    $exported = @{}
    foreach ($command in $exports) {
        $elements = @($command.CommandElements)
        for ($index = 1; $index -lt $elements.Count - 1; $index++) {
            if ($elements[$index] -isnot [Management.Automation.Language.CommandParameterAst] -or $elements[$index].ParameterName -ne 'Function') { continue }
            foreach ($name in $elements[$index + 1].FindAll({ param($node) $node -is [Management.Automation.Language.StringConstantExpressionAst] }, $true)) {
                if ($defined.Contains($name.Value)) { $exported[$name.Value] = $defined[$name.Value] }
            }
        }
    }
    return $exported
}

function Test-DxUiConsumerInterface([Parameter(Mandatory)][string] $Root) {
    # What a consumer may call or import from its pinned checkout, as capabilities.json lists it: scripts with their parameters,
    # module functions with theirs, the MSBuild files and the public header root. Removing or renaming any of them breaks a
    # pinned consumer, so it fails here until capabilities.json changes too, under the revision rule of
    # Specs/Build/Build_ToolchainAndConsumption.md. Parameter and function names compare as PowerShell binds them, ignoring case.
    $failures = [Collections.Generic.List[string]]::new()
    $manifest = Read-JsonFile (Join-Path $Root 'capabilities.json') -StripBom
    $revision = Get-JsonValue $manifest 'apiRevision'
    if (-not ($revision -is [int] -or $revision -is [long]) -or $revision -lt 1) { $failures.Add('capabilities.json needs a positive integer apiRevision') }
    $interface = Get-JsonValue $manifest 'consumerInterface'
    if ($interface -isnot [Collections.IDictionary]) {
        $failures.Add('capabilities.json needs a consumerInterface object')
        return [pscustomobject]@{ Failures = $failures.ToArray(); Entries = 0; Revision = $revision }
    }
    $consumerEntries = 0
    $scripts = Get-JsonValue $interface 'scripts' @{}
    foreach ($relative in Sort-Ordinal @($scripts.Keys)) {
        $consumerEntries++
        $path = Resolve-ConsumerInterfacePath $Root $relative
        if ($null -eq $path -or -not [IO.File]::Exists($path)) { $failures.Add("Consumer script is missing: $relative"); continue }
        $details = Get-ScriptParameterNames $path
        $present = $details.Names
        foreach ($parameter in @($scripts[$relative])) {
            if ($parameter -notin $present) { $failures.Add("Consumer script $relative has no -$parameter parameter") }
        }
        $declaredMandatory = Get-ConsumerMandatoryParameters $interface 'scripts' $relative
        foreach ($parameter in $details.Mandatory) { if ($parameter -notin $declaredMandatory) { $failures.Add("Consumer script $relative added undeclared mandatory -$parameter parameter") } }
    }
    $modules = Get-JsonValue $interface 'modules' @{}
    foreach ($relative in Sort-Ordinal @($modules.Keys)) {
        $path = Resolve-ConsumerInterfacePath $Root $relative
        if ($null -eq $path -or -not [IO.File]::Exists($path)) { $consumerEntries++; $failures.Add("Consumer module is missing: $relative"); continue }
        $exported = Get-ModuleFunctionParameters $path
        foreach ($function in Sort-Ordinal @($modules[$relative].Keys)) {
            $consumerEntries++
            $match = @($exported.Keys | Where-Object { $_ -eq $function })
            if ($match.Count -eq 0) { $failures.Add("Consumer module $relative does not export $function"); continue }
            foreach ($parameter in @($modules[$relative][$function])) {
                if ($parameter -notin $exported[$match[0]].Names) { $failures.Add("Consumer function $function in $relative has no -$parameter parameter") }
            }
            $declaredMandatory = Get-ConsumerMandatoryParameters $interface 'modules' $relative $function
            foreach ($parameter in $exported[$match[0]].Mandatory) { if ($parameter -notin $declaredMandatory) { $failures.Add("Consumer function $function in $relative added undeclared mandatory -$parameter parameter") } }
        }
    }
    $mandatory = Get-JsonValue $interface 'mandatoryParameters' @{}
    $declaredScripts = Get-JsonValue $mandatory 'scripts' @{}
    foreach ($relative in Sort-Ordinal @($declaredScripts.Keys)) { if ($relative -notin $scripts.Keys) { $failures.Add("Mandatory-parameter contract names an undeclared consumer script: $relative") } }
    $declaredModules = Get-JsonValue $mandatory 'modules' @{}
    foreach ($relative in Sort-Ordinal @($declaredModules.Keys)) {
        if ($relative -notin $modules.Keys) { $failures.Add("Mandatory-parameter contract names an undeclared consumer module: $relative"); continue }
        foreach ($function in @($declaredModules[$relative].Keys)) { if ($function -notin $modules[$relative].Keys) { $failures.Add("Mandatory-parameter contract names an undeclared function $function in $relative") } }
    }
    $powershell = Get-JsonValue $interface 'powershell' @{}
    foreach ($kind in @('scripts','modules')) {
        $promises = Get-JsonValue $powershell $kind @{}
        $contractEntries = Get-JsonValue $interface $kind @{}
        foreach ($relative in Sort-Ordinal @($contractEntries.Keys)) {
            $versions = @($promises[$relative] | ForEach-Object { [string]$_ } | Sort-Object -Unique)
            if (-not $versions.Count -or @($versions | Where-Object { $_ -notin @('5.1','7') }).Count) {
                $failures.Add("Consumer $kind entry $relative must declare supported PowerShell versions (5.1 and/or 7).")
            }
        }
        foreach ($relative in Sort-Ordinal @($promises.Keys)) { if ($relative -notin $contractEntries.Keys) { $failures.Add("PowerShell contract names an undeclared consumer $kind entry: $relative") } }
    }
    $msbuild = Get-JsonValue $interface 'msbuild' @()
    $headers = Get-JsonValue $interface 'headers' @()
    foreach ($relative in @($msbuild) + @($headers)) {
        $consumerEntries++
        $path = Resolve-ConsumerInterfacePath $Root $relative
        if ($null -eq $path -or -not (Test-PathExists $path)) { $failures.Add("Consumer entry point is missing: $relative") }
    }
    return [pscustomobject]@{ Failures = $failures.ToArray(); Entries = $consumerEntries; Revision = $revision }
}

# --- Inherited test accounting ----------------------------------------------------------------------------------

function Test-TestDefinition([string] $Path, [string] $Name) {
    return [regex]::IsMatch((Read-TextFile $Path -StripBom), '\bvoid\s+' + [regex]::Escape($Name) + '\s*\(\s*\)')
}

function Test-SourcePolicyDispositions([string] $Root, [object[]] $Cases) {
    $origins = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($case in $Cases) {
        $reason = Get-JsonValue $case 'reason' ''
        if ((Test-SameValue $case['status'] 'excluded') -and $reason -is [string] -and $reason.StartsWith('Source-text', [StringComparison]::Ordinal)) {
            [void]$origins.Add("$($case['file'])`0$($case['test'])")
        }
    }
    if ($origins.Count -eq 0) { return }
    $manifest = Join-Path $Root 'Specs/Testing/SourcePolicyDispositions.json'
    if (-not [IO.File]::Exists($manifest)) { return 'Missing current source-policy dispositions' }
    $data = Read-JsonFile $manifest
    $errors = [Collections.Generic.List[string]]::new()
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    if (-not (Test-SameValue (Get-JsonValue $data 'schemaVersion') 1)) { $errors.Add('Unsupported source-policy disposition schema') }
    $controls = Get-FullPath (Join-Path $Root 'Tests/Controls')
    $dispositions = Get-JsonValue $data 'dispositions' @()
    foreach ($row in @($dispositions)) {
        $identity = "$(Get-JsonValue $row 'originFile')`0$(Get-JsonValue $row 'originTest')"
        $display = "('$(Get-JsonValue $row 'originFile')', '$(Get-JsonValue $row 'originTest')')"
        if (-not $origins.Contains($identity) -or $seen.Contains($identity)) { $errors.Add("Duplicate or unknown source-policy origin: $display") }
        [void]$seen.Add($identity)
        $decision = Get-JsonValue $row 'decision'
        $rationale = Get-JsonValue $row 'rationale' ''
        if ($decision -cnotin @('runtime', 'runtime-restored', 'retired-spelling', 'retired-diagnostics', 'reviewed-policy', 'product-owned') -or
            $decision -isnot [string] -or $rationale -isnot [string] -or -not $rationale.Trim()) {
            $errors.Add("Missing source-policy decision/rationale: $display")
        }
        $runtimeCases = Get-JsonValue $row 'runtimeCases' @()
        $replacements = @($runtimeCases)
        if ($decision -cin @('runtime', 'runtime-restored') -and $replacements.Count -eq 0) { $errors.Add("Runtime disposition has no replacement: $display") }
        foreach ($replacement in $replacements) {
            $path = Get-FullPath (Join-DataPath $Root ([string](Get-RequiredJsonValue $replacement 'file')))
            $name = [string](Get-RequiredJsonValue $replacement 'test')
            if (-not (Test-UnderPath $path $controls) -or -not [IO.File]::Exists($path)) { $errors.Add("Invalid source-policy replacement path: $path"); continue }
            if (-not (Test-TestDefinition $path $name)) { $errors.Add("Missing source-policy replacement case: $name") }
        }
    }
    if (-not $seen.SetEquals($origins)) { $errors.Add('Every historical source-policy exclusion requires exactly one current disposition') }
    return $errors.ToArray()
}

function Test-DxUiTestPort([Parameter(Mandatory)][string] $Root) {
    $Root = Get-FullPath $Root
    $data = Read-JsonFile (Join-Path $Root 'Specs/Done/SourceImport/test-port.json')
    $errors = [Collections.Generic.List[string]]::new()
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $tests = Get-RequiredJsonValue $data 'tests'
    $cases = @($tests)
    if (-not (Test-SameValue (Get-JsonValue $data 'schemaVersion') 1) -or -not (Test-SameValue (Get-JsonValue $data 'originCaseCount') $cases.Count)) {
        $errors.Add('Every inherited case requires a disposition; origin count differs')
    }
    if (-not (Test-FullMatch (Get-JsonValue $data 'originCommit' '') '[0-9a-f]{40}')) { $errors.Add('Historical test origin must be an exact commit') }
    $controls = Get-FullPath (Join-Path $Root 'Tests/Controls')
    foreach ($case in $cases) {
        $file = [string](Get-RequiredJsonValue $case 'file')
        $test = [string](Get-RequiredJsonValue $case 'test')
        $identity = "('$file', '$test')"
        if ($seen.Contains("$file`0$test")) { $errors.Add("Duplicate test origin: $identity") }
        [void]$seen.Add("$file`0$test")
        # The origin identity stays immutable when an owned source is renamed.
        $currentFile = [string](Get-JsonValue $case 'currentFile' $file)
        $parts = @($currentFile.Split('/') | Where-Object { $_ -and $_ -ne '.' })
        $path = Get-FullPath (Join-DataPath $Root $currentFile)
        if ($currentFile.StartsWith('/') -or $parts -contains '..' -or -not (Test-UnderPath $path $controls) -or -not [IO.File]::Exists($path)) {
            $errors.Add("Invalid owned test source: $(($parts) -join '/')")
            continue
        }
        $status = Get-RequiredJsonValue $case 'status'
        if ($status -cnotin @('ported', 'excluded') -or $status -isnot [string] -or -not (Test-PyTruthy (Get-JsonValue $case 'reason'))) {
            $errors.Add("Test needs status and rationale: $identity")
        }
        if (Test-SameValue $status 'ported') {
            $name = [string](Get-JsonValue $case 'currentTest' $test)
            if (-not (Test-TestDefinition $path $name)) { $errors.Add("Retained case is missing: $identity -> $name") }
        }
    }
    $errors.AddRange([string[]]@(Test-SourcePolicyDispositions $Root $cases))
    $retained = @($cases | Where-Object { Test-SameValue $_['status'] 'ported' }).Count
    return New-ValidationResult $errors @("Accounted for $($cases.Count) inherited cases: $retained retained, $($cases.Count - $retained) excluded with reasons")
}

# --- Six-configuration build matrix ------------------------------------------------------------------------------

$script:Matrix = foreach ($configuration in @('Debug', 'Release', 'ASan Debug')) { foreach ($platform in @('x64', 'ARM64')) { "$configuration|$platform" } }

function Get-BuildMatrix { return Sort-Ordinal $script:Matrix }

function Test-DxUiProjectConfigurations([Parameter(Mandatory)][string] $Path) {
    $document = [Xml.XmlDocument]::new()
    $document.Load((Get-FullPath $Path))
    $configurations = @($document.SelectNodes('//*') | Where-Object { $_.LocalName -ceq 'ProjectConfiguration' } | ForEach-Object { $_.GetAttribute('Include') })
    $present = [Collections.Generic.HashSet[string]]::new([string[]]$configurations, [StringComparer]::Ordinal)
    $errors = [Collections.Generic.List[string]]::new()
    foreach ($name in Get-BuildMatrix) { if (-not $present.Contains($name)) { $errors.Add("${Path}: missing $name") } }
    if ($configurations.Count -ne $present.Count) { $errors.Add("${Path}: duplicate project configuration") }
    return $errors.ToArray()
}

function Test-DxUiSolutionConfigurations([Parameter(Mandatory)][string] $Path) {
    $content = Read-TextFile (Get-FullPath $Path) -StripBom
    $projects = [regex]::Matches($content, '(?m)^Project\("\{[^}]+\}"\) = "[^"]+", "([^"]+\.vcxproj)", "(\{[^}]+\})"')
    $section = [regex]::Match($content, '(?s)GlobalSection\(SolutionConfigurationPlatforms\) = preSolution(.*?)EndGlobalSection')
    $declared = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    if ($section.Success) { foreach ($line in [regex]::Matches($section.Groups[1].Value, '(?m)^\s*(.+?)\s*=.*$')) { [void]$declared.Add($line.Groups[1].Value) } }
    $errors = [Collections.Generic.List[string]]::new()
    foreach ($name in Get-BuildMatrix) { if (-not $declared.Contains($name)) { $errors.Add("${Path}: missing solution configuration $name") } }
    foreach ($project in $projects) {
        $projectName = $project.Groups[1].Value
        $guid = $project.Groups[2].Value
        foreach ($configuration in Get-BuildMatrix) {
            foreach ($kind in @('ActiveCfg', 'Build.0')) {
                $key = [regex]::Escape("$guid.$configuration.$kind")
                $values = @([regex]::Matches($content, '(?m)^\s*' + $key + '\s*=\s*(.*?)\s*$') | ForEach-Object { $_.Groups[1].Value })
                if ($values.Count -ne 1 -or $values[0] -cne $configuration) {
                    $errors.Add("${Path}: $projectName $configuration.$kind maps to $(if ($values.Count) { "['" + ($values -join "', '") + "']" } else { '[]' }), expected itself")
                }
            }
        }
    }
    return $errors.ToArray()
}

function Test-DxUiBuildMatrix([Parameter(Mandatory)][string] $Root) {
    if (-not [IO.Path]::IsPathRooted($Root)) { $Root = [IO.Path]::Combine((Get-Location).ProviderPath, $Root) }
    $Root = [IO.Path]::GetFullPath($Root)
    $Root = Get-FullPath $Root
    $listed = @(Get-GitPathList $Root @('ls-files', '--cached', '--others', '--exclude-standard', '-z', '--', '*.vcxproj', '*.sln'))
    $live = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($path in $listed) { if (-not $path.StartsWith('Specs/Plans/Done/', [StringComparison]::Ordinal)) { [void]$live.Add($path) } }
    $paths = @(Sort-Ordinal @($live))
    $errors = [Collections.Generic.List[string]]::new()
    foreach ($relative in $paths) {
        $path = Join-Path $Root $relative
        $found = @(if ($relative.EndsWith('.vcxproj', [StringComparison]::Ordinal)) { Test-DxUiProjectConfigurations $path } else { Test-DxUiSolutionConfigurations $path })
        $errors.AddRange([string[]]$found)
    }
    if ($paths.Count -eq 0) { $errors.Add('No live native projects found.') }
    return New-ValidationResult $errors @("$($paths.Count) native project/solution files; $($errors.Count) six-configuration mapping errors. Build definitions do not establish instrumentation or native runtime qualification.")
}

# --- Entry points ---------------------------------------------------------------------------------------------------

function Complete-DxUiValidation([Parameter(Mandatory)][pscustomobject] $Result, [Parameter(Mandatory)][string] $Name) {
    # Prints the findings and the summary; a finding fails the calling validate-*.ps1 script.
    foreach ($failure in $Result.Failures) { [Console]::Error.WriteLine($failure) }
    foreach ($message in $Result.Messages) { Write-Host $message }
    if ($Result.Failures.Count) { throw "$Name validation failed with $($Result.Failures.Count) finding(s)." }
}

function Get-DxUiValidationSteps {
    # What validate.ps1 runs, relative to the repository root: every validator, then the tooling tests.
    return @('validate-skills.ps1', 'validate-specs.ps1', 'validate-dependencies.ps1', 'validate-test-port.ps1',
        'validate-build-matrix.ps1', 'Tools/tests/Invoke-ToolingTests.ps1')
}

function Invoke-DxUiValidation {
    <# Runs every step in order, whether or not an earlier one failed, and returns one result per step (Step, Passed, Error).
       A step fails by throwing or by exiting with a nonzero code; one that cannot be started fails like any other. Each
       runs in its own PowerShell process. The validators and tests import this module and each other's with -Force, and a
       script run from a module function shares that module's session, which the import would tear down under the runner. #>
    param([Parameter(Mandatory)][string] $Root, [string[]] $Steps = @(Get-DxUiValidationSteps))
    $results = [Collections.Generic.List[object]]::new()
    foreach ($step in $Steps) {
        Write-Host "== $step"
        $path = Join-Path $Root $step
        $failure = $null
        if (-not [IO.File]::Exists($path)) { $failure = "$step does not exist" }
        else {
            # A step's output belongs on the console, not in this function's result.
            & ([Environment]::ProcessPath) -NoProfile -NonInteractive -File $path | Out-Host
            if ($LASTEXITCODE -ne 0) { $failure = "$step exited with code $LASTEXITCODE" }
        }
        if ($failure) { Write-Host "FAILED: $failure" }
        $results.Add([pscustomobject]@{ Step = $step; Passed = -not $failure; Error = $failure })
    }
    return $results.ToArray()
}

Export-ModuleMember -Function Get-MarkdownProse, Test-DxUiDocs, Test-DxUiDesignSystem, Test-DxUiMeasurements, Test-DxUiSpecs, Test-DxUiChangelog,
    ConvertFrom-SkillFrontMatter, Test-DxUiSkills, Test-DxUiDependencies, Test-DxUiTestPort, Get-BuildMatrix,
    Test-DxUiProjectConfigurations, Test-DxUiSolutionConfigurations, Test-DxUiBuildMatrix, Complete-DxUiValidation,
    Get-DxUiValidationSteps, Invoke-DxUiValidation
