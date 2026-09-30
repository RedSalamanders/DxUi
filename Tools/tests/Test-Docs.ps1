# Catalog usage coverage, gallery integrity, fenced-code link parsing, measurement ownership and the design system.
[CmdletBinding()] param()
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../Validation.psm1') -Force

function Set-DocsFixture([string] $Root) {
    Set-FixtureFile $Root 'README.md' '[Docs](docs/README.md)'
    Set-FixtureFile $Root 'include/DxUi/ControlCatalog.h' 'enum class ControlKind { Label, Button };'
    Set-FixtureFile $Root 'docs/controls.md' "| Label | Caption |`n| Button | Action |`n"
    $image = [byte[]](0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A) + [Text.Encoding]::ASCII.GetBytes('fixture')
    $entries = foreach ($index in 0..5) {
        Set-FixtureBytes $Root "docs/gallery/$index.png" $image
        [ordered]@{ file = "$index.png"; sha256 = Get-Sha256Hex $image }
    }
    Set-FixtureJson $Root 'docs/gallery/generation.json' @{ controlCount = 2; images = @($entries) }
}

Invoke-FixtureCase 'complete docs pass' {
    param($root)
    Set-DocsFixture $root
    Assert-Equal 0 (Test-DxUiDocs $root).Failures.Count 'failures'
}

Invoke-FixtureCase 'missing control usage fails' {
    param($root)
    Set-DocsFixture $root
    Set-FixtureFile $root 'docs/controls.md' "| Label | Caption |`n"
    Assert-Contains (Test-DxUiDocs $root).Failures 'Missing control usage: Button' 'usage'
}

Invoke-FixtureCase 'changed gallery bytes fail' {
    param($root)
    Set-DocsFixture $root
    Set-FixtureFile $root 'docs/gallery/0.png' 'invalid image'
    Assert-True (Test-DxUiDocs $root).Failures.Count 'changed bytes fail'
}

Invoke-TestCase 'lambdas in fenced code are not links' {
    $result = Get-MarkdownProse "``````cpp`nx.SetOnClick([](bool checked) {});`n```````n[Docs](docs/README.md)"
    Assert-True (-not $result.Contains('checked')) 'the fenced lambda is removed'
    Assert-True $result.Contains('[Docs]') 'the prose link remains'
}

Invoke-FixtureCase 'raw measurements cannot return to docs' {
    param($root)
    Set-FixtureFile $root 'docs/measurements/application/run.json' '{}'
    Assert-True (Test-DxUiMeasurements $root).Failures.Count 'raw docs measurements fail'
}

Invoke-FixtureCase 'measurements require library ownership and an explanation' {
    param($root)
    $receipt = [ordered]@{ workloadOwner = 'DxUi'; fixture = 'dxui-complex-ui-v2'; benchmarkInputs = @{ 'scene.h' = 'a' * 64 } }
    Set-FixtureJson $root 'Measurements/example/run.json' $receipt
    Assert-True (Test-DxUiMeasurements $root).Failures.Count 'an unexplained receipt fails'
    Set-FixtureFile $root 'Measurements/example/README.md' 'Independent synthetic scene; offscreen only.'
    Assert-Equal 0 (Test-DxUiMeasurements $root).Failures.Count 'failures'
    $receipt.workloadOwner = 'Application'
    Set-FixtureJson $root 'Measurements/example/run.json' $receipt
    Assert-True (Test-DxUiMeasurements $root).Failures.Count 'an application receipt fails'
    $receipt.workloadOwner = 'dxui'
    Set-FixtureJson $root 'Measurements/example/run.json' $receipt
    Assert-True (Test-DxUiMeasurements $root).Failures.Count 'ownership compares case-sensitively'
}

function Set-DesignSystemFixture([string] $Root) {
    Set-FixtureFile $Root 'include/DxUi/ControlCatalog.h' 'enum class ControlKind { Label, Button };'
    Set-FixtureJson $Root 'Specs/DesignSystem/design-system.json' @{ v = 3; layout = 'files'; title = 'DxUi' }
    Set-FixtureJson $Root 'Specs/DesignSystem/tokens.json' @{ color = @{ themes = @(@{ id = 'light' }); tokens = @(@{ name = 'text' }) } }
    Set-FixtureFile $Root 'Specs/DesignSystem/README.md' 'Usage rules.'
    Set-FixtureFile $Root 'Specs/DesignSystem/components/Cover/preview.html' '<!-- @dsCard height=288 -->'
    foreach ($name in @('Label', 'Button')) {
        Set-FixtureFile $Root "Specs/DesignSystem/components/$name/README.md" "$name guidelines."
        Set-FixtureFile $Root "Specs/DesignSystem/components/$name/preview.html" "<!-- @dsCard group=`"Text`" height=80 -->`n<!doctype html>"
    }
}

