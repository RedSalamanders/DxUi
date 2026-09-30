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

Complete-TestRun 'Docs'
