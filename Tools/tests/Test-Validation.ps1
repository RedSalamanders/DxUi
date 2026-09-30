# Owned-source evolution, dependency boundaries and skill metadata, validated in isolated fixture trees.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../Validation.psm1') -Force

function Set-DependencyFixture([string] $Root) {
    $revision = 'a' * 40
    Set-FixtureJson $Root 'vcpkg.json' @{ 'builtin-baseline' = $revision }
    Set-FixtureJson $Root 'vcpkg-tool.json' @{ commit = $revision }
    Set-FixtureFile $Root 'src/Controls/Control.cpp' "original`n"
    Set-FixtureJson $Root 'Specs/Done/SourceImport/source-origin.json' @{ schemaVersion = 2; commit = $revision; files = @(@{
                source = 'Common/DxUi/Control.cpp'; currentPath = 'src/Controls/Control.cpp'; disposition = 'owned'
                originalSha256 = Get-Sha256Hex ([Text.Encoding]::UTF8.GetBytes("original`n")); originalBytes = 9
            }) }
    Set-FixtureJson $Root 'Specs/Done/SourceImport/pending-dependencies.json' @{ schemaVersion = 1; files = @() }
}

function Set-Origin([string] $Root, [hashtable] $Changes) {
    $manifest = Get-FixtureJson $Root 'Specs/Done/SourceImport/source-origin.json'
    foreach ($key in $Changes.Keys) { $manifest['files'][0][$key] = $Changes[$key] }
    Set-FixtureJson $Root 'Specs/Done/SourceImport/source-origin.json' $manifest
}

function Get-DependencyFailureCount([string] $Root) { return (Test-DxUiDependencies $Root).Failures.Count }

Invoke-FixtureCase 'owned source can evolve without rewriting its historical hash' {
    param($root)
    Set-DependencyFixture $root
    Set-FixtureFile $root 'src/Controls/Control.cpp' "// independent improvements`n"
    Assert-Equal 0 (Get-DependencyFailureCount $root) 'failures'
}

Invoke-FixtureCase 'new owned source needs no fake historical origin' {
    param($root)
    Set-DependencyFixture $root
    Set-FixtureFile $root 'src/New.cpp' "#include <cstdint>`n"
    Assert-Equal 0 (Get-DependencyFailureCount $root) 'failures'
}

Invoke-FixtureCase 'missing owned source is rejected' {
    param($root)
    Set-DependencyFixture $root
    Set-Origin $root @{ currentPath = 'src/missing.cpp' }
    Assert-True (Get-DependencyFailureCount $root) 'a missing owned source fails'
}

Invoke-FixtureCase 'a parent path cannot escape the owned tree' {
    param($root)
    Set-DependencyFixture $root
    Set-Origin $root @{ currentPath = '../elsewhere.cpp' }
    Assert-True (Get-DependencyFailureCount $root) 'an escaping path fails'
}

Invoke-FixtureCase 'developer settings cannot be owned source' {
    param($root)
    Set-DependencyFixture $root
    Set-FixtureFile $root 'src/Controls/project.user' "settings`n"
    Set-Origin $root @{ currentPath = 'src/Controls/project.user' }
    Assert-True (Get-DependencyFailureCount $root) 'a .user file fails'
}

Invoke-FixtureCase 'a duplicate original source tree is rejected' {
    param($root)
    Set-DependencyFixture $root
    Set-FixtureFile $root 'upstream/original.cpp' "duplicate`n"
    Assert-True (Get-DependencyFailureCount $root) 'an upstream tree fails'
}

Invoke-FixtureCase 'application debt cannot be reintroduced' {
    param($root)
    Set-DependencyFixture $root
    Set-FixtureFile $root 'src/Controls/Control.cpp' "#include `"Helpers.h`"`n"
    Assert-True (Get-DependencyFailureCount $root) 'an application include fails'
    Set-FixtureJson $root 'Specs/Done/SourceImport/pending-dependencies.json' @{ schemaVersion = 1; files = @(@{ path = 'src/Controls/Control.cpp'; includes = @('Helpers.h') }) }
    Assert-True (Get-DependencyFailureCount $root) 'recording the include does not waive it'
    Set-FixtureFile $root 'src/Controls/Control.cpp' "#include <cstdint>`n"
    Assert-True (Get-DependencyFailureCount $root) 'a stale pending record fails'
}

