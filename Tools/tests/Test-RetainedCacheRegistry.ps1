<# .SYNOPSIS Retained-cache registry coverage (Testing_Validation.md, "Incremental-versus-fresh oracle"): every cache-named
   member declared in include/DxUi or src (an underscore member whose name contains "cache" or "cached") is named in
   src/Controls/DxUi.RetainedCaches.cpp, as a registered row or an exemption with its reason, so a new cache cannot skip the
   drop hook silently. Seeded fixtures fail for an unlisted member and pass once it is listed. #>
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

# A declaration: optional qualifiers and a type, then the member name, then an initializer or the end of the statement.
$declaration = '^\s*(?:mutable\s+|static\s+|inline\s+|constexpr\s+)*[\w:<>,\s\*&]+?[\s\*&](?<name>_\w*[Cc]ache\w*)\s*(?:\{[^;]*\}|=[^;]*)?;'

function Get-UnregisteredCacheMembers([string] $Root) {
    $registryPath = Join-Path $Root 'src/Controls/DxUi.RetainedCaches.cpp'
    if (-not (Test-Path -LiteralPath $registryPath)) { throw "Missing retained-cache registry: $registryPath" }
    $registry = Get-Content -LiteralPath $registryPath -Raw
    $sources = @(Get-ChildItem -LiteralPath (Join-Path $Root 'include/DxUi') -Filter '*.h' -File -Recurse) +
        @(Get-ChildItem -LiteralPath (Join-Path $Root 'src') -Include '*.h', '*.cpp' -File -Recurse)
    $missing = [Collections.Generic.List[string]]::new()
    foreach ($source in $sources) {
        $line = 0
        foreach ($text in [IO.File]::ReadAllLines($source.FullName)) {
            ++$line
            $match = [regex]::Match($text, $declaration)
            if (-not $match.Success) { continue }
            $name = $match.Groups['name'].Value
            if (-not [regex]::IsMatch($registry, "\b$([regex]::Escape($name))\b")) {
                $missing.Add(('{0}:{1}: {2}' -f [IO.Path]::GetRelativePath($Root, $source.FullName).Replace('\', '/'), $line, $name))
            }
        }
    }
    return $missing.ToArray()
}

Invoke-TestCase 'every cache-named member at HEAD is registered or exempted' {
    $missing = @(Get-UnregisteredCacheMembers $repository)
    foreach ($entry in $missing) { Write-Host "     not in DxUi.RetainedCaches.cpp: $entry" }
    Assert-Equal 0 $missing.Count 'unregistered cache members'
}

Invoke-FixtureCase 'an unlisted cache member fails and a listed one passes' {
    param($root)
    Set-FixtureFile $root 'include/DxUi/DxUi.h' "class Widget`n{`n    mutable std::vector<int> _widthCache;`n    int _width = 0;`n};`n"
    Set-FixtureFile $root 'src/Controls/Widget.cpp' "void Widget::Paint() { _widthCache.clear(); }`n"
    Set-FixtureFile $root 'src/Controls/DxUi.RetainedCaches.cpp' "// rows`n"
    $missing = @(Get-UnregisteredCacheMembers $root)
    Assert-Equal 1 $missing.Count 'one unlisted cache member'
    Assert-True ($missing[0] -like 'include/DxUi/DxUi.h:3: _widthCache') "the report names the declaration: $($missing[0])"
    Set-FixtureFile $root 'src/Controls/DxUi.RetainedCaches.cpp' "// Widget::_widthCache row`n"
    Assert-Equal 0 @(Get-UnregisteredCacheMembers $root).Count 'a listed member passes'
}

Invoke-FixtureCase 'a missing registry fails closed' {
    param($root)
    Set-FixtureFile $root 'include/DxUi/DxUi.h' "int x;`n"
    Set-FixtureFile $root 'src/Controls/Other.cpp' "int y;`n"
    Assert-Throws { Get-UnregisteredCacheMembers $root } 'no registry'
}

Complete-TestRun 'RetainedCacheRegistry'
