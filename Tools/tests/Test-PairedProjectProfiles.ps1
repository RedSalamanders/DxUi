<#
.SYNOPSIS Checks paired timestamp staging against real installed-MSBuild evaluated library and benchmark profiles.
.DESCRIPTION
Evaluates synthetic copies of the repository's real project/import files. No targets, compilation, dependency restore,
attestation or desktop input runs. Retains original/staged/restored timestamps and evaluated metadata beneath
.build/paired-project-profile-fixtures/<unique-id>. Linux reports this Windows-specific qualification unavailable.
#>
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(-not $IsWindows){Write-Host 'SKIP paired project-profile evaluation: Windows MSBuild required; portable staging tests run separately.';return}
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../VisualStudio.psm1') -Force
$policy=Import-Module (Join-Path $PSScriptRoot '../PerformancePolicy.psm1') -Force -PassThru
$repository=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$msbuild=Join-Path (Get-DxUiVisualStudioInstallation) 'MSBuild/Current/Bin/MSBuild.exe'
$scratch=Join-Path $repository ('.build/paired-project-profile-fixtures/'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($scratch)
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $repository 'performance-paired.ps1'),[ref]$tokens,[ref]$errors)
foreach($name in @('Set-PairedProjectStamps','Restore-PairedProjectStamps','Assert-PairedBuildCompatibility')){
    $definition=$ast.Find({param($node) $node-is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name-ceq $name},$true)
    if($null-eq $definition){throw "Missing production function $name"}
    . ([scriptblock]::Create($definition.Extent.Text))
}
function Read-ActualProfile([string]$Root,[Collections.IDictionary]$Project,[string]$Label){
    & $policy {
        param($Root,$Project,$Label,$MSBuild,$Scratch)
        Push-Location -LiteralPath $Root
        try {
            $metadata=Get-PerformanceProjectMetadataCore -MSBuildPath $MSBuild -ProjectPath (Join-Path $Root $Project.path) -Configuration Release -Platform x64 -OutputRoot ($Root+'\.build\') -Root $Root -Role $Project.role
            # Preserve the complete exact inputs to the canonical profile, including every target-consumed property.
            $metadata|ConvertTo-Json -Depth 30|Set-Content (Join-Path $Scratch ($Label+'-'+$Project.role+'-metadata.json'))
            $normalized=[ordered]@{}
            foreach($name in $metadata.properties.Keys){$normalized[$name]=Normalize-PerformanceBuildPath $metadata.properties[$name] $Root}
            $normalized|ConvertTo-Json -Depth 10|Set-Content (Join-Path $Scratch ($Label+'-'+$Project.role+'-normalized-properties.json'))
            return $metadata
        } finally {Pop-Location}
    } $Root $Project $Label $msbuild $scratch
}
Invoke-TestCase 'real library and benchmark profiles expose the benchmark-only omission and match after complete catalog staging' {
    $projects=@(Get-PerformanceProjectCatalog)
    $roots=@((Join-Path $scratch 'candidate'),(Join-Path $scratch 'nested/baseline'))
    $files=@('DxUi.sln','Directory.Build.props','Directory.Build.targets','Build/DxUi.AddressSanitizer.targets','Build/NativeBuildPolicy.targets')+@($projects.path)
    $stamp=[DateTime]::UtcNow.AddDays(2)
    foreach($root in $roots){foreach($relative in $files){
        $destination=Join-Path $root $relative
        [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
        [IO.File]::Copy((Join-Path $repository $relative),$destination)
        [IO.File]::SetLastWriteTimeUtc($destination,$stamp.AddDays(-10))
    }}
    # Opposing newest-import/project histories model a checkout versus a restored overlay. The bytes are identical.
    foreach($root in $roots){[IO.File]::SetLastWriteTimeUtc((Join-Path $root 'Directory.Build.targets'),$stamp.AddMinutes(-2))}
    [IO.File]::SetLastWriteTimeUtc((Join-Path $roots[1] 'src/DxUi.vcxproj'),$stamp.AddMinutes(-1))
    $originals=@(foreach($root in $roots){foreach($relative in $files){$path=Join-Path $root $relative;
        [ordered]@{path=$path;originalUtc=[IO.File]::GetLastWriteTimeUtc($path);sha256=(Get-FileHash $path).Hash}}})
    $originals|ConvertTo-Json -Depth 5|Set-Content (Join-Path $scratch 'originals.json')
    $propsPath=Join-Path $roots[1] 'Directory.Build.props'
    $propsBytes=[IO.File]::ReadAllBytes($propsPath)
    $records=@()
    try {
        # Reproduce the old operation exactly: only benchmark projects receive the common timestamp.
        foreach($root in $roots){[IO.File]::SetLastWriteTimeUtc((Join-Path $root 'Tests/Embedded/DxUi.EmbeddedTests.vcxproj'),$stamp)}
        $old=@{}
        foreach($side in 0,1){foreach($project in $projects){$old["$side-$($project.role)"]=Read-ActualProfile $roots[$side] $project "benchmark-only-$side"}}
        Assert-Equal $old['0-benchmark'].profile.profileIdentity $old['1-benchmark'].profile.profileIdentity 'old operation equalizes the complete benchmark profile'
        Assert-True ($old['0-library'].profile.profileIdentity-cne $old['1-library'].profile.profileIdentity) 'the omitted library still differs'
        Assert-True ($old['0-library'].properties.MSBuildAllProjects.Contains('Directory.Build.targets')) 'first library selected the newer import'
        Assert-True ($old['1-library'].properties.MSBuildAllProjects.Contains('src\DxUi.vcxproj')) 'second library selected its newer project'
        foreach($record in $originals){[IO.File]::SetLastWriteTimeUtc($record.path,$record.originalUtc)}
        $records=@(Set-PairedProjectStamps -Roots $roots -StampUtc $stamp)
        $records|ConvertTo-Json -Depth 5|Set-Content (Join-Path $scratch 'staged-projects.json')
        Assert-Equal ($roots.Count*$projects.Count) $records.Count 'the staging manifest covers the canonical project set'
        $current=@{}
        foreach($side in 0,1){foreach($project in $projects){$current["$side-$($project.role)"]=Read-ActualProfile $roots[$side] $project "catalog-$side"}}
        foreach($project in $projects){Assert-Equal $current["0-$($project.role)"].profile.profileIdentity $current["1-$($project.role)"].profile.profileIdentity 'all complete profiles now match'}
        # Keep timestamps fixed while changing a real compiler option: semantic differences still fail strict admission.
        $changed=[Text.Encoding]::UTF8.GetString($propsBytes).Replace('/utf-8 /Zc:preprocessor','/utf-8 /Zc:preprocessor /Ob0')
        Assert-True ($changed.Contains('/Ob0')) 'the negative control changes an effective compiler option'
        [IO.File]::WriteAllText($propsPath,$changed,[Text.UTF8Encoding]::new($false))
        [IO.File]::SetLastWriteTimeUtc($propsPath,($originals|Where-Object path -CEQ $propsPath).originalUtc)
        $negative=Read-ActualProfile $roots[1] $projects[0] 'changed-option'
        Assert-True ($negative.profile.profileIdentity-cne $current['0-library'].profile.profileIdentity) 'complete staging does not conceal the compiler change'
        $left=[ordered]@{buildIdentityVersion=2;identityStatus='verifiable';identityScope='full';benchmarkSha256='same';harnessIdentity='same';toolchainIdentity=$current['0-library'].profile.profileIdentity;dependencyIdentity='same';environmentIdentity='same'}
        $right=[ordered]@{};foreach($name in $left.Keys){$right[$name]=$left[$name]};$right.toolchainIdentity=$negative.profile.profileIdentity
        Assert-Throws {Assert-PairedBuildCompatibility $left $right} 'the original strict preflight rejects changed compiler semantics'
    } finally {
        [IO.File]::WriteAllBytes($propsPath,$propsBytes)
        if($records.Count){Restore-PairedProjectStamps $records}
        foreach($record in $originals){[IO.File]::SetLastWriteTimeUtc($record.path,$record.originalUtc)}
        $restored=@(foreach($record in $originals){[ordered]@{path=$record.path;expectedUtc=$record.originalUtc;actualUtc=[IO.File]::GetLastWriteTimeUtc($record.path);expectedSha256=$record.sha256;actualSha256=(Get-FileHash $record.path).Hash}})
        $restored|ConvertTo-Json -Depth 5|Set-Content (Join-Path $scratch 'restored.json')
        foreach($record in $restored){Assert-Equal $record.expectedUtc.Ticks $record.actualUtc.Ticks 'every exact original timestamp returns';Assert-Equal $record.expectedSha256 $record.actualSha256 'every original byte returns'}
    }
}
Write-Host "Retained metadata-only profile fixture: $scratch"
Complete-TestRun 'PairedProjectProfiles'
