<# .SYNOPSIS Verifies complete evaluation-only MSBuild metadata with the installed engine and independent batch queries. #>
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $root 'Tools/VisualStudio.psm1')
$module=Import-Module (Join-Path $root 'Tools/PerformancePolicy.psm1') -PassThru
$msbuild=Join-Path (Get-DxUiVisualStudioInstallation) 'MSBuild/Current/Bin/MSBuild.exe'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
function Query([string]$Root,[string]$MSBuild,[string[]]$Arguments,[string[]]$Names){
    & $module {param($Root,$MSBuild,$Arguments,$Names) Get-PerformancePropertyValues -Root $Root -MSBuildPath $MSBuild -Arguments $Arguments -Names $Names -Role fixture} $Root $MSBuild $Arguments $Names
}
Invoke-FixtureCase 'one response-file query preserves every value beyond Windows command-line limits without executing targets' {
    param($fixture)
    $project=Join-Path $fixture 'metadata.proj'
    $names=@(0..1499|ForEach-Object{'Probe_Property_{0:D4}_With_A_Long_Explicit_Metadata_Name' -f $_})
    $xml='<Project DefaultTargets="NeverBuild" InitialTargets="NeverBuild"><PropertyGroup>'
    foreach($name in $names){$xml+="<$name>"+[Security.SecurityElement]::Escape("$name Unicode 漢字 é Ω & &quot; literal")+"</$name>"}
    $xml+='</PropertyGroup><ItemGroup><Compile Include="Example.cpp" /></ItemGroup><PropertyGroup><AfterItems>@(Compile)</AfterItems></PropertyGroup>'
    $xml+='<Target Name="NeverBuild"><Error Text="Metadata queries must not execute targets." /></Target></Project>'
    [IO.File]::WriteAllText($project,$xml,[Text.UTF8Encoding]::new($false))
    $names+=@('AfterItems','UndefinedProperty')
    Assert-True ((('-getProperty:'+($names -join ',')).Length) -gt 32768) 'the fixture exceeds the native argv bound'
    $before=(Get-FileHash -LiteralPath $project).Hash
    $expected=[ordered]@{}
    # Independent reference: the original bounded command-line batches, not the response-file implementation.
    for($offset=0;$offset -lt $names.Count;$offset+=200){
        $batch=$names[$offset..([Math]::Min($offset+199,$names.Count-1))]
        $json=@(& $msbuild $project /nologo "-getProperty:$($batch -join ',')" 2>&1)
        if($LASTEXITCODE){throw 'Independent property reference failed.'}
        $values=($json -join "`n"|ConvertFrom-Json -AsHashtable).Properties
        foreach($name in $batch){$expected[$name]=$values[$name]}
    }
    $actual=Query $fixture $msbuild @($project,'/nologo') $names
    Assert-Equal ($expected|ConvertTo-Json -Compress) ($actual|ConvertTo-Json -Compress) 'every property, Unicode/escaped value, item-dependent value and absent value matches'
    Assert-Equal $before (Get-FileHash -LiteralPath $project).Hash 'evaluation does not change the project'
    Assert-Equal 0 @(Get-ChildItem (Join-Path $fixture '.build/msbuild-queries') -File).Count 'only the owned response file was removed'
    Assert-Equal 1 @(Get-ChildItem $fixture -File).Count 'no compiler or target output was created'
}
Invoke-FixtureCase 'nonzero evaluation and malformed JSON cannot return a partial property map' {
    param($fixture)
    $project=Join-Path $fixture 'invalid.proj';[IO.File]::WriteAllText($project,'<Project>')
    Assert-Throws {Query $fixture $msbuild @($project,'/nologo') @('One','Two')} 'nonzero MSBuild evaluation refuses'
    Assert-Throws {Query $fixture (Join-Path $fixture 'missing-engine.exe') @($project,'/nologo') @('One','Two')} 'process launch failure refuses'
    $fake=Join-Path $fixture 'fake.ps1'
    [IO.File]::WriteAllText($fake,'param([Parameter(ValueFromRemainingArguments=$true)][string[]]$Ignored); Write-Output "invalid JSON"; exit 0')
    Assert-Throws {Query $fixture (Get-Process -Id $PID).Path @('-NoProfile','-File',$fake) @('One','Two')} 'malformed output refuses'
    [IO.File]::WriteAllText($fake,'param([Parameter(ValueFromRemainingArguments=$true)][string[]]$Ignored); Write-Output ''{"Properties":{"One":"present"}}''; exit 0')
    Assert-Throws {Query $fixture (Get-Process -Id $PID).Path @('-NoProfile','-File',$fake) @('One','Two')} 'missing properties refuse'
    Assert-Equal 0 @(Get-ChildItem (Join-Path $fixture '.build/msbuild-queries') -File).Count 'all refused queries removed their own response file'
}
Invoke-FixtureCase 'invalid property syntax, duplicate names and foreign roots refuse before query scratch' {
    param($fixture)
    foreach($names in @(@('One','One'),@('One','one'),@('One','Two,Other'),@('One',"Two`n-target:NeverBuild"),@('One','-target:NeverBuild'))){
        Assert-Throws {Query $fixture $msbuild @('nonexistent.proj') $names} 'invalid names cannot inject another switch'
    }
    Assert-Throws {Query $fixture $msbuild @() @('One')} 'one property would change JSON output shape'
    Assert-True (-not(Test-Path (Join-Path $fixture '.build'))) 'invalid requests created no query scratch'
    Assert-Throws {Query ([IO.Path]::GetPathRoot($fixture)) $msbuild @() @('One','Two')} 'drive-root output refuses'
    Assert-Throws {& $module {param($Root) Assert-PerformanceQueryPath $Root (Join-Path $Root 'outside.rsp')} $fixture} 'foreign query path refuses'
}
Complete-TestRun 'MSBuild metadata queries'