Invoke-FixtureCase 'a complete design system passes' {
    param($root)
    Set-DesignSystemFixture $root
    Assert-Equal 0 (Test-DxUiDesignSystem $root).Failures.Count 'failures'
}

Invoke-FixtureCase 'a new control requires a design-system preview' {
    param($root)
    Set-DesignSystemFixture $root
    Set-FixtureFile $root 'include/DxUi/ControlCatalog.h' 'enum class ControlKind { Label, Button, Slider };'
    Assert-Contains (Test-DxUiDesignSystem $root).Failures 'Missing design-system component: Slider' 'missing component'
}

Invoke-FixtureCase 'a preview requires its card marker' {
    param($root)
    Set-DesignSystemFixture $root
    Set-FixtureFile $root 'Specs/DesignSystem/components/Button/preview.html' '<!doctype html>'
    Assert-Contains (Test-DxUiDesignSystem $root).Failures 'Design-system preview lacks its card marker: Button' 'marker'
}

Invoke-FixtureCase 'a removed control leaves no stale component' {
    param($root)
    Set-DesignSystemFixture $root
    Set-FixtureFile $root 'include/DxUi/ControlCatalog.h' 'enum class ControlKind { Label };'
    Assert-Contains (Test-DxUiDesignSystem $root).Failures 'Design-system component is not in the catalog: Button' 'stale component'
}

function Set-SpecsFixture([string] $Root) {
    # The smallest tree Test-DxUiSpecs accepts: its authority files, an empty plan index, the docs and the design system.
    $authority = @('AGENTS.md', 'Specs/README.md', 'Specs/Core/Core_Architecture.md', 'Specs/Core/Core_PerformanceAndResources.md',
        'Specs/Build/Build_ToolchainAndConsumption.md', 'Specs/UI/UI_ControlsAndLayout.md', 'Specs/UI/UI_ThemeAndTypography.md',
        'Specs/UI/UI_InputAndAccessibility.md', 'Specs/Rendering/Rendering_EmbeddedD3D11.md', 'Specs/Rendering/Rendering_Win32Host.md',
        'Specs/Testing/Testing_Validation.md', 'Specs/Core/Core_Documentation.md', 'Specs/UI/UI_DesignSystem.md', 'Specs/DesignSystem/README.md',
        'docs/README.md', 'docs/controls.md', 'docs/getting-started.md', 'docs/hosting.md', 'docs/performance.md', 'docs/gallery/README.md',
        'docs/samples.md', 'Measurements/README.md')
    foreach ($name in $authority) { Set-FixtureFile $Root $name "# $name`n" }
    Set-FixtureFile $Root 'Specs/Plans/WIP/README.md' "# Active plans`n"
    Set-DocsFixture $Root
    Set-DesignSystemFixture $Root
}

function Get-MarkdownCount([object] $Result) {
    return [int][regex]::Match($Result.Messages[0], 'Validated (\d+) Markdown files').Groups[1].Value
}

Invoke-FixtureCase 'a complete specification tree passes' {
    param($root)
    Set-SpecsFixture $root
    $result = Test-DxUiSpecs $root
    Assert-Equal 0 $result.Failures.Count "failures: $($result.Failures -join '; ')"
    Assert-True ((Get-MarkdownCount $result) -gt 20) 'its Markdown files are counted'
}

