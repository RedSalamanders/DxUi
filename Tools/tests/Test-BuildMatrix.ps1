# Missing configurations, duplicate entries, silent Debug fallback and missing solution build mappings.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../Validation.psm1') -Force

Invoke-FixtureCase 'every project requires real configuration entries' {
    param($root)
    $path = Join-Path $root 'library.vcxproj'
    $write = { param([string[]] $Configurations) Set-FixtureFile $root 'library.vcxproj' ('<Project><ItemGroup>' + (($Configurations | ForEach-Object { "<ProjectConfiguration Include=`"$_`" />" }) -join '') + '</ItemGroup></Project>') }
    $matrix = @(Get-BuildMatrix)
    & $write $matrix
    Assert-Equal 0 @(Test-DxUiProjectConfigurations $path).Count 'errors'
    & $write @($matrix | Where-Object { $_ -cne 'ASan Debug|ARM64' })
    Assert-True @(Test-DxUiProjectConfigurations $path | Where-Object { $_.Contains('ASan Debug|ARM64') }).Count 'the missing configuration is named'
    & $write ($matrix + @('Debug|x64'))
    Assert-True @(Test-DxUiProjectConfigurations $path | Where-Object { $_.Contains('duplicate') }).Count 'a duplicate is reported'
}

Invoke-FixtureCase 'the consumer validator resolves a relative Root from another working directory' {
    param($root)
    $matrix = @(Get-BuildMatrix)
    Set-FixtureFile $root 'consumer.vcxproj' ('<Project><ItemGroup>' + (($matrix | ForEach-Object { "<ProjectConfiguration Include=`"$_`" />" }) -join '') + '</ItemGroup></Project>')
    & git -C $root init --quiet
    Assert-Equal 0 $LASTEXITCODE 'fixture git init'
    $caller = Split-Path -Parent $root
    $relativeRoot = Split-Path -Leaf $root
    $entry = Join-Path $PSScriptRoot '../../validate-build-matrix.ps1'
    Push-Location $caller
    try { & $entry -Root $relativeRoot } finally { Pop-Location }
}

Invoke-FixtureCase 'a silent Debug fallback and a missing build are rejected' {
    param($root)
    $path = Join-Path $root 'consumer.sln'
    $matrix = @(Get-BuildMatrix)
    $header = "Project(`"{TYPE}`") = `"app`", `"app.vcxproj`", `"{APP}`"`nEndProject`nGlobalSection(SolutionConfigurationPlatforms) = preSolution`n"
    $header += (($matrix | ForEach-Object { "  $_ = $_`n" }) -join '') + "EndGlobalSection`n"
    $mappings = (($matrix | ForEach-Object { $configuration = $_; @('ActiveCfg', 'Build.0') | ForEach-Object { "  {APP}.$configuration.$_ = $configuration`n" } }) -join '')
    Set-FixtureFile $root 'consumer.sln' ($header + $mappings)
    Assert-Equal 0 @(Test-DxUiSolutionConfigurations $path).Count 'errors'
    Set-FixtureFile $root 'consumer.sln' ($header + $mappings).Replace('ASan Debug|ARM64.ActiveCfg = ASan Debug|ARM64', 'ASan Debug|ARM64.ActiveCfg = Debug|ARM64')
    Assert-True @(Test-DxUiSolutionConfigurations $path | Where-Object { $_.Contains('maps to') }).Count 'a fallback mapping is reported'
    Set-FixtureFile $root 'consumer.sln' ($header + $mappings).Replace("  {APP}.Release|ARM64.Build.0 = Release|ARM64`n", '')
    Assert-True @(Test-DxUiSolutionConfigurations $path | Where-Object { $_.Contains('Build.0') }).Count 'a missing build mapping is reported'
    Set-FixtureFile $root 'consumer.sln' ($header + $mappings).Replace("`n", "`r`n")
    Assert-Equal 0 @(Test-DxUiSolutionConfigurations $path).Count 'a CRLF solution reads like an LF one'
}

Complete-TestRun 'BuildMatrix'
