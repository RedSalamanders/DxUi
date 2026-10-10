# Which runs of the validation workflow (ci.yml) need its six native jobs. A push to main and a manual run always do:
# consumers adopt the main commit whose latest push run succeeded (Tools/ConsumerUpdate.psm1), so a main run is never cut
# short. A pull request does unless every path it changes is documentation, which the validation job checks and no native
# job builds, runs or reads. Anything else, a path these rules do not name included, needs them: a wrong skip would pass
# untested code, a wrong run only costs time. The pull request's commits and paths come from BenchmarkGate.psm1, as the
# paired benchmark's do.
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'BenchmarkGate.psm1')

function Get-DocumentationScopeRules {
    <# The documentation of the repository: a reason for the summary and the paths that carry it. A path is a file or a
       directory (it covers everything beneath it), and Markdown is documentation wherever it is. Nothing here is compiled,
       run or read by a native job: the specifications, design-system previews, plans, changelog fragments, retained
       measurements (their archived harness copies included), usage documentation and the committed gallery, which the x64
       Release job regenerates and never compares. The validation job checks all of them. #>
    return @(
        [ordered]@{ Reason = 'specification, design system or plan'; Paths = @('Specs') }
        [ordered]@{ Reason = 'changelog fragment'; Paths = @('Changes') }
        [ordered]@{ Reason = 'retained measurement'; Paths = @('Measurements') }
        [ordered]@{ Reason = 'usage documentation or gallery'; Paths = @('docs') }
        [ordered]@{ Reason = 'agent skill'; Paths = @('.agents') }
        [ordered]@{ Reason = 'license'; Paths = @('LICENSE') }
        [ordered]@{ Reason = 'another workflow'; Paths = @('.github/workflows/format.yml', '.github/workflows/gallery.yml') }
    )
}

function Get-DocumentationReason {
    <# Why a repository path is documentation, or $null when it is not. #>
    param([Parameter(Mandatory)][string] $Path)
    if ($Path.EndsWith('.md', [StringComparison]::Ordinal)) { return 'Markdown' }
    foreach ($rule in @(Get-DocumentationScopeRules)) {
        foreach ($root in $rule['Paths']) {
            if ($Path -ceq $root -or $Path.StartsWith("$root/", [StringComparison]::Ordinal)) { return $rule['Reason'] }
        }
    }
    return $null
}

function Get-NativeScope {
    <# Whether a pull request's changed paths need the native jobs, and the paths that say so. Only a change of at least one
       path, every one of them documentation, leaves them out: a change whose paths could not be read is not known to be
       documentation. #>
    param([Parameter(Mandatory)][AllowEmptyCollection()][AllowEmptyString()][AllowNull()][string[]] $ChangedPaths)
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $native = [Collections.Generic.List[string]]::new()
    foreach ($changed in @($ChangedPaths | Where-Object { $_ })) {
        $path = Format-RepositoryPath $changed
        if (-not $seen.Add($path)) { continue }
        if ($null -eq (Get-DocumentationReason $path)) { $native.Add($path) }
    }
    return [ordered]@{ Native = ($native.Count -gt 0 -or $seen.Count -eq 0); Total = $seen.Count; Documentation = $seen.Count - $native.Count; NativePaths = $native.ToArray() }
}

Export-ModuleMember -Function Get-DocumentationScopeRules, Get-DocumentationReason, Get-NativeScope