Invoke-FixtureCase 'nested git checkouts are not part of the tree' {
    param($root)
    Set-SpecsFixture $root
    $before = Get-MarkdownCount (Test-DxUiSpecs $root)
    # A linked worktree (its .git is a file), as agents keep under .claude/worktrees, and a nested clone (a .git directory),
    # each holding Markdown with broken links, as a half-edited copy would.
    foreach ($name in @('.claude/worktrees/agent-a', 'vendor/clone')) {
        Set-FixtureFile $root "$name/README.md" "[broken](missing.md)`n"
        Set-FixtureFile $root "$name/docs/deep.md" "[also broken](nothing.md)`n"
    }
    Set-FixtureFile $root '.claude/worktrees/agent-a/.git' "gitdir: elsewhere`n"
    New-Item -ItemType Directory -Path (Join-Path $root 'vendor/clone/.git') | Out-Null
    $result = Test-DxUiSpecs $root
    Assert-Equal 0 $result.Failures.Count "failures: $($result.Failures -join '; ')"
    Assert-Equal $before (Get-MarkdownCount $result) 'the Markdown count excludes both nested checkouts'
    # The same Markdown in an ordinary folder is validated, so the checkouts were skipped by rule, not overlooked.
    Set-FixtureFile $root 'notes/README.md' "[broken](missing.md)`n"
    $control = Test-DxUiSpecs $root
    Assert-Equal ($before + 1) (Get-MarkdownCount $control) 'an ordinary folder is counted'
    Assert-Equal 1 @($control.Failures | Where-Object { $_ -like '*README.md: broken reference missing.md' }).Count 'and its broken link is found'
    # The tree being validated may itself be a checkout: its own .git entry never hides it.
    Set-FixtureFile $root '.git' "gitdir: elsewhere`n"
    Assert-Equal ($before + 1) (Get-MarkdownCount (Test-DxUiSpecs $root)) 'the root is walked even with a .git entry'
}

Invoke-FixtureCase 'measurement receipts of a nested checkout are not this tree''s' {
    param($root)
    Set-FixtureJson $root 'Measurements/other/run.json' @{ workloadOwner = 'Application'; fixture = 'app-scene' }
    Assert-True (Test-DxUiMeasurements $root).Failures.Count 'an application receipt in the tree fails'
    Set-FixtureFile $root 'Measurements/other/.git' "gitdir: elsewhere`n"
    Assert-Equal 0 (Test-DxUiMeasurements $root).Failures.Count 'the same receipt in a nested checkout is not this tree''s'
}

# --- Publishing docs/gallery from a manual workflow ------------------------------------------------------------------------

$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$commitGallery = Join-Path $repository 'Tools/Commit-Gallery.ps1'

function Invoke-FixtureGit([string] $Root, [string[]] $Arguments) {
    & git -C $Root -c core.autocrlf=false @Arguments 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "git $($Arguments -join ' ') failed in $Root" }
}

function Get-FixtureGit([string] $Root, [string[]] $Arguments) {
    $output = & git -C $Root @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "git $($Arguments -join ' ') failed in $Root" }
    # The comma keeps a one-line answer an array: a bare scalar string would be indexed by character.
    return , @($output | ForEach-Object { "$_" })
}

# Built once and copied for each test, because a git process costs far more than a copy: a working copy holding a committed
# gallery and an unrelated source file, beside the bare origin it has pushed to. The remote is a relative path, so a copy of
# both stays a working pair.
$galleryBase = New-FixtureRoot
$baseWork = Join-Path $galleryBase 'work'
$baseOrigin = Join-Path $galleryBase 'origin.git'
New-Item -ItemType Directory -Path $baseOrigin, $baseWork | Out-Null
Invoke-FixtureGit $baseOrigin @('init', '--bare', '-q', '-b', 'main')
Invoke-FixtureGit $baseWork @('init', '-q', '-b', 'main')
Invoke-FixtureGit $baseWork @('config', 'user.name', 'fixture')
Invoke-FixtureGit $baseWork @('config', 'user.email', 'fixture@example.invalid')
Set-FixtureBytes $baseWork 'docs/gallery/light.png' ([byte[]](0x89, 0x50, 1, 2, 3))
Set-FixtureBytes $baseWork 'docs/gallery/dark.png' ([byte[]](0x89, 0x50, 4, 5, 6))
Set-FixtureFile $baseWork 'docs/gallery/README.md' "# Generated control gallery`n"
Set-FixtureFile $baseWork 'docs/gallery/index.html' "<!doctype html>`n"
Set-FixtureFile $baseWork 'docs/gallery/generation.json' "{ `"sourceCommit`": `"aaa`" }`n"
Set-FixtureFile $baseWork 'src/Code.cpp' "int a;`n"
Invoke-FixtureGit $baseWork @('add', '-A')
Invoke-FixtureGit $baseWork @('commit', '-q', '-m', 'base')
Invoke-FixtureGit $baseWork @('remote', 'add', 'origin', '../origin.git')
Invoke-FixtureGit $baseWork @('push', '-q', 'origin', 'main')

