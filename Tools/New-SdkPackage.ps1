<#
.SYNOPSIS
Packages the exact Release archive as a relocatable DxUi SDK ZIP and runs an extracted SDK-only consumer.
.PARAMETER Platform
Matching native x64 or ARM64 host and library.
.PARAMETER Version
Frozen batch version (0.1.build).
.OUTPUTS
.build/sdk/DxUi-SDK-<version>-<platform>.zip and source/compiler/archive identity receipt.
.NOTES
Requires a completed production-only Release build, PowerShell 7.4 and the selected MSVC 14.52 toolchain.
Copies public headers, WIL public dependencies, licenses, Release library, integration props
and the unchanged reviewed native build policy. SDK headers are external dependencies of
the extracted consumer; its own translation units retain /Wall and zero-warning validation.
The smoke compiles its own consumer against extracted files only; it never rebuilds DxUi.
.EXAMPLE
./Tools/New-SdkPackage.ps1 -Platform x64 -Version 0.1.1234
#>
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform,
    [Parameter(Mandatory)][ValidatePattern('^0\.1\.[0-9]+$')][string]$Version)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Import-Module (Join-Path $root 'Tools/VisualStudio.psm1') -Force
Import-Module (Join-Path $root 'Tools/ConsumerBuild.psm1') -Force
Import-Module (Join-Path $root 'Build/BuildConsole.psm1') -Force
$installation=Get-DxUiVisualStudioInstallation
$hostArchitecture=if([Runtime.InteropServices.RuntimeInformation]::OSArchitecture -eq 'Arm64'){'arm64'}else{'amd64'}
$msbuild=Join-Path $installation "MSBuild/Current/Bin/$hostArchitecture/MSBuild.exe"
if(-not(Test-Path -LiteralPath $msbuild)){throw 'Native MSBuild is missing from the selected installation.'}
$identity=Get-DxUiConsumerBuildIdentity -DxUiRoot $root -MSBuildPath $msbuild -Platform $Platform
$library=Join-Path $root ".build/$Platform/Release/DxUi.lib"
if(-not(Test-Path -LiteralPath $library -PathType Leaf)){throw 'Build the production Release library before packaging.'}
$native=if($Platform -eq 'ARM64'){'Arm64'}else{'X64'}
if([Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString() -cne $native){throw 'SDK smoke must execute on its native architecture.'}
$directory=Join-Path $root '.build/sdk'
$stage=Join-Path $directory ('stage-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $stage "lib/$Platform/Release") -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'include') -Destination $stage -Recurse
$triplet=if($Platform -eq 'ARM64'){'arm64-windows'}else{'x64-windows'}
$installed=Join-Path $root ".build/vcpkg_installed/$Platform/$triplet"
Copy-Item -Path (Join-Path $installed 'include/*') -Destination (Join-Path $stage 'include') -Recurse
Copy-Item -LiteralPath $library -Destination (Join-Path $stage "lib/$Platform/Release/DxUi.lib")
New-Item -ItemType Directory -Path (Join-Path $stage 'build') | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'Build/NativeBuildPolicy.targets') -Destination (Join-Path $stage 'build/NativeBuildPolicy.targets')
New-Item -ItemType Directory -Path (Join-Path $stage 'licenses') | Out-Null
foreach($name in @('LICENSE.txt','THIRD-PARTY-NOTICES.md')){Copy-Item -LiteralPath (Join-Path $root $name) -Destination (Join-Path $stage 'licenses')}
Copy-Item -LiteralPath (Join-Path $installed 'share/wil/copyright') -Destination (Join-Path $stage 'licenses/WIL-LICENSE.txt')
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
$props=$props.Replace('@PLATFORM@',$Platform).Replace('@COMPILER@',$identity.Identity.vcToolsVersion)
[IO.File]::WriteAllText((Join-Path $stage 'DxUi.Sdk.props'),$props,[Text.UTF8Encoding]::new($false))
@'
Import DxUi.Sdk.props after Microsoft.Cpp.props in a Release project for the archive's architecture.
Use the exact MSVC compiler version in sdk-manifest.json; the library contains LTCG objects and uses /MD.
This archive contains no producer source, producer test binaries, SDK-private test headers or Debug/ASan archive.
Public source-pin consumers retain the separate Build/DxUi.Consumer.props contract in the source repository.
The included native policy preserves /Wall for your sources and the producer's reviewed diagnostic exceptions.
'@ | Set-Content -LiteralPath (Join-Path $stage 'README.txt') -Encoding utf8
$manifest=[ordered]@{schemaVersion=1;version=$Version;platform=$Platform;configuration='Release';kind='sdk';
    source=(& git -C $root rev-parse HEAD).Trim();apiRevision=$identity.Identity.apiRevision;compiler=$identity.Identity;
    librarySha256=(Get-FileHash -LiteralPath $library).Hash.ToLowerInvariant();runtime='MD';testsIncluded=$false}
$manifest|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $stage 'sdk-manifest.json') -Encoding utf8
New-Item -ItemType Directory -Path (Join-Path $stage 'samples/Smoke') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'Samples/SdkSmoke/Main.cpp') -Destination (Join-Path $stage 'samples/Smoke/Main.cpp')
$archive=Join-Path $directory "DxUi-SDK-$Version-$Platform.zip"
if(Test-Path -LiteralPath $archive){throw 'A previous SDK ZIP already occupies this identity; use a fresh production workspace.'}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive
$smoke=Join-Path $directory ('consumer-'+[guid]::NewGuid().ToString('N'))
$extracted=Join-Path $smoke 'extracted SDK'
Expand-Archive -LiteralPath $archive -DestinationPath $extracted
Copy-Item -LiteralPath (Join-Path $extracted 'samples/Smoke/Main.cpp') -Destination (Join-Path $smoke 'Main.cpp')
# Empty parent policy isolates the consumer from the source checkout's build imports.
'<Project />'|Set-Content -LiteralPath (Join-Path $smoke 'Directory.Build.props')
'<Project />'|Set-Content -LiteralPath (Join-Path $smoke 'Directory.Build.targets')
$project=@'
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|@PLATFORM@"><Configuration>Release</Configuration><Platform>@PLATFORM@</Platform></ProjectConfiguration></ItemGroup>
  <PropertyGroup Label="Globals"><ProjectGuid>{8082EFD1-95CD-4559-BBB7-98B9A4C6B203}</ProjectGuid><WindowsTargetPlatformVersion>@SDK@</WindowsTargetPlatformVersion></PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v145</PlatformToolset><UseDebugLibraries>false</UseDebugLibraries></PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <PropertyGroup><OutDir>$(MSBuildProjectDirectory)\bin\</OutDir><IntDir>$(MSBuildProjectDirectory)\obj\</IntDir></PropertyGroup>
  <Import Project="$(MSBuildProjectDirectory)\extracted SDK\DxUi.Sdk.props" />
  <ItemDefinitionGroup><Link><SubSystem>Console</SubSystem></Link><ClCompile><WarningLevel>EnableAllWarnings</WarningLevel><TreatWarningAsError>false</TreatWarningAsError></ClCompile></ItemDefinitionGroup>
  <ItemGroup><ClCompile Include="Main.cpp" /></ItemGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
