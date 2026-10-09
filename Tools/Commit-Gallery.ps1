<#
.SYNOPSIS Commits a regenerated docs/gallery on the checked-out branch and pushes it, unless nothing changed.
.DESCRIPTION
gallery.ps1 -PublishDocs rewrites docs/gallery. Its generation.json records the commit the sheets were generated from, so it
differs after every commit even when every sheet is identical; the gallery counts as changed only when a sheet, the HTML
index or the README differs, and a lone new generation.json is left uncommitted (the sheets, and so the gallery, are
current). Only docs/gallery is staged, so nothing else a build touched is committed. The push is an ordinary one, never
forced: a branch that moved since the checkout fails the run, which is then repeated. The manual Gallery workflow runs this
after regenerating the gallery natively; run it locally with -NoPush to see what a publish would commit.
.PARAMETER Root Repository whose docs/gallery is committed; defaults to this one.
.PARAMETER NoPush Commit and stop, without pushing.
#>
[CmdletBinding()]
param([string] $Root = (Split-Path $PSScriptRoot -Parent), [switch] $NoPush)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$branch = & git -C $Root symbolic-ref --quiet --short HEAD
if ($LASTEXITCODE -ne 0 -or -not $branch) { throw 'Gallery publication requires a named branch.' }
if ($branch -ceq 'main') { throw 'Gallery publication must use a review branch, not main.' }
# A hosted run commits as the Actions bot; anywhere else the developer's own identity is used, and no configuration changes.
$identity = if ($env:GITHUB_ACTIONS -eq 'true') { @('-c', 'user.name=github-actions[bot]', '-c', 'user.email=41898282+github-actions[bot]@users.noreply.github.com') } else { @() }
& git -C $Root add -- docs/gallery
if ($LASTEXITCODE -ne 0) { throw 'Cannot stage docs/gallery.' }
& git -C $Root diff --cached --quiet -- docs/gallery ':(exclude)docs/gallery/generation.json'
if ($LASTEXITCODE -eq 0) { Write-Host 'The published gallery is already current; nothing to commit.'; exit 0 }
if ($LASTEXITCODE -ne 1) { throw 'Cannot inspect gallery changes.' }
& git -C $Root @identity commit -q -m 'docs: regenerate the control gallery'
if ($LASTEXITCODE -ne 0) { throw 'Gallery commit failed.' }
if ($NoPush) { Write-Host 'Committed the regenerated gallery; -NoPush was given, so nothing was pushed.'; exit 0 }
& git -C $Root push origin HEAD
if ($LASTEXITCODE -ne 0) { throw 'Gallery push failed; no force push was attempted.' }
Write-Host 'Committed and pushed the regenerated gallery.'
exit 0
