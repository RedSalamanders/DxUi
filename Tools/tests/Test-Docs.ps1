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

Complete-TestRun 'Docs'
