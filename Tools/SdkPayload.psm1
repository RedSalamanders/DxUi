# Shared byte staging only. Native profile/build/SDK qualification remains with each caller.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'SdkLibraryInputs.psm1')
Import-Module (Join-Path $PSScriptRoot 'SdkNotices.psm1')

function New-DxUiSdkPayload {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][string]$Stage,
        [Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform,
        [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{40}$')][string]$SourceCommit,
        [Parameter(Mandatory)][ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+$')][string]$CompilerVersion,
        [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedLibraryHash,
        [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedPdbHash)
    $stage=Assert-DxUiSdkOwnedPath $Root $Stage
    if(Test-Path -LiteralPath $stage){throw 'SDK staging requires a fresh owned directory.'}
    $library=Join-Path $Root ".build/$Platform/Release/DxUi.lib"
    $libraryPdb=Join-Path $Root ".build/$Platform/Release/DxUi.pdb"
    $libraryHash=$ExpectedLibraryHash
    $libraryPdbHash=$ExpectedPdbHash
New-Item -ItemType Directory -Path (Join-Path $stage "lib/$Platform/Release") -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'include') -Destination $stage -Recurse
$triplet=if($Platform -eq 'ARM64'){'arm64-windows'}else{'x64-windows'}
$installed=Join-Path $root ".build/vcpkg_installed/$Platform/$triplet"
Copy-Item -Path (Join-Path $installed 'include/*') -Destination (Join-Path $stage 'include') -Recurse
Copy-Item -LiteralPath $library -Destination (Join-Path $stage "lib/$Platform/Release/DxUi.lib")
Copy-Item -LiteralPath $libraryPdb -Destination (Join-Path $stage "lib/$Platform/Release/DxUi.pdb")
if((Get-FileHash -LiteralPath (Join-Path $stage "lib/$Platform/Release/DxUi.lib") -Algorithm SHA256).Hash.ToLowerInvariant() -cne $libraryHash -or
   (Get-FileHash -LiteralPath (Join-Path $stage "lib/$Platform/Release/DxUi.pdb") -Algorithm SHA256).Hash.ToLowerInvariant() -cne $libraryPdbHash){
    throw 'The staged library or compiler PDB changed during packaging.'
}
New-Item -ItemType Directory -Path (Join-Path $stage 'build') | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'Build/NativeBuildPolicy.targets') -Destination (Join-Path $stage 'build/NativeBuildPolicy.targets')
Copy-DxUiSdkNotices -Root $root -InstalledRoot $installed -Stage $stage -SourceCommit $sourceCommit
$licenseInventory=@(Assert-DxUiSdkNotices -Root $root -InstalledRoot $installed -Stage $stage -SourceCommit $sourceCommit)
$props=@'
<Project>
  <PropertyGroup>
    <DxUiSdkRoot>$(MSBuildThisFileDirectory)</DxUiSdkRoot>
    <ExternalIncludePath>$(DxUiSdkRoot)include;$(ExternalIncludePath)</ExternalIncludePath>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>$(DxUiSdkRoot)include;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <LanguageStandard>stdcpplatest</LanguageStandard><RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>
      <ExceptionHandling>Sync</ExceptionHandling><AdditionalOptions>/utf-8 /Zc:preprocessor %(AdditionalOptions)</AdditionalOptions>
      <PreprocessorDefinitions>UNICODE;_UNICODE;WIN32_LEAN_AND_MEAN;NOMINMAX;NDEBUG;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    </ClCompile>
    <Link><AdditionalDependencies>$(DxUiSdkRoot)lib\$(Platform)\Release\DxUi.lib;%(AdditionalDependencies)</AdditionalDependencies><LinkTimeCodeGeneration>UseLinkTimeCodeGeneration</LinkTimeCodeGeneration></Link>
  </ItemDefinitionGroup>
  <Target Name="ValidateDxUiSdkProfile" BeforeTargets="PrepareForBuild">
    <Error Condition="'$(Configuration)'!='Release'" Text="This SDK ZIP contains Release only." />
    <Error Condition="'$(Platform)'!='@PLATFORM@'" Text="This SDK ZIP has another native architecture." />
    <Error Condition="'$(VCToolsVersion)'!='@COMPILER@'" Text="Use the SDK manifest's exact MSVC compiler version for LTCG compatibility." />
  </Target>
  <Import Project="$(MSBuildThisFileDirectory)build\NativeBuildPolicy.targets" />
</Project>
'@
$props=$props.Replace('@PLATFORM@',$Platform).Replace('@COMPILER@',$CompilerVersion)
[IO.File]::WriteAllText((Join-Path $stage 'DxUi.Sdk.props'),$props,[Text.UTF8Encoding]::new($false))
    return [pscustomobject]@{Path=$stage;InstalledRoot=$installed;Licenses=@($licenseInventory)}
}

Export-ModuleMember -Function New-DxUiSdkPayload
