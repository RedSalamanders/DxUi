# Compatibility import path for existing validator, benchmark and operator callers.
# Implementation is shared with the other five repositories under Tools/Modules/Documentation.
Import-Module (Join-Path $PSScriptRoot 'Modules/Documentation/Changelog.psm1') -Scope Local
Export-ModuleMember -Function Get-ChangelogFragmentProblems, Get-ChangelogFragments, Get-ChangelogProblems, Get-ChangelogFoldProblems, Merge-ChangelogFragments, Invoke-ChangelogFold