'@
$project=$project.Replace('@PLATFORM@',$Platform).Replace('@SDK@',$identity.Identity.windowsSdkVersion)
$projectPath=Join-Path $smoke 'SdkSmoke.vcxproj'
[IO.File]::WriteAllText($projectPath,$project,[Text.UTF8Encoding]::new($false))
$before=(Get-FileHash -LiteralPath (Join-Path $extracted "lib/$Platform/Release/DxUi.lib")).Hash
$logs=Join-Path $root '.build/logs'
[void][IO.Directory]::CreateDirectory($logs)
$smokeLog=Join-Path $logs "sdk-consumer-$Version-$Platform.log"
& $msbuild $projectPath /nologo /m /v:minimal /p:Configuration=Release "/p:Platform=$Platform" "/p:VCToolsVersion=$($identity.Identity.vcToolsVersion)" 2>&1 | Tee-Object -FilePath $smokeLog | ForEach-Object {Write-Host $_}
if($LASTEXITCODE -ne 0){throw 'Extracted SDK-only consumer failed to compile/link.'}
Assert-NativeBuildDiagnostics -LogPath $smokeLog
& (Join-Path $smoke 'bin/SdkSmoke.exe')
if($LASTEXITCODE -ne 0){throw 'Extracted SDK-only consumer failed at runtime.'}
if((Get-FileHash -LiteralPath (Join-Path $extracted "lib/$Platform/Release/DxUi.lib")).Hash -cne $before){throw 'Consumer build changed the SDK archive library.'}
# A real first-party warning must still be rejected after SDK headers become external.
# Keep separate outputs so this negative control cannot replace the positive consumer.
$warningProject=Join-Path $smoke 'SdkWarningControl.vcxproj'
$warningSource=Join-Path $smoke 'WarningControl.cpp'
[IO.File]::WriteAllText($warningSource,'int main() { int unused = 1; return 0; }')
[IO.File]::WriteAllText($warningProject,$project.Replace('Include="Main.cpp"','Include="WarningControl.cpp"'))
$warningLog=Join-Path $logs "sdk-warning-control-$Version-$Platform.log"
$warningArguments=@($warningProject,'/nologo','/m','/v:minimal','/p:Configuration=Release',
    ("/p:Platform=$Platform"),("/p:VCToolsVersion=$($identity.Identity.vcToolsVersion)"),
    ('/p:IntDir='+(Join-Path $smoke 'warning-obj')+'\'),('/p:OutDir='+(Join-Path $smoke 'warning-bin')+'\'))
& $msbuild @warningArguments 2>&1 | Tee-Object -FilePath $warningLog | ForEach-Object {Write-Host $_}
if($LASTEXITCODE -ne 0){throw 'SDK warning control must compile successfully with /WX disabled.'}
$diagnostics=Get-NativeBuildDiagnostics -LogPath $warningLog
if($diagnostics.WarningCount -lt 1 -or $diagnostics.ErrorCount -ne 0 -or
    [IO.File]::ReadAllText($warningLog) -notmatch '\bC4189\b'){throw 'SDK integration hid the first-party warning control.'}
$refused=$false
try { Assert-NativeBuildDiagnostics -LogPath $warningLog } catch { $refused=$_.Exception.Message -like 'Zero-warning build validation failed:*' }
if(-not $refused){throw 'SDK first-party warning did not fail zero-warning validation.'}
$manifest['smokePassed']=$true
$manifest['firstPartyWarningControlPassed']=$true
$manifest['nativePolicySha256']=(Get-FileHash -LiteralPath (Join-Path $extracted 'build/NativeBuildPolicy.targets')).Hash.ToLowerInvariant()
$manifest['archiveSha256']=(Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant()
$manifest|ConvertTo-Json -Depth 12|Set-Content -LiteralPath ($archive+'.receipt.json') -Encoding utf8
Write-Host "Qualified extracted SDK ZIP: $archive"
