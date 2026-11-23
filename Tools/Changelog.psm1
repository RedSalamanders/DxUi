# Compatibility import path for DxUi changelog validators. The implementation lives in the pinned CI engine.
Set-StrictMode -Version Latest
# Engine shim (Specs/ProjectCommands.md in the engine); the same lines in every shim.
# The product root holds .ci/project.json: this folder or one up to three levels above, never past a repository root.
$project = $PSScriptRoot
foreach ($level in 0..3) {
    if ([IO.File]::Exists((Join-Path $project '.ci/project.json'))) { break }
    if ($level -eq 3 -or (Test-Path -LiteralPath (Join-Path $project '.git')) -or -not (Split-Path $project -Parent)) { $project = $PSScriptRoot; break }
    $project = Split-Path $project -Parent
}
$binding = Join-Path $project '.ci-engine/Tools/Modules/Ci.EngineBinding.psm1'
if (-not [IO.File]::Exists($binding)) {
    throw "$([IO.Path]::GetFileName($PSCommandPath)) runs the shared CI engine that .ci/project.json pins from .ci-engine, and $project/.ci-engine is missing or predates engine shims. Check out the pinned commit (engine.ref in .ci/project.json): git -C .ci-engine fetch origin <engine.ref>; git -C .ci-engine checkout --detach <engine.ref> (first time: git clone https://github.com/RedSalamanders/.github.git .ci-engine)."
}
Import-Module $binding -Force
Import-Module (Resolve-CiPinnedEngineModule -ProjectRoot $project -Name ([IO.Path]::GetFileName($PSCommandPath))) -Force -Scope Local -ErrorAction Stop
Export-ModuleMember -Function Get-ChangelogFragmentProblems, Get-ChangelogFragments, Get-ChangelogProblems, Get-ChangelogFoldProblems, Merge-ChangelogFragments, Invoke-ChangelogFold
