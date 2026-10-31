<#
.SYNOPSIS Commits a regenerated docs/gallery on the checked-out branch and pushes it, unless nothing changed.
.DESCRIPTION
gallery.ps1 -PublishDocs rewrites docs/gallery. Its generation.json records the commit the sheets were generated from, so it
differs after every commit even when every sheet is identical; the gallery counts as changed only when a sheet, the HTML
index or the README differs, and a lone new generation.json is left uncommitted (the sheets, and so the gallery, are
current). Only docs/gallery is staged, so nothing else a build touched is committed. The push is an ordinary one, never
forced: a branch that moved since the checkout fails the run, which is then repeated. This remains an explicit local
compatibility command; DxUi has no hosted gallery publication workflow. Use -NoPush to commit without pushing.
.PARAMETER Root Repository whose docs/gallery is committed; defaults to this one.
.PARAMETER NoPush Commit and stop, without pushing.
.PARAMETER PullRequest Optional same-repository PR number. After a token push, verify its new head and explicitly dispatch fresh compiler-free checks.
#>
[CmdletBinding()]
param([string] $Root = (Split-Path $PSScriptRoot -Parent), [switch] $NoPush, [ValidateRange(0,2147483647)][int]$PullRequest=0)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$branch = & git -C $Root symbolic-ref --quiet --short HEAD
if ($LASTEXITCODE -ne 0 -or -not $branch) { throw 'Gallery publication requires a named branch.' }
if ($branch -ceq 'main') { throw 'Gallery publication must use a review branch, not main.' }
if($PullRequest){
    $pr=& gh api "repos/$env:GITHUB_REPOSITORY/pulls/$PullRequest" | ConvertFrom-Json
    $head=(& git -C $Root rev-parse HEAD).Trim()
    if($LASTEXITCODE -ne 0 -or $pr.state -cne 'open' -or $pr.head.repo.full_name -cne $env:GITHUB_REPOSITORY -or
        $pr.head.sha -cne $head -or $pr.head.ref -cne $branch -or $pr.base.ref -cne 'main'){throw 'The open PR branch identity changed before gallery publication.'}
}
# A hosted run commits as the Actions bot; anywhere else the developer's own identity is used, and no configuration changes.
$identity = if ($env:GITHUB_ACTIONS -eq 'true') { @('-c', 'user.name=github-actions[bot]', '-c', 'user.email=41898282+github-actions[bot]@users.noreply.github.com') } else { @() }
& git -C $Root add -- docs/gallery
if ($LASTEXITCODE -ne 0) { throw 'Cannot stage docs/gallery.' }
$staged=@(& git -C $Root -c core.quotepath=false diff --cached --name-only)
if($LASTEXITCODE -ne 0 -or @($staged|Where-Object {-not $_.StartsWith('docs/gallery/',[StringComparison]::Ordinal)}).Count){
    throw 'Gallery publication refuses unrelated staged paths.'
}
& git -C $Root diff --cached --quiet -- docs/gallery ':(exclude)docs/gallery/generation.json'
if ($LASTEXITCODE -eq 0) { Write-Host 'The published gallery is already current; nothing to commit.'; exit 0 }
if ($LASTEXITCODE -ne 1) { throw 'Cannot inspect gallery changes.' }
& git -C $Root @identity commit -q -m 'docs: regenerate the control gallery'
if ($LASTEXITCODE -ne 0) { throw 'Gallery commit failed.' }
if ($NoPush) { Write-Host 'Committed the regenerated gallery; -NoPush was given, so nothing was pushed.'; exit 0 }
& git -C $Root push origin HEAD
if ($LASTEXITCODE -ne 0) { throw 'Gallery push failed; no force push was attempted.' }
if($PullRequest){
    $newHead=(& git -C $Root rev-parse HEAD).Trim()
    if($LASTEXITCODE -ne 0){throw 'Cannot identify the gallery commit.'}
    $published=& gh api "repos/$env:GITHUB_REPOSITORY/pulls/$PullRequest" | ConvertFrom-Json
    if($LASTEXITCODE -ne 0 -or $published.head.sha -cne $newHead){throw 'Published gallery PR head differs.'}
    & gh workflow run ci.yml --repo $env:GITHUB_REPOSITORY --ref main -f "pr_number=$PullRequest" -f "expected_head=$newHead"
    if($LASTEXITCODE -ne 0){throw 'Gallery pushed but fresh PR check dispatch failed; request it explicitly before merging.'}
}
Write-Host 'Committed and pushed the regenerated gallery.'
exit 0