function Copy-GalleryRepository([string] $Root) {
    Copy-Item -LiteralPath $baseWork -Destination (Join-Path $Root 'work') -Recurse
    Copy-Item -LiteralPath $baseOrigin -Destination (Join-Path $Root 'origin.git') -Recurse
    return [pscustomobject]@{ Work = (Join-Path $Root 'work'); Origin = (Join-Path $Root 'origin.git') }
}

function Invoke-CommitGallery([string] $Root, [switch] $NoPush) {
    # The workflow step's script, in this process. A run that fails throws, as it fails the step.
    $arguments = @{ Root = $Root; NoPush = [bool]$NoPush }
    try {
        $output = & $commitGallery @arguments 6>&1 2>&1
        return [pscustomobject]@{ Failed = $false; Output = (@($output | ForEach-Object { "$_" }) -join "`n") }
    } catch {
        return [pscustomobject]@{ Failed = $true; Output = $_.Exception.Message }
    }
}

function Get-Head([string] $Root, [string] $Branch = 'HEAD') { return (Get-FixtureGit $Root @('rev-parse', $Branch))[0] }

Invoke-FixtureCase 'a regenerated sheet is committed and pushed with the gallery only' {
    param($root)
    $repo = Copy-GalleryRepository $root
    $before = Get-Head $repo.Work
    # A new sheet and a new generation receipt, and a source file the build touched, which must not be published.
    Set-FixtureBytes $repo.Work 'docs/gallery/light.png' ([byte[]](0x89, 0x50, 9, 9, 9))
    Set-FixtureFile $repo.Work 'docs/gallery/generation.json' "{ `"sourceCommit`": `"bbb`" }`n"
    Set-FixtureFile $repo.Work 'src/Code.cpp' "int b;`n"
    $result = Invoke-CommitGallery $repo.Work
    Assert-True (-not $result.Failed) "the run passes: $($result.Output)"
    $shown = Get-FixtureGit $repo.Work @('show', '--name-only', '--format=%s', 'HEAD')
    Assert-Equal 'docs: regenerate the control gallery' $shown[0] 'commit message'
    Assert-Equal 'docs/gallery/generation.json docs/gallery/light.png' (($shown | Select-Object -Skip 1 | Where-Object { $_ } | Sort-Object) -join ' ') 'the commit holds the gallery files that changed and nothing else'
    Assert-Equal ' M src/Code.cpp' (Get-FixtureGit $repo.Work @('status', '--porcelain'))[0] 'the source edit stays uncommitted'
    Assert-Equal (Get-Head $repo.Work) (Get-Head $repo.Origin 'main') 'the branch was pushed'
    Assert-True ($before -cne (Get-Head $repo.Origin 'main')) 'and moved'
}

Invoke-FixtureCase 'a new generation receipt or an identical rewrite alone is not a change' {
    param($root)
    $repo = Copy-GalleryRepository $root
    $before = Get-Head $repo.Work
    # The receipt records the commit the sheets came from, so it differs after every commit even when every sheet is the same;
    # a regeneration also rewrites each sheet with the bytes it already has.
    Set-FixtureFile $repo.Work 'docs/gallery/generation.json' "{ `"sourceCommit`": `"ccc`" }`n"
    Set-FixtureBytes $repo.Work 'docs/gallery/light.png' ([byte[]](0x89, 0x50, 1, 2, 3))
    Set-FixtureFile $repo.Work 'docs/gallery/README.md' "# Generated control gallery`n"
    $result = Invoke-CommitGallery $repo.Work
    Assert-True (-not $result.Failed) "the run passes: $($result.Output)"
    Assert-True $result.Output.Contains('already current') "reported as a no-op: $($result.Output)"
    Assert-Equal $before (Get-Head $repo.Work) 'no commit'
    Assert-Equal $before (Get-Head $repo.Origin 'main') 'nothing pushed'
}

Invoke-FixtureCase 'a changed index or README is committed even when no sheet changed' {
    param($root)
    $repo = Copy-GalleryRepository $root
    $before = Get-Head $repo.Work
    Set-FixtureFile $repo.Work 'docs/gallery/README.md' "# Generated control gallery`n`nA new sentence.`n"
    Assert-True (-not (Invoke-CommitGallery $repo.Work).Failed) 'the README run passes'
    $afterReadme = Get-Head $repo.Work
    Assert-True ($afterReadme -cne $before) 'the README is committed'
    Set-FixtureFile $repo.Work 'docs/gallery/index.html' "<!doctype html><title>x</title>`n"
    Assert-True (-not (Invoke-CommitGallery $repo.Work).Failed) 'the index run passes'
    Assert-True ((Get-Head $repo.Work) -cne $afterReadme) 'the index is committed'
}