Invoke-FixtureCase 'supported source cannot waive an application dependency' {
    param($root)
    Set-DependencyFixture $root
    Set-FixtureFile $root 'src/Foundation/one.cpp' "#include `"Helpers.h`"`n"
    Set-FixtureJson $root 'Specs/Done/SourceImport/pending-dependencies.json' @{ schemaVersion = 1; files = @(@{ path = 'src/Foundation/one.cpp'; includes = @('Helpers.h') }) }
    Assert-True (Get-DependencyFailureCount $root) 'a waived application include fails'
}

Invoke-FixtureCase 'independent controls can enter the single supported build' {
    param($root)
    Set-DependencyFixture $root
    Set-FixtureFile $root 'src/Bad.vcxproj' '<Project><ClCompile Include="Controls/Control.cpp" /></Project>'
    Assert-Equal 0 (Get-DependencyFailureCount $root) 'failures'
}

Invoke-FixtureCase 'owned source can use other owned headers' {
    param($root)
    Set-DependencyFixture $root
    Set-FixtureFile $root 'src/Controls/Control.h' "#pragma once`n"
    Set-FixtureFile $root 'src/Foundation/one.cpp' "#include `"../Controls/Control.h`"`n"
    Assert-Equal 0 (Get-DependencyFailureCount $root) 'failures'
}

Invoke-FixtureCase 'the old application namespace is rejected' {
    param($root)
    Set-DependencyFixture $root
    Set-FixtureFile $root 'src/Controls/Control.cpp' "namespace RedSalamander::DxUi {}`n"
    Assert-True (Get-DependencyFailureCount $root) 'the old namespace fails'
}

Invoke-FixtureCase 'a second static library is rejected' {
    param($root)
    Set-DependencyFixture $root
    $library = '<Project><PropertyGroup><ConfigurationType>StaticLibrary</ConfigurationType></PropertyGroup></Project>'
    Set-FixtureFile $root 'src/One.vcxproj' $library
    Assert-Equal 0 (Get-DependencyFailureCount $root) 'one library'
    Set-FixtureFile $root 'Samples/Two.vcxproj' $library
    Assert-Contains (Test-DxUiDependencies $root).Failures 'DxUi ships one static library; split targets are not supported' 'two libraries'
}

Invoke-FixtureCase 'a nested checkout under owned source is not scanned' {
    param($root)
    Set-DependencyFixture $root
    Set-FixtureFile $root 'src/vendor/lib/.git' "gitdir: elsewhere`n"
    Set-FixtureFile $root 'src/vendor/lib/Bad.cpp' "#include `"Helpers.h`"`n"
    Set-FixtureFile $root 'src/vendor/lib/Broken.vcxproj' '<Project><ClCompile Include="missing.cpp" /></Project>'
    Assert-Equal 0 (Get-DependencyFailureCount $root) 'failures'
    Set-FixtureFile $root 'src/other/Bad.cpp' "#include `"Helpers.h`"`n"
    Assert-True (Get-DependencyFailureCount $root) 'the same file in the tree itself fails'
}

Invoke-FixtureCase 'a path longer than a consumer''s pinned restore can check out is rejected' {
    param($root)
    Set-DependencyFixture $root
    $folder = 'Measurements/' + ('a' * 100) + '/'
    Set-FixtureFile $root ($folder + ('b' * (150 - $folder.Length))) "fits`n"
    Assert-Equal 0 (Get-DependencyFailureCount $root) 'a path of exactly 150 characters'
    Set-FixtureFile $root ($folder + ('c' * (151 - $folder.Length))) "too long`n"
    $failures = @((Test-DxUiDependencies $root).Failures)
    Assert-Equal 1 $failures.Count 'one failure'
    Assert-True $failures[0].StartsWith('Path of 151 characters, over the 150') "the failure gives the length and the budget: $($failures[0])"
}

