# Inherited-test accounting: count, origin, missing case and incomplete-disposition regressions.
[CmdletBinding()] param()
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../Validation.psm1') -Force

function New-TestPortManifest {
    return [ordered]@{ schemaVersion = 1; originCommit = 'a' * 40; originCaseCount = 1; tests = @(
            [ordered]@{ file = 'Tests/Controls/Example.cpp'; test = 'TestToggle'; status = 'ported'; reason = 'Runtime behavior' }) }
}

function Set-TestPortFixture([string] $Root, [Collections.IDictionary] $Manifest) {
    Set-FixtureFile $Root 'Tests/Controls/Example.cpp' "void TestToggle() {}`n"
    Set-FixtureJson $Root 'Specs/Done/SourceImport/test-port.json' $Manifest
}

function New-SourcePolicy {
    return [ordered]@{ schemaVersion = 1; dispositions = @([ordered]@{
                originFile = 'Tests/Controls/Example.cpp'; originTest = 'TestToggle'; decision = 'runtime-restored'
                rationale = 'Behavior replaces spelling'; runtimeCases = @([ordered]@{ file = 'Tests/Controls/Example.cpp'; test = 'TestToggle' })
            }) }
}

function Set-PolicyFixture([string] $Root, [Collections.IDictionary] $Policy) {
    $manifest = New-TestPortManifest
    $manifest.tests[0].status = 'excluded'
    $manifest.tests[0].reason = 'Source-text implementation assertion'
    Set-TestPortFixture $Root $manifest
    Set-FixtureJson $Root 'Specs/Testing/SourcePolicyDispositions.json' $Policy
}

function Get-TestPortFailureCount([string] $Root) { return (Test-DxUiTestPort $Root).Failures.Count }

Invoke-FixtureCase 'a retained case is accounted for' {
    param($root)
    Set-TestPortFixture $root (New-TestPortManifest)
    Assert-Equal 0 (Get-TestPortFailureCount $root) 'failures'
}

Invoke-FixtureCase 'silently deleting a case fails' {
    param($root)
    Set-TestPortFixture $root (New-TestPortManifest)
    Set-FixtureFile $root 'Tests/Controls/Example.cpp' ''
    Assert-True (Get-TestPortFailureCount $root) 'a deleted case fails'
}

Invoke-FixtureCase 'discarding a disposition fails' {
    param($root)
    $manifest = New-TestPortManifest
    $manifest.tests = @()
    Set-TestPortFixture $root $manifest
    Assert-True (Get-TestPortFailureCount $root) 'a discarded disposition fails'
}

Invoke-FixtureCase 'an explicit rename retains the origin' {
    param($root)
    $manifest = New-TestPortManifest
    $manifest.tests[0].currentTest = 'TestNeutralToggle'
    Set-TestPortFixture $root $manifest
    Set-FixtureFile $root 'Tests/Controls/Example.cpp' "void TestNeutralToggle() {}`n"
    Assert-Equal 0 (Get-TestPortFailureCount $root) 'failures'
}

Invoke-FixtureCase 'an exclusion needs a reason' {
    param($root)
    $manifest = New-TestPortManifest
    $manifest.tests[0].status = 'excluded'
    $manifest.tests[0].reason = ''
    Set-TestPortFixture $root $manifest
    Assert-True (Get-TestPortFailureCount $root) 'an unexplained exclusion fails'
}

Invoke-FixtureCase 'a current runtime replacement passes' {
    param($root)
    Set-PolicyFixture $root (New-SourcePolicy)
    Assert-Equal 0 (Get-TestPortFailureCount $root) 'failures'
}

Invoke-FixtureCase 'a missing current policy fails' {
    param($root)
    $policy = New-SourcePolicy
    $policy.dispositions = @()
    Set-PolicyFixture $root $policy
    Assert-True (Get-TestPortFailureCount $root) 'a missing disposition fails'
}

Invoke-FixtureCase 'a deleted replacement fails' {
    param($root)
    Set-PolicyFixture $root (New-SourcePolicy)
    Set-FixtureFile $root 'Tests/Controls/Example.cpp' ''
    Assert-True (Get-TestPortFailureCount $root) 'a deleted replacement fails'
}

Invoke-FixtureCase 'a duplicate policy fails' {
    param($root)
    $policy = New-SourcePolicy
    $policy.dispositions = @($policy.dispositions[0], $policy.dispositions[0])
    Set-PolicyFixture $root $policy
    Assert-True (Get-TestPortFailureCount $root) 'a duplicate disposition fails'
}

Complete-TestRun 'TestPort'