Invoke-FixtureCase 'the push is never forced' {
    param($root)
    $repo = Copy-GalleryRepository $root
    # Someone else's commit lands on the branch after this checkout was made: push one, then take this checkout back.
    Invoke-FixtureGit $repo.Work @('commit', '-q', '--allow-empty', '-m', 'meanwhile')
    Invoke-FixtureGit $repo.Work @('push', '-q', 'origin', 'main')
    $theirs = Get-Head $repo.Origin 'main'
    Invoke-FixtureGit $repo.Work @('reset', '-q', '--hard', 'HEAD~1')
    Set-FixtureBytes $repo.Work 'docs/gallery/light.png' ([byte[]](0x89, 0x50, 7, 7, 7))
    $result = Invoke-CommitGallery $repo.Work
    Assert-True $result.Failed 'the run fails'
    Assert-True $result.Output.Contains('no force push was attempted') "and says so: $($result.Output)"
    Assert-Equal $theirs (Get-Head $repo.Origin 'main') 'the other commit is intact on the remote'
}

Invoke-FixtureCase 'nothing is pushed when asked not to' {
    param($root)
    $repo = Copy-GalleryRepository $root
    $before = Get-Head $repo.Origin 'main'
    Set-FixtureBytes $repo.Work 'docs/gallery/dark.png' ([byte[]](0x89, 0x50, 8, 8, 8))
    $result = Invoke-CommitGallery $repo.Work -NoPush
    Assert-True (-not $result.Failed) "the run passes: $($result.Output)"
    Assert-True ((Get-Head $repo.Work) -cne $before) 'committed locally'
    Assert-Equal $before (Get-Head $repo.Origin 'main') 'and not pushed'
}

Invoke-TestCase 'the gallery workflow publishes only on manual dispatch, with the safeguards of the formatting workflow' {
    $workflow = [IO.File]::ReadAllText((Join-Path $repository '.github/workflows/gallery.yml'))
    $lines = $workflow -split "`r?`n"
    # The events under on:, at the first level of indentation.
    $inside = $false
    $events = foreach ($line in $lines) {
        if ($line -cmatch '^on:\s*$') { $inside = $true; continue }
        if ($inside -and $line -cmatch '^\S') { break }
        if ($inside -and $line -cmatch '^  (\S+):') { $Matches[1] }
    }
    Assert-Equal 'workflow_dispatch' (@($events) -join ',') 'the only trigger is a manual dispatch'
    Assert-True ($workflow -cmatch '(?s)inputs:\s+publish_docs:.*?type: boolean\s+default: false') 'an explicit boolean input that defaults to false'
    Assert-True (-not $workflow.Contains('pull_request_target')) 'never pull_request_target'
    Assert-True ($workflow -cmatch '(?m)^permissions:\s*\r?\n\s+contents: read\s*$') 'read-only by default'
    $writes = @([regex]::Matches($workflow, '(?m)^\s*contents:\s*write\s*$'))
    Assert-Equal 1 $writes.Count 'exactly one write permission'
    Assert-True ($writes[0].Index -gt $workflow.IndexOf("`njobs:")) 'and it belongs to the job, not the workflow'
    Assert-True ($workflow.Contains("if: github.event_name == 'workflow_dispatch' && inputs.publish_docs && startsWith(github.ref, 'refs/heads/')")) 'the job needs the input, and a branch to push to'
    Assert-True ($workflow.Contains('ref: ${{ github.ref }}')) 'the selected branch is checked out'
    Assert-True ($workflow.Contains('./vcpkg-install.ps1 -Platform x64')) 'dependencies are restored'
    Assert-True ($workflow.Contains('./gallery.ps1 -Configuration Release -Platform x64 -PublishDocs')) 'a native x64 Release gallery is generated and published'
    Assert-True ($workflow.Contains('./Tools/Commit-Gallery.ps1')) 'and committed by the tested script'
    $script = [IO.File]::ReadAllText($commitGallery)
    Assert-True ($script.Contains('push origin HEAD')) 'an ordinary push of the checked-out branch'
    Assert-True ($script -cnotmatch '--force|--force-with-lease|push\s+(-\w*f|\+)') 'never a force push'
}

Remove-FixtureRoot $galleryBase
Complete-TestRun 'Docs'
