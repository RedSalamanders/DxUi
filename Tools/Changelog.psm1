# Compatibility import path for existing validator, benchmark and operator callers.
# Implementation is shared with the other four repositories under Tools/Modules/Documentation.
Import-Module (Join-Path $PSScriptRoot 'Modules/Documentation/Changelog.psm1') -Scope Local
Export-ModuleMember -Function Get-ChangelogFragmentProblems, Get-ChangelogFragments, Get-ChangelogProblems, Merge-ChangelogFragments, Invoke-ChangelogFold