Invoke-FixtureCase 'in a checkout the path budget covers what a commit would hold, not ignored output' {
    param($root)
    Set-DependencyFixture $root
    git -C $root init --quiet
    Assert-Equal 0 $LASTEXITCODE 'git init'
    Set-FixtureFile $root '.gitignore' ".build/`n"
    # 151 characters: one over the budget, and short enough for git itself under a worktree's fixture root.
    Set-FixtureFile $root ('.build/' + ('d' * 144)) "ignored output`n"
    Assert-Equal 0 (Get-DependencyFailureCount $root) 'ignored build output is not checked'
    Set-FixtureFile $root ('Samples/' + ('e' * 143)) "not yet added`n"
    Assert-Equal 1 (Get-DependencyFailureCount $root) 'an untracked file a commit would add is checked'
}

Invoke-FixtureCase 'every validation step runs and every failure is reported' {
    param($root)
    Set-FixtureFile $root 'first.ps1' 'exit 1'
    Set-FixtureFile $root 'second.ps1' "Write-Output '(expected in this test: a step printing to the success stream)'"
    Set-FixtureFile $root 'third.ps1' 'exit 3'
    Set-FixtureFile $root 'fourth.ps1' 'exit 0'
    $steps = @('first.ps1', 'second.ps1', 'third.ps1', 'missing.ps1', 'fourth.ps1')
    $results = @(Invoke-DxUiValidation -Root $root -Steps $steps 6>$null)
    Assert-Equal ($steps -join ' ') (($results | ForEach-Object { $_.Step }) -join ' ') 'every step ran, in order, after earlier failures'
    Assert-Equal 'False True False False True' (($results | ForEach-Object { $_.Passed }) -join ' ') 'which steps passed'
    Assert-True $results[0].Error.Contains('first.ps1 exited with code 1') 'a failing step is named with its exit code'
    Assert-True $results[2].Error.Contains('code 3') 'a nonzero exit code fails the step with that code'
    Assert-True $results[3].Error.Contains('missing.ps1 does not exist') 'a step that is not there fails'
    Assert-Equal $null $results[4].Error 'a passing step has no error'
    Assert-Equal 5 $results.Count 'stray step output is not a result'
}

Invoke-TestCase 'every tooling test script is strict itself, so a direct run fails as its run under test.ps1 does' {
    # test.ps1 and Invoke-ToolingTests.ps1 are strict and the scripts they call inherit it: a script that is not strict itself
    # can pass when run directly and fail under them, as .Count on a function's unrolled output did in Test-TestWatchdog.ps1.
    $lax = @(Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.ps1' | Where-Object { [IO.File]::ReadAllText($_.FullName) -notmatch '(?m)^Set-StrictMode -Version Latest\r?$' } | ForEach-Object Name)
    Assert-Equal 0 $lax.Count "tooling tests without Set-StrictMode -Version Latest: $($lax -join ', ')"
}

Invoke-TestCase 'the validation entry point covers every validator and is what CI and test.ps1 run' {
    $repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
    $steps = @(Get-DxUiValidationSteps)
    foreach ($script in Get-ChildItem -LiteralPath $repository -Filter 'validate-*.ps1') { Assert-Contains $steps $script.Name "the validator $($script.Name) is a step" }
    Assert-Contains $steps 'Tools/tests/Invoke-ToolingTests.ps1' 'the tooling tests are a step'
    Assert-Equal $steps.Count @($steps | Select-Object -Unique).Count 'no step twice'
    foreach ($step in $steps) { Assert-True (Test-Path -LiteralPath (Join-Path $repository $step) -PathType Leaf) "the step exists: $step" }
    $ci = [IO.File]::ReadAllText((Join-Path $repository '.github/workflows/ci.yml'))
    Assert-True ($ci -cmatch '(?m)^\s+run: \./validate\.ps1\s*$') 'the validation job runs validate.ps1'
    Assert-True (-not $ci.Contains('run: ./validate-skills.ps1')) 'and not the validators one by one'
    Assert-True ([IO.File]::ReadAllText((Join-Path $repository 'test.ps1')).Contains("'Tools/tests/Invoke-ToolingTests.ps1'")) 'test.ps1 runs the tooling tests'
    foreach ($name in @('validate.ps1', 'test.ps1')) {
        $tokens = $null
        $errors = $null
        [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $repository $name), [ref]$tokens, [ref]$errors)
        Assert-Equal 0 @($errors).Count "$name has no syntax errors: $(@($errors | ForEach-Object { $_.Message }) -join '; ')"
    }
}

function Get-SkillFailures([string] $Root, [string] $Folder, [string] $Text) {
    Set-FixtureFile $Root ".agents/skills/$Folder/SKILL.md" $Text
    return , @((Test-DxUiSkills $Root).Failures)
}

Invoke-FixtureCase 'invalid skill metadata is rejected' {
    param($root)
    Assert-True (Get-SkillFailures $root 'example' "---`nname: wrong`ndescription: Useful task.`n---`nDo work.`n").Count 'a mismatched name fails'
}

Invoke-FixtureCase 'valid skill metadata passes' {
    param($root)
    Assert-Equal 0 (Get-SkillFailures $root 'example' "---`nname: example`ndescription: Useful task.`n---`nDo work.`n").Count 'failures'
}

Invoke-FixtureCase 'CRLF skill files and quoted values read like YAML' {
    param($root)
    $text = "---`r`nname: 'example'`r`ndescription: `"Build: then test \`"all\`" suites.`" # quoted`r`n# comment`r`n---`r`nDo work.`r`n"
    Assert-Equal 0 (Get-SkillFailures $root 'example' $text).Count 'failures'
    $metadata = ConvertFrom-SkillFrontMatter "name: 'it''s'`ndescription: `"a \\ b`""
    Assert-Equal "it's" $metadata['name'] 'single-quoted escape'
    Assert-Equal 'a \ b' $metadata['description'] 'double-quoted escape'
}

Invoke-FixtureCase 'front matter outside the strict subset is rejected' {
    param($root)
    foreach ($description in @('Build: then test.', 'Useful # task', '>', '[a, b]', "'unterminated", '"bad \n escape"')) {
        $failures = Get-SkillFailures $root 'example' "---`nname: example`ndescription: $description`n---`nDo work.`n"
        Assert-True $failures.Count "description <$description> is rejected"
    }
    Assert-True (Get-SkillFailures $root 'example' "---`nname: example`n  nested: value`ndescription: Useful.`n---`nDo work.`n").Count 'an indented line fails'
    Assert-True (Get-SkillFailures $root 'example' "---`nname: example`nname: example`ndescription: Useful.`n---`nDo work.`n").Count 'a duplicate key fails'
}

Invoke-FixtureCase 'values YAML types as non-strings are not names or descriptions' {
    param($root)
    Assert-Contains (Get-SkillFailures $root 'example' "---`nname: example`ndescription: yes`n---`nDo work.`n") 'example: invalid description' 'a boolean description'
    Assert-Contains (Get-SkillFailures $root 'example' "---`nname: example`ndescription: 2026-09-30`n---`nDo work.`n") 'example: invalid description' 'a date description'
    Remove-Item -LiteralPath (Join-Path $root '.agents/skills/example') -Recurse -Force
    Assert-Contains (Get-SkillFailures $root '123' "---`nname: 123`ndescription: Useful.`n---`nDo work.`n") '123: invalid name' 'an integer name'
    Assert-Equal 0 (Get-SkillFailures $root '123' "---`nname: '123'`ndescription: Useful.`n---`nDo work.`n").Count 'a quoted numeric name'
}

Invoke-FixtureCase 'missing front matter and empty instructions are rejected' {
    param($root)
    Assert-Contains (Get-SkillFailures $root 'example' "name: example`n") 'example: missing YAML front matter' 'no front matter'
    Assert-Contains (Get-SkillFailures $root 'example' "---`nname: example`ndescription: Useful.`n---`n  `n") 'example: empty instructions' 'blank body'
}

Complete-TestRun 'Validation'
